#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/core/tour_steps.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include <QPointer>
#include <QWidget>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>
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
class QBoxLayout;
class QVBoxLayout;
namespace bpr::qt {
	class markdown_body_view;
	class stop_action_row;
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
	/// \brief [[BPR-1593]]. Where the tour is shown: the route in the drawer's
	/// Tour tab (RFC-BPR-0085 D4, layout A), or the tour page in the main pane
	/// (layout C).
	enum class tour_editor_mode : std::uint8_t {
		route,
		page,
	};

	/// \brief [[BPR-1593]]. Code around a stop's lines, for the tour page.
	struct tour_excerpt
	{
		/// One-based number of the first line shown.
		std::uint32_t        First = 1U;
		std::vector<QString> Lines;
		/// One-based lines the stop is on, under the amber bracket.
		std::uint32_t StopFirst = 1U;
		std::uint32_t StopLast  = 1U;
	};

	/// \brief Native Markdown interaction and inert projections for private tour preparation.
	class review_tour_editor final : public QWidget
	{
	public:
		explicit review_tour_editor(QWidget* aParent = nullptr);
		/// \brief [[BPR-1593]]. The route, or the tour page.
		void               set_mode(tour_editor_mode aMode);
		[[nodiscard]] auto mode() const noexcept -> tour_editor_mode { return Mode; }
		/// \brief [[BPR-1593]]. Expand (false) or Preview and publish (true)
		/// asks the window for the tour page.
		void set_expand_dispatch(std::function<void(bool)> aDispatch);
		/// \brief [[BPR-1593]]. Back to the code asks the window for the route.
		void set_collapse_dispatch(std::function<void()> aDispatch);
		/// \brief [[BPR-1593]]. The code around a stop's lines, when the window
		/// has it.
		void set_excerpt_source(std::function<std::optional<tour_excerpt>(const core::tour_source_range&)> aSource);
		/// \brief [[BPR-1593]]. The brief's fields open in place in the route.
		[[nodiscard]] auto brief_open() const noexcept -> int { return BriefOpen; }
		/// \brief [[BPR-1784]]. The three steps as last presented: each one's
		/// state and the step that is open.
		[[nodiscard]] auto steps() const noexcept -> const core::tour_steps& { return Steps; }
		/// \brief [[BPR-1784]]. Opens a step, as its header, Change or Next
		/// does, and puts the keyboard in its first field.
		void open_step(core::tour_step aStep);
		/// \brief [[BPR-1593]]. The publication column of the tour page.
		[[nodiscard]] auto publication_panel() const noexcept -> QWidget* { return PublicationPanel; }
		/// \brief [[BPR-1593]]. The private draft's state and Preview and publish.
		[[nodiscard]] auto foot() const noexcept -> QWidget* { return Foot; }
		[[nodiscard]] auto excerpt() const noexcept -> QWidget* { return Excerpt; }
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
		/// \brief [[BPR-1781]]. Shows the stops with `aStop` current, and goes
		/// to its lines as choosing it on the route does.
		void go_to_stop(core::tour_stop_id aStop);
		/// \brief [[BPR-1781]]. Told after every presentation, so a summary of
		/// the tour elsewhere follows the same projection.
		void               set_presented_observer(std::function<void()> aObserver);
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
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void restyle_working();
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;
		bool focusNextPrevChild(bool aNext) override;

	private:
		std::function<void()>                                                      PresentedObserver;
		void                                                                       edit(core::tour_command_kind aKind, const std::string& aText);
		void                                                                       add_writing_button(scoped_prose_editor* aEditor, core::writing_hook aHook, QBoxLayout* aLayout);
		std::function<void(core::writing_hook, prose_capture)>                     WritingRequest;
		std::function<void()>                                                      WritingCancel;
		void                                                                       refresh_preview();
		void                                                                       build_stops();
		void                                                                       present_stops(bool aOwnerChanged);
		void                                                                       edit_stop();
		void                                                                       structural(core::tour_command_kind aKind, std::size_t aPosition = 0);
		[[nodiscard]] auto                                                         selected_stops() const -> std::vector<core::tour_stop_id>;
		void                                                                       select_stops(const std::vector<core::tour_stop_id>& aStops);
		void                                                                       update_stop_actions();
		void                                                                       move_selection(int aOffset);
		void                                                                       next_stop(int aOffset);
		void                                                                       next_changed_stop();
		void                                                                       open_brief(int aField);
		void                                                                       choose_step(int aField);
		void                                                                       refresh_steps();
		void                                                                       focus_step();
		void                                                                       place_stop_card();
		void                                                                       fit_route();
		void                                                                       fit_page_columns();
		void                                                                       refresh_excerpt();
		void                                                                       refresh_foot();
		tour_editor_mode                                                           Mode = tour_editor_mode::route;
		QPointer<QWidget>                                                          RefocusStop;
		stop_action_row*                                                           StopActions   = nullptr;
		QPushButton*                                                               AddRelated    = nullptr;
		QPushButton*                                                               RemoveRelated = nullptr;
		QWidget*                                                                   MoreButton    = nullptr;
		std::function<void(bool)>                                                  ExpandDispatch;
		std::function<void()>                                                      CollapseDispatch;
		std::function<std::optional<tour_excerpt>(const core::tour_source_range&)> ExcerptSource;
		int                                                                        BriefOpen      = -1;
		QWidget*                                                                   Brief          = nullptr;
		QWidget*                                                                   BriefSlots[2]  = { nullptr, nullptr };
		QWidget*                                                                   BriefPanels[2] = { nullptr, nullptr };
		QWidget*                                                                   BriefHosts[2]  = { nullptr, nullptr };
		/// [[BPR-1784]]. The steps' headers, and the Stops step's body, which
		/// holds the route in the drawer.
		QWidget*            StepRows[3] = { nullptr, nullptr, nullptr };
		QWidget*            StopsSlot   = nullptr;
		core::tour_steps    Steps;
		QWidget*            RouteColumn        = nullptr;
		QWidget*            PageColumns        = nullptr;
		QWidget*            PageLeft           = nullptr;
		QWidget*            PageRight          = nullptr;
		QWidget*            StopsHost          = nullptr;
		QWidget*            StopHeader         = nullptr;
		QLabel*             StopHeading        = nullptr;
		QWidget*            Excerpt            = nullptr;
		QWidget*            PublicationPanel   = nullptr;
		QWidget*            OutlineTools       = nullptr;
		QWidget*            Notices            = nullptr;
		QWidget*            Foot               = nullptr;
		QLabel*             FootState          = nullptr;
		QPushButton*        PreviewPublish     = nullptr;
		QLabel*             StopState          = nullptr;
		QPushButton*        Reaffirm           = nullptr;
		QPushButton*        Previous           = nullptr;
		QPushButton*        Next               = nullptr;
		QComboBox*          PublicationMode    = nullptr;
		QCheckBox*          DescriptionEdit    = nullptr;
		QLabel*             PublicationStatus  = nullptr;
		markdown_body_view* PublicationPreview = nullptr;
		QPushButton*        PublishTour        = nullptr;
		QPushButton*        ResumeTour         = nullptr;
		QPushButton*        ReconcileTour      = nullptr;
		QPushButton*        StopTour           = nullptr;
		/// [[BPR-1782]]. Compose review's waiting panel, for the tour's Publish:
		/// what is being sent while it runs, and why it did not finish.
		QWidget*                                     PublicationWorking = nullptr;
		QLabel*                                      WorkingMark        = nullptr;
		QLabel*                                      WorkingTitle       = nullptr;
		QLabel*                                      WorkingStages      = nullptr;
		QLabel*                                      WorkingStatus      = nullptr;
		QLabel*                                      PublicationLink    = nullptr;
		QPushButton*                                 PreparePublication = nullptr;
		QComboBox*                                   Imports            = nullptr;
		QPushButton*                                 Import             = nullptr;
		QListWidget*                                 SuggestedOutline   = nullptr;
		QPushButton*                                 InsertOutline      = nullptr;
		QListWidget*                                 Outline            = nullptr;
		QListWidget*                                 Related            = nullptr;
		QLineEdit*                                   Title              = nullptr;
		scoped_prose_editor*                         Body               = nullptr;
		QLabel*                                      Source             = nullptr;
		QWidget*                                     StopFields         = nullptr;
		QAction*                                     MergeStops         = nullptr;
		QAction*                                     DuplicateStops     = nullptr;
		QAction*                                     RemoveStops        = nullptr;
		QAction*                                     UndoStructure      = nullptr;
		QAction*                                     RedoStructure      = nullptr;
		core::tour_stop_id                           Selected;
		core::tour_stop_id                           Editing;
		std::optional<core::tour_command_kind>       EditingKind;
		std::map<core::tour_stop_id, QTextDocument*> BodyDocuments;
		std::map<core::tour_stop_id, int>            BodyPositions;
		application::tour_presentation               Shown;
		std::function<void(core::tour_command)>      Dispatch;
		std::function<void(tour_editor_action)>      Action;
		scoped_prose_editor*                         Problem           = nullptr;
		scoped_prose_editor*                         Solution          = nullptr;
		QPlainTextEdit*                              Conflict          = nullptr;
		QTabWidget*                                  Roadmap           = nullptr;
		markdown_body_view*                          Preview           = nullptr;
		QLabel*                                      RevisionChanges   = nullptr;
		QPushButton*                                 NextChanged       = nullptr;
		QLabel*                                      WritingStatus     = nullptr;
		QLabel*                                      Status            = nullptr;
		QPushButton*                                 Retry             = nullptr;
		QPushButton*                                 UseSaved          = nullptr;
		QPushButton*                                 KeepLocalConflict = nullptr;
		QPushButton*                                 UseRemoteConflict = nullptr;
		std::set<const QObject*>                     Composing;
		bool                                         SelectionChanged = false;
		bool                                         Applying         = false;
	};
}
#endif
