#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
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
#include <vector>

class QAction;
class QEvent;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QResizeEvent;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QToolBar;
class QToolButton;

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

	enum class workbench_toolbar_mode : std::uint8_t {
		expanded,
		compact,
	};

	/// \brief Records directly observable workbench-window presentation activity.
	struct workbench_window_statistics
	{
		std::uint64_t Publications       = 0U;
		std::uint64_t Failures           = 0U;
		std::uint64_t CommandUpdates     = 0U;
		std::uint64_t ToolbarModeChanges = 0U;
		std::uint64_t Announcements      = 0U;
		std::uint64_t DrawerUpdates      = 0U;
		std::uint64_t FileRowUpdates     = 0U;
		std::uint64_t FileSelections     = 0U;
		std::uint64_t LayoutUpdates      = 0U;
		std::uint64_t EpochUpdates       = 0U;
		std::uint64_t FocusOrderUpdates  = 0U;
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

	/// \brief Presents the native workbench shell and its primary review toolbar.
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
	/// Primary navigation stays top-level while the window is at least
	/// compact_width_threshold() wide. Below that it moves into an explicit compact
	/// overflow control. The actions stay associated with the window in both modes,
	/// so their window shortcuts remain live and the overflow itself is focusable
	/// and mnemonic-addressable; navigation never silently disappears.
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
		  public application::github_projection_port
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
		void fail(
			const application::workbench_failure_publication& aFailure) override;

		void set_dispatch(workbench_intent_dispatch aDispatch);
		void set_change_dispatch(workbench_change_dispatch aChangeDispatch);
		void set_width_dispatch(workbench_drawer_width_dispatch aWidthDispatch);

		[[nodiscard]] auto primary_toolbar() const noexcept -> QToolBar*;
		[[nodiscard]] auto previous_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto navigation_overflow_button() const noexcept
			-> QToolButton*;
		[[nodiscard]] auto navigation_overflow_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto toolbar_mode() const noexcept -> workbench_toolbar_mode;

		/// \brief Computes the narrowest width that still fits top-level navigation.
		///
		/// \details Derived from the current style and font only, never from the
		/// current toolbar mode, so entering the compact overflow cannot change the
		/// answer that put the toolbar there.
		[[nodiscard]] auto compact_width_threshold() const -> int;

		/// \brief Presents the drawer toggles outside the primary toolbar.
		///
		/// \details The drawer toggles stay top-level at every supported width, so
		/// they must never compete with primary navigation for toolbar width and
		/// must not enter compact_width_threshold().
		[[nodiscard]] auto drawer_toolbar() const noexcept -> QToolBar*;
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
		/// \details The order is rebuilt from live widgets, so it names exactly
		/// the interactive stops the shell currently offers: primary navigation
		/// or its compact overflow, the drawer toggles, the file drawer, the two
		/// epoch selectors with their exact OIDs, the composed comparison
		/// surface, the conversation drawer, and the status line. The composed
		/// surface contributes one stop, because ordering inside it stays
		/// RFC-BPR-0004's decision and the shell must not restate it.
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
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief Binds one primary action to its presentation wording.
		///
		/// \details `Name` is the visible toolbar label and the width the compact
		/// threshold reserves for it. `Accessible` is the assistive-technology
		/// name, which must stay unique inside the assembled window: the composed
		/// comparison surface offers its own in-surface change buttons, so the
		/// toolbar cannot answer to the same name.
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
		void build_drawer_toolbar();
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
		void apply_layout_mode();
		void configure_tool_buttons();
		void apply_focus_order();
		void update_action_state();
		void update_drawer_action(
			std::size_t                  aIndex,
			core::workbench_command_id   aId,
			core::workbench_drawer_state aState);
		void update_status_text();
		void announce_update();
		void apply_file_rows(
			const std::vector<application::workbench_file_projection>& aFiles);
		void apply_file_selection();
		void dispatch_file_selection(const QModelIndex& aIndex);

		[[nodiscard]] auto command(core::workbench_command_id aId) const
			-> std::optional<core::workbench_command_projection>;
		[[nodiscard]] auto central_sizes() const -> QList<int>;

		workbench_intent_dispatch       Dispatch;
		workbench_change_dispatch       ChangeDispatch;
		workbench_drawer_width_dispatch WidthDispatch;

		QToolBar*       PrimaryToolbar    = nullptr;
		QToolBar*       DrawerToolbar     = nullptr;
		QAction*        SeparatorAction   = nullptr;
		QAction*        OverflowAction    = nullptr;
		QToolButton*    OverflowButton    = nullptr;
		QMenu*          OverflowMenu      = nullptr;
		QLabel*         StatusLabel       = nullptr;
		QSplitter*      CentralSplitter   = nullptr;
		QWidget*        CentreSurface     = nullptr;
		diff_workbench* ComparisonSurface = nullptr;

		epoch_comparison_control* EpochSelectors = nullptr;

		std::array<navigation_entry, 4>                    Navigation{ };
		std::array<drawer_entry, 2>                        Drawers{ };
		github_conversation_panel*                         ConversationPanel    = nullptr;
		QScrollArea*                                       HeaderScroll         = nullptr;
		github_session_panel*                              SessionPanel         = nullptr;
		github_pull_request_panel*                         PullRequestPanel     = nullptr;
		appearance_settings_panel*                         AppearanceSettings   = nullptr;
		QStackedWidget*                                    PullRequestStack     = nullptr;
		QLabel*                                            EmptyStateTitleLabel = nullptr;
		QLabel*                                            EmptyStateBodyLabel  = nullptr;
		QString                                            LastGithubAnnouncement;
		QAction*                                           GithubConnectAction     = nullptr;
		QAction*                                           GithubOpenAction        = nullptr;
		QAction*                                           GithubRefreshAction     = nullptr;
		QAction*                                           ShowReviewThreadsAction = nullptr;
		std::vector<application::github_profile_option>    GithubProfiles;
		application::github_intent_dispatch                GithubDispatch;
		bool                                               HasGithubPullRequest = false;
		core::workbench_command_delta                      Commands;
		core::workbench_drawer_delta                       DrawerStates;
		std::optional<application::workbench_layout_delta> LayoutWidths;
		std::optional<core::workbench_navigation_cursor>   Cursor;
		std::optional<application::workbench_host_failure> Failure;
		workbench_toolbar_mode                             Mode                  = workbench_toolbar_mode::expanded;
		bool                                               HasCommands           = false;
		bool                                               ToolbarConfigured     = false;
		bool                                               ApplyingLayout        = false;
		bool                                               ApplyingFileSelection = false;
		// Set by the first published review-history update. Before one arrives
		// the shell has no review-history state to state, and quoting the
		// control's idle wording would report activity that never happened.
		bool                        HasEpochState = false;
		QString                     LastAnnouncement;
		workbench_window_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
