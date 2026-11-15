#ifndef BPR_QT_AGENT_RUN_SETTINGS_PICKER_HPP
#define BPR_QT_AGENT_RUN_SETTINGS_PICKER_HPP
#include <QObject>
#include <QPointer>
#include <QString>
#include <QToolButton>

class QMessageBox;

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
		/// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104 as amended 2026-11-11
		/// (owner look e97c6a3d). Two toggles for this pull request under the
		/// model and effort, each explained in its tooltip; none shown when
		/// the agent takes no grant. A toggle is checked only while the launch
		/// carries what it names, and unavailable (with why) when it cannot.
		bool GrantApplies = false;
		/// "Use scratch folder", or "Use scratch folder (full)".
		QString ScratchText;
		bool    ScratchOn      = false;
		bool    ScratchEnabled = false;
		QString ScratchTip;
		bool    BuildsOn      = false;
		bool    BuildsEnabled = false;
		QString BuildsTip;
		/// The session's scratch folder is known and can be opened.
		bool ScratchOpenable = false;
		/// Owner look e97c6a3d ("I'm not even sure if type /MODEL or type
		/// /EFFORT is true in general with custom agents"): the Model and
		/// Effort menus' tooltips say the question field takes /model or
		/// /effort only when it does for this agent; empty otherwise.
		QString ModelHint;
		QString EffortHint;
		/// [[BPR-2139]], RFC-BPR-0104 (owner, 2026-11-12). The agent's own MCP
		/// servers load in its conversations; the tooltip says so.
		bool McpOn = false;
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

	/// \brief [[BPR-2008]], RFC-BPR-0104. Asks, once per pull request, before
	/// the first turn that would build: true allows, false declines (the
	/// default), none when the question was closed.
	[[nodiscard]] auto ask_build_question(QWidget* aParent) -> std::optional<bool>;
	/// \brief The question's box, not yet shown, for `ask_build_question` and
	/// for a render of it.
	[[nodiscard]] auto make_build_question(QWidget* aParent) -> std::unique_ptr<QMessageBox>;

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
		/// [[BPR-2007]]. The scratch folder turned on or off for this pull
		/// request and agent; Open scratch folder.
		std::function<void(std::uint64_t, bool)> ToggleScratch;
		std::function<void(std::uint64_t)>       OpenScratch;
		/// Owner look e97c6a3d. Allow build and run tests turned on or off
		/// for this pull request and agent.
		std::function<void(std::uint64_t, bool)> ToggleBuilds;

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
	/// agent, the model and the effort, and opens Agent settings. Its whole
	/// words always; a narrow field gives it a row of its own ([[BPR-1924]]).
	/// Hidden while no agent is chosen; "Unavailable:"
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
		/// The pill's whole words, however it is shortened.
		[[nodiscard]] auto full_text() const -> QString { return Full; }
		/// \brief [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"). The
		/// chosen agent needs setup: the pill says what is missing after the
		/// agent's name ("Codex · not signed in") with an outline. Owner note
		/// on the footer look ("Why is it red?"): only a check that ran and
		/// failed (`aFailed`) is drawn in the failed check's colour; an agent
		/// not yet checked is drawn in the quiet secondary ink. Empty: it
		/// needs nothing.
		void               set_attention(const QString& aNeed, bool aFailed = false);
		[[nodiscard]] auto attention_failed() const noexcept -> bool { return AttentionFailed; }
		[[nodiscard]] auto attention() const -> const QString& { return Attention; }
		[[nodiscard]] auto source() const -> const std::shared_ptr<agent_run_settings_source>& { return Source; }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;

	private:
		void present_unavailable();
		void fit();
		void choose(std::uint64_t aProfile);

		std::shared_ptr<agent_run_settings_source> Source;
		QString                                    Full;
		QString                                    Tip;
		QString                                    Attention;
		bool                                       AttentionFailed = false;
		bool                                       MenuStale       = false;

		/// The words' inset, and the chevron's width and inset on the right.
		static constexpr int text_left     = 10;
		static constexpr int chevron_width = 7;
		static constexpr int chevron_right = 9;
		/// Everything around the words: their inset, a gap, the chevron.
		static constexpr int chrome_width = text_left + 6 + chevron_width + chevron_right;
	};
}
#endif
