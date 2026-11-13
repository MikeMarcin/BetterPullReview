#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_TOUR_VIEW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_TOUR_VIEW_HPP

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0110 as amended by the owner ("I thought we chose to stick
/// with the PR view and just augment it"). A local draft's tour is shown and
/// edited by the pull request window's own Tour panel. The panel reads a
/// `review_tour`, a `tour_agent_session` and a `tour_proposal`; this projects
/// a local draft's preparation into those shapes, for presentation only, and
/// translates the panel's commands back into the local draft's own edits.
///
/// \details The local draft keeps its own record (`local_draft_preparation`):
/// stops bound to a candidate's exact lines, conversations scoped to the
/// draft, proposals that go stale. Nothing here is stored, published or sent
/// to GitHub. The projected tour's owner and commit identities are derived
/// from the draft's identity and a candidate's ordinal in a domain of their
/// own (as `local_presentation` derives the viewer's comparison identities):
/// they are shapes the panel needs, never a pull request, account or commit
/// the draft claims. A stop's ring comes from `decide_local_stop`; the
/// projection writes the acceptance record the panel decides from so that it
/// reaches the same state.
namespace bpr::core {
	/// \brief The commit identity the panel sees for a candidate of a draft.
	/// Domain-separated from every Git object; equal inputs, equal identity.
	[[nodiscard]] auto local_tour_commit(std::string_view aDraft, std::uint64_t aCandidate) -> git_commit_oid;
	/// \brief The tour owner the panel sees for a draft. Its account, host,
	/// repository and pull request are derived from the draft alone.
	[[nodiscard]] auto local_tour_owner(std::string_view aDraft) -> tour_owner;

	/// \brief A local conversation thread as the panel names it, and back.
	[[nodiscard]] auto local_tour_thread(std::string_view aThread) -> tour_thread;
	[[nodiscard]] auto local_thread_of(const tour_thread& aThread) -> std::string;
	/// \brief The subject a question on the panel's thread is about.
	[[nodiscard]] auto local_subject_of(const tour_thread& aThread) -> local_subject;

	/// \brief What the panel presents for a local draft.
	struct local_tour_view
	{
		/// The tour as the panel reads it; `Head` is the latest candidate.
		review_tour Tour;
		/// The conversations' turns as the side chat reads them, oldest first.
		tour_agent_session Session;
		/// Proposals still to decide (and stale ones, refused), as the panel
		/// shows proposals.
		std::optional<tour_proposal> Proposal;
		/// The local proposal behind each of `Proposal`'s items, in order.
		std::vector<std::uint64_t> ProposalItems;
		/// The latest candidate's changed paths.
		std::vector<std::string> Files;
		/// Why each yellow stop is yellow, in the local draft's words
		/// (RFC-BPR-0110 X05: "with that word as its reason").
		std::vector<std::pair<std::uint64_t, local_stop_reason>> Reasons;
		auto                                                     operator==(const local_tour_view&) const -> bool = default;
	};

	/// \brief The question running now, when one is, on its thread.
	struct local_tour_asking
	{
		std::string Thread;
		std::string Text;
	};

	/// \brief Projects `aDraft`'s preparation for the panel.
	[[nodiscard]] auto view_local_tour(const local_draft& aDraft, const std::optional<local_tour_asking>& aAsking = std::nullopt) -> local_tour_view;

	/// \brief One edit of the local draft a panel command asks for.
	enum class local_tour_edit_kind : std::uint8_t {
		/// The panel asked for something a local tour does not do (a split,
		/// a duplicate, an outline): nothing changes.
		none,
		/// Problem and Solution, saved together.
		brief,
		/// A stop's title and text, saved as typed; not accepted.
		stop_text,
		accept,
		unaccept,
		remove,
		/// To `Index` in the route.
		move,
	};
	struct local_tour_edit
	{
		local_tour_edit_kind Kind = local_tour_edit_kind::none;
		std::uint64_t        Stop = 0;
		std::string          Problem;
		std::string          Solution;
		std::string          Title;
		std::string          Text;
		std::size_t          Index                                            = 0;
		auto                 operator==(const local_tour_edit&) const -> bool = default;
	};
	/// \brief RFC-BPR-0111's tour follow-up, its private half (RFC-BPR-0110
	/// "On publication"): the draft's tour as the pull request's own private
	/// tour draft, `aTarget` (its owner, identity, base and head), mapped by
	/// `map_local_publication` against the pull request's exact result.
	/// \details The Problem and Solution come across. Every stop comes across
	/// in route order: one whose lines are exactly at the pull request's head
	/// gets that anchor, and keeps its acceptance when the author had accepted
	/// it (accepted again there, at that head); any other keeps its title and
	/// text with no lines, so it needs the author's attention and blocks the
	/// tour's publication until it has lines and is accepted. Nothing is
	/// published; the pull request's Tour publishes it (RFC-BPR-0099).
	struct local_tour_carry
	{
		review_tour               Tour;
		local_publication_mapping Mapping;
	};
	[[nodiscard]] auto carry_local_tour_to_pull_request(const local_draft& aDraft, const local_publication_result& aResult, const review_tour& aTarget) -> local_tour_carry;
	/// \brief What the window says when the pull request's tour draft refuses
	/// the carry (`aRefusal` from applying it as a restore), so a carried
	/// tour is never dropped in silence. The local draft keeps its tour. Empty
	/// for `tour_refusal::none`.
	[[nodiscard]] auto local_tour_carry_refusal_words(tour_refusal aRefusal) -> std::string;

	/// \brief The local edits a panel command stands for, in order.
	[[nodiscard]] auto translate_local_tour_command(const local_draft& aDraft, const tour_command& aCommand) -> std::vector<local_tour_edit>;
}

#endif
