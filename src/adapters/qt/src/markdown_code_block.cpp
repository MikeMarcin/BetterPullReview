#include "markdown_code_block.hpp"

#include "markdown_code.hpp"

#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/drawer_parts.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextList>
#include <QTextTable>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace bpr::qt::detail {
	namespace {
		constexpr auto text_object_interface = std::string_view{ "org.qt-project.Qt.QTextObjectInterface" };
		/// The box: the radius the reader's other code boxes have, the code
		/// inset from its sides, and room above and below it.
		constexpr auto box_radius = 6.0;
		constexpr auto inset      = 10;
		constexpr auto code_top   = 6;
		constexpr auto code_foot  = 8;
		constexpr auto tool_side  = 22;
		constexpr auto glyph_side = 14.0;

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
		{
			const auto mix = [aAmount](const float aA, const float aB) { return static_cast<float>(aA + (aB - aA) * aAmount); };
			return QColor::fromRgbF(mix(aFrom.redF(), aTo.redF()), mix(aFrom.greenF(), aTo.greenF()), mix(aFrom.blueF(), aTo.blueF()));
		}

		/// \brief The code: selectable, and sideways scrolling its own; a
		/// vertical wheel belongs to the chat that holds the answer.
		class code_editor final : public QTextEdit
		{
		public:
			using QTextEdit::QTextEdit;

		protected:
			void wheelEvent(QWheelEvent* aEvent) override
			{
				const auto delta = aEvent->pixelDelta().isNull() ? aEvent->angleDelta() : aEvent->pixelDelta();
				if (std::abs(delta.y()) >= std::abs(delta.x())) {
					aEvent->ignore();
					return;
				}
				QTextEdit::wheelEvent(aEvent);
			}
		};
	} // namespace

	markdown_code_block::markdown_code_block(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("markdownCodeBlock"));
		setAttribute(Qt::WA_Hover);
		Editor = new code_editor{ this };
		Editor->setObjectName(QStringLiteral("markdownCodeBlockText"));
		Editor->setReadOnly(true);
		Editor->setAcceptRichText(false);
		Editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Editor->setFrameShape(QFrame::NoFrame);
		Editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		Editor->setLineWrapMode(QTextEdit::NoWrap);
		Editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		Editor->viewport()->setAutoFillBackground(false);
		Editor->setFocusPolicy(Qt::ClickFocus);
		Editor->setTabChangesFocus(true);
		Editor->document()->setDocumentMargin(0.0);
		Wrap = make_drawer_tool(this, QIcon{ }, tr("Wrap lines"), QStringLiteral("markdownCodeBlockWrap"));
		Wrap->setCheckable(true);
		Wrap->setFocusPolicy(Qt::TabFocus);
		Copy = make_drawer_tool(this, QIcon{ }, tr("Copy code"), QStringLiteral("markdownCodeBlockCopy"));
		Copy->setFocusPolicy(Qt::TabFocus);
		for (auto* const tool : { Wrap, Copy }) {
			tool->setIconSize(QSize{ 15, 15 });
			tool->setFixedSize(tool_side, tool_side);
		}
		CopiedFor = new QTimer{ this };
		CopiedFor->setSingleShot(true);
		CopiedFor->setInterval(1500);
		connect(CopiedFor, &QTimer::timeout, this, [this] {
			Copied = false;
			Copy->setToolTip(tr("Copy code"));
			Copy->setAccessibleName(tr("Copy code"));
			refresh_icons();
		});
		connect(Wrap, &QToolButton::toggled, this, [this](const bool aOn) {
			set_wrapped(aOn);
			if (WrapChanged) {
				WrapChanged(aOn);
			}
		});
		connect(Copy, &QToolButton::clicked, this, &markdown_code_block::copy_code);
		setTabOrder(Wrap, Copy);
		refresh_icons();
		refresh_wrap_names();
	}

	void markdown_code_block::set_code(const QString& aLanguage, const std::string& aText)
	{
		Language = aLanguage;
		Text     = aText;
		setAccessibleName(tr("%1 code").arg(Language));
		Editor->setAccessibleName(tr("%1 code").arg(Language));
		Editor->setPlainText(QString::fromStdString(Text));
		set_colours(Colours);
		update();
	}

	void markdown_code_block::set_fonts(const QFont& aFont, const QFont& aCode, const qreal aTabStopDistance)
	{
		setFont(aFont);
		Editor->setFont(aCode);
		Editor->document()->setDefaultFont(aCode);
		if (aTabStopDistance > 0.0) {
			Editor->setTabStopDistance(aTabStopDistance);
		}
		arrange();
		update();
	}

	void markdown_code_block::set_colours(const markdown_code_block_colours& aColours)
	{
		Colours = aColours;
		if (Colours.Normal.isValid()) {
			// The code's own ink, as text; its tokens' colours are layout
			// formats over it.
			auto cursor = QTextCursor{ Editor->document() };
			cursor.select(QTextCursor::Document);
			auto ink = QTextCharFormat{ };
			ink.setForeground(Colours.Normal);
			cursor.mergeCharFormat(ink);
		}
		update();
	}

	void markdown_code_block::set_line_formats(const std::vector<QList<QTextLayout::FormatRange>>& aFormats)
	{
		auto*      document = Editor->document();
		auto       index    = std::size_t{ 0U };
		auto       changed  = false;
		const auto none     = QList<QTextLayout::FormatRange>{ };
		for (auto block = document->begin(); block.isValid(); block = block.next(), ++index) {
			changed = apply_syntax_format_ranges(*document, block, index < aFormats.size() ? aFormats[index] : none) || changed;
		}
		if (changed) {
			Editor->viewport()->update();
		}
	}

	void markdown_code_block::set_wrapped(const bool aWrapped)
	{
		if (Wrap->isChecked() != aWrapped) {
			const auto blocked = Wrap->blockSignals(true);
			Wrap->setChecked(aWrapped);
			Wrap->blockSignals(blocked);
		}
		if (Wrapped == aWrapped) {
			return;
		}
		Wrapped = aWrapped;
		Editor->setLineWrapMode(aWrapped ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
		refresh_wrap_names();
	}

	auto markdown_code_block::header_font() const -> QFont
	{
		auto headerFont = font();
		if (headerFont.pointSizeF() > 0.0) {
			headerFont.setPointSizeF(headerFont.pointSizeF() * 0.86);
		}
		return headerFont;
	}

	auto markdown_code_block::header_height() const -> int
	{
		return std::max(tool_side, static_cast<int>(std::ceil(QFontMetricsF{ header_font() }.height()))) + 8;
	}

	auto markdown_code_block::height_for(const int aWidth) const -> int
	{
		const auto inner    = std::max(1, aWidth - 2 * inset);
		auto*      document = Editor->document();
		auto       content  = 0.0;
		auto       wide     = false;
		if (Wrapped) {
			const auto previous = document->textWidth();
			document->setTextWidth(inner);
			content = document->size().height();
			document->setTextWidth(previous);
		} else {
			content = document->size().height();
			wide    = document->idealWidth() > inner + 0.5;
		}
		auto bar = 0;
		if (wide) {
			// A scroll bar that takes room gets it; an overlay one lies over
			// the foot, which is kept for it.
			const auto transient = Editor->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, Editor->horizontalScrollBar()) != 0;
			bar                  = transient ? 4 : Editor->horizontalScrollBar()->sizeHint().height();
		}
		return header_height() + code_top + static_cast<int>(std::ceil(content)) + bar + code_foot;
	}

	void markdown_code_block::copy_code()
	{
		QApplication::clipboard()->setText(QString::fromStdString(Text));
		Copied = true;
		Copy->setToolTip(tr("Copied"));
		Copy->setAccessibleName(tr("Copied"));
		refresh_icons();
		CopiedFor->start();
	}

	void markdown_code_block::refresh_icons()
	{
		Wrap->setIcon(drawer_icon(drawer_glyph::wrap, palette()));
		Copy->setIcon(drawer_icon(Copied ? drawer_glyph::check : drawer_glyph::copy, palette()));
	}

	void markdown_code_block::refresh_wrap_names()
	{
		Wrap->setToolTip(Wrapped ? tr("Don't wrap") : tr("Wrap lines"));
		Wrap->setAccessibleName(Wrap->toolTip());
		Wrap->setAccessibleDescription(Wrapped ? tr("Lines wrap at the block's width.") : tr("Long lines scroll sideways."));
	}

	void markdown_code_block::arrange()
	{
		const auto header = header_height();
		const auto top    = (header - tool_side) / 2;
		Copy->move(width() - 6 - tool_side, top);
		Wrap->move(width() - 6 - 2 * tool_side - 2, top);
		Editor->setGeometry(inset, header + code_top, std::max(1, width() - 2 * inset), std::max(1, height() - header - code_top - code_foot));
	}

	void markdown_code_block::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		arrange();
	}

	void markdown_code_block::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_icons();
		}
		if (aEvent->type() == QEvent::FontChange) {
			arrange();
		}
	}

	void markdown_code_block::paintEvent(QPaintEvent*)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto back   = Colours.Back.isValid() ? Colours.Back : palette().color(QPalette::Active, QPalette::Window);
		const auto ink    = Colours.Normal.isValid() ? Colours.Normal : palette().color(QPalette::Active, QPalette::Text);
		const auto rule   = Colours.Rule.isValid() ? Colours.Rule : blend(back, ink, 0.18);
		const auto muted  = Colours.Muted.isValid() ? Colours.Muted : blend(ink, back, 0.4);
		const auto bounds = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		auto       shape  = QPainterPath{ };
		shape.addRoundedRect(bounds, box_radius, box_radius);
		painter.fillPath(shape, back);
		// The header a shade toward the ink, over a hairline.
		const auto header = static_cast<qreal>(header_height());
		painter.save();
		painter.setClipPath(shape);
		painter.fillRect(QRectF{ 0.0, 0.0, static_cast<qreal>(width()), header }, blend(back, ink, 0.045));
		painter.restore();
		painter.setPen(QPen{ rule, 1.0 });
		painter.drawLine(QPointF{ bounds.left(), header + 0.5 }, QPointF{ bounds.right(), header + 0.5 });
		painter.setBrush(Qt::NoBrush);
		painter.drawPath(shape);
		// The code glyph and the language.
		const auto unit = glyph_side / 24.0;
		const auto left = static_cast<qreal>(inset);
		const auto top  = (header - glyph_side) / 2.0;
		painter.save();
		painter.translate(left, top);
		painter.setPen(QPen{ muted, glyph_side / 13.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
		painter.drawLine(QPointF{ 8.0 * unit, 7.0 * unit }, QPointF{ 3.5 * unit, 12.0 * unit });
		painter.drawLine(QPointF{ 3.5 * unit, 12.0 * unit }, QPointF{ 8.0 * unit, 17.0 * unit });
		painter.drawLine(QPointF{ 16.0 * unit, 7.0 * unit }, QPointF{ 20.5 * unit, 12.0 * unit });
		painter.drawLine(QPointF{ 20.5 * unit, 12.0 * unit }, QPointF{ 16.0 * unit, 17.0 * unit });
		painter.drawLine(QPointF{ 13.5 * unit, 5.5 * unit }, QPointF{ 10.5 * unit, 18.5 * unit });
		painter.restore();
		const auto headerFont = header_font();
		const auto textLeft   = left + glyph_side + 6.0;
		const auto textRight  = static_cast<qreal>(Wrap->x()) - 6.0;
		painter.setFont(headerFont);
		painter.setPen(muted);
		const auto shown = QFontMetricsF{ headerFont }.elidedText(Language, Qt::ElideRight, std::max(0.0, textRight - textLeft));
		painter.drawText(QRectF{ textLeft, 0.0, std::max(0.0, textRight - textLeft), header }, Qt::AlignLeft | Qt::AlignVCenter, shown);
	}

	markdown_code_block_objects::markdown_code_block_objects(QTextEdit& aView)
		: QObject{ &aView }
		, View{ aView }
	{
		View.document()->documentLayout()->registerHandler(markdown_code_block_object, this);
		View.viewport()->installEventFilter(this);
		connect(View.document()->documentLayout(), &QAbstractTextDocumentLayout::update, this, [this] { schedule(); });
		connect(View.verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { place(); });
		connect(View.horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { place(); });
	}

	bool markdown_code_block_objects::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aWatched == View.viewport() && aEvent->type() == QEvent::Resize) {
			schedule();
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	void markdown_code_block_objects::clear()
	{
		for (auto& [ordinal, box] : Boxes) {
			delete box.data();
		}
		Boxes.clear();
	}

	void markdown_code_block_objects::create(const markdown_code_store& aCode)
	{
		auto* previous = static_cast<QWidget*>(&View);
		for (const auto& entry : aCode.blocks()) {
			if (!entry.Framed) {
				continue;
			}
			const auto  ordinal = static_cast<int>(entry.Fence.Ordinal);
			auto* const box     = new markdown_code_block{ View.viewport() };
			box->hide();
			box->set_code(markdown_code_store::language_label(entry), entry.Fence.text());
			box->set_wrapped(Wrapped.contains(ordinal));
			box->WrapChanged = [this, ordinal](const bool aWrapped) {
				if (aWrapped) {
					Wrapped.insert(ordinal);
				} else {
					Wrapped.erase(ordinal);
				}
				reflow(ordinal);
			};
			// Tab reaches each box's Wrap and Copy after the reader's links.
			QWidget::setTabOrder(previous, box->wrap_button());
			QWidget::setTabOrder(box->wrap_button(), box->copy_button());
			previous       = box->copy_button();
			Boxes[ordinal] = box;
		}
	}

	void markdown_code_block_objects::style(const markdown_code_store& aCode, const code_role_colors& aRoles, const QFont& aFont, const QFont& aCodeFont, const qreal aTabStopDistance)
	{
		if (Boxes.empty()) {
			return;
		}
		const auto& colours  = aCode.colours();
		auto*       document = View.document();
		for (const auto& entry : aCode.blocks()) {
			const auto found = Boxes.find(static_cast<int>(entry.Fence.Ordinal));
			if (!entry.Framed || found == Boxes.end() || found->second.isNull()) {
				continue;
			}
			auto* const box = found->second.data();
			box->set_fonts(aFont, aCodeFont, aTabStopDistance);
			box->set_colours({ .Back = colours.Back, .Rule = colours.Rule, .Muted = colours.Muted, .Normal = colours.Normal });
			auto formats = std::vector<QList<QTextLayout::FormatRange>>{ };
			formats.reserve(entry.Fence.Lines.size());
			for (std::uint32_t line = 0U; line < entry.Fence.Lines.size(); ++line) {
				formats.push_back(aCode.line_formats_of(entry, line, aRoles));
			}
			box->set_line_formats(formats);
		}
		// A new font can change a box's height: its place is measured again.
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(markdown_code_frame_property)) {
				document->markContentsDirty(block.position(), 1);
			}
		}
	}

	void markdown_code_block_objects::reflow(const int aOrdinal)
	{
		auto* document = View.document();
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(markdown_code_frame_property) && block.blockFormat().intProperty(markdown_code_frame_property) == aOrdinal) {
				document->markContentsDirty(block.position(), 1);
				break;
			}
		}
		if (HeightChanged) {
			HeightChanged();
		}
		place();
	}

	void markdown_code_block_objects::schedule()
	{
		if (PlacementQueued || Boxes.empty()) {
			return;
		}
		PlacementQueued = true;
		QTimer::singleShot(0, this, [this] {
			PlacementQueued = false;
			place();
		});
	}

	void markdown_code_block_objects::place()
	{
		if (Boxes.empty()) {
			return;
		}
		auto*       document = View.document();
		auto* const layout   = document->documentLayout();
		const auto  offset   = QPointF{ static_cast<qreal>(-View.horizontalScrollBar()->value()), static_cast<qreal>(-View.verticalScrollBar()->value()) };
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			const auto format = block.blockFormat();
			if (!format.hasProperty(markdown_code_frame_property)) {
				continue;
			}
			const auto found = Boxes.find(format.intProperty(markdown_code_frame_property));
			if (found == Boxes.end() || found->second.isNull()) {
				continue;
			}
			auto* const box   = found->second.data();
			const auto* lines = block.layout();
			if (!block.isVisible() || lines == nullptr || lines->lineCount() == 0) {
				box->hide();
				continue;
			}
			const auto origin = layout->blockBoundingRect(block).topLeft() + offset;
			const auto line   = lines->lineAt(0);
			const auto width  = markdown_code_block_objects::available_width(*document, block.position());
			const auto at     = QPoint{ static_cast<int>(std::lround(origin.x() + line.cursorToX(0))), static_cast<int>(std::lround(origin.y() + line.y())) };
			const auto shape  = QRect{ at, QSize{ width, box->height_for(width) } };
			if (box->geometry() != shape) {
				box->setGeometry(shape);
			}
			box->show();
		}
	}

	auto markdown_code_block_objects::qt_metacast(const char* aName) -> void*
	{
		// QTextDocumentLayout finds the handler interface by its interface
		// identifier; answering it here stands in for Q_INTERFACES.
		if (aName != nullptr && text_object_interface == aName) {
			return static_cast<QTextObjectInterface*>(this);
		}
		return QObject::qt_metacast(aName);
	}

	auto markdown_code_block_objects::available_width(const QTextDocument& aDocument, const int aPosition) -> int
	{
		auto width = aDocument.textWidth();
		if (width <= 0.0) {
			width = aDocument.pageSize().width() > 0.0 ? aDocument.pageSize().width() : 480.0;
		}
		width -= 2.0 * aDocument.documentMargin();
		const auto block  = aDocument.findBlock(aPosition);
		const auto format = block.blockFormat();
		width -= format.leftMargin() + format.rightMargin() + format.indent() * aDocument.indentWidth();
		if (const auto* list = block.textList()) {
			width -= list->format().indent() * aDocument.indentWidth();
		}
		if (const auto* table = QTextCursor{ block }.currentTable()) {
			width = width / std::max(1, table->columns()) - 2.0 * table->format().cellPadding();
		}
		return std::max(1, static_cast<int>(std::floor(width)));
	}

	auto markdown_code_block_objects::intrinsicSize(QTextDocument* aDocument, const int aPosition, const QTextFormat& aFormat) -> QSizeF
	{
		if (aDocument == nullptr) {
			return { 1.0, 1.0 };
		}
		const auto found = Boxes.find(aFormat.intProperty(markdown_code_block_ordinal));
		const auto width = available_width(*aDocument, aPosition);
		if (found == Boxes.end() || found->second.isNull()) {
			return { static_cast<qreal>(width), 1.0 };
		}
		return { static_cast<qreal>(width), static_cast<qreal>(found->second->height_for(width)) };
	}

	void markdown_code_block_objects::drawObject(QPainter*, const QRectF&, QTextDocument*, int, const QTextFormat&)
	{
		// The box is a widget over its place, drawn by itself.
	}
} // namespace bpr::qt::detail
