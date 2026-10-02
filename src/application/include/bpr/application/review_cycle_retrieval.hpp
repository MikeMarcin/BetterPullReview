#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RETRIEVAL_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RETRIEVAL_HPP

#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_round.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_policy.hpp"

#include <optional>
#include <stop_token>

/// \file
/// \brief Scoped retrieval of a prepared round and response-only grants
/// (RFC-BPR-0058, [[BPR-1169]]), on the effect lane.
/// \details The reviewer exposes one frozen round to a harness by storing a
/// grant for it, which the same channel decision as every export allows or
/// refuses. A harness then reads that round, and appends a response to it,
/// only as the stored grant allows: `core::authorize` decides each request as
/// a granted consumer, never as the reviewer, and an identity a caller names
/// is not a grant. Nothing here reads a draft, a note outside the round, a
/// capture or the database on a consumer's behalf; nothing verifies,
/// publishes, freezes or amends. Every decision combines the profile's stored
/// channel policy with any policy the host imposes.
namespace bpr::application {
	/// \brief The policy every decision uses: the profile's stored policy
	/// combined with `aImposed` (`core::combine_policies`).
	[[nodiscard]] auto effective_cycle_policy(review_cycle_store& aStore, const core::cycle_disclosure_policy& aImposed, std::stop_token aStop)
		-> cycle_store_result<core::cycle_disclosure_policy>;

	/// \brief What exposing or withdrawing one round did.
	struct cycle_grant_result
	{
		/// The channel decision for external retrieval of this round.
		core::cycle_authorization Decision = core::cycle_authorization::allowed;
		/// The grant as now stored, when one is.
		std::optional<core::cycle_round_grant> Grant;
		std::optional<cycle_write_receipt>     Receipt;
	};
	/// \brief The reviewer exposes frozen round `aRound` to a harness: reading
	/// it, and appending a response unless `aReadOnly`. Refused, and nothing
	/// stored, when external retrieval is not allowed for the cycle's source.
	[[nodiscard]] auto grant_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, bool aReadOnly,
										 const core::cycle_disclosure_policy& aImposed, const cycle_operation_id& aOperation, std::stop_token aStop)
		-> cycle_store_result<cycle_grant_result>;
	/// \brief Withdraws the round's grant; it stays stored as revoked. Bytes
	/// already read cannot be recalled. Nothing when the round has no grant.
	[[nodiscard]] auto revoke_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, const cycle_operation_id& aOperation,
										  std::stop_token aStop) -> cycle_store_result<cycle_grant_result>;

	/// \brief A round as a granted consumer may see it.
	struct cycle_granted_round
	{
		/// The decision for this consumer request.
		core::cycle_authorization Decision = core::cycle_authorization::not_granted;
		/// The frozen round, verified against its digest, only when allowed.
		std::optional<cycle_frozen_round> Round;
	};
	/// \brief Reads frozen round `aRound` for a consumer holding its stored
	/// grant. A round that was never frozen is `not_found`.
	[[nodiscard]] auto read_granted_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, const core::cycle_disclosure_policy& aImposed,
										  std::stop_token aStop) -> cycle_store_result<cycle_granted_round>;

	/// \brief A granted consumer's response, appended or refused.
	struct cycle_granted_response
	{
		core::cycle_authorization            Decision = core::cycle_authorization::not_granted;
		std::optional<cycle_response_result> Result;
	};
	/// \brief For a consumer whose grant allows it, reads a structured
	/// response with `aRead` and appends it to round `aRound` as a statement
	/// received through the local interface. Nothing is read when the grant
	/// does not allow it. It changes no note, verifies nothing and publishes
	/// nothing.
	[[nodiscard]] auto append_granted_response(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, const cycle_response_file_reader& aRead,
											   const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_granted_response>;
}

#endif
