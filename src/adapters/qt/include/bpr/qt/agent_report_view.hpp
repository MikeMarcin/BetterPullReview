#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/core/agent_report_keep.hpp"
#include "bpr/qt/report_page.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

class QFrame;
class QLabel;
class QListWidget;
class QScrollArea;
class QTextEdit;
class QToolButton;

namespace bpr::qt {
	class markdown_body_view;

	/// \brief What an opened report shows: the turn it came from, its title,
	/// its Markdown (references already links), the question it answers and
	/// who answered, when and at which head.
	/// \brief [[BPR-2184]] (review N5). The line under a report's title, as
	/// data: how its turn ended, when, and the head it was asked at.
	struct agent_report_byline
	{
		/// How the turn ended, as its footer first says it ("Claude answered
		/// in 12s").
		QString Ending;
		/// When it answered, as the tab says it.
		QString When;
		/// The head it was asked at, short.
		QString Head;

		auto operator==(const agent_report_byline&) const -> bool = default;
	};

	/// \brief [[BPR-2231]]. One finding card of the report's answer, in the
	/// order of `agent_report_document::Suggested`: the card's delivery and
	/// place, the finding it carries, and where its state is kept.
	struct agent_report_card
	{
		std::uint64_t         Session = 0U;
		std::size_t           Ordinal = 0U;
		core::agent_finding   Finding;
		core::report_card_key Key;
	};

	struct agent_report_document
	{
		/// [[BPR-2184]] (review N5). The turn's stable identity in its Agent
		/// tab, which a report is keyed by: never its place in the list.
		std::uint64_t       Turn = 0U;
		QString             Title;
		QString             Markdown;
		QString             Question;
		agent_report_byline Byline;
		/// [[BPR-2219]]. The comments the agent suggested for the answer's
		/// findings in its findings block, by place: a report finding at the
		/// same place drafts from the agent's own words.
		std::vector<core::report_suggested_comment> Suggested;
		/// [[BPR-2231]]. The cards `Suggested` lists, in its order: a report
		/// finding at a card's place is that card, and shares its state.
		std::vector<agent_report_card> Cards;
		/// [[BPR-2231]]. The answer as its turn holds it: what the report's
		/// findings no card carries are kept under (`digest_report_findings`
		/// of its report).
		QString Answer;
	};

	/// \brief RFC-BPR-0128 ([[BPR-2166]], owner Q8: "file tab is more
	/// flexible"), as the owner chose its reading on 2026-11-14
	/// (docs/design/report-card-v4, C, Floating): a straight read in the file
	/// tab. One centred column, everything open, the page scrolling as one;
	/// a floating menu button at the pane's top right opens a popover of the
	/// report's sections (the one being read marked) and its other actions
	/// (Show in conversation, View source, Copy source). It reads only what
	/// it is given.
	/// \brief [[BPR-2219]]. One keep action of a report, as the core read
	/// it: a finding's Draft comment or Keep local, or the summary kept as a
	/// Local comment on the pull request.
	struct report_draft
	{
		/// The finding's row in the report's menu; 0 for the summary.
		std::uint32_t Outline = 0U;
		/// Draft comment at the lines, or Keep local (on `Path`, or on the
		/// pull request when it is empty).
		core::report_keep Keep = core::report_keep::draft_comment;
		QString           Path;
		std::uint32_t     First = 0U;
		std::uint32_t     Last  = 0U;
		/// What the draft or the Local comment starts from.
		QString Comment;
		/// [[BPR-2231]]. Where what it makes is kept: its card's state, or
		/// the report's own record for it.
		core::report_finding_key Key;
		/// [[BPR-2231]]. The finding card it is, when one is at its place:
		/// the keep goes through the card's delivery, so the card says so too.
		std::optional<agent_report_card> Card;
	};
	/// \brief How it went: made, or why not.
	using report_draft_done     = std::function<void(bool aDrafted, const QString& aWhy)>;
	using report_draft_dispatch = std::function<void(const report_draft& aDraft, report_draft_done aDone)>;
	/// \brief [[BPR-2231]]. The one kept state of a finding (or the summary),
	/// as the host keeps it; `open` when nothing is.
	using report_keep_records = std::function<core::agent_finding_card_record(const core::report_finding_key& aKey)>;
	/// \brief [[BPR-2232]]. Draft all from the report: the findings to draft,
	/// in the report's order, drafted by the Agent tab's Draft all path;
	/// `aPlanned` counts those passed over before drafting. `aDone` says how
	/// the batch ended. Each finding's state goes through the records.
	using report_draft_all_done     = std::function<void(const core::draft_all_outcome& aOutcome)>;
	using report_draft_all_dispatch = std::function<void(std::vector<report_draft> aDrafts, const core::draft_all_outcome& aPlanned, report_draft_all_done aDone)>;

	class agent_report_view final : public QWidget
	{
	public:
		explicit agent_report_view(QWidget* aParent = nullptr);

		void present(const agent_report_document& aDocument);
		/// \brief Where a link goes (a code reference, a web address).
		void set_link_dispatch(std::function<void(const QString&)> aDispatch);
		/// \brief Lets the window dress the body as the Agent tab's answers are.
		void set_dresser(std::function<void(markdown_body_view&)> aDresser);
		/// \brief The question under the title, and Show in conversation: the
		/// Agent tab at the report's turn.
		void set_conversation_dispatch(std::function<void(std::uint64_t aTurn)> aDispatch);
		/// \brief Scrolls so section `aIndex` of the menu (0: Summary, the
		/// top) is at the top of the pane.
		void go_to_heading(int aIndex);
		/// \brief Opens the menu's popover, its current section focused.
		void open_menu();
		/// \brief The menu's row for the section being read: the last heading
		/// at or above the pane's top; Summary before the first.
		[[nodiscard]] auto current_section() const -> int;
		/// \brief Where section `aIndex`'s heading stands in the pane, from
		/// the top of what shows; -1 when the report has no such heading.
		[[nodiscard]] auto section_top(int aIndex) const -> int;

		/// \brief docs/design/report-templates-v2, [[BPR-2236]]. The theme's
		/// three tones (`tone_colours_of`, as the Agent tab reads them); the
		/// page is drawn again in them.
		void               set_tone_colours(const tone_colours& aColours);
		[[nodiscard]] auto colours() const noexcept -> const report_colours& { return Colours; }
		/// \brief [[BPR-2219]], round 2. Which of the page's looks is drawn
		/// (`cards` is the product's; the others are for the design round's
		/// renders).
		void set_look(report_look aLook);
		/// \brief Where a keep action goes: a finding's Draft comment or Keep
		/// local, and Keep summary.
		void set_draft_dispatch(report_draft_dispatch aDispatch);
		/// \brief Where Go to draft and Go to comment go: what the keep made.
		void set_go_dispatch(std::function<void(const report_draft&)> aDispatch);
		/// \brief [[BPR-2231]]. Where each keep action's state is read: the
		/// turn's one state for that finding. Without it the view remembers
		/// what was kept for the session only.
		void set_keep_records(report_keep_records aRecords);
		/// \brief [[BPR-2231]]. The state kept at `aKey` changed (from the
		/// report, its finding card or Draft all): the page says it now.
		void settle_keep(const core::report_finding_key& aKey);
		/// \brief [[BPR-2231]]. The kept states were read back: the page says them.
		void refresh_keeps();
		/// \brief [[BPR-2232]]. Draft all from the report, and whether a
		/// finding's lines can take a draft now, as the host knows.
		void set_draft_all_dispatch(report_draft_all_dispatch aDispatch);
		void set_placeable(std::function<bool(const report_draft&)> aPlaceable);
		/// \brief [[BPR-2286]]. Ask follow-up on a finding opens its
		/// follow-up: the finding as its keep action would make it.
		void set_follow_up_dispatch(std::function<void(const report_draft&)> aDispatch);
		/// \brief [[BPR-2286]]. A finding's suggested comment as a follow-up
		/// updated it, if one did: its keep action drafts that, and the page
		/// shows it.
		void set_revision_source(std::function<std::optional<QString>(const report_draft&)> aSource);
		/// \brief [[BPR-2232]]. How many findings Draft all would draft (the
		/// count its link shows); 0 when it is not offered.
		[[nodiscard]] auto draft_all_offered() const -> std::size_t;
		/// \brief [[BPR-2232]]. Presses Draft all; false when it is not offered.
		auto press_draft_all() -> bool;
		/// \brief [[BPR-2232]]. What the report says of its last Draft all:
		/// under way, or how many were drafted and which were not.
		[[nodiscard]] auto        draft_all_said() const -> QString;
		[[nodiscard]] static auto draft_all_words(const core::draft_all_outcome& aOutcome) -> QString;
		/// \brief [[BPR-2219]]. The report's findings, as the core read them,
		/// each with its keep action.
		[[nodiscard]] auto findings() const noexcept -> const std::vector<core::report_finding>& { return Findings; }
		/// \brief Whether the page offers Keep summary.
		[[nodiscard]] auto summary_offered() const -> bool;
		/// \brief What became of the keep action at `aOutline` (0: the
		/// summary's) of the report shown.
		[[nodiscard]] auto keep_state(std::uint32_t aOutline) const -> report_keep_state;
		/// \brief [[BPR-2258]]. The last thing the report said to a screen
		/// reader: a keep action made, or why not.
		[[nodiscard]] auto last_announcement() const noexcept -> const QString& { return LastAnnouncement; }
		/// \brief Presses the keep action at `aOutline`, or its Go to, as
		/// following its link does; false when the page offers none there.
		auto press(std::uint32_t aOutline, report_link aLink) -> bool;
		/// \brief The report as read by the convention; its page shows when it
		/// has sections (`dressed_shown`), else the Markdown as written.
		[[nodiscard]] auto dressed() const noexcept -> const core::dressed_report& { return Dress; }
		[[nodiscard]] auto dressed_shown() const noexcept -> bool;
		/// \brief The dressed page: a read-only reader whose document denies
		/// every resource load, as the Markdown readers' does.
		[[nodiscard]] auto dressed_view() const noexcept -> QTextEdit* { return Dressed; }
		/// \brief Follows a link of the page as a press does.
		void follow(const QString& aLink);
		/// \brief Look report-templates-v003. The page's code boxes, the
		/// Agent tab's own, in the order the page shows them.
		[[nodiscard]] auto code_views() const -> const std::vector<markdown_body_view*>& { return CodeViews; }
		/// \brief Syntax colours for the report's code, as the window's
		/// readers take them.
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);

		[[nodiscard]] auto outline() const noexcept -> QListWidget* { return Sections; }
		[[nodiscard]] auto menu_button() const noexcept -> QToolButton* { return MenuButton; }
		[[nodiscard]] auto popover() const noexcept -> QFrame* { return Popover; }
		[[nodiscard]] auto body() const noexcept -> markdown_body_view* { return Body; }
		[[nodiscard]] auto title() const -> QString;
		[[nodiscard]] auto document() const noexcept -> const agent_report_document& { return Document; }

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;

	private:
		void               place_menu_button();
		void               fit_body();
		void               dress_headings();
		void               choose(int aRow);
		void               render_dressed();
		void               colour_inline_code();
		void               place_code();
		[[nodiscard]] auto code_height(int aSlot, int aWidth) const -> int;
		void               locate_headings();
		void               apply_colours();
		[[nodiscard]] auto shown_text() const -> QTextEdit*;
		[[nodiscard]] auto heading_offset(int aIndex) const -> int;
		[[nodiscard]] auto native_colours() const -> report_colours;
		[[nodiscard]] auto keep_request(std::uint32_t aOutline) const -> std::optional<report_draft>;
		void               announce(const QString& aText);
		void               focus_link(std::uint32_t aOutline, report_link aLink);
		[[nodiscard]] auto keep_key(std::uint32_t aOutline) const -> core::report_finding_key;
		[[nodiscard]] auto card_keys() const -> std::vector<core::report_card_key>;
		[[nodiscard]] auto draft_all_plan() const -> std::pair<core::draft_all_plan, std::vector<std::uint32_t>>;

		agent_report_document Document;
		std::vector<QString>  Headings;
		/// The rendered heading blocks, in the outline's order; empty when
		/// the rendering and the outline disagree on how many there are.
		std::vector<int>     HeadingBlocks;
		std::vector<QString> OutlineTitles;
		QWidget*             Column  = nullptr;
		QWidget*             Tail    = nullptr;
		QTextEdit*           Dressed = nullptr;
		/// The page's code boxes, by slot, and the page's places for them.
		std::vector<report_code_slot>    Slots;
		std::vector<markdown_body_view*> CodeViews;
		std::vector<int>                 CodePlaces;
		QObject*                         CodeObjects = nullptr;
		core::dressed_report             Dress;
		report_colours                   Colours;
		report_look                      Look = report_look::cards;
		/// The report's findings, and what became of each keep action, by
		/// the report's turn and the action's row (0: the summary), so a
		/// report opened again says what was already kept.
		std::vector<core::report_finding>                                   Findings;
		std::map<std::uint64_t, std::map<std::uint32_t, report_keep_state>> Keeps;
		/// [[BPR-2231]]. The digest the report's own records are kept under.
		core::agent_findings_digest ReportDigest{ };
		report_keep_records         KeepRecords;
		QString                     LastAnnouncement;
		/// [[BPR-2232]]. Draft all, by the report's turn: under way, and what
		/// its last press said.
		std::map<std::uint64_t, bool>            DraftAllBusy;
		std::map<std::uint64_t, QString>         DraftAllSaid;
		report_draft_all_dispatch                DraftAllDispatch;
		std::function<bool(const report_draft&)> Placeable;
		/// This render's keep links, by token.
		std::map<QString, std::pair<std::uint32_t, report_link>>   ActionLinks;
		report_draft_dispatch                                      DraftDispatch;
		std::function<void(const report_draft&)>                   GoDispatch;
		std::function<void(const report_draft&)>                   FollowUpDispatch;
		std::function<std::optional<QString>(const report_draft&)> RevisionSource;
		QLabel*                                                    Title      = nullptr;
		QLabel*                                                    Byline     = nullptr;
		QScrollArea*                                               Scroll     = nullptr;
		markdown_body_view*                                        Body       = nullptr;
		QToolButton*                                               MenuButton = nullptr;
		QFrame*                                                    Popover    = nullptr;
		QListWidget*                                               Sections   = nullptr;
		std::function<void(const QString&)>                        LinkDispatch;
		std::function<void(markdown_body_view&)>                   Dresser;
		std::function<void(std::uint64_t)>                         ConversationDispatch;
	};
} // namespace bpr::qt

#endif
