#ifndef BPR_CORE_TOUR_STOP_ACCEPTANCE_HPP
#define BPR_CORE_TOUR_STOP_ACCEPTANCE_HPP

#include "bpr/core/review_tour.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2014]], RFC-BPR-0106. A tour stop is accepted the way a file
/// is: the author accepts its text, its destination and the code at its lines
/// and at every reference in its text, and the stop is Accepted, Not accepted
/// yet or Changed since accepted from what was accepted. One acknowledgement
/// replaces Ready, the Head changed chip and the separate confirmation of lines.
namespace bpr::core {
	/// \brief The ring's states: green for Accepted, yellow for the other two.
	enum class tour_acceptance_state : std::uint8_t {
		accepted,
		not_accepted,
		changed,
	};

	/// \brief Why a stop is yellow, first reason first.
	enum class tour_acceptance_reason : std::uint8_t {
		none,
		/// New, outline-suggested, or brought in by an agent proposal.
		not_accepted,
		/// Its title or text differs from what was accepted.
		text_changed,
		/// It was moved to another destination.
		moved,
		/// Its own lines differ at the pull request's current head.
		lines_changed,
		/// Its own lines can no longer be found at the current head.
		lines_missing,
		/// Lines a reference in its text names differ at the current head.
		reference_changed,
		/// Lines a reference in its text names can no longer be found.
		reference_missing,
		/// The pull request has a new head and the code there is not compared yet.
		unchecked,
	};

	struct tour_acceptance_decision
	{
		tour_acceptance_state  State  = tour_acceptance_state::not_accepted;
		tour_acceptance_reason Reason = tour_acceptance_reason::not_accepted;
		/// The reference as written, for the two reference reasons.
		std::string Reference;
		auto        operator==(const tour_acceptance_decision&) const -> bool = default;
	};

	[[nodiscard]] auto stable_name(tour_acceptance_state aState) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(tour_acceptance_reason aReason) noexcept -> std::string_view;

	/// \brief The head the tour is checked against: the observed head, else its own.
	[[nodiscard]] auto tour_current_head(const review_tour& aTour) -> git_commit_oid;

	/// \brief Decides one stop's state from what was accepted and the tour's
	/// current head. A range at the current head, or checked unchanged there,
	/// holds; one changed or missing there, or not yet compared, turns the
	/// stop yellow.
	[[nodiscard]] auto decide_tour_stop_acceptance(const review_tour& aTour, const tour_stop& aStop) -> tour_acceptance_decision;

	/// \brief True when `decide_tour_stop_acceptance` says Accepted.
	[[nodiscard]] auto tour_stop_accepted(const review_tour& aTour, const tour_stop& aStop) -> bool;

	/// \brief Why `accept` cannot record the stop, first need first.
	enum class tour_accept_need : std::uint8_t {
		nothing,
		title,
		explanation,
		/// No lines and no destination: nothing to accept the code of.
		lines,
		/// Its own lines changed or are gone at the current head and none
		/// are mapped there: its lines must be chosen at this head.
		lines_at_head,
	};
	[[nodiscard]] auto tour_stop_accept_need(const tour_stop& aStop) noexcept -> tour_accept_need;
	/// \brief As above, and `lines_at_head` when Accept at the tour's current
	/// head could only keep its own lines changed (critical review finding 3).
	[[nodiscard]] auto tour_stop_accept_need(const review_tour& aTour, const tour_stop& aStop) -> tour_accept_need;

	/// \brief The code a stop is accepted on at `aHead`: its own lines when
	/// they are at that head (as authored, or as mapped there), then each range
	/// a reference in its text names that resolves to exactly one of
	/// `aFiles` (the pull request's changed paths) on the head's side. A bare
	/// "line N" names the stop's own file. Ranges at other commits are left out:
	/// those bytes cannot change.
	[[nodiscard]] auto tour_stop_material(const tour_stop& aStop, const git_commit_oid& aHead, std::span<const std::string> aFiles) -> std::vector<tour_material>;

	/// \brief True when the stop's text names a range in a file by path: that
	/// reference resolves only against the pull request's changed files.
	[[nodiscard]] auto tour_stop_names_files(const tour_stop& aStop) -> bool;

	/// \brief What `accept` records for `aStop` in `aTour` now. Its own lines
	/// not at the current head stay to be compared: what was accepted of them
	/// before, or its lines at the tour's head before a push, not compared yet.
	[[nodiscard]] auto accept_tour_stop(const review_tour& aTour, const tour_stop& aStop, std::span<const std::string> aFiles) -> tour_stop_acceptance;

	/// \brief Carries a draft saved before acceptance existed across: a stop
	/// the author made Ready, reaffirmed at the current head, not suggested by
	/// an outline and naming no other file, is Accepted on its lines and its
	/// text's references; every other stop stays Not accepted yet.
	void carry_tour_acceptance(review_tour& aTour);

} // namespace bpr::core

#endif
