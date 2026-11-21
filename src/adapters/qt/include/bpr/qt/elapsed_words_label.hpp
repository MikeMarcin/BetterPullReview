#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ELAPSED_WORDS_LABEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ELAPSED_WORDS_LABEL_HPP

#include <QLabel>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

class QTimer;

namespace bpr::qt {
	/// \brief A monotonic clock in milliseconds: the steady clock, or a
	/// test's own.
	using steady_milliseconds = std::function<std::int64_t()>;

	/// \brief The steady clock in milliseconds, the clock the hosts stamp
	/// their start instants with.
	[[nodiscard]] auto steady_clock_milliseconds() -> std::int64_t;

	/// \brief [[BPR-2215]] (owner, 0.5.0 build 76: "Preparing workspace…
	/// 0:12"). Words with the time since an instant the host gave, ticking
	/// once a second.
	///
	/// \details Presents what it is told: the instant comes from the host's
	/// projection, so a label made again for the same work shows the same
	/// time. It ticks only while it is visible and has an instant. Its
	/// accessible name is the words alone, so a screen reader is not told
	/// every second.
	class elapsed_words_label final : public QLabel
	{
	public:
		explicit elapsed_words_label(QWidget* aParent = nullptr, steady_milliseconds aClock = { });

		/// \brief Shows `aWords`, then the time since `aStartedMs` when there
		/// is one.
		void               present(const QString& aWords, std::optional<std::int64_t> aStartedMs);
		[[nodiscard]] auto ticking() const -> bool;

	protected:
		void showEvent(QShowEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;

	private:
		void tick();

		steady_milliseconds         Clock;
		QString                     Words;
		std::optional<std::int64_t> StartedMs;
		QTimer*                     Ticker = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ELAPSED_WORDS_LABEL_HPP
