#include "bpr/qt/pull_request_overview_page.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "conversation_glyphs.hpp"
#include <QAction>
#include <QActionGroup>

#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/steady_columns.hpp"
#include "button_surface.hpp"
#include "overview_summary_splitter.hpp"
#include "painted_glyph_icon.hpp"
#include <QLatin1StringView>
#include <QStringDecoder>
#include <QTextEdit>
#include <type_traits>

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QDateTime>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-1677]]. The files' keys for their top and bottom row,
		/// as a screen reader speaks them: "Command-Up Arrow and Command-Down
		/// Arrow go to the top and bottom row" on macOS.
		[[nodiscard]] auto files_ends_help() -> QString
		{
			const auto ends = shortcut_keys(shortcut_command::files_ends);
			return QStringLiteral("%1 and %2 go to the top and bottom row").arg(spoken_key(ends.value(0)), spoken_key(ends.value(1)));
		}

		class check_result_delegate final : public QStyledItemDelegate
		{
		public:
			explicit check_result_delegate(QTreeWidget* aView)
				: QStyledItemDelegate{ aView }
				, View{ aView }
			{ }
			auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto available = std::max(1, View->viewport()->width() - 8);
				const auto bounds    = aOption.fontMetrics.boundingRect(QRect{ 0, 0, available, 10000 }, Qt::TextWordWrap, aIndex.data().toString());
				return { available, bounds.height() + 6 };
			}
			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				option.features |= QStyleOptionViewItem::WrapText;
				option.textElideMode = Qt::ElideNone;
				QStyledItemDelegate::paint(aPainter, option, aIndex);
			}

		private:
			QTreeWidget* View;
		};

		auto full_view_icon(const QPalette& aPalette, const bool aExpanded) -> QIcon
		{
			return painted_glyph_icon([aExpanded](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				aPainter.setPen(QPen{ aInk, aSide / 10.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				const auto corner = aExpanded ? 0.56 : 0.8;
				const auto other  = aExpanded ? 0.44 : 0.2;
				aPainter.drawLine(QPointF{ 0.56 * aSide, other * aSide }, QPointF{ 0.8 * aSide, other * aSide });
				aPainter.drawLine(QPointF{ corner * aSide, 0.2 * aSide }, QPointF{ corner * aSide, 0.44 * aSide });
				aPainter.drawLine(QPointF{ 0.2 * aSide, (1.0 - other) * aSide }, QPointF{ 0.44 * aSide, (1.0 - other) * aSide });
				aPainter.drawLine(QPointF{ (1.0 - corner) * aSide, 0.56 * aSide }, QPointF{ (1.0 - corner) * aSide, 0.8 * aSide });
			},
									  aPalette.color(QPalette::Active, QPalette::ButtonText), aPalette.color(QPalette::Disabled, QPalette::ButtonText));
		}
	}
	// One shared reader keeps both native documents through normal/expanded layout.
	class overview_description_view final : public QWidget
	{
	public:
		explicit overview_description_view(QWidget* aParent)
			: QWidget{ aParent }
		{
			Reader = new markdown_body_view{ this };
			Reader->set_context_controls(true);
			auto* layout = new QVBoxLayout{ this };
			layout->setContentsMargins(0, 0, 0, 0);
			layout->addWidget(Reader);
			setFocusProxy(Reader);
			watch(Reader->rendered_view());
			watch(Reader->source_view());
		}
		markdown_body_view* Reader = nullptr;
		// The full-view toggle rides the reader's top-right corner, inside the
		// frame and left of the vertical scroll bar, so it takes no column or
		// row from the reader (owner, overview-v002 rejection).
		std::function<void()> LayoutChanged;
		void                  set_corner(QWidget* aCorner)
		{
			Corner = aCorner;
			Corner->installEventFilter(this);
			place_corner();
		}
		[[nodiscard]] auto line_height() const -> int
		{
			return Reader->source_visible() ? Reader->source_view()->fontMetrics().lineSpacing() : Reader->rendered_view()->fontMetrics().lineSpacing();
		}
		[[nodiscard]] auto chrome_height() const -> int
		{
			const auto viewport = Reader->source_visible() ? Reader->source_view()->viewport()->height() : Reader->rendered_view()->viewport()->height();
			return std::max(0, Reader->height() - viewport);
		}
		[[nodiscard]] auto natural_height() const -> int
		{
			if (!Reader->source_visible()) {
				return static_cast<int>(std::ceil(Reader->rendered_view()->document()->size().height())) + chrome_height();
			}
			// Only the content-aware default needs a measurement. Stop as soon as
			// the body exceeds all available room, including wrapped source lines.
			auto natural = static_cast<int>(2 * Reader->source_view()->document()->documentMargin());
			for (auto block = Reader->source_view()->document()->begin(); block.isValid() && natural < window()->height(); block = block.next()) {
				natural += std::max(1, block.layout()->lineCount()) * line_height();
			}
			return natural + chrome_height();
		}
		void present_body(const std::string& aText)
		{
			const auto changed = Reader->original_body() != aText;
			Adjusting          = true;
			Reader->set_body(aText, "description");
			if (changed) {
				RenderedPlace = { };
				SourcePlace   = { };
			}
			Adjusting = false;
			queue_layout();
		}
		void reset_reader()
		{
			Expanded      = false;
			RenderedPlace = { };
			SourcePlace   = { };
			Adjusting     = true;
			Reader->clear();
			Adjusting = false;
			queue_layout();
		}
		void set_expanded(const bool aExpanded)
		{
			remember();
			Expanded = aExpanded;
			setMinimumHeight(0);
			setMaximumHeight(QWIDGETSIZE_MAX);
			setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			queue_layout();
		}
		[[nodiscard]] auto expanded() const -> bool
		{
			return Expanded;
		}

	protected:
		bool event(QEvent* aEvent) override
		{
			if (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::FontChange
				|| aEvent->type() == QEvent::ApplicationFontChange || aEvent->type() == QEvent::StyleChange) {
				queue_layout();
			}
			const auto handled = QWidget::event(aEvent);
			if (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::LayoutRequest) {
				place_corner();
			}
			return handled;
		}
		bool eventFilter(QObject* aObject, QEvent* aEvent) override
		{
			if ((aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::MouseButtonPress || aEvent->type() == QEvent::MouseButtonRelease || aEvent->type() == QEvent::Wheel)) {
				ReadingInput = true;
			}

			if ((!Expanded && aEvent->type() == QEvent::Show) || aEvent->type() == QEvent::DevicePixelRatioChange) {
				queue_layout();
			}
			// A scroll bar appearing narrows the viewport; the corner follows it.
			if (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::Show || aEvent->type() == QEvent::Hide) {
				QTimer::singleShot(0, this, [this] { place_corner(); });
			}
			return QWidget::eventFilter(aObject, aEvent);
		}

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
		void remember()
		{
			if (Adjusting || Queued) {
				return;
			}
			if (Reader->source_visible()) {
				save(Reader->source_view(), SourcePlace);
			} else {
				save(Reader->rendered_view(), RenderedPlace);
			}
		}
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
		void queue_layout()
		{
			if (Queued || Reader == nullptr) {
				return;
			}
			Queued = true;
			QTimer::singleShot(0, this, [this] {
				// A reading gesture after layout was queued takes precedence over
				// the saved pre-layout place, including a new source fallback.
				if (ReadingInput) {
					if (Reader->source_visible()) {
						save(Reader->source_view(), SourcePlace);
					} else {
						save(Reader->rendered_view(), RenderedPlace);
					}
				}
				ReadingInput   = false;
				Adjusting      = true;
				auto* rendered = Reader->rendered_view();
				auto* source   = Reader->source_view();
				rendered->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
				source->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
				if (Reader->source_visible()) {
					restore(source, SourcePlace);
				} else {
					restore(rendered, RenderedPlace);
				}
				Adjusting = false;
				Queued    = false;
				place_corner();
				if (LayoutChanged) {
					LayoutChanged();
				}
			});
		}
		// The block property that keeps a reserved block's own right margin.
		static constexpr auto OwnRightMargin = QTextFormat::UserProperty + 1314;
		void                  place_corner()
		{
			if (Corner == nullptr || Reader == nullptr) {
				return;
			}
			QAbstractScrollArea* area = Reader->source_visible() ? static_cast<QAbstractScrollArea*>(Reader->source_view()) : Reader->rendered_view();
			const auto           size = Corner->sizeHint();
			const auto           top  = area->viewport()->mapTo(this, QPoint{ area->viewport()->width(), 0 });
			Corner->setGeometry(QRect{ QPoint{ top.x() - size.width(), top.y() }, size });
			Corner->raise();
			reserve_corner(Corner->isHidden() ? QSize{ } : size);
		}
		// Rendered lines beside the corner wrap short of it, so no word sits
		// under the toggle at rest; every later line keeps the full width. Each
		// block's top depends only on the blocks before it, so one ordered pass
		// settles. Source stays unwrapped and scrolls horizontally instead.
		void reserve_corner(const QSize aSize)
		{
			auto* const document  = Reader->rendered_view()->document();
			auto* const layout    = document->documentLayout();
			const auto  adjusting = std::exchange(Adjusting, true);
			for (auto block = document->begin(); block.isValid(); block = block.next()) {
				auto       format = block.blockFormat();
				const auto own    = format.hasProperty(OwnRightMargin) ? format.doubleProperty(OwnRightMargin) : format.rightMargin();
				const auto beside = !aSize.isEmpty() && layout->blockBoundingRect(block).top() < aSize.height();
				const auto wanted = beside ? own + aSize.width() : own;
				if (!beside && !format.hasProperty(OwnRightMargin)) {
					break;
				}
				if (format.rightMargin() != wanted || format.hasProperty(OwnRightMargin) != beside) {
					format.setRightMargin(wanted);
					if (beside) {
						format.setProperty(OwnRightMargin, own);
					} else {
						format.clearProperty(OwnRightMargin);
					}
					QTextCursor{ block }.setBlockFormat(format);
				}
			}
			Adjusting = adjusting;
		}
		QWidget*      Corner = nullptr;
		reading_place RenderedPlace, SourcePlace;
		bool          Expanded = false, Adjusting = false, Queued = false, ReadingInput = false;
	};

	namespace {
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto operation_letter(const application::overview_file_operation aOperation)
			-> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("A");
			case application::overview_file_operation::removed:
				return QStringLiteral("D");
			case application::overview_file_operation::renamed:
				return QStringLiteral("R");
			case application::overview_file_operation::copied:
				return QStringLiteral("C");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("T");
			case application::overview_file_operation::unchanged:
				return QStringLiteral("=");
			case application::overview_file_operation::modified:
				break;
			}
			return QStringLiteral("M");
		}

		[[nodiscard]] auto operation_name(const application::overview_file_operation aOperation)
			-> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("Added");
			case application::overview_file_operation::removed:
				return QStringLiteral("Removed");
			case application::overview_file_operation::renamed:
				return QStringLiteral("Renamed");
			case application::overview_file_operation::copied:
				return QStringLiteral("Copied");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("Type changed");
			case application::overview_file_operation::unchanged:
				return QStringLiteral("Unchanged");
			case application::overview_file_operation::modified:
				break;
			}
			return QStringLiteral("Modified");
		}

		[[nodiscard]] auto changed_text(const application::overview_change_since_review aChanged)
			-> QString
		{
			switch (aChanged) {
			case application::overview_change_since_review::changed:
				return QStringLiteral("changed");
			case application::overview_change_since_review::added:
				return QStringLiteral("new");
			case application::overview_change_since_review::unknown:
				return QStringLiteral("?");
			case application::overview_change_since_review::unchanged:
				break;
			}
			return { };
		}

		/// \brief [[BPR-1339]]. The read column's word: the row's mark, where
		/// accepted is stronger than read, seen and unread; unread with no
		/// receipt.
		[[nodiscard]] auto read_text(const application::overview_file_row& aRow) -> QString
		{
			if (!aRow.Mark.has_value()) {
				return QStringLiteral("unread");
			}
			const auto word = core::name(*aRow.Mark);
			return QString::fromLatin1(word.data(), static_cast<qsizetype>(word.size()));
		}

		[[nodiscard]] auto role_text(const application::overview_participant_role aRole) -> QString
		{
			switch (aRole) {
			case application::overview_participant_role::author:
				return QStringLiteral("author");
			case application::overview_participant_role::reviewer:
				return QStringLiteral("reviewer");
			case application::overview_participant_role::observer:
				break;
			}
			return QStringLiteral("observer");
		}

		[[nodiscard]] auto signoff_text(const application::overview_signoff aSignoff) -> QString
		{
			switch (aSignoff) {
			case application::overview_signoff::uploaded:
				return QStringLiteral("uploaded");
			case application::overview_signoff::commented:
				return QStringLiteral("commented");
			case application::overview_signoff::approved:
				return QStringLiteral("approved");
			case application::overview_signoff::changes_requested:
				return QStringLiteral("changes requested");
			case application::overview_signoff::dismissed:
				return QStringLiteral("review dismissed");
			case application::overview_signoff::pending:
				return QStringLiteral("review pending");
			case application::overview_signoff::none:
				break;
			}
			return { };
		}

		[[nodiscard]] auto count_text(const std::size_t aCount) -> QString
		{
			return QString::number(static_cast<qulonglong>(aCount));
		}

		[[nodiscard]] auto checks_text(const application::overview_check_summary& aChecks) -> QString
		{
			if (aChecks.total() == 0U) {
				return QStringLiteral("none");
			}
			auto parts = QStringList{ };
			if (aChecks.Passed != 0U) {
				parts.push_back(QStringLiteral("%1 passed").arg(count_text(aChecks.Passed)));
			}
			if (aChecks.Failed != 0U) {
				parts.push_back(QStringLiteral("%1 failed").arg(count_text(aChecks.Failed)));
			}
			if (aChecks.Pending != 0U) {
				parts.push_back(QStringLiteral("%1 pending").arg(count_text(aChecks.Pending)));
			}
			if (aChecks.Skipped != 0U) {
				parts.push_back(QStringLiteral("%1 skipped").arg(count_text(aChecks.Skipped)));
			}
			if (aChecks.Other != 0U) {
				parts.push_back(QStringLiteral("%1 other").arg(count_text(aChecks.Other)));
			}
			return parts.join(QStringLiteral(", "));
		}

		[[nodiscard]] auto heading_font(const QWidget& aWidget) -> QFont
		{
			auto font = aWidget.font();
			font.setPointSizeF(font.pointSizeF() * 0.9);
			font.setWeight(QFont::DemiBold);
			return font;
		}

		[[nodiscard]] auto make_heading(const QString& aText, const QString& aName, QWidget* const aParent)
			-> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setFont(heading_font(*aParent));
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

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

			void present(const QString& aText, const QString& aNote)
			{
				setText(aText);
				Note = aNote;
				refit();
			}

			[[nodiscard]] auto elided() const -> bool
			{
				return Elided;
			}

			[[nodiscard]] auto shown_text() const -> QString
			{
				return fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, contentsRect().width()));
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				// One character and the ellipsis: the row may narrow the
				// heading to almost nothing rather than widen the page.
				const auto hint = QLabel::minimumSizeHint();
				return QSize{ fontMetrics().horizontalAdvance(QStringLiteral("F…")), hint.height() };
			}

		protected:
			void resizeEvent(QResizeEvent* const aEvent) override
			{
				QLabel::resizeEvent(aEvent);
				refit();
			}

			void changeEvent(QEvent* const aEvent) override
			{
				QLabel::changeEvent(aEvent);
				if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
					refit();
				}
			}

			void paintEvent(QPaintEvent* const /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				style()->drawItemText(&painter, contentsRect(), static_cast<int>(alignment()), palette(), isEnabled(), shown_text(), foregroundRole());
			}

		private:
			void refit()
			{
				Elided = shown_text() != text();
				setAccessibleName(text());
				if (Elided) {
					setToolTip(Note.isEmpty() ? text() : text() + QStringLiteral("\n\n") + Note);
				} else {
					setToolTip(Note);
				}
				update();
			}

			QString Note;
			bool    Elided = false;
		};

		void present_heading(QLabel& aHeading, const QString& aText, const QString& aNote = { })
		{
			static_cast<eliding_heading&>(aHeading).present(aText, aNote);
		}

		[[nodiscard]] auto value_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("Uploads");
			case overview_value::files_changed:
				return QStringLiteral("Files changed");
			case overview_value::new_comments:
				return QStringLiteral("New comments");
			case overview_value::replies_to_me:
				return QStringLiteral("Replies to me");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("My unresolved threads");
			case overview_value::checks:
				return QStringLiteral("Checks");
			case overview_value::files_read:
				return QStringLiteral("Files read");
			case overview_value::private_notes:
				return QStringLiteral("Private notes");
			case overview_value::drafted_comments:
				break;
			}
			// [[BPR-1589]]. My comments not yet published, as Compose review
			// lists them.
			return QStringLiteral("Pending comments");
		}

		[[nodiscard]] auto value_object_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("overviewUploadsValue");
			case overview_value::files_changed:
				return QStringLiteral("overviewFilesChangedValue");
			case overview_value::new_comments:
				return QStringLiteral("overviewNewCommentsValue");
			case overview_value::replies_to_me:
				return QStringLiteral("overviewRepliesToMeValue");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("overviewMyUnresolvedThreadsValue");
			case overview_value::checks:
				return QStringLiteral("overviewChecksValue");
			case overview_value::files_read:
				return QStringLiteral("overviewFilesReadValue");
			case overview_value::private_notes:
				return QStringLiteral("overviewPrivateNotesValue");
			case overview_value::drafted_comments:
				break;
			}
			return QStringLiteral("overviewDraftedCommentsValue");
		}

		constexpr int chip_width = 58;
		/// [[BPR-1331]] v003. The read column's mark: a 24-unit design box this
		/// wide paints a mark ten pixels across, as the dot it replaced.
		constexpr qreal read_mark_side = 15.0;

		auto cached_side_text(core::cached_thread_side aSide) -> QString
		{
			switch (aSide) {
			case core::cached_thread_side::left:    return QStringLiteral("left");
			case core::cached_thread_side::right:   return QStringLiteral("right");
			case core::cached_thread_side::unknown: return QStringLiteral("unknown side");
			}
			return QStringLiteral("unknown side");
		}
		auto cached_thread_text(const std::vector<core::cached_review_thread>& aThreads) -> QString
		{
			QString result;
			for (const auto& thread : aThreads) {
				result += QStringLiteral("%1 · %2%3\n")
							  .arg(text(thread.Path), thread.Resolved ? QStringLiteral("was resolved") : QStringLiteral("was unresolved"),
								   thread.Outdated ? QStringLiteral(" · was outdated") : QString{ });
				if (thread.FileLevel) {
					result += QStringLiteral("File discussion\n");
				} else {
					const auto side = cached_side_text(thread.Side);
					result += QStringLiteral("Observed anchor: %1 · line %2 · original line %3\n")
								  .arg(side, thread.Line ? QString::number(*thread.Line) : QStringLiteral("unavailable"),
									   thread.OriginalLine ? QString::number(*thread.OriginalLine) : QStringLiteral("unavailable"));
				}
				if (thread.Comments.empty()) {
					result += QStringLiteral("No comments observed in this thread.\n");
				}
				for (const auto& comment : thread.Comments) {
					result += QStringLiteral("%1%2%3\n%4\n\n")
								  .arg(comment.ReplyTo ? QStringLiteral("Reply · ") : QString{ },
									   comment.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(comment.AuthorLogin),
									   comment.Outdated ? QStringLiteral(" · was outdated") : QString{ },
									   comment.Deleted ? QStringLiteral("Comment was deleted") : text(comment.Body));
				}
			}
			return result;
		}
		auto displayable_source(const std::optional<std::string>& aSource) -> bool
		{
			if (!aSource) {
				return true;
			}
			QStringDecoder decoder{ QStringDecoder::Utf8 };
			const QString  decoded = decoder(QByteArrayView{ aSource->data(), static_cast<qsizetype>(aSource->size()) });
			static_cast<void>(decoded);
			return !decoder.hasError();
		}
		auto cached_discussion_text(const std::optional<std::vector<core::cached_discussion_message>>& aMessages, const QString& aHeading) -> QString
		{
			if (!aMessages) {
				return aHeading + QStringLiteral(" — unavailable offline\n\n");
			}
			auto result = aHeading + QStringLiteral(" — %1 observed · freshness unknown\n").arg(count_text(aMessages->size()));
			for (const auto& message : *aMessages) {
				QString state;
				switch (message.Kind) {
				case core::cached_discussion_kind::general_comment:          break;
				case core::cached_discussion_kind::pending_review:           state = QStringLiteral("was pending"); break;
				case core::cached_discussion_kind::commented_review:         state = QStringLiteral("was commented"); break;
				case core::cached_discussion_kind::approved_review:          state = QStringLiteral("was approved"); break;
				case core::cached_discussion_kind::changes_requested_review: state = QStringLiteral("changes were requested"); break;
				case core::cached_discussion_kind::dismissed_review:         state = QStringLiteral("was dismissed"); break;
				}
				result += text(message.AuthorLogin.empty() ? "Unknown author" : message.AuthorLogin) + QStringLiteral(" · ") + state;
				if (message.Deleted) {
					result += QStringLiteral(" · was deleted");
				}
				if (message.CommitOid) {
					result += QStringLiteral("\nObserved review commit: ") + text(*message.CommitOid);
				}
				result += QStringLiteral("\n") + text(message.Body) + QStringLiteral("\n\n");
			}
			return result + QStringLiteral("\n");
		}
		auto cached_file_rows(const std::optional<std::vector<core::cached_review_file>>& aCatalogue)
			-> std::vector<application::overview_file_row>
		{
			std::vector<application::overview_file_row> rows;
			if (aCatalogue) {
				rows.reserve(aCatalogue->size());
				for (const auto& file : *aCatalogue) {
					auto       row       = application::overview_file_row{ .Path = file.Path, .PreviousPath = file.PreviousPath };
					const auto separator = file.Path.rfind('/');
					row.Folder           = separator == std::string::npos ? std::string{ } : file.Path.substr(0, separator);
					row.Name             = separator == std::string::npos ? file.Path : file.Path.substr(separator + 1);
					switch (file.Change) {
					case core::cached_file_change::added:        row.Operation = application::overview_file_operation::added; break;
					case core::cached_file_change::modified:     row.Operation = application::overview_file_operation::modified; break;
					case core::cached_file_change::removed:      row.Operation = application::overview_file_operation::removed; break;
					case core::cached_file_change::renamed:      row.Operation = application::overview_file_operation::renamed; break;
					case core::cached_file_change::copied:       row.Operation = application::overview_file_operation::copied; break;
					case core::cached_file_change::type_changed: row.Operation = application::overview_file_operation::type_changed; break;
					case core::cached_file_change::unchanged:    row.Operation = application::overview_file_operation::unchanged; break;
					}
					row.Additions = file.Additions;
					row.Deletions = file.Deletions;
					rows.push_back(std::move(row));
				}
			}
			return rows;
		}

		void expand_new_folders(QTreeView& aView, const QModelIndex& aParent, const int aFirst, const int aLast)
		{
			auto pending = std::vector<QModelIndex>{ };
			for (auto row = aFirst; row <= aLast; ++row) {
				pending.push_back(aView.model()->index(row, 0, aParent));
			}
			while (!pending.empty()) {
				const auto index = pending.back();
				pending.pop_back();
				if (!index.data(overview_file_model::folder_role).toBool()) {
					continue;
				}
				aView.expand(index);
				for (auto row = 0; row < aView.model()->rowCount(index); ++row) {
					pending.push_back(aView.model()->index(row, 0, index));
				}
			}
		}

		// [[BPR-1089]]. The Comments column's words and drawing.

		/// The "Not on a file" row's place among folder paths: no folder has it.
		constexpr auto remainder_path = QLatin1StringView{ "\x01not-on-a-file" };

		constexpr qreal chip_mark = 16.0;
		constexpr qreal chip_gap  = 5.0;

		[[nodiscard]] auto chip_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setPointSizeF(font.pointSizeF() * 0.85);
			font.setWeight(QFont::DemiBold);
			return font;
		}

		/// The space between a chip's mark and its count.
		constexpr qreal count_gap = 3.0;

		/// A count is its digits, wide enough for All's state bar beneath it.
		[[nodiscard]] auto count_width(const QFontMetrics& aMetrics, const std::size_t aCount) -> qreal
		{
			return std::max<qreal>(10.0, aMetrics.horizontalAdvance(QString::number(static_cast<qulonglong>(aCount))) + 2.0);
		}

		[[nodiscard]] auto state_of(const core::feedback_state_counts& aStates, const core::feedback_resolution aState) -> std::size_t
		{
			return aStates[static_cast<std::size_t>(aState)];
		}

		[[nodiscard]] auto mode_word(const core::feedback_resolution_mode aMode) -> QString
		{
			return aMode == core::feedback_resolution_mode::unresolved ? QStringLiteral("open") : QString{ };
		}

		[[nodiscard]] auto conversations(const std::size_t aCount) -> QString
		{
			return aCount == 1U ? QStringLiteral("thread") : QStringLiteral("threads");
		}

		/// "1 open, 1 resolved, 1 state unknown": a breakdown in the states'
		/// order, zeros left out.
		[[nodiscard]] auto breakdown_text(const core::feedback_state_counts& aStates) -> QString
		{
			auto parts = QStringList{ };
			for (std::size_t state = 0U; state < core::feedback_resolution_count; ++state) {
				if (aStates[state] != 0U) {
					parts.push_back(QStringLiteral("%1 %2").arg(count_text(aStates[state]), feedback_state_text(static_cast<core::feedback_resolution>(state))));
				}
			}
			return parts.join(QStringLiteral(", "));
		}

		/// The limits and the units the count leaves out, each as a clause in
		/// plain words: "2 whose state is unknown", "1 whose label can't be
		/// read" (owner, accepting `overview-chips-v001`: the earlier wording
		/// read as jargon).
		[[nodiscard]] auto qualifier_clauses(const core::feedback_counter_projection& aCounts) -> QStringList
		{
			auto clauses = QStringList{ };
			if (const auto unknown = state_of(aCounts.States, core::feedback_resolution::unknown); unknown != 0U) {
				clauses.push_back(QStringLiteral("%1 whose state is unknown").arg(count_text(unknown)));
			}
			if (aCounts.ScopeUnclassified != 0U) {
				clauses.push_back(QStringLiteral("%1 whose label can't be read").arg(count_text(aCounts.ScopeUnclassified)));
			}
			if (const auto none = state_of(aCounts.States, core::feedback_resolution::not_applicable); none != 0U && aCounts.Mode == core::feedback_resolution_mode::unresolved) {
				clauses.push_back(QStringLiteral("%1 that can't be resolved").arg(count_text(none)));
			}
			for (const auto limit : aCounts.Limits) {
				switch (limit) {
				case core::feedback_count_limit::collection_partial:
					clauses.push_back(QStringLiteral("not every comment was loaded"));
					break;
				case core::feedback_count_limit::collection_stale:
					clauses.push_back(QStringLiteral("counted at an earlier refresh"));
					break;
				case core::feedback_count_limit::collection_unavailable:
					clauses.push_back(QStringLiteral("comments couldn't be loaded"));
					break;
				case core::feedback_count_limit::attribution_unknown:
					clauses.push_back(QStringLiteral("some comments' files are unknown"));
					break;
				case core::feedback_count_limit::classification_partial:
				case core::feedback_count_limit::resolution_unknown:
					break; // said above, with their numbers
				}
			}
			return clauses;
		}

		[[nodiscard]] auto any_columns(const std::optional<application::overview_feedback_projection>& aFeedback) -> bool
		{
			return aFeedback.has_value() && aFeedback->Request.Columns.Count != 0U;
		}

		[[nodiscard]] auto chip_name(const overview_comment_chip& aChip, const bool aAnyColumns) -> QString
		{
			switch (aChip.Kind) {
			case overview_comment_chip::kind::label:
				return feedback_label_text(*aChip.Label);
			case overview_comment_chip::kind::other:
				return aAnyColumns ? QStringLiteral("Other") : QStringLiteral("Comments");
			case overview_comment_chip::kind::more:
				return QStringLiteral("More");
			case overview_comment_chip::kind::uncertain:
				break;
			}
			return QStringLiteral("Not counted");
		}

		/// One chip's tooltip, next to it: what it counts, in which state, and
		/// what it leaves out.
		[[nodiscard]] auto chip_tooltip(const overview_comment_chip& aChip, const core::feedback_counter_projection& aCounts, const bool aAnyColumns) -> QString
		{
			const auto open = aCounts.Mode == core::feedback_resolution_mode::unresolved;
			switch (aChip.Kind) {
			case overview_comment_chip::kind::label:
			case overview_comment_chip::kind::other: {
				const auto what  = aChip.Kind == overview_comment_chip::kind::label ? feedback_label_text(*aChip.Label) + QLatin1Char{ ' ' } : QString{ };
				auto       lines = QStringList{ };
				if (aChip.Kind == overview_comment_chip::kind::other && aAnyColumns) {
					// Other names what it holds: the labels Settings did not
					// tick, and comments with no label.
					lines.push_back(QStringLiteral("Other labels: %1 %2%3 here.").arg(count_text(aChip.Count), open ? QStringLiteral("open ") : QString{ }, conversations(aChip.Count)));
					lines.push_back(QStringLiteral("Labels not ticked in Settings' Overview chips, and comments with no label."));
				} else {
					lines.push_back(QStringLiteral("%1 %2%3%4 here.").arg(count_text(aChip.Count), open ? QStringLiteral("open ") : QString{ }, what, conversations(aChip.Count)));
				}
				if (!open) {
					lines.push_back(breakdown_text(aChip.States) + QLatin1Char{ '.' });
				}
				if (open) {
					const auto unknown = state_of(aChip.States, core::feedback_resolution::unknown);
					if (unknown != 0U) {
						lines.push_back(QStringLiteral("%1 more whose state is unknown, not counted.").arg(count_text(unknown)));
					}
				}
				lines.push_back(QStringLiteral("Replies don't count. Click or press Enter to list them."));
				return lines.join(QLatin1Char{ '\n' });
			}
			case overview_comment_chip::kind::more: {
				auto lines = QStringList{ };
				for (const auto& folded : aChip.Folded) {
					lines.push_back(QStringLiteral("%1 %2").arg(chip_name(folded, aAnyColumns), count_text(folded.Count)));
				}
				lines.push_back(QStringLiteral("Click or press Enter to choose one."));
				return lines.join(QLatin1Char{ '\n' });
			}
			case overview_comment_chip::kind::uncertain:
				break;
			}
			// Named as its menu entry is, then each reason on its own line.
			auto lines = QStringList{ aCounts.Uncertain == 0U ? QStringLiteral("These counts may be incomplete:")
															  : QStringLiteral("Not counted: %1 %2").arg(count_text(aCounts.Uncertain), conversations(aCounts.Uncertain)) };
			for (const auto& clause : qualifier_clauses(aCounts)) {
				lines.push_back(QStringLiteral("• ") + clause);
			}
			if (aCounts.Uncertain != 0U) {
				lines.push_back(QStringLiteral("Click or press Enter to list them."));
			}
			return lines.join(QLatin1Char{ '\n' });
		}

		/// A speech bubble: Comments with no chip chosen; with three dots in
		/// it, Other, the rest.
		void paint_bubble(QPainter& aPainter, const QRectF& aBox, const QColor& aInk, const bool aOther)
		{
			aPainter.save();
			aPainter.setRenderHint(QPainter::Antialiasing, true);
			const auto unit   = aBox.width() / 16.0;
			auto       bubble = QPainterPath{ };
			const auto body   = QRectF{ aBox.left() + 1.5 * unit, aBox.top() + 2.5 * unit, 13.0 * unit, 9.0 * unit };
			bubble.addRoundedRect(body, 2.5 * unit, 2.5 * unit);
			auto tail = QPainterPath{ };
			tail.moveTo(body.left() + 3.0 * unit, body.bottom() - 0.5 * unit);
			tail.lineTo(body.left() + 3.0 * unit, aBox.top() + 14.5 * unit);
			tail.lineTo(body.left() + 6.5 * unit, body.bottom() - 0.5 * unit);
			bubble = bubble.united(tail);
			aPainter.setPen(QPen{ aInk, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawPath(bubble);
			if (aOther) {
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aInk);
				for (const auto dot : { -3.0, 0.0, 3.0 }) {
					aPainter.drawEllipse(QPointF{ body.center().x() + dot * unit, body.center().y() }, 0.9 * unit, 0.9 * unit);
				}
			}
			aPainter.restore();
		}

		/// A key: the Label key's icon.
		void paint_key(QPainter& aPainter, const qreal aSide, const QColor& aInk)
		{
			const auto unit = aSide / 16.0;
			aPainter.setPen(QPen{ aInk, std::max<qreal>(1.25, aSide / 11.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawEllipse(QPointF{ 5.0 * unit, 8.0 * unit }, 3.0 * unit, 3.0 * unit);
			aPainter.drawLine(QPointF{ 8.0 * unit, 8.0 * unit }, QPointF{ 14.0 * unit, 8.0 * unit });
			aPainter.drawLine(QPointF{ 12.0 * unit, 8.0 * unit }, QPointF{ 12.0 * unit, 10.5 * unit });
			aPainter.drawLine(QPointF{ 14.0 * unit, 8.0 * unit }, QPointF{ 14.0 * unit, 10.5 * unit });
		}

		/// Whether the cell's qualifier is the unknown mark or, with only
		/// conversations that have no resolution, the not-resolvable one.
		[[nodiscard]] auto qualifier_state(const core::feedback_counter_projection& aCounts) -> core::feedback_resolution
		{
			const auto onlyUnresolvable = aCounts.Exact && state_of(aCounts.States, core::feedback_resolution::unknown) == 0U && aCounts.ScopeUnclassified == 0U;
			return onlyUnresolvable ? core::feedback_resolution::not_applicable : core::feedback_resolution::unknown;
		}
	} // namespace

	auto overview_operation_mark_colours(const application::overview_file_operation aOperation, const QPalette& aPalette, const overview_colours& aColours, const bool aSelected,
										 const QPalette::ColorGroup aGroup) -> overview_operation_mark
	{
		const auto base = aPalette.color(QPalette::Active, QPalette::Base);
		switch (aOperation) {
		case application::overview_file_operation::added:
			return { .Fill = aColours.Accepted, .Outline = { }, .Letter = base };
		case application::overview_file_operation::removed:
			return { .Fill = aColours.Defect, .Outline = { }, .Letter = base };
		case application::overview_file_operation::renamed:
		case application::overview_file_operation::copied:
		case application::overview_file_operation::type_changed:
			return { .Fill = aColours.Marker, .Outline = { }, .Letter = base };
		case application::overview_file_operation::modified:
		case application::overview_file_operation::unchanged:
			break;
		}
		// The selected frames' `.op`: the panel's raised tone, its rule and the
		// secondary ink, mixed from the row's ground and text. Mid is no tile
		// colour: in the dark theme it sits below the code ground, and Base's
		// letter on it read as a black square on a selected row (owner,
		// rejecting `overview-keyboard-v001`). On the highlight the mark is the
		// selection's own text, outlined, with no tile.
		const auto mixed = [](const QColor& aTone, const QColor& aGround, const qreal aShare) {
			const auto channel = [aShare](const float aTo, const float aFrom) { return aFrom + (aTo - aFrom) * static_cast<float>(aShare); };
			const auto tone    = aTone.toRgb();
			const auto ground  = aGround.toRgb();
			return QColor::fromRgbF(channel(tone.redF(), ground.redF()), channel(tone.greenF(), ground.greenF()), channel(tone.blueF(), ground.blueF()));
		};
		if (aSelected) {
			const auto ink = aPalette.color(aGroup, QPalette::HighlightedText);
			return { .Fill = { }, .Outline = mixed(ink, aPalette.color(aGroup, QPalette::Highlight), 0.6), .Letter = ink };
		}
		const auto ink = aPalette.color(QPalette::Active, QPalette::Text);
		return { .Fill = mixed(ink, base, 0.07), .Outline = mixed(ink, base, 0.32), .Letter = mixed(ink, base, 0.74) };
	}

	auto overview_row_band(const QPalette& aPalette, const QPalette::ColorGroup aGroup) -> QColor
	{
		const auto ground = aPalette.color(aGroup, QPalette::Base).toRgb();
		const auto ink    = aPalette.color(aGroup, QPalette::Text).toRgb();
		const auto toward = [&ground, &ink](const double aShare) {
			const auto channel = [aShare](const float aFrom, const float aTo) { return aFrom + ((aTo - aFrom) * static_cast<float>(aShare)); };
			return QColor::fromRgbF(channel(ground.redF(), ink.redF()), channel(ground.greenF(), ink.greenF()), channel(ground.blueF(), ink.blueF()));
		};
		const auto step = [&ground](const QColor& aColour) { return contrast_ratio(to_appearance_color(aColour), to_appearance_color(ground)); };
		if (step(ink) <= overview_row_band_contrast) {
			return ground;
		}
		// The ratio grows along the way from the ground to the text: halve the
		// interval to the first share that reaches the step.
		auto nearer  = 0.0;
		auto further = 1.0;
		for (auto pass = 0; pass < 16; ++pass) {
			const auto share                                                      = (nearer + further) / 2.0;
			(step(toward(share)) < overview_row_band_contrast ? nearer : further) = share;
		}
		return toward(further);
	}

	auto overview_count_name(const core::feedback_count_target& aTarget, const bool aAnyColumns) -> QString
	{
		switch (aTarget.Kind) {
		case core::feedback_count_kind::dedicated:
			return aTarget.Label ? feedback_label_text(*aTarget.Label) : QString{ };
		case core::feedback_count_kind::catchall:
			return aAnyColumns ? QStringLiteral("Other") : QStringLiteral("Comments");
		case core::feedback_count_kind::uncertain:
			return QStringLiteral("Not counted");
		case core::feedback_count_kind::included:
			break;
		}
		return QStringLiteral("Comments");
	}

	auto overview_comment_chips(const core::feedback_counter_projection& aCounts, const QRect& aCell, const QFont& aFont) -> std::vector<overview_comment_chip>
	{
		const auto metrics = QFontMetrics{ chip_font(aFont) };
		auto       chips   = std::vector<overview_comment_chip>{ };
		for (const auto& column : aCounts.Dedicated) {
			if (column.Count != 0U) {
				auto chip   = overview_comment_chip{ };
				chip.Kind   = overview_comment_chip::kind::label;
				chip.Label  = column.Label;
				chip.Count  = column.Count;
				chip.States = column.States;
				chips.push_back(std::move(chip));
			}
		}
		if (aCounts.Catchall != 0U) {
			auto chip   = overview_comment_chip{ };
			chip.Kind   = overview_comment_chip::kind::other;
			chip.Count  = aCounts.Catchall;
			chip.States = aCounts.CatchallStates;
			chips.push_back(std::move(chip));
		}
		const auto qualified = aCounts.Uncertain != 0U || !aCounts.Exact;
		const auto top       = static_cast<qreal>(aCell.center().y()) - chip_mark / 2.0 + 0.5;
		const auto left      = aCell.left() + 6.0;
		const auto right     = aCell.right() - 4.0 - (qualified ? chip_mark + 4.0 : 0.0);
		const auto widthOf   = [&](const overview_comment_chip& aChip) { return chip_mark + count_gap + count_width(metrics, aChip.Count); };
		auto       total     = 0.0;
		for (const auto& chip : chips) {
			total += widthOf(chip) + chip_gap;
		}
		auto shown = chips.size();
		if (left + total - chip_gap > right) {
			// Fold the tail into "+n" so the chips that are shown stay whole.
			const auto moreWidth = [&](const std::size_t aFolded) { return metrics.horizontalAdvance(QStringLiteral("+%1").arg(aFolded)) + 10.0; };
			auto       used      = 0.0;
			shown                = 0U;
			while (shown < chips.size() && left + used + widthOf(chips[shown]) + chip_gap + moreWidth(chips.size() - shown - 1U) <= right) {
				used += widthOf(chips[shown]) + chip_gap;
				++shown;
			}
		}
		auto result = std::vector<overview_comment_chip>{ };
		auto x      = left;
		for (std::size_t index = 0U; index < shown; ++index) {
			auto chip = chips[index];
			chip.Box  = QRectF{ x, top, widthOf(chip), chip_mark };
			x += chip.Box.width() + chip_gap;
			result.push_back(std::move(chip));
		}
		if (shown < chips.size()) {
			auto more   = overview_comment_chip{ };
			more.Kind   = overview_comment_chip::kind::more;
			more.Count  = chips.size() - shown;
			more.Folded = std::vector<overview_comment_chip>{ chips.begin() + static_cast<std::ptrdiff_t>(shown), chips.end() };
			more.Box    = QRectF{ x, top, metrics.horizontalAdvance(QStringLiteral("+%1").arg(more.Count)) + 10.0, chip_mark };
			result.push_back(std::move(more));
		}
		if (qualified) {
			auto qualifier  = overview_comment_chip{ };
			qualifier.Kind  = overview_comment_chip::kind::uncertain;
			qualifier.Count = aCounts.Uncertain;
			qualifier.Box   = QRectF{ aCell.right() - 4.0 - chip_mark, top, chip_mark, chip_mark };
			result.push_back(std::move(qualifier));
		}
		return result;
	}

	// ---- overview_file_model ------------------------------------------------

	overview_file_model::overview_file_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void overview_file_model::update_file(node& aNode, const application::overview_file_row& aRow)
	{
		if (aNode.File != aRow) {
			aNode.File = aRow;
			emit dataChanged(index_of(aNode), index_of(aNode, column_count - 1));
		}
	}

	void overview_file_model::build_tree(node& aRoot, const std::vector<application::overview_file_row>& aRows)
	{
		auto       folders = QHash<QString, node*>{ };
		const auto folder  = [&folders](node& aParent, const QString& aPath) {
			if (auto* found = folders.value(aPath, nullptr)) {
				return found;
			}
			auto child    = std::make_unique<node>();
			child->Path   = aPath;
			child->Parent = &aParent;
			child->Row    = static_cast<int>(aParent.Children.size());
			auto* result  = child.get();
			aParent.Children.push_back(std::move(child));
			folders.insert(aPath, result);
			return result;
		};
		// [[BPR-1213]]. Flat: one row per folder that holds files, named by
		// its whole path, so a folder with no file of its own takes no row and
		// every file sits one level down, at the same left column.
		for (const auto& row : aRows) {
			auto* const parent = folder(aRoot, text(row.Folder));
			auto        file   = std::make_unique<node>();
			file->Path         = text(row.Path);
			file->File         = row;
			file->Parent       = parent;
			file->Row          = static_cast<int>(parent->Children.size());
			parent->Children.push_back(std::move(file));
			for (auto* ancestor = parent; ancestor != nullptr; ancestor = ancestor->Parent) {
				++ancestor->FileCount;
			}
		}
	}

	void overview_file_model::update_rows(node& aParent, const int aFirst, const int aLast)
	{
		for (auto row = aFirst; row <= aLast; ++row) {
			aParent.Children[static_cast<std::size_t>(row)]->Row = row;
		}
	}

	void overview_file_model::reconcile(node& aParent, node& aDesired)
	{
		auto folders = QHash<QString, node*>{ };
		auto files   = QHash<QString, node*>{ };
		for (const auto& child : aParent.Children) {
			(child->File ? files : folders).insert(child->Path, child.get());
		}
		const auto parentIndex = index_of(aParent);
		for (std::size_t position = 0; position < aDesired.Children.size(); ++position) {
			auto&      desired  = aDesired.Children[position];
			auto*      existing = (desired->File ? files : folders).value(desired->Path, nullptr);
			const auto row      = static_cast<int>(position);
			if (existing == nullptr) {
				beginInsertRows(parentIndex, row, row);
				desired->Parent = &aParent;
				aParent.Children.insert(aParent.Children.begin() + row, std::move(desired));
				update_rows(aParent, row, static_cast<int>(aParent.Children.size()) - 1);
				endInsertRows();
				continue;
			}
			if (existing->Row != row) {
				const auto previous = existing->Row;
				beginMoveRows(parentIndex, previous, previous, parentIndex, row);
				std::rotate(aParent.Children.begin() + row, aParent.Children.begin() + previous,
							aParent.Children.begin() + previous + 1);
				update_rows(aParent, row, previous);
				endMoveRows();
			}
			if (desired->File) {
				update_file(*existing, *desired->File);
			} else {
				reconcile(*existing, *desired);
			}
		}
		const auto retained = static_cast<int>(aDesired.Children.size());
		if (aParent.Children.size() > aDesired.Children.size()) {
			beginRemoveRows(parentIndex, retained, static_cast<int>(aParent.Children.size()) - 1);
			aParent.Children.erase(aParent.Children.begin() + retained, aParent.Children.end());
			endRemoveRows();
		}
		if (aParent.FileCount != aDesired.FileCount) {
			aParent.FileCount = aDesired.FileCount;
			if (parentIndex.isValid()) {
				emit dataChanged(parentIndex, index_of(aParent, column_count - 1));
			}
		}
	}

	void overview_file_model::rebuild_lookup(const std::vector<application::overview_file_row>& aRows)
	{
		FileNodes.clear();
		FileOrder.clear();
		FolderCount        = 0U;
		LargestFolderCount = 0U;
		auto pending       = std::vector<node*>{ &Root };
		while (!pending.empty()) {
			auto* parent = pending.back();
			pending.pop_back();
			for (const auto& child : parent->Children) {
				if (child->File) {
					FileNodes.insert(child->Path, child.get());
				} else if (!child->Remainder) {
					++FolderCount;
					LargestFolderCount = std::max(LargestFolderCount, child->FileCount);
					pending.push_back(child.get());
				}
			}
		}
		FileOrder.reserve(aRows.size());
		for (const auto& row : aRows) {
			FileOrder.push_back(FileNodes.value(text(row.Path)));
		}
	}

	void overview_file_model::apply(const std::vector<application::overview_file_row>& aRows)
	{
		// Metadata-only refreshes retain the tree and allocate no replacement nodes.
		const auto sameOrder = std::ranges::equal(FileOrder, aRows,
												  [](const node* aNode, const application::overview_file_row& aRow) {
													  return aNode->File->Path == aRow.Path && aNode->File->Folder == aRow.Folder;
												  });
		if (sameOrder) {
			for (std::size_t row = 0; row < aRows.size(); ++row) {
				update_file(*FileOrder[row], aRows[row]);
			}
			return;
		}
		auto desired = node{ };
		build_tree(desired, aRows);
		// [[BPR-1089]]. The "Not on a file" row stays last, as it is.
		if (!Root.Children.empty() && Root.Children.back()->Remainder) {
			auto remainder       = std::make_unique<node>();
			remainder->Path      = remainder_path;
			remainder->Remainder = true;
			remainder->Parent    = &desired;
			remainder->Row       = static_cast<int>(desired.Children.size());
			desired.Children.push_back(std::move(remainder));
		}
		reconcile(Root, desired);
		rebuild_lookup(aRows);
	}

	void overview_file_model::apply_feedback(std::optional<application::overview_feedback_projection> aFeedback)
	{
		const auto had = Feedback.has_value();
		Feedback       = std::move(aFeedback);
		FileFeedback.clear();
		FolderFeedback.clear();
		if (Feedback) {
			for (std::size_t index = 0U; index < Feedback->Files.size(); ++index) {
				FileFeedback.insert(text(Feedback->Files[index].Path), index);
			}
			for (std::size_t index = 0U; index < Feedback->Folders.size(); ++index) {
				FolderFeedback.insert(text(Feedback->Folders[index].Path), index);
			}
		}
		sync_remainder();
		// Only the Comments column moves: one change per parent's run of rows.
		auto pending = std::vector<const node*>{ &Root };
		while (!pending.empty()) {
			const auto* parent = pending.back();
			pending.pop_back();
			if (!parent->Children.empty()) {
				emit dataChanged(index(0, comments_column, index_of(*parent)), index(static_cast<int>(parent->Children.size()) - 1, comments_column, index_of(*parent)));
			}
			for (const auto& child : parent->Children) {
				if (!child->File && !child->Remainder) {
					pending.push_back(child.get());
				}
			}
		}
		if (had != Feedback.has_value()) {
			emit headerDataChanged(Qt::Horizontal, comments_column, comments_column);
		}
	}

	void overview_file_model::sync_remainder()
	{
		const auto shown  = !Root.Children.empty() && Root.Children.back()->Remainder;
		const auto wanted = Feedback.has_value() && Feedback->Remainder.Counts.ScopeTotal != 0U;
		if (shown == wanted) {
			return;
		}
		const auto row = static_cast<int>(Root.Children.size()) - (shown ? 1 : 0);
		if (wanted) {
			beginInsertRows({ }, row, row);
			auto remainder       = std::make_unique<node>();
			remainder->Path      = remainder_path;
			remainder->Remainder = true;
			remainder->Parent    = &Root;
			remainder->Row       = row;
			Root.Children.push_back(std::move(remainder));
			endInsertRows();
		} else {
			beginRemoveRows({ }, row, row);
			Root.Children.pop_back();
			endRemoveRows();
		}
	}

	auto overview_file_model::feedback() const noexcept -> const std::optional<application::overview_feedback_projection>&
	{
		return Feedback;
	}

	auto overview_file_model::feedback_at(const QModelIndex& aIndex) const -> const core::feedback_counter_projection*
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr || !Feedback) {
			return nullptr;
		}
		if (item->Remainder) {
			return &Feedback->Remainder.Counts;
		}
		const auto& lookup = item->File ? FileFeedback : FolderFeedback;
		const auto  found  = lookup.constFind(item->Path);
		if (found == lookup.cend()) {
			return nullptr;
		}
		return item->File ? &Feedback->Files[*found].Counts : &Feedback->Folders[*found].Counts;
	}

	auto overview_file_model::feedback_row(const QModelIndex& aIndex) const
		-> std::optional<std::pair<application::overview_feedback_row_kind, std::string>>
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr || feedback_at(aIndex) == nullptr) {
			return std::nullopt;
		}
		if (item->Remainder) {
			return std::pair{ application::overview_feedback_row_kind::remainder, std::string{ } };
		}
		return std::pair{ item->File ? application::overview_feedback_row_kind::file : application::overview_feedback_row_kind::folder, item->Path.toStdString() };
	}

	auto overview_file_model::remainder_index(const int aColumn) const -> QModelIndex
	{
		return !Root.Children.empty() && Root.Children.back()->Remainder ? index_of(*Root.Children.back(), aColumn) : QModelIndex{ };
	}

	auto overview_file_model::comments_text(const core::feedback_counter_projection& aCounts, const bool aAccessible) const -> QString
	{
		const auto anyColumns = any_columns(Feedback);
		const auto all        = aCounts.Mode == core::feedback_resolution_mode::all;
		auto       parts      = QStringList{ };
		// In All each count says its breakdown, as its state bar shows it.
		const auto part = [&](const QString& aName, const std::size_t aCount, const core::feedback_state_counts& aStates) {
			parts.push_back(aAccessible && all ? QStringLiteral("%1 %2 (%3)").arg(aName, count_text(aCount), breakdown_text(aStates)) : QStringLiteral("%1 %2").arg(aName, count_text(aCount)));
		};
		for (const auto& column : aCounts.Dedicated) {
			if (column.Count != 0U) {
				part(feedback_label_text(column.Label), column.Count, column.States);
			}
		}
		if (aCounts.Catchall != 0U) {
			part(anyColumns ? QStringLiteral("Other") : QStringLiteral("Comments"), aCounts.Catchall, aCounts.CatchallStates);
		}
		if (!aAccessible) {
			if (aCounts.Uncertain != 0U || !aCounts.Exact) {
				parts.push_back(QStringLiteral("? %1").arg(count_text(aCounts.Uncertain)));
			}
			return parts.join(QStringLiteral(" · "));
		}
		const auto open    = mode_word(aCounts.Mode);
		auto       said    = parts.isEmpty() ? QStringLiteral("No %1threads").arg(open.isEmpty() ? QString{ } : open + QLatin1Char{ ' ' })
											 : QStringLiteral("%1 %2threads").arg(parts.join(QStringLiteral(", ")), open.isEmpty() ? QString{ } : open + QLatin1Char{ ' ' });
		const auto clauses = qualifier_clauses(aCounts);
		if (!clauses.isEmpty()) {
			said += QStringLiteral("; ") + clauses.join(QStringLiteral("; "));
		}
		return said;
	}

	void overview_file_model::update_rows(const std::span<const application::overview_file_row> aRows)
	{
		for (const auto& row : aRows) {
			if (auto* const file = FileNodes.value(text(row.Path), nullptr)) {
				update_file(*file, row);
			}
		}
	}

	auto overview_file_model::node_at(const QModelIndex& aIndex) const -> const node*
	{
		return aIndex.isValid() && aIndex.model() == this
			? static_cast<const node*>(aIndex.internalPointer())
			: nullptr;
	}

	auto overview_file_model::index_of(const node& aNode, const int aColumn) const -> QModelIndex
	{
		return &aNode == &Root ? QModelIndex{ }
							   : createIndex(aNode.Row, aColumn, const_cast<node*>(&aNode));
	}

	auto overview_file_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const
		-> QModelIndex
	{
		if (aRow < 0 || aColumn < 0 || aColumn >= column_count
			|| (aParent.isValid() && aParent.column() != 0)) {
			return { };
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent != nullptr && static_cast<std::size_t>(aRow) < parent->Children.size()
			? index_of(*parent->Children[static_cast<std::size_t>(aRow)], aColumn)
			: QModelIndex{ };
	}

	auto overview_file_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->Parent != nullptr ? index_of(*item->Parent) : QModelIndex{ };
	}

	auto overview_file_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (aParent.isValid() && aParent.column() != 0) {
			return 0;
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent == nullptr ? 0 : static_cast<int>(parent->Children.size());
	}

	auto overview_file_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return column_count;
	}

	auto overview_file_model::row_at(const QModelIndex& aIndex) const
		-> const application::overview_file_row*
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->File ? &*item->File : nullptr;
	}

	auto overview_file_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr) {
			return { };
		}
		if (aRole == remainder_role) {
			return item->Remainder;
		}
		const auto* counts = aIndex.column() == comments_column ? feedback_at(aIndex) : nullptr;
		if (counts != nullptr) {
			switch (aRole) {
			case Qt::DisplayRole:
				return comments_text(*counts, false);
			case Qt::AccessibleTextRole:
				return comments_text(*counts, true);
			case Qt::ToolTipRole:
				return comments_text(*counts, true) + QStringLiteral("\nTab steps onto each chip; Enter lists its threads.");
			default:
				break;
			}
		}
		if (item->Remainder) {
			if (aRole == folder_role) {
				return false;
			}
			if (aIndex.column() == file_column && aRole == Qt::DisplayRole) {
				return QStringLiteral("Not on a file");
			}
			if (aIndex.column() == file_column && (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleTextRole)) {
				return QStringLiteral("Not on a file: unanchored threads, pull request comments and review summaries");
			}
			return { };
		}
		if (!item->File) {
			if (aRole == folder_role) {
				return true;
			}
			if (aRole == file_count_role) {
				return static_cast<qulonglong>(item->FileCount);
			}
			if (aIndex.column() != file_column && aIndex.column() != operation_column) {
				return { };
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == file_column) {
				return item->Path.isEmpty() ? QStringLiteral("/") : item->Path;
			}
			if (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleTextRole) {
				return QStringLiteral("Folder %1, %2 files").arg(item->Path.isEmpty() ? QStringLiteral("/") : item->Path).arg(item->FileCount);
			}
			return { };
		}
		const auto* const row = row_at(aIndex);
		if (row == nullptr) {
			return { };
		}
		switch (aRole) {
		case folder_role:
			return false;
		case path_role:
			return text(row->Path);
		case operation_role:
			return static_cast<int>(row->Operation);
		case changed_role:
			return static_cast<int>(row->Changed);
		case read_role:
			return row->Read.has_value() ? static_cast<int>(*row->Read) : -1;
		case progress_role:
			return row->Mark.has_value() ? static_cast<int>(*row->Mark) : -1;
		case threads_role:
			return static_cast<qulonglong>(row->ThreadCount);
		case unresolved_role:
			return static_cast<qulonglong>(row->UnresolvedThreadCount);
		case Qt::TextAlignmentRole:
			if (aIndex.column() == added_column || aIndex.column() == removed_column) {
				return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
			}
			return { };
		case Qt::ToolTipRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_name(row->Operation);
			case file_column:
				return row->PreviousPath.has_value()
					? QStringLiteral("%1 (was %2)").arg(text(row->Path), text(*row->PreviousPath))
					: text(row->Path);
			case changed_column:
				return row->Changed == application::overview_change_since_review::unknown
					? QStringLiteral("Not yet resolved: compare since my last review to find out")
					: changed_text(row->Changed);
			case read_column:
				return read_text(*row);
			case comments_column:
				return QStringLiteral("%1 threads, %2 unresolved")
					.arg(count_text(row->ThreadCount), count_text(row->UnresolvedThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		case Qt::DisplayRole:
		case Qt::AccessibleTextRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_letter(row->Operation);
			case file_column:
				return text(row->Name);
			case added_column:
				return row->Additions == 0U ? QString{ } : QStringLiteral("+%1").arg(row->Additions);
			case removed_column:
				return row->Deletions == 0U ? QString{ } : QStringLiteral("−%1").arg(row->Deletions);
			case changed_column:
				return changed_text(row->Changed);
			case read_column:
				return aRole == Qt::AccessibleTextRole ? read_text(*row) : QString{ };
			case comments_column:
				if (row->ThreadCount == 0U) {
					return QString{ };
				}
				return row->UnresolvedThreadCount == 0U
					? count_text(row->ThreadCount)
					: QStringLiteral("%1 / %2").arg(count_text(row->UnresolvedThreadCount), count_text(row->ThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		default:
			return { };
		}
	}

	auto overview_file_model::headerData(const int aSection, const Qt::Orientation aOrientation, const int aRole) const
		-> QVariant
	{
		if (aOrientation != Qt::Horizontal) {
			return { };
		}
		// [[BPR-1089]]. Comments ▾ chooses the number the rows sort by.
		if (aSection == comments_column && Feedback && (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleDescriptionRole)) {
			const auto open = Feedback->Request.Mode == core::feedback_resolution_mode::unresolved;
			auto       tip  = QStringLiteral("%1 by label, in your Settings order; %2. Replies don't count.\nClick a chip, or Tab onto it and press Enter, to list its threads.")
								  .arg(open ? QStringLiteral("Open threads") : QStringLiteral("Threads"),
									   any_columns(Feedback) ? QStringLiteral("the bubble with dots holds the other labels") : QStringLiteral("no labels are ticked, so the bubble counts them all"));
			tip += Feedback->Request.Sort ? QStringLiteral("\nSorted by %1, most first. Click to change.").arg(overview_count_name(*Feedback->Request.Sort, any_columns(Feedback)))
										  : QStringLiteral("\nClick to sort by a label.");
			return tip;
		}
		if (aRole != Qt::DisplayRole) {
			return { };
		}
		if (aSection == comments_column && Feedback) {
			return Feedback->Request.Sort ? QStringLiteral("Comments ↓ ▾") : QStringLiteral("Comments ▾");
		}
		switch (aSection) {
		case operation_column:
			return QString{ };
		case file_column:
			return QStringLiteral("File");
		case added_column:
			return QStringLiteral("Added");
		case removed_column:
			return QStringLiteral("Removed");
		case changed_column:
			return QStringLiteral("Changed");
		case read_column:
			return QStringLiteral("Read");
		case comments_column:
			return QStringLiteral("Comments");
		case note_column:
			return QStringLiteral("Note");
		default:
			return { };
		}
	}

	auto overview_file_model::folder_count() const noexcept -> std::size_t
	{
		return FolderCount;
	}

	auto overview_file_model::file_count() const noexcept -> std::size_t
	{
		return FileOrder.size();
	}

	auto overview_file_model::largest_folder_file_count() const noexcept -> std::size_t
	{
		return LargestFolderCount;
	}

	auto overview_file_model::index_for_path(const QString& aPath) const -> QModelIndex
	{
		const auto* file = FileNodes.value(aPath, nullptr);
		return file == nullptr ? QModelIndex{ } : index_of(*file, file_column);
	}

	// ---- overview_row_delegate ----------------------------------------------

	overview_row_delegate::overview_row_delegate(const overview_colours& aColours, QObject* const aParent)
		: QStyledItemDelegate{ aParent }
		, Colours{ aColours }
	{
	}

	auto overview_row_delegate::banded(const QModelIndex& aIndex, const QWidget* const aView) -> bool
	{
		const auto* const tree = qobject_cast<const QTreeView*>(aView);
		const auto        row  = aIndex.siblingAtColumn(0);
		const auto        heading
			= [](const QModelIndex& aRow) { return aRow.data(overview_file_model::folder_role).toBool() || aRow.data(overview_file_model::remainder_role).toBool(); };
		if (tree == nullptr || !row.isValid() || heading(row)) {
			return false;
		}
		// The rows shown between this one and the header its group starts
		// at. A file sits under its folder's header, and with uniform rows
		// the view's own geometry counts them: a hidden or collapsed row has
		// none, and the count costs no walk over the folder's earlier files.
		const auto parent = row.parent();
		if (parent.isValid()) {
			const auto header = tree->visualRect(parent);
			const auto own    = tree->visualRect(row);
			if (!header.isValid() || own.height() <= 0) {
				return false;
			}
			return ((own.top() - header.bottom() - 1) / own.height()) % 2 == 0;
		}
		// At the top level a heading row ends the group.
		auto shown = 0;
		for (auto before = row.row() - 1; before >= 0; --before) {
			if (!parent.isValid() && heading(row.sibling(before, 0))) {
				break;
			}
			if (!tree->isRowHidden(before, parent)) {
				++shown;
			}
		}
		return shown % 2 == 0;
	}

	void overview_row_delegate::initStyleOption(QStyleOptionViewItem* const aOption, const QModelIndex& aIndex) const
	{
		QStyledItemDelegate::initStyleOption(aOption, aIndex);
		if ((aOption->state & QStyle::State_Selected) != 0 || !banded(aIndex, aOption->widget)) {
			return;
		}
		const auto active   = overview_row_band(aOption->palette, QPalette::Active);
		const auto inactive = overview_row_band(aOption->palette, QPalette::Inactive);
		aOption->palette.setColor(QPalette::Active, QPalette::Base, active);
		aOption->palette.setColor(QPalette::Inactive, QPalette::Base, inactive);
		aOption->backgroundBrush = (aOption->state & QStyle::State_Active) != 0 ? active : inactive;
	}

	namespace {
		/// [[BPR-1363]] v002. A folder's file count, in the row's regular
		/// weight with tabular figures, so equal digit counts are equally wide
		/// and right-aligned counts line up digit for digit.
		[[nodiscard]] auto folder_count_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setWeight(QFont::Normal);
			font.setFeature(QFont::Tag{ "tnum" }, 1U);
			return font;
		}

		[[nodiscard]] auto folder_count_text(const QModelIndex& aIndex) -> QString
		{
			return QString::number(aIndex.data(overview_file_model::file_count_role).toULongLong());
		}

		/// The width of the count slot: the largest count's digits, as zeros.
		[[nodiscard]] auto folder_count_width(const QFont& aFont, const QModelIndex& aIndex) -> int
		{
			const auto* const model   = dynamic_cast<const overview_file_model*>(aIndex.model());
			const auto        largest = model != nullptr ? model->largest_folder_file_count() : aIndex.data(overview_file_model::file_count_role).toULongLong();
			const auto        digits  = QString::number(largest).size();
			return QFontMetrics{ folder_count_font(aFont) }.horizontalAdvance(QString(digits, QLatin1Char('0')));
		}

		/// The space between the count and the name that follows it.
		[[nodiscard]] auto folder_count_gap(const QFont& aFont) -> int
		{
			return QFontMetrics{ aFont }.horizontalAdvance(QLatin1Char(' '));
		}
	} // namespace

	auto overview_row_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QSize
	{
		const auto folder = aIndex.data(overview_file_model::folder_role).toBool();
		auto       option = aOption;
		if (folder && aIndex.column() == overview_file_model::operation_column) {
			// [[BPR-1363]] v002. A folder row's first cell holds its disclosure
			// mark, then its count's slot and the space before the name.
			const auto width = disclosure_width(aOption) + folder_count_width(aOption.font, aIndex) + folder_count_gap(aOption.font);
			return QSize{ width, std::max(QStyledItemDelegate::sizeHint(option, aIndex).height(), 22) };
		}
		const auto folderPath = folder && aIndex.column() == overview_file_model::file_column;
		if (folderPath) {
			// Measured in the weight it is painted in, so the path is never elided.
			option.font.setWeight(QFont::DemiBold);
		}
		auto hint = QStyledItemDelegate::sizeHint(option, aIndex);
		hint.setHeight(std::max(hint.height(), 22));
		return hint;
	}

	auto overview_row_delegate::disclosure_width(const QStyleOptionViewItem& aOption) -> int
	{
		const auto* const style = aOption.widget != nullptr ? aOption.widget->style() : QApplication::style();
		return style->pixelMetric(QStyle::PM_TreeViewIndentation, &aOption, aOption.widget);
	}

	auto overview_row_delegate::disclosure_rect(const QStyleOptionViewItem& aOption) -> QRect
	{
		return QRect{ aOption.rect.left(), aOption.rect.top(), disclosure_width(aOption), aOption.rect.height() };
	}

	auto overview_row_delegate::count_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QRect
	{
		return QRect{ aOption.rect.left() + disclosure_width(aOption), aOption.rect.top(), folder_count_width(aOption.font, aIndex), aOption.rect.height() };
	}

	void overview_row_delegate::paint_disclosure(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		// [[BPR-1213]]. The rows stay flat, so the view has no indentation to
		// draw its branch in; the folder row draws the native disclosure mark
		// itself. [[BPR-1363]]: in its first cell, so the mark is the leftmost
		// thing on the row (owner, 2026-10-09). v002: the file count follows
		// it, right-aligned in a slot every folder row shares, so the counts
		// read as one column (owner, session d827fdb7).
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		option.text.clear();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		const auto* const tree     = qobject_cast<const QTreeView*>(option.widget);
		const auto        expanded = tree != nullptr && tree->isExpanded(aIndex.siblingAtColumn(0));
		auto              branch   = QStyleOption{ };
		branch.QStyleOption::operator=(option);
		branch.rect  = disclosure_rect(option);
		branch.state = (option.state & (QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected | QStyle::State_MouseOver))
			| QStyle::State_Children | QStyle::State_Item | (expanded ? QStyle::State_Open : QStyle::State_None);
		style->drawPrimitive(QStyle::PE_IndicatorBranch, &branch, aPainter, option.widget);

		const auto group = option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
		aPainter->save();
		aPainter->setClipRect(option.rect);
		aPainter->setFont(folder_count_font(option.font));
		aPainter->setPen(option.state & QStyle::State_Selected ? option.palette.color(group, QPalette::HighlightedText) : option.palette.color(group, QPalette::PlaceholderText));
		aPainter->drawText(count_rect(option, aIndex), Qt::AlignRight | Qt::AlignVCenter, folder_count_text(aIndex));
		aPainter->restore();
	}

	void overview_row_delegate::paint_folder(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		// The folder's whole path, where every file name starts; its file
		// count is in the row's first cell ([[BPR-1363]] v002).
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		const auto        text  = option.text;
		option.text.clear();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		const auto group      = option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
		auto       label      = option;
		label.text            = text;
		label.backgroundBrush = Qt::NoBrush;
		label.state &= ~(QStyle::State_HasFocus | QStyle::State_Selected);
		label.font.setWeight(QFont::DemiBold);
		label.palette.setColor(QPalette::Text, option.state & QStyle::State_Selected ? option.palette.color(group, QPalette::HighlightedText) : option.palette.color(group, QPalette::PlaceholderText));
		style->drawControl(QStyle::CE_ItemViewItem, &label, aPainter, label.widget);
	}

	void overview_row_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		// [[BPR-1089]]. While a chip of this row has the keyboard, its ring is
		// the only focus mark: the current cell draws no frame of its own.
		if (ChipFocus && ChipFocus->Row.isValid() && ChipFocus->Row == aIndex.siblingAtColumn(0) && (aOption.state & QStyle::State_HasFocus) != 0) {
			auto option = aOption;
			option.state &= ~QStyle::State_HasFocus;
			paint_cell(aPainter, option, aIndex);
			return;
		}
		paint_cell(aPainter, aOption, aIndex);
	}

	void overview_row_delegate::paint_cell(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		const auto folder = aIndex.data(overview_file_model::folder_role).toBool();
		const auto column = aIndex.column();
		// [[BPR-1089]]. The label chips, on file, folder and remainder rows alike.
		if (column == overview_file_model::comments_column) {
			const auto* model  = dynamic_cast<const overview_file_model*>(aIndex.model());
			const auto* counts = model != nullptr ? model->feedback_at(aIndex) : nullptr;
			if (counts != nullptr) {
				auto option = aOption;
				initStyleOption(&option, aIndex);
				option.text.clear();
				const auto* style = option.widget != nullptr ? option.widget->style() : QApplication::style();
				style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
				paint_chips(aPainter, option, *counts, model->feedback()->Request.Mode);
				return;
			}
		}
		// [[BPR-1585]]. The Not on a file row is no file: it has no operation,
		// change or read state to mark, only its name and its chips.
		const auto remainder = aIndex.data(overview_file_model::remainder_role).toBool();
		const auto marked    = !folder && !remainder
			&& (column == overview_file_model::operation_column
				|| column == overview_file_model::changed_column
				|| column == overview_file_model::read_column
				|| column == overview_file_model::comments_column);
		if (folder && column == overview_file_model::operation_column) {
			paint_disclosure(aPainter, aOption, aIndex);
			return;
		}
		if (folder && column == overview_file_model::file_column) {
			paint_folder(aPainter, aOption, aIndex);
			return;
		}
		if (!marked) {
			auto option = aOption;
			initStyleOption(&option, aIndex);
			if (folder) {
				option.font.setWeight(QFont::DemiBold);
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::PlaceholderText));
			}
			if (!folder && column == overview_file_model::added_column) {
				option.palette.setColor(QPalette::Text, Colours.Accepted);
			}
			if (!folder && column == overview_file_model::removed_column) {
				option.palette.setColor(QPalette::Text, Colours.Defect);
			}
			if (!folder && column == overview_file_model::note_column) {
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::PlaceholderText));
			}
			if (remainder) {
				option.font.setWeight(QFont::DemiBold);
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::PlaceholderText));
			}
			const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
			style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
			return;
		}
		// The background and selection as the style draws them, then the mark.
		auto option = aOption;
		initStyleOption(&option, aIndex);
		option.text.clear();
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		const auto rect = option.rect;
		const auto ink  = option.palette.color(QPalette::Active, QPalette::Text);
		auto       font = option.font;
		font.setPointSizeF(font.pointSizeF() * 0.85);
		font.setWeight(QFont::DemiBold);
		aPainter->setFont(font);
		const auto metrics = QFontMetrics{ font };
		const auto chip    = [&](const QString& aText, const QColor& aFill, const QColor& aText2, const int aWidth) {
			const auto box = QRectF{ rect.left() + 6.0, rect.center().y() - 8.0, static_cast<qreal>(aWidth), 16.0 };
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(aFill);
			aPainter->drawRoundedRect(box, 8.0, 8.0);
			aPainter->setPen(aText2);
			aPainter->drawText(box, Qt::AlignCenter, aText);
		};
		switch (column) {
		case overview_file_model::operation_column: {
			const auto operation = static_cast<application::overview_file_operation>(
				aIndex.data(overview_file_model::operation_role).toInt());
			const auto selected = (option.state & QStyle::State_Selected) != 0;
			const auto group    = (option.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
			const auto mark     = overview_operation_mark_colours(operation, option.palette, Colours, selected, group);
			const auto box      = QRectF{ rect.left() + 6.0, rect.center().y() - 7.0, 14.0, 14.0 };
			aPainter->setPen(mark.Outline.isValid() ? QPen{ mark.Outline, 1.0 } : QPen{ Qt::NoPen });
			aPainter->setBrush(mark.Fill.isValid() ? QBrush{ mark.Fill } : QBrush{ Qt::NoBrush });
			aPainter->drawRoundedRect(mark.Outline.isValid() ? box.adjusted(0.5, 0.5, -0.5, -0.5) : box, 3.0, 3.0);
			aPainter->setPen(mark.Letter);
			aPainter->drawText(box, Qt::AlignCenter, operation_letter(operation));
			break;
		}
		case overview_file_model::changed_column: {
			const auto changed = static_cast<application::overview_change_since_review>(
				aIndex.data(overview_file_model::changed_role).toInt());
			if (changed == application::overview_change_since_review::changed) {
				chip(QStringLiteral("changed"), Colours.Unread, option.palette.color(QPalette::Active, QPalette::Base), chip_width);
			} else if (changed == application::overview_change_since_review::added) {
				chip(QStringLiteral("new"), Colours.Accepted, option.palette.color(QPalette::Active, QPalette::Base), chip_width);
			} else if (changed == application::overview_change_since_review::unknown) {
				aPainter->setPen(option.palette.color(QPalette::Active, QPalette::PlaceholderText));
				aPainter->drawText(rect.adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("?"));
			}
			break;
		}
		case overview_file_model::read_column: {
			// [[BPR-1331]] v003. The review bar's marks, one family wherever a
			// file's progress shows: a ring unread (and with no receipt), half
			// filled seen, a disc read, a disc with a tick cut out accepted, a
			// ring struck through excluded, each in its own theme colour and
			// its own shape, so the state survives without its colour.
			const auto mark   = aIndex.data(overview_file_model::progress_role).toInt();
			const auto centre = QPointF{ rect.left() + 13.0, static_cast<qreal>(rect.center().y()) };
			const auto shown  = mark < 0 ? core::file_progress_mark::unread : static_cast<core::file_progress_mark>(mark);
			paint_progress_mark(*aPainter, QRectF{ centre.x() - (read_mark_side / 2.0), centre.y() - (read_mark_side / 2.0), read_mark_side, read_mark_side }, shown, Colours.Progress);
			break;
		}
		case overview_file_model::comments_column: {
			const auto threads    = aIndex.data(overview_file_model::threads_role).toULongLong();
			const auto unresolved = aIndex.data(overview_file_model::unresolved_role).toULongLong();
			auto       left       = rect.left() + 6.0;
			const auto bubble     = [&](const QString& aText, const QColor& aFill, const QColor& aText2) {
				const auto width = std::max(18, metrics.horizontalAdvance(aText) + 8);
				const auto box   = QRectF{ left, rect.center().y() - 7.0, static_cast<qreal>(width), 14.0 };
				aPainter->setPen(Qt::NoPen);
				aPainter->setBrush(aFill);
				aPainter->drawRoundedRect(box, 7.0, 7.0);
				aPainter->setPen(aText2);
				aPainter->drawText(box, Qt::AlignCenter, aText);
				left += width + 4.0;
			};
			if (unresolved != 0U) {
				bubble(QString::number(unresolved), Colours.Marker, option.palette.color(QPalette::Active, QPalette::Base));
			}
			if (threads != 0U) {
				bubble(QString::number(threads), option.palette.color(QPalette::Active, QPalette::Mid), ink);
			}
			break;
		}
		default:
			break;
		}
		aPainter->restore();
	}

	void overview_row_delegate::paint_chips(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const core::feedback_counter_projection& aCounts,
											const core::feedback_resolution_mode aMode) const
	{
		const auto chips = overview_comment_chips(aCounts, aOption.rect, aOption.font);
		if (chips.empty()) {
			return; // a complete zero: blank, and nothing else is blank
		}
		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		const auto  selected   = (aOption.state & QStyle::State_Selected) != 0;
		const auto  group      = (aOption.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
		const auto  ink        = aOption.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
		const auto* model      = dynamic_cast<const overview_file_model*>(aOption.index.model());
		const auto  anyColumns = model != nullptr && any_columns(model->feedback());
		const auto  open       = aMode == core::feedback_resolution_mode::unresolved;
		aPainter->setFont(chip_font(aOption.font));
		for (const auto& chip : chips) {
			const auto& box = chip.Box;
			switch (chip.Kind) {
			case overview_comment_chip::kind::label:
			case overview_comment_chip::kind::other: {
				const auto mark = QRectF{ box.left(), box.top(), chip_mark, chip_mark };
				if (chip.Kind == overview_comment_chip::kind::label) {
					paint_feedback_mark(*aPainter, mark, *chip.Label, { }, feedback_mark_colours_for(Colours.Marks, *chip.Label, aOption.palette));
				} else {
					paint_bubble(*aPainter, mark, ink, anyColumns);
				}
				// The count is its digits beside the mark, as the selected design
				// draws it: in Open, the open amber on every row; in All, the
				// text ink with the state breakdown beneath it. A selected row
				// keeps its highlighted ink, so the digits stay legible.
				const auto pill = QRectF{ mark.right() + count_gap, box.top() + (open ? 0.5 : 0.0), box.width() - chip_mark - count_gap, open ? 15.0 : 13.5 };
				aPainter->setPen(open && !selected ? Colours.States.Open : ink);
				aPainter->drawText(pill, Qt::AlignLeft | Qt::AlignVCenter, QString::number(static_cast<qulonglong>(chip.Count)));
				if (!open) {
					// Open, resolved, unknown and not resolvable, in proportion.
					const auto total = std::accumulate(chip.States.begin(), chip.States.end(), std::size_t{ 0U });
					const auto bar   = QRectF{ pill.left(), pill.bottom() + 0.75, pill.width(), 2.5 };
					const auto fills = std::array{ Colours.States.Open, Colours.States.Resolved, aOption.palette.color(group, QPalette::PlaceholderText), aOption.palette.color(group, QPalette::Mid) };
					auto       x     = bar.left();
					aPainter->setPen(Qt::NoPen);
					for (std::size_t state = 0U; state < chip.States.size() && total != 0U; ++state) {
						const auto width = bar.width() * static_cast<qreal>(chip.States[state]) / static_cast<qreal>(total);
						if (width > 0.0) {
							aPainter->setBrush(fills[state]);
							aPainter->drawRect(QRectF{ x, bar.top(), width, bar.height() });
							x += width;
						}
					}
				}
				break;
			}
			case overview_comment_chip::kind::more:
				aPainter->setBrush(Qt::NoBrush);
				aPainter->setPen(QPen{ aOption.palette.color(group, QPalette::Mid), 1.0 });
				aPainter->drawRoundedRect(box.adjusted(0.5, 1.0, -0.5, -1.0), 7.0, 7.0);
				aPainter->setPen(ink);
				aPainter->drawText(box, Qt::AlignCenter, QStringLiteral("+%1").arg(chip.Count));
				break;
			case overview_comment_chip::kind::uncertain: {
				auto states  = Colours.States;
				states.Faint = selected ? ink : feedback_state_faint(aOption.palette, group);
				paint_feedback_state_mark(*aPainter, box, qualifier_state(aCounts), states);
				break;
			}
			}
		}
		// The chip the keyboard is on wears a ring, in the selection's own
		// text colour on the selected row, while the files have focus.
		const auto focused = ChipFocus && ChipFocus->Row.isValid() && ChipFocus->Row == aOption.index.siblingAtColumn(0) && aOption.widget != nullptr && aOption.widget->hasFocus();
		if (focused) {
			const auto found = std::ranges::find_if(chips, [this](const overview_comment_chip& aChip) { return aChip.Kind == ChipFocus->Kind && aChip.Label == ChipFocus->Label; });
			if (found != chips.end()) {
				aPainter->setBrush(Qt::NoBrush);
				aPainter->setPen(QPen{ aOption.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Highlight), 1.5 });
				aPainter->drawRoundedRect(found->Box.adjusted(-3.0, -2.0, 3.0, 2.0), 5.0, 5.0);
			}
		}
		aPainter->restore();
	}

	void overview_row_delegate::set_chip_focus(std::optional<overview_chip_focus> aFocus)
	{
		ChipFocus = std::move(aFocus);
	}

	namespace {
		/// \brief [[BPR-1405]]. The merge section said once, for a polite
		/// announcement when an accepted refresh changes it.
		[[nodiscard]] auto merge_announcement(const application::pull_request_overview_projection& aProjection) -> QString
		{
			auto parts = QStringList{ QStringLiteral("Merge: %1").arg(text(aProjection.MergeState)) };
			for (const auto& condition : aProjection.MergeConditions) {
				parts.push_back(text(condition));
			}
			if (!aProjection.ReviewRequests.empty()) {
				auto names = QStringList{ };
				for (const auto& request : aProjection.ReviewRequests) {
					names.push_back(request.Name.empty() ? QStringLiteral("an unavailable reviewer") : text(request.Name));
				}
				parts.push_back(QStringLiteral("Waiting on %1").arg(names.join(QStringLiteral(", "))));
			}
			return parts.join(QStringLiteral(". ")) + QLatin1Char{ '.' };
		}

		/// \brief One roster line: an avatar, the name, and a short coloured
		/// word at the right, over a rule. A circle is a person; [[BPR-1405]] a
		/// rounded square is a team. [[BPR-1583]] The avatar is the shared one,
		/// in the person's own colour.
		struct roster_line
		{
			QString              Name;
			identity_avatar_face Face;
			QString              Detail;
			QColor               DetailInk;
			/// [[BPR-1405]]. Requests keep both ends: teams of one organization
			/// differ only in their slug.
			Qt::TextElideMode Elide = Qt::ElideRight;
		};

		void paint_roster_line(QWidget& aRow, const roster_line& aLine)
		{
			auto painter = QPainter{ &aRow };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto& colours = aRow.palette();
			const auto  centre  = QPointF{ 11.0, aRow.height() / 2.0 };
			const auto  tile    = QRectF{ centre.x() - 9.0, centre.y() - 9.0, 18.0, 18.0 };
			paint_identity_avatar(painter, tile, aLine.Face, identity_avatar_grounds_of(colours), aRow.font());
			// One compact line; the whole name stays in the tooltip and the
			// accessible name when space is tight.
			painter.setFont(aRow.font());
			const auto metrics     = aRow.fontMetrics();
			const auto detailWidth = std::min(metrics.horizontalAdvance(aLine.Detail), std::max(0, aRow.width() - 60));
			painter.setPen(aLine.DetailInk);
			painter.drawText(QRect{ aRow.width() - detailWidth - 4, 0, detailWidth, aRow.height() },
							 Qt::AlignVCenter | Qt::AlignRight, aLine.Detail);
			painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
			const auto nameWidth = std::max(0, aRow.width() - detailWidth - 44);
			painter.drawText(QRect{ 28, 0, nameWidth, aRow.height() }, Qt::AlignVCenter | Qt::AlignLeft,
							 metrics.elidedText(aLine.Name, aLine.Elide, nameWidth));
			// The rule beneath.
			painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Mid), 1.0 });
			painter.drawLine(QPointF{ 0.0, aRow.height() - 0.5 }, QPointF{ static_cast<qreal>(aRow.width()), aRow.height() - 0.5 });
		}

		[[nodiscard]] auto roster_height(const QWidget& aRow) -> int
		{
			return std::max(28, aRow.fontMetrics().height() + 10);
		}

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
			explicit counts_mode_button(QWidget* const aParent)
				: QToolButton{ aParent }
			{
				setAttribute(Qt::WA_Hover, true);
			}

			/// \brief The mark drawn before the word, in \p aColours.
			void set_mark(const core::feedback_resolution aState, const feedback_state_colours& aColours)
			{
				Mark        = aState;
				MarkColours = aColours;
				updateGeometry();
				update();
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = fontMetrics();
				const auto across  = (2 * counts_mode_padding) + mark_advance() + metrics.horizontalAdvance(text());
				// The height Qt gives a tool button with a 16 px icon beside its
				// words, as the ? count has.
				auto option = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.toolButtonStyle = Qt::ToolButtonTextBesideIcon;
				const auto contents    = QSize{ across, std::max(option.iconSize.height(), metrics.height()) };
				const auto tall        = style()->sizeFromContents(QStyle::CT_ToolButton, &option, contents, this).height();
				return QSize{ across, std::max(tall, metrics.height() + 6) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return sizeHint();
			}

		protected:
			void focusInEvent(QFocusEvent* const aEvent) override
			{
				// A click leaves no ring that reads as a choice; Tab, an arrow
				// within the switch or a shortcut does. A window coming back
				// keeps what was shown.
				update_button_keyboard_focus(KeyboardFocus, *aEvent);
				QToolButton::focusInEvent(aEvent);
			}

			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto& colours = palette();
				const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
				paint_button_surface(painter, *this, isChecked(), KeyboardFocus);
				const auto metrics = fontMetrics();
				const auto content = mark_advance() + metrics.horizontalAdvance(text());
				auto       x       = static_cast<qreal>(width() - content) / 2.0;
				if (Mark) {
					auto marks = MarkColours;
					if (!isEnabled()) {
						marks.Open = colours.color(QPalette::Disabled, QPalette::WindowText);
					}
					paint_feedback_state_mark(painter, QRectF{ x, (height() - feedback_mark_size) / 2.0, feedback_mark_size, feedback_mark_size }, *Mark, marks);
					x += static_cast<qreal>(mark_advance());
				}
				// A whole-pixel baseline, as a label sets its words.
				const int baseline = (height() + metrics.ascent() - metrics.descent()) / 2;
				painter.setFont(font());
				painter.setPen(isEnabled() ? ink : colours.color(QPalette::Disabled, QPalette::WindowText));
				painter.drawText(QPointF{ x, static_cast<qreal>(baseline) }, text());
			}

		private:
			static constexpr int counts_mode_padding = 6;
			static constexpr int counts_mode_gap     = 4;
			/// The width the mark and its gap take before the word.
			[[nodiscard]] auto mark_advance() const -> int
			{
				return Mark ? static_cast<int>(feedback_mark_size) + counts_mode_gap : 0;
			}

			std::optional<core::feedback_resolution> Mark;
			feedback_state_colours                   MarkColours;
			bool                                     KeyboardFocus = false;
		};
	} // namespace

	// ---- overview_participant_row -------------------------------------------

	overview_participant_row::overview_participant_row(
		const application::overview_participant& aParticipant,
		const overview_colours&                  aColours,
		QWidget* const                           aParent)
		: QWidget{ aParent }
		, Participant{ aParticipant }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("overviewParticipant"));
		setAccessibleName(QStringLiteral("%1, %2, %3")
							  .arg(login(), role_text(aParticipant.Role), standing_text()));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setToolTip(accessibleName());
	}

	auto overview_participant_row::login() const -> QString
	{
		return text(Participant.Login);
	}

	auto overview_participant_row::standing_text() const -> QString
	{
		return signoff_text(Participant.Signoff);
	}

	auto overview_participant_row::sizeHint() const -> QSize
	{
		return { 200, heightForWidth(200) };
	}

	auto overview_participant_row::minimumSizeHint() const -> QSize
	{
		return { 0, heightForWidth(width()) };
	}

	auto overview_participant_row::heightForWidth(const int aWidth) const -> int
	{
		static_cast<void>(aWidth);
		return roster_height(*this);
	}

	void overview_participant_row::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		const auto& colours = palette();
		// The standing is coloured by what it means.
		auto standing = colours.color(QPalette::Active, QPalette::PlaceholderText);
		switch (Participant.Signoff) {
		case application::overview_signoff::approved:
			standing = Colours.Accepted;
			break;
		case application::overview_signoff::changes_requested:
			// [[BPR-1589]]. Red is blocking (RFC-BPR-0085 principle 4), as the face's mark.
			standing = Colours.Defect;
			break;
		case application::overview_signoff::dismissed:
		case application::overview_signoff::pending:
		case application::overview_signoff::uploaded:
		case application::overview_signoff::commented:
		case application::overview_signoff::none:
			break;
		}
		// [[BPR-1583]]. Mine is ringed, as Threads rings it.
		const auto face = identity_avatar_face{ .Identity = login(), .Ring = Participant.Viewer ? colours.color(QPalette::Active, QPalette::PlaceholderText) : QColor{ } };
		paint_roster_line(*this, { .Name = login(), .Face = face, .Detail = standing_text().isEmpty() ? role_text(Participant.Role) : standing_text(), .DetailInk = standing });
	}

	// ---- overview_request_row -----------------------------------------------

	overview_request_row::overview_request_row(
		const application::overview_review_request& aRequest,
		const overview_colours&                     aColours,
		QWidget* const                              aParent)
		: QWidget{ aParent }
		, Request{ aRequest }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("overviewReviewRequest"));
		const auto name  = Request.Name.empty() ? QStringLiteral("Reviewer unavailable") : text(Request.Name);
		auto       label = Request.Team ? QStringLiteral("%1, team").arg(name) : name;
		if (!Request.DisplayName.empty()) {
			label += QStringLiteral(" (%1)").arg(text(Request.DisplayName));
		}
		setAccessibleName(QStringLiteral("%1, %2").arg(label, Request.CodeOwner ? QStringLiteral("code-owner review requested") : QStringLiteral("review requested")));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setToolTip(accessibleName());
	}

	auto overview_request_row::request() const noexcept -> const application::overview_review_request&
	{
		return Request;
	}

	auto overview_request_row::sizeHint() const -> QSize
	{
		return { 200, heightForWidth(200) };
	}

	auto overview_request_row::minimumSizeHint() const -> QSize
	{
		return { 0, heightForWidth(width()) };
	}

	auto overview_request_row::heightForWidth(const int aWidth) const -> int
	{
		static_cast<void>(aWidth);
		return roster_height(*this);
	}

	void overview_request_row::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		const auto& colours = palette();
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		const auto  name    = Request.Name.empty() ? QStringLiteral("Reviewer unavailable") : text(Request.Name);
		// [[BPR-1583]]. A team is keyed on organization/slug and initialled from
		// its slug; a hidden reviewer is the neutral face.
		const auto identity = text(Request.Name);
		const auto face     = identity_avatar_face{ .Identity     = identity,
													.InitialsFrom = Request.Team ? identity.mid(identity.indexOf(QLatin1Char{ '/' }) + 1) : QString{ },
													.Shape        = Request.Team ? identity_avatar_shape::group : identity_avatar_shape::person };
		paint_roster_line(*this, { .Name = name, .Face = face, .Detail = Request.CodeOwner ? QStringLiteral("code owner") : QStringLiteral("requested"), .DetailInk = Request.CodeOwner ? Colours.Waiting : faint, .Elide = Qt::ElideMiddle });
	}

	// ---- pull_request_overview_page -----------------------------------------

	pull_request_overview_page::pull_request_overview_page(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchOverviewPage"));
		setAccessibleName(QStringLiteral("Overview"));
		setAccessibleDescription(QStringLiteral(
			"What changed in the pull request and who has signed off, with my "
			"review progress and the way in"));

		auto* const main = new QWidget{ this };
		main->setObjectName(QStringLiteral("overviewMainColumn"));
		auto* const mainColumn = new QVBoxLayout{ main };
		mainColumn->setContentsMargins(18, 12, 18, 12);
		mainColumn->setSpacing(4);
		Header             = new QWidget{ main };
		auto* headerLayout = new QVBoxLayout{ Header };
		headerLayout->setContentsMargins(0, 0, 0, 0);
		headerLayout->setSpacing(4);
		mainColumn->addWidget(Header);

		Title = new QLabel{ main };
		Title->setObjectName(QStringLiteral("overviewTitle"));
		Title->setAccessibleName(QStringLiteral("Pull request title"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		headerLayout->addWidget(Title);

		Summary = new QLabel{ main };
		Summary->setObjectName(QStringLiteral("overviewSummary"));
		Summary->setAccessibleName(QStringLiteral("Pull request author and branches"));
		Summary->setTextFormat(Qt::PlainText);
		Summary->setWordWrap(true);
		Summary->setForegroundRole(QPalette::PlaceholderText);
		headerLayout->addWidget(Summary);
		// [[BPR-0671]]. The commit oids behind the branches, disclosed. They
		// were in the files drawer's summary box, which the overview replaces;
		// they belong beside the branches they identify.
		TechnicalRow = new QHBoxLayout;
		TechnicalRow->setContentsMargins(0, 0, 0, 0);
		TechnicalRow->setSpacing(6);
		headerLayout->addLayout(TechnicalRow);

		SummarySplit = new overview_summary_splitter{ main };
		SummarySplit->setObjectName(QStringLiteral("overviewSummarySplitter"));
		mainColumn->addWidget(SummarySplit, 1);
		SummaryPane = new QWidget{ SummarySplit };
		SummaryPane->setObjectName(QStringLiteral("overviewSummaryPane"));
		auto* summaryColumn = new QVBoxLayout{ SummaryPane };
		summaryColumn->setContentsMargins(0, 0, 0, 0);
		summaryColumn->setSpacing(0);
		Body = new overview_description_view{ SummaryPane };
		Body->setObjectName(QStringLiteral("overviewBody"));
		Body->setAccessibleName(QStringLiteral("Pull request description"));
		Body->Reader->setAccessibleName(QStringLiteral("Pull request description"));
		summaryColumn->addWidget(Body, 1);
		// [[BPR-1774]]. "No description", and beside it, for the author,
		// Add a description.
		auto* const emptyRow    = new QWidget{ main };
		auto* const emptyLayout = new QHBoxLayout{ emptyRow };
		emptyLayout->setContentsMargins(0, 0, 0, 0);
		emptyLayout->setSpacing(8);
		EmptyBody = new QLabel{ QStringLiteral("No description"), emptyRow };
		EmptyBody->setObjectName(QStringLiteral("overviewNoDescription"));
		emptyLayout->addWidget(EmptyBody);
		DescriptionAdd = new quiet_push_button{ QStringLiteral("Add a description"), emptyRow };
		DescriptionAdd->setObjectName(QStringLiteral("overviewAddDescription"));
		DescriptionAdd->setAccessibleDescription(QStringLiteral("Write this pull request's description. Nothing is sent until you save."));
		DescriptionAdd->hide();
		emptyLayout->addWidget(DescriptionAdd);
		emptyLayout->addStretch(1);
		mainColumn->insertWidget(mainColumn->indexOf(SummarySplit), emptyRow);
		// [[BPR-1774]]. The reader's corner holds Edit, for the author, beside
		// Full view: both act on the description they sit on.
		DescriptionCorner = new QWidget{ Body };
		DescriptionCorner->setObjectName(QStringLiteral("overviewDescriptionCorner"));
		auto* const cornerLayout = new QHBoxLayout{ DescriptionCorner };
		cornerLayout->setContentsMargins(0, 0, 0, 0);
		cornerLayout->setSpacing(0);
		DescriptionEdit = new QToolButton{ DescriptionCorner };
		DescriptionEdit->setObjectName(QStringLiteral("overviewEditDescription"));
		DescriptionEdit->setAutoRaise(true);
		DescriptionEdit->setFocusPolicy(Qt::StrongFocus);
		DescriptionEdit->setText(QStringLiteral("Edit description"));
		DescriptionEdit->setAccessibleName(DescriptionEdit->text());
		DescriptionEdit->setAccessibleDescription(QStringLiteral("Edit this pull request's description in Markdown. Nothing is sent until you save."));
		DescriptionEdit->setToolTip(DescriptionEdit->text());
		DescriptionEdit->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		DescriptionEdit->setBackgroundRole(QPalette::Base);
		DescriptionEdit->setAutoFillBackground(true);
		DescriptionEdit->hide();
		cornerLayout->addWidget(DescriptionEdit);
		FullView = new QToolButton{ DescriptionCorner };
		cornerLayout->addWidget(FullView);
		FullView->setObjectName(QStringLiteral("overviewFullView"));
		// Not checkable: Exit keeps Enter's quiet auto-raised style and only the
		// glyph, name and tooltip change (owner, overview-v003 rejection).
		FullView->setAutoRaise(true);
		FullView->setFocusPolicy(Qt::StrongFocus);
		FullView->setText(QStringLiteral("Enter full view"));
		FullView->setAccessibleName(FullView->text());
		FullView->setToolTip(FullView->text());
		FullView->setIcon(full_view_icon(palette(), false));
		// The reader's own paper behind the glyph, so text scrolling under the
		// corner never shows through it.
		FullView->setBackgroundRole(QPalette::Base);
		FullView->setAutoFillBackground(true);
		Body->set_corner(DescriptionCorner);
		connect(FullView, &QToolButton::clicked, this, [this] { set_description_expanded(!Body->expanded()); });
		connect(DescriptionEdit, &QToolButton::clicked, this, [this] { static_cast<void>(begin_description_edit()); });
		connect(DescriptionAdd, &QPushButton::clicked, this, [this] { static_cast<void>(begin_description_edit()); });
		// [[BPR-1774]]. The editor takes the reader's place in the summary
		// pane: the description is edited where it is read. The app's Markdown
		// prose editor keeps the body's untouched bytes (CRLF included), so a
		// save changes only what was typed.
		DescriptionPanel = new QWidget{ SummaryPane };
		DescriptionPanel->setObjectName(QStringLiteral("overviewDescriptionEditor"));
		DescriptionPanel->setAccessibleName(QStringLiteral("Edit description"));
		auto* const panelColumn = new QVBoxLayout{ DescriptionPanel };
		panelColumn->setContentsMargins(0, 2, 0, 2);
		panelColumn->setSpacing(6);
		auto* const hint = new QLabel{ QStringLiteral("Markdown. Nothing is sent to GitHub until you save."), DescriptionPanel };
		hint->setObjectName(QStringLiteral("overviewDescriptionHint"));
		hint->setForegroundRole(QPalette::PlaceholderText);
		hint->setTextFormat(Qt::PlainText);
		panelColumn->addWidget(hint);
		DescriptionText = new scoped_prose_editor{ DescriptionPanel };
		DescriptionText->setObjectName(QStringLiteral("overviewDescriptionText"));
		DescriptionText->setAccessibleName(QStringLiteral("Description Markdown source"));
		DescriptionText->setPlaceholderText(QStringLiteral("Describe what this pull request changes and why."));
		DescriptionText->setTabChangesFocus(true);
		DescriptionText->setMinimumHeight(6 * DescriptionText->fontMetrics().lineSpacing());
		panelColumn->addWidget(DescriptionText, 1);
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(8);
		DescriptionStatus = new QLabel{ DescriptionPanel };
		DescriptionStatus->setObjectName(QStringLiteral("overviewDescriptionStatus"));
		DescriptionStatus->setTextFormat(Qt::PlainText);
		DescriptionStatus->setWordWrap(true);
		DescriptionStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		foot->addWidget(DescriptionStatus, 1);
		DescriptionNewer = new quiet_push_button{ QStringLiteral("Edit GitHub's newer text"), DescriptionPanel };
		DescriptionNewer->setObjectName(QStringLiteral("overviewDescriptionNewer"));
		DescriptionNewer->setAccessibleDescription(QStringLiteral("Replaces your text with the description GitHub has now, so you can edit from it."));
		DescriptionNewer->hide();
		foot->addWidget(DescriptionNewer);
		DescriptionCancel = new quiet_push_button{ QStringLiteral("Cancel"), DescriptionPanel };
		DescriptionCancel->setObjectName(QStringLiteral("overviewDescriptionCancel"));
		DescriptionCancel->setAccessibleDescription(QStringLiteral("Stops editing. Nothing is sent."));
		foot->addWidget(DescriptionCancel);
		DescriptionSave = new QPushButton{ QStringLiteral("Save"), DescriptionPanel };
		DescriptionSave->setObjectName(QStringLiteral("overviewDescriptionSave"));
		DescriptionSave->setAccessibleDescription(QStringLiteral("Saves the description to GitHub."));
		DescriptionSave->setDefault(true);
		DescriptionSave->setToolTip(QStringLiteral("Save (%1)").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
		foot->addWidget(DescriptionSave);
		panelColumn->addLayout(foot);
		summaryColumn->addWidget(DescriptionPanel, 1);
		DescriptionPanel->hide();
		const auto save = [this] {
			if (!DescriptionEditing || DescriptionSaving || !DescriptionSaveDispatch) {
				return;
			}
			DescriptionSaveDispatch(overview_description_save{ .Original = DescriptionOriginal, .Body = DescriptionText->raw() });
		};
		connect(DescriptionSave, &QPushButton::clicked, this, save);
		auto* const saveKey = new QShortcut{ QKeySequence{ Qt::CTRL | Qt::Key_Return }, DescriptionPanel };
		saveKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(saveKey, &QShortcut::activated, this, save);
		connect(DescriptionCancel, &QPushButton::clicked, this, [this] { close_description_editor(); });
		auto* const cancelKey = new QShortcut{ QKeySequence{ Qt::Key_Escape }, DescriptionPanel };
		cancelKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(cancelKey, &QShortcut::activated, this, [this] {
			if (!DescriptionSaving) {
				close_description_editor();
			}
		});
		connect(DescriptionNewer, &QPushButton::clicked, this, [this] {
			if (!DescriptionNewerText.has_value()) {
				return;
			}
			DescriptionOriginal = std::move(*DescriptionNewerText);
			DescriptionNewerText.reset();
			DescriptionText->present(DescriptionOriginal, true);
			DescriptionNewer->hide();
			present_description_status(QStringLiteral("Editing GitHub's newer text."));
			DescriptionText->setFocus(Qt::OtherFocusReason);
		});
		Body->LayoutChanged = [this] { SummarySplit->queue_layout(); };
		FilesPane           = new QWidget{ SummarySplit };
		FilesPane->setObjectName(QStringLiteral("overviewFilesPane"));
		auto* filesColumn = new QVBoxLayout{ FilesPane };
		filesColumn->setContentsMargins(0, 0, 0, 0);
		filesColumn->setSpacing(4);
		SummarySplit->addWidget(SummaryPane);
		SummarySplit->addWidget(FilesPane);
		SummarySplit->LineHeight    = [this] { return Body->line_height(); };
		SummarySplit->NaturalHeight = [this] {
			// [[BPR-1774]]. The editor asks for room to write in.
			if (DescriptionEditing) {
				return std::max(DescriptionPanel->sizeHint().height(), 16 * DescriptionText->fontMetrics().lineSpacing());
			}
			return std::max(Body->natural_height(), FullView->sizeHint().height() + Body->chrome_height());
		};
		SummarySplit->SummaryMinimum = [this] {
			if (DescriptionEditing) {
				return DescriptionPanel->minimumSizeHint().height();
			}
			return std::max(3 * Body->line_height(), FullView->sizeHint().height()) + Body->chrome_height();
		};
		SummarySplit->FilesMinimum = [this] { return FilesHeading->sizeHint().height() + 4 + Files->header()->sizeHint().height() + 3 * std::max(Files->fontMetrics().height() + 8, Files->sizeHintForRow(0)); };
		auto* escape               = new QShortcut{ shortcut_key(shortcut_command::fold_description), this };
		escape->setContext(Qt::WidgetWithChildrenShortcut);
		escape->setEnabled(false);
		connect(escape, &QShortcut::activated, this, [this] { set_description_expanded(false); });
		escape->setObjectName(QStringLiteral("overviewDescriptionEscape"));

		{
			auto* const heading = new eliding_heading{ main };
			heading->setObjectName(QStringLiteral("overviewFilesHeading"));
			heading->setTextFormat(Qt::PlainText);
			heading->setFont(heading_font(*main));
			heading->setForegroundRole(QPalette::PlaceholderText);
			heading->present(QStringLiteral("Files"), { });
			FilesHeading = heading;
		}

		// [[BPR-1269]]. Collapse all and expand all, as glyphs beside the heading
		// with their names in tooltips; local to the page, like one folder's.
		auto* const headingRow = new QHBoxLayout{ };
		headingRow->setContentsMargins(0, 0, 0, 0);
		headingRow->setSpacing(2);
		headingRow->addWidget(FilesHeading);
		headingRow->addStretch(1);
		const auto folderButton = [main](const QString& aName, const QString& aObjectName) {
			auto* const button = new QToolButton{ main };
			button->setObjectName(aObjectName);
			button->setAccessibleName(aName);
			button->setToolTip(aName);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setIconSize(QSize{ 16, 16 });
			button->hide();
			return button;
		};
		CollapseFolders = folderButton(QStringLiteral("Collapse all folders"), QStringLiteral("overviewCollapseFolders"));
		ExpandFolders   = folderButton(QStringLiteral("Expand all folders"), QStringLiteral("overviewExpandFolders"));
		connect(CollapseFolders, &QToolButton::clicked, this, [this] { collapse_all_folders(); });
		connect(ExpandFolders, &QToolButton::clicked, this, [this] { expand_all_folders(); });
		// [[BPR-1089]]. Open or All, the ? chip and the Label key (owner,
		// BPR-1079 Overview states), with their names in tooltips.
		// [[BPR-1577]]. Each half is drawn by the page: the chosen one a quiet
		// chip, the other its word alone.
		const auto countsButton = [main](const QString& aText, const QString& aName, const QString& aAccessible, const QString& aTip) {
			auto* const button = new counts_mode_button{ main };
			button->setObjectName(aName);
			button->setText(aText);
			button->setAccessibleName(aAccessible);
			button->setToolTip(aTip);
			button->setCheckable(true);
			button->setAutoExclusive(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setIconSize(QSize{ 16, 16 });
			button->hide();
			anchor_tooltips(*button);
			return button;
		};
		OpenCounts = countsButton(QStringLiteral("Open"), QStringLiteral("overviewModeOpen"), QStringLiteral("Open threads"), QStringLiteral("Unresolved threads (default)"));
		AllCounts  = countsButton(QStringLiteral("All"), QStringLiteral("overviewModeAll"), QStringLiteral("All threads"), QStringLiteral("Every thread: open, resolved, unknown"));
		AllCounts->setToolButtonStyle(Qt::ToolButtonTextOnly);
		connect(OpenCounts, &QToolButton::toggled, this, [this](const bool aOn) {
			if (aOn) {
				choose_mode(core::feedback_resolution_mode::unresolved);
			}
		});
		connect(AllCounts, &QToolButton::toggled, this, [this](const bool aOn) {
			if (aOn) {
				choose_mode(core::feedback_resolution_mode::all);
			}
		});
		Uncertain = new QToolButton{ main };
		Uncertain->setObjectName(QStringLiteral("overviewUncertain"));
		Uncertain->setAutoRaise(true);
		Uncertain->setFocusPolicy(Qt::StrongFocus);
		Uncertain->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Uncertain->setIconSize(QSize{ 16, 16 });
		Uncertain->hide();
		anchor_tooltips(*Uncertain);
		connect(Uncertain, &QToolButton::clicked, this, [this] { drill(application::overview_feedback_row_kind::pull_request, { }, { .Kind = core::feedback_count_kind::uncertain }); });
		LabelKey = folderButton(QStringLiteral("Label key"), QStringLiteral("overviewLabelKey"));
		LabelKey->setToolTip(QStringLiteral("Label key (%1)").arg(shortcut_key(shortcut_command::label_key).toString(QKeySequence::NativeText)));
		LabelKey->setAccessibleDescription(QStringLiteral("Lists every label mark with its name, and the state marks"));
		anchor_tooltips(*LabelKey);
		connect(LabelKey, &QToolButton::clicked, this, [this] { show_label_key(); });
		auto* const keyShortcut = new QShortcut{ shortcut_key(shortcut_command::label_key), this };
		keyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
		keyShortcut->setObjectName(QStringLiteral("overviewLabelKeyShortcut"));
		connect(keyShortcut, &QShortcut::activated, this, [this] {
			if (Model->feedback()) {
				show_label_key();
			}
		});
		headingRow->addWidget(OpenCounts);
		headingRow->addWidget(AllCounts);
		headingRow->addSpacing(4);
		headingRow->addWidget(Uncertain);
		headingRow->addWidget(LabelKey);
		headingRow->addSpacing(6);
		headingRow->addWidget(CollapseFolders);
		headingRow->addWidget(ExpandFolders);
		filesColumn->addLayout(headingRow);
		// RFC-BPR-0089. What the commit filter shows, and Clear all filters.
		CommitChip = new commit_filter_chip{ false, main };
		CommitChip->set_clear_dispatch([this] {
			if (CommitFilterDispatch) {
				CommitFilterDispatch(application::commit_filter_gesture{ .Kind = core::commit_filter_operation_kind::clear, .Commits = { } });
			}
		});
		filesColumn->addWidget(CommitChip);
		refresh_folder_icons();

		Model = new overview_file_model{ this };
		Files = new QTreeView{ main };
		Files->setObjectName(QStringLiteral("overviewFiles"));
		Files->setAccessibleName(QStringLiteral("Files table"));
		const auto keysHelp = QStringLiteral(
			"Every file with its operation, line counts, whether it changed "
			"since my last review, my read state, comment counts and my note; "
			"activate a file to read it; click a folder, or use Left and Right, to hide and show its files; "
			"%1; "
			"Tab steps from a row onto its comment chips, Enter or Space lists a chip's threads and Escape returns to the row; "
			"the context-menu key lists every chip of the row");
		Files->setAccessibleDescription(keysHelp.arg(files_ends_help()));
		Files->setModel(Model);
		RowDelegate = new overview_row_delegate{ Colours, Files };
		Files->setItemDelegate(RowDelegate);
		// [[BPR-1213]]. Flat rows: no indentation, so every file aligns at the
		// same left column; the folder cell draws its own disclosure.
		Files->setRootIsDecorated(false);
		Files->setIndentation(0);
		Files->setItemsExpandable(true);
		Files->setExpandsOnDoubleClick(false);
		Files->setTreePosition(overview_file_model::file_column);
		Files->setUniformRowHeights(true);
		Files->setAlternatingRowColors(false);
		Files->setSelectionBehavior(QAbstractItemView::SelectRows);
		Files->setSelectionMode(QAbstractItemView::SingleSelection);
		Files->setFrameShape(QFrame::NoFrame);
		Files->setEditTriggers(QAbstractItemView::NoEditTriggers);
		// [[BPR-1363]]. Each column as wide as the widest cell of the whole
		// tree, every file whether its folder is open or not, so opening or
		// closing a folder, a scroll or a refresh moves no column; Note takes
		// the rest. Delegate measures stay bounded on a large review: only the
		// widest few cells by their text are measured.
		FileColumns = new steady_columns{ *Files, overview_file_model::note_column, [](const QModelIndex& aIndex) {
											 return aIndex.data(overview_file_model::folder_role).toBool() ? 1 : 0;
										 } };
		connect(Model, &QAbstractItemModel::rowsInserted, this,
				[this](const QModelIndex& aModelParent, const int aFirst, const int aLast) {
					expand_new_folders(*Files, aModelParent, aFirst, aLast);
				});
		connect(Model, &QAbstractItemModel::rowsInserted, this, [this] { update_folder_controls(); });
		connect(Model, &QAbstractItemModel::rowsRemoved, this, [this] { update_folder_controls(); });
		connect(Model, &QAbstractItemModel::modelReset, this, [this] { update_folder_controls(); });
		connect(Files, &QTreeView::collapsed, this, [this](const QModelIndex& aFolder) {
			for (auto parent = Files->currentIndex().parent(); parent.isValid(); parent = parent.parent()) {
				if (parent == aFolder) {
					Files->setCurrentIndex(aFolder);
					break;
				}
			}
		});
		// A click on a folder row shows or hides its files; files open on
		// activation as before. Neither dispatches anything for a folder.
		connect(Files, &QAbstractItemView::clicked, this, [this](const QModelIndex& aIndex) {
			if (aIndex.data(overview_file_model::folder_role).toBool()) {
				const auto folder = aIndex.siblingAtColumn(0);
				Files->setExpanded(folder, !Files->isExpanded(folder));
			}
		});
		connect(Files, &QAbstractItemView::activated, this, &pull_request_overview_page::open_file_at);
		// [[BPR-1089]]. A chip opens the conversations it counted: a click on
		// it, Enter or Space once Tab has stepped onto it, or the context-menu
		// key or a right-click on its row, whose menu reaches every chip,
		// Other, "+n" and ?. Another row leaves the chip.
		connect(Files->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& aCurrent) {
			if (ChipFocus && ChipFocus->Row != aCurrent.siblingAtColumn(0)) {
				focus_chip({ }, nullptr);
			}
		});
		Files->viewport()->installEventFilter(this);
		Files->installEventFilter(this);
		Files->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(Files, &QWidget::customContextMenuRequested, this, [this](const QPoint& aPosition) {
			auto index = Files->indexAt(aPosition);
			if (!index.isValid()) {
				index = Files->currentIndex();
			}
			if (auto* const menu = make_file_menu(index); menu != nullptr) {
				menu->setAttribute(Qt::WA_DeleteOnClose);
				menu->popup(Files->viewport()->mapToGlobal(aPosition));
			}
		});
		Files->header()->setSectionsClickable(true);
		connect(Files->header(), &QHeaderView::sectionClicked, this, [this](const int aSection) {
			if (aSection != overview_file_model::comments_column || !Model->feedback()) {
				return;
			}
			auto* const menu = make_sort_menu();
			menu->setAttribute(Qt::WA_DeleteOnClose);
			const auto left = Files->header()->sectionViewportPosition(aSection);
			menu->popup(Files->header()->mapToGlobal(QPoint{ left, Files->header()->height() }));
		});
		anchor_tooltips(*Files->viewport(), [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			if (const auto chip = chip_at(aPosition)) {
				const auto* counts = Model->feedback_at(chip->first);
				return anchored_tooltip_target{ .Anchor = chip->second.Box.toAlignedRect(), .Text = chip_tooltip(chip->second, *counts, any_columns(Model->feedback())) };
			}
			const auto index = Files->indexAt(aPosition);
			const auto text  = index.isValid() ? index.data(Qt::ToolTipRole).toString() : QString{ };
			if (text.isEmpty()) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = Files->visualRect(index), .Text = text };
		});
		filesColumn->addWidget(Files, 1);
		// RFC-BPR-0089 M2. The tree ends with the generated files set aside.
		GeneratedRow = new generated_files_row{ main };
		GeneratedRow->set_gesture_dispatch([this](const application::generated_file_gesture& aGesture) {
			if (GeneratedDispatch) {
				GeneratedDispatch(aGesture);
			}
		});
		filesColumn->addWidget(GeneratedRow, 0);
		CachedThreadsHeading = make_heading(QString{ }, QStringLiteral("overviewCachedThreadsHeading"), main);
		mainColumn->addWidget(CachedThreadsHeading);
		CachedThreads = new QPlainTextEdit{ main };
		CachedThreads->setObjectName(QStringLiteral("overviewCachedThreads"));
		CachedThreads->setAccessibleName(QStringLiteral("Historical review threads; freshness unknown"));
		CachedThreads->setAccessibleDescription(QStringLiteral("Read-only historical text. Anchors and links cannot be opened."));
		CachedThreads->setReadOnly(true);
		CachedThreads->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedThreads, 1);
		CachedDiscussion = new QPlainTextEdit{ main };
		CachedDiscussion->setObjectName(QStringLiteral("overviewCachedDiscussion"));
		CachedDiscussion->setAccessibleName(QStringLiteral("Historical general comments and review summaries; freshness unknown"));
		CachedDiscussion->setReadOnly(true);
		CachedDiscussion->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedDiscussion, 1);
		CachedSourceBack = new quiet_push_button{ QStringLiteral("Back to cached overview"), main };
		CachedSourceBack->setObjectName(QStringLiteral("overviewCachedSourceBack"));
		connect(CachedSourceBack, &QPushButton::clicked, this, [this] { show_cached_source(false); Files->setFocus(Qt::OtherFocusReason); });
		mainColumn->addWidget(CachedSourceBack);
		CachedSource = new QPlainTextEdit{ main };
		CachedSource->setObjectName(QStringLiteral("overviewCachedSource"));
		CachedSource->setAccessibleName(QStringLiteral("Historical exact base and head source; freshness unknown"));
		CachedSource->setReadOnly(true);
		CachedSource->setLineWrapMode(QPlainTextEdit::NoWrap);
		mainColumn->addWidget(CachedSource, 2);

		auto* const sideScroll = new QScrollArea{ this };
		sideScroll->setObjectName(QStringLiteral("overviewSideScroll"));
		sideScroll->setAccessibleName(QStringLiteral("Participants and review summary"));
		sideScroll->setWidgetResizable(true);
		// [[BPR-1588]]. The Status drawer sets the width; beneath the column's
		// own minimum it scrolls sideways rather than clipping.
		sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		sideScroll->setFrameShape(QFrame::NoFrame);
		auto* const side = new QWidget{ sideScroll };
		Side             = sideScroll;
		side->setObjectName(QStringLiteral("overviewSideColumn"));
		side->setAutoFillBackground(true);
		side->setBackgroundRole(QPalette::AlternateBase);
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(14, 12, 14, 12);
		sideColumn->setSpacing(2);
		sideColumn->setSizeConstraint(QLayout::SetMinimumSize);

		// [[BPR-1589]]. RFC-BPR-0085 D7, the owner's "attention first": my role,
		// then what needs me, my review, the checks and the people, each a
		// section that opens in place to its details. Nothing is drawn to say
		// 0, — or not available; each failed check is named in one place.
		StatusNote = new QLabel{ side };
		StatusNote->setObjectName(QStringLiteral("overviewStatusNote"));
		StatusNote->setTextFormat(Qt::PlainText);
		StatusNote->setWordWrap(true);
		StatusNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		StatusNote->setForegroundRole(QPalette::PlaceholderText);
		StatusNote->hide();
		sideColumn->addWidget(StatusNote);

		const auto makeText = [](QWidget* const aOwner, const QString& aName) {
			auto* label = new QLabel{ aOwner };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		};

		// [[BPR-1589]] v002. My role: my face, one short label, and any other
		// roles as chips; the reasons are in the tooltip.
		RoleRow = new overview_role_line{ side };
		RoleRow->setObjectName(QStringLiteral("overviewStatusRole"));
		sideColumn->addWidget(RoleRow);

		struct section_layouts
		{
			QVBoxLayout* Body    = nullptr;
			QVBoxLayout* Details = nullptr;
		};
		const auto makeSection = [this, side, sideColumn](const overview_status_section aSection, const QString& aTitle, const QString& aName) {
			auto& parts  = Sections[static_cast<std::size_t>(aSection)];
			parts.Frame  = new QWidget{ side };
			auto* column = new QVBoxLayout{ parts.Frame };
			column->setContentsMargins(0, 6, 0, 2);
			column->setSpacing(2);
			parts.Header = new drawer_section_header{ parts.Frame };
			parts.Header->setObjectName(aName);
			parts.Header->setText(aTitle);
			parts.Header->setAccessibleName(aTitle);
			column->addWidget(parts.Header);
			auto* const body       = new QWidget{ parts.Frame };
			auto* const bodyLayout = new QVBoxLayout{ body };
			bodyLayout->setContentsMargins(4, 0, 4, 0);
			bodyLayout->setSpacing(2);
			column->addWidget(body);
			parts.Details             = new QWidget{ parts.Frame };
			auto* const detailsLayout = new QVBoxLayout{ parts.Details };
			detailsLayout->setContentsMargins(4, 8, 4, 2);
			detailsLayout->setSpacing(2);
			parts.Details->hide();
			column->addWidget(parts.Details);
			sideColumn->addWidget(parts.Frame);
			connect(parts.Header, &QToolButton::toggled, this, [this, aSection](const bool aOpen) {
				Sections[static_cast<std::size_t>(aSection)].Details->setVisible(aOpen);
				remember_sections();
				if (aSection == overview_status_section::checks) {
					present_attention();
				}
			});
			return section_layouts{ bodyLayout, detailsLayout };
		};
		const auto addValue = [this](QVBoxLayout* const aColumn, const overview_value aValue) {
			auto* const row       = new QWidget{ aColumn->parentWidget() };
			auto* const rowLayout = new QHBoxLayout{ row };
			rowLayout->setContentsMargins(0, 2, 0, 2);
			auto* const name = new QLabel{ value_name(aValue), row };
			name->setTextFormat(Qt::PlainText);
			auto* const value = new QLabel{ row };
			value->setObjectName(value_object_name(aValue));
			value->setAccessibleName(value_name(aValue));
			value->setTextFormat(Qt::PlainText);
			value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			rowLayout->addWidget(name, 1);
			rowLayout->addWidget(value, 0);
			aColumn->addWidget(row);
			Values[static_cast<std::size_t>(aValue)] = value;
		};

		// Needs attention, opening to Merge's conditions and the requirement
		// details. With nothing to attend to, it is Merge, and says its state.
		const auto attention         = makeSection(overview_status_section::attention, QStringLiteral("Needs attention"), QStringLiteral("overviewAttentionSection"));
		AttentionList                = attention.Body;
		auto* const attentionDetails = Sections[static_cast<std::size_t>(overview_status_section::attention)].Details;
		MergeState                   = makeText(attentionDetails, QStringLiteral("overviewMergeState"));
		auto stateFont               = MergeState->font();
		stateFont.setWeight(QFont::DemiBold);
		MergeState->setFont(stateFont);
		attention.Details->addWidget(MergeState);
		MergeConditions = makeText(attentionDetails, QStringLiteral("overviewMergeConditions"));
		attention.Details->addWidget(MergeConditions);
		// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
		RequirementDisclosure = new disclosure_button{ attentionDetails };
		RequirementDisclosure->setObjectName(QStringLiteral("overviewRequirementDisclosure"));
		RequirementDisclosure->setText(QStringLiteral("Requirement details"));
		RequirementDisclosure->setAccessibleDescription(QStringLiteral("Show applicable rule origins, approval restrictions and auto-merge observation"));
		attention.Details->addSpacing(4);
		attention.Details->addWidget(RequirementDisclosure);
		RequirementDetails = makeText(attentionDetails, QStringLiteral("overviewRequirementDetails"));
		RequirementDetails->setForegroundRole(QPalette::PlaceholderText);
		RequirementDetails->hide();
		attention.Details->addWidget(RequirementDetails);
		connect(RequirementDisclosure, &QToolButton::toggled, this, [this](bool aOpen) { RequirementDetails->setVisible(aOpen); });

		// My review: files read with a bar, my notes and pending comments, and
		// the threads with posts I have not read, with Go to them. It opens to
		// what happened since my last review that has a value.
		const auto  myReview = makeSection(overview_status_section::my_review, QStringLiteral("My review"), QStringLiteral("overviewMyReviewSection"));
		auto* const myBody   = myReview.Body->parentWidget();
		addValue(myReview.Body, overview_value::files_read);
		ReadMeter = new drawer_meter{ myBody };
		ReadMeter->setObjectName(QStringLiteral("overviewReadMeter"));
		myReview.Body->addWidget(ReadMeter);
		myReview.Body->addSpacing(4);
		addValue(myReview.Body, overview_value::private_notes);
		addValue(myReview.Body, overview_value::drafted_comments);
		NewThreadsRow = new QWidget{ myBody };
		NewThreadsRow->setObjectName(QStringLiteral("overviewNewThreads"));
		auto* const newLayout = new QHBoxLayout{ NewThreadsRow };
		newLayout->setContentsMargins(0, 0, 0, 0);
		NewThreadsText = new QLabel{ NewThreadsRow };
		NewThreadsText->setObjectName(QStringLiteral("overviewNewThreadsText"));
		NewThreadsText->setTextFormat(Qt::PlainText);
		NewThreadsGo = new QToolButton{ NewThreadsRow };
		NewThreadsGo->setObjectName(QStringLiteral("overviewGoToNewThreads"));
		NewThreadsGo->setText(QStringLiteral("Go to them"));
		NewThreadsGo->setAutoRaise(true);
		NewThreadsGo->setCursor(Qt::PointingHandCursor);
		NewThreadsGo->setToolTip(QStringLiteral("Open Threads with Only new on"));
		newLayout->addWidget(NewThreadsText, 1);
		newLayout->addWidget(NewThreadsGo, 0);
		NewThreadsRow->hide();
		myReview.Body->addWidget(NewThreadsRow);
		connect(NewThreadsGo, &QToolButton::clicked, this, [this] {
			if (NewThreadsDispatch) {
				NewThreadsDispatch();
			}
		});
		// The conversation's new comments, which open in Discuss. Hidden until
		// the ledger keeps read state for comments that are not on a file.
		NewConversationRow = new QWidget{ myBody };
		NewConversationRow->setObjectName(QStringLiteral("overviewNewConversation"));
		auto* const conversationLayout = new QHBoxLayout{ NewConversationRow };
		conversationLayout->setContentsMargins(0, 0, 0, 0);
		NewConversationText = new QLabel{ NewConversationRow };
		NewConversationText->setObjectName(QStringLiteral("overviewNewConversationText"));
		NewConversationText->setTextFormat(Qt::PlainText);
		NewConversationGo = new QToolButton{ NewConversationRow };
		NewConversationGo->setObjectName(QStringLiteral("overviewGoToNewConversation"));
		NewConversationGo->setText(QStringLiteral("Go to them"));
		NewConversationGo->setAutoRaise(true);
		NewConversationGo->setCursor(Qt::PointingHandCursor);
		NewConversationGo->setToolTip(QStringLiteral("Open Discuss on the pull request"));
		conversationLayout->addWidget(NewConversationText, 1);
		conversationLayout->addWidget(NewConversationGo, 0);
		NewConversationRow->hide();
		myReview.Body->addWidget(NewConversationRow);
		connect(NewConversationGo, &QToolButton::clicked, this, [this] {
			if (NewConversationDispatch) {
				NewConversationDispatch();
			}
		});
		SinceHeading = make_heading(QStringLiteral("Since my last review"), QStringLiteral("overviewSinceHeading"), Sections[static_cast<std::size_t>(overview_status_section::my_review)].Details);
		myReview.Details->addWidget(SinceHeading);
		addValue(myReview.Details, overview_value::uploads);
		addValue(myReview.Details, overview_value::files_changed);
		addValue(myReview.Details, overview_value::new_comments);
		addValue(myReview.Details, overview_value::replies_to_me);
		addValue(myReview.Details, overview_value::my_unresolved_threads);

		// [[BPR-1588]]. No Start review: the owner, 2026-10-17, "Start review is
		// probably unnecessary now that the Review tab is so convenient, and
		// double-clicking a file or clicking Next file also works." Its one
		// job, Review at the first unseen change, is the first entry into
		// Review from here (`entry_path`).

		// Checks: one bar and its counts, opening to every result, the selected
		// result's card and Open check details (RFC-BPR-0068).
		const auto checks      = makeSection(overview_status_section::checks, QStringLiteral("Checks"), QStringLiteral("overviewChecksDisclosure"));
		ChecksDisclosure       = Sections[static_cast<std::size_t>(overview_status_section::checks)].Header;
		CheckDetails           = Sections[static_cast<std::size_t>(overview_status_section::checks)].Details;
		auto* const checksBody = checks.Body->parentWidget();
		ChecksMeter            = new drawer_meter{ checksBody };
		ChecksMeter->setObjectName(QStringLiteral("overviewChecksMeter"));
		checks.Body->addWidget(ChecksMeter);
		auto* const checksValue = makeText(checksBody, value_object_name(overview_value::checks));
		checksValue->setAccessibleName(value_name(overview_value::checks));
		checks.Body->addWidget(checksValue);
		Values[static_cast<std::size_t>(overview_value::checks)] = checksValue;
		ChecksNote                                               = makeText(CheckDetails, QStringLiteral("overviewChecksNote"));
		ChecksNote->setForegroundRole(QPalette::PlaceholderText);
		checks.Details->addWidget(ChecksNote);
		CheckList = new QTreeWidget{ CheckDetails };
		CheckList->setObjectName(QStringLiteral("overviewCheckList"));
		CheckList->setAccessibleName(QStringLiteral("Check results"));
		CheckList->setHeaderHidden(true);
		CheckList->setRootIsDecorated(false);
		CheckList->setWordWrap(true);
		CheckList->setItemDelegate(new check_result_delegate{ CheckList });
		CheckList->setTextElideMode(Qt::ElideNone);
		CheckList->header()->setSectionResizeMode(QHeaderView::Stretch);
		CheckList->setMinimumWidth(0);
		CheckList->setFixedHeight(180);
		checks.Details->addWidget(CheckList);
		CheckHeading = makeText(CheckDetails, QStringLiteral("overviewSelectedCheck"));
		checks.Details->addWidget(CheckHeading);
		CheckDescription = new QPlainTextEdit{ CheckDetails };
		CheckDescription->setObjectName(QStringLiteral("overviewCheckDescription"));
		CheckDescription->setAccessibleName(QStringLiteral("Check explanation"));
		CheckDescription->setReadOnly(true);
		CheckDescription->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		CheckDescription->setFixedHeight(140);
		checks.Details->addWidget(CheckDescription);
		CheckOpen = new quiet_push_button{ QStringLiteral("Open check details"), CheckDetails };
		CheckOpen->setObjectName(QStringLiteral("overviewOpenCheck"));
		CheckOpen->setAccessibleName(QStringLiteral("Open check details"));
		checks.Details->addWidget(CheckOpen, 0, Qt::AlignLeft);

		// People: faces with their standing and one line, opening to the
		// roster with each person's request, review and code-owner marking.
		const auto  people     = makeSection(overview_status_section::people, QStringLiteral("People"), QStringLiteral("overviewPeopleSection"));
		auto* const peopleBody = people.Body->parentWidget();
		auto* const facesRow   = new QHBoxLayout;
		facesRow->setContentsMargins(0, 2, 0, 2);
		facesRow->setSpacing(10);
		Faces = new overview_faces{ peopleBody };
		facesRow->addWidget(Faces, 0);
		PeopleSummary = makeText(peopleBody, QStringLiteral("overviewPeopleSummary"));
		PeopleSummary->setForegroundRole(QPalette::PlaceholderText);
		facesRow->addWidget(PeopleSummary, 1);
		people.Body->addLayout(facesRow);
		Participants = new QVBoxLayout;
		Participants->setContentsMargins(0, 0, 0, 0);
		Participants->setSpacing(0);
		people.Details->addLayout(Participants);
		Requests = new QVBoxLayout;
		Requests->setContentsMargins(0, 0, 0, 0);
		Requests->setSpacing(0);
		people.Details->addLayout(Requests);
		RequestsNote = makeText(Sections[static_cast<std::size_t>(overview_status_section::people)].Details, QStringLiteral("overviewReviewRequestsNote"));
		RequestsNote->setForegroundRole(QPalette::PlaceholderText);
		people.Details->addWidget(RequestsNote);
		connect(CheckList, &QTreeWidget::currentItemChanged, this, [this] { present_selected_check(); });
		connect(CheckOpen, &QPushButton::clicked, this, [this] {
			if (!Projection || !CheckList->currentItem() || !CheckLinkDispatch) {
				return;
			}
			const auto identity = CheckList->currentItem()->data(0, Qt::UserRole).toString().toStdString();
			const auto row      = std::ranges::find(Projection->CheckRows, identity, &application::overview_check_row::Identity);
			if (row != Projection->CheckRows.end() && row->LinkAvailable) {
				CheckLinkDispatch(row->Identity, row->DetailsUrl);
			}
		});
		sideColumn->addStretch(1);
		sideScroll->setWidget(side);

		// [[BPR-1588]]. The column is the Status drawer, beside the main column
		// on a splitter, so it resizes, collapses and is remembered as the
		// workbench drawers are (RFC-BPR-0085 principle 7).
		StatusSplit  = new overview_status_splitter{ this };
		StatusDrawer = new overview_status_drawer{ sideScroll, StatusSplit };
		StatusSplit->addWidget(main);
		StatusSplit->addWidget(StatusDrawer);
		StatusSplit->setStretchFactor(0, 1);
		StatusSplit->setStretchFactor(1, 0);
		// A width leaves only from a handle drag, as the workbench's does.
		connect(StatusSplit, &QSplitter::splitterMoved, this, [this](int, int) { StatusDrawer->observe_drag(StatusDrawer->width()); });
		auto* const columns = new QHBoxLayout{ this };
		columns->setContentsMargins(0, 0, 0, 0);
		columns->setSpacing(0);
		columns->addWidget(StatusSplit, 1);

		Colours.Marks  = default_feedback_mark_theme(palette());
		Colours.States = default_feedback_state_colours(palette());
		refresh_feedback_icons();
		// [[BPR-1576]]. Each section says its own wait, over its own place, so
		// a section that is ready shows while the others still load.
		Veils.Header      = new pending_veil{ *this, pending_shape::text, QStringLiteral("Pull request"), Header };
		Veils.Description = new pending_veil{ *this, pending_shape::text, QStringLiteral("Description"), SummaryPane };
		// Over the rows only: the column names stay readable above them.
		Veils.Files = new pending_veil{ *this, pending_shape::rows, QStringLiteral("Files"), Files->viewport() };
		Veils.Side  = new pending_veil{ *this, pending_shape::pairs, QStringLiteral("Review summary"), sideScroll };
		Veils.Side->set_placeholder_count(overview_value_count + 3);
		// One voice for the page: the files say when the Overview is ready.
		Veils.Header->set_announces(false);
		Veils.Description->set_announces(false);
		Veils.Side->set_announces(false);
		clear();
	}

	void pull_request_overview_page::present(const application::pull_request_overview_projection& aProjection)
	{
		Cached   = false;
		Loading  = false;
		Observed = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		const auto bodyChanged         = Projection.has_value() && Projection->Body != aProjection.Body;
		const auto requirementsChanged = Projection && (Projection->MergeState != aProjection.MergeState || Projection->MergeConditions != aProjection.MergeConditions || Projection->ReviewRequests != aProjection.ReviewRequests);
		const auto updateBody          = !Projection.has_value() || bodyChanged;
		// [[BPR-1089]]. Another pull request starts from the Settings default,
		// in file order, with no counts until the host answers for it.
		const auto samePullRequest = Projection.has_value() && Projection->Owner == aProjection.Owner && Projection->RepositoryName == aProjection.RepositoryName
			&& Projection->Number == aProjection.Number;
		if (!samePullRequest) {
			ModeChosen = false;
			Mode       = DefaultMode;
			Sort.reset();
			AskedFeedback.reset();
			Model->apply_feedback(std::nullopt);
		}
		Projection = aProjection;
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, false);
		}
		Files->setAccessibleDescription(QStringLiteral("Current file catalogue; activate a file to read it; %1.").arg(files_ends_help()));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Title->setText(text(aProjection.Title));
		auto state = QString{ };
		switch (aProjection.State) {
		case application::github_pull_request_state::merged:
			state = QStringLiteral(" Merged.");
			break;
		case application::github_pull_request_state::closed:
			state = QStringLiteral(" Closed without merge.");
			break;
		case application::github_pull_request_state::open:
			state = aProjection.Draft ? QStringLiteral(" Draft.") : QString{ };
			break;
		}
		auto commits = aProjection.CommitCount == 1U
			? QStringLiteral("1 commit")
			: QStringLiteral("%1 commits").arg(count_text(aProjection.CommitCount));
		if (aProjection.ReturnedCommitCount < aProjection.CommitCount) {
			commits += QStringLiteral(" (%1 returned by GitHub)")
						   .arg(count_text(aProjection.ReturnedCommitCount));
		}
		Summary->setText(QStringLiteral("%1 wants to merge %2 into %3 from %4.%5")
							 .arg(text(aProjection.AuthorLogin),
								  commits,
								  text(aProjection.BaseRef),
								  text(aProjection.HeadRef),
								  state));
		if (updateBody) {
			Body->present_body(aProjection.Body);
		}
		Body->setVisible(!aProjection.Body.empty());
		SummaryPane->setVisible(!aProjection.Body.empty());
		FullView->setVisible(!aProjection.Body.empty());
		SummarySplit->queue_layout();
		EmptyBody->setVisible(aProjection.Body.empty());
		if (aProjection.Body.empty() && Body->expanded()) {
			set_description_expanded(false);
		}
		// [[BPR-1774]]. Another pull request ends an edit of this one; the
		// same one keeps it, typed text and all, whatever GitHub now says.
		if (!samePullRequest && DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
		if (bodyChanged || requirementsChanged) {
			const auto                   announcement = (bodyChanged ? QStringLiteral("Description updated. ") : QString{ }) + (requirementsChanged ? merge_announcement(aProjection) : QString{ });
			QAccessibleAnnouncementEvent event{ Body, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}

		const auto& since = aProjection.SinceLastReview;
		present_files_heading(aProjection);
		apply_rows();
		update_folder_controls();
		present_feedback_controls();
		dispatch_feedback_request();

		rebuild_participants(aProjection.Participants);

		apply_history({ .Known = since.Known, .ReviewedAt = since.ReviewedAt, .Uploads = since.Uploads });
		// [[BPR-1589]]. What happened since my last review, in My review's
		// details: a fact with no value is not drawn.
		set_value(overview_value::files_changed,
				  since.FilesChanged.has_value() ? count_text(*since.FilesChanged) : QStringLiteral("not yet resolved"),
				  since.FilesChanged.value_or(0U) != 0U);
		show_value(overview_value::files_changed, since.Known && since.FilesChanged.value_or(1U) != 0U);
		set_value(overview_value::new_comments, count_text(since.NewComments.value_or(0U)), since.NewComments.value_or(0U) != 0U);
		show_value(overview_value::new_comments, since.NewComments.value_or(0U) != 0U);
		set_value(overview_value::replies_to_me, count_text(since.RepliesToMe), since.RepliesToMe != 0U);
		show_value(overview_value::replies_to_me, since.RepliesToMe != 0U);
		set_value(overview_value::my_unresolved_threads, count_text(since.MyUnresolvedThreads), false);
		show_value(overview_value::my_unresolved_threads, since.MyUnresolvedThreads != 0U);
		present_checks(samePullRequest);
		present_merge(aProjection);
		if (!samePullRequest) {
			RequirementDisclosure->setChecked(false);
		}
		RequirementDetails->setVisible(RequirementDisclosure->isChecked());
		present_status(samePullRequest);

		apply_pending();
	}

	void pull_request_overview_page::set_check_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		CheckLinkDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::present_checks(const bool aSamePullRequest)
	{
		const auto& shown = *Projection;
		// [[BPR-1589]]. One bar of passed, failed and skipped, and their counts;
		// a pull request with no results draws no Checks at all.
		Sections[static_cast<std::size_t>(overview_status_section::checks)].Frame->setVisible(!shown.CheckRows.empty() || shown.Checks.total() != 0U);
		auto summary = shown.Checks.total() == 0U ? QStringLiteral("No results yet") : checks_text(shown.Checks);
		if (!shown.ChecksComplete) {
			summary += QStringLiteral(" (partial)");
		}
		set_value(overview_value::checks, summary, false);
		const auto& tally = shown.Checks;
		ChecksMeter->set_segments({ { static_cast<double>(tally.Passed), Colours.Accepted },
									{ static_cast<double>(tally.Failed), Colours.Defect },
									{ static_cast<double>(tally.Pending), Colours.Caution } },
								  static_cast<double>(tally.total()));
		ChecksMeter->setAccessibleName(QStringLiteral("Checks: %1").arg(summary));
		ChecksDisclosure->set_count(tally.total() == 0U ? QString{ } : count_text(tally.total()));
		ChecksDisclosure->setAccessibleName(QStringLiteral("Checks: %1").arg(summary));
		ChecksNote->setText(text(shown.ChecksNote));
		ChecksNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		const auto           selected = aSamePullRequest && CheckList->currentItem() ? CheckList->currentItem()->data(0, Qt::UserRole).toString() : QString{ };
		const auto           scroll   = aSamePullRequest ? CheckList->verticalScrollBar()->value() : 0;
		const QSignalBlocker blocked{ CheckList };
		auto                 desired = QSet<QString>{ };
		for (const auto& row : shown.CheckRows) {
			desired.insert(text(row.Identity));
		}
		for (auto it = CheckItems.begin(); it != CheckItems.end();) {
			if (!desired.contains(it.key())) {
				delete it.value();
				it = CheckItems.erase(it);
			} else {
				++it;
			}
		}
		int position = 0;
		for (const auto& row : shown.CheckRows) {
			const auto key  = text(row.Identity);
			auto*      item = CheckItems.value(key, nullptr);
			if (!item) {
				item = new QTreeWidgetItem{ };
				item->setData(0, Qt::UserRole, key);
				CheckItems.insert(key, item);
				CheckList->insertTopLevelItem(position, item);
			} else if (CheckList->topLevelItem(position) != item) {
				CheckList->takeTopLevelItem(CheckList->indexOfTopLevelItem(item));
				CheckList->insertTopLevelItem(position, item);
			}
			const auto label = text(row.Name) + QStringLiteral(" — ") + text(row.Outcome);
			if (item->text(0) != label) {
				item->setText(0, label);
			}
			item->setData(0, Qt::AccessibleTextRole, label);
			++position;
		}
		if (auto* item = CheckItems.value(selected, nullptr)) {
			CheckList->setCurrentItem(item);
		} else if (CheckList->topLevelItemCount() != 0) {
			CheckList->setCurrentItem(CheckList->topLevelItem(0));
		}
		// The list is as tall as its results, up to 180 pixels, then scrolls.
		const auto listed = CheckList->topLevelItemCount() == 0 ? 0 : CheckList->sizeHintForRow(0) * CheckList->topLevelItemCount();
		CheckList->setFixedHeight(std::clamp(listed + (2 * CheckList->frameWidth()) + 4, 48, 180));
		CheckList->verticalScrollBar()->setValue(scroll);
		CheckDetails->setVisible(ChecksDisclosure->isChecked());
		present_selected_check();
	}

	void pull_request_overview_page::present_selected_check()
	{
		const auto identity = CheckList->currentItem() ? CheckList->currentItem()->data(0, Qt::UserRole).toString().toStdString() : std::string{ };
		const auto row      = Projection ? std::ranges::find(Projection->CheckRows, identity, &application::overview_check_row::Identity) : std::vector<application::overview_check_row>::iterator{ };
		if (!Projection || row == Projection->CheckRows.end()) {
			CheckHeading->clear();
			CheckDescription->clear();
			CheckOpen->setEnabled(false);
			return;
		}
		const auto heading = text(row->Name) + QStringLiteral("\n%1: %2\n%3\n%4").arg(text(row->Kind), text(row->Outcome), row->Producer.empty() ? QStringLiteral("Producer unknown") : text(row->Producer), text(row->Requirement));
		if (CheckHeading->text() != heading) {
			CheckHeading->setText(heading);
		}
		auto description = row->Description.empty() ? QStringLiteral("No details were supplied.") : text(row->Description);
		if (!row->LinkAvailable) {
			description += row->DetailsUrl.empty() ? QStringLiteral("\nNo detail destination was supplied.") : QStringLiteral("\nThe detail destination cannot be opened safely.");
		}
		if (CheckDescription->toPlainText() != description) {
			CheckDescription->setPlainText(description);
		}
		// As tall as its explanation, up to 140 pixels, then it scrolls.
		const auto lines = static_cast<int>(std::ceil(CheckDescription->document()->size().height()));
		CheckDescription->setFixedHeight(std::clamp((std::max(lines, 2) * CheckDescription->fontMetrics().lineSpacing()) + (2 * CheckDescription->frameWidth()) + 12, 48, 140));
		CheckOpen->setEnabled(row->LinkAvailable);
		CheckOpen->setToolTip(row->LinkAvailable ? QStringLiteral("Open the supplied destination in your browser") : QStringLiteral("No admitted detail destination"));
	}

	void pull_request_overview_page::present_files_heading(const application::pull_request_overview_projection& aProjection)
	{
		const auto& since   = aProjection.SinceLastReview;
		const auto  notRead = aProjection.Progress.FilesTotal - std::min(aProjection.Progress.FilesRead, aProjection.Progress.FilesTotal);
		// RFC-BPR-0089. A filtered count says so, and why.
		auto heading = aProjection.CommitFilterComparisonFiles
			? QStringLiteral("Files: %1 of %2 in this pull request")
				  .arg(count_text(aProjection.Progress.FilesTotal), count_text(*aProjection.CommitFilterComparisonFiles))
			: QStringLiteral("Files: %1 in this pull request").arg(count_text(aProjection.Progress.FilesTotal));
		if (aProjection.CommitFilterComparisonFiles) {
			if (aProjection.FilteredByCommits) {
				heading += QStringLiteral(", filtered by commits");
			}
			if (aProjection.FileFilteredFiles != 0U) {
				heading += QStringLiteral(", %1 hidden by file filters").arg(count_text(aProjection.FileFilteredFiles));
			}
			if (aProjection.GeneratedFiles != 0U) {
				heading += QStringLiteral(", %1 generated set aside").arg(count_text(aProjection.GeneratedFiles));
			}
		}
		// [[BPR-1692]]. When linguist-generated could not be read, say why:
		// only the reviewer's own marks set files aside then.
		using attributes = application::generated_attributes_read;
		switch (aProjection.GeneratedAttributes) {
		case attributes::pending:
		case attributes::read:                 break;
		case attributes::too_many_directories: heading += QStringLiteral(", linguist-generated not read: files in more than 4,096 folders"); break;
		case attributes::too_many_files:       heading += QStringLiteral(", linguist-generated not read: more than 64 .gitattributes files"); break;
		case attributes::file_too_large:
			heading += aProjection.GeneratedAttributesExceeded && !aProjection.GeneratedAttributesExceeded->empty()
				? QStringLiteral(", linguist-generated not read: %1/.gitattributes is over 512 KiB").arg(QString::fromStdString(*aProjection.GeneratedAttributesExceeded))
				: QStringLiteral(", linguist-generated not read: .gitattributes is over 512 KiB");
			break;
		case attributes::too_many_rules:      heading += QStringLiteral(", linguist-generated not read: too many .gitattributes rules"); break;
		case attributes::objects_unavailable: heading += QStringLiteral(", linguist-generated not read: the head's .gitattributes are not available"); break;
		case attributes::failed:              heading += QStringLiteral(", linguist-generated could not be read"); break;
		}
		if (since.Known && since.FilesChanged.has_value()) {
			heading += QStringLiteral(", %1 changed since my last review").arg(count_text(*since.FilesChanged));
		} else if (since.Known) {
			heading += QStringLiteral(", changes since my last review not yet resolved");
		}
		heading += QStringLiteral(", %1 not read").arg(count_text(notRead));
		present_heading(*FilesHeading, heading);
	}

	void pull_request_overview_page::apply_history(const application::overview_history_update& aUpdate)
	{
		if (!Projection) {
			return;
		}
		auto& since      = Projection->SinceLastReview;
		since.Known      = aUpdate.Known;
		since.ReviewedAt = aUpdate.ReviewedAt;
		since.Uploads    = aUpdate.Uploads;
		if (!since.Known) {
			SinceHeading->setText(QStringLiteral("Since my last review: none yet"));
		} else if (since.ReviewedAt) {
			const auto when = QDateTime::fromMSecsSinceEpoch(since.ReviewedAt->value(), QTimeZone::UTC).toLocalTime();
			SinceHeading->setText(QStringLiteral("Since my last review on %1").arg(when.toString(QStringLiteral("MMM d"))));
		} else {
			SinceHeading->setText(QStringLiteral("Since my last review"));
		}
		set_value(overview_value::uploads, count_text(since.Uploads.value_or(0U)), since.Uploads.value_or(0U) != 0U);
		show_value(overview_value::uploads, since.Known && since.Uploads.value_or(0U) != 0U);
	}

	void pull_request_overview_page::apply_receipts(const application::overview_receipt_update& aUpdate)
	{
		if (!Projection.has_value()) {
			return;
		}
		// The page keeps the progress and the action it dispatches; its rows
		// live in the model, which updates only the changed files.
		Projection->Progress  = aUpdate.Progress;
		Projection->Entry     = aUpdate.Entry;
		Projection->EntryPath = aUpdate.EntryPath;
		Model->update_rows(aUpdate.Rows);
		FileColumns->apply();
		present_files_heading(*Projection);
		present_my_review();
	}

	void pull_request_overview_page::clear()
	{
		// [[BPR-1601]]. The next pull request waits for its own ledger count.
		NewConversation.reset();
		set_description_expanded(false);
		Projection.reset();
		MergeState->hide();
		MergeConditions->hide();
		RequestsNote->hide();
		while (Requests->count() != 0) {
			auto* const item = Requests->takeAt(0);
			delete item->widget();
			delete item;
		}
		RequirementDisclosure->hide();
		RequirementDetails->hide();
		hide_status();
		CheckItems.clear();
		CheckList->clear();
		CheckDescription->clear();
		CheckOpen->setEnabled(false);
		Cached = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		Title->setText(QStringLiteral("No pull request"));
		Summary->clear();
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Body->reset_reader();
		Body->setVisible(false);
		SummaryPane->setVisible(false);
		EmptyBody->setVisible(false);
		SummaryPane->setVisible(false);
		FullView->setVisible(false);
		// [[BPR-1774]]. An invalidated authority withdraws Edit and ends an
		// edit; nothing is sent.
		DescriptionEditable = false;
		if (DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
		present_heading(*FilesHeading, QStringLiteral("Files"));
		ModeChosen = false;
		Mode       = DefaultMode;
		Sort.reset();
		AskedFeedback.reset();
		Model->apply_feedback(std::nullopt);
		Model->apply({ });
		update_folder_controls();
		present_feedback_controls();
		rebuild_participants({ });
		SinceHeading->setText(QStringLiteral("Since my last review"));
		for (auto* const value : Values) {
			if (value != nullptr) {
				value->clear();
				value->setForegroundRole(QPalette::PlaceholderText);
			}
		}
		// An observation clears the page and goes on loading: its waits keep
		// their clocks rather than starting again.
		if (!Observing) {
			Loading       = false;
			Observed      = false;
			ReportedFiles = 0U;
			apply_pending();
		}
	}

	void pull_request_overview_page::present_loading()
	{
		Title->setText(QStringLiteral("Loading pull request…"));
		Summary->setText(QStringLiteral("Details, files and threads are loading in the background."));
		present_heading(*FilesHeading, QStringLiteral("Files — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
		// [[BPR-1576]]. The description's place is held while it is unknown,
		// so the placeholders stand where it will.
		SummaryPane->setVisible(true);
		SummarySplit->queue_layout();
		Loading  = true;
		Observed = false;
		apply_pending();
	}

	auto pull_request_overview_page::pending_veils() const noexcept -> overview_pending_veils
	{
		return Veils;
	}

	void pull_request_overview_page::set_read_waiting(const QString& aWaiting)
	{
		if (ReadWaiting == aWaiting) {
			return;
		}
		ReadWaiting = aWaiting;
		apply_pending();
	}

	void pull_request_overview_page::set_pending_appearance(const bool aFullMotion, const bool aHighContrast, const QColor& aFailureInk)
	{
		for (auto* const veil : { Veils.Header, Veils.Description, Veils.Files, Veils.Side }) {
			if (veil != nullptr) {
				veil->set_full_motion(aFullMotion);
				veil->set_high_contrast(aHighContrast);
				veil->set_failure_ink(aFailureInk);
			}
		}
	}

	void pull_request_overview_page::apply_pending()
	{
		if (Veils.Header == nullptr) {
			return;
		}
		const auto waiting = ReadWaiting.isEmpty() ? QString{ } : QStringLiteral("Still waiting for GitHub: %1.").arg(ReadWaiting);
		const auto section = [&waiting](pending_veil& aVeil, const bool aPending, QString aLoading, QString aDone = { }) {
			if (!aPending) {
				aVeil.present_ready();
				return;
			}
			aVeil.present(core::pending_activity::loading, false, false, { .Loading = std::move(aLoading), .Waiting = waiting, .Done = std::move(aDone) });
		};
		// The read's provisional answer brings the title and the description
		// first; the catalogue and the side column come with the accepted review.
		section(*Veils.Header, Loading && !Observed, QStringLiteral("Opening the pull request…"));
		section(*Veils.Description, Loading && !Observed, QStringLiteral("Loading the description…"));
		section(*Veils.Side, Loading, QStringLiteral("Loading reviewers, checks and progress…"));
		// As many placeholder rows as GitHub reported files, once it has.
		Veils.Files->set_placeholder_count(Observed ? static_cast<int>(std::min<std::size_t>(std::max<std::size_t>(ReportedFiles, 1U), 40U)) : 6);
		if (Loading) {
			section(*Veils.Files, true,
					Observed ? QStringLiteral("Loading the file list: %1 reported…").arg(ReportedFiles == 1U ? QStringLiteral("1 file") : QStringLiteral("%1 files").arg(count_text(ReportedFiles)))
							 : QStringLiteral("Loading the file list…"),
					QStringLiteral("Overview ready: files, reviewers and checks are shown."));
			return;
		}
		// The counts by file are the files' last part to arrive: said, not
		// dimmed, while the rows themselves are already there to read.
		const auto& feedback = Model->feedback();
		const auto  counting = Projection.has_value() && !Cached && FeedbackDispatch && AskedFeedback.has_value()
			&& (!feedback.has_value() || feedback->Request != *AskedFeedback);
		if (counting) {
			Veils.Files->present(core::pending_activity::loading, true, true, { .Coming = QStringLiteral("Counting comments by file…") });
		} else {
			Veils.Files->present_ready();
		}
	}

	void pull_request_overview_page::present_observation(const application::github_loading_overview_projection& aObservation)
	{
		Observing = true;
		clear();
		Observing     = false;
		Loading       = true;
		Observed      = true;
		ReportedFiles = aObservation.ReportedFiles;
		Title->setText(text(aObservation.Title));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		auto state = aObservation.Draft ? QStringLiteral("Draft") : QStringLiteral("Open");
		if (aObservation.State == application::github_pull_request_state_projection::merged) {
			state = QStringLiteral("Merged");
		} else if (aObservation.State == application::github_pull_request_state_projection::closed) {
			state = QStringLiteral("Closed without merge");
		}
		const auto commits = aObservation.ReportedCommits == 1U ? QStringLiteral("1 commit")
																: QStringLiteral("%1 commits").arg(count_text(aObservation.ReportedCommits));
		Summary->setText(QStringLiteral("%1 · %2 · %3 → %4 · %5. Provisional metadata; review data is still loading.")
							 .arg(aObservation.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(aObservation.AuthorLogin),
								  commits, text(aObservation.HeadRef), text(aObservation.BaseRef), state));
		Body->present_body(aObservation.Body);
		Body->setVisible(!aObservation.Body.empty());
		SummaryPane->setVisible(!aObservation.Body.empty());
		FullView->setVisible(!aObservation.Body.empty());
		present_description_controls();
		SummarySplit->queue_layout();
		present_heading(*FilesHeading, QStringLiteral("Files — %1 reported; catalogue loading…").arg(count_text(aObservation.ReportedFiles)));
		SinceHeading->setText(QStringLiteral("Review state — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
		apply_pending();
	}

	void pull_request_overview_page::present_cached(const application::github_cached_overview_projection& aOverview)
	{
		present_observation(aOverview.Overview);
		// A cached overview is all there is: nothing more is on its way.
		Loading    = false;
		Cached     = true;
		CachedBase = aOverview.BaseOid;
		CachedHead = aOverview.HeadOid;
		Summary->setText(QStringLiteral("Cached overview · freshness unknown. Open this pull request again to retry."));
		Summary->setToolTip(QStringLiteral("Previously accepted base %1\nPreviously accepted head %2")
								.arg(text(aOverview.BaseOid), text(aOverview.HeadOid)));
		Summary->setAccessibleDescription(Summary->toolTip());
		present_heading(*FilesHeading,
						aOverview.FileCatalogue
							? QStringLiteral("Cached file catalogue — %1 files observed; open a file to read retained source").arg(count_text(aOverview.FileCatalogue->size()))
							: QStringLiteral("File catalogue unavailable offline; source unavailable offline"),
						QStringLiteral("Historical GitHub change kinds and line counts, reported with the retained base/head. These are not a recomputed comparison of the base tip and head."));
		Model->apply(cached_file_rows(aOverview.FileCatalogue));
		update_folder_controls();
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, true);
		}
		FileColumns->apply();
		Files->setAccessibleDescription(QStringLiteral("Historical file catalogue; freshness unknown. Open a file to read its exact retained base and head when available; %1. Current thread counts and review progress unavailable.").arg(files_ends_help()));
		CachedThreadsHeading->setText(aOverview.ReviewThreads
										  ? QStringLiteral("Cached review threads — %1 observed · freshness unknown").arg(count_text(aOverview.ReviewThreads->size()))
										  : QStringLiteral("Review threads unavailable offline"));
		CachedThreadsHeading->show();
		CachedDiscussion->setPlainText(cached_discussion_text(aOverview.GeneralComments, QStringLiteral("General comments")) + cached_discussion_text(aOverview.ReviewSummaries, QStringLiteral("Review summaries")));
		CachedDiscussion->show();
		CachedThreads->setPlainText(aOverview.ReviewThreads ? cached_thread_text(*aOverview.ReviewThreads) : QString{ });
		CachedThreads->setVisible(aOverview.ReviewThreads && !aOverview.ReviewThreads->empty());
		SinceHeading->setText(QStringLiteral("Review state unavailable offline"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setWordWrap(true);
			none->setText(QStringLiteral("Participants unavailable offline"));
		}
		// [[BPR-1589]]. Offline the Status drawer has nothing current to say,
		// and says that once.
		StatusNote->setText(QStringLiteral("My role, checks and people are unavailable offline. Open this pull request again to retry."));
		StatusNote->show();
		apply_pending();
	}

	void pull_request_overview_page::show_cached_source(bool aShow)
	{
		CachedSource->setVisible(aShow);
		CachedSourceBack->setVisible(aShow);
		Files->setVisible(!aShow);
		FilesHeading->setVisible(!aShow);
		update_folder_controls();
		CachedThreadsHeading->setVisible(!aShow);
		CachedThreads->setVisible(!aShow && !CachedThreads->toPlainText().isEmpty());
		CachedDiscussion->setVisible(!aShow);
	}

	void pull_request_overview_page::set_cached_source_dispatch(overview_path_dispatch aDispatch) { CachedSourceDispatch = std::move(aDispatch); }
	void pull_request_overview_page::present_cached_source(const application::cached_review_source_projection& aSource)
	{
		if (!Cached || aSource.BaseOid != CachedBase || aSource.HeadOid != CachedHead) {
			return;
		}
		auto content = QStringLiteral("Historical source: %1 · freshness unknown\nBase %2\nHead %3\n\n").arg(text(aSource.Path), text(aSource.BaseOid), text(aSource.HeadOid));
		switch (aSource.Status) {
		case application::cached_source_status::loading:     content += QStringLiteral("Reading retained source…"); break;
		case application::cached_source_status::unavailable: content += QStringLiteral("Source unavailable for this exact retained revision."); break;
		case application::cached_source_status::binary:      content += QStringLiteral("Binary source cannot be displayed as text."); break;
		case application::cached_source_status::too_large:   content += QStringLiteral("Source exceeds the offline reading limit."); break;
		case application::cached_source_status::available:
			if (!displayable_source(aSource.BaseText) || !displayable_source(aSource.HeadText)) {
				content += QStringLiteral("Source encoding cannot be displayed as UTF-8 text.");
				break;
			}
			content += QStringLiteral("BASE — ") + (aSource.BaseText ? QStringLiteral("observed source\n") + text(*aSource.BaseText) : QStringLiteral("path absent at this commit"));
			content += QStringLiteral("\n\nHEAD — ") + (aSource.HeadText ? QStringLiteral("observed source\n") + text(*aSource.HeadText) : QStringLiteral("path absent at this commit"));
			break;
		}
		CachedSource->setPlainText(content);
		show_cached_source(true);
		CachedSource->setFocus(Qt::OtherFocusReason);
	}

	void pull_request_overview_page::set_description_expanded(const bool aExpanded)
	{
		Body->set_expanded(aExpanded);
		SummarySplit->set_expanded(aExpanded);
		FilesPane->setVisible(!aExpanded);
		Header->setVisible(!aExpanded);
		StatusDrawer->setVisible(!aExpanded);
		Files->setVisible(!aExpanded);
		FilesHeading->setVisible(!aExpanded);
		update_folder_controls();
		FullView->setText(aExpanded ? QStringLiteral("Exit full view") : QStringLiteral("Enter full view"));
		FullView->setAccessibleName(FullView->text());
		FullView->setToolTip(aExpanded ? QStringLiteral("Exit full view (Return to Files)") : FullView->text());
		FullView->setIcon(full_view_icon(palette(), aExpanded));
		FullView->setVisible(!Body->Reader->original_body().empty());
		EmptyBody->setVisible(!aExpanded && Projection.has_value() && Projection->Body.empty());
		present_description_controls();
		findChild<QShortcut*>(QStringLiteral("overviewDescriptionEscape"))->setEnabled(aExpanded);
		if (aExpanded) {
			Body->setFocus(Qt::OtherFocusReason);
		} else if (isVisible()) {
			Files->setFocus(Qt::OtherFocusReason);
		}
		QWidget::setTabOrder(FullView, Body);
	}

	void pull_request_overview_page::set_summary_fraction(const std::optional<std::uint32_t> aFraction)
	{
		SummarySplit->set_preference(aFraction);
	}
	auto pull_request_overview_page::summary_fraction() const -> std::optional<std::uint32_t>
	{
		return SummarySplit->preference();
	}
	void pull_request_overview_page::set_summary_resize_dispatch(std::function<void(std::optional<std::uint32_t>)> aDispatch)
	{
		SummarySplit->Changed = std::move(aDispatch);
	}

	void pull_request_overview_page::adopt_technical_details(
		QWidget* const aButton, QWidget* const aDetails)
	{
		if (TechnicalRow == nullptr) {
			return;
		}
		for (auto* const adopted : { aButton, aDetails }) {
			if (adopted == nullptr) {
				continue;
			}
			adopted->setParent(this);
			TechnicalRow->addWidget(adopted);
		}
		TechnicalRow->addStretch(1);
	}

	auto pull_request_overview_page::presented() const noexcept -> bool
	{
		return Projection.has_value();
	}

	auto pull_request_overview_page::projection() const noexcept
		-> const std::optional<application::pull_request_overview_projection>&
	{
		return Projection;
	}

	void pull_request_overview_page::adopt_appearance(const appearance_profile& aProfile)
	{
		Body->Reader->set_code_font(aProfile.code_font(), aProfile.code_tab_stop_distance());
		// [[BPR-1562]]. Fenced code in the review panes' colours.
		Body->Reader->set_code_theme({ .Colors = code_role_colors_of(aProfile.palette()), .Variant = aProfile.variant() });
		Colours.Unread   = aProfile.color(core::appearance_role::state_unread);
		Colours.Accepted = aProfile.color(core::appearance_role::state_accepted);
		Colours.Defect   = aProfile.color(core::appearance_role::state_defect);
		Colours.Waiting  = aProfile.attention_color(attention_tone::waiting_on_others);
		Colours.Caution  = aProfile.color(core::appearance_role::state_attention);
		Colours.Progress = progress_mark_colours_for(aProfile);
		// [[BPR-1089]]. The state marks: open amber and resolved green.
		// [[BPR-1604]]. The table's amber draws digits, the R badge with its
		// letter cut from the ground, and the unresolved bubble, on the ground
		// and on the band: the theme's amber where it holds the text floor
		// there, otherwise darkened toward a burnt orange until it does.
		// `Marker` had kept its default, the Dark theme's amber, in every theme.
		const auto table          = aProfile.color(core::appearance_role::code_background);
		const auto grounds        = std::array{ core::contrast_ground{ .Colour = to_appearance_color(table), .MinimumHundredths = core::banded_floor_hundredths(core::text_contrast_minimum_hundredths) } };
		const auto amber          = to_presentation_color(core::readable_signal_ink(to_appearance_color(aProfile.color(core::appearance_role::state_attention)), grounds));
		Colours.Marker            = amber;
		Colours.States.Open       = amber;
		Colours.States.Resolved   = Colours.Accepted;
		Colours.States.OnResolved = feedback_mark_blocking_ink(Colours.Accepted);
		refresh_folder_icons();
		refresh_feedback_icons();
		if (Projection) {
			present_merge(*Projection);
		}
		update();
		Files->viewport()->update();
	}

	void pull_request_overview_page::collapse_all_folders()
	{
		// One folder at a time, so a hidden current file moves to its folder
		// exactly as a single collapse moves it.
		for (auto row = 0; row < Model->rowCount(); ++row) {
			const auto folder = Model->index(row, 0);
			if (folder.data(overview_file_model::folder_role).toBool()) {
				Files->collapse(folder);
			}
		}
	}

	void pull_request_overview_page::expand_all_folders()
	{
		for (auto row = 0; row < Model->rowCount(); ++row) {
			const auto folder = Model->index(row, 0);
			if (folder.data(overview_file_model::folder_role).toBool()) {
				Files->expand(folder);
			}
		}
	}

	auto pull_request_overview_page::collapse_folders_button() const noexcept -> QToolButton*
	{
		return CollapseFolders;
	}

	auto pull_request_overview_page::expand_folders_button() const noexcept -> QToolButton*
	{
		return ExpandFolders;
	}

	void pull_request_overview_page::update_folder_controls()
	{
		const auto shown = FilesHeading != nullptr && !FilesHeading->isHidden() && Model != nullptr && Model->folder_count() > 0U;
		CollapseFolders->setVisible(shown);
		ExpandFolders->setVisible(shown);
	}

	void pull_request_overview_page::refresh_folder_icons()
	{
		// Two chevrons that close upward, or open downward, in the heading's ink.
		const auto chevrons = [](const bool aDown) {
			return [aDown](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				auto pen = QPen{ aInk, std::max<qreal>(1.25, aSide / 11.0) };
				pen.setCapStyle(Qt::RoundCap);
				pen.setJoinStyle(Qt::RoundJoin);
				aPainter.setPen(pen);
				const auto lean = aSide * (aDown ? 0.11 : -0.11);
				for (const auto middle : { aSide * 0.34, aSide * 0.66 }) {
					aPainter.drawPolyline(std::array{ QPointF{ aSide * 0.25, middle - lean }, QPointF{ aSide * 0.5, middle + lean },
													  QPointF{ aSide * 0.75, middle - lean } }
											  .data(),
										  3);
				}
			};
		};
		const auto ink      = palette().color(QPalette::Text);
		const auto disabled = palette().color(QPalette::Disabled, QPalette::Text);
		CollapseFolders->setIcon(painted_glyph_icon(chevrons(false), ink, disabled));
		ExpandFolders->setIcon(painted_glyph_icon(chevrons(true), ink, disabled));
	}

	auto pull_request_overview_page::colours() const noexcept -> const overview_colours&
	{
		return Colours;
	}

	// ---- [[BPR-1089]]: the Comments column's chips --------------------------

	auto pull_request_overview_page::feedback_request() const -> application::overview_feedback_request
	{
		auto request    = application::overview_feedback_request{ };
		request.Columns = Columns;
		request.Mode    = Mode;
		request.Sort    = Sort;
		return request;
	}

	void pull_request_overview_page::dispatch_feedback_request()
	{
		if (!Projection || Cached || !FeedbackDispatch) {
			return;
		}
		const auto request = feedback_request();
		if (AskedFeedback == std::optional{ request }) {
			return;
		}
		AskedFeedback = request;
		FeedbackDispatch(request);
		apply_pending();
	}

	void pull_request_overview_page::present_feedback(const application::overview_feedback_projection& aFeedback)
	{
		// An answer to an earlier request is replaced by the one on its way.
		if (!Projection || Cached || aFeedback.Request != feedback_request()) {
			return;
		}
		const auto reorder = !Model->feedback() || Model->feedback()->Order != aFeedback.Order;
		Model->apply_feedback(aFeedback);
		if (reorder) {
			apply_rows();
		}
		present_feedback_controls();
		apply_pending();
	}

	auto pull_request_overview_page::ordered_rows() const -> std::vector<application::overview_file_row>
	{
		if (!Projection) {
			return { };
		}
		const auto& feedback = Model->feedback();
		if (!feedback || feedback->Order.empty()) {
			return Projection->Files;
		}
		auto byPath = std::unordered_map<std::string_view, const application::overview_file_row*>{ };
		byPath.reserve(Projection->Files.size());
		for (const auto& row : Projection->Files) {
			byPath.emplace(row.Path, &row);
		}
		auto rows = std::vector<application::overview_file_row>{ };
		rows.reserve(Projection->Files.size());
		for (const auto& path : feedback->Order) {
			if (const auto found = byPath.find(path); found != byPath.end()) {
				rows.push_back(*found->second);
				byPath.erase(found);
			}
		}
		for (const auto& row : Projection->Files) {
			if (byPath.contains(row.Path)) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	void pull_request_overview_page::apply_rows()
	{
		if (!Projection) {
			return;
		}
		// File order needs no copy of the rows.
		if (const auto& feedback = Model->feedback(); !feedback || feedback->Order.empty()) {
			Model->apply(Projection->Files);
			return;
		}
		Model->apply(ordered_rows());
	}

	void pull_request_overview_page::present_feedback_controls()
	{
		const auto& feedback = Model->feedback();
		const auto  shown    = feedback.has_value() && Projection.has_value() && !Cached;
		{
			const QSignalBlocker openBlocker{ OpenCounts };
			const QSignalBlocker allBlocker{ AllCounts };
			OpenCounts->setChecked(Mode == core::feedback_resolution_mode::unresolved);
			AllCounts->setChecked(Mode == core::feedback_resolution_mode::all);
		}
		OpenCounts->setVisible(shown);
		AllCounts->setVisible(shown);
		LabelKey->setVisible(shown);
		const auto* whole     = shown ? &feedback->PullRequest.Counts : nullptr;
		const auto  qualified = whole != nullptr && (whole->Uncertain != 0U || !whole->Exact);
		Uncertain->setVisible(qualified);
		if (qualified) {
			Uncertain->setText(count_text(whole->Uncertain));
			auto chip  = overview_comment_chip{ };
			chip.Kind  = overview_comment_chip::kind::uncertain;
			chip.Count = whole->Uncertain;
			Uncertain->setToolTip(chip_tooltip(chip, *whole, any_columns(feedback)));
			Uncertain->setAccessibleName(QStringLiteral("Not counted: %1").arg(count_text(whole->Uncertain)));
			Uncertain->setAccessibleDescription(qualifier_clauses(*whole).join(QStringLiteral("; ")));
			Uncertain->setEnabled(whole->Uncertain != 0U);
		}
		FileColumns->set_fixed_width(overview_file_model::comments_column, shown ? std::optional{ overview_comments_width } : std::nullopt);
	}

	void pull_request_overview_page::refresh_feedback_icons()
	{
		const auto ratio = devicePixelRatioF();
		auto       state = Colours.States;
		state.Faint      = feedback_state_faint(palette());
		OpenCounts->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unresolved, state, ratio) });
		static_cast<counts_mode_button*>(OpenCounts)->set_mark(core::feedback_resolution::unresolved, state);
		Uncertain->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) });
		LabelKey->setIcon(painted_glyph_icon(paint_key, palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text)));
	}

	void pull_request_overview_page::set_label_columns(const core::feedback_label_columns& aColumns)
	{
		if (Columns == aColumns) {
			return;
		}
		Columns = aColumns;
		// A sort by a chip that is gone returns to the file order.
		if (Sort && ((Sort->Kind == core::feedback_count_kind::dedicated && std::ranges::find(core::labels_of(Columns), Sort->Label) == core::labels_of(Columns).end()) || (Sort->Kind == core::feedback_count_kind::catchall && Columns.Count == 0U))) {
			Sort.reset();
		}
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_default_mode(const core::feedback_resolution_mode aMode)
	{
		DefaultMode = aMode;
		if (!ModeChosen && Mode != aMode) {
			Mode = aMode;
			present_feedback_controls();
			dispatch_feedback_request();
		}
	}

	void pull_request_overview_page::choose_mode(const core::feedback_resolution_mode aMode)
	{
		ModeChosen = true;
		if (Mode == aMode) {
			return;
		}
		Mode = aMode;
		present_feedback_controls();
		dispatch_feedback_request();
	}

	void pull_request_overview_page::choose_sort(std::optional<core::feedback_count_target> aSort)
	{
		if (Sort == aSort) {
			return;
		}
		Sort = std::move(aSort);
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Colours.Marks = aTheme;
		Files->viewport()->update();
	}

	void pull_request_overview_page::set_attention_colour(const QColor& aColour)
	{
		Colours.States.Open = aColour;
		refresh_feedback_icons();
		Files->viewport()->update();
	}

	void pull_request_overview_page::set_feedback_dispatch(overview_feedback_dispatch aDispatch)
	{
		FeedbackDispatch = std::move(aDispatch);
		AskedFeedback.reset();
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_drill_down_dispatch(overview_drill_down_dispatch aDispatch)
	{
		DrillDownDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::drill(const application::overview_feedback_row_kind aRow, const std::string& aPath, const core::feedback_count_target& aTarget)
	{
		if (!DrillDownDispatch || !Model->feedback()) {
			return;
		}
		auto down   = application::overview_drill_down{ };
		down.Row    = aRow;
		down.Path   = aPath;
		down.Target = aTarget;
		DrillDownDispatch(down);
	}

	auto pull_request_overview_page::chip_at(const QPoint& aViewportPosition) const -> std::optional<std::pair<QModelIndex, overview_comment_chip>>
	{
		const auto index = Files->indexAt(aViewportPosition);
		if (!index.isValid() || index.column() != overview_file_model::comments_column) {
			return std::nullopt;
		}
		const auto* counts = Model->feedback_at(index);
		if (counts == nullptr) {
			return std::nullopt;
		}
		for (auto& chip : overview_comment_chips(*counts, Files->visualRect(index), Files->font())) {
			if (chip.Box.contains(QPointF{ aViewportPosition })) {
				return std::pair{ index, std::move(chip) };
			}
		}
		return std::nullopt;
	}

	auto pull_request_overview_page::make_comments_menu(const QModelIndex& aIndex) -> QMenu*
	{
		const auto  cell   = aIndex.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = Model->feedback_at(cell);
		const auto  row    = Model->feedback_row(cell);
		if (counts == nullptr || !row) {
			return nullptr;
		}
		const auto  anyColumns = any_columns(Model->feedback());
		const auto  name       = row->first == application::overview_feedback_row_kind::remainder ? QStringLiteral("Not on a file")
			: row->second.empty()                                                                 ? QStringLiteral("/")
																								  : text(row->second);
		auto* const menu       = new QMenu{ this };
		// A popup is its own window, which takes the application's palette
		// rather than the theme the window shows, unless it propagates.
		menu->setAttribute(Qt::WA_WindowPropagation);
		menu->setObjectName(QStringLiteral("overviewCommentsMenu"));
		menu->setTitle(QStringLiteral("Comments: %1").arg(name));
		menu->setAccessibleName(menu->title());
		const auto ratio  = devicePixelRatioF();
		const auto open   = counts->Mode == core::feedback_resolution_mode::unresolved;
		const auto amount = [&](const std::size_t aCount, const core::feedback_state_counts& aStates) {
			return open ? QStringLiteral("%1 open").arg(count_text(aCount)) : QStringLiteral("%1 (%2)").arg(count_text(aCount), breakdown_text(aStates));
		};
		const auto add = [&](const QIcon& aIcon, const QString& aText, const core::feedback_count_target& aTarget) {
			auto* const action = menu->addAction(aIcon, aText);
			connect(action, &QAction::triggered, this, [this, kind = row->first, path = row->second, aTarget] { drill(kind, path, aTarget); });
		};
		for (const auto& column : counts->Dedicated) {
			if (column.Count != 0U) {
				add(QIcon{ feedback_mark_pixmap(column.Label, { }, feedback_mark_colours_for(Colours.Marks, column.Label, palette()), ratio) },
					QStringLiteral("%1: %2").arg(feedback_label_text(column.Label), amount(column.Count, column.States)), { .Kind = core::feedback_count_kind::dedicated, .Label = column.Label });
			}
		}
		if (counts->Catchall != 0U) {
			const auto bubble = painted_glyph_icon([anyColumns](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, anyColumns); },
												   palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text));
			add(bubble, QStringLiteral("%1: %2").arg(anyColumns ? QStringLiteral("Other") : QStringLiteral("Comments"), amount(counts->Catchall, counts->CatchallStates)),
				{ .Kind = core::feedback_count_kind::catchall });
		}
		if (counts->Uncertain != 0U) {
			auto state  = Colours.States;
			state.Faint = feedback_state_faint(palette());
			add(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) }, QStringLiteral("Not counted: %1").arg(count_text(counts->Uncertain)),
				{ .Kind = core::feedback_count_kind::uncertain });
		}
		if (menu->actions().isEmpty()) {
			menu->addAction(open ? QStringLiteral("No open threads") : QStringLiteral("No threads"))->setEnabled(false);
		}
		const auto clauses = qualifier_clauses(*counts);
		if (!clauses.isEmpty()) {
			menu->addSeparator();
			menu->addAction(clauses.join(QStringLiteral("; ")))->setEnabled(false);
		}
		menu->addSeparator();
		auto* const sort = make_sort_menu();
		sort->setParent(menu, sort->windowFlags());
		menu->addMenu(sort);
		return menu;
	}

	auto pull_request_overview_page::make_sort_menu() -> QMenu*
	{
		auto* const menu = new QMenu{ QStringLiteral("Sort by"), this };
		menu->setAttribute(Qt::WA_WindowPropagation);
		menu->setObjectName(QStringLiteral("overviewSortMenu"));
		auto* const group      = new QActionGroup{ menu };
		const auto  anyColumns = Columns.Count != 0U;
		const auto  add        = [&](const QString& aText, std::optional<core::feedback_count_target> aTarget) {
			auto* const action = menu->addAction(aText);
			action->setCheckable(true);
			action->setChecked(Sort == aTarget);
			group->addAction(action);
			connect(action, &QAction::triggered, this, [this, aTarget] { choose_sort(aTarget); });
		};
		add(QStringLiteral("File order"), std::nullopt);
		add(QStringLiteral("All comments"), core::feedback_count_target{ .Kind = core::feedback_count_kind::included });
		for (const auto label : core::labels_of(Columns)) {
			add(feedback_label_text(label), core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = label });
		}
		if (anyColumns) {
			add(QStringLiteral("Other"), core::feedback_count_target{ .Kind = core::feedback_count_kind::catchall });
		}
		add(QStringLiteral("Not counted"), core::feedback_count_target{ .Kind = core::feedback_count_kind::uncertain });
		return menu;
	}

	void pull_request_overview_page::open_comments_menu(const QModelIndex& aIndex, const QPoint& aGlobal)
	{
		auto* const menu = make_comments_menu(aIndex);
		if (menu == nullptr) {
			return;
		}
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->popup(aGlobal);
	}

	void pull_request_overview_page::open_file_at(const QModelIndex& aIndex)
	{
		const auto path = aIndex.siblingAtColumn(overview_file_model::file_column).data(overview_file_model::path_role).toString();
		if (Projection && !path.isEmpty() && FileDispatch) {
			FileDispatch(path);
		} else if (Cached && !path.isEmpty() && CachedSourceDispatch) {
			CachedSourceDispatch(path);
		}
	}

	bool pull_request_overview_page::eventFilter(QObject* const aObject, QEvent* const aEvent)
	{
		if (aObject == Files->viewport()) {
			const auto type = aEvent->type();
			if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease || type == QEvent::MouseButtonDblClick) {
				const auto* mouse = static_cast<QMouseEvent*>(aEvent);
				const auto  chip  = mouse->button() == Qt::LeftButton ? chip_at(mouse->position().toPoint()) : std::nullopt;
				if (chip) {
					if (type == QEvent::MouseButtonPress) {
						// The chip's row becomes current; a chip opens no file.
						// The pointer leaves no keyboard ring behind.
						Files->setCurrentIndex(chip->first);
						focus_chip({ }, nullptr);
						Files->setFocus(Qt::MouseFocusReason);
					} else if (type == QEvent::MouseButtonRelease) {
						activate_chip(chip->first, chip->second);
					}
					return true;
				}
			}
		} else if (aObject == Files && aEvent->type() == QEvent::KeyPress) {
			if (handle_files_key(*static_cast<QKeyEvent*>(aEvent))) {
				return true;
			}
		} else if (aObject == Files && aEvent->type() == QEvent::FocusIn) {
			// Tab into the files lands on the row, as it always has.
			const auto reason = static_cast<QFocusEvent*>(aEvent)->reason();
			if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) {
				focus_chip({ }, nullptr);
			}
		} else if (aObject == Files && aEvent->type() == QEvent::FocusOut && ChipFocus) {
			Files->viewport()->update();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	auto pull_request_overview_page::handle_files_key(const QKeyEvent& aKey) -> bool
	{
		const auto current   = Files->currentIndex();
		const auto modifiers = aKey.modifiers() & ~Qt::KeypadModifier;
		const auto plain     = modifiers == Qt::NoModifier;
		const auto enter     = aKey.key() == Qt::Key_Return || aKey.key() == Qt::Key_Enter;
		const auto chips     = current.isValid() ? row_chips(current) : std::vector<overview_comment_chip>{ };
		const auto at        = focused_chip_position(chips);
		// [[BPR-1677]]. The document start and end, Command-Up and
		// Command-Down on macOS and Ctrl+Home and Ctrl+End elsewhere, go to
		// the top and bottom row of the list, as the owner asked. A chip
		// focused on the old row is left as any other row change leaves it.
		if (const auto edge = document_edge_for(aKey); edge.has_value() && move_files_to(*edge)) {
			return true;
		}
		// Tab steps from the row onto its chips and along them; past the last
		// it leaves the files as Tab always does. Shift+Tab steps back to the
		// row. The tree keeps Left and Right for folders until a chip is on.
		if (aKey.key() == Qt::Key_Tab && plain) {
			if (!at) {
				if (chips.empty()) {
					return false;
				}
				focus_chip(current, &chips.front());
				return true;
			}
			if (*at + 1U < chips.size()) {
				focus_chip(current, &chips[*at + 1U]);
				return true;
			}
			focus_chip({ }, nullptr);
			return false;
		}
		if (aKey.key() == Qt::Key_Backtab || (aKey.key() == Qt::Key_Tab && modifiers == Qt::ShiftModifier)) {
			if (!at) {
				return false;
			}
			focus_chip(current, *at == 0U ? nullptr : &chips[*at - 1U]);
			return true;
		}
		if (at) {
			if ((aKey.key() == Qt::Key_Left || aKey.key() == Qt::Key_Right) && plain) {
				const auto next = aKey.key() == Qt::Key_Right ? std::min(*at + 1U, chips.size() - 1U) : (*at == 0U ? 0U : *at - 1U);
				focus_chip(current, &chips[next]);
				return true;
			}
			if ((enter || aKey.key() == Qt::Key_Space) && plain) {
				activate_chip(current.siblingAtColumn(overview_file_model::comments_column), chips[*at]);
				return true;
			}
			if (aKey.key() == Qt::Key_Escape && plain) {
				focus_chip({ }, nullptr);
				return true;
			}
		}
		// Enter on a file's row reads it, whichever cell is current. Qt's item
		// view emits no activation for Return on macOS, so the page opens it
		// itself.
		if (enter && plain && current.isValid() && !current.data(overview_file_model::folder_role).toBool()) {
			open_file_at(current);
			return true;
		}
		return false;
	}

	auto pull_request_overview_page::move_files_to(const document_edge aEdge) -> bool
	{
		// The first or last row the tree shows among `aParent`'s children.
		const auto shownChild = [this, aEdge](const QModelIndex& aParent) {
			const auto count = Model->rowCount(aParent);
			for (auto step = 0; step < count; ++step) {
				const auto at = aEdge == document_edge::start ? step : count - 1 - step;
				if (!Files->isRowHidden(at, aParent)) {
					return Model->index(at, 0, aParent);
				}
			}
			return QModelIndex{ };
		};
		auto target = shownChild({ });
		// The bottom row is the last shown row of the deepest open folder.
		while (aEdge == document_edge::end && target.isValid() && Files->isExpanded(target)) {
			const auto last = shownChild(target);
			if (!last.isValid()) {
				break;
			}
			target = last;
		}
		if (!target.isValid()) {
			return false;
		}
		// The current column stays, as the tree's own Home and End keep it.
		const auto current = Files->currentIndex();
		Files->setCurrentIndex(current.isValid() ? target.siblingAtColumn(current.column()) : target);
		if (aEdge == document_edge::start) {
			Files->scrollToTop();
		} else {
			Files->scrollToBottom();
		}
		return true;
	}

	auto pull_request_overview_page::row_chips(const QModelIndex& aRow) const -> std::vector<overview_comment_chip>
	{
		const auto  cell   = aRow.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = cell.isValid() ? Model->feedback_at(cell) : nullptr;
		if (counts == nullptr || Files->isColumnHidden(overview_file_model::comments_column)) {
			return { };
		}
		return overview_comment_chips(*counts, Files->visualRect(cell), Files->font());
	}

	auto pull_request_overview_page::focused_chip_position(const std::vector<overview_comment_chip>& aChips) const -> std::optional<std::size_t>
	{
		if (!ChipFocus || !ChipFocus->Row.isValid() || ChipFocus->Row != Files->currentIndex().siblingAtColumn(0)) {
			return std::nullopt;
		}
		const auto found = std::ranges::find_if(aChips, [this](const overview_comment_chip& aChip) { return aChip.Kind == ChipFocus->Kind && aChip.Label == ChipFocus->Label; });
		return found == aChips.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(found - aChips.begin()) };
	}

	auto pull_request_overview_page::focused_chip() const -> std::optional<overview_comment_chip>
	{
		const auto chips = row_chips(Files->currentIndex());
		const auto at    = focused_chip_position(chips);
		return at ? std::optional{ chips[*at] } : std::nullopt;
	}

	void pull_request_overview_page::focus_chip(const QModelIndex& aRow, const overview_comment_chip* const aChip)
	{
		if (aChip == nullptr) {
			if (!ChipFocus) {
				return;
			}
			ChipFocus.reset();
		} else {
			auto focus  = overview_chip_focus{ };
			focus.Row   = QPersistentModelIndex{ aRow.siblingAtColumn(0) };
			focus.Kind  = aChip->Kind;
			focus.Label = aChip->Label;
			ChipFocus   = std::move(focus);
		}
		RowDelegate->set_chip_focus(ChipFocus);
		Files->viewport()->update();
		if (aChip == nullptr) {
			return;
		}
		// The keyboard cannot hover: the chip's tooltip comes up beside it, and
		// a screen reader hears the same words.
		const auto  cell   = aRow.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = Model->feedback_at(cell);
		if (counts == nullptr) {
			return;
		}
		const auto tip = chip_tooltip(*aChip, *counts, any_columns(Model->feedback()));
		show_anchored_tooltip(*Files->viewport(), aChip->Box.toAlignedRect(), tip, anchored_tooltip_hold::focus);
		QAccessibleAnnouncementEvent announcement{ Files, QString{ tip }.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' }) };
		announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&announcement);
	}

	void pull_request_overview_page::activate_chip(const QModelIndex& aCell, const overview_comment_chip& aChip)
	{
		const auto row = Model->feedback_row(aCell);
		if (!row) {
			return;
		}
		switch (aChip.Kind) {
		case overview_comment_chip::kind::label:
			drill(row->first, row->second, { .Kind = core::feedback_count_kind::dedicated, .Label = aChip.Label });
			break;
		case overview_comment_chip::kind::other:
			drill(row->first, row->second, { .Kind = core::feedback_count_kind::catchall });
			break;
		case overview_comment_chip::kind::uncertain:
			if (aChip.Count != 0U) {
				drill(row->first, row->second, { .Kind = core::feedback_count_kind::uncertain });
			}
			break;
		case overview_comment_chip::kind::more:
			open_comments_menu(aCell, Files->viewport()->mapToGlobal(aChip.Box.bottomLeft().toPoint()));
			break;
		}
	}

	namespace {
		/// [[BPR-1089]]. The Label key: every mark with its name,
		/// and what the other numbers and the round marks mean. A list, so the
		/// keyboard and a screen reader can walk it; Escape closes it.
		class overview_label_key final : public QFrame
		{
		public:
			explicit overview_label_key(QWidget* aParent)
				: QFrame{ aParent, Qt::Popup }
			{
				setObjectName(QStringLiteral("overviewLabelKeyPopup"));
				// Its own window: it shows the theme the page shows, not the
				// application's palette.
				setAttribute(Qt::WA_WindowPropagation);
				setAccessibleName(QStringLiteral("Label key"));
				setFrameShape(QFrame::StyledPanel);
				auto* const layout = new QVBoxLayout{ this };
				layout->setContentsMargins(4, 4, 4, 4);
				List = new QListWidget{ this };
				List->setObjectName(QStringLiteral("overviewLabelKeyList"));
				List->setAccessibleName(QStringLiteral("Label key"));
				List->setFrameShape(QFrame::NoFrame);
				List->setIconSize(QSize{ 16, 16 });
				List->setSelectionMode(QAbstractItemView::SingleSelection);
				layout->addWidget(List);
			}
			QListWidget* List = nullptr;

		protected:
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Escape) {
					close();
					return;
				}
				QFrame::keyPressEvent(aEvent);
			}
		};
	} // namespace

	void pull_request_overview_page::show_label_key()
	{
		auto* popup = static_cast<overview_label_key*>(LabelKeyPopup);
		if (popup == nullptr) {
			popup         = new overview_label_key{ this };
			LabelKeyPopup = popup;
		}
		auto* const list = popup->List;
		list->clear();
		const auto ratio   = devicePixelRatioF();
		const auto heading = [list](const QString& aText) {
			auto* const item = new QListWidgetItem{ aText, list };
			auto        font = item->font();
			font.setWeight(QFont::DemiBold);
			item->setFont(font);
			item->setFlags(Qt::ItemIsEnabled);
		};
		const auto entry = [list](const QIcon& aIcon, const QString& aText, const QString& aAccessible, const QString& aTip) {
			auto* const item = new QListWidgetItem{ aIcon, aText, list };
			item->setData(Qt::AccessibleTextRole, aAccessible);
			item->setToolTip(aTip);
		};
		heading(QStringLiteral("Labels (the first comment of a thread)"));
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			const auto label = static_cast<core::feedback_label>(index);
			entry(QIcon{ feedback_mark_pixmap(label, { }, feedback_mark_colours_for(Colours.Marks, label, palette()), ratio) }, feedback_label_text(label),
				  QStringLiteral("%1: %2").arg(feedback_label_text(label), feedback_label_description(label)), feedback_label_description(label));
		}
		heading(QStringLiteral("Other numbers"));
		const auto ink    = palette().color(QPalette::Text);
		const auto faded  = palette().color(QPalette::Disabled, QPalette::Text);
		const auto bubble = [ink, faded](const bool aOther) {
			return painted_glyph_icon([aOther](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, aOther); }, ink, faded);
		};
		entry(bubble(true), QStringLiteral("Other labels"), QStringLiteral("Other labels: labels not ticked in Settings' Overview chips, and comments with no label"),
			  QStringLiteral("Labels not ticked in Settings' Overview chips, and comments with no label"));
		entry(bubble(false), QStringLiteral("Comments: all, when no label is ticked"), QStringLiteral("Comments: every thread, when no label is ticked in Settings"),
			  QStringLiteral("Every thread, when no label is ticked in Settings' Overview chips"));
		auto state  = Colours.States;
		state.Faint = feedback_state_faint(palette());
		for (const auto resolution : { core::feedback_resolution::unresolved, core::feedback_resolution::resolved, core::feedback_resolution::unknown, core::feedback_resolution::not_applicable }) {
			auto words = feedback_state_text(resolution);
			words[0]   = words[0].toUpper();
			entry(QIcon{ feedback_state_mark_pixmap(resolution, state, ratio) }, words, words, words);
		}
		entry(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) }, QStringLiteral("? beside a count: some not counted"),
			  QStringLiteral("Question mark beside a count: some threads are not counted, such as those whose state is unknown"),
			  QStringLiteral("Its tooltip says why. Click it or press Enter on it to list them"));
		// The keys, where someone looking up the marks will see them.
		heading(QStringLiteral("Keys"));
		// No mark of their own, but indented as the entries above are.
		auto blank = QPixmap{ QSize{ 16, 16 } * ratio };
		blank.setDevicePixelRatio(ratio);
		blank.fill(Qt::transparent);
		const auto keys = [&](const QString& aText) { entry(QIcon{ blank }, aText, aText, aText); };
		keys(QStringLiteral("Tab: from a row onto its chips, then along them"));
		keys(QStringLiteral("Left and Right: along the chips, once on one"));
		keys(QStringLiteral("Enter or Space: list a chip's threads"));
		keys(QStringLiteral("Esc: back to the row"));
		list->setCurrentRow(1);
		const auto rows = list->count();
		list->setMinimumSize(QSize{ list->sizeHintForColumn(0) + 24, std::min(rows, 32) * list->sizeHintForRow(1) + 8 });
		popup->adjustSize();
		const auto anchor = LabelKey->mapToGlobal(QPoint{ LabelKey->width() - popup->width(), LabelKey->height() + 2 });
		popup->move(anchor);
		popup->show();
		list->setFocus(Qt::PopupFocusReason);
	}

	void pull_request_overview_page::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_folder_icons();
			refresh_feedback_icons();
			if (FullView) {
				FullView->setIcon(full_view_icon(palette(), Body->expanded()));
			}
			if (DescriptionEdit) {
				DescriptionEdit->setIcon(conversation_icon(conversation_glyph::edit, palette()));
			}
		}
	}

	auto pull_request_overview_page::label_key() const noexcept -> QWidget*
	{
		return LabelKeyPopup;
	}

	auto pull_request_overview_page::open_counts_button() const noexcept -> QToolButton*
	{
		return OpenCounts;
	}

	auto pull_request_overview_page::all_counts_button() const noexcept -> QToolButton*
	{
		return AllCounts;
	}

	auto pull_request_overview_page::uncertain_button() const noexcept -> QToolButton*
	{
		return Uncertain;
	}

	auto pull_request_overview_page::label_key_button() const noexcept -> QToolButton*
	{
		return LabelKey;
	}

	auto pull_request_overview_page::count_mark(const core::feedback_count_target& aTarget, const bool aAnyColumns) const -> QPixmap
	{
		const auto ratio = devicePixelRatioF();
		switch (aTarget.Kind) {
		case core::feedback_count_kind::dedicated:
			if (aTarget.Label) {
				return feedback_mark_pixmap(*aTarget.Label, { }, feedback_mark_colours_for(Colours.Marks, *aTarget.Label, palette()), ratio);
			}
			break;
		case core::feedback_count_kind::uncertain: {
			auto state  = Colours.States;
			state.Faint = feedback_state_faint(palette());
			return feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio);
		}
		case core::feedback_count_kind::catchall:
		case core::feedback_count_kind::included:
			break;
		}
		const auto other = aTarget.Kind == core::feedback_count_kind::catchall && aAnyColumns;
		return painted_glyph_icon([other](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, other); },
								  palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text))
			.pixmap(QSize{ 16, 16 }, ratio);
	}

	void pull_request_overview_page::set_prepare_tour_action(QAction* aAction)
	{
		// [[BPR-1588]]. An icon on the Status drawer's tab row, no longer a
		// full-width button at the column's foot (RFC-BPR-0085 principle 5).
		StatusDrawer->set_tools(aAction, nullptr);
	}

	void pull_request_overview_page::set_open_on_github_action(QAction* aAction)
	{
		StatusDrawer->set_tools(nullptr, aAction);
	}

	void pull_request_overview_page::present_commit_filter(const application::commit_filter_projection& aProjection)
	{
		StatusDrawer->commits()->present(aProjection);
		CommitChip->present(aProjection);
		GeneratedRow->present(aProjection);
		FilterProjection = aProjection;
	}

	void pull_request_overview_page::set_file_filter_dispatch(std::function<void(const std::vector<std::string>&)> aDispatch)
	{
		StatusDrawer->commits()->set_file_filter_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_generated_dispatch(std::function<void(const application::generated_file_gesture&)> aDispatch)
	{
		GeneratedDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::generated_row() const noexcept -> generated_files_row*
	{
		return GeneratedRow;
	}

	auto pull_request_overview_page::make_file_menu(const QModelIndex& aIndex) -> QMenu*
	{
		auto*      menu   = make_comments_menu(aIndex);
		const auto file   = aIndex.siblingAtColumn(overview_file_model::file_column);
		const auto path   = file.data(overview_file_model::path_role).toString();
		const auto folder = file.data(overview_file_model::folder_role).toBool();
		if (folder || path.isEmpty() || !FilterProjection || !Projection) {
			return menu;
		}
		if (menu == nullptr) {
			menu = new QMenu{ this };
			menu->setAttribute(Qt::WA_WindowPropagation);
			menu->setObjectName(QStringLiteral("overviewFileMenu"));
		}
		const auto chosen = path.toStdString();
		add_generated_actions(*menu, chosen, generated_gestures_for(*FilterProjection, chosen), GeneratedDispatch);
		return menu;
	}

	void pull_request_overview_page::set_commit_filter_dispatch(std::function<void(const application::commit_filter_gesture&)> aDispatch)
	{
		CommitFilterDispatch = aDispatch;
		StatusDrawer->commits()->set_gesture_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_commit_filter_shown_dispatch(std::function<void()> aDispatch)
	{
		StatusDrawer->commits()->set_shown_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_commit_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		StatusDrawer->commits()->set_open_dispatch(std::move(aDispatch));
	}

	auto pull_request_overview_page::commit_chip() const noexcept -> commit_filter_chip*
	{
		return CommitChip;
	}

	auto pull_request_overview_page::status_drawer() const noexcept -> overview_status_drawer*
	{
		return StatusDrawer;
	}

	auto pull_request_overview_page::status_splitter() const noexcept -> QSplitter*
	{
		return StatusSplit;
	}

	auto pull_request_overview_page::entry_path() const -> std::optional<QString>
	{
		if (!Projection || !Projection->EntryPath) {
			return std::nullopt;
		}
		return text(*Projection->EntryPath);
	}

	void pull_request_overview_page::set_file_dispatch(overview_path_dispatch aDispatch)
	{
		FileDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::present_description_controls()
	{
		if (DescriptionCorner == nullptr) {
			return;
		}
		const auto hasBody  = !Body->Reader->original_body().empty();
		const auto expanded = Body->expanded();
		const auto offered  = DescriptionEditable && Projection.has_value() && !Cached && !DescriptionEditing;
		DescriptionEdit->setVisible(offered && hasBody);
		DescriptionCorner->setVisible(!FullView->isHidden() || !DescriptionEdit->isHidden());
		DescriptionCorner->adjustSize();
		DescriptionAdd->setVisible(offered && !hasBody && !expanded);
		DescriptionPanel->setVisible(DescriptionEditing);
		if (DescriptionEditing) {
			Body->setVisible(false);
			EmptyBody->setVisible(false);
			SummaryPane->setVisible(true);
		}
		SummarySplit->queue_layout();
	}

	void pull_request_overview_page::set_description_editable(const bool aEditable)
	{
		if (DescriptionEditable == aEditable) {
			return;
		}
		DescriptionEditable = aEditable;
		if (!aEditable && DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
	}

	auto pull_request_overview_page::description_editable() const noexcept -> bool
	{
		return DescriptionEditable;
	}

	void pull_request_overview_page::set_description_save_dispatch(std::function<void(const overview_description_save&)> aDispatch)
	{
		DescriptionSaveDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::begin_description_edit() -> bool
	{
		if (!DescriptionEditable || !Projection.has_value() || Cached) {
			return false;
		}
		if (!DescriptionEditing) {
			if (Body->expanded()) {
				set_description_expanded(false);
			}
			DescriptionEditing  = true;
			DescriptionSaving   = false;
			DescriptionOriginal = Projection->Body;
			DescriptionNewerText.reset();
			DescriptionText->setReadOnly(false);
			DescriptionText->present(DescriptionOriginal, true);
			DescriptionNewer->hide();
			present_description_status({ });
			present_description_controls();
		}
		DescriptionText->setFocus(Qt::OtherFocusReason);
		return true;
	}

	auto pull_request_overview_page::description_editing() const noexcept -> bool
	{
		return DescriptionEditing;
	}

	void pull_request_overview_page::present_description_status(const QString& aText, const bool aSaving)
	{
		DescriptionSaving = aSaving && DescriptionEditing;
		DescriptionStatus->setText(aText);
		DescriptionStatus->setAccessibleName(aText);
		DescriptionText->setReadOnly(DescriptionSaving);
		DescriptionSave->setEnabled(!DescriptionSaving);
		DescriptionCancel->setEnabled(!DescriptionSaving);
		DescriptionNewer->setEnabled(!DescriptionSaving);
		if (!aText.isEmpty() && DescriptionEditing) {
			QAccessibleAnnouncementEvent event{ DescriptionStatus, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	void pull_request_overview_page::offer_newer_description(std::string aNewer)
	{
		if (!DescriptionEditing) {
			return;
		}
		DescriptionNewerText = std::move(aNewer);
		DescriptionNewer->show();
	}

	void pull_request_overview_page::close_description_editor()
	{
		const auto wasEditing = DescriptionEditing;
		DescriptionEditing    = false;
		DescriptionSaving     = false;
		DescriptionNewerText.reset();
		DescriptionNewer->hide();
		DescriptionStatus->clear();
		DescriptionText->setReadOnly(false);
		DescriptionText->present({ }, true);
		DescriptionPanel->hide();
		if (wasEditing) {
			const auto hasBody = !Body->Reader->original_body().empty();
			Body->setVisible(hasBody);
			SummaryPane->setVisible(hasBody);
			EmptyBody->setVisible(Projection.has_value() && Projection->Body.empty() && !Body->expanded());
		}
		present_description_controls();
		if (wasEditing && isVisible()) {
			auto* const next = DescriptionEdit->isVisible() ? static_cast<QWidget*>(DescriptionEdit) : DescriptionAdd->isVisible() ? static_cast<QWidget*>(DescriptionAdd)
																																   : static_cast<QWidget*>(Files);
			next->setFocus(Qt::OtherFocusReason);
		}
	}

	auto pull_request_overview_page::description_edit_button() const noexcept -> QToolButton* { return DescriptionEdit; }
	auto pull_request_overview_page::description_add_button() const noexcept -> QPushButton* { return DescriptionAdd; }
	auto pull_request_overview_page::description_editor() const noexcept -> scoped_prose_editor* { return DescriptionText; }
	auto pull_request_overview_page::description_save_button() const noexcept -> QPushButton* { return DescriptionSave; }
	auto pull_request_overview_page::description_cancel_button() const noexcept -> QPushButton* { return DescriptionCancel; }
	auto pull_request_overview_page::description_newer_button() const noexcept -> QPushButton* { return DescriptionNewer; }
	auto pull_request_overview_page::description_status() const noexcept -> QLabel* { return DescriptionStatus; }
	auto pull_request_overview_page::description_original() const noexcept -> const std::string& { return DescriptionOriginal; }

	auto pull_request_overview_page::title_label() const noexcept -> QLabel*
	{
		return Title;
	}

	auto pull_request_overview_page::summary_label() const noexcept -> QLabel*
	{
		return Summary;
	}

	auto pull_request_overview_page::body_view() const noexcept -> markdown_body_view*
	{
		return Body->Reader;
	}

	auto pull_request_overview_page::files_heading() const noexcept -> QLabel*
	{
		return FilesHeading;
	}

	auto pull_request_overview_page::files_view() const noexcept -> QTreeView*
	{
		return Files;
	}

	auto pull_request_overview_page::file_model() const noexcept -> overview_file_model*
	{
		return Model;
	}

	auto pull_request_overview_page::participant_rows() const -> std::vector<overview_participant_row*>
	{
		auto rows = std::vector<overview_participant_row*>{ };
		for (int index = 0; index < Participants->count(); ++index) {
			if (auto* const row = dynamic_cast<overview_participant_row*>(Participants->itemAt(index)->widget());
				row != nullptr) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	auto pull_request_overview_page::request_rows() const -> std::vector<overview_request_row*>
	{
		auto rows = std::vector<overview_request_row*>{ };
		for (int index = 0; index < Requests->count(); ++index) {
			if (auto* const row = dynamic_cast<overview_request_row*>(Requests->itemAt(index)->widget()); row != nullptr) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	void pull_request_overview_page::present_merge(const application::pull_request_overview_projection& aProjection)
	{
		auto tone = palette().color(QPalette::Active, QPalette::PlaceholderText);
		switch (aProjection.MergeTone) {
		case application::overview_merge_tone::blocked:
			tone = Colours.Defect;
			break;
		case application::overview_merge_tone::caution:
			tone = Colours.Caution;
			break;
		case application::overview_merge_tone::mergeable:
			tone = Colours.Accepted;
			break;
		case application::overview_merge_tone::unknown:
			break;
		}
		auto statePalette = MergeState->palette();
		statePalette.setColor(QPalette::WindowText, tone);
		MergeState->setPalette(statePalette);
		MergeState->setText(text(aProjection.MergeState));
		MergeState->setAccessibleName(QStringLiteral("Merge state: %1").arg(MergeState->text()));
		MergeState->show();

		// [[BPR-1589]]. Each failed check is named in Needs attention; the
		// details carry Merge's other conditions.
		auto conditions = QStringList{ };
		for (const auto& condition : aProjection.Status.OpenConditions) {
			conditions.push_back(text(condition));
		}
		MergeConditions->setText(conditions.join(QLatin1Char{ '\n' }));
		MergeConditions->setAccessibleName(conditions.join(QStringLiteral(". ")));
		MergeConditions->setVisible(!conditions.isEmpty());

		// Rows are rebuilt only when the roster changed, so a refresh that
		// settles the same requests leaves them, and their focus, alone.
		const auto current = request_rows();
		if (!std::ranges::equal(current, aProjection.ReviewRequests, { }, [](const overview_request_row* aRow) -> const auto& { return aRow->request(); })) {
			while (Requests->count() != 0) {
				auto* const item = Requests->takeAt(0);
				delete item->widget();
				delete item;
			}
			for (const auto& request : aProjection.ReviewRequests) {
				Requests->addWidget(new overview_request_row{ request, Colours });
			}
		}
		RequestsNote->setText(text(aProjection.ReviewRequestsNote));
		RequestsNote->setAccessibleName(RequestsNote->text());
		RequestsNote->setVisible(!aProjection.ReviewRequestsNote.empty());

		RequirementDetails->setText(text(aProjection.RequirementDetails));
		RequirementDetails->setAccessibleName(RequirementDetails->text());
		RequirementDisclosure->show();
	}

	auto pull_request_overview_page::since_heading() const noexcept -> QLabel*
	{
		return SinceHeading;
	}

	auto pull_request_overview_page::value_label(const overview_value aValue) const noexcept -> QLabel*
	{
		return Values[static_cast<std::size_t>(aValue)];
	}

	auto pull_request_overview_page::minimumSizeHint() const -> QSize
	{
		// A stacked widget's minimum is the largest of its pages', shown or
		// not. The comparison decides the centre's minimum; this page takes
		// what it is given and scrolls its table.
		return { 0, 0 };
	}

	void pull_request_overview_page::set_value(const overview_value aValue, const QString& aText, const bool aHighlight)
	{
		auto* const label = Values[static_cast<std::size_t>(aValue)];
		if (label == nullptr) {
			return;
		}
		label->setText(aText);
		// A count that means new activity is in the unread colour; everything
		// else steps back, as the accepted design draws the side column.
		auto colours = palette();
		colours.setColor(QPalette::WindowText, aHighlight ? Colours.Unread : palette().color(QPalette::Active, QPalette::PlaceholderText));
		label->setPalette(colours);
		label->setForegroundRole(QPalette::WindowText);
	}

	void pull_request_overview_page::rebuild_participants(
		const std::vector<application::overview_participant>& aParticipants)
	{
		while (Participants->count() != 0) {
			auto* const item = Participants->takeAt(0);
			delete item->widget();
			delete item;
		}
		for (const auto& participant : aParticipants) {
			Participants->addWidget(new overview_participant_row{ participant, Colours });
		}
		if (aParticipants.empty()) {
			auto* const none = new QLabel{ QStringLiteral("No one yet"), this };
			none->setObjectName(QStringLiteral("overviewNoParticipants"));
			none->setTextFormat(Qt::PlainText);
			none->setForegroundRole(QPalette::PlaceholderText);
			Participants->addWidget(none);
		}
	}
	// ---- [[BPR-1589]] the Status drawer's content -----------------------------

	namespace {
		[[nodiscard]] auto plural(const std::size_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return aCount == 1U ? aOne : aMany.arg(count_text(aCount));
		}

		/// \brief Names in order: one, two joined by "and", or the first two
		/// and how many more.
		[[nodiscard]] auto name_list(const std::vector<std::string>& aNames) -> QString
		{
			if (aNames.empty()) {
				return { };
			}
			if (aNames.size() == 1U) {
				return text(aNames.front());
			}
			if (aNames.size() == 2U) {
				return QStringLiteral("%1 and %2").arg(text(aNames[0]), text(aNames[1]));
			}
			return QStringLiteral("%1, %2 and %3 more").arg(text(aNames[0]), text(aNames[1]), count_text(aNames.size() - 2U));
		}

		[[nodiscard]] auto review_words(const core::my_review_state aState) -> QString
		{
			switch (aState) {
			case core::my_review_state::approved:
				return QStringLiteral("Reviewer · approved");
			case core::my_review_state::changes_requested:
				return QStringLiteral("Reviewer · changes requested");
			case core::my_review_state::dismissed:
				return QStringLiteral("Reviewer · dismissed");
			case core::my_review_state::commented:
				break;
			}
			return QStringLiteral("Reviewer · commented");
		}

		[[nodiscard]] auto team_names(const std::vector<std::string>& aTeams) -> QString
		{
			auto names = QStringList{ };
			for (const auto& team : aTeams) {
				names.push_back(QLatin1Char{ '@' } + text(team));
			}
			return names.join(QStringLiteral(", "));
		}

		/// \brief [[BPR-1589]] v002. One role as a short label. It never says
		/// my review is required beyond GitHub's code-owner marking
		/// (RFC-BPR-0069), and it names "your team" only when GitHub said so.
		[[nodiscard]] auto role_label(const core::review_role& aRole) -> QString
		{
			switch (aRole.Kind) {
			case core::review_role_kind::author:
				return QStringLiteral("Author");
			case core::review_role_kind::requested:
				return QStringLiteral("Review requested");
			case core::review_role_kind::team_requested:
				return aRole.Membership == core::team_membership::member ? QStringLiteral("Review requested") : QStringLiteral("Team review requested");
			case core::review_role_kind::reviewed:
				return aRole.Review ? review_words(*aRole.Review) : QStringLiteral("Reviewer");
			case core::review_role_kind::mentioned:
				break;
			}
			return QStringLiteral("Mentioned");
		}

		/// \brief [[BPR-1589]] v002. The team a team role names, as a chip that
		/// moves whole to the next line rather than breaking a team's name.
		[[nodiscard]] auto role_team_chip(const core::review_role& aRole) -> QString
		{
			if (aRole.Kind != core::review_role_kind::team_requested) {
				return { };
			}
			return aRole.Membership == core::team_membership::member ? QStringLiteral("Your team: %1").arg(team_names(aRole.Teams)) : team_names(aRole.Teams);
		}

		/// \brief The tooltip's line for one role: what the label means and,
		/// for a team GitHub would not say I am in, why.
		[[nodiscard]] auto role_explanation(const core::review_role& aRole) -> QString
		{
			switch (aRole.Kind) {
			case core::review_role_kind::author:
				return QStringLiteral("You opened this pull request.");
			case core::review_role_kind::requested:
				return aRole.CodeOwner ? QStringLiteral("GitHub requested your review as a code owner.") : QStringLiteral("GitHub requested your review.");
			case core::review_role_kind::team_requested: {
				if (aRole.Membership == core::team_membership::member) {
					return QStringLiteral("GitHub requested a review from %1, a team you are in.").arg(team_names(aRole.Teams));
				}
				auto why = QStringLiteral("GitHub did not answer whether you are in it.");
				if (aRole.Reason == core::team_membership_reason::needs_read_org) {
					why = QStringLiteral("Your GitHub sign-in lacks the read:org scope, so GitHub will not say whether you are in it.");
				} else if (aRole.Reason == core::team_membership_reason::not_visible) {
					why = QStringLiteral("GitHub does not show you this organization's teams.");
				}
				return QStringLiteral("GitHub requested a review from %1. %2").arg(team_names(aRole.Teams), why);
			}
			case core::review_role_kind::reviewed:
				return QStringLiteral("Your latest review: %1.").arg(aRole.Review ? review_words(*aRole.Review).section(QStringLiteral(" · "), 1) : QStringLiteral("submitted"));
			case core::review_role_kind::mentioned:
				break;
			}
			return aRole.Mention == core::mention_place::description ? QStringLiteral("Someone mentioned you in the description.") : QStringLiteral("Someone mentioned you in a comment.");
		}
	} // namespace

	// ---- overview_role_line --------------------------------------------------

	namespace {
		constexpr int role_face   = 22;
		constexpr int role_indent = 34;
		constexpr int role_gap    = 6;
		constexpr int role_pad    = 7;

		[[nodiscard]] auto role_font(const QFont& aBase, const bool aPrimary) -> QFont
		{
			auto font = aBase;
			font.setWeight(aPrimary ? QFont::DemiBold : QFont::Normal);
			return font;
		}
	} // namespace

	overview_role_line::overview_role_line(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		setAttribute(Qt::WA_Hover, true);
	}

	void overview_role_line::present(const QString& aViewer, const QString& aPrimary, const QStringList& aChips, const QString& aExplanation)
	{
		Viewer  = aViewer;
		Primary = aPrimary;
		Chips   = aChips;
		setToolTip(aExplanation);
		setAccessibleName(Chips.isEmpty() ? QStringLiteral("My role: %1").arg(Primary) : QStringLiteral("My role: %1; also %2").arg(Primary, Chips.join(QStringLiteral(", "))));
		setAccessibleDescription(aExplanation);
		updateGeometry();
		update();
	}

	auto overview_role_line::primary() const -> const QString&
	{
		return Primary;
	}

	auto overview_role_line::chips() const -> const QStringList&
	{
		return Chips;
	}

	auto overview_role_line::layout_boxes(const int aWidth) const -> std::vector<QRect>
	{
		auto       boxes  = std::vector<QRect>{ };
		const auto room   = std::max(role_face, aWidth - role_indent - 4);
		const auto strong = QFontMetrics{ role_font(font(), true) };
		const auto plain  = QFontMetrics{ role_font(font(), false) };
		const auto line   = std::max(role_face, plain.height() + 6);
		// The label wraps rather than hiding a team's name; chips follow it on
		// its line when there is room, or start the next.
		const auto label  = strong.boundingRect(QRect{ 0, 0, room, 1'000 }, Qt::TextWordWrap, Primary);
		const auto height = std::max(line, label.height() + 4);
		boxes.emplace_back(role_indent, 2, std::min(room, label.width() + 2), height);
		const auto wrapped = label.height() > strong.height() + 2;
		auto       x       = wrapped ? role_indent + room + 1 : role_indent + boxes.back().width() + role_gap + 2;
		auto       y       = 2 + height - line;
		for (const auto& chip : Chips) {
			const auto width = std::min(room, plain.horizontalAdvance(chip) + (2 * role_pad) + 2);
			if (x + width > role_indent + room) {
				x = role_indent;
				y += line + 2;
			}
			boxes.emplace_back(x, y + 2, width, line - 4);
			x += width + role_gap;
		}
		return boxes;
	}

	auto overview_role_line::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto overview_role_line::heightForWidth(const int aWidth) const -> int
	{
		const auto boxes  = layout_boxes(aWidth);
		auto       bottom = 0;
		for (const auto& box : boxes) {
			bottom = std::max(bottom, box.bottom() + 1);
		}
		return std::max(bottom, role_face + 2) + 6;
	}

	auto overview_role_line::sizeHint() const -> QSize
	{
		return { 300, heightForWidth(300) };
	}

	auto overview_role_line::minimumSizeHint() const -> QSize
	{
		return { role_indent + role_face, heightForWidth(std::max(width(), role_indent + role_face)) };
	}

	void overview_role_line::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  boxes   = layout_boxes(width());
		const auto  middle  = std::min(boxes.front().center().y(), 2 + (role_face / 2) + 2);
		paint_identity_avatar(painter, QRectF{ 4.0, middle - (role_face / 2.0), static_cast<qreal>(role_face), static_cast<qreal>(role_face) },
							  { .Identity = Viewer, .Ring = colours.color(QPalette::Active, QPalette::PlaceholderText) }, identity_avatar_grounds_of(colours), font());
		painter.setFont(role_font(font(), true));
		painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
		painter.drawText(boxes.front(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, Primary);
		painter.setFont(role_font(font(), false));
		// A chip is the ink at a low alpha, so it reads on Light and Dark alike.
		auto fill = colours.color(QPalette::Active, QPalette::WindowText);
		fill.setAlpha(26);
		for (std::size_t index = 0U; index < static_cast<std::size_t>(Chips.size()); ++index) {
			const auto& box = boxes[index + 1U];
			painter.setPen(Qt::NoPen);
			painter.setBrush(fill);
			painter.drawRoundedRect(QRectF{ box }, box.height() / 2.0, box.height() / 2.0);
			painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
			painter.drawText(box.adjusted(role_pad, 0, -role_pad, 0), Qt::AlignCenter, painter.fontMetrics().elidedText(Chips[static_cast<qsizetype>(index)], Qt::ElideRight, box.width() - (2 * role_pad)));
		}
	}

	overview_faces::overview_faces(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("overviewPeopleFaces"));
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	namespace {
		constexpr int face_side  = 22;
		constexpr int face_step  = 26;
		constexpr int most_faces = 8;
	} // namespace

	void overview_faces::set_faces(std::vector<overview_face> aFaces)
	{
		if (Faces == aFaces) {
			return;
		}
		Faces = std::move(aFaces);
		updateGeometry();
		update();
	}

	auto overview_faces::faces() const -> const std::vector<overview_face>&
	{
		return Faces;
	}

	auto overview_faces::sizeHint() const -> QSize
	{
		const auto shown = static_cast<int>(std::min<std::size_t>(Faces.size(), most_faces));
		return QSize{ shown == 0 ? 0 : ((shown - 1) * face_step) + face_side + 2, face_side + 4 };
	}

	auto overview_faces::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void overview_faces::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto grounds = identity_avatar_grounds_of(palette());
		auto       left    = 1.0;
		for (std::size_t index = 0U; index < Faces.size() && index < most_faces; ++index) {
			const auto& face = Faces[index];
			const auto  box  = QRectF{ left, 2.0, static_cast<qreal>(face_side), static_cast<qreal>(face_side) };
			paint_identity_avatar(painter, box, { .Identity = face.Identity, .InitialsFrom = face.InitialsFrom, .Shape = face.Team ? identity_avatar_shape::group : identity_avatar_shape::person }, grounds, font());
			if (face.Badge.isValid()) {
				// The standing's mark at the corner, cut from the drawer's ground.
				const auto mark = QRectF{ box.right() - 7.0, box.bottom() - 7.0, 9.0, 9.0 };
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::AlternateBase), 2.0 });
				painter.setBrush(face.Badge);
				painter.drawEllipse(mark);
			}
			left += face_step;
		}
	}

	auto pull_request_overview_page::status_section_header(const overview_status_section aSection) const noexcept -> drawer_section_header*
	{
		return Sections[static_cast<std::size_t>(aSection)].Header;
	}

	auto pull_request_overview_page::status_section(const overview_status_section aSection) const noexcept -> QWidget*
	{
		return Sections[static_cast<std::size_t>(aSection)].Frame;
	}

	auto pull_request_overview_page::role_line() const noexcept -> overview_role_line*
	{
		return RoleRow;
	}

	auto pull_request_overview_page::people_faces() const noexcept -> overview_faces*
	{
		return Faces;
	}

	auto pull_request_overview_page::people_summary() const noexcept -> QLabel*
	{
		return PeopleSummary;
	}

	auto pull_request_overview_page::new_threads_button() const noexcept -> QToolButton*
	{
		return NewThreadsGo;
	}

	auto pull_request_overview_page::new_threads() const noexcept -> std::size_t
	{
		return NewThreads;
	}

	void pull_request_overview_page::set_new_threads_dispatch(std::function<void()> aDispatch)
	{
		NewThreadsDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::new_conversation_button() const noexcept -> QToolButton*
	{
		return NewConversationGo;
	}

	void pull_request_overview_page::set_new_conversation_dispatch(std::function<void()> aDispatch)
	{
		NewConversationDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::set_new_conversation_comments(const std::optional<std::size_t> aCount)
	{
		NewConversation = aCount;
		present_my_review();
	}

	void pull_request_overview_page::set_new_threads(const std::size_t aCount)
	{
		NewThreads = aCount;
		present_my_review();
	}

	auto pull_request_overview_page::attention_rows() const -> QStringList
	{
		auto rows = QStringList{ };
		for (int index = 0; index < AttentionList->count(); ++index) {
			if (auto* const row = AttentionList->itemAt(index)->widget(); row != nullptr && !row->isHidden()) {
				rows.push_back(row->property("statusText").toString());
			}
		}
		return rows;
	}

	void pull_request_overview_page::show_value(const overview_value aValue, const bool aShown)
	{
		if (auto* const label = Values[static_cast<std::size_t>(aValue)]; label != nullptr && label->parentWidget() != nullptr) {
			label->parentWidget()->setVisible(aShown);
		}
	}

	void pull_request_overview_page::hide_status()
	{
		StatusNote->hide();
		RoleRow->hide();
		for (const auto& section : Sections) {
			section.Frame->hide();
		}
		NewThreadsRow->hide();
		NewConversationRow->hide();
	}

	void pull_request_overview_page::remember_sections()
	{
		if (SectionKey.isEmpty()) {
			return;
		}
		auto open = std::uint8_t{ 0U };
		for (std::size_t index = 0U; index < overview_status_section_count; ++index) {
			if (Sections[index].Header->isChecked()) {
				open = static_cast<std::uint8_t>(open | (1U << index));
			}
		}
		SectionMemory.insert(SectionKey, open);
	}

	void pull_request_overview_page::present_status(const bool aSamePullRequest)
	{
		const auto& shown  = *Projection;
		const auto& status = shown.Status;
		StatusNote->hide();
		if (!aSamePullRequest) {
			// A disclosure remembers its state for the pull request in this
			// window; another pull request opens as it was left, or closed.
			SectionKey       = QStringLiteral("%1/%2#%3").arg(text(shown.Owner), text(shown.RepositoryName), QString::number(shown.Number.value()));
			const auto open  = SectionMemory.value(SectionKey, 0U);
			const auto saved = SectionKey;
			SectionKey.clear();
			for (std::size_t index = 0U; index < overview_status_section_count; ++index) {
				Sections[index].Header->setChecked((open & (1U << index)) != 0U);
				Sections[index].Details->setVisible(Sections[index].Header->isChecked());
			}
			SectionKey = saved;
		}

		// My role: the strongest leads, the others follow faintly.
		RoleRow->setVisible(status.Roles.ViewerKnown);
		if (status.Roles.ViewerKnown) {
			const auto  viewer = std::ranges::find_if(shown.Participants, &application::overview_participant::Viewer);
			const auto& roles  = status.Roles.Roles;
			auto        chips  = QStringList{ };
			auto        why    = QStringList{ };
			for (std::size_t index = 0U; index < roles.size(); ++index) {
				if (index != 0U) {
					chips.push_back(role_label(roles[index]));
				}
				if (const auto team = role_team_chip(roles[index]); !team.isEmpty()) {
					chips.push_back(team);
				}
				if (roles[index].CodeOwner && roles[index].Kind != core::review_role_kind::author) {
					chips.push_back(QStringLiteral("Code owner"));
				}
				why.push_back(role_explanation(roles[index]));
			}
			chips.removeDuplicates();
			RoleRow->present(viewer != shown.Participants.end() ? text(viewer->Login) : text(status.Viewer), roles.empty() ? QStringLiteral("No role") : role_label(roles.front()), chips,
							 roles.empty() ? QStringLiteral("You are not the author, no review was requested from you or a team you are in, and no one mentioned you.") : why.join(QLatin1Char{ '\n' }));
		}
		for (auto section : { overview_status_section::attention, overview_status_section::my_review, overview_status_section::people }) {
			Sections[static_cast<std::size_t>(section)].Frame->show();
		}
		present_attention();
		present_my_review();
		present_people();
	}

	void pull_request_overview_page::present_attention()
	{
		if (!Projection) {
			return;
		}
		const auto& shown  = *Projection;
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::attention)].Header;
		while (AttentionList->count() != 0) {
			auto* const item = AttentionList->takeAt(0);
			delete item->widget();
			delete item;
		}
		const auto& items = shown.Status.Attention;
		if (items.empty()) {
			// Nothing needs me: the section is Merge, and says its state.
			header->setText(QStringLiteral("Merge"));
			header->set_count(text(shown.MergeState));
			header->setAccessibleName(QStringLiteral("Merge: %1").arg(text(shown.MergeState)));
			return;
		}
		header->setText(QStringLiteral("Needs attention"));
		header->set_count(count_text(items.size()));
		header->setAccessibleName(QStringLiteral("Needs attention: %1").arg(plural(items.size(), QStringLiteral("1 item"), QStringLiteral("%1 items"))));
		// Each failed check is named once: here, unless Checks is open and
		// lists them.
		const auto checksOpen = Sections[static_cast<std::size_t>(overview_status_section::checks)].Header->isChecked()
			&& !Sections[static_cast<std::size_t>(overview_status_section::checks)].Frame->isHidden();
		auto* const parent = AttentionList->parentWidget();
		const auto  faint  = palette().color(QPalette::Active, QPalette::PlaceholderText);
		for (const auto& item : items) {
			auto title  = QString{ };
			auto detail = QString{ };
			auto tone   = faint;
			switch (item.Kind) {
			case core::status_attention_kind::conflicts:
				title  = QStringLiteral("Conflicts with %1").arg(text(shown.BaseRef));
				detail = QStringLiteral("GitHub cannot merge until they are resolved");
				tone   = Colours.Defect;
				break;
			case core::status_attention_kind::failed_checks:
				title = plural(item.Count, QStringLiteral("1 check failed"), QStringLiteral("%1 checks failed"));
				if (!checksOpen) {
					auto names = QStringList{ };
					for (const auto& name : item.Names) {
						names.push_back(text(name));
					}
					detail = names.join(QStringLiteral(", "));
				}
				tone = Colours.Defect;
				break;
			case core::status_attention_kind::waiting_on: {
				title            = QStringLiteral("Waiting on %1").arg(name_list(item.Names));
				const auto owner = std::ranges::any_of(shown.ReviewRequests, [&](const application::overview_review_request& aRequest) {
					return aRequest.CodeOwner && std::ranges::contains(item.Names, aRequest.Name);
				});
				detail           = owner ? QStringLiteral("Code-owner review requested") : plural(item.Count, QStringLiteral("Review requested"), QStringLiteral("%1 reviews requested"));
				break;
			}
			case core::status_attention_kind::replies_to_me:
				title = plural(item.Count, QStringLiteral("1 reply to you"), QStringLiteral("%1 replies to you"));
				tone  = Colours.Unread;
				break;
			}
			// One wrapping label: the tone's dot hangs beside the title and its
			// second line, so a long line wraps under its own text.
			const auto ink  = palette().color(QPalette::Active, QPalette::WindowText);
			auto       html = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"16\" valign=\"top\"><span style=\"color:%1\">&#9679;</span></td>"
											 "<td valign=\"top\"><span style=\"color:%2\">%3</span>")
								  .arg(tone.name(), ink.name(), title.toHtmlEscaped());
			if (!detail.isEmpty()) {
				html += QStringLiteral("<br><span style=\"color:%1\">%2</span>").arg(faint.name(), detail.toHtmlEscaped());
			}
			html += QStringLiteral("</td></tr></table>");
			auto* const row = new QLabel{ parent };
			row->setObjectName(QStringLiteral("overviewAttentionRow"));
			row->setTextFormat(Qt::RichText);
			row->setText(html);
			row->setWordWrap(true);
			row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			row->setContentsMargins(0, 2, 0, 2);
			row->setProperty("statusText", detail.isEmpty() ? title : title + QLatin1Char{ '\n' } + detail);
			row->setAccessibleName(detail.isEmpty() ? title : title + QStringLiteral(": ") + detail);
			AttentionList->addWidget(row);
		}
	}

	void pull_request_overview_page::present_my_review()
	{
		if (!Projection) {
			NewThreadsRow->hide();
			NewConversationRow->hide();
			return;
		}
		const auto& progress = Projection->Progress;
		set_value(overview_value::files_read, QStringLiteral("%1 of %2").arg(count_text(progress.FilesRead), count_text(progress.FilesTotal)), false);
		show_value(overview_value::files_read, progress.FilesTotal != 0U);
		ReadMeter->set_segments({ { static_cast<double>(progress.FilesRead), Colours.Accepted } }, static_cast<double>(progress.FilesTotal));
		ReadMeter->setAccessibleName(QStringLiteral("%1 of %2 files read").arg(count_text(progress.FilesRead), count_text(progress.FilesTotal)));
		ReadMeter->setVisible(progress.FilesTotal != 0U);
		set_value(overview_value::private_notes, count_text(progress.PrivateNotes), false);
		show_value(overview_value::private_notes, progress.PrivateNotes != 0U);
		set_value(overview_value::drafted_comments, count_text(progress.DraftedComments.value_or(0U)), false);
		show_value(overview_value::drafted_comments, progress.DraftedComments.value_or(0U) != 0U);
		NewThreadsText->setText(plural(NewThreads, QStringLiteral("1 thread with new posts"), QStringLiteral("%1 threads with new posts")));
		NewThreadsGo->setAccessibleName(QStringLiteral("Go to the %1").arg(NewThreadsText->text()));
		auto unread = palette();
		unread.setColor(QPalette::WindowText, Colours.Unread);
		NewThreadsText->setPalette(unread);
		for (auto* const go : { NewThreadsGo, NewConversationGo }) {
			auto link = go->palette();
			link.setColor(QPalette::ButtonText, Colours.Unread);
			go->setPalette(link);
		}
		NewThreadsRow->setVisible(NewThreads != 0U);
		const auto conversation = NewConversation.value_or(0U);
		NewConversationText->setText(plural(conversation, QStringLiteral("1 new comment in the conversation"), QStringLiteral("%1 new comments in the conversation")));
		NewConversationText->setPalette(unread);
		NewConversationGo->setAccessibleName(QStringLiteral("Go to the %1 in Discuss").arg(NewConversationText->text()));
		NewConversationRow->setVisible(conversation != 0U);
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::my_review)].Header;
		// The files-read row under it says the count; the header does not repeat it.
		header->setAccessibleName(QStringLiteral("My review: %1 of %2 files read").arg(count_text(progress.FilesRead), count_text(progress.FilesTotal)));
	}

	void pull_request_overview_page::present_people()
	{
		const auto& shown = *Projection;
		auto        faces = std::vector<overview_face>{ };
		auto        tally = std::array<std::size_t, 3>{ };
		for (const auto& person : shown.Participants) {
			auto face = overview_face{ .Identity = text(person.Login) };
			switch (person.Signoff) {
			case application::overview_signoff::approved:
				face.Badge = Colours.Accepted;
				++tally[0];
				break;
			case application::overview_signoff::changes_requested:
				face.Badge = Colours.Defect;
				++tally[1];
				break;
			case application::overview_signoff::commented:
				++tally[2];
				break;
			case application::overview_signoff::none:
			case application::overview_signoff::uploaded:
			case application::overview_signoff::dismissed:
			case application::overview_signoff::pending:
				break;
			}
			faces.push_back(std::move(face));
		}
		for (const auto& request : shown.ReviewRequests) {
			const auto identity = text(request.Name);
			faces.push_back({ .Identity = identity, .InitialsFrom = request.Team ? identity.mid(identity.indexOf(QLatin1Char{ '/' }) + 1) : QString{ }, .Team = request.Team });
		}
		Faces->set_faces(std::move(faces));
		auto words = QStringList{ };
		if (tally[0] != 0U) {
			words.push_back(QStringLiteral("%1 approved").arg(count_text(tally[0])));
		}
		if (tally[1] != 0U) {
			words.push_back(QStringLiteral("%1 requested changes").arg(count_text(tally[1])));
		}
		if (tally[2] != 0U) {
			words.push_back(QStringLiteral("%1 commented").arg(count_text(tally[2])));
		}
		if (!shown.ReviewRequests.empty()) {
			words.push_back(QStringLiteral("%1 requested").arg(count_text(shown.ReviewRequests.size())));
		}
		if (words.isEmpty()) {
			words.push_back(plural(shown.Participants.size(), QStringLiteral("1 person"), QStringLiteral("%1 people")));
		}
		PeopleSummary->setText(words.join(QStringLiteral(", ")));
		Faces->setAccessibleName(QStringLiteral("People: %1").arg(PeopleSummary->text()));
		const auto  people = shown.Participants.size() + shown.ReviewRequests.size();
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::people)].Header;
		header->set_count(count_text(people));
		header->setAccessibleName(QStringLiteral("People: %1").arg(PeopleSummary->text()));
	}
} // namespace bpr::qt
