#ifndef BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP
#define BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP

#if !defined(BPR_BUILD_TESTING)
	#error "The GitHub application fixture access is test-build only"
#endif

#include "bpr/app/github_application.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::app::detail {
	enum class github_public_path_fixture {
		github_com,
		ghes_3_18,
		/// google/UIforETW#74, recorded once and replayed. Served on github.com
		/// like `github_com`, and unlike it serves a real pull request with a
		/// single recorded head rather than a synthetic two-head graph.
		uiforetw_74,
	};

	/// \brief Reports whether the named fixture serves a second head.
	///
	/// \details The epoch seed and reuse phases, and every epoch maintenance
	/// probe, compare a second head against a first. A fixture that records one
	/// head cannot perform a weaker version of those runs - it cannot perform
	/// them at all. Answered from the serving descriptor rather than from a list
	/// kept beside the caller, so a fixture added later answers for itself.
	[[nodiscard]] auto github_public_path_fixture_serves_second_head(
		github_public_path_fixture aFixture) noexcept -> bool;

	enum class github_epoch_fixture_phase {
		single,
		seed,
		reuse,
	};

	struct github_public_path_fixture_options
	{
		std::optional<std::filesystem::path> ApplicationDataRoot;
		github_epoch_fixture_phase           EpochPhase                       = github_epoch_fixture_phase::single;
		bool                                 DisableWorkbenchSelectionWire    = false;
		bool                                 DisableReviewLedgerGestureWire   = false;
		bool                                 LoseFirstReviewMutationResult    = false;
		bool                                 UseGrantedNotificationSink       = false;
		bool                                 CaptureSystemNotificationDisplay = false;
		bool                                 DisableNotificationRequestWire   = false;
		bool                                 DisableNotificationSelectionWire = false;
		bool                                 DisableAgentHostComposition      = false;
		bool                                 DisableAgentGestureWire          = false;
		std::optional<std::filesystem::path> ExternalAgentExecutable;
		std::optional<std::filesystem::path> GitExecutable;
	};

	struct queue_notification_fixture_evidence
	{
		std::size_t                                        RequestsAttempted                  = 0U;
		std::size_t                                        Submitted                          = 0U;
		std::size_t                                        DisplaySubmissions                 = 0U;
		std::size_t                                        SuppressedUnattributableActivation = 0U;
		std::size_t                                        Activations                        = 0U;
		std::size_t                                        Selections                         = 0U;
		std::size_t                                        SelectionFailures                  = 0U;
		bool                                               QueueQuiescent                     = false;
		std::optional<application::delivered_notification> LastSubmitted;

		auto operator<=>(const queue_notification_fixture_evidence&) const = default;
	};

	struct review_mutation_fixture_evidence
	{
		std::size_t TargetObservations = 0U;
		std::size_t Submissions        = 0U;
		std::size_t StoredComments     = 0U;
		bool        LostResult         = false;
		bool        DuplicateReturned  = false;

		auto operator<=>(const review_mutation_fixture_evidence&) const = default;
	};

	struct github_public_path_fixture_evidence
	{
		bool        RecordingConsumed          = false;
		bool        CapabilityProbeConsumed    = false;
		bool        BootstrapTransportSelected = false;
		bool        AccountTransportSelected   = false;
		bool        CredentialStored           = false;
		bool        CredentialRetrieved        = false;
		bool        CredentialDeleted          = false;
		bool        SecretRetained             = false;
		int         RecordingFailure           = -1;
		int         AccountReadCount           = 0;
		int         LastReadOperation          = -1;
		int         SmartAcquireCount          = 0;
		int         SmartDiscoveryCount        = 0;
		int         SmartLsRefsCount           = 0;
		int         SmartFetchCount            = 0;
		std::string BaseOid;
		std::string HeadOneOid;
		std::string HeadTwoOid;
		std::string BaseBlobOid;
		std::string HeadOneBlobOid;
		std::string HeadTwoBlobOid;
	};

	struct github_epoch_maintenance_fixture_evidence
	{
		std::size_t  PruneIntentDispatchCount          = 0U;
		std::size_t  DeleteIntentDispatchCount         = 0U;
		std::size_t  CancelIntentDispatchCount         = 0U;
		std::int64_t CatalogRevision                   = 0;
		bool         TargetBaseRefPresent              = false;
		bool         TargetHeadRefPresent              = false;
		bool         NonTargetBaseRefPresent           = false;
		bool         NonTargetHeadRefPresent           = false;
		bool         RepositoryPresent                 = false;
		bool         CacheDeletionMarkerPresent        = false;
		bool         TargetBaseCacheDeleted            = false;
		bool         TargetHeadCacheDeleted            = false;
		bool         RetiredRepositoryPresent          = false;
		bool         RetiredCacheDeletionMarkerPresent = false;
		int          TargetBaseAvailability            = -1;
		int          TargetHeadAvailability            = -1;
	};

	enum class appearance_theme_customization_probe_result {
		pending,
		passed,
		failed,
	};

	enum class workbench_composition_probe_result {
		pending,
		passed,
		failed,
	};

	enum class uiforetw_selected_diff_probe_result {
		pending,
		passed,
		failed,
	};

	enum class native_appearance_selected_diff_probe_result {
		pending,
		ready,
		refused,
	};

	/// \brief Names the exact condition currently blocking the native course.
	enum class native_appearance_selected_diff_reason {
		none,
		awaiting_workbench_observation,
		awaiting_selection_projection,
		awaiting_selected_diff_publication,
		awaiting_diff_presentation,
		composition_unavailable,
		fixture_authority_mismatch,
		fixture_material_mismatch,
		file_projection_mismatch,
		selection_wire_unavailable,
		selected_diff_failed,
		selected_diff_mismatch,
		fixture_diff_helper_invoked,
	};

	enum class review_ledger_composition_probe {
		seed,
		reopen,
		unavailable,
	};

	enum class review_ledger_composition_probe_result {
		pending,
		passed,
		failed,
	};

	enum class review_round_trip_composition_probe_result {
		pending,
		passed,
		failed,
	};

	enum class defect_composition_probe {
		seed,
		reopen,
	};

	enum class defect_composition_probe_result {
		pending,
		passed,
		failed,
	};

	struct defect_composition_probe_evidence
	{
		bool ExactThreads          = false;
		bool LocalClassification   = false;
		bool CorrelatedEvidence    = false;
		bool ExplicitVerification  = false;
		bool DurableHistory        = false;
		bool MultiDefectNavigation = false;
		bool SharedActionSeparate  = false;
		bool RestartRestored       = false;
		bool ZeroReopenWrites      = false;
		bool VisibleProjection     = false;

		auto operator<=>(const defect_composition_probe_evidence&) const = default;
	};

	struct review_round_trip_composition_probe_evidence
	{
		bool ExactDesignatedTarget = false;
		bool EditorComposed        = false;
		bool SemanticEdit          = false;
		bool StaleHeadRefused      = false;
		bool LostResultRefreshed   = false;
		bool SelectionDeferred     = false;
		bool LatestSelectionWins   = false;
		bool PendingCleared        = false;
		bool DeferredDraftExact    = false;
		bool StaleTerminalWithheld = false;
		bool RetryIdempotent       = false;
		bool CapabilityFallback    = false;
		bool ServerAuthority       = false;
		bool TypedQtProjection     = false;

		auto operator<=>(const review_round_trip_composition_probe_evidence&) const
			= default;
	};

	struct dormant_live_provider_selection_evidence
	{
		bool        ExactTupleAccepted         = false;
		std::size_t MismatchedTuplesRejected   = 0U;
		std::size_t ExpectedMismatchRejections = 0U;
		bool        FixtureProviderSelected    = false;
		bool        LiveProviderAbsent         = false;

		auto operator<=>(const dormant_live_provider_selection_evidence&) const
			= default;
	};

	struct agent_invocation_composition_evidence
	{
		bool                                          HostComposed = false;
		bool                                          Quiescent    = false;
		application::agent_invocation_host_statistics Statistics;
		std::filesystem::path                         WorkspaceRoot;

		auto operator<=>(const agent_invocation_composition_evidence&) const
			= default;
	};

	/// \brief Raw accepted authorities used to falsify the emitted agent context.
	struct agent_invocation_context_evidence
	{
		core::application_session_id                        ApplicationSession;
		core::pull_request_authority                        Authority;
		core::agent_session_id                              NextSession;
		core::agent_revision_binding                        Binding;
		std::string                                         PullRequestUrl;
		std::optional<core::git_commit_oid>                 PreviousUpload;
		std::vector<application::agent_file_context_input>  Files;
		application::private_ledger_observation             Private;
		std::optional<core::supplement_semantic_context>    Selection;
		std::optional<application::agent_draft_observation> CurrentDraft;

		auto operator<=>(const agent_invocation_context_evidence&) const = default;
	};

	struct agent_session_catalog_evidence
	{
		application::agent_session_catalog_state   Catalog;
		application::review_ledger_host_statistics Statistics;

		auto operator<=>(const agent_session_catalog_evidence&) const = default;
	};

	/// \brief Records the independently observable receipt-composition path.
	struct review_ledger_composition_probe_evidence
	{
		bool ExactAuthority                = false;
		bool HostComposed                  = false;
		bool RealFileGesture               = false;
		bool RealSelectedDiff              = false;
		bool ExactMaterial                 = false;
		bool ExpectedPreGesturePersistence = false;
		bool ReceiptLoadedOnEntry          = false;
		bool GestureSequence               = false;
		bool DurableReceipt                = false;
		bool ActionabilityProjection       = false;
		bool PrivateRecords                = false;
		bool PublicPathAvailable           = false;
		bool UnavailableVisible            = false;
		bool HostOnlyProbe                 = false;

		auto operator<=>(const review_ledger_composition_probe_evidence&) const
			= default;
	};

	/// \brief Records independently falsifiable workbench-composition wires.
	struct workbench_composition_probe_evidence
	{
		bool ExactAuthority          = false;
		bool BoundedObservation      = false;
		bool IntentDispatch          = false;
		bool ProjectionDelivery      = false;
		bool FileGesture             = false;
		bool SelectedDiffRequest     = false;
		bool ExactComparison         = false;
		bool ChangeDispatch          = false;
		bool DrawerWidthDispatch     = false;
		bool SessionSettingsFallback = false;
		bool MismatchPreserved       = false;
		bool InvalidationSuperseded  = false;
		bool FailureDelivery         = false;
		bool FixtureDiffHelperUnused = false;

		auto operator<=>(const workbench_composition_probe_evidence&) const
			= default;
	};

	/// \brief Records the real recorded-file selection and intraline path.
	struct uiforetw_selected_diff_probe_evidence
	{
		bool ExactCommitPair           = false;
		bool ResourceGesture           = false;
		bool ResourceExactBlobs        = false;
		bool ResourceIntralinePair     = false;
		bool ResourceRolePaint         = false;
		bool ResourcePublication       = false;
		bool RcGesture                 = false;
		bool RcExactBlobs              = false;
		bool RcIntralinePairs          = false;
		bool RcRolePaint               = false;
		bool RcPublication             = false;
		bool PreviousFileNavigation    = false;
		bool NextFileNavigation        = false;
		bool PreviousChangeNavigation  = false;
		bool NextChangeNavigation      = false;
		bool FilesDrawerToggle         = false;
		bool ConversationsDrawerToggle = false;
		bool FocusedReviewLayout       = false;
		bool FixtureDiffHelperUnused   = false;

		auto operator<=>(const uiforetw_selected_diff_probe_evidence&) const
			= default;
	};

	/// \brief Records the exact selected material required by native appearance.
	struct native_appearance_selected_diff_probe_evidence
	{
		native_appearance_selected_diff_reason Reason
			= native_appearance_selected_diff_reason::awaiting_workbench_observation;
		bool ExactAuthority          = false;
		bool ExactRevisions          = false;
		bool SelectionGesture        = false;
		bool SelectionIntent         = false;
		bool ExactFile               = false;
		bool ExactBlobs              = false;
		bool ExactOptions            = false;
		bool PresentationReady       = false;
		bool FixtureDiffHelperUnused = false;

		auto operator<=>(
			const native_appearance_selected_diff_probe_evidence&) const
			= default;
	};

	class github_application_fixture_access final
	{
	public:
		/// \brief Attempts one native HTTPS read through the shipped adapter.
		///
		/// \details The endpoint returns an ordinary HTTP response outside the
		/// automated-test boundary. The isolation contract requires the attempt to
		/// fail earlier as a typed network error from the production application
		/// binary.
		[[nodiscard]] static auto network_isolation_probe() -> bool;
		[[nodiscard]] static auto make(
			github_public_path_fixture         aFixture,
			github_public_path_fixture_options aOptions = { })
			-> std::unique_ptr<github_application>;
		[[nodiscard]] static auto evidence(
			const github_application& aApplication) noexcept
			-> github_public_path_fixture_evidence;
		[[nodiscard]] static auto review_mutation_evidence(
			const github_application& aApplication) noexcept
			-> review_mutation_fixture_evidence;
		/// \brief Reports the composed queue-to-platform notification evidence.
		[[nodiscard]] static auto queue_notification_evidence(
			const github_application& aApplication) noexcept
			-> queue_notification_fixture_evidence;
		/// \brief Emits one activation through the legacy fixture-only sink.
		[[nodiscard]] static auto activate_fixture_notification(
			github_application& aApplication) -> bool;
		[[nodiscard]] static auto maintenance_evidence(
			const github_application& aApplication,
			core::review_epoch_id     aTarget) noexcept
			-> github_epoch_maintenance_fixture_evidence;
		/// \brief Requests the deterministic fixture file through the real host/cache path.
		[[nodiscard]] static auto request_fixture_diff(
			github_application& aApplication,
			bool                aUseSecondHead = false) -> bool;
		/// \brief Replays an accepted metadata-only or changed-head fixture handoff.
		[[nodiscard]] static auto reaccept_fixture_revision(
			github_application& aApplication,
			bool                aChangeHead) -> bool;
		/// \brief Reports what the composed review ledger already holds.
		///
		/// \details Read before anything is written, so a second run against the
		/// same application data root can tell persistence from a fresh write.
		[[nodiscard]] static auto review_ledger_receipt_present(
			github_application& aApplication) -> bool;
		/// \brief Selects a different appearance mode through the composed host.
		///
		/// \details Exists so a probe can measure what a theme change costs.
		/// The token-span cache is keyed by blob identity and definition
		/// version alone, so this must repaint and must not reparse; the
		/// submission count in the application status is what says which
		/// happened.
		///
		/// \returns Whether the composed host applied a different mode.
		[[nodiscard]] static auto select_other_appearance_mode(
			github_application& aApplication) -> bool;
		/// \brief Selects one exact appearance mode through the composed host.
		///
		/// \details BPR-0114 needs an explicit mode selected while the
		/// operating system reports the opposite scheme, which is the only way
		/// to show that an explicit mode ignores the observation rather than
		/// happening to agree with it.
		[[nodiscard]] static auto select_appearance_mode(
			github_application&   aApplication,
			core::appearance_mode aMode) -> bool;
		/// \brief Drives the composed settings surface as a reviewer would.
		///
		/// \details Goes through the widgets rather than through the host, so
		/// what it proves is that a gesture reaches the record. Calling the
		/// host directly would prove only that the host works, which its own
		/// suite already establishes and which was true while no person could
		/// reach it.
		///
		/// \returns Whether the host adopted both the mode and the tab width.
		[[nodiscard]] static auto apply_appearance_gesture(
			github_application&   aApplication,
			core::appearance_mode aMode,
			std::uint32_t         aTabWidth) -> bool;
		/// \brief Offers an illegal family through the composed surface.
		///
		/// \returns Whether the record was refused whole, the surface reported
		/// a typed reason, and the previous value stayed in effect.
		[[nodiscard]] static auto refuse_appearance_gesture(
			github_application& aApplication) -> bool;
		/// \brief Advances the real selected-diff theme import/preview/commit course.
		///
		/// \details Repeated calls poll the host-owned import effect. The course
		/// proves preview and cancel write nothing, commit writes once, and colour
		/// presentation preserves comparison, selection, spans, classification,
		/// and measured geometry.
		[[nodiscard]] static auto exercise_appearance_theme_customization(
			github_application& aApplication,
			std::string_view    aThemePath)
			-> appearance_theme_customization_probe_result;
		/// \brief Drives the real window through the composed workbench host.
		///
		/// \details File selection traverses the shipped drawer model, semantic
		/// workbench intent, retained epoch cache actor, and real diff host. The
		/// change action, drawer observation, projections, invalidation, and failure
		/// presentation traverse the same shipped window.
		[[nodiscard]] static auto exercise_workbench_composition(
			github_application& aApplication)
			-> workbench_composition_probe_result;
		[[nodiscard]] static auto workbench_composition_evidence(
			const github_application& aApplication) noexcept
			-> workbench_composition_probe_evidence;
		/// \brief Selects the recorded UIforETW files through the shipped drawer.
		[[nodiscard]] static auto exercise_uiforetw_selected_diff(
			github_application& aApplication)
			-> uiforetw_selected_diff_probe_result;
		[[nodiscard]] static auto uiforetw_selected_diff_evidence(
			const github_application& aApplication) noexcept
			-> uiforetw_selected_diff_probe_evidence;
		/// \brief Awaits the native course's exact file-row selected diff.
		///
		/// \details Repeated calls drive the shipped drawer selection through the
		/// semantic workbench host, retained epoch actor, and diff composition. A
		/// pending result is normal while those effects progress; a refusal names
		/// the exact failed precondition.
		[[nodiscard]] static auto exercise_native_appearance_selected_diff(
			github_application& aApplication)
			-> native_appearance_selected_diff_probe_result;
		[[nodiscard]] static auto native_appearance_selected_diff_evidence(
			const github_application& aApplication) noexcept
			-> native_appearance_selected_diff_probe_evidence;
		/// \brief Persists criterion-specific pixels after the semantic course passed.
		[[nodiscard]] static auto capture_uiforetw_selected_diff_evidence(
			github_application&   aApplication,
			std::string_view      aDirectory,
			core::appearance_mode aMode) -> bool;
		/// \brief Drives one process of the real selected-diff ledger contract.
		[[nodiscard]] static auto exercise_review_ledger_composition(
			github_application&             aApplication,
			review_ledger_composition_probe aCourse)
			-> review_ledger_composition_probe_result;
		[[nodiscard]] static auto review_ledger_composition_evidence(
			const github_application& aApplication) noexcept
			-> review_ledger_composition_probe_evidence;
		[[nodiscard]] static auto exercise_review_round_trip_composition(
			github_application& aApplication)
			-> review_round_trip_composition_probe_result;
		[[nodiscard]] static auto review_round_trip_composition_evidence(
			const github_application& aApplication) noexcept
			-> review_round_trip_composition_probe_evidence;
		[[nodiscard]] static auto dormant_live_provider_selection_evidence_for(
			const github_application& aApplication)
			-> dormant_live_provider_selection_evidence;
		[[nodiscard]] static auto review_mutation_provider_marker(
			const github_application& aApplication) noexcept -> std::string_view;
		[[nodiscard]] static auto exercise_defect_composition(
			github_application&      aApplication,
			defect_composition_probe aCourse) -> defect_composition_probe_result;
		[[nodiscard]] static auto defect_composition_evidence(
			const github_application& aApplication) noexcept
			-> defect_composition_probe_evidence;
		[[nodiscard]] static auto agent_invocation_evidence(
			const github_application& aApplication) noexcept
			-> agent_invocation_composition_evidence;
		/// \brief Seeds unselected private prose and a last-reviewed head.
		[[nodiscard]] static auto seed_agent_private_context_canary(
			github_application& aApplication,
			std::string_view    aSelectedNote,
			std::string_view    aUnselectedNote) -> bool;
		/// \brief Reads raw accepted context independently of agent serialization.
		[[nodiscard]] static auto agent_invocation_context(
			github_application& aApplication)
			-> std::optional<agent_invocation_context_evidence>;
		[[nodiscard]] static auto agent_session_catalog(
			github_application& aApplication)
			-> std::optional<agent_session_catalog_evidence>;
		/// \brief Commits a receipt through the composed ledger and reads it back.
		///
		/// \details The read goes through the same store the application
		/// composed, so it reaches the database file the application actually
		/// opened rather than one the probe chose.
		[[nodiscard]] static auto review_ledger_round_trip(
			github_application& aApplication) -> bool;
	};
}

#endif // BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP
