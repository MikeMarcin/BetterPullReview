#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_VIEW_HPP

#include "bpr/application/local_changes_host.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/qt/local_changes_panel.hpp"
#include "bpr/qt/local_draft_preparation_area.hpp"
#include "bpr/qt/view_picker.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <string>

class QComboBox;
class QMenu;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QWidget;

namespace bpr::qt {
	class local_draft_publication_area;
	class local_diff_view;
	class suggestion_editor;

	/// \brief The gestures the local draft page sends to its host.
	struct local_draft_dispatch
	{
		std::function<void(const std::string&)>                     OpenFile;
		std::function<void(const std::string&, bool)>               Include;
		std::function<void()>                                       Refresh;
		std::function<void(std::uint64_t)>                          ShowCandidate;
		std::function<void(const std::string&, const std::string&)> AddNote;
		std::function<void(std::uint64_t)>                          RemoveNote;
		std::function<void(const std::string&)>                     Rename;
		std::function<void(core::local_draft_target)>               ChooseTarget;
		std::function<void()>                                       Rebind;
		/// The page has asked; the host removes the private preparation.
		std::function<void()> Discard;
		std::function<void()> Close;
		/// Author state: a Stage Change or Unstage Change on the shown comparison.
		std::function<void(const std::string&, core::local_change_hunk, bool)> StageChange;
		/// Author state: opens a file of Local Changes' comparison.
		std::function<void(const std::string&)> OpenLocalFile;
		/// Author state: Save writes the edited bytes if the disk still holds the read ones.
		std::function<void(const std::string&, std::vector<std::uint8_t>, std::vector<std::uint8_t>)> Save;
		/// Create Draft PR…: opens the Status tab's steps.
		std::function<void()> CreatePullRequest;
	};

	/// \brief A local PR draft in the pull request window (RFC-BPR-0108 as
	/// amended by the owner 2026-11-12, mockup A): Overview holds the title,
	/// description and files; Review holds the proposed diff; the Files and
	/// Local Changes drawer tabs at the left; Status (the publish steps),
	/// Notes and Tour at the right; the candidate pop-up, the view picker and
	/// Create Draft PR… on the bar. On the author's own worktree (author
	/// state) the bar has the branch with ahead and behind, the picker its
	/// Edit view, and Local Changes its staging, commit and push. Each draft
	/// opens on Review with no chat (X23 as amended).
	/// \details It decides nothing and performs no effect: gestures leave
	/// through `local_draft_dispatch`. Secondary identity (exact commits, the
	/// remote chain, qualifications) is in tooltips, not on the page.
	class local_draft_view final : public QWidget
	{
	public:
		explicit local_draft_view(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_dispatch aDispatch);
		void present(const application::local_draft_workbench_projection& aProjection);
		void set_code_font(const QFont& aFont);

		/// \brief The window title row's text for the page.
		[[nodiscard]] auto title_text() const -> QString;
		[[nodiscard]] auto title_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto identity() const noexcept -> QLabel*;
		[[nodiscard]] auto notice() const noexcept -> QLabel*;
		[[nodiscard]] auto changes() const noexcept -> QListWidget*;
		[[nodiscard]] auto working() const noexcept -> QListWidget*;
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto note_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto diff() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto refresh_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto discard_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;
		/// \brief The candidate pop-up; its menu lists the candidates, Refresh and Discard….
		[[nodiscard]] auto candidates() const noexcept -> QToolButton*;
		[[nodiscard]] auto candidate_actions() const -> QList<QAction*>;
		[[nodiscard]] auto target_repository() const noexcept -> QComboBox*;
		[[nodiscard]] auto target_branch() const noexcept -> QComboBox*;
		[[nodiscard]] auto use_target_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto rebind_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto projection() const noexcept -> const application::local_draft_workbench_projection&;
		/// \brief The right drawer's tabs: Status, Notes and Tour.
		[[nodiscard]] auto areas() const noexcept -> QTabBar*;
		/// \brief The left drawer's tabs: Files and, in author state, Local Changes.
		[[nodiscard]] auto drawer() const noexcept -> QTabBar*;
		/// \brief Overview (true) or Review.
		void               set_overview(bool aOverview);
		[[nodiscard]] auto overview() const noexcept -> bool;
		[[nodiscard]] auto review_diff() const noexcept -> local_diff_view*;
		[[nodiscard]] auto picker() const noexcept -> view_picker*;
		[[nodiscard]] auto local_changes() const noexcept -> local_changes_panel*;
		[[nodiscard]] auto editor() const noexcept -> suggestion_editor*;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto create_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto branch_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto comparing_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto status_line() const noexcept -> QLabel*;
		/// \brief Author state's projection, from its own host.
		void present_local(const application::local_changes_projection& aProjection);
		[[nodiscard]] auto local_projection() const noexcept -> const application::local_changes_projection&;
		/// \brief The window's title row text: the repository, the title and Local draft.
		[[nodiscard]] auto window_title() const -> QString;
		[[nodiscard]] auto description() const noexcept -> local_draft_description_area*;
		[[nodiscard]] auto tour() const noexcept -> local_draft_tour_area*;
		/// \brief RFC-BPR-0111. The Pull request area.
		[[nodiscard]] auto publication() const noexcept -> local_draft_publication_area*;
		/// \brief RFC-BPR-0110. The Description and Tour areas' gestures.
		void set_preparation_dispatch(local_draft_preparation_dispatch aDispatch);
		/// \brief The new-side lines selected in Changes, when they are the latest candidate's.
		[[nodiscard]] auto selected_lines() const -> std::optional<local_draft_lines>;
		/// \brief Arrives at lines: Changes, that file, those lines selected.
		void reveal(const std::string& aPath, std::uint32_t aFirst, std::uint32_t aLast);
		/// \brief Replaces the discard question, for a course that cannot answer one.
		void set_discard_question(std::function<bool()> aQuestion);

	private:
		void build_bar();
		void build_body();
		void present_bar();
		void present_center();
		void present_edit();
		void present_status_line();
		void show_view(review_view aView);
		void present_header();
		void present_changes();
		void present_working();
		void present_notes();
		void present_diff();
		void present_recovery();
		void ask_discard();
		void apply_reveal();

		local_draft_dispatch                          Dispatch;
		application::local_draft_workbench_projection Projection;
		std::function<bool()>                         DiscardQuestion;
		bool                                          Presenting = false;

		QLineEdit*                       Title            = nullptr;
		QLabel*                          Badge            = nullptr;
		QLabel*                          Identity         = nullptr;
		QLabel*                          Notice           = nullptr;
		QWidget*                         TargetRow        = nullptr;
		QComboBox*                       TargetRepository = nullptr;
		QComboBox*                       TargetBranch     = nullptr;
		QPushButton*                     UseTarget        = nullptr;
		QPushButton*                     Rebind           = nullptr;
		QToolButton*                     Candidates       = nullptr;
		QMenu*                           CandidateMenu    = nullptr;
		QToolButton*                     Branch           = nullptr;
		QToolButton*                     Comparing        = nullptr;
		view_picker*                     Picker           = nullptr;
		QLabel*                          PublishState     = nullptr;
		QPushButton*                     Create           = nullptr;
		QTabBar*                         Drawer           = nullptr;
		QStackedWidget*                  DrawerStack      = nullptr;
		local_changes_panel*             LocalChanges     = nullptr;
		QStackedWidget*                  Center           = nullptr;
		QWidget*                         OverviewPage     = nullptr;
		QListWidget*                     OverviewFiles    = nullptr;
		QStackedWidget*                  ReviewStack      = nullptr;
		local_diff_view*                 ReviewDiff       = nullptr;
		QWidget*                         EditPage         = nullptr;
		QLabel*                          EditName         = nullptr;
		QLabel*                          EditState        = nullptr;
		QPushButton*                     Save             = nullptr;
		suggestion_editor*               Editor           = nullptr;
		QLabel*                          StatusLine       = nullptr;
		application::local_changes_projection LocalProjection;
		/// Which source the centre shows: the candidate's file or Local Changes'.
		bool ShowingLocal = false;
		/// The Edit view's file, the bytes it read or last wrote, and its edits.
		std::string               EditPath;
		std::vector<std::uint8_t> EditRead;
		bool                      EditDirty = false;
		std::vector<core::local_change_hunk> ShownHunks;
		QPushButton*                     Refresh          = nullptr;
		QPushButton*                     Discard          = nullptr;
		QPushButton*                     Close            = nullptr;
		QListWidget*                     Changes          = nullptr;
		QLabel*                          WorkingTitle     = nullptr;
		QListWidget*                     Working          = nullptr;
		QListWidget*                     Notes            = nullptr;
		QLineEdit*                       NoteEditor       = nullptr;
		QPlainTextEdit*                  Diff             = nullptr;
		QTabBar*                         Areas            = nullptr;
		QStackedWidget*                  AreaStack        = nullptr;
		local_draft_description_area*    DescriptionArea  = nullptr;
		local_draft_tour_area*           TourArea         = nullptr;
		local_draft_publication_area*    PublicationArea  = nullptr;
		local_draft_preparation_dispatch PreparationDispatch;
		local_draft_page_links           Links;
		/// Lines to select once their file is shown.
		std::optional<local_draft_lines> PendingReveal;
		/// The draft last presented; another draft opens on Changes.
		std::string PresentedDraft;
	};
}

#endif
