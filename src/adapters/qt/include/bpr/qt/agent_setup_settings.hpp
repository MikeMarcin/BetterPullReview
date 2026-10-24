#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP

#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"
#include "bpr/qt/agent_entry_dialog.hpp"
#include "bpr/qt/folder_list_editor.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace bpr::qt {
	/// \brief [[BPR-1753]], RFC-BPR-0092, owner amendment 2026-10-23
	/// ("for the actual settings it should be more like add agent"). Settings ›
	/// Agents & context: the agents the Agent tab offers, in order, with
	/// **Add agent…** (Claude Code and Codex when found and not yet added, or a
	/// custom agent), **Edit…**, **Remove**, **Move up** and **Move down**, and
	/// **Set up agent…**.
	class agent_programs_panel final : public QWidget
	{
	public:
		using answer = std::function<void(std::optional<core::agent_entry_refusal>)>;
		struct dispatch
		{
			/// Owner look v006: Reset agents…, which asks before it clears.
			std::function<void()>                          Reset;
			std::function<void(core::agent_entry, answer)> Add;
			std::function<void(core::agent_entry, answer)> Update;
			std::function<void(std::uint32_t)>             Remove;
			std::function<void(std::uint32_t, int)>        Move;
			/// Owner decision 2026-10-23: the run time limit in minutes, 0 for
			/// none. Answers whether it was taken.
			std::function<bool(std::uint32_t)> SetRunLimit;
			/// Kept for the built-in profiles: an empty path clears it.
			std::function<void(core::setup_agent, std::string)> SetProgram;
			std::function<void(core::codex_protocol_choice)>    SetProtocol;
		};

		explicit agent_programs_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		/// \brief The list, and which popular agents were found on this
		/// computer (offered by Add agent… until they are added).
		void present(const application::agent_setup_preferences& aPreferences, std::vector<core::agent_detection> aFound = { });
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
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }
		[[nodiscard]] auto run_limit_toggle() const noexcept -> QCheckBox* { return RunLimitOn; }
		[[nodiscard]] auto run_limit_minutes() const noexcept -> QSpinBox* { return RunLimit; }

	private:
		void                               settle_buttons();
		[[nodiscard]] auto                 selected() const -> const core::agent_entry*;
		dispatch                           Dispatch;
		agent_entry_editor                 Editor;
		std::vector<core::agent_entry>     Agents;
		std::vector<core::agent_detection> Found;
		QListWidget*                       List         = nullptr;
		QPushButton*                       AddButton    = nullptr;
		QPushButton*                       EditButton   = nullptr;
		QPushButton*                       RemoveButton = nullptr;
		QPushButton*                       UpButton     = nullptr;
		QPushButton*                       DownButton   = nullptr;
		QLabel*                            Status       = nullptr;
		QCheckBox*                         RunLimitOn   = nullptr;
		QSpinBox*                          RunLimit     = nullptr;
		void                               send_run_limit();
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
	/// Agents & context › Review skill: the skill's text, Save, Restore
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

	private:
		dispatch        Dispatch;
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
