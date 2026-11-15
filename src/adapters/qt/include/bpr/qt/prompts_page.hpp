#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPTS_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPTS_PAGE_HPP

#include "bpr/application/prompt_macro_host.hpp"
#include "bpr/core/prompt_surfaces.hpp"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief [[BPR-2179]] (docs/design/prompts-settings-v4/SPEC.md, with the
/// owner's answers on the card). Settings › Prompts: Surfaces above, the
/// Prompt library below, one page that scrolls in a short window.
///
/// \details Surfaces are where prompts appear: the pull request, file and
/// thread Ask menus and their suggested chips, and the tour's three chats.
/// The library holds every prompt, the system prompts included. A prompt is
/// placed by dragging it up from the library or by Add to; a placement is
/// reordered by dragging it or with Option-Up and Option-Down, and removed
/// with Delete. Every change saves as it is made. The core decides which
/// surfaces may take a prompt and what each surface shows; this page
/// presents and edits values, and hands the whole edited set to the host.
namespace bpr::qt {
	class writing_hooks_panel;

	/// \brief The library's filter.
	enum class prompt_library_filter : std::uint8_t {
		all,
		built_in,
		yours,
		not_on_a_surface,
		system,
	};

	class prompts_page final : public QWidget
	{
	public:
		explicit prompts_page(QWidget* aParent = nullptr);
		~prompts_page() override;
		prompts_page(const prompts_page&)                    = delete;
		auto operator=(const prompts_page&) -> prompts_page& = delete;

		/// \brief The profile's prompts and surfaces, and whether edits can
		/// be made.
		void present(const application::prompt_macro_projection& aProjection);
		/// \brief Saves the whole library, in order, with every surface; the
		/// host stores what differs from the shipped set.
		using save_dispatch = std::function<void(std::vector<core::prompt_macro>, core::prompt_surfaces, std::vector<core::prompt_repository_layer>)>;
		void set_save_dispatch(save_dispatch aDispatch);
		/// \brief The system prompts' editor (today's Built-in prompts): its
		/// rows join the library under System, and it edits the one chosen.
		void adopt_system_prompts(writing_hooks_panel* aPanel);
		/// \brief Ask › Edit Quick Asks…: the page brought into view, the
		/// library's filter field focused.
		void reveal();

		/// \brief [[BPR-2179]] (spec section 11). The repositories Prompts for
		/// lists besides All repositories and those with a layer.
		void set_repositories(const QStringList& aRepositories);
		/// \brief Prompts for: a repository's layer, or all repositories
		/// (empty). Both Surfaces and the library switch.
		void               choose_repository(const std::string& aRepository);
		[[nodiscard]] auto repository() const noexcept -> const std::string& { return Repository; }
		/// \brief In a repository: Use app-wide for this prompt, every field.
		void use_app_wide(const std::string& aId);
		/// \brief In a repository: one field back to the app-wide value
		/// (`name`, `text`, `answer`, `conditions`, `on`).
		void use_app_wide_field(const std::string& aId, const QString& aField);
		/// \brief In a repository: a placement added or removed here back as
		/// it is app-wide.
		void revert_placement(const std::string& aId, core::prompt_surface aSurface);
		/// \brief In a repository: the surface's app-wide order again.
		void revert_order(core::prompt_surface aSurface);
		/// \brief Shows only the prompts that differ here.
		void set_changed_here(bool aOnly);
		/// \brief The layer this repository has, as it would be saved.
		[[nodiscard]] auto layers() const noexcept -> const std::vector<core::prompt_repository_layer>& { return Layers; }

		/// \brief Selects a prompt, as clicking its library row does.
		void select(const std::string& aId);
		/// \brief Selects a system prompt by its key.
		void               select_system(int aKey);
		[[nodiscard]] auto selected() const -> std::optional<std::string>;
		/// \brief Places a prompt, as a drop or Add to does; a prompt already
		/// on the surface moves.
		auto place(const std::string& aId, core::prompt_surface aSurface, std::optional<std::size_t> aAt = std::nullopt) -> core::placement_refusal;
		void remove(const std::string& aId, core::prompt_surface aSurface);
		void move(const std::string& aId, core::prompt_surface aSurface, int aBy);
		void set_on(const std::string& aId, bool aOn);
		/// \brief New prompt: one of the reviewer's own, on no surface,
		/// selected, its name focused.
		auto new_prompt() -> std::string;
		/// \brief Duplicate: the reviewer's own copy, on no surface; none for
		/// a system prompt.
		auto duplicate(const std::string& aId) -> std::optional<std::string>;
		/// \brief Reset to default: a built-in's shipped name, text, Answer
		/// as and conditions; On and placements are untouched.
		void reset_to_default(const std::string& aId);
		/// \brief Deletes one of the reviewer's own prompts and its
		/// placements; `aConfirm` asks first, as the menu does.
		auto delete_prompt(const std::string& aId, bool aConfirm = false) -> bool;
		/// \brief [[BPR-2146]]'s Restore built-ins…: every built-in's shipped
		/// text and On, and the shipped surfaces, the reviewer's own prompts
		/// kept where they are; `aConfirm` asks first, as the button does.
		auto restore_built_ins(bool aConfirm = false) -> bool;
		void set_filter(prompt_library_filter aFilter);
		/// \brief Edits the selected prompt as typing in its fields does.
		void edit_selected(const std::function<void(core::prompt_macro&)>& aEdit);

		[[nodiscard]] auto surfaces() const noexcept -> const core::prompt_surfaces& { return Surfaces; }
		[[nodiscard]] auto library() const noexcept -> const std::vector<core::prompt_macro>& { return Library; }
		[[nodiscard]] auto surface_list(core::prompt_surface aSurface) const noexcept -> QListWidget* { return Lists.at(static_cast<std::size_t>(aSurface)); }
		[[nodiscard]] auto library_list() const noexcept -> QListWidget* { return LibraryList; }
		[[nodiscard]] auto name_field() const noexcept -> QLineEdit* { return Name; }
		[[nodiscard]] auto text_field() const noexcept -> QPlainTextEdit* { return Text; }
		[[nodiscard]] auto add_to_button() const noexcept -> QPushButton* { return AddTo; }
		[[nodiscard]] auto footer_text() const -> QString;
		[[nodiscard]] auto saved_text() const -> QString;
		/// \brief The Add to menu for `aId`, as the editor, a library row and
		/// a placement show it.
		void fill_add_to(QMenu& aMenu, const std::string& aId);
		/// \brief The library row's context menu for `aId`.
		void fill_library_menu(QMenu& aMenu, const std::string& aId);
		/// \brief The words a placement is named with for assistive
		/// technology: "Thread chips, 1 of 4, Summarize, has conditions".
		[[nodiscard]] auto placement_name(core::prompt_surface aSurface, std::size_t aIndex) const -> QString;
		/// \brief The words a drop that is refused says.
		[[nodiscard]] auto last_refusal() const -> QString;

		/// \brief A drop on a surface: from the library (`aFrom` none) or
		/// from another surface, which moves it unless `aCopy`.
		auto               drop(const std::string& aId, std::optional<core::prompt_surface> aFrom, core::prompt_surface aTo, std::size_t aAt, bool aCopy) -> core::placement_refusal;
		[[nodiscard]] auto refusal(const std::string& aId, core::prompt_surface aSurface) const -> core::placement_refusal;

	private:
		void               build_surfaces(QVBoxLayout& aColumn);
		void               build_library(QVBoxLayout& aColumn);
		void               build_editor(QWidget& aEditor);
		void               rebuild_surfaces();
		void               rebuild_library();
		void               refresh_editor();
		void               rebuild_conditions();
		void               add_group_rows(QVBoxLayout& aColumn, core::prompt_macro_body& aBody, const std::vector<std::size_t>& aPath);
		void               fill_condition_menu(QMenu& aMenu, const std::vector<std::size_t>& aPath);
		void               changed(bool aTyping = false);
		void               flush();
		void               show_system(bool aShown);
		[[nodiscard]] auto find(const std::string& aId) -> core::prompt_macro*;
		[[nodiscard]] auto find(const std::string& aId) const -> const core::prompt_macro*;
		[[nodiscard]] auto shipped(const std::string& aId) const -> const core::prompt_macro*;
		[[nodiscard]] auto differs_from_shipped(const core::prompt_macro& aMacro) const -> bool;
		[[nodiscard]] auto shown_in_library(const core::prompt_macro& aMacro) const -> bool;
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		void               resizeEvent(QResizeEvent* aEvent) override;

		void               adopt_layer();
		[[nodiscard]] auto app_entry(const std::string& aId) const -> const core::prompt_macro*;
		[[nodiscard]] auto differs_here(const core::prompt_macro& aMacro) const -> bool;
		[[nodiscard]] auto here_count() const -> int;

		application::prompt_macro_projection Projection;
		/// The app-wide library and surfaces, and every repository's layer;
		/// `Library` and `Surfaces` are what the chosen layer shows.
		std::vector<core::prompt_macro>                             AppLibrary;
		core::prompt_surfaces                                       AppSurfaces;
		std::vector<core::prompt_repository_layer>                  Layers;
		std::string                                                 Repository;
		QStringList                                                 Repositories;
		bool                                                        ChangedHereOnly = false;
		QCheckBox*                                                  ChangedHere     = nullptr;
		QLabel*                                                     Differs         = nullptr;
		QLabel*                                                     RepositoryMark  = nullptr;
		std::array<QLabel*, core::prompt_surface_count>             OrderTags{ };
		std::vector<core::prompt_macro>                             Library;
		std::vector<core::prompt_builtin>                           Shipped;
		core::prompt_surfaces                                       Surfaces;
		std::optional<std::string>                                  Selected;
		std::optional<int>                                          SelectedSystem;
		std::optional<std::pair<core::prompt_surface, std::size_t>> SelectedPlacement;
		prompt_library_filter                                       Filter = prompt_library_filter::all;
		save_dispatch                                               Save;
		writing_hooks_panel*                                        System   = nullptr;
		bool                                                        Filling  = false;
		bool                                                        Dirty    = false;
		bool                                                        InFlight = false;
		QString                                                     Refusal;
		std::array<QListWidget*, core::prompt_surface_count>        Lists{ };
		/// Which condition rows are open, by their place in the tree.
		QSet<QString>   OpenConditions;
		QComboBox*      PromptsFor    = nullptr;
		QLabel*         Caption       = nullptr;
		QLineEdit*      Search        = nullptr;
		QButtonGroup*   Filters       = nullptr;
		QListWidget*    LibraryList   = nullptr;
		QStackedWidget* EditorPages   = nullptr;
		QLineEdit*      Name          = nullptr;
		QLabel*         BuiltInTag    = nullptr;
		QToolButton*    AsQuestion    = nullptr;
		QToolButton*    AsFindings    = nullptr;
		QPlainTextEdit* Text          = nullptr;
		QLabel*         ActionNote    = nullptr;
		QWidget*        InsertRow     = nullptr;
		QLabel*         Bytes         = nullptr;
		QToolButton*    MatchAll      = nullptr;
		QToolButton*    MatchAny      = nullptr;
		QToolButton*    MatchNone     = nullptr;
		QPushButton*    AddCondition  = nullptr;
		QVBoxLayout*    Conditions    = nullptr;
		QLabel*         Problems      = nullptr;
		QPushButton*    AddTo         = nullptr;
		QPushButton*    Duplicate     = nullptr;
		QPushButton*    Reset         = nullptr;
		QLabel*         OnSurfaces    = nullptr;
		QLabel*         SavedLabel    = nullptr;
		QLabel*         Status        = nullptr;
		QTimer*         Typing        = nullptr;
		QBoxLayout*     ToolsRow      = nullptr;
		QBoxLayout*     Body          = nullptr;
		QBoxLayout*     EditorColumns = nullptr;
		QWidget*        RightBox      = nullptr;
	};
} // namespace bpr::qt

#endif
