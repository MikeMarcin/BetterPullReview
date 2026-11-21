#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONTEXT_CLICK_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONTEXT_CLICK_HPP

#include <QAbstractItemView>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>

namespace bpr::qt {
	namespace detail {
		/// \brief Consumes the secondary button's presses, releases, double
		/// clicks and drags on an item view's viewport.
		class context_click_filter final : public QObject
		{
		public:
			using QObject::QObject;

		protected:
			auto eventFilter(QObject* const aObject, QEvent* const aEvent) -> bool override
			{
				switch (aEvent->type()) {
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
					if (static_cast<QMouseEvent*>(aEvent)->button() == Qt::RightButton) {
						aEvent->accept();
						return true;
					}
					break;
				case QEvent::MouseMove:
					if (static_cast<QMouseEvent*>(aEvent)->buttons() == Qt::RightButton) {
						aEvent->accept();
						return true;
					}
					break;
				default:
					break;
				}
				return QObject::eventFilter(aObject, aEvent);
			}
		};
	} // namespace detail

	/// \brief [[BPR-2334]]. A right-click on a row opens that row's menu and
	/// leaves the current row, the selection and what they show alone.
	///
	/// \details Owner, attended review bbbd62f8: "other apps just open the
	/// context menu for the thing you mouse over instead of selecting it".
	/// A QAbstractItemView makes the pressed row current for any button, and
	/// a view that opens what is current (the files drawer opens the file,
	/// source search jumps to the match) then loads it. The filter keeps the
	/// secondary button from reaching the view; the context menu event still
	/// arrives, so the view's menu names the row under the pointer by
	/// position. Use it only on a view whose menu reads the row from the
	/// position, not from the current row.
	inline void open_menu_without_choosing(QAbstractItemView& aView)
	{
		aView.viewport()->installEventFilter(new detail::context_click_filter{ &aView });
	}
} // namespace bpr::qt

#endif
