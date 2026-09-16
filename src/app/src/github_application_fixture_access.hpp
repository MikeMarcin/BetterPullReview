#ifndef BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP
#define BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP

#if !defined(BPR_BUILD_TESTING)
	#error "The GitHub application fixture access is test-build only"
#endif

#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/application/agent_chat_session_host.hpp"
#include "bpr/application/agent_comment_workshop_host.hpp"
#include "bpr/application/agent_finding_host.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/mcp_context_helper_supervisor.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::qt {
	class device_authorization_panel;
	class finding_queue_view;
	class side_chat_panel;
} // namespace bpr::qt

namespace bpr::app::detail {
	enum class github_public_path_fixture {
		github_com,
		ghes_3_18,
		/// google/UIforETW#74, recorded once and replayed. Served on github.com
		/// like `github_com`, and unlike it serves a real pull request with a
		/// single recorded head rather than a synthetic two-head graph.
		uiforetw_74,
		/// Synthetic 515-file, 1,000-thread latest-head workload.
		large_review_v1,
		/// Synthetic two-file sibling-head graph for independent window restoration.
		profile_window_restoration_v1,
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

	struct review_submission_fixture_state;
	struct github_fixture_vault_state;
	/// \brief Owns runtime-only fake vault material across fully reconstructed applications.
	[[nodiscard]] auto make_shared_github_fixture_vault() -> std::shared_ptr<github_fixture_vault_state>;
	/// \brief Transfers only the synthetic device fixture slots through a bounded parent-memory channel.
	/// \details Canonical lowercase hex wraps Qt_6_8 big-endian data: seven
	/// uint64 context/authority fields, then two uint32-length slots (128 bytes
	/// maximum each). The entire wire is capped at 2048 bytes. Import accepts
	/// only the initial documented fixture grant, GitHub.com and Default profile;
	/// no refresh rotation, production-vault access or product switch is added.
	[[nodiscard]] auto export_github_fixture_vault(const std::shared_ptr<github_fixture_vault_state>& aState) -> std::string;
	/// \brief Recreates the exact synthetic authority and slots with fresh per-process effect counters.
	[[nodiscard]] auto import_github_fixture_vault(std::string_view aBytes) -> std::shared_ptr<github_fixture_vault_state>;

	struct github_public_path_fixture_options
	{
		std::optional<std::filesystem::path>             ApplicationDataRoot;
		std::shared_ptr<qt::dif_catalog_provider>        DifCatalog;
		/// [[BPR-0777]]. Drives the composition's empty-desktop policy from a course,
		/// so both the resident and the quit outcome are observable on one machine.
		/// Unset selects the running platform's own policy.
		empty_desktop_policy                             EmptyDesktopPolicy;
		core::diff_compute_limits                        DiffComputeLimits;
		bool                                             LargeReviewHistory = false;
		github_epoch_fixture_phase                       EpochPhase         = github_epoch_fixture_phase::single;
		std::string                                      TimelineCourse;
		bool                                             EmptyHistoricalTimeline        = false;
		bool                                             UnfetchableFirstHistoricalHead = false;
		bool                                             DisableWorkbenchSelectionWire  = false;
		bool                                             DisableReviewLedgerGestureWire = false;
		std::shared_ptr<review_submission_fixture_state> ReviewSubmissionServer;
		bool                                             LoseFirstReviewMutationResult          = false;
		bool                                             UseGrantedNotificationSink             = false;
		bool                                             CaptureSystemNotificationDisplay       = false;
		bool                                             DisableNotificationRequestWire         = false;
		bool                                             DisableNotificationSelectionWire       = false;
		bool                                             DisableAgentProcessLauncherComposition = false;
		bool                                             DisableAgentHostComposition            = false;
		bool                                             DisableAgentGestureWire                = false;
		// RFC-BPR-0012 construction cuts. One per composed surface, each
		// removing exactly one construction and nothing else, so a course can
		// show that the gesture still fires and its effect is gone.
		bool                                 DisableAgentFindingHostComposition = false;
		bool                                 DisableChatSessionHostComposition  = false;
		bool                                 DisableCommentWorkshopComposition  = false;
		bool                                 DisableMcpHelperComposition        = false;
		bool                                 DisableSideChatPanelComposition    = false;
		bool                                 DisableFindingQueueComposition     = false;
		std::optional<std::filesystem::path> ExternalAgentExecutable;
		/// Points the Codex JSONL profile at a fixture executable. That
		/// protocol is the one with a result frame a finding payload can ride
		/// on; the external terminal has none and refuses one by design.
		std::optional<std::filesystem::path> CodexAgentExecutable;
		std::optional<std::filesystem::path> GitExecutable;
		// RFC-BPR-0016. Composes a registered device capability against the
		// recorded BPR-0276 grant so the enrollment reaches the real vault.
		// Only meaningful with a fixture; a shipped build has no identity to
		// register and this option does not exist there.
		/// Starts the GHES fixture behind real server onboarding instead of a configured profile.
		bool SetupUnconfiguredEnterprise = false;

		bool RegisteredDeviceCourse = false;
		// RFC-BPR-0016 renewal. The same registered grant, plus the two things
		// a rotation needs that the read-path fixture has no use for: a real
		// second vault slot and the documented renewal answer on the
		// transport. Only meaningful with a fixture.
		bool DeviceRefreshCourse   = false;
		bool PersistDeviceBindings = false;
		/// [[BPR-0657]]. Lets a course refresh the pull request: the recorded
		/// read script is replayed for each session the product establishes,
		/// with the fixture's replies and resolutions overlaid.
		bool RepeatReadSessions = false;
		/// [[BPR-0657]]. Whether the fixture's mutation provider resolves
		/// threads. Off, the composed editor reports the degradation, which
		/// the round-trip course pins; on, the threaded-reply course resolves.
		bool                                        ResolvesThreads         = false;
		std::int64_t                                DeviceWallOffsetSeconds = 0;
		std::shared_ptr<github_fixture_vault_state> SharedDeviceVault;
		// RFC-BPR-0016 construction cut. Composes the device host with no
		// browser capability bound at all, which is what every build was
		// before [[BPR-0516]]. Removes exactly one construction: the flow
		// still starts, the address is still presented, and the open action
		// answers `unavailable` rather than doing nothing.
		bool SuppressDeviceBrowser = false;
		// [[BPR-0540]]. Puts the documented refresh shape in the refresh slot
		// before this process has a session at all, which is the state a past
		// grant's orderly shutdown used to leave behind: the access token
		// deleted, the material keyed beside it still there. Seeded rather
		// than earned, because the run that earned it is gone -- that is the
		// whole condition. Only meaningful beside `DeviceRefreshCourse`,
		// which is what composes a second slot to seed.
		bool SeedOrphanedRefreshMaterial = false;
		// [[BPR-0526]] construction cut. The window still builds and mounts the
		// device sign-in panel -- placement is the window's decision, not the
		// composition's -- but the composition adopts neither end of it: no
		// gesture sink is set and no presentation is forwarded. The panel is on
		// screen and inert, the recording sink still reads every publication,
		// and the flow stays reachable through the self-test, so a graph that
		// silently stopped composing the panel cannot pass for one that did.
		bool DisableDeviceAuthorizationPanelComposition = false;
		// RFC-BPR-0017. Forces the shipped public-preview selection available
		// against the recorded fixture, so the composed credential-free
		// authority can be driven through the real host and the real
		// admission rules. Only meaningful with a fixture; a shipped build has
		// no such override and this option does not exist there.
		bool PublicPreviewEnabledCourse = false;
		// The same override, activated on the enterprise profile instead. The
		// refusal there is the core policy's rather than the composition's,
		// which is a different claim and needs a different course.
		bool PublicPreviewGhesCourse = false;
		// [[BPR-0316]]. Forces the shipped public-preview selection available
		// with no fixture at all. The two options above override the selection
		// against recorded traffic; this one overrides it against the shipped
		// composition, so the authority the host is handed is built from the
		// production factories and a read issued through it would leave the
		// machine. Only the tests-enabled binary has it, and only the
		// live-candidate and forced-selection flags set it.
		bool                        PublicPreviewSelectionAvailable = false;
		core::bpr_profile_id        Profile                         = core::default_bpr_profile_id;
		core::profile_launch_intent ProfileLaunchIntent             = core::profile_launch_intent::activate;
		/// Bounded recorded PR 17/18 reads, independently repeatable per request.
		bool IndependentWindowReads = false;
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
		/// [[BPR-0657]]. Replies published under a thread and thread
		/// resolutions asked of the provider.
		std::size_t Replies     = 0U;
		std::size_t Resolutions = 0U;

		auto operator<=>(const review_mutation_fixture_evidence&) const = default;
	};

	struct github_public_path_fixture_evidence
	{
		/// Raw port-entry attempts include rejected admission and absent slots.
		std::size_t VaultStoreAttempts         = 0U;
		std::size_t VaultRetrieveAttempts      = 0U;
		std::size_t VaultDeleteAttempts        = 0U;
		std::size_t RefreshStoreAttempts       = 0U;
		std::size_t RefreshRetrieveAttempts    = 0U;
		std::size_t RefreshDeleteAttempts      = 0U;
		std::size_t BootstrapFactoryAttempts   = 0U;
		std::size_t AccountFactoryAttempts     = 0U;
		std::size_t BootstrapFactoryCalls      = 0U;
		std::size_t AccountFactoryCalls        = 0U;
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
		// RFC-BPR-0016. How many times a credential crossed the vault
		// boundary, and how many submissions the recorded device transport
		// received. Counted rather than flagged, because "exactly once" is
		// the claim.
		std::size_t VaultStores                = 0U;
		std::size_t DeviceTransportSubmissions = 0U;
		// [[BPR-0539]]. Which material the vault stored, rather than how many
		// times it was asked. The digest is a truncated SHA-256 of the bytes
		// the store accepted, taken at the store and kept afterwards so a
		// later delete cannot erase the evidence; the flag is that digest
		// compared against the recorded grant's own access token. A count
		// alone cannot tell the grant's enrollment from the fixture's connect
		// credential, and those are exactly the two candidates.
		std::string VaultStoredDigest;
		bool        VaultStoredIsDeviceGrant = false;
		// [[BPR-0540]]. The refresh slot, reported the way `SecretRetained`
		// above reports the base one. Nothing answered for the material
		// beside the credential, so a course could show the access token
		// deleted at shutdown while the refresh entry keyed under the same
		// authority survived it -- which is exactly the orphan a later
		// pasted-token session must never spend.
		bool        RefreshRetained  = false;
		std::size_t RefreshStores    = 0U;
		std::size_t RefreshRetrieves = 0U;
		// [[BPR-0547]]. How many times the slot was erased, which the retained
		// flag above cannot answer: it reports the state one erasure or three
		// leave behind. The refresh course's own re-enrollment used to erase
		// the material its rotation had just stored, and the shutdown's
		// erasure after it made the end state look correct.
		std::size_t RefreshDeletes = 0U;
		// [[BPR-0547]]. The identity of the last material written into the slot,
		// which is how a course states that the rotation it drove last is the
		// one whose material the slot held. The retained flag above cannot say
		// it: the shutdown empties the slot before this evidence is read.
		std::string RefreshStoredDigest;
		// The digest of the last credential this fixture handed the connect
		// path, taken with the same function and the same truncation the
		// store uses so the two are comparable. A course that connects rather
		// than granting has no recorded token to compare a store against, and
		// this is what it compares instead.
		std::string LastEnrollmentDigest;
	};

	struct github_epoch_maintenance_fixture_evidence
	{
		std::size_t  PruneIntentDispatchCount  = 0U;
		std::size_t  DeleteIntentDispatchCount = 0U;
		std::size_t  CancelIntentDispatchCount = 0U;
		std::int64_t CatalogRevision           = 0;
		std::string  TargetBaseCatalogOid, TargetHeadCatalogOid, TargetBaseRefOid, TargetHeadRefOid;
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

	/// [[BPR-0657]]. The threaded-reply course over the fixture.
	enum class threaded_reply_probe_result {
		pending,
		passed,
		failed,
	};

	struct threaded_reply_probe_evidence
	{
		bool ReplyDrafted     = false;
		bool ReplyPublished   = false;
		bool ReplyUnderThread = false;
		bool ResolveAsked     = false;
		bool ThreadResolved   = false;

		auto operator<=>(const threaded_reply_probe_evidence&) const = default;
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
		bool ExactThreads              = false;
		bool LocalClassification       = false;
		bool CorrelatedEvidence        = false;
		bool ExplicitVerification      = false;
		bool DurableHistory            = false;
		bool MultiDefectNavigation     = false;
		bool SharedActionSeparate      = false;
		bool RestartRestored           = false;
		bool ZeroReopenWrites          = false;
		bool VisibleProjection         = false;
		bool PersistFailureFailsClosed = false;

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
		bool                                          AgentProcessLauncherComposed    = false;
		std::size_t                                   AgentProcessLauncherInvocations = 0U;
		bool                                          HostComposed                    = false;
		bool                                          Quiescent                       = false;
		application::agent_invocation_host_statistics Statistics;
		std::filesystem::path                         WorkspaceRoot;

		auto operator<=>(const agent_invocation_composition_evidence&) const
			= default;
	};

	/// \brief Raw accepted authorities used to falsify the emitted agent context.
	/// \brief What the RFC-BPR-0012 composition actually did, as values.
	/// \brief What the RFC-BPR-0012 side-chat session composition did.
	struct agent_chat_composition_evidence
	{
		bool                                              ChatHostComposed      = false;
		bool                                              SideChatPanelComposed = false;
		std::size_t                                       Intents               = 0U;
		std::size_t                                       Presentations         = 0U;
		std::size_t                                       Turns                 = 0U;
		std::size_t                                       RefusalsPresented     = 0U;
		application::agent_chat_session_statistics        Statistics;
		std::optional<core::agent_session>                Session;
		std::optional<core::agent_revision_binding>       Observed;
		std::optional<application::agent_chat_projection> Projection;
		std::string                                       SendEnabled;
		std::string                                       SessionStateText;
		std::string                                       HeadOidText;
		std::string                                       AgentPanelStatusText;

		/// The last turn the composition handed to the launch path, as a
		/// value. Read after a send because the launch path admits a session
		/// of its own, so the chat's current session is no longer the one the
		/// turn was produced under.
		std::optional<application::agent_chat_turn_request> LastTurn;
	};

	struct agent_finding_composition_evidence
	{
		bool                                           FindingHostComposed     = false;
		bool                                           CommentWorkshopComposed = false;
		bool                                           McpHelperComposed       = false;
		bool                                           SideChatPanelComposed   = false;
		bool                                           FindingQueueComposed    = false;
		std::size_t                                    QueuedFindings          = 0U;
		std::size_t                                    WorkshopProposals       = 0U;
		std::size_t                                    RefusalsPresented       = 0U;
		std::size_t                                    SideChatPresentations   = 0U;
		std::size_t                                    SideChatIntents         = 0U;
		std::size_t                                    FrozenSnapshotWrites    = 0U;
		std::size_t                                    FrozenFileCount         = 0U;
		application::agent_finding_host_statistics     FindingStatistics;
		application::agent_comment_workshop_statistics WorkshopStatistics;
		application::mcp_helper_supervisor_statistics  McpStatistics;
		std::optional<application::mcp_helper_launch>  McpLaunch;
		std::string                                    McpAnswer;
		std::string                                    QueuedClaim;
		std::string                                    QueuedAnchorState;
		std::string                                    LocalDraftBody;
		std::string                                    WorkshopOriginalText;
		std::string                                    WorkshopProposalText;
		/// What the editor says about the operation: the question while a
		/// press is unanswered, the operation that produced the proposal once
		/// one arrives, and empty for a revision nobody workshopped.
		std::string WorkshopOperationText;
		/// The stable name of the operation still waiting for its turn's
		/// answer, and empty when none is. Reported because "the operation was
		/// recorded" and "the operation was spent by the turn it was asked in"
		/// are different claims, and only a value that can be read between the
		/// two can tell them apart.
		std::string WorkshopPendingOperation;
		/// Whether that pending press has learned which agent turn is
		/// answering it. False between the dispatch and the admission, and
		/// false is why an outcome arriving in that window names no operation.
		bool        WorkshopPendingSessionBound = false;
		std::string SideChatHeadOidText;
		std::string AgentPanelStatusText;
		/// Why the last continued turn froze what it froze. `none` names a
		/// re-alignment that produced the old revision's identities, `deadline`
		/// the wall-clock bound, and anything else is the refusal's own stable
		/// name. Empty until a continuation has been sent.
		std::string LastContinuationRealignment;
		/// Whether that re-alignment asked about a different epoch pair than
		/// the selection on screen. False for a request that would have
		/// resolved exactly what is already displayed.
		bool LastContinuationRealignmentRetargeted = false;
	};

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
		/// [[BPR-0656]]. The ledger host loaded the review's conversation
		/// receipts when the pull request opened: the read state is composed.
		bool ReadStateObserved = false;

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
		bool RepositoriesDrawerToggle  = false;
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

	/// \brief Reports existing diff-host work without copying the accepted review.
	struct diff_work_fixture_evidence
	{
		std::size_t   OutstandingJobs              = 0U;
		std::size_t   WorkbenchOutstandingRequests = 0U;
		std::size_t   EpochOutstandingJobs         = 0U;
		bool          EpochMaintenancePending      = false;
		std::size_t   CacheEntries                 = 0U;
		std::uint64_t CacheBytes                   = 0U;
		std::size_t   FixtureDiffHelperInvocations = 0U;
		bool          Pending                      = false;
		bool          NavigationReady              = false;
		bool          Quiescent                    = false;
	};

	class github_application_fixture_access final
	{
	public:
		/// \brief Copies the host-accepted records for independent fixture identity checks.
		[[nodiscard]] static auto large_review_snapshot(const github_application& aApplication)
			-> std::optional<application::github_pull_request_snapshot>;
		[[nodiscard]] static auto large_review_snapshot(const github_application& aApplication, core::bpr_window_context aContext) -> std::optional<application::github_pull_request_snapshot>;
		static void               hold_window_read_terminal(github_application& aApplication, core::bpr_window_context aContext, bool aHold);
		[[nodiscard]] static auto held_window_read_terminals(const github_application& aApplication, core::bpr_window_context aContext) noexcept -> std::size_t;
		[[nodiscard]] static auto diff_work_evidence(const github_application& aApplication, core::bpr_window_context aContext) noexcept -> diff_work_fixture_evidence;
		/// \brief Samples existing diff-host work counters on the decision lane.
		[[nodiscard]] static auto diff_work_evidence(const github_application& aApplication) noexcept
			-> diff_work_fixture_evidence;
		[[nodiscard]] static auto device_binding_store_idle(const github_application& aApplication) noexcept -> bool;
		[[nodiscard]] static auto device_binding_durable(const github_application& aApplication) noexcept -> bool;
		/// \brief Attempts one native HTTPS read through the shipped adapter.
		///
		/// \details The endpoint returns an ordinary HTTP response outside the
		/// automated-test boundary. The isolation contract requires the attempt to
		/// fail earlier as a typed network error from the production application
		/// binary.
		[[nodiscard]] static auto network_isolation_probe() -> bool;
		[[nodiscard]] static auto start_desktop(github_public_path_fixture aFixture, github_public_path_fixture_options aOptions, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>;
		[[nodiscard]] static auto start_desktop_public_preview_selection_available(github_application_config aConfig, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>;
		[[nodiscard]] static auto start_profile(github_public_path_fixture aFixture, github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>;
		[[nodiscard]] static auto start_profile_public_preview_selection_available(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>;
		[[nodiscard]] static auto start(
			github_public_path_fixture aFixture, github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion)
			-> std::unique_ptr<github_application_startup>;
		/// \brief Composes the shipped graph with the public-preview selection
		/// forced available and no fixture.
		///
		/// \details [[BPR-0316]]. Every factory below this is the production
		/// one, so a read issued through the authority it composes reaches the
		/// real service. Construction sends nothing; what the caller does with
		/// the composed application decides whether anything is issued at all.
		[[nodiscard]] static auto start_public_preview_selection_available(
			github_application_config aConfig, github_application_startup_completion aCompletion)
			-> std::unique_ptr<github_application_startup>;
		[[nodiscard]] static auto evidence(
			const github_application& aApplication) noexcept
			-> github_public_path_fixture_evidence;
		static void               hold_review_submission_terminals(github_application& aApplication, bool aHold);
		[[nodiscard]] static auto held_review_submission_terminals(const github_application& aApplication) -> std::size_t;
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
			const github_application&                       aApplication,
			core::review_epoch_id                           aTarget,
			const std::optional<core::file_comparison_key>& aRetainedComparison = std::nullopt) noexcept
			-> github_epoch_maintenance_fixture_evidence;
		/// \brief Requests the deterministic fixture file through the real host/cache path.
		[[nodiscard]] static auto request_fixture_diff(
			github_application& aApplication,
			bool                aUseSecondHead = false) -> bool;
		/// \brief Replays an accepted metadata-only or changed-head fixture handoff.
		[[nodiscard]] static auto reaccept_fixture_revision(
			github_application& aApplication,
			bool                aChangeHead) -> bool;
		/// \brief Persists the named commit as my last reviewed head and
		/// republishes the overview ([[BPR-0654]]).
		[[nodiscard]] static auto mark_head_reviewed(
			github_application& aApplication,
			std::string_view    aHeadOid) -> bool;
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
		/// \brief [[BPR-0657]]. Replies to a thread from its card, publishes, sees
		/// the reply under the thread on the next read, resolves the thread and
		/// sees it resolved.
		[[nodiscard]] static auto exercise_threaded_reply(github_application& aApplication)
			-> threaded_reply_probe_result;
		[[nodiscard]] static auto threaded_reply_evidence(const github_application& aApplication)
			-> threaded_reply_probe_evidence;
		/// \brief The probe's phase and the facts it last saw, for the run summary.
		[[nodiscard]] static auto threaded_reply_diagnostic(const github_application& aApplication)
			-> std::string;
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
		[[nodiscard]] static auto agent_finding_evidence(
			const github_application& aApplication) noexcept
			-> agent_finding_composition_evidence;
		[[nodiscard]] static auto agent_chat_evidence(
			const github_application& aApplication) noexcept
			-> agent_chat_composition_evidence;
		/// \brief Hands one side-chat intent to the composed dispatch.
		///
		/// \details The same edge the panel's own dispatch callback uses, and
		/// the only way to put a value the panel's enablement rules will not
		/// let a reviewer emit — a send against a stale session — to the host
		/// that must refuse it. It composes nothing and decides nothing.
		[[nodiscard]] static auto dispatch_side_chat_send(
			github_application& aApplication, std::string aText) -> bool;
		/// \brief Hands one private-context opt-in to the composed dispatch.
		[[nodiscard]] static auto dispatch_side_chat_private_context(
			github_application&    aApplication,
			core::agent_session_id aSession,
			bool                   aNotes,
			bool                   aDrafts) -> bool;
		/// \brief The composed finding queue, or nothing when it was cut.
		[[nodiscard]] static auto finding_queue(
			const github_application& aApplication) noexcept
			-> qt::finding_queue_view*;
		/// \brief The composed side-chat panel, or nothing when it was cut.
		[[nodiscard]] static auto side_chat_panel(
			const github_application& aApplication) noexcept
			-> qt::side_chat_panel*;
		/// \brief Starts the shipped read-only helper on one request line.
		[[nodiscard]] static auto start_mcp_context_helper(
			github_application& aApplication, std::string aRequest)
			-> application::mcp_helper_refusal;
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
