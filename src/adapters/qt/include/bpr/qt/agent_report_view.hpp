#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/report_page.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <map>
#include <vector>

class QFrame;
class QLabel;
class QListWidget;
class QScrollArea;
class QTextBrowser;
class QTextEdit;
class QToolButton;

namespace bpr::qt {
	class markdown_body_view;

	/// \brief What an opened report shows: the turn it came from, its title,
	/// its Markdown (references already links), the question it answers and
	/// who answered, when and at which head.
	struct agent_report_document
	{
		std::size_t Turn = 0U;
		QString     Title;
		QString     Markdown;
		QString     Question;
		QString     Byline;
	};

	/// \brief RFC-BPR-0128 ([[BPR-2166]], owner Q8: "file tab is more
	/// flexible"), as the owner chose its reading on 2026-11-14
	/// (docs/design/report-card-v4, C, Floating): a straight read in the file
	/// tab. One centred column, everything open, the page scrolling as one;
	/// a floating menu button at the pane's top right opens a popover of the
	/// report's sections (the one being read marked) and its other actions
	/// (Show in conversation, View source, Copy source). It reads only what
	/// it is given.
	/// \brief A report item's Add as draft comment: where and what.
	struct report_draft
	{
		QString       Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		/// The comment: the item's label and heading, its reasons and the
		/// suggestion block.
		QString Comment;
	};
	/// \brief How a draft went: made, or why not.
	using report_draft_done     = std::function<void(bool aDrafted, const QString& aWhy)>;
	using report_draft_dispatch = std::function<void(const report_draft& aDraft, report_draft_done aDone)>;

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
		void set_conversation_dispatch(std::function<void(std::size_t aTurn)> aDispatch);
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

		/// \brief docs/design/report-templates-v2. The report's colours, the
		/// theme's roles; the page is drawn again in them.
		void               set_report_colours(const report_colours& aColours);
		[[nodiscard]] auto colours() const noexcept -> const report_colours& { return Colours; }
		/// \brief Where a suggestion's Add as draft comment goes.
		void set_draft_dispatch(report_draft_dispatch aDispatch);
		/// \brief The report as read by the convention; its page shows when it
		/// has sections (`dressed_shown`), else the Markdown as written.
		[[nodiscard]] auto dressed() const noexcept -> const core::dressed_report& { return Dress; }
		[[nodiscard]] auto dressed_shown() const noexcept -> bool;
		[[nodiscard]] auto dressed_view() const noexcept -> QTextBrowser* { return Dressed; }
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
		void               place_code();
		[[nodiscard]] auto code_height(int aSlot, int aWidth) const -> int;
		void               locate_headings();
		void               apply_colours();
		[[nodiscard]] auto shown_text() const -> QTextEdit*;
		[[nodiscard]] auto heading_offset(int aIndex) const -> int;

		agent_report_document Document;
		std::vector<QString>  Headings;
		/// The rendered heading blocks, in the outline's order; empty when
		/// the rendering and the outline disagree on how many there are.
		std::vector<int>     HeadingBlocks;
		std::vector<QString> OutlineTitles;
		QWidget*             Column  = nullptr;
		QWidget*             Tail    = nullptr;
		QTextBrowser*        Dressed = nullptr;
		/// The page's code boxes, by slot, and the page's places for them.
		std::vector<report_code_slot>            Slots;
		std::vector<markdown_body_view*>         CodeViews;
		std::vector<int>                         CodePlaces;
		QObject*                                 CodeObjects = nullptr;
		core::dressed_report                     Dress;
		report_colours                           Colours;
		std::map<std::uint32_t, QString>         DraftNotes;
		report_draft_dispatch                    DraftDispatch;
		QLabel*                                  Title      = nullptr;
		QLabel*                                  Byline     = nullptr;
		QScrollArea*                             Scroll     = nullptr;
		markdown_body_view*                      Body       = nullptr;
		QToolButton*                             MenuButton = nullptr;
		QFrame*                                  Popover    = nullptr;
		QListWidget*                             Sections   = nullptr;
		std::function<void(const QString&)>      LinkDispatch;
		std::function<void(markdown_body_view&)> Dresser;
		std::function<void(std::size_t)>         ConversationDispatch;
	};
} // namespace bpr::qt

#endif
