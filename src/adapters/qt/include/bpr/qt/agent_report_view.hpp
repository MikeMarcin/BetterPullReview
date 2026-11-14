#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP

#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <vector>

class QFrame;
class QLabel;
class QListWidget;
class QScrollArea;
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
		[[nodiscard]] auto heading_offset(int aIndex) const -> int;

		agent_report_document                    Document;
		std::vector<QString>                     Headings;
		QWidget*                                 Column     = nullptr;
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
