#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_STICK_TO_BOTTOM_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_STICK_TO_BOTTOM_HPP

#include <QObject>
#include <QPointer>

class QEvent;
class QScrollArea;
class QScrollBar;
class QWidget;

namespace bpr::qt {
	/// \brief [[BPR-2296]] (owner, 2026-11-19: "The the agent tab and any
	/// similar session transcript window should Auto scroll to the bottom if
	/// you're at the bottom when anything new gets added below"). A
	/// transcript that is at its end stays there as it grows.
	/// \details Attached to a resizable QScrollArea, it follows the vertical
	/// bar: a transcript at its end, or within `slack` pixels of it, is
	/// pinned, and while pinned every growth of its range (an appended turn,
	/// streamed text, a tool row or a disclosure opening below, a shorter
	/// viewport) moves it to the new end. Any movement up, by the reader or
	/// the view (a failure revealed above), unpins it, and then nothing it
	/// does moves the transcript until the reader comes back to the end.
	///
	/// Owner, 2026-11-19: "If tool rows grow and then contract at the bottom
	/// it should stay at its lowest auto scroll value it shouldn't ping pong
	/// up and down". While a turn runs and the transcript is pinned, the
	/// content keeps the tallest height it reached as its least height, so a
	/// row that shrinks leaves room under it rather than pulling the
	/// transcript back up. The turn's end lets that go, and so does a change
	/// of width (a reflow, not the turn). Wheel events are untouched, so the
	/// nested scroll latch ([[BPR-2279]], [[BPR-2292]]) still decides which
	/// scroller a gesture moves; this only answers the content's growth.
	class stick_to_bottom final : public QObject
	{
	public:
		/// How far from the end, in pixels, still counts as at the end.
		static constexpr int slack = 8;

		/// \brief Attaches to `aArea`, which owns it; again, the one attached.
		static auto attach(QScrollArea& aArea) -> stick_to_bottom&;
		/// \brief The one attached to `aArea`, if any.
		[[nodiscard]] static auto of(const QScrollArea& aArea) -> stick_to_bottom*;

		/// \brief Whether the transcript follows its growth.
		[[nodiscard]] auto pinned() const noexcept -> bool { return Pinned; }
		/// \brief The least height held against shrinking; 0 when none.
		[[nodiscard]] auto floor() const noexcept -> int { return Floor; }
		/// \brief Moves to the end now and follows from there: a new
		/// transcript shown, or the reader's own question sent.
		void pin();
		/// \brief Whether a turn is running. Its end lets the content shrink
		/// to what it holds, and a pinned transcript moves to its new end
		/// once.
		void set_turn_running(bool aRunning);

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		explicit stick_to_bottom(QScrollArea& aArea);

		void moved(int aValue);
		void ranged(int aMaximum);
		void to_end();
		void hold();
		void settle();

		QPointer<QScrollArea> Area;
		QPointer<QScrollBar>  Bar;
		bool                  Pinned    = true;
		bool                  Following = false;
		bool                  Running   = false;
		int                   Last      = 0;
		int                   Floor     = 0;
		int                   Width     = -1;
	};
} // namespace bpr::qt

#endif
