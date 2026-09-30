#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_VIEW_HPP

#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/suggestion_composer.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>

class QAction;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QToolButton;

namespace bpr::qt {
	/// \brief The gestures the local cycle page sends to its host.
	struct review_cycle_dispatch
	{
		std::function<void(const std::string&, bool)>                                                           OpenFile;
		std::function<void(const core::document_tab_intent&)>                                                   Tab;
		std::function<void(core::document_tab_id, std::uint64_t)>                                               ObserveLine;
		std::function<void(core::begin_cycle_feedback)>                                                         Comment;
		std::function<void(application::review_cycle_suggestion_request)>                                       Suggest;
		std::function<void(const core::cycle_feedback_item_ref&, std::uint64_t, core::cycle_related_reference)> Relate;
	};

	/// \brief A local Review Cycle's review surface (RFC-BPR-0061, BPR-1157).
	///
	/// \details Built from the RFC-BPR-0052 scaffold's own parts in the same
	/// order as a pull request's review surface: the review bar (Updates
	/// disclosure, the full-source revision, file navigation and position,
	/// Comment, Suggest change and Compose feedback), the folding Updates line
	/// of captured revisions, the document tabs, and the files drawer, one
	/// full-source pane and the notes tray. It adds no permanent row. The
	/// source pane shows the active tab's exact retained bytes; a selection
	/// becomes an exact byte anchor. It decides nothing and performs no
	/// effect: gestures leave through `review_cycle_dispatch`.
	class review_cycle_view final : public QWidget
	{
	public:
		explicit review_cycle_view(QWidget* aParent = nullptr);

		void set_dispatch(review_cycle_dispatch aDispatch);
		/// \brief Shows the host's projection.
		void present(const application::review_cycle_workbench_projection& aProjection);
		void set_code_font(const QFont& aFont);

		[[nodiscard]] auto review_bar() const noexcept -> workbench_strip*;
		[[nodiscard]] auto tabs() const noexcept -> document_tab_strip*;
		[[nodiscard]] auto source() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto files() const noexcept -> QListWidget*;
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto composer() const noexcept -> suggestion_composer*;
		[[nodiscard]] auto comment_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto updates_line() const noexcept -> QLabel*;
		[[nodiscard]] auto comment_action() const noexcept -> QAction*;
		[[nodiscard]] auto suggest_action() const noexcept -> QAction*;
		[[nodiscard]] auto compose_action() const noexcept -> QAction*;
		[[nodiscard]] auto updates_toggle() const noexcept -> updates_disclosure*;
		/// \brief Links the source selection to the selected note as a related location.
		[[nodiscard]] auto relate_button() const noexcept -> QPushButton*;
		/// \brief The exact anchor for the source pane's selection, if it has one.
		[[nodiscard]] auto selection_anchor() const -> std::optional<core::cycle_source_anchor>;
		[[nodiscard]] auto projection() const noexcept -> const application::review_cycle_workbench_projection&;

	private:
		void               begin_comment();
		void               begin_suggestion();
		void               save_comment();
		void               refresh_actions();
		void               step_file(int aDirection);
		[[nodiscard]] auto source_offset(int aPosition) const -> std::optional<std::uint64_t>;

		review_cycle_dispatch                          Dispatch;
		application::review_cycle_workbench_projection Projection;
		/// What the open comment composer will note.
		std::optional<core::cycle_feedback_target> PendingTarget;
		workbench_strip*                           ReviewBar     = nullptr;
		updates_disclosure*                        UpdatesToggle = nullptr;
		QLabel*                                    RevisionLabel = nullptr;
		QLabel*                                    FilePosition  = nullptr;
		QLabel*                                    UpdatesLine   = nullptr;
		document_tab_strip*                        Tabs          = nullptr;
		QSplitter*                                 Body          = nullptr;
		QListWidget*                               Files         = nullptr;
		QStackedWidget*                            Centre        = nullptr;
		QPlainTextEdit*                            Source        = nullptr;
		QLabel*                                    SourceNotice  = nullptr;
		QListWidget*                               Notes         = nullptr;
		QStackedWidget*                            TrayComposer  = nullptr;
		QPlainTextEdit*                            CommentEditor = nullptr;
		QLabel*                                    CommentTarget = nullptr;
		suggestion_composer*                       Composer      = nullptr;
		QAction*                                   PreviousFile  = nullptr;
		QAction*                                   NextFile      = nullptr;
		QAction*                                   CommentAction = nullptr;
		QAction*                                   SuggestAction = nullptr;
		QAction*                                   ComposeAction = nullptr;
		QAction*                                   FilesToggle   = nullptr;
		QAction*                                   NotesToggle   = nullptr;
		QPushButton*                               RelateButton  = nullptr;
		bool                                       Presenting    = false;
	};
}

#endif
