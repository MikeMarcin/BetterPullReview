#include "bpr/qt/prompts_page.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/prompt_words.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"

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
#include <QHBoxLayout>
#include <QKeyEvent>
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
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
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

		[[nodiscard]] auto segmented(const QString& aText, QWidget* const aParent) -> QToolButton*
		{
			auto* const button = new QToolButton{ aParent };
			button->setText(aText);
			button->setCheckable(true);
			button->setAutoRaise(false);
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
		, Shipped{ core::shipped_prompt_macros() }
	{
		setObjectName(QStringLiteral("promptsPage"));
		setAccessibleName(tr("Prompts"));
		Projection.Set = core::merge_prompt_macros(Shipped, { });
		Library        = Projection.Set.macros();
		Surfaces       = Projection.Set.Surfaces;
		AppLibrary     = Library;
		AppSurfaces    = Surfaces;

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
		connect(Typing, &QTimer::timeout, this, [this] { flush(); });

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
		ToolsRow->setDirection(width() < 940 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
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
		LibraryList->setMaximumHeight(narrow ? 240 : 360);
		const auto editor = narrow ? width() : width() - 284;
		EditorColumns->setDirection(editor < 640 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
		RightBox->setMaximumWidth(editor < 640 ? QWIDGETSIZE_MAX : 328);
		RightBox->setMinimumWidth(editor < 640 ? 240 : 328);
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
		const auto scopeNames = std::array{ tr("Pull request"), tr("File"), tr("Thread") };
		for (std::size_t scope = 0U; scope < scope_surfaces.size(); ++scope) {
			auto [frame, layout] = panel(scopeNames.at(scope));
			addList(*layout, scope_surfaces.at(scope)[0], tr("Ask menu"), { });
			layout->addSpacing(4);
			addList(*layout, scope_surfaces.at(scope)[1], tr("Suggested chips"), tr("up to 5 show"));
			layout->addStretch(1);
			scopes->addWidget(frame, 1, Qt::AlignTop);
		}
		aColumn.addLayout(scopes);

		// The owner, 2026-11-14: the tour chats get surfaces under the same model.
		auto* const tours = new QHBoxLayout{ };
		tours->setSpacing(12);
		const auto tourNames = std::array{ tr("Tour stop"), tr("Tour part"), tr("Whole tour") };
		for (std::size_t tour = 0U; tour < tour_surfaces.size(); ++tour) {
			auto [frame, layout] = panel(tourNames.at(tour));
			addList(*layout, tour_surfaces.at(tour), tr("Suggested chips"), tr("up to 5 show"));
			layout->addStretch(1);
			tours->addWidget(frame, 1, Qt::AlignTop);
		}
		aColumn.addLayout(tours);

		auto* const legend = secondary(tr("◆ Has conditions. Its chip shows only while they hold; its menu row is greyed or hidden while they do not."), this);
		legend->setObjectName(QStringLiteral("promptsLegend"));
		aColumn.addWidget(legend);
	}

	void prompts_page::build_library(QVBoxLayout& aColumn)
	{
		aColumn.addSpacing(13);
		// One row as drawn; two in a narrow window, the filters under the title.
		ToolsRow = new QBoxLayout{ QBoxLayout::LeftToRight };
		ToolsRow->setSpacing(8);
		auto* const title = new QHBoxLayout{ };
		title->setSpacing(8);
		title->addWidget(heading(tr("Prompt library"), this));
		auto* const tools = new QHBoxLayout{ };
		tools->setSpacing(8);
		Search = new QLineEdit{ this };
		Search->setObjectName(QStringLiteral("promptsFilter"));
		Search->setPlaceholderText(tr("Filter prompts"));
		Search->setAccessibleName(tr("Filter prompts"));
		Search->setClearButtonEnabled(true);
		Search->setMaximumWidth(168);
		Search->setMinimumWidth(96);
		tools->addWidget(Search);
		Filters = new QButtonGroup{ this };
		Filters->setExclusive(true);
		auto* const segments = new QHBoxLayout{ };
		segments->setSpacing(0);
		const auto names = std::array{ tr("All"), tr("Built-in"), tr("Yours"), tr("Not on a surface"), tr("System") };
		for (std::size_t segment = 0U; segment < names.size(); ++segment) {
			auto* const button = segmented(names.at(segment), this);
			button->setObjectName(QStringLiteral("promptsFilter-%1").arg(segment));
			Filters->addButton(button, static_cast<int>(segment));
			segments->addWidget(button);
		}
		Filters->button(0)->setChecked(true);
		tools->addLayout(segments);
		ChangedHere = new QCheckBox{ this };
		ChangedHere->setObjectName(QStringLiteral("promptsChangedHere"));
		ChangedHere->hide();
		connect(ChangedHere, &QCheckBox::toggled, this, [this](const bool aOn) { set_changed_here(aOn); });
		tools->addWidget(ChangedHere);
		tools->addStretch(1);
		auto* const restore = new quiet_push_button{ tr("Restore built-ins\u2026"), this };
		restore->setObjectName(QStringLiteral("promptsRestore"));
		connect(restore, &QPushButton::clicked, this, [this] { static_cast<void>(restore_built_ins(true)); });
		auto* const add = new QPushButton{ tr("New prompt"), this };
		add->setObjectName(QStringLiteral("promptsNew"));
		ToolsRow->addLayout(title, 0);
		ToolsRow->addLayout(tools, 1);
		auto* const buttons = new QHBoxLayout{ };
		buttons->addStretch(1);
		buttons->addWidget(restore);
		buttons->addWidget(add);
		ToolsRow->addLayout(buttons, 0);
		aColumn.addLayout(ToolsRow);
		aColumn.addSpacing(9 - 7);
		connect(Search, &QLineEdit::textChanged, this, [this] { rebuild_library(); });
		connect(Filters, &QButtonGroup::idClicked, this, [this](const int aId) { set_filter(static_cast<prompt_library_filter>(aId)); });
		connect(add, &QPushButton::clicked, this, [this] { static_cast<void>(new_prompt()); });

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
		editor->setObjectName(QStringLiteral("promptsEditor"));
		editor->setFrameShape(QFrame::StyledPanel);
		editor->setAutoFillBackground(true);
		editor->setBackgroundRole(QPalette::AlternateBase);
		editor->setMinimumHeight(360);
		editor->setMaximumHeight(360);
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
		left->addWidget(Text, 1);
		InsertRow                = new QWidget{ page };
		auto* const insertLayout = new QHBoxLayout{ InsertRow };
		insertLayout->setContentsMargins(0, 0, 0, 0);
		insertLayout->setSpacing(4);
		left->addWidget(InsertRow);
		columns->addLayout(left, 1);

		auto* const right = new QVBoxLayout{ };
		right->setSpacing(4);
		// The heading and its add button on one row, the list's match under
		// them, so the add button is never cut at the column's edge.
		auto* const conditionsHeader = new QHBoxLayout{ };
		conditionsHeader->addWidget(heading(tr("Conditions"), page));
		conditionsHeader->addStretch(1);
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
		matchRow->addWidget(MatchNone);
		matchRow->addStretch(1);
		right->addLayout(matchRow);
		// Nested groups can outgrow the editor's height: the conditions
		// scroll on their own rather than squeeze.
		auto* const conditionsScroll = new QScrollArea{ page };
		conditionsScroll->setObjectName(QStringLiteral("promptsConditionsScroll"));
		conditionsScroll->setFrameShape(QFrame::NoFrame);
		conditionsScroll->setWidgetResizable(true);
		conditionsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const conditionsBox = new QWidget{ conditionsScroll };
		conditionsBox->setAutoFillBackground(false);
		conditionsScroll->viewport()->setAutoFillBackground(false);
		auto* const conditionsColumn = new QVBoxLayout{ conditionsBox };
		conditionsColumn->setContentsMargins(0, 0, 0, 0);
		conditionsColumn->setSpacing(0);
		auto* const conditionsList = new QWidget{ conditionsBox };
		Conditions                 = new QVBoxLayout{ conditionsList };
		Conditions->setContentsMargins(0, 0, 0, 0);
		Conditions->setSpacing(2);
		conditionsColumn->addWidget(conditionsList);
		conditionsColumn->addStretch(1);
		conditionsScroll->setWidget(conditionsBox);
		right->addWidget(conditionsScroll, 1);
		Problems = secondary({ }, page);
		Problems->setObjectName(QStringLiteral("promptsProblems"));
		right->addWidget(Problems);
		auto* const rightBox = new QWidget{ page };
		rightBox->setLayout(right);
		rightBox->setMinimumWidth(240);
		rightBox->setMaximumWidth(328);
		rightBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		RightBox = rightBox;
		columns->addWidget(rightBox);
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
				edit_selected([match](core::prompt_macro& aMacro) { aMacro.Body.Match = match; });
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
			if (Repository.empty()) {
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
				rebuild_library();
			}
		});
		rebuild_library();
	}

	void prompts_page::present(const application::prompt_macro_projection& aProjection)
	{
		Projection = aProjection;
		InFlight   = false;
		if (!Dirty) {
			AppLibrary  = aProjection.Set.macros();
			AppSurfaces = aProjection.Set.Surfaces;
			Layers      = aProjection.Set.Repositories;
			adopt_layer();
			set_repositories(Repositories);
			if (Selected && find(*Selected) == nullptr) {
				Selected.reset();
			}
			rebuild_surfaces();
			rebuild_library();
			refresh_editor();
		}
		if (aProjection.LastWrite == application::prompt_macro_write::failed) {
			Status->setText(tr("Your prompts couldn't be saved. The last change is kept here; it saves with the next one."));
		} else if (aProjection.LastWrite == application::prompt_macro_write::stale) {
			Status->setText(tr("Another window changed your prompts first; this page shows theirs."));
		} else if (!aProjection.Readable && aProjection.Loaded) {
			Status->setText(tr("Your prompts can't be read, so the built-ins are shown and changes aren't saved."));
		} else {
			Status->clear();
		}
		if (Dirty && !Typing->isActive()) {
			flush();
		}
		SavedLabel->setText(Dirty || InFlight ? tr("Saving…") : tr("Saved"));
	}

	void prompts_page::set_save_dispatch(save_dispatch aDispatch) { Save = std::move(aDispatch); }

	void prompts_page::set_repositories(const QStringList& aRepositories)
	{
		Repositories = aRepositories;
		auto listed  = aRepositories;
		for (const auto& layer : Layers) {
			if (!listed.contains(QString::fromStdString(layer.Repository), Qt::CaseInsensitive)) {
				listed << QString::fromStdString(layer.Repository);
			}
		}
		if (!Repository.empty() && !listed.contains(QString::fromStdString(Repository), Qt::CaseInsensitive)) {
			listed << QString::fromStdString(Repository);
		}
		listed.sort(Qt::CaseInsensitive);
		const auto blocker = QSignalBlocker{ PromptsFor };
		PromptsFor->clear();
		PromptsFor->addItem(tr("All repositories"));
		PromptsFor->addItems(listed);
		PromptsFor->setCurrentIndex(Repository.empty() ? 0 : std::max(0, PromptsFor->findText(QString::fromStdString(Repository))));
	}

	void prompts_page::choose_repository(const std::string& aRepository)
	{
		if (aRepository == Repository) {
			return;
		}
		Typing->stop();
		flush();
		Repository = aRepository;
		set_repositories(Repositories);
		adopt_layer();
		if (Selected && find(*Selected) == nullptr) {
			Selected.reset();
		}
		SelectedPlacement.reset();
		// A teal edge says the edits are this repository's.
		const auto teal = override_colour(palette());
		RepositoryMark->setVisible(!Repository.empty());
		auto mark = RepositoryMark->palette();
		mark.setColor(QPalette::WindowText, teal);
		RepositoryMark->setPalette(mark);
		Caption->setText(Repository.empty() ? tr("Where prompts appear, in order. Drag a prompt up from the library, or right-click it and choose Add to.")
											: tr("%1 starts from your prompts for all repositories. Changes here apply to its pull requests only.").arg(QString::fromStdString(Repository)));
		ChangedHere->setVisible(!Repository.empty());
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
	}

	void prompts_page::adopt_layer()
	{
		const auto layer = std::ranges::find(Layers, Repository, &core::prompt_repository_layer::Repository);
		if (Repository.empty() || layer == Layers.end()) {
			Library  = AppLibrary;
			Surfaces = AppSurfaces;
			return;
		}
		auto layered = core::apply_repository_layer(AppLibrary, AppSurfaces, *layer);
		Library      = std::move(layered.Library);
		Surfaces     = std::move(layered.Surfaces);
	}

	auto prompts_page::app_entry(const std::string& aId) const -> const core::prompt_macro*
	{
		const auto found = std::ranges::find(AppLibrary, aId, &core::prompt_macro::Id);
		return found == AppLibrary.end() ? nullptr : &*found;
	}

	auto prompts_page::differs_here(const core::prompt_macro& aMacro) const -> bool
	{
		if (Repository.empty()) {
			return false;
		}
		const auto* const app = app_entry(aMacro.Id);
		return app == nullptr || app->Body != aMacro.Body || app->On != aMacro.On;
	}

	auto prompts_page::here_count() const -> int
	{
		return static_cast<int>(std::ranges::count_if(Library, [this](const core::prompt_macro& aMacro) { return differs_here(aMacro); }));
	}

	void prompts_page::use_app_wide(const std::string& aId)
	{
		auto* const       macro = find(aId);
		const auto* const app   = app_entry(aId);
		if (macro == nullptr || app == nullptr || Repository.empty() || (macro->Body == app->Body && macro->On == app->On)) {
			return;
		}
		macro->Body = app->Body;
		macro->On   = app->On;
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	void prompts_page::use_app_wide_field(const std::string& aId, const QString& aField)
	{
		auto* const       macro = find(aId);
		const auto* const app   = app_entry(aId);
		if (macro == nullptr || app == nullptr || Repository.empty()) {
			return;
		}
		if (aField == QStringLiteral("name")) {
			macro->Body.Name = app->Body.Name;
		} else if (aField == QStringLiteral("text")) {
			macro->Body.Template = app->Body.Template;
		} else if (aField == QStringLiteral("answer")) {
			macro->Body.Kind = app->Body.Kind;
		} else if (aField == QStringLiteral("conditions")) {
			macro->Body.Conditions = app->Body.Conditions;
			macro->Body.Match      = app->Body.Match;
		} else if (aField == QStringLiteral("on")) {
			macro->On = app->On;
		}
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	void prompts_page::revert_placement(const std::string& aId, const prompt_surface aSurface)
	{
		if (Repository.empty()) {
			return;
		}
		const auto& app  = AppSurfaces.Lists.at(index(aSurface));
		auto&       here = Surfaces.Lists.at(index(aSurface));
		const auto  was  = std::ranges::find(app, aId);
		if (was == app.end()) {
			std::erase(here, aId);
		} else if (std::ranges::find(here, aId) == here.end()) {
			// Back where it is app-wide, after the placements before it there.
			auto at = std::size_t{ 0 };
			for (auto before = app.begin(); before != was; ++before) {
				if (const auto found = std::ranges::find(here, *before); found != here.end()) {
					at = static_cast<std::size_t>(found - here.begin()) + 1U;
				}
			}
			here.insert(here.begin() + static_cast<std::ptrdiff_t>(std::min(at, here.size())), aId);
		}
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	void prompts_page::revert_order(const prompt_surface aSurface)
	{
		const auto layer = std::ranges::find(Layers, Repository, &core::prompt_repository_layer::Repository);
		if (Repository.empty() || layer == Layers.end()) {
			return;
		}
		auto change = std::ranges::find(layer->Surfaces, aSurface, &core::prompt_repository_surface::Surface);
		if (change == layer->Surfaces.end() || !change->Order) {
			return;
		}
		auto unordered                     = *layer;
		auto reverted                      = std::ranges::find(unordered.Surfaces, aSurface, &core::prompt_repository_surface::Surface);
		reverted->Order                    = std::nullopt;
		Surfaces.Lists.at(index(aSurface)) = core::apply_repository_layer(AppLibrary, AppSurfaces, unordered).Surfaces.Lists.at(index(aSurface));
		rebuild_surfaces();
		changed();
	}

	void prompts_page::set_changed_here(const bool aOnly)
	{
		ChangedHereOnly = aOnly;
		if (ChangedHere->isChecked() != aOnly) {
			const auto blocker = QSignalBlocker{ ChangedHere };
			ChangedHere->setChecked(aOnly);
		}
		rebuild_library();
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

	void prompts_page::changed(const bool aTyping)
	{
		// What is edited goes to its layer: app-wide, or this repository's.
		if (Repository.empty()) {
			AppLibrary  = Library;
			AppSurfaces = Surfaces;
		} else {
			auto layer = core::repository_layer_of(AppLibrary, AppSurfaces, { .Library = Library, .Surfaces = Surfaces }, Repository);
			std::erase_if(Layers, [this](const core::prompt_repository_layer& aLayer) { return aLayer.Repository == Repository; });
			if (layer != core::prompt_repository_layer{ .Repository = Repository }) {
				Layers.push_back(std::move(layer));
				std::ranges::sort(Layers, { }, &core::prompt_repository_layer::Repository);
			}
		}
		Dirty = true;
		SavedLabel->setText(tr("Saving…"));
		if (aTyping) {
			Typing->start();
			return;
		}
		Typing->stop();
		flush();
	}

	void prompts_page::flush()
	{
		// One write at a time, each from the revision the last one made.
		if (!Dirty || InFlight || !Save) {
			return;
		}
		Dirty    = false;
		InFlight = true;
		Save(AppLibrary, AppSurfaces, Layers);
	}

	auto prompts_page::find(const std::string& aId) -> core::prompt_macro*
	{
		const auto found = std::ranges::find(Library, aId, &core::prompt_macro::Id);
		return found == Library.end() ? nullptr : &*found;
	}

	auto prompts_page::find(const std::string& aId) const -> const core::prompt_macro*
	{
		const auto found = std::ranges::find(Library, aId, &core::prompt_macro::Id);
		return found == Library.end() ? nullptr : &*found;
	}

	auto prompts_page::shipped(const std::string& aId) const -> const core::prompt_macro*
	{
		const auto found = std::ranges::find_if(Shipped, [&aId](const core::prompt_builtin& aBuiltin) { return aBuiltin.Macro.Id == aId; });
		return found == Shipped.end() ? nullptr : &found->Macro;
	}

	auto prompts_page::differs_from_shipped(const core::prompt_macro& aMacro) const -> bool
	{
		const auto* const original = shipped(aMacro.Id);
		return original != nullptr && original->Body != aMacro.Body;
	}

	auto prompts_page::refusal(const std::string& aId, const prompt_surface aSurface) const -> core::placement_refusal
	{
		const auto* const macro = find(aId);
		return macro == nullptr ? core::placement_refusal::unknown_prompt : core::placement_refusal_of(*macro, aSurface);
	}

	auto prompts_page::place(const std::string& aId, const prompt_surface aSurface, const std::optional<std::size_t> aAt) -> core::placement_refusal
	{
		const auto refused = core::place(Surfaces, Library, aId, aSurface, aAt);
		if (refused != core::placement_refusal::none) {
			Refusal = QString::fromUtf8(core::placement_reason(refused).data(), static_cast<qsizetype>(core::placement_reason(refused).size()));
			Status->setText(Refusal);
			return refused;
		}
		Refusal.clear();
		Selected = aId;
		SelectedSystem.reset();
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
		return refused;
	}

	auto prompts_page::drop(const std::string& aId, const std::optional<prompt_surface> aFrom, const prompt_surface aTo, std::size_t aAt, const bool aCopy) -> core::placement_refusal
	{
		if (aFrom && *aFrom == aTo) {
			// Within a surface: the drop line counts the row being moved.
			const auto& list = Surfaces.Lists.at(index(aTo));
			if (const auto found = std::ranges::find(list, aId); found != list.end() && static_cast<std::size_t>(found - list.begin()) < aAt) {
				--aAt;
			}
		}
		const auto refused = core::place(Surfaces, Library, aId, aTo, aAt);
		if (refused != core::placement_refusal::none) {
			Refusal = QString::fromUtf8(core::placement_reason(refused).data(), static_cast<qsizetype>(core::placement_reason(refused).size()));
			Status->setText(Refusal);
			return refused;
		}
		if (aFrom && *aFrom != aTo && !aCopy) {
			static_cast<void>(core::remove_placement(Surfaces, aId, *aFrom));
		}
		Selected = aId;
		SelectedSystem.reset();
		SelectedPlacement.reset();
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
		return refused;
	}

	void prompts_page::remove(const std::string& aId, const prompt_surface aSurface)
	{
		if (core::remove_placement(Surfaces, aId, aSurface) != core::placement_refusal::none) {
			return;
		}
		SelectedPlacement.reset();
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	void prompts_page::move(const std::string& aId, const prompt_surface aSurface, const int aBy)
	{
		if (core::move_placement(Surfaces, aId, aSurface, aBy) != core::placement_refusal::none) {
			return;
		}
		const auto& list  = Surfaces.Lists.at(index(aSurface));
		const auto  found = std::ranges::find(list, aId);
		SelectedPlacement = std::pair{ aSurface, static_cast<std::size_t>(found - list.begin()) };
		rebuild_surfaces();
		changed();
		Lists.at(index(aSurface))->setFocus(Qt::OtherFocusReason);
	}

	void prompts_page::set_on(const std::string& aId, const bool aOn)
	{
		auto* const macro = find(aId);
		if (macro == nullptr || macro->On == aOn) {
			return;
		}
		macro->On = aOn;
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	auto prompts_page::new_prompt() -> std::string
	{
		auto macro      = core::prompt_macro{ .Id = "own." + QUuid::createUuid().toString(QUuid::Id128).toStdString(), .Place = core::prompt_place::agent_tab };
		macro.Body.Name = tr("New prompt").toStdString();
		Library.push_back(macro);
		Selected = macro.Id;
		SelectedSystem.reset();
		set_filter(prompt_library_filter::all);
		rebuild_surfaces();
		refresh_editor();
		changed();
		Name->setFocus(Qt::OtherFocusReason);
		Name->selectAll();
		return macro.Id;
	}

	auto prompts_page::duplicate(const std::string& aId) -> std::optional<std::string>
	{
		const auto* const original = find(aId);
		if (original == nullptr) {
			return std::nullopt;
		}
		auto copy      = *original;
		copy.Id        = "own." + QUuid::createUuid().toString(QUuid::Id128).toStdString();
		copy.BuiltIn   = false;
		copy.Action    = core::prompt_action::none;
		copy.Surface   = std::nullopt;
		copy.Body.Name = tr("%1 copy").arg(QString::fromStdString(original->Body.Name)).toStdString();
		if (copy.Body.Name.size() > core::maximum_prompt_name_bytes) {
			copy.Body.Name.resize(core::maximum_prompt_name_bytes);
		}
		const auto id = copy.Id;
		Library.push_back(std::move(copy));
		Selected = id;
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
		Name->setFocus(Qt::OtherFocusReason);
		Name->selectAll();
		return id;
	}

	void prompts_page::reset_to_default(const std::string& aId)
	{
		auto* const       macro    = find(aId);
		const auto* const original = shipped(aId);
		if (macro == nullptr || original == nullptr || macro->Body == original->Body) {
			return;
		}
		macro->Body = original->Body;
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
	}

	auto prompts_page::delete_prompt(const std::string& aId, const bool aConfirm) -> bool
	{
		const auto* const macro = find(aId);
		// In a repository only its own can be deleted; an app-wide prompt is
		// turned off there.
		if (macro == nullptr || macro->BuiltIn || (!Repository.empty() && app_entry(aId) != nullptr)) {
			return false;
		}
		if (aConfirm) {
			const auto on    = core::surfaces_of(Surfaces, aId);
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
		for (auto& list : Surfaces.Lists) {
			std::erase(list, aId);
		}
		std::erase_if(Library, [&aId](const core::prompt_macro& aMacro) { return aMacro.Id == aId; });
		Selected.reset();
		SelectedPlacement.reset();
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
		return true;
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
		auto builtIns = std::vector<core::prompt_macro>{ };
		for (const auto& builtin : Shipped) {
			builtIns.push_back(builtin.Macro);
			if (auto* const macro = find(builtin.Macro.Id); macro != nullptr) {
				macro->Body = builtin.Macro.Body;
				macro->On   = builtin.Macro.On;
			}
		}
		auto restored = core::shipped_surfaces(builtIns);
		for (std::size_t surface = 0U; surface < core::prompt_surface_count; ++surface) {
			for (const auto& id : Surfaces.Lists.at(surface)) {
				const auto* const macro = find(id);
				if (macro != nullptr && !macro->BuiltIn) {
					restored.Lists.at(surface).push_back(id);
				}
			}
		}
		Surfaces = std::move(restored);
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed();
		return true;
	}

	void prompts_page::set_filter(const prompt_library_filter aFilter)
	{
		Filter = aFilter;
		if (auto* const button = Filters->button(static_cast<int>(aFilter)); button != nullptr) {
			button->setChecked(true);
		}
		rebuild_library();
	}

	void prompts_page::edit_selected(const std::function<void(core::prompt_macro&)>& aEdit)
	{
		if (Filling || !Selected) {
			return;
		}
		auto* const macro = find(*Selected);
		if (macro == nullptr) {
			return;
		}
		const auto before = *macro;
		aEdit(*macro);
		if (*macro == before) {
			return;
		}
		const auto typing = macro->Body.Name != before.Body.Name || macro->Body.Template != before.Body.Template;
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
		changed(typing);
	}

	void prompts_page::select(const std::string& aId)
	{
		if (find(aId) == nullptr) {
			return;
		}
		Selected = aId;
		SelectedSystem.reset();
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
	}

	void prompts_page::select_system(const int aKey)
	{
		Selected.reset();
		SelectedPlacement.reset();
		SelectedSystem = aKey;
		if (System != nullptr) {
			System->select_system(aKey);
		}
		rebuild_surfaces();
		rebuild_library();
		refresh_editor();
	}

	auto prompts_page::selected() const -> std::optional<std::string> { return Selected; }

	auto prompts_page::shown_in_library(const core::prompt_macro& aMacro) const -> bool
	{
		const auto needle = Search->text().trimmed();
		if (!needle.isEmpty() && !QString::fromStdString(aMacro.Body.Name).contains(needle, Qt::CaseInsensitive)
			&& !QString::fromStdString(aMacro.Body.Template).contains(needle, Qt::CaseInsensitive)) {
			return false;
		}
		if (ChangedHereOnly && !Repository.empty() && !differs_here(aMacro)) {
			return false;
		}
		switch (Filter) {
		case prompt_library_filter::all:              return true;
		case prompt_library_filter::built_in:         return aMacro.BuiltIn;
		case prompt_library_filter::yours:            return !aMacro.BuiltIn;
		case prompt_library_filter::not_on_a_surface: return core::surfaces_of(Surfaces, aMacro.Id).empty();
		case prompt_library_filter::system:           return false;
		}
		return true;
	}

	void prompts_page::rebuild_library()
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
		for (const auto& macro : Library) {
			if (!shown_in_library(macro)) {
				continue;
			}
			any       = true;
			auto tags = QStringList{ };
			if (!Repository.empty() && app_entry(macro.Id) == nullptr) {
				tags << tr("Only here");
			} else if (!macro.BuiltIn) {
				tags << tr("Yours");
			} else if (differs_from_shipped(macro)) {
				tags << tr("Edited");
			}
			if (const auto* const app = Repository.empty() ? nullptr : app_entry(macro.Id); app != nullptr) {
				if (app->On != macro.On && !macro.On) {
					tags << tr("Off here");
				}
				if (app->Body != macro.Body) {
					tags << tr("Changed here");
				}
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
		if (!any) {
			auto* const item = new QListWidgetItem{ tr("No prompts match."), LibraryList };
			item->setFlags(Qt::NoItemFlags);
		}
		ChangedHere->setText(tr("Changed here (%1)").arg(here_count()));
		if (current != nullptr) {
			LibraryList->setCurrentItem(current);
		} else {
			LibraryList->setCurrentRow(-1);
		}
		Filling = false;
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
			const auto& ids = Surfaces.Lists.at(surface);
			for (std::size_t row = 0U; row < ids.size(); ++row) {
				const auto* const macro = find(ids[row]);
				if (macro == nullptr) {
					continue;
				}
				auto flags = 0;
				if (!macro->Body.Conditions.empty()) {
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
				const auto& app   = AppSurfaces.Lists.at(surface);
				const auto  added = !Repository.empty() && std::ranges::find(app, macro->Id) == app.end();
				auto        tag   = macro->On ? QString{ } : tr("Off");
				if (added) {
					tag = tr("Added here");
					flags |= revertable;
				} else if (const auto* const appMacro = Repository.empty() ? nullptr : app_entry(macro->Id); appMacro != nullptr && appMacro->On != macro->On && !macro->On) {
					tag = tr("Off here");
					flags |= is_override;
				}
				if (differs_here(*macro) && !added) {
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
			if (!Repository.empty()) {
				for (const auto& id : AppSurfaces.Lists.at(surface)) {
					const auto* const macro = find(id);
					if (macro == nullptr || std::ranges::find(ids, id) != ids.end()) {
						continue;
					}
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
				const auto layer   = std::ranges::find(Layers, Repository, &core::prompt_repository_layer::Repository);
				auto       ordered = false;
				if (!Repository.empty() && layer != Layers.end()) {
					const auto change = std::ranges::find(layer->Surfaces, static_cast<prompt_surface>(surface), &core::prompt_repository_surface::Surface);
					ordered           = change != layer->Surfaces.end() && change->Order.has_value();
				}
				order->setVisible(ordered);
				order->setText(tr("<span style=\"color:%1\">Order changed here</span> <a href=\"revert\">Revert</a>").arg(override_colour(palette()).name()));
			}
			list->fit();
		}
		Filling = false;
	}

	auto prompts_page::placement_name(const prompt_surface aSurface, const std::size_t aIndex) const -> QString
	{
		const auto& ids = Surfaces.Lists.at(index(aSurface));
		if (aIndex >= ids.size()) {
			return { };
		}
		const auto* const macro = find(ids[aIndex]);
		auto              name  = tr("%1, %2 of %3, %4").arg(prompt_surface_name(aSurface)).arg(aIndex + 1U).arg(ids.size()).arg(macro != nullptr ? QString::fromStdString(macro->Body.Name) : QString{ });
		if (macro != nullptr && !macro->Body.Conditions.empty()) {
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
			EditorPages->parentWidget()->setMaximumHeight(aShown ? QWIDGETSIZE_MAX : 360);
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
		BuiltInTag->setText(macro->BuiltIn ? (differs_from_shipped(*macro) ? tr("Built-in, edited") : tr("Built-in")) : tr("Yours"));
		AsQuestion->setChecked(macro->Body.Kind == core::prompt_ask::question);
		AsFindings->setChecked(macro->Body.Kind == core::prompt_ask::review);
		MatchAll->setChecked(macro->Body.Match == core::prompt_match::all);
		MatchAny->setChecked(macro->Body.Match == core::prompt_match::any);
		MatchNone->setChecked(macro->Body.Match == core::prompt_match::none);
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
		const auto  on           = core::surfaces_of(Surfaces, macro->Id);
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

		AddCondition->setEnabled(macro->Body.Conditions.size() < core::maximum_prompt_conditions || macro->Body.Groups.size() < core::maximum_prompt_conditions);
		AddCondition->setToolTip(macro->Body.Conditions.size() < core::maximum_prompt_conditions ? tr("Add a condition or a group") : tr("A list holds four conditions; put more in a group."));
		rebuild_conditions();

		// Problems, as the prompt reads where it is placed.
		auto problems = QStringList{ };
		for (const auto surface : on) {
			for (const auto& problem : core::validate_prompt_macro(core::placed_on(*macro, surface))) {
				if (core::prompt_problem_blocks(problem.Problem)) {
					problems << tr("On %1 it isn't offered: %2.").arg(prompt_surface_name(surface), QString::fromLatin1(core::stable_name(problem.Problem)));
					break;
				}
			}
		}
		Problems->setText(problems.join(QLatin1Char('\n')));
		Duplicate->setEnabled(true);
		if (Repository.empty()) {
			Reset->setText(tr("Reset to default"));
			Reset->setVisible(macro->BuiltIn);
			Reset->setEnabled(macro->BuiltIn && differs_from_shipped(*macro));
			Differs->hide();
		} else {
			const auto* const app = app_entry(macro->Id);
			Reset->setText(tr("Use app-wide for this prompt"));
			Reset->setVisible(app != nullptr);
			Reset->setEnabled(app != nullptr && differs_here(*macro));
			auto fields = QStringList{ };
			if (app != nullptr) {
				const auto link = [](const QString& aField, const QString& aWords) { return QStringLiteral("<a href=\"%1\">%2</a>").arg(aField, aWords); };
				if (app->Body.Name != macro->Body.Name) {
					fields << link(QStringLiteral("name"), tr("Use app-wide name"));
				}
				if (app->Body.Template != macro->Body.Template) {
					fields << link(QStringLiteral("text"), tr("Use app-wide text"));
				}
				if (app->Body.Kind != macro->Body.Kind) {
					fields << link(QStringLiteral("answer"), tr("Use app-wide Answer as"));
				}
				if (app->Body.Conditions != macro->Body.Conditions || app->Body.Match != macro->Body.Match) {
					fields << link(QStringLiteral("conditions"), tr("Use app-wide conditions"));
				}
				if (app->On != macro->On) {
					fields << link(QStringLiteral("on"), app->On ? tr("Turn on, as app-wide") : tr("Turn off, as app-wide"));
				}
			}
			Differs->setVisible(!fields.isEmpty() || app == nullptr);
			Differs->setText(app == nullptr ? tr("<span style=\"color:%1\">Only here.</span> Delete it to remove it.").arg(override_colour(palette()).name())
											: tr("<span style=\"color:%1\">Differs from app-wide.</span> %2").arg(override_colour(palette()).name(), fields.join(QStringLiteral(" \u00B7 "))));
		}
		OnSurfaces->setText(on.empty() ? tr("On no surface") : on.size() == 1U ? tr("On 1 surface, outlined above")
																			   : tr("On %1 surfaces, outlined above").arg(on.size()));
		SavedLabel->setText(Dirty || InFlight ? tr("Saving…") : tr("Saved"));
		Filling = false;
	}

	namespace {
		/// One group of a prompt's tree, the list itself included.
		struct group_ref
		{
			core::prompt_match*                        Match      = nullptr;
			std::vector<core::prompt_condition>*       Conditions = nullptr;
			std::vector<core::prompt_condition_group>* Groups     = nullptr;
		};

		/// The group at `aPath` (indices into each level's groups); none when
		/// the path no longer names one.
		[[nodiscard]] auto group_at(core::prompt_macro_body& aBody, const std::vector<std::size_t>& aPath) -> std::optional<group_ref>
		{
			auto ref = group_ref{ .Match = &aBody.Match, .Conditions = &aBody.Conditions, .Groups = &aBody.Groups };
			for (const auto at : aPath) {
				if (at >= ref.Groups->size()) {
					return std::nullopt;
				}
				auto& group = (*ref.Groups)[at];
				ref         = { .Match = &group.Match, .Conditions = &group.Conditions, .Groups = &group.Groups };
			}
			return ref;
		}

		[[nodiscard]] auto leaf_count(const std::vector<core::prompt_condition>& aConditions, const std::vector<core::prompt_condition_group>& aGroups) -> std::size_t
		{
			auto count = aConditions.size();
			for (const auto& group : aGroups) {
				count += leaf_count(group.Conditions, group.Groups);
			}
			return count;
		}

		[[nodiscard]] auto path_key(const std::vector<std::size_t>& aPath, const std::optional<std::size_t> aLeaf = std::nullopt) -> QString
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

	void prompts_page::fill_condition_menu(QMenu& aMenu, const std::vector<std::size_t>& aPath)
	{
		const auto* const macro = Selected ? find(*Selected) : nullptr;
		if (macro == nullptr) {
			return;
		}
		auto       body  = macro->Body;
		const auto group = group_at(body, aPath);
		if (!group) {
			return;
		}
		const auto full    = leaf_count(body.Conditions, body.Groups) >= core::maximum_prompt_condition_leaves;
		const auto crowded = group->Conditions->size() >= core::maximum_prompt_conditions;
		for (const auto& name : { tr("Pull request"), tr("File"), tr("Thread"), tr("Chat") }) {
			aMenu.addSection(name);
			for (std::size_t fact = 0U; fact < core::prompt_fact_count; ++fact) {
				const auto value = static_cast<core::prompt_fact>(fact);
				if (prompt_fact_group(value) != name) {
					continue;
				}
				auto* const action = aMenu.addAction(prompt_fact_name(value), this, [this, value, aPath] {
					edit_selected([this, value, aPath](core::prompt_macro& aMacro) {
						const auto into = group_at(aMacro.Body, aPath);
						if (!into || into->Conditions->size() >= core::maximum_prompt_conditions || leaf_count(aMacro.Body.Conditions, aMacro.Body.Groups) >= core::maximum_prompt_condition_leaves) {
							return;
						}
						auto condition = core::prompt_condition{ .Fact = value };
						if (value == core::prompt_fact::path || value == core::prompt_fact::repository) {
							condition.Patterns = { value == core::prompt_fact::path ? "src/**" : "owner/*" };
						}
						into->Conditions->push_back(std::move(condition));
						// A row just added is open.
						OpenConditions.insert(path_key(aPath, into->Conditions->size() - 1U));
					});
				});
				action->setEnabled(!full && !crowded);
			}
		}
		// Look prompts-v001: groups nest, as the owner's tag queries do.
		aMenu.addSection(tr("Group"));
		for (const auto match : { core::prompt_match::all, core::prompt_match::any, core::prompt_match::none }) {
			auto* const action = aMenu.addAction(tr("Group: %1").arg(match_words(match)), this, [this, match, aPath] {
				edit_selected([match, aPath](core::prompt_macro& aMacro) {
					if (const auto into = group_at(aMacro.Body, aPath); into && into->Groups->size() < core::maximum_prompt_conditions && aPath.size() + 1U < core::maximum_prompt_condition_depth) {
						into->Groups->push_back({ .Match = match });
					}
				});
			});
			action->setEnabled(aPath.size() + 1U < core::maximum_prompt_condition_depth && group->Groups->size() < core::maximum_prompt_conditions);
		}
		aMenu.setToolTipsVisible(true);
		if (full) {
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
		if (macro->Body.Conditions.empty() && macro->Body.Groups.empty()) {
			Conditions->addWidget(secondary(tr("None: it is offered wherever it is placed."), Conditions->parentWidget()));
		}
		auto body = macro->Body;
		add_group_rows(*Conditions, body, { });
	}

	void prompts_page::add_group_rows(QVBoxLayout& aColumn, core::prompt_macro_body& aBody, const std::vector<std::size_t>& aPath)
	{
		const auto group = group_at(aBody, aPath);
		if (!group) {
			return;
		}
		auto* const parent = aColumn.parentWidget() != nullptr ? aColumn.parentWidget() : Conditions->parentWidget();
		for (std::size_t at = 0U; at < group->Conditions->size(); ++at) {
			const auto& condition = (*group->Conditions)[at];
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
			auto* const kind = secondary(core::prompt_fact_kind(condition.Fact) == core::prompt_condition_kind::hides ? tr("hides") : tr("greys"), row);
			kind->setWordWrap(false);
			line->addWidget(kind);
			auto* const removeButton = new quiet_tool_button{ row };
			removeButton->setText(QStringLiteral("−"));
			removeButton->setAutoRaise(true);
			removeButton->setAccessibleName(tr("Remove %1").arg(prompt_fact_name(condition.Fact)));
			line->addWidget(removeButton);
			column->addLayout(line);
			connect(disclosure, &QToolButton::clicked, this, [this, key] {
				if (!OpenConditions.remove(key)) {
					OpenConditions.insert(key);
				}
				rebuild_conditions();
			});
			connect(removeButton, &QToolButton::clicked, this, [this, aPath, at] {
				OpenConditions.clear();
				edit_selected([aPath, at](core::prompt_macro& aMacro) {
					if (const auto from = group_at(aMacro.Body, aPath); from && at < from->Conditions->size()) {
						from->Conditions->erase(from->Conditions->begin() + static_cast<std::ptrdiff_t>(at));
					}
				});
			});
			const auto edit = [this, aPath, at](const std::function<void(core::prompt_condition&)>& aChange) {
				edit_selected([&aChange, aPath, at](core::prompt_macro& aMacro) {
					if (const auto in = group_at(aMacro.Body, aPath); in && at < in->Conditions->size()) {
						aChange((*in->Conditions)[at]);
					}
				});
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
		for (std::size_t at = 0U; at < group->Groups->size(); ++at) {
			auto path = aPath;
			path.push_back(at);
			const auto  match = (*group->Groups)[at].Match;
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
				connect(button, &QToolButton::clicked, this, [this, path, choice] {
					edit_selected([path, choice](core::prompt_macro& aMacro) {
						if (const auto in = group_at(aMacro.Body, path)) {
							*in->Match = choice;
						}
					});
				});
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
			remove->setText(QStringLiteral("−"));
			remove->setAutoRaise(true);
			remove->setAccessibleName(tr("Remove this group"));
			connect(remove, &QToolButton::clicked, this, [this, aPath, at] {
				OpenConditions.clear();
				edit_selected([aPath, at](core::prompt_macro& aMacro) {
					if (const auto from = group_at(aMacro.Body, aPath); from && at < from->Groups->size()) {
						from->Groups->erase(from->Groups->begin() + static_cast<std::ptrdiff_t>(at));
					}
				});
			});
			head->addWidget(remove);
			inner->addLayout(head);
			if ((*group->Groups)[at].Conditions.empty() && (*group->Groups)[at].Groups.empty()) {
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
		const auto group = [&](const QString& aTitle, const std::initializer_list<std::pair<prompt_surface, QString>> aSurfaces) {
			aMenu.addSection(aTitle);
			for (const auto& [surface, name] : aSurfaces) {
				const auto  on      = std::ranges::find(Surfaces.Lists.at(index(surface)), aId) != Surfaces.Lists.at(index(surface)).end();
				const auto  refused = refusal(aId, surface);
				auto* const action  = aMenu.addAction(name);
				action->setObjectName(QStringLiteral("promptsAddTo-%1").arg(QString::fromLatin1(core::stable_name(surface))));
				action->setCheckable(true);
				action->setChecked(on);
				action->setEnabled(on || refused == core::placement_refusal::none);
				if (refused != core::placement_refusal::none) {
					action->setToolTip(QString::fromUtf8(core::placement_reason(refused).data(), static_cast<qsizetype>(core::placement_reason(refused).size())));
				}
				connect(action, &QAction::triggered, this, [this, aId, surface, on] {
					if (on) {
						remove(aId, surface);
					} else {
						static_cast<void>(place(aId, surface));
					}
				});
			}
		};
		group(tr("Pull request"), { { prompt_surface::pull_request_menu, tr("Ask menu") }, { prompt_surface::pull_request_chips, tr("Suggested chips") } });
		group(tr("File"), { { prompt_surface::file_menu, tr("Ask menu") }, { prompt_surface::file_chips, tr("Suggested chips") } });
		group(tr("Thread"), { { prompt_surface::thread_menu, tr("Ask menu") }, { prompt_surface::thread_chips, tr("Suggested chips") } });
		group(tr("Tour"), { { prompt_surface::tour_stop, tr("Tour stop") }, { prompt_surface::tour_part, tr("Tour part") }, { prompt_surface::whole_tour, tr("Whole tour") } });
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
		if (macro->BuiltIn && differs_from_shipped(*macro)) {
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
