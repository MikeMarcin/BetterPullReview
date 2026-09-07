#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP

#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/account_card.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/defect_card_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/repositories_drawer.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <QList>
#include <QMainWindow>
#include <QSplitter>
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
class QStackedWidget;
class QToolBar;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	class diff_workbench;
	class epoch_comparison_control;
	// RFC-BPR-0012's two agent surfaces. Declared rather than included: the
	// window places them and hands them out, and nothing in this header needs
	// their shape, so a consumer of the shell does not acquire the finding
	// delivery and chat-scope vocabularies along with it.
	class finding_queue_view;
	class side_chat_panel;
	// RFC-BPR-0016's second way into a profile, declared for the same reason:
	// the window places it and hands it out, and a consumer of the shell does
	// not need the panel's shape to ask for it.
	class device_authorization_panel;

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
	/// \brief Mark read for the thread whose root projection identity is given ([[BPR-0656]]).
	using conversation_read_dispatch = std::function<void(const std::string&)>;

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

	/// \brief The central splitter: three drawers and the centre.
	///
	/// \details [[BPR-0662]]. A drawer's width reaches the host only from a
	/// drag of one of these handles: `QSplitter::splitterMoved` is emitted for
	/// nothing else. A width the splitter merely granted -- a hidden page laid
	/// out at its default size, a window narrower than the persisted widths
	/// -- is a squeeze, not a choice, and is never persisted. `move_handle` is
	/// the same call a handle drag ends in, so a course or a test drives the
	/// user's path rather than resizing a panel directly.
	class workbench_central_splitter final : public QSplitter
	{
	public:
		explicit workbench_central_splitter(QWidget* aParent = nullptr);

		/// \brief Moves handle `aIndex` so its left edge sits at `aPosition`.
		void move_handle(int aIndex, int aPosition);
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
		  public application::public_preview_projection_port,
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
		/// \brief Adopts one public-preview presentation.
		///
		/// \details The mode is named in all three places it is named — the
		/// host picker entry, the window title and the review header — from
		/// this one publication, so no two of them can disagree. It also
		/// decides what the mode cannot do: triage, queue role, viewed state,
		/// resolution and publishing are disabled with an explanation rather
		/// than hidden, and the inline review conversation is shown read-only
		/// when it was observed and named unobservable when it was not.
		void publish(
			const application::public_preview_presentation& aPresentation)
			override;
		/// \brief Adopts the instant public-preview text is measured from.
		///
		/// \details The window is given a now; it never reads a clock. A
		/// presentation that carries the host's own instant overrides this.
		void set_public_preview_now(std::uint64_t aNowUnixSeconds);
		void fail(
			const application::workbench_failure_publication& aFailure) override;
		/// \brief Adopts the selected file's core-decided private receipt actions.
		void publish(
			const application::file_receipt_action_projection& aProjection);
		/// \brief Adopts the host-decided overview of the open pull request.
		///
		/// \details [[BPR-0647]]. The Overview modality shows it in the centre
		/// beneath the epoch line, in place of the comparison; the other
		/// modalities keep the comparison. Its one action and its file rows
		/// enter Read at the named file through the core's file selection.
		void               publish(const application::pull_request_overview_projection& aProjection);
		void               clear_overview();
		[[nodiscard]] auto overview_page() const noexcept -> pull_request_overview_page*;
		/// \brief Whether the centre shows the overview rather than the comparison.
		[[nodiscard]] auto showing_overview() const -> bool;
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
		void set_review_submission_dispatch(review_submission_editor_dispatch aDispatch);
		void set_defect_dispatch(workbench_defect_dispatch aDefectDispatch);
		void set_agent_dispatch(agent_intent_dispatch aAgentDispatch);
		/// \brief Adopts the sink the mounted device-sign-in panel dispatches to.
		///
		/// \details On the same terms as `set_agent_dispatch`: the window
		/// mounts the panel because the review header is where sign-in lives,
		/// and supplies no sink of its own. Until the composition sets one,
		/// every gesture on the panel goes nowhere — which is exactly what the
		/// construction cut leaves behind.
		void set_device_authorization_dispatch(
			application::device_authorization_gesture_dispatch aDispatch);
		void set_defect_classification_dispatch(
			workbench_defect_classification_dispatch aClassificationDispatch);
		void set_queue_rule_dispatch(workbench_queue_rule_dispatch aDispatch);
		void set_queue_preferences_dispatch(
			workbench_queue_preferences_dispatch aDispatch);
		/// \brief Where a card's Mark read goes: the composition, then the ledger host.
		void set_conversation_read_dispatch(conversation_read_dispatch aDispatch);
		/// \brief Adopts the read state of the review's conversations.
		///
		/// \details [[BPR-0656]]. The cards draw it, the gutter marks a new
		/// thread's line blue, and the strip's "next new comment" group
		/// counts the unread threads and walks them in file order.
		void               present_read_state(std::vector<thread_read_state> aState);
		[[nodiscard]] auto next_new_comment_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_comment_count_label() const noexcept -> QLabel*;
		/// \brief Adopts the complete bounded local defect queue.
		void present_defects(std::vector<core::defect_history> aDefects);
		void clear_defects();

		[[nodiscard]] auto primary_toolbar() const noexcept -> QToolBar*;
		/// \brief The strip's modality control, alone at its centre.
		///
		/// \details [[BPR-0646]]. Overview, Read, Review and Discuss are
		/// presets for which drawers are open. Choosing one dispatches the
		/// core's exact set-drawer intents; the control shows the last chosen
		/// preset until the next, whatever a manual toggle did in between.
		[[nodiscard]] auto modality() const noexcept -> modality_control*;
		/// \brief Home's landing while nothing is open: sign in, or choose.
		[[nodiscard]] auto home_landing() const noexcept -> workbench_landing*;
		/// \brief The strip's Home button: Home on demand, checked while shown.
		[[nodiscard]] auto home_action() const noexcept -> QAction*;
		/// \brief Shows Home, or returns to the review when Home was asked for.
		void choose_home(bool aHome);
		/// \brief Home: the whole centre while nothing is open.
		///
		/// \details [[BPR-0646]] [[BPR-0650]]. Home carries the landing card and
		/// the review queue; the review workbench (drawers, epoch line, panes,
		/// review editor) is shown only once a pull request, a comparison or an
		/// active workbench exists. Nothing of the workbench makes sense before.
		[[nodiscard]] auto home_page() const noexcept -> QWidget*;
		[[nodiscard]] auto showing_home() const -> bool;
		[[nodiscard]] auto current_modality() const noexcept -> review_modality;
		/// \brief Chooses a modality exactly as the strip or its shortcut would.
		void choose_modality(review_modality aModality);
		/// \brief The strip's account button: sign-in while signed out,
		/// the accounts settings page once signed in.
		[[nodiscard]] auto account_action() const noexcept -> QAction*;
		/// \brief The repository and pull request identity at the strip's left.
		[[nodiscard]] auto identity_label() const noexcept -> QLabel*;
		/// \brief "change 3 of 11", derived from the comparison the surface holds.
		[[nodiscard]] auto change_position_label() const noexcept -> QLabel*;
		/// \brief "file 3 of 40", from the core's navigation cursor.
		[[nodiscard]] auto file_position_label() const noexcept -> QLabel*;
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
		[[nodiscard]] auto central_splitter() const noexcept
			-> workbench_central_splitter*;
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
		/// \brief The side chat, placed beside the agent launcher it belongs to.
		///
		/// \details BPR-0513. The panel is composed here because the drawer is
		/// where a reviewer can see it; the window supplies no dispatch, so
		/// until the application sets one every gesture on it goes nowhere.
		/// Placement adds nothing to what the panel holds — the aggregate its
		/// own tests bind by name is untouched.
		[[nodiscard]] auto side_chat() const noexcept -> side_chat_panel*;
		/// \brief The finding queue, placed above the draft the promotion fills.
		///
		/// \details BPR-0513, on the same terms as `side_chat()`. Its
		/// conversion actions are offered as buttons in this drawer, and the
		/// workshop those promotions reach is the one already inside
		/// `review_editor()`, one widget below it.
		[[nodiscard]] auto finding_queue() const noexcept -> finding_queue_view*;
		[[nodiscard]] auto review_submission_editor_view() const noexcept -> review_submission_editor*;
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
		/// \brief The device sign-in panel, mounted beside the token prompt.
		///
		/// \details [[BPR-0526]]. Sign-in has two ways in and they belong in
		/// one place: this sits directly under the session panel's host picker
		/// and Connect control, in the review header column, and is bound to
		/// whichever profile that picker currently names. The composition
		/// publishes device presentations straight to it and reads its gestures
		/// through `set_device_authorization_dispatch`.
		[[nodiscard]] auto device_authorization() const noexcept
			-> device_authorization_panel*;
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
		/// \brief The public-preview consumed-budget ring in the review header.
		[[nodiscard]] auto usage_indicator() const noexcept -> usage_ring*;
		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool;
		/// \brief The refusal wording currently shown, empty when none is.
		[[nodiscard]] auto public_preview_refusal_text() const -> QString;

		/// \brief The appearance surface, composed beside the review it governs.
		/// \brief Shows the Settings window on a page; a no-op before composition.
		void               open_settings(settings_page aPage);
		[[nodiscard]] auto settings() const noexcept -> settings_window*;
		[[nodiscard]] auto settings_action() const noexcept -> QAction*;
		[[nodiscard]] auto appearance_settings() const noexcept
			-> appearance_settings_panel*;
		[[nodiscard]] auto files_drawer_toggle_action() const noexcept -> QAction*;
		/// \brief [[BPR-0651]]. The repositories drawer, left of the files.
		[[nodiscard]] auto repositories_drawer() const noexcept -> workbench_drawer_panel*;
		[[nodiscard]] auto repositories_drawer_toggle_action() const noexcept -> QAction*;
		[[nodiscard]] auto repositories_tree() const noexcept -> repositories_tree_view*;
		/// \brief [[BPR-0652]]. Accounts & hosts' account card.
		[[nodiscard]] auto account_card_view() const noexcept -> account_card*;
		/// \brief [[BPR-0652]]. The observer-rule editor, on Reviews & queue.
		[[nodiscard]] auto rules_editor() const noexcept -> QWidget*;
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
		void resizeEvent(QResizeEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;

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
		/// \brief Disables what the signed-out mode cannot do, with wording.
		void apply_public_preview_availability();
		/// \brief Presents the observed inline conversation, or says it was not.
		void apply_public_preview_conversation();
		void update_public_preview_title();
		void focus_review_threads();
		void announce_github_presentation_update();
		void dispatch_navigation(std::size_t aIndex);
		void dispatch_drawer_toggle(std::size_t aIndex);
		void observe_drawer_width(
			core::workbench_drawer aDrawer,
			std::uint32_t          aWidth);
		void apply_drawer_layout();
		void apply_drawer_widths();
		void observe_handle_drag();
		void configure_tool_buttons();
		void refresh_toolbar_icons();
		void update_strip_positions();
		void update_strip_identity();
		void update_account_action();
		void apply_strip_width();
		void apply_review_details_visibility();
		void update_home_landing();
		/// \brief Shows the overview or the comparison for the current modality.
		void apply_modality_surface();
		/// \brief [[BPR-0648]]. Re-aligns the comment cards and the gutter marks.
		void refresh_cards();
		void apply_thread_marks();
		void update_new_comment_group();
		void step_to_next_new_comment();
		/// \brief Selects the file and enters Read, from the overview.
		void               enter_read_at(const QString& aPath);
		[[nodiscard]] auto file_for_path(const QString& aPath) const
			-> std::optional<core::workbench_file_id>;
		void               update_rail_summaries();
		void               infer_modality_from_drawers();
		[[nodiscard]] auto presented_files_state() const -> core::workbench_drawer_state;
		[[nodiscard]] auto presented_repositories_state() const -> core::workbench_drawer_state;
		void               apply_focus_order();
		void               update_action_state();
		void               build_menu_bar();
		void               dispatch_line_numbers_toggle();
		void               apply_line_numbers();
		void               update_drawer_action(
			std::size_t                  aIndex,
			core::workbench_command_id   aId,
			core::workbench_drawer_state aState);
		void update_status_text();
		void announce_update();
		void apply_file_rows(
			const std::vector<application::workbench_file_projection>& aFiles);
		[[nodiscard]] auto conversation_anchor_file() const -> std::optional<core::file_identity>;
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
		/// \brief Points device sign-in at the host profile the picker names.
		void rebind_device_authorization_profile();
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
		review_submission_editor_dispatch        ReviewSubmissionDispatch;
		review_submission_editor*                ReviewSubmissionEditor = nullptr;
		workbench_defect_dispatch                DefectDispatch;
		workbench_defect_classification_dispatch DefectClassificationDispatch;
		workbench_queue_rule_dispatch            QueueRuleDispatch;
		workbench_queue_preferences_dispatch     QueuePreferencesDispatch;

		QToolBar*                   PrimaryToolbar       = nullptr;
		workbench_strip*            Strip                = nullptr;
		modality_control*           Modality             = nullptr;
		QAction*                    AccountAction        = nullptr;
		QLabel*                     IdentityLabel        = nullptr;
		QLabel*                     ChangePositionLabel  = nullptr;
		QLabel*                     FilePositionLabel    = nullptr;
		QWidget*                    ModalitySpacer       = nullptr;
		QWidget*                    StripInset           = nullptr;
		QScrollArea*                DrawerTopScroll      = nullptr;
		workbench_landing*          HomeLanding          = nullptr;
		QWidget*                    HomePage             = nullptr;
		bool                        StripHome            = false;
		bool                        HomeRequested        = false;
		QAction*                    HomeAction           = nullptr;
		QVBoxLayout*                HomeColumn           = nullptr;
		QVBoxLayout*                DrawerTopColumn      = nullptr;
		QStackedWidget*             CentralStack         = nullptr;
		bool                        ModalityChosen       = false;
		std::size_t                 ReviewThreadTotal    = 0U;
		QAction*                    IdentityAction       = nullptr;
		QAction*                    ChangePositionAction = nullptr;
		QAction*                    FilePositionAction   = nullptr;
		QString                     IdentityText;
		QMenuBar*                   PrimaryMenuBar       = nullptr;
		QMenu*                      ViewMenu             = nullptr;
		QAction*                    LineNumbersAction    = nullptr;
		QAction*                    ReviewDetailsAction  = nullptr;
		QToolButton*                ReviewControlsButton = nullptr;
		QMenu*                      ReviewControlsMenu   = nullptr;
		QWidget*                    ToolbarSpacer        = nullptr;
		QLabel*                     StatusLabel          = nullptr;
		workbench_central_splitter* CentralSplitter      = nullptr;
		QWidget*                    CentreSurface        = nullptr;
		diff_workbench*             ComparisonSurface    = nullptr;
		pull_request_overview_page* OverviewPage         = nullptr;
		QStackedWidget*             CentreStack          = nullptr;
		QWidget*                    ComparisonPage       = nullptr;

		epoch_comparison_control* EpochSelectors = nullptr;

		std::array<navigation_entry, 4> Navigation{ };
		QAction*                        NextNewCommentAction  = nullptr;
		QLabel*                         NewCommentCountLabel  = nullptr;
		QAction*                        NewCommentCountAction = nullptr;
		conversation_read_dispatch      ConversationReadDispatch;
		std::array<QAction*, 4>         ReceiptActions{ };
		std::array<drawer_entry, 3>     Drawers{ };
		repositories_tree_view*         RepositoriesTree = nullptr;
		account_card*                   AccountCard      = nullptr;
		QWidget*                        RulesEditor      = nullptr;
		/// Set when a pull request was chosen from the repositories tree, so
		/// it opens on its overview when it arrives.
		bool                       OpenOnOverview    = false;
		github_conversation_panel* ConversationPanel = nullptr;
		/// Prevents the drawer refilter from clearing the anchor that triggered it.
		bool                                                       ApplyingConversationAnchor       = false;
		bool                                                       LeftConversationEndpointCurrent  = false;
		bool                                                       RightConversationEndpointCurrent = false;
		agent_launch_panel*                                        AgentPanel                       = nullptr;
		side_chat_panel*                                           SideChat                         = nullptr;
		finding_queue_view*                                        FindingQueue                     = nullptr;
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
		github_session_panel*                                      SessionPanel = nullptr;
		device_authorization_panel*                                DevicePanel  = nullptr;
		application::device_authorization_gesture_dispatch         DeviceDispatch;
		github_pull_request_panel*                                 PullRequestPanel     = nullptr;
		appearance_settings_panel*                                 AppearanceSettings   = nullptr;
		settings_window*                                           SettingsWindow       = nullptr;
		QAction*                                                   SettingsAction       = nullptr;
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
		application::public_preview_presentation                   PublicPreview;
		std::uint64_t                                              PublicPreviewNow         = 0;
		bool                                                       PublicPreviewSignedOut   = false;
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
