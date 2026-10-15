#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include <QWidget>
#include <functional>
#include <map>
#include <set>
class QAction;
class QCheckBox;
class QComboBox;
class QListWidget;
class QLineEdit;
class QTextDocument;
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class QPushButton;
class QVBoxLayout;
namespace bpr::qt {
	class markdown_body_view;
	enum class tour_editor_action : std::uint8_t { import_selected,
												   retry_save,
												   use_saved_copy,
												   keep_local_conflict,
												   use_remote_conflict,
												   map_problem,
												   map_solution,
												   add_stop,
												   insert_after,
												   add_related,
												   retarget_stop,
												   navigate_stop,
												   navigate_related,
												   undo_structure,
												   redo_structure,
												   preview_publication,
												   publish_publication,
												   resume_publication,
												   reconcile_publication,
												   stop_publication,
												   suggest_outline,
												   insert_outline };
	/// \brief Native Markdown interaction and inert projections for private tour preparation.
	class review_tour_editor final : public QWidget
	{
	public:
		explicit review_tour_editor(QWidget* aParent = nullptr);
		void               set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction);
		void               present(const application::tour_presentation& aProjection);
		void               set_writing_dispatch(std::function<void(core::writing_hook, prose_capture)> aRequest, std::function<void()> aCancel);
		[[nodiscard]] auto writing_editor(core::writing_hook aHook) const -> scoped_prose_editor*;
		[[nodiscard]] auto writing_visible(core::writing_hook aHook) const -> bool;
		void               present_publication(const application::tour_publication_presentation& aProjection);
		[[nodiscard]] auto compatibility_publication() const -> bool;
		[[nodiscard]] auto description_publication() const -> bool;
		void               show_problem();
		void               present_outline(const std::vector<core::tour_outline_entry>& aEntries);
		[[nodiscard]] auto retained_outline_entries() const -> std::vector<std::size_t>;
		[[nodiscard]] auto import_choice() const -> std::size_t;
		void               show_notice(const QString& aText);
		void               show_writing_notice(const QString& aText);
		void               show_stops();
		void               trigger(tour_editor_action aAction);
		[[nodiscard]] auto selected_stop() const -> const core::tour_stop*;
		[[nodiscard]] auto selected_related() const -> std::optional<core::tour_source_range>;
		[[nodiscard]] auto outline() const -> QListWidget* { return Outline; }
		[[nodiscard]] auto stop_title() const -> QLineEdit* { return Title; }
		[[nodiscard]] auto stop_body() const -> QPlainTextEdit* { return Body; }
		[[nodiscard]] auto problem_editor() const -> QPlainTextEdit* { return Problem; }
		[[nodiscard]] auto solution_editor() const -> QPlainTextEdit* { return Solution; }
		[[nodiscard]] auto roadmap() const -> QTabWidget* { return Roadmap; }
		[[nodiscard]] auto preview() const -> markdown_body_view* { return Preview; }
		[[nodiscard]] auto publication_preview() const -> markdown_body_view* { return PublicationPreview; }
		[[nodiscard]] auto shown() const -> const application::tour_presentation& { return Shown; }
		/// \brief True while an input method holds uncommitted preedit text in any field; committed text is unaffected.
		[[nodiscard]] auto composing() const -> bool { return !Composing.empty(); }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void hideEvent(QHideEvent* aEvent) override;
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;
		bool focusNextPrevChild(bool aNext) override;

	private:
		void                                                   edit(core::tour_command_kind aKind, const std::string& aText);
		void                                                   add_writing_button(scoped_prose_editor* aEditor, core::writing_hook aHook, QVBoxLayout* aLayout);
		std::function<void(core::writing_hook, prose_capture)> WritingRequest;
		std::function<void()>                                  WritingCancel;
		void                                                   refresh_preview();
		void                                                   build_stops();
		void                                                   present_stops(bool aOwnerChanged);
		void                                                   edit_stop();
		void                                                   structural(core::tour_command_kind aKind, std::size_t aPosition = 0);
		[[nodiscard]] auto                                     selected_stops() const -> std::vector<core::tour_stop_id>;
		void                                                   select_stops(const std::vector<core::tour_stop_id>& aStops);
		void                                                   update_stop_actions();
		void                                                   move_selection(int aOffset);
		void                                                   next_stop(int aOffset);
		void                                                   next_changed_stop();
		QComboBox*                                             PublicationMode    = nullptr;
		QCheckBox*                                             DescriptionEdit    = nullptr;
		QLabel*                                                PublicationStatus  = nullptr;
		markdown_body_view*                                    PublicationPreview = nullptr;
		QPushButton*                                           PublishTour        = nullptr;
		QPushButton*                                           ResumeTour         = nullptr;
		QPushButton*                                           ReconcileTour      = nullptr;
		QPushButton*                                           StopTour           = nullptr;
		QComboBox*                                             Imports            = nullptr;
		QPushButton*                                           Import             = nullptr;
		QListWidget*                                           SuggestedOutline   = nullptr;
		QPushButton*                                           InsertOutline      = nullptr;
		QListWidget*                                           Outline            = nullptr;
		QListWidget*                                           Related            = nullptr;
		QLineEdit*                                             Title              = nullptr;
		scoped_prose_editor*                                   Body               = nullptr;
		QLabel*                                                Source             = nullptr;
		QWidget*                                               StopFields         = nullptr;
		QAction*                                               MergeStops         = nullptr;
		QAction*                                               DuplicateStops     = nullptr;
		QAction*                                               RemoveStops        = nullptr;
		QAction*                                               UndoStructure      = nullptr;
		QAction*                                               RedoStructure      = nullptr;
		core::tour_stop_id                                     Selected;
		core::tour_stop_id                                     Editing;
		std::optional<core::tour_command_kind>                 EditingKind;
		std::map<core::tour_stop_id, QTextDocument*>           BodyDocuments;
		std::map<core::tour_stop_id, int>                      BodyPositions;
		application::tour_presentation                         Shown;
		std::function<void(core::tour_command)>                Dispatch;
		std::function<void(tour_editor_action)>                Action;
		scoped_prose_editor*                                   Problem           = nullptr;
		scoped_prose_editor*                                   Solution          = nullptr;
		QPlainTextEdit*                                        Conflict          = nullptr;
		QTabWidget*                                            Roadmap           = nullptr;
		markdown_body_view*                                    Preview           = nullptr;
		QLabel*                                                RevisionChanges   = nullptr;
		QPushButton*                                           NextChanged       = nullptr;
		QLabel*                                                WritingStatus     = nullptr;
		QLabel*                                                Status            = nullptr;
		QPushButton*                                           Retry             = nullptr;
		QPushButton*                                           UseSaved          = nullptr;
		QPushButton*                                           KeepLocalConflict = nullptr;
		QPushButton*                                           UseRemoteConflict = nullptr;
		std::set<const QObject*>                               Composing;
		bool                                                   SelectionChanged = false;
		bool                                                   Applying         = false;
	};
}
#endif
