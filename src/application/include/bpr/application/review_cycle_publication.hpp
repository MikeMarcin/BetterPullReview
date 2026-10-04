#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLICATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLICATION_HPP

#include "bpr/application/review_cycle_outgoing.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_publication.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief Recording, authorizing and journaling one publication
/// (RFC-BPR-0059, BPR-1174).
/// \details Preparing builds the candidate in the private store and records
/// the finite plan; nothing is authorized or sent. Only the reviewer's
/// confirmation of the plan's digest, under the account now authenticated,
/// authorizes it, and every attempted remote effect is journaled in the
/// store before it is made. After a restart every attempt without an
/// observed outcome becomes uncertain and no authorization survives, so
/// recovery reads before anything is written again.
namespace bpr::application {
	struct cycle_publication_request
	{
		cycle_candidate_request       Candidate;
		core::publication_destination Destination;
		std::string                   PullTitle;
		std::string                   PullBody;
		bool                          Draft = false;
	};

	struct cycle_publication_preparation
	{
		cycle_candidate_outcome                  Built;
		std::optional<core::publication_refusal> Refusal;
		std::optional<cycle_store_error>         Store;
		std::optional<stored_cycle_publication>  Publication;
		/// Everything a confirmation authorizes, ending with the plan digest.
		std::string Preview;
	};

	/// \brief A confirmation as the host established it: who confirmed, the
	/// digest they saw, and the account authenticated now.
	struct cycle_publication_confirmation
	{
		core::cycle_actor           Actor = core::cycle_actor::imported_content;
		core::cycle_material_digest PlanDigest;
		core::github_account_id     Account;
	};

	/// \brief A journal change and the record it produced.
	struct cycle_publication_change
	{
		stored_cycle_publication Publication;
		/// The sequence a begun effect was given.
		std::uint64_t Sequence = 0;
		bool          Replayed = false;
	};

	/// \brief After a restart: every open publication's unobserved attempts
	/// become uncertain and its authorization is spent. Nothing remote is read
	/// or written, and no catalog or workspace is needed.
	[[nodiscard]] auto recover_publications_after_restart(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<std::vector<stored_cycle_publication>>;

	/// \brief Runs on the effect lane; the outgoing host and store must outlive it.
	class review_cycle_publication_host
	{
	public:
		review_cycle_publication_host(review_cycle_outgoing_host& aOutgoing, review_cycle_store& aStore) noexcept;

		/// \brief Builds the candidate and records the plan, superseding older
		/// plans nothing was attempted for.
		auto prepare(const core::review_cycle_state& aCycle, const cycle_operation_id& aOperation, const cycle_publication_request& aRequest, std::stop_token aStop)
			-> cycle_publication_preparation;
		auto load(const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication, std::stop_token aStop) -> cycle_store_result<stored_cycle_publication>;
		/// \brief Applies one journal command to the revision the caller last
		/// read; a retried operation names that same revision and replays.
		auto change(const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation, core::cycle_publication_id aPublication, std::uint64_t aExpectedRevision,
					const core::publication_command& aCommand, std::stop_token aStop) -> cycle_store_result<cycle_publication_change>;
		/// \brief Records the reviewer's confirmation of the plan digest they saw.
		auto authorize(const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation, core::cycle_publication_id aPublication, std::uint64_t aExpectedRevision,
					   const cycle_publication_confirmation& aConfirmation, std::stop_token aStop) -> cycle_store_result<cycle_publication_change>;
		/// \brief After a restart: every open publication's unobserved attempts
		/// become uncertain and its authorization is spent. Nothing remote is
		/// read or written.
		auto recover_after_restart(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::vector<stored_cycle_publication>>;

	private:
		review_cycle_outgoing_host& Outgoing;
		review_cycle_store&         Store;
	};
}

#endif
