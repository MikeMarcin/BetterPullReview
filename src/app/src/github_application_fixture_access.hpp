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
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/mcp_context_helper_supervisor.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/core/document_tabs.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <atomic>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

class QSystemTrayIcon;

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
		/// [[BPR-1213]]. Constructed 17-file pull request with several folders,
		/// nested subfolders and a root file, served on github.com with one head.
		nested_folders_v1,
		/// RFC-BPR-0074 synthetic cross-folder counterparts.
		header_source_v1,
		/// RFC-BPR-0077 complete Unicode/CRLF offscreen pane source.
		source_find_v1,
		source_pr_search_v1,
		source_pr_search_large_v1,
		/// RFC-BPR-0072 complete-blob namespace/enum declarations and uses.
		cpp_identifiers_v1,
		/// [[BPR-1083]]. The nested-folders pull request with review threads
		/// whose posts carry every Conventional Comments label, for judging the
		/// thread marks and the label chip in the application.
		conventional_comments_v1,
		/// [[BPR-1290]]. conventional-comments-v1's files and threads plus a
		/// 1557-line file with eight threads, for judging the inline drawer
		/// against its prototype.
		inline_drawer_v1,
		/// [[BPR-1089]]. The nested-folders pull request with 34 labelled
		/// threads on all its files, pull request comments and review
		/// summaries, for judging the Overview's label chips.
		overview_labels_v1,
		/// [[BPR-1293]]. Prototype v2's pull request: twenty-one conversations
		/// in ten files, one of them branching, for judging Threads
		/// against its prototype.
		conversations_v1,
		/// [[BPR-1334]]. Four files whose changed lines are long: minified
		/// JSON, unwrapped Markdown, a long SQL literal and call, and a
		/// twelve-thousand-character minified line, for judging Line details.
		line_details_v1,
		/// [[BPR-1537]]. Five files of everyday changed lines (renames,
		/// arguments, strings, operators, spacing, numbers, rewritten lines
		/// and prose, non-Latin text), for judging intraline emphasis.
		intraline_v1,
		/// RFC-BPR-0084. The nested-folders pull request whose description and
		/// two pull request comments hold GitHub's image forms, served with
		/// GitHub's rendering and the images it serves, for judging inline images.
		markdown_images_v1,
		/// [[BPR-1543]]. The nested-folders pull request whose description
		/// and four pull request comments are the GitHub-parity Markdown
		/// fixtures, for judging comments, line breaks, alerts and footnotes.
		markdown_parity_v1,
		/// [[BPR-1544]]. The nested-folders pull request whose description
		/// and three pull request comments are the GitHub-parity HTML,
		/// details and emoji fixtures.
		markdown_html_v1,
		/// [[BPR-1562]]. The conventional-comments pull request whose
		/// description and comments hold fenced code in many languages and
		/// diff blocks, and whose threads suggest changes to its lines.
		markdown_code_v1,
		/// [[BPR-1560]]. The recorded base is ahead of the merge base: the
		/// base branch moved on, reverting nothing of the pull request's but
		/// landing its `b.txt` change separately. GitHub's Files changed is
		/// the merge base's comparison; see `scripts/build_merge_base_fixture.py`.
		merge_base_v1,
		/// [[BPR-1572]]. A stacked pull request whose base branch fast-forwarded
		/// to the head's first commit while GitHub kept its recorded base:
		/// GitHub's Files changed is still the recorded base's comparison; see
		/// `scripts/build_stacked_merge_base_fixture.py`.
		stacked_merge_base_v1,
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

	/// \brief [[BPR-0990]]. The fixture repository's open pull requests, as
	/// the personal discovery reads see them.
	///
	/// \details The fixture server answers the personal search by evaluating
	/// its qualifier over this list, as GitHub would: `author:` matches the
	/// author, `review-requested:` matches a named user or any team request
	/// (the viewer is taken to be on every team) on a pull request someone
	/// else authored. Everything else in the list is unrelated, and a read
	/// that fetched it would have to ask for it.
	struct github_personal_discovery_fixture
	{
		struct pull_request
		{
			std::uint32_t            Number = 0U;
			std::string              Author;
			std::vector<std::string> RequestedUsers;
			std::size_t              RequestedTeams = 0U;
			/// [[BPR-1379]]. Changing this server fact lets Home Refresh observe
			/// a submitted review without opening that PR in the course.
			std::shared_ptr<std::atomic<bool>> ReviewedByViewer;
			/// \brief When supplied, false means the fake server no longer requests reviewers.
			std::shared_ptr<std::atomic<bool>> ReviewerRequestsPresent;
			std::shared_ptr<std::atomic<bool>> Draft;
			bool                               WaitingDetails = false;
			std::shared_ptr<std::atomic<bool>> RequiredReviewPresent, CheckFailed;
		};
		std::vector<pull_request> OpenPullRequests;
	};

	/// \brief Safe wire-level primary/secondary refusal control for BPR-0968.
	struct github_rate_limit_fixture
	{
		/// 0 serves the recording, 1 exhausts core, 2 throttles core.
		std::atomic<unsigned>     Mode{ 0U };
		std::atomic<unsigned>     Responses{ 0U };
		std::atomic<unsigned>     Requests{ 0U };
		std::atomic<std::int64_t> LastRequestNanoseconds{ 0 };
		std::atomic<std::int64_t> LastResponseNanoseconds{ 0 };
		std::uint32_t             RetryAfterSeconds = 2U;
		std::uint32_t             ResetAfterSeconds = 1U;
	};

	/// \brief BPR-0969's safe network-loss control, shared by wire and smart-Git fixtures.
	struct github_offline_fixture
	{
		std::atomic_bool      Offline{ false };
		std::atomic<unsigned> WireRefusals{ 0U };
		std::atomic<unsigned> GitRefusals{ 0U };
	};

	/// \brief [[BPR-1357]]. A slow network for a pull request's first answers.
	///
	/// \details Holds the first read of a pull request (its repository, then
	/// the pull request itself) on the fixture transport for `Milliseconds`
	/// each, so Home's opening can be seen and cancelled. A cancelled read stops
	/// waiting at once. `FailNext` answers that many first reads with a network
	/// failure after their wait, for the failure course and its Retry, and
	/// every first read of pull request `FailNumber` fails the same way.
	/// Nothing else the fixture serves is delayed.
	struct github_read_delay_fixture
	{
		std::atomic<std::uint32_t> Milliseconds{ 0U };
		std::atomic<std::uint32_t> FailNext{ 0U };
		std::atomic<std::uint32_t> FailNumber{ 0U };
		std::atomic<std::uint32_t> Delayed{ 0U };
		std::atomic<std::uint32_t> Cancelled{ 0U };
		std::atomic<std::uint32_t> Failed{ 0U };
	};

	/// \brief [[BPR-1477]]. The shown pull request's accepted snapshot, and
	/// whether a read of it is in flight or owed, so a course can wait for a
	/// reread the window makes by itself.
	struct window_read_progress_evidence
	{
		std::optional<std::uint64_t> Snapshot;
		bool                         Reading = false;
	};

	/// \brief [[BPR-1198]]. Read-only identities at an epoch refresh boundary.
	struct epoch_refresh_identity_evidence
	{
		std::optional<core::file_identity>   SelectedFile;
		std::optional<core::document_tab>    ActiveTab;
		std::optional<core::comparison_pair> AcceptedComparison;
		std::optional<core::comparison_pair> PublishedComparison;
		std::optional<core::file_identity>   PublishedFile;
		bool                                 ComparisonPending    = false;
		bool                                 TabPending           = false;
		bool                                 TabModality          = false;
		bool                                 PublicationDisplayed = false;
	};

	/// \brief [[BPR-1301]]. The `PATH` and usual install folders a Claude
	/// launch with no wrapper searches, in place of the application's `PATH`
	/// and the shipped folders, which on a developer's machine may hold a
	/// real `claude`.
	struct claude_search_environment
	{
		std::string              PathVariable;
		std::vector<std::string> UsualFolders;
	};

	struct github_public_path_fixture_options
	{
		struct github_cli_discovery_account
		{
			std::string Hostname;
			std::string Login;
			/// [[BPR-1373]]. The full inventory's facts; a fixture account is the
			/// host's active, signed-in, keychain-held account unless it says not.
			bool Active       = true;
			bool SignedIn     = true;
			bool KeychainHeld = true;
		};

		std::optional<std::filesystem::path>      ApplicationDataRoot;
		std::shared_ptr<qt::dif_catalog_provider> DifCatalog;
		/// [[BPR-1250]]. The desktop's menu bar status item, set by `start_desktop`.
		std::shared_ptr<QSystemTrayIcon> StatusItem;
		/// [[BPR-0777]]. Drives the composition's empty-desktop policy from a course,
		/// so both the resident and the quit outcome are observable on one machine.
		/// Unset selects the running platform's own policy.
		empty_desktop_policy      EmptyDesktopPolicy;
		core::diff_compute_limits DiffComputeLimits;
		bool                      LargeReviewHistory = false;
		/// BPR-0972: the synthetic large-review posts belong to another fixture actor.
		bool LargeReviewForeignPosts = false;
		bool ReviewTourCourse        = false;
		/// BPR-0972: each read session of the paged large review replays its
		/// complete script from the repository read, as the strict scripts do
		/// with `RepeatReadSessions`, so a sibling window can load it again.
		bool                       LargeReviewRepeatReads = false;
		github_epoch_fixture_phase EpochPhase             = github_epoch_fixture_phase::single;
		std::string                TimelineCourse;
		/// [[BPR-1560]]. GitHub's compare answers with a server error, so no
		/// merge base is named for the pull request.
		bool MergeBaseUnavailable = false;
		/// [[BPR-1560]]. GitHub records another base for the pull request
		/// between the snapshot and the read that confirms it.
		bool MergeBaseRace = false;
		/// [[BPR-1560]]. GitHub's compare answers that the credential is no
		/// longer accepted.
		bool MergeBaseUnauthorized = false;
		/// [[BPR-1572]]. On a stacked fixture, GitHub's compare answers that the
		/// recorded base is not an ancestor of the base branch's tip: the
		/// branch was rewritten, so GitHub is replacing the recorded base.
		bool StackedBaseRewritten = false;
		/// [[BPR-1572]]. On a stacked fixture, GitHub replaces the recorded
		/// base with the base branch's tip (a descendant of it) between the
		/// snapshot and the read that confirms it: the window after the base
		/// branch is force-pushed, before the snapshot shows the new base.
		bool StackedBaseReset               = false;
		bool EmptyHistoricalTimeline        = false;
		bool UnfetchableFirstHistoricalHead = false;
		/// [[BPR-0922]]. Serves the GHES contract over the recorded two-file
		/// reading graph so the CLI-profile course can prove exact source recovery.
		bool GhesCliSourceUpdateCourse = false;
		/// [[BPR-0943]]. Provider M while the selected base tree lacks review.cpp.
		bool ModifiedMissingEndpointCourse = false;
		/// Test-owned gate before catalogue reads; absent from production builds.
		std::function<void(std::stop_token)> BeforeFilesPage;
		/// BPR-1514: controls the remembered connection validation on the fixture transport.
		std::function<void(std::stop_token)> BeforeViewerIdentity;
		/// [[BPR-0922]]. Refuses exactly the first valid smart-Git acquisition.
		/// The next application course is served normally, making the boundary
		/// and the supported path independently observable.
		bool                                             RefuseFirstSmartAcquire        = false;
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
		bool DisableAgentFindingHostComposition = false;
		bool DisableChatSessionHostComposition  = false;
		bool DisableCommentWorkshopComposition  = false;
		bool DisableMcpHelperComposition        = false;
		bool DisableSideChatPanelComposition    = false;
		bool DisableFindingQueueComposition     = false;
		/// [[BPR-0909]]. Synthetic token-free `gh auth status` result. Presence
		/// enables discovery inside the test boundary without inspecting the
		/// developer's real CLI configuration.
		std::optional<std::vector<github_cli_discovery_account>> GithubCliDiscoveryAccounts;
		std::shared_ptr<std::atomic_size_t>                      GithubCliInventoryChecks;
		/// Selected fixture capability cut; absent uses the strict synthetic CLI path.
		std::shared_ptr<std::atomic_bool>    GithubCliStrictCapability;
		std::optional<std::filesystem::path> ExternalAgentExecutable;
		/// Points the Codex JSONL profile at a fixture executable. That
		/// protocol is the one with a result frame a finding payload can ride
		/// on; the external terminal has none and refuses one by design.
		std::optional<std::filesystem::path> CodexAgentExecutable;
		std::optional<std::filesystem::path> ClaudeAgentExecutable;
		/// [[BPR-1301]]. Where the shipped bare `claude` is looked for, so a
		/// course decides what is found and never finds the developer's own.
		/// Absent, the application's environment is searched, as shipped.
		std::optional<claude_search_environment> ClaudeSearch;
		std::optional<std::filesystem::path>     GitExecutable;
		// RFC-BPR-0016. Composes a registered device capability against the
		// recorded BPR-0276 grant so the enrollment reaches the real vault.
		// Only meaningful with a fixture; a shipped build has no identity to
		// register and this option does not exist there.
		/// Starts the GHES fixture behind real server onboarding instead of a configured profile.
		bool SetupUnconfiguredEnterprise = false;

		bool RegisteredDeviceCourse = false;
		/// RFC-0027: fail the recorded capability probe with a useful unfamiliar error.
		bool CapabilityDiagnosticFailure = false;
		// RFC-BPR-0016 renewal. The same registered grant, plus the two things
		// a rotation needs that the read-path fixture has no use for: a real
		// second vault slot and the documented renewal answer on the
		// transport. Only meaningful with a fixture.
		bool DeviceRefreshCourse   = false;
		bool PersistDeviceBindings = false;
		/// [[BPR-0644]]. The renewal answer becomes the server's refusal of the
		/// refresh material, so a course can drive a launch whose stored grant
		/// the server no longer honours. Only meaningful beside a renewal course.
		bool RejectDeviceRefresh = false;
		/// [[BPR-0644]]. The access slot answers `locked`, as a platform keychain
		/// does when it will not answer without a prompt this launch.
		bool LockedDeviceVault = false;
		/// [[BPR-0644]]. The token endpoint cannot be reached: every renewal
		/// attempt answers a network failure, as an offline launch does.
		bool OfflineDeviceRefresh = false;
		/// [[BPR-0644]]. `/user` answers as an account no binding names, on the
		/// bootstrap and the account transports alike: the stored or rotated
		/// token now signs in as someone else.
		bool ViewerNamesAnotherAccount = false;
		/// [[BPR-0803]]. While this holds true, the device flow's poll answers
		/// `authorization_pending`, as GitHub does until the reviewer finishes
		/// in the browser; the course sets it false to finish. Absent, the first
		/// poll is authorized, as before.
		std::shared_ptr<std::atomic_bool> DeviceAuthorizationPending;
		/// [[BPR-0657]]. Lets a course refresh the pull request: the recorded
		/// read script is replayed for each session the product establishes,
		/// with the fixture's replies and resolutions overlaid.
		bool                                    RepeatReadSessions = false;
		std::shared_ptr<github_offline_fixture> Offline;
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
		// [[BPR-1338]]. An attended look is a person's session, so the window's
		// own opens (Open on GitHub and body links) reach the real system
		// opener instead of the recorder every fixture run otherwise binds
		// ([[BPR-0797]]). Device sign-in keeps the recorder: its fixture
		// verification address is recorded traffic, not a page to show.
		// `main` sets it only for `--attended-launch` outside the automated
		// test boundary; a test that sets it substitutes the platform launch.
		bool AttendedWindowBrowser = false;
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
		/// [[BPR-1083]]. Turns the label helper on. Off by default, as the
		/// product is. Since [[BPR-1088]] it writes the profile's Label helper
		/// preference, the Settings row's own path, rather than reaching into
		/// the editor; since [[BPR-1565]] that preference only sends the Review
		/// feedback guidance with a workshop request.
		bool LabelHelper = false;
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
		/// Two same-server accounts selected only by the test-owned BPR profile.
		bool DistinctProfileAccounts = false;
		/// Safe generated viewer selected before each real sign-in in one profile.
		std::shared_ptr<std::atomic<std::uint64_t>> RecentAccount;
		/// [[BPR-0990]]. Serves personal discovery over this repository list.
		std::optional<github_personal_discovery_fixture> PersonalDiscovery;
		std::shared_ptr<github_rate_limit_fixture>       RateLimit;
		/// [[BPR-1357]]. Delays a pull request's first reads; none when null.
		std::shared_ptr<github_read_delay_fixture> ReadDelay;
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

	/// \brief [[BPR-0990]]. The profile's one discovery scheduler, as composed.
	struct personal_discovery_fixture_evidence
	{
		bool                                       Composed = false;
		application::personal_discovery_statistics Statistics;
		application::queue_discovery_status        Status;
		bool                                       Quiescent = false;
	};

	struct review_mutation_fixture_evidence
	{
		std::size_t TargetObservations   = 0U;
		std::size_t Submissions          = 0U;
		std::size_t StoredComments       = 0U;
		std::size_t NativePosts          = 0U;
		std::size_t FilePosts            = 0U;
		std::size_t SessionConstructions = 0U;
		std::size_t DurableJournalReads  = 0U;
		bool        LostResult           = false;
		bool        DuplicateReturned    = false;
		/// [[BPR-0657]]. Replies published under a thread and thread
		/// resolutions asked of the provider.
		std::size_t Replies     = 0U;
		std::size_t Resolutions = 0U;
		/// [[BPR-1084]]. The last created comment's decoded body, path, side
		/// and line exactly as the fixture server received them, and whether
		/// it was a file comment.
		std::string                  LastBody;
		std::string                  LastPath;
		std::string                  LastSide;
		std::optional<std::uint32_t> LastLine;
		bool                         LastFile = false;
		/// [[BPR-1212]]. The last created comment's multi-line fields as
		/// received: none when it sent no `start_line` or `start_side`.
		std::optional<std::uint32_t> LastStartLine;
		std::optional<std::string>   LastStartSide;

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
		/// RFC-BPR-0084. Body image resolutions and fetches the fixture served.
		std::size_t MarkdownImageResolutions = 0U;
		std::size_t MarkdownImageFetches     = 0U;
		/// [[BPR-1560]]. Compare reads and pull request confirmations served.
		std::size_t MergeBaseReads           = 0U;
		std::size_t MergeBaseConfirmations   = 0U;
		int         LastReadOperation        = -1;
		int         SmartAcquireCount        = 0;
		int         SmartAcquireRefusalCount = 0;
		int         SmartDiscoveryCount      = 0;
		int         SmartLsRefsCount         = 0;
		int         SmartFetchCount          = 0;
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
		/// [[BPR-0990]]. The personal searches and requested-reviewer reads the
		/// fixture answered, each search's terms, and the operation name of
		/// every account-transport read in the order it arrived.
		std::size_t              DiscoverySearches    = 0U;
		std::size_t              DiscoveryDetailReads = 0U;
		std::vector<std::string> DiscoverySearchTerms;
		std::vector<std::string> AccountReadOperations;
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

	/// [[BPR-1083]]. Opening one of the fixture's files in Read, as a
	/// reviewer would from the files drawer.
	enum class fixture_file_reveal_result {
		pending,
		shown,
		failed,
	};

	/// [[BPR-1083]]. The label-authoring course over `conventional-comments-v1`.
	enum class label_authoring_probe_result {
		pending,
		passed,
		failed,
	};

	struct label_authoring_probe_evidence
	{
		/// [[BPR-1287]]. Read with no file chosen shows the direction to open
		/// one, not two empty panes.
		bool ReviewEmptyDirected = false;
		/// The file opened in Read with its threads in the drawer.
		bool FileShown = false;
		/// Its labelled threads' cards carry the marks the bodies name.
		bool ThreadMarksDrawn = false;
		/// Comment opened an editable inline page in Read.
		bool CommentEditable = false;
		/// [[BPR-1565]]. `:qu:` typed into an empty body stayed that text, with
		/// no label and no control edit, while the label helper was on.
		bool CodeStaysLiteral = false;
		/// The chip's menu changed the label.
		bool ChipApplied = false;
		/// ⌘L in the body opened the menu, which changed it again.
		bool ShortcutApplied = false;
		/// Publish posted one comment with the labelled body.
		bool Published = false;
		/// After a refresh, the new thread's card carries its mark.
		bool PublishedMarked = false;
		/// [[BPR-1084]]. An issue with a suggested change on a free line: the
		/// mark beside Publish said native before Publish.
		bool PatchMarkedNative = false;
		/// It published once as a native line comment whose decoded body and
		/// line the fixture server received exactly as drafted, and its thread
		/// came back as an issue with the fence in its body.
		bool PatchPublishedExact = false;
		/// A suggestion with no patch showed no mark and published exactly its
		/// prose: no fence was invented.
		bool ProseUnmarkedExact = false;
		/// A suggested change on a line GitHub will not take an inline comment
		/// on was refused (incompatible_body): one native attempt on the exact
		/// line, no file comment, no other line, the text kept as written.
		bool RefusedTextKept = false;
		/// After that refusal the mark says text only before the next Publish.
		bool RefusedMarkedTextOnly = false;
		/// A reply written in place in a thread on a current line: the reply
		/// editor's mark and the draft editor's both say native.
		bool ReplyMarkedNative = false;
		/// [[BPR-1086]]. After every labelled comment above, no defect exists,
		/// and the issue root's thread menu offers Classify as defect.
		bool IssueOffersTracking = false;
		/// That explicit gesture, and only it, made one outstanding defect on
		/// that thread, with no review submission or post.
		bool TrackedOnlyByGesture = false;
		/// The website removed the root's `issue` label and the application
		/// refreshed: the same thread now reads plain and the defect is kept,
		/// its state, severity and category unchanged.
		bool RelabelKeptDefect = false;
		/// The website resolved the thread and the application refreshed: the
		/// defect stays outstanding, never verified or withdrawn, verification
		/// is still the reviewer's explicit action, and nothing was submitted.
		bool ResolveOnlyEvidence = false;
		/// A tour stop the website shows on another thread, its narrative
		/// labelled `question`, is marked from its narrative and kept byte for
		/// byte.
		bool TourStopLabelled = false;
		/// [[BPR-1212]]. Return on a left-side line with the left pane focused
		/// opened nothing and the status line said why (Flagged 4).
		bool LeftLineRefused = false;
		/// Return on one free line opened its draft in the drawer, named for
		/// that one line, with no first line.
		bool SingleLineOpenedPlain = false;
		/// Shift+Down twice from a selected right line, with the right pane
		/// focused, selected three lines inside one hunk.
		bool KeyboardRangeSelected = false;
		/// Shift+Return opened the drawer draft anchored to the last of them
		/// with the first as its start, named "Lines a–b" with the gutter's
		/// bracket over all three, its body the right side's exact lines in a
		/// suggestion block, and the caret at the end of the block's last line.
		bool SuggestionPrefilled = false;
		/// The drawer draft's Publish sent GitHub's multi-line fields: the fixture server received
		/// the block, `line` the last line, `start_line` the first, and both
		/// sides RIGHT.
		bool RangePublishedExact = false;
		/// With no line selected, Return opened nothing and the status line
		/// said how to select one.
		bool NoLineExplained = false;
		/// [[BPR-1212]] v002. The epoch host answered that the file's base is
		/// numbered as GitHub's base, and the window and the source took it.
		bool BaseNumberedKnown = false;
		/// [[BPR-1570]]. A refresh that kept the file, adopted under a new
		/// workbench authority, kept that answer in the window and the source.
		bool BaseNumberingKept = false;
		/// Return on a base line in a hunk opened its draft on the left in the
		/// drawer, named "base line N".
		bool BaseLineOpened = false;
		/// Its Publish sent `side: LEFT` and the base line, with no range, on
		/// the file's path.
		bool BasePublishedLeft = false;

		auto operator<=>(const label_authoring_probe_evidence&) const = default;
	};

	/// [[BPR-1326]]. The published-comment file-switch course over
	/// `conventional-comments-v1`.
	/// [[BPR-1326]]. What the file-switch course writes and when it leaves
	/// the comment's file: published, once its thread shows (`settled`);
	/// published, with the application's own refresh answering only once the
	/// other file shows, as a slow server's does (`late`); or kept as my
	/// unpublished draft (`draft`).
	enum class published_file_switch_timing {
		settled,
		late,
		draft,
	};

	enum class published_file_switch_probe_result {
		pending,
		passed,
		failed,
	};

	struct published_file_switch_probe_evidence
	{
		/// A new inline comment on `src/engine/renderer.cpp` was published once
		/// on its line, or, for `draft`, kept unpublished.
		bool Written = false;
		/// Before leaving the file, it showed as a note, a gutter marker and a
		/// scroll-bar tick on its line (not asked of `late`).
		bool ShownBefore = false;
		/// Another file was opened and shown in Review.
		bool OtherFileShown = false;
		/// Back on `renderer.cpp`, the accepted pull request still holds the
		/// thread, or the draft is still saved.
		bool Kept = false;
		/// Back on `renderer.cpp`, its note, gutter marker and scroll-bar tick
		/// show again on the same line, whichever line is selected.
		bool ShownAfterReturn = false;

		auto operator<=>(const published_file_switch_probe_evidence&) const = default;
	};

	/// [[BPR-1374]]. An unsent reply in the composed application over
	/// `conventional-comments-v1`, on `src/engine/renderer.cpp`'s first
	/// thread (`PRRT_cc_1`, line 6), written as a reviewer does: the thread's
	/// Reply, then typing in the reply in place. Then the public Refresh:
	/// with no change on the server (`refresh`); after another client edits
	/// the thread's root (`edited`); after another client resolves it
	/// (`resolved`); after another client deletes its root, then the kept
	/// reply copied and discarded (`deleted`) or kept as a comment on its
	/// line (`kept`). `write` writes the reply and quits; `restart`, over the
	/// same data root, finds it again. Nothing is ever sent to the server.
	enum class reply_draft_course {
		refresh,
		edited,
		write,
		restart,
		deleted,
		kept,
		resolved,
	};

	enum class reply_draft_probe_result {
		pending,
		passed,
		failed,
	};

	struct reply_draft_probe_evidence
	{
		/// The reply was written in place on `PRRT_cc_1` and saved.
		bool Written = false;
		/// After the Refresh (or the restart) the window shows the server's
		/// truth: the thread as it was, its edited root, its resolution, or
		/// no thread at all.
		bool ServerShown = false;
		/// The reply is still saved, being written, with its words and thread.
		bool Kept = false;
		/// It is presented: held again by the editor and marked on its
		/// thread's note, or after a restart marked there and opened again by
		/// Reply, or kept as a note of its own at its line when its thread is
		/// gone.
		bool Presented = false;
		/// What became of the thread is said where the reply is shown: that
		/// it was deleted or resolved on GitHub, and nothing when it was not.
		bool Flagged = false;
		/// The course's own step: typing after the Refresh changes the same
		/// draft (`refresh`); Publish is still offered (`resolved`); Copy text
		/// copies the words and Discard, once asked, discards it (`deleted`);
		/// Keep as comment makes it a comment on its line (`kept`); Reply
		/// shows its words (`restart`).
		bool Acted = false;
		/// The fixture server received no post, reply, review or resolution.
		bool NothingSent = false;

		auto operator<=>(const reply_draft_probe_evidence&) const = default;
	};

	/// [[BPR-1478]]. A reread of the shown pull request lands just after the
	/// reviewer picks another file, in the composed application over
	/// `conventional-comments-v1`. Review shows `src/engine/renderer.cpp`
	/// with a line selected. The read starts, from the public Refresh
	/// (`refresh`) or from the window's own reread after a publication
	/// (`reread`), and is held. The reviewer picks `src/engine/renderer.h` in
	/// the files drawer while the window's reflection of that pick into the
	/// active tab is held too. The read lands first, then the reflection
	/// runs: the order a slow event loop can give, made the same every run.
	enum class refresh_pick_course {
		refresh,
		reread,
	};

	enum class refresh_pick_probe_result {
		pending,
		passed,
		failed,
	};

	struct refresh_pick_probe_evidence
	{
		/// The read landed after the pick and before the active tab took it:
		/// a new snapshot was accepted while the active tab still named
		/// `renderer.cpp`.
		bool Raced = false;
		/// Once the read settled, Review shows `renderer.h`: the file chosen,
		/// the active tab on it and its diff on screen.
		bool Kept = false;
		/// Twenty ticks later it still does, with nothing being read.
		bool Stayed = false;

		auto operator<=>(const refresh_pick_probe_evidence&) const = default;
	};

	/// [[BPR-1345]]. Start a review in the composed application over
	/// `conventional-comments-v1`, on `src/engine/renderer.cpp`. `pending`
	/// starts a review on one line, adds a range to it, publishes a third
	/// comment on its own and opens another file and comes back; `finish`,
	/// over the same data root after a restart, finds the two pending, opens
	/// Finish review and submits Comment with no summary; `refused` has the
	/// server refuse the review and then discards it; `uncertain` loses the
	/// server's answer and its reads, then checks status once they are back.
	/// v002: `again` is the owner's discard-review v001 flow. It keeps an
	/// older draft, starts a review of two, opens Finish review by its key,
	/// deletes one comment from the page and discards the review, then
	/// discards a summary on the Compose review page as well. It then takes
	/// the older draft into a new review, finishes it with Comment, and
	/// checks that nothing says "Review draft discarded.".
	enum class start_review_course {
		pending,
		finish,
		refused,
		uncertain,
		again,
		/// [[BPR-1541]]. `uncertain` over a kept data root, stopped once the
		/// unknown review shows why; `restored` reopens that root.
		kept,
		restored,
	};

	enum class start_review_probe_result {
		pending,
		passed,
		failed,
	};

	struct start_review_probe_evidence
	{
		// `pending`.
		/// The drawer draft offered Add single comment and Start a review;
		/// Start a review kept it pending with nothing published.
		bool Started = false;
		/// With a review in progress the next draft's main action was Add
		/// review comment, ⌘↩ added it, and the bar said "Review · 2 pending".
		bool Added = false;
		/// Each pending draft was drawn at its line, with its gutter marker.
		bool Drawn = false;
		/// Add single comment on a third draft published exactly that one;
		/// the review still held two and nothing was submitted.
		bool SingleAlone = false;
		/// Another file opened and this one again: still two pending, still
		/// nothing published.
		bool NavigationKept = false;
		// `finish`.
		/// After the restart both drafts were pending again, counted and drawn.
		bool Restored = false;
		/// Finish review opened the summary in Discuss with both listed.
		bool FinishOpened = false;
		/// Comment with no summary could be submitted.
		bool CommentWithoutSummary = false;
		/// One review was sent whose comments were exactly the two drafts:
		/// their path, lines, side and words.
		bool SubmittedExact = false;
		/// On GitHub's acceptance each draft was marked published, and the
		/// review bar's chip went.
		bool MarkedPublished = false;
		/// [[BPR-1475]]. Right after the submission the window read the pull
		/// request again by itself, and Review, the file and its selected line
		/// stayed where they were.
		bool RereadAfterPublish = false;
		/// [[BPR-1475]]. A note being written survived that reread: still
		/// open in the drawer, with its words.
		bool NoteKept = false;
		/// [[BPR-1472]]. Home, then the pull request's Recently viewed row,
		/// read it again (BPR-1475: every reopen reads fresh).
		bool ReopenedFresh = false;
		// `refused`.
		/// A refused review left its draft pending, with its text.
		bool RefusedKept = false;
		/// v002: Discard review, confirmed, deleted the review's pending
		/// comments and closed the page; a draft not in the review stayed.
		bool DiscardDeleted = false;
		// `again`.
		/// Delete on a row of Finish your review, confirmed, deleted that
		/// one comment and no other.
		bool RowDeleted = false;
		/// A row of Finish your review went to its comment: Review, its file,
		/// its line selected; Finish review came back by its action.
		bool RowRevealed = false;
		/// Discard on Compose review, with a summary and no comments, asked,
		/// discarded and closed the page.
		bool SecondDiscard = false;
		/// The older draft, taken into a new review by ⌘↩, was its one
		/// pending comment.
		bool AgainStarted = false;
		/// Finish your review listed it, offered Comment, and said nothing
		/// of an earlier discard.
		bool AgainOffered = false;
		/// Comment submitted one review carrying exactly that comment, and it
		/// was marked published.
		bool AgainSubmitted = false;
		// `uncertain`.
		/// With the answer lost the draft was held, its text kept, and no
		/// retry was offered.
		bool UncertainHeld = false;
		/// Check status found the review and marked the draft published,
		/// with one review sent.
		bool Reconciled = false;
		// [[BPR-1541]].
		/// The unknown review's page said why under its status, as part of
		/// the status's description, with no identity.
		bool ReasonShown = false;
		/// Check status's next answer replaced the line with its own reason.
		bool ReasonReplaced = false;
		/// Once confirmed, no reason was shown.
		bool ReasonCleared = false;
		/// After a restart, the restored review showed the reason its record
		/// kept before Check status answered.
		bool ReasonRestored = false;

		auto operator<=>(const start_review_probe_evidence&) const = default;
	};

	/// [[BPR-1331]]. The private progress course across a push of
	/// `conventional-comments-v1`: `before` marks three files on the first
	/// head, `after` reads them back on the pushed head, which changed only
	/// `src/engine/renderer.cpp`.
	enum class review_progress_push_course {
		before,
		after,
	};

	enum class review_progress_push_result {
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
		/// [[BPR-1331]]. The private progress button's accessible name once
		/// the course's receipt is shown: after the gestures on the first
		/// head, and on reopening the changed file on the pushed head.
		std::string ProgressMark;

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

	/// \brief [[BPR-0863]]. The surfaces a modality can show a course.
	///
	/// \details A course states the ones it needs rather than leaving them
	/// inferred, so a modality that withholds one fails the step by name,
	/// saying what it withholds, instead of leaving the course to stall on a
	/// hidden drawer or lose a label.
	struct course_surfaces
	{
		bool Comparison          = false;
		bool FilesDrawer         = false;
		bool ConversationsDrawer = false;
		bool RepositoriesDrawer  = false;
	};

	/// \brief [[BPR-0863]]. The outcome of a course's step into its modality.
	enum class course_modality_result {
		pending,
		entered,
		failed,
	};

	/// \brief [[BPR-0863]]. A course's step into the modality it states.
	///
	/// \details A pull request named in the Home bar opens on the reviewer's
	/// opening modality, Overview by default ([[BPR-0661]], [[BPR-0771]]), and
	/// Overview shows neither the comparison nor any drawer ([[BPR-0727]]). A
	/// course that reads either states the modality it reads in and what it
	/// needs there, and advances this step until it answers. Which modality
	/// shows what is written down once, in `docs/verification/fixture-catalog.md`
	/// under "The modality a course states".
	struct course_modality_step
	{
		qt::review_modality Modality = qt::review_modality::review;
		course_surfaces     Needs;
		/// \brief Sticky once the step has entered the modality or failed.
		course_modality_result Result = course_modality_result::pending;
		/// \brief When the strip's action was triggered; empty until then.
		std::optional<std::chrono::steady_clock::time_point> ChosenAt;
		std::size_t                                          Waits = 0U;
	};

	/// \brief [[BPR-0864]], [[BPR-1037]]. The clock and count of one fixture
	/// probe phase's bounded wait.
	///
	/// \details A probe's state owns one. The fixture-access region starts it
	/// at the phase's first wait, counts each call, and after about five
	/// seconds and at least sixteen calls fails the phase by name. Only a phase
	/// change clears it, never a tick, so a wait cannot restart itself and never
	/// reach its bound ([[BPR-0859]]).
	struct fixture_probe_wait
	{
		std::optional<std::chrono::steady_clock::time_point> Since;
		std::size_t                                          Calls = 0U;
	};

	/// \brief Reports existing diff-host work without copying the accepted review.
	/// \details [[BPR-1344]]. EpochReviewerJobs and EpochPreparationJobs split
	/// EpochOutstandingJobs, each read under the epoch host's one lock.
	/// PreparationPending: a preparation round is scheduled, or deferred while
	/// the reviewer has work, and has not started.
	struct diff_work_fixture_evidence
	{
		std::size_t   OutstandingJobs              = 0U;
		std::size_t   WorkbenchOutstandingRequests = 0U;
		std::size_t   EpochOutstandingJobs         = 0U;
		std::size_t   EpochReviewerJobs            = 0U;
		std::size_t   EpochPreparationJobs         = 0U;
		bool          PreparationPending           = false;
		bool          EpochMaintenancePending      = false;
		std::size_t   CacheEntries                 = 0U;
		std::uint64_t CacheBytes                   = 0U;
		std::size_t   FixtureDiffHelperInvocations = 0U;
		bool          Pending                      = false;
		bool          NavigationReady              = false;
		bool          TabPresentationPending       = false;
		bool          Quiescent                    = false;
		/// [[BPR-1327]]. The part of the cache above held by comparisons
		/// prepared ahead of the reviewer, the preparation still queued or
		/// running in the diff host and in the window, and what preparation
		/// has done so far.
		std::size_t   PreparedEntries       = 0U;
		std::uint64_t PreparedBytes         = 0U;
		std::size_t   Preparing             = 0U;
		std::size_t   PreparationsInFlight  = 0U;
		std::size_t   PreparationRounds     = 0U;
		std::size_t   PreparedResolutions   = 0U;
		std::size_t   PreparedRemoteBlobs   = 0U;
		std::size_t   PreparedRemoteBatches = 0U;
		std::uint64_t PreparedHits          = 0U;
	};

	/// \brief Observes public-composition scaling without invoking any effect.
	struct application_scaling_evidence
	{
		application::workbench_work_statistics     Workbench;
		application::review_ledger_host_statistics Ledger;
		application::review_queue_host_statistics  Queue;
		std::uint64_t                              OverviewBuilds             = 0U;
		std::uint64_t                              OverviewReceiptVisits      = 0U;
		std::uint64_t                              OverviewHistoryUpdates     = 0U;
		std::uint64_t                              ConversationThreadsVisited = 0U;
		std::uint64_t                              ConversationPostsVisited   = 0U;
		std::uint64_t                              OverviewReceiptUpdates     = 0U;
		std::uint64_t                              OverviewReceiptRows        = 0U;
		/// [[BPR-1094]]. The window's label buckets: bodies parsed, scopes and
		/// units tallied, and how often the Overview's counts were published.
		application::feedback_counter_work FeedbackWork;
		std::uint64_t                      OverviewFeedbackPublications = 0U;
		/// Observations offered to the buckets and bodies they parsed, counted
		/// by the window, so they keep counting if the model were made afresh.
		std::uint64_t FeedbackObservations = 0U;
		std::uint64_t FeedbackBodiesParsed = 0U;
		/// [[BPR-1500]]. No queue read or store is outstanding: an applied
		/// refresh whose facts changed publishes its item only after the store.
		bool QueueQuiescent = true;
	};

	/// \brief Samples comparison ownership without driving any host.
	struct tab_comparison_fixture_evidence
	{
		std::optional<core::comparison_pair> Accepted;
		std::optional<core::comparison_pair> Window;
		std::optional<core::comparison_pair> ActiveTab;
	};

	class github_application_fixture_access final
	{
	public:
		[[nodiscard]] static auto scaling_evidence(const github_application& aApplication, std::optional<core::bpr_window_context> aContext = std::nullopt) noexcept -> application_scaling_evidence;
		/// \brief Copies the accepted and tab-owned pairs on the decision lane.
		[[nodiscard]] static auto tab_comparison_evidence(const github_application& aApplication) -> tab_comparison_fixture_evidence;
		/// \brief Copies the host-accepted records for independent fixture identity checks.
		[[nodiscard]] static auto large_review_snapshot(const github_application& aApplication)
			-> std::optional<application::github_pull_request_snapshot>;
		[[nodiscard]] static auto large_review_snapshot(const github_application& aApplication, core::bpr_window_context aContext) -> std::optional<application::github_pull_request_snapshot>;
		/// Change fake remote records; the course must still issue a public Refresh.
		static void stage_server_conversation(github_application& aApplication, std::string aThread, bool aAddedAndResolved);
		/// [[BPR-1085]]. Change one thread's root body, resolution or root author
		/// on the fake server; the course must still issue a public Refresh.
		static void stage_server_root(github_application& aApplication, std::string aThread, std::optional<std::string> aRootBody, std::optional<bool> aResolved,
									  std::optional<std::pair<std::string, std::string>> aRootAuthor = std::nullopt);
		/// \brief [[BPR-1177]]. A course's account for Publish as Pull Request in
		/// one window, in place of the session's: a simulated GitHub behind the
		/// publication catalog's port. Nothing else of the composition changes.
		static void               set_cycle_publication_account(github_application& aApplication, core::bpr_window_context aContext,
																std::function<std::optional<application::github_publication_account>()> aAccount);
		static void               hold_window_read_terminal(github_application& aApplication, core::bpr_window_context aContext, bool aHold);
		[[nodiscard]] static auto held_window_read_terminals(const github_application& aApplication, core::bpr_window_context aContext) noexcept -> std::size_t;
		[[nodiscard]] static auto diff_work_evidence(const github_application& aApplication, core::bpr_window_context aContext) noexcept -> diff_work_fixture_evidence;
		/// \brief Samples existing diff-host work counters on the decision lane.
		[[nodiscard]] static auto diff_work_evidence(const github_application& aApplication) noexcept
			-> diff_work_fixture_evidence;
		/// \brief [[BPR-1369]]. How many times the active window's diff host
		/// has read this blob, from Git or the network, for any comparison
		/// it computed: the reviewer's or a prepared one.
		[[nodiscard]] static auto diff_blob_reads(const github_application& aApplication, const core::git_blob_oid& aBlob) -> std::uint64_t;
		/// \brief [[BPR-1327]]. Turns preparation off or on in the active
		/// window, so one course can measure opening files both ways.
		static void set_preparation_enabled(github_application& aApplication, bool aEnabled);
		/// \brief [[BPR-1212]]. The next `aCount` base-numbering answers apply as
		/// `cancelled`, as an accept that stops queued epoch work leaves them.
		static void cancel_base_numbering_answers(github_application& aApplication, std::size_t aCount);
		/// \brief [[BPR-1570]]. After the label-authoring course's kept
		/// refresh, the kept base-numbering answer is made one about another
		/// question, the window's left side left placed.
		static void               stale_base_numbering_question(github_application& aApplication);
		[[nodiscard]] static auto device_binding_store_idle(const github_application& aApplication) noexcept -> bool;
		[[nodiscard]] static auto device_binding_durable(const github_application& aApplication) noexcept -> bool;
		/// \brief [[BPR-1338]]. The last address the recording browser was handed; empty before any.
		[[nodiscard]] static auto browser_last_address(const github_application& aApplication) -> std::string;
		/// \brief [[BPR-1338]]. How many addresses the recording browser was handed.
		[[nodiscard]] static auto browser_opens(const github_application& aApplication) noexcept -> std::size_t;
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
		/// \brief [[BPR-1329]]. Holds, or releases, every inline publication's
		/// terminal on its way to the window, so a course can see what is shown
		/// while one is in flight.
		static void               hold_review_mutation_terminals(github_application& aApplication, bool aHold);
		[[nodiscard]] static auto held_review_mutation_terminals(const github_application& aApplication) -> std::size_t;
		/// \brief [[BPR-1558]]. Holds, or releases, every syntax classifier
		/// blob read at its start, as a slow read would hold it. A departure
		/// from the comparison still ends a held read, as it ends a real one.
		static void               hold_syntax_blob_reads(github_application& aApplication, bool aHold);
		[[nodiscard]] static auto held_syntax_blob_reads(const github_application& aApplication) -> std::size_t;
		[[nodiscard]] static auto review_mutation_evidence(
			const github_application& aApplication) noexcept
			-> review_mutation_fixture_evidence;
		/// \brief [[BPR-0990]]. The profile's discovery counters and status.
		[[nodiscard]] static auto personal_discovery_evidence(
			const github_application& aApplication) noexcept
			-> personal_discovery_fixture_evidence;
		/// \brief Reports the composed queue-to-platform notification evidence.
		[[nodiscard]] static auto queue_notification_evidence(
			const github_application& aApplication) noexcept
			-> queue_notification_fixture_evidence;
		/// \brief The runtime notification sink's platform tray icon, if composed.
		/// \details The profile runtime owns the sink, so the icon is not a child of
		/// any window (BPR-0705, [[BPR-0861]]).
		[[nodiscard]] static auto notification_tray(
			github_application& aApplication) noexcept -> QSystemTrayIcon*;
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
		/// \brief [[BPR-0863]]. Advances a course's step into the modality it states.
		///
		/// \details The first call triggers the strip's public modality action,
		/// as a reviewer's press does. Later calls answer `entered` once the
		/// window and the host agree -- the strip, the core's retained modality
		/// and all three drawers -- with no host request outstanding, so the
		/// drawer store the modality causes has landed. Once entered or failed
		/// the answer is sticky, so a course may call it on every tick. After
		/// about five seconds it fails by name on standard output, the line
		/// ending with `course_modality_facts`: `COURSE_MODALITY_WITHHOLDS` when
		/// the modality has settled but withholds a surface the step needs, and
		/// `COURSE_MODALITY_STALLED` when the window and the host never agreed.
		[[nodiscard]] static auto enter_course_modality(
			github_application&   aApplication,
			course_modality_step& aStep) -> course_modality_result;
		/// \brief [[BPR-0863]]. The modality facts a course's failure line ends with.
		///
		/// \details The window's modality and the host's, what the centre shows,
		/// which surfaces are offered and which withheld, each drawer in the
		/// window and in the host, the drawer-controls preference, outstanding
		/// host requests and the splitter sizes: enough to tell a course in the
		/// wrong modality from any other stall in one run.
		[[nodiscard]] static auto course_modality_facts(
			const github_application& aApplication) -> QString;
		/// \brief Tests exact authority retirement of a retained first-file choice.
		/// Uses the already-observed core pending state after the public course;
		/// a foreign invalidation keeps it and its exact authority retires it.
		[[nodiscard]] static auto verify_pending_first_file_choice_invalidation(
			github_application& aApplication) -> bool;
		/// \brief Observes the exact host, tab and displayed-publication identities.
		[[nodiscard]] static auto epoch_refresh_identity(
			const github_application& aApplication) -> epoch_refresh_identity_evidence;
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
		/// \brief [[BPR-1211]]. Whether this window's review ledger has
		/// answered its opening and holds no outstanding operation.
		[[nodiscard]] static auto review_ledger_settled(
			github_application& aApplication) -> bool;
		/// \brief [[BPR-1211]]. Holds the profile's review-ledger effect lane:
		/// operations posted after this queue behind it until the returned
		/// function releases it, or thirty seconds pass.
		[[nodiscard]] static auto hold_review_ledger_effects(
			github_application& aApplication) -> std::function<void()>;
		/// \brief [[BPR-1404]]. Holds private draft persistence on its existing
		/// effect lane until the returned function releases it, or thirty
		/// seconds pass. Decision delivery and public gestures keep turning.
		[[nodiscard]] static auto hold_private_review_effects(
			github_application& aApplication) -> std::function<void()>;
		/// \brief [[BPR-1254]]. Whether this window's review ledger answered
		/// its latest opening as available; empty while it has not answered.
		[[nodiscard]] static auto review_ledger_available(
			github_application& aApplication) -> std::optional<bool>;
		/// \brief [[BPR-1254]]. Whether this window's review ledger store and
		/// private review store take turns with the review-ledger file.
		///
		/// \details Two directions. A task held on the ledger's store lane keeps
		/// a private store question, asked from another thread, unanswered until
		/// the task is let go. The file exclusion held here keeps a ledger
		/// session-catalog read from finishing on its lane until it is let go.
		/// Empty when the window has no ledger or private review to ask, or a
		/// step did not finish within its bound.
		[[nodiscard]] static auto review_ledger_file_exclusive(
			github_application& aApplication) -> std::optional<bool>;
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
		/// \brief [[BPR-1083]]. Selects `aPath` in the files drawer in Read and
		/// waits for its comparison. Pending until the pull request's files are
		/// listed; the gesture is the drawer's own selection.
		[[nodiscard]] static auto reveal_fixture_file(github_application& aApplication, std::string_view aPath)
			-> fixture_file_reveal_result;
		/// \brief [[BPR-1331]]. The open pull request's accepted head, in
		/// hexadecimal, or empty before one is accepted.
		[[nodiscard]] static auto accepted_head(const github_application& aApplication) -> std::string;
		/// \brief [[BPR-1477]]. Where the shown pull request's read stands.
		[[nodiscard]] static auto window_read_progress(const github_application& aApplication) -> window_read_progress_evidence;
		/// \brief [[BPR-1293]]. For an attended look: \p aThread read through
		/// \p aComment, recorded through the review ledger as Mark read
		/// records it, so the posts after it are new. False when the accepted
		/// pull request has no such thread or post.
		static auto mark_fixture_read_through(github_application& aApplication, const std::string& aThread, const std::string& aComment) -> bool;
		/// \brief [[BPR-1293]]. Every thread of a constructed fixture read
		/// through the post its `read_state.json` names; nothing for a fixture
		/// without one, or one naming a thread or post the read lacks.
		static auto apply_fixture_read_state(github_application& aApplication, const std::string& aFixture) -> std::optional<std::size_t>;
		/// \brief [[BPR-1084]] and [[BPR-1085]]. For an attended look, in Read
		/// on a revealed file: a new comment on the first `aSide` line no thread
		/// is on, holding `aBody`, through the window's own Comment action and
		/// editor, the way a reviewer would write it. Shown once the draft and
		/// the editor hold `aBody`. Nothing is published.
		[[nodiscard]] static auto seed_attended_comment_draft(github_application& aApplication, core::diff_side aSide, const std::string& aBody)
			-> fixture_file_reveal_result;
		/// \brief [[BPR-1083]]. In Read on `src/engine/renderer.cpp`: the
		/// thread marks, then a new comment where a typed `:qu:` stays text
		/// ([[BPR-1565]]) and the chip and ⌘L label it,
		/// published, and its mark after a refresh. Saves window renders into
		/// `aEvidenceDirectory` when one is named.
		[[nodiscard]] static auto exercise_label_authoring(github_application& aApplication, const std::optional<std::string>& aEvidenceDirectory, bool aLineDraftOnly = false)
			-> label_authoring_probe_result;
		[[nodiscard]] static auto label_authoring_evidence(const github_application& aApplication)
			-> label_authoring_probe_evidence;
		[[nodiscard]] static auto label_authoring_diagnostic(const github_application& aApplication)
			-> std::string;
		/// \brief [[BPR-1326]]. In Review on `src/engine/renderer.cpp`: a new
		/// comment written, another file opened, and `renderer.cpp` opened
		/// again; the comment must still show as a note, a gutter marker and a
		/// scroll-bar tick. \p aTiming says whether it is published and when
		/// the other file is opened.
		[[nodiscard]] static auto exercise_published_file_switch(github_application& aApplication, published_file_switch_timing aTiming)
			-> published_file_switch_probe_result;
		[[nodiscard]] static auto published_file_switch_evidence(const github_application& aApplication)
			-> published_file_switch_probe_evidence;
		[[nodiscard]] static auto published_file_switch_diagnostic(const github_application& aApplication)
			-> std::string;
		/// \brief [[BPR-1374]]. One tick of the unsent-reply course.
		[[nodiscard]] static auto exercise_reply_draft(github_application& aApplication, reply_draft_course aCourse)
			-> reply_draft_probe_result;
		[[nodiscard]] static auto reply_draft_evidence(const github_application& aApplication)
			-> reply_draft_probe_evidence;
		[[nodiscard]] static auto reply_draft_diagnostic(const github_application& aApplication)
			-> std::string;
		/// \brief [[BPR-1478]]. One tick of the course in which a reread lands
		/// just after the reviewer picks another file.
		[[nodiscard]] static auto exercise_refresh_pick(github_application& aApplication, refresh_pick_course aCourse)
			-> refresh_pick_probe_result;
		[[nodiscard]] static auto refresh_pick_evidence(const github_application& aApplication)
			-> refresh_pick_probe_evidence;
		[[nodiscard]] static auto refresh_pick_diagnostic(const github_application& aApplication)
			-> std::string;
		/// \brief [[BPR-1345]]. One tick of the start-a-review course.
		[[nodiscard]] static auto exercise_start_review(github_application& aApplication, start_review_course aCourse)
			-> start_review_probe_result;
		[[nodiscard]] static auto start_review_evidence(const github_application& aApplication)
			-> start_review_probe_evidence;
		[[nodiscard]] static auto start_review_diagnostic(const github_application& aApplication)
			-> std::string;
		/// \brief [[BPR-1331]]. Opens `src/engine/renderer.cpp`,
		/// `src/main.cpp` and `src/ui/widgets/button.cpp` in Review in turn and
		/// reads the private progress button on each once its receipt is
		/// shown; `before` first accepts the first two and excludes the third.
		[[nodiscard]] static auto exercise_review_progress_push(github_application& aApplication, review_progress_push_course aCourse)
			-> review_progress_push_result;
		/// \brief Each file's path and the button's accessible name on it,
		/// `path=name` joined by `;`, plus the pull request's head.
		[[nodiscard]] static auto review_progress_push_marks(const github_application& aApplication)
			-> std::string;
		[[nodiscard]] static auto review_progress_push_diagnostic(const github_application& aApplication)
			-> std::string;
		[[nodiscard]] static auto dormant_live_provider_selection_evidence_for(
			const github_application& aApplication)
			-> dormant_live_provider_selection_evidence;
		[[nodiscard]] static auto review_mutation_provider_marker(
			const github_application& aApplication) noexcept -> std::string_view;
		[[nodiscard]] static auto exercise_defect_composition(
			github_application&      aApplication,
			defect_composition_probe aCourse) -> defect_composition_probe_result;
		/// \brief [[BPR-1211]]. The defect course's verify and reclassify
		/// steps, each read once its write is durable.
		[[nodiscard]] static auto defect_composition_after_verify(
			github_application& aApplication) -> defect_composition_probe_result;
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
		/// \brief The application session the composition gave one live window.
		///
		/// \details Each window session owns an application session distinct
		/// from the profile runtime's (BPR-0705), so a course derives the session
		/// it expects here rather than assuming the one-window value (BPR-0826).
		[[nodiscard]] static auto window_application_session(
			const github_application& aApplication,
			core::bpr_window_context  aContext) noexcept
			-> std::optional<core::application_session_id>;
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
