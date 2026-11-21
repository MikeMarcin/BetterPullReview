#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_RUN_STATUS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_RUN_STATUS_HPP

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_turn.hpp"

#include <QElapsedTimer>
#include <QLabel>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QGridLayout;
class QPushButton;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1941]]. What a running agent is doing now, as the Agent
	/// tab's turn says it: making its workspace, checking the workspace it
	/// kept ([[BPR-2261]]), thinking (nothing written yet), or writing its
	/// answer.
	enum class agent_run_stage : std::uint8_t {
		preparing,
		thinking,
		writing,
		/// [[BPR-2261]]. Before the run, the pull request's retained
		/// workspace is checked; nothing is made.
		checking,
	};

	/// \brief [[BPR-2261]]. The run has not started: its workspace is being
	/// made or checked.
	[[nodiscard]] constexpr auto agent_run_before_start(const agent_run_stage aStage) noexcept -> bool
	{
		return aStage == agent_run_stage::preparing || aStage == agent_run_stage::checking;
	}

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
		/// [[BPR-2005]], RFC-BPR-0103. What the runtime's events say it is
		/// doing; absent where a runtime reports none, and the stage is said.
		std::optional<core::agent_turn_activity> Activity;
		/// When the run started, on `agent_activity_clock_ms`; the
		/// activity's times count from it.
		qint64 ActivityStartedMs = 0;
		/// [[BPR-2252]]. What the turn has cost so far, as the agent reported
		/// it mid-turn; none until it reports a figure, and none for an
		/// agent that reports none.
		std::optional<core::agent_cost> Cost;

		auto operator==(const agent_run_status&) const -> bool = default;
	};

	/// \brief [[BPR-2005]]. The clock a running block reads, in
	/// milliseconds; monotonic. A test sets its own (`set_agent_activity_clock`).
	[[nodiscard]] auto agent_activity_clock_ms() -> qint64;
	void               set_agent_activity_clock(std::function<qint64()> aClock);

	/// \brief [[BPR-2005]]. What the agent is doing, after its name:
	/// "is reading frame_pool.cpp", "is waiting on 2 helpers".
	[[nodiscard]] auto agent_doing_words(const core::agent_turn_activity& aActivity) -> QString;
	/// \brief "last activity 4 s ago", "last activity 2 min ago".
	[[nodiscard]] auto agent_last_activity_words(qint64 aSinceMs) -> QString;
	/// \brief The folded line: "Worked for 7 min: 3 helpers, 41 files read,
	/// 12 commands".
	[[nodiscard]] auto agent_activity_summary_words(const core::agent_turn_activity& aActivity, qint64 aSeconds) -> QString;

	/// \brief `m:ss`, the time a run has taken; `h:mm:ss` from an hour.
	[[nodiscard]] auto agent_elapsed_words(qint64 aSeconds) -> QString;
	/// \brief The live line: "● Claude is thinking, 0:12." The Agent tab's
	/// turns and every tour chat say a running request with these words.
	/// Once the agent has reported a cost it follows the time: "● Claude is
	/// waiting on 2 helpers, 3:12 · $0.41 · last activity 4 s ago." ([[BPR-2252]]).
	[[nodiscard]] auto agent_run_status_words(const agent_run_status& aStatus, qint64 aSeconds) -> QString;
	/// \brief How a run ended, without its full stop: "Claude answered in
	/// 0:12", or "Stopped after 0:12".
	[[nodiscard]] auto agent_run_answered_words(const QString& aAgent, qint64 aSeconds) -> QString;
	[[nodiscard]] auto agent_run_stopped_words(qint64 aSeconds) -> QString;
	/// \brief [[BPR-2038]]. The agent's name as a turn's ending line says
	/// it: the product, without its "Code" ("Claude Code" is "Claude").
	/// Any other name is said as it is.
	[[nodiscard]] auto agent_short_name(const QString& aAgent) -> QString;
	/// \brief [[BPR-2241]]. A settled turn's ending, the first part of its
	/// line: `agent_run_answered_words` with the agent's short name when it
	/// answered, else `agent_run_stopped_words`. A turn restored without its
	/// duration says "Claude answered" or "Stopped".
	[[nodiscard]] auto agent_turn_ending_words(const QString& aAgent, bool aAnswered, std::optional<qint64> aSeconds) -> QString;
	/// \brief [[BPR-2261]]. As above, for a turn kept with how it ended: one
	/// the application's closing stopped says so, "Stopped when the app
	/// closed, after 16:46", rather than only "Stopped after 16:46".
	[[nodiscard]] auto agent_turn_ending_words(const QString& aAgent, core::agent_turn_outcome aOutcome, std::optional<qint64> aSeconds) -> QString;

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

	/// \brief [[BPR-2005]], RFC-BPR-0103. Under a turn's line: one row for
	/// each helper the runtime reported (its description, its state and what
	/// it last did, its time), the quiet notice once nothing has come for two
	/// minutes, and, after the turn, the folded line that opens to the rows.
	///
	/// \details The Agent tab's turns and the tour side chat's use this one
	/// block. It presents what it is told and the time on
	/// `agent_activity_clock_ms`; the core decides when the run is quiet. It
	/// never stops a run by itself.
	class agent_activity_view final : public QWidget
	{
	public:
		explicit agent_activity_view(QWidget* aParent = nullptr);
		/// \brief Live: the agent's name, its activity, and when the run
		/// started on the activity clock.
		void present(const QString& aAgent, const core::agent_turn_activity& aActivity, qint64 aStartedMs);
		/// \brief Ended: the folded line, closed, over the same rows. Nothing
		/// shows for a turn that reported no work.
		void fold(const core::agent_turn_activity& aActivity, qint64 aSeconds);
		/// \brief Stop and keep what arrived (the notice's first choice).
		void set_stop(std::function<void()> aStop);

		[[nodiscard]] auto live() const noexcept -> bool { return Live; }
		[[nodiscard]] auto quiet_shown() const -> bool;
		[[nodiscard]] auto helper_rows() const -> int;
		/// One row's words, as shown: description, then state and what it
		/// last did, then its time, tab separated.
		[[nodiscard]] auto helper_row_text(int aRow) const -> QString;
		[[nodiscard]] auto summary_text() const -> QString;
		/// \brief [[BPR-2285]]. The folded rows' commands, in order.
		[[nodiscard]] auto command_rows() const -> int;
		/// \brief A command row's words as shown, shortened to its first line.
		[[nodiscard]] auto command_row_text(int aRow) const -> QString;
		/// \brief The row's disclosure, closed by default; null when the row
		/// already shows the whole command.
		[[nodiscard]] auto command_disclosure(int aRow) const -> QToolButton*;
		/// \brief The whole command, shown while the row is open.
		[[nodiscard]] auto command_full(int aRow) const -> QLabel*;
		[[nodiscard]] auto rows_open() const -> bool;
		[[nodiscard]] auto quiet_text() const -> QString;
		[[nodiscard]] auto keep_waiting_button() const noexcept -> QPushButton* { return KeepWaiting; }
		[[nodiscard]] auto stop_button() const noexcept -> QPushButton* { return StopKeeping; }
		[[nodiscard]] auto summary_button() const noexcept -> QToolButton* { return Summary; }
		/// Recomputes the times and the notice now, as the second's tick does.
		void tick();

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void rebuild_rows();
		/// The folded line, cut to the width it has; whole in its tooltip.
		void fit_summary();

		QString                   Agent;
		core::agent_turn_activity Activity;
		qint64                    StartedMs = 0;
		std::optional<qint64>     KeptWaitingAtMs;
		bool                      Live        = false;
		QTimer*                   Ticker      = nullptr;
		QToolButton*              Summary     = nullptr;
		QWidget*                  Rows        = nullptr;
		QGridLayout*              RowGrid     = nullptr;
		QWidget*                  Quiet       = nullptr;
		QLabel*                   QuietTitle  = nullptr;
		QLabel*                   QuietWords  = nullptr;
		QPushButton*              StopKeeping = nullptr;
		QPushButton*              KeepWaiting = nullptr;
		std::function<void()>     Stop;
		/// [[BPR-2285]]. A command row: its shortened words, the disclosure
		/// (null when nothing was left out) and the whole command.
		struct command_row
		{
			QLabel*      Short  = nullptr;
			QToolButton* Toggle = nullptr;
			QLabel*      Full   = nullptr;
		};
		std::vector<command_row> CommandRows;
		/// The commands the reviewer opened, kept when the rows are drawn again.
		QSet<QString> OpenCommands;
	};
}

#endif
