#include "bpr/qt/prompts_page.hpp"
#include "bpr/qt/disclosure_button.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/prompt_words.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#include "conversation_glyphs.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <span>
#include <utility>

namespace bpr::qt {
	namespace {
		/// [[BPR-2219]]. A prompt's text, coloured as it is typed: its
		/// variables (`{file}`), a line's leading label ("Focus:"), headings,
		/// list markers, code spans and fences, and strong text. Colours are
		/// the palette's own roles, read at each block, so a theme change
		/// shows on the next rehighlight.
		class prompt_text_highlighter final : public QSyntaxHighlighter
		{
		public:
			explicit prompt_text_highlighter(QTextDocument* aDocument)
				: QSyntaxHighlighter{ aDocument }
			{ }

		protected:
			void highlightBlock(const QString& aText) override
			{
				const auto palette = QGuiApplication::palette();
				auto       quiet   = QTextCharFormat{ };
				quiet.setForeground(palette.brush(QPalette::PlaceholderText));
				auto strong = QTextCharFormat{ };
				strong.setFontWeight(QFont::DemiBold);
				auto variable = QTextCharFormat{ };
				variable.setForeground(palette.brush(QPalette::Link));
				variable.setFontWeight(QFont::DemiBold);
				auto code = QTextCharFormat{ };
				code.setBackground(palette.brush(QPalette::AlternateBase));
				// Inside a fenced block every line is code; the fence is quiet.
				const auto fence  = aText.trimmed().startsWith(QStringLiteral("```")) || aText.trimmed().startsWith(QStringLiteral("~~~"));
				const auto inside = previousBlockState() == 1;
				setCurrentBlockState(fence ? (inside ? 0 : 1) : (inside ? 1 : 0));
				if (fence) {
					setFormat(0, static_cast<int>(aText.size()), quiet);
					return;
				}
				if (inside) {
					setFormat(0, static_cast<int>(aText.size()), code);
					return;
				}
				static const auto heading   = QRegularExpression{ QStringLiteral(R"(^\s{0,3}#{1,6}\s.*$)") };
				static const auto label     = QRegularExpression{ QStringLiteral(R"(^(?:\s*[-*]\s+)?([A-Z][^:.{}`]{0,38}:)(?=\s|$))") };
				static const auto marker    = QRegularExpression{ QStringLiteral(R"(^\s*([-*]|\d+\.)\s)") };
				static const auto spans     = QRegularExpression{ QStringLiteral(R"(`[^`]+`)") };
				static const auto bold      = QRegularExpression{ QStringLiteral(R"(\*\*[^*]+\*\*)") };
				static const auto variables = QRegularExpression{ QStringLiteral(R"(\{[a-z][a-z0-9_.]*\})") };
				if (const auto match = heading.match(aText); match.hasMatch()) {
					setFormat(0, static_cast<int>(aText.size()), strong);
				}
				if (const auto match = label.match(aText); match.hasMatch()) {
					setFormat(static_cast<int>(match.capturedStart(1)), static_cast<int>(match.capturedLength(1)), strong);
				}
				if (const auto match = marker.match(aText); match.hasMatch()) {
					setFormat(static_cast<int>(match.capturedStart(1)), static_cast<int>(match.capturedLength(1)), quiet);
				}
				for (const auto* const each : { &bold }) {
					for (auto found = each->globalMatch(aText); found.hasNext();) {
						const auto match = found.next();
						setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), strong);
					}
				}
				for (auto found = spans.globalMatch(aText); found.hasNext();) {
					const auto match = found.next();
					setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), code);
				}
				for (auto found = variables.globalMatch(aText); found.hasNext();) {
					const auto match = found.next();
					setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), variable);
				}
			}
		};

		using core::prompt_surface;

		constexpr auto mime_type   = "application/x-bpr-prompt-placement";
		constexpr int  id_role     = Qt::UserRole + 1;
		constexpr int  system_role = Qt::UserRole + 2;
		constexpr int  header_role = Qt::UserRole + 3;
		constexpr int  tags_role   = Qt::UserRole + 4;
		constexpr int  flags_role  = Qt::UserRole + 5;
		/// A placement's flags, for the delegate.
		constexpr int has_conditions = 1;
		constexpr int is_off         = 2;
		constexpr int is_chip        = 4;
		constexpr int is_outlined    = 8;
		constexpr int is_filled      = 16;
		/// [[BPR-2179]] spec section 11: removed in this repository (struck
		/// through and dimmed), and a tag with a revert button.
		constexpr int is_removed  = 32;
		constexpr int revertable  = 64;
		constexpr int is_override = 128;

		constexpr int row_height  = 22;
		constexpr int row_spacing = 6;

		constexpr auto scope_surfaces = std::array{
			std::array{ prompt_surface::pull_request_menu, prompt_surface::pull_request_chips },
			std::array{ prompt_surface::file_menu, prompt_surface::file_chips },
			std::array{ prompt_surface::thread_menu, prompt_surface::thread_chips },
		};
		constexpr auto tour_surfaces = std::array{ prompt_surface::tour_stop, prompt_surface::tour_part, prompt_surface::whole_tour };

		[[nodiscard]] auto index(const prompt_surface aSurface) -> std::size_t { return static_cast<std::size_t>(aSurface); }

		// The Surfaces section's words, which Add to repeats so its groups say
		// the surface the section shows them under.

		/// \brief The panel a surface sits in: "Pull request", "Tour stop".
		[[nodiscard]] auto surface_panel_name(const prompt_surface aSurface) -> QString
		{
			using enum prompt_surface;
			switch (aSurface) {
			case pull_request_menu:
			case pull_request_chips: return prompts_page::tr("Pull request");
			case file_menu:
			case file_chips:         return prompts_page::tr("File");
			case thread_menu:
			case thread_chips:       return prompts_page::tr("Thread");
			case tour_stop:          return prompts_page::tr("Tour stop");
			case tour_part:          return prompts_page::tr("Tour part");
			case whole_tour:         return prompts_page::tr("Whole tour");
			case commits_menu:       return prompts_page::tr("Commits");
			}
			return { };
		}

		/// \brief The list a surface is within its panel: "Ask menu" or
		/// "Suggested chips".
		[[nodiscard]] auto surface_list_name(const prompt_surface aSurface) -> QString
		{
			using enum prompt_surface;
			if (aSurface == commits_menu) {
				return prompts_page::tr("Selection menu");
			}
			const auto isMenu = aSurface == pull_request_menu || aSurface == file_menu || aSurface == thread_menu;
			return isMenu ? prompts_page::tr("Ask menu") : prompts_page::tr("Suggested chips");
		}

		/// \brief The tour panels together, as Add to groups them.
		[[nodiscard]] auto tour_group_name() -> QString { return prompts_page::tr("Tour"); }

		[[nodiscard]] auto mime_of(const std::string& aId, const std::optional<prompt_surface> aFrom) -> QMimeData*
		{
			auto* const payload = new QMimeData;
			payload->setData(QString::fromLatin1(mime_type), (QString::fromStdString(aId) + QLatin1Char('\n') + QString::number(aFrom ? static_cast<int>(*aFrom) : -1)).toUtf8());
			return payload;
		}

		[[nodiscard]] auto read_mime(const QMimeData* const aData) -> std::optional<std::pair<std::string, std::optional<prompt_surface>>>
		{
			if (aData == nullptr || !aData->hasFormat(QString::fromLatin1(mime_type))) {
				return std::nullopt;
			}
			const auto parts = QString::fromUtf8(aData->data(QString::fromLatin1(mime_type))).split(QLatin1Char('\n'));
			if (parts.size() != 2) {
				return std::nullopt;
			}
			const auto from = parts[1].toInt();
			return std::pair{ parts[0].toStdString(), from >= 0 && from < static_cast<int>(core::prompt_surface_count) ? std::optional{ static_cast<prompt_surface>(from) } : std::nullopt };
		}

		/// The new colour: "differs from app-wide", in the spec's teal.
		[[nodiscard]] auto override_colour(const QPalette& aPalette) -> QColor
		{
			return aPalette.color(QPalette::Window).lightness() < 128 ? QColor{ 0x4f, 0xd0, 0xc9 } : QColor{ 0x0b, 0x76, 0x72 };
		}

		/// A placement row: grip, the Ask sparkle and name for a menu, the
		/// name in a capsule for a chip; the condition diamond and tags at
		/// the right; outlined, with a link glyph, while its prompt is
		/// selected elsewhere.
		class placement_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				static_cast<void>(aIndex);
				return { aOption.rect.width(), row_height + row_spacing };
			}

			void paint(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				const auto flags   = aIndex.data(flags_role).toInt();
				const auto palette = aOption.palette;
				const auto accent  = palette.color(QPalette::Highlight);
				auto       box     = QRectF{ aOption.rect }.adjusted(0.5, row_spacing / 2.0, -0.5, -row_spacing / 2.0);
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing);
				if ((flags & is_filled) != 0) {
					auto fill = accent;
					fill.setAlphaF(0.22F);
					aPainter->setBrush(fill);
					aPainter->setPen(QPen{ accent, 1.5 });
				} else if ((flags & is_outlined) != 0) {
					aPainter->setBrush(Qt::NoBrush);
					aPainter->setPen(QPen{ accent, 1.5 });
				} else {
					aPainter->setBrush(palette.color(QPalette::Base));
					aPainter->setPen(Qt::NoPen);
				}
				aPainter->drawRoundedRect(box, 6, 6);
				if ((aOption.state & QStyle::State_HasFocus) != 0) {
					aPainter->setBrush(Qt::NoBrush);
					aPainter->setPen(QPen{ accent, 1, Qt::DotLine });
					aPainter->drawRoundedRect(box.adjusted(1.5, 1.5, -1.5, -1.5), 5, 5);
				}
				auto words = palette.color(QPalette::Text);
				if ((flags & (is_off | is_removed)) != 0) {
					words = palette.color(QPalette::PlaceholderText);
				}
				// The grip: two columns of three dots.
				aPainter->setPen(Qt::NoPen);
				aPainter->setBrush(palette.color(QPalette::PlaceholderText));
				for (int column = 0; column < 2; ++column) {
					for (int dot = 0; dot < 3; ++dot) {
						aPainter->drawEllipse(QPointF{ box.left() + 6 + column * 3.5, box.center().y() - 4 + dot * 4 }, 1.0, 1.0);
					}
				}
				auto       left   = box.left() + 16;
				const auto metric = aOption.fontMetrics;
				// The right end: tags, then the diamond and the link glyph.
				auto right = box.right() - 6;
				if ((flags & is_outlined) != 0 || (flags & is_filled) != 0) {
					aPainter->setPen(QPen{ accent, 1.4 });
					aPainter->setBrush(Qt::NoBrush);
					aPainter->drawRoundedRect(QRectF{ right - 11, box.center().y() - 3, 6, 5 }, 2, 2);
					aPainter->drawRoundedRect(QRectF{ right - 6, box.center().y() - 2, 6, 5 }, 2, 2);
					right -= 16;
				}
				const auto teal = override_colour(palette);
				if ((flags & revertable) != 0) {
					// The revert button: a counter-clockwise arrow.
					const auto button = QRectF{ right - 20, box.center().y() - 8, 20, 16 };
					auto       tint   = teal;
					tint.setAlphaF(0.13F);
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(tint);
					aPainter->drawRoundedRect(button, 4, 4);
					aPainter->setPen(QPen{ teal, 1.3 });
					aPainter->setBrush(Qt::NoBrush);
					aPainter->drawArc(button.adjusted(6, 4, -6, -4), 30 * 16, 280 * 16);
					right -= 24;
				}
				const auto tags = aIndex.data(tags_role).toString();
				if (!tags.isEmpty()) {
					auto small = aOption.font;
					small.setPointSizeF(small.pointSizeF() * 0.82);
					const auto width = QFontMetrics{ small }.horizontalAdvance(tags) + 10;
					auto       tag   = QRectF{ right - width, box.center().y() - 8, static_cast<qreal>(width), 16 };
					aPainter->setPen(Qt::NoPen);
					const auto here = (flags & (revertable | is_override)) != 0;
					auto       tint = here ? teal : palette.color(QPalette::PlaceholderText);
					tint.setAlphaF(here ? 0.13F : 0.18F);
					aPainter->setBrush(tint);
					aPainter->drawRoundedRect(tag, 4, 4);
					aPainter->setFont(small);
					aPainter->setPen(here ? teal : palette.color(QPalette::Text));
					aPainter->drawText(tag, Qt::AlignCenter, tags);
					aPainter->setFont(aOption.font);
					right -= width + 6;
				}
				if ((flags & has_conditions) != 0) {
					const auto centre = QPointF{ right - 5, box.center().y() };
					auto       path   = QPainterPath{ };
					path.moveTo(centre + QPointF{ 0, -4.5 });
					path.lineTo(centre + QPointF{ 4.5, 0 });
					path.lineTo(centre + QPointF{ 0, 4.5 });
					path.lineTo(centre + QPointF{ -4.5, 0 });
					path.closeSubpath();
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(agent_accent(palette));
					aPainter->drawPath(path);
					right -= 14;
				}
				const auto name = aIndex.data(Qt::DisplayRole).toString();
				if ((flags & is_chip) != 0) {
					auto small = aOption.font;
					small.setPointSizeF(small.pointSizeF() * 0.92);
					const auto measure = QFontMetrics{ small };
					const auto room    = static_cast<int>(right - left - 16);
					const auto shown   = measure.elidedText(name, Qt::ElideRight, std::max(room, 20));
					const auto capsule = QRectF{ left, box.center().y() - 9.5, static_cast<qreal>(measure.horizontalAdvance(shown) + 16), 19 };
					aPainter->setPen(QPen{ palette.color(QPalette::Mid), 1 });
					aPainter->setBrush(palette.color(QPalette::Window));
					aPainter->drawRoundedRect(capsule, 9.5, 9.5);
					small.setStrikeOut((flags & is_removed) != 0);
					aPainter->setFont(small);
					aPainter->setPen(words);
					aPainter->drawText(capsule, Qt::AlignCenter, shown);
				} else {
					// The Ask sparkle: a four-pointed star in the agent accent.
					const auto centre = QPointF{ left + 6, box.center().y() };
					auto       star   = QPainterPath{ };
					star.moveTo(centre + QPointF{ 0, -6 });
					star.quadTo(centre, centre + QPointF{ 6, 0 });
					star.quadTo(centre, centre + QPointF{ 0, 6 });
					star.quadTo(centre, centre + QPointF{ -6, 0 });
					star.quadTo(centre, centre + QPointF{ 0, -6 });
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush((flags & is_off) != 0 ? palette.color(QPalette::PlaceholderText) : agent_accent(palette));
					aPainter->drawPath(star);
					left += 18;
					aPainter->setPen(words);
					auto font = aOption.font;
					font.setStrikeOut((flags & is_removed) != 0);
					aPainter->setFont(font);
					aPainter->drawText(QRectF{ left, box.top(), right - left - 4, box.height() }, Qt::AlignVCenter | Qt::AlignLeft, metric.elidedText(name, Qt::ElideRight, static_cast<int>(right - left - 4)));
				}
				if ((flags & is_override) != 0) {
					// Differs from app-wide: a teal dot by the name.
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(teal);
					aPainter->drawEllipse(QPointF{ box.left() + 13, box.top() + 5 }, 2.5, 2.5);
				}
				aPainter->restore();
			}
		};

		/// One surface: its placements in order, a drop target for the
		/// library's rows and the other surfaces' placements.
		class surface_list_view final : public QListWidget
		{
		public:
			surface_list_view(prompts_page& aPage, const prompt_surface aSurface, QWidget* const aParent)
				: QListWidget{ aParent }
				, Page{ aPage }
				, Surface{ aSurface }
			{
				setObjectName(QStringLiteral("promptSurface-%1").arg(QString::fromLatin1(core::stable_name(aSurface))));
				setAccessibleName(prompt_surface_name(aSurface));
				setItemDelegate(new placement_delegate{ this });
				setFrameShape(QFrame::NoFrame);
				setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
				setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
				setSelectionMode(QAbstractItemView::SingleSelection);
				setDragEnabled(true);
				setAcceptDrops(true);
				setDropIndicatorShown(true);
				setDragDropMode(QAbstractItemView::DragDrop);
				setDefaultDropAction(Qt::MoveAction);
				setContextMenuPolicy(Qt::CustomContextMenu);
				viewport()->setAutoFillBackground(false);
				setAttribute(Qt::WA_TranslucentBackground);
			}

			[[nodiscard]] auto surface() const noexcept -> prompt_surface { return Surface; }

			void fit()
			{
				setFixedHeight(std::max(count(), 1) * (row_height + row_spacing) + 4);
			}

		protected:
			[[nodiscard]] auto mimeTypes() const -> QStringList override { return { QString::fromLatin1(mime_type) }; }

			[[nodiscard]] auto mimeData(const QList<QListWidgetItem*>& aItems) const -> QMimeData* override
			{
				if (aItems.isEmpty()) {
					return nullptr;
				}
				return mime_of(aItems.front()->data(id_role).toString().toStdString(), Surface);
			}

			[[nodiscard]] auto supportedDropActions() const -> Qt::DropActions override { return Qt::MoveAction | Qt::CopyAction; }

			void dragEnterEvent(QDragEnterEvent* const aEvent) override
			{
				if (!accepts(aEvent->mimeData())) {
					aEvent->ignore();
					return;
				}
				QListWidget::dragEnterEvent(aEvent);
				aEvent->acceptProposedAction();
			}

			void dragMoveEvent(QDragMoveEvent* const aEvent) override
			{
				if (!accepts(aEvent->mimeData())) {
					aEvent->ignore();
					return;
				}
				QListWidget::dragMoveEvent(aEvent);
				aEvent->accept();
			}

			void dropEvent(QDropEvent* const aEvent) override
			{
				const auto dropped = read_mime(aEvent->mimeData());
				if (!dropped) {
					aEvent->ignore();
					return;
				}
				auto       at    = static_cast<std::size_t>(count());
				const auto under = indexAt(aEvent->position().toPoint());
				if (under.isValid()) {
					at = static_cast<std::size_t>(under.row());
					if (dropIndicatorPosition() == QAbstractItemView::BelowItem) {
						++at;
					}
				}
				// The source keeps its row: the page rebuilds every list.
				aEvent->setDropAction(Qt::CopyAction);
				aEvent->accept();
				const auto copy = (aEvent->modifiers() & Qt::AltModifier) != 0;
				QTimer::singleShot(0, &Page, [page = &Page, id = dropped->first, from = dropped->second, to = Surface, at, copy] { static_cast<void>(page->drop(id, from, to, at, copy)); });
			}

			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				const auto* const item = itemAt(aEvent->position().toPoint());
				if (item != nullptr && (item->data(flags_role).toInt() & revertable) != 0 && aEvent->position().x() > viewport()->width() - 30) {
					QTimer::singleShot(0, &Page, [page = &Page, id = item->data(id_role).toString().toStdString(), surface = Surface] { page->revert_placement(id, surface); });
					return;
				}
				QListWidget::mousePressEvent(aEvent);
			}

			void paintEvent(QPaintEvent* const aEvent) override
			{
				QListWidget::paintEvent(aEvent);
				if (count() == 0) {
					auto painter = QPainter{ viewport() };
					painter.setPen(palette().color(QPalette::PlaceholderText));
					painter.drawText(viewport()->rect().adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, prompts_page::tr("Drag a prompt here"));
				}
			}

		private:
			[[nodiscard]] auto accepts(const QMimeData* const aData) const -> bool
			{
				const auto dropped = read_mime(aData);
				return dropped && Page.refusal(dropped->first, Surface) == core::placement_refusal::none;
			}

			prompts_page&  Page;
			prompt_surface Surface;
		};

		/// The library's list: a placeable row drags out as a placement.
		class library_list_view final : public QListWidget
		{
		public:
			using QListWidget::QListWidget;

		protected:
			[[nodiscard]] auto mimeTypes() const -> QStringList override { return { QString::fromLatin1(mime_type) }; }

			[[nodiscard]] auto mimeData(const QList<QListWidgetItem*>& aItems) const -> QMimeData* override
			{
				if (aItems.isEmpty() || aItems.front()->data(system_role).isValid() || aItems.front()->data(header_role).toBool()) {
					return nullptr;
				}
				return mime_of(aItems.front()->data(id_role).toString().toStdString(), std::nullopt);
			}

			[[nodiscard]] auto supportedDropActions() const -> Qt::DropActions override { return Qt::CopyAction; }
		};

		/// A choice among a few, drawn as the Overview's Open and All
		/// (owner, look prompts-v003: "Use this styling from overview"): the
		/// chosen one a quiet chip, the others their words alone.
		[[nodiscard]] auto segmented(const QString& aText, QWidget* const aParent) -> QToolButton*
		{
			auto* const button = new counts_mode_button{ aParent };
			button->setText(aText);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			return button;
		}

		[[nodiscard]] auto heading(const QString& aText, QWidget* const aParent, const qreal aScale = 1.0) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			font.setPointSizeF(font.pointSizeF() * aScale);
			label->setFont(font);
			return label;
		}

		[[nodiscard]] auto secondary(const QString& aText, QWidget* const aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setForegroundRole(QPalette::PlaceholderText);
			label->setWordWrap(true);
			return label;
		}

		/// The system prompt an action row sends (the coordinator's default:
		/// what an action row sends is a system prompt).
		[[nodiscard]] auto action_system_key(const core::prompt_action aAction) -> std::optional<int>
		{
			switch (aAction) {
			case core::prompt_action::what_changed:  return static_cast<int>(core::writing_hook::review_latest_upload);
			case core::prompt_action::was_addressed: return static_cast<int>(core::writing_hook::review_proposed_fix);
			case core::prompt_action::explain_file:  return static_cast<int>(core::writing_hook::review_explain_file);
			case core::prompt_action::none:          return std::nullopt;
			}
			return std::nullopt;
		}

		/// Takes a rebuilt row out of its layout; its widget goes once the
		/// signal that rebuilt it returns.
		void retire(QLayoutItem* const aItem)
		{
			if (auto* const widget = aItem->widget(); widget != nullptr) {
				widget->hide();
				widget->deleteLater();
			}
			delete aItem;
		}

		[[nodiscard]] auto is_tour(const core::prompt_macro& aMacro) -> bool
		{
			return aMacro.Place == core::prompt_place::tour_stop || aMacro.Place == core::prompt_place::tour_part || aMacro.Place == core::prompt_place::tour_whole;
		}
	} // namespace

	prompts_page::prompts_page(QWidget* const aParent)
		: QWidget{ aParent }
		, Editor{ core::make_prompt_editor(core::shipped_prompt_macros()) }
		, View{ core::prompt_edit_view(Editor) }
	{
		setObjectName(QStringLiteral("promptsPage"));
		setAccessibleName(tr("Prompts"));

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(7);
		// The page fits the window's width: in a narrow one the library's
		// editor goes under its list rather than scroll sideways.
		column->setSizeConstraint(QLayout::SetNoConstraint);

		// The title row: Settings draws "Prompts"; Prompts for is at its right.
		auto* const titleRow = new QHBoxLayout{ };
		titleRow->addStretch(1);
		// A repository chosen: a teal word says the edits are its own.
		RepositoryMark = new QLabel{ tr("\u25CF Changes apply to this repository only"), this };
		RepositoryMark->setObjectName(QStringLiteral("promptsRepositoryMark"));
		RepositoryMark->hide();
		titleRow->addWidget(RepositoryMark);
		titleRow->addWidget(new QLabel{ tr("Prompts for"), this });
		PromptsFor = new QComboBox{ this };
		PromptsFor->setObjectName(QStringLiteral("promptsFor"));
		PromptsFor->setAccessibleName(tr("Prompts for"));
		PromptsFor->addItem(tr("All repositories"));
		titleRow->addWidget(PromptsFor);
		connect(PromptsFor, &QComboBox::activated, this, [this](const int aIndex) {
			choose_repository(aIndex <= 0 ? std::string{ } : PromptsFor->itemText(aIndex).toStdString());
		});
		column->addLayout(titleRow);

		build_surfaces(*column);
		build_library(*column);

		Typing = new QTimer{ this };
		Typing->setSingleShot(true);
		Typing->setInterval(450);
		connect(Typing, &QTimer::timeout, this, [this] {
			core::end_prompt_typing(Editor);
			flush();
		});

		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
	}

	prompts_page::~prompts_page() = default;

	auto prompts_page::minimumSizeHint() const -> QSize { return { 480, QWidget::minimumSizeHint().height() }; }

	void prompts_page::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		const auto narrow = width() < 880;
		arrange_library_tools();
		Body->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
		if (narrow) {
			LibraryList->setMinimumWidth(0);
			LibraryList->setMaximumWidth(QWIDGETSIZE_MAX);
			LibraryList->setMinimumHeight(240);
		} else {
			LibraryList->setFixedWidth(272);
			LibraryList->setMinimumHeight(360);
		}
		// As drawn the body is 360 high; stacked, the list takes 240 and the
		// editor its own 360.
		LibraryList->setMaximumHeight(narrow ? 240 : QWIDGETSIZE_MAX);
		const auto editor = narrow ? width() : width() - 284;
		EditorColumns->setDirection(editor < 640 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
		RightBox->setMinimumWidth(editor < 640 ? 240 : 300);
	}

	void prompts_page::arrange_library_tools()
	{
		// What each row needs at its controls' own sizes, the filter at a
		// comfortable width; it takes whatever is left over.
		constexpr auto gap      = 8;
		constexpr auto filter   = 160;
		const auto     buttons  = Restore->sizeHint().width() + gap + NewPrompt->sizeHint().width();
		const auto     choices  = Segments->sizeHint().width();
		const auto     line     = filter + gap + choices + gap + buttons;
		const auto     oneRow   = LibraryTitle->sizeHint().width() + gap + line <= width();
		const auto     together = line <= width();
		ToolsRow->setDirection(oneRow ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
		const auto onFilterLine = FilterLine->indexOf(Segments) >= 0;
		if (together && !onFilterLine) {
			SegmentsLine->removeWidget(Segments);
			FilterLine->insertWidget(1, Segments, 0);
		} else if (!together && onFilterLine) {
			FilterLine->removeWidget(Segments);
			SegmentsLine->addWidget(Segments, 0, Qt::AlignLeft);
		}
	}

	void prompts_page::build_surfaces(QVBoxLayout& aColumn)
	{
		auto* const headingRow = new QHBoxLayout{ };
		headingRow->setSpacing(10);
		headingRow->addWidget(heading(tr("Surfaces"), this));
		Caption = secondary(tr("Where prompts appear, in order. Drag a prompt up from the library, or right-click it and choose Add to."), this);
		Caption->setObjectName(QStringLiteral("promptsSurfacesCaption"));
		headingRow->addWidget(Caption, 1);
		aColumn.addLayout(headingRow);

		const auto addList = [this](QVBoxLayout& aPanel, const prompt_surface aSurface, const QString& aTitle, const QString& aNote) {
			auto* const titleRow = new QHBoxLayout{ };
			auto* const title    = heading(aTitle, this, 0.88);
			title->setForegroundRole(QPalette::PlaceholderText);
			titleRow->addWidget(title);
			if (!aNote.isEmpty()) {
				auto* const note = secondary(aNote, this);
				note->setWordWrap(false);
				titleRow->addWidget(note);
			}
			titleRow->addStretch(1);
			// In a repository that reordered this surface.
			auto* const order = new QLabel{ this };
			order->setObjectName(QStringLiteral("promptsOrderHere-%1").arg(QString::fromLatin1(core::stable_name(aSurface))));
			order->setTextFormat(Qt::RichText);
			order->hide();
			connect(order, &QLabel::linkActivated, this, [this, aSurface] { revert_order(aSurface); });
			OrderTags.at(index(aSurface)) = order;
			titleRow->addWidget(order);
			aPanel.addLayout(titleRow);
			auto* const list          = new surface_list_view{ *this, aSurface, this };
			Lists.at(index(aSurface)) = list;
			list->installEventFilter(this);
			connect(list, &QListWidget::currentRowChanged, this, [this, list](const int aRow) {
				if (Filling || aRow < 0) {
					return;
				}
				// Rebuilt once the list's own signal returns.
				const auto id = list->item(aRow)->data(id_role).toString().toStdString();
				QTimer::singleShot(0, this, [this, id, surface = list->surface(), aRow] {
					if (find(id) == nullptr) {
						return;
					}
					SelectedPlacement = std::pair{ surface, static_cast<std::size_t>(aRow) };
					Selected          = id;
					SelectedSystem.reset();
					rebuild_surfaces();
					rebuild_library();
					refresh_editor();
				});
			});
			connect(list, &QListWidget::customContextMenuRequested, this, [this, list](const QPoint& aAt) {
				const auto* const item = list->itemAt(aAt);
				if (item == nullptr) {
					return;
				}
				const auto  id   = item->data(id_role).toString().toStdString();
				auto        menu = QMenu{ this };
				auto* const sub  = menu.addMenu(tr("Add to"));
				fill_add_to(*sub, id);
				menu.addAction(tr("Remove from %1").arg(prompt_surface_name(list->surface())), this, [this, id, surface = list->surface()] { remove(id, surface); });
				menu.addSeparator();
				menu.addAction(tr("Edit"), this, [this, id] {
					select(id);
					Text->setFocus(Qt::OtherFocusReason);
				});
				menu.exec(list->viewport()->mapToGlobal(aAt));
			});
			aPanel.addWidget(list);
		};

		const auto panel = [this](const QString& aName) {
			auto* const frame = new QFrame{ this };
			frame->setObjectName(QStringLiteral("promptsScopePanel"));
			frame->setFrameShape(QFrame::StyledPanel);
			frame->setAutoFillBackground(true);
			frame->setBackgroundRole(QPalette::AlternateBase);
			auto* const layout = new QVBoxLayout{ frame };
			layout->setContentsMargins(8, 8, 8, 8);
			layout->setSpacing(4);
			layout->addWidget(heading(aName, frame));
			return std::pair{ frame, layout };
		};

		auto* const scopes = new QHBoxLayout{ };
		scopes->setSpacing(12);
		for (const auto& scope : scope_surfaces) {
			auto [frame, layout] = panel(surface_panel_name(scope[0]));
			addList(*layout, scope[0], surface_list_name(scope[0]), { });
			layout->addSpacing(4);
			addList(*layout, scope[1], surface_list_name(scope[1]), tr("up to 5 show"));
			layout->addStretch(1);
			scopes->addWidget(frame, 1, Qt::AlignTop);
		}
		aColumn.addLayout(scopes);

		// The owner, 2026-11-14: the tour chats get surfaces under the same model.
		auto* const tours = new QHBoxLayout{ };
		tours->setSpacing(12);
		for (const auto tour : tour_surfaces) {
			auto [frame, layout] = panel(surface_panel_name(tour));
			addList(*layout, tour, surface_list_name(tour), tr("up to 5 show"));
			layout->addStretch(1);
			tours->addWidget(frame, 1, Qt::AlignTop);
		}
		aColumn.addLayout(tours);

		// [[BPR-2248]]. The Commits tab's menu over the selected commits.
		auto* const commits = new QHBoxLayout{ };
		commits->setSpacing(12);
		{
			auto [frame, layout] = panel(surface_panel_name(prompt_surface::commits_menu));
			addList(*layout, prompt_surface::commits_menu, surface_list_name(prompt_surface::commits_menu), tr("asks about the selected commits"));
			layout->addStretch(1);
			commits->addWidget(frame, 1, Qt::AlignTop);
			// As wide as one panel of the row above.
			commits->addStretch(2);
		}
		aColumn.addLayout(commits);

		auto* const legend = secondary(tr("◆ Has conditions. Its chip shows only while they hold; its menu row is disabled or hidden while they do not."), this);
		legend->setObjectName(QStringLiteral("promptsLegend"));
		aColumn.addWidget(legend);
	}

	void prompts_page::build_library(QVBoxLayout& aColumn)
	{
		aColumn.addSpacing(13);
		// [[BPR-2220]] (owner, 2026-11-16): New prompt and Restore built-ins
		// sit on the filter's row, at its right; the filter takes the room
		// left. As drawn the title leads that row; narrower, the title goes
		// above it, then the filter choices under it
		// (`arrange_library_tools`).
		ToolsRow = new QBoxLayout{ QBoxLayout::LeftToRight };
		ToolsRow->setSpacing(8);
		LibraryTitle = heading(tr("Prompt library"), this);
		ToolsRow->addWidget(LibraryTitle, 0);
		FilterLine = new QBoxLayout{ QBoxLayout::LeftToRight };
		FilterLine->setSpacing(8);
		Search = new QLineEdit{ this };
		Search->setObjectName(QStringLiteral("promptsFilter"));
		Search->setPlaceholderText(tr("Filter prompts"));
		Search->setAccessibleName(tr("Filter prompts"));
		Search->setClearButtonEnabled(true);
		Search->setMinimumWidth(120);
		FilterLine->addWidget(Search, 1);
		Filters  = new QButtonGroup{ this };
		Segments = new QWidget{ this };
		Segments->setObjectName(QStringLiteral("promptsFilterChoices"));
		Filters->setExclusive(true);
		auto* const segments = new QHBoxLayout{ Segments };
		segments->setContentsMargins(0, 0, 0, 0);
		segments->setSpacing(0);
		const auto names = std::array{ tr("All"), tr("Built-in"), tr("Yours"), tr("Not on a surface"), tr("System") };
		for (std::size_t segment = 0U; segment < names.size(); ++segment) {
			auto* const button = segmented(names.at(segment), Segments);
			button->setObjectName(QStringLiteral("promptsFilter-%1").arg(segment));
			Filters->addButton(button, static_cast<int>(segment));
			segments->addWidget(button);
		}
		Filters->button(0)->setChecked(true);
		segments->addSpacing(8);
		ChangedHere = new QCheckBox{ Segments };
		ChangedHere->setObjectName(QStringLiteral("promptsChangedHere"));
		ChangedHere->hide();
		connect(ChangedHere, &QCheckBox::toggled, this, [this](const bool aOn) { set_changed_here(aOn); });
		segments->addWidget(ChangedHere);
		FilterLine->addWidget(Segments, 0);
		Restore = new quiet_push_button{ tr("Restore built-ins\u2026"), this };
		Restore->setObjectName(QStringLiteral("promptsRestore"));
		connect(Restore, &QPushButton::clicked, this, [this] { static_cast<void>(restore_built_ins(true)); });
		NewPrompt = new QPushButton{ tr("New prompt"), this };
		NewPrompt->setObjectName(QStringLiteral("promptsNew"));
		FilterLine->addWidget(Restore, 0);
		FilterLine->addWidget(NewPrompt, 0);
		ToolsRow->addLayout(FilterLine, 1);
		aColumn.addLayout(ToolsRow);
		// The filter choices when they don't fit on the filter's row.
		SegmentsLine = new QHBoxLayout{ };
		SegmentsLine->setSpacing(0);
		aColumn.addLayout(SegmentsLine);
		aColumn.addSpacing(9 - 7);
		connect(Search, &QLineEdit::textChanged, this, [this] { refilter(); });
		connect(Filters, &QButtonGroup::idClicked, this, [this](const int aId) { set_filter(static_cast<prompt_library_filter>(aId)); });
		connect(NewPrompt, &QPushButton::clicked, this, [this] { static_cast<void>(new_prompt()); });

		// The body keeps its full height in every window; the page scrolls
		// (owner, look v004).
		auto* const body = new QBoxLayout{ QBoxLayout::LeftToRight };
		Body             = body;
		body->setSpacing(12);
		LibraryList = new library_list_view{ this };
		LibraryList->setObjectName(QStringLiteral("promptsLibrary"));
		LibraryList->setAccessibleName(tr("Prompt library"));
		LibraryList->setFixedWidth(272);
		LibraryList->setMinimumHeight(360);
		LibraryList->setDragEnabled(true);
		LibraryList->setDragDropMode(QAbstractItemView::DragOnly);
		LibraryList->setSelectionMode(QAbstractItemView::SingleSelection);
		LibraryList->setContextMenuPolicy(Qt::CustomContextMenu);
		LibraryList->installEventFilter(this);
		body->addWidget(LibraryList, 0);
		connect(LibraryList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || aItem->data(header_role).toBool()) {
				return;
			}
			SelectedPlacement.reset();
			// Rebuilt once the list's own signal returns.
			if (const auto system = aItem->data(system_role); system.isValid()) {
				QTimer::singleShot(0, this, [this, key = system.toInt()] { select_system(key); });
				return;
			}
			QTimer::singleShot(0, this, [this, id = aItem->data(id_role).toString().toStdString()] { select(id); });
		});
		connect(LibraryList, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || aItem->data(system_role).isValid() || aItem->data(header_role).toBool()) {
				return;
			}
			QTimer::singleShot(0, this, [this, id = aItem->data(id_role).toString().toStdString(), on = aItem->checkState() == Qt::Checked] { set_on(id, on); });
		});
		connect(LibraryList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			const auto* const item = LibraryList->itemAt(aAt);
			if (item == nullptr || item->data(header_role).toBool()) {
				return;
			}
			auto menu = QMenu{ this };
			if (const auto system = item->data(system_role); system.isValid()) {
				menu.addAction(tr("Edit"), this, [this, key = system.toInt()] { select_system(key); });
				auto* const addTo = menu.addAction(tr("Add to"));
				addTo->setEnabled(false);
				addTo->setToolTip(tr("System prompts are sent by the app and can't be added to a surface."));
				menu.setToolTipsVisible(true);
			} else {
				fill_library_menu(menu, item->data(id_role).toString().toStdString());
			}
			menu.exec(LibraryList->viewport()->mapToGlobal(aAt));
		});

		auto* const editor = new QFrame{ this };
		EditorFrame        = editor;
		editor->setObjectName(QStringLiteral("promptsEditor"));
		editor->setFrameShape(QFrame::StyledPanel);
		editor->setAutoFillBackground(true);
		editor->setBackgroundRole(QPalette::AlternateBase);
		editor->setMinimumHeight(360);
		build_editor(*editor);
		body->addWidget(editor, 1);
		aColumn.addLayout(body);
		Status = secondary({ }, this);
		Status->setObjectName(QStringLiteral("promptsStatus"));
		aColumn.addWidget(Status);
		aColumn.addStretch(1);
	}

	void prompts_page::build_editor(QWidget& aEditor)
	{
		auto* const outer = new QVBoxLayout{ &aEditor };
		outer->setContentsMargins(14, 13, 14, 12);
		EditorPages = new QStackedWidget{ &aEditor };
		outer->addWidget(EditorPages);

		auto* const page   = new QWidget{ EditorPages };
		auto* const column = new QVBoxLayout{ page };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(10);

		auto* const header = new QHBoxLayout{ };
		Name               = new QLineEdit{ page };
		Name->setObjectName(QStringLiteral("promptsName"));
		Name->setAccessibleName(tr("Name"));
		Name->setMinimumWidth(220);
		Name->setMaximumWidth(300);
		auto nameFont = Name->font();
		nameFont.setWeight(QFont::DemiBold);
		nameFont.setPointSizeF(nameFont.pointSizeF() * 1.1);
		Name->setFont(nameFont);
		header->addWidget(Name);
		BuiltInTag = secondary(tr("Built-in"), page);
		BuiltInTag->setWordWrap(false);
		header->addWidget(BuiltInTag);
		header->addStretch(1);
		header->addWidget(new QLabel{ tr("Answer as"), page });
		AsQuestion = segmented(tr("A question"), page);
		AsQuestion->setObjectName(QStringLiteral("promptsAsQuestion"));
		AsFindings = segmented(tr("Findings"), page);
		AsFindings->setObjectName(QStringLiteral("promptsAsFindings"));
		AsFindings->setToolTip(tr("A review with findings, about the pull request or the open file."));
		header->addWidget(AsQuestion);
		header->addWidget(AsFindings);
		column->addLayout(header);
		// In a repository: which fields differ from app-wide, each with its
		// way back.
		Differs = new QLabel{ page };
		Differs->setObjectName(QStringLiteral("promptsDiffers"));
		Differs->setTextFormat(Qt::RichText);
		Differs->setWordWrap(true);
		Differs->hide();
		connect(Differs, &QLabel::linkActivated, this, [this](const QString& aField) {
			if (Selected) {
				use_app_wide_field(*Selected, aField);
			}
		});
		column->addWidget(Differs);

		auto* const columns = new QBoxLayout{ QBoxLayout::LeftToRight };
		EditorColumns       = columns;
		columns->setSpacing(18);
		auto* const left = new QVBoxLayout{ };
		left->setSpacing(7);
		ActionNote = secondary({ }, page);
		ActionNote->setObjectName(QStringLiteral("promptsActionNote"));
		ActionNote->setTextFormat(Qt::RichText);
		connect(ActionNote, &QLabel::linkActivated, this, [this](const QString& aKey) { select_system(aKey.toInt()); });
		left->addWidget(ActionNote);
		Text = new QPlainTextEdit{ page };
		Text->setObjectName(QStringLiteral("promptsText"));
		Text->setAccessibleName(tr("Prompt text"));
		Text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		Text->setTabChangesFocus(true);
		// Look review-report-v001 (owner: "needs syntax highlighting"): the
		// body's Markdown and its variables are coloured as they are typed.
		new prompt_text_highlighter{ Text->document() };
		left->addWidget(Text, 1);
		InsertRow = new QWidget{ page };
		InsertRow->setObjectName(QStringLiteral("promptsInsertRow"));
		auto* const insertLayout = new QHBoxLayout{ InsertRow };
		insertLayout->setContentsMargins(0, 0, 0, 0);
		insertLayout->setSpacing(4);
		left->addWidget(InsertRow);
		// [[BPR-2219]]. What a review prompt is always sent with: the report
		// format the application reads the answer by. Look review-report-v001
		// (owner, 2026-11-16: "the result needs to look non-editable ... we
		// shouldn't show the result prompt by default it should require an
		// extra click. And then those controls at the bottom look like
		// they're attached to the result but they're really for modifying
		// the prompt"): under the prompt's own controls, closed behind a
		// disclosure, and drawn as something read, not edited (the pane's
		// ground, no frame of a field, no caret).
		ResultBox = new QWidget{ page };
		ResultBox->setObjectName(QStringLiteral("promptsResultBox"));
		auto* const resultLayout = new QVBoxLayout{ ResultBox };
		resultLayout->setContentsMargins(0, 8, 0, 0);
		resultLayout->setSpacing(4);
		ResultToggle = new disclosure_button{ ResultBox };
		ResultToggle->setObjectName(QStringLiteral("promptsResultToggle"));
		ResultToggle->setCheckable(true);
		ResultToggle->setText(tr("Result: Findings report \u2014 show format"));
		ResultToggle->setToolTip(tr("Always added to this prompt: the report format BetterPullReview reads. It is not edited here."));
		resultLayout->addWidget(ResultToggle);
		ResultBody = new QWidget{ ResultBox };
		ResultBody->setObjectName(QStringLiteral("promptsResultBody"));
		auto* const resultBody = new QVBoxLayout{ ResultBody };
		resultBody->setContentsMargins(0, 0, 0, 0);
		resultBody->setSpacing(4);
		auto* const resultNote = new QLabel{ tr("Read-only. Always added: the report format BetterPullReview reads. It shows each finding at its place and makes Draft comment work, so it is not edited here."), ResultBody };
		resultNote->setObjectName(QStringLiteral("promptsResultNote"));
		resultNote->setWordWrap(true);
		resultNote->setForegroundRole(QPalette::PlaceholderText);
		resultBody->addWidget(resultNote);
		ResultText = new QPlainTextEdit{ ResultBody };
		ResultText->setObjectName(QStringLiteral("promptsResultText"));
		ResultText->setAccessibleName(tr("Result: Findings report, always added, read-only"));
		ResultText->setReadOnly(true);
		ResultText->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ResultText->setFocusPolicy(Qt::ClickFocus);
		ResultText->setFrameShape(QFrame::NoFrame);
		ResultText->viewport()->setBackgroundRole(QPalette::Window);
		ResultText->setForegroundRole(QPalette::PlaceholderText);
		ResultText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		ResultText->setMaximumHeight(170);
		new prompt_text_highlighter{ ResultText->document() };
		resultBody->addWidget(ResultText);
		ResultBody->hide();
		resultLayout->addWidget(ResultBody);
		connect(ResultToggle, &QToolButton::toggled, this, [this](const bool aOpen) {
			ResultBody->setVisible(aOpen);
			ResultToggle->setText(aOpen ? tr("Result: Findings report \u2014 hide format") : tr("Result: Findings report \u2014 show format"));
		});
		ResultBox->hide();
		left->addWidget(ResultBox);
		columns->addLayout(left, 1);

		auto* const right = new QVBoxLayout{ };
		right->setSpacing(4);
		// The heading and its add button on one row, the list's match under
		// them, so the add button is never cut at the column's edge.
		auto* const conditionsHeader = new QHBoxLayout{ };
		conditionsHeader->addWidget(heading(tr("Conditions"), page));
		conditionsHeader->addStretch(1);
		UndoButton = new quiet_tool_button{ page };
		UndoButton->setObjectName(QStringLiteral("promptsUndo"));
		UndoButton->setText(tr("Undo"));
		UndoButton->setToolTip(tr("Undo the last change to this prompt (%1)").arg(QKeySequence{ QKeySequence::Undo }.toString(QKeySequence::NativeText)));
		RedoButton = new quiet_tool_button{ page };
		RedoButton->setObjectName(QStringLiteral("promptsRedo"));
		RedoButton->setText(tr("Redo"));
		RedoButton->setToolTip(tr("Redo (%1)").arg(QKeySequence{ QKeySequence::Redo }.toString(QKeySequence::NativeText)));
		connect(UndoButton, &QToolButton::clicked, this, [this] { static_cast<void>(undo()); });
		connect(RedoButton, &QToolButton::clicked, this, [this] { static_cast<void>(redo()); });
		conditionsHeader->addWidget(UndoButton);
		conditionsHeader->addWidget(RedoButton);
		AddCondition = new QPushButton{ tr("Add"), page };
		AddCondition->setObjectName(QStringLiteral("promptsAddCondition"));
		AddCondition->setAccessibleName(tr("Add a condition or a group"));
		auto* const conditionMenu = new QMenu{ AddCondition };
		AddCondition->setMenu(conditionMenu);
		conditionsHeader->addWidget(AddCondition);
		right->addLayout(conditionsHeader);
		auto* const matchRow = new QHBoxLayout{ };
		matchRow->setSpacing(0);
		MatchAll = segmented(tr("All of these"), page);
		MatchAll->setObjectName(QStringLiteral("promptsMatchAll"));
		MatchAny = segmented(tr("Any of these"), page);
		MatchAny->setObjectName(QStringLiteral("promptsMatchAny"));
		MatchNone = segmented(tr("None of these"), page);
		MatchNone->setObjectName(QStringLiteral("promptsMatchNone"));
		matchRow->addWidget(MatchAll);
		matchRow->addWidget(MatchAny);
		// Look prompts-v002 (owner: "None of these ... doesn't make sense for
		// top level"): None is a group's choice only.
		MatchNone->hide();
		matchRow->addStretch(1);
		right->addLayout(matchRow);
		// Look prompts-v003 (owner: "Disable on a condition still doens't make
		// sense"): the coordinator's proposal, one choice for the prompt.
		WhenUnmetRow          = new QWidget{ page };
		auto* const whenUnmet = new QHBoxLayout{ WhenUnmetRow };
		whenUnmet->setContentsMargins(0, 2, 0, 4);
		whenUnmet->setSpacing(0);
		auto* const whenUnmetSay = secondary(tr("When they aren't met:"), WhenUnmetRow);
		whenUnmetSay->setWordWrap(false);
		whenUnmet->addWidget(whenUnmetSay);
		whenUnmet->addSpacing(6);
		UnmetHide = segmented(tr("Hide it"), WhenUnmetRow);
		UnmetHide->setObjectName(QStringLiteral("promptsUnmetHide"));
		UnmetHide->setToolTip(tr("While its conditions aren't met, its Ask menu row is hidden. Its chip is never offered then."));
		UnmetDisable = segmented(tr("Show it disabled"), WhenUnmetRow);
		UnmetDisable->setObjectName(QStringLiteral("promptsUnmetDisable"));
		UnmetDisable->setToolTip(tr("While its conditions aren't met, its Ask menu row is shown disabled, saying why: the first unmet condition. Its chip is never offered then."));
		whenUnmet->addWidget(UnmetHide);
		whenUnmet->addWidget(UnmetDisable);
		whenUnmet->addStretch(1);
		right->addWidget(WhenUnmetRow);
		for (auto* const button : { UnmetHide, UnmetDisable }) {
			connect(button, &QToolButton::clicked, this, [this, button] {
				const auto when = button == UnmetHide ? core::prompt_when_unmet::hide : core::prompt_when_unmet::disable;
				edit_selected([when](core::prompt_macro& aMacro) { aMacro.Body.WhenUnmet = when; });
			});
		}
		// Look prompts-v002 (owner: "we're narrower than we need ... the
		// scroll bar makes it harder to read"): no scroll of their own; the
		// editor grows with the conditions and the page scrolls.
		auto* const conditionsList = new QWidget{ page };
		conditionsList->setObjectName(QStringLiteral("promptsConditionsList"));
		Conditions = new QVBoxLayout{ conditionsList };
		Conditions->setContentsMargins(0, 0, 0, 0);
		Conditions->setSpacing(2);
		right->addWidget(conditionsList);
		right->addStretch(1);
		Problems = secondary({ }, page);
		Problems->setObjectName(QStringLiteral("promptsProblems"));
		right->addWidget(Problems);
		auto* const rightBox = new QWidget{ page };
		rightBox->setLayout(right);
		rightBox->setMinimumWidth(300);
		rightBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		RightBox = rightBox;
		// Undo and Redo keys while the conditions have the focus (a text
		// field keeps its own).
		// Named and in Help › Keyboard Shortcuts, as every bound key is.
		for (const auto& [command, back] : { std::pair{ shortcut_command::prompts_conditions_undo, true }, std::pair{ shortcut_command::prompts_conditions_redo, false } }) {
			auto* const shortcut = new QShortcut{ rightBox };
			shortcut->setObjectName(back ? QStringLiteral("promptsConditionsUndo") : QStringLiteral("promptsConditionsRedo"));
			shortcut->setKeys(shortcut_keys(command));
			shortcut->setContext(Qt::WidgetWithChildrenShortcut);
			connect(shortcut, &QShortcut::activated, this, [this, back] { static_cast<void>(back ? undo() : redo()); });
		}
		columns->addWidget(rightBox, 1);
		column->addLayout(columns, 1);

		auto* const footer = new QHBoxLayout{ };
		AddTo              = new QPushButton{ tr("Add to"), page };
		AddTo->setObjectName(QStringLiteral("promptsAddTo"));
		auto* const addToMenu = new QMenu{ AddTo };
		AddTo->setMenu(addToMenu);
		connect(addToMenu, &QMenu::aboutToShow, this, [this, addToMenu] {
			addToMenu->clear();
			if (Selected) {
				fill_add_to(*addToMenu, *Selected);
			}
		});
		footer->addWidget(AddTo);
		Duplicate = new quiet_push_button{ tr("Duplicate"), page };
		Duplicate->setObjectName(QStringLiteral("promptsDuplicate"));
		footer->addWidget(Duplicate);
		Reset = new quiet_push_button{ tr("Reset to default"), page };
		Reset->setObjectName(QStringLiteral("promptsReset"));
		footer->addWidget(Reset);
		footer->addStretch(1);
		OnSurfaces = secondary({ }, page);
		OnSurfaces->setObjectName(QStringLiteral("promptsOnSurfaces"));
		OnSurfaces->setWordWrap(false);
		footer->addWidget(OnSurfaces);
		SavedLabel = secondary({ }, page);
		SavedLabel->setObjectName(QStringLiteral("promptsSaved"));
		SavedLabel->setWordWrap(false);
		footer->addWidget(SavedLabel);
		column->addLayout(footer);
		EditorPages->addWidget(page);

		connect(Name, &QLineEdit::textEdited, this, [this](const QString& aText) {
			edit_selected([&aText](core::prompt_macro& aMacro) { aMacro.Body.Name = aText.toStdString(); });
		});
		connect(Text, &QPlainTextEdit::textChanged, this, [this] {
			if (Filling) {
				return;
			}
			const auto text = Text->toPlainText().toStdString();
			edit_selected([&text](core::prompt_macro& aMacro) { aMacro.Body.Template = text; });
		});
		for (auto* const button : { AsQuestion, AsFindings }) {
			connect(button, &QToolButton::clicked, this, [this, button] {
				const auto kind = button == AsFindings ? core::prompt_ask::review : core::prompt_ask::question;
				edit_selected([kind](core::prompt_macro& aMacro) { aMacro.Body.Kind = kind; });
			});
		}
		for (auto* const button : { MatchAll, MatchAny, MatchNone }) {
			connect(button, &QToolButton::clicked, this, [this, button] {
				const auto match = button == MatchAny ? core::prompt_match::any : button == MatchNone ? core::prompt_match::none
																									  : core::prompt_match::all;
				if (Selected && !Filling) {
					static_cast<void>(applied(core::set_prompt_group_match(Editor, *Selected, { }, match)));
				}
			});
		}
		connect(conditionMenu, &QMenu::aboutToShow, this, [this, conditionMenu] {
			conditionMenu->clear();
			fill_condition_menu(*conditionMenu, { });
		});
		connect(Duplicate, &QPushButton::clicked, this, [this] {
			if (Selected) {
				static_cast<void>(duplicate(*Selected));
			}
		});
		connect(Reset, &QPushButton::clicked, this, [this] {
			if (!Selected) {
				return;
			}
			if (Editor.Repository.empty()) {
				reset_to_default(*Selected);
			} else {
				use_app_wide(*Selected);
			}
		});
	}

	void prompts_page::adopt_system_prompts(writing_hooks_panel* const aPanel)
	{
		if (aPanel == nullptr || System != nullptr) {
			return;
		}
		System = aPanel;
		System->set_embedded(true);
		System->setParent(EditorPages);
		EditorPages->addWidget(System);
		System->set_rows_changed([this] {
			if (!Filling) {
				refilter();
			}
		});
		refilter();
	}

	void prompts_page::present(const application::prompt_macro_projection& aProjection)
	{
		// [[BPR-2185]] (review N1): the core decides whether the projection
		// replaces what is shown; waiting edits and a write in flight keep it.
		if (core::adopt_prompt_projection(Editor, aProjection.Set, aProjection.Revision, aProjection.Readable, aProjection.Loaded)) {
			View = core::prompt_edit_view(Editor);
			set_repositories(Repositories);
			if (Selected && find(*Selected) == nullptr) {
				Selected.reset();
				SelectedPlacement.reset();
			}
			refresh_all();
		} else {
			set_repositories(Repositories);
		}
		show_status();
		flush();
	}

	void prompts_page::set_save_dispatch(save_dispatch aDispatch)
	{
		Save = std::move(aDispatch);
		flush();
	}

	void prompts_page::settle_save(const core::prompt_save_outcome aOutcome)
	{
		core::settle_prompt_save(Editor, aOutcome);
		show_status();
		flush();
	}

	void prompts_page::set_identity_source(identity_source aSource) { Identities = std::move(aSource); }

	void prompts_page::show_status()
	{
		switch (Editor.Notice) {
		case core::prompt_edit_notice::save_failed: Status->setText(tr("Your prompts couldn't be saved. The last change is kept here; it saves with the next one.")); break;
		case core::prompt_edit_notice::save_stale:  Status->setText(tr("Another window changed your prompts first; this page shows theirs.")); break;
		case core::prompt_edit_notice::unreadable:  Status->setText(tr("Your prompts can't be read, so the built-ins are shown and changes aren't saved.")); break;
		case core::prompt_edit_notice::none:
			if (Refusal.isEmpty()) {
				Status->clear();
			}
			break;
		}
		SavedLabel->setText(core::prompt_edits_saving(Editor) ? tr("Saving…") : Editor.Held ? tr("Not saved")
																							: tr("Saved"));
	}

	void prompts_page::set_repositories(const QStringList& aRepositories)
	{
		Repositories = aRepositories;
		auto listed  = aRepositories;
		for (const auto& layer : Editor.Document.Layers) {
			if (!listed.contains(QString::fromStdString(layer.Repository), Qt::CaseInsensitive)) {
				listed << QString::fromStdString(layer.Repository);
			}
		}
		const auto& repository = Editor.Repository;
		if (!repository.empty() && !listed.contains(QString::fromStdString(repository), Qt::CaseInsensitive)) {
			listed << QString::fromStdString(repository);
		}
		listed.sort(Qt::CaseInsensitive);
		const auto blocker = QSignalBlocker{ PromptsFor };
		PromptsFor->clear();
		PromptsFor->addItem(tr("All repositories"));
		PromptsFor->addItems(listed);
		PromptsFor->setCurrentIndex(repository.empty() ? 0 : std::max(0, PromptsFor->findText(QString::fromStdString(repository))));
	}

	void prompts_page::choose_repository(const std::string& aRepository)
	{
		Typing->stop();
		core::end_prompt_typing(Editor);
		flush();
		if (!core::choose_prompt_repository(Editor, aRepository)) {
			return;
		}
		View = core::prompt_edit_view(Editor);
		set_repositories(Repositories);
		if (Selected && find(*Selected) == nullptr) {
			Selected.reset();
		}
		SelectedPlacement.reset();
		// A teal edge says the edits are this repository's.
		const auto teal = override_colour(palette());
		RepositoryMark->setVisible(!aRepository.empty());
		auto mark = RepositoryMark->palette();
		mark.setColor(QPalette::WindowText, teal);
		RepositoryMark->setPalette(mark);
		Caption->setText(aRepository.empty() ? tr("Where prompts appear, in order. Drag a prompt up from the library, or right-click it and choose Add to.")
											 : tr("%1 starts from your prompts for all repositories. Changes here apply to its pull requests only.").arg(QString::fromStdString(aRepository)));
		ChangedHere->setVisible(!aRepository.empty());
		arrange_library_tools();
		refresh_all();
	}

	auto prompts_page::here_count() const -> int
	{
		return static_cast<int>(std::ranges::count_if(View.Library, [this](const core::prompt_macro& aMacro) { return core::prompt_standing_of(Editor, aMacro).differs_here(); }));
	}

	void prompts_page::use_app_wide(const std::string& aId) { static_cast<void>(applied(core::use_app_wide_prompt(Editor, aId))); }

	void prompts_page::use_app_wide_field(const std::string& aId, const QString& aField)
	{
		if (const auto field = core::find_prompt_field(aField.toStdString())) {
			static_cast<void>(applied(core::use_app_wide_field(Editor, aId, *field)));
		}
	}

	void prompts_page::revert_placement(const std::string& aId, const prompt_surface aSurface) { static_cast<void>(applied(core::revert_prompt_placement(Editor, aId, aSurface))); }

	void prompts_page::revert_order(const prompt_surface aSurface) { static_cast<void>(applied(core::revert_prompt_order(Editor, aSurface))); }

	void prompts_page::set_changed_here(const bool aOnly)
	{
		ChangedHereOnly = aOnly;
		if (ChangedHere->isChecked() != aOnly) {
			const auto blocker = QSignalBlocker{ ChangedHere };
			ChangedHere->setChecked(aOnly);
		}
		refilter();
	}

	void prompts_page::reveal()
	{
		Search->setFocus(Qt::OtherFocusReason);
		for (auto* parent = parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const scroll = qobject_cast<QScrollArea*>(parent); scroll != nullptr) {
				scroll->ensureWidgetVisible(this, 0, 0);
				break;
			}
		}
	}

	auto prompts_page::applied(const core::prompt_edit_outcome& aOutcome, const bool aTyping) -> bool
	{
		if (aOutcome.Refusal != core::prompt_edit_refusal::none) {
			auto words = QString{ };
			if (aOutcome.Refusal == core::prompt_edit_refusal::placement) {
				const auto reason = core::placement_reason(aOutcome.Placement);
				words             = QString::fromUtf8(reason.data(), static_cast<qsizetype>(reason.size()));
			} else if (aOutcome.Refusal == core::prompt_edit_refusal::library_full) {
				words = tr("Your library holds %1 prompts. Delete one to add another.").arg(core::maximum_prompt_macros);
			}
			if (!words.isEmpty()) {
				Refusal = words;
				Status->setText(Refusal);
			}
			return false;
		}
		Refusal.clear();
		if (!aOutcome.Changed) {
			return false;
		}
		View = core::prompt_edit_view(Editor);
		if (Selected && find(*Selected) == nullptr) {
			Selected.reset();
			SelectedPlacement.reset();
		}
		refresh_all();
		// A run of typing saves when it pauses; anything else at once.
		if (aTyping) {
			Typing->start();
		} else {
			Typing->stop();
		}
		show_status();
		flush();
		return true;
	}

	void prompts_page::refresh_all()
	{
		rebuild_surfaces();
		if (rebuild_library()) {
			rebuild_surfaces();
		}
		refresh_editor();
	}

	void prompts_page::refilter()
	{
		if (rebuild_library(true)) {
			rebuild_surfaces();
			refresh_editor();
		}
	}

	void prompts_page::flush()
	{
		// One write at a time, each from the revision its edits came from;
		// the core says when one is due.
		if (!Save || Typing->isActive()) {
			return;
		}
		if (auto request = core::next_prompt_save(Editor)) {
			Save(std::move(*request));
		}
		show_status();
	}

	auto prompts_page::find(const std::string& aId) const -> const core::prompt_macro*
	{
		const auto found = std::ranges::find(View.Library, aId, &core::prompt_macro::Id);
		return found == View.Library.end() ? nullptr : &*found;
	}

	auto prompts_page::refusal(const std::string& aId, const prompt_surface aSurface) const -> core::placement_refusal
	{
		const auto* const macro = find(aId);
		return macro == nullptr ? core::placement_refusal::unknown_prompt : core::placement_refusal_of(*macro, aSurface);
	}

	auto prompts_page::place(const std::string& aId, const prompt_surface aSurface, const std::optional<std::size_t> aAt) -> core::placement_refusal
	{
		const auto outcome = core::place_prompt(Editor, aId, aSurface, aAt);
		if (outcome.Refusal == core::prompt_edit_refusal::none) {
			Selected = aId;
			SelectedSystem.reset();
		}
		if (!applied(outcome) && outcome.Refusal == core::prompt_edit_refusal::none) {
			refresh_all();
		}
		return outcome.Placement;
	}

	auto prompts_page::drop(const std::string& aId, const std::optional<prompt_surface> aFrom, const prompt_surface aTo, const std::size_t aAt, const bool aCopy) -> core::placement_refusal
	{
		const auto outcome = core::drop_prompt(Editor, aId, aFrom, aTo, aAt, aCopy);
		if (outcome.Refusal == core::prompt_edit_refusal::none) {
			Selected = aId;
			SelectedSystem.reset();
			SelectedPlacement.reset();
		}
		if (!applied(outcome) && outcome.Refusal == core::prompt_edit_refusal::none) {
			refresh_all();
		}
		return outcome.Placement;
	}

	void prompts_page::remove(const std::string& aId, const prompt_surface aSurface)
	{
		const auto outcome = core::unplace_prompt(Editor, aId, aSurface);
		if (outcome.Changed) {
			SelectedPlacement.reset();
		}
		static_cast<void>(applied(outcome));
	}

	void prompts_page::move(const std::string& aId, const prompt_surface aSurface, const int aBy)
	{
		const auto outcome = core::move_prompt(Editor, aId, aSurface, aBy);
		if (outcome.Changed) {
			const auto  view   = core::prompt_edit_view(Editor);
			const auto& placed = view.Surfaces.Lists.at(index(aSurface));
			SelectedPlacement  = std::pair{ aSurface, static_cast<std::size_t>(std::ranges::find(placed, aId) - placed.begin()) };
		}
		static_cast<void>(applied(outcome));
		Lists.at(index(aSurface))->setFocus(Qt::OtherFocusReason);
	}

	void prompts_page::set_on(const std::string& aId, const bool aOn) { static_cast<void>(applied(core::set_prompt_on(Editor, aId, aOn))); }

	auto prompts_page::new_prompt() -> std::string
	{
		// [[BPR-2185]] (review N2): the host mints the identity.
		const auto id = Identities ? Identities() : std::string{ };
		if (!applied(core::add_prompt(Editor, id, tr("New prompt").toStdString()))) {
			return { };
		}
		Selected = id;
		SelectedSystem.reset();
		set_filter(prompt_library_filter::all);
		refresh_all();
		Name->setFocus(Qt::OtherFocusReason);
		Name->selectAll();
		return id;
	}

	auto prompts_page::duplicate(const std::string& aId) -> std::optional<std::string>
	{
		const auto* const original = find(aId);
		if (original == nullptr) {
			return std::nullopt;
		}
		const auto id   = Identities ? Identities() : std::string{ };
		const auto name = tr("%1 copy").arg(QString::fromStdString(original->Body.Name)).toStdString();
		if (!applied(core::duplicate_prompt(Editor, aId, id, name))) {
			return std::nullopt;
		}
		Selected = id;
		SelectedSystem.reset();
		refresh_all();
		Name->setFocus(Qt::OtherFocusReason);
		Name->selectAll();
		return id;
	}

	void prompts_page::reset_to_default(const std::string& aId) { static_cast<void>(applied(core::reset_prompt(Editor, aId))); }

	auto prompts_page::delete_prompt(const std::string& aId, const bool aConfirm) -> bool
	{
		const auto* const macro = find(aId);
		// In a repository only its own can be deleted; an app-wide prompt is
		// turned off there. The core refuses the rest; asking first is ours.
		if (macro == nullptr || macro->BuiltIn || (!Editor.Repository.empty() && !core::prompt_standing_of(Editor, *macro).OnlyHere)) {
			return false;
		}
		if (aConfirm) {
			const auto on    = core::surfaces_of(View.Surfaces, aId);
			auto       names = QStringList{ };
			for (const auto surface : on) {
				names << prompt_surface_name(surface);
			}
			const auto  detail = on.empty() ? tr("It is not on any surface. This cannot be undone.")
											: tr("It is on %n surface(s) and will be removed from them: %1. This cannot be undone.", nullptr, static_cast<int>(on.size())).arg(names.join(tr(" and ")));
			auto        box    = QMessageBox{ QMessageBox::Warning, tr("Delete “%1”?").arg(QString::fromStdString(macro->Body.Name)), detail, QMessageBox::Cancel, this };
			auto* const remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
			box.setDefaultButton(QMessageBox::Cancel);
			box.exec();
			if (box.clickedButton() != remove) {
				return false;
			}
		}
		// [[BPR-2220]]: deleting the selected prompt selects its neighbour in
		// the list, the next row or else the one before.
		auto neighbour = std::optional<std::string>{ };
		if (Selected && *Selected == aId) {
			const auto prompt_at = [this](const int aRow) -> std::optional<std::string> {
				const auto id = LibraryList->item(aRow)->data(id_role);
				return id.isValid() ? std::optional{ id.toString().toStdString() } : std::nullopt;
			};
			auto row = -1;
			for (int at = 0; at < LibraryList->count() && row < 0; ++at) {
				row = prompt_at(at) == aId ? at : -1;
			}
			for (int at = row + 1; row >= 0 && at < LibraryList->count() && !neighbour; ++at) {
				neighbour = prompt_at(at);
			}
			for (int at = row - 1; at >= 0 && !neighbour; --at) {
				neighbour = prompt_at(at);
			}
		}
		const auto outcome = core::delete_prompt(Editor, aId);
		if (outcome.Changed && Selected && *Selected == aId) {
			Selected = neighbour;
			SelectedPlacement.reset();
		}
		return applied(outcome);
	}

	auto prompts_page::restore_built_ins(const bool aConfirm) -> bool
	{
		if (aConfirm) {
			auto        box     = QMessageBox{ QMessageBox::Question, tr("Restore the built-in prompts?"),
											   tr("Every built-in prompt gets its shipped text and is turned on, and the surfaces go back to where they shipped. Your own prompts are kept."), QMessageBox::Cancel, this };
			auto* const restore = box.addButton(QMessageBox::Reset);
			restore->setText(tr("Restore"));
			box.exec();
			if (box.clickedButton() != restore) {
				return false;
			}
		}
		const auto outcome = core::restore_built_in_prompts(Editor);
		static_cast<void>(applied(outcome));
		return outcome.Refusal == core::prompt_edit_refusal::none;
	}

	void prompts_page::set_review_result(const QString& aFormat)
	{
		ResultText->setPlainText(aFormat);
		const auto* const macro = Selected ? find(*Selected) : nullptr;
		ResultBox->setVisible(macro != nullptr && macro->Body.Kind == core::prompt_ask::review && !aFormat.isEmpty());
	}

	void prompts_page::set_filter(const prompt_library_filter aFilter)
	{
		Filter = aFilter;
		if (auto* const button = Filters->button(static_cast<int>(aFilter)); button != nullptr) {
			button->setChecked(true);
		}
		refilter();
	}

	void prompts_page::edit_selected(const std::function<void(core::prompt_macro&)>& aEdit)
	{
		if (Filling || !Selected) {
			return;
		}
		const auto* const macro = find(*Selected);
		if (macro == nullptr) {
			return;
		}
		auto edited = *macro;
		aEdit(edited);
		// A run of typing in the name or text is one undo step.
		const auto typing = edited.Body.Name != macro->Body.Name || edited.Body.Template != macro->Body.Template;
		static_cast<void>(applied(core::edit_prompt(Editor, edited, typing), typing));
	}

	auto prompts_page::undo() -> bool
	{
		Typing->stop();
		const auto outcome = core::undo_prompt_edit(Editor);
		if (outcome.Changed && outcome.Prompt) {
			Selected = outcome.Prompt;
			SelectedSystem.reset();
			OpenConditions.clear();
		}
		return applied(outcome);
	}

	auto prompts_page::redo() -> bool
	{
		Typing->stop();
		const auto outcome = core::redo_prompt_edit(Editor);
		if (outcome.Changed && outcome.Prompt) {
			Selected = outcome.Prompt;
			SelectedSystem.reset();
			OpenConditions.clear();
		}
		return applied(outcome);
	}

	void prompts_page::select(const std::string& aId)
	{
		if (find(aId) == nullptr) {
			return;
		}
		Selected = aId;
		SelectedSystem.reset();
		refresh_all();
	}

	void prompts_page::select_system(const int aKey)
	{
		Selected.reset();
		SelectedPlacement.reset();
		SelectedSystem = aKey;
		if (System != nullptr) {
			System->select_system(aKey);
		}
		refresh_all();
	}

	auto prompts_page::selected() const -> std::optional<std::string> { return Selected; }

	auto prompts_page::shown_in_library(const core::prompt_macro& aMacro) const -> bool
	{
		const auto needle = Search->text().trimmed();
		if (!needle.isEmpty() && !QString::fromStdString(aMacro.Body.Name).contains(needle, Qt::CaseInsensitive)
			&& !QString::fromStdString(aMacro.Body.Template).contains(needle, Qt::CaseInsensitive)) {
			return false;
		}
		if (ChangedHereOnly && !Editor.Repository.empty() && !core::prompt_standing_of(Editor, aMacro).differs_here()) {
			return false;
		}
		switch (Filter) {
		case prompt_library_filter::all:              return true;
		case prompt_library_filter::built_in:         return aMacro.BuiltIn;
		case prompt_library_filter::yours:            return !aMacro.BuiltIn;
		case prompt_library_filter::not_on_a_surface: return core::surfaces_of(View.Surfaces, aMacro.Id).empty();
		case prompt_library_filter::system:           return false;
		}
		return true;
	}

	auto prompts_page::rebuild_library(const bool aFiltered) -> bool
	{
		Filling = true;
		LibraryList->clear();
		const auto header = [this](const QString& aText) {
			auto* const item = new QListWidgetItem{ aText, LibraryList };
			item->setData(header_role, true);
			item->setFlags(Qt::NoItemFlags);
			auto font = item->font();
			font.setWeight(QFont::DemiBold);
			font.setPointSizeF(font.pointSizeF() * 0.88);
			item->setFont(font);
		};
		QListWidgetItem* current = nullptr;
		auto             any     = false;
		for (const auto& macro : View.Library) {
			if (!shown_in_library(macro)) {
				continue;
			}
			any                 = true;
			auto       tags     = QStringList{ };
			const auto standing = core::prompt_standing_of(Editor, macro);
			if (standing.OnlyHere) {
				tags << tr("Only here");
			} else if (!macro.BuiltIn) {
				tags << tr("Yours");
			} else if (standing.Edited) {
				tags << tr("Edited");
			}
			if (standing.OffHere) {
				tags << tr("Off here");
			}
			if (standing.ChangedHere) {
				tags << tr("Changed here");
			}
			if (is_tour(macro)) {
				tags << tr("Tour");
			}
			auto* const item = new QListWidgetItem{ QString::fromStdString(macro.Body.Name), LibraryList };
			item->setData(id_role, QString::fromStdString(macro.Id));
			item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
			item->setCheckState(macro.On ? Qt::Checked : Qt::Unchecked);
			if (!tags.isEmpty()) {
				item->setText(item->text() + QStringLiteral("   ") + tags.join(QStringLiteral(", ")));
			}
			item->setToolTip(tr("%1. Drag it up to a surface, or right-click and choose Add to.").arg(QString::fromStdString(macro.Body.Name)));
			item->setData(Qt::AccessibleDescriptionRole, tags.join(QStringLiteral(", ")));
			if (Selected && *Selected == macro.Id) {
				current = item;
			}
		}
		if (System != nullptr && (Filter == prompt_library_filter::all || Filter == prompt_library_filter::system)) {
			auto group  = QString{ };
			auto titled = false;
			for (const auto& row : System->system_rows()) {
				const auto needle = Search->text().trimmed();
				if (!needle.isEmpty() && !row.Name.contains(needle, Qt::CaseInsensitive)) {
					continue;
				}
				if (!titled) {
					header(tr("System"));
					titled = true;
				}
				if (row.Group != group) {
					group = row.Group;
					header(QStringLiteral("  ") + group);
				}
				auto text = row.Name + QStringLiteral("   ") + tr("System");
				if (!row.Marker.isEmpty()) {
					text += QStringLiteral(", ") + row.Marker;
				}
				auto* const item = new QListWidgetItem{ text, LibraryList };
				item->setData(system_role, row.Key);
				item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
				item->setToolTip(tr("Sent by the app: %1. Edit and Reset only.").arg(row.Usage));
				if (SelectedSystem && *SelectedSystem == row.Key) {
					current = item;
				}
				any = true;
			}
		}
		// [[BPR-2220]] (owner, 2026-11-16: "the bottom of prompt library looks
		// like a broken menu when no prompt is selected"): while the list
		// shows a prompt, one is selected. When the selection is gone, or a
		// filter hides it, the first prompt shown is; a placement chosen on a
		// surface stays chosen though the library's filter hides it. A list
		// showing none says so and hides the editor, keeping the selection
		// for when the filter shows it again.
		const auto lost  = !(Selected && find(*Selected) != nullptr) && !(SelectedSystem && System != nullptr);
		auto       moved = false;
		if (current == nullptr && any && (aFiltered || lost)) {
			for (int row = 0; row < LibraryList->count() && current == nullptr; ++row) {
				auto* const item = LibraryList->item(row);
				if ((item->flags() & Qt::ItemIsSelectable) == 0) {
					continue;
				}
				current = item;
				SelectedPlacement.reset();
				if (const auto system = item->data(system_role); system.isValid()) {
					Selected.reset();
					SelectedSystem = system.toInt();
					if (System != nullptr) {
						System->select_system(*SelectedSystem);
					}
				} else {
					Selected = item->data(id_role).toString().toStdString();
					SelectedSystem.reset();
				}
				moved = true;
			}
		}
		if (!any) {
			const auto searched = !Search->text().trimmed().isEmpty();
			auto       words    = tr("No prompts match.");
			if (!searched && Filter == prompt_library_filter::yours) {
				words = tr("None of your own yet. New prompt writes one.");
			} else if (!searched && Filter == prompt_library_filter::not_on_a_surface) {
				words = tr("Every prompt is on a surface.");
			}
			auto* const item = new QListWidgetItem{ words, LibraryList };
			item->setFlags(Qt::NoItemFlags);
		}
		EditorFrame->setVisible(any || (SelectedPlacement.has_value() && !lost));
		ChangedHere->setText(tr("Changed here (%1)").arg(here_count()));
		if (current != nullptr) {
			LibraryList->setCurrentItem(current);
		} else {
			LibraryList->setCurrentRow(-1);
		}
		Filling = false;
		return moved;
	}

	void prompts_page::rebuild_surfaces()
	{
		Filling = true;
		for (std::size_t surface = 0U; surface < core::prompt_surface_count; ++surface) {
			auto* const list = static_cast<surface_list_view*>(Lists.at(surface));
			if (list == nullptr) {
				continue;
			}
			list->clear();
			const auto& ids         = View.Surfaces.Lists.at(surface);
			const auto  surfaceHere = core::prompt_surface_standing_of(Editor, static_cast<prompt_surface>(surface));
			for (std::size_t row = 0U; row < ids.size(); ++row) {
				const auto* const macro = find(ids[row]);
				if (macro == nullptr) {
					continue;
				}
				auto flags = 0;
				// Review N3: nested groups are conditions too.
				if (core::prompt_has_conditions(macro->Body)) {
					flags |= has_conditions;
				}
				if (!macro->On) {
					flags |= is_off;
				}
				if (!core::is_menu(static_cast<prompt_surface>(surface))) {
					flags |= is_chip;
				}
				const auto here = SelectedPlacement && SelectedPlacement->first == static_cast<prompt_surface>(surface) && SelectedPlacement->second == row;
				if (Selected && *Selected == macro->Id) {
					flags |= here ? is_filled : is_outlined;
				}
				// In a repository: what it added, and what differs from app-wide.
				const auto standing = core::prompt_standing_of(Editor, *macro);
				const auto added    = std::ranges::find(surfaceHere.Added, macro->Id) != surfaceHere.Added.end();
				auto       tag      = macro->On ? QString{ } : tr("Off");
				if (added) {
					tag = tr("Added here");
					flags |= revertable;
				} else if (standing.OffHere) {
					tag = tr("Off here");
					flags |= is_override;
				}
				if (standing.differs_here() && !added) {
					flags |= is_override;
				}
				auto* const item = new QListWidgetItem{ QString::fromStdString(macro->Body.Name), list };
				item->setData(id_role, QString::fromStdString(macro->Id));
				item->setData(flags_role, flags);
				item->setData(tags_role, tag);
				item->setData(Qt::AccessibleTextRole, placement_name(static_cast<prompt_surface>(surface), row) + (added ? tr(", Added here") : QString{ }));
				item->setToolTip(item->data(Qt::AccessibleTextRole).toString());
				if (here) {
					list->setCurrentItem(item);
				}
			}
			// Removed here: the row stays, struck through, after the rest.
			for (const auto& id : surfaceHere.Removed) {
				if (const auto* const macro = find(id); macro != nullptr) {
					auto* const item = new QListWidgetItem{ QString::fromStdString(macro->Body.Name), list };
					item->setData(id_role, QString::fromStdString(id));
					item->setData(flags_role, is_removed | revertable | (core::is_menu(static_cast<prompt_surface>(surface)) ? 0 : is_chip));
					item->setData(tags_role, tr("Removed here"));
					item->setFlags(Qt::ItemIsEnabled);
					item->setData(Qt::AccessibleTextRole, tr("%1, %2, Removed here").arg(prompt_surface_name(static_cast<prompt_surface>(surface)), QString::fromStdString(macro->Body.Name)));
					item->setToolTip(tr("Removed here. The revert button restores it."));
				}
			}
			if (auto* const order = OrderTags.at(surface); order != nullptr) {
				order->setVisible(surfaceHere.Ordered);
				order->setText(tr("<span style=\"color:%1\">Order changed here</span> <a href=\"revert\">Revert</a>").arg(override_colour(palette()).name()));
			}
			list->fit();
		}
		Filling = false;
	}

	auto prompts_page::placement_name(const prompt_surface aSurface, const std::size_t aIndex) const -> QString
	{
		const auto& ids = View.Surfaces.Lists.at(index(aSurface));
		if (aIndex >= ids.size()) {
			return { };
		}
		const auto* const macro = find(ids[aIndex]);
		auto              name  = tr("%1, %2 of %3, %4").arg(prompt_surface_name(aSurface)).arg(aIndex + 1U).arg(ids.size()).arg(macro != nullptr ? QString::fromStdString(macro->Body.Name) : QString{ });
		if (macro != nullptr && core::prompt_has_conditions(macro->Body)) {
			name += tr(", has conditions");
		}
		if (macro != nullptr && !macro->On) {
			name += tr(", Off");
		}
		return name;
	}

	void prompts_page::show_system(const bool aShown)
	{
		if (System != nullptr) {
			EditorPages->setCurrentWidget(aShown ? static_cast<QWidget*>(System) : EditorPages->widget(0));
			// A system prompt's editor (preview, test, save) takes the room
			// it needs; the page scrolls.
			EditorPages->parentWidget()->setMaximumHeight(QWIDGETSIZE_MAX);
			EditorPages->parentWidget()->setMinimumHeight(aShown ? 560 : 360);
		}
	}

	void prompts_page::refresh_editor()
	{
		if (SelectedSystem && System != nullptr) {
			show_system(true);
			return;
		}
		show_system(false);
		Filling                 = true;
		const auto* const macro = Selected ? find(*Selected) : nullptr;
		const auto        shown = macro != nullptr;
		for (auto* const widget : std::initializer_list<QWidget*>{ Name, AsQuestion, AsFindings, Text, MatchAll, MatchAny, MatchNone, AddCondition, AddTo, Duplicate, Reset }) {
			widget->setEnabled(shown);
		}
		if (!shown) {
			Name->clear();
			Text->clear();
			BuiltInTag->clear();
			ActionNote->hide();
			OnSurfaces->setText(tr("Choose a prompt to edit it."));
			while (auto* const item = Conditions->takeAt(0)) {
				retire(item);
			}
			Filling = false;
			return;
		}
		if (Name->text().toStdString() != macro->Body.Name) {
			Name->setText(QString::fromStdString(macro->Body.Name));
			Name->setCursorPosition(0);
		}
		const auto standing = core::prompt_standing_of(Editor, *macro);
		BuiltInTag->setText(macro->BuiltIn ? (standing.Edited ? tr("Built-in, edited") : tr("Built-in")) : tr("Yours"));
		AsQuestion->setChecked(macro->Body.Kind == core::prompt_ask::question);
		AsFindings->setChecked(macro->Body.Kind == core::prompt_ask::review);
		ResultBox->setVisible(macro->Body.Kind == core::prompt_ask::review && !ResultText->toPlainText().isEmpty());
		MatchAll->setChecked(macro->Body.Match == core::prompt_match::all);
		MatchAny->setChecked(macro->Body.Match == core::prompt_match::any);
		MatchNone->setChecked(macro->Body.Match == core::prompt_match::none);
		UnmetHide->setChecked(macro->Body.WhenUnmet == core::prompt_when_unmet::hide);
		UnmetDisable->setChecked(macro->Body.WhenUnmet == core::prompt_when_unmet::disable);
		WhenUnmetRow->setVisible(core::prompt_has_conditions(macro->Body));
		const auto system = action_system_key(macro->Action);
		ActionNote->setVisible(system.has_value());
		Text->setVisible(!system.has_value());
		InsertRow->setVisible(!system.has_value());
		AsQuestion->setEnabled(!system.has_value());
		AsFindings->setEnabled(!system.has_value());
		if (system) {
			ActionNote->setText(tr("This row runs an action. What it sends is a system prompt: <a href=\"%1\">edit it in the library</a>.").arg(*system));
		} else if (Text->toPlainText().toStdString() != macro->Body.Template) {
			Text->setPlainText(QString::fromStdString(macro->Body.Template));
		}
		// Insert offers what the prompt's surfaces can fill in.
		auto* const insert = static_cast<QHBoxLayout*>(InsertRow->layout());
		while (auto* const item = insert->takeAt(0)) {
			retire(item);
		}
		// Insert offers the variables its surfaces fill in: the first three
		// as tokens, every one in its menu.
		const auto  on           = core::surfaces_of(View.Surfaces, macro->Id);
		const auto  place        = on.empty() ? macro->Place : core::placed_on(*macro, on.front()).Place;
		auto* const insertButton = new quiet_tool_button{ InsertRow };
		insertButton->setObjectName(QStringLiteral("promptsInsert"));
		insertButton->setText(tr("Insert"));
		insertButton->setPopupMode(QToolButton::InstantPopup);
		auto* const variables = new QMenu{ insertButton };
		insertButton->setMenu(variables);
		insert->addWidget(insertButton);
		auto tokens = 0;
		for (const auto variable : core::prompt_variables(place)) {
			const auto name = QStringLiteral("{%1}").arg(QString::fromLatin1(core::stable_name(variable)));
			variables->addAction(name, this, [this, name] {
				Text->insertPlainText(name);
				Text->setFocus(Qt::OtherFocusReason);
			});
			if (tokens++ < 3) {
				auto* const token = new quiet_tool_button{ InsertRow };
				token->setText(name);
				connect(token, &QToolButton::clicked, this, [this, name] {
					Text->insertPlainText(name);
					Text->setFocus(Qt::OtherFocusReason);
				});
				insert->addWidget(token);
			}
		}
		insert->addStretch(1);
		Bytes = secondary(tr("%1 of 8,192 bytes").arg(macro->Body.Template.size()), InsertRow);
		Bytes->setWordWrap(false);
		insert->addWidget(Bytes);

		UndoButton->setEnabled(!Editor.Undo.empty());
		RedoButton->setEnabled(!Editor.Redo.empty());
		const auto room = core::prompt_condition_room_of(*macro, { });
		AddCondition->setEnabled(room.Condition || room.Group);
		AddCondition->setToolTip(room.Crowded ? tr("A list holds four conditions; put more in a group.") : tr("Add a condition or a group"));
		rebuild_conditions();

		// Problems, as the prompt reads where it is placed (review N3: in words).
		auto problems = QStringList{ };
		for (const auto& problem : core::prompt_placement_problems(*macro, View.Surfaces)) {
			problems << tr("On %1 it isn't offered: %2.").arg(prompt_surface_name(problem.Surface), prompt_problem_words(problem.Problem));
		}
		Problems->setText(problems.join(QLatin1Char('\n')));
		Duplicate->setEnabled(true);
		if (Editor.Repository.empty()) {
			Reset->setText(tr("Reset to default"));
			Reset->setVisible(macro->BuiltIn);
			Reset->setEnabled(macro->BuiltIn && standing.Edited);
			Differs->hide();
		} else {
			Reset->setText(tr("Use app-wide for this prompt"));
			Reset->setVisible(!standing.OnlyHere);
			Reset->setEnabled(!standing.OnlyHere && !standing.Differing.empty());
			auto fields = QStringList{ };
			for (const auto field : standing.Differing) {
				auto words = QString{ };
				switch (field) {
				case core::prompt_field::name:       words = tr("Use app-wide name"); break;
				case core::prompt_field::text:       words = tr("Use app-wide text"); break;
				case core::prompt_field::answer:     words = tr("Use app-wide Answer as"); break;
				case core::prompt_field::conditions: words = tr("Use app-wide conditions"); break;
				case core::prompt_field::on:         words = macro->On ? tr("Turn off, as app-wide") : tr("Turn on, as app-wide"); break;
				}
				fields << QStringLiteral("<a href=\"%1\">%2</a>").arg(QString::fromLatin1(core::stable_name(field)), words);
			}
			Differs->setVisible(!fields.isEmpty() || standing.OnlyHere);
			Differs->setText(standing.OnlyHere ? tr("<span style=\"color:%1\">Only here.</span> Delete it to remove it.").arg(override_colour(palette()).name())
											   : tr("<span style=\"color:%1\">Differs from app-wide.</span> %2").arg(override_colour(palette()).name(), fields.join(QStringLiteral(" \u00B7 "))));
		}
		OnSurfaces->setText(on.empty() ? tr("On no surface") : on.size() == 1U ? tr("On 1 surface, outlined above")
																			   : tr("On %1 surfaces, outlined above").arg(on.size()));
		show_status();
		Filling = false;
	}

	namespace {
		/// The group at `aPath` (indices into each level's groups), read
		/// only; none when the path no longer names one.
		[[nodiscard]] auto group_at(const core::prompt_macro_body& aBody, const core::prompt_condition_path& aPath) -> std::optional<core::prompt_condition_group>
		{
			auto group = core::prompt_condition_group{ .Match = aBody.Match, .Conditions = aBody.Conditions, .Groups = aBody.Groups };
			for (const auto at : aPath) {
				if (at >= group.Groups.size()) {
					return std::nullopt;
				}
				auto inner = group.Groups[at];
				group      = std::move(inner);
			}
			return group;
		}

		[[nodiscard]] auto path_key(const core::prompt_condition_path& aPath, const std::optional<std::size_t> aLeaf = std::nullopt) -> QString
		{
			auto key = QStringLiteral("g");
			for (const auto at : aPath) {
				key += QLatin1Char('.') + QString::number(at);
			}
			return aLeaf ? key + QLatin1Char(':') + QString::number(*aLeaf) : key;
		}

		[[nodiscard]] auto match_words(const core::prompt_match aMatch) -> QString
		{
			switch (aMatch) {
			case core::prompt_match::all:  return QObject::tr("All of these");
			case core::prompt_match::any:  return QObject::tr("Any of these");
			case core::prompt_match::none: return QObject::tr("None of these");
			}
			return { };
		}
	} // namespace

	void prompts_page::fill_condition_menu(QMenu& aMenu, const core::prompt_condition_path& aPath)
	{
		const auto* const macro = Selected ? find(*Selected) : nullptr;
		if (macro == nullptr) {
			return;
		}
		// Review N3: the core says what may be added here and which facts
		// the prompt's chats hold.
		const auto room = core::prompt_condition_room_of(*macro, aPath);
		if (!room.Exists) {
			return;
		}
		// Look prompts-v002 (owner: "I would reduce the option to just Group
		// in the Add since its so easy to change between all/any/none"): Add
		// offers a condition (a submenu) or a group.
		auto* const conditions = aMenu.addMenu(tr("Condition"));
		conditions->setObjectName(QStringLiteral("promptsAddConditionMenu"));
		conditions->setEnabled(room.Condition);
		for (const auto& name : { tr("Pull request"), tr("File"), tr("Thread"), tr("Chat") }) {
			auto titled = false;
			for (const auto fact : room.Facts) {
				if (prompt_fact_group(fact) != name) {
					continue;
				}
				if (!titled) {
					conditions->addSection(name);
					titled = true;
				}
				auto* const action = conditions->addAction(prompt_fact_name(fact), this, [this, fact, aPath] {
					if (!Selected) {
						return;
					}
					const auto id = *Selected;
					if (applied(core::add_prompt_condition(Editor, id, aPath, fact))) {
						// A row just added is open.
						if (const auto* const added = find(id); added != nullptr) {
							if (const auto group = group_at(added->Body, aPath); group && !group->Conditions.empty()) {
								OpenConditions.insert(path_key(aPath, group->Conditions.size() - 1U));
								rebuild_conditions();
							}
						}
					}
				});
				action->setEnabled(room.Condition);
			}
		}
		// Groups nest, as the owner's tag queries do; a new one is All of
		// these, changed in its own box.
		auto* const groupAction = aMenu.addAction(tr("Group"), this, [this, aPath] {
			if (Selected) {
				static_cast<void>(applied(core::add_prompt_condition_group(Editor, *Selected, aPath)));
			}
		});
		groupAction->setObjectName(QStringLiteral("promptsAddGroup"));
		groupAction->setEnabled(room.Group);
		aMenu.setToolTipsVisible(true);
		if (room.Full) {
			aMenu.setToolTip(tr("A prompt can have twelve conditions."));
		}
	}

	void prompts_page::rebuild_conditions()
	{
		while (auto* const item = Conditions->takeAt(0)) {
			retire(item);
		}
		const auto* const macro = Selected ? find(*Selected) : nullptr;
		if (macro == nullptr) {
			return;
		}
		if (!core::prompt_has_conditions(macro->Body)) {
			Conditions->addWidget(secondary(tr("None: it is offered wherever it is placed."), Conditions->parentWidget()));
		}
		const auto body = macro->Body;
		add_group_rows(*Conditions, body, { });
	}

	void prompts_page::add_group_rows(QVBoxLayout& aColumn, const core::prompt_macro_body& aBody, const core::prompt_condition_path& aPath)
	{
		const auto group = group_at(aBody, aPath);
		if (!group || !Selected) {
			return;
		}
		const auto  id     = *Selected;
		auto* const parent = aColumn.parentWidget() != nullptr ? aColumn.parentWidget() : Conditions->parentWidget();
		for (std::size_t at = 0U; at < group->Conditions.size(); ++at) {
			const auto& condition = group->Conditions[at];
			const auto  key       = path_key(aPath, at);
			const auto  open      = OpenConditions.contains(key);
			auto* const row       = new QFrame{ parent };
			row->setObjectName(aPath.empty() ? QStringLiteral("promptsCondition-%1").arg(at) : QStringLiteral("promptsCondition-%1").arg(key));
			auto* const column = new QVBoxLayout{ row };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(2);
			auto* const line       = new QHBoxLayout{ };
			auto* const disclosure = new quiet_tool_button{ row };
			disclosure->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
			disclosure->setAutoRaise(true);
			disclosure->setAccessibleName(open ? tr("Close") : tr("Open"));
			line->addWidget(disclosure);
			line->addWidget(new QLabel{ prompt_fact_name(condition.Fact), row }, 1);
			auto* const removeButton = new quiet_tool_button{ row };
			// Look prompts-v002 (owner: "The minus is not clear that it is a
			// delete"): a trash can, as the pending list's.
			removeButton->setIcon(conversation_icon(conversation_glyph::trash, palette()));
			removeButton->setAutoRaise(true);
			removeButton->setObjectName(QStringLiteral("promptsRemoveCondition"));
			removeButton->setAccessibleName(tr("Remove %1").arg(prompt_fact_name(condition.Fact)));
			removeButton->setToolTip(removeButton->accessibleName());
			line->addWidget(removeButton);
			column->addLayout(line);
			connect(disclosure, &QToolButton::clicked, this, [this, key] {
				if (!OpenConditions.remove(key)) {
					OpenConditions.insert(key);
				}
				rebuild_conditions();
			});
			connect(removeButton, &QToolButton::clicked, this, [this, id, aPath, at] {
				OpenConditions.clear();
				static_cast<void>(applied(core::remove_prompt_condition(Editor, id, aPath, at)));
			});
			// One condition changed: the core puts it back where it is.
			const auto edit = [this, id, aPath, at, condition](const std::function<void(core::prompt_condition&)>& aChange) {
				auto changed = condition;
				aChange(changed);
				static_cast<void>(applied(core::set_prompt_condition(Editor, id, aPath, at, std::move(changed))));
			};
			if (open) {
				auto* const settings = new QHBoxLayout{ };
				settings->setContentsMargins(15, 0, 0, 0);
				const auto patterned = condition.Fact == core::prompt_fact::path || condition.Fact == core::prompt_fact::repository;
				const auto polar     = condition.Fact != core::prompt_fact::pushes_since_review && condition.Fact != core::prompt_fact::file_unreviewed
					&& condition.Fact != core::prompt_fact::replies_since_mine && condition.Fact != core::prompt_fact::thread_activity;
				if (polar) {
					auto* const polarity = new QComboBox{ row };
					polarity->addItems(patterned ? QStringList{ tr("matches"), tr("doesn't match") } : QStringList{ tr("is"), tr("is not") });
					polarity->setCurrentIndex(condition.Negated ? 1 : 0);
					settings->addWidget(polarity);
					connect(polarity, &QComboBox::activated, this, [edit](const int aIndex) { edit([aIndex](core::prompt_condition& aCondition) { aCondition.Negated = aIndex == 1; }); });
				}
				if (patterned) {
					auto* const patterns = new QLineEdit{ row };
					auto        joined   = QStringList{ };
					for (const auto& pattern : condition.Patterns) {
						joined << QString::fromStdString(pattern);
					}
					patterns->setText(joined.join(QStringLiteral(", ")));
					patterns->setPlaceholderText(tr("src/**, *.cpp"));
					settings->addWidget(patterns, 1);
					connect(patterns, &QLineEdit::editingFinished, this, [edit, patterns] {
						const auto parsed = core::parse_prompt_patterns(patterns->text().toStdString());
						edit([&parsed](core::prompt_condition& aCondition) { aCondition.Patterns = parsed; });
					});
				}
				if (condition.Fact == core::prompt_fact::selected_lines) {
					auto* const lines = new QComboBox{ row };
					lines->addItems({ tr("any"), tr("one line"), tr("several lines") });
					lines->setCurrentIndex(static_cast<int>(condition.Lines));
					settings->addWidget(lines);
					connect(lines, &QComboBox::activated, this, [edit](const int aIndex) { edit([aIndex](core::prompt_condition& aCondition) { aCondition.Lines = static_cast<core::prompt_lines_value>(aIndex); }); });
				}
				if (condition.Fact == core::prompt_fact::thread_activity) {
					auto* const count = new QComboBox{ row };
					count->setAccessibleName(tr("Count"));
					count->addItems({ tr("Replies and pushes"), tr("Replies"), tr("Pushes") });
					count->setCurrentIndex(static_cast<int>(condition.Activity));
					settings->addWidget(new QLabel{ tr("Count"), row });
					settings->addWidget(count);
					connect(count, &QComboBox::activated, this, [edit](const int aIndex) { edit([aIndex](core::prompt_condition& aCondition) { aCondition.Activity = static_cast<core::prompt_activity_value>(aIndex); }); });
				}
				settings->addStretch(1);
				column->addLayout(settings);
				auto* const note = secondary(prompt_condition_unmet_line(condition), row);
				note->setContentsMargins(15, 0, 0, 0);
				column->addWidget(note);
			}
			aColumn.addWidget(row);
		}
		// Each nested group: a bordered box with its own match, add and remove.
		for (std::size_t at = 0U; at < group->Groups.size(); ++at) {
			auto path = aPath;
			path.push_back(at);
			const auto  match = group->Groups[at].Match;
			auto* const box   = new QFrame{ parent };
			box->setObjectName(QStringLiteral("promptsConditionGroup-%1").arg(path_key(path)));
			box->setFrameShape(QFrame::StyledPanel);
			box->setAccessibleName(tr("Group: %1").arg(match_words(match)));
			auto* const inner = new QVBoxLayout{ box };
			inner->setContentsMargins(8, 6, 6, 6);
			inner->setSpacing(2);
			auto* const head = new QHBoxLayout{ };
			head->setSpacing(0);
			// Short in a group, its full words in its tooltip.
			for (const auto choice : { core::prompt_match::all, core::prompt_match::any, core::prompt_match::none }) {
				auto* const button = segmented(choice == core::prompt_match::all ? tr("All") : choice == core::prompt_match::any ? tr("Any")
																																 : tr("None"),
											   box);
				button->setToolTip(match_words(choice));
				button->setAccessibleName(match_words(choice));
				button->setChecked(choice == match);
				button->setObjectName(QStringLiteral("promptsGroupMatch-%1-%2").arg(path_key(path), QString::fromLatin1(core::stable_name(choice))));
				connect(button, &QToolButton::clicked, this, [this, id, path, choice] { static_cast<void>(applied(core::set_prompt_group_match(Editor, id, path, choice))); });
				head->addWidget(button);
			}
			head->addStretch(1);
			auto* const add = new QPushButton{ tr("Add"), box };
			add->setAccessibleName(tr("Add to this group"));
			auto* const menu = new QMenu{ add };
			add->setMenu(menu);
			connect(menu, &QMenu::aboutToShow, this, [this, menu, path] {
				menu->clear();
				fill_condition_menu(*menu, path);
			});
			head->addWidget(add);
			auto* const remove = new quiet_tool_button{ box };
			remove->setIcon(conversation_icon(conversation_glyph::trash, palette()));
			remove->setAutoRaise(true);
			remove->setObjectName(QStringLiteral("promptsRemoveGroup"));
			remove->setAccessibleName(tr("Remove this group"));
			remove->setToolTip(remove->accessibleName());
			connect(remove, &QToolButton::clicked, this, [this, id, aPath, at] {
				OpenConditions.clear();
				static_cast<void>(applied(core::remove_prompt_condition_group(Editor, id, aPath, at)));
			});
			head->addWidget(remove);
			inner->addLayout(head);
			if (group->Groups[at].Conditions.empty() && group->Groups[at].Groups.empty()) {
				inner->addWidget(secondary(tr("Empty: add a condition to it."), box));
			}
			add_group_rows(*inner, aBody, path);
			aColumn.addWidget(box);
		}
	}

	auto prompts_page::footer_text() const -> QString { return OnSurfaces->text(); }

	auto prompts_page::saved_text() const -> QString { return SavedLabel->text(); }

	auto prompts_page::last_refusal() const -> QString { return Refusal; }

	void prompts_page::fill_add_to(QMenu& aMenu, const std::string& aId)
	{
		aMenu.setToolTipsVisible(true);
		// The owner, 2026-11-15: "there's no information on where we're adding
		// it to". Each group is headed with the Surfaces section's name for
		// it. Not addSection: macOS shows a context menu as a native menu,
		// where a section is a bare divider and its title is dropped.
		const auto heading = [&aMenu](const QString& aTitle) {
			if (!aMenu.isEmpty()) {
				aMenu.addSeparator();
			}
			auto* const title = aMenu.addAction(aTitle);
			title->setObjectName(QStringLiteral("promptsAddToHeading"));
			title->setEnabled(false);
			auto font = title->font();
			font.setBold(true);
			title->setFont(font);
		};
		const auto group = [&](const QString& aTitle, const std::span<const prompt_surface> aSurfaces, const auto& aItemName) {
			heading(aTitle);
			auto reasons = QStringList{ };
			for (const auto surface : aSurfaces) {
				const auto& placed  = View.Surfaces.Lists.at(index(surface));
				const auto  on      = std::ranges::find(placed, aId) != placed.end();
				const auto  refused = refusal(aId, surface);
				auto* const action  = aMenu.addAction(aItemName(surface));
				action->setObjectName(QStringLiteral("promptsAddTo-%1").arg(QString::fromLatin1(core::stable_name(surface))));
				action->setCheckable(true);
				action->setChecked(on);
				action->setEnabled(on || refused == core::placement_refusal::none);
				if (refused != core::placement_refusal::none) {
					const auto reason = core::placement_reason(refused);
					action->setToolTip(QString::fromUtf8(reason.data(), static_cast<qsizetype>(reason.size())));
				}
				if (!action->isEnabled()) {
					reasons << action->toolTip();
				}
				connect(action, &QAction::triggered, this, [this, aId, surface, on] {
					if (on) {
						remove(aId, surface);
					} else {
						static_cast<void>(place(aId, surface));
					}
				});
			}
			// A group greyed throughout for one reason says it under its
			// items: a native menu may not show a disabled item's tooltip.
			const auto allGreyed = std::cmp_equal(reasons.size(), aSurfaces.size());
			reasons.removeDuplicates();
			if (allGreyed && reasons.size() == 1) {
				auto* const note = aMenu.addAction(reasons.front());
				note->setObjectName(QStringLiteral("promptsAddToNote"));
				note->setEnabled(false);
				auto font = note->font();
				font.setItalic(true);
				note->setFont(font);
			}
		};
		for (const auto& scope : scope_surfaces) {
			group(surface_panel_name(scope[0]), scope, surface_list_name);
		}
		group(tour_group_name(), tour_surfaces, surface_panel_name);
		group(surface_panel_name(prompt_surface::commits_menu), std::array{ prompt_surface::commits_menu }, surface_list_name);
	}

	void prompts_page::fill_library_menu(QMenu& aMenu, const std::string& aId)
	{
		const auto* const macro = find(aId);
		if (macro == nullptr) {
			return;
		}
		aMenu.addAction(tr("Edit"), this, [this, aId] {
			select(aId);
			Text->setFocus(Qt::OtherFocusReason);
		});
		fill_add_to(*aMenu.addMenu(tr("Add to")), aId);
		aMenu.addAction(tr("Duplicate"), this, [this, aId] { static_cast<void>(duplicate(aId)); });
		aMenu.addAction(macro->On ? tr("Turn Off") : tr("Turn On"), this, [this, aId, on = macro->On] { set_on(aId, !on); });
		if (macro->BuiltIn && core::prompt_standing_of(Editor, *macro).Edited) {
			aMenu.addAction(tr("Reset to Default"), this, [this, aId] { reset_to_default(aId); });
		}
		if (!macro->BuiltIn) {
			aMenu.addSeparator();
			aMenu.addAction(tr("Delete…"), this, [this, aId] { static_cast<void>(delete_prompt(aId, true)); });
		}
	}

	auto prompts_page::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() != QEvent::KeyPress) {
			return QWidget::eventFilter(aWatched, aEvent);
		}
		const auto* const key = static_cast<QKeyEvent*>(aEvent);
		if (aWatched == LibraryList) {
			if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) && Selected) {
				static_cast<void>(delete_prompt(*Selected, true));
				return true;
			}
			return QWidget::eventFilter(aWatched, aEvent);
		}
		for (auto* const list : Lists) {
			if (aWatched != list) {
				continue;
			}
			const auto  surface = static_cast<surface_list_view*>(list)->surface();
			const auto* item    = list->currentItem();
			if (item == nullptr) {
				return false;
			}
			const auto id = item->data(id_role).toString().toStdString();
			if ((key->modifiers() & Qt::AltModifier) != 0 && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
				move(id, surface, key->key() == Qt::Key_Up ? -1 : 1);
				return true;
			}
			if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) {
				remove(id, surface);
				return true;
			}
			if (key->key() == Qt::Key_Menu) {
				auto menu = QMenu{ this };
				fill_add_to(*menu.addMenu(tr("Add to")), id);
				menu.exec(list->viewport()->mapToGlobal(list->visualItemRect(item).bottomLeft()));
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
