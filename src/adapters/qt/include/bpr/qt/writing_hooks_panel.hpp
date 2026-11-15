#ifndef BPR_QT_WRITING_HOOKS_PANEL_HPP
#define BPR_QT_WRITING_HOOKS_PANEL_HPP
#include "bpr/application/skill_guidance.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/writing_hooks.hpp"
#include <QWidget>
#include <array>
#include <functional>
#include <optional>
#include <vector>
class QButtonGroup;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
namespace bpr::qt {
	class skill_completer;
	class review_skill_panel;
	/// \brief [[BPR-1917]]. Agent settings' built-in prompts: a list of every
	/// prompt with what it is for and where it is used, a roomy editor that
	/// says when a prompt is changed from its built-in text, Reset to
	/// built-in, a preview of what a run receives with `/skill` references
	/// resolved ([[BPR-1916]]), and save state that loses nothing on close.
	///
	/// \details Effect-free: Save, Reset and Test are intents the host
	/// performs; the panel keeps unsaved edits per prompt in memory, so
	/// moving between prompts or closing Settings keeps them until the
	/// person saves or discards them.
	class writing_hooks_panel final : public QWidget
	{
	public:
		explicit writing_hooks_panel(QWidget* aParent = nullptr);
		void set_dispatch(std::function<bool(core::writing_hook, std::string)> aSave, std::function<void(core::writing_hook)> aTest);
		void present(const core::writing_hook_settings& aSettings);
		/// \brief [[BPR-1090]] The profile's label helper, which decides whether the feedback hook is sent.
		void present_label_helper(application::review_label_helper aHelper);
		/// \brief [[BPR-1916]]. The skills a prompt can name, for completion and the preview.
		void               present_skills(const application::skill_library& aLibrary);
		void               show_notice(std::string_view aText);
		[[nodiscard]] auto selected_hook() const -> core::writing_hook;
		/// \brief [[BPR-1568]]. Shows one hook, as choosing it in the list does.
		void select_hook(core::writing_hook aHook);
		/// \brief [[BPR-1917]]. Lists the Agent tab's review skill with the
		/// built-in prompts and shows its own editor when it is chosen.
		void adopt_review_skill(QWidget* aPanel);
		/// \brief Shows the review skill's row, as choosing it does.
		void select_review_skill();
		/// \brief [[BPR-1917]]. Whether the review skill is the profile's own
		/// copy: Changed in its row and beside its title, as for the others.
		void present_review_skill_changed(bool aChanged);
		/// \brief Edit or preview what the run receives.
		void show_preview(bool aPreview);
		/// \brief Prompts with edits not saved yet.
		[[nodiscard]] auto unsaved_count() const -> std::size_t;
		/// \brief [[BPR-2017]]. Whether one prompt has edits not saved yet.
		[[nodiscard]] auto has_unsaved(core::writing_hook aHook) const -> bool { return Unsaved[static_cast<std::size_t>(aHook)].has_value(); }
		[[nodiscard]] auto preview_text() const -> QString;
		[[nodiscard]] auto skill_completion() const noexcept -> skill_completer* { return Completer; }

		/// \brief [[BPR-2179]]. One system prompt as the Prompts library lists
		/// it: its group (Tour tab, Comments and notes, Local drafts, Agent
		/// tab), name, where it is used, and Changed or Unsaved.
		struct system_row
		{
			QString Group;
			QString Name;
			QString Usage;
			QString Marker;
			/// A `core::writing_hook`, or the review skill's own key.
			int Key = 0;
		};
		/// \brief [[BPR-2179]]. Every system prompt, in the section's order.
		[[nodiscard]] auto system_rows() const -> std::vector<system_row>;
		/// \brief [[BPR-2179]]. Shows one system prompt, by its key.
		void select_system(int aKey);
		/// \brief [[BPR-2179]]. Shown inside the Prompts library, which lists
		/// the prompts itself: the heading and list are hidden.
		void set_embedded(bool aEmbedded);
		/// \brief [[BPR-2179]]. Told when a row's Changed or Unsaved changes.
		void set_rows_changed(std::function<void()> aChanged) { RowsChanged = std::move(aChanged); }

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void                                                             show_hook();
		void                                                             refresh_state();
		void                                                             refresh_rows();
		void                                                             refresh_preview();
		void                                                             apply_fonts();
		[[nodiscard]] auto                                               saved_text(core::writing_hook aHook) const -> std::string;
		[[nodiscard]] auto                                               shown_text(core::writing_hook aHook) const -> std::string;
		core::writing_hook_settings                                      Settings;
		application::review_label_helper                                 LabelHelper = application::review_label_helper::off;
		application::skill_library                                       Skills;
		std::array<std::optional<std::string>, core::writing_hook_count> Unsaved;
		std::optional<std::pair<core::writing_hook, std::string>>        Undo;
		std::function<bool(core::writing_hook, std::string)>             Save;
		std::function<void(core::writing_hook)>                          Test;
		std::function<void()>                                            RowsChanged;
		QTimer*                                                          AutoSave         = nullptr;
		bool                                                             Loading          = false;
		bool                                                             ReviewSkillShown = false;
		QListWidget*                                                     Hook;
		QWidget*                                                         Pages;
		QWidget*                                                         ReviewPage    = nullptr;
		QLabel*                                                          ReviewTitle   = nullptr;
		QLabel*                                                          ReviewBadge   = nullptr;
		bool                                                             ReviewChanged = false;
		review_skill_panel*                                              ReviewSkill   = nullptr;
		QWidget*                                                         HookPage;
		QLabel*                                                          Title;
		QLabel*                                                          Badge;
		QLabel*                                                          Purpose;
		QLabel*                                                          UsedBy;
		QPushButton*                                                     EditMode;
		QPushButton*                                                     PreviewMode;
		QStackedWidget*                                                  Views;
		QPlainTextEdit*                                                  Instructions;
		QPlainTextEdit*                                                  Preview;
		QLabel*                                                          SkillLine;
		QLabel*                                                          Source;
		QLabel*                                                          Scope;
		QLabel*                                                          Count;
		QLabel*                                                          SaveState;
		QPushButton*                                                     SaveButton;
		QPushButton*                                                     DiscardButton;
		QPushButton*                                                     ResetButton;
		QPushButton*                                                     UndoButton;
		QPushButton*                                                     TestButton = nullptr;
		QLabel*                                                          Status;
		skill_completer*                                                 Completer = nullptr;
	};
}
#endif
