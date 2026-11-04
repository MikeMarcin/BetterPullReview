#ifndef BPR_CORE_DRAFT_STATE_HPP
#define BPR_CORE_DRAFT_STATE_HPP

#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1852]]. Whether a pull request's draft state may be changed
/// here, whether a change may be written, and what the window shows while
/// reads catch up with it.
///
/// \details The owner, 2026-11-03: "I can't figure out how to publish my
/// draft PR from the app. I can only barely tell that my PR is a draft from
/// the overview tab". The pull request's author is offered Ready for review
/// on an open draft and Convert to draft on an open pull request that is not
/// one. The change is decided twice: on the gesture, from what the accepted
/// read said, and on what GitHub says just before the write, so nothing is
/// sent for a pull request already in the state asked or no longer open.
/// GitHub's answer moves the window in place (`draft_state_overlay`), as a
/// resolve does ([[BPR-1841]]) and a review request does ([[BPR-1849]]).
namespace bpr::core {
	/// \brief The two changes: out of draft, and back into it.
	enum class draft_state_change : std::uint8_t {
		/// GitHub's `markPullRequestReadyForReview`.
		ready_for_review,
		/// GitHub's `convertPullRequestToDraft`.
		convert_to_draft,
	};

	[[nodiscard]] auto name(draft_state_change aChange) noexcept -> std::string_view;

	/// \brief The draft state a change leaves the pull request in.
	[[nodiscard]] constexpr auto draft_after(const draft_state_change aChange) noexcept -> bool
	{
		return aChange == draft_state_change::convert_to_draft;
	}

	/// \brief What the accepted read says of the pull request and who reads it.
	struct draft_state_facts
	{
		/// The pull request's author as the accepted read gave it.
		std::string_view Author;
		/// The signed-in account's login; empty for an anonymous read.
		std::string_view Viewer;
		/// The pull request is open: not closed and not merged.
		bool Open = false;
		/// The draft state the window shows (the read's, or the viewer's own
		/// change the read has not caught up with).
		bool Draft = false;
	};

	/// \brief The change offered: Ready for review on an open draft and
	/// Convert to draft on an open pull request, to its author alone (by
	/// `same_login`); nothing to anyone else, or on a closed pull request.
	[[nodiscard]] auto draft_state_offered(const draft_state_facts& aFacts) noexcept -> std::optional<draft_state_change>;

	/// \brief Why a change gesture is refused before anything is asked.
	enum class draft_state_refusal : std::uint8_t {
		none,
		/// The viewer is not the pull request's author, or reads anonymously.
		not_author,
		/// The pull request is closed or merged.
		not_open,
		/// The pull request is already in the state asked.
		already,
		/// GitHub, asked before the write, says this viewer may not change it.
		not_permitted,
	};

	[[nodiscard]] auto name(draft_state_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Decides a gesture from the read alone, in order: not the
	/// author, not open, already in the state asked.
	[[nodiscard]] auto decide_draft_state_change(const draft_state_facts& aFacts, draft_state_change aChange) noexcept -> draft_state_refusal;

	/// \brief What GitHub said of the pull request just before the write.
	struct draft_state_observation
	{
		/// GitHub's `isDraft`.
		bool Draft = false;
		/// GitHub's `state` is `OPEN`.
		bool Open = false;
		/// GitHub's `viewerCanUpdate`.
		bool ViewerCanUpdate = false;
	};

	/// \brief What a change does once GitHub has been asked.
	enum class draft_state_outcome : std::uint8_t {
		/// Write the change once.
		write,
		/// GitHub already has the state asked; nothing is sent.
		already,
		/// The pull request is no longer open; nothing is sent.
		not_open,
		/// GitHub does not let this viewer change the pull request.
		not_permitted,
	};

	[[nodiscard]] auto name(draft_state_outcome aOutcome) noexcept -> std::string_view;

	/// \brief Decides a change from GitHub's answer, in order: already (the
	/// answer to a lost write is settled first), not open, not permitted, write.
	[[nodiscard]] auto decide_draft_state_write(const draft_state_observation& aObservation, draft_state_change aChange) noexcept -> draft_state_outcome;

	/// \brief [[BPR-1852]]. The viewer's change of draft state that reads have
	/// not caught up with: shown at once, confirmed or withdrawn by GitHub's
	/// answer, and kept until a read agrees.
	struct draft_state_overlay
	{
		/// The change's request identity; 0 while no change is held.
		std::uint64_t Request = 0U;
		/// The draft state the change leaves the pull request in.
		bool Draft = false;
		/// GitHub answered that the pull request is in that state.
		bool Confirmed = false;
		/// How many reads since GitHub's answer disagreed. A read begun before
		/// the change can arrive after it; the second one that disagrees is
		/// believed, as `conversation_overlay` does for a resolve.
		std::uint8_t Disagreements = 0U;

		[[nodiscard]] auto held() const noexcept -> bool { return Request != 0U; }

		auto operator<=>(const draft_state_overlay&) const = default;
	};

	/// \brief Records the change a gesture asked for, pending, under
	/// `aRequest` (non-zero). A change already held is replaced.
	void begin_draft_state_change(draft_state_overlay& aOverlay, std::uint64_t aRequest, draft_state_change aChange) noexcept;

	/// \brief GitHub answered request `aRequest` with the pull request's
	/// draft state `aDraft`: confirmed when it is the state asked, withdrawn
	/// otherwise. An answer to a request the overlay does not hold changes
	/// nothing.
	void settle_draft_state_change(draft_state_overlay& aOverlay, std::uint64_t aRequest, bool aDraft) noexcept;

	/// \brief The change failed or went unanswered: the window shows the read again.
	void abandon_draft_state_change(draft_state_overlay& aOverlay, std::uint64_t aRequest) noexcept;

	/// \brief A new read arrived with draft state `aDraft`. A confirmed change
	/// it agrees with is done; one it disagrees with a second time is given
	/// up. A pending change stays.
	void observe_draft_state(draft_state_overlay& aOverlay, bool aDraft) noexcept;

	/// \brief The draft state the window shows over a read.
	struct projected_draft_state
	{
		bool Draft = false;
		/// The viewer's change is shown and GitHub has not answered yet.
		bool Pending = false;

		auto operator<=>(const projected_draft_state&) const = default;
	};

	[[nodiscard]] auto project_draft_state(bool aReadDraft, const draft_state_overlay& aOverlay) noexcept -> projected_draft_state;
} // namespace bpr::core

#endif
