#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP

#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/skill_references.hpp"

#include <QDialog>
#include <QList>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTreeWidget;
class QToolButton;
class QTreeWidgetItem;

namespace bpr::qt {
	/// \brief A prompt's name as Settings › Built-in prompts shows it.
	[[nodiscard]] auto prompt_title(std::string_view aPurpose) -> QString;

	/// \brief [[BPR-2017]], RFC-BPR-0107; [[BPR-2198]]. Export Profile
	/// Settings…: the profile's parts to share, as sections to tick, and the
	/// file's exact text as it will be written.
	///
	/// \details Effect-free: Export… asks where through `set_file_chooser`
	/// and closes accepted; the composition writes the bytes. The sections are
	/// Agents, Prompts, Workflows, Identity and Repositories; ticking a section
	/// ticks every item in it and unticking it unticks them, and each starts as
	/// `core::exported_by_default` says (all but Identity). Within Prompts an
	/// edited prompt starts chosen and a prompt not edited is offered with its
	/// shipped text (owner, 2026-11-11), unchosen; a prompt with edits not
	/// saved yet cannot be chosen until they are saved or discarded.
	class agent_configuration_export_dialog final : public QDialog
	{
	public:
		/// \details [[BPR-2283]]: `aTheme`, the theme on screen, is the Theme
		/// section; none leaves the section out. [[BPR-2301]]: `aReview`, the
		/// profile's Settings › Review, is the Review section, likewise.
		agent_configuration_export_dialog(application::agent_configuration_snapshot aSnapshot, std::array<bool, core::writing_hook_count + 1U> aUnsaved, std::string aExportedBy,
										  core::profile_facts aProfile = { }, QWidget* aParent = nullptr, std::optional<core::portable_theme> aTheme = std::nullopt,
										  std::optional<core::portable_review> aReview = std::nullopt);
		/// \brief How Export… asks for the file; none answers cancelled.
		void set_file_chooser(std::function<QString()> aChooser);
		/// \brief RFC-BPR-0125 AC24. The UUIDs the host minted for this export:
		/// a new pack's ID, and one for each own prompt made before UUIDs. With
		/// none, suggested prompts cannot be chosen.
		void set_minted_identities(std::string aNewPack, std::vector<std::string> aFresh);
		/// \brief RFC-BPR-0125 AC24. The suggested prompts ticked, by ID, in
		/// library order; the pack chosen (none for a new one) and its name.
		[[nodiscard]] auto suggested_prompts() const -> std::vector<std::string>;
		/// \brief The prompt pack the current choice exports and records; none
		/// when no suggested prompt is ticked or the pack has no name.
		[[nodiscard]] auto prompt_pack() const -> const std::optional<core::prompt_pack_export>& { return Pack; }
		[[nodiscard]] auto pack_choice() const noexcept -> QComboBox* { return PackChoice; }
		[[nodiscard]] auto pack_name() const noexcept -> QLineEdit* { return PackName; }
		[[nodiscard]] auto snapshot() const noexcept -> const application::agent_configuration_snapshot& { return Snapshot; }

		[[nodiscard]] auto agents() const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto prompts() const -> std::vector<application::exported_prompt>;
		/// \brief [[BPR-2032]]. The workflows chosen, by ID; their agents travel with them.
		[[nodiscard]] auto workflows() const -> std::vector<std::string>;
		/// \brief [[BPR-2198]]. Whether Identity is ticked, and the repositories ticked.
		[[nodiscard]] auto identity_chosen() const -> bool;
		[[nodiscard]] auto repositories() const -> std::vector<core::portable_repository>;
		/// \brief [[BPR-2283]]. Whether the Theme is ticked.
		[[nodiscard]] auto theme_chosen() const -> bool;
		/// \brief [[BPR-2301]]. Whether Review is ticked.
		[[nodiscard]] auto review_chosen() const -> bool;
		/// \brief A section's row; ticking it ticks the section.
		[[nodiscard]] auto section(core::profile_settings_section aSection) const -> QTreeWidgetItem*;
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
		core::portable_identity                   Identity;
		std::vector<core::portable_repository>    Repositories;
		std::optional<core::portable_theme>       Theme;
		std::optional<core::portable_review>      Review;
		std::string                               Bytes;
		QString                                   Path;
		std::function<QString()>                  Chooser;
		/// RFC-BPR-0125 AC24.
		std::vector<core::prompt_pack_here>     Packs;
		std::string                             NewPack;
		std::vector<std::string>                Fresh;
		std::optional<core::prompt_pack_export> Pack;
		QComboBox*                              PackChoice   = nullptr;
		QLineEdit*                              PackName     = nullptr;
		QTreeWidget*                            Items        = nullptr;
		QLineEdit*                              Label        = nullptr;
		QPlainTextEdit*                         Exact        = nullptr;
		QLabel*                                 Size         = nullptr;
		QPushButton*                            ExportButton = nullptr;
		QPushButton*                            AllPrompts   = nullptr;
	};

	/// \brief [[BPR-2017]], RFC-BPR-0107; [[BPR-2032]], design
	/// workflow-import-v1, B (the owner's choice, 2026-11-15). Import
	/// configuration…'s preview: the Prompt library's shape, a ticked list
	/// of the file's items at the left and the selected item in full at the
	/// right, before anything is saved.
	///
	/// \details Effect-free: Choose program… asks through
	/// `set_program_chooser`; Import closes accepted, and the composition
	/// applies `agent_choices`, `prompt_choices` and `workflow_choices`. The
	/// core decides each item's standing (New, Updated, Unchanged, or cannot
	/// be imported) and its starting choice (`core::plan_configuration_import`):
	/// everything the file can bring starts chosen, so Import is enabled as the
	/// window opens. Each item's choice is one segmented control (Add, Use …,
	/// Leave out; or Update, Leave out) and its tick in the list. A custom
	/// agent added waits for its program and is left out until it has one;
	/// relaxed run checks and builds apply only when ticked.
	class agent_configuration_import_dialog final : public QDialog
	{
	public:
		struct facts
		{
			/// Skills a prompt can name on this computer.
			std::vector<core::skill_entry> Skills;
			/// Popular agents setup found here.
			std::vector<core::setup_agent> Found;
			/// The profile the import lands in, as Settings names it.
			std::string Profile;
		};
		agent_configuration_import_dialog(application::agent_configuration_preview aPreview, facts aFacts, QWidget* aParent = nullptr);
		/// \brief How Choose program… asks; an empty answer keeps the binding.
		void set_program_chooser(std::function<QString(const QString& aHint)> aChooser);
		/// \brief Says why an application did not happen, in the dialog.
		void show_problem(const QString& aText);

		[[nodiscard]] auto agent_choices() const -> std::vector<core::agent_import_choice>;
		[[nodiscard]] auto prompt_choices() const -> std::vector<std::size_t>;
		/// \brief [[BPR-2032]]. The workflows chosen, with the programs bound
		/// here; agents are bound by the agent choices.
		[[nodiscard]] auto workflow_choices() const -> std::vector<core::workflow_import_choice>;
		/// \brief [[BPR-2198]]. Whether Identity is chosen, and the
		/// repositories chosen (indexes into the file's); the composition
		/// applies them through `core::profile_changes`.
		[[nodiscard]] auto identity_choice() const -> bool;
		[[nodiscard]] auto repository_choices() const -> std::vector<std::size_t>;
		/// \brief [[BPR-2283]]. The palette Import keeps for the theme on screen
		/// (`preview().Theme->OnScreen`); none when the Theme is not chosen.
		[[nodiscard]] auto theme_choice() const -> std::optional<core::appearance_palette>;
		/// \brief [[BPR-2283]]. The Theme's plan; none when the file has none.
		[[nodiscard]] auto theme_plan() const -> const std::optional<core::theme_import_plan>& { return ThemePlan; }
		/// \brief [[BPR-2301]]. The Review settings Import keeps
		/// (`preview().Review->Imported`); none when Review is not chosen.
		[[nodiscard]] auto review_choice() const -> std::optional<application::review_behaviour_settings>;
		/// \brief [[BPR-2301]]. Review's plan; none when the file has none.
		[[nodiscard]] auto review_plan() const -> const std::optional<core::review_import_plan>& { return ReviewPlan; }
		[[nodiscard]] auto preview() const -> const application::agent_configuration_preview& { return Preview; }
		[[nodiscard]] auto plan() const -> const core::configuration_import_plan& { return Plan; }
		/// \brief RFC-BPR-0125 AC24. The prompt pack's plan (empty without
		/// one) and the choices made: prompts taken and dropped ones removed.
		[[nodiscard]] auto pack_plan() const -> const core::prompt_pack_import_plan& { return PackPlan; }
		[[nodiscard]] auto prompt_pack_choices() const -> core::prompt_pack_choices;

		/// \brief Shows one file item, as clicking its row does.
		void select_agent(std::size_t aItem);
		void select_prompt(std::size_t aItem);
		void select_workflow(std::size_t aItem);
		void select_identity();
		void select_repository(std::size_t aItem);
		/// [[BPR-2283]]. The file's Theme.
		void select_theme();
		/// [[BPR-2301]]. The file's Review.
		void select_review();
		/// RFC-BPR-0125 AC24. One suggested prompt of the file; one the pack dropped.
		void select_suggested(std::size_t aItem);
		void select_dropped(std::size_t aItem);

		[[nodiscard]] auto items() const noexcept -> QListWidget* { return Items; }
		/// \brief The shown item's choice, one button per part (Add, Use …,
		/// Leave out); none for an item with nothing to choose.
		[[nodiscard]] auto choice_buttons() const -> QList<QToolButton*>;
		[[nodiscard]] auto detail() const noexcept -> QWidget* { return Detail; }
		/// The shown agent's parts; null while another item is shown.
		[[nodiscard]] auto program_button() const noexcept -> QPushButton* { return ProgramButton; }
		[[nodiscard]] auto program_line() const noexcept -> QLabel* { return ProgramLine; }
		[[nodiscard]] auto relaxed_checks() const noexcept -> QCheckBox* { return Relaxed; }
		[[nodiscard]] auto builds() const noexcept -> QCheckBox* { return Builds; }
		/// \brief What the shown item's choice does, in a sentence or two.
		[[nodiscard]] auto outcome() const noexcept -> QLabel* { return Outcome; }
		/// \brief The shown agent's settings: what it is set to, or yours
		/// beside the file's for an update.
		[[nodiscard]] auto settings_table() const noexcept -> QWidget* { return Settings; }
		/// The shown workflow's parts.
		[[nodiscard]] auto workflow_comparison() const noexcept -> QWidget* { return Comparison; }
		[[nodiscard]] auto workflow_program_buttons() const -> QList<QPushButton*>;
		/// The shown prompt's texts.
		[[nodiscard]] auto yours() const noexcept -> QPlainTextEdit* { return Yours; }
		[[nodiscard]] auto theirs() const noexcept -> QPlainTextEdit* { return Theirs; }
		[[nodiscard]] auto summary() const noexcept -> QLabel* { return Summary; }
		[[nodiscard]] auto apply_button() const noexcept -> QPushButton* { return ApplyButton; }

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		enum class item_kind : std::uint8_t {
			agent,
			prompt,
			workflow,
			identity,
			repository,
			/// RFC-BPR-0125 AC24. A suggested prompt of the file's pack.
			suggested,
			/// A prompt here that the pack dropped.
			dropped,
			/// [[BPR-2283]]. The file's Theme.
			theme,
			/// [[BPR-2301]]. The file's Review.
			review,
		};
		struct agent_state
		{
			core::agent_import_action Action = core::agent_import_action::leave;
			std::uint32_t             Target = 0U;
			std::string               Program;
			bool                      AcceptRelaxed = false;
			bool                      AcceptBuilds  = false;
			/// The choice a tick brings back.
			core::agent_import_action Ticked = core::agent_import_action::leave;
		};
		/// [[BPR-2032]]. Each file workflow's choice and the programs bound for it.
		struct workflow_state
		{
			core::workflow_import_action        Action = core::workflow_import_action::leave;
			std::vector<core::workflow_program> Programs;
		};
		void               fill_list();
		void               refresh_rows();
		void               show_selected();
		void               show_agent(std::size_t aItem, QWidget& aPage);
		void               show_prompt(std::size_t aItem, QWidget& aPage);
		void               show_workflow(std::size_t aItem, QWidget& aPage);
		void               show_refused(const QString& aName, const QString& aWhy, QWidget& aPage);
		void               show_identity(QWidget& aPage);
		void               show_repository(std::size_t aItem, QWidget& aPage);
		void               show_suggested(std::size_t aItem, QWidget& aPage);
		void               show_dropped(std::size_t aItem, QWidget& aPage);
		void               show_theme(QWidget& aPage);
		void               show_review(QWidget& aPage);
		void               choice_row(item_kind aKind, std::size_t aItem, const QString& aTake, QWidget& aPage, const QString& aLeave = { });
		[[nodiscard]] auto tag(item_kind aKind, std::size_t aItem) const -> QString;
		[[nodiscard]] auto tone(item_kind aKind, std::size_t aItem) const -> int;
		void               choose(item_kind aKind, std::size_t aItem, int aPart, std::uint32_t aTarget = 0U);
		void               select(item_kind aKind, std::size_t aItem);
		[[nodiscard]] auto row_line(item_kind aKind, std::size_t aItem) const -> QString;
		[[nodiscard]] auto standing(item_kind aKind, std::size_t aItem) const -> core::import_standing;
		[[nodiscard]] auto theme_problem() const -> QString;
		[[nodiscard]] auto review_problem() const -> QString;
		[[nodiscard]] auto chosen(item_kind aKind, std::size_t aItem) const -> bool;
		[[nodiscard]] auto bound(std::size_t aItem) const -> bool;
		[[nodiscard]] auto local_name(std::uint32_t aId) const -> QString;
		[[nodiscard]] auto asking_actions(std::size_t aAgent) const -> QStringList;

		application::agent_configuration_preview Preview;
		facts                                    Facts;
		core::configuration_import_plan          Plan;
		std::vector<agent_state>                 Agents;
		std::vector<bool>                        Prompts;
		std::vector<workflow_state>              Workflows;
		std::vector<bool>                        Repositories;
		/// RFC-BPR-0125 AC24.
		core::prompt_pack_import_plan PackPlan;
		std::vector<bool>             Suggested;
		std::vector<bool>             Removed;
		bool                          IdentityChosen = false;
		/// [[BPR-2283]].
		std::optional<core::theme_import_plan> ThemePlan;
		bool                                   ThemeChosen = false;
		/// [[BPR-2301]].
		std::optional<core::review_import_plan>          ReviewPlan;
		bool                                             ReviewChosen = false;
		std::optional<std::pair<item_kind, std::size_t>> Shown;
		/// [[BPR-2204]]. The item the detail was last built for.
		std::optional<std::pair<item_kind, std::size_t>> Built;
		std::function<QString(const QString&)>           Chooser;
		bool                                             Filling       = false;
		QListWidget*                                     Items         = nullptr;
		QScrollArea*                                     Scroll        = nullptr;
		QWidget*                                         Detail        = nullptr;
		QButtonGroup*                                    Choice        = nullptr;
		QPushButton*                                     ProgramButton = nullptr;
		QLabel*                                          ProgramLine   = nullptr;
		QCheckBox*                                       Relaxed       = nullptr;
		QCheckBox*                                       Builds        = nullptr;
		QLabel*                                          Outcome       = nullptr;
		QWidget*                                         Settings      = nullptr;
		QWidget*                                         Comparison    = nullptr;
		QPlainTextEdit*                                  Yours         = nullptr;
		QPlainTextEdit*                                  Theirs        = nullptr;
		QLabel*                                          Summary       = nullptr;
		QLabel*                                          Problem       = nullptr;
		QPushButton*                                     ApplyButton   = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_CONFIGURATION_DIALOGS_HPP
