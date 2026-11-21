#include "bpr/qt/elapsed_words_label.hpp"

#include "bpr/qt/agent_run_status.hpp"

#include <QTimer>

#include <algorithm>
#include <chrono>
#include <utility>

namespace bpr::qt {
	auto steady_clock_milliseconds() -> std::int64_t
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	elapsed_words_label::elapsed_words_label(QWidget* aParent, steady_milliseconds aClock)
		: QLabel{ aParent }
		, Clock{ aClock ? std::move(aClock) : steady_milliseconds{ steady_clock_milliseconds } }
		, Ticker{ new QTimer{ this } }
	{
		setObjectName(QStringLiteral("elapsedWords"));
		Ticker->setSingleShot(true);
		connect(Ticker, &QTimer::timeout, this, [this] { tick(); });
	}

	void elapsed_words_label::present(const QString& aWords, const std::optional<std::int64_t> aStartedMs)
	{
		Words     = aWords;
		StartedMs = aStartedMs;
		setAccessibleName(Words);
		tick();
	}

	auto elapsed_words_label::ticking() const -> bool { return Ticker->isActive(); }

	void elapsed_words_label::showEvent(QShowEvent* aEvent)
	{
		QLabel::showEvent(aEvent);
		tick();
	}

	void elapsed_words_label::hideEvent(QHideEvent* aEvent)
	{
		QLabel::hideEvent(aEvent);
		Ticker->stop();
	}

	void elapsed_words_label::tick()
	{
		if (!StartedMs) {
			Ticker->stop();
			setText(Words);
			return;
		}
		const auto elapsed = std::max(std::int64_t{ 0 }, Clock() - *StartedMs);
		setText(Words + QLatin1Char{ ' ' } + agent_elapsed_words(elapsed / 1000));
		if (isVisible()) {
			// The next whole second.
			Ticker->start(static_cast<int>(1000 - elapsed % 1000));
		} else {
			Ticker->stop();
		}
	}
} // namespace bpr::qt
