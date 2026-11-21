#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP

#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"
#include "bpr/qt/agent_entry_dialog.hpp"
#include "bpr/qt/folder_list_editor.hpp"

#include <QPointer>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QDialog;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	namespace detail {
		class list_row_handles;
	}

	/// \brief [[BPR-1753]], RFC-BPR-0092, owner amendment 2026-10-23
	/// ("for the actual settings it should be more like add agent"). Settings ›
	/// Agents: the agents the Agent tab offers, in order, with
	/// **Add agent…** (Claude Code and Codex when found and not yet added, or a
	/// custom agent), **Edit…**, **Remove**, **Move up** and **Move down**, and
	/// **Set up agent…**.
	/// \brief [[BPR-1859]]. What Check agent on Settings shows: the
	/// setup as published, the core's offer, and why the last workspace check
	/// could not make the workspace.
	struct agent_check_presentation
	{
		application::agent_setup_projection                            Setup;
		core::agent_check_decision                                     Decision;
		std::optional<application::agent_workspace_unavailable_reason> WorkspaceReason;
	};

	class agent_programs_panel final : public QWidget
	{
	public:
		using answer = std::function<void(std::optional<core::agent_entry_refusal>)>;
		struct dispatch
		{
			/// [[BPR-1859]]. Check agent and its Stop: the setup's own check,
			/// through the same host as Check it works.
			std::function<void()> Check;
			std::function<void()> StopCheck;
			/// [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"). Check
			/// all, and one agent's check from its row's menu.
			std::function<void()>              CheckAll;
			std::function<void(std::uint32_t)> CheckEntry;
			/// Owner look v006: Reset agents…, which asks before it clears.
			std::function<void()>                          Reset;
			std::function<void(core::agent_entry, answer)> Add;
			std::function<void(core::agent_entry, answer)> Update;
			std::function<void(std::uint32_t)>             Remove;
			std::function<void(std::uint32_t, int)>        Move;
			/// Owner decision 2026-10-23: the run time limit in minutes, 0 for
			/// none. Answers whether it was taken.
			std::function<bool(std::uint32_t)> SetRunLimit;
			/// RFC-BPR-0126 Q4/Q5: Queue (false) or Steer for a follow-up.
			std::function<void(bool)> SetFollowUpSteer;
			/// RFC-BPR-0104 as amended 2026-11-11 (owner look e97c6a3d): the
			/// scratch folder limit in MiB and the days an unused folder is
			/// kept (0: until cleared), answering whether they were taken;
			/// Clear scratch folders now.
			std::function<bool(std::uint32_t, std::uint32_t)> SetScratchPolicy;
			std::function<void()>                             ClearScratch;
			/// [[BPR-2293]]. The days an unused retained agent workspace is
			/// kept (0: until removed) and the GB they may hold together,
			/// answering whether they were taken; Remove every workspace not
			/// in use, after the panel's own question.
			std::function<bool(std::uint32_t, std::uint32_t)> SetWorkspacePolicy;
			std::function<void()>                             RemoveWorkspaces;
			/// Kept for the built-in profiles: an empty path clears it.
			std::function<void(core::setup_agent, std::string)> SetProgram;
			std::function<void(core::codex_protocol_choice)>    SetProtocol;
			/// [[BPR-2254]]. Opens Settings › Storage & privacy, whose
			/// Troubleshooting resets settings ([[BPR-1602]]).
			std::function<void()> OpenTroubleshooting;
		};

		explicit agent_programs_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		/// \brief The list, and which popular agents were found on this
		/// computer (offered by Add agent… until they are added).
		void present(const application::agent_setup_preferences& aPreferences, std::vector<core::agent_detection> aFound = { });
		/// \brief [[BPR-2254]]. The saved agents could not be read from
		/// `aFile` (empty: they were read). The page says so where the list
		/// would be, offers Troubleshooting, and takes no change; it never
		/// shows the empty list a fresh profile has.
		void               present_settings_unreadable(const QString& aFile);
		[[nodiscard]] auto unreadable_notice() const noexcept -> QWidget* { return Unreadable; }
		[[nodiscard]] auto unreadable_text() const noexcept -> QLabel* { return UnreadableText; }
		[[nodiscard]] auto troubleshooting_button() const noexcept -> QPushButton* { return Troubleshooting; }
		/// The answer to a SetProgram: nothing when it was saved.
		void show_answer(core::setup_agent aAgent, const std::optional<core::agent_wrapper_refusal>& aRefusal);
		/// \brief How Add and Edit ask for the entry; a test answers itself.
		void set_editor(agent_entry_editor aEditor);
		/// \brief Opens Add agent… for one preset (Claude Code or Codex), or
		/// for a custom agent with none.
		void add_agent(std::optional<core::setup_agent> aPreset);

		[[nodiscard]] auto agent_list() const noexcept -> QListWidget* { return List; }
		[[nodiscard]] auto add_button() const noexcept -> QPushButton* { return AddButton; }
		[[nodiscard]] auto edit_button() const noexcept -> QPushButton* { return EditButton; }
		[[nodiscard]] auto remove_button() const noexcept -> QPushButton* { return RemoveButton; }
		[[nodiscard]] auto up_button() const noexcept -> QPushButton* { return UpButton; }
		[[nodiscard]] auto down_button() const noexcept -> QPushButton* { return DownButton; }
		/// \brief [[BPR-2318]]. The row's grip and trash, or null.
		[[nodiscard]] auto agent_grip(int aRow) const -> QWidget*;
		[[nodiscard]] auto agent_trash(int aRow) const -> QToolButton*;
		/// \brief [[BPR-2318]]. The question a row's trash, Remove or Delete
		/// asks before an agent is removed, while it is open, or null.
		[[nodiscard]] auto remove_question() const noexcept -> QDialog* { return RemoveQuestion; }
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }
		[[nodiscard]] auto run_limit_toggle() const noexcept -> QCheckBox* { return RunLimitOn; }
		[[nodiscard]] auto run_limit_minutes() const noexcept -> QSpinBox* { return RunLimit; }
		/// \brief What the scratch folders hold now: bytes and folders; none
		/// while unmeasured. A cleanup's answer is said beside it.
		void               present_scratch_usage(std::optional<std::pair<std::uint64_t, std::size_t>> aUsage, const QString& aSaid = { });
		[[nodiscard]] auto scratch_limit() const noexcept -> QSpinBox* { return ScratchLimit; }
		[[nodiscard]] auto scratch_keep_toggle() const noexcept -> QCheckBox* { return ScratchKeepOn; }
		[[nodiscard]] auto scratch_keep_days() const noexcept -> QSpinBox* { return ScratchKeepDays; }
		[[nodiscard]] auto scratch_usage() const noexcept -> QLabel* { return ScratchUsage; }
		[[nodiscard]] auto scratch_clear_button() const noexcept -> QPushButton* { return ScratchClear; }
		/// \brief [[BPR-2293]]. The retained agent workspaces: how many, the
		/// bytes they hold and how many a turn uses; none while unmeasured. A
		/// removal's answer is said beside it.
		struct workspace_usage
		{
			std::size_t   Count = 0U;
			std::uint64_t Bytes = 0U;
			std::size_t   InUse = 0U;
		};
		void present_workspace_usage(std::optional<workspace_usage> aUsage, const QString& aSaid = { });
		/// \brief Remove…: asks first, then dispatches `RemoveWorkspaces`.
		void               remove_workspaces();
		[[nodiscard]] auto workspace_cap() const noexcept -> QSpinBox* { return WorkspaceCap; }
		[[nodiscard]] auto workspace_keep_toggle() const noexcept -> QCheckBox* { return WorkspaceKeepOn; }
		[[nodiscard]] auto workspace_keep_days() const noexcept -> QSpinBox* { return WorkspaceKeepDays; }
		[[nodiscard]] auto workspace_usage_label() const noexcept -> QLabel* { return WorkspaceUsage; }
		[[nodiscard]] auto workspace_remove_button() const noexcept -> QPushButton* { return WorkspaceRemove; }
		/// \brief The question Remove… asks while it is open, or null.
		[[nodiscard]] auto workspace_question() const noexcept -> QDialog* { return WorkspaceQuestion; }

		/// \brief [[BPR-1859]]. Check agent: offered, running with Stop, or
		/// disabled with its reason; its rows, a failure's words and remedy,
		/// and its details, all on the page.
		void               present_check(const agent_check_presentation& aCheck);
		[[nodiscard]] auto check_button() const noexcept -> QPushButton* { return CheckButton; }
		/// \brief [[BPR-2305]]. Use the start of the prompt: shown when a
		/// custom Codex's check found its developer instructions ignored.
		[[nodiscard]] auto switch_instructions_button() const noexcept -> QPushButton* { return SwitchInstructions; }
		/// \brief [[BPR-2051]]. A roster row as read aloud and as tests read
		/// it: the agent's name, its line under the name, and its state.
		[[nodiscard]] auto roster_row_text(int aRow) const -> QString;
		[[nodiscard]] auto stop_check_button() const noexcept -> QPushButton* { return StopCheckButton; }
		/// The line beside the button: what the check does, what runs, or why
		/// it cannot run.
		[[nodiscard]] auto check_note() const noexcept -> QLabel* { return CheckNote; }
		/// The rows and a failure's words, as plain text, for tests and
		/// accessibility.
		[[nodiscard]] auto check_result_text() const -> QString;

	protected:
		/// The marks' colours come from the palette: a palette or style change
		/// draws them again.
		void changeEvent(QEvent* aEvent) override;
		/// [[BPR-2318]]. Return and Enter on the list open the selected agent.
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override;

	private:
		void                                    build_check(QVBoxLayout& aColumn, QHBoxLayout& aRow);
		std::optional<agent_check_presentation> ShownCheck;
		QPushButton*                            CheckButton        = nullptr;
		QPushButton*                            StopCheckButton    = nullptr;
		QWidget*                                CheckProgress      = nullptr;
		QLabel*                                 CheckNote          = nullptr;
		QWidget*                                CheckRows          = nullptr;
		QLabel*                                 AgentMark          = nullptr;
		QLabel*                                 AgentRow           = nullptr;
		QWidget*                                WorkspaceLine      = nullptr;
		QLabel*                                 WorkspaceMark      = nullptr;
		QLabel*                                 WorkspaceRow       = nullptr;
		QLabel*                                 CheckFailure       = nullptr;
		QWidget*                                SignInLine         = nullptr;
		QLabel*                                 SignInCommand      = nullptr;
		QPushButton*                            SwitchInstructions = nullptr;
		std::optional<std::uint32_t>            SwitchingEntry;
		void                                    switch_instructions();
		QToolButton*                            DetailsToggle   = nullptr;
		QPlainTextEdit*                         DetailsText     = nullptr;
		QLabel*                                 WorkspaceReason = nullptr;
		std::string                             ShownDetail;
		void                                    settle_buttons();
		[[nodiscard]] auto                      selected() const -> const core::agent_entry*;
		dispatch                                Dispatch;
		agent_entry_editor                      Editor;
		std::vector<core::agent_entry>          Agents;
		std::vector<core::agent_detection>      Found;
		/// [[BPR-2051]]. The kept checks, the agent a check runs on now, and
		/// the roster's rows drawn from them.
		std::vector<application::agent_entry_check_record> Kept;
		std::optional<std::uint32_t>                       Checking;
		void                                               present_roster();
		void                                               say(const QString& aText);
		void                                               open_row_menu(const QPoint& aAt);
		/// [[BPR-2318]]. Asks before the selected agent is removed.
		void                           ask_remove();
		detail::list_row_handles*      Handles = nullptr;
		QPointer<QDialog>              RemoveQuestion;
		QListWidget*                   List            = nullptr;
		QPushButton*                   AddButton       = nullptr;
		QPushButton*                   EditButton      = nullptr;
		QPushButton*                   RemoveButton    = nullptr;
		QPushButton*                   UpButton        = nullptr;
		QPushButton*                   DownButton      = nullptr;
		QLabel*                        Status          = nullptr;
		QCheckBox*                     RunLimitOn      = nullptr;
		QToolButton*                   FollowUpQueue   = nullptr;
		QToolButton*                   FollowUpSteer   = nullptr;
		QSpinBox*                      RunLimit        = nullptr;
		QSpinBox*                      ScratchLimit    = nullptr;
		QCheckBox*                     ScratchKeepOn   = nullptr;
		QSpinBox*                      ScratchKeepDays = nullptr;
		QLabel*                        ScratchUsage    = nullptr;
		QPushButton*                   ScratchClear    = nullptr;
		void                           send_scratch_policy();
		QSpinBox*                      WorkspaceCap      = nullptr;
		QCheckBox*                     WorkspaceKeepOn   = nullptr;
		QSpinBox*                      WorkspaceKeepDays = nullptr;
		QLabel*                        WorkspaceUsage    = nullptr;
		QPushButton*                   WorkspaceRemove   = nullptr;
		QPointer<QDialog>              WorkspaceQuestion;
		std::optional<workspace_usage> ShownWorkspaces;
		void                           send_workspace_policy();
		void                           send_run_limit();
		/// [[BPR-2254]]. The file the saved agents could not be read from.
		QString      UnreadableFile;
		QWidget*     Unreadable      = nullptr;
		QLabel*      UnreadableText  = nullptr;
		QPushButton* Troubleshooting = nullptr;
		QPushButton* ResetButton     = nullptr;
		void         settle_unreadable();
	};

	/// \brief [[BPR-1753]], RFC-BPR-0092 (owner decision D2). Settings ›
	/// Repositories: the clone folders and the linked clones.
	class clone_folders_panel final : public QWidget
	{
	public:
		struct dispatch
		{
			/// The whole ordered list after an edit, a removal or a move.
			std::function<void(std::vector<std::string>)>            SetFolders;
			std::function<void(const application::repository_link&)> Forget;
		};

		explicit clone_folders_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void present(const application::clone_link_projection& aProjection);

		[[nodiscard]] auto folders() const noexcept -> folder_list_editor* { return Folders; }
		[[nodiscard]] auto links() const noexcept -> QListWidget* { return Links; }

	private:
		dispatch                           Dispatch;
		application::clone_link_projection Shown;
		folder_list_editor*                Folders = nullptr;
		QListWidget*                       Links   = nullptr;
		QPushButton*                       Forget  = nullptr;
	};

	/// \brief [[BPR-1753]], RFC-BPR-0092 (owner decision D1). Settings ›
	/// Agents › Review skill: the skill's text, Save, Restore
	/// default, and a newer default's Compare, Use version N and Keep mine.
	class review_skill_panel final : public QWidget
	{
	public:
		struct dispatch
		{
			std::function<void(std::string)> Save;
			/// Restore default and Use version N; true saves a copy to Downloads first.
			std::function<void(bool)> UseBuiltIn;
			std::function<void()>     KeepMine;
		};

		explicit review_skill_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void present(const application::review_skill_projection& aProjection, const std::string& aBuiltIn);
		void show_answer(application::review_skill_answer aAnswer);

		[[nodiscard]] auto editor() const noexcept -> QPlainTextEdit* { return Editor; }
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto offer() const noexcept -> QWidget* { return Offer; }
		/// \brief [[BPR-1917]]. Shown as one of the built-in prompts: the
		/// prompts pane gives its title, purpose and state, so its own heading
		/// and introduction go, and its editor is the prompts' fixed-pitch one.
		void show_as_built_in_prompt(const QFont& aEditorFont);
		/// \brief Whether the profile has its own copy (Changed from built-in).
		[[nodiscard]] auto changed() const noexcept -> bool { return Changed; }

	private:
		dispatch        Dispatch;
		QLabel*         Heading    = nullptr;
		QLabel*         Intro      = nullptr;
		QPushButton*    Restore    = nullptr;
		bool            Changed    = false;
		std::uint64_t   Generation = 0U;
		QPlainTextEdit* Editor     = nullptr;
		QPlainTextEdit* Default    = nullptr;
		QLabel*         Source     = nullptr;
		QLabel*         Status     = nullptr;
		QCheckBox*      KeepCopy   = nullptr;
		QWidget*        Offer      = nullptr;
		QLabel*         OfferText  = nullptr;
		QPushButton*    UseVersion = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP
