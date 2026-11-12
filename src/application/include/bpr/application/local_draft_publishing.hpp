#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_PUBLISHING_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_PUBLISHING_HPP

#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/local_draft_store.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_publication.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>

/// \file
/// \brief RFC-BPR-0111 ([[BPR-2029]], [[BPR-2030]]): the effect-lane steps of
/// Create draft PR… for a local draft. Each reads GitHub through the
/// account's pull request catalog and writes the draft's journal through its
/// store; the core decides every transition.
/// \details Check reads the repository, the draft's hosted branch, the
/// target's tip and any pull request already open from the branch, then
/// records the plan. Create confirms the plan's digest, reads the branch
/// again, records the creation, sends it once and records what came back; a
/// lost answer is read for at once and otherwise left uncertain. Recover only
/// reads. Associate reads exactly the pull request the author chose. Nothing
/// here pushes, force-updates, merges, closes or rolls back anything.
namespace bpr::application {
	/// \brief What one step observed and the draft as stored afterwards.
	struct local_publication_step
	{
		/// The draft as stored after the step (or as last read).
		std::optional<core::local_draft> Draft;
		core::hosted_branch_state        Branch = core::hosted_branch_state::unobserved;
		/// The push to run when the hosted branch is not the candidate.
		std::optional<core::named_push>                Push;
		std::optional<core::local_publication_refusal> Refusal;
		/// An open pull request from the draft's branch into its target: the
		/// author may associate it, never duplicate it.
		std::optional<core::local_publication_link> Existing;
		std::optional<github_publication_error>     Provider;
		std::optional<local_draft_store_error>      Store;
	};

	/// \brief The link a provider observation describes.
	[[nodiscard]] auto local_link_of(const github_pull_request_observation& aPull) -> core::local_publication_link;

	/// \brief Reads GitHub and records the plan for the draft's latest candidate.
	/// \param aAccount Absent signed out: the push is still named.
	[[nodiscard]] auto check_local_publication(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account* aAccount, bool aTour,
											   std::int64_t aNow, std::stop_token aStop) -> local_publication_step;

	/// \brief The author's confirmation of `aDigest`, then the one creation.
	[[nodiscard]] auto create_local_pull_request(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, const std::string& aDigest,
												 std::int64_t aNow, std::stop_token aStop) -> local_publication_step;

	/// \brief After a lost answer: reads the branch's pull requests and
	/// records what is there. Writes nothing to GitHub.
	[[nodiscard]] auto recover_local_publication(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, std::int64_t aNow,
												 std::stop_token aStop) -> local_publication_step;

	/// \brief Associates the pull request numbered `aNumber`, read exactly, when it is from the draft's branch into its target.
	[[nodiscard]] auto associate_local_pull_request(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, std::uint64_t aNumber,
													std::int64_t aNow, std::stop_token aStop) -> local_publication_step;
}

#endif
