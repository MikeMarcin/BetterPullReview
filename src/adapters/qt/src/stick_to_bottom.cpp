#include "bpr/qt/stick_to_bottom.hpp"

#include <QEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>

#include <algorithm>

namespace bpr::qt {
	auto stick_to_bottom::attach(QScrollArea& aArea) -> stick_to_bottom&
	{
		if (auto* const attached = of(aArea); attached != nullptr) {
			return *attached;
		}
		return *new stick_to_bottom{ aArea };
	}

	auto stick_to_bottom::of(const QScrollArea& aArea) -> stick_to_bottom*
	{
		for (auto* const child : aArea.children()) {
			if (auto* const attached = dynamic_cast<stick_to_bottom*>(child); attached != nullptr) {
				return attached;
			}
		}
		return nullptr;
	}

	stick_to_bottom::stick_to_bottom(QScrollArea& aArea)
		: QObject{ &aArea }
		, Area{ &aArea }
		, Bar{ aArea.verticalScrollBar() }
		, Last{ aArea.verticalScrollBar()->value() }
		, Width{ aArea.viewport()->width() }
	{
		// Pinned from the start: a bar not yet laid out says 0..99, which is
		// not where the reader is.
		connect(Bar, &QScrollBar::valueChanged, this, [this](const int aValue) { moved(aValue); });
		connect(Bar, &QScrollBar::rangeChanged, this, [this](int, const int aMaximum) { ranged(aMaximum); });
		aArea.viewport()->installEventFilter(this);
	}

	void stick_to_bottom::pin()
	{
		Pinned = true;
		to_end();
	}

	void stick_to_bottom::set_turn_running(const bool aRunning)
	{
		Running = aRunning;
		if (!aRunning) {
			settle();
		} else if (Pinned) {
			hold();
		}
	}

	void stick_to_bottom::settle()
	{
		if (Floor == 0) {
			return;
		}
		Floor = 0;
		if (auto* const content = Area != nullptr ? Area->widget() : nullptr; content != nullptr) {
			content->setMinimumHeight(0);
		}
	}

	auto stick_to_bottom::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		// A narrower or wider transcript reflows; that is not the turn
		// shrinking, so what was held for the turn is let go.
		if (aEvent->type() == QEvent::Resize && Area != nullptr && aWatched == Area->viewport()) {
			const auto width = static_cast<QResizeEvent*>(aEvent)->size().width();
			if (width != Width) {
				Width = width;
				settle();
			}
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	void stick_to_bottom::moved(const int aValue)
	{
		if (!Following) {
			// Down to the end pins; any movement up lets go, however small,
			// so a reader's first nudge up is never pulled back.
			if (aValue > Last) {
				Pinned = aValue >= Bar->maximum() - slack;
			} else if (aValue < Last) {
				Pinned = false;
			}
		}
		Last = aValue;
	}

	void stick_to_bottom::ranged(const int aMaximum)
	{
		if (!Pinned) {
			return;
		}
		// Ahead of the bar's own clamp, so a shorter range is met at its end.
		if (Bar->value() != aMaximum) {
			to_end();
		}
		hold();
	}

	void stick_to_bottom::to_end()
	{
		if (Bar == nullptr) {
			return;
		}
		Following = true;
		Bar->setValue(Bar->maximum());
		Following = false;
		Last      = Bar->value();
	}

	void stick_to_bottom::hold()
	{
		auto* const content = Area != nullptr ? Area->widget() : nullptr;
		// Only while a turn runs, and only a transcript taller than its
		// viewport has an end to hold: one that fits is as tall as the
		// viewport, which may yet shrink.
		if (!Running || content == nullptr || !Area->widgetResizable() || Bar->maximum() <= 0 || content->height() <= Floor) {
			return;
		}
		Floor = content->height();
		content->setMinimumHeight(Floor);
	}
} // namespace bpr::qt
