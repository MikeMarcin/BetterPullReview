#ifndef BPR_APP_GITHUB_APPLICATION_WINDOW_PROBE_STATES_HPP
#define BPR_APP_GITHUB_APPLICATION_WINDOW_PROBE_STATES_HPP
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/diff_transition.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QString>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"
	#include <QScopeGuard>
#endif

// Test-build state the composition courses keep on a window while they drive
// it through its public path (github_application_fixture_access).
namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	struct appearance_theme_probe_state
	{
		enum class phase {
			awaiting_comparison,
			awaiting_stable_comparison,
			awaiting_manual_preview,
			awaiting_preview_adoption,
			awaiting_preview_restoration,
			awaiting_first_preview,
			awaiting_second_preview,
			awaiting_store,
			awaiting_stored_preview,
			awaiting_stored_preview_adoption,
			awaiting_stored_preview_restoration,
			awaiting_equal_stored_preview,
			awaiting_equal_stored_commit,
			awaiting_export,
			awaiting_export_refusal,
			passed,
			failed,
		};

		phase Phase = phase::awaiting_comparison;
		/// \brief [[BPR-1037]]. The current phase's bounded wait.
		detail::fixture_probe_wait       Wait;
		std::string                      Path;
		application::appearance_settings BeforeSettings;
		core::appearance_palette         BeforePalette{ };
		core::appearance_palette         ManualCandidate{ };
		core::appearance_palette         Candidate{ };
		core::appearance_palette         StoredCandidate{ };
		core::appearance_observation     NativeObservation;
		/// \brief The theme the probe committed its palette to.
		///
		/// \details Captured at the commit so the assertions afterwards can
		/// name it. That is the point of the phases that follow: the
		/// observation is flipped to another variant and the committed theme's
		/// colours must still be exactly where they were put.
		core::appearance_variant CustomisedVariant
			= core::appearance_variant::light;
		std::shared_ptr<const core::computed_file_diff> Result;
		std::optional<core::diff_request_context>       Context;
		std::optional<core::semantic_selection>         Selection;
		application::shared_token_spans                 LeftSpans;
		application::shared_token_spans                 RightSpans;
		qt::diff_workbench_statistics                   Statistics;
		double                                          Position                  = 0.0;
		std::uint64_t                                   ProfileGeneration         = 0U;
		std::size_t                                     ClassificationSubmissions = 0U;
		std::size_t                                     PersistedSettings         = 0U;
	};

	struct workbench_composition_probe_state
	{
		enum class phase {
			awaiting_observation,
			awaiting_file_projection,
			awaiting_diff,
			awaiting_change_projection,
			awaiting_wrap_projection,
			awaiting_files_drawer,
			awaiting_width_store,
			awaiting_invalidation,
			awaiting_failure,
			passed,
			failed,
		};

		phase                                         Phase = phase::awaiting_observation;
		detail::workbench_composition_probe_evidence  Evidence;
		application::workbench_pull_request_authority Authority;
		std::vector<core::workbench_file_id>          ExpectedFiles;
		std::optional<core::semantic_selection>       Selection;
		qt::workbench_window_statistics               WindowStatistics;
		core::request_generation                      Generation;
		std::size_t                                   IntentDispatches         = 0U;
		std::size_t                                   ChangeDispatches         = 0U;
		std::size_t                                   SelectedDiffBegins       = 0U;
		std::size_t                                   SelectedDiffPublications = 0U;
		std::size_t                                   SelectedDiffFailures     = 0U;
		std::size_t                                   WidthDispatches          = 0U;
		std::size_t                                   Invalidations            = 0U;
		std::size_t                                   PersistedLayouts         = 0U;
		/// \brief [[BPR-0864]], [[BPR-1037]]. The current phase's bounded wait.
		detail::fixture_probe_wait Wait;
		std::uint32_t              InitialFilesWidth = 0U;
		/// \brief [[BPR-0860]]. The files width the handle drag asked for.
		///
		/// \details Kept so a stalled store can say what it asked for beside
		/// what the splitter and the host actually hold.
		int RequestedFilesWidth = 0;
		/// \brief [[BPR-0863]]. The drag needs Review's files drawer.
		detail::course_modality_step Modality{
			.Modality = qt::review_modality::review,
			.Needs    = { .FilesDrawer = true },
		};
	};

	struct uiforetw_selected_diff_probe_state
	{
		enum class phase {
			awaiting_observation,
			awaiting_review_modality,
			awaiting_resource_diff,
			awaiting_support_diff,
			awaiting_resource_return,
			awaiting_rc_diff,
			awaiting_focused_layout,
			passed,
			failed,
		};

		phase                                         Phase = phase::awaiting_observation;
		detail::uiforetw_selected_diff_probe_evidence Evidence;
		qt::workbench_window_statistics               WindowStatistics;
		std::size_t                                   SelectedDiffPublications = 0U;
		std::size_t                                   SelectedDiffFailures     = 0U;
		std::size_t                                   IntentDispatches         = 0U;
		std::size_t                                   ChangeDispatches         = 0U;
		std::optional<core::semantic_selection>       ChangeOrigin;
		/// \brief [[BPR-0859]]. Ticks of the waits this probe bounds by
		/// count and names itself: the panes, the role paint, the change
		/// walk and the focused layout.
		std::size_t Waits = 0U;
		/// \brief [[BPR-1036]]. The current phase's bounded wait for an
		/// effect: the observation, a selection's publication and its blobs.
		detail::fixture_probe_wait Wait;
		/// \brief [[BPR-0859]]. Whether the reveal this phase waited for landed.
		///
		/// \details Kept so the wait for the product's own scroll and the
		/// wait for the paint that follows it are separate bounds, rather
		/// than one counter each tick resets for the other. A reset that
		/// runs on every tick of a phase makes the next cap in that phase
		/// unreachable, which is why both role-paint caps below were dead.
		std::size_t RevealWaits      = 0U;
		bool        ResourceRevealed = false;
		bool        RcRevealed       = false;
		/// \brief [[BPR-0859]], [[BPR-0863]]. The reveal and the paint it
		/// measures need Review's comparison.
		detail::course_modality_step Modality{
			.Modality = qt::review_modality::review,
			.Needs    = { .Comparison = true },
		};
	};

	struct native_appearance_selected_diff_probe_state
	{
		enum class phase {
			awaiting_observation,
			awaiting_selection_projection,
			awaiting_diff,
			ready,
			refused,
		};

		phase                                                  Phase = phase::awaiting_observation;
		detail::native_appearance_selected_diff_probe_evidence Evidence;
		application::workbench_pull_request_authority          Authority;
		core::workbench_file_id                                File;
		qt::workbench_window_statistics                        WindowStatistics;
		std::size_t                                            IntentDispatches         = 0U;
		std::size_t                                            SelectedDiffBegins       = 0U;
		std::size_t                                            SelectedDiffPublications = 0U;
		std::size_t                                            SelectedDiffFailures     = 0U;
	};

	struct review_ledger_composition_probe_state
	{
		enum class phase {
			awaiting_observation,
			awaiting_hidden_diff,
			awaiting_diff,
			awaiting_gestures,
			awaiting_private_records,
			awaiting_actionability,
			awaiting_material_actionability,
			passed,
			failed,
		};

		phase                        Phase = phase::awaiting_observation;
		detail::course_modality_step Overview{ .Modality = qt::review_modality::overview };
		detail::course_modality_step Review{ .Modality = qt::review_modality::review,
											 .Needs    = { .Comparison  = true,
														   .FilesDrawer = true } };
		/// [[BPR-1211]]. The seed course's gestures, one per quiescent
		/// ledger, and the private writes it then makes.
		std::size_t                                       Gesture     = 0U;
		bool                                              SeenOnEntry = false;
		application::review_ledger_host_statistics        GesturesBefore;
		std::optional<application::local_note_record>     AcceptedNote;
		std::optional<application::durable_defect_record> PrivateDefect;
		std::size_t                                       PrivateAnswers  = 0U;
		bool                                              PrivateAccepted = true;
		/// \brief [[BPR-1037]]. The current phase's bounded wait.
		detail::fixture_probe_wait              Wait;
		detail::review_ledger_composition_probe Course
			= detail::review_ledger_composition_probe::seed;
		detail::review_ledger_composition_probe_evidence Evidence;
		application::review_ledger_host_statistics       InitialStatistics;
		core::file_identity                              File;
		core::file_material_id                           Material;
		// [[BPR-1331]]. The comparison the probe's note and defect attach
		// to; the receipt attaches to `Material`.
		core::file_comparison_digest Comparison;
		core::file_material_id       ReopenedMaterial;
	};

	struct review_round_trip_composition_probe_state
	{
		enum class phase {
			awaiting_editor,
			awaiting_edited_body,
			awaiting_fallback_draft,
			awaiting_fallback_body,
			awaiting_stale_refusal,
			awaiting_lost_effect,
			awaiting_deferred_terminal,
			awaiting_retry,
			awaiting_native_conversation,
			awaiting_fallback_conversation,
			passed,
			failed,
		};

		detail::review_round_trip_composition_probe_evidence Evidence;
		std::uint64_t                                        PriorDraftRevision = 0;
		std::size_t                                          DraftIntents       = 0;
		phase                                                Phase              = phase::awaiting_editor;
		/// \brief [[BPR-1037]]. The current phase's bounded wait.
		detail::fixture_probe_wait           Wait;
		bool                                 NativeLineChosen = false;
		std::optional<core::source_line_ref> OffHunkAnchor;
		/// \brief [[BPR-1277]]. The same diff asked for again while the
		/// fallback draft is written, and the counts before it was.
		bool                                      SameDiffAskedAgain   = false;
		std::size_t                               SameDiffBegins       = 0U;
		std::size_t                               SameDiffPublications = 0U;
		std::optional<core::semantic_selection>   PublishingSelection;
		std::optional<core::semantic_selection>   DeferredSelection;
		std::optional<core::review_comment_draft> PublishingDraft;
	};

	// [[BPR-0657]].
	struct threaded_reply_probe_state
	{
		enum class phase {
			awaiting_ready,
			awaiting_line_comparison,
			awaiting_line_draft,
			awaiting_line_body,
			awaiting_line_discarded,
			awaiting_reply_draft,
			awaiting_reply_body,
			awaiting_reply_published,
			awaiting_reply_seen,
			awaiting_resolved,
			// [[BPR-1741]].
			awaiting_reopened,
			awaiting_reactions_read,
			awaiting_reaction_confirmed,
			awaiting_reaction_removed,
			awaiting_picker_reaction,
			awaiting_reaction_reconciled,
			// [[BPR-1774]].
			awaiting_description_offered,
			awaiting_description_saved,
			awaiting_description_conflict,
			// [[BPR-1775]].
			awaiting_comment_edit_offered,
			awaiting_comment_edit_saved,
			awaiting_comment_edit_conflict,
			// [[BPR-1804]].
			awaiting_reviewer_offered,
			awaiting_reviewer_listed,
			awaiting_reviewer_filtered,
			awaiting_reviewer_requested,
			// [[BPR-1849]].
			awaiting_reviewer_refusal_listed,
			awaiting_reviewer_refused,
			// [[BPR-1852]].
			awaiting_draft_offered,
			awaiting_draft_ready,
			awaiting_draft_refused,
			passed,
			failed,
		};
		detail::threaded_reply_probe_evidence Evidence;
		phase                                 Phase = phase::awaiting_ready;
		std::string                           Diagnostic;
		std::optional<std::uint32_t>          ThreadlessLine;
		int                                   ReadsBeforeRefresh = 0;
		/// [[BPR-1841]]. The counters as the Resolve press found them.
		struct resolve_baseline
		{
			int           Reads          = 0;
			std::size_t   Resolutions    = 0U;
			std::size_t   ModelResets    = 0U;
			std::size_t   RowsChanged    = 0U;
			std::uint64_t FinderRebuilds = 0U;
			std::uint64_t CardLayouts    = 0U;
			std::uint64_t OverviewBuilds = 0U;
			std::size_t   OverviewMoves  = 0U;
			std::size_t   Unresolved     = 0U;
		};
		std::optional<std::size_t>      UnresolvedWhenResolved;
		std::optional<resolve_baseline> ResolveBaseline;
		/// [[BPR-1849]]. The counters and the time as the reviewer choice
		/// found them.
		struct reviewer_baseline
		{
			int                                   Reads          = 0;
			std::uint64_t                         OverviewBuilds = 0U;
			std::size_t                           OverviewMoves  = 0U;
			std::chrono::steady_clock::time_point Chosen;
		};
		std::optional<reviewer_baseline> ReviewerBaseline;
		/// [[BPR-1852]]. The counters and the time as Ready for review
		/// found them.
		std::optional<reviewer_baseline> DraftBaseline;
		std::optional<std::uint64_t>     DraftQuietBuilds;
		std::size_t                      DraftQuietTicks = 0U;
		/// [[BPR-1775]]. The thread's first post was given to another
		/// account, so the course has someone else's comment to refuse.
		bool CommentRootRestaged = false;
	};

	// [[BPR-1083]]. One fixture file opened in Review, for an attended look
	// and for the label-authoring course.
	struct fixture_file_reveal_state
	{
		enum class phase {
			awaiting_files,
			awaiting_diff,
			shown,
			failed,
		};
		phase       Phase = phase::awaiting_files;
		std::string Path;
	};

	// [[BPR-1083]]. The label-authoring course.
	struct label_authoring_probe_state
	{
		enum class phase {
			awaiting_review_empty,
			awaiting_file,
			awaiting_marks,
			awaiting_line_draft,
			awaiting_literal_code,
			awaiting_chip,
			awaiting_shortcut,
			awaiting_published,
			awaiting_refreshed,
			// [[BPR-1084]]. Suggested changes in the same composition.
			awaiting_patch_draft,
			awaiting_patch_body,
			awaiting_patch_published,
			awaiting_prose_draft,
			awaiting_prose_body,
			awaiting_prose_published,
			awaiting_refused_draft,
			awaiting_refused_body,
			awaiting_refused,
			awaiting_reply_open,
			awaiting_reply_draft,
			awaiting_reply_body,
			// [[BPR-1086]]. Private tracking and a tour stop beside the labels.
			awaiting_defect_offer,
			awaiting_defect_tracked,
			awaiting_relabel_refreshed,
			awaiting_resolve_refreshed,
			awaiting_tour_refreshed,
			// [[BPR-1212]]. The keyboard line cursor and Return in the code.
			awaiting_left_refused,
			awaiting_keyboard_range,
			awaiting_range_prefilled,
			awaiting_range_published,
			// [[BPR-1721]]. A dragged selection, a right-click inside
			// it and Add comment, as a mouse delivers them to the window.
			awaiting_mouse_range,
			awaiting_mouse_range_opened,
			awaiting_mouse_range_published,
			// [[BPR-1212]] v002. A comment on a base line, as `side: LEFT`.
			awaiting_base_numbering,
			// [[BPR-1570]]. A refresh that keeps the file comes first.
			awaiting_kept_refresh,
			awaiting_base_line,
			awaiting_base_published,
			awaiting_single_line,
			awaiting_no_line,
			passed,
			failed,
		};
		detail::label_authoring_probe_evidence Evidence;
		/// [[BPR-1212]]. The range's first line and the body its suggestion
		/// is published with, and whether the course is only the
		/// line-draft phases (`--line-draft-course`). [[RFC-BPR-0090]]:
		/// Shift+Return opens the lines for writing in the head pane and
		/// the drawer prefills nothing (owner, Q5), so the course types
		/// its change there, once (`RangeTyped`).
		std::optional<std::uint32_t> RangeStart;
		std::string                  Prefill;
		bool                         RangeTyped = false;
		/// [[BPR-1721]]. The dragged range's first line, and whether
		/// the drag has been made and the menu's item chosen.
		std::optional<std::uint32_t> MouseRangeStart;
		bool                         MouseDragged  = false;
		std::size_t                  Waited        = 0U;
		bool                         LineDraftOnly = false;
		/// Whether this phase's key went to the pane; the drawer draft
		/// may open a turn later, once the drawer shows.
		bool KeySent = false;
		/// [[BPR-1086]]. The tracked defect as the gesture left it, the
		/// fixture's review submissions and posts before it, and the bodies
		/// the fixture's website gives the issue root and the tour stop.
		std::optional<core::defect_record> Tracked;
		std::size_t                        SubmissionsBefore = 0U;
		std::string                        EditedRoot;
		std::string                        TourStop;
		/// [[BPR-1084]]. The line the current root comment is on, and the
		/// fixture's native post count before it was published.
		std::optional<core::source_line_ref> Anchor;
		std::size_t                          NativeBefore = 0U;
		/// The free line chosen for the next root comment, chosen once.
		std::optional<std::uint32_t> Line;
		/// [[BPR-1570]]. The workbench authority the shown file had when
		/// the course asked for a refresh.
		std::optional<application::workbench_pull_request_authority> RefreshedFrom;
		phase                                                        Phase = phase::awaiting_review_empty;
		std::string                                                  Diagnostic;
		std::size_t                                                  Renders = 0U;
		/// Characters still to type, one a tick once the draft has the
		/// text on screen, as a person's keystrokes arrive.
		QString Typing;
	};

	// [[BPR-1326]]. The published-comment file-switch course.
	struct published_file_switch_probe_state
	{
		enum class phase {
			awaiting_file,
			awaiting_line_draft,
			awaiting_body,
			awaiting_published,
			awaiting_other_file,
			awaiting_return,
			awaiting_settled,
			awaiting_refreshed,
			passed,
			failed,
		};
		detail::published_file_switch_probe_evidence Evidence;
		/// The line the comment is on, and the fixture's native post
		/// count before it was written.
		std::optional<core::source_line_ref> Anchor;
		std::size_t                          NativeBefore = 0U;
		/// Ticks spent waiting for the returned file's notes to settle.
		std::size_t Waited = 0U;
		phase       Phase  = phase::awaiting_file;
		std::string Diagnostic;
	};

	// [[BPR-1374]]. The unsent-reply course.
	struct reply_draft_probe_state
	{
		enum class phase {
			awaiting_file,
			awaiting_reply_open,
			awaiting_reply_draft,
			awaiting_reply_saved,
			awaiting_refreshed,
			awaiting_restored,
			awaiting_reopened,
			awaiting_typed,
			awaiting_copied,
			awaiting_discarded,
			awaiting_kept,
			/// [[BPR-1745]]. The thread reads outdated after the push.
			awaiting_outdated,
			/// [[BPR-1745]]. The reply's Publish is answered.
			awaiting_published,
			passed,
			failed,
		};
		detail::reply_draft_probe_evidence                Evidence;
		std::optional<core::review_draft_id>              Draft;
		detail::review_mutation_fixture_evidence          Before;
		std::size_t                                       Waited = 0U;
		phase                                             Phase  = phase::awaiting_file;
		std::string                                       Diagnostic;
		std::optional<QScopeGuard<std::function<void()>>> PrivateEffectsHold;
		bool                                              HeldDiscard = false;
	};

	// [[BPR-1478]]. The course in which a reread lands just after the
	// reviewer picks another file.
	struct refresh_pick_probe_state
	{
		enum class phase {
			awaiting_file,
			awaiting_line,
			awaiting_read,
			awaiting_landed,
			awaiting_settled,
			passed,
			failed,
		};
		detail::refresh_pick_probe_evidence       Evidence;
		std::optional<core::provider_snapshot_id> ShownBefore;
		std::optional<std::uint32_t>              Line;
		std::size_t                               Waited = 0U;
		phase                                     Phase  = phase::awaiting_file;
		std::string                               Diagnostic;
	};

	// [[BPR-1862]]. The draft-delete course.
	struct draft_delete_probe_state
	{
		enum class phase {
			awaiting_file,
			awaiting_lines,
			awaiting_line,
			awaiting_saved,
			awaiting_question,
			awaiting_deleted,
			awaiting_stayed,
			awaiting_again_saved,
			awaiting_review_line,
			awaiting_review_saved,
			awaiting_review_pending,
			awaiting_review_finish,
			awaiting_review_deleted,
			passed,
			failed,
		};
		detail::draft_delete_probe_evidence  Evidence;
		std::optional<std::uint32_t>         ReviewLine;
		std::optional<std::string>           ReviewKey;
		std::optional<std::uint32_t>         Line;
		std::optional<core::review_draft_id> Draft;
		std::optional<core::review_draft_id> Again;
		std::optional<std::string>           AgentKey;
		bool                                 AgentAsked = false;
		std::size_t                          Waited     = 0U;
		phase                                Phase      = phase::awaiting_file;
		std::string                          Diagnostic;
	};

	// [[BPR-1345]]. The start-a-review course.
	struct start_review_probe_state
	{
		enum class phase {
			awaiting_file,
			awaiting_first_line,
			awaiting_first_text,
			awaiting_first_offer,
			awaiting_first_pending,
			awaiting_second_line,
			awaiting_second_text,
			awaiting_second_offer,
			awaiting_second_pending,
			awaiting_third_line,
			awaiting_third_text,
			awaiting_third_published,
			awaiting_other_file,
			awaiting_return,
			awaiting_restored,
			awaiting_finish_page,
			awaiting_submitted,
			// [[BPR-1475]]. The reread a submission makes by itself, then a
			// note held open across a reread.
			awaiting_publication_reread,
			awaiting_note_line,
			awaiting_note_text,
			awaiting_note_reread,
			// [[BPR-1472]]. Home, then the pull request's Recently viewed row.
			awaiting_recent_open,
			awaiting_recent_read,
			awaiting_refused,
			awaiting_discarded,
			awaiting_held,
			/// [[BPR-1541]].
			awaiting_replaced,
			awaiting_restored_unknown,
			awaiting_restored_replaced,
			awaiting_reconciled,
			// [[BPR-1345]] v002, `again`.
			awaiting_old_line,
			awaiting_old_text,
			awaiting_old_kept,
			awaiting_finish_again,
			awaiting_row_revealed,
			awaiting_row_deleted,
			awaiting_review_discarded,
			awaiting_compose_page,
			awaiting_compose_written,
			awaiting_compose_discarded,
			awaiting_old_again,
			awaiting_again_pending,
			awaiting_again_page,
			awaiting_again_submitted,
			passed,
			failed,
		};
		detail::start_review_probe_evidence Evidence;
		/// The lines written on, in order, and the third, published one.
		std::vector<std::uint32_t>   Lines;
		std::optional<std::uint32_t> Third;
		/// [[BPR-1345]] v002. The older draft, kept out of the review.
		std::optional<std::uint32_t>         Old;
		std::optional<core::review_draft_id> OldDraft;
		bool                                 RowRevealed = false;
		/// [[BPR-1833]]. The multi-step course chooses its finish return once.
		bool FinishReturnChosen = false;
		/// The drafts the review carries.
		std::vector<core::review_draft_id> Drafts;
		std::size_t                        NativeBefore = 0U;
		std::size_t                        Waited       = 0U;
		phase                              Phase        = phase::awaiting_file;
		std::string                        Diagnostic;
		/// [[BPR-1472]]. The reads and the shown read before the reopen.
		int                                       ReadsBefore = 0;
		std::optional<core::provider_snapshot_id> ShownBefore;
		/// [[BPR-1475]]. The reviewer's place before the submission.
		std::optional<core::semantic_selection> SelectionBefore;
	};

	// [[BPR-1331]]. The private progress course across a push.
	struct review_progress_push_probe_state
	{
		std::size_t File     = 0U;
		bool        Revealed = false;
		bool        Acted    = false;
		std::size_t Waited   = 0U;
		bool        Passed   = false;
		bool        Failed   = false;
		std::string Marks;
		std::string Diagnostic;
	};

	struct defect_composition_probe_state
	{
		enum class phase {
			awaiting_ready,
			awaiting_diff,
			awaiting_classified,
			awaiting_reverse_right,
			awaiting_epoch_change,
			awaiting_verified,
			awaiting_reclassified,
			awaiting_shared_ready,
			passed,
			failed,
		};
		detail::defect_composition_probe_evidence Evidence;
		detail::defect_composition_probe          Course
			= detail::defect_composition_probe::seed;
		phase Phase = phase::awaiting_ready;
		/// \brief [[BPR-1037]]. The current phase's bounded wait.
		detail::fixture_probe_wait   Wait;
		detail::course_modality_step Modality{ .Modality = qt::review_modality::review, .Needs = { .Comparison = true, .FilesDrawer = true } };
		bool                         CommentsRequested = false;
		core::file_comparison_digest BeforeComparison;
		int                          ReverseLeftRow              = -1;
		int                          ReverseRightRow             = -1;
		bool                         RefuseCorrelatedPersistence = false;
		std::size_t                  RefusedPersistenceAttempts  = 0U;
	};
#endif
}
#endif
