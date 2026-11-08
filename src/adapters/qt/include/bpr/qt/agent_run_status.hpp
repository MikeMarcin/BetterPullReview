#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_RUN_STATUS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_RUN_STATUS_HPP

#include <QElapsedTimer>
#include <QLabel>
#include <QString>

#include <cstdint>
#include <optional>

class QTimer;

namespace bpr::qt {
	/// \brief [[BPR-1941]]. What a running agent is doing now, as the Agent
	/// tab's turn says it: making its workspace, thinking (nothing written
	/// yet), or writing its answer.
	enum class agent_run_stage : std::uint8_t {
		preparing,
		thinking,
		writing,
	};

	/// \brief [[BPR-1941]]. One running request, as its live line says it.
	struct agent_run_status
	{
		/// The agent's name as the picker shows it.
		QString         Agent;
		agent_run_stage Stage = agent_run_stage::thinking;
		/// The last line it wrote, said after "thinking" while it has
		/// written nothing an answer is made of; empty for none.
		QString Latest;
		/// [[BPR-1873]]. A review with no clone: it reads only the pull
		/// request's files.
		bool FilesOnly = false;

		auto operator==(const agent_run_status&) const -> bool = default;
	};

	/// \brief `m:ss`, the time a run has taken.
	[[nodiscard]] auto agent_elapsed_words(qint64 aSeconds) -> QString;
	/// \brief The live line: "● Claude is thinking, 0:12." The Agent tab's
	/// turns and every tour chat say a running request with these words.
	[[nodiscard]] auto agent_run_status_words(const agent_run_status& aStatus, qint64 aSeconds) -> QString;
	/// \brief How a run ended, without its full stop: "Claude answered in
	/// 0:12", or "Stopped after 0:12".
	[[nodiscard]] auto agent_run_answered_words(const QString& aAgent, qint64 aSeconds) -> QString;
	[[nodiscard]] auto agent_run_stopped_words(qint64 aSeconds) -> QString;

	/// \brief [[BPR-1941]], owner 2026-11-08: "the '<agent> is thinking
	/// <elapsed time>' ... is much better than the tour side chat's little
	/// status disclosure thingy that has no useful info." A turn's quiet
	/// ending line. While its run is live it says what the agent is doing
	/// and the time it has taken, and ticks every second by itself; once the
	/// run ends it says how, in italics when there was no answer.
	///
	/// \details The Agent tab's turns and the tour chats' turns use this one
	/// line. It presents what it is told and decides nothing; the clock it
	/// reads is the owner's, so a turn drawn again keeps its time.
	class agent_run_status_line final : public QLabel
	{
	public:
		explicit agent_run_status_line(QWidget* aParent = nullptr);
		/// \brief Live: `aStatus`, timed from `aStarted` (an invalid clock
		/// reads as no time yet).
		void present(const agent_run_status& aStatus, const QElapsedTimer& aStarted);
		/// \brief Ended: `aWords`, and the ticking stops. Not `aAnswered`:
		/// in italics, as the agent drawers' Interrupted is.
		void               settle(const QString& aWords, bool aAnswered);
		[[nodiscard]] auto live() const noexcept -> bool { return Status.has_value(); }
		[[nodiscard]] auto status() const noexcept -> const std::optional<agent_run_status>& { return Status; }

	private:
		void tick();

		QTimer*                         Ticker = nullptr;
		std::optional<agent_run_status> Status;
		QElapsedTimer                   Started;
	};
}

#endif
