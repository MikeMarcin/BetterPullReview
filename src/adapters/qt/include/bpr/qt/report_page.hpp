#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP

#include "bpr/core/agent_report.hpp"

#include <QColor>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0128 (docs/design/report-templates-v1 and v2; owner,
/// 2026-11-14: B, Dressed, in Qt's rich text). A report the core read by the
/// shared convention, drawn as one page of Qt rich text in the hand of the
/// attended-review page: serif reading text, sans labels, mono places; a
/// rule at the left of each verdict; the tally as a meter and its counts;
/// code numbered from its item's place; a suggestion as a diff with Add as
/// draft comment, a link. Colour is spent only on verdicts.
namespace bpr::qt {
	/// \brief The report's colours, the theme's named roles
	/// (`core::appearance_role::report_*`).
	struct report_colours
	{
		QColor Paper{ 0xEE, 0xF1, 0xF0 };
		QColor Ink{ 0x1C, 0x24, 0x27 };
		QColor Open{ 0xB2, 0x39, 0x2A };
		QColor Partial{ 0x94, 0x61, 0x0A };
		QColor Done{ 0x1D, 0x70, 0x40 };
		QColor CodeWash{ 0xFF, 0xFF, 0xFF };

		auto operator==(const report_colours&) const -> bool = default;
	};

	/// \brief The link Add as draft comment carries: `bpr-draft:` and its
	/// item's row in the menu.
	inline constexpr auto report_draft_scheme = "bpr-draft:";
	/// \brief The anchor a heading's paragraph carries: `bpr-h-` and its row
	/// in the menu.
	inline constexpr auto report_heading_anchor = "bpr-h-";

	/// \brief Look report-templates-v003 (owner: "we need code syntax
	/// highlighting and normal code block coloring rules on code blocks").
	/// A code block of the page is drawn by the Agent tab's own code box over
	/// its place: the page holds `report_code_marker` and the slot's ordinal
	/// there, and the slot says what the box shows.
	struct report_code_slot
	{
		std::string                  Language;
		std::vector<std::string>     Lines;
		std::optional<std::uint32_t> FirstLine;
		/// A suggestion: the lines it replaces and the file they are in, so
		/// it shows as the Agent tab shows a suggested change.
		bool                     Suggestion = false;
		std::vector<std::string> Replaced;
		std::string              Path;

		auto operator==(const report_code_slot&) const -> bool = default;
	};
	inline constexpr auto report_code_marker = "BPRREPORTCODE";

	/// \brief The page's HTML. `aDraftNotes` says, by an item's row in the
	/// menu, what became of its Add as draft comment. Each code block is a
	/// marker paragraph, its slot appended to `aSlots`.
	[[nodiscard]] auto report_page_html(const core::dressed_report& aReport, const report_colours& aColours, const std::map<std::uint32_t, QString>& aDraftNotes = { },
										std::vector<report_code_slot>* aSlots = nullptr) -> QString;

	/// \brief The colour a tone wears; neutral is the stronger rule.
	[[nodiscard]] auto report_tone_colour(core::report_tone aTone, const report_colours& aColours) -> QColor;

	/// \brief The serif the reading text is set in, with its fallbacks.
	[[nodiscard]] auto report_serif_families() -> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP
