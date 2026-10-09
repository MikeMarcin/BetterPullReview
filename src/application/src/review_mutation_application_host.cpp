#include "bpr/application/review_mutation_application_host.hpp"

#include "review_mutation_execution_authority.hpp"

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

		[[nodiscard]] auto valid_execution_scope(
			const review_mutation_execution_scope& aScope) noexcept -> bool
		{
			return aScope.ApplicationSession.value() != 0U
				&& aScope.WorkbenchGeneration.value() != 0U
				&& valid(aScope.Target) && valid(aScope.ActingIdentity)
				&& aScope.Comparison.valid() && aScope.File.valid()
				&& aScope.Anchor.Comparison == aScope.Comparison
				// [[BPR-0657]]. Either side: a reply is drafted from whichever
				// line was selected; `validate` still holds a root comment to
				// the right side.
				&& (aScope.Anchor.Side == core::diff_side::left
					|| aScope.Anchor.Side == core::diff_side::right)
				&& !aScope.ResolvedPath.empty()
				&& aScope.Commit == aScope.Target.Semantic.ExpectedHead;
		}

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

	review_mutation_execution_authority::review_mutation_execution_authority(
		review_mutation_execution_scope                    aScope,
		std::optional<review_mutation_publication_request> aRequest)
		: Request{ std::move(aRequest) }
		, Scope{ std::move(aScope) }
		, Target{ Scope.Target }
		, ActingIdentity{ Scope.ActingIdentity }
		, Bound{ true }
	{
		if (Request) {
			Kind      = kind::user_one_shot;
			ExpiresAt = std::chrono::steady_clock::now() + std::chrono::minutes{ 5 };
		}
	}

	review_mutation_execution_authority::review_mutation_execution_authority(
		review_mutation_target_binding              aTarget,
		const std::chrono::steady_clock::time_point aExpiresAt) noexcept
		: Target{ std::move(aTarget) }
		, ExpiresAt{ aExpiresAt }
		, Kind{ kind::attended_one_shot }
	{
	}

	auto review_mutation_execution_authority::bind(
		const review_mutation_execution_scope& aScope) noexcept -> bool
	{
		if (Revoked.load(std::memory_order_acquire)
			|| aScope.ApplicationSession.value() == 0U
			|| aScope.WorkbenchGeneration.value() == 0U
			|| !valid(aScope.Target) || !valid(aScope.ActingIdentity)
			|| aScope.Target != Target) {
			return false;
		}
		std::scoped_lock lock{ Mutex };
		if (Revoked.load(std::memory_order_relaxed)) {
			return false;
		}
		if (Kind != kind::testing_reusable
			&& std::chrono::steady_clock::now() >= ExpiresAt) {
			Revoked.store(true, std::memory_order_release);
			return false;
		}
		if (Bound) {
			return Scope.ApplicationSession == aScope.ApplicationSession
				&& Scope.WorkbenchGeneration == aScope.WorkbenchGeneration
				&& Scope.Target == aScope.Target
				&& ActingIdentity.has_value()
				&& *ActingIdentity == aScope.ActingIdentity;
		}
		Scope.ApplicationSession  = aScope.ApplicationSession;
		Scope.WorkbenchGeneration = aScope.WorkbenchGeneration;
		Scope.Target              = aScope.Target;
		Scope.ActingIdentity      = aScope.ActingIdentity;
		ActingIdentity            = aScope.ActingIdentity;
		Bound                     = true;
		return true;
	}

	auto review_mutation_execution_authority::permits(
		const review_mutation_execution_scope&     aScope,
		const review_mutation_publication_request& aRequest) noexcept -> bool
	{
		if (!valid_execution_scope(aScope)
			|| Revoked.load(std::memory_order_acquire)) {
			return false;
		}
		std::scoped_lock lock{ Mutex };
		if (!Bound || Revoked.load(std::memory_order_relaxed)
			|| !ActingIdentity.has_value()
			|| Scope.ApplicationSession != aScope.ApplicationSession
			|| Scope.WorkbenchGeneration != aScope.WorkbenchGeneration
			|| Scope.Target != aScope.Target
			|| *ActingIdentity != aScope.ActingIdentity) {
			return false;
		}
		if (Kind == kind::user_one_shot && (!Request || *Request != aRequest)) {
			return false;
		}
		if (Kind == kind::testing_reusable) {
			return true;
		}
		if (std::chrono::steady_clock::now() >= ExpiresAt) {
			Revoked.store(true, std::memory_order_release);
			return false;
		}
		auto expected = false;
		return Consumed.compare_exchange_strong(
			expected,
			true,
			std::memory_order_acq_rel,
			std::memory_order_acquire);
	}

	review_mutation_execution_authority::review_mutation_execution_authority(
		review_mutation_thread_scope                 aScope,
		review_thread_resolution_publication_request aRequest)
		: ThreadRequest{ std::move(aRequest) }
		, Scope{ .ApplicationSession  = aScope.ApplicationSession,
				 .WorkbenchGeneration = aScope.WorkbenchGeneration,
				 .Target              = aScope.Target,
				 .ActingIdentity      = aScope.ActingIdentity }
		, Target{ aScope.Target }
		, ActingIdentity{ aScope.ActingIdentity }
		, ExpiresAt{ std::chrono::steady_clock::now() + std::chrono::minutes{ 5 } }
		, Kind{ kind::user_one_shot }
		, Bound{ true }
	{
	}
	auto review_mutation_execution_authority::bind_thread(
		const review_mutation_thread_scope& aScope) noexcept -> bool
	{
		return bind(review_mutation_execution_scope{
			.ApplicationSession  = aScope.ApplicationSession,
			.WorkbenchGeneration = aScope.WorkbenchGeneration,
			.Target              = aScope.Target,
			.ActingIdentity      = aScope.ActingIdentity,
		});
	}
	auto review_mutation_execution_authority::permits_thread(
		const review_mutation_thread_scope&                 aScope,
		const review_thread_resolution_publication_request& aRequest) noexcept -> bool
	{
		if (aScope.ApplicationSession.value() == 0U || aScope.WorkbenchGeneration.value() == 0U
			|| !valid(aScope.Target) || !valid(aScope.ActingIdentity) || aScope.Thread.empty()
			|| aRequest.Thread != aScope.Thread || Revoked.load(std::memory_order_acquire)) {
			return false;
		}
		std::scoped_lock lock{ Mutex };
		if (!Bound || Revoked.load(std::memory_order_relaxed)
			|| !ActingIdentity.has_value()
			|| Scope.ApplicationSession != aScope.ApplicationSession
			|| Scope.WorkbenchGeneration != aScope.WorkbenchGeneration
			|| Scope.Target != aScope.Target
			|| *ActingIdentity != aScope.ActingIdentity) {
			return false;
		}
		// A one-shot grant issued for a publication permits no resolution,
		// and one issued for a resolution permits only that resolution.
		if (Kind == kind::user_one_shot && (Request.has_value() || !ThreadRequest || *ThreadRequest != aRequest)) {
			return false;
		}
		if (Kind == kind::testing_reusable) {
			return true;
		}
		if (std::chrono::steady_clock::now() >= ExpiresAt) {
			Revoked.store(true, std::memory_order_release);
			return false;
		}
		auto expected = false;
		return Consumed.compare_exchange_strong(
			expected,
			true,
			std::memory_order_acq_rel,
			std::memory_order_acquire);
	}
	void review_mutation_execution_authority::revoke() noexcept
	{
		std::scoped_lock lock{ Mutex };
		if (Kind != kind::testing_reusable) {
			Revoked.store(true, std::memory_order_release);
		}
	}

	auto detail::review_mutation_user_authority_issuer::issue_thread(
		core::application_session_id aSession, core::request_generation aGeneration,
		review_mutation_target_binding aTarget, review_mutation_acting_identity aActor,
		review_thread_resolution_publication_request aRequest) -> review_mutation_execution_grant
	{
#if defined(BPR_APPLICATION_TESTING)
		static_cast<void>(aSession);
		static_cast<void>(aGeneration);
		static_cast<void>(aTarget);
		static_cast<void>(aActor);
		static_cast<void>(aRequest);
		return { };
#else
		if (aSession.value() == 0U || aGeneration.value() == 0U
			|| !valid(aTarget) || !valid(aActor) || aRequest.Thread.empty()) {
			return { };
		}
		return review_mutation_execution_grant{ new review_mutation_execution_authority{
			review_mutation_thread_scope{ .ApplicationSession  = aSession,
										  .WorkbenchGeneration = aGeneration,
										  .Target              = std::move(aTarget),
										  .ActingIdentity      = std::move(aActor),
										  .Thread              = aRequest.Thread },
			std::move(aRequest) } };
#endif
	}
	auto detail::review_mutation_user_authority_issuer::issue(
		core::application_session_id aSession, core::request_generation aGeneration,
		review_mutation_target_binding aTarget, review_mutation_acting_identity aActor,
		review_mutation_publication_request aRequest) -> review_mutation_execution_grant
	{
#if defined(BPR_APPLICATION_TESTING)
		static_cast<void>(aSession);
		static_cast<void>(aGeneration);
		static_cast<void>(aTarget);
		static_cast<void>(aActor);
		static_cast<void>(aRequest);
		return { };
#else
		if (aSession.value() == 0U || aGeneration.value() == 0U
			|| !valid(aTarget) || !valid(aActor)) {
			return { };
		}
		return review_mutation_execution_grant{ new review_mutation_execution_authority{
			review_mutation_execution_scope{ .ApplicationSession  = aSession,
											 .WorkbenchGeneration = aGeneration,
											 .Target              = std::move(aTarget),
											 .ActingIdentity      = std::move(aActor) },
			std::move(aRequest) } };
#endif
	}

#if defined(BPR_APPLICATION_TESTING)
	auto make_review_mutation_testing_grant(
		const core::application_session_id                 aSession,
		const core::request_generation                     aGeneration,
		review_mutation_target_binding                     aTarget,
		review_mutation_acting_identity                    aActingIdentity,
		std::optional<review_mutation_publication_request> aRequest)
		-> review_mutation_execution_grant
	{
		return review_mutation_execution_grant{
			new review_mutation_execution_authority{
				review_mutation_execution_scope{
					.ApplicationSession  = aSession,
					.WorkbenchGeneration = aGeneration,
					.Target              = std::move(aTarget),
					.ActingIdentity      = aActingIdentity,
				},
				std::move(aRequest) }
		};
	}

	auto make_review_mutation_one_shot_testing_grant(
		review_mutation_target_binding              aTarget,
		const std::chrono::steady_clock::time_point aExpiresAt)
		-> review_mutation_execution_grant
	{
		return review_mutation_execution_grant{
			new review_mutation_execution_authority{
				std::move(aTarget), aExpiresAt }
		};
	}
#endif

	struct review_mutation_application_host::implementation final
		: std::enable_shared_from_this<implementation>
	{
		struct request_record final
		{
			review_mutation_request_context               Context;
			review_mutation_publication_request           Request;
			review_mutation_anchor_binding                Anchor;
			std::optional<review_mutation_source_binding> Source;
			std::stop_source                              Stop;
			bool                                          Superseded = false;
			bool                                          Refusal    = false;
		};

		struct validated_publication final
		{
			review_mutation_anchor_binding                Anchor;
			review_mutation_execution_scope               Scope;
			std::optional<review_mutation_source_binding> Source;
		};

		// [[BPR-0657]]. One thread resolution in flight: its own record and
		// queue, beside the publications; a resolution neither supersedes a
		// publication nor is superseded by one.
		struct resolution_record final
		{
			review_mutation_request_context              Context;
			review_thread_resolution_publication_request Request;
			std::stop_source                             Stop;
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
			review_mutation_projection_port&      aProjection,
			review_mutation_source_port*          aSources)
			: OwnedProvider{ std::move(aProvider) }
			, TargetBinding{ std::move(aTarget) }
			, ActingIdentityBinding{ std::move(aActingIdentity) }
			, Coordinator{ *OwnedProvider, TargetBinding, ActingIdentityBinding }
			, Session{ aSession }
			, Generation{ aWorkbenchGeneration }
			, Grant{ std::move(aExecutionGrant) }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, Sources{ aSources }
			, Capabilities{ Coordinator.capabilities() }
			, CompleteLiveBinding{ true }
		{
			bind_grant();
		}

#if defined(BPR_APPLICATION_TESTING)
		implementation(
			review_mutation_port&              aProvider,
			review_mutation_target_binding     aTarget,
			review_mutation_acting_identity    aActingIdentity,
			const core::application_session_id aSession,
			const core::request_generation     aWorkbenchGeneration,
			review_mutation_execution_grant    aExecutionGrant,
			effect_executor&                   aEffects,
			decision_executor&                 aDecisionLane,
			review_mutation_projection_port&   aProjection,
			review_mutation_source_port*       aSources)
			: TargetBinding{ std::move(aTarget) }
			, ActingIdentityBinding{ std::move(aActingIdentity) }
			, Coordinator{ aProvider, TargetBinding, ActingIdentityBinding }
			, Session{ aSession }
			, Generation{ aWorkbenchGeneration }
			, Grant{ std::move(aExecutionGrant) }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, Sources{ aSources }
			, Capabilities{ Coordinator.capabilities() }
			, CompleteLiveBinding{ true }
		{
			bind_grant();
		}
#endif

		void bind_grant() noexcept
		{
			if (Grant == nullptr) {
				return;
			}
			const auto scope = review_mutation_execution_scope{
				.ApplicationSession  = Session,
				.WorkbenchGeneration = Generation,
				.Target              = TargetBinding,
				.ActingIdentity      = ActingIdentityBinding,
			};
			if (!Grant->bind(scope)) {
				Grant.reset();
			}
		}

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
			const review_mutation_publication_request& aRequest) const
			-> std::expected<validated_publication, review_mutation_failure>
		{
			const auto& publication = aRequest.SelectedDiff;
			const auto& selected    = aRequest.Selection;
			const auto& key         = publication.Key;
			const auto& comparison  = publication.Request.Diff.Comparison;
			// [[BPR-0657]], [[BPR-1335]] v002. A new root comment is anchored
			// where it is published, on either side: GitHub's `side` names it,
			// and a base-side comment is `side: LEFT` on the base line. A reply
			// is anchored by the thread it answers. A selection on no known
			// side carries neither.
			if (!core::supported_comment_target(selected.PreferredLine.Side, aRequest.Proposal.ReplyToThread.has_value())) {
				return std::unexpected(review_mutation_failure::unsupported);
			}
			// [[BPR-1212]]. A range is a new comment whose first line precedes
			// its anchor, the last line, on the anchor's side; whether both are
			// in one hunk was decided against the diff before the draft took it.
			if (aRequest.Proposal.StartLine
				&& (aRequest.Proposal.ReplyToThread || !core::supported_comment_target(aRequest.Proposal.Anchor.Side, false)
					|| *aRequest.Proposal.StartLine >= aRequest.Proposal.Anchor.Line)) {
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
			auto source = std::optional<review_mutation_source_observation>{ };
			try {
				if (Sources != nullptr) {
					source = Sources->observe_review_source(key.File);
				}
			} catch (...) {
				return std::unexpected(review_mutation_failure::unavailable);
			}
			if (!source || source->Authority != publication.Request.WorkbenchAuthority
				|| source->File != publication.Request.File || source->Material.Key != key
				|| source->Target != TargetBinding || source->ActingIdentity != ActingIdentityBinding
				|| source->File.CurrentPath != *publication.RightPath
				|| source->Placement > core::review_placement::off_hunk) {
				return std::unexpected(review_mutation_failure::undesignated_target);
			}
			const auto lines = selected.PreferredLine.Side == core::diff_side::right
				? source->Material.RightLines
				: source->Material.LeftLines;
			if (!source->Material.RightLines || !lines || selected.PreferredLine.Line >= *lines) {
				return std::unexpected(review_mutation_failure::unsupported);
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
				.Target              = TargetBinding,
				.ActingIdentity      = ActingIdentityBinding,
				.Comparison          = selected.Comparison,
				.File                = key.File,
				.Anchor              = selected.PreferredLine,
				.ResolvedPath        = anchor.Path,
				.Commit              = comparison.Right.Commit,
			};
			return validated_publication{
				.Anchor = std::move(anchor),
				.Scope  = scope,
				.Source = review_mutation_source_binding{ .Endpoint = publication.RightEndpoint, .Comparison = key, .LineCount = *source->Material.RightLines, .Placement = source->Placement },
			};
		}

		void complete(
			const std::shared_ptr<request_record>& aRecord,
			review_mutation_submission             aSubmission)
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
				publish = !Closing && !aRecord->Superseded
					&& (aRecord->Refusal || Active == aRecord);
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
						.Context           = aRecord->Context,
						.Result            = std::move(aSubmission.Result),
						.Reconciliation    = std::move(aSubmission.Reconciliation),
						.MutationAttempted = aSubmission.MutationAttempted,
						.Lineage           = aSubmission.Lineage,
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
				self->complete(aRecord, review_mutation_submission{
											.Result = std::unexpected(aFailure),
										});
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

				auto result = review_mutation_submission{
					.Result = std::unexpected(review_mutation_failure::cancelled),
				};
				if (!record->Stop.stop_requested()) {
					try {
						result = CompleteLiveBinding
							? Coordinator.publish_attempt(
								  record->Request.Proposal,
								  record->Anchor,
								  record->Request.Acknowledged,
								  record->Stop.get_token(), record->Source)
							: review_mutation_submission{
								  .Result = Coordinator.publish(
									  record->Request.Proposal,
									  record->Request.Acknowledged,
									  record->Stop.get_token()),
							  };
					} catch (...) {
						result = review_mutation_submission{
							.Result = std::unexpected(
								review_mutation_failure::unavailable),
						};
					}
				}

				auto self = shared_from_this();
				DecisionLane.post(
					[self, record, mutationSubmission = std::move(result)]() mutable {
						self->complete(record, std::move(mutationSubmission));
					});
			}
		}

		[[nodiscard]] auto queue_effect(
			const std::shared_ptr<request_record>& aRecord,
			const review_mutation_execution_grant& aGrant,
			const review_mutation_execution_scope& aScope)
			-> std::optional<review_mutation_failure>
		{
			auto refused = std::deque<std::shared_ptr<request_record>>{ };
			{
				std::scoped_lock lock{ Mutex };
				if (Closing || Outstanding.size() >= maximum_outstanding_review_mutations) {
					return review_mutation_failure::unavailable;
				}
				if (aRecord->Stop.stop_requested() || aGrant == nullptr
					|| !aGrant->bind(aScope) || !aGrant->permits(aScope, aRecord->Request)) {
					return review_mutation_failure::unavailable;
				}
				for (auto& entry : Outstanding) {
					auto& outstanding = entry.second;
					if (!outstanding->Refusal && !outstanding->Superseded) {
						outstanding->Superseded = true;
						static_cast<void>(outstanding->Stop.request_stop());
						++Statistics.Superseded;
					}
				}
				Outstanding.emplace(aRecord->Context.Request.value(), aRecord);
				Active = aRecord;
				EffectQueue.push_back(aRecord);
				++Statistics.EffectsQueued;
				if (DrainScheduled) {
					return std::nullopt;
				}
				DrainScheduled = true;
				auto self      = shared_from_this();
				try {
					Effects.post([self] { self->drain_effects(); });
					return std::nullopt;
				} catch (...) {
					DrainScheduled = false;
					refused.swap(EffectQueue);
				}
			}
			for (const auto& record : refused) {
				schedule_terminal(
					record, review_mutation_failure::unavailable);
			}
			return std::nullopt;
		}

		void retain_refusal(
			const std::shared_ptr<request_record>& aRecord,
			const review_mutation_failure          aFailure)
		{
			{
				std::scoped_lock lock{ Mutex };
				++Statistics.RefusedBeforeEffect;
				if (Closing || Outstanding.size() >= maximum_outstanding_review_mutations) {
					return;
				}
				aRecord->Refusal = true;
				Outstanding.emplace(aRecord->Context.Request.value(), aRecord);
			}
			schedule_terminal(aRecord, aFailure);
		}

		[[nodiscard]] auto resolve(review_thread_resolution_publication_request aRequest,
								   review_mutation_execution_grant              aRequestGrant)
			-> review_mutation_request_context
		{
			auto record = std::make_shared<resolution_record>(resolution_record{
				.Context = context(),
				.Request = std::move(aRequest),
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
				grant = aRequestGrant ? std::move(aRequestGrant) : Grant;
			}
			if (record->Request.Thread.empty() || record->Request.Thread.size() > 256U) {
				refuse_resolution(record, review_mutation_failure::undesignated_target);
				return record->Context;
			}
			if (!Coordinator.capabilities().ResolvesThreads) {
				refuse_resolution(record, review_mutation_failure::unsupported);
				return record->Context;
			}
			if (!valid(TargetBinding) || !valid(ActingIdentityBinding)) {
				refuse_resolution(record, review_mutation_failure::undesignated_target);
				return record->Context;
			}
			const auto scope = review_mutation_thread_scope{
				.ApplicationSession  = Session,
				.WorkbenchGeneration = Generation,
				.Target              = TargetBinding,
				.ActingIdentity      = ActingIdentityBinding,
				.Thread              = record->Request.Thread,
			};
			{
				std::scoped_lock lock{ Mutex };
				if (Closing || OutstandingResolutions.size() >= maximum_outstanding_review_mutations
					|| grant == nullptr || !grant->bind_thread(scope)
					|| !grant->permits_thread(scope, record->Request)) {
					++Statistics.RefusedBeforeEffect;
				} else {
					OutstandingResolutions.emplace(record->Context.Request.value(), record);
					ResolutionQueue.push_back(record);
					++Statistics.EffectsQueued;
					auto self = shared_from_this();
					try {
						Effects.post([self] { self->drain_resolutions(); });
						return record->Context;
					} catch (...) {
						OutstandingResolutions.erase(record->Context.Request.value());
						ResolutionQueue.pop_back();
					}
				}
			}
			refuse_resolution(record, review_mutation_failure::unavailable);
			return record->Context;
		}
		void refuse_resolution(const std::shared_ptr<resolution_record>& aRecord,
							   const review_mutation_failure             aFailure)
		{
			auto self = shared_from_this();
			try {
				DecisionLane.post([self, aRecord, aFailure] {
					self->complete_resolution(aRecord, std::unexpected(aFailure));
				});
			} catch (...) {
				complete_resolution(aRecord, std::unexpected(aFailure));
			}
		}
		void drain_resolutions()
		{
			for (;;) {
				auto record = std::shared_ptr<resolution_record>{ };
				{
					std::scoped_lock lock{ Mutex };
					if (ResolutionQueue.empty()) {
						return;
					}
					record = std::move(ResolutionQueue.front());
					ResolutionQueue.pop_front();
				}
				auto result = std::expected<review_thread_resolution_receipt, review_mutation_failure>{
					std::unexpected(review_mutation_failure::cancelled)
				};
				if (!record->Stop.stop_requested()) {
					try {
						result = Coordinator.resolve_thread(
							record->Request.Thread, record->Request.Resolve, record->Stop.get_token());
					} catch (...) {
						result = std::unexpected(review_mutation_failure::unavailable);
					}
				}
				auto self = shared_from_this();
				DecisionLane.post([self, record, answer = std::move(result)]() mutable {
					self->complete_resolution(record, std::move(answer));
				});
			}
		}
		void complete_resolution(
			const std::shared_ptr<resolution_record>&                                aRecord,
			std::expected<review_thread_resolution_receipt, review_mutation_failure> aResult)
		{
			auto publish = false;
			{
				std::scoped_lock lock{ Mutex };
				OutstandingResolutions.erase(aRecord->Context.Request.value());
				++Statistics.Completions;
				publish = !Closing;
				if (publish) {
					++Statistics.TerminalsPublished;
					if (aResult.has_value()) {
						++Statistics.ThreadResolutions;
					}
				}
			}
			if (publish) {
				try {
					Projection.publish_resolution(review_thread_resolution_terminal{
						.Context = aRecord->Context,
						.Request = aRecord->Request,
						.Result  = std::move(aResult),
					});
				} catch (...) {
				}
			}
		}
		[[nodiscard]] auto publish(review_mutation_publication_request aRequest,
								   review_mutation_execution_grant     aRequestGrant)
			-> review_mutation_request_context
		{
			// [[BPR-0822]]. Name the nested type: GCC 15 destroys a braced
			// aggregate member twice when a later member initializer throws.
			auto record = std::make_shared<request_record>(request_record{
				.Context = context(),
				.Request = std::move(aRequest),
				.Anchor  = review_mutation_anchor_binding{ },
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
				grant = aRequestGrant ? std::move(aRequestGrant) : Grant;
			}

			auto validated = validate(record->Request);
			if (!validated.has_value()) {
				retain_refusal(record, validated.error());
				return record->Context;
			}
			record->Anchor = std::move(validated->Anchor);
			record->Source = std::move(validated->Source);

			if (const auto failure = queue_effect(
					record, grant, validated->Scope);
				failure.has_value()) {
				retain_refusal(record, *failure);
			}
			return record->Context;
		}

		void cancel(const review_mutation_request_context aContext) noexcept
		{
			std::scoped_lock lock{ Mutex };
			const auto       found = Outstanding.find(aContext.Request.value());
			if (found != Outstanding.end()
				&& found->second->Context == aContext) {
				static_cast<void>(found->second->Stop.request_stop());
				if (!found->second->Refusal && Grant != nullptr) {
					Grant->revoke();
				}
			}
		}

		void shutdown() noexcept
		{
			std::scoped_lock lock{ Mutex };
			if (Closing) {
				return;
			}
			Closing = true;
			static_cast<void>(RepairStop.request_stop());
			if (Grant != nullptr) {
				Grant->revoke();
			}
			Grant.reset();
			Active.reset();
			for (auto& entry : Outstanding) {
				auto& record = entry.second;
				if (record->Stop.request_stop()) {
					++Statistics.ShutdownRequestsStopped;
				}
			}
			for (auto& entry : OutstandingResolutions) {
				if (entry.second->Stop.request_stop()) {
					++Statistics.ShutdownRequestsStopped;
				}
			}
		}

		std::unique_ptr<review_mutation_port> OwnedProvider;
		review_mutation_target_binding        TargetBinding;
		review_mutation_acting_identity       ActingIdentityBinding;
		review_mutation_coordinator           Coordinator;
		core::application_session_id          Session;
		core::request_generation              Generation;
		review_mutation_execution_grant       Grant;
		effect_executor&                      Effects;
		decision_executor&                    DecisionLane;
		review_mutation_projection_port&      Projection;
		review_mutation_source_port*          Sources = nullptr;
		review_provider_capabilities          Capabilities;
		bool                                  CompleteLiveBinding = false;

		mutable std::mutex                                          Mutex;
		std::map<std::uint64_t, std::shared_ptr<request_record>>    Outstanding;
		std::deque<std::shared_ptr<request_record>>                 EffectQueue;
		std::deque<std::shared_ptr<resolution_record>>              ResolutionQueue;
		std::map<std::uint64_t, std::shared_ptr<resolution_record>> OutstandingResolutions;
		std::shared_ptr<request_record>                             Active;
		review_mutation_application_host_statistics                 Statistics;
		std::atomic<std::uint64_t>                                  NextRequest    = 1U;
		bool                                                        Closing        = false;
		bool                                                        DrainScheduled = false;
		bool                                                        RepairPending  = false;
		std::stop_source                                            RepairStop;
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
		review_mutation_projection_port&      aProjection,
		review_mutation_source_port*          aSources)
		: Implementation{ std::make_shared<implementation>(
			  std::move(aProvider),
			  std::move(aTarget),
			  std::move(aActingIdentity),
			  aSession,
			  aWorkbenchGeneration,
			  std::move(aExecutionGrant),
			  aEffects,
			  aDecisionLane,
			  aProjection, aSources) }
	{
	}

#if defined(BPR_APPLICATION_TESTING)
	review_mutation_application_host::review_mutation_application_host(
		review_mutation_port&              aFixtureProvider,
		review_mutation_target_binding     aTarget,
		review_mutation_acting_identity    aActingIdentity,
		const core::application_session_id aSession,
		const core::request_generation     aWorkbenchGeneration,
		review_mutation_execution_grant    aExecutionGrant,
		effect_executor&                   aEffects,
		decision_executor&                 aDecisionLane,
		review_mutation_projection_port&   aProjection,
		review_mutation_source_port*       aSources)
		: Implementation{ std::make_shared<implementation>(
			  aFixtureProvider,
			  std::move(aTarget),
			  aActingIdentity,
			  aSession,
			  aWorkbenchGeneration,
			  std::move(aExecutionGrant),
			  aEffects,
			  aDecisionLane,
			  aProjection, aSources) }
	{
	}
#endif

	review_mutation_application_host::~review_mutation_application_host()
	{
		shutdown();
	}

	auto review_mutation_application_host::publish(
		review_mutation_publication_request aRequest,
		review_mutation_execution_grant     aRequestGrant)
		-> review_mutation_request_context
	{
		return Implementation->publish(std::move(aRequest), std::move(aRequestGrant));
	}

	auto review_mutation_application_host::resolve(
		review_thread_resolution_publication_request aRequest,
		review_mutation_execution_grant              aRequestGrant)
		-> review_mutation_request_context
	{
		return Implementation->resolve(std::move(aRequest), std::move(aRequestGrant));
	}

	void review_mutation_application_host::cancel(
		const review_mutation_request_context aContext) noexcept
	{
		Implementation->cancel(aContext);
	}
	void review_mutation_application_host::repair_pending_publications()
	{
		auto self = Implementation;
		{
			std::scoped_lock lock{ self->Mutex };
			if (self->Closing || self->RepairPending || !self->OwnedProvider) {
				return;
			}
			self->RepairPending = true;
		}
		try {
			self->Effects.post([self] {
				if (!self->RepairStop.stop_requested()) {
					self->OwnedProvider->repair_pending_publications(self->RepairStop.get_token());
				}
				std::scoped_lock lock{ self->Mutex };
				self->RepairPending = false;
			});
		} catch (...) {
			std::scoped_lock lock{ self->Mutex };
			self->RepairPending = false;
		}
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
			&& !Implementation->DrainScheduled
			&& !Implementation->RepairPending;
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
