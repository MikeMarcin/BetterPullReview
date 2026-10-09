#include "bpr/qt/anchored_tooltip.hpp"

#include <QApplication>
#include <QChildEvent>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPaintEvent>
#include <QPointer>
#include <QVariant>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QStyleHintReturnMask>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// How often the pointer is checked against the anchor, and how long a
		/// tooltip stays at most.
		constexpr int pointer_check_ms = 100;
		constexpr int lifetime_ms      = 10000;

		/// The tooltip window: the style's own tooltip panel, palette and font
		/// around plain text, as `QToolTip`'s label draws them.
		class anchored_tip final : public QLabel
		{
		public:
			anchored_tip(QWidget& aOwner, const QRect& aAnchor, const QString& aText)
				: QLabel{ &aOwner, Qt::ToolTip | Qt::BypassGraphicsProxyWidget }
				, Owner{ &aOwner }
				, Anchor{ aAnchor }
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
				setGeometry(anchored_tooltip_geometry(anchor, sizeHint(), area));
				qApp->installEventFilter(this);
				show();
				raise();
				Pointer.start();
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

			QPointer<QWidget> Owner;
			QRect             Anchor;
			QTimer            Pointer;
			QTimer            Lifetime;
		};

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

		/// [[BPR-1328]]. Follows every widget inside one root and answers its
		/// tooltip events with its own tooltip, anchored below it.
		class anchored_descendant_filter final : public QObject
		{
		public:
			explicit anchored_descendant_filter(QWidget& aRoot)
				: QObject{ &aRoot }
				, Root{ &aRoot }
			{
				follow(aRoot);
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ChildAdded) {
					// Watching every widget inside the root sees a widget's
					// own children arrive too, even while its constructor
					// builds them.
					if (auto* const child = static_cast<QChildEvent*>(aEvent)->child(); child != nullptr && child->isWidgetType()) {
						follow(*child);
					}
					return QObject::eventFilter(aWatched, aEvent);
				}
				if (aEvent->type() != QEvent::ToolTip || aWatched == Root || !aWatched->isWidgetType()) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				auto* const widget = static_cast<QWidget*>(aWatched);
				if (widget->toolTip().isEmpty() || Root == nullptr || !Root->isAncestorOf(widget)) {
					return QObject::eventFilter(aWatched, aEvent);
				}
				QToolTip::hideText();
				show_anchored_tooltip(*widget, widget->rect(), widget->toolTip());
				return true;
			}

		private:
			void follow(QObject& aObject)
			{
				if (aObject.property(followed_property).toBool()) {
					return;
				}
				aObject.setProperty(followed_property, true);
				aObject.installEventFilter(this);
				for (auto* const child : aObject.children()) {
					if (child->isWidgetType()) {
						follow(*child);
					}
				}
			}

			static constexpr auto followed_property = "bprAnchoredDescendantTooltip";
			QPointer<QWidget>     Root;
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

	void show_anchored_tooltip(QWidget& aOwner, const QRect& aAnchor, const QString& aText)
	{
		auto& tip = current();
		if (tip != nullptr && tip->isVisible() && tip->names(aOwner, aAnchor, aText)) {
			return;
		}
		hide_anchored_tooltip();
		if (aText.isEmpty()) {
			return;
		}
		tip = new anchored_tip{ aOwner, aAnchor, aText };
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
		aWidget.installEventFilter(new anchored_tooltip_filter{ aWidget, std::move(aSource) });
	}

	void anchor_descendant_tooltips(QWidget& aRoot)
	{
		static_cast<void>(new anchored_descendant_filter{ aRoot });
	}
} // namespace bpr::qt
