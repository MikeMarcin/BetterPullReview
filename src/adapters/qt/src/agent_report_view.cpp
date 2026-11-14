#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		// docs/design/report-card-v4/SPEC.md, the drawings' points.
		constexpr int column_width      = 572;
		constexpr int title_top         = 22;
		constexpr int button_right      = 14;
		constexpr int button_top        = 12;
		constexpr int button_width      = 32;
		constexpr int button_height     = 28;
		constexpr int popover_width     = 286;
		constexpr int popover_gap       = 10;
		constexpr int popover_arrow     = 7;
		constexpr int row_height        = 24;
		constexpr int sub_indent        = 14;
		constexpr int kind_role         = Qt::UserRole;
		constexpr int index_role        = Qt::UserRole + 1;
		constexpr int current_role      = Qt::UserRole + 2;
		constexpr int depth_role        = Qt::UserRole + 4;
		constexpr int kind_section      = 0;
		constexpr int kind_divider      = 1;
		constexpr int kind_conversation = 2;
		constexpr int kind_source       = 3;
		constexpr int kind_copy         = 4;

		[[nodiscard]] auto menu_glyph(const QColor& aInk) -> QIcon
		{
			auto pixmap = QPixmap{ 30, 30 };
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ aInk, 2.0, Qt::SolidLine, Qt::RoundCap });
			for (const auto y : { 9.0, 15.0, 21.0 }) {
				painter.drawLine(QPointF{ 7.0, y }, QPointF{ 23.0, y });
			}
			return QIcon{ pixmap };
		}

		/// A heading's Markdown title as it reads rendered: code spans,
		/// emphasis and links give up their marks.
		[[nodiscard]] auto plain_heading(QString aTitle) -> QString
		{
			static const auto link     = QRegularExpression{ QStringLiteral(R"(!?\[([^\]]*)\]\([^)]*\))") };
			static const auto emphasis = QRegularExpression{ QStringLiteral(R"((\*\*|__|\*|_)(\S(?:.*?\S)?)\1)") };
			// A link keeps its words (a code span among them); outside a code
			// span, emphasis gives up its marks.
			aTitle.replace(link, QStringLiteral("\\1"));
			auto       plain = QString{ };
			const auto spans = aTitle.split(QLatin1Char('`'));
			for (auto at = 0; at < spans.size(); ++at) {
				auto span = spans[at];
				if (at % 2 == 0) {
					span.replace(emphasis, QStringLiteral("\\2"));
				}
				plain += span;
			}
			return plain.trimmed();
		}

		[[nodiscard]] auto bubble_glyph(const QColor& aInk) -> QPixmap
		{
			auto pixmap = QPixmap{ 28, 28 };
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ aInk, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			auto path = QPainterPath{ };
			path.addRoundedRect(QRectF{ 4.0, 5.0, 20.0, 14.0 }, 4.0, 4.0);
			painter.drawPath(path);
			painter.drawLine(QPointF{ 9.0, 19.0 }, QPointF{ 7.0, 24.0 });
			painter.drawLine(QPointF{ 7.0, 24.0 }, QPointF{ 13.0, 19.0 });
			pixmap.setDevicePixelRatio(2.0);
			return pixmap;
		}

		/// The popover: a raised panel hanging from the button by an arrow.
		class report_popover final : public QFrame
		{
		public:
			explicit report_popover(QWidget* aParent)
				: QFrame{ aParent, Qt::Popup }
			{
				setObjectName(QStringLiteral("agentReportMenu"));
				setAttribute(Qt::WA_TranslucentBackground, true);
				setAccessibleName(tr("Report menu"));
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box   = QRectF{ rect() }.adjusted(0.5, popover_arrow + 0.5, -0.5, -0.5);
				auto       shape = QPainterPath{ };
				shape.addRoundedRect(box, 10.0, 10.0);
				// The arrow, under the button's centre.
				const auto tip   = QPointF{ width() - button_width / 2.0, 0.5 };
				auto       arrow = QPainterPath{ };
				arrow.moveTo(tip + QPointF{ -popover_arrow, popover_arrow });
				arrow.lineTo(tip);
				arrow.lineTo(tip + QPointF{ popover_arrow, popover_arrow });
				arrow.closeSubpath();
				shape     = shape.united(arrow);
				auto edge = palette().color(QPalette::WindowText);
				edge.setAlphaF(0.18F);
				painter.setPen(QPen{ edge, 1.0 });
				painter.setBrush(palette().color(QPalette::Window));
				painter.drawPath(shape);
			}

			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Escape) {
					close();
					return;
				}
				QFrame::keyPressEvent(aEvent);
			}
		};

		/// The current section's row: a two point bar in the link colour at
		/// its left and the selection tint.
		class section_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				if (aIndex.data(kind_role).toInt() == kind_divider) {
					auto rule = aOption.palette.color(QPalette::WindowText);
					rule.setAlphaF(0.15F);
					aPainter->setPen(QPen{ rule, 1.0 });
					const auto y = aOption.rect.center().y();
					aPainter->drawLine(aOption.rect.left() + 8, y, aOption.rect.right() - 8, y);
					return;
				}
				if (aIndex.data(current_role).toBool()) {
					auto tint = aOption.palette.color(QPalette::Highlight);
					tint.setAlphaF(0.18F);
					aPainter->fillRect(aOption.rect.adjusted(4, 1, -4, -1), tint);
					aPainter->fillRect(QRect{ aOption.rect.left() + 4, aOption.rect.top() + 3, 2, aOption.rect.height() - 6 }, aOption.palette.color(QPalette::Link));
				}
				// A sub-section's row is indented 14 points.
				auto shifted = aOption;
				shifted.rect.adjust(8 + aIndex.data(depth_role).toInt() * sub_indent, 0, 0, 0);
				QStyledItemDelegate::paint(aPainter, shifted, aIndex);
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				return { QStyledItemDelegate::sizeHint(aOption, aIndex).width(), aIndex.data(kind_role).toInt() == kind_divider ? 9 : row_height };
			}
		};
	} // namespace

	agent_report_view::agent_report_view(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentReportView"));
		setAccessibleName(tr("Agent's report"));
		auto* const row = new QVBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("agentReportScroll"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setBackgroundRole(QPalette::Base);
		Scroll->viewport()->setBackgroundRole(QPalette::Base);
		row->addWidget(Scroll);
		// The page: one centred column, everything open, scrolling as one.
		auto* const page = new QWidget{ Scroll };
		page->setBackgroundRole(QPalette::Base);
		page->setAutoFillBackground(true);
		auto* const centre = new QHBoxLayout{ page };
		centre->setContentsMargins(24, title_top, 24 + button_width + button_right, 32);
		Column = new QWidget{ page };
		Column->setMaximumWidth(column_width);
		auto* const column = new QVBoxLayout{ Column };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		Title = new QLabel{ Column };
		Title->setObjectName(QStringLiteral("agentReportTitle"));
		Title->setWordWrap(true);
		Title->setTextFormat(Qt::PlainText);
		auto big = Title->font();
		big.setPointSizeF(22.0);
		big.setBold(true);
		Title->setFont(big);
		column->addWidget(Title);
		Byline = new QLabel{ Column };
		Byline->setObjectName(QStringLiteral("agentReportByline"));
		Byline->setTextFormat(Qt::RichText);
		Byline->setWordWrap(true);
		Byline->setForegroundRole(QPalette::PlaceholderText);
		auto small = Byline->font();
		small.setPointSizeF(12.0);
		Byline->setFont(small);
		connect(Byline, &QLabel::linkActivated, this, [this](const QString&) {
			if (ConversationDispatch) {
				ConversationDispatch(Document.Turn);
			}
		});
		auto* const meta = new QHBoxLayout{ };
		meta->setSpacing(6);
		auto* const icon = new QLabel{ Column };
		icon->setObjectName(QStringLiteral("agentReportQuestionIcon"));
		icon->setPixmap(bubble_glyph(palette().color(QPalette::Link)));
		meta->addWidget(icon, 0, Qt::AlignTop);
		meta->addWidget(Byline, 1);
		column->addLayout(meta);
		column->addSpacing(10);
		Body = new markdown_body_view{ Column };
		Body->setObjectName(QStringLiteral("agentReportBody"));
		Body->setAccessibleName(tr("Report"));
		// Defect 1: in place, as tall as its content; the page scrolls.
		Body->set_in_place(true);
		Body->set_height_observer([this] { fit_body(); });
		Body->set_link_dispatch([this](const std::string& aLink) {
			if (LinkDispatch) {
				LinkDispatch(QString::fromStdString(aLink));
			}
		});
		column->addWidget(Body);
		column->addStretch(1);
		// Look ask-v008: room under the last section, made when a section is
		// chosen, so any section can come to the top however short the
		// report is.
		Tail = new QWidget{ Column };
		Tail->setObjectName(QStringLiteral("agentReportTail"));
		Tail->setFixedHeight(0);
		column->addWidget(Tail);
		centre->addStretch(1);
		centre->addWidget(Column, 100);
		centre->addStretch(1);
		Scroll->setWidget(page);

		// The floating button: the pane's, not the page's.
		MenuButton = new QToolButton{ this };
		MenuButton->setObjectName(QStringLiteral("agentReportMenuButton"));
		MenuButton->setAccessibleName(tr("Report menu"));
		MenuButton->setToolTip(tr("Report menu"));
		MenuButton->setFixedSize(button_width, button_height);
		MenuButton->setIconSize(QSize{ 15, 15 });
		MenuButton->setIcon(menu_glyph(palette().color(QPalette::ButtonText)));
		MenuButton->setCheckable(true);
		MenuButton->setFocusPolicy(Qt::StrongFocus);
		auto* const shadow = new QGraphicsDropShadowEffect{ MenuButton };
		shadow->setBlurRadius(10.0);
		shadow->setOffset(0.0, 1.0);
		shadow->setColor(QColor{ 0, 0, 0, 60 });
		MenuButton->setGraphicsEffect(shadow);
		connect(MenuButton, &QToolButton::clicked, this, [this] { open_menu(); });
		MenuButton->raise();

		Popover          = new report_popover{ this };
		auto* const list = new QVBoxLayout{ Popover };
		list->setContentsMargins(6, popover_arrow + 6, 6, 6);
		Sections = new QListWidget{ Popover };
		Sections->setObjectName(QStringLiteral("agentReportOutline"));
		Sections->setAccessibleName(tr("Report sections"));
		Sections->setFrameShape(QFrame::NoFrame);
		Sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Sections->setItemDelegate(new section_delegate{ Sections });
		Sections->setAttribute(Qt::WA_TranslucentBackground, true);
		Sections->viewport()->setAutoFillBackground(false);
		Sections->installEventFilter(this);
		// The current section is marked by its bar and tint, not the list's
		// own selection; the focused row reads as a quiet tint.
		{
			auto quiet = Sections->palette();
			auto tint  = quiet.color(QPalette::Highlight);
			tint.setAlphaF(0.10F);
			quiet.setColor(QPalette::Highlight, tint);
			quiet.setColor(QPalette::HighlightedText, quiet.color(QPalette::Text));
			Sections->setPalette(quiet);
		}
		list->addWidget(Sections);
		connect(Sections, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) { choose(Sections->row(aItem)); });
		connect(Sections, &QListWidget::itemClicked, this, [this](QListWidgetItem* const aItem) { choose(Sections->row(aItem)); });
		Popover->installEventFilter(this);
		Scroll->verticalScrollBar()->installEventFilter(this);
	}

	void agent_report_view::present(const agent_report_document& aDocument)
	{
		Document = aDocument;
		Title->setText(aDocument.Title);
		Byline->setText(QStringLiteral("<a href=\"conversation\" style=\"text-decoration:none\">%1</a>%2")
							.arg(aDocument.Question.toHtmlEscaped(), aDocument.Byline.isEmpty() ? QString{ } : QStringLiteral(" &nbsp;·&nbsp; ") + aDocument.Byline.toHtmlEscaped()));
		Byline->setToolTip(tr("Show in conversation"));
		const auto text = aDocument.Markdown.toStdString();
		if (Dresser) {
			Dresser(*Body);
		}
		// Defect 5: the title once in the pane. The page shows it above, so
		// the body starts after the heading it opens with.
		const auto card    = core::report_card_of(text, aDocument.Title.toStdString());
		auto       shown   = text;
		const auto outline = core::report_outline(text);
		if (!outline.empty() && outline.front().Title == card.Title) {
			auto skipped = std::size_t{ 0 };
			auto line    = std::uint32_t{ 0 };
			while (skipped < shown.size() && line <= outline.front().Line) {
				const auto end = shown.find('\n', skipped);
				skipped        = end == std::string::npos ? shown.size() : end + 1U;
				++line;
			}
			shown = shown.substr(skipped);
		}
		Body->set_body(shown, "agent-report-" + std::to_string(aDocument.Turn));
		dress_headings();
		Headings.clear();
		Tail->setFixedHeight(0);
		// Look ask-v008 (the menu went nowhere): a section is found by the
		// block its heading renders as, in order, and named as it reads
		// there. Its Markdown title ("`src/main.cpp:6`: Not addressed") is not
		// the rendered text, and a block's heading level is no guide: the
		// paragraph under a heading can carry its level too.
		HeadingBlocks.clear();
		if (auto* const view = Body->rendered_view(); view != nullptr) {
			auto block = view->document()->begin();
			for (const auto& section : card.Outline) {
				const auto wanted = plain_heading(QString::fromStdString(section.Title));
				while (block.isValid() && block.text().trimmed() != wanted) {
					block = block.next();
				}
				if (!block.isValid()) {
					break;
				}
				HeadingBlocks.push_back(block.blockNumber());
				block = block.next();
			}
			if (HeadingBlocks.size() != card.Outline.size()) {
				HeadingBlocks.clear();
			}
		}
		const QSignalBlocker blocker{ Sections };
		Sections->clear();
		const auto add = [this](const QString& aText, const int aKind, const int aIndex, const int aIndent) {
			auto* const item = new QListWidgetItem{ aText, Sections };
			item->setData(kind_role, aKind);
			item->setData(index_role, aIndex);
			item->setData(Qt::AccessibleTextRole, aText);
			item->setData(depth_role, aIndent);
			if (aKind == kind_divider) {
				item->setFlags(Qt::NoItemFlags);
			}
			return item;
		};
		add(tr("Summary"), kind_section, 0, 0);
		auto index = 1;
		for (const auto& section : card.Outline) {
			auto name = plain_heading(QString::fromStdString(section.Title));
			if (!HeadingBlocks.empty()) {
				name = Body->rendered_view()->document()->findBlockByNumber(HeadingBlocks[static_cast<std::size_t>(index - 1)]).text().trimmed();
			}
			Headings.push_back(name);
			// A sub-section's row is indented 14 points.
			add(name, kind_section, index++, static_cast<int>(std::min<std::uint32_t>(section.Depth, 3U)));
		}
		add({ }, kind_divider, 0, 0);
		auto* const conversation = add(tr("Show in conversation"), kind_conversation, 0, 0);
		conversation->setIcon(QIcon{ bubble_glyph(palette().color(QPalette::WindowText)) });
		add(tr("View source"), kind_source, 0, 0)->setData(Qt::UserRole + 3, 0);
		add(tr("Copy source"), kind_copy, 0, 0);
		Scroll->verticalScrollBar()->setValue(0);
		fit_body();
		place_menu_button();
	}

	void agent_report_view::set_link_dispatch(std::function<void(const QString&)> aDispatch) { LinkDispatch = std::move(aDispatch); }

	void agent_report_view::set_dresser(std::function<void(markdown_body_view&)> aDresser) { Dresser = std::move(aDresser); }

	void agent_report_view::set_conversation_dispatch(std::function<void(std::size_t)> aDispatch) { ConversationDispatch = std::move(aDispatch); }

	auto agent_report_view::title() const -> QString { return Title->text(); }

	/// The spec's type: a section at 15 semibold, a sub-section at 14
	/// semibold, each with room above it.
	void agent_report_view::dress_headings()
	{
		auto* const view = Body->rendered_view();
		if (view == nullptr) {
			return;
		}
		auto shallowest = 0;
		for (auto block = view->document()->begin(); block.isValid(); block = block.next()) {
			if (const auto level = block.blockFormat().headingLevel(); level > 0) {
				shallowest = shallowest == 0 ? level : std::min(shallowest, level);
			}
		}
		for (auto block = view->document()->begin(); block.isValid(); block = block.next()) {
			const auto level = block.blockFormat().headingLevel();
			if (level <= 0) {
				continue;
			}
			auto cursor = QTextCursor{ block };
			cursor.select(QTextCursor::BlockUnderCursor);
			auto format = QTextCharFormat{ };
			format.setFontPointSize(level == shallowest ? 15.0 : 14.0);
			format.setFontWeight(QFont::DemiBold);
			cursor.mergeCharFormat(format);
			auto spacing = block.blockFormat();
			spacing.setTopMargin(level == shallowest ? 18.0 : 12.0);
			spacing.setBottomMargin(4.0);
			cursor.setBlockFormat(spacing);
		}
		fit_body();
	}

	void agent_report_view::fit_body()
	{
		const auto width = std::max(1, Column->width());
		Body->setFixedHeight(Body->heightForWidth(width));
	}

	void agent_report_view::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		place_menu_button();
		fit_body();
	}

	void agent_report_view::place_menu_button()
	{
		MenuButton->move(width() - button_right - button_width, button_top);
		MenuButton->raise();
	}

	auto agent_report_view::heading_offset(const int aIndex) const -> int
	{
		if (aIndex <= 0 || static_cast<std::size_t>(aIndex) > Headings.size()) {
			return 0;
		}
		auto* const view = Body->rendered_view();
		if (view == nullptr) {
			return -1;
		}
		// The nth heading's own block: by its place among the headings, else
		// the first heading block whose text is the heading's.
		auto block = QTextBlock{ };
		if (static_cast<std::size_t>(aIndex) <= HeadingBlocks.size()) {
			block = view->document()->findBlockByNumber(HeadingBlocks[static_cast<std::size_t>(aIndex - 1)]);
		}
		for (auto each = view->document()->begin(); !block.isValid() && each.isValid(); each = each.next()) {
			if (each.text().trimmed() == Headings[static_cast<std::size_t>(aIndex - 1)] && each.blockFormat().headingLevel() > 0) {
				block = each;
			}
		}
		if (!block.isValid()) {
			const auto found = view->document()->find(Headings[static_cast<std::size_t>(aIndex - 1)]);
			if (found.isNull()) {
				return -1;
			}
			return view->mapTo(Scroll->widget(), view->cursorRect(found).topLeft()).y();
		}
		auto cursor = QTextCursor{ block };
		return view->mapTo(Scroll->widget(), view->cursorRect(cursor).topLeft()).y();
	}

	void agent_report_view::go_to_heading(const int aIndex)
	{
		// Laid out now, inner first, so the offsets and the range are this
		// report's and not what the page last held.
		const auto settle = [this] {
			fit_body();
			Column->layout()->activate();
			Scroll->widget()->layout()->activate();
			QEvent request{ QEvent::LayoutRequest };
			QCoreApplication::sendEvent(Scroll, &request);
		};
		settle();
		const auto offset = heading_offset(aIndex);
		if (offset < 0) {
			return;
		}
		auto* const bar    = Scroll->verticalScrollBar();
		const auto  wanted = aIndex == 0 ? 0 : std::max(0, offset - 8);
		// Look ask-v008: a report that fits the pane (or a last section
		// shorter than it) cannot scroll its heading to the top; the room
		// under it is made first.
		if (wanted > bar->maximum()) {
			// The page is as tall as its least height or the pane, so the
			// least height is what the room is added to.
			const auto least = Scroll->widget()->minimumSizeHint().height();
			Tail->setFixedHeight(Tail->height() + std::max(0, wanted + Scroll->viewport()->height() - least));
			settle();
		}
		bar->setValue(wanted);
	}

	auto agent_report_view::section_top(const int aIndex) const -> int
	{
		const auto offset = heading_offset(aIndex);
		return offset < 0 ? -1 : offset - Scroll->verticalScrollBar()->value();
	}

	auto agent_report_view::current_section() const -> int
	{
		const auto top     = Scroll->verticalScrollBar()->value();
		const auto atEnd   = Scroll->verticalScrollBar()->value() >= Scroll->verticalScrollBar()->maximum() && Scroll->verticalScrollBar()->maximum() > 0;
		auto       current = 0;
		for (auto index = 1; static_cast<std::size_t>(index) <= Headings.size(); ++index) {
			const auto offset = heading_offset(index);
			if (offset < 0) {
				continue;
			}
			// At the end, where the last headings cannot reach the top, the
			// last heading on screen.
			if (offset - 8 <= top || (atEnd && offset < top + Scroll->viewport()->height())) {
				current = index;
			}
		}
		return current;
	}

	void agent_report_view::open_menu()
	{
		const auto current = current_section();
		for (auto row = 0; row < Sections->count(); ++row) {
			auto* const item = Sections->item(row);
			item->setData(current_role, item->data(kind_role).toInt() == kind_section && item->data(index_role).toInt() == current);
			if (item->data(kind_role).toInt() == kind_source) {
				item->setText(Body->source_visible() ? tr("Rendered") : tr("View source"));
			}
		}
		// Never taller than the pane less its margins; past that the sections
		// scroll.
		auto rows = 0;
		for (auto row = 0; row < Sections->count(); ++row) {
			rows += Sections->sizeHintForRow(row);
		}
		const auto tallest = std::max(120, height() - button_top - button_height - popover_gap - 24);
		const auto height  = std::min(tallest, rows + popover_arrow + 14);
		Popover->resize(popover_width, height);
		const auto corner = MenuButton->mapToGlobal(QPoint{ MenuButton->width(), MenuButton->height() + popover_gap - popover_arrow });
		Popover->move(corner.x() - popover_width, corner.y());
		MenuButton->setChecked(true);
		Popover->show();
		Sections->setFocus(Qt::PopupFocusReason);
		Sections->setCurrentRow(current);
	}

	void agent_report_view::choose(const int aRow)
	{
		auto* const item = Sections->item(aRow);
		if (item == nullptr) {
			return;
		}
		const auto kind = item->data(kind_role).toInt();
		if (kind == kind_divider) {
			return;
		}
		Popover->close();
		switch (kind) {
		case kind_section: go_to_heading(item->data(index_role).toInt()); break;
		case kind_conversation:
			if (ConversationDispatch) {
				ConversationDispatch(Document.Turn);
			}
			break;
		case kind_source:
			Body->show_source(!Body->source_visible());
			fit_body();
			break;
		case kind_copy: Body->copy_source(); break;
		default:        break;
		}
	}

	auto agent_report_view::eventFilter(QObject* const aObject, QEvent* const aEvent) -> bool
	{
		if (aObject == Popover && aEvent->type() == QEvent::Hide) {
			MenuButton->setChecked(false);
			MenuButton->setFocus(Qt::PopupFocusReason);
		}
		if (aObject == Sections && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Space || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				choose(Sections->currentRow());
				return true;
			}
			if (key->key() == Qt::Key_Escape) {
				Popover->close();
				return true;
			}
		}
		return QWidget::eventFilter(aObject, aEvent);
	}
} // namespace bpr::qt
