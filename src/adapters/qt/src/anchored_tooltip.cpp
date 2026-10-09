#include "bpr/qt/anchored_tooltip.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QModelIndex>
#include <QPaintEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QStyleHintReturnMask>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QTabBar>
#include <QTimer>
#include <QToolTip>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// How often the pointer is checked against the anchor, and how long a
		/// tooltip stays at most.
		constexpr int pointer_check_ms = 100;
		constexpr int lifetime_ms      = 10000;
		/// About how many characters a tooltip's line holds before it wraps.
		constexpr int wrap_characters = 80;

		/// The tooltip window: the style's own tooltip panel, palette and font
		/// around plain text, as `QToolTip`'s label draws them.
		class anchored_tip final : public QLabel
		{
		public:
			anchored_tip(QWidget& aOwner, const QRect& aAnchor, const QString& aText, const anchored_tooltip_hold aHold)
				: QLabel{ &aOwner, Qt::ToolTip | Qt::BypassGraphicsProxyWidget }
				, Owner{ &aOwner }
				, Anchor{ aAnchor }
				, Hold{ aHold }
			{
				setObjectName(QStringLiteral("anchoredTooltip"));
				setForegroundRole(QPalette::ToolTipText);
				setBackgroundRole(QPalette::ToolTipBase);
				setPalette(QToolTip::palette());
				setFont(QToolTip::font());
				setTextFormat(Qt::PlainText);
				setMargin(1 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, this));
				setFrameStyle(QFrame::NoFrame);
				setAlignment(Qt::AlignLeft);
				setIndent(1);
				setAttribute(Qt::WA_TransparentForMouseEvents);
				setAttribute(Qt::WA_ShowWithoutActivating);
				setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, nullptr, this) / 255.0);
				setText(aText);
				adjustSize();
				Pointer.setInterval(pointer_check_ms);
				QObject::connect(&Pointer, &QTimer::timeout, this, [this] {
					if (!global_anchor().adjusted(-1, -1, 1, 1).contains(QCursor::pos())) {
						dismiss();
					}
				});
				Lifetime.setSingleShot(true);
				Lifetime.setInterval(lifetime_ms);
				QObject::connect(&Lifetime, &QTimer::timeout, this, [this] { dismiss(); });
			}
			~anchored_tip() override
			{
				qApp->removeEventFilter(this);
			}
			anchored_tip(const anchored_tip&)                    = delete;
			anchored_tip(anchored_tip&&)                         = delete;
			auto operator=(const anchored_tip&) -> anchored_tip& = delete;
			auto operator=(anchored_tip&&) -> anchored_tip&      = delete;

			[[nodiscard]] auto names(const QWidget& aOwner, const QRect& aAnchor, const QString& aText) const -> bool
			{
				return Owner == &aOwner && Anchor == aAnchor && text() == aText;
			}

			void present()
			{
				const auto anchor = global_anchor();
				auto*      screen = QGuiApplication::screenAt(anchor.center());
				if (screen == nullptr) {
					screen = Owner->screen();
				}
				const auto area = screen != nullptr ? screen->availableGeometry() : QRect{ anchor.topLeft() - QPoint{ 4096, 4096 }, QSize{ 8192, 8192 } };
				// One line, unless that line is longer than about eighty
				// characters or wider than the screen: then it wraps there. A
				// tooltip as wide as the window is not next to anything.
				auto       size  = sizeHint();
				const auto limit = std::min(area.width(), (fontMetrics().averageCharWidth() * wrap_characters) + (2 * margin()));
				if (size.width() > limit) {
					setWordWrap(true);
					size = QSize{ limit, heightForWidth(limit) };
				}
				setGeometry(anchored_tooltip_geometry(anchor, size, area));
				qApp->installEventFilter(this);
				show();
				raise();
				// A keyboard reviewer's pointer is anywhere, or nowhere: a
				// tooltip shown for focus does not follow it ([[BPR-0790]]).
				if (Hold == anchored_tooltip_hold::pointer) {
					Pointer.start();
				}
				Lifetime.start();
			}

			void dismiss()
			{
				Pointer.stop();
				Lifetime.stop();
				qApp->removeEventFilter(this);
				hide();
				deleteLater();
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				switch (aEvent->type()) {
				case QEvent::Leave:
					if (aWatched == Owner && Hold == anchored_tooltip_hold::pointer) {
						dismiss();
					}
					break;
				case QEvent::Hide:
					if (aWatched == Owner) {
						dismiss();
					}
					break;
				case QEvent::KeyPress:
					// A modifier alone leaves the tooltip, as `QToolTip`'s own
					// does: holding Shift over a scroll-bar tick shows its
					// tooltip at once ([[BPR-1290]]), and must not then take it
					// away.
					if (const auto key = static_cast<QKeyEvent*>(aEvent)->key(); key < Qt::Key_Shift || key > Qt::Key_ScrollLock) {
						dismiss();
					}
					break;
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonDblClick:
				case QEvent::Wheel:
				case QEvent::WindowDeactivate:
				case QEvent::FocusOut:
					dismiss();
					break;
				default:
					break;
				}
				return QLabel::eventFilter(aWatched, aEvent);
			}

			void paintEvent(QPaintEvent* const aEvent) override
			{
				{
					auto painter = QStylePainter{ this };
					auto option  = QStyleOptionFrame{ };
					option.initFrom(this);
					painter.drawPrimitive(QStyle::PE_PanelTipLabel, option);
				}
				QLabel::paintEvent(aEvent);
			}

			void resizeEvent(QResizeEvent* const aEvent) override
			{
				auto option = QStyleOptionFrame{ };
				option.initFrom(this);
				auto mask = QStyleHintReturnMask{ };
				if (style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &mask) != 0) {
					setMask(mask.region);
				}
				QLabel::resizeEvent(aEvent);
			}

		private:
			[[nodiscard]] auto global_anchor() const -> QRect
			{
				return Owner != nullptr ? QRect{ Owner->mapToGlobal(Anchor.topLeft()), Anchor.size() } : QRect{ };
			}

			QPointer<QWidget>     Owner;
			QRect                 Anchor;
			anchored_tooltip_hold Hold = anchored_tooltip_hold::pointer;
			QTimer                Pointer;
			QTimer                Lifetime;
		};

		/// Set on a widget whose tooltips `anchor_tooltips` answers, so the
		/// application-wide placement leaves them to it.
		constexpr auto own_source_property = "bprAnchoredTooltipSource";
		/// Set on the application once its placement is installed.
		constexpr auto installed_property = "bprAnchoredTooltips";

		auto current() -> QPointer<anchored_tip>&
		{
			static auto tip = QPointer<anchored_tip>{ };
			return tip;
		}

		/// Answers one widget's tooltip events with an anchored tooltip.
		class anchored_tooltip_filter final : public QObject
		{
		public:
			anchored_tooltip_filter(QWidget& aWidget, anchored_tooltip_source aSource)
				: QObject{ &aWidget }
				, Widget{ &aWidget }
				, Source{ std::move(aSource) }
			{
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aWatched != Widget || aEvent->type() != QEvent::ToolTip) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				const auto* const help   = static_cast<QHelpEvent*>(aEvent);
				auto              target = std::optional<anchored_tooltip_target>{ };
				if (Source) {
					target = Source(help->pos());
				} else if (!Widget->toolTip().isEmpty()) {
					target = anchored_tooltip_target{ .Anchor = Widget->rect(), .Text = Widget->toolTip() };
				}
				if (target.has_value() && !target->Text.isEmpty()) {
					QToolTip::hideText();
					show_anchored_tooltip(*Widget, target->Anchor, target->Text);
					return true;
				}
				hide_anchored_tooltip();
				aEvent->ignore();
				return true;
			}

		private:
			QWidget*                Widget = nullptr;
			anchored_tooltip_source Source;
		};

		/// `QAction::toolTip()` with no tooltip of its own set: its text
		/// without an ellipsis or mnemonic ampersands, as Qt derives it. A menu
		/// shows only a tooltip that was set ([[BPR-1354]] keeps that).
		auto derived_tooltip(QString aText) -> QString
		{
			aText.remove(QStringLiteral("..."));
			for (auto index = qsizetype{ 0 }; index < aText.size(); ++index) {
				if (aText.at(index) == QLatin1Char{ '&' }) {
					aText.remove(index, 1);
				}
			}
			return aText.trimmed();
		}

		/// [[BPR-1354]]. Answers every tooltip event of the application with
		/// an anchored tooltip, before the widget would have handed it to
		/// `QToolTip`. Installed once on the application object, which calls
		/// it for every receiver: when it answers nothing, the event goes on
		/// to the widget as before, and Qt hands an unanswered tooltip event
		/// to the parent, which comes back through here.
		class anchored_application_filter final : public QObject
		{
		public:
			explicit anchored_application_filter(QObject& aApplication)
				: QObject{ &aApplication }
			{
				setObjectName(QStringLiteral("anchoredTooltipPlacement"));
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() != QEvent::ToolTip || !aWatched->isWidgetType()) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				auto* const widget = static_cast<QWidget*>(aWatched);
				if (widget->property(own_source_property).toBool()) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				const auto target = standard_tooltip_target(*widget, static_cast<QHelpEvent*>(aEvent)->pos());
				if (!target.has_value()) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				QToolTip::hideText();
				show_anchored_tooltip(*widget, target->Anchor, target->Text);
				aEvent->accept();
				return true;
			}
		};
	} // namespace

	auto anchored_tooltip_geometry(const QRect& aAnchor, const QSize& aTip, const QRect& aScreen) -> QRect
	{
		auto       top    = aAnchor.bottom() + 1 + anchored_tooltip_gap;
		const auto bottom = aScreen.top() + aScreen.height();
		if (top + aTip.height() > bottom) {
			top = std::max(aScreen.top(), aAnchor.top() - anchored_tooltip_gap - aTip.height());
		}
		const auto right = aScreen.left() + aScreen.width();
		const auto left  = std::max(aScreen.left(), std::min(aAnchor.left(), right - aTip.width()));
		return QRect{ QPoint{ left, top }, aTip };
	}

	void show_anchored_tooltip(QWidget& aOwner, const QRect& aAnchor, const QString& aText, const anchored_tooltip_hold aHold)
	{
		auto& tip = current();
		if (tip != nullptr && tip->isVisible() && tip->names(aOwner, aAnchor, aText)) {
			return;
		}
		hide_anchored_tooltip();
		if (aText.isEmpty()) {
			return;
		}
		tip = new anchored_tip{ aOwner, aAnchor, aText, aHold };
		tip->present();
	}

	void hide_anchored_tooltip()
	{
		if (auto& tip = current(); tip != nullptr) {
			tip->dismiss();
			tip = nullptr;
		}
	}

	auto anchored_tooltip() -> QWidget*
	{
		const auto& tip = current();
		return tip != nullptr && tip->isVisible() ? tip.data() : nullptr;
	}

	void anchor_tooltips(QWidget& aWidget, anchored_tooltip_source aSource)
	{
		aWidget.setProperty(own_source_property, true);
		aWidget.installEventFilter(new anchored_tooltip_filter{ aWidget, std::move(aSource) });
	}

	auto standard_tooltip_target(const QWidget& aWidget, const QPoint aPosition) -> std::optional<anchored_tooltip_target>
	{
		if (const auto* const menu = qobject_cast<const QMenu*>(&aWidget); menu != nullptr) {
			// A menu shows an action's tooltip only when it shows tooltips
			// at all and the action has one of its own, as `QMenu` does.
			auto* const action = menu->actionAt(aPosition);
			if (!menu->toolTipsVisible() || action == nullptr || action->toolTip().isEmpty() || action->toolTip() == derived_tooltip(action->text())) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = menu->actionGeometry(action), .Text = action->toolTip() };
		}
		if (const auto* const bar = qobject_cast<const QTabBar*>(&aWidget); bar != nullptr) {
			if (const auto index = bar->tabAt(aPosition); index >= 0 && !bar->tabToolTip(index).isEmpty()) {
				return anchored_tooltip_target{ .Anchor = bar->tabRect(index), .Text = bar->tabToolTip(index) };
			}
		} else if (const auto* const view = qobject_cast<const QAbstractItemView*>(aWidget.parentWidget()); view != nullptr && view->viewport() == &aWidget) {
			if (const auto* const header = qobject_cast<const QHeaderView*>(view); header != nullptr) {
				const auto section = header->logicalIndexAt(aPosition);
				const auto text    = section >= 0 && header->model() != nullptr ? header->model()->headerData(section, header->orientation(), Qt::ToolTipRole).toString() : QString{ };
				if (!text.isEmpty()) {
					const auto start = header->sectionViewportPosition(section);
					const auto size  = header->sectionSize(section);
					return anchored_tooltip_target{ .Anchor = header->orientation() == Qt::Horizontal ? QRect{ start, 0, size, aWidget.height() } : QRect{ 0, start, aWidget.width(), size }, .Text = text };
				}
			} else if (const auto index = view->indexAt(aPosition); index.isValid()) {
				if (const auto text = index.data(Qt::ToolTipRole).toString(); !text.isEmpty()) {
					const auto item = view->visualRect(index).intersected(aWidget.rect());
					return anchored_tooltip_target{ .Anchor = item.isEmpty() ? QRect{ aPosition, QSize{ 1, 1 } } : item, .Text = text };
				}
			}
		}
		if (!aWidget.toolTip().isEmpty()) {
			return anchored_tooltip_target{ .Anchor = aWidget.rect(), .Text = aWidget.toolTip() };
		}
		return std::nullopt;
	}

	void install_anchored_tooltips()
	{
		auto* const application = QCoreApplication::instance();
		if (application == nullptr || application->property(installed_property).toBool()) {
			return;
		}
		application->setProperty(installed_property, true);
		application->installEventFilter(new anchored_application_filter{ *application });
	}
} // namespace bpr::qt

namespace {
	/// [[BPR-1354]]. Every widget's tooltip is anchored from the moment the
	/// application object exists, with no surface left to opt in.
	void install_anchored_tooltips_at_startup()
	{
		bpr::qt::install_anchored_tooltips();
	}
} // namespace
Q_COREAPP_STARTUP_FUNCTION(install_anchored_tooltips_at_startup)
