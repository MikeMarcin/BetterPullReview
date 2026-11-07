#include "pull_request_overview_support.hpp"

#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "button_surface.hpp"
#include "painted_glyph_icon.hpp"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractTextDocumentLayout>
#include <QBoxLayout>
#include <QBrush>
#include <QChar>
#include <QFontMetrics>
#include <QFrame>
#include <QLatin1Char>
#include <QLayout>
#include <QList>
#include <QListSpecialMethods>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QSizePolicy>
#include <QStringDecoder>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

namespace bpr::qt {
	auto files_ends_help() -> QString
	{
		const auto ends = shortcut_keys(shortcut_command::files_ends);
		return QStringLiteral("%1 and %2 go to the top and bottom row").arg(spoken_key(ends.value(0)), spoken_key(ends.value(1)));
	}

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

	overview_description_view::overview_description_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		Reader = new markdown_body_view{ this };
		Reader->set_context_controls(true);
		// [[BPR-1774]]. The description reads as read-only: no caret, so
		// only the Edit pencil suggests it can change (owner: the caret threw
		// them off). Text is still selectable.
		Reader->rendered_view()->setCursorWidth(0);
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(Reader);
		setFocusProxy(Reader);
		watch(Reader->rendered_view());
		watch(Reader->source_view());
	}

	void overview_description_view::set_corner(QWidget* aCorner)
	{
		Corner = aCorner;
		Corner->installEventFilter(this);
		place_corner();
	}

	auto overview_description_view::line_height() const -> int
	{
		return Reader->source_visible() ? Reader->source_view()->fontMetrics().lineSpacing() : Reader->rendered_view()->fontMetrics().lineSpacing();
	}

	auto overview_description_view::chrome_height() const -> int
	{
		const auto viewport = Reader->source_visible() ? Reader->source_view()->viewport()->height() : Reader->rendered_view()->viewport()->height();
		return std::max(0, Reader->height() - viewport);
	}

	auto overview_description_view::natural_height() const -> int
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

	void overview_description_view::present_body(const std::string& aText)
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

	void overview_description_view::reset_reader()
	{
		Expanded      = false;
		RenderedPlace = { };
		SourcePlace   = { };
		Adjusting     = true;
		Reader->clear();
		Adjusting = false;
		queue_layout();
	}

	void overview_description_view::set_expanded(const bool aExpanded)
	{
		remember();
		Expanded = aExpanded;
		setMinimumHeight(0);
		setMaximumHeight(QWIDGETSIZE_MAX);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		queue_layout();
	}

	auto overview_description_view::expanded() const -> bool
	{
		return Expanded;
	}

	bool overview_description_view::event(QEvent* aEvent)
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

	bool overview_description_view::eventFilter(QObject* aObject, QEvent* aEvent)
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

	void overview_description_view::remember()
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

	void overview_description_view::queue_layout()
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

	void overview_description_view::place_corner()
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

	void overview_description_view::reserve_corner(const QSize aSize)
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

	auto text(const std::string& aValue) -> QString
	{
		return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
	}

	auto operation_letter(const application::overview_file_operation aOperation)
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

	auto count_text(const std::size_t aCount) -> QString
	{
		return QString::number(static_cast<qulonglong>(aCount));
	}

	void eliding_heading::present(const QString& aText, const QString& aNote)
	{
		setText(aText);
		Note = aNote;
		refit();
	}

	auto eliding_heading::elided() const -> bool
	{
		return Elided;
	}

	auto eliding_heading::shown_text() const -> QString
	{
		return fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, contentsRect().width()));
	}

	auto eliding_heading::minimumSizeHint() const -> QSize
	{
		// One character and the ellipsis: the row may narrow the
		// heading to almost nothing rather than widen the page.
		const auto hint = QLabel::minimumSizeHint();
		return QSize{ fontMetrics().horizontalAdvance(QStringLiteral("F…")), hint.height() };
	}

	void eliding_heading::resizeEvent(QResizeEvent* const aEvent)
	{
		QLabel::resizeEvent(aEvent);
		refit();
	}

	void eliding_heading::changeEvent(QEvent* const aEvent)
	{
		QLabel::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			refit();
		}
	}

	void eliding_heading::paintEvent(QPaintEvent* const /*aEvent*/)
	{
		auto painter = QPainter{ this };
		style()->drawItemText(&painter, contentsRect(), static_cast<int>(alignment()), palette(), isEnabled(), shown_text(), foregroundRole());
	}

	void eliding_heading::refit()
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

	auto chip_font(const QFont& aFont) -> QFont
	{
		auto font = aFont;
		font.setPointSizeF(font.pointSizeF() * 0.85);
		font.setWeight(QFont::DemiBold);
		return font;
	}

	auto state_of(const core::feedback_state_counts& aStates, const core::feedback_resolution aState) -> std::size_t
	{
		return aStates[static_cast<std::size_t>(aState)];
	}

	auto conversations(const std::size_t aCount) -> QString
	{
		return aCount == 1U ? QStringLiteral("thread") : QStringLiteral("threads");
	}

	auto breakdown_text(const core::feedback_state_counts& aStates) -> QString
	{
		auto parts = QStringList{ };
		for (std::size_t state = 0U; state < core::feedback_resolution_count; ++state) {
			if (aStates[state] != 0U) {
				parts.push_back(QStringLiteral("%1 %2").arg(count_text(aStates[state]), feedback_state_text(static_cast<core::feedback_resolution>(state))));
			}
		}
		return parts.join(QStringLiteral(", "));
	}

	auto qualifier_clauses(const core::feedback_counter_projection& aCounts) -> QStringList
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

	auto any_columns(const std::optional<application::overview_feedback_projection>& aFeedback) -> bool
	{
		return aFeedback.has_value() && aFeedback->Request.Columns.Count != 0U;
	}

	auto chip_name(const overview_comment_chip& aChip, const bool aAnyColumns) -> QString
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

	auto chip_tooltip(const overview_comment_chip& aChip, const core::feedback_counter_projection& aCounts, const bool aAnyColumns) -> QString
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

	counts_mode_button::counts_mode_button(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	void counts_mode_button::set_mark(const core::feedback_resolution aState, const feedback_state_colours& aColours)
	{
		Mark        = aState;
		MarkColours = aColours;
		updateGeometry();
		update();
	}

	auto counts_mode_button::sizeHint() const -> QSize
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

	auto counts_mode_button::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void counts_mode_button::focusInEvent(QFocusEvent* const aEvent)
	{
		// A click leaves no ring that reads as a choice; Tab, an arrow
		// within the switch or a shortcut does. A window coming back
		// keeps what was shown.
		update_button_keyboard_focus(KeyboardFocus, *aEvent);
		QToolButton::focusInEvent(aEvent);
	}

	void counts_mode_button::paintEvent(QPaintEvent* /*aEvent*/)
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

	auto counts_mode_button::mark_advance() const -> int
	{
		return Mark ? static_cast<int>(feedback_mark_size) + counts_mode_gap : 0;
	}
}
