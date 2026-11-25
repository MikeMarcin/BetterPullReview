#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP

#include "bpr/core/agent_report.hpp"

#include <QColor>
#include <QString>

class QTextDocument;

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0128 (docs/design/report-templates-v1 and v2; owner,
/// 2026-11-14: B, Dressed, in Qt's rich text). A report the core read by the
/// shared convention, drawn as one page of Qt rich text in the hand of the
/// application's own hand (round 2: its font and palette, no serif); mono places; a
/// rule at the left of each verdict; the tally as a meter and its counts;
/// code numbered from its item's place; a suggestion as a diff; a review's
/// verdict over its summary and one keep action at each finding's foot
/// ([[BPR-2219]]). Colour is spent only on verdicts.
namespace bpr::qt {
	class appearance_profile;

	/// \brief [[BPR-2236]] (owner, 2026-11-16: "colors in the agent tab and
	/// the report which correspond to the same elements but use different
	/// colors"). The three tones' colours, the theme's roles
	/// `core::appearance_role::report_verdict_*`. One source for every
	/// surface that colours a finding, verdict or label by its tone: the
	/// report page and the Agent tab's cards both read it.
	struct tone_colours
	{
		QColor Open;
		QColor Partial;
		QColor Done;

		auto operator==(const tone_colours&) const -> bool = default;
	};

	/// \brief The theme's three tones, as the profile resolves them.
	[[nodiscard]] auto tone_colours_of(const appearance_profile& aProfile) -> tone_colours;

	/// \brief The built-in Light's or Dark's, for a view no theme reached yet.
	[[nodiscard]] auto builtin_tone_colours(bool aDark) -> tone_colours;

	/// \brief The colour a tone wears; none for neutral, which each surface
	/// says in its own quiet ink.
	[[nodiscard]] auto tone_colour(core::report_tone aTone, const tone_colours& aColours) -> std::optional<QColor>;

	/// \brief The report's colours: the view's own ground, ink and code wash
	/// (round 2 of [[BPR-2219]]: the page stands on its pane's palette), and
	/// the theme's three tones.
	struct report_colours
	{
		QColor       Paper{ 0xFF, 0xFF, 0xFF };
		QColor       Ink{ 0x1F, 0x23, 0x28 };
		QColor       CodeWash{ 0xF2, 0xF3, 0xF4 };
		tone_colours Tones = builtin_tone_colours(false);

		auto operator==(const report_colours&) const -> bool = default;
	};

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
		/// The finding's file: a suggestion's, and the file every code block
		/// under the finding is anchored to ([[BPR-2351]]).
		std::string Path;

		auto operator==(const report_code_slot&) const -> bool = default;
	};
	inline constexpr auto report_code_marker = "BPRREPORTCODE";

	/// \brief [[BPR-2219]]. What became of one keep action of the page: a
	/// finding's Draft comment or Keep local, or the summary's Keep summary.
	enum class report_keep_phase : std::uint8_t {
		/// Offered: nothing was made.
		offered,
		/// Asked for; not yet answered.
		working,
		/// Made: a draft at its lines, or a Local comment.
		kept,
		/// Not made, and why.
		refused,
		/// [[BPR-2331]]. Dismissed: set aside, nothing made; Undo takes it
		/// back.
		dismissed,
	};
	struct report_keep_state
	{
		report_keep_phase Phase = report_keep_phase::offered;
		QString           Why;

		auto operator==(const report_keep_state&) const -> bool = default;
	};

	/// \brief The two links a keep action wears: making it, and going to what
	/// it made.
	enum class report_link : std::uint8_t {
		keep,
		go,
		/// [[BPR-2232]]. Draft all from the report.
		draft_all,
		/// [[BPR-2286]]. Ask follow-up on a finding.
		ask,
		/// [[BPR-2331]]. Dismiss a finding, and take that back: the Agent
		/// tab's finding card offered them, and the report now holds it.
		dismiss,
		undo_dismiss,
		/// [[BPR-2331]]. Undo the drafts the report's last Draft all made,
		/// as the Agent tab's Draft all offered.
		undo_draft_all,
		/// [[BPR-2376]]. Put a finding's updated suggested comment in the
		/// draft it made, which the reviewer changed since.
		update_draft,
	};

	/// \brief [[BPR-2219]] (docs/design/review-report-v1, A). The page's keep
	/// actions: the findings the core read, each with the action the core
	/// chose, drawn at its finding's foot; Keep summary under the summary;
	/// what became of each, by its row in the menu (0 is the summary). Review
	/// of 2026-11-14: an action never lives in a scheme agent text could
	/// write; `Link` mints, per render, the link an action carries, and only
	/// the view that minted it knows it. Without one, the page offers no
	/// action.
	struct report_page_actions
	{
		std::vector<core::report_finding>                  Findings;
		bool                                               Summary = false;
		std::map<std::uint32_t, report_keep_state>         States;
		std::function<QString(std::uint32_t, report_link)> Link;
		/// [[BPR-2232]]. Draft all: how many it would draft (0: not
		/// offered), whether it is under way, and what its last press said.
		std::size_t DraftAll     = 0U;
		bool        DraftAllBusy = false;
		QString     DraftAllSaid;
		/// [[BPR-2286]]. Ask follow-up is offered beside each finding's keep
		/// action; the comment a follow-up updated, by its finding's row in
		/// the menu. The finding is drawn from it (its Suggested change) and
		/// the comment is not written out again under it: one line says that
		/// Draft comment drafts the update.
		bool                             FollowUp = false;
		std::map<std::uint32_t, QString> Revised;
		/// [[BPR-2331]]. Dismiss is offered beside each finding's keep
		/// action; the comment the agent suggested for a finding, by its
		/// row, as its Agent tab card showed it; and Undo of the last Draft
		/// all.
		bool                             Dismiss = false;
		std::map<std::uint32_t, QString> Suggested;
		bool                             DraftAllUndo = false;
		/// [[BPR-2376]]. The drafted findings, by row, whose draft the
		/// reviewer changed before a follow-up updated their suggested
		/// comment: each says its draft differs and offers Update draft.
		std::vector<std::uint32_t> Outdated;
	};

	/// \brief [[BPR-2219]], round 2 (owner, look review-report-v001: "redesign
	/// the report styling and formatting to feel more native but also still
	/// special compared with a normal file tab"). The page is set in the
	/// application's own font and stands on its own palette; what sets it
	/// apart from a file is one of three looks. `cards` is built: the opening
	/// on a raised band with the verdict as a filled mark, and each finding a
	/// bordered box as a conversation card is. `band` keeps the band and
	/// leaves findings beside their rules; `plain` has neither.
	enum class report_look : std::uint8_t {
		cards,
		band,
		plain,
	};

	/// \brief [[BPR-2266]]. Marks the inline code of a page `report_page_html`
	/// drew fixed pitch, as the Markdown reader's inline code is, so the
	/// reader's colours can find it: each run in the code font that is not a
	/// link (a place or a link's words are). HTML cannot say fixed pitch.
	/// \returns How many runs it marked.
	auto mark_report_inline_code(QTextDocument& aDocument) -> std::size_t;

	/// \brief The width a page's rows are arranged for.
	/// \details A keep action's row is its controls and a sentence. Rich
	/// text lays a table row out on one line at any width, and takes the room
	/// it lacks from the sentence, down to a letter a line. So the page
	/// arranges each row for the width it is drawn at: on one line where it
	/// fits; the sentence beside its controls where it has room for about two
	/// lines of whole words; else the sentence under them at the row's whole
	/// width, the controls on as many lines as they need.
	struct report_page_fit
	{
		/// The page's text width, in pixels. 0: no limit, every row on one
		/// line as it is written.
		int Width = 0;
		/// Set by `report_page_html`: the widths at which some row's
		/// arrangement changes, ascending. Two widths with the same breaks at
		/// or under them give the same page, so a view draws the page again
		/// only when its width crosses one.
		std::vector<int> Breaks;
	};

	/// \brief The page's HTML. Each code block is a marker paragraph, its
	/// slot appended to `aSlots`. `aFit`: the width its rows are arranged
	/// for; none arranges each on one line.
	[[nodiscard]] auto report_page_html(const core::dressed_report& aReport, const report_colours& aColours, const report_page_actions& aActions = { }, std::vector<report_code_slot>* aSlots = nullptr,
										report_look aLook = report_look::cards, report_page_fit* aFit = nullptr) -> QString;

	/// \brief The colour a tone wears on the page; neutral is the stronger
	/// rule.
	[[nodiscard]] auto report_tone_colour(core::report_tone aTone, const report_colours& aColours) -> QColor;

	/// \brief The colour every link of the page wears: the page's own blue,
	/// which the core derives from the paper and ink and holds to the text
	/// floor (`core::derive_report_shades`). A platform's link role is not
	/// it: no theme owns that role, and it is the brighter system blue.
	[[nodiscard]] auto report_link_colour(const report_colours& aColours) -> QColor;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_PAGE_HPP
