#ifndef BPR_SRC_ADAPTERS_QT_SRC_PULL_REQUEST_OVERVIEW_SUPPORT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_PULL_REQUEST_OVERVIEW_SUPPORT_HPP
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"

#include <QAbstractSlider>
#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QFocusEvent>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QLatin1StringView>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QTextCursor>
#include <QTextFormat>
#include <QToolButton>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

// Colours, models, delegates and widgets the pull-request overview page's sources share.
namespace bpr::qt {
	/// \brief [[BPR-1677]]. The files' keys for their top and bottom row,
	/// as a screen reader speaks them: "Command-Up Arrow and Command-Down
	/// Arrow go to the top and bottom row" on macOS.
	[[nodiscard]] auto files_ends_help() -> QString;

	auto full_view_icon(const QPalette& aPalette, const bool aExpanded) -> QIcon;

	// One shared reader keeps both native documents through normal/expanded layout.
	class overview_description_view final : public QWidget
	{
	public:
		explicit overview_description_view(QWidget* aParent);
		markdown_body_view* Reader = nullptr;
		// The full-view toggle rides the reader's top-right corner, inside the
		// frame and left of the vertical scroll bar, so it takes no column or
		// row from the reader (owner, overview-v002 rejection).
		std::function<void()> LayoutChanged;
		void                  set_corner(QWidget* aCorner);
		[[nodiscard]] auto    line_height() const -> int;
		[[nodiscard]] auto    chrome_height() const -> int;
		[[nodiscard]] auto    natural_height() const -> int;
		void                  present_body(const std::string& aText);
		void                  reset_reader();
		void                  set_expanded(const bool aExpanded);
		[[nodiscard]] auto    expanded() const -> bool;

	protected:
		bool event(QEvent* aEvent) override;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;

	private:
		struct reading_place
		{
			int Top        = 0;
			int Anchor     = 0;
			int Position   = 0;
			int Horizontal = 0;
		};
		template <typename TView>
		void watch(TView* aView)
		{
			aView->installEventFilter(this);
			aView->viewport()->installEventFilter(this);
			connect(aView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { remember(); });
			connect(aView, &TView::selectionChanged, this, [this] { remember(); });
			connect(aView, &TView::cursorPositionChanged, this, [this] { remember(); });
		}
		template <typename TView>
		void save(TView* aView, reading_place& aPlace)
		{
			aPlace = { aView->cursorForPosition(QPoint{ 0, 0 }).position(), aView->textCursor().anchor(), aView->textCursor().position(), aView->horizontalScrollBar()->value() };
		}
		void remember();
		template <typename TView>
		void restore(TView* aView, const reading_place& aPlace)
		{
			const auto last   = aView->document()->characterCount() - 1;
			auto       cursor = QTextCursor{ aView->document() };
			cursor.setPosition(std::min(aPlace.Top, last));
			aView->setTextCursor(cursor);
			aView->ensureCursorVisible();
			auto* bar = aView->verticalScrollBar();
			if constexpr (std::is_same_v<TView, QPlainTextEdit>) {
				bar->setValue(cursor.blockNumber());
			} else {
				bar->setValue(bar->value() + aView->cursorRect(cursor).top());
			}
			const auto offset = bar->value();
			cursor.setPosition(std::min(aPlace.Anchor, last));
			cursor.setPosition(std::min(aPlace.Position, last), QTextCursor::KeepAnchor);
			aView->setTextCursor(cursor);
			bar->setValue(offset);
			aView->horizontalScrollBar()->setValue(aPlace.Horizontal);
		}
		void queue_layout();
		// The block property that keeps a reserved block's own right margin.
		static constexpr auto OwnRightMargin = QTextFormat::UserProperty + 1314;
		void                  place_corner();
		// Rendered lines beside the corner wrap short of it, so no word sits
		// under the toggle at rest; every later line keeps the full width. Each
		// block's top depends only on the blocks before it, so one ordered pass
		// settles. Source stays unwrapped and scrolls horizontally instead.
		void          reserve_corner(const QSize aSize);
		QWidget*      Corner = nullptr;
		reading_place RenderedPlace, SourcePlace;
		bool          Expanded = false, Adjusting = false, Queued = false, ReadingInput = false;
	};

	[[nodiscard]] auto text(const std::string& aValue) -> QString;

	[[nodiscard]] auto operation_letter(const application::overview_file_operation aOperation)
		-> QString;

	[[nodiscard]] auto count_text(const std::size_t aCount) -> QString;

	/// \brief [[BPR-1350]]. A heading that shortens with an ellipsis when
	/// its row is too narrow, and never cuts a word.
	///
	/// \details Its text stays the whole sentence; only the painting is
	/// shortened. The whole sentence is always its accessible name, and is
	/// its tooltip whenever it is shortened, ahead of any note the heading
	/// carries.
	class eliding_heading final : public QLabel
	{
	public:
		using QLabel::QLabel;

		void present(const QString& aText, const QString& aNote);

		[[nodiscard]] auto elided() const -> bool;

		[[nodiscard]] auto shown_text() const -> QString;

		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void resizeEvent(QResizeEvent* const aEvent) override;

		void changeEvent(QEvent* const aEvent) override;

		void paintEvent(QPaintEvent* const /*aEvent*/) override;

	private:
		void refit();

		QString Note;
		bool    Elided = false;
	};

	constexpr qreal chip_mark = 16.0;

	[[nodiscard]] auto chip_font(const QFont& aFont) -> QFont;

	/// The space between a chip's mark and its count.
	constexpr qreal count_gap = 3.0;

	[[nodiscard]] auto state_of(const core::feedback_state_counts& aStates, const core::feedback_resolution aState) -> std::size_t;

	[[nodiscard]] auto conversations(const std::size_t aCount) -> QString;

	/// "1 open, 1 resolved, 1 state unknown": a breakdown in the states'
	/// order, zeros left out.
	[[nodiscard]] auto breakdown_text(const core::feedback_state_counts& aStates) -> QString;

	/// The limits and the units the count leaves out, each as a clause in
	/// plain words: "2 whose state is unknown", "1 whose label can't be
	/// read" (owner, accepting `overview-chips-v001`: the earlier wording
	/// read as jargon).
	[[nodiscard]] auto qualifier_clauses(const core::feedback_counter_projection& aCounts) -> QStringList;

	[[nodiscard]] auto any_columns(const std::optional<application::overview_feedback_projection>& aFeedback) -> bool;

	[[nodiscard]] auto chip_name(const overview_comment_chip& aChip, const bool aAnyColumns) -> QString;

	/// One chip's tooltip, next to it: what it counts, in which state, and
	/// what it leaves out.
	[[nodiscard]] auto chip_tooltip(const overview_comment_chip& aChip, const core::feedback_counter_projection& aCounts, const bool aAnyColumns) -> QString;

	/// A speech bubble: Comments with no chip chosen; with three dots in
	/// it, Other, the rest.
	void paint_bubble(QPainter& aPainter, const QRectF& aBox, const QColor& aInk, const bool aOther);

	/// \brief [[BPR-1577]]. One half of the Open and All switch beside the
	/// Files heading, drawn by the page so it looks the same in every style.
	/// \details The macOS style drew the checked half as a heavy grey block,
	/// the only filled control on the row (owner, 2026-10-17: "it's
	/// highlighted like that and nothing else is"). Fusion's quiet chip was
	/// what the owner liked (v001's review), so this draws that chip itself:
	/// the chosen half a softly filled rounded box with a one-pixel border,
	/// the other its word alone, both in the text colour at its ordinary
	/// weight and the row's icon-button height. The border is the page's
	/// colour moved towards the text just far enough for 3:1, a little firmer
	/// than Fusion's 2:1, so the choice is still seen. Open draws its round
	/// open mark in a 16 px box, as its icon was, because it is what Open
	/// counts.
	class counts_mode_button final : public QToolButton
	{
	public:
		explicit counts_mode_button(QWidget* const aParent);

		/// \brief The mark drawn before the word, in \p aColours.
		void set_mark(const core::feedback_resolution aState, const feedback_state_colours& aColours);

		[[nodiscard]] auto sizeHint() const -> QSize override;

		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void focusInEvent(QFocusEvent* const aEvent) override;

		void paintEvent(QPaintEvent* /*aEvent*/) override;

	private:
		static constexpr int counts_mode_padding = 6;
		static constexpr int counts_mode_gap     = 4;
		/// The width the mark and its gap take before the word.
		[[nodiscard]] auto mark_advance() const -> int;

		std::optional<core::feedback_resolution> Mark;
		feedback_state_colours                   MarkColours;
		bool                                     KeyboardFocus = false;
	};
}
#endif
