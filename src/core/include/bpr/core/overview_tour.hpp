#ifndef BPR_CORE_OVERVIEW_TOUR_HPP
#define BPR_CORE_OVERVIEW_TOUR_HPP

#include "bpr/core/review_tour.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1781]]. What Overview's Tour tab says about the pull
/// request's tour, and whether it is shown at all.
///
/// \details The owner, 2026-10-27: "Why isn't the tour tab on overview for an
/// authored PR, or possibly any PR". The tab presents and routes; writing and
/// following stay in Review beside the code. The decision reads the window's
/// tour draft, the published tour GitHub offers for import, and whether the
/// viewer wrote the pull request, and says which of four states applies.
namespace bpr::core {
	enum class overview_tour_kind : std::uint8_t {
		/// No tour, and not the author: no tab.
		hidden,
		/// The author, with no tour yet: Prepare review tour.
		prepare,
		/// A tour I am writing (a private draft or one I published): the brief,
		/// the stops with their readiness, and Continue in Review.
		writer,
		/// A published tour I did not write: the brief and the stop titles,
		/// read-only, and Start tour.
		reader,
	};

	struct overview_tour_stop
	{
		tour_stop_id     Id;
		std::string      Title;
		tour_preparation Preparation = tour_preparation::editing;

		auto operator==(const overview_tour_stop&) const -> bool = default;
	};

	struct overview_tour
	{
		overview_tour_kind              Kind = overview_tour_kind::hidden;
		std::string                     Problem;
		std::string                     Solution;
		std::vector<overview_tour_stop> Stops;
		/// Whether the tour is on GitHub; a writer's draft that is not is private.
		bool Published = false;
		/// Whether the reader's tour is already imported into this window, so
		/// Start tour needs no import first.
		bool Imported = false;

		auto operator==(const overview_tour&) const -> bool = default;
	};

	/// \brief Decides Overview's Tour tab.
	///
	/// \param aDraft The window's tour draft, or null while none is loaded.
	/// \param aOffered The first published tour GitHub offers for import, or null.
	/// \param aAuthor Whether the viewer wrote the pull request.
	///
	/// \details A draft with text, stops or a published binding is a tour.
	/// One I may edit is mine whoever wrote the pull request; a read-only one
	/// that is published is a reader's. Without either, a published tour on
	/// offer is a reader's too, and the author is offered Prepare.
	[[nodiscard]] auto decide_overview_tour(const review_tour* aDraft, const review_tour* aOffered, bool aAuthor) -> overview_tour;
} // namespace bpr::core

#endif
