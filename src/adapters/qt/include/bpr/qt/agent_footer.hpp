#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FOOTER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FOOTER_HPP

#include "bpr/core/agent_cost.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_run_status.hpp"

#include <QElapsedTimer>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;

namespace bpr::qt {
	/// \brief [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"). Where
	/// an agent run started, as the footer names it.
	enum class agent_footer_place : std::uint8_t {
		agent_tab,
		tour_chat,
		/// Enhance on a comment, a reply, a note or a tour field.
		enhance,
		draft_tour,
		/// Draft from PR context or Generate comment on the Tour tab.
		tour_tab,
	};
	inline constexpr std::size_t agent_footer_place_count = 5U;

	/// \brief "Agent tab", "Tour chat", "Enhance", "Draft the tour", "Tour tab".
	[[nodiscard]] auto agent_footer_place_name(agent_footer_place aPlace) -> QString;

	/// \brief One live run as the footer says it.
	struct agent_footer_run
	{
		agent_run_status Status;
		/// When it began; an invalid clock says no time yet.
		QElapsedTimer Started;
		/// Stop and keep what arrived; none: it cannot be stopped from here.
		std::function<void()> Stop;
	};

	/// \brief What the run is doing, without the agent's name: "waiting on 2
	/// helpers", "thinking", "making its workspace"; "no activity for 2 min"
	/// once the run is quiet (RFC-BPR-0103's 120 seconds).
	[[nodiscard]] auto agent_footer_doing_words(const agent_run_status& aStatus) -> QString;
	/// \brief Whether the run has sent nothing for RFC-BPR-0103's 120 seconds.
	[[nodiscard]] auto agent_footer_run_quiet(const agent_run_status& aStatus) -> bool;

	/// \brief [[BPR-2051]], owner decision 2026-11-11 ("As we make agent a
	/// bigger part of this tool, I'm thinking we might need to move agent
	/// picker out of the individual drawers and into an always present footer
	/// on the PR window"). The pull request window's agent footer, in its
	/// status bar: what runs and where it started, with Stop; the one agent
	/// picker every agent feature of the pull request uses; and what this
	/// pull request's agent work has cost.
	///
	/// \details Fixed regions, right to left: the session cost, the picker,
	/// then the run (one run with its Stop; several as a count that opens the
	/// list, each with its own Stop; a just-finished run's ending for a few
	/// seconds). The review's own status line, to the left, yields width
	/// first. While the chosen agent needs setup the picker says what is
	/// missing and Agent settings… leads to Settings. It presents what it is
	/// told; the drawers keep each run's detail.
	class agent_footer final : public QWidget
	{
	public:
		explicit agent_footer(std::shared_ptr<agent_run_settings_source> aSource, QWidget* aParent = nullptr);
		~agent_footer() override;
		agent_footer(const agent_footer&)                    = delete;
		auto operator=(const agent_footer&) -> agent_footer& = delete;

		/// \brief A run at `aPlace` started or moved on; none: it ended.
		void present_run(agent_footer_place aPlace, std::optional<agent_footer_run> aRun);
		/// \brief How a run ended, said for a few seconds once nothing else
		/// runs: "answered in 7:03 · $1.84 · 2 findings".
		void present_finished(agent_footer_place aPlace, const QString& aEnding);
		/// \brief What a place's conversation has cost so far; none: not
		/// known. The footer shows their sum.
		void present_cost(agent_footer_place aPlace, std::optional<core::agent_cost> aCost);
		/// \brief The chosen agent needs setup: what is missing ("not signed
		/// in"), or empty when it does not. With no agent at all the picker is
		/// hidden and Set up an agent… leads to Settings.
		/// `aFailed`: a check ran and failed (drawn in the failed check's
		/// colour); otherwise the agent is not checked yet (drawn quiet).
		void present_setup(const QString& aNeed, bool aFailed = false);
		/// \brief Agent settings… and Set up an agent…: Settings › Agents & context.
		void set_settings_dispatch(std::function<void()> aDispatch);
		/// \brief RFC-BPR-0126 AC09: how many messages wait in any session's
		/// queue; Stop all shows while one does or a turn runs.
		void present_queued(std::size_t aQueued);
		/// \brief Stop all, after it has stopped every run it shows: the window
		/// pauses every queue, so nothing starts next.
		void set_stop_all_dispatch(std::function<void()> aDispatch);
		/// \brief How long a finished run's ending stays, in milliseconds.
		void set_finished_hold(int aMilliseconds);

		[[nodiscard]] auto picker() const noexcept -> agent_run_settings_picker* { return Picker; }
		[[nodiscard]] auto run_where() const noexcept -> QLabel* { return Where; }
		[[nodiscard]] auto run_doing() const noexcept -> QLabel* { return Doing; }
		[[nodiscard]] auto run_elapsed() const noexcept -> QLabel* { return Elapsed; }
		[[nodiscard]] auto stop_button() const noexcept -> QToolButton* { return StopButton; }
		[[nodiscard]] auto runs_button() const noexcept -> QToolButton* { return RunsButton; }
		[[nodiscard]] auto runs_menu() const noexcept -> QMenu* { return RunsMenu; }
		[[nodiscard]] auto settings_button() const noexcept -> QPushButton* { return SettingsButton; }
		[[nodiscard]] auto stop_all_button() const noexcept -> QPushButton* { return StopAllButton; }
		[[nodiscard]] auto cost_label() const noexcept -> QLabel* { return Cost; }
		/// \brief The footer as words, left to right, for tests and the
		/// accessible description.
		[[nodiscard]] auto text() const -> QString;
		[[nodiscard]] auto running() const -> std::size_t;
		[[nodiscard]] auto total_cost() const -> std::optional<core::agent_cost>;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void refresh();
		void match_fonts();
		void tick();
		void stop_all();
		void rebuild_menu();

		std::array<std::optional<agent_footer_run>, agent_footer_place_count> Runs;
		std::array<std::optional<core::agent_cost>, agent_footer_place_count> Costs;
		std::array<QString, agent_footer_place_count>                         Endings;
		std::optional<agent_footer_place>                                     Finished;
		QString                                                               Need;
		bool                                                                  NeedFailed = false;
		std::function<void()>                                                 SettingsDispatch;
		std::function<void()>                                                 StopAllDispatch;
		std::size_t                                                           Queued         = 0U;
		QPushButton*                                                          StopAllButton  = nullptr;
		agent_run_settings_picker*                                            Picker         = nullptr;
		QWidget*                                                              RunArea        = nullptr;
		QLabel*                                                               Dot            = nullptr;
		QLabel*                                                               Where          = nullptr;
		QLabel*                                                               Doing          = nullptr;
		QLabel*                                                               Elapsed        = nullptr;
		QToolButton*                                                          StopButton     = nullptr;
		QToolButton*                                                          RunsButton     = nullptr;
		QMenu*                                                                RunsMenu       = nullptr;
		QLabel*                                                               Divider        = nullptr;
		QPushButton*                                                          SettingsButton = nullptr;
		QLabel*                                                               Cost           = nullptr;
		QTimer*                                                               Ticker         = nullptr;
		QTimer*                                                               FinishedTimer  = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FOOTER_HPP
