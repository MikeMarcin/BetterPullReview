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
#include <string_view>

class QAction;
class QCheckBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QMenu;
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
		/// BPR-1163: Capture revision, deliberately when the bytes may be unchanged.
		std::function<void(bool)>                            Capture;
		std::function<void(const core::cycle_view_request&)> Compare;
		std::function<void()>                                FullSource;
		std::function<void()>                                Refresh;
		std::function<void()>                                MarkReviewed;
		/// BPR-1164: choose a response file for a round; add a hand-entered reply.
		std::function<void(core::cycle_round_id)>                           ChooseResponse;
		std::function<void(core::cycle_round_id, core::cycle_manual_reply)> Reply;
		/// BPR-1165: the reviewer's decision about a note, and the next round's draft.
		std::function<void(const core::cycle_feedback_item_ref&, std::optional<core::cycle_reviewer_decision>)> Decide;
		std::function<void()>                                                                                   PrepareNextRound;
	};

	class appearance_profile;

	/// \brief The source pane asks for the file it shows to be classified
	/// ([[BPR-1157]]).
	/// \details The page runs no lexer. Whoever composed it classifies
	/// `Document`, chosen by `Path`, and hands the spans back through
	/// `review_cycle_view::adopt_source_syntax` with the same bytes.
	struct source_syntax_request
	{
		std::string Path;
		std::string Document;
	};

	/// \brief A local Review Cycle's review surface (RFC-BPR-0061, BPR-1157).
	///
	/// \details Built from the RFC-BPR-0052 scaffold's own parts in the same
	/// order as a pull request's review surface: the review bar (Updates
	/// disclosure, the full-source revision, file navigation and position,
	/// Comment, Suggest change and Compose feedback), the folding Updates line
	/// of captured revisions, the document tabs, and the files drawer, one
	/// full-source pane and the notes tray. It adds no permanent row. The
	/// source pane is the shared code view (`suggestion_editor`, read-only)
	/// over the active tab's exact retained bytes, in the review panes' code
	/// font and tab width; a selection becomes an exact byte anchor. BPR-1163 adds to the same review bar
	/// Capture revision, the view chooser (full source, this round, since my
	/// last review, all cycle changes, chosen revisions), Mark reviewed and a
	/// refresh that appears only when newer material exists; a comparison
	/// shows the shared engine's rows in the same pane. The full source is
	/// coloured from a classification it asks for (`SourceSyntaxWanted`),
	/// painted through the review panes' role colours and span-to-format
	/// rule. Suggest change starts from the caret, from the review bar or the
	/// source pane's context menu, and edits the whole file in place: the
	/// same code view becomes editable until the suggestion is saved or
	/// cancelled, so nothing on screen moves or changes colour, and the tray
	/// holds the rationale and the before/after (owner amendment to BPR-1156,
	/// 2026-10-01, and its v004 rejection, 2026-10-02). It decides nothing and
	/// performs no effect: gestures leave through `review_cycle_dispatch`.
	class review_cycle_view final : public QWidget
	{
	public:
		explicit review_cycle_view(QWidget* aParent = nullptr);

		void set_dispatch(review_cycle_dispatch aDispatch);
		/// \brief Shows the host's projection.
		void present(const application::review_cycle_workbench_projection& aProjection);
		void set_code_font(const QFont& aFont);
		/// \brief Takes the syntax role colours of the applied appearance, for
		/// the source pane and Suggest change.
		void adopt_appearance(const appearance_profile& aProfile);
		/// \brief Colours the source pane from one classification.
		/// \returns Whether the spans were adopted: only a classification of
		/// the exact bytes the pane shows now is.
		auto adopt_source_syntax(std::string_view aDocument, application::shared_token_spans aSpans) -> bool;

		/// Asks for the shown file to be classified, whenever the pane shows
		/// other bytes.
		std::function<void(const source_syntax_request&)> SourceSyntaxWanted;

		[[nodiscard]] auto review_bar() const noexcept -> workbench_strip*;
		[[nodiscard]] auto tabs() const noexcept -> document_tab_strip*;
		/// \brief The code view: the file read, and edited by Suggest change.
		[[nodiscard]] auto source() const noexcept -> suggestion_editor*;
		[[nodiscard]] auto files() const noexcept -> QListWidget*;
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto composer() const noexcept -> suggestion_composer*;
		/// \brief Whether Suggest change is editing a file in the main pane.
		[[nodiscard]] auto suggesting() const -> bool;
		/// \brief The source pane's context menu at a viewport point: the
		/// code view's own Copy and Select All after Comment and Suggest
		/// change, or its editing menu while a suggestion is edited. A click
		/// outside the selection moves the caret there first. The caller owns
		/// the menu; it deletes itself when closed.
		[[nodiscard]] auto source_menu(const QPoint& aAt) -> QMenu*;
		[[nodiscard]] auto comment_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto updates_line() const noexcept -> QLabel*;
		[[nodiscard]] auto comment_action() const noexcept -> QAction*;
		[[nodiscard]] auto suggest_action() const noexcept -> QAction*;
		[[nodiscard]] auto compose_action() const noexcept -> QAction*;
		[[nodiscard]] auto updates_toggle() const noexcept -> updates_disclosure*;
		/// \brief Links the source selection to the selected note as a related location.
		[[nodiscard]] auto relate_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto capture_action() const noexcept -> QAction*;
		/// \brief Records another observation even when the bytes are unchanged.
		[[nodiscard]] auto capture_again_action() const noexcept -> QAction*;
		[[nodiscard]] auto view_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto mark_reviewed_action() const noexcept -> QAction*;
		[[nodiscard]] auto refresh_action() const noexcept -> QAction*;
		/// \brief The comparison pane: the active tab's rows, when it shows a comparison.
		[[nodiscard]] auto comparison() const noexcept -> QPlainTextEdit*;
		/// \brief The notes tray's Add reply button and its reply editor (BPR-1164).
		[[nodiscard]] auto reply_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto reply_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto reply_from() const noexcept -> QLineEdit*;
		[[nodiscard]] auto reply_about_note() const noexcept -> QCheckBox*;
		/// \brief Opens the reply editor for round `aRound`.
		void begin_reply(core::cycle_round_id aRound);
		/// \brief The decisions offered for the note at `aRow` (BPR-1165).
		[[nodiscard]] auto decision_menu(int aRow) -> QMenu*;
		[[nodiscard]] auto next_round_button() const noexcept -> QToolButton*;
		/// \brief The exact anchor for the source pane's selection, if it has one.
		[[nodiscard]] auto selection_anchor() const -> std::optional<core::cycle_source_anchor>;
		[[nodiscard]] auto projection() const noexcept -> const application::review_cycle_workbench_projection&;

	private:
		void begin_comment();
		void begin_suggestion();
		void end_suggestion();
		void show_reading_page();
		/// The code view back to reading, and the shown file's bytes in it.
		void read_source();
		void show_source();
		void save_comment();
		void refresh_actions();
		void step_file(int aDirection);
		void rebuild_view_menu();
		void rebuild_reply_menu();
		void save_reply();
		void apply_source_syntax();
		void present_comparison(const application::review_cycle_comparison_view& aComparison, bool aChanged);

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
		suggestion_editor*                         Source        = nullptr;
		/// What the centre shows when no suggestion is being edited.
		QWidget* ReadingPage = nullptr;
		/// The review panes' tab width, in columns.
		std::uint32_t TabWidth = core::default_suggestion_tab_width;
		/// The exact file a suggestion edits, kept while it is edited.
		std::optional<application::review_cycle_source_view> SuggestionSource;
		QLabel*                                              SourceNotice  = nullptr;
		QListWidget*                                         Notes         = nullptr;
		QStackedWidget*                                      TrayComposer  = nullptr;
		QPlainTextEdit*                                      CommentEditor = nullptr;
		QLabel*                                              CommentTarget = nullptr;
		suggestion_composer*                                 Composer      = nullptr;
		QAction*                                             PreviousFile  = nullptr;
		QAction*                                             NextFile      = nullptr;
		QAction*                                             CommentAction = nullptr;
		QAction*                                             SuggestAction = nullptr;
		QAction*                                             ComposeAction = nullptr;
		QAction*                                             FilesToggle   = nullptr;
		QAction*                                             NotesToggle   = nullptr;
		QPushButton*                                         RelateButton  = nullptr;
		QAction*                                             CaptureAction = nullptr;
		QAction*                                             CaptureAgain  = nullptr;
		QToolButton*                                         ViewButton    = nullptr;
		QMenu*                                               ViewMenu      = nullptr;
		QAction*                                             MarkReviewed  = nullptr;
		QAction*                                             RefreshAction = nullptr;
		QPlainTextEdit*                                      Comparison    = nullptr;
		QToolButton*                                         ReplyButton   = nullptr;
		QToolButton*                                         NextRound     = nullptr;
		QMenu*                                               ReplyMenu     = nullptr;
		QWidget*                                             ReplyPage     = nullptr;
		QLabel*                                              ReplyTarget   = nullptr;
		QLineEdit*                                           ReplyFrom     = nullptr;
		QCheckBox*                                           ReplyAbout    = nullptr;
		QPlainTextEdit*                                      ReplyEditor   = nullptr;
		std::optional<core::cycle_round_id>                  ReplyRound;
		std::optional<std::string>                           ReplyItem;
		bool                                                 Presenting = false;

		/// The shown file's classification, and the colours it is painted in.
		classified_text  SourceSyntax;
		code_role_colors SyntaxColors{ };
	};
}

#endif
