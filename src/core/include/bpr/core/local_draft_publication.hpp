#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_PUBLICATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_PUBLICATION_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0111 ([[BPR-2029]], [[BPR-2030]]). Creating a GitHub draft
/// pull request from a local draft's candidate, after the author pushed it.
/// \details PUBLISH-01 as settled on 2026-11-10: the author pushes with their
/// own Git; BPR verifies that the hosted branch's head is the candidate and
/// creates the pull request. When the branch is missing or elsewhere, the
/// push to the resolved hosted repository is named and nothing is pushed.
/// A plan binds the candidate, destination, account and the public content
/// (title, body, selected evidence, the tour follow-up) into one digest; only
/// the author's confirmation of that digest under the plan's account
/// authorizes it. The journal records the creation before it is attempted
/// and what was observed after. A lost answer is uncertain and recovered only
/// by reading; creation is never retried blindly, and a pull request that
/// already exists is associated only on the author's explicit choice.
/// Nothing here is undone remotely.
namespace bpr::core {
	struct local_draft;
	struct local_draft_candidate;
	struct local_draft_id;

	// --- The hosted branch ------------------------------------------------------

	/// \brief How the hosted branch relates to the candidate.
	enum class hosted_branch_state : std::uint8_t {
		/// The branch's head is the candidate's commit: a pull request can be created.
		matches,
		/// No such branch in the hosted repository.
		missing,
		/// The branch is at another commit.
		differs,
		/// The candidate holds working files: they are not committed, so no
		/// pushed branch can be the candidate. The author commits them and
		/// refreshes; BPR does not stage or commit.
		uncommitted,
		/// The branch could not be read (signed out, or GitHub did not answer).
		unobserved,
	};
	[[nodiscard]] auto name(hosted_branch_state aState) noexcept -> std::string_view;

	[[nodiscard]] auto decide_hosted_branch(const local_draft_candidate& aCandidate, bool aObserved, const std::optional<git_commit_oid>& aHead) -> hosted_branch_state;

	/// \brief The push the author runs: the exact candidate commit to the
	/// draft's branch in the resolved hosted repository, run in the worktree so
	/// its hooks and the author's credentials apply. Never a bare `origin`,
	/// which in an isolated local clone names another clone.
	struct named_push
	{
		/// The worktree the push runs in.
		std::string Directory;
		/// `https://HOST/OWNER/NAME.git`.
		std::string Repository;
		/// The command, with the directory quoted for a POSIX shell.
		std::string Command;
		auto        operator<=>(const named_push&) const = default;
	};
	[[nodiscard]] auto name_push(const local_draft& aDraft, const local_draft_candidate& aCandidate) -> named_push;

	// --- The plan ------------------------------------------------------------------

	/// \brief Where the pull request goes and as whom.
	struct local_publication_destination
	{
		/// The canonical host name of the account's GitHub host.
		std::string       Host;
		github_account_id Account;
		std::string       Login;
		std::string       Owner;
		std::string       Repository;
		/// The branch the pull request merges into.
		std::string BaseBranch;
		/// The draft's branch, in the same repository.
		std::string HeadBranch;
		auto        operator<=>(const local_publication_destination&) const = default;
	};

	/// \brief One finite creation: exactly what goes public, where, as whom.
	struct local_publication_plan
	{
		/// The local draft's identity.
		std::string   DraftId;
		std::uint64_t Candidate = 0;
		/// The candidate's commit, which is the hosted branch's head.
		git_commit_oid Head;
		/// The comparison base and the target tip as the candidate was captured.
		git_commit_oid Base;
		git_commit_oid TargetTip;
		/// The target branch's tip on GitHub when the plan was made; it may
		/// have moved since the capture, and GitHub compares against it.
		std::optional<git_commit_oid> ObservedTargetTip;
		local_publication_destination Destination;
		std::string                   Title;
		std::string                   Body;
		bool                          Draft = true;
		/// Evidence records whose text may go public, by identity.
		std::vector<std::uint64_t> Evidence;
		/// Selected results that do not speak for this candidate (produced on
		/// an earlier candidate whose inputs changed, or unbound).
		std::vector<std::uint64_t> Stale;
		/// What stays on this computer: unselected evidence records, notes and
		/// agent conversations.
		std::size_t PrivateRecords       = 0;
		std::size_t PrivateNotes         = 0;
		std::size_t PrivateConversations = 0;
		/// The tour is published afterwards, as its own explicitly chosen follow-up.
		bool TourFollowUp = false;
		/// Stops not Accepted: listed, they do not block creating the draft
		/// pull request, but block the tour's publication (X20).
		std::vector<std::uint64_t> Unaccepted;
		std::size_t                Stops = 0;
		auto                       operator<=>(const local_publication_plan&) const = default;
	};

	/// \brief What GitHub showed for a plan, read just before it is made.
	struct local_publication_observation
	{
		/// The account the publication would act as.
		std::string       Host;
		github_account_id Account;
		std::string       Login;
		/// The hosted branch was read, and its head if it exists.
		bool                          BranchObserved = false;
		std::optional<git_commit_oid> BranchHead;
		std::optional<git_commit_oid> TargetTip;
		bool                          CanPush  = true;
		bool                          Archived = false;
		/// An open pull request from the draft's branch into the target branch.
		std::optional<std::uint64_t> Existing;
	};

	enum class local_publication_refusal : std::uint8_t {
		/// The draft has no candidate.
		no_candidate,
		/// No account can create pull requests here, or its host is not the target's.
		no_account,
		uncommitted,
		branch_missing,
		branch_differs,
		branch_unobserved,
		/// The account cannot push to the repository or it is archived.
		no_permission,
		/// A pull request from this branch already exists: associate it explicitly.
		pull_request_exists,
		/// The title is empty or a field is out of bounds.
		invalid,
		/// A creation is under way or uncertain: recover it first.
		unresolved,
		/// The draft already has a pull request.
		linked,
		/// The plan changed since it was confirmed, or the account did.
		stale_plan,
		authority_changed,
		unauthorized,
		/// The draft changed since it was read.
		stale,
	};
	[[nodiscard]] auto name(local_publication_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Makes the plan for the draft's latest candidate. The hosted
	/// branch must be the candidate; a missing or different branch is refused
	/// with the push to name.
	[[nodiscard]] auto make_local_publication(const local_draft& aDraft, const local_publication_observation& aObserved, bool aTourFollowUp)
		-> std::expected<local_publication_plan, local_publication_refusal>;

	/// \brief The plan's canonical text and its SHA-256 in lowercase hex,
	/// which a confirmation names.
	[[nodiscard]] auto encode_local_publication_plan(const local_publication_plan& aPlan) -> std::string;
	[[nodiscard]] auto local_publication_digest(const local_publication_plan& aPlan) -> std::string;

	/// \brief Everything the author confirms, in words.
	[[nodiscard]] auto render_local_publication_preview(const local_publication_plan& aPlan) -> std::string;

	// --- The journal -----------------------------------------------------------------

	enum class local_publication_phase : std::uint8_t {
		/// Recorded; nothing authorized.
		prepared,
		/// Confirmed; the creation is not yet attempted.
		authorized,
		/// The creation is recorded and being sent.
		creating,
		/// The pull request exists and the draft is associated with it.
		linked,
		/// The creation's answer was lost: read GitHub before anything else.
		uncertain,
		/// GitHub definitively refused the creation.
		refused,
		/// The author cancelled before anything was sent.
		cancelled,
	};
	[[nodiscard]] auto name(local_publication_phase aPhase) noexcept -> std::string_view;

	/// \brief One step as the author sees it (RFC-BPR-0099's presentation).
	enum class local_step_state : std::uint8_t {
		waiting,
		sending,
		sent,
		failed,
		not_sent,
	};
	[[nodiscard]] auto name(local_step_state aState) noexcept -> std::string_view;

	enum class local_follow_up : std::uint8_t {
		/// Recording the association with the draft (local).
		association,
		/// The tour, published to the pull request under RFC-BPR-0099.
		tour,
	};
	[[nodiscard]] auto name(local_follow_up aFollowUp) noexcept -> std::string_view;

	struct local_follow_up_step
	{
		local_follow_up  Kind  = local_follow_up::association;
		local_step_state State = local_step_state::waiting;
		/// Why it failed or was not sent, in the author's words.
		std::string Reason;
		auto        operator<=>(const local_follow_up_step&) const = default;
	};

	/// \brief The pull request as GitHub reported it.
	struct local_publication_link
	{
		std::uint64_t  Number = 0;
		std::string    Url;
		std::string    NodeId;
		std::string    Owner;
		std::string    Repository;
		std::string    HeadBranch;
		git_commit_oid Head;
		std::string    BaseBranch;
		std::string    Author;
		bool           Draft = false;
		/// The head or base GitHub reports is not the plan's.
		bool Drifted = false;
		/// An existing pull request the author chose to associate; BPR did not create it.
		bool Adopted                                          = false;
		auto operator<=>(const local_publication_link&) const = default;
	};

	struct local_publication_journal
	{
		std::string             Digest;
		local_publication_phase Phase = local_publication_phase::prepared;
		/// The current authorization; zero when none is current.
		std::uint64_t    Generation     = 0;
		std::uint64_t    NextGeneration = 1;
		local_step_state Create         = local_step_state::waiting;
		/// What stopped the creation, in the author's words.
		std::string                           Reason;
		std::optional<local_publication_link> Link;
		std::vector<local_follow_up_step>     FollowUps;
		auto                                  operator<=>(const local_publication_journal&) const = default;
	};

	/// \brief The draft's current publication: its plan and journal.
	struct local_draft_publication
	{
		local_publication_plan    Plan;
		local_publication_journal Journal;
		auto                      operator<=>(const local_draft_publication&) const = default;
	};
	[[nodiscard]] auto encode_local_draft_publication(const local_draft_publication& aPublication) -> std::string;
	[[nodiscard]] auto decode_local_draft_publication(std::string_view aText) -> std::optional<local_draft_publication>;

	/// \brief Records a new plan. A plan with nothing attempted is replaced;
	/// one under way, uncertain or linked refuses.
	[[nodiscard]] auto record_local_publication(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_publication_plan aPlan, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;
	/// \brief The author's confirmation of the digest they saw, under the account now signed in.
	[[nodiscard]] auto authorize_local_publication(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aDigest, github_account_id aAccount, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;
	/// \brief Records the creation before it is sent, under the current authorization.
	[[nodiscard]] auto begin_local_creation(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aGeneration, github_account_id aAccount, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;

	enum class local_creation_outcome : std::uint8_t {
		/// GitHub created it and answered with it.
		created,
		/// GitHub definitively refused.
		refused,
		/// It never left this computer.
		not_sent,
		/// It may have reached GitHub; nobody knows.
		uncertain,
	};
	/// \brief What the attempt observed. A creation carries its pull request;
	/// the draft is associated with it.
	[[nodiscard]] auto complete_local_creation(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_creation_outcome aOutcome, std::optional<local_publication_link> aLink,
											   std::string aReason, std::int64_t aNow) -> std::expected<local_draft, local_publication_refusal>;
	/// \brief After a restart: an unobserved creation becomes uncertain and no
	/// authorization survives. A draft with nothing to recover is returned unchanged.
	[[nodiscard]] auto restart_local_publication(const local_draft& aDraft) -> local_draft;
	/// \brief A read after uncertainty: the pull request found from the
	/// draft's branch, or nothing. Nothing found returns the plan to prepared,
	/// so creating it needs a new confirmation.
	[[nodiscard]] auto observe_local_creation(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::optional<local_publication_link> aFound, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;
	/// \brief The author's explicit association of a pull request that
	/// already exists from the draft's branch into its target.
	[[nodiscard]] auto adopt_local_pull_request(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_publication_link aLink, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;
	/// \brief Cancels a plan nothing was sent for. A creation under way is
	/// not undone by this; its answer decides.
	[[nodiscard]] auto cancel_local_publication(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::int64_t aNow) -> std::expected<local_draft, local_publication_refusal>;
	/// \brief A follow-up's state after an attempt.
	[[nodiscard]] auto record_follow_up(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_follow_up aKind, local_step_state aState, std::string aReason, std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>;

	/// \brief Whether a pull request GitHub reports is this plan's: its head is
	/// the draft's branch in the destination repository and its base the
	/// target branch. Its head commit and author are compared separately.
	[[nodiscard]] auto local_pull_request_matches(const local_publication_plan& aPlan, const local_publication_link& aLink) -> bool;
	/// \brief The link with `Drifted` set when its head or base is not the plan's.
	[[nodiscard]] auto settle_link(const local_publication_plan& aPlan, local_publication_link aLink) -> local_publication_link;

	/// \brief Why the tour follow-up cannot run now, or nothing when it can.
	enum class tour_follow_up_block : std::uint8_t {
		/// No pull request yet.
		not_linked,
		/// The tour was not selected.
		not_selected,
		/// The pull request's head is not the candidate: stops are not mapped by similarity.
		drifted,
		/// Stops not Accepted (X20).
		unaccepted,
		/// No stop to publish.
		empty,
	};
	[[nodiscard]] auto name(tour_follow_up_block aBlock) noexcept -> std::string_view;
	[[nodiscard]] auto tour_follow_up_blocked(const local_draft& aDraft) -> std::optional<tour_follow_up_block>;
}

#endif
