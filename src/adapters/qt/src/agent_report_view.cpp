#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_draft_all.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/core/agent_report_keep.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "markdown_document.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QApplication>
#include <QBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextObjectInterface>
#include <QTextTable>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>

namespace bpr::qt {
	namespace {
		/// Look report-templates-v003. The object a code box's place is in the
		/// page, and its slot.
		constexpr int report_code_object  = QTextFormat::UserObject + 86;
		constexpr int report_code_ordinal = QTextFormat::UserProperty + 0x0B67;

		/// Sizes each code box's place in the page to the box at the width
		/// the page is laid out at; the box is laid over it.
		class report_code_objects final : public QObject, public QTextObjectInterface
		{
		public:
			explicit report_code_objects(QObject* aParent, std::function<int(int, int)> aHeight)
				: QObject{ aParent }
				, Height{ std::move(aHeight) }
			{ }

			[[nodiscard]] auto qt_metacast(const char* aName) -> void* override
			{
				if (aName != nullptr && std::string_view{ "org.qt-project.Qt.QTextObjectInterface" } == aName) {
					return static_cast<QTextObjectInterface*>(this);
				}
				return QObject::qt_metacast(aName);
			}

			[[nodiscard]] auto intrinsicSize(QTextDocument* aDocument, const int aPosition, const QTextFormat& aFormat) -> QSizeF override
			{
				const auto width = place_width(aDocument, aPosition);
				return { static_cast<qreal>(width), static_cast<qreal>(std::max(1, Height(aFormat.intProperty(report_code_ordinal), width))) };
			}

			void drawObject(QPainter*, const QRectF&, QTextDocument*, int, const QTextFormat&) override { }

			/// An item's code is in its cell: the page less the box's borders,
			/// the rule's column and the cell's own padding at both sides.
			/// [[BPR-2238]]: read from the cell, not assumed; the cards look
			/// pads both sides of a finding's box, the plain look its left.
			[[nodiscard]] static auto place_width(QTextDocument* aDocument, const int aPosition) -> int
			{
				if (aDocument == nullptr) {
					return 1;
				}
				auto        width  = aDocument->textWidth() - 2.0 * aDocument->documentMargin();
				const auto  cursor = QTextCursor{ aDocument->findBlock(aPosition) };
				auto* const table  = cursor.currentTable();
				if (table == nullptr) {
					return std::max(1, static_cast<int>(width));
				}
				const auto shape = table->format();
				const auto cell  = table->cellAt(aPosition);
				const auto own   = cell.format().toTableCellFormat();
				const auto side  = [&](const QTextFormat::Property aSide) { return own.hasProperty(aSide) ? own.doubleProperty(aSide) : shape.cellPadding(); };
				width -= 2.0 * shape.border() + side(QTextFormat::TableCellLeftPadding) + side(QTextFormat::TableCellRightPadding) + shape.cellSpacing() * (table->columns() + 1);
				const auto widths = shape.columnWidthConstraints();
				for (auto column = 0; column < table->columns() && column < widths.size(); ++column) {
					if (column != cell.column() && widths[column].type() == QTextLength::FixedLength) {
						width -= widths[column].rawValue() + 2.0 * shape.cellPadding();
					}
				}
				return std::max(1, static_cast<int>(std::floor(width)));
			}

		private:
			std::function<int(int, int)> Height;
		};

		// docs/design/report-card-v4/SPEC.md, the drawings' points. The
		// column's width is the reading width's ([[BPR-2299]]).
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

		/// [[BPR-2338]]. Where the item a heading block heads starts, in
		/// `aView`'s viewport: the border of the outermost box the block is
		/// in (its bounding rect starts there, under its margin) (a finding's box, whose label and place stand above its
		/// heading; a section's rule), else the block's own line.
		[[nodiscard]] auto item_top(const QTextEdit& aView, const QTextBlock& aBlock) -> int
		{
			const auto  line = aView.cursorRect(QTextCursor{ aBlock }).top();
			auto* const root = aView.document()->rootFrame();
			auto*       box  = QTextCursor{ aBlock }.currentFrame();
			while (box != nullptr && box != root && box->parentFrame() != root) {
				box = box->parentFrame();
			}
			if (box == nullptr || box == root) {
				return line;
			}
			const auto drawn = aView.document()->documentLayout()->frameBoundingRect(box);
			const auto top   = static_cast<int>(std::floor(drawn.top())) - aView.verticalScrollBar()->value();
			return std::min(line, top);
		}

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

		/// [[BPR-2258]]. What a screen reader hears when a keep action ends.
		[[nodiscard]] auto kept_words(const report_draft& aDraft, const bool aMade, const QString& aWhy) -> QString
		{
			if (!aMade) {
				return aWhy.isEmpty() ? QObject::tr("Nothing was made.") : aWhy;
			}
			if (aDraft.Keep == core::report_keep::draft_comment) {
				const auto lines = aDraft.Last > aDraft.First ? QObject::tr("lines %1 to %2").arg(aDraft.First).arg(aDraft.Last) : QObject::tr("line %1").arg(aDraft.First);
				return QObject::tr("Draft comment made on %1, %2. Go to draft opens it.").arg(aDraft.Path, lines);
			}
			return aDraft.Path.isEmpty() ? QObject::tr("Kept as a local comment on the pull request.") : QObject::tr("Kept as a local comment on %1.").arg(aDraft.Path);
		}
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
		// The page is laid out at the column's width, which the scroll area
		// gives it after the view's own resize: a report opened while the
		// column was still narrow left its code boxes one pixel wide.
		Column->installEventFilter(this);
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
		// docs/design/report-templates-v2 (owner, 2026-11-14: B, Dressed, in
		// Qt's rich text): a report that follows the convention is drawn as
		// a page; the Markdown above stays for View source and for a report
		// that does not.
		// Review (2026-11-14): agent-written text never reaches a resource.
		// The page is a read-only reader over the Markdown readers' own
		// document, which denies every resource load unconditionally, and a
		// link is taken as its text (`anchorAt`), never as an address.
		Dressed = new QTextEdit{ Column };
		Dressed->setDocument(new detail::markdown_document{ Dressed });
		Dressed->setReadOnly(true);
		Dressed->setProperty("bprReader", true);
		Dressed->setAcceptRichText(false);
		Dressed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		Dressed->setMouseTracking(true);
		Dressed->setObjectName(QStringLiteral("agentReportPage"));
		Dressed->setAccessibleName(tr("Report"));
		Dressed->setFrameShape(QFrame::NoFrame);
		Dressed->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Dressed->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Dressed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Dressed->viewport()->setAutoFillBackground(false);
		Dressed->setAttribute(Qt::WA_TranslucentBackground, true);
		Dressed->document()->setDocumentMargin(0.0);
		Dressed->setVisible(false);
		Dressed->viewport()->installEventFilter(this);
		Dressed->installEventFilter(this);
		CodeObjects = new report_code_objects{ this, [this](const int aSlot, const int aWidth) { return code_height(aSlot, aWidth); } };
		column->addWidget(Dressed);
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
		// [[BPR-2299]]. As wide as the reading width lets it.
		set_reading_limit(ReadingLimit);

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
		auto said = QStringList{ };
		for (const auto& part : { aDocument.Byline.Ending, aDocument.Byline.When, aDocument.Byline.Head.isEmpty() ? QString{ } : tr("at %1").arg(aDocument.Byline.Head) }) {
			if (!part.isEmpty()) {
				said << part;
			}
		}
		const auto byline = said.join(QStringLiteral(" · "));
		Byline->setText(QStringLiteral("<a href=\"conversation\" style=\"text-decoration:none\">%1</a>%2")
							.arg(aDocument.Question.toHtmlEscaped(), byline.isEmpty() ? QString{ } : QStringLiteral(" &nbsp;·&nbsp; ") + byline.toHtmlEscaped()));
		Byline->setToolTip(tr("Show in conversation"));
		// [[BPR-2219]]. What the agent said while it worked is not the report:
		// the report starts at its title.
		const auto text = core::read_report_answer(aDocument.Markdown.toStdString()).Report;
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
		Dress    = core::dress_report(text, aDocument.Title.toStdString());
		Findings = core::report_findings(Dress, aDocument.Suggested);
		// [[BPR-2231]]. The report's own findings are kept under its text as
		// the turn holds it.
		ReportDigest = core::digest_report_findings(aDocument.Answer.isEmpty() ? text : core::read_report_answer(aDocument.Answer.toStdString()).Report);
		render_dressed();
		OutlineTitles.clear();
		for (const auto& section : card.Outline) {
			OutlineTitles.push_back(QString::fromStdString(section.Title));
		}
		locate_headings();
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
			const auto name = plain_heading(QString::fromStdString(section.Title));
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

	/// Where each heading of the menu stands in what shows.
	void agent_report_view::locate_headings()
	{
		// Look ask-v008 (the menu went nowhere): a section is found by the
		// block its heading renders as, in order, and named as it reads
		// there. Its Markdown title ("`src/main.cpp:6`: Not addressed") is not
		// the rendered text, and a block's heading level is no guide: the
		// paragraph under a heading can carry its level too.
		HeadingBlocks.clear();
		if (dressed_shown()) {
			// The dressed page: each heading's paragraph carries its anchor.
			auto found = std::map<int, int>{ };
			for (auto block = Dressed->document()->begin(); block.isValid(); block = block.next()) {
				for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
					for (const auto& name : fragment.fragment().charFormat().anchorNames()) {
						if (name.startsWith(QString::fromLatin1(report_heading_anchor))) {
							found.emplace(name.mid(static_cast<qsizetype>(std::string_view{ report_heading_anchor }.size())).toInt(), block.blockNumber());
						}
					}
				}
			}
			for (auto index = 1; index <= static_cast<int>(OutlineTitles.size()); ++index) {
				const auto at = found.find(index);
				if (at == found.end()) {
					HeadingBlocks.clear();
					break;
				}
				HeadingBlocks.push_back(at->second);
			}
		} else if (auto* const view = Body->rendered_view(); view != nullptr) {
			auto block = view->document()->begin();
			for (const auto& title : OutlineTitles) {
				const auto wanted = plain_heading(title);
				while (block.isValid() && block.text().trimmed() != wanted) {
					block = block.next();
				}
				if (!block.isValid()) {
					break;
				}
				HeadingBlocks.push_back(block.blockNumber());
				block = block.next();
			}
			if (HeadingBlocks.size() != OutlineTitles.size()) {
				HeadingBlocks.clear();
			}
		}
	}

	void agent_report_view::set_link_dispatch(std::function<void(const QString&)> aDispatch) { LinkDispatch = std::move(aDispatch); }

	void agent_report_view::set_dresser(std::function<void(markdown_body_view&)> aDresser) { Dresser = std::move(aDresser); }

	void agent_report_view::set_conversation_dispatch(std::function<void(std::uint64_t)> aDispatch) { ConversationDispatch = std::move(aDispatch); }

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
			// The heading's own text and block only. A BlockUnderCursor
			// selection starts at the separator before the block, so its
			// block format reached the paragraph above the heading too: that
			// paragraph took the heading's level (look ask-v008's report,
			// whose reasons read as headings to the menu).
			auto cursor = QTextCursor{ block };
			cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
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

	void agent_report_view::set_reading_limit(const std::optional<std::uint32_t> aLimit)
	{
		ReadingLimit = aLimit;
		// The column's resize lays the page and its code boxes out again
		// (`eventFilter`), so the report shown takes the width at once.
		Column->setMaximumWidth(aLimit.has_value() ? static_cast<int>(*aLimit) : QWIDGETSIZE_MAX);
		// Capped, the stretches either side centre it; uncapped they take
		// nothing and it fills the room.
		if (auto* const centre = qobject_cast<QBoxLayout*>(Column->parentWidget()->layout()); centre != nullptr && centre->count() == 3) {
			centre->setStretch(0, aLimit.has_value() ? 1 : 0);
			centre->setStretch(2, aLimit.has_value() ? 1 : 0);
		}
	}

	void agent_report_view::fit_body()
	{
		const auto width = std::max(1, Column->width());
		Body->setFixedHeight(Body->heightForWidth(width));
		if (Dressed != nullptr) {
			Dressed->document()->setTextWidth(width);
			Dressed->setFixedHeight(static_cast<int>(std::ceil(Dressed->document()->size().height())) + 4);
			place_code();
		}
	}

	auto agent_report_view::code_height(const int aSlot, const int aWidth) const -> int
	{
		if (aSlot < 0 || static_cast<std::size_t>(aSlot) >= CodeViews.size()) {
			return 1;
		}
		return CodeViews[static_cast<std::size_t>(aSlot)]->heightForWidth(aWidth);
	}

	/// Each code box over its place in the page.
	void agent_report_view::place_code()
	{
		auto* const document = Dressed->document();
		for (auto index = std::size_t{ 0U }; index < CodeViews.size() && index < CodePlaces.size(); ++index) {
			auto* const view = CodeViews[index];
			if (CodePlaces[index] < 0) {
				view->hide();
				continue;
			}
			auto cursor = QTextCursor{ document };
			cursor.setPosition(CodePlaces[index]);
			const auto line  = Dressed->cursorRect(cursor);
			auto       width = report_code_objects::place_width(document, CodePlaces[index]);
			// [[BPR-2238]]. Never past the right of the line its place is
			// laid out on: the cell as the layout made it.
			if (const auto block = document->findBlock(CodePlaces[index]); block.isValid()) {
				const auto laid = document->documentLayout()->blockBoundingRect(block);
				const auto room = static_cast<int>(std::floor(laid.right())) - Dressed->horizontalScrollBar()->value() - line.left();
				if (laid.width() > 0.0 && room > 0) {
					width = std::min(width, room);
				}
			}
			const auto shape = QRect{ line.left(), line.top(), width, view->heightForWidth(width) };
			if (view->geometry() != shape) {
				view->setGeometry(shape);
				// Laid out now, so its reader and the box in it take the
				// width at once, not a default one until the next pass.
				if (auto* const inner = view->layout(); inner != nullptr) {
					inner->activate();
				}
			}
		}
	}

	void agent_report_view::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		Body->apply_code_syntax(aSpans);
		for (auto* const view : CodeViews) {
			view->apply_code_syntax(aSpans);
		}
		colour_inline_code();
	}

	/// [[BPR-2266]] (owner, 2026-11-18: "Seems like we lost syntax highlights
	/// for inline code on reports"). The page's code spans in the colours the
	/// Markdown body, which holds the same text and asks for their spans,
	/// was given.
	void agent_report_view::colour_inline_code()
	{
		if (!Dress.Sections.empty()) {
			Body->style_inline_code_of(*Dressed->document());
		}
	}

	auto agent_report_view::dressed_shown() const noexcept -> bool { return !Dress.Sections.empty() && !Body->source_visible(); }

	auto agent_report_view::shown_text() const -> QTextEdit* { return dressed_shown() ? static_cast<QTextEdit*>(Dressed) : Body->rendered_view(); }

	/// The page in the report's colours, and which of the page and the
	/// Markdown shows.
	void agent_report_view::render_dressed()
	{
		const auto shown = dressed_shown();
		if (!Dress.Sections.empty()) {
			const auto top = Scroll->verticalScrollBar()->value();
			Dressed->document()->setDefaultFont(font());
			auto collected = std::vector<report_code_slot>{ };
			// The keep actions' links, minted afresh for this render: a
			// random token no answer can know, mapped to its action.
			ActionLinks.clear();
			auto actions = report_page_actions{ .Findings = Findings, .Summary = summary_offered() };
			// [[BPR-2231]]. Each action's state, as the turn keeps it.
			actions.States[0U] = keep_state(0U);
			for (const auto& finding : Findings) {
				actions.States[finding.Outline] = keep_state(finding.Outline);
			}
			// [[BPR-2286]]. Ask follow-up, and what a follow-up updated.
			actions.FollowUp = static_cast<bool>(FollowUpDispatch);
			if (RevisionSource) {
				for (const auto& finding : Findings) {
					if (const auto request = keep_request(finding.Outline); request && RevisionSource(*request)) {
						actions.Revised[finding.Outline] = request->Comment;
					}
				}
			}
			actions.DraftAll     = draft_all_offered();
			actions.DraftAllBusy = DraftAllBusy[Document.Turn];
			actions.DraftAllSaid = draft_all_said();
			// [[BPR-2331]]. What the Agent tab's cards offered and showed.
			actions.DraftAllUndo = DraftAllUndo.contains(Document.Turn);
			actions.Dismiss      = static_cast<bool>(DismissDispatch);
			for (const auto& finding : Findings) {
				if (finding.Card && *finding.Card < Document.Cards.size()) {
					// [[BPR-2352]]. As Draft comment drafts it: the agent's
					// words with the report's fix.
					if (const auto& comment = Document.Cards[*finding.Card].Finding.SuggestedComment; comment && !core::writing_blank(*comment)) {
						actions.Suggested[finding.Outline] = QString::fromStdString(finding.Comment);
					}
				}
			}
			actions.Link = [this](const std::uint32_t aOutline, const report_link aLink) {
				auto token = QStringLiteral("report-action-%1%2").arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char{ '0' }).arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char{ '0' });
				ActionLinks.emplace(token, std::pair{ aOutline, aLink });
				return token;
			};
			Dressed->setHtml(report_page_html(Dress, native_colours(), actions, &collected, Look));
			mark_report_inline_code(*Dressed->document());
			Dressed->document()->documentLayout()->registerHandler(report_code_object, CodeObjects);
			// Look report-templates-v003: each code block is the Agent tab's
			// own code box (its syntax colours, the code colours, Wrap and
			// Copy, a long line scrolling sideways), numbered from its place,
			// laid over its place in the page.
			if (collected != Slots) {
				// The last report's boxes go now, not only when the loop next
				// deletes them.
				for (auto* const view : CodeViews) {
					view->hide();
					view->deleteLater();
				}
				CodeViews.clear();
				Slots = std::move(collected);
				for (const auto& slot : Slots) {
					auto* const view = new markdown_body_view{ Dressed->viewport() };
					view->setObjectName(QStringLiteral("agentReportCode"));
					view->set_in_place(true);
					view->set_code_block_tools(true);
					view->set_code_first_line(slot.Suggestion ? std::nullopt : slot.FirstLine);
					// [[BPR-2351]]. Before the dresser binds the dispatch, so the
					// first request already names the finding's file.
					view->set_code_context(slot.Path);
					if (Dresser) {
						Dresser(*view);
					}
					auto text = std::string{ "```" } + (slot.Suggestion ? std::string{ "suggestion" } : slot.Language) + "\n";
					for (const auto& line : slot.Lines) {
						text += line + "\n";
					}
					text += "```\n";
					if (slot.Suggestion) {
						view->set_suggestions(markdown_suggestion_presentation{
							.Path  = slot.Path,
							.Lines = slot.Replaced.empty() ? std::nullopt : std::optional{ core::markdown_suggestion_context{ .FirstLine = slot.FirstLine.value_or(1U), .Lines = slot.Replaced } } });
					}
					view->set_body(text, "agent-report-" + std::to_string(Document.Turn) + "-code-" + std::to_string(CodeViews.size()));
					view->set_height_observer([this] {
						Dressed->document()->markContentsDirty(0, Dressed->document()->characterCount());
						fit_body();
						place_code();
					});
					view->show();
					CodeViews.push_back(view);
				}
			}
			// The markers become the boxes' places.
			CodePlaces.assign(Slots.size(), -1);
			for (auto index = std::size_t{ 0U }; index < Slots.size(); ++index) {
				auto found = Dressed->document()->find(QString::fromLatin1(report_code_marker) + QString::number(index), 0, QTextDocument::FindCaseSensitively | QTextDocument::FindWholeWords);
				if (found.isNull()) {
					continue;
				}
				auto format = QTextCharFormat{ };
				format.setObjectType(report_code_object);
				format.setProperty(report_code_ordinal, static_cast<int>(index));
				found.insertText(QString{ QChar::ObjectReplacementCharacter }, format);
				CodePlaces[index] = found.position() - 1;
			}
			colour_inline_code();
			Scroll->verticalScrollBar()->setValue(top);
		}
		for (auto* const view : CodeViews) {
			view->setVisible(shown);
		}
		Dressed->setVisible(shown);
		Body->setVisible(!shown);
		apply_colours();
		fit_body();
		locate_headings();
	}

	/// The page's paper and ink, and the title in the report's serif, when
	/// the page shows; the pane's own colours otherwise.
	void agent_report_view::apply_colours()
	{
		// [[BPR-2219]], round 2 (owner: "feel more native"): the page stands on
		// the pane's own ground, in its own ink and font; the report's
		// theme roles are spent only on verdicts. The title is the app's
		// font, larger and heavier, with or without the page.
		auto*      page    = Scroll->widget();
		const auto colours = palette();
		page->setPalette(colours);
		Scroll->viewport()->setPalette(colours);
		Title->setPalette(colours);
		Byline->setPalette(colours);
		auto big = font();
		big.setPointSizeF(font().pointSizeF() * 1.55);
		big.setWeight(QFont::DemiBold);
		Title->setFont(big);
	}

	/// The page's colours: the pane's own ground and ink, the theme's three
	/// tones, and a wash a little off the ground for code and
	/// the action's face.
	auto agent_report_view::native_colours() const -> report_colours
	{
		auto native  = Colours;
		native.Paper = palette().color(QPalette::Base);
		native.Ink   = palette().color(QPalette::Text);
		// A little off the ground towards the ink, in either theme: a
		// platform's alternate base can be far from its base.
		const auto towards = [](const int aGround, const int aInk) { return aGround + (aInk - aGround) * 8 / 100; };
		native.CodeWash    = QColor{ towards(native.Paper.red(), native.Ink.red()), towards(native.Paper.green(), native.Ink.green()), towards(native.Paper.blue(), native.Ink.blue()) };
		return native;
	}

	void agent_report_view::set_look(const report_look aLook)
	{
		if (Look == aLook) {
			return;
		}
		Look = aLook;
		render_dressed();
	}

	void agent_report_view::set_tone_colours(const tone_colours& aColours)
	{
		if (Colours.Tones == aColours) {
			return;
		}
		Colours.Tones = aColours;
		render_dressed();
	}

	void agent_report_view::set_draft_dispatch(report_draft_dispatch aDispatch) { DraftDispatch = std::move(aDispatch); }

	void agent_report_view::set_go_dispatch(std::function<void(const report_draft&)> aDispatch) { GoDispatch = std::move(aDispatch); }

	/// Keep summary is offered where the report has findings to sum up.
	auto agent_report_view::summary_offered() const -> bool { return !Findings.empty() && !core::report_summary_comment(Dress).empty(); }

	auto agent_report_view::keep_state(const std::uint32_t aOutline) const -> report_keep_state
	{
		const auto kept  = Keeps.find(Document.Turn);
		const auto local = [&]() -> report_keep_state {
			if (kept == Keeps.end()) {
				return { };
			}
			const auto found = kept->second.find(aOutline);
			return found == kept->second.end() ? report_keep_state{ } : found->second;
		}();
		// An action under way is the view's until it settles; what it made is
		// the turn's one state for the finding ([[BPR-2231]]).
		if (local.Phase == report_keep_phase::working || !KeepRecords) {
			return local;
		}
		const auto record = KeepRecords(keep_key(aOutline));
		switch (core::report_finding_standing_of(record.State)) {
		case core::report_finding_standing::drafted: {
			// [[BPR-2331]]. What its draft said of its lines, if anything.
			const auto key  = keep_key(aOutline);
			const auto said = std::ranges::find_if(DraftedSaid, [&key](const auto& aSaid) { return aSaid.first.Findings == key.Findings && aSaid.first.Ordinal == key.Ordinal; });
			return { .Phase = report_keep_phase::kept, .Why = said == DraftedSaid.end() ? QString{ } : said->second };
		}
		case core::report_finding_standing::kept_local: return { .Phase = report_keep_phase::kept };
		case core::report_finding_standing::refused:    return { .Phase = report_keep_phase::refused, .Why = QString::fromStdString(record.Why) };
		case core::report_finding_standing::dismissed:  return { .Phase = report_keep_phase::dismissed };
		case core::report_finding_standing::open:       return { };
		}
		return { };
	}

	auto agent_report_view::card_keys() const -> std::vector<core::report_card_key>
	{
		auto keys = std::vector<core::report_card_key>{ };
		keys.reserve(Document.Cards.size());
		for (const auto& card : Document.Cards) {
			keys.push_back(card.Key);
		}
		return keys;
	}

	/// [[BPR-2231]]. Where the action at `aOutline` is kept: a finding's card,
	/// or the report's own record at its row (0: the summary).
	auto agent_report_view::keep_key(const std::uint32_t aOutline) const -> core::report_finding_key
	{
		const auto found = std::ranges::find(Findings, aOutline, &core::report_finding::Outline);
		if (aOutline == 0U || found == Findings.end()) {
			return { .Findings = ReportDigest, .Ordinal = aOutline, .Card = false };
		}
		const auto keys = card_keys();
		return core::report_finding_key_of(*found, keys, ReportDigest);
	}

	void agent_report_view::set_keep_records(report_keep_records aRecords)
	{
		KeepRecords = std::move(aRecords);
		render_dressed();
	}

	void agent_report_view::settle_keep(const core::report_finding_key& aKey)
	{
		auto changed = false;
		for (const auto& finding : Findings) {
			const auto key = keep_key(finding.Outline);
			if (key.Findings == aKey.Findings && key.Ordinal == aKey.Ordinal) {
				changed = true;
				if (auto kept = Keeps.find(Document.Turn); kept != Keeps.end()) {
					kept->second.erase(finding.Outline);
				}
			}
		}
		if (aKey.Findings == ReportDigest && aKey.Ordinal == 0U) {
			changed = true;
		}
		if (changed) {
			render_dressed();
		}
	}

	void agent_report_view::refresh_keeps() { render_dressed(); }

	void agent_report_view::set_draft_all_dispatch(report_draft_all_dispatch aDispatch)
	{
		DraftAllDispatch = std::move(aDispatch);
		render_dressed();
	}

	void agent_report_view::set_placeable(std::function<bool(const report_draft&)> aPlaceable)
	{
		Placeable = std::move(aPlaceable);
		render_dressed();
	}

	/// [[BPR-2232]]. The report's findings as the Agent tab's Draft all reads
	/// its cards, planned by the core; with each one's row.
	auto agent_report_view::draft_all_plan() const -> std::pair<core::draft_all_plan, std::vector<std::uint32_t>>
	{
		auto facts = std::vector<core::report_draft_all_fact>{ };
		auto rows  = std::vector<std::uint32_t>{ };
		facts.reserve(Findings.size());
		for (const auto& finding : Findings) {
			const auto state   = keep_state(finding.Outline);
			const auto request = keep_request(finding.Outline);
			const auto record  = KeepRecords ? KeepRecords(keep_key(finding.Outline)) : core::agent_finding_card_record{ };
			auto       fact    = core::report_draft_all_fact{ .Standing  = core::report_finding_standing_of(record.State),
															  .Working   = state.Phase == report_keep_phase::working,
															  .Placeable = request && (!Placeable || Placeable(*request)) };
			if (!KeepRecords && state.Phase == report_keep_phase::kept) {
				fact.Standing = finding.Keep == core::report_keep::draft_comment ? core::report_finding_standing::drafted : core::report_finding_standing::kept_local;
			}
			facts.push_back(fact);
			rows.push_back(finding.Outline);
		}
		const auto candidates = core::report_draft_all_candidates(Findings, facts);
		return { core::plan_draft_all(candidates), std::move(rows) };
	}

	auto agent_report_view::draft_all_offered() const -> std::size_t
	{
		if (!DraftAllDispatch || !dressed_shown() || Findings.empty()) {
			return 0U;
		}
		// As the Agent tab's: offered with two suggested comments or more and
		// one to draft.
		const auto suggested = static_cast<std::size_t>(std::ranges::count(Findings, core::report_keep::draft_comment, &core::report_finding::Keep));
		return suggested >= core::minimum_draft_all_suggestions ? draft_all_plan().first.Draft.size() : 0U;
	}

	auto agent_report_view::draft_all_said() const -> QString
	{
		const auto found = DraftAllSaid.find(Document.Turn);
		return found == DraftAllSaid.end() ? QString{ } : found->second;
	}

	auto agent_report_view::press_draft_all() -> bool
	{
		const auto turn = Document.Turn;
		if (!DraftAllDispatch || DraftAllBusy[turn] || draft_all_offered() == 0U) {
			return false;
		}
		const auto [plan, rows] = draft_all_plan();
		auto drafts             = std::vector<report_draft>{ };
		for (const auto position : plan.Draft) {
			if (auto request = keep_request(rows[position])) {
				Keeps[turn][rows[position]] = { .Phase = report_keep_phase::working };
				drafts.push_back(std::move(*request));
			}
		}
		DraftAllBusy[turn] = true;
		DraftAllSaid[turn] = drafts.size() == 1U ? tr("Drafting 1 finding\u2026") : tr("Drafting %1 findings\u2026").arg(drafts.size());
		render_dressed();
		auto rowsDrafted = std::vector<std::uint32_t>{ };
		for (const auto& draft : drafts) {
			rowsDrafted.push_back(draft.Outline);
		}
		DraftAllDispatch(std::move(drafts), core::start_draft_all(plan), [this, turn, rowsDrafted, alive = QPointer<agent_report_view>{ this }](const core::draft_all_outcome& aOutcome) {
			if (alive.isNull()) {
				return;
			}
			DraftAllBusy[turn] = false;
			DraftAllSaid[turn] = draft_all_words(aOutcome);
			if (auto kept = Keeps.find(turn); kept != Keeps.end()) {
				for (const auto row : rowsDrafted) {
					if (const auto found = kept->second.find(row); found != kept->second.end() && found->second.Phase == report_keep_phase::working) {
						kept->second.erase(found);
					}
				}
			}
			if (Document.Turn == turn) {
				render_dressed();
			}
		});
		return true;
	}

	/// As the Agent tab's Draft all says it, of findings rather than cards.
	auto agent_report_view::draft_all_words(const core::draft_all_outcome& aOutcome) -> QString
	{
		const auto counted = [](const std::size_t aCount, const QString& aOne, const QString& aMany) { return QStringLiteral("%1 %2").arg(aCount).arg(aCount == 1U ? aOne : aMany); };
		auto       reasons = QStringList{ };
		if (aOutcome.AlreadyDrafted > 0U) {
			reasons << tr("%1 already drafted").arg(aOutcome.AlreadyDrafted);
		}
		if (aOutcome.NotInDiff > 0U) {
			reasons << tr("%1 no longer in the diff").arg(aOutcome.NotInDiff);
		}
		if (aOutcome.Dismissed > 0U) {
			reasons << tr("%1 dismissed").arg(aOutcome.Dismissed);
		}
		if (aOutcome.Busy > 0U) {
			reasons << tr("%1 still being drafted or saved").arg(aOutcome.Busy);
		}
		if (aOutcome.KeptLocal > 0U) {
			reasons << tr("%1 kept local").arg(aOutcome.KeptLocal);
		}
		if (aOutcome.Refused > 0U) {
			reasons << (aOutcome.Refused == 1U ? tr("1 could not be drafted; its finding says why") : tr("%1 could not be drafted; their findings say why").arg(aOutcome.Refused));
		}
		const auto drafted = aOutcome.Drafted == 0U ? tr("Nothing drafted.") : tr("Drafted %1.").arg(counted(aOutcome.Drafted, tr("private draft"), tr("private drafts")));
		if (reasons.isEmpty()) {
			return drafted;
		}
		return drafted + QLatin1Char{ ' ' } + tr("Skipped %1: %2.").arg(aOutcome.skipped()).arg(reasons.join(QStringLiteral(", ")));
	}

	/// The keep action at `aOutline`, as the core read it.
	auto agent_report_view::keep_request(const std::uint32_t aOutline) const -> std::optional<report_draft>
	{
		if (aOutline == 0U) {
			if (!summary_offered()) {
				return std::nullopt;
			}
			return report_draft{ .Outline = 0U, .Keep = core::report_keep::keep_local, .Comment = QString::fromStdString(core::report_summary_comment(Dress)), .Key = keep_key(0U) };
		}
		const auto found = std::ranges::find(Findings, aOutline, &core::report_finding::Outline);
		if (found == Findings.end() || found->Keep == core::report_keep::none) {
			return std::nullopt;
		}
		auto request = report_draft{ .Outline = aOutline, .Keep = found->Keep, .Path = QString::fromStdString(found->Path), .First = found->First, .Last = found->Last, .Comment = QString::fromStdString(found->Comment), .Key = keep_key(aOutline) };
		if (found->Card && *found->Card < Document.Cards.size()) {
			request.Card = Document.Cards[*found->Card];
		}
		// [[BPR-2286]]. A follow-up's update is what it drafts from now on.
		if (RevisionSource) {
			if (auto revised = RevisionSource(request)) {
				request.Comment = *revised;
				if (request.Card) {
					request.Card->Finding.SuggestedComment = revised->toStdString();
				}
			}
		}
		return request;
	}

	void agent_report_view::set_follow_up_dispatch(std::function<void(const report_draft&)> aDispatch)
	{
		FollowUpDispatch = std::move(aDispatch);
		if (Scroll != nullptr && Dressed != nullptr) {
			render_dressed();
		}
	}

	void agent_report_view::set_revision_source(std::function<std::optional<QString>(const report_draft&)> aSource)
	{
		RevisionSource = std::move(aSource);
		if (Scroll != nullptr && Dressed != nullptr) {
			render_dressed();
		}
	}

	void agent_report_view::set_dismiss_dispatch(report_dismiss_dispatch aDispatch)
	{
		DismissDispatch = std::move(aDispatch);
		if (Scroll != nullptr && Dressed != nullptr) {
			render_dressed();
		}
	}

	void agent_report_view::offer_draft_all_undo(const std::uint64_t aTurn, report_draft_all_undo aUndo)
	{
		if (aUndo) {
			DraftAllUndo[aTurn] = std::move(aUndo);
		} else {
			DraftAllUndo.erase(aTurn);
		}
		if (Document.Turn == aTurn) {
			render_dressed();
		}
	}

	auto agent_report_view::undo_draft_all() -> bool
	{
		const auto turn  = Document.Turn;
		const auto found = DraftAllUndo.find(turn);
		if (found == DraftAllUndo.end() || DraftAllBusy[turn]) {
			return false;
		}
		auto undo = std::move(found->second);
		DraftAllUndo.erase(found);
		DraftAllBusy[turn] = true;
		DraftAllSaid[turn] = tr("Removing the drafts\u2026");
		render_dressed();
		undo([this, turn, alive = QPointer<agent_report_view>{ this }](const std::size_t aRemoved, const std::size_t aKept) {
			if (alive.isNull()) {
				return;
			}
			const auto counted = [](const std::size_t aCount) { return aCount == 1U ? tr("1 draft") : tr("%1 drafts").arg(aCount); };
			const auto removed = aRemoved == 0U ? tr("Nothing removed.") : tr("Removed %1.").arg(counted(aRemoved));
			DraftAllBusy[turn] = false;
			DraftAllSaid[turn] = aKept == 0U ? removed : removed + QLatin1Char{ ' ' } + tr("Kept %1 you changed since.").arg(counted(aKept));
			if (Document.Turn == turn) {
				render_dressed();
			}
		});
		return true;
	}

	void agent_report_view::say_drafted(const core::report_finding_key& aKey, const QString& aSaid)
	{
		std::erase_if(DraftedSaid, [&aKey](const auto& aEntry) { return aEntry.first.Findings == aKey.Findings && aEntry.first.Ordinal == aKey.Ordinal; });
		if (!aSaid.isEmpty()) {
			DraftedSaid.emplace_back(aKey, aSaid);
		}
	}

	auto agent_report_view::press(const std::uint32_t aOutline, const report_link aLink) -> bool
	{
		const auto request = keep_request(aOutline);
		if (!request) {
			return false;
		}
		if (aLink == report_link::ask) {
			if (aOutline == 0U || !FollowUpDispatch) {
				return false;
			}
			FollowUpDispatch(*request);
			return true;
		}
		const auto state = keep_state(aOutline);
		// [[BPR-2331]]. Dismiss where its keep action is offered; Undo where
		// it was dismissed.
		if (aLink == report_link::dismiss || aLink == report_link::undo_dismiss) {
			const auto dismissing = aLink == report_link::dismiss;
			const auto offered    = dismissing ? state.Phase == report_keep_phase::offered || state.Phase == report_keep_phase::refused : state.Phase == report_keep_phase::dismissed;
			if (aOutline == 0U || !DismissDispatch || !offered) {
				return false;
			}
			if (!KeepRecords) {
				Keeps[Document.Turn][aOutline] = dismissing ? report_keep_state{ .Phase = report_keep_phase::dismissed } : report_keep_state{ };
			}
			DismissDispatch(*request, dismissing);
			render_dressed();
			announce(dismissing ? tr("Dismissed. Undo takes it back.") : tr("No longer dismissed."));
			return true;
		}
		if (aLink == report_link::go) {
			if (state.Phase != report_keep_phase::kept || !GoDispatch) {
				return false;
			}
			GoDispatch(*request);
			return true;
		}
		if (state.Phase == report_keep_phase::working || state.Phase == report_keep_phase::kept || state.Phase == report_keep_phase::dismissed || !DraftDispatch) {
			return false;
		}
		const auto turn       = Document.Turn;
		Keeps[turn][aOutline] = { .Phase = report_keep_phase::working };
		// [[BPR-2258]]. The report stays in front while the host makes the
		// draft; focus that was on the page comes back to it.
		const auto focused = isAncestorOf(QApplication::focusWidget());
		render_dressed();
		DraftDispatch(*request, [this, turn, aOutline, focused, made = *request, alive = QPointer<agent_report_view>{ this }](const bool aDrafted, const QString& aWhy) {
			if (alive.isNull()) {
				return;
			}
			// With the host's records, what was made is the turn's one state
			// for the finding, kept there ([[BPR-2231]]); the view keeps none.
			if (KeepRecords) {
				Keeps[turn].erase(aOutline);
			} else {
				Keeps[turn][aOutline] = aDrafted ? report_keep_state{ .Phase = report_keep_phase::kept } : report_keep_state{ .Phase = report_keep_phase::refused, .Why = aWhy };
			}
			if (Document.Turn != turn) {
				return;
			}
			render_dressed();
			if (focused) {
				Dressed->setFocus(Qt::OtherFocusReason);
				if (aDrafted) {
					focus_link(aOutline, report_link::go);
				}
			}
			announce(kept_words(made, aDrafted, aWhy));
		});
		return true;
	}

	void agent_report_view::announce(const QString& aText)
	{
		LastAnnouncement = aText;
		if (aText.isEmpty() || !isVisible()) {
			return;
		}
		auto event = QAccessibleAnnouncementEvent{ Dressed, aText };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	/// [[BPR-2258]]. The page's link for `aLink` at `aOutline`, made the
	/// keyboard's current link, as Tab to it would.
	void agent_report_view::focus_link(const std::uint32_t aOutline, const report_link aLink)
	{
		const auto minted = std::ranges::find_if(ActionLinks, [aOutline, aLink](const auto& aEntry) { return aEntry.second == std::pair{ aOutline, aLink }; });
		if (minted == ActionLinks.end()) {
			return;
		}
		for (auto block = Dressed->document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				const auto part = fragment.fragment();
				if (!part.isValid() || part.charFormat().anchorHref() != minted->first) {
					continue;
				}
				auto cursor = QTextCursor{ Dressed->document() };
				cursor.setPosition(part.position());
				cursor.setPosition(part.position() + part.length(), QTextCursor::KeepAnchor);
				Dressed->setTextCursor(cursor);
				return;
			}
		}
	}

	/// A link of the page: a keep action, or a place or address.
	void agent_report_view::follow(const QString& aLink)
	{
		// Review of 2026-11-14: an action is only a link this render minted;
		// anything the report's own text links to is a place or an address,
		// never an action, whatever it is written as.
		const auto minted = ActionLinks.find(aLink);
		if (minted == ActionLinks.end()) {
			if (LinkDispatch) {
				LinkDispatch(aLink);
			}
			return;
		}
		const auto [outline, link] = minted->second;
		if (link == report_link::draft_all) {
			static_cast<void>(press_draft_all());
			return;
		}
		if (link == report_link::undo_draft_all) {
			static_cast<void>(undo_draft_all());
			return;
		}
		static_cast<void>(press(outline, link));
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
		auto* const view = shown_text();
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
		// [[BPR-2338]] (owner, 2026-11-20: "the thing we're jumping to is cut
		// off by the tab header"). The heading is not the item's top: a
		// finding's label line stands above it in its box.
		return view->mapTo(Scroll->widget(), QPoint{ 0, item_top(*view, block) }).y();
	}

	auto agent_report_view::jump_margin() const -> int
	{
		const auto* const view = shown_text();
		return (view != nullptr ? view->fontMetrics() : fontMetrics()).lineSpacing();
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
		auto* const bar = Scroll->verticalScrollBar();
		// [[BPR-2338]]. Measuring a freshly drawn page lays it out further
		// (the page's height follows its text, and what stands above it
		// moves), so the item is aimed at again until it holds still where
		// it was aimed.
		for (auto pass = 0; pass < 4; ++pass) {
			settle();
			const auto offset = heading_offset(aIndex);
			if (offset < 0) {
				return;
			}
			const auto wanted = aIndex == 0 ? 0 : std::max(0, offset - jump_margin());
			// Look ask-v008: a report that fits the pane (or a last section
			// shorter than it) cannot scroll its heading to the top; the
			// room under it is made first.
			if (wanted > bar->maximum()) {
				// The page is as tall as its least height or the pane, so
				// the least height is what the room is added to.
				const auto least = Scroll->widget()->minimumSizeHint().height();
				Tail->setFixedHeight(Tail->height() + std::max(0, wanted + Scroll->viewport()->height() - least));
				settle();
			}
			bar->setValue(wanted);
			if (heading_offset(aIndex) == offset && bar->value() == wanted) {
				return;
			}
		}
	}

	auto agent_report_view::section_top(const int aIndex) const -> int
	{
		const auto offset = heading_offset(aIndex);
		return offset < 0 ? -1 : offset - Scroll->verticalScrollBar()->value();
	}

	auto agent_report_view::current_section() const -> int
	{
		const auto top     = Scroll->verticalScrollBar()->value();
		const auto margin  = jump_margin();
		const auto atEnd   = Scroll->verticalScrollBar()->value() >= Scroll->verticalScrollBar()->maximum() && Scroll->verticalScrollBar()->maximum() > 0;
		auto       current = 0;
		for (auto index = 1; static_cast<std::size_t>(index) <= Headings.size(); ++index) {
			const auto offset = heading_offset(index);
			if (offset < 0) {
				continue;
			}
			// At the end, where the last headings cannot reach the top, the
			// last heading on screen.
			if (offset - margin <= top || (atEnd && offset < top + Scroll->viewport()->height())) {
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
			render_dressed();
			break;
		case kind_copy: Body->copy_source(); break;
		default:        break;
		}
	}

	auto agent_report_view::eventFilter(QObject* const aObject, QEvent* const aEvent) -> bool
	{
		if (aObject == Column && aEvent->type() == QEvent::Resize) {
			if (const auto* const resized = static_cast<QResizeEvent*>(aEvent); resized->oldSize().width() != resized->size().width()) {
				// The column's own layout gives its children the new width
				// only after its filters: laid out first, the body and the
				// page take the width they are measured at.
				if (Column->layout() != nullptr) {
					Column->layout()->activate();
				}
				fit_body();
			}
		}
		// The page's links, by their text: a click on one, or Return on the
		// one the keyboard reached.
		if (Dressed != nullptr && aObject == Dressed->viewport()) {
			if (aEvent->type() == QEvent::MouseMove) {
				const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
				Dressed->viewport()->setCursor(Dressed->anchorAt(mouse->position().toPoint()).isEmpty() ? Qt::IBeamCursor : Qt::PointingHandCursor);
			}
			if (aEvent->type() == QEvent::MouseButtonRelease) {
				const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
				if (const auto link = Dressed->anchorAt(mouse->position().toPoint()); mouse->button() == Qt::LeftButton && !link.isEmpty() && !Dressed->textCursor().hasSelection()) {
					follow(link);
					return true;
				}
			}
		}
		if (Dressed != nullptr && aObject == Dressed && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			// [[BPR-2184]] (review N10). Tab and Shift+Tab walk the page's
			// links, as a browser's did; past the last one the focus moves on.
			if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) && (key->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier) {
				const auto forward = key->key() == Qt::Key_Tab && (key->modifiers() & Qt::ShiftModifier) == 0;
				const auto from    = forward ? Dressed->textCursor().selectionEnd() : Dressed->textCursor().selectionStart();
				auto       found   = std::optional<std::pair<int, int>>{ };
				for (auto block = Dressed->document()->begin(); block.isValid(); block = block.next()) {
					for (auto it = block.begin(); !it.atEnd(); ++it) {
						const auto fragment = it.fragment();
						if (fragment.charFormat().anchorHref().isEmpty()) {
							continue;
						}
						if (forward && fragment.position() >= from && (!found || fragment.position() < found->first)) {
							found = std::pair{ fragment.position(), fragment.position() + fragment.length() };
						}
						if (!forward && fragment.position() + fragment.length() <= from && (!found || fragment.position() > found->first)) {
							found = std::pair{ fragment.position(), fragment.position() + fragment.length() };
						}
					}
				}
				if (found) {
					auto cursor = Dressed->textCursor();
					cursor.setPosition(found->first);
					cursor.setPosition(found->second, QTextCursor::KeepAnchor);
					Dressed->setTextCursor(cursor);
					Dressed->ensureCursorVisible();
					return true;
				}
			}
			auto at = Dressed->textCursor();
			if (at.hasSelection()) {
				at.setPosition(at.selectionStart() + 1);
			}
			if (const auto link = at.charFormat().anchorHref(); (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !link.isEmpty()) {
				follow(link);
				return true;
			}
		}
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
