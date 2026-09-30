#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_TABS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_TABS_HPP

#include "bpr/core/document_tabs.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Document tabs of a local Review Cycle (RFC-BPR-0061, BPR-1157).
/// \details The same tab gestures as a pull request's tabs (RFC-BPR-0052's
/// `document_tab_intent`), over the cycle's own exact views: a file of one
/// captured revision, read as full source, and a reading line. A tab keeps
/// its revision when newer material is captured; only an explicit gesture
/// moves it. A tab may instead show a named comparison (BPR-1163): the view
/// the reviewer asked for and the exact pair it resolved to, which a pinned
/// view never leaves and a semantic view leaves only on refresh. The shared
/// pull-request comparison gestures stay refused here, so no tab ever shows
/// an invented baseline.
namespace bpr::core {
	inline constexpr std::size_t maximum_cycle_tabs = 64U;

	/// \brief A named comparison a tab shows: what was asked and the left
	/// revision it resolved to. The right revision is the tab's own.
	struct cycle_tab_comparison
	{
		cycle_view_request       Request;
		cycle_source_revision_id Left;
		auto                     operator<=>(const cycle_tab_comparison&) const = default;
	};
	/// \brief One cycle tab's exact view.
	struct cycle_tab_view
	{
		/// Full source: the revision read. A comparison: its right side.
		cycle_source_revision_id Revision;
		std::string              Path;
		/// Zero-based first visible line, as the view last reported it.
		std::uint64_t Line = 0;
		/// Absent for full source.
		std::optional<cycle_tab_comparison> Comparison;
		auto                                operator<=>(const cycle_tab_view&) const = default;
	};
	/// \brief The exact pair a tab shows.
	[[nodiscard]] auto shown_pair(const local_cycle_subject& aSubject, const cycle_tab_view& aView) -> cycle_comparison_pair;
	struct cycle_tab
	{
		document_tab_id Id;
		cycle_tab_view  View;
		auto            operator<=>(const cycle_tab&) const = default;
	};
	struct cycle_tabs_state
	{
		/// The revision new tabs take: the latest captured when the cycle was
		/// opened, until the reviewer chooses another.
		std::optional<cycle_source_revision_id> Revision;
		std::vector<cycle_tab>                  Tabs;
		std::optional<document_tab_id>          Active;
		std::uint64_t                           LastTab                                    = 0;
		auto                                    operator<=>(const cycle_tabs_state&) const = default;
	};

	/// \brief The tab identity of a path in a cycle: the same file always has
	/// the same identity, distinct from every pull-request file identity.
	[[nodiscard]] auto cycle_file_identity(const local_cycle_subject& aSubject, std::string_view aPath) -> file_identity;

	struct cycle_tabs_output
	{
		bool                Changed                                     = false;
		document_tab_reason Reason                                      = document_tab_reason::none;
		auto                operator<=>(const cycle_tabs_output&) const = default;
	};
	/// \brief Applies one tab gesture. `aPaths` resolves a gesture's file
	/// identity to the path it names among the files the view lists.
	[[nodiscard]] auto transition(cycle_tabs_state& aState, const local_cycle_subject& aSubject, const document_tab_intent& aIntent,
								  const std::vector<std::string>& aPaths) -> cycle_tabs_output;
	/// \brief Adopts the first visible line the active tab reports.
	[[nodiscard]] auto observe_line(cycle_tabs_state& aState, document_tab_id aTab, std::uint64_t aLine) -> cycle_tabs_output;
	/// \brief Opens a path directly (the files list, a note's location).
	[[nodiscard]] auto open_path(cycle_tabs_state& aState, std::string_view aPath, bool aNewTab) -> cycle_tabs_output;
	[[nodiscard]] auto active_tab(const cycle_tabs_state& aState) noexcept -> const cycle_tab*;

	/// \brief Shows a named comparison of the active tab's file: `aPair`, the
	/// pair the view resolved to now. The same view and pair again changes nothing.
	[[nodiscard]] auto show_comparison(cycle_tabs_state& aState, const cycle_view_request& aRequest, const cycle_comparison_pair& aPair) -> cycle_tabs_output;
	/// \brief Returns the active tab to full source at the revision it showed on its right.
	[[nodiscard]] auto show_full_source(cycle_tabs_state& aState) -> cycle_tabs_output;
	/// \brief The reviewer's explicit refresh of the active tab: a semantic
	/// comparison takes `aPair`, the pair it resolves to now; full source
	/// moves to `aLatest`. A pinned comparison never moves.
	[[nodiscard]] auto refresh_active(cycle_tabs_state& aState, const std::optional<cycle_comparison_pair>& aPair, cycle_source_revision_id aLatest) -> cycle_tabs_output;

	/// \brief The persisted form: views in order and the active position.
	struct cycle_tabs_record
	{
		std::optional<cycle_source_revision_id> Revision;
		std::vector<cycle_tab_view>             Views;
		std::optional<std::size_t>              Active;
		auto                                    operator<=>(const cycle_tabs_record&) const = default;
	};
	[[nodiscard]] auto persisted(const cycle_tabs_state& aState) -> cycle_tabs_record;
	/// \brief Restores a record, minting fresh identities; views naming a
	/// revision the cycle never allocated (on either side of a comparison, or
	/// in its request), an invalid path or too many tabs are refused whole.
	[[nodiscard]] auto restore(cycle_tabs_state& aState, const review_cycle_state& aCycle, const cycle_tabs_record& aRecord) -> cycle_tabs_output;
}

#endif
