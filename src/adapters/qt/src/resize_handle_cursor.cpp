#include "bpr/qt/resize_handle_cursor.hpp"

#include <QCursor>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWindow>

namespace bpr::qt {
	namespace {
		/// \brief Re-applies its seam's cursor after an arrival, a move or a
		/// tooltip; several in one pass coalesce into one.
		class resize_cursor_hold final : public QObject
		{
		public:
			explicit resize_cursor_hold(QWidget& aHandle)
				: QObject{ &aHandle }
				, Handle{ &aHandle }
			{
				setObjectName(QStringLiteral("resizeCursorHold"));
				aHandle.setAttribute(Qt::WA_Hover);
				aHandle.installEventFilter(this);
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aWatched == Handle) {
					switch (aEvent->type()) {
					case QEvent::HoverEnter:
					case QEvent::HoverMove:
					case QEvent::Enter:
					case QEvent::ToolTip:
						queue();
						break;
					default:
						break;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			void queue()
			{
				if (Queued) {
					return;
				}
				Queued = true;
				// After the event that caused it, so the platform's own handling
				// of the same pointer move, or the tooltip window's arrival, has
				// already happened when the seam's cursor is put back.
				QTimer::singleShot(0, this, [this] {
					Queued = false;
					apply();
				});
			}

			void apply() const
			{
				if (Handle == nullptr || !Handle->isVisible() || !Handle->isEnabled() || !Handle->underMouse()) {
					return;
				}
				auto* const native = Handle->nativeParentWidget();
				auto* const window = native != nullptr ? native->windowHandle() : nullptr;
				if (window == nullptr) {
					return;
				}
				const auto wanted = Handle->cursor();
				if (window->cursor().shape() != wanted.shape()) {
					window->setCursor(wanted);
				}
				refresh_platform_cursor(*window);
			}

			QPointer<QWidget> Handle;
			bool              Queued = false;
		};
	} // namespace

	void hold_resize_cursor(QWidget& aHandle)
	{
		if (aHandle.findChild<QObject*>(QStringLiteral("resizeCursorHold"), Qt::FindDirectChildrenOnly) == nullptr) {
			new resize_cursor_hold{ aHandle };
		}
	}

#if !defined(__APPLE__)
	void refresh_platform_cursor(QWindow& aWindow)
	{
		static_cast<void>(aWindow);
	}
#endif
} // namespace bpr::qt
