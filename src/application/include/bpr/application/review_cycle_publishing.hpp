#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLISHING_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLISHING_HPP

#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/publication_candidate_port.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_outgoing.hpp"
#include "bpr/application/review_cycle_publication.hpp"
#include "bpr/application/review_cycle_publisher.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/review_cycle_association.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Publish as Pull Request from the desktop (RFC-BPR-0059 and
/// RFC-BPR-0061, BPR-1177): the steps the cycle surface takes, each one
/// effect-lane call over the cycle's store, composed from the hosts the
/// publication milestone built — outgoing planning, the private candidate,
/// the finite plan and its journal, the publisher — and the association of
/// the pull request that resulted.
/// \details Nothing here decides: the core plans, authorizes and admits.
/// Preparing builds the candidate in the private store and records a plan;
/// only publishing, with the reviewer's confirmation of that plan's digest
/// under the account authenticated now, sends anything, and then only the
/// plan's objects, branch and pull request. Linking an externally published
/// pull request reads exactly that pull request and records what was read.
/// No step uploads a note, round, response or verification, and none writes
/// a GitHub comment or review.
namespace bpr::application {
	/// \brief The account native publication acts as, as the composition sees it now.
	using review_cycle_publication_account = github_publication_account;

	/// \brief What Publish as Pull Request may use, set by the composition.
	struct review_cycle_publication_environment
	{
		/// Makes the local publication catalog, on the effect lane, at first use.
		std::function<publication_git_result<std::unique_ptr<publication_candidate_port>>(std::stop_token)> Candidates;
		/// The account native publication would act as, asked each time it is needed.
		std::function<std::optional<review_cycle_publication_account>()> Account;
		/// Now, as seconds since the epoch and minutes east of UTC, for the
		/// commit's author and committer.
		std::function<core::git_signature()> Now;
	};

	/// \brief The local catalog, made once on the effect lane and kept.
	class review_cycle_candidate_catalog
	{
	public:
		explicit review_cycle_candidate_catalog(std::function<publication_git_result<std::unique_ptr<publication_candidate_port>>(std::stop_token)> aMake);
		/// \brief The catalog, made now if it was not; effect lane only.
		auto get(std::stop_token aStop) -> publication_git_result<publication_candidate_port*>;

	private:
		std::function<publication_git_result<std::unique_ptr<publication_candidate_port>>(std::stop_token)> Make;
		std::unique_ptr<publication_candidate_port>                                                         Catalog;
	};

	/// \brief What one publication step observed. Every step ends by reading
	/// the cycle's publications and associations again.
	struct review_cycle_publication_step
	{
		std::optional<cycle_outgoing_outcome>        Planned;
		std::optional<cycle_publication_preparation> Prepared;
		std::optional<cycle_publish_outcome>         Published;
		std::optional<cycle_external_outcome>        External;
		/// The local catalog could not be made.
		std::optional<publication_git_error> Catalog;
		/// A provider read for a link failed.
		std::optional<github_publication_error> Provider;
		std::optional<core::cycle_link_refusal> LinkRefusal;
		/// The association a link recorded, or found already recorded.
		std::optional<core::cycle_pull_association> Linked;
		/// A patch saved for one's own Git tools, or why it was not.
		std::optional<cycle_round_save>     PatchSave;
		std::optional<cycle_export_failure> PatchFailure;
		/// The cycle's publications, newest last.
		std::vector<stored_cycle_publication> Publications;
		/// Every association: linked native publications, then external links.
		std::vector<core::cycle_pull_association> Links;
	};

	/// \brief Reads every publication and association of the cycle; with
	/// `aRecover`, first turns every unobserved attempt after a restart into
	/// an uncertain one (`review_cycle_publication_host::recover_after_restart`).
	[[nodiscard]] auto read_publications(review_cycle_store& aStore, const core::review_cycle_state& aCycle, bool aRecover, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>;

	/// \brief Plans what would go out. Writes nothing anywhere.
	[[nodiscard]] auto plan_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
										const cycle_outgoing_request& aRequest, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>;

	/// \brief Builds the candidate in the private store and records the plan.
	[[nodiscard]] auto prepare_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
										   const cycle_operation_id& aOperation, const cycle_publication_request& aRequest, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>;

	/// \brief The reviewer's confirmation of the digest they saw, then every
	/// remaining step under it.
	struct review_cycle_publish_request
	{
		core::cycle_publication_id  Publication;
		std::uint64_t               ExpectedRevision = 0;
		core::cycle_material_digest Digest;
		cycle_operation_id          Operation;
	};
	[[nodiscard]] auto publish_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
										   const review_cycle_publication_account& aAccount, const review_cycle_publish_request& aRequest, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>;

	/// \brief Reads the provider for every uncertain effect. Writes nothing remote.
	[[nodiscard]] auto recover_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
										   const review_cycle_publication_account& aAccount, core::cycle_publication_id aPublication, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>;

	/// \brief Cancels a publication. Confirmed remote effects remain, and are said to.
	[[nodiscard]] auto cancel_publication(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const cycle_operation_id& aOperation, core::cycle_publication_id aPublication,
										  std::uint64_t aExpectedRevision, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>;

	/// \brief External preparation: the selection as a binary patch from P,
	/// written to the chosen destination, for the reviewer's own Git tools.
	[[nodiscard]] auto save_publication_patch(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
											  const cycle_outgoing_request& aRequest, cycle_export_destination& aDestination, const std::string& aName, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>;

	/// \brief Links a pull request the reviewer published with their own
	/// tools: reads exactly `aAddress` under `aAccount`, lets the core admit
	/// it, and records it as externally published.
	[[nodiscard]] auto link_pull_request(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const review_cycle_publication_account& aAccount, const cycle_operation_id& aOperation,
										 const core::cycle_pull_address& aAddress, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>;
}

#endif
