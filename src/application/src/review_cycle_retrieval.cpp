#include "bpr/application/review_cycle_retrieval.hpp"

#include "bpr/core/review_cycle_share.hpp"

#include <utility>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		auto load_state(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const std::stop_token& aStop) -> cycle_store_result<core::review_cycle_state>
		{
			auto cycle = aStore.load(aSubject, aStop);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (!*cycle) {
				return failed(cycle_store_failure::not_found);
			}
			return std::move(**cycle);
		}

		/// One request as a granted consumer: the stored grant, never the caller's word.
		auto consumer_decision(const core::cycle_disclosure_policy& aPolicy, const core::local_cycle_subject& aSubject, const core::review_cycle_state& aCycle,
							   const cycle_frozen_round& aRound, const std::optional<core::cycle_round_grant>& aGrant, const core::cycle_capability aCapability)
			-> core::cycle_authorization
		{
			return core::authorize(aPolicy, {
												.Actor      = core::cycle_actor::granted_consumer,
												.Capability = aCapability,
												.Subject    = aSubject,
												.Origin     = core::cycle_origin(aCycle),
												.Round      = aRound.Round.Round.Id,
												.Digest     = aRound.Round.Round.Digest,
												.Channel    = core::cycle_disclosure_channel::external_retrieval,
												.Grant      = aGrant,
											});
		}
	}

	auto effective_cycle_policy(review_cycle_store& aStore, const core::cycle_disclosure_policy& aImposed, std::stop_token aStop) -> cycle_store_result<core::cycle_disclosure_policy>
	{
		auto stored = aStore.disclosure_policy(std::move(aStop));
		if (!stored) {
			return std::unexpected(stored.error());
		}
		return core::combine_policies(*stored, aImposed);
	}

	auto grant_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const bool aReadOnly,
						   const core::cycle_disclosure_policy& aImposed, const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_grant_result>
	{
		auto cycle = load_state(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		auto round = read_cycle_round(aStore, aSubject, aRound, aStop);
		if (!round) {
			return std::unexpected(round.error());
		}
		auto policy = effective_cycle_policy(aStore, aImposed, aStop);
		if (!policy) {
			return std::unexpected(policy.error());
		}
		// Exposing a round is the reviewer's export of it through external
		// retrieval: the same decision Copy, Save and the terminal get.
		auto result     = cycle_grant_result{ };
		result.Decision = core::authorize(*policy, {
													   .Actor      = core::cycle_actor::reviewer,
													   .Capability = core::cycle_capability::export_round,
													   .Subject    = aSubject,
													   .Origin     = core::cycle_origin(*cycle),
													   .Round      = round->Round.Round.Id,
													   .Digest     = round->Round.Round.Digest,
													   .Channel    = core::cycle_disclosure_channel::external_retrieval,
												   });
		if (result.Decision != core::cycle_authorization::allowed) {
			return result;
		}
		const auto grant = aReadOnly ? core::read_only_grant(aSubject, round->Round.Round) : core::response_only_grant(aSubject, round->Round.Round);
		auto       saved = aStore.record_grant(aOperation, grant, std::move(aStop));
		if (!saved) {
			return std::unexpected(saved.error());
		}
		result.Grant   = grant;
		result.Receipt = *saved;
		return result;
	}

	auto revoke_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const cycle_operation_id& aOperation,
							std::stop_token aStop) -> cycle_store_result<cycle_grant_result>
	{
		auto stored = aStore.load_grant(aSubject, aRound, aStop);
		if (!stored) {
			return std::unexpected(stored.error());
		}
		auto result = cycle_grant_result{ };
		if (!*stored) {
			return result;
		}
		auto revoked    = **stored;
		revoked.Revoked = true;
		auto saved      = aStore.record_grant(aOperation, revoked, std::move(aStop));
		if (!saved) {
			return std::unexpected(saved.error());
		}
		result.Grant   = revoked;
		result.Receipt = *saved;
		return result;
	}

	auto read_granted_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const core::cycle_disclosure_policy& aImposed,
							std::stop_token aStop) -> cycle_store_result<cycle_granted_round>
	{
		auto cycle = load_state(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		auto grant = aStore.load_grant(aSubject, aRound, aStop);
		if (!grant) {
			return std::unexpected(grant.error());
		}
		auto policy = effective_cycle_policy(aStore, aImposed, aStop);
		if (!policy) {
			return std::unexpected(policy.error());
		}
		auto round = read_cycle_round(aStore, aSubject, aRound, std::move(aStop));
		if (!round) {
			return std::unexpected(round.error());
		}
		auto result     = cycle_granted_round{ };
		result.Decision = consumer_decision(*policy, aSubject, *cycle, *round, *grant, core::cycle_capability::read_round);
		if (result.Decision == core::cycle_authorization::allowed) {
			result.Round = std::move(*round);
		}
		return result;
	}

	auto append_granted_response(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const cycle_response_file_reader& aRead,
								 const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_granted_response>
	{
		auto cycle = load_state(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		auto grant = aStore.load_grant(aSubject, aRound, aStop);
		if (!grant) {
			return std::unexpected(grant.error());
		}
		auto round = read_cycle_round(aStore, aSubject, aRound, aStop);
		if (!round) {
			return std::unexpected(round.error());
		}
		auto result     = cycle_granted_response{ };
		result.Decision = consumer_decision({ }, aSubject, *cycle, *round, *grant, core::cycle_capability::append_response);
		if (result.Decision != core::cycle_authorization::allowed) {
			return result;
		}
		auto document = aRead(aStop);
		if (aStop.stop_requested()) {
			return failed(cycle_store_failure::cancelled);
		}
		if (!document) {
			result.Result = cycle_response_result{ .Read = document.error() };
			return result;
		}
		auto appended = import_cycle_response(aStore, aSubject, aRound, *document, aOperation, std::move(aStop), core::cycle_response_channel::local_interface);
		if (!appended) {
			return std::unexpected(appended.error());
		}
		result.Result = std::move(*appended);
		return result;
	}
}
