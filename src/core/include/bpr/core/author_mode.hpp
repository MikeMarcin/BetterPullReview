#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AUTHOR_REVISE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AUTHOR_REVISE_HPP

/// \file
/// \brief [[BPR-2196]], RFC-BPR-0112 as the owner reshaped it on 2026-11-15
/// (design `local-draft-workflow-v2`). Author is the author's mode: whether a
/// window offers it, which mode a window opens in, the one handoff at the end
/// of Author's bar, and the author's checklist in its Workflow tab.

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_publication.hpp"
#include "bpr/core/workflow_action.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief Which of the author's three cases a window shows.
	///
	/// \details The owner, 2026-11-15: the design "works for a Local PR, an
	/// existing Draft PR that we own, and a live PR we need to make revisions
	/// to". The third is named for the author's work, never for why: "Sent
	/// back is too strong".
	enum class author_case : std::uint8_t {
		/// A local draft whose pull request does not exist yet.
		local_draft,
		/// A draft pull request the viewer opened.
		draft_pull_request,
		/// An open pull request the viewer opened, being revised.
		open_pull_request,
	};
	[[nodiscard]] auto stable_name(author_case aCase) noexcept -> std::string_view;

	/// \brief What a window knows when it decides whether Author is offered.
	struct author_facts
	{
		/// A local draft shows: always the author's own worktree.
		bool LocalDraft = false;
		/// The local draft's pull request exists (it is linked or associated).
		bool Linked = false;
		/// A pull request read from GitHub shows.
		bool PullRequest = false;
		/// The pull request's author and the signed-in viewer, by login.
		std::string_view Author;
		std::string_view Viewer;
		/// The pull request is open: not closed and not merged.
		bool Open = false;
		/// The pull request is a draft. A linked local draft that has no read
		/// says true: Create Draft PR makes a draft.
		bool Draft = true;
	};

	/// \brief The author's case, or none: Author is absent.
	///
	/// \details A local draft is always its author's. A pull request is the
	/// viewer's when its author is the signed-in login (`same_login`) and it
	/// is open; anyone else's pull request, an anonymous read and a closed
	/// one offer no Author.
	[[nodiscard]] auto author_case_of(const author_facts& aFacts) noexcept -> std::optional<author_case>;

	/// \brief The window's five modes, in the switcher's order (their keys'
	/// order: Command-1 to Command-5).
	enum class author_mode : std::uint8_t {
		overview,
		review,
		discuss,
		threads,
		author,
	};
	[[nodiscard]] auto stable_name(author_mode aMode) noexcept -> std::string_view;

	/// \brief The mode a window opens a pull request or local draft in.
	///
	/// \details The owner's accepted default: a local draft and a draft pull
	/// request the viewer owns open in Author; an open pull request the viewer
	/// owns, and anyone else's, open as the window's own setting says
	/// (`aOtherwise`, Overview by default). After that the window reopens in
	/// the mode it was left in (`aLeftIn`), unless that mode is Author and
	/// Author is no longer offered. A local draft has no Discuss or Threads
	/// until its pull request exists, so a draft left there opens in Author.
	[[nodiscard]] auto opening_mode(std::optional<author_case> aCase, std::optional<author_mode> aLeftIn, author_mode aOtherwise) noexcept -> author_mode;

	/// \brief The one handoff at the right end of Author's bar.
	enum class author_handoff : std::uint8_t {
		/// Create Draft PR…: BetterPullReview creates the pull request.
		create_draft_pull_request,
		/// Hand Off Plan…: the workflow's own publisher creates it.
		hand_off_plan,
		/// Ready for review: the draft pull request is marked ready.
		ready_for_review,
		/// Publish update…: the push (RFC-BPR-0124's replies are not built).
		publish_update,
	};
	[[nodiscard]] auto stable_name(author_handoff aHandoff) noexcept -> std::string_view;
	[[nodiscard]] auto author_handoff_of(author_case aCase, workflow_publisher aPublisher) noexcept -> author_handoff;

	/// \brief Where a checklist row stands.
	enum class checklist_section : std::uint8_t {
		/// The workflow's preparation actions.
		prepare,
		/// Getting the pull request onto GitHub, or its state there.
		publish,
		/// What runs once the pull request exists.
		follow_up,
	};

	/// \brief Who runs a row; said once per row.
	enum class checklist_runner : std::uint8_t {
		/// BetterPullReview itself (it reads GitHub, it creates the pull request).
		betterpullreview,
		/// One of the profile's agents, asked in BetterPullReview: the agent accent.
		agent,
		/// A companion program the author runs in their own terminal.
		companion,
		/// The author.
		author,
	};

	/// \brief A row's mark. Only `observed` is filled: BetterPullReview saw
	/// it itself. Everything a companion returns is `reported`, never
	/// `observed` (RFC-BPR-0112 AC09).
	enum class checklist_mark : std::uint8_t {
		/// Not run on this candidate yet.
		not_yet,
		/// It waits for something else first (the pull request, a required
		/// check, a worktree).
		waiting,
		/// Handed off or asked; no answer yet, or the companion asked a question.
		in_hand,
		/// BetterPullReview saw it done.
		observed,
		/// A companion reported it done.
		reported,
		/// A companion reported it not met, failed, stopped or uncertain.
		reported_unmet,
	};
	[[nodiscard]] auto stable_name(checklist_mark aMark) noexcept -> std::string_view;

	/// \brief What a waiting row waits for.
	enum class checklist_wait : std::uint8_t {
		none,
		/// The pull request does not exist yet.
		pull_request,
		/// A required check is not met.
		required_check,
		/// No worktree is associated with the pull request.
		worktree,
	};

	/// \brief One row of the author's checklist.
	struct checklist_row
	{
		/// The workflow action's ID, or a built-in row's own ("branch", "create", "worktree").
		std::string       Id;
		std::string       Task;
		checklist_section Section = checklist_section::prepare;
		checklist_runner  Runner  = checklist_runner::betterpullreview;
		checklist_mark    Mark    = checklist_mark::not_yet;
		checklist_wait    Wait    = checklist_wait::none;
		/// The workflow's own publisher requires it.
		bool Required = false;

		auto operator<=>(const checklist_row&) const = default;
	};

	/// \brief The author's checklist and its headline facts.
	struct author_checklist
	{
		std::vector<checklist_row> Rows;
		/// Required checks not met on this candidate.
		std::size_t RequiredUnmet = 0U;
		/// Nothing left before the handoff.
		bool Ready = false;

		auto operator<=>(const author_checklist&) const = default;
	};

	/// \brief What the checklist is decided from.
	struct checklist_facts
	{
		author_case Case = author_case::local_draft;
		/// The local draft, when the window has one (none: no worktree yet).
		const local_draft* Draft = nullptr;
		/// The hosted branch as BetterPullReview last read it.
		hosted_branch_state Branch = hosted_branch_state::unobserved;
	};

	/// \brief The author's checklist, in order: the workflow's preparation,
	/// then publishing (a local draft) or the pull request's state, then
	/// follow-up.
	[[nodiscard]] auto decide_author_checklist(const checklist_facts& aFacts) -> author_checklist;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AUTHOR_REVISE_HPP
