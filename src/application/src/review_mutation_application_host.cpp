#include "bpr/application/review_mutation_application_host.hpp"

#include <atomic>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <stop_token>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr auto maximum_outstanding_review_mutations = std::size_t{ 8U };

		[[nodiscard]] auto portable_path_text(
			const std::filesystem::path& aPath) -> std::optional<std::string>
		{
			if (aPath.empty() || aPath.is_absolute()
				|| aPath.lexically_normal() != aPath) {
				return std::nullopt;
			}
			for (const auto& component : aPath) {
				if (component == "..") {
					return std::nullopt;
				}
			}
			const auto encoded = aPath.generic_u8string();
			return std::string{
				reinterpret_cast<const char*>(encoded.data()), encoded.size()
			};
		}

	} // namespace

#if defined(BPR_APPLICATION_TESTING)
	auto make_review_mutation_testing_grant(
		const core::application_session_id aSession,
		const core::request_generation     aGeneration,
		core::review_mutation_target       aTarget,
		const core::github_account_id      aActingIdentity)
		-> review_mutation_execution_grant
	{
		return review_mutation_execution_grant{
			new review_mutation_execution_authority{
				review_mutation_execution_scope{
					.ApplicationSession  = aSession,
					.WorkbenchGeneration = aGeneration,
					.Target              = std::move(aTarget),
					.ActingIdentity      = aActingIdentity,
				} }
		};
	}
#endif

	struct review_mutation_application_host::implementation final
		: std::enable_shared_from_this<implementation>
	{
		struct request_record final
		{
			review_mutation_request_context     Context;
			review_mutation_publication_request Request;
			review_mutation_anchor_binding      Anchor;
			std::stop_source                    Stop;
			bool                                Superseded = false;
		};

		implementation(
			std::unique_ptr<review_mutation_port> aProvider,
			review_mutation_target_binding        aTarget,
			review_mutation_acting_identity       aActingIdentity,
			const core::application_session_id    aSession,
			const core::request_generation        aWorkbenchGeneration,
			review_mutation_execution_grant       aExecutionGrant,
			effect_executor&                      aEffects,
			decision_executor&                    aDecisionLane,
			review_mutation_projection_port&      aProjection)
			: OwnedProvider{ std::move(aProvider) }
			, Coordinator{ *OwnedProvider, std::move(aTarget), aActingIdentity }
			, Session{ aSession }
			, Generation{ aWorkbenchGeneration }
			, ActingIdentity{ aActingIdentity.Account }
			, Grant{ std::move(aExecutionGrant) }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, Capabilities{ Coordinator.capabilities() }
			, CompleteLiveBinding{ true }
		{
		}

#if defined(BPR_APPLICATION_TESTING)
		implementation(
			review_mutation_port&              aProvider,
			core::review_mutation_target       aTarget,
			const core::github_account_id      aActingIdentity,
			const core::application_session_id aSession,
			const core::request_generation     aWorkbenchGeneration,
			review_mutation_execution_grant    aExecutionGrant,
			effect_executor&                   aEffects,
			decision_executor&                 aDecisionLane,
			review_mutation_projection_port&   aProjection)
			: Coordinator{ aProvider, std::move(aTarget), aActingIdentity }
			, Session{ aSession }
			, Generation{ aWorkbenchGeneration }
			, ActingIdentity{ aActingIdentity }
			, Grant{ std::move(aExecutionGrant) }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, Capabilities{ Coordinator.capabilities() }
		{
		}
#endif

		[[nodiscard]] auto context() noexcept -> review_mutation_request_context
		{
			return {
				.Request = core::request_id{
					NextRequest.fetch_add(1U, std::memory_order_relaxed) },
				.Session    = Session,
				.Generation = Generation,
			};
		}

		[[nodiscard]] auto validate(
			const review_mutation_publication_request& aRequest,
			const review_mutation_execution_grant&     aGrant) const
			-> std::expected<review_mutation_anchor_binding, review_mutation_failure>
		{
			const auto& publication = aRequest.SelectedDiff;
			const auto& selected    = aRequest.Selection;
			const auto& key         = publication.Key;
			const auto& comparison  = publication.Request.Diff.Comparison;
			if (selected.PreferredLine.Side != core::diff_side::right) {
				return std::unexpected(review_mutation_failure::unsupported);
			}
			if (!publication.RightPath.has_value() || !key.RightBlob.has_value()
				|| !publication.RightEndpoint.has_value()) {
				return std::unexpected(review_mutation_failure::unsupported);
			}
			if (publication.Request.Context.Session != Session
				|| publication.Request.Context.Generation != Generation) {
				return std::unexpected(review_mutation_failure::stale_head);
			}
			if (comparison.Right.Commit != Coordinator.designated().ExpectedHead) {
				return std::unexpected(review_mutation_failure::stale_head);
			}
			if (!core::valid(key)
				|| key.Repository != Coordinator.designated().Repository
				|| key.PullRequest != Coordinator.designated().PullRequest
				|| key.Authority.Host != Coordinator.designated().Host
				|| publication.Request.File.File != key.File
				|| publication.Request.WorkbenchAuthority.Host
					!= Coordinator.designated().Host
				|| publication.Request.WorkbenchAuthority.Repository != key.Repository
				|| publication.Request.WorkbenchAuthority.PullRequest != key.PullRequest
				|| key.LeftAnchor != comparison.Left.Identity
				|| key.RightAnchor != comparison.Right.Identity
				|| key.LeftBlob.has_value() != publication.LeftPath.has_value()
				|| key.RightBlob.has_value() != publication.RightPath.has_value()
				|| key.LeftBlob.has_value()
					!= publication.LeftEndpoint.has_value()
				|| (key.LeftBlob.has_value()
					&& !valid(
						*publication.LeftEndpoint,
						publication.Request,
						core::diff_side::left,
						*key.LeftBlob,
						*publication.LeftPath))
				|| !valid(
					*publication.RightEndpoint,
					publication.Request,
					core::diff_side::right,
					*key.RightBlob,
					*publication.RightPath)
				|| selected.Comparison != core::comparison_digest(key)
				|| selected.PreferredLine.Comparison != selected.Comparison
				|| aRequest.Proposal.Anchor != selected.PreferredLine) {
				return std::unexpected(review_mutation_failure::undesignated_target);
			}
			auto path = portable_path_text(*publication.RightPath);
			if (!path.has_value()) {
				return std::unexpected(review_mutation_failure::unsupported);
			}
			auto anchor = review_mutation_anchor_binding{
				.Semantic = selected.PreferredLine,
				.Path     = std::move(*path),
				.Commit   = comparison.Right.Commit,
			};
			if (!valid(anchor)) {
				return std::unexpected(review_mutation_failure::undesignated_target);
			}
			const auto scope = review_mutation_execution_scope{
				.ApplicationSession  = Session,
				.WorkbenchGeneration = Generation,
				.Target              = Coordinator.designated(),
				.ActingIdentity      = ActingIdentity,
				.Comparison          = selected.Comparison,
				.File                = key.File,
				.Anchor              = selected.PreferredLine,
				.Commit              = comparison.Right.Commit,
			};
			if (!aGrant || !aGrant->permits(scope)) {
				return std::unexpected(review_mutation_failure::unavailable);
			}
			return anchor;
		}

		void complete(
			const std::shared_ptr<request_record>&                          aRecord,
			std::expected<review_mutation_receipt, review_mutation_failure> aResult)
		{
			auto publish = false;
			{
				std::scoped_lock lock{ Mutex };
				const auto       found = Outstanding.find(aRecord->Context.Request.value());
				if (found == Outstanding.end() || found->second != aRecord) {
					++Statistics.StaleTerminalsRejected;
					return;
				}
				Outstanding.erase(found);
				++Statistics.Completions;
				publish = !Closing && !aRecord->Superseded && Active == aRecord;
				if (Active == aRecord) {
					Active.reset();
				}
				if (!publish) {
					++Statistics.StaleTerminalsRejected;
				} else {
					++Statistics.TerminalsPublished;
				}
			}
			if (publish) {
				try {
					Projection.publish(review_mutation_terminal{
						.Context = aRecord->Context,
						.Result  = std::move(aResult),
					});
				} catch (...) {
				}
			}
		}

		void schedule_terminal(
			const std::shared_ptr<request_record>& aRecord,
			review_mutation_failure                aFailure)
		{
			auto self = shared_from_this();
			DecisionLane.post([self, aRecord, aFailure] {
				self->complete(aRecord, std::unexpected(aFailure));
			});
		}

		void drain_effects()
		{
			for (;;) {
				auto record = std::shared_ptr<request_record>{ };
				{
					std::scoped_lock lock{ Mutex };
					if (EffectQueue.empty()) {
						DrainScheduled = false;
						return;
					}
					record = std::move(EffectQueue.front());
					EffectQueue.pop_front();
				}

				auto result = std::expected<
					review_mutation_receipt,
					review_mutation_failure>{
					std::unexpected(review_mutation_failure::cancelled)
				};
				if (!record->Stop.stop_requested()) {
					try {
						result = CompleteLiveBinding
							? Coordinator.publish(
								  record->Request.Proposal,
								  record->Anchor,
								  record->Request.Acknowledged,
								  record->Stop.get_token())
							: Coordinator.publish(
								  record->Request.Proposal,
								  record->Request.Acknowledged,
								  record->Stop.get_token());
					} catch (...) {
						result = std::unexpected(
							review_mutation_failure::unavailable);
					}
				}

				auto self = shared_from_this();
				DecisionLane.post(
					[self, record, result = std::move(result)]() mutable {
						self->complete(record, std::move(result));
					});
			}
		}

		void queue_effect(const std::shared_ptr<request_record>& aRecord)
		{
			auto refused = std::deque<std::shared_ptr<request_record>>{ };
			{
				std::scoped_lock lock{ Mutex };
				EffectQueue.push_back(aRecord);
				++Statistics.EffectsQueued;
				if (DrainScheduled) {
					return;
				}
				DrainScheduled = true;
				auto self      = shared_from_this();
				try {
					Effects.post([self] { self->drain_effects(); });
					return;
				} catch (...) {
					DrainScheduled = false;
					refused.swap(EffectQueue);
				}
			}
			for (const auto& record : refused) {
				schedule_terminal(
					record, review_mutation_failure::unavailable);
			}
		}

		[[nodiscard]] auto publish(review_mutation_publication_request aRequest)
			-> review_mutation_request_context
		{
			auto record = std::make_shared<request_record>(request_record{
				.Context = context(),
				.Request = std::move(aRequest),
				.Anchor  = { },
				.Stop    = { },
			});
			auto grant  = review_mutation_execution_grant{ };
			{
				std::scoped_lock lock{ Mutex };
				++Statistics.Attempts;
				if (Closing) {
					++Statistics.RefusedBeforeEffect;
					return {
						.Session    = Session,
						.Generation = Generation,
					};
				}
				if (Outstanding.size() >= maximum_outstanding_review_mutations) {
					++Statistics.RefusedBeforeEffect;
					return {
						.Session    = Session,
						.Generation = Generation,
					};
				}
				for (auto& entry : Outstanding) {
					auto& outstanding = entry.second;
					if (!outstanding->Superseded) {
						outstanding->Superseded = true;
						static_cast<void>(outstanding->Stop.request_stop());
						++Statistics.Superseded;
					}
				}
				Outstanding.emplace(record->Context.Request.value(), record);
				Active = record;
				grant  = Grant;
			}

			auto anchor = validate(record->Request, grant);
			if (!anchor.has_value()) {
				{
					std::scoped_lock lock{ Mutex };
					++Statistics.RefusedBeforeEffect;
				}
				schedule_terminal(record, anchor.error());
				return record->Context;
			}
			record->Anchor = std::move(*anchor);

			queue_effect(record);
			return record->Context;
		}

		void cancel(const review_mutation_request_context aContext) noexcept
		{
			std::scoped_lock lock{ Mutex };
			const auto       found = Outstanding.find(aContext.Request.value());
			if (found != Outstanding.end()
				&& found->second->Context == aContext) {
				static_cast<void>(found->second->Stop.request_stop());
			}
		}

		void shutdown() noexcept
		{
			std::scoped_lock lock{ Mutex };
			if (Closing) {
				return;
			}
			Closing = true;
			Grant.reset();
			Active.reset();
			for (auto& entry : Outstanding) {
				auto& record = entry.second;
				if (record->Stop.request_stop()) {
					++Statistics.ShutdownRequestsStopped;
				}
			}
		}

		std::unique_ptr<review_mutation_port> OwnedProvider;
		review_mutation_coordinator           Coordinator;
		core::application_session_id          Session;
		core::request_generation              Generation;
		core::github_account_id               ActingIdentity;
		review_mutation_execution_grant       Grant;
		effect_executor&                      Effects;
		decision_executor&                    DecisionLane;
		review_mutation_projection_port&      Projection;
		review_provider_capabilities          Capabilities;
		bool                                  CompleteLiveBinding = false;

		mutable std::mutex                                       Mutex;
		std::map<std::uint64_t, std::shared_ptr<request_record>> Outstanding;
		std::deque<std::shared_ptr<request_record>>              EffectQueue;
		std::shared_ptr<request_record>                          Active;
		review_mutation_application_host_statistics              Statistics;
		std::atomic<std::uint64_t>                               NextRequest    = 1U;
		bool                                                     Closing        = false;
		bool                                                     DrainScheduled = false;
	};

	review_mutation_application_host::review_mutation_application_host(
		std::unique_ptr<review_mutation_port> aProvider,
		review_mutation_target_binding        aTarget,
		review_mutation_acting_identity       aActingIdentity,
		const core::application_session_id    aSession,
		const core::request_generation        aWorkbenchGeneration,
		review_mutation_execution_grant       aExecutionGrant,
		effect_executor&                      aEffects,
		decision_executor&                    aDecisionLane,
		review_mutation_projection_port&      aProjection)
		: Implementation{ std::make_shared<implementation>(
			  std::move(aProvider),
			  std::move(aTarget),
			  std::move(aActingIdentity),
			  aSession,
			  aWorkbenchGeneration,
			  std::move(aExecutionGrant),
			  aEffects,
			  aDecisionLane,
			  aProjection) }
	{
	}

#if defined(BPR_APPLICATION_TESTING)
	review_mutation_application_host::review_mutation_application_host(
		review_mutation_port&              aFixtureProvider,
		core::review_mutation_target       aTarget,
		const core::github_account_id      aActingIdentity,
		const core::application_session_id aSession,
		const core::request_generation     aWorkbenchGeneration,
		review_mutation_execution_grant    aExecutionGrant,
		effect_executor&                   aEffects,
		decision_executor&                 aDecisionLane,
		review_mutation_projection_port&   aProjection)
		: Implementation{ std::make_shared<implementation>(
			  aFixtureProvider,
			  std::move(aTarget),
			  aActingIdentity,
			  aSession,
			  aWorkbenchGeneration,
			  std::move(aExecutionGrant),
			  aEffects,
			  aDecisionLane,
			  aProjection) }
	{
	}
#endif

	review_mutation_application_host::~review_mutation_application_host()
	{
		shutdown();
	}

	auto review_mutation_application_host::publish(
		review_mutation_publication_request aRequest)
		-> review_mutation_request_context
	{
		return Implementation->publish(std::move(aRequest));
	}

	void review_mutation_application_host::cancel(
		const review_mutation_request_context aContext) noexcept
	{
		Implementation->cancel(aContext);
	}

	void review_mutation_application_host::shutdown() noexcept
	{
		if (Implementation) {
			Implementation->shutdown();
		}
	}

	auto review_mutation_application_host::designated() const noexcept
		-> const core::review_mutation_target&
	{
		return Implementation->Coordinator.designated();
	}

	auto review_mutation_application_host::capabilities() const noexcept
		-> review_provider_capabilities
	{
		return Implementation->Capabilities;
	}

	auto review_mutation_application_host::is_quiescent() const noexcept -> bool
	{
		std::scoped_lock lock{ Implementation->Mutex };
		return Implementation->Outstanding.empty()
			&& Implementation->EffectQueue.empty()
			&& !Implementation->DrainScheduled;
	}

	auto review_mutation_application_host::outstanding_requests() const noexcept
		-> std::size_t
	{
		std::scoped_lock lock{ Implementation->Mutex };
		return Implementation->Outstanding.size();
	}

	auto review_mutation_application_host::statistics() const noexcept
		-> review_mutation_application_host_statistics
	{
		std::scoped_lock lock{ Implementation->Mutex };
		return Implementation->Statistics;
	}
} // namespace bpr::application
