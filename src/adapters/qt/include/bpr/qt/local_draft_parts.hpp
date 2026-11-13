#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PARTS_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/local_changes_host.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/local_changes_panel.hpp"
#include "bpr/qt/view_picker.hpp"

#include <QObject>
#include <QPointer>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QComboBox;
class QFont;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QToolButton;
class QWidget;

namespace bpr::qt {
	class disclosure_button;
	class local_draft_publication_area;
	class suggestion_editor;

	/// \brief The gestures a local draft's additions send to their host.
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
		/// The author has confirmed; the host removes the private preparation.
		std::function<void()> Discard;
		std::function<void()> Close;
		/// Author state: opens a file of Local Changes' comparison, or the Edit view's file.
		std::function<void(const std::string&)> OpenLocalFile;
		/// Author state: Save writes the edited bytes if the disk still holds the read ones.
		std::function<void(const std::string&, std::vector<std::uint8_t>, std::vector<std::uint8_t>)> Save;
		/// Create Draft PR…: opens the publishing steps.
		std::function<void()> CreatePullRequest;
	};

	/// \brief Lines chosen in the window's comparison: a file of the latest
	/// candidate and its new-side lines.
	struct local_draft_lines
	{
		std::string   Path;
		std::uint32_t First = 0;
		std::uint32_t Last  = 0;
	};

	/// \brief What the parts lend: the comparison's selection, and arriving
	/// at lines there.
	struct local_draft_page_links
	{
		std::function<std::optional<local_draft_lines>()>                     Selection;
		std::function<void(const std::string&, std::uint32_t, std::uint32_t)> Reveal;
	};

	/// \brief What a local draft adds to the pull request window (RFC-BPR-0108
	/// as amended by the owner, 2026-11-13: "augment the real PR window, do not
	/// rebuild it").
	///
	/// \details Not a page. The window shows a draft in its own review surface,
	/// with its own Files tree, Search tab, diff viewer, Overview, drawers and
	/// status bar; these are the only parts a draft adds, each placed by the
	/// window in a slot of its own:
	///
	/// - the review bar's leading controls: the candidate pop-up and, in author
	///   state, the branch with its ahead and behind counts;
	/// - the review bar's trailing controls: where publishing stands and
	///   Create Draft PR…;
	/// - a notice row for a recovery or a target choice, shown only when there
	///   is one;
	/// - the Files tab's footer: the working files a candidate can include,
	///   behind a disclosure;
	/// - the Local Changes tab (author state only, always present then);
	/// - the Edit view's page (the view picker's third view);
	/// - the Notes tab's page;
	/// - Overview's Status tab page: the publish steps.
	///
	/// The Tour tab is the window's own Tour panel, and the description is
	/// Overview's own (RFC-BPR-0110 as amended, owner 2026-11-13: "stick with
	/// the PR view and just augment it"); the composition presents the
	/// draft's tour and description in them.
	///
	/// It decides nothing and performs no effect: gestures leave through
	/// `local_draft_dispatch`.
	class local_draft_parts final : public QObject
	{
	public:
		explicit local_draft_parts(QWidget* aOwner);
		~local_draft_parts() override;

		local_draft_parts(const local_draft_parts&)                    = delete;
		auto operator=(const local_draft_parts&) -> local_draft_parts& = delete;
		local_draft_parts(local_draft_parts&&)                         = delete;
		auto operator=(local_draft_parts&&) -> local_draft_parts&      = delete;

		void set_dispatch(local_draft_dispatch aDispatch);
		/// \brief How a tour stop reads the lines selected in the window's
		/// comparison and reveals a stop's lines there.
		void set_comparison_links(std::function<std::optional<local_draft_lines>()> aSelection, std::function<void(const std::string&, std::uint32_t, std::uint32_t)> aReveal);
		void present(const application::local_draft_workbench_projection& aProjection);
		/// \brief Author state's projection, from its own host.
		void present_local(const application::local_changes_projection& aProjection);
		void set_code_font(const QFont& aFont);
		/// \brief The Edit view's syntax colours, adopted when they describe the text it holds.
		void adopt_edit_syntax(const std::string& aDocument, application::shared_token_spans aSpans);
		/// \brief The panes' code colours, variant and font, so the Edit view reads as Review does.
		void adopt_appearance(const appearance_profile& aProfile);

		// The window's slots.
		[[nodiscard]] auto bar_leading() const noexcept -> QWidget*;
		[[nodiscard]] auto bar_trailing() const noexcept -> QWidget*;
		[[nodiscard]] auto notice_row() const noexcept -> QWidget*;
		[[nodiscard]] auto files_footer() const noexcept -> QWidget*;
		[[nodiscard]] auto local_changes() const noexcept -> local_changes_panel*;
		[[nodiscard]] auto edit_page() const noexcept -> QWidget*;
		[[nodiscard]] auto notes_page() const noexcept -> QWidget*;
		[[nodiscard]] auto status_page() const noexcept -> QWidget*;

		// What the window reads.
		[[nodiscard]] auto projection() const noexcept -> const application::local_draft_workbench_projection&;
		[[nodiscard]] auto local_projection() const noexcept -> const application::local_changes_projection&;
		/// \brief Author state: the window is on the author's own worktree.
		[[nodiscard]] auto author() const noexcept -> bool;
		/// \brief The title row's text: the repository, the number when there is one, the title.
		[[nodiscard]] auto window_title() const -> QString;
		[[nodiscard]] auto title_text() const -> QString;
		/// \brief The status bar's draft part (rich text): candidate, uncommitted and to push.
		[[nodiscard]] auto status_text(int aFile, int aFiles) const -> QString;
		/// \brief Why the Edit view is refused for the file shown, or empty.
		[[nodiscard]] auto edit_refusal() const -> QString;
		/// \brief The Edit view's file, when one is open.
		[[nodiscard]] auto edit_path() const -> const std::string&;
		/// \brief The Edit view's text as read, for its syntax request.
		[[nodiscard]] auto edit_document() const -> std::string;

		/// \brief Arrives at lines: Review, that file, those lines selected, on
		/// candidate `aCandidate` (0: the latest).
		void reveal(const std::string& aPath, std::uint32_t aFirst, std::uint32_t aLast, std::uint64_t aCandidate = 0U);
		/// \brief The lines selected in the window's comparison of the latest
		/// candidate, for a tour stop.
		[[nodiscard]] auto selected_lines() const -> std::optional<local_draft_lines>;
		/// \brief The window opened a file of the candidate (Files or Search);
		/// in the Edit view the same file opens in the editor too.
		void open_file(const std::string& aPath, bool aEditing);
		/// \brief Replaces the discard question, for a course that cannot answer one.
		void set_discard_question(std::function<bool()> aQuestion);
		/// \brief Replaces Rename…'s question, for a course: the title chosen, or none.
		void set_rename_question(std::function<std::optional<QString>(const QString&)> aQuestion);
		/// \brief Rename… in the candidate pop-up: the pull request's title.
		void ask_rename();

		// Controls, for courses.
		[[nodiscard]] auto identity() const noexcept -> QLabel*;
		[[nodiscard]] auto notice() const noexcept -> QLabel*;
		[[nodiscard]] auto working() const noexcept -> QListWidget*;
		[[nodiscard]] auto working_disclosure() const noexcept -> disclosure_button*;
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto note_editor() const noexcept -> QLineEdit*;
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
		[[nodiscard]] auto editor() const noexcept -> suggestion_editor*;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto create_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto branch_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto publication() const noexcept -> local_draft_publication_area*;

		/// \brief Called after any presentation, so the window lays its slots out again.
		std::function<void()> Changed;

	private:
		void build();
		void present_header();
		void present_recovery();
		void present_working();
		void present_notes();
		void present_bar();
		void present_edit();
		void ask_discard();
		void changed();

		QWidget*                                      Owner = nullptr;
		local_draft_dispatch                          Dispatch;
		application::local_draft_workbench_projection Projection;
		application::local_changes_projection         LocalProjection;
		std::function<bool()>                         DiscardQuestion;
		std::function<std::optional<QString>(const QString&)> RenameQuestion;
		bool                                          Presenting = false;

		QPointer<QWidget>               BarLeading;
		QPointer<QWidget>               BarTrailing;
		QPointer<QWidget>               NoticeRow;
		QPointer<QWidget>               FilesFooter;
		QPointer<local_changes_panel>   LocalChanges;
		QPointer<QWidget>               EditPage;
		QPointer<QWidget>               NotesPage;
		QPointer<QWidget>               StatusPage;

		QLabel*                                                               Identity          = nullptr;
		QLabel*                                                               Notice            = nullptr;
		QWidget*                                                              TargetRow         = nullptr;
		QComboBox*                                                            TargetRepository  = nullptr;
		QComboBox*                                                            TargetBranch      = nullptr;
		QPushButton*                                                          UseTarget         = nullptr;
		QPushButton*                                                          Rebind            = nullptr;
		QToolButton*                                                          Candidates        = nullptr;
		QMenu*                                                                CandidateMenu     = nullptr;
		QToolButton*                                                          Branch            = nullptr;
		QLabel*                                                               PublishState      = nullptr;
		QPushButton*                                                          Create            = nullptr;
		QPushButton*                                                          Refresh           = nullptr;
		QPushButton*                                                          Discard           = nullptr;
		QPushButton*                                                          Close             = nullptr;
		disclosure_button*                                                    WorkingDisclosure = nullptr;
		QListWidget*                                                          Working           = nullptr;
		QListWidget*                                                          Notes             = nullptr;
		QLineEdit*                                                            NoteEditor        = nullptr;
		QLabel*                                                               EditName          = nullptr;
		QLabel*                                                               EditState         = nullptr;
		QPushButton*                                                          Save              = nullptr;
		suggestion_editor*                                                    Editor            = nullptr;
		local_draft_publication_area*                                         PublicationArea   = nullptr;
		local_draft_page_links                                                Links;
		std::function<void(const std::string&, std::uint32_t, std::uint32_t)> RevealInComparison;

		/// The Edit view's file, the bytes it read or last wrote, and its edits.
		std::string               EditPath;
		std::vector<std::uint8_t> EditRead;
		bool                      EditDirty = false;
		code_role_colors          SyntaxColors;
		/// The draft last presented; another draft opens on Review.
		std::string PresentedDraft;
	};
}

#endif
