#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/core/tour_steps.hpp"
#include "bpr/core/tour_stop_view.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include <QFont>
#include <QPointer>
#include <QWidget>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>
class QAction;
class QHBoxLayout;
class QCheckBox;
class QComboBox;
class QListWidget;
class QLineEdit;
class QTextDocument;
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class QPushButton;
class QToolButton;
class QBoxLayout;
class QHBoxLayout;
class QVBoxLayout;
namespace bpr::qt {
	class markdown_body_view;
	class stop_action_row;
	class tour_agent_panel;
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
												   insert_outline,
												   /// [[BPR-1914]] v002. Ask about the open stop's code: the
												   /// lines selected in the code, or the stop's own.
												   ask_code };
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
		/// \brief [[BPR-1892]], owner 2026-11-06. A failed agent request,
		/// beside the control that made it: "<request> failed." and
		/// `aWhy`, the Agent tab's own words for the failure, with the way
		/// to the Agent tab's card for the run. Nothing switches tabs.
		void show_agent_failure(core::writing_hook aHook, const QString& aWhy);
		/// \brief [[BPR-1892]]. Clears it: its request made again, or
		/// succeeded.
		void clear_agent_failure(core::writing_hook aHook);
		/// \brief [[BPR-1892]]. Show on the Agent tab: asks the window for
		/// the Agent tab at the failed run's card.
		void set_agent_failure_dispatch(std::function<void()> aDispatch);
		/// \brief [[BPR-1900]]. The tour session's gestures: Draft the tour,
		/// Stop (a drafting run, from the control that started it), and one
		/// proposed part's Accept, with the author's edit, or Discard.
		void set_tour_session_dispatch(std::function<void()> aDraft, std::function<void()> aStop, std::function<void(std::size_t, core::tour_proposal_decision, const QString&)> aDecide, std::function<void(std::size_t)> aRelocate = { });
		/// \brief [[BPR-1900]]. The tour session and its proposal, as last
		/// decided.
		void present_tour_session(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal);
		/// \brief [[BPR-1900]]. A drafting run is running: started by the
		/// control of `aHook` (Draft from PR context, Suggest an outline), or
		/// by Draft the tour. Its control says Stop drafting and stops it;
		/// nothing else waits on it, and leaving the window does not stop it.
		void set_tour_running(std::optional<core::writing_hook> aHook, bool aDraftTour, bool aAsking = false);
		/// \brief [[BPR-1914]] v003. Every side chat's composer and Update send
		/// here: the one entry point a reviewer's `/skill` references
		/// ([[BPR-1916]]) are resolved through.
		void set_tour_ask_dispatch(tour_side_chat::ask_dispatch aAsk);
		/// \brief Shows (true) or closes (false) the chat column; the window
		/// places it beside the tour, in the Tour tab ([[BPR-1925]]).
		void set_side_chat_visibility_dispatch(std::function<void(bool)> aVisibility);
		/// \brief [[BPR-1925]]. Told after the tour has marked the part under
		/// discussion and counted the chats: the window's Tour tab badge.
		void set_side_chat_observer(std::function<void()> aObserver) { SideChatObserver = std::move(aObserver); }
		/// \brief The chat column, for the window to place in the Tour tab.
		[[nodiscard]] auto side_chat() const noexcept -> tour_side_chat* { return SideChat; }
		/// \brief Opens the side chat about `aThread`, quoting `aPassage`.
		void open_side_chat(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage = std::nullopt);
		/// \brief [[BPR-1925]] v002. What a part's chat toggle does: opens the
		/// chat about `aThread` (a stop's with its lines in the code), or
		/// closes the column when that chat is the one in it.
		void toggle_side_chat(const core::tour_thread& aThread);
		/// \brief Opens the open stop's side chat on its code: `aSelected` (the
		/// lines selected in the code) or the stop's own.
		void ask_about_code(const std::optional<core::tour_source_range>& aSelected);
		/// \brief Brings the tour to a chat's part, and a stop's lines into
		/// the code.
		void reveal_part(const core::tour_thread& aThread);
		/// \brief [[BPR-1919]]. The panes' code colours for rendered tour text.
		void set_code_theme(const markdown_code_theme& aTheme);
		/// \brief [[BPR-1900]]. Draft the tour failed, in BPR-1892's notice.
		void show_tour_draft_failure(const QString& aWhy);
		void clear_tour_draft_failure();
		/// \brief [[BPR-1900]]. What a request's control said when pressed.
		[[nodiscard]] auto request_name(core::writing_hook aHook) const -> QString;
		[[nodiscard]] auto tour_agent() const noexcept -> tour_agent_panel* { return TourAgentPanel; }
		/// \brief [[BPR-1892]]. The notice for a request's control, shown or
		/// not, and the line it says.
		[[nodiscard]] auto agent_failure_notice(core::writing_hook aHook) const -> QWidget*;
		[[nodiscard]] auto agent_failure_text(core::writing_hook aHook) const -> QString;
		/// \brief [[BPR-1878]], owner looks v001 to v003. The Agent tab's own
		/// agent picker (a second `agent_run_settings_picker` on its source)
		/// in the tab's bottom bar, styled as the Agent tab's and the last
		/// thing in the tab, so choosing here chooses there.
		void               set_agent_picker(agent_run_settings_picker* aPicker);
		[[nodiscard]] auto agent_picker() const noexcept -> agent_run_settings_picker* { return AgentPicker; }
		/// \brief [[BPR-1881]], owner 2026-11-06. The Agent tab's setup
		/// readiness. Until it is `ready` the tab's top says to set the agent
		/// up (only while `unfinished`), and every agent feature is disabled
		/// with a tooltip saying why. Agent features wear `agent_accent`
		/// in every state. Ready until told otherwise.
		void               set_agent_readiness(agent_setup_readiness aReadiness);
		[[nodiscard]] auto agent_readiness() const noexcept -> agent_setup_readiness { return AgentReadiness; }
		/// \brief [[BPR-1881]]. Set up the agent, in the call to action:
		/// asks the window for the Agent tab at its setup. The tab never
		/// asks by itself.
		void set_agent_setup_dispatch(std::function<void()> aDispatch);
		/// \brief [[BPR-1881]]. Told each time the tab shows, so the setup
		/// may look for agents and say whether it has finished.
		void set_shown_observer(std::function<void()> aObserver);
		/// \brief [[BPR-1881]]. The call to action at the top of the tab.
		[[nodiscard]] auto agent_setup_call() const noexcept -> QWidget* { return AgentSetupCall; }
		/// \brief [[BPR-1881]]. Every agent feature on the tab: the drafting
		/// helpers, Suggest an outline and its Insert, and the agent bar.
		[[nodiscard]] auto agent_features() const -> std::vector<QWidget*>;
		/// \brief [[BPR-1881]]. What a disabled agent feature's tooltip says.
		[[nodiscard]] auto agent_unavailable_reason() const -> QString;
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
		/// \brief [[BPR-1805]]. The open stop, and whether it is read or written.
		[[nodiscard]] auto stop_view() const noexcept -> const core::tour_stop_view& { return StopView; }
		/// \brief [[BPR-1805]]. The pencil: opens the stop's title and source,
		/// with the keyboard in them. Refused while the draft cannot be written.
		void begin_stop_edit();
		/// \brief [[BPR-1805]]. Done: reads the stop again, unless the editors
		/// hold text the draft has not taken.
		void finish_stop_edit();
		/// \brief [[BPR-1805]]. The stop's rendered explanation.
		[[nodiscard]] auto stop_reader() const -> markdown_body_view* { return StopReader; }
		[[nodiscard]] auto outline() const -> QListWidget* { return Outline; }
		[[nodiscard]] auto stop_title() const -> QLineEdit* { return Title; }
		[[nodiscard]] auto stop_body() const -> QPlainTextEdit* { return Body; }
		[[nodiscard]] auto problem_editor() const -> QPlainTextEdit* { return Problem; }
		[[nodiscard]] auto solution_editor() const -> QPlainTextEdit* { return Solution; }
		[[nodiscard]] auto roadmap() const -> QTabWidget* { return Roadmap; }
		[[nodiscard]] auto preview() const -> markdown_body_view* { return Preview; }
		[[nodiscard]] auto publication_preview() const -> markdown_body_view* { return PublicationPreview; }
		/// \brief [[BPR-1805]] repair. The code typography the stop's reader
		/// and the previews set code in, as comments set theirs; the default
		/// typography's code font until the window hands the profile's.
		void set_code_font(const QFont& aFont);
		/// \brief [[BPR-1805]] repair. Where the readers ask for their fenced
		/// code's colours, as comments do; answered by `apply_code_syntax`.
		void               set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void               apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		[[nodiscard]] auto shown() const -> const application::tour_presentation& { return Shown; }
		/// \brief True while an input method holds uncommitted preedit text in any field; committed text is unaffected.
		[[nodiscard]] auto composing() const -> bool { return !Composing.empty(); }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void hideEvent(QHideEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
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
		void                                                                       view_stop(core::tour_stop_view_event aEvent);
		void                                                                       present_stop_view();
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
		void                                                                       place_stop_card_now();
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
		QWidget*         StepRows[3] = { nullptr, nullptr, nullptr };
		QWidget*         StopsSlot   = nullptr;
		core::tour_steps Steps;
		QWidget*         RouteColumn      = nullptr;
		QWidget*         PageColumns      = nullptr;
		QWidget*         PageLeft         = nullptr;
		QWidget*         PageRight        = nullptr;
		QWidget*         StopsHost        = nullptr;
		QWidget*         StopHeader       = nullptr;
		QLabel*          StopHeading      = nullptr;
		QWidget*         Excerpt          = nullptr;
		QWidget*         PublicationPanel = nullptr;
		QWidget*         OutlineTools     = nullptr;
		QWidget*         Notices          = nullptr;
		/// [[BPR-1878]]. The tab's bottom bar, its field, and the picker in it.
		QWidget*                   AgentBar    = nullptr;
		QWidget*                   AgentField  = nullptr;
		agent_run_settings_picker* AgentPicker = nullptr;
		/// [[BPR-1881]]. The call to action, its button, and the setup's
		/// readiness; the helpers' labels, run again when it changes.
		QWidget*                           AgentSetupCall   = nullptr;
		QPushButton*                       AgentSetupButton = nullptr;
		QPushButton*                       SuggestOutline   = nullptr;
		std::function<void()>              AgentSetupDispatch;
		std::function<void()>              ShownObserver;
		agent_setup_readiness              AgentReadiness = agent_setup_readiness::ready;
		std::vector<std::function<void()>> AgentLabels;
		tour_side_chat*                    SideChat = nullptr;
		std::vector<QToolButton*>          ChatButtons;
		std::function<void()>              SideChatObserver;
		/// [[BPR-1925]]. The widgets that show a part, each with the part it
		/// shows (empty while it shows none): marked with the agent's rule
		/// while that part's chat is in the column.
		std::vector<std::pair<QWidget*, std::function<std::optional<core::tour_thread>()>>> PartMarks;
		void                                                                                mark_part(QWidget* aWidget, std::function<std::optional<core::tour_thread>()> aPart);
		/// [[BPR-1925]]. Marks the part under discussion, counts each part's
		/// turns on its glyph, and has the chat follow the route between stops.
		void                                                                           mark_side_chat();
		tour_side_chat::ask_dispatch                                                   TourAsk;
		std::function<void()>                                                          TourStop;
		std::function<void(std::size_t, core::tour_proposal_decision, const QString&)> TourDecide;
		std::optional<markdown_code_theme>                                             CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)>    CodeSyntaxDispatch;
		void                                                                           wire_side_chat();
		/// [[BPR-1914]] v004. Keeps the Tour tab's scroll position through what a
		/// click sets off.
		void                    hold_scroll();
		std::optional<int>      HeldScroll;
		std::uint64_t           HeldScrollGeneration = 0U;
		QMetaObject::Connection HeldScrollWatch;
		QMetaObject::Connection HeldScrollRelease;
		void                    open_side_chat_about(core::writing_hook aHook, const QString& aSelected);
		/// [[BPR-1925]] v002. A part's toggle: opens its chat, switches to it,
		/// or, on the part whose chat is open, closes the column.
		void         toggle_side_chat_about(core::writing_hook aHook, const QString& aSelected);
		QHBoxLayout* StopTitleRow = nullptr;
		void         apply_agent_code_style();
		/// [[BPR-1892]]. Each request's failure notice, what its control
		/// said when it was pressed, and where Show on the Agent tab goes.
		std::map<core::writing_hook, QWidget*> FailureNotices;
		std::map<core::writing_hook, QString>  RequestNames;
		std::function<void()>                  AgentFailureDispatch;
		/// [[BPR-1900]]. The tour session's panel, Draft the tour's failure
		/// notice, and the drafting run in flight.
		tour_agent_panel* TourAgentPanel = nullptr;
		QWidget*          DraftFailure   = nullptr;
		/// [[BPR-1925]] v003. The proposal waits in the tour, on the parts it
		/// changes: proposed stops under the route, grey until accepted; a
		/// stop's new explanation on its card; the Problem's and the
		/// Solution's under their own text.
		QWidget*                                   ProposedStops = nullptr;
		std::map<std::uint64_t, QPointer<QWidget>> StopTextProposals;
		std::array<bool, 2>                        PartProposed{ false, false };
		std::size_t                                ProposedStopCount = 0U;
		bool                                       ShowsStopProposal = false;
		QVBoxLayout*                               StopFieldsLayout  = nullptr;
		QPushButton*                               Complete          = nullptr;
		QPushButton*                               ShowInCode        = nullptr;
		std::optional<core::tour_stop_id>          AdvanceFrom;
		void                                       begin_proposals();
		[[nodiscard]] auto                         place_proposal(std::size_t aIndex, const core::tour_proposal_item& aItem, QWidget* aBox) -> bool;
		void                                       mark_proposed();
		void                                       place_stop_text_proposal();
		void                                       refresh_complete();
		std::optional<core::writing_hook>          RunningHook;
		bool                                       RunningDraft = false;
		void                                       open_failed_run();
		auto                                       add_failure_notice(core::writing_hook aHook, QWidget* aParent) -> QWidget*;
		void                                       refresh_agent_features();
		void                                       restyle_agent_features();
		QWidget*                                   Foot               = nullptr;
		QLabel*                                    FootState          = nullptr;
		QPushButton*                               PreviewPublish     = nullptr;
		QLabel*                                    StopState          = nullptr;
		QPushButton*                               Reaffirm           = nullptr;
		QPushButton*                               Previous           = nullptr;
		QPushButton*                               Next               = nullptr;
		QComboBox*                                 PublicationMode    = nullptr;
		QCheckBox*                                 DescriptionEdit    = nullptr;
		QLabel*                                    PublicationStatus  = nullptr;
		markdown_body_view*                        PublicationPreview = nullptr;
		QPushButton*                               PublishTour        = nullptr;
		QPushButton*                               ResumeTour         = nullptr;
		QPushButton*                               ReconcileTour      = nullptr;
		QPushButton*                               StopTour           = nullptr;
		/// [[BPR-1782]]. Compose review's waiting panel, for the tour's Publish:
		/// what is being sent while it runs, and why it did not finish.
		QWidget*             PublicationWorking = nullptr;
		QLabel*              WorkingMark        = nullptr;
		QLabel*              WorkingTitle       = nullptr;
		QLabel*              WorkingStages      = nullptr;
		QLabel*              WorkingStatus      = nullptr;
		QLabel*              PublicationLink    = nullptr;
		QPushButton*         PreparePublication = nullptr;
		QComboBox*           Imports            = nullptr;
		QPushButton*         Import             = nullptr;
		QListWidget*         SuggestedOutline   = nullptr;
		QPushButton*         InsertOutline      = nullptr;
		QListWidget*         Outline            = nullptr;
		QListWidget*         Related            = nullptr;
		QLineEdit*           Title              = nullptr;
		scoped_prose_editor* Body               = nullptr;
		QLabel*              Source             = nullptr;
		/// [[BPR-1805]]. The stop read: its title, its rendered explanation,
		/// the pencil; and Done while it is written.
		core::tour_stop_view StopView;
		QLabel*              ReadTitle        = nullptr;
		markdown_body_view*  StopReader       = nullptr;
		QToolButton*         EditStop         = nullptr;
		QPushButton*         DoneStop         = nullptr;
		QWidget*             StopMagic        = nullptr;
		bool                 StopPlacePending = false;
		/// [[BPR-1805]] repair. The readers' code font, once the window
		/// hands the profile's; and applying it to every reader.
		std::optional<QFont>                         ReaderCodeFont;
		void                                         apply_reader_code_font();
		QWidget*                                     StopFields     = nullptr;
		QAction*                                     MergeStops     = nullptr;
		QAction*                                     DuplicateStops = nullptr;
		QAction*                                     RemoveStops    = nullptr;
		QAction*                                     UndoStructure  = nullptr;
		QAction*                                     RedoStructure  = nullptr;
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
