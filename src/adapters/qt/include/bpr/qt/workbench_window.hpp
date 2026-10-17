#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP

#include "bpr/application/profile_management_projection.hpp"
#include "bpr/application/setup_destination_projection.hpp"

#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/core/reference_page.hpp"
#include "bpr/core/release_maintenance.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/claude_wrapper_panel.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/defect_card_view.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_refusal.hpp"
#include "bpr/qt/home_status_line.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/repositories_drawer.hpp"
#include "bpr/qt/resident_filter.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/source_search_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#if defined(_WIN32)
	#include "bpr/qt/windows_caption_buttons.hpp"
#endif

#include <QAction>
#include <QColor>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QSplitter>
class QTabWidget;
#include <QString>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

class QCheckBox;
class QComboBox;
class QEvent;
class QElapsedTimer;
class QHBoxLayout;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QDialog;
class QListView;
class QListWidget;
class QMessageBox;
class QMenu;
class QMenuBar;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class QTimer;
class QToolBar;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	class destructive_question;
	/// \brief [[BPR-1336]]. Whether an editable text field keeps a typed
	/// Control+Alt character from the window's shortcuts.
	///
	/// \details Windows reports AltGr as Control+Alt, so on a German or Nordic
	/// layout AltGr+0 types `}` and would also match a window shortcut on
	/// Ctrl+Alt+0, as Actual Size's was until [[BPR-1535]] moved it to Ctrl+0.
	/// A field that would type the character claims the key. A key that types
	/// nothing, or a read-only surface such as the code panes, leaves it to the
	/// window.
	[[nodiscard]] auto editor_keeps_typed_text(const QObject* aReceiver, const QKeyEvent& aEvent) -> bool;

	class review_tour_editor;
	enum class dif_clip_id : std::size_t;
	class dif_catalog_provider;
	class feedback_mark_preview;
	class feedback_label_columns_editor;
	class dif_cue_router;
	class dif_widget;
	class home_list;
	class keyboard_shortcuts_window;
	class home_panel;
	class diff_workbench;
	class epoch_comparison_control;
	// RFC-BPR-0012's two agent surfaces. Declared rather than included: the
	// window places them and hands them out, and nothing in this header needs
	// their shape, so a consumer of the shell does not acquire the finding
	// delivery and chat-scope vocabularies along with it.
	class finding_queue_view;
	class side_chat_panel;
	class private_review_panel;
	class private_notes_panel;
	// RFC-BPR-0016's second way into a profile, declared for the same reason:
	// the window places it and hands it out, and a consumer of the shell does
	// not need the panel's shape to ask for it.
	class device_authorization_panel;
	class review_cycle_view;

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
	/// [[BPR-0657]]. A card's Reply: the thread identity the next draft answers.
	using workbench_review_reply_dispatch = std::function<void(const std::string&)>;
	/// [[BPR-0657]]. A card's Resolve or Unresolve, with what the window knows
	/// of the thread and the server so the composition can decide it.
	struct thread_resolution_gesture
	{
		std::string                   Identity;
		bool                          Resolve    = true;
		bool                          Resolved   = false;
		core::github_capability_state Capability = core::github_capability_state::unknown;

		auto operator<=>(const thread_resolution_gesture&) const = default;
	};
	using workbench_thread_resolution_dispatch = std::function<void(const thread_resolution_gesture&)>;
	using workbench_defect_dispatch            = std::function<void(core::defect_command)>;

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
	/// \brief Save or forget a repository ([[BPR-0659]]).
	using workbench_saved_repository_dispatch = std::function<void(application::saved_repository_intent)>;
	/// \brief [[BPR-0990]]. The local Follow or Unfollow of the open pull request.
	using workbench_queue_follow_dispatch = std::function<void(application::queue_follow_intent)>;
	/// \brief [[BPR-0990]]. Home's activation and its explicit Refresh.
	using workbench_queue_discovery_dispatch = std::function<void(application::queue_discovery_intent)>;
	/// \brief [[BPR-0660]]. One remember or forget gesture for an enterprise server.
	using workbench_saved_server_dispatch = std::function<void(application::saved_server_intent)>;
	/// \brief Presentation state for the explicit GitHub CLI account inventory.
	enum class github_cli_discovery_state : std::uint8_t {
		idle,
		discovering,
		ready,
		empty,
		unsupported_version,
		process_failed,
		malformed_response,
		ineligible_accounts,
		missing_executable,
		launch_failed,
		timed_out,
		exited_unsuccessfully,
		output_rejected,
	};
	using github_cli_discovery_dispatch = std::function<void()>;
	/// [[BPR-0661]]. The review-behaviour preferences the page dispatches.
	using review_behaviour_dispatch = std::function<void(const application::review_behaviour_settings&)>;

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

	protected:
		[[nodiscard]] auto createHandle() -> QSplitterHandle* override;
	};

	/// \brief A central splitter handle that shows where it is.
	///
	/// \details [[BPR-0739]]. The platform style paints the handle in the
	/// window colour, so between the comparison and a drawer the reviewer saw
	/// nothing to aim at and read the comparison's scroll bar as the border;
	/// the resize cursor showed only while the pointer crossed the seven
	/// pixels. The handle draws the drawer's edge rule from the design asset:
	/// a one pixel `Mid` line, the rule the collapsed rail draws, at one
	/// constant weight. [[BPR-0749]] centres that rule within the native hot
	/// zone so aiming at it leaves space on both sides: column 3 at macOS's
	/// width 7, column 1 at Fusion's width 4 (the left middle column for even
	/// widths). The hot zone retains the style's width; the rule stays one pixel.
	///
	/// It brightened to `Highlight` under the pointer until 2026-09-12, when
	/// the owner chose the asset's constant weight in the v009 attended
	/// session: "Drop it: draw the rule at one constant weight, as the asset
	/// does. We don't need it anymore now that the cursor is fixed, it wasn't
	/// drawn in the right location anyways." The brightening had been added on
	/// the theory that the hot zone was too narrow to find; his own v008
	/// screenshot refuted that theory, which is [[BPR-0746]].
	///
	/// The two-state pen was sound while it lasted, and `qt_workbench_window`
	/// keeps the measurement that says so: `Qt::WA_Hover` is set on the handle
	/// and the pointer's arrival and departure each repaint it, so a rule drawn
	/// from `underMouse()` witnessed where the pointer was. BPR-0746 rests on
	/// that, because it admits a screenshot of a lit rule as proof the handle
	/// held the pointer while the cursor was wrong.
	class workbench_splitter_handle final : public QSplitterHandle
	{
	public:
		workbench_splitter_handle(Qt::Orientation aOrientation, QSplitter* aParent);

		/// \brief The column the rule is drawn in, in the handle's coordinates.
		[[nodiscard]] auto rule_edge() const -> int;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief [[BPR-1580]]. The submenus of Home's View menu, top to bottom.
	enum class home_view_choice : std::uint8_t {
		group,
		sort,
		role,
		added_by,
		quiet,
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
			QWidget*                                        aParent         = nullptr,
			std::shared_ptr<dif_catalog_provider>           aDifCatalog     = { });
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
		/// modalities keep the comparison. Start enters Review, Continue follows
		/// its preference, and file rows enter Read at the core-selected file.
		void publish(const application::pull_request_overview_projection& aProjection);
		/// \brief [[BPR-0972]]. Adopts receipt changes to the presented overview.
		///
		/// \details Only the changed file rows, the progress and the one action
		/// move; the page's table and the drawer tree signal those rows alone.
		/// Without a presented overview there is nothing to change.
		void publish(const application::overview_receipt_update& aUpdate);
		void publish(const application::overview_history_update& aUpdate);
		/// \brief [[BPR-1089]]. Adopts the host's label counts for the
		/// Overview's request: its chips, mode and sort.
		void publish(const application::overview_feedback_projection& aFeedback);
		/// \brief [[BPR-1089]]. Where the Overview's request goes: whenever its
		/// chips (from Settings), its mode or its sort change.
		void set_overview_feedback_dispatch(std::function<void(const application::overview_feedback_request&)> aDispatch);
		/// \brief [[BPR-1089]]. Where an Overview count's activation goes.
		void set_overview_drill_down_dispatch(std::function<void(const application::overview_drill_down&)> aDispatch);
		/// \brief [[BPR-1089]]. Lists exactly the conversations one Overview
		/// count counted, in Discuss. With \p aReveal, Discuss is shown and
		/// takes focus; without it (a refresh of the same count) the list
		/// changes in place and focus stays where it is.
		void list_conversations(const application::overview_drill_down_listing& aListing, bool aReveal);
		/// \brief Whether Discuss still lists an Overview count's conversations.
		[[nodiscard]] auto conversation_listing_active() const -> bool;
		/// \brief [[BPR-0733]]. Adopts the window's recently viewed pull requests.
		///
		/// \details Home lists them between the open bar and the review queue,
		/// in the order they were given, most recent first. The composition has
		/// already filtered the list for the session this window has, so the
		/// section follows what it was given rather than the account: a
		/// signed-out Home lists whatever public entries it was handed.
		/// Publishing never moves keyboard focus.
		void publish(const application::recent_reviews_projection& aProjection);
		/// \brief [[BPR-0733]]. Where a recently viewed row's open or resume goes.
		void set_recent_review_dispatch(
			std::function<void(application::recent_review_intent)> aDispatch);
		/// \brief Where a recently viewed row's Remove goes.
		void               set_recent_removal_dispatch(std::function<void(application::recent_review_row)> aDispatch);
		void               clear_overview();
		[[nodiscard]] auto overview_page() const noexcept -> pull_request_overview_page*;
		/// \brief Whether the centre shows the overview rather than the comparison.
		[[nodiscard]] auto showing_overview() const -> bool;
		/// \brief Presents one typed receipt refusal without changing the model.
		void fail(const application::receipt_failure_publication& aFailure);
		/// \brief Retires receipt actions, optionally naming unavailable storage.
		void clear_review_ledger(bool aUnavailable = false);
		/// \brief [[BPR-0742]]. Adopts whether this window's saved layout is unavailable.
		///
		/// \details Composition decides it once, from the one store open it does
		/// not keep. The status line then carries the settings-unavailable sentence
		/// for as long as it is set; unlike a host failure, a later workbench
		/// projection does not clear it.
		void set_saved_layout_unavailable(bool aUnavailable);
		/// \brief [[BPR-0817]]. Adopts whether this profile's disposable agent
		/// workspaces are unavailable because startup recovery refused them.
		///
		/// \details Retained like the saved-layout sentence. The refusal is decided
		/// once, at startup, and holds while the profile runs, so the status line
		/// says it on Home and on every other surface. RFC-BPR-0033's
		/// owner-delegated amendment routes it here, as B3 routes a profile failure.
		void set_agent_workspaces_unavailable(bool aUnavailable);

		void add_setup_profile(application::github_profile_option aProfile);
		void set_setup_destination_dispatch(application::setup_destination_dispatch aDispatch);
		void present_setup_destination(const application::setup_destination_projection& aProjection);
		/// \brief [[BPR-0987]]. Says, beside the Home bar and in the status bar,
		/// why a pasted link's location was not shown.
		void present_link_location_notice(const QString& aText);
		/// \brief [[BPR-0987]]. Reveals one conversation of the open pull request
		/// in Discuss, as a press on its row would; false when the window does
		/// not list it. [[BPR-1293]]: a conversation on a file opens in Review
		/// instead, in place at the named post, as a pick in Threads does.
		[[nodiscard]] auto reveal_conversation(const std::string& aIdentity) -> bool;
		/// \brief [[BPR-1293]]. Opens a conversation on a file in Review: the
		/// file through the files drawer's intent, Review in the arrangement
		/// it last had, the Comments drawer if it is closed (a manual open),
		/// and the thread in place at \p aPost, once the file's comparison
		/// shows it. False when the window has no such conversation.
		auto open_conversation_in_review(const std::string& aThread, const std::string& aPost) -> bool;
		/// \brief [[BPR-1293]]. Opens a conversation on a file in Review, named
		/// by its root or any of its posts; false for one that is not on a file.
		auto reveal_conversation_in_review(const std::string& aIdentity) -> bool;
		/// \brief [[BPR-1356]]. Opens a thread picked in Threads where the core
		/// routes it: the pick's destination, the thread's facts and the
		/// filters Discuss would hide it with go to
		/// `core::route_workbench_thread`, and the window performs the route.
		/// False when the window has no such thread.
		auto open_thread_from_threads(const std::string& aThread, const std::string& aPost, core::workbench_thread_destination aDestination) -> bool;
		/// \brief [[BPR-1293]]. Threads, the pull request's finder page.
		[[nodiscard]] auto finder() const noexcept -> conversation_finder*;
		/// \brief [[BPR-1293]]. Command-J (Ctrl+J): goes to Threads, and
		/// from it back to the modality it was entered from.
		[[nodiscard]] auto show_threads_action() const noexcept -> QAction*;
		/// \brief [[BPR-1293]]. [[BPR-1087]]'s conversations per label, for
		/// the finder's label filter.
		void set_conversation_labels(std::optional<application::conversation_labels_projection> aLabels);

		/// \brief Routes profile gestures and desktop management/quit to their composition owner.
		void set_profile_management_dispatch(application::profile_management_dispatch aDispatch,
											 std::function<void()> aManage, std::function<void()> aQuit = { });
		/// \brief Updates native menu rows by profile ID and marks this window's profile.
		void               adopt_profile_management(const application::profile_management_projection& aProjection,
													core::bpr_profile_id                              aCurrentProfile);
		[[nodiscard]] auto profiles_menu() const noexcept -> QMenu*;

		void set_dispatch(workbench_intent_dispatch aDispatch);
		/// \brief Window PR Search commands; the composition owns query effects and exact opens.
		void               set_pr_search_dispatch(std::function<void(source_find_gesture)> aDispatch);
		[[nodiscard]] auto pr_search() const -> source_search_panel*;
		void               set_change_dispatch(workbench_change_dispatch aChangeDispatch);
		void               set_width_dispatch(workbench_drawer_width_dispatch aWidthDispatch);
		void               set_receipt_dispatch(workbench_receipt_dispatch aReceiptDispatch);
		void               set_review_draft_dispatch(
			workbench_review_draft_dispatch aDraftDispatch);
		void set_review_submission_dispatch(review_submission_editor_dispatch aDispatch);
		/// \brief [[BPR-1345]]. Where Start a review and Add review comment go:
		/// the composition keeps the shown draft pending in the review and
		/// answers whether it took it. Nothing is published.
		void set_start_review_dispatch(std::function<bool()> aDispatch);
		/// \brief [[BPR-1374]]. What a kept reply's note asks of the
		/// composition: keep its words as a new comment on its line, or, once
		/// the window has asked, discard it. Nothing is published.
		enum class unsent_reply_action : std::uint8_t {
			keep_as_comment,
			discard,
		};
		void set_unsent_reply_dispatch(std::function<void(core::review_draft_id, unsent_reply_action)> aDispatch);
		/// \brief [[BPR-1345]] v002. Where a confirmed Delete of a pending
		/// comment on Finish your review goes: the composition deletes that
		/// draft at the revision the page listed. Nothing is published.
		void set_comment_draft_unavailable(QString aReason);
		void set_pending_comment_delete_dispatch(std::function<void(core::review_draft_id, std::uint64_t)> aDispatch);
		/// \brief [[BPR-0657]]. Adopts the sink a card's Reply names its thread to.
		void set_review_reply_dispatch(workbench_review_reply_dispatch aDispatch);
		/// \brief [[BPR-0657]]. Adopts the sink a card's Resolve or Unresolve goes to.
		void set_thread_resolution_dispatch(workbench_thread_resolution_dispatch aDispatch);
		/// \brief [[BPR-0657]]. Whether the composed provider resolves threads; with
		/// the server's capability this is what offers Resolve on the cards.
		void set_thread_resolution_provider(bool aResolves);
		void set_defect_dispatch(workbench_defect_dispatch aDefectDispatch);
		void set_agent_dispatch(agent_intent_dispatch aAgentDispatch);
		/// \brief [[BPR-1179]]. Takes every agent control off this window for its life.
		///
		/// \details For a composition built without agent integrations, which
		/// has nothing to answer them with. The launch panel, side chat and
		/// finding queue and their buttons, the comment workshop, tour writing
		/// assistance, the private Ask agent and agent-assisted composition,
		/// and the Agents & context settings page are withdrawn; private notes,
		/// private history and every stored record stay. Presentation only:
		/// the window starts nothing either way.
		void withdraw_agent_surfaces();
		/// \brief Adopts the sink the mounted device-sign-in panel dispatches to.
		///
		/// \details On the same terms as `set_agent_dispatch`: the window
		/// mounts the panel because the review header is where sign-in lives,
		/// and supplies no sink of its own. Until the composition sets one,
		/// every gesture on the panel goes nowhere — which is exactly what the
		/// construction cut leaves behind.
		void set_device_authorization_dispatch(
			application::device_authorization_gesture_dispatch aDispatch);
		/// \brief [[BPR-0771]]. Where the public-preview ring's sign-in gesture
		/// leaves, from whichever card carries the ring.
		void set_public_preview_dispatch(
			application::public_preview_gesture_dispatch aDispatch);
		void set_defect_classification_dispatch(
			workbench_defect_classification_dispatch aClassificationDispatch);
		void set_queue_rule_dispatch(workbench_queue_rule_dispatch aDispatch);
		void set_queue_preferences_dispatch(
			workbench_queue_preferences_dispatch aDispatch);
		/// \brief Where Save and Forget go, from the tree and from Settings.
		void set_saved_repository_dispatch(workbench_saved_repository_dispatch aDispatch);
		/// \brief The final Add gesture, correlated with its displayed accepted session.
		void set_repository_addition_dispatch(std::function<void(application::repository_addition_intent)> aDispatch);
		/// \brief Opens the shared Home/Settings draft; effects remain host-owned.
		void open_repository_addition();
		/// \brief Settings › Repositories' new-repository card; shown while a
		/// repository is being added. There is no dialog (RFC-BPR-0062, owner
		/// verdict 2026-10-08).
		[[nodiscard]] auto repository_addition_draft() const noexcept -> QWidget*;
		/// \brief [[BPR-0990]]. Where the Pull Request menu's Follow goes.
		void set_queue_follow_dispatch(workbench_queue_follow_dispatch aDispatch);
		/// \brief [[BPR-0990]]. Where Home's refresh glyph and Home's
		/// activation go: the profile's discovery, never a provider.
		void set_queue_discovery_dispatch(workbench_queue_discovery_dispatch aDispatch);
		/// \brief [[BPR-1407]]. The host's clock, read when the queue is
		/// projected, that a Home row's age is measured to. The window reads no
		/// clock of its own; without one no age is drawn.
		void set_home_clock(std::function<std::optional<std::int64_t>()> aClock);
		/// \brief [[BPR-0660]], [[BPR-0774]]. Receives the enterprise servers the
		/// reviewer keeps: Add enterprise server, a card's Remember switch and
		/// its confirmed Forget server.
		void set_saved_server_dispatch(workbench_saved_server_dispatch aDispatch);
		/// \brief Adopts the explicit `Use GitHub CLI` gesture; absent means hidden.
		void set_github_cli_discovery_dispatch(github_cli_discovery_dispatch aDispatch);
		/// \brief [[BPR-1373]]. Where a card's Check gh again goes: the same
		/// inventory, without opening sign-in or connecting.
		void set_github_cli_check_dispatch(github_cli_discovery_dispatch aDispatch);
		/// \brief [[BPR-1373]]. Every gh account by hostname, from the last check.
		void present_cli_accounts(std::map<std::string, std::vector<server_card_cli_account>, std::less<>> aAccounts);
		/// \brief Presents the bounded result of an explicitly requested CLI inventory.
		void               present_github_cli_discovery(github_cli_discovery_state aState, std::size_t aAccounts = 0U);
		[[nodiscard]] auto use_github_cli_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto github_cli_discovery_status() const noexcept -> QLabel*;
		/// \brief [[BPR-0774]]. The bordered button under the server cards, shown
		/// while the queue that keeps servers is ready.
		[[nodiscard]] auto add_enterprise_server_button() const noexcept -> QPushButton*;
		/// \brief The card in its edit state that Add enterprise server opens.
		[[nodiscard]] auto enterprise_server_draft() const noexcept -> QWidget*;
		[[nodiscard]] auto enterprise_server_hostname() const noexcept -> QLineEdit*;
		[[nodiscard]] auto enterprise_server_account() const noexcept -> QLineEdit*;
		[[nodiscard]] auto enterprise_server_add() const noexcept -> QPushButton*;
		[[nodiscard]] auto enterprise_server_cancel() const noexcept -> QPushButton*;
		/// \brief The sentence beside a refused Add; hidden while nothing is refused.
		[[nodiscard]] auto enterprise_server_refusal() const noexcept -> QLabel*;
		/// \brief The open Forget server confirmation; null while none is open.
		[[nodiscard]] auto forget_server_confirmation() const noexcept -> QMessageBox*;
		/// \brief The records the review queue last published as kept, exactly.
		[[nodiscard]] auto kept_servers() const noexcept -> const std::vector<application::saved_server>&;
		/// \brief Settings › Repositories: the saved repositories of the displayed
		/// account and host, Add repository and Remove (RFC-BPR-0062, owner
		/// direction 2026-10-07).
		[[nodiscard]] auto saved_repositories_editor() const noexcept -> QWidget*;
		/// \brief The page's list: one row per saved repository of the displayed
		/// account and host, in the order the queue host keeps them.
		[[nodiscard]] auto saved_repository_list() const noexcept -> QListWidget*;
		/// \brief The page's empty state, shown instead of the list.
		[[nodiscard]] auto saved_repositories_empty() const noexcept -> QWidget*;
		[[nodiscard]] auto saved_repository_name() const noexcept -> QLineEdit*;
		[[nodiscard]] auto add_saved_repository_action() const noexcept -> QAction*;
		[[nodiscard]] auto remove_saved_repository_action() const noexcept -> QAction*;
		/// \brief Home's first-repository call to action: shown signed in, with the
		/// queue ready and no saved repository for the displayed account and host.
		[[nodiscard]] auto home_repository_onboarding() const noexcept -> QWidget*;
		[[nodiscard]] auto home_repository_onboarding_button() const noexcept -> QPushButton*;
		/// \brief Where a card's Mark read goes: the composition, then the ledger host.
		void set_conversation_read_dispatch(conversation_read_dispatch aDispatch);
		/// \brief [[BPR-0661]]. Adopts the sink the Reviews & queue preference rows dispatch to.
		void set_behaviour_dispatch(review_behaviour_dispatch aDispatch);
		/// \brief The review-behaviour preferences the host last published.
		[[nodiscard]] auto behaviour() const noexcept -> const application::review_behaviour_settings&;
		/// \brief The Reviews & queue preference editor ([[BPR-0661]]).
		[[nodiscard]] auto behaviour_editor() const noexcept -> QWidget*;
		/// \brief [[BPR-1083]]. The marks beside Comment labels › Label marks.
		[[nodiscard]] auto feedback_marks_preview() const noexcept -> feedback_mark_preview*;
		/// \brief [[BPR-1088]]. Comment labels › Label helper: since
		/// [[BPR-1565]] only whether comment workshop requests carry the
		/// Review feedback guidance ([[BPR-1091]]).
		[[nodiscard]] auto label_helper_switch() const noexcept -> QCheckBox*;
		/// \brief [[BPR-1088]]. Comment labels › Overview chips.
		[[nodiscard]] auto label_columns_editor() const noexcept -> feedback_label_columns_editor*;
		/// \brief [[BPR-1088]]. What the Overview chips row says when the stored
		/// list named labels it left out; hidden otherwise.
		[[nodiscard]] auto label_columns_recovery() const noexcept -> QLabel*;
		/// \brief [[BPR-1088]]. Comment labels › Mark colours, which opens
		/// Appearance with its colour filter set to "label".
		[[nodiscard]] auto mark_colours_link() const noexcept -> QToolButton*;
		/// \brief [[BPR-1566]]. Comment labels' first line: the sentence that
		/// says where the labels come from, and About Conventional Comments,
		/// which asks the application to open that page in the browser.
		[[nodiscard]] auto conventional_comments_caption() const noexcept -> QLabel*;
		[[nodiscard]] auto conventional_comments_link() const noexcept -> QToolButton*;
		/// \brief [[BPR-1566]]. Why the page did not open, under the link;
		/// hidden after an open and before any.
		[[nodiscard]] auto conventional_comments_status() const noexcept -> QLabel*;
		/// \brief Adopts the read state of the review's conversations.
		///
		/// \details [[BPR-0656]]. The cards draw it, the gutter marks a new
		/// thread's line blue, and "next new comment" walks the unread threads
		/// in file order. [[BPR-1129]]: the title row's comments toggle carries
		/// the selected file's count. Next new comment remains review-wide.
		void present_read_state(std::vector<thread_read_state> aState);
		/// \brief [[BPR-1326]]. My unpublished drafts as they are saved. Each
		/// on the shown comparison is a dashed note at its line with its
		/// gutter marker and scroll-bar tick, whichever line is selected; the
		/// selected line's draft is drawn as the editor holds it.
		void present_saved_drafts(std::vector<core::review_comment_draft> aDrafts);
		/// \brief [[BPR-1374]]. As above, with where each unsent reply stands
		/// against the server's threads, as the core decided: on its thread's
		/// note, or kept as a note of its own when its thread is gone.
		void present_saved_drafts(std::vector<core::review_comment_draft> aDrafts, std::vector<core::reply_draft_decision> aReplies);
		/// \brief [[BPR-0972]]. Adopts one thread's read state; empty means read.
		///
		/// \details The cards, the map and the gutter change for that thread
		/// alone; the unread-thread count follows without walking the review.
		void update_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts);
		/// \brief Next new comment: window-owned, with its key, and no presenter
		/// in the title row ([[BPR-1129]]).
		[[nodiscard]] auto next_new_comment_action() const noexcept -> QAction*;
		/// \brief Adopts the complete bounded local defect queue.
		void present_defects(std::vector<core::defect_history> aDefects, std::map<core::defect_id, std::string> aThreads = { });
		void clear_defects();

		/// \brief The title row: the window, the pull request and the modality.
		[[nodiscard]] auto primary_toolbar() const noexcept -> QToolBar*;
		/// \brief [[BPR-1129]]. The review bar under the title row.
		///
		/// \details RFC-BPR-0052's bar, full width over the review surface: from
		/// its left, Updates and the comparison with its dates and menu, the
		/// change and file navigation with their positions and private
		/// progress; at its right Compose review and Comment. Home has none.
		[[nodiscard]] auto review_bar() const noexcept -> QToolBar*;
		/// \brief [[BPR-1134]]. The right tray's tabs: Comments, then Notes.
		[[nodiscard]] auto tray_tabs() const noexcept -> QTabBar*;
		/// \brief [[BPR-1134]]. Each tab's own scrolled column.
		[[nodiscard]] auto comments_tab_scroll() const noexcept -> QScrollArea*;
		[[nodiscard]] auto findings_tab_scroll() const noexcept -> QScrollArea*;
		/// \brief [[BPR-1134]]. Next new comment, the Comments tab's tool.
		[[nodiscard]] auto next_new_comment_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1134]]. The count the Comments tab carries; empty for none.
		[[nodiscard]] auto comments_count_text() const -> QString;
		/// \brief [[BPR-1130]]. The bar's Updates disclosure, before the
		/// comparison: it folds the Updates line's timeline and opens it.
		[[nodiscard]] auto updates_disclosure_button() const noexcept -> updates_disclosure*;
		/// \brief [[BPR-1130]]. The fold's command, in the View menu and on the
		/// disclosure; checked while the line is open.
		[[nodiscard]] auto updates_line_action() const noexcept -> QAction*;
		/// \brief [[BPR-1130]]. Whether the Updates line's timeline is open.
		[[nodiscard]] auto updates_line_open() const noexcept -> bool;
		/// \brief [[BPR-1129]]. Compose review: Option-R (Alt+R), window-owned,
		/// marked while a review draft is kept.
		[[nodiscard]] auto compose_review_action() const noexcept -> QAction*;
		/// \brief [[BPR-1345]]. The review bar's "Review · N pending" chip
		/// with Finish review; hidden with no review in progress.
		[[nodiscard]] auto review_chip() const noexcept -> QToolButton*;
		[[nodiscard]] auto review_chip_action() const noexcept -> QAction*;
		/// \brief [[BPR-1345]] v002. Finish review: ⌘⌥↩ (Ctrl+Alt+Return),
		/// window-owned, enabled while a review is in progress and not being
		/// submitted; it opens the review's summary in Discuss.
		[[nodiscard]] auto finish_review_action() const noexcept -> QAction*;
		/// \brief [[BPR-1345]] v002. The drawer draft's Delete question while
		/// it is open, or null.
		[[nodiscard]] auto delete_draft_confirmation() const noexcept -> destructive_question*;
		/// \brief [[BPR-1345]] v002. Goes to a pending comment from Finish
		/// your review: Review, its file, and its line selected once that
		/// file's comparison shows. Navigation only.
		void reveal_pending_comment(const QString& aPath, core::diff_side aSide, std::uint32_t aLine);
		/// \brief [[BPR-1345]]. Where the shown draft stands with the review.
		[[nodiscard]] auto draft_review() const -> draft_review_state;
		[[nodiscard]] auto prepare_tour_action() const noexcept -> QAction*;
		[[nodiscard]] auto add_tour_stop_action() const noexcept -> QAction*;
		[[nodiscard]] auto tour_editor() const noexcept -> review_tour_editor*;
		void               open_review_tour();
		/// \brief [[BPR-1129]]. Comment: Option-C (Alt+C), window-owned.
		///
		/// \details [[BPR-1133]]. It answers the selected review thread when
		/// there is one, and comments on the current line otherwise; its text
		/// and tooltip name that target ("Comment on line 431…", "Reply to
		/// tbutler's thread…").
		[[nodiscard]] auto comment_action() const noexcept -> QAction*;
		/// \brief [[BPR-1133]]. Comment's arrow menu: the target-named command,
		/// where RFC-BPR-0048's labels will be offered.
		[[nodiscard]] auto comment_menu() const noexcept -> QMenu*;
		/// \brief [[BPR-1132]]. Shows the window's document tabs, as the host
		/// published them, in the comparison's header.
		void publish(const application::document_tabs_projection& aProjection);
		/// \brief [[BPR-1132]]. Where every tab gesture goes: the strip, the
		/// Files tree's Cmd-click and Open in New Tab, the tab keys and the
		/// older-pair notice's Show them.
		void set_document_tab_dispatch(document_tab_dispatch aDispatch);
		/// \brief [[BPR-1132]]. The comparison's header: the document tabs.
		[[nodiscard]] auto document_tabs() const noexcept -> document_tab_strip*;
		/// \brief [[BPR-1132]]. Ctrl+Tab and Ctrl+Shift+Tab (the Control key on
		/// macOS too), window-owned.
		[[nodiscard]] auto next_tab_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_tab_action() const noexcept -> QAction*;
		/// \brief [[BPR-1132]]. Cmd-W (Ctrl+W): closes the active tab.
		[[nodiscard]] auto close_tab_action() const noexcept -> QAction*;
		/// \brief [[BPR-1132]]. The Comments tab's notice for a tab over an
		/// older pair: how many threads are on later updates, and Show them.
		[[nodiscard]] auto older_pair_notice() const noexcept -> QWidget*;
		[[nodiscard]] auto older_pair_notice_text() const -> QString;
		[[nodiscard]] auto show_later_threads_button() const noexcept -> QPushButton*;
#if defined(_WIN32)
		/// \brief Which part of the Windows title row a window point falls on.
		///
		/// \details [[BPR-0653]]. The strip is the title row on Windows. Its bare
		/// surface is the caption, the three caption buttons are their own parts,
		/// and every control is client. Presentation answers; the host turns the
		/// answer into the platform's hit-test code.
		[[nodiscard]] auto title_row_part_at(QPoint aWindowPoint) const -> title_row_part;
		/// \brief Minimise, maximise-or-restore and close, the strip's last three stops.
		[[nodiscard]] auto caption_buttons() const noexcept -> windows_caption_buttons*;
		/// \brief The compact menu control at the strip's left, standing in for the menu bar.
		[[nodiscard]] auto strip_menu_button() const noexcept -> QToolButton*;
#endif
		/// \brief The title row's modality control, at its right beside refresh.
		///
		/// \details [[BPR-0646]]. Overview, Read, Review and Discuss are
		/// presets for which drawers are open. Choosing one dispatches the
		/// core's exact set-drawer intents; the control shows the last chosen
		/// preset until the next, whatever a manual toggle did in between.
		[[nodiscard]] auto modality() const noexcept -> modality_control*;
		/// \brief Home's landing while nothing is open: sign in, or choose.
		[[nodiscard]] auto home_landing() const noexcept -> workbench_landing*;
		/// \brief The decorative Home presentation, absent when assets were refused.
		[[nodiscard]] auto dif() const noexcept -> dif_widget*;
		/// \brief The explicit action that reveals the private Notes area.
		[[nodiscard]] auto ai_tools_action() const noexcept -> QAction*;
		/// \brief Compose's illustration slot for the currently offered workshop revision.
		[[nodiscard]] auto dif_workshop_slot() const noexcept -> QWidget*;
		/// \brief [[BPR-0775]]. Dif's resting place at the start of the status line.
		///
		/// \details Mandate r2: Dif greets on the first-launch card and otherwise
		/// rests in the window's bottom-left corner. The slot reserves the status
		/// line's leading edge while Home shows him there; the sprite itself is a
		/// window child laid over the slot's bottom-left, with no pointer or
		/// focus path.
		[[nodiscard]] auto dif_corner_slot() const noexcept -> QWidget*;
		/// \brief Applies the profile's accepted visibility and motion resolution.
		void adopt_dif(core::dif_resolution aResolution);
		/// \brief Primes the exact accepted review whose AI projections may cue Dif.
		void observe_dif_context(
			core::application_session_id aSession,
			core::pull_request_authority aAuthority,
			core::agent_revision_binding aBinding);
		void clear_dif_context();
		/// \brief Offers already-presented accepted AI facts to the Qt-local router.
		void admit_dif_invocation(
			const application::agent_invocation_projection_identity& aIdentity);
		void start_dif_invocation(
			const application::agent_invocation_projection_identity& aIdentity);
		void present_dif_agent_delivery(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_finding_delivery&               aDelivery,
			bool                                                     aReplyPresented,
			bool                                                     aFindingsPresented);
		void finish_dif_invocation(
			const application::agent_invocation_projection_identity& aIdentity,
			application::agent_run_outcome                           aOutcome);
		void present_dif_workshop(
			const core::agent_session& aSession,
			std::uint64_t              aActionGeneration,
			core::review_draft_id      aDraft,
			std::uint64_t              aRevision,
			std::string_view           aProposed);
		void begin_dif_draft(
			const core::revisioned_review_draft_command& aCommand);
		void accept_dif_draft(
			const core::revisioned_review_draft_command& aCommand,
			const core::revisioned_review_draft&         aAccepted);
		void cancel_dif_draft(
			const core::revisioned_review_draft_command& aCommand);
		void present_dif_continuation(
			const core::agent_session&            aSession,
			const core::agent_continuation_offer& aOffer);
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

		/// \brief [[BPR-1157]]. File > Open Project…: the local Review Cycles entry.
		[[nodiscard]] auto open_project_action() const noexcept -> QAction*;
		/// \brief [[BPR-1157]]. Takes the local cycle page as the review surface's
		/// alternative: the same place, the title row and the window, no new row.
		void               adopt_local_cycle(review_cycle_view* aView);
		[[nodiscard]] auto local_cycle() const noexcept -> review_cycle_view*;
		/// \brief Shows the local cycle page, titled in the title row with its
		/// Local cycle badge. A pull request stays open behind it.
		void show_local_cycle(const QString& aTitle);
		/// \brief [[BPR-1171]]. Keeps the local cycle's title current without
		/// moving the window: Home or a pull request in front stays in front.
		void retitle_local_cycle(const QString& aTitle);
		/// \brief Returns to Home or to the open pull request.
		void               leave_local_cycle();
		[[nodiscard]] auto showing_local_cycle() const noexcept -> bool;
		/// \brief Home's Review Cycles section: Open Project and recent cycles.
		void               present_local_cycle_recents(const std::vector<application::review_cycle_recent>& aRecents);
		void               set_local_cycle_entry_dispatch(std::function<void()> aOpenProject, std::function<void(const core::local_cycle_subject&)> aResume);
		[[nodiscard]] auto home_cycles_list() const noexcept -> QListWidget*;
		/// [[BPR-0727]]. The reason of the last gesture the core refused, as
		/// published; `none` until one is.
		[[nodiscard]] auto last_refusal() const noexcept -> core::workbench_reason_code;
		[[nodiscard]] auto current_modality() const noexcept -> review_modality;
		/// \brief Enters a modality, or chooses it as the strip or its key does.
		///
		/// \details [[BPR-1295]]. Every path that lands in a modality enters
		/// it, and Review then keeps the arrangement it last had in this
		/// window. `choose` is the strip's button and key: choosing Review
		/// while it shows applies its preset.
		void choose_modality(review_modality aModality, core::workbench_modality_entry aEntry = core::workbench_modality_entry::enter);
		/// \brief [[BPR-1295]]. Hide Drawers: hides every open drawer, or
		/// shows the ones it hid; Command-B (Ctrl+B), the View menu and the
		/// title row's button beside the Comments toggle.
		[[nodiscard]] auto toggle_drawers_action() const noexcept -> QAction*;
		/// \brief The strip's account button: sign-in while signed out,
		/// the accounts settings page once signed in.
		[[nodiscard]] auto account_action() const noexcept -> QAction*;
		/// \brief The repository and pull request identity at the strip's left.
		[[nodiscard]] auto identity_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1338]]. Open on GitHub: the open pull request's page, or on
		/// Home the current row's. Command-Shift-O (Ctrl+Shift+O), the Pull Request
		/// menu, the title row's button and Home's row menus.
		[[nodiscard]] auto open_on_github_action() const noexcept -> QAction*;
		/// \brief [[BPR-1338]]. The icon button right after the pull request's name.
		[[nodiscard]] auto open_on_github_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1338]]. The menu a Home row opens on a right click.
		[[nodiscard]] auto home_row_menu() const noexcept -> QMenu*;
		/// \brief [[BPR-1342]]. Open Review: the Home row the menu was opened on,
		/// in this window (Return). The menu's default.
		[[nodiscard]] auto home_open_review_action() const noexcept -> QAction*;
		/// \brief [[BPR-1342]]. Open Review in New Window: that row in a new
		/// window of this profile (⌘Return, Ctrl+Return off macOS).
		[[nodiscard]] auto home_open_review_window_action() const noexcept -> QAction*;
		/// \brief "change 3 of 11", derived from the comparison the surface holds.
		[[nodiscard]] auto change_position_label() const noexcept -> QLabel*;
		/// \brief "file 3 of 40", from the core's navigation cursor.
		[[nodiscard]] auto file_position_label() const noexcept -> QLabel*;
		[[nodiscard]] auto previous_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_file_action() const noexcept -> QAction*;
		/// \brief The review window's single View > Go to header/source command.
		[[nodiscard]] auto header_source_action() const noexcept -> QAction*;
		void               set_header_source_dispatch(std::function<void()> aDispatch);
		/// \brief Presents one command result, announcing no-op reasons or focusing the source.
		void present_header_source_result(core::header_source_resolution aResult);
		/// \brief Routes a navigation press through exact Review restoration; empty cancels retained entry work.
		void set_review_navigation_dispatch(std::function<void(std::optional<core::workbench_command_id>)> aDispatch);
		/// \brief Applies one navigation command after the host has restored Review's exact context.
		void perform_review_navigation(core::workbench_command_id aCommand);
		/// \brief Focuses visible Review content or the originating navigation control.
		void focus_review_navigation(core::workbench_command_id aCommand);
		/// \brief [[BPR-1539]]. Says that a retained navigation press took no
		/// step because the active tab's exact view is unavailable, then focuses
		/// as `focus_review_navigation` does.
		void               refuse_review_navigation(core::workbench_command_id aCommand);
		[[nodiscard]] auto previous_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_change_action() const noexcept -> QAction*;
		/// \brief [[BPR-1532]]. View > First change, Option-Shift-Up (Alt+Shift+Up).
		[[nodiscard]] auto first_change_action() const noexcept -> QAction*;
		/// \brief [[BPR-1532]]. View > Last change, Option-Shift-Down (Alt+Shift+Down).
		[[nodiscard]] auto last_change_action() const noexcept -> QAction*;
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
		/// [[BPR-1334]]. View > Line Details.
		[[nodiscard]] auto line_details_action() const noexcept -> QAction*;
		[[nodiscard]] auto one_pane_action() const noexcept -> QAction*;
		/// \brief [[BPR-1336]]. View > Zoom In, Zoom Out and Actual Size.
		[[nodiscard]] auto zoom_in_action() const noexcept -> QAction*;
		[[nodiscard]] auto zoom_out_action() const noexcept -> QAction*;
		[[nodiscard]] auto actual_size_action() const noexcept -> QAction*;
		/// \brief [[BPR-1336]]. Where a zoom step from the menu, a key, ⌘-scroll
		/// or pinch goes: the profile's appearance host, which decides.
		void set_code_zoom_dispatch(std::function<void(core::code_zoom_step)> aDispatch);
		/// \brief [[BPR-1336]]. Adopts the accepted code record: the commands'
		/// state and reasons, and the comparison's indicator when the zoom moved.
		void               adopt_code_zoom(const core::code_font_settings& aCode);
		[[nodiscard]] auto files_drawer() const noexcept
			-> workbench_drawer_panel*;
		/// \brief [[BPR-0658]]. The files drawer's tree and its order and filter.
		[[nodiscard]] auto files_tree() const noexcept -> files_tree_view*;
		[[nodiscard]] auto files_order_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto files_filter() const noexcept -> QLineEdit*;
		/// \brief [[BPR-0981]]. The files drawer's empty state under a filter.
		[[nodiscard]] auto files_filter_notice() const noexcept -> filter_empty_notice*;
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
		[[nodiscard]] auto writing_hooks() const noexcept -> writing_hooks_panel* { return WritingHooks; }
		/// [[BPR-1301]]. Settings > Agents & context's Claude process wrapper.
		[[nodiscard]] auto claude_wrapper() const noexcept -> claude_wrapper_panel* { return ClaudeWrapper; }
		/// \brief The side chat, placed beside the agent launcher it belongs to.
		///
		/// \details BPR-0513. The panel is composed here because the drawer is
		/// where a reviewer can see it; the window supplies no dispatch, so
		/// until the application sets one every gesture on it goes nowhere.
		/// Placement adds nothing to what the panel holds — the aggregate its
		/// own tests bind by name is untouched.
		[[nodiscard]] auto side_chat() const noexcept -> side_chat_panel*;
		[[nodiscard]] auto private_review() const noexcept -> private_review_panel* { return PrivateReview; }
		[[nodiscard]] auto private_notes() const noexcept -> private_notes_panel*;
		/// \brief The finding queue, placed above the draft the promotion fills.
		///
		/// \details BPR-0513, on the same terms as `side_chat()`. Its
		/// conversion actions are offered as buttons in this drawer, and the
		/// workshop those promotions reach is the one already inside
		/// `review_editor()`, one widget below it.
		[[nodiscard]] auto finding_queue() const noexcept -> finding_queue_view*;
		[[nodiscard]] auto review_submission_editor_view() const noexcept -> review_submission_editor*;
		[[nodiscard]] auto review_editor() const noexcept -> review_draft_editor*;
		/// \brief [[BPR-1083]]. The label tints of the theme on screen. With the
		/// mark style the reviewer chose on Reviews & queue, they reach every
		/// surface in this window that draws a label mark.
		void set_label_tints(const feedback_label_tints& aTints);
		/// \brief [[BPR-1085]]. The theme's attention amber (`state_attention`),
		/// for Approve's dot while my blocking feedback is open.
		void set_attention_colour(const QColor& aColour);
		/// \brief [[BPR-1331]] v003. The private progress marks' colours in
		/// the theme on screen, for the review bar's button and its menu.
		void set_progress_mark_colours(const progress_mark_colours& aColours);
		/// \brief The colours the progress marks are painted in: the theme's
		/// once the composition supplies them, else the built-in light or dark
		/// variant, whichever the window's palette is.
		[[nodiscard]] auto shown_progress_mark_colours() const -> progress_mark_colours;
		/// \brief The tints and mark style this window draws marks with.
		[[nodiscard]] auto label_marks() const -> feedback_mark_theme;
		/// \brief Opens a retained draft for writing without changing or
		/// publishing it.
		/// \details [[BPR-1212]]. The draft on the selected line opens in the
		/// drawer at its line while the code shows; otherwise, and for the
		/// whole-review summary, the full composer opens in Discuss.
		void open_review_composition(bool aInline = false);
		/// \brief [[BPR-1212]]. Opens the draft on the selected line in
		/// Discuss's full composer (Maximize, ⌘⇧↩): a workshop proposal's
		/// before and after needs its room.
		void open_draft_in_discuss();
		/// \brief Returns to the invoking surface and focus without submitting.
		/// \details [[BPR-1212]]. A draft maximized from the drawer goes back
		/// to the drawer, writing, with its text and caret.
		void close_review_composition();
		/// \brief [[BPR-1212]]. Starts a new comment on the selected line (or
		/// a suggested change on it) as a draft in the drawer.
		/// \returns Whether the draft opened, in the drawer or in Discuss.
		auto begin_new_comment(bool aSuggestion = false) -> bool;
		/// \brief [[BPR-1477]]. Opens the editor's new comment at its line in
		/// the drawer again, as choosing its card does, locked or not: a
		/// comment whose publication answer was lost shows its Retry there.
		/// \details Only while Review shows the code with the Comments drawer
		/// open; nothing else is opened, expanded or changed.
		/// \returns Whether the draft is being written in the drawer.
		auto reopen_draft_in_drawer() -> bool;
		/// \brief [[BPR-1212]]. Add comment (or Suggest change) on the selected
		/// lines: Return and Shift+Return in a focused code pane, the code's
		/// right-click items and the gutter's "+".
		/// \details The core decides the selection (`decide_comment_range`);
		/// a refusal opens nothing and the status line gives its reason.
		/// Otherwise the range's last line is selected, the draft there takes
		/// the range's first line in one edit, and `begin_new_comment` opens
		/// it in the drawer. Return always takes the selection, so one line
		/// narrows a range draft ending on it (coordinator, 2026-10-07).
		/// \returns Whether a draft opened or is opening once its line's draft
		/// arrives.
		auto comment_on_selected_lines(bool aSuggestion = false) -> bool;
		/// \brief [[BPR-1212]]. The code's Add comment and Suggest change, as
		/// the right-click menu offers them.
		[[nodiscard]] auto add_comment_action() const noexcept -> QAction*;
		[[nodiscard]] auto suggest_change_action() const noexcept -> QAction*;
		/// \brief [[BPR-1212]]. The menu a right-click on the code opens at a
		/// point of a pane, built as the gesture builds it; the caller owns it.
		[[nodiscard]] auto code_context_menu(QPlainTextEdit* aPane, QPoint aPosition) -> QMenu*;
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
		/// \brief [[BPR-0776]]. Home's list: a panel per state in state order, and
		/// Recently viewed last.
		[[nodiscard]] auto home_list() const noexcept -> qt::home_list*;
		/// \brief [[BPR-0783]]. Where this window's Home view leaves for the host,
		/// which validates it and persists it beside the window's layout.
		///
		/// \details Sent whole on every change: Group, State headers, the collapse
		/// of the panels that carry no name, and the window's own Filter once it
		/// owns one. A group panel's collapse is not in it (Decisions (a)).
		void set_home_view_dispatch(std::function<void(application::home_view_state)> aDispatch);
		void set_geometry_dispatch(std::function<void(application::workbench_window_geometry)> aDispatch);
		/// \brief [[BPR-0783]]. The Home view this window shows.
		[[nodiscard]] auto home_view() const noexcept -> const application::home_view_state&;
		/// \brief [[BPR-1580]]. Home's one strip control, shown while Home is and an
		/// account can fill it: View, whose menu holds Group by, Sort by, State
		/// headers, My role, Added by, Quiet items and Reset to defaults.
		[[nodiscard]] auto home_view_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto home_view_menu() const noexcept -> QMenu*;
		/// \brief One of the View menu's submenus.
		[[nodiscard]] auto home_view_submenu(home_view_choice aChoice) const noexcept -> QMenu*;
		/// \brief Restores every View choice to its default.
		[[nodiscard]] auto home_view_reset_action() const noexcept -> QAction*;
		[[nodiscard]] auto home_group_action(application::home_grouping aGrouping) const noexcept -> QAction*;
		[[nodiscard]] auto home_state_headers_action() const noexcept -> QAction*;
		/// \brief The View menu's way to Settings › Reviews & queue, where the
		/// saved repositories and the observer rules are.
		[[nodiscard]] auto home_filter_settings_action() const noexcept -> QAction*;
		/// \brief [[BPR-0776]]. The current queue row on Home, whichever state holds it.
		[[nodiscard]] auto selected_queue_item() const -> std::optional<core::queue_item>;
		/// \brief [[BPR-0650]]. Home's open bar.
		[[nodiscard]] auto home_open_bar() const noexcept -> QLineEdit*;
		/// \brief [[BPR-0775]]. The bar's own label, "Open", inside its frame.
		[[nodiscard]] auto home_open_label() const noexcept -> QLabel*;
		/// \brief [[BPR-0775]]. The bordered frame that holds the label, the field and its shortcut.
		[[nodiscard]] auto home_open_frame() const noexcept -> QWidget*;
		/// \brief [[BPR-0775]]. Command-L: Home, with the bar focused and its text selected.
		[[nodiscard]] auto home_focus_bar_action() const noexcept -> QAction*;
		/// \brief The window's one status line; on Home it is the session line.
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief [[BPR-0669]]. Opens what the bar names; disabled until it names one.
		[[nodiscard]] auto home_open_action() const noexcept -> QAction*;
		/// \brief [[BPR-0669]]. States a refusal beside the bar rather than doing nothing.
		[[nodiscard]] auto home_open_notice() const noexcept -> QLabel*;
		/// \brief [[BPR-0771]]. Copy details, beside a load refusal under the bar.
		[[nodiscard]] auto home_open_details_button() const noexcept -> QPushButton*;
		/// \brief [[BPR-1200]]. The card under the bar that says a pull request
		/// was refused on this connection and offers what could open it.
		[[nodiscard]] auto home_refusal_card() const noexcept -> home_refusal*;
		/// \brief [[BPR-1357]]. Home's status band under the bar: what the bar
		/// asked for, then Home's general status, at body size behind a mark.
		[[nodiscard]] auto home_status_line() const noexcept -> qt::home_status_line*;
		/// \brief [[BPR-1357]]. Home's general status in the band: a problem the
		/// window has, a connection under way, or what was just done. Hidden
		/// while Home is quiet; the session line stays in the status bar.
		[[nodiscard]] auto home_status_text() const noexcept -> QLabel*;
		/// \brief [[BPR-1381]]. Whether Home's band, docked above the status bar,
		/// is saying the status bar's own message, which the status bar then
		/// does not draw a second time.
		[[nodiscard]] auto home_status_echoes_bar() const -> bool;
		/// \brief [[BPR-1357]]. Cancel and Retry for an opening the band says.
		[[nodiscard]] auto home_open_cancel_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto home_open_retry_button() const noexcept -> QPushButton*;
		/// \brief [[BPR-1357]]. The pull request Home asked to open, as the
		/// application's Home-open host decided it, in the gesture's own turn.
		///
		/// \details The row that asked shows it; the band under the bar shows it
		/// when no shown row does. Cancel and Retry are offered only where the
		/// projection says the core offers them.
		void               present(const application::home_open_projection& aProjection);
		[[nodiscard]] auto home_open() const noexcept -> const std::optional<application::home_open_projection>&;
		void               set_home_open_dispatch(application::home_open_dispatch aDispatch);
		/// \brief [[BPR-1357]]. The theme's failure ink for Home's marks.
		void set_home_failure_ink(const QColor& aInk);
		/// \brief [[BPR-0771]]. Says the composition's review-restoration notice under the bar.
		///
		/// \details The window is the one writer of the notice under the bar: a
		/// setup stage's notice speaks first, then this, then a load refusal. An
		/// empty text withdraws it.
		void present_home_restoration_notice(const QString& aText);
		/// \brief [[BPR-0733]] [[BPR-0776]]. Home's Recently viewed panel, its
		/// list, its Resume action and the row button that shows it.
		///
		/// \details The panel is shown only while entries were published; an
		/// empty list is no panel at all. Resume is a row action: on the hovered
		/// row, on the current row while the list has focus, and in the row's
		/// menu.
		[[nodiscard]] auto home_recent_panel() const noexcept -> home_panel*;
		[[nodiscard]] auto home_recent_list() const noexcept -> QListWidget*;
		[[nodiscard]] auto home_recent_resume_action() const noexcept -> QAction*;
		[[nodiscard]] auto home_recent_remove_action() const noexcept -> QAction*;
		[[nodiscard]] auto home_recent_row_action() const noexcept -> QToolButton*;
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
		void               set_storage_location(const QString& aPath);
		void               set_profile_label(const QString& aLabel);
		void               set_notification_preview_dispatch(std::function<bool(const core::pull_request_ref&)> aDispatch);
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
		/// \brief [[BPR-0990]]. Home's refresh glyph: the profile's discovery.
		[[nodiscard]] auto queue_refresh_action() const noexcept -> QAction*;
		/// \brief [[BPR-0990]]. The one sentence Home says about discovery.
		[[nodiscard]] auto home_discovery_status() const noexcept -> QLabel*;
		/// \brief [[BPR-0990]]. The Pull Request menu and its two local gestures.
		[[nodiscard]] auto pull_request_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto follow_pull_request_action() const noexcept -> QAction*;
		[[nodiscard]] auto save_pull_request_repository_action() const noexcept -> QAction*;
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
		/// \brief [[BPR-0771]], [[BPR-0772]]. Accounts & hosts' cards, one per
		/// configured server (hostname), in the order their hosts were first
		/// configured.
		[[nodiscard]] auto server_cards() const -> std::vector<server_card*>;
		/// \brief The card of the server one host profile is a way in to; null
		/// when the profile is not configured.
		[[nodiscard]] auto server_card_for(core::host_profile_id aProfile) const
			-> server_card*;
		/// \brief The public-preview hourly-limit ring, on the card of the host
		/// being read.
		[[nodiscard]] auto usage_indicator() const noexcept -> usage_ring*;
		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool;
		/// \brief The refusal wording currently shown, empty when none is.
		[[nodiscard]] auto public_preview_refusal_text() const -> QString;

		/// \brief The appearance surface, composed beside the review it governs.
		/// \brief Shows the Settings window on a page; a no-op before composition.
		/// \brief Lets the profile owner route every Settings gesture to its single shell.
		void set_settings_presenter(std::function<void(settings_page)> aPresenter);
		/// \brief Routes direct Help exports to the profile-owned maintenance host.
		void set_diagnostic_exporter(std::function<void(core::diagnostic_export_policy)> aExporter);
		/// \brief Mounts this live window's pages into a profile-owned shell.
		/// \details The shell must outlive this mount; unmount its previous session first.
		/// Destruction restores pages before destroying their callback-owning workbench.
		void mount_settings(settings_window& aSettings);
		void unmount_settings();

		void open_settings(settings_page aPage);
		/// \brief Presents the shared connection controls without Settings navigation.
		void               open_sign_in(std::string aHostname = { }, bool aCliOnly = false);
		void               set_sign_in_cancel_dispatch(std::function<void()> aDispatch);
		void               connect_github_profile(core::host_profile_id aProfile);
		void               present_cli_account_choices(std::vector<core::host_profile_id> aProfiles);
		[[nodiscard]] auto sign_in_dialog() const noexcept -> QDialog*;
		[[nodiscard]] auto settings() const noexcept -> settings_window*;
		[[nodiscard]] auto settings_action() const noexcept -> QAction*;
		/// \brief [[BPR-1567]]. Help → Keyboard Shortcuts, on Command-/ (Ctrl+/).
		[[nodiscard]] auto keyboard_shortcuts_action() const noexcept -> QAction*;
		/// \brief [[BPR-1567]]. Opens the window's Keyboard Shortcuts, or raises it.
		void show_keyboard_shortcuts();
		/// \brief [[BPR-1567]]. The window's Keyboard Shortcuts, once opened.
		[[nodiscard]] auto keyboard_shortcuts() const noexcept -> keyboard_shortcuts_window*;
		[[nodiscard]] auto appearance_settings() const noexcept
			-> appearance_settings_panel*;
		[[nodiscard]] auto files_drawer_toggle_action() const noexcept -> QAction*;
		/// \brief [[BPR-0651]]. The repositories drawer, left of the files.
		[[nodiscard]] auto repositories_drawer() const noexcept -> workbench_drawer_panel*;
		[[nodiscard]] auto repositories_drawer_toggle_action() const noexcept -> QAction*;
		[[nodiscard]] auto repositories_tree() const noexcept -> repositories_tree_view*;
		/// \brief [[BPR-0981]]. The repositories drawer's filter and its empty state.
		[[nodiscard]] auto repositories_filter() const noexcept -> QLineEdit*;
		[[nodiscard]] auto repositories_filter_notice() const noexcept -> filter_empty_notice*;
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
		/// \brief [[BPR-1212]]. Whether the left side is numbered as GitHub's
		/// base, so a base line takes threads and new comments.
		[[nodiscard]] auto left_conversation_endpoint_current() const noexcept -> bool;
		/// \brief Returns the composed full-width line-over-line detail pane.
		///
		/// \details This forwards to RFC-BPR-0004's comparison surface, which
		/// owns the pane and resolves its rows from the one canonical semantic
		/// mapping. The window adds no second detail authority.
		[[nodiscard]] auto detail_pane() const noexcept -> QPlainTextEdit*;

		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> workbench_window_statistics;

		/// \brief Binds explicit reader gestures to current-context application policy.
		void set_markdown_link_dispatch(std::function<core::markdown_link_result(const core::markdown_link_intent&)> aDispatch);
		/// \brief RFC-BPR-0084. Sends a rendered body's image references, with this
		/// window's accepted Markdown context, to the application.
		void set_markdown_image_dispatch(std::function<void(const core::markdown_image_intent&)> aDispatch);
		/// \brief [[BPR-1562]]. Sends the fenced code of the description and
		/// the open thread's posts to the application's classification host.
		void set_markdown_code_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		/// \brief Colours every reader of this window that shows the answer's code.
		void apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief [[BPR-1338]]. Binds Open on GitHub to the host, which admits the
		/// named pull request and builds its address.
		void set_pull_request_page_dispatch(std::function<core::markdown_link_result(const application::pull_request_page_intent&)> aDispatch);
		/// \brief [[BPR-1566]]. Binds About Conventional Comments, in Settings
		/// and in every label menu of this window, to the application, which
		/// admits the named page and opens it. Unset, neither is offered.
		void set_reference_page_dispatch(std::function<core::markdown_link_result(core::reference_page)> aDispatch);
		/// \brief [[BPR-1342]]. Binds Open Review in New Window to the host, which
		/// admits the named row and asks the profile for a new window that opens
		/// it. True when the new window was asked for.
		void set_review_window_dispatch(std::function<bool(const application::review_window_intent&)> aDispatch);

	protected:
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void moveEvent(QMoveEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		/// \brief [[BPR-0733]]. Reads Return on Home's recently viewed list.
		///
		/// \details Return opens the current row and Shift+Return resumes its
		/// saved reading. Both are read here rather than left to the view,
		/// which activates a row on Return on some platforms and treats the
		/// key as an editing gesture on others.
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void                                                        restore_geometry(const application::workbench_window_geometry& aGeometry);
		void                                                        dispatch_geometry();
		std::function<void(application::workbench_window_geometry)> GeometryDispatch;
		bool                                                        GeometryReady    = false;
		bool                                                        ApplyingGeometry = false;
		QString                                                     ProfileLabel     = QStringLiteral("Default");
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
#if defined(_WIN32)
		/// \brief Opens the strip's application menu under its control.
		///
		/// \details [[BPR-1028]]. The one way that menu opens. It is a heap child
		/// of the window that deletes itself when it closes, shown with
		/// `popup()`: nothing waits for it, so a Quit or close that destroys the
		/// window while it is open takes the menu with it and leaves no frame of
		/// the window's on the stack. [[BPR-1022]]'s keyboard path opens it here
		/// too and is handed the menu to highlight its first entry.
		auto open_strip_menu() -> QMenu*;
		/// \brief Reads the platform's menu keys from the window's own input.
		///
		/// \details [[BPR-1022]]. Alt pressed and released alone, or F10, opens
		/// the strip's application menu with its first menu highlighted, as the
		/// hidden menu bar would have. Measured on the Windows station, Qt's
		/// key mapper consumes both keys, so the platform never enters its own
		/// menu mode and Qt's hidden bar does nothing: the gesture is read here,
		/// from the key and mouse events the window receives before any of its
		/// widgets. Any other key, a shortcut, a mouse button, a wheel turn or a
		/// change of focus between Alt's press and release makes Alt a modifier,
		/// and a modifier opens nothing. Returns whether the event was taken.
		[[nodiscard]] auto read_strip_menu_key(const QEvent& aEvent) -> bool;
		/// \brief Opens the strip's application menu with its first menu highlighted.
		void open_strip_menu_from_keyboard();
#endif
		/// [[BPR-0981]]. Shows each drawer's empty state when its filter
		/// leaves nothing, and hides it otherwise.
		void refresh_filter_notices();
		void build_central_surface();
		void build_github_header();
		void apply_github_transition(
			const application::github_transition_projection& aTransition);
		void apply_github_pull_request(
			const application::github_pull_request_projection& aPullRequest, bool aPreserveWindowNavigation);
		void apply_github_content_command(
			const core::github_command_projection& aCommand);
		void apply_github_host_diagnostic(
			const application::github_host_diagnostic_projection& aDiagnostic);
		void clear_github_pull_request();
		void update_github_presentation_state();
		void update_review_readiness();
		/// \brief Disables what the signed-out mode cannot do, with wording.
		void apply_public_preview_availability();
		/// \brief Presents the observed inline conversation, or says it was not.
		void apply_public_preview_conversation();
		void update_public_preview_title();
		void announce_github_presentation_update();
		void dispatch_navigation(std::size_t aIndex);
		/// \brief [[BPR-1532]]. Binds First and Last change's keys unless the
		/// focused widget takes typed text or binds the same key itself.
		void update_change_end_keys();
		/// \brief [[BPR-1532]]. Enables and describes First and Last change
		/// from the core's projection.
		void describe_change_ends();
		void dispatch_drawer_toggle(std::size_t aIndex);
		void observe_drawer_width(
			core::workbench_drawer aDrawer,
			std::uint32_t          aWidth);
		void apply_drawer_layout();
		void apply_drawer_widths();
		void observe_handle_drag();
		void configure_tool_buttons();
		void refresh_toolbar_icons();
		/// \brief [[BPR-1331]] v003. Paints the review bar's progress button
		/// with the selected file's mark and each of its menu's actions with
		/// the mark it names, all from one painter in the theme's colours.
		void refresh_progress_icons();
		void update_strip_positions();
		void update_idle_direction();
		void update_strip_identity();
		void update_account_action();
		void apply_strip_width();
		/// \brief [[BPR-1129]]. What yields on the review bar as it narrows: the
		/// comparison's dates, then the positions' words. Nothing folds away.
		void apply_review_bar_width();
		/// \brief [[BPR-1129]]. Marks Compose review while a review draft is kept.
		void update_compose_mark();
		/// [[BPR-1345]]. The chip counts what the saved drafts say.
		void update_review_chip();
		/// \brief [[BPR-1133]]. The review thread Comment answers: the one the
		/// reviewer chose, or the one on the line they selected.
		[[nodiscard]] auto comment_reply_thread() const -> std::optional<std::string>;
		/// \brief [[BPR-1133]]. What Comment would start, in words; empty when
		/// neither a thread nor a line is selected.
		[[nodiscard]] auto comment_target_text() const -> QString;
		/// \brief [[BPR-1133]]. Names Comment's target on the command, its
		/// tooltip and its presenter.
		void update_comment_target();
		/// \brief [[BPR-1133]]. Comment's gesture: a reply to the selected
		/// thread, or a comment on the current line.
		void comment_or_reply();
		/// \brief The one Reply path, from a card or from Comment.
		/// \brief [[BPR-1290]]. In place when the gesture came from the thread's
		/// note beside the code; on the composition page otherwise.
		void reply_to_thread(const std::string& aIdentity, bool aInPlace = false);
		/// \brief [[BPR-1132]]. Gives the strip the words for the published tabs.
		void present_document_tabs();
		/// \brief [[BPR-1132]]. The tab keys follow how many tabs there are.
		void update_tab_actions();
		/// \brief [[BPR-1132]]. Shows or hides the older-pair notice.
		void update_older_pair_notice();
		/// \brief [[BPR-1132]]. Show them: the file's threads where they sit.
		void show_later_threads();
		/// \brief [[BPR-1132]]. A pair in words: "Feb 4 → Feb 9", or short,
		/// "Feb 4 → 9", for a tab's pill.
		[[nodiscard]] auto pair_text(const core::comparison_pair& aPair, bool aShort) const -> QString;
		/// \brief [[BPR-1132]]. A file's path, as the files drawer lists it.
		[[nodiscard]] auto path_of(const core::file_identity& aFile) const -> QString;
		/// \brief [[BPR-1132]]. Whether the comparison shown is off the pull
		/// request's current head, where no new comment may start.
		[[nodiscard]] auto comparing_off_current_head() const -> bool;
		void               apply_review_details_visibility();
		void               update_home_landing();
		/// \brief [[BPR-0775]]. Puts the landing above the bar as the return row,
		/// or under it as the first-launch card.
		void place_home_landing();
		/// \brief [[BPR-0775]]. Home with the bar focused and its text selected.
		void focus_home_bar();
		void update_dif_presentation();
		/// \brief [[BPR-0775]]. Lays the corner sprite over its status-line slot.
		void               place_dif_corner();
		void               play_dif_cue(std::optional<dif_clip_id> aCue);
		[[nodiscard]] auto showing_ai_tools() const -> bool;
		[[nodiscard]] auto showing_dif_workshop() const -> bool;
		/// \brief [[BPR-0733]]. Rebuilds Home's recently viewed rows and shows
		/// or hides the section by whether there are any.
		void apply_recent_reviews();
		/// \brief [[BPR-0733]]. Offers Resume only while the current row kept a reading.
		void update_recent_resume_action();
		/// \brief [[BPR-0776]]. Shows the queue surface, its controls, its status and
		/// the list's state panels by account, readiness and recent entries.
		void update_home_surface();
		/// \brief [[BPR-0733]]. Sends one recently viewed row out, to open or to resume.
		void dispatch_recent_review(int aRow, bool aResume);
		void remove_recent_review(int aRow);
		/// \brief [[BPR-1157]] [[BPR-1364]]. Resumes one of Home's recent review
		/// cycles, by a double-click or Return on its row.
		void resume_home_cycle(int aRow);
		/// [[BPR-1365]]. Home's lists: the queue (every arrangement's rows),
		/// Recently viewed and Review Cycles.
		enum class home_selection_list : std::uint8_t {
			queue,
			recent,
			cycles,
		};
		/// \brief [[BPR-1365]]. Makes one Home list the one that shows a
		/// selection: its current row is selected, natively, and every other
		/// list keeps its current row but selects nothing.
		void claim_home_selection(home_selection_list aList);

		/// [[BPR-0722]]. Pixels the other visible strip items leave the identity.
		[[nodiscard]] auto strip_identity_budget() const -> int;
		/// [[BPR-0722]]. Least width the identity's number-and-fragment form needs.
		[[nodiscard]] auto strip_identity_minimum() const -> int;
		/// \brief Shows the overview or the comparison for the current modality.
		void apply_modality_surface();
		void refresh_open_count_mark();
		/// \brief [[BPR-0648]]. Re-aligns the comment cards and the gutter marks.
		void refresh_cards();
		/// \brief Scopes the comments drawer to the window's conversation anchor.
		void apply_conversation_anchor();
		/// \brief [[BPR-0818]]. Applies the anchor on its own event-loop turn.
		void schedule_conversation_anchor();
		void apply_thread_marks();
		/// \brief [[BPR-1290]]. The thread a gutter marker or scroll-bar tick
		/// marks, with the drawer shown on its Comments tab; none when no
		/// placed thread is on that zero-based line.
		[[nodiscard]] auto thread_on_line(core::diff_side aSide, std::uint32_t aLine) -> std::optional<std::string>;
		/// \brief [[BPR-1290]]. A thread's gutter marker opens or closes the
		/// thread it marks.
		void toggle_thread_at(core::diff_side aSide, std::uint32_t aLine);
		/// \brief [[BPR-1290]]. Shift-click on a thread's scroll-bar tick:
		/// opens that thread, as pressing its note does, and brings its line
		/// into view even when it was already current.
		void go_to_thread_at(core::diff_side aSide, std::uint32_t aLine);
		/// \brief [[BPR-1290]]. Show line: reveals a thread's exact line.
		void reveal_thread_line(const std::string& aIdentity);
		/// \brief [[BPR-1290]]. The reply written in place is the draft
		/// editor's text: each edit goes through the editor's one edit path.
		void write_inline_reply(const std::string& aThread, const QString& aText);
		/// \brief [[BPR-1290]]. Tells the notes what the draft editor holds
		/// for the reply in place.
		void present_inline_reply();
		/// \brief [[BPR-1212]]. The draft written in the drawer is the draft
		/// editor's text, as the reply in place is.
		void write_inline_draft(const QString& aText);
		/// \brief [[BPR-1212]]. Tells the drawer's composer what the draft
		/// editor holds for the new comment.
		void present_inline_draft();
		/// \brief [[BPR-1212]]. Opens the drawer draft once the drawer shows.
		void resume_pending_draft();
		/// \brief [[BPR-1212]]. What the selection allows as a new comment's
		/// lines, or the reason it allows none.
		/// \details `aShort`, when given, receives the reason in the few words
		/// the right-click menu shows under its items.
		[[nodiscard]] auto decide_selected_lines(QString& aReason, QString* aShort = nullptr) const -> std::optional<core::comment_range_decision>;
		/// \brief [[BPR-1212]]. Why the selection takes no new comment, or
		/// nothing. With `aDraftRequired` false a draft that has not arrived
		/// yet is no reason: the host answers a line selected just now, as a
		/// right-click selects one.
		[[nodiscard]] auto comment_target_reason(bool aDraftRequired, QString* aShort = nullptr) const -> QString;
		/// \brief [[BPR-1212]]. Says why a waiting comment did not open, when
		/// its line's draft has not come within the wait.
		void report_line_comment_wait(core::source_line_ref aAnchor);
		/// \brief [[BPR-1212]]. Sets the range on the shown draft and opens it,
		/// once the shown draft is the waiting comment's; true when it did.
		auto resume_line_comment() -> bool;
		/// \brief [[BPR-1212]]. Leaves the drawer draft: a written draft is
		/// kept at its line, an empty one dropped, and focus goes to the code.
		void leave_drawer_draft(bool aDiscard);
		/// [[BPR-1345]] v002. Delete in the drawer asks first when the draft
		/// has words.
		void confirm_delete_drawer_draft();
		/// \brief [[BPR-1212]]. Maximize: the drawer draft goes to Discuss.
		void maximize_drawer_draft();
		/// \brief [[BPR-1212]]. The exact head lines the shown draft is on.
		[[nodiscard]] auto draft_source_lines() const -> std::optional<std::vector<std::string>>;
		/// [[BPR-1084]]. Tells the draft editor where the reply it shows is attached.
		void present_reply_placement();
		void update_new_comment_group();
		void step_to_next_new_comment();
		/// \brief Selects the overview entry file and enters its semantic modality.
		void enter_overview_at(
			application::overview_entry_kind aEntry,
			const QString&                   aPath);
		/// \brief Selects a file-row or thread path and enters Review.
		void enter_review_at(const QString& aPath);
		/// [[BPR-0657]]. Selects a thread's line on the comparison, as a click on
		/// it would, so the draft a card starts anchors on the thread's line.
		[[nodiscard]] auto select_thread_line(const std::string& aIdentity) -> bool;
		/// [[BPR-1046]]. Whether the comparison of a current thread's own file
		/// has been asked for and has not arrived, so its line cannot be
		/// selected yet but will be.
		[[nodiscard]] auto thread_comparison_loading(const std::string& aIdentity) const -> bool;
		/// [[BPR-1046]]. Selects a thread's line and only then names the thread
		/// to the composition, so the draft on that line answers it; refuses
		/// with a notice when the line cannot be selected.
		void begin_thread_reply(const std::string& aIdentity, bool aInPlace);
		/// [[BPR-1046]]. Completes or refuses a reply that waited for its
		/// thread's comparison, once a comparison has arrived.
		void resume_deferred_reply();
		/// \brief [[BPR-1293]]. ⌘J: Threads, or back from it.
		void toggle_threads();
		/// \brief [[BPR-1293]]. Back to the modality Threads was entered from.
		void leave_threads();
		/// \brief [[BPR-1293]]. Opens a waiting pick once its file's
		/// comparison shows its thread; drops it once another file shows.
		void resume_conversation_pick();
		/// [[BPR-1345]] v002. Selects a pending comment's line once its file's
		/// comparison is the one shown.
		void resume_pending_comment_pick();
		/// \brief [[BPR-1293]]. Tells the finder where Review is.
		void present_finder_reading();
		/// \brief [[BPR-1356]]. Discuss with the route's filters cleared and
		/// the thread's card open at \p aPost, selected, in view and focused.
		auto open_conversation_in_discuss(const std::string& aThread, const std::string& aPost, const core::workbench_discuss_filters& aClear) -> bool;
		// [[BPR-0661]]. The preference rows and what honours them.
		void               build_behaviour_editor(QWidget* aPage, QVBoxLayout* aColumn);
		void               apply_behaviour_controls();
		void               apply_label_marks();
		void               dispatch_behaviour_from_controls();
		[[nodiscard]] auto behaviour_from_controls() const -> application::review_behaviour_settings;
		[[nodiscard]] auto opening_modality() const -> review_modality;
		void               apply_first_comments_filter();
		void               honour_return_comparison();
		void               consider_mark_read_when_fully_seen();
		[[nodiscard]] auto file_for_path(const QString& aPath) const
			-> std::optional<core::workbench_file_id>;
		void               update_rail_summaries();
		void               infer_modality_from_drawers();
		[[nodiscard]] auto presented_files_state() const -> core::workbench_drawer_state;
		[[nodiscard]] auto presented_repositories_state() const -> core::workbench_drawer_state;
		void               apply_focus_order();
		void               update_action_state();
		void               update_header_source_action();
		void               build_menu_bar();
		void               dispatch_line_numbers_toggle();
		/// [[BPR-1334]]. View > Line Details: dispatches the profile's
		/// behaviour with Line details flipped.
		void dispatch_line_details_toggle();
		/// [[BPR-1334]]. Shows or hides Line details as published.
		void apply_line_details();
		/// RFC-BPR-0084 Q3. Shows or hides images in descriptions and comments.
		void apply_markdown_images();
		void apply_line_numbers();
		void dispatch_one_pane_toggle();
		void apply_one_pane();
		/// \brief [[BPR-1295]]. Re-asserts the drawers the core decided and
		/// sends the hide-and-show gesture.
		void dispatch_drawers_toggle();
		/// \brief [[BPR-1295]]. The command's check, glyph, words and
		/// enablement, from the core's projection.
		void apply_drawers_toggle();
		/// \brief [[BPR-1295]]. The drawer panel holding focus, if one does.
		[[nodiscard]] auto drawer_holding_focus() const -> QWidget*;
		/// \brief [[BPR-1295]]. A drawer that hid the focused control gives
		/// focus to the comparison rather than to whatever is next.
		void keep_focus_out_of_hidden_drawers(QWidget* aHeld);
		/// \brief [[BPR-1130]]. The disclosure's gesture: re-asserts the fold
		/// the core holds and sends the toggle.
		void dispatch_updates_line_toggle();
		/// \brief [[BPR-1130]]. Shows the fold the core decided: the timeline's
		/// row, the disclosure's arrow and mark, and the command's check.
		void apply_updates_line();
		void update_drawer_action(
			std::size_t                  aIndex,
			core::workbench_command_id   aId,
			core::workbench_drawer_state aState);
		void update_status_text();
		/// \brief The status line for what the window shows now, computed without side effects.
		[[nodiscard]] auto status_line() const -> QString;
		/// \brief [[BPR-0775]]. Home's session line.
		[[nodiscard]] auto home_session_line() const -> QString;
		/// \brief Whether the session is signed in, with or without an account.
		[[nodiscard]] auto session_signed_in() const -> bool;
		void               announce_update();
		void               apply_file_rows(
			const std::vector<application::workbench_file_projection>& aFiles);
		void               apply_file_delta(const application::workbench_files_delta& aDelta);
		[[nodiscard]] auto conversation_anchor_file() const -> std::optional<core::file_identity>;
		void               apply_file_selection();
		void               dispatch_file_selection(const QModelIndex& aIndex);
		void               dispatch_receipt(std::size_t aIndex);
		void               update_receipt_actions();
		/// [[BPR-1578]]. Whether a click on the Seen or Read step moves the
		/// file forward; a step already reached does nothing.
		[[nodiscard]] auto progress_step_offered(std::size_t aIndex) const -> bool;
		void               take_progress_step(std::size_t aIndex);
		void               update_progress_track();
		void               update_accept_file_shortcut();
		/// [[BPR-1536]]. Re-reads whether the keyboard is in a widget that
		/// takes typed text; when that changed, refreshes Accept file's key
		/// and, on macOS, lifts the menu bar's Option+character keys while it
		/// is and restores them after.
		void update_typing_keys();
		/// [[BPR-1536]]. An action's key as its tooltip names it, also while
		/// typing has lifted the key from the menu bar.
		[[nodiscard]] auto presented_key(const QAction* aAction) const -> QString;
		void               classify_selected_conversation();
		void               navigate_defect(int aDirection);
		void               update_defect_surface();
		void               update_defect_badges();
		void               append_thread_defect_menu(QMenu& aMenu, const std::string& aThread);
		void               show_defect_dialog(core::defect_id aDefect);
		[[nodiscard]] auto selected_conversation_classification() const
			-> std::optional<workbench_defect_classification>;
		/// \brief The current conversation if the reviewer chose it, else none.
		[[nodiscard]] auto chosen_conversation() const -> QModelIndex;
		void               synchronize_selected_conversation(const QModelIndex& aIndex);
		void               build_queue_landing(QWidget* aParent, QVBoxLayout& aLayout);
		/// \brief Points device sign-in at the host profile last chosen.
		void rebind_device_authorization_profile();
		/// \brief [[BPR-0773]]. Lays the device sign-in into the account row of
		/// the card holding the profile it is bound to.
		void place_device_sign_in();
		/// \brief [[BPR-0771]]. Makes one host the one a Connect or sign-in means.
		void select_github_profile(core::host_profile_id aProfile);
		void add_server_card(const application::github_profile_option& aOption);
		/// \brief Decides each card's presentation from the session and the host
		/// last chosen.
		void update_server_cards();
		/// \brief Says the setup notice, or a pull request's load refusal, under the bar.
		void update_home_open_notice();
		/// \brief [[BPR-1357]]. Re-presents the opening on its row and the band.
		void refresh_home_open();
		/// \brief [[BPR-1357]]. What the opening says now: "Opening #42…", then
		/// the step it is on, or why it failed. `aRow` drops the repository,
		/// which the row already shows.
		[[nodiscard]] auto home_open_text(bool aRow) const -> QString;
		/// \brief [[BPR-1357]]. Home's general status in the band, and its tone.
		void               update_home_status();
		void               dispatch_home_open_action();
		[[nodiscard]] auto home_refusal_text() const -> QString;
		/// \brief [[BPR-1200]]. The core's words for the refusal Home is showing, if it has its own.
		[[nodiscard]] auto home_refusal_wording() const -> std::optional<core::pull_request_refusal_wording>;
		void               take_refusal_action(core::pull_request_refusal_action aAction);
		/// \brief The card of the host the session is on, or of the chosen host
		/// while none is.
		[[nodiscard]] auto current_server_card() const -> server_card*;
		void               begin_selected_sign_in();
		void               update_sign_in_surface();
		/// \brief [[BPR-0775]]. The first-launch card's second way in: Accounts &
		/// hosts with a new enterprise server card in its edit state.
		void begin_enterprise_server_from_home();
		void dispatch_queue_rule(application::queue_rule_action aAction);
		void update_queue_rule_actions();
		void open_pull_request_item(const core::queue_item& aItem);
		/// \brief [[BPR-1338]]. What Open on GitHub names now: on Home the row
		/// last made current, elsewhere the open pull request.
		[[nodiscard]] auto pull_request_page_target() const -> std::optional<application::pull_request_page_intent>;
		void               open_pull_request_page(const application::pull_request_page_intent& aIntent);
		void               update_open_on_github_action();
		/// [[BPR-1566]]. Hands the named page to the application and says what
		/// became of it: in the status bar, and from Settings under its link.
		void open_reference_page(core::reference_page aPage, bool aFromSettings);
		/// \brief [[BPR-1342]]. The Home row Open Review and Open Review in New
		/// Window name: the row last made current, in whichever list.
		[[nodiscard]] auto home_review_target() const -> std::optional<std::variant<core::queue_item, application::recent_review_row>>;
		void               update_home_review_actions();
		void               open_home_review(bool aNewWindow);
		void               request_review_window(const application::review_window_intent& aIntent);
		/// \brief [[BPR-1342]]. Shows the one row menu for a queue or a Recently
		/// viewed row the right click made current.
		void popup_home_row_menu(const QPoint& aGlobal);
		void open_from_home_bar();
		/// \brief Builds Settings › Repositories and adopts it on its page.
		void build_repositories_page();
		/// \brief Lists the displayed account's saved repositories, keeping
		/// unchanged rows and the selection where the record still exists.
		void update_saved_repository_list();
		/// \brief Shows Home's first-repository call to action or its ordinary row.
		void update_home_repository_onboarding();
		void build_enterprise_server_draft(QWidget* aParent, QVBoxLayout& aLayout);
		void begin_github_cli_from_home();
		void open_enterprise_server_draft();
		void close_enterprise_server_draft();
		void add_enterprise_server();
		void update_enterprise_server_actions();
		void remember_enterprise_server(const std::string& aHostname);
		void confirm_forget_server(const application::saved_server& aServer);
		void dispatch_saved_repository(application::saved_repository_action aAction);
		void update_saved_repository_actions();
		void update_repository_destination();
		void present_repository_addition(const application::repository_addition_result& aResult);
		void repository_addition_notice(const QString& aText);
		/// \brief Discards the new-repository draft without saving.
		void close_repository_draft();
		/// \brief Sends the draft as one repository-addition gesture.
		void submit_repository_addition();
		// [[BPR-0990]]. Discovery's glyph and sentence, and the open pull
		// request's Follow and Save repository.
		void               update_discovery_controls();
		void               update_pull_request_queue_actions();
		[[nodiscard]] auto open_queue_ref() const -> std::optional<core::pull_request_ref>;
		void               dispatch_queue_preferences(QComboBox* aSource);
		void               build_queue_settings(QWidget* aReviews, QVBoxLayout* aColumn);
		// [[BPR-0783]], [[BPR-1580]]. Home's view: the strip's one View menu over
		// the arrangement and the four surviving typed preferences, and the
		// window's own view.
		void build_home_strip_controls();
		void build_home_filter_menus();
		/// \brief Adds one View submenu, named and explained, and returns it.
		[[nodiscard]] auto add_home_view_submenu(home_view_choice aChoice, const QString& aName, const QString& aPurpose) -> QMenu*;
		/// \brief [[BPR-1580]]. Restores the defaults: the whole view, or only the
		/// three filters that hide rows (My role, Added by, Quiet items).
		void restore_home_view_defaults(bool aWholeView);
		/// \brief Shows the view the window holds, reporting nothing back out.
		void apply_home_view();
		/// \brief Shows what Home is filtered by on the View control.
		///
		/// \details [[BPR-0783]]. Separate from `apply_home_view` because a window
		/// that never filtered follows the profile: an accepted preference arrives
		/// without any view of its own changing, and the control must still say so.
		void apply_home_filter_controls();
		/// \brief Sends the whole view to the host, for a change the reviewer made.
		void dispatch_home_view();
		/// \brief Adopts a published view; equal to what Home shows, it does nothing
		/// at all, so focus, selection and scroll are undisturbed.
		void adopt_home_view(const application::home_view_state& aView);
		/// \brief One Filter gesture: the profile's typed intent, and from now on
		/// this window's own four values.
		void choose_home_filter(
			application::queue_preference_field aField, const application::home_filter& aFilter);
		/// \brief What Home filters by, whoever owns it.
		[[nodiscard]] auto home_filter() const -> application::home_filter;
		void               update_notification_controls();
		void               build_capability_settings();
		void               apply_queue_preferences(
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

		QToolBar*         PrimaryToolbar      = nullptr;
		workbench_strip*  Strip               = nullptr;
		modality_control* Modality            = nullptr;
		QAction*          AccountAction       = nullptr;
		QLabel*           IdentityLabel       = nullptr;
		QLabel*           ChangePositionLabel = nullptr;
		QLabel*           FilePositionLabel   = nullptr;
		QWidget*          StripInset          = nullptr;
		/// [[BPR-1129]]. The review bar, its Updates disclosure ([[BPR-1130]])
		/// and its two commands.
		workbench_strip*    ReviewBar           = nullptr;
		updates_disclosure* UpdatesDisclosure   = nullptr;
		QAction*            UpdatesLineAction   = nullptr;
		QAction*            ComposeReviewAction = nullptr;
		QAction*            CommentAction       = nullptr;
		/// [[BPR-1345]]. The review in progress: its chip, the action that
		/// shows it in the bar, Start a review's sink, and how many of my
		/// saved drafts are pending in it or held by its submission.
		QToolButton*          ReviewChip       = nullptr;
		QAction*              ReviewChipAction = nullptr;
		std::function<bool()> StartReviewDispatch;
		std::size_t           PendingReviewComments = 0U;
		std::size_t           HeldReviewComments    = 0U;
		/// [[BPR-1345]] v002. Finish review's key, the pending row Delete's
		/// sink and the drawer Delete's question.
		QAction*                                                  FinishReviewAction = nullptr;
		std::function<void(core::review_draft_id, std::uint64_t)> PendingCommentDeleteDispatch;
		QPointer<destructive_question>                            DeleteDraftConfirmation;
		/// [[BPR-1374]]. A kept reply's actions, and Discard's question.
		std::function<void(core::review_draft_id, unsent_reply_action)> UnsentReplyDispatch;
		QPointer<destructive_question>                                  DiscardReplyConfirmation;
		void                                                            act_on_kept_reply(card_action aAction, const std::string& aIdentity);
		/// [[BPR-1133]]. Comment's arrow menu.
		QMenu* CommentMenu = nullptr;
		/// [[BPR-1132]]. The document tabs: the strip, where their gestures go,
		/// the tabs the host last published, the tab keys and the older-pair
		/// notice.
		document_tab_strip*                                  DocumentTabs = nullptr;
		document_tab_dispatch                                DocumentTabDispatch;
		std::optional<application::document_tabs_projection> TabsProjection;
		QAction*                                             NextTabAction     = nullptr;
		QAction*                                             PreviousTabAction = nullptr;
		QAction*                                             CloseTabAction    = nullptr;
		QWidget*                                             OlderPairNotice   = nullptr;
		QLabel*                                              OlderPairText     = nullptr;
		QPushButton*                                         ShowLaterThreads  = nullptr;
		/// [[BPR-1129]]. The positions' short form, "3/11", taken when the bar is narrow.
		bool ReviewBarCompact        = false;
		bool ReviewBarPositionsShown = true;
#if defined(_WIN32)
		QToolButton*             StripMenuButton = nullptr;
		windows_caption_buttons* CaptionButtons  = nullptr;
		/// [[BPR-1022]]. Alt is down, and nothing has yet made it a modifier.
		bool StripMenuAltAlone = false;
#endif
		core::workbench_reason_code           LastRefusal = core::workbench_reason_code::none;
		workbench_landing*                    HomeLanding = nullptr;
		std::shared_ptr<dif_catalog_provider> DifCatalog;
		std::unique_ptr<dif_cue_router>       DifCues;
		std::unique_ptr<QElapsedTimer>        DifCueClock;
		dif_widget*                           Dif                = nullptr;
		QWidget*                              DifWorkshopSlot    = nullptr;
		QWidget*                              WorkshopHeader     = nullptr;
		QWidget*                              DifCornerSlot      = nullptr;
		QAction*                              AiToolsAction      = nullptr;
		QScrollArea*                          ConversationScroll = nullptr;
		bool                                  AiToolsSelected    = false;
		core::dif_resolution                  DifResolution;
		bool                                  DifResolutionReady = false;
		QWidget*                              HomePage           = nullptr;
		/// [[BPR-1157]]. The local cycle page and Home's Review Cycles section.
		review_cycle_view*                                    LocalCyclePage  = nullptr;
		bool                                                  LocalCycleShown = false;
		QString                                               LocalCycleTitle;
		QAction*                                              OpenProjectAction = nullptr;
		QWidget*                                              HomeCycles        = nullptr;
		QListWidget*                                          HomeCyclesList    = nullptr;
		std::vector<application::review_cycle_recent>         CycleRecents;
		std::function<void()>                                 OpenProjectDispatch;
		std::function<void(const core::local_cycle_subject&)> ResumeCycleDispatch;
		bool                                                  StripHome     = false;
		bool                                                  HomeRequested = false;
		bool                                                  HomeFocused   = false;
		/// [[BPR-1134]]. The right tray: its tabs, their pages, and the
		/// Comments tab's column, tool and count. ConversationScroll is the
		/// Notes tab's column.
		QTabBar*        TrayTabs             = nullptr;
		QStackedWidget* TrayPages            = nullptr;
		QScrollArea*    CommentsScroll       = nullptr;
		QToolButton*    NextNewCommentButton = nullptr;
		QWidget*        DefectRow            = nullptr;
		QWidget*        CommentsTools        = nullptr;
		QWidget*        TrayCommentsCount    = nullptr;
		/// [[BPR-0775]]. Where focus was in the review when Home covered it.
		QPointer<QWidget>                        FocusBeforeHome;
		QAction*                                 HomeAction           = nullptr;
		QVBoxLayout*                             HomeColumn           = nullptr;
		QHBoxLayout*                             HomeSetupRow         = nullptr;
		QWidget*                                 QueueSurface         = nullptr;
		QVBoxLayout*                             DrawerTopColumn      = nullptr;
		QStackedWidget*                          CentralStack         = nullptr;
		bool                                     ModalityChosen       = false;
		std::size_t                              ReviewThreadTotal    = 0U;
		QAction*                                 IdentityAction       = nullptr;
		QAction*                                 ChangePositionAction = nullptr;
		QAction*                                 FilePositionAction   = nullptr;
		QString                                  IdentityText;
		QString                                  IdentityOwner;
		QString                                  IdentityRepository;
		QString                                  IdentityNumber;
		int                                      IdentityTitleBudget  = std::numeric_limits<int>::max();
		QMenuBar*                                PrimaryMenuBar       = nullptr;
		QMenu*                                   ViewMenu             = nullptr;
		QMenu*                                   ProfilesMenu         = nullptr;
		QAction*                                 NewBprWindowAction   = nullptr;
		QAction*                                 ManageProfilesAction = nullptr;
		QAction*                                 QuitProfilesAction   = nullptr;
		std::map<core::bpr_profile_id, QAction*> ProfileActions;
		core::bpr_profile_id                     CurrentBprProfile;
		application::profile_management_dispatch ProfileManagementDispatch;
		std::function<void()>                    ManageProfiles;
		std::function<void()>                    QuitProfiles;
		/// [[BPR-0777]]. RFC-BPR-0033 B3 reports a profile refusal in the window that
		/// has one, so these carry it in the Profiles menu beside the status line.
		QAction*                                               ProfileStatusAction    = nullptr;
		QAction*                                               ProfileStatusSeparator = nullptr;
		std::optional<application::profile_management_failure> ProfileFailure;

		QAction* LineNumbersAction = nullptr;
		QAction* OnePaneAction     = nullptr;
		/// [[BPR-1336]]. The zoom commands, their sink, and the accepted code record.
		QAction*                                  ZoomInAction     = nullptr;
		QAction*                                  ZoomOutAction    = nullptr;
		QAction*                                  ActualSizeAction = nullptr;
		std::function<void(core::code_zoom_step)> CodeZoomDispatch;
		std::function<void()>                     HeaderSourceDispatch;
		QAction*                                  HeaderSourceAction = nullptr;
		std::array<QAction*, 3>                   SourceFindActions{ };
		std::array<QAction*, 3>                   SourceFilesActions{ };
		source_search_panel*                      SearchPanel    = nullptr;
		QTabWidget*                               FileSearchTabs = nullptr;
		std::function<void(source_find_gesture)>  PrSearchDispatch;
		bool                                      SearchWasCollapsed = false;
		int                                       SearchPreviousTab  = 0;
		void                                      update_source_find_actions();
		std::optional<core::code_font_settings>   CodeZoom;
		void                                      dispatch_code_zoom(core::code_zoom_step aStep);
		void                                      apply_code_zoom();

		std::function<void(std::optional<core::workbench_command_id>)> ReviewNavigationDispatch;
		/// [[BPR-1334]]. View > Line Details.
		QAction* LineDetailsAction = nullptr;
		/// [[BPR-1295]]. Hide Drawers, and the filter by which an editable
		/// multi-line editor keeps its key.
		QAction* ToggleDrawersAction = nullptr;
		QObject* EditorKeyYield      = nullptr;
		/// [[BPR-1536]]. Whether the keyboard was last in a widget that takes
		/// typed text, and the menu-bar keys lifted while it is.
		bool                                                           TypingFocus = false;
		std::vector<std::pair<QPointer<QAction>, QList<QKeySequence>>> LiftedOptionKeys;
		QAction*                                                       ReviewDetailsAction = nullptr;
		QToolButton*                                                   AcceptFileButton    = nullptr;
		/// [[BPR-1578]]. The review bar's progress track and its two steps.
		progress_track*                                              ProgressTrack        = nullptr;
		QToolButton*                                                 SeenStepButton       = nullptr;
		QToolButton*                                                 ReadStepButton       = nullptr;
		QToolButton*                                                 ReviewControlsButton = nullptr;
		QMenu*                                                       ReviewControlsMenu   = nullptr;
		QWidget*                                                     ToolbarSpacer        = nullptr;
		QLabel*                                                      StatusLabel          = nullptr;
		QWidget*                                                     ReviewSurface        = nullptr;
		std::optional<application::github_read_progress>             ReadProgress;
		workbench_central_splitter*                                  CentralSplitter   = nullptr;
		QWidget*                                                     CentreSurface     = nullptr;
		diff_workbench*                                              ComparisonSurface = nullptr;
		pull_request_overview_page*                                  OverviewPage      = nullptr;
		std::optional<application::pull_request_overview_projection> PresentedOverview;
		/// [[BPR-0972]]. The presented overview's first row for each path.
		std::unordered_map<std::string, std::size_t> PresentedOverviewRows;
		bool                                         OverviewFileFactsDirty = true;
		QStackedWidget*                              CentreStack            = nullptr;
		QWidget*                                     ComparisonPage         = nullptr;
		QWidget*                                     DiscussPage            = nullptr;
		/// [[BPR-1293]]. Threads' page and its finder.
		QWidget*             ThreadsPage       = nullptr;
		conversation_finder* Finder            = nullptr;
		QAction*             ShowThreadsAction = nullptr;
		/// The modality Threads was entered from, and where focus was.
		std::optional<review_modality> ThreadsReturn;
		QPointer<QWidget>              ThreadsReturnFocus;
		/// A pick waiting for its file's comparison to show the thread.
		struct conversation_pick
		{
			std::string                                 Thread;
			std::string                                 Post;
			core::file_identity                         File{ };
			std::optional<core::file_comparison_digest> Before;
		};
		std::optional<conversation_pick> PendingPick;
		/// [[BPR-1345]] v002. A pending comment waiting for its file's
		/// comparison.
		struct pending_comment_pick
		{
			core::file_identity                         File{ };
			core::diff_side                             Side = core::diff_side::right;
			std::uint32_t                               Line = 0U;
			std::optional<core::file_comparison_digest> Before;
		};
		std::optional<pending_comment_pick> PendingCommentPick;
		/// [[BPR-1356]]. Discuss's notice says which filters a Threads pick
		/// cleared, until the reviewer changes one or picks again.
		bool              ThreadsRevealNotice = false;
		QVBoxLayout*      ConversationLayout  = nullptr;
		QScrollArea*      ComposeReviewPage   = nullptr;
		QScrollArea*      ComposeInlinePage   = nullptr;
		QWidget*          CompositionPage     = nullptr;
		QPointer<QWidget> CompositionReturnFocus;
		/// [[BPR-1212]]. Discuss's composer column, which shows one of the
		/// two composition pages beside the conversations.
		QWidget* DiscussComposer = nullptr;
		/// [[BPR-1212]]. The draft excerpt above Discuss's inline composer.
		QWidget* DraftExcerpt = nullptr;
		/// [[BPR-1212]]. Where the full composer returns to, and whether the
		/// draft goes back to the drawer, writing, at its caret.
		std::optional<review_modality> CompositionReturnModality;
		bool                           CompositionReturnsToDrawer = false;
		int                            CompositionCaret           = -1;
		/// [[BPR-1212]]. A new comment waiting for the drawer to open, and
		/// whether it is a suggested change.
		std::optional<bool> PendingDraft;
		/// [[BPR-1212]]. A comment on the selected lines waiting for the
		/// draft on its last line: the range's first line, and whether it is
		/// a suggested change.
		struct pending_line_comment
		{
			core::source_line_ref        Anchor;
			std::optional<std::uint32_t> StartLine;
			bool                         Suggestion = false;
		};
		std::optional<pending_line_comment>  PendingLineComment;
		QString                              CommentDraftUnavailable;
		std::optional<core::source_line_ref> WritingAnchor;
		QAction*                             AddCommentAction    = nullptr;
		QAction*                             SuggestChangeAction = nullptr;
		QPointer<QWidget>                    DiscussReturnFocus;
		/// [[BPR-1089]]. Where focus was on the Overview when a count opened
		/// Discuss, so going back to the Overview finds the same cell.
		QPointer<QWidget> OverviewReturnFocus;

		epoch_comparison_control* EpochSelectors = nullptr;

		std::array<navigation_entry, 4> Navigation{ };
		/// [[BPR-1532]]. First and Last change; menu and keys only, no
		/// review-bar button.
		std::array<navigation_entry, 2> ChangeEnds{ };
		QAction*                        NextNewCommentAction = nullptr;
		conversation_read_dispatch      ConversationReadDispatch;
		std::array<QAction*, 4>         ReceiptActions{ };
		std::array<drawer_entry, 3>     Drawers{ };
		files_tree_view*                FilesTree                = nullptr;
		QComboBox*                      FilesOrderSelector       = nullptr;
		QLineEdit*                      FilesFilter              = nullptr;
		filter_empty_notice*            FilesFilterNotice        = nullptr;
		repositories_tree_view*         RepositoriesTree         = nullptr;
		QLineEdit*                      RepositoriesFilter       = nullptr;
		filter_empty_notice*            RepositoriesFilterNotice = nullptr;
		std::vector<server_card*>       ServerCards;
		QVBoxLayout*                    ServerCardColumn = nullptr;
		QWidget*                        RulesEditor      = nullptr;
		/// Set when a pull request was chosen from the repositories tree, so
		/// it opens on its overview when it arrives.
		bool                       OpenOnOverview    = false;
		github_conversation_panel* ConversationPanel = nullptr;
		/// Prevents the drawer refilter from clearing the anchor that triggered it.
		bool                                   ApplyingConversationAnchor       = false;
		bool                                   LeftConversationEndpointCurrent  = false;
		bool                                   RightConversationEndpointCurrent = false;
		agent_launch_panel*                    AgentPanel                       = nullptr;
		private_notes_panel*                   PrivateNotes                     = nullptr;
		review_tour_editor*                    TourEditor                       = nullptr;
		QAction*                               PrepareTourAction                = nullptr;
		QAction*                               AddTourStopAction                = nullptr;
		private_review_panel*                  PrivateReview                    = nullptr;
		side_chat_panel*                       SideChat                         = nullptr;
		finding_queue_view*                    FindingQueue                     = nullptr;
		review_draft_editor*                   ReviewEditor                     = nullptr;
		defect_card_view*                      DefectCard                       = nullptr;
		QLabel*                                DefectCountLabel                 = nullptr;
		QAction*                               ClassifyDefect                   = nullptr;
		QAction*                               PreviousDefect                   = nullptr;
		QAction*                               NextDefect                       = nullptr;
		std::vector<core::defect_history>      Defects;
		std::map<core::defect_id, std::string> DefectThreads;
		std::optional<std::string>             PendingDefectDialog;
		QDialog*                               DefectDialog         = nullptr;
		QToolButton*                           PreviousDefectButton = nullptr;
		QToolButton*                           NextDefectButton     = nullptr;
		std::optional<std::size_t>             ShownDefectIndex;
		review_landing_model*                  QueueLanding     = nullptr;
		review_landing_filter_model*           QueueFilterModel = nullptr;
		qt::home_list*                         HomeList         = nullptr;
		// [[BPR-0783]]. This window's Home view and the strip controls over it.
		application::home_view_state                      HomeView;
		std::function<void(application::home_view_state)> HomeViewDispatch;
		QToolButton*                                      HomeViewButton = nullptr;
		QMenu*                                            HomeViewMenu   = nullptr;
		QAction*                                          HomeViewItem   = nullptr;
		/// [[BPR-1580]]. The View menu's submenus, by `home_view_choice`.
		std::array<QMenu*, 5>   HomeViewSubmenus{ };
		std::array<QAction*, 4> HomeGroupActions{ };
		QAction*                HomeStateHeaders   = nullptr;
		QAction*                HomeViewReset      = nullptr;
		QAction*                HomeFilterSettings = nullptr;
		/// The four filter choices: My role, Added by, Quiet items and Sort
		/// within a group, each holding every typed value of its vocabulary.
		std::array<std::vector<QAction*>, 4> HomeFilterSections{ };
		/// Each filter value's short name, which its submenu's own item shows.
		std::map<const QAction*, QString> HomeFilterShortNames;
		/// Set while the window is showing a view it was given, so adopting one
		/// is never reported back out as the reviewer's own gesture.
		bool                                               ApplyingHomeView      = false;
		QComboBox*                                         QueueRoleFilter       = nullptr;
		QComboBox*                                         QueueObserverFilter   = nullptr;
		QComboBox*                                         QueueBucketFilter     = nullptr;
		QComboBox*                                         QueueRepositoryFilter = nullptr;
		QComboBox*                                         QueueQuietFilter      = nullptr;
		QComboBox*                                         QueueSort             = nullptr;
		QCheckBox*                                         NotificationsEnabled  = nullptr;
		QCheckBox*                                         NotificationsPaused   = nullptr;
		QPushButton*                                       NotificationPreview   = nullptr;
		QPushButton*                                       NotificationReset     = nullptr;
		QPushButton*                                       QueueDefaultsReset    = nullptr;
		QLabel*                                            NotificationStatus    = nullptr;
		QLineEdit*                                         StorageLocation       = nullptr;
		std::function<bool(const core::pull_request_ref&)> NotificationPreviewDispatch;
		QComboBox*                                         QueueRuleSelector         = nullptr;
		QComboBox*                                         QueueRuleKind             = nullptr;
		QLineEdit*                                         QueueRulePattern          = nullptr;
		QCheckBox*                                         QueueRuleExcluding        = nullptr;
		QAction*                                           AddQueueRule              = nullptr;
		QAction*                                           RemoveQueueRule           = nullptr;
		QToolButton*                                       AddQueueRuleButton        = nullptr;
		QToolButton*                                       RemoveQueueRuleButton     = nullptr;
		QLabel*                                            QueueStatus               = nullptr;
		QLabel*                                            HomeDiscoveryStatus       = nullptr;
		QWidget*                                           HomeRepositoryRow         = nullptr;
		QToolButton*                                       HomeAddRepositoryButton   = nullptr;
		QWidget*                                           HomeOnboarding            = nullptr;
		QPushButton*                                       HomeOnboardingButton      = nullptr;
		QAction*                                           QueueRefreshAction        = nullptr;
		QMenu*                                             PullRequestMenu           = nullptr;
		QAction*                                           FollowPullRequest         = nullptr;
		QAction*                                           SavePullRequestRepository = nullptr;
		workbench_queue_follow_dispatch                    QueueFollowDispatch;
		workbench_queue_discovery_dispatch                 QueueDiscoveryDispatch;
		std::function<std::optional<std::int64_t>()>       HomeClock;
		// The open pull request as the queue names it: owner/name, number and
		// title, and whether it was read signed out.
		std::string  OpenQueueRepository;
		std::int64_t OpenQueueNumber = 0;
		std::string  OpenQueueTitle;
		bool         OpenQueuePublicPreview = false;
		QLabel*      SettingsQueueStatus    = nullptr;
		QLineEdit*   HomeOpenBar            = nullptr;
		QLabel*      HomeOpenLabel          = nullptr;
		QWidget*     HomeOpenFrame          = nullptr;
		/// [[BPR-1381]]. The bar's row, where the way back to a review stands.
		QHBoxLayout*                                     HomeOpenRow        = nullptr;
		QLabel*                                          HomeOpenShortcut   = nullptr;
		QWidget*                                         HomeOpenActivity   = nullptr;
		QAction*                                         HomeFocusBarAction = nullptr;
		QAction*                                         HomeOpenAction     = nullptr;
		QLabel*                                          HomeOpenNotice     = nullptr;
		QPushButton*                                     HomeOpenDetails    = nullptr;
		qt::home_status_line*                            HomeStatus         = nullptr;
		QLabel*                                          HomeStatusText     = nullptr;
		QPushButton*                                     HomeOpenCancel     = nullptr;
		QPushButton*                                     HomeOpenRetry      = nullptr;
		std::optional<application::home_open_projection> HomeOpen;
		application::home_open_dispatch                  HomeOpenDispatch;
		/// Whether a shown Home row carries the opening, so the band does not.
		bool                                                         HomeOpenOnRow = false;
		QString                                                      LastHomeOpenAnnouncement;
		QString                                                      LastHomeStatusAnnouncement;
		home_refusal*                                                HomeRefusal = nullptr;
		QString                                                      HomeSetupNotice;
		QString                                                      HomeRestorationNotice;
		QString                                                      HomeRefusalDetails;
		QString                                                      DismissedHomeRefusal;
		std::vector<core::observer_rule>                             QueueRules;
		application::queue_landing_projection                        QueueProjection;
		workbench_saved_repository_dispatch                          SavedRepositoryDispatch;
		std::function<void(application::repository_addition_intent)> RepositoryAdditionDispatch;
		QWidget*                                                     RepositoryDraft          = nullptr;
		QPushButton*                                                 RepositoryCancelButton   = nullptr;
		QLabel*                                                      RepositoriesConfirmation = nullptr;
		QLabel*                                                      RepositoryDestination    = nullptr;
		QLabel*                                                      RepositoryAdditionStatus = nullptr;
		QPushButton*                                                 RepositoryAddButton      = nullptr;
		QPushButton*                                                 RepositoryPrepareButton  = nullptr;
		application::queue_session_id                                RepositoryDisplayedDestination;
		std::uint64_t                                                RepositoryGesture        = 0U;
		std::uint64_t                                                RepositoryPendingGesture = 0U;
		bool                                                         RepositorySaving         = false;
		bool                                                         RepositoryPreparing      = false;
		bool                                                         RepositoryDraftOpen      = false;
		workbench_saved_server_dispatch                              SavedServerDispatch;
		workbench_review_reply_dispatch                              ReviewReplyDispatch;
		/// [[BPR-1046]]. A Reply pressed while its thread's comparison was
		/// loading; the next comparison to arrive completes or refuses it.
		std::optional<std::string>           DeferredReply;
		bool                                 DeferredReplyInPlace = false;
		workbench_thread_resolution_dispatch ThreadResolutionDispatch;
		bool                                 ProviderResolvesThreads = false;
		core::github_capabilities            Capabilities;
		// [[BPR-0661]].
		review_behaviour_dispatch                 BehaviourDispatch;
		application::review_behaviour_settings    Behaviour;
		QWidget*                                  BehaviourEditor = nullptr;
		std::array<std::vector<QToolButton*>, 10> BehaviourOptions{ };
		feedback_mark_preview*                    FeedbackMarksPreview = nullptr;
		/// [[BPR-1088]]. The rest of Comment labels: the helper's switch, the
		/// Overview chips, what a load left out of them, and Mark colours.
		QCheckBox*                     LabelHelperSwitch    = nullptr;
		QSpinBox*                      CenterRibbonWidth    = nullptr;
		feedback_label_columns_editor* LabelColumnsEditor   = nullptr;
		QLabel*                        LabelColumnsRecovery = nullptr;
		QToolButton*                   MarkColoursLink      = nullptr;
		/// [[BPR-1566]]. Comment labels' first line and its outcome.
		QWidget*     ConventionalCommentsRow     = nullptr;
		QLabel*      ConventionalCommentsCaption = nullptr;
		QToolButton* ConventionalCommentsLink    = nullptr;
		QLabel*      ConventionalCommentsStatus  = nullptr;
		/// [[BPR-1088]]. The record last asked of the host and not yet
		/// answered by a publication, so a gesture that returns to the
		/// published record while an earlier one is in flight is still sent.
		std::optional<application::review_behaviour_settings> AskedBehaviour;
		/// [[BPR-1083]]. The label tints of the theme on screen, once the
		/// composition supplies them; the mark style is `Behaviour`'s.
		std::optional<feedback_label_tints> LabelTints;
		/// [[BPR-1085]]. The theme's attention amber, once the composition
		/// supplies it.
		std::optional<QColor> AttentionColour;
		/// [[BPR-1331]] v003. The theme's progress mark colours, once the
		/// composition supplies them.
		std::optional<progress_mark_colours> ProgressMarkColours;
		bool                                 ApplyingBehaviour = false;
		std::optional<review_modality>       LastUsedModality;
		bool                                 ReturnComparisonPending = false;
		/// Set once the host has published the preferences; until then a pull
		/// request that opens takes the defaults and is revisited when they land.
		bool                                                                     BehaviourPublished    = false;
		bool                                                                     FirstFilterApplied    = false;
		bool                                                                     ReturnComparisonAsked = false;
		std::optional<core::file_receipt_command>                                MarkedReadWhenSeen;
		QWidget*                                                                 SavedRepositoriesEditor      = nullptr;
		QLabel*                                                                  SavedRepositoriesDestination = nullptr;
		QListWidget*                                                             SavedRepositoryList          = nullptr;
		QWidget*                                                                 SavedRepositoriesEmpty       = nullptr;
		QLineEdit*                                                               SavedRepositoryName          = nullptr;
		QAction*                                                                 AddSavedRepository           = nullptr;
		QAction*                                                                 RemoveSavedRepository        = nullptr;
		QPushButton*                                                             AddSavedRepositoryButton     = nullptr;
		QPushButton*                                                             RemoveSavedRepositoryButton  = nullptr;
		QWidget*                                                                 EnterpriseServerDraft        = nullptr;
		QLineEdit*                                                               EnterpriseServerHostname     = nullptr;
		QLineEdit*                                                               EnterpriseServerAccount      = nullptr;
		QLabel*                                                                  EnterpriseServerRefusal      = nullptr;
		QPushButton*                                                             EnterpriseServerAdd          = nullptr;
		QPushButton*                                                             EnterpriseServerCancel       = nullptr;
		QPushButton*                                                             AddEnterpriseServerButton    = nullptr;
		QPushButton*                                                             UseGithubCliButton           = nullptr;
		QLabel*                                                                  GithubCliDiscoveryStatus     = nullptr;
		github_cli_discovery_dispatch                                            GithubCliDiscoveryDispatch;
		github_cli_discovery_dispatch                                            GithubCliCheckDispatch;
		std::map<std::string, std::vector<server_card_cli_account>, std::less<>> CliAccountsByHost;
		github_cli_discovery_state                                               GithubCliDiscoveryState = github_cli_discovery_state::idle;
		QPointer<QMessageBox>                                                    ForgetServerConfirmation;
		bool                                                                     EnterpriseDraftOpen = false;
		/// [[BPR-0773]]. The hostname a device flow was last asked for; its
		/// terminal sentence is said only on that host's card.
		std::optional<std::string>                         DeviceFlowHost;
		github_session_panel*                              SessionPanel = nullptr;
		device_authorization_panel*                        DevicePanel  = nullptr;
		application::device_authorization_gesture_dispatch DeviceDispatch;
		application::public_preview_gesture_dispatch       PublicPreviewDispatch;
		github_pull_request_panel*                         PullRequestPanel   = nullptr;
		appearance_settings_panel*                         AppearanceSettings = nullptr;
		QPointer<settings_window>                          MountedSettings;
		writing_hooks_panel*                               WritingHooks  = nullptr;
		claude_wrapper_panel*                              ClaudeWrapper = nullptr;
		std::function<void(settings_page)>                 SettingsPresenter;
		settings_window*                                   SettingsWindow = nullptr;
		QWidget*                                           AccountsPanel  = nullptr;
		QPointer<QWidget>                                  AccountsReturnParent;
		QDialog*                                           SignInDialog = nullptr;
		QScrollArea*                                       SignInScroll = nullptr;
		std::string                                        SignInHostname;
		bool                                               SignInCliOnly = false;
		std::vector<core::host_profile_id>                 SignInCliProfiles;
		std::function<void()>                              SignInCancelDispatch;
		QAction*                                           SettingsAction          = nullptr;
		QAction*                                           KeyboardShortcutsAction = nullptr;
		keyboard_shortcuts_window*                         KeyboardShortcuts       = nullptr;
		QStackedWidget*                                    PullRequestStack        = nullptr;
		QLabel*                                            EmptyStateTitleLabel    = nullptr;
		QLabel*                                            EmptyStateBodyLabel     = nullptr;
		QString                                            LastGithubAnnouncement;
		QAction*                                           GithubConnectAction = nullptr;
		QAction*                                           GithubOpenAction    = nullptr;
		QAction*                                           GithubRefreshAction = nullptr;
		application::setup_destination_dispatch            SetupDestinationDispatch;
		std::optional<core::setup_request_id>              SetupDestinationRequest;
		bool                                               SetupDestinationNeedsHome = false;
		QPushButton*                                       SetupContinueButton       = nullptr;
		QPushButton*                                       SetupCancelButton         = nullptr;
		bool                                               ApplyingSetupProjection   = false;
		std::vector<application::github_profile_option>    GithubProfiles;
		application::github_intent_dispatch                GithubDispatch;
		application::public_preview_presentation           PublicPreview;
		std::uint64_t                                      PublicPreviewNow       = 0;
		bool                                               PublicPreviewSignedOut = false;
		bool                                               HasGithubPullRequest   = false;
		bool                                               HasCachedOverview      = false;
		/// [[BPR-1200]]. The read in progress has shown its overview, so this
		/// connection can read what it names.
		bool LoadingOverviewShown = false;
		/// [[BPR-1200]]. The first read the core last refused; cleared by the next read.
		std::optional<core::pull_request_refusal> PullRequestRefusal;

		std::function<void(core::diagnostic_export_policy)> DiagnosticExporter;

		std::function<core::markdown_link_result(const core::markdown_link_intent&)> MarkdownDispatch;
		std::function<void(const core::markdown_image_intent&)>                      MarkdownImageDispatch;
		std::function<void(std::vector<application::markdown_code_syntax_request>)>  MarkdownCodeDispatch;
		std::string                                                                  MarkdownAuthority;
		/// [[BPR-1338]]. Open on GitHub and what it names.
		std::function<core::markdown_link_result(const application::pull_request_page_intent&)> PageDispatch;
		std::function<core::markdown_link_result(core::reference_page)>                         ReferenceDispatch;
		std::optional<core::markdown_context>                                                   PageContext;
		QAction*                                                                                OpenOnGithubAction       = nullptr;
		QToolButton*                                                                            OpenOnGithubButton       = nullptr;
		QAction*                                                                                OpenOnGithubButtonAction = nullptr;
		QMenu*                                                                                  HomeRowMenu              = nullptr;
		/// [[BPR-1342]]. Open Review and Open Review in New Window, in Home's row menu.
		QAction*                                                      HomeOpenReviewAction       = nullptr;
		QAction*                                                      HomeOpenReviewWindowAction = nullptr;
		std::function<bool(const application::review_window_intent&)> ReviewWindowDispatch;
		/// Which of Home's lists last took the keyboard. Its current row is
		/// Home's one selection, and the row Home's actions name.
		home_selection_list HomeSelection = home_selection_list::queue;

		/// [[BPR-0668]]. Whether the comparison gave the drawer any files.
		bool                          FileRowsPresent          = false;
		bool                          ApplyingQueuePreferences = false;
		core::workbench_command_delta Commands;
		core::workbench_drawer_delta  DrawerStates;
		/// [[BPR-1295]]. What showing the drawers would open, as published.
		core::workbench_drawer_restore_delta DrawerRestore;
		core::workbench_line_numbers_delta   LineNumberState;
		/// [[BPR-1130]]. The fold the core decided; folded until it says otherwise.
		core::workbench_updates_line_delta                    UpdatesLineState;
		core::workbench_one_pane_delta                        OnePaneState;
		std::optional<application::workbench_layout_delta>    LayoutWidths;
		std::optional<core::workbench_navigation_cursor>      Cursor;
		std::optional<application::workbench_request_context> LastWorkbenchContext;
		/// Visual scope during a same-session observation gap; never a selection.
		std::optional<core::file_identity>                         RefreshConversationAnchor;
		std::optional<application::workbench_request_context>      RefreshConversationContext;
		QTimer*                                                    ConversationAnchorTimer = nullptr;
		std::optional<application::workbench_host_failure>         Failure;
		std::optional<application::file_receipt_action_projection> Receipt;
		std::optional<application::receipt_failure_publication>    ReceiptFailure;
		bool                                                       LedgerUnavailable          = false;
		bool                                                       SavedLayoutUnavailable     = false;
		bool                                                       AgentWorkspacesUnavailable = false;
		bool                                                       HasCommands                = false;
		bool                                                       ApplyingLayout             = false;
		bool                                                       ApplyingFileSelection      = false;
		bool                                                       QueueReady                 = false;
		/// [[BPR-0776]]. Whether the window has an account to list a queue for.
		/// Until the session decides, the queue is shown, as its surface always was.
		bool HomeAccount = true;
		/// [[BPR-0776]]. Whether the queue status says something Home's panels do not.
		bool QueueStatusNotable = true;
		// Set by the first published review-history update. Before one arrives
		// the shell has no review-history state to state, and quoting the
		// control's idle wording would report activity that never happened.
		bool                        HasEpochState = false;
		QString                     LastAnnouncement;
		workbench_window_statistics Statistics;
		// [[BPR-0733]]. Home's recently viewed section and the list it was
		// given. The entries are the window's only record of what it shows:
		// a row's index into them is what leaves through the dispatch.
		QListWidget*                                           HomeRecentList         = nullptr;
		QAction*                                               HomeRecentResumeAction = nullptr;
		QAction*                                               HomeRecentRemoveAction = nullptr;
		std::function<void(application::recent_review_row)>    RecentRemovalDispatch;
		application::recent_reviews_projection                 RecentReviews;
		std::function<void(application::recent_review_intent)> RecentReviewDispatch;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
