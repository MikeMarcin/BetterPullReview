#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_NESTED_SCROLL_LATCH_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_NESTED_SCROLL_LATCH_HPP

#include <QObject>
#include <QPointer>

class QAbstractScrollArea;
class QEvent;
class QWheelEvent;
class QWidget;

namespace bpr::qt {
	/// \brief [[BPR-2279]] (owner, 2026-11-19: "momentum scrolling that
	/// reaches the end of a nested scroll window won't scroll the outer
	/// window without an additional gesture. In fact that appears to be
	/// exactly how it works in the chrome browser"). One trackpad gesture
	/// moves one scroller.
	/// \details Installed once on the application, it watches every
	/// spontaneous phased wheel event (a trackpad's or a Magic Mouse's)
	/// that begins inside a scroll area nested in another. The gesture's
	/// first movement picks its scroller: the innermost one that can still
	/// move that way and takes that movement, else the outermost, which
	/// decides for itself (the comments drawer moves the code with it). A
	/// scroller with no range on the gesture's axis, at its end that way, or
	/// that declines the axis, such as a code block or suggestion that only
	/// scrolls sideways, is passed over ([[BPR-2292]]). Every later event of the gesture, its momentum
	/// included, goes to that scroller alone; at its end the rest is
	/// dropped, not handed outwards. The next gesture chooses again. A
	/// mouse wheel's unphased notches, events a widget sends itself, and a
	/// scroller not nested in another are left to Qt.
	class nested_scroll_latch final : public QObject
	{
	public:
		/// \brief Installs the latch on the application; again, does nothing.
		static void install();
		/// \brief Whether the wheel event being delivered is the latch's
		/// own, handed to the gesture's scroller. A filter that passes a
		/// wheel on to an outer area leaves such an event where it is.
		[[nodiscard]] static auto delivering() noexcept -> bool;
		/// \brief The scroller the current gesture belongs to, if any.
		[[nodiscard]] static auto owner() -> QAbstractScrollArea*;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		using QObject::QObject;

		/// \brief Starts a gesture from the widget it began over.
		void begin(QWidget& aOrigin);
		/// \brief Hands `aEvent` to `aOwner`'s viewport, where it stays;
		/// whether `aOwner` took it.
		static auto deliver(QAbstractScrollArea& aOwner, const QWheelEvent& aEvent) -> bool;

		QPointer<QWidget>             Origin;
		QPointer<QAbstractScrollArea> Owner;
		/// The gesture moved, so its scroller is chosen.
		bool Decided = false;
		/// The gesture's ScrollEnd came; momentum may still follow it.
		bool Ended = true;
	};
} // namespace bpr::qt

#endif
