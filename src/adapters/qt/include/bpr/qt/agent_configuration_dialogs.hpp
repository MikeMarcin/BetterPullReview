#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP

#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/skill_references.hpp"

#include <QDialog>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace bpr::qt {
	/// \brief A prompt's name as Settings › Built-in prompts shows it.
	[[nodiscard]] auto prompt_title(std::string_view aPurpose) -> QString;

	/// \brief [[BPR-2017]], RFC-BPR-0107. Export configuration…: the agents
	/// and prompts to share, and the file's exact text as it will be written.
	///
	/// \details Effect-free: Export… asks where through `set_file_chooser`
	/// and closes accepted; the composition writes the bytes. Every agent and
	/// every edited prompt starts chosen; a prompt not edited is offered with
	/// its shipped text (owner, 2026-11-11) and starts unchosen. A prompt with
	/// edits not saved yet cannot be chosen until they are saved or discarded.
	class agent_configuration_export_dialog final : public QDialog
	{
	public:
		agent_configuration_export_dialog(application::agent_configuration_snapshot aSnapshot, std::array<bool, core::writing_hook_count + 1U> aUnsaved, std::string aExportedBy,
										  QWidget* aParent = nullptr);
		/// \brief How Export… asks for the file; none answers cancelled.
		void set_file_chooser(std::function<QString()> aChooser);

		[[nodiscard]] auto agents() const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto prompts() const -> std::vector<application::exported_prompt>;
		/// \brief The exact bytes the current choice writes; empty when over the bound.
		[[nodiscard]] auto bytes() const -> const std::string& { return Bytes; }
		[[nodiscard]] auto chosen_path() const -> const QString& { return Path; }

		[[nodiscard]] auto items() const noexcept -> QTreeWidget* { return Items; }
		[[nodiscard]] auto label_field() const noexcept -> QLineEdit* { return Label; }
		[[nodiscard]] auto exact_text() const noexcept -> QPlainTextEdit* { return Exact; }
		[[nodiscard]] auto export_button() const noexcept -> QPushButton* { return ExportButton; }
		[[nodiscard]] auto all_prompts_button() const noexcept -> QPushButton* { return AllPrompts; }

	private:
		void                                      refresh();
		application::agent_configuration_snapshot Snapshot;
		std::vector<core::local_prompt>           Local;
		std::string                               ExportedBy;
		std::string                               Bytes;
		QString                                   Path;
		std::function<QString()>                  Chooser;
		QTreeWidget*                              Items        = nullptr;
		QLineEdit*                                Label        = nullptr;
		QPlainTextEdit*                           Exact        = nullptr;
		QLabel*                                   Size         = nullptr;
		QPushButton*                              ExportButton = nullptr;
		QPushButton*                              AllPrompts   = nullptr;
	};

	/// \brief [[BPR-2017]], RFC-BPR-0107. Import configuration…'s preview:
	/// every item of the file, what applying it would change, and the local
	/// bindings it needs, decided item by item before anything is saved.
	///
	/// \details Effect-free: Choose program… asks through
	/// `set_program_chooser`; Import selected closes accepted, and the
	/// composition applies `agent_choices` and `prompt_choices`. An agent that
	/// meets one already here, and an edited prompt here, start left as they
	/// are; a custom agent added waits for its program and is left out until
	/// it has one. Relaxed run checks and builds apply only when ticked.
	class agent_configuration_import_dialog final : public QDialog
	{
	public:
		struct facts
		{
			/// Skills a prompt can name on this computer.
			std::vector<core::skill_entry> Skills;
			/// Popular agents setup found here.
			std::vector<core::setup_agent> Found;
		};
		agent_configuration_import_dialog(application::agent_configuration_preview aPreview, facts aFacts, QWidget* aParent = nullptr);
		/// \brief How Choose program… asks; an empty answer keeps the binding.
		void set_program_chooser(std::function<QString(const QString& aHint)> aChooser);
		/// \brief Says why an application did not happen, in the dialog.
		void show_problem(const QString& aText);

		[[nodiscard]] auto agent_choices() const -> std::vector<core::agent_import_choice>;
		[[nodiscard]] auto prompt_choices() const -> std::vector<std::size_t>;
		[[nodiscard]] auto preview() const -> const application::agent_configuration_preview& { return Preview; }

		/// \brief Shows one file item: an agent's index, or a prompt's.
		void               select_agent(std::size_t aItem);
		void               select_prompt(std::size_t aItem);
		[[nodiscard]] auto items() const noexcept -> QTreeWidget* { return Items; }
		[[nodiscard]] auto action() const noexcept -> QComboBox* { return Action; }
		[[nodiscard]] auto program_button() const noexcept -> QPushButton* { return ProgramButton; }
		[[nodiscard]] auto program_line() const noexcept -> QLabel* { return ProgramLine; }
		[[nodiscard]] auto relaxed_checks() const noexcept -> QCheckBox* { return Relaxed; }
		[[nodiscard]] auto builds() const noexcept -> QCheckBox* { return Builds; }
		[[nodiscard]] auto changes() const noexcept -> QLabel* { return Changes; }
		[[nodiscard]] auto use_prompt() const noexcept -> QCheckBox* { return UsePrompt; }
		[[nodiscard]] auto prompt_note() const noexcept -> QLabel* { return PromptNote; }
		[[nodiscard]] auto summary() const noexcept -> QLabel* { return Summary; }
		[[nodiscard]] auto apply_button() const noexcept -> QPushButton* { return ApplyButton; }

	private:
		struct agent_state
		{
			core::agent_import_action Action = core::agent_import_action::leave;
			std::uint32_t             Target = 0U;
			std::string               Program;
			bool                      AcceptRelaxed = false;
			bool                      AcceptBuilds  = false;
		};
		/// One Action entry: what it does and the entry it names.
		struct action_option
		{
			core::agent_import_action Action = core::agent_import_action::leave;
			std::uint32_t             Target = 0U;
		};
		void                                     show_item(QTreeWidgetItem* aItem);
		void                                     show_agent(std::size_t aItem);
		void                                     show_prompt(std::size_t aItem);
		void                                     refresh_rows();
		[[nodiscard]] auto                       needs_program(std::size_t aItem) const -> bool;
		[[nodiscard]] auto                       bound(std::size_t aItem) const -> bool;
		[[nodiscard]] auto                       row_state(std::size_t aItem) const -> QString;
		application::agent_configuration_preview Preview;
		facts                                    Facts;
		std::vector<agent_state>                 Agents;
		std::vector<bool>                        Prompts;
		std::vector<action_option>               Options;
		std::optional<std::size_t>               ShownAgent;
		std::optional<std::size_t>               ShownPrompt;
		bool                                     Showing = false;
		std::function<QString(const QString&)>   Chooser;
		QTreeWidget*                             Items         = nullptr;
		QStackedWidget*                          Detail        = nullptr;
		QLabel*                                  Title         = nullptr;
		QLabel*                                  Kind          = nullptr;
		QComboBox*                               Action        = nullptr;
		QLabel*                                  ProgramLine   = nullptr;
		QPushButton*                             ProgramButton = nullptr;
		QCheckBox*                               Relaxed       = nullptr;
		QCheckBox*                               Builds        = nullptr;
		QLabel*                                  Changes       = nullptr;
		QLabel*                                  PromptTitle   = nullptr;
		QCheckBox*                               UsePrompt     = nullptr;
		QLabel*                                  PromptNote    = nullptr;
		QPlainTextEdit*                          Yours         = nullptr;
		QPlainTextEdit*                          Theirs        = nullptr;
		QLabel*                                  ProblemTitle  = nullptr;
		QLabel*                                  ProblemText   = nullptr;
		QLabel*                                  Summary       = nullptr;
		QLabel*                                  Problem       = nullptr;
		QPushButton*                             ApplyButton   = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP
