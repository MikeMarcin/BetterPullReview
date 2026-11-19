#include "bpr/qt/window_file_drop.hpp"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief The quiet frame a hovering file draws over its window: a
		/// thin rounded edge in the palette's highlight, a faint wash and
		/// one line at the foot saying what a drop does.
		class drop_highlight final : public QWidget
		{
		public:
			explicit drop_highlight(QWidget& aWindow)
				: QWidget{ &aWindow }
			{
				setObjectName(QStringLiteral("windowFileDropHighlight"));
				setAttribute(Qt::WA_TransparentForMouseEvents);
				setAttribute(Qt::WA_NoSystemBackground);
				setFocusPolicy(Qt::NoFocus);
				setAccessibleName(caption());
				hide();
			}

			[[nodiscard]] static auto caption() -> QString { return QObject::tr("Drop to preview the import"); }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto accent = palette().color(QPalette::Highlight);
				auto       wash   = accent;
				wash.setAlpha(18);
				painter.fillRect(rect(), wash);
				constexpr auto inset  = 5.0;
				constexpr auto radius = 10.0;
				auto           edge   = QPen{ accent, 2.0 };
				painter.setPen(edge);
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(inset, inset, -inset, -inset), radius, radius);

				const auto metrics = QFontMetrics{ font() };
				const auto text    = caption();
				const auto padX    = metrics.height();
				const auto padY    = metrics.height() / 3;
				const auto width   = metrics.horizontalAdvance(text) + (2 * padX);
				const auto height  = metrics.height() + (2 * padY);
				const auto pill    = QRectF{ (this->width() - width) / 2.0, this->height() - height - (3.0 * metrics.height()), static_cast<qreal>(width), static_cast<qreal>(height) };
				painter.setPen(Qt::NoPen);
				painter.setBrush(accent);
				painter.drawRoundedRect(pill, height / 2.0, height / 2.0);
				painter.setPen(palette().color(QPalette::HighlightedText));
				painter.drawText(pill, Qt::AlignCenter, text);
			}
		};

		[[nodiscard]] auto modal_open() -> bool { return QApplication::activeModalWidget() != nullptr; }

		/// Accepts as a copy where the source offers one: the file stays where it is.
		void accept_copy(QDropEvent& aEvent)
		{
			if (aEvent.possibleActions().testFlag(Qt::CopyAction)) {
				aEvent.setDropAction(Qt::CopyAction);
				aEvent.accept();
			} else {
				aEvent.acceptProposedAction();
			}
		}
	} // namespace

	window_file_drop::window_file_drop(QWidget& aWindow, handlers aHandlers)
		: QObject{ &aWindow }
		, Window{ &aWindow }
		, Highlight{ new drop_highlight{ aWindow } }
		, Handlers{ std::move(aHandlers) }
	{
		setObjectName(QStringLiteral("windowFileDrop"));
		// Every point of the window then has a widget a drag is delivered to.
		aWindow.setAcceptDrops(true);
		qApp->installEventFilter(this);
	}

	auto window_file_drop::several_files_text() -> QString { return QObject::tr("Drop one file at a time."); }

	auto window_file_drop::belongs(const QObject* const aWatched) const -> bool
	{
		const auto* const widget = qobject_cast<const QWidget*>(aWatched);
		return widget != nullptr && Window != nullptr && widget->window() == Window.data();
	}

	void window_file_drop::hover(const bool aOn)
	{
		Hovering = aOn;
		if (aOn && Window != nullptr) {
			Highlight->setGeometry(Window->rect());
			Highlight->raise();
			Highlight->show();
		} else {
			Highlight->hide();
		}
	}

	auto window_file_drop::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		const auto type = aEvent->type();
		if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::DragLeave && type != QEvent::Drop) {
			return QObject::eventFilter(aWatched, aEvent);
		}
		if (!belongs(aWatched)) {
			return QObject::eventFilter(aWatched, aEvent);
		}
		if (type == QEvent::DragLeave) {
			// Not taken: the widget that had the drag still hears it leave.
			hover(false);
			return QObject::eventFilter(aWatched, aEvent);
		}
		auto&      drop      = static_cast<QDropEvent&>(*aEvent);
		const auto admission = drop.mimeData() != nullptr && Handlers.Admit ? Handlers.Admit(*drop.mimeData()) : dropped_file_admission{ };
		if (admission.Kind == dropped_file_admission::kind::none) {
			return QObject::eventFilter(aWatched, aEvent);
		}
		if (admission.Kind == dropped_file_admission::kind::several_files || modal_open()) {
			hover(false);
			drop.ignore();
			// A refused drag climbs from the widget under the pointer to the
			// window; it is said once, at the window.
			if (type == QEvent::DragEnter && aWatched == Window.data() && admission.Kind == dropped_file_admission::kind::several_files && !modal_open() && Handlers.Say) {
				Handlers.Say(several_files_text());
			}
			return true;
		}
		if (type == QEvent::Drop) {
			hover(false);
			accept_copy(drop);
			QTimer::singleShot(0, this, [this, path = admission.Path] {
				if (!modal_open() && Handlers.Dropped) {
					Handlers.Dropped(path);
				}
			});
			return true;
		}
		hover(true);
		accept_copy(drop);
		return true;
	}
} // namespace bpr::qt
