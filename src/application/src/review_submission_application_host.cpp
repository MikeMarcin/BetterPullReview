#include "bpr/application/review_submission_application_host.hpp"

#include "bpr/application/review_submission_recovery_port.hpp"
#include "review_submission_execution_authority.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

namespace bpr::application {
	review_submission_execution_authority::review_submission_execution_authority(
		core::application_session_id aSession, core::request_generation aGeneration,
		review_submission_request aRequest, std::chrono::steady_clock::time_point aExpiresAt)
		: Session{ aSession }
		, Generation{ aGeneration }
		, Request{ std::move(aRequest) }
		, ExpiresAt{ aExpiresAt }
	{ }
	void review_submission_execution_authority::revoke() noexcept { Revoked.store(true); }
	auto review_submission_execution_authority::active() const noexcept -> bool
	{
		return !Revoked.load() && std::chrono::steady_clock::now() < ExpiresAt;
	}
	auto review_submission_execution_authority::consume(core::application_session_id aSession,
														core::request_generation aGeneration, const review_submission_request& aRequest) -> bool
	{
		// Correlation precedes any summary comparison or validation.
		if (Session != aSession || Generation != aGeneration
			|| Request.Proposal.Draft != aRequest.Proposal.Draft
			|| Request.Proposal.Attempt != aRequest.Proposal.Attempt || !active()
			|| Plan || Request != aRequest || !valid(aRequest) || !aRequest.LiveBinding) {
			return false;
		}
		bool expected = false;
		return Consumed.compare_exchange_strong(expected, true);
	}
	auto detail::review_submission_user_authority_issuer::issue(core::application_session_id aSession,
																core::request_generation aGeneration, const review_submission_request& aRequest) -> review_submission_execution_grant
	{
#if defined(BPR_APPLICATION_TESTING)
		static_cast<void>(aSession);
		static_cast<void>(aGeneration);
		static_cast<void>(aRequest);
		return { };
#else
		if (aSession.value() == 0U || aGeneration.value() == 0U || !valid(aRequest) || !aRequest.LiveBinding) {
			return { };
		}
		return review_submission_execution_grant{ new review_submission_execution_authority{
			aSession, aGeneration, aRequest, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 } } };
#endif
	}
#if defined(BPR_APPLICATION_TESTING)
	auto make_review_submission_testing_grant(core::application_session_id aSession,
											  core::request_generation aGeneration, const review_submission_request& aRequest,
											  std::chrono::steady_clock::time_point aExpiresAt) -> review_submission_execution_grant
	{
		if (aSession.value() == 0U || aGeneration.value() == 0U || !valid(aRequest) || !aRequest.LiveBinding) {
			return { };
		}
		return review_submission_execution_grant{ new review_submission_execution_authority{ aSession, aGeneration, aRequest, aExpiresAt } };
	}
#endif

	auto review_submission_execution_authority::consume(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan) -> bool
	{
		if (Session != aSession || Generation != aGeneration || !Plan || *Plan != aPlan || !valid(aPlan) || !active()) {
			return false;
		}
		bool expected = false;
		return Consumed.compare_exchange_strong(expected, true);
	}
	auto review_submission_execution_authority::consume_child(const staged_publication_record& aRecord) -> bool
	{
		if (!Consumed.load() || !active() || !Plan || *Plan != aRecord.Plan || !valid(aRecord) || aRecord.PossiblyTransmitted || !aRecord.Pending || aRecord.Pending != staged_next_request(aRecord)) {
			return false;
		}
		auto last = LastChild.load();
		return aRecord.Revision > last && LastChild.compare_exchange_strong(last, aRecord.Revision);
	}
	auto detail::review_submission_user_authority_issuer::issue(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan) -> review_submission_execution_grant
	{
#if defined(BPR_APPLICATION_TESTING)
		static_cast<void>(aSession);
		static_cast<void>(aGeneration);
		static_cast<void>(aPlan);
		return { };
#else
		if (!valid(aPlan)) {
			return { };
		}
		auto grant = issue(aSession, aGeneration, aPlan.Submission);
		if (grant) {
			grant->Plan = aPlan;
		}
		return grant;
#endif
	}
#if defined(BPR_APPLICATION_TESTING)
	auto make_review_submission_testing_grant(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan, std::chrono::steady_clock::time_point aExpiresAt) -> review_submission_execution_grant
	{
		if (!valid(aPlan)) {
			return { };
		}
		auto grant = make_review_submission_testing_grant(aSession, aGeneration, aPlan.Submission, aExpiresAt);
		if (grant) {
			grant->Plan = aPlan;
		}
		return grant;
	}
#endif

	struct review_submission_application_host::implementation final : std::enable_shared_from_this<implementation>
	{
		enum class operation { discover_conversation_replies,
							   save_conversation_reply,
							   discover,
							   submit,
							   reconcile,
							   discover_tours,
							   publish_tour,
							   reconcile_tour,
							   lookup };
		struct record
		{
			review_submission_host_context           Context;
			operation                                Operation = operation::discover;
			std::optional<review_submission_request> Request;
			std::optional<staged_review_plan>        Plan;
			review_submission_execution_grant        Grant;
			std::stop_source                         Stop;
			std::string                              Key;
		};
		review_submission_provider_factory Factory;
		review_submission_live_binding     Binding;
		core::application_session_id       Session;
		core::request_generation           Generation;
		effect_executor&                   Effects;
		decision_executor&                 Decisions;
		review_submission_projection_port& Projection;
		mutable std::mutex                 Mutex;
		std::shared_ptr<record>            Active;
		std::uint64_t                      Next    = 0U;
		bool                               Closing = false;

		implementation(review_submission_provider_factory aFactory, review_submission_live_binding aBinding,
					   core::application_session_id aSession, core::request_generation aGeneration,
					   effect_executor& aEffects, decision_executor& aDecisions, review_submission_projection_port& aProjection)
			: Factory{ std::move(aFactory) }
			, Binding{ std::move(aBinding) }
			, Session{ aSession }
			, Generation{ aGeneration }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Projection{ aProjection }
		{ }

		static auto refused(const record& aRecord, review_mutation_failure aFailure, bool aBeforeEffect) -> review_submission_host_terminal
		{
			if (aRecord.Operation == operation::discover_tours || aRecord.Operation == operation::discover_conversation_replies) {
				return { aRecord.Context, staged_review_discovery{ std::unexpected(aFailure) } };
			}
			if (aRecord.Plan) {
				return { aRecord.Context, staged_publication_result{ } };
			}
			if (aRecord.Operation == operation::discover) {
				return { aRecord.Context, review_submission_discovery{ std::unexpected(aFailure) } };
			}
			if (aRecord.Operation == operation::lookup) {
				return { aRecord.Context, review_submission_lookup{ .Key = aRecord.Key, .Request = std::unexpected(aFailure) } };
			}
			return { aRecord.Context, review_submission_result{ .Request = *aRecord.Request, .Publication = aBeforeEffect && aRecord.Operation == operation::submit ? review_submission_publication::definitely_not_published : review_submission_publication::indeterminate, .Failure = aFailure } };
		}
		void complete(const std::shared_ptr<record>& aRecord, review_submission_host_terminal aTerminal)
		{
			{
				std::scoped_lock lock{ Mutex };
				if (Active != aRecord || aTerminal.Context != aRecord->Context) {
					return;
				}
				Active.reset();
			}
			// Accepted work still terminates during shutdown. The owner keeps
			// the projection alive and rejects retired session/generation first.
			try {
				Projection.publish(aTerminal);
			} catch (...) { }
		}
		void terminal(const std::shared_ptr<record>& aRecord, review_submission_host_terminal aTerminal)
		{
			auto self = shared_from_this();
			Decisions.post([self, aRecord, terminal = std::move(aTerminal)]() mutable {
				self->complete(aRecord, std::move(terminal));
			});
		}
		auto perform(const std::shared_ptr<record>& aRecord) -> review_submission_host_terminal
		{
			if (aRecord->Stop.stop_requested() || (aRecord->Grant && !aRecord->Grant->active())) {
				return refused(*aRecord, review_mutation_failure::cancelled, true);
			}
			// Each operation opens and closes its provider on this effect lane.
			// Recovery is durable; no storage destructor runs on the UI lane.
			auto created = Factory();
			if (!created || !*created) {
				return refused(*aRecord, review_mutation_failure::unavailable, true);
			}
			auto provider = std::move(*created);
			if (aRecord->Stop.stop_requested() || (aRecord->Grant && !aRecord->Grant->active())) {
				return refused(*aRecord, review_mutation_failure::cancelled, true);
			}
			if (aRecord->Operation == operation::discover_tours || aRecord->Operation == operation::discover_conversation_replies) {
				auto pending = aRecord->Operation == operation::discover_tours ? provider->pending_tours(aRecord->Stop.get_token()) : provider->conversation_replies();
				if (pending && (pending->size() > maximum_review_mutation_recovery_records || std::ranges::any_of(*pending, [&](const auto& aP) { return !valid(aP) || !same_review_submission_identity(*aP.Submission.LiveBinding, Binding); }))) {
					return refused(*aRecord, review_mutation_failure::undesignated_target, true);
				}
				return { aRecord->Context, std::move(pending) };
			}
			if (aRecord->Plan) {
				auto result = aRecord->Operation == operation::save_conversation_reply ? provider->save_conversation_reply(*aRecord->Plan) : aRecord->Operation == operation::publish_tour ? provider->publish_tour(*aRecord->Plan, aRecord->Stop.get_token(), [grant = aRecord->Grant](const auto& aChild) { return grant && grant->consume_child(aChild); })
																																														   : provider->reconcile_tour(*aRecord->Plan, aRecord->Stop.get_token());
				if (result.Record && (!valid(*result.Record) || result.Record->Plan != *aRecord->Plan)) {
					return refused(*aRecord, review_mutation_failure::unavailable, false);
				}
				return { aRecord->Context, std::move(result) };
			}
			if (aRecord->Operation == operation::discover) {
				auto pending = provider->pending(aRecord->Stop.get_token());
				if (pending) {
					if (pending->size() > maximum_review_mutation_recovery_records) {
						return refused(*aRecord, review_mutation_failure::unavailable, true);
					}
					for (const auto& request : *pending) {
						if (!valid(request) || !request.LiveBinding || !same_review_submission_identity(*request.LiveBinding, Binding)) {
							return refused(*aRecord, review_mutation_failure::undesignated_target, true);
						}
					}
				}
				// [[BPR-1541]]. Each restored attempt brings the reason its
				// record keeps, so the page can say it before Check status.
				auto unknown = std::map<std::string, core::review_submission_unknown_diagnosis>{ };
				if (pending) {
					for (const auto& request : *pending) {
						if (auto kept = provider->last_unknown(request.IdempotencyKey, aRecord->Stop.get_token()); kept && core::valid(*kept)) {
							unknown.emplace(request.IdempotencyKey, *kept);
						}
					}
				}
				return { aRecord->Context, std::move(pending), std::move(unknown) };
			}
			if (aRecord->Operation == operation::lookup) {
				auto found = provider->admitted(aRecord->Key, aRecord->Stop.get_token());
				if (found && *found
					&& (!valid(**found) || (**found).IdempotencyKey != aRecord->Key || !(**found).LiveBinding || !same_review_submission_identity(*(**found).LiveBinding, Binding))) {
					return refused(*aRecord, review_mutation_failure::undesignated_target, true);
				}
				return { aRecord->Context, review_submission_lookup{ .Key = aRecord->Key, .Request = std::move(found) } };
			}
			auto        result   = aRecord->Operation == operation::submit
				? provider->submit_and_reconcile(*aRecord->Request, aRecord->Stop.get_token())
				: provider->reconcile(*aRecord->Request, aRecord->Stop.get_token());
			const auto& expected = *aRecord->Request;
			if (result.Request.Proposal.Draft != expected.Proposal.Draft
				|| result.Request.Proposal.Attempt != expected.Proposal.Attempt
				|| result.Request.Proposal.Target != expected.Proposal.Target
				|| result.Request.Proposal.ActingAccount != expected.Proposal.ActingAccount
				|| result.Request != expected) {
				return refused(*aRecord, review_mutation_failure::unavailable, false);
			}
			if ((result.Publication != review_submission_publication::published
				 && result.Publication != review_submission_publication::indeterminate
				 && result.Publication != review_submission_publication::definitely_not_published)
				|| (result.Publication == review_submission_publication::published
					&& (!result.Receipt || !valid(*result.Receipt, expected)))
				|| (result.Publication != review_submission_publication::published && result.Receipt)) {
				return refused(*aRecord, review_mutation_failure::unavailable, false);
			}
			return { aRecord->Context, std::move(result) };
		}
		auto enqueue(operation aOperation, const review_submission_request* aRequest, review_submission_execution_grant aGrant, const staged_review_plan* aPlan = nullptr, std::string aKey = { }) -> std::optional<review_submission_host_context>
		{
			std::shared_ptr<record> request;
			{
				std::scoped_lock lock{ Mutex };
				if (Closing || Active || !Factory || Session.value() == 0U || Generation.value() == 0U
					|| !valid(Binding) || Next == std::numeric_limits<std::uint64_t>::max()) {
					return std::nullopt;
				}
				if (aOperation == operation::lookup
					&& (aKey.empty() || aKey.size() > maximum_review_submission_idempotency_key_bytes
						|| !std::ranges::all_of(aKey, [](const char aValue) { return aValue > ' ' && aValue < '\x7f'; }))) {
					return std::nullopt;
				}
				if (aPlan) {
					if (!valid(*aPlan) || (aOperation == operation::publish_tour ? *aPlan->Submission.LiveBinding != Binding : !same_review_submission_identity(*aPlan->Submission.LiveBinding, Binding))) {
						return std::nullopt;
					}
					if (aOperation == operation::publish_tour && (!aGrant || !aGrant->consume(Session, Generation, *aPlan))) {
						return std::nullopt;
					}
				}
				if (aRequest) {
					if (!aRequest->LiveBinding
						|| (aOperation == operation::submit ? *aRequest->LiveBinding != Binding : !same_review_submission_identity(*aRequest->LiveBinding, Binding))) {
						return std::nullopt;
					}
					if (aOperation == operation::submit) {
						if (!aGrant || !aGrant->consume(Session, Generation, *aRequest)) {
							return std::nullopt;
						}
					} else if (!valid(*aRequest)) {
						return std::nullopt;
					}
				}
				request            = std::make_shared<record>();
				request->Context   = { core::request_id{ ++Next }, Session, Generation };
				request->Operation = aOperation;
				if (aRequest) {
					request->Request = *aRequest;
				}
				if (aPlan) {
					request->Plan = *aPlan;
				}
				request->Grant = std::move(aGrant);
				request->Key   = std::move(aKey);
				Active         = request;
			}
			auto self = shared_from_this();
			try {
				Effects.post([self, request] {
					auto outcome = refused(*request, review_mutation_failure::unavailable, false);
					try {
						outcome = self->perform(request);
					} catch (...) { }
					self->terminal(request, std::move(outcome));
				});
			} catch (...) {
				terminal(request, refused(*request, review_mutation_failure::unavailable, true));
			}
			return request->Context;
		}
	};
	review_submission_application_host::review_submission_application_host(review_submission_provider_factory aFactory,
																		   review_submission_live_binding aBinding, core::application_session_id aSession, core::request_generation aGeneration,
																		   effect_executor& aEffects, decision_executor& aDecisions, review_submission_projection_port& aProjection)
		: Implementation{ std::make_shared<implementation>(std::move(aFactory), std::move(aBinding), aSession, aGeneration, aEffects, aDecisions, aProjection) }
	{ }
	review_submission_application_host::~review_submission_application_host() { shutdown(); }
	auto review_submission_application_host::discover() -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::discover, nullptr, { }); }
	auto review_submission_application_host::submit(const review_submission_request& aRequest, review_submission_execution_grant aGrant) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::submit, &aRequest, std::move(aGrant)); }
	auto review_submission_application_host::reconcile(const review_submission_request& aRequest) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::reconcile, &aRequest, { }); }
	auto review_submission_application_host::discover_conversation_replies() -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::discover_conversation_replies, nullptr, { }); }
	auto review_submission_application_host::save_conversation_reply(const staged_review_plan& aPlan) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::save_conversation_reply, nullptr, { }, &aPlan); }
	auto review_submission_application_host::discover_tours() -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::discover_tours, nullptr, { }); }
	auto review_submission_application_host::publish_tour(const staged_review_plan& aPlan, review_submission_execution_grant aGrant) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::publish_tour, nullptr, std::move(aGrant), &aPlan); }
	auto review_submission_application_host::reconcile_tour(const staged_review_plan& aPlan) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::reconcile_tour, nullptr, { }, &aPlan); }
	auto review_submission_application_host::lookup(std::string aKey) -> std::optional<review_submission_host_context> { return Implementation->enqueue(implementation::operation::lookup, nullptr, { }, nullptr, std::move(aKey)); }
	void review_submission_application_host::cancel(review_submission_host_context aContext) noexcept
	{
		std::scoped_lock lock{ Implementation->Mutex };
		if (Implementation->Active && Implementation->Active->Context == aContext) {
			Implementation->Active->Stop.request_stop();
			if (Implementation->Active->Grant) {
				Implementation->Active->Grant->revoke();
			}
		}
	}
	void review_submission_application_host::shutdown() noexcept
	{
		std::scoped_lock lock{ Implementation->Mutex };
		Implementation->Closing = true;
		if (Implementation->Active) {
			Implementation->Active->Stop.request_stop();
			if (Implementation->Active->Grant) {
				Implementation->Active->Grant->revoke();
			}
		}
	}
	auto review_submission_application_host::is_quiescent() const noexcept -> bool
	{
		std::scoped_lock lock{ Implementation->Mutex };
		return !Implementation->Active;
	}
}
