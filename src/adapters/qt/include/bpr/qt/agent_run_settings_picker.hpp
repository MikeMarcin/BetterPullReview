#ifndef BPR_QT_AGENT_RUN_SETTINGS_PICKER_HPP
#define BPR_QT_AGENT_RUN_SETTINGS_PICKER_HPP
#include <QObject>
#include <QPointer>
#include <QString>
#include <QToolButton>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	class agent_launch_panel;

	/// \brief [[BPR-1753]], owner look v002 ("I really have no clue
	/// what my agent will be actually doing"). What one agent runs with, for
	/// the agent picker's model and effort.
	struct agent_run_settings
	{
		/// The profile these settings are for.
		std::uint64_t Profile = 0U;
		/// The agent's plain name and the program that runs.
		QString Agent;
		QString Program;
		bool    ModelChoosable  = false;
		bool    EffortChoosable = false;
		/// Empty: the agent's own default.
		std::string Model;
		std::string Effort;
		/// Models to offer by name (value, label), besides Other….
		std::vector<std::pair<std::string, QString>> Models;
		std::vector<std::string>                     Efforts;
		/// Why the settings cannot be chosen, when they cannot.
		QString Note;
		/// Owner look v006. The harness is being asked for its models.
		bool Listing = false;
	};

	/// \brief Where a model or effort chosen in the picker goes; answers
	/// whether the agent took it.
	using agent_run_settings_dispatch = std::function<bool(std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort)>;

	/// \brief Owner look v004 ("The models aren't listing their version
	/// numbers"). A model as a reviewer reads it: `claude-sonnet-5-5` is
	/// "Sonnet 5.5", a bare alias such as `sonnet` is "Sonnet (newest)", and
	/// any other name is shown as the agent reported it.
	[[nodiscard]] auto agent_model_display_name(const QString& aModel) -> QString;

	class agent_run_settings_picker;

	/// \brief [[BPR-1878]]. What every agent picker shows: the agents offered
	/// with their run settings, the model each last reported running, and
	/// where a choice goes. One source behind several pickers (the Agent
	/// tab's question field and the Tour tab) keeps them one choice: the
	/// agent chosen is the launch panel's, and each picker follows it.
	class agent_run_settings_source final
	{
	public:
		explicit agent_run_settings_source(agent_launch_panel& aLaunch);
		agent_run_settings_source(const agent_run_settings_source&)                    = delete;
		auto operator=(const agent_run_settings_source&) -> agent_run_settings_source& = delete;

		void present(std::vector<agent_run_settings> aSettings);
		void report_model(std::uint64_t aProfile, QString aModel);
		/// The chosen agent's settings, or none.
		[[nodiscard]] auto current() const -> const agent_run_settings*;
		[[nodiscard]] auto shown() const noexcept -> const std::vector<agent_run_settings>& { return Shown; }
		[[nodiscard]] auto reported(std::uint64_t aProfile) const -> QString;
		/// The chosen agent's name when it is no longer offered; empty otherwise.
		[[nodiscard]] auto unavailable_name() const -> QString;

		/// Choose an agent; answers whether it was taken (the Agent tab
		/// refuses while its agent works).
		std::function<bool(std::uint64_t)> Choose;
		/// Model or effort changed in a picker's menu.
		agent_run_settings_dispatch Dispatch;
		/// The menu opened: ask the harness what it offers.
		std::function<void(std::uint64_t)> Catalog;
		/// Agent settings…
		std::function<void()> Setup;
		/// The agent refused a setting.
		std::function<void(const QString&)> Refused;

		/// Each picker presents again (settings, report or choice changed).
		void refresh();

	private:
		friend class agent_run_settings_picker;
		void attach(agent_run_settings_picker* aPicker);
		void detach(agent_run_settings_picker* aPicker);

		QPointer<agent_launch_panel>            Launch;
		QObject                                 Context;
		std::vector<agent_run_settings>         Shown;
		std::map<std::uint64_t, QString>        ReportedModels;
		std::vector<agent_run_settings_picker*> Pickers;
	};

	/// \brief Owner decision (c) and amendment 2026-10-23: the agent, model
	/// and effort as a pill ("agent · model · effort"); its menu chooses the
	/// agent, the model and the effort, and opens Agent settings. An icon
	/// alone when narrow. Hidden while no agent is chosen; "Unavailable:"
	/// when the chosen agent went (BPR-1823). [[BPR-1878]]: one component,
	/// used by the Agent tab and the Tour tab over one source.
	class agent_run_settings_picker final : public QToolButton
	{
	public:
		explicit agent_run_settings_picker(std::shared_ptr<agent_run_settings_source> aSource, QWidget* aParent = nullptr);
		~agent_run_settings_picker() override;
		agent_run_settings_picker(const agent_run_settings_picker&)                    = delete;
		auto operator=(const agent_run_settings_picker&) -> agent_run_settings_picker& = delete;

		/// Shows the source's current agent and rebuilds the menu (later, if
		/// the menu is open).
		void present();
		/// Icon only, words in the tooltip.
		void set_narrow(bool aNarrow);
		/// The pill's whole words, however it is shortened.
		[[nodiscard]] auto full_text() const -> QString { return Full; }
		[[nodiscard]] auto source() const -> const std::shared_ptr<agent_run_settings_source>& { return Source; }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void present_unavailable();
		void fit();
		void choose(std::uint64_t aProfile);

		std::shared_ptr<agent_run_settings_source> Source;
		QString                                    Full;
		QString                                    Tip;
		bool                                       Narrow    = false;
		bool                                       MenuStale = false;
	};
}
#endif
