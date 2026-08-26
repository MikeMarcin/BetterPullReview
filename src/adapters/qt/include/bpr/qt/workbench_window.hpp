#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/defect_card_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QList>
#include <QMainWindow>
#include <QString>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

class QAction;
class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QMenuBar;
class QPlainTextEdit;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QToolBar;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	class diff_workbench;
	class epoch_comparison_control;

	/// \brief Carries one semantic workbench gesture out of the presentation layer.
	using workbench_intent_dispatch = std::function<void(core::workbench_interaction_intent)>;

	/// \brief Carries one semantic change-navigation gesture out of the toolbar.
	///
	/// \details Change navigation is an RFC-BPR-0004 diff gesture rather than a
	/// workbench-state gesture, so it leaves through its own sink instead of being
	/// folded into `core::workbench_interaction_intent`.
	using workbench_change_dispatch = std::function<void(core::navigate_diff_change_intent)>;

	/// \brief Carries one already-decided receipt command out of presentation.
	using workbench_receipt_dispatch      = std::function<void(core::file_receipt_command)>;
	using workbench_review_draft_dispatch = std::function<void(core::review_draft_command)>;
	using workbench_defect_dispatch       = std::function<void(core::defect_command)>;

	/// \brief Carries the exact placeable conversation anchor chosen in Qt.
	struct workbench_defect_classification
	{
		std::string           Conversation;
		core::source_line_ref Anchor;

		auto operator<=>(const workbench_defect_classification&) const = default;
	};

	using workbench_defect_classification_dispatch
		= std::function<void(workbench_defect_classification)>;
	using workbench_queue_rule_dispatch        = std::function<void(application::queue_rule_intent)>;
	using workbench_queue_preferences_dispatch = std::function<void(application::queue_preferences_intent)>;

	/// \brief Records directly observable workbench-window presentation activity.
	struct workbench_window_statistics
	{
		std::uint64_t Publications          = 0U;
		std::uint64_t Failures              = 0U;
		std::uint64_t CommandUpdates        = 0U;
		std::uint64_t Announcements         = 0U;
		std::uint64_t DrawerUpdates         = 0U;
		std::uint64_t FileRowUpdates        = 0U;
		std::uint64_t FileSelections        = 0U;
		std::uint64_t LayoutUpdates         = 0U;
		std::uint64_t EpochUpdates          = 0U;
		std::uint64_t FocusOrderUpdates     = 0U;
		std::uint64_t ReceiptUpdates        = 0U;
		std::uint64_t ReceiptFailures       = 0U;
		std::uint64_t ReceiptGestures       = 0U;
		std::uint64_t DefectUpdates         = 0U;
		std::uint64_t DefectIntents         = 0U;
		std::uint64_t DefectClassifications = 0U;
		std::uint64_t DefectNavigation      = 0U;
		std::uint64_t QueueUpdates          = 0U;
		std::uint64_t QueueRuleIntents      = 0U;
		/// \brief Full GitHub presentation resets, which must stay at zero.
		///
		/// \details Carried over from `github_window` because the render probes
		/// assert on it: a projection must reach the panels incrementally rather
		/// than by rebuilding the presentation.
		std::uint64_t GithubFullResets        = 0U;
		std::uint64_t GithubIdentityUpdates   = 0U;
		std::uint64_t GithubDiagnosticUpdates = 0U;

		auto operator<=>(const workbench_window_statistics&) const = default;
	};

	/// \brief Presents the native workbench shell and its focused review row.
	///
	/// \details The window is an event and action adapter, never a controller. Each
	/// toolbar action translates one gesture into a semantic intent and every
	/// enabled state arrives as a decided projection; the window contains no
	/// navigation legality of its own and performs no GitHub, Git, SQLite,
	/// filesystem, network, or process work.
	///
	/// All four primary actions follow one `core::workbench_command_delta`: entry
	/// `i` of that delta decides navigation action `i`, and the delta's retained
	/// stop-versus-wrap policy travels back out with the gesture it offered. The
	/// window holds no second carrier for any command's legality.
	///
	/// One compact icon-only row presents the Files drawer at the far left, then
	/// previous/next file and previous/next change, bounded secondary review
	/// controls, a flexible spacer, and the Conversations drawer at the far right.
	/// All six primary actions stay top-level at every supported width. Their text
	/// remains available through stable tooltips, accessibility, and shortcuts,
	/// never as persistent toolbar width.
	///
	/// The central surface is a splitter holding the two review drawers and the
	/// centre. Collapsed and expanded arrive in the core's drawer delta and drawer
	/// widths arrive in the host's layout delta; the window applies both and
	/// reports an observed width back out, so neither meaning gains a second
	/// carrier. The centre holds RFC-BPR-0003's review-history control above
	/// RFC-BPR-0004's comparison workbench, both composed unchanged, and the
	/// detail pane spans the central width.
	///
	/// The window owns placement, one declared focus order, and one combined
	/// status line. Review-history loading, failure, cancellation, and empty
	/// wording is the composed control's decision; the window quotes it rather
	/// than restating it, so no state gains a second vocabulary.
	class workbench_window final
		: public QMainWindow,
		  public application::workbench_projection_port,
		  public application::epoch_projection_port,
		  public application::github_projection_port,
		  public application::queue_landing_projection_port
	{
	public:
		explicit workbench_window(
			workbench_intent_dispatch                       aDispatch       = { },
			workbench_change_dispatch                       aChangeDispatch = { },
			workbench_drawer_width_dispatch                 aWidthDispatch  = { },
			std::vector<application::github_profile_option> aProfiles       = { },
			application::github_intent_dispatch             aGithubDispatch = { },
			QWidget*                                        aParent         = nullptr);
		~workbench_window() override;

		workbench_window(const workbench_window&)                    = delete;
		auto operator=(const workbench_window&) -> workbench_window& = delete;
		workbench_window(workbench_window&&)                         = delete;
		auto operator=(workbench_window&&) -> workbench_window&      = delete;

		void publish(const application::workbench_projection& aProjection) override;
		/// \brief Adopts one decided review-history presentation update.
		///
		/// \details This is the window's single review-history entry point. The
		/// update reaches the composed control unchanged, and the window then
		/// refreshes its combined status line so the review-history state the
		/// control decided is also stated in the shell's own text.
		void publish(const application::epoch_presentation_update& aUpdate) override;

		/// \brief Adopts one GitHub presentation update.
		///
		/// \details BPR-0191: the shell is the window, so it is the window that
		/// implements RFC-BPR-0002's projection port. The work is delegation —
		/// the session chrome, the pull-request metadata and the conversation
		/// each own their own region — which is what extracting those three
		/// panels bought.
		void publish(
			const application::github_presentation_update& aUpdate) override;
		void publish(
			const application::queue_landing_projection& aProjection) override;
		void fail(
			const application::workbench_failure_publication& aFailure) override;
		/// \brief Adopts the selected file's core-decided private receipt actions.
		void publish(
			const application::file_receipt_action_projection& aProjection);
		/// \brief Presents one typed receipt refusal without changing the model.
		void fail(const application::receipt_failure_publication& aFailure);
		/// \brief Retires receipt actions, optionally naming unavailable storage.
		void clear_review_ledger(bool aUnavailable = false);

		void set_dispatch(workbench_intent_dispatch aDispatch);
		void set_change_dispatch(workbench_change_dispatch aChangeDispatch);
		void set_width_dispatch(workbench_drawer_width_dispatch aWidthDispatch);
		void set_receipt_dispatch(workbench_receipt_dispatch aReceiptDispatch);
		void set_review_draft_dispatch(
			workbench_review_draft_dispatch aDraftDispatch);
		void set_defect_dispatch(workbench_defect_dispatch aDefectDispatch);
		void set_agent_dispatch(agent_intent_dispatch aAgentDispatch);
		void set_defect_classification_dispatch(
			workbench_defect_classification_dispatch aClassificationDispatch);
		void set_queue_rule_dispatch(workbench_queue_rule_dispatch aDispatch);
		void set_queue_preferences_dispatch(
			workbench_queue_preferences_dispatch aDispatch);
		/// \brief Adopts the complete bounded local defect queue.
		void present_defects(std::vector<core::defect_history> aDefects);
		void clear_defects();

		[[nodiscard]] auto primary_toolbar() const noexcept -> QToolBar*;
		[[nodiscard]] auto previous_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto seen_action() const noexcept -> QAction*;
		[[nodiscard]] auto read_action() const noexcept -> QAction*;
		[[nodiscard]] auto accept_action() const noexcept -> QAction*;
		[[nodiscard]] auto exclude_action() const noexcept -> QAction*;
		/// \brief Opens the bounded secondary private-progress action menu.
		[[nodiscard]] auto review_controls_button() const noexcept
			-> QToolButton*;
		[[nodiscard]] auto review_controls_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto menu_bar() const noexcept -> QMenuBar*;
		[[nodiscard]] auto view_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto line_numbers_action() const noexcept -> QAction*;
		[[nodiscard]] auto files_drawer() const noexcept
			-> workbench_drawer_panel*;
		[[nodiscard]] auto conversations_drawer() const noexcept
			-> workbench_drawer_panel*;

		/// \brief The threaded conversation the conversations drawer hosts.
		///
		/// \details BPR-0191 route 2: the drawer shows a forest of review
		/// threads rather than a list, so it has no `contents()` model of its
		/// own. This is what it shows instead.
		[[nodiscard]] auto conversation_panel() const noexcept
			-> github_conversation_panel*;
		[[nodiscard]] auto agent_panel() const noexcept -> agent_launch_panel*;
		[[nodiscard]] auto review_editor() const noexcept -> review_draft_editor*;
		[[nodiscard]] auto defect_card() const noexcept -> defect_card_view*;
		[[nodiscard]] auto classify_defect_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_defect_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_defect_action() const noexcept -> QAction*;
		[[nodiscard]] auto defect_count_label() const noexcept -> QLabel*;
		[[nodiscard]] auto shown_defect_index() const noexcept -> std::optional<std::size_t>;
		[[nodiscard]] auto defect_count() const noexcept -> std::size_t;
		[[nodiscard]] auto queue_landing() const noexcept -> review_landing_model*;
		[[nodiscard]] auto queue_filter_model() const noexcept
			-> review_landing_filter_model*;
		[[nodiscard]] auto queue_view() const noexcept -> QListView*;
		/// \brief Selects and focuses one exact visible local-queue identity.
		///
		/// \details A hidden or absent identity reports a visible local failure and
		/// leaves queue preferences and provider navigation untouched.
		[[nodiscard]] auto select_queue_item(
			const core::pull_request_ref& aRef) -> bool;
		[[nodiscard]] auto queue_role_filter() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_observer_filter() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_bucket_filter() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_repository_filter() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_quiet_filter() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_sort_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_rule_kind_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto queue_rule_pattern_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto queue_rule_exclusion() const noexcept -> QCheckBox*;
		[[nodiscard]] auto add_queue_rule_action() const noexcept -> QAction*;
		[[nodiscard]] auto remove_queue_rule_action() const noexcept -> QAction*;
		[[nodiscard]] auto queue_status_label() const noexcept -> QLabel*;

		[[nodiscard]] auto session_panel() const noexcept -> github_session_panel*;
		[[nodiscard]] auto pull_request_panel() const noexcept
			-> github_pull_request_panel*;
		[[nodiscard]] auto github_connect_action() const noexcept -> QAction*;
		[[nodiscard]] auto github_open_action() const noexcept -> QAction*;
		[[nodiscard]] auto github_refresh_action() const noexcept -> QAction*;
		[[nodiscard]] auto show_review_threads_action() const noexcept -> QAction*;

		/// \brief RFC-BPR-0002's session wording, which the shell now presents.
		///
		/// \details Named apart from `status_text()`, which is the workbench's
		/// own status and a different string in a different type. Two windows
		/// each having a `status_text` was survivable; one window with two is
		/// not, and silently returning the wrong one would be worse than the
		/// longer name.
		[[nodiscard]] auto github_status_text() const -> std::string;
		[[nodiscard]] auto github_diagnostic_text() const -> std::string;
		[[nodiscard]] auto active_host_text() const -> std::string;
		[[nodiscard]] auto active_account_text() const -> std::string;

		/// \brief The composed conversation's view and model.
		[[nodiscard]] auto conversation_view() const noexcept -> QTreeView*;
		[[nodiscard]] auto conversation_model() const noexcept
			-> github_conversation_model*;
		[[nodiscard]] auto profile_selector() const noexcept -> QComboBox*;

		/// \brief The appearance surface, composed beside the review it governs.
		[[nodiscard]] auto appearance_settings() const noexcept
			-> appearance_settings_panel*;
		[[nodiscard]] auto files_drawer_toggle_action() const noexcept -> QAction*;
		[[nodiscard]] auto conversations_drawer_toggle_action() const noexcept
			-> QAction*;
		/// \brief Shows or hides secondary review metadata around the comparison.
		///
		/// \details The action stays visible while the metadata is hidden, so a
		/// focused reviewer can return the vertical space to the code panes without
		/// losing access to session, queue, pull-request, epoch, appearance, or
		/// selected-change details.
		[[nodiscard]] auto review_details_toggle_action() const noexcept -> QAction*;

		/// \brief Presents the composed independent left and right epoch selectors.
		///
		/// \details RFC-BPR-0003 stays the sole owner of review-history identity,
		/// legality, and wording. The window composes its control unchanged above
		/// the panes, so the exact copyable OIDs, the arbitrary retained-epoch
		/// selection, and the review-history state text keep one owner and the
		/// shell adds no second epoch authority.
		[[nodiscard]] auto epoch_selectors() const noexcept
			-> epoch_comparison_control*;

		/// \brief Reports the window's declared deterministic focus order.
		///
		/// \details The order is rebuilt from live widget identities and declares
		/// the shell-owned structural sequence: the compact icon row, the file
		/// drawer, the two
		/// epoch selectors with their exact OIDs, the composed comparison
		/// surface, the conversation drawer, and the status line. The composed
		/// surface contributes one stop, because ordering inside it stays
		/// RFC-BPR-0004's decision and the shell must not restate it. Evidence for
		/// the currently available chain filters this declaration by visibility,
		/// enabled state, and `Qt::TabFocus` rather than creating another order.
		[[nodiscard]] auto focus_order() const -> QList<QWidget*>;

		/// \brief Presents the composed RFC-BPR-0004 comparison surface.
		///
		/// \details RFC-BPR-0004 stays the sole owner of semantic diff, mapping,
		/// grouping, and linked-scroll behaviour. The window owns placement only,
		/// so the composed surface keeps its one shared vertical scrollbar, its one
		/// link map, and its natural-layout documents. The window adds no second
		/// mapping, no presentation-side regrouping, no follower-scroll coupling,
		/// and no synthetic alignment rows.
		[[nodiscard]] auto comparison_surface() const noexcept -> diff_workbench*;
		/// \brief Binds GitHub current-line placement to the selected endpoints.
		void set_conversation_endpoint_placement(
			bool aLeftHasCurrentDiffRevision,
			bool aRightHasCurrentDiffRevision);
		/// \brief Returns the composed full-width line-over-line detail pane.
		///
		/// \details This forwards to RFC-BPR-0004's comparison surface, which
		/// owns the pane and resolves its rows from the one canonical semantic
		/// mapping. The window adds no second detail authority.
		[[nodiscard]] auto detail_pane() const noexcept -> QPlainTextEdit*;

		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> workbench_window_statistics;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		/// \brief Binds one primary action to its presentation wording.
		///
		/// \details `Name` is retained action wording for native menus and platform
		/// affordances; the persistent toolbar presenter is always icon-only.
		/// `Accessible` stays unique inside the assembled window.
		struct navigation_entry
		{
			QAction* Action = nullptr;
			QString  Name;
			QString  Accessible;
			QString  Purpose;
		};

		/// \brief Binds one drawer to its toggle action and presentation wording.
		///
		/// \details `Accessible` names the toggle rather than the drawer, because
		/// the composed drawer panel and its list view already answer to the
		/// drawer's own name.
		struct drawer_entry
		{
			workbench_drawer_panel* Panel  = nullptr;
			QAction*                Toggle = nullptr;
			QString                 Name;
			QString                 Accessible;
			QString                 Collapse;
			QString                 Expand;
		};

		void build_primary_toolbar();
		void build_central_surface();
		void build_github_header();
		void apply_github_transition(
			const application::github_transition_projection& aTransition);
		void apply_github_pull_request(
			const application::github_pull_request_projection& aPullRequest);
		void apply_github_content_command(
			const core::github_command_projection& aCommand);
		void apply_github_host_diagnostic(
			const application::github_host_diagnostic_projection& aDiagnostic);
		void clear_github_pull_request();
		void update_github_presentation_state();
		void focus_review_threads();
		void announce_github_presentation_update();
		void dispatch_navigation(std::size_t aIndex);
		void dispatch_drawer_toggle(std::size_t aIndex);
		void observe_drawer_width(
			core::workbench_drawer aDrawer,
			std::uint32_t          aWidth);
		void apply_drawer_layout();
		void configure_tool_buttons();
		void refresh_toolbar_icons();
		void apply_focus_order();
		void update_action_state();
		void build_menu_bar();
		void dispatch_line_numbers_toggle();
		void apply_line_numbers();
		void update_drawer_action(
			std::size_t                  aIndex,
			core::workbench_command_id   aId,
			core::workbench_drawer_state aState);
		void update_status_text();
		void announce_update();
		void apply_file_rows(
			const std::vector<application::workbench_file_projection>& aFiles);
		void               apply_file_selection();
		void               dispatch_file_selection(const QModelIndex& aIndex);
		void               dispatch_receipt(std::size_t aIndex);
		void               update_receipt_actions();
		void               classify_selected_conversation();
		void               navigate_defect(int aDirection);
		void               update_defect_surface();
		[[nodiscard]] auto selected_conversation_classification() const
			-> std::optional<workbench_defect_classification>;
		void synchronize_selected_conversation(const QModelIndex& aIndex);
		void build_queue_landing(QWidget* aParent, QVBoxLayout& aLayout);
		void dispatch_queue_rule(application::queue_rule_action aAction);
		void update_queue_rule_actions();
		void dispatch_queue_preferences();
		void apply_queue_preferences(
			const application::queue_presentation_preferences& aPreferences);
		void rebuild_queue_repositories(
			std::span<const core::queue_item> aItems,
			const std::string&                aSelectedRepository);
		void update_queue_status(
			const application::queue_landing_projection& aProjection);

		[[nodiscard]] auto command(core::workbench_command_id aId) const
			-> std::optional<core::workbench_command_projection>;
		[[nodiscard]] auto central_sizes() const -> QList<int>;

		workbench_intent_dispatch                Dispatch;
		workbench_change_dispatch                ChangeDispatch;
		workbench_drawer_width_dispatch          WidthDispatch;
		workbench_receipt_dispatch               ReceiptDispatch;
		workbench_review_draft_dispatch          ReviewDraftDispatch;
		workbench_defect_dispatch                DefectDispatch;
		workbench_defect_classification_dispatch DefectClassificationDispatch;
		workbench_queue_rule_dispatch            QueueRuleDispatch;
		workbench_queue_preferences_dispatch     QueuePreferencesDispatch;

		QToolBar*       PrimaryToolbar       = nullptr;
		QMenuBar*       PrimaryMenuBar       = nullptr;
		QMenu*          ViewMenu             = nullptr;
		QAction*        LineNumbersAction    = nullptr;
		QAction*        ReviewDetailsAction  = nullptr;
		QToolButton*    ReviewControlsButton = nullptr;
		QMenu*          ReviewControlsMenu   = nullptr;
		QWidget*        ToolbarSpacer        = nullptr;
		QLabel*         StatusLabel          = nullptr;
		QSplitter*      CentralSplitter      = nullptr;
		QWidget*        CentreSurface        = nullptr;
		diff_workbench* ComparisonSurface    = nullptr;

		epoch_comparison_control* EpochSelectors = nullptr;

		std::array<navigation_entry, 4> Navigation{ };
		std::array<QAction*, 4>         ReceiptActions{ };
		std::array<drawer_entry, 2>     Drawers{ };
		github_conversation_panel*      ConversationPanel = nullptr;
		/// Prevents the drawer refilter from clearing the anchor that triggered it.
		bool                                                       ApplyingConversationAnchor       = false;
		bool                                                       LeftConversationEndpointCurrent  = false;
		bool                                                       RightConversationEndpointCurrent = false;
		agent_launch_panel*                                        AgentPanel                       = nullptr;
		review_draft_editor*                                       ReviewEditor                     = nullptr;
		defect_card_view*                                          DefectCard                       = nullptr;
		QLabel*                                                    DefectCountLabel                 = nullptr;
		QAction*                                                   ClassifyDefect                   = nullptr;
		QAction*                                                   PreviousDefect                   = nullptr;
		QAction*                                                   NextDefect                       = nullptr;
		std::vector<core::defect_history>                          Defects;
		std::optional<std::size_t>                                 ShownDefectIndex;
		review_landing_model*                                      QueueLanding          = nullptr;
		review_landing_filter_model*                               QueueFilterModel      = nullptr;
		QListView*                                                 QueueView             = nullptr;
		QComboBox*                                                 QueueRoleFilter       = nullptr;
		QComboBox*                                                 QueueObserverFilter   = nullptr;
		QComboBox*                                                 QueueBucketFilter     = nullptr;
		QComboBox*                                                 QueueRepositoryFilter = nullptr;
		QComboBox*                                                 QueueQuietFilter      = nullptr;
		QComboBox*                                                 QueueSort             = nullptr;
		QComboBox*                                                 QueueRuleSelector     = nullptr;
		QComboBox*                                                 QueueRuleKind         = nullptr;
		QLineEdit*                                                 QueueRulePattern      = nullptr;
		QCheckBox*                                                 QueueRuleExcluding    = nullptr;
		QAction*                                                   AddQueueRule          = nullptr;
		QAction*                                                   RemoveQueueRule       = nullptr;
		QToolButton*                                               AddQueueRuleButton    = nullptr;
		QToolButton*                                               RemoveQueueRuleButton = nullptr;
		QLabel*                                                    QueueStatus           = nullptr;
		std::vector<core::observer_rule>                           QueueRules;
		application::queue_landing_projection                      QueueProjection;
		QScrollArea*                                               HeaderScroll         = nullptr;
		github_session_panel*                                      SessionPanel         = nullptr;
		github_pull_request_panel*                                 PullRequestPanel     = nullptr;
		appearance_settings_panel*                                 AppearanceSettings   = nullptr;
		QStackedWidget*                                            PullRequestStack     = nullptr;
		QLabel*                                                    EmptyStateTitleLabel = nullptr;
		QLabel*                                                    EmptyStateBodyLabel  = nullptr;
		QString                                                    LastGithubAnnouncement;
		QAction*                                                   GithubConnectAction     = nullptr;
		QAction*                                                   GithubOpenAction        = nullptr;
		QAction*                                                   GithubRefreshAction     = nullptr;
		QAction*                                                   ShowReviewThreadsAction = nullptr;
		std::vector<application::github_profile_option>            GithubProfiles;
		application::github_intent_dispatch                        GithubDispatch;
		bool                                                       HasGithubPullRequest     = false;
		bool                                                       ApplyingQueuePreferences = false;
		core::workbench_command_delta                              Commands;
		core::workbench_drawer_delta                               DrawerStates;
		core::workbench_line_numbers_delta                         LineNumberState;
		std::optional<application::workbench_layout_delta>         LayoutWidths;
		std::optional<core::workbench_navigation_cursor>           Cursor;
		std::optional<application::workbench_host_failure>         Failure;
		std::optional<application::file_receipt_action_projection> Receipt;
		std::optional<application::receipt_failure_publication>    ReceiptFailure;
		bool                                                       LedgerUnavailable     = false;
		bool                                                       HasCommands           = false;
		bool                                                       ApplyingLayout        = false;
		bool                                                       ApplyingFileSelection = false;
		bool                                                       QueueReady            = false;
		// Set by the first published review-history update. Before one arrives
		// the shell has no review-history state to state, and quoting the
		// control's idle wording would report activity that never happened.
		bool                        HasEpochState = false;
		QString                     LastAnnouncement;
		workbench_window_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
