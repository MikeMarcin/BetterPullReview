#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP

#include "bpr/app/release_maintenance_controls.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QSystemTrayIcon;

namespace bpr::qt {
	class dif_catalog_provider;
	class motion_observer;
}

namespace bpr::app {
	namespace detail {
		class github_application_fixture_access;
		struct window_context_less
		{
			[[nodiscard]] auto operator()(const core::bpr_window_context& aLeft, const core::bpr_window_context& aRight) const noexcept -> bool
			{
				if (aLeft.Profile != aRight.Profile) {
					return aLeft.Profile.value() < aRight.Profile.value();
				}
				if (aLeft.Window != aRight.Window) {
					return aLeft.Window.value() < aRight.Window.value();
				}
				return aLeft.Generation.value() < aRight.Generation.value();
			}
		};
	}

	/// \brief Describes the github application config record.
	struct github_application_config
	{
		std::shared_ptr<application::diagnostic_log_port> DiagnosticLog;
		std::optional<std::string>                        EnterpriseHostname  = std::nullopt;
		std::optional<std::filesystem::path>              ApplicationDataRoot = std::nullopt;
		std::optional<std::filesystem::path>              GitExecutable       = std::nullopt;
		std::optional<std::filesystem::path>              ExistingCheckout    = std::nullopt;
		std::optional<std::filesystem::path>              AttendedDesignatedLiveApproval
			= std::nullopt;
		std::filesystem::path RunningExecutable;
		std::string           AttendedSourceSha256;
		/// The stage-one supervisor and the capsule inventory the attended
		/// approval is granted against (BPR-0262 fact_5).
		std::string AttendedSupervisorSha256;
		std::string AttendedInventorySha256;
		std::string BuildConfiguration;
		bool        BuildTesting = true;
		/// [[BPR-0515]]. Denies this process's own outbound HTTPS locally.
		///
		/// \details The device-authorization wire is composed against a closed
		/// loopback proxy instead of the discovered system proxy, so the connect
		/// is refused on the host and the dev-tree gate reaches `failed` without
		/// anything leaving the machine. Only a tests-enabled build reads this:
		/// the composition branch that honours it, the fixture factory it
		/// selects, and the `--network-denied` flag that sets it are all behind
		/// the tests-enabled build guard, so a shipped binary has no way to set
		/// it and no code that would notice if it were set. The field itself
		/// stays outside that guard for one reason only - this header is
		/// included by a target that links the composition without defining
		/// the guard, and a conditionally-sized record passed by value across
		/// that seam would disagree about its own layout.
		bool                        NetworkDenied       = false;
		core::bpr_profile_id        Profile             = core::default_bpr_profile_id;
		core::profile_launch_intent ProfileLaunchIntent = core::profile_launch_intent::activate;
		/// One explicit process-owned native observation shared by profile windows.
		std::shared_ptr<qt::motion_observer> MotionObserver;
		/// One lazily decoded production catalog shared by every profile and window.
		std::shared_ptr<qt::dif_catalog_provider> DifCatalog;
		/// [[BPR-1250]]. The process's one menu bar status item, when the desktop
		/// keeps one (`native_menu_bar_status_item`). A profile's notification sink
		/// submits through it instead of owning a tray that would leave the menu
		/// bar with the profile's last window.
		std::shared_ptr<QSystemTrayIcon> StatusItem;
	};

	/// \brief Initializes or deliberately resets the designated-live replay
	/// namespace below one explicit application-data root.
	///
	/// \details This is an operator operation, not an application-startup
	/// fallback. It returns no replay path or namespace identity, so neither can
	/// escape through the command line or diagnostics.
	[[nodiscard]] auto initialize_or_reset_designated_live_replay_state(
		const std::filesystem::path& aApplicationDataRoot) noexcept -> bool;

	/// \brief Which profile refusal a startup actually saw, or empty.
	///
	/// \details [[BPR-0877]]. `github_application_startup_failure::registry` is
	/// the catch-all arm of the profile mapping, so several different refusals
	/// reach a reader under one name, and finding which needed a bisect. This
	/// names the one that fired, first refusal wins. Diagnostic only: every
	/// caller keeps reading the mapped failure it already reads.
	[[nodiscard]] auto profile_startup_refusal_name() -> std::string;

	/// \brief Which composition arm refused a startup, and in a tests-enabled
	/// graph what it caught, or empty.
	///
	/// \details [[BPR-0882]]. Every step between a profile's admission and its
	/// first window reports `github_application_startup_failure::unavailable`,
	/// from a runtime constructor that throws to a window that does not open,
	/// so that one word could not say which step it was. The desktop's own
	/// registry host reports `registry`, which the profile mapping also
	/// reports. Each arm records a fixed name before it refuses; the first
	/// refusal wins, as for the profile refusal above. A tests-enabled graph
	/// keeps at most 2 KiB of the exception the arm caught after the name, on one
	/// line: `profile-runtime-construction: Git cache composition failed`.
	/// ASCII controls become spaces and a truncated cause ends in `[truncated]`. A
	/// tests-disabled graph keeps the name only, because `what()` is free text
	/// that can carry a path. Diagnostic only: every caller keeps reading the
	/// mapped failure it already reads.
	[[nodiscard]] auto startup_failure_source() -> std::string;

	namespace detail {
		/// \brief Records the composition arm that is refusing a startup.
		///
		/// \details [[BPR-0882]]. `aSource` is a fixed name. A catch passes
		/// `std::current_exception()` as `aCause`, and a tests-enabled graph
		/// keeps its text. The first record wins and later ones are dropped.
		/// Never throws, so a catch block can call it.
		void note_startup_failure(std::string_view aSource, const std::exception_ptr& aCause = nullptr) noexcept;
	}

	/// \brief Whether this process runs inside the automated test boundary.
	///
	/// \details [[BPR-0802]], [[BPR-0983]]. The composition's one reader of
	/// the marker `bpr_add_network_isolated_test` injects into every
	/// registered row. A profile's composition, the updater and the update
	/// lifetime all ask it here rather than reading the environment
	/// themselves. A tests-disabled graph answers false and carries no name
	/// to read, so a stray variable changes nothing in a shipped build.
	[[nodiscard]] auto inside_automated_test_boundary() -> bool;

	/// \brief Names the device-authorization browser capability a run bound.
	///
	/// \details `DeviceAuthorizationBrowserComposed` answers whether anything
	/// was bound at all. That was true of the run the owner reported, and it
	/// told them nothing: what was bound was a recorder. This answers which
	/// one, and it is reported by the composition rather than inferred from the
	/// build's defines, because [[BPR-0797]] is the discovery that the defines
	/// were the wrong witness.
	enum class composed_device_browser : std::uint8_t {
		// Nothing bound. The construction cut, and the only state in which the
		// handoff may answer `unavailable`.
		none,
		// `bpr::app::system_browser_opener`, which hands the address to the
		// desktop. What a person running this application gets, in either graph.
		system,
		// The recording stand-in, which keeps the address, counts the call and
		// starts no program. Only a run inside the automated test boundary.
		recording,
	};

	/// \brief Describes the github application status record.
	struct github_application_status
	{
		bool Closing                     = false;
		bool ShutdownComplete            = false;
		bool ShutdownCleanupFailed       = false;
		bool AgentWorkspaceCleanupFailed = false;
		// [[BPR-0817]]. Whether startup recovery refused the profile's disposable
		// agent workspaces. It is reported separately from the cleanup flag above
		// because it is not a failure of the quit: every window states it, and an
		// orderly quit still exits 0.
		bool AgentWorkspaceRecoveryRefused = false;
		bool NativePlatformVaultSelected   = false;
		bool EpochProjectionDeliveryFailed = false;
		bool EpochHandoffDeliveryFailed    = false;
		// Whether the composition actually constructed the workbench host, as
		// opposed to merely linking it. `nm` cannot tell those apart: the code
		// is compiled and its symbols are present whether or not the
		// constructor ever runs. This is the runtime half of that evidence.
		bool WorkbenchHostComposed  = false;
		bool AppearanceHostComposed = false;
		bool AgentHostComposed      = false;
		/// Whether the appearance host is backed by its durable settings store.
		/// False means appearance remains available for this session, but changes
		/// cannot survive a restart.
		bool AppearancePersistenceAvailable = false;
		/// Durable settings writes completed by the appearance host. Session-only
		/// writes must never advance this evidence counter.
		std::size_t AppearanceDurableWrites = 0U;
		// The window palette's selection and code-background colours after the
		// profile was applied, as #rrggbb. Empty when no profile was applied.
		// Reported rather than a bare boolean because "a profile exists" and
		// "the palette reached the window" are different claims, and only the
		// second one matters.
		std::string AppliedSelectionColor;
		std::string AppliedBaseColor;
		// Whether those colours came from the appearance profile's semantic
		// roles rather than from the platform style. Reporting the colours
		// alone is not evidence: a window that was never given the profile
		// still has well-formed colours.
		bool AppliedFromProfile = false;
		// What the composition observed of the operating system, and what that
		// resolved to. Reported because the applied colours alone cannot say
		// *why* they are what they are: a System-mode window painted light on a
		// dark desktop looks identical to a correctly resolved light one, which
		// is how BPR-0216 stayed invisible behind an all-green marker set.
		core::os_color_scheme    ObservedScheme  = core::os_color_scheme::unknown;
		core::appearance_variant ResolvedVariant = core::appearance_variant::light;
		// Roles the profile believes a reviewer customised. On a fresh
		// installation this must be zero; anything else means a default record
		// was mistaken for a set of deliberate overrides.
		std::size_t CustomisedRoles = 0U;
		// Whether the composition constructed the packaged syntax classifier.
		// Separate from the count below because "the classifier exists" and "a
		// pane took its answer" fail independently: a classifier nothing asks
		// and a pane nothing tells look identical from a symbol table.
		bool        SyntaxClassifierComposed = false;
		std::size_t AdoptedClassifications   = 0U;
		// Complete-blob passes the appearance host actually submitted. It
		// advances exactly when lexical work was unavoidable, so a repaint that
		// moves it is a repaint that reparsed.
		std::size_t ClassificationSubmissions = 0U;
		// Where the deliveries that did not reach a pane went (BPR-0368).
		// Adopted alone cannot distinguish "everything arrived" from "half was
		// silently dropped": a publication whose comparison was already left,
		// whose spans were empty, or whose ordering the surface refused all
		// vanish without a trace the counters above would show.
		std::size_t ClassificationCacheHits  = 0U;
		std::size_t CoalescedClassifications = 0U;
		std::size_t DroppedClassifications   = 0U;
		std::size_t ClassificationFailures   = 0U;
		std::size_t ClassificationTerminals  = 0U;
		// Whether the comparison on screen right now actually carries spans,
		// per side. The composition and delivery counters are history; the
		// reviewer looks at the present, which is how a monochrome pane hid
		// behind an all-green marker set.
		bool DisplayedLeftClassified  = false;
		bool DisplayedRightClassified = false;
		// How many spans those panes carry. Presence alone is too weak to be the
		// check the reviewer needs: `definition_for_path` falls back to plain
		// text rather than failing, and a plain-text classification still
		// publishes a non-null list, so a pane with a span list can still be
		// entirely uncoloured. A count distinguishes them. BPR-0390.
		std::size_t DisplayedLeftSpans  = 0U;
		std::size_t DisplayedRightSpans = 0U;
		// Whether a comparison is on screen at all. Without it a span count of
		// zero is ambiguous between "shown and uncoloured", which is the defect,
		// and "nothing is shown", which is not.
		bool DisplayedComparison = false;
		// Adoptions taken on the display turn rather than on delivery.
		std::size_t DisplayAdoptedClassifications = 0U;
		// The appearance settings in effect, as the core's stable names and
		// values.
		//
		// Deliberately not "what the record held on entry". `restore()` is an
		// effect: it returns before the durable record has been read, so a
		// value sampled just after it names what the composition started from
		// and not what was stored. That marker was written, measured, and
		// found to report `system` on a process whose window was already
		// showing the stored dark palette.
		//
		// The restart claim survives the correction. A composition starts from
		// `system` and a tab width of four, so a run that makes no gesture and
		// reports anything else read it from the durable record.
		std::string          AppearanceMode;
		std::uint32_t        AppearanceTabWidth = 0U;
		core::dif_resolution DifResolution;
		std::size_t          DifCatalogLoadAttempts = 0U;
		std::size_t          DifCatalogDecodedClips = 0U;
		std::size_t          AppearanceDivergences  = 0U;
		// The composed profile's selected mode and the variant it resolved to.
		// BPR-0114's claim is about these two, not about the platform palette:
		// the existing native probe proves Qt's palette follows the operating
		// system, which it would whether or not this product had an appearance
		// model at all.
		core::appearance_mode    ProfileMode = core::appearance_mode::system;
		core::appearance_variant ProfileVariant
			= core::appearance_variant::light;
		// Whether the composition constructed the queue notification delivery
		// path, and what the desktop says about showing one. Reported rather
		// than assumed: a refused or absent notification surface is a normal
		// outcome and must not read as a failure.
		bool                                 NotificationDeliveryComposed = false;
		application::notification_permission NotificationPermission
			= application::notification_permission::refused;
		/// Queue transitions that produced a metadata-only notification request.
		std::size_t NotificationRequestsAttempted = 0U;
		/// Requests the selected platform sink accepted as attributable submissions.
		std::size_t NotificationRequestsSubmitted = 0U;
		/// Requests suppressed because a later identityless activation is ambiguous.
		std::size_t NotificationRequestsSuppressedUnattributable = 0U;
		/// Submitted targets the platform routed back through the notifier.
		std::size_t NotificationActivations = 0U;
		/// Activated targets selected and focused in the visible queue proxy.
		std::size_t NotificationSelections = 0U;
		/// Activated targets that were hidden or absent from the queue proxy.
		std::size_t        NotificationSelectionFailures = 0U;
		core::epoch_status EpochStatus                   = core::epoch_status::inactive;
		// Whether composition actually constructed the device-authorization
		// host, as opposed to merely linking it. The same distinction the
		// workbench flag above draws, for the same reason.
		bool DeviceAuthorizationHostComposed = false;
		// The stage presentation was last told about. On an unregistered
		// platform selecting device sign-in must reach `unavailable` rather
		// than doing nothing; on a registered platform a real flow begins.
		core::device_authorization_stage DeviceAuthorizationStage
			= core::device_authorization_stage::idle;
		// Whether the composition selected a registered device capability.
		// macOS selects the recorded distributable client since BPR-0318;
		// Linux and Windows remain unregistered until BPR-0310 and BPR-0311
		// flip their own platforms.
		bool DeviceAuthorizationCapabilityRegistered = false;
		// How many device-authorization submissions reached the refusing
		// unregistered transport. Zero always: on an unregistered platform the
		// capability gate refuses before a request is constructed, and on a
		// registered platform that transport is not wired at all, so this
		// counts the gate being bypassed rather than requests being sent.
		std::size_t DeviceAuthorizationTransportReached = 0U;
		// How many presentations the device host published. Zero would mean the
		// choice is invisible rather than typed.
		std::size_t DeviceAuthorizationPublications = 0U;
		// The registered course's evidence, and zero everywhere else because
		// only a tests-enabled build with the recorded grant can enroll.
		// `VaultStores` counts credentials crossing the vault boundary;
		// `DurableCopies` counts files under the application data root that
		// contain the enrolled token, which must be none — the vault is the
		// only place a credential belongs, and settings and SQLite are the two
		// places a second copy would plausibly appear.
		//
		// Whether composition bound a browser capability to the handoff, as
		// opposed to linking one. A shipped build binds the system opener; a
		// tests-enabled build binds a recording one so no suite can start a
		// browser; the construction cut binds none, and only then may the
		// action answer `unavailable`.
		bool DeviceAuthorizationBrowserComposed = false;
		// [[BPR-0797]]. Which one of them, which is the question the line above
		// cannot answer and the owner's report turned on.
		composed_device_browser DeviceAuthorizationBrowser
			= composed_device_browser::none;
		// [[BPR-0797]]. Whether the composed wire-transport factory can make a
		// GitHub CLI read at all. Read off the factory this run actually holds,
		// not off the platform or the build: a factory that answers false is a
		// GitHub CLI profile with nothing behind it, which is what the account
		// row was calling `(unavailable)`.
		bool ComposedCliReadAvailable = false;
		/// [[BPR-0797]], [[BPR-0802]]. Whether this run is standing inside the
		/// automated test boundary, as the composition itself decided it.
		///
		/// \details The composition needs to know whether it is in a suite or
		/// in somebody's hands, and it used to ask `BPR_BUILD_TESTING`. That
		/// define answers whether the build was configured with tests, and
		/// `BUILD_TESTING` is on by default, so every build a developer makes
		/// bound a recording browser that reports success without opening
		/// anything and offered the GitHub CLI profile as `(unavailable)`.
		///
		/// The answer comes from the marker `bpr_add_network_isolated_test`
		/// injects into every registered row, and it is read in exactly one
		/// place: the composition, where the browser and the CLI factory are
		/// chosen. It is reported here rather than passed in, so no composition
		/// root -- the application's `main` or an in-process test -- can hold a
		/// different answer from the one the composition acted on.
		///
		/// Only a tests-enabled build sets this; a shipped build has no
		/// boundary and always reports false. It stays outside the build guard
		/// because this header is included by a target that links the
		/// composition without defining the guard, and a conditionally-sized
		/// record passed by value across that seam would disagree about its
		/// own layout.
		bool InsideAutomatedTestBoundary = false;
		// The fate of the last handoff attempt in this run, kept across the
		// stage change that clears it from the live presentation. Reported
		// rather than the current value, because a run that finished
		// authorizing would otherwise show `not_attempted` for an attempt
		// that did happen.
		application::device_authorization_handoff DeviceAuthorizationHandoff
			= application::device_authorization_handoff::not_attempted;
		// Addresses handed to the recording opener. Zero in a shipped build,
		// which has no recording opener; the system opener counts nothing,
		// because its evidence is a browser window on a station rather than a
		// number this process can read.
		std::size_t DeviceAuthorizationBrowserOpens         = 0U;
		std::size_t DeviceAuthorizationVaultStores          = 0U;
		std::size_t DeviceAuthorizationDurableCopies        = 0U;
		std::size_t DeviceAuthorizationTransportSubmissions = 0U;
		// [[BPR-0539]]. Which material the one counted store holds, rather
		// than how many stores happened. The digest is a truncated SHA-256 of
		// the bytes the vault accepted; the flag is that digest compared
		// against the recorded grant's own access token, which is a
		// comparison only the fixture can make and only after the exchange
		// has answered. Empty and false in a shipped build, which has no
		// recorded grant to compare against.
		std::string DeviceAuthorizationVaultStoredDigest;
		bool        DeviceAuthorizationVaultStoredIsGrant = false;
		// Credentials that crossed the enrollment boundary. Counted there
		// rather than at the vault, because the GitHub host collapses a
		// repeated enrollment for the same profile before the vault sees it —
		// so a vault store count alone cannot tell one enrollment from two.
		std::size_t DeviceAuthorizationEnrollments = 0U;
		// Whether composition adopted the window's device sign-in panel, as
		// opposed to the window having mounted one. [[BPR-0526]]: the panel is
		// drawn either way, so the fact worth reporting is whether its gestures
		// reach the device host and whether the host's publications reach it.
		// The construction cut turns exactly this false and leaves every line
		// above it as it was.
		bool DeviceAuthorizationPanelComposed = false;
		// [[BPR-0536]]. The renewal lifecycle, as composition observed it
		// rather than as the host holds it. Rotations and state come from the
		// published observation, so a graph that stopped delivering
		// `publish_refresh` reports zero and idle no matter what the host did;
		// the two counters below come from composition's own arming, so a
		// graph that stopped calling `bind_refresh_authority` or
		// `resume_refresh` reports zero no matter what the observation says.
		// Kept apart because "renewal happened" and "composition asked for it"
		// fail for different reasons.
		std::size_t DeviceAuthorizationRefreshRotations = 0U;
		application::device_authorization_refresh_state
			DeviceAuthorizationRefreshState
			= application::device_authorization_refresh_state::idle;
		std::size_t DeviceAuthorizationRefreshBindings = 0U;
		std::size_t DeviceAuthorizationRefreshResumes  = 0U;
		// RFC-BPR-0017. Whether composition handed the host a credential-free
		// read authority at all. False on every shipped platform in this
		// candidate slice, which is exactly what makes the signed-out mode
		// answer `public_preview_unavailable` rather than degrade.
		bool PublicPreviewComposed = false;
		// The typed answer the last activation produced, as three exclusive
		// facts rather than one verdict: the composition refusal, the core
		// policy's refusal, and the mode actually being entered all fail for
		// different reasons and a single flag would hide which.
		bool PublicPreviewUnavailable   = false;
		bool PublicPreviewPolicyRefused = false;
		bool PublicPreviewSignedOut     = false;
		// Presentations the host published. Zero would mean the mode's answer
		// is invisible rather than typed.
		std::size_t PublicPreviewPublications = 0U;
		// Credential-free pull-request reads that reached the anonymous
		// authority. Zero on an unavailable platform: the authority is
		// composed either way, so this counts the gate being bypassed rather
		// than requests being sent.
		std::size_t PublicPreviewRequests = 0U;
		// Authenticated transports the session selected, and credential
		// operations that crossed the vault boundary, during the run. Both
		// must be zero for a signed-out read: an anonymous request carries no
		// authorization header and consults no vault, and these two counters
		// are where that stops being an assertion.
		std::size_t PublicPreviewHeaderEffects = 0U;
		std::size_t PublicPreviewVaultEffects  = 0U;
		// Smart-Git leases the anonymous factory issued. Zero unless a
		// signed-out session materialised a repository.
		std::size_t PublicPreviewSmartAcquisitions = 0U;
		// Switch-to-sign-in gestures the host forwarded to composition.
		std::size_t PublicPreviewSignInRequests = 0U;
		// Whether the last publication reported an observed inline review
		// conversation, and how many groups it carried. An unobservable
		// conversation and an observed empty one are different answers.
		bool        PublicPreviewReviewCommentsObserved = false;
		std::size_t PublicPreviewReviewCommentGroups    = 0U;
		// Write intents the mode refused. Counted rather than asserted,
		// because the claim is that the refusal happened at all: an intent
		// that was answered by a request nobody could serve would show up
		// here as a zero and in the fixture's evidence as a request.
		std::size_t PublicPreviewWriteRefusals = 0U;
		// [[BPR-0316]] live-candidate evidence. Whether the anonymous read
		// authority the host was handed is the one this composition built from
		// the shipped native factory, checked by pointer identity against what
		// the anonymous provider was actually constructed with rather than by
		// naming a call site. A composition that routed the signed-out read
		// through any other factory — a fixture seam, a loopback test twin —
		// hands the provider a different object and turns this false.
		bool PublicPreviewLiveCandidateProductionFactories = false;
		// REST requests the anonymous transport itself issued, counted at the
		// wire boundary rather than at the pull-request port above it, and
		// capability probes it was asked for. The second must stay zero: a
		// signed-out read builds no capability request.
		std::size_t PublicPreviewWireRequests         = 0U;
		std::size_t PublicPreviewWireCapabilityProbes = 0U;
		// Credentialed transports the anonymous factory was asked to build.
		// It has no such form and refuses both, so this counts a composition
		// reaching for an authorization header rather than one obtaining it.
		std::size_t PublicPreviewWireCredentialedRefusals = 0U;
		// The shared hourly core budget the session projection carries, taken
		// from the response headers the host observed. Absent until a response
		// reported one, which is why the observation is a separate fact from
		// the three numbers.
		bool          PublicPreviewBudgetObserved         = false;
		std::uint64_t PublicPreviewBudgetLimit            = 0U;
		std::uint64_t PublicPreviewBudgetRemaining        = 0U;
		std::uint64_t PublicPreviewBudgetResetUnixSeconds = 0U;
		// The mode the accepted snapshot was read in, and the head revision it
		// named. The mode is the snapshot's own discriminant, not the
		// session's: a snapshot accepted under an authenticated authority
		// would report false here even if the session said signed out.
		bool        PublicPreviewSnapshotPublicPreview = false;
		std::string PublicPreviewSnapshotHeadOid;
	};

	/// \brief Describes one REST request the signed-out live-candidate
	/// transport put on the wire, and what came back for it.
	///
	/// \details [[BPR-0316]]. The counted total says how much of the shared
	/// hourly budget a candidate read spent; it does not say which operation
	/// spent it or which one failed, and a run whose budget expires mid-read
	/// reports nothing else. One record per forwarded request answers both.
	/// It carries no body, no URL and no header beyond the rate-limit triple
	/// a failure named, because this output goes on a card.
	struct public_preview_wire_operation_record
	{
		/// The read operation's contract name, as `operation_name` spells it.
		std::string   Operation;
		std::uint32_t Page = 1U;
		/// `http_<status>` for a response, `error_<failure>` with the
		/// retry-after and rate-limit triple it carried, or `cancelled`.
		std::string   Outcome;
		std::uint64_t ElapsedMilliseconds = 0U;
	};

	/// \brief Checks in process that the live-candidate transport forwards one
	/// attempt only and caps successful pagination at nine requests.
	///
	/// \details [[BPR-0316]]. Drives the counting transport over a stub that
	/// always fails, and answers whether the second request was refused
	/// without reaching the stub. A successful paginated stub also proves the
	/// tenth page is refused across replacement transports sharing the budget.
	/// Nothing here opens a socket or names a host,
	/// so the single-attempt claim the live candidate prints is proven by the
	/// isolated dry run rather than by the authorized read it bounds. False in
	/// a build without the testing guard, where no such transport exists.
	[[nodiscard]] auto public_preview_single_attempt_self_check() noexcept
		-> bool;

	class github_application;
	class profile_desktop;
	class github_profile_runtime;
	enum class github_application_startup_failure { owner_busy,
													activation_queued,
													activation_refused,
													activation_indeterminate,
													invalid_profile,
													registry,
													root,
													unavailable,
													cancelled };
	using github_application_startup_result     = std::expected<std::unique_ptr<github_application>, github_application_startup_failure>;
	using github_application_startup_completion = std::function<void(github_application_startup_result)>;
	class github_application_startup final
	{
	public:
		~github_application_startup();
		github_application_startup(const github_application_startup&)                                  = delete;
		auto               operator=(const github_application_startup&) -> github_application_startup& = delete;
		void               cancel();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		class implementation;
		explicit github_application_startup(std::shared_ptr<implementation> aImplementation);
		std::shared_ptr<implementation> Implementation;
		friend class github_application;
		friend class detail::github_application_fixture_access;
	};

	/// \brief Provides the github application contract.
	class github_application final
	{
	public:
		[[nodiscard]] static auto start(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>;
		~github_application();

		/// \brief Expires the moment this application stops being usable.
		///
		/// \details Destruction pumps `processEvents` while it drains shutdown,
		/// which delivers whatever timers were still armed when `exec()`
		/// returned. When BPR-0217 added this token, that pump ran inside
		/// `~implementation` after libc++'s `reset` had nulled the owning handle,
		/// so a queued callback reaching back in through this class read a
		/// member off a null pointer and the process segfaulted. Since BPR-0705
		/// the drain runs in `~github_application` itself, over windows that are
		/// already shutting down.
		///
		/// Every poller that re-enters the application holds a weak reference
		/// to this and does nothing once it has expired. It is cleared first in
		/// the destructor body, before shutdown begins and before anything is
		/// pumped, so the window between "unusable" and "says so" is empty.
		/// Internal callbacks that must still run during that drain use their
		/// own guard, never this one (BPR-0827).
		///
		/// BPR-0215 first fixed this with a token local to one scheduler, and a
		/// second scheduler with the identical shape crashed the same way an
		/// hour later. One mechanism, owned by the thing whose lifetime it
		/// describes, is what stops that recurring.
		[[nodiscard]] auto liveness() const noexcept -> std::weak_ptr<const void>;

		github_application(const github_application&)                    = delete;
		auto operator=(const github_application&) -> github_application& = delete;
		github_application(github_application&&)                         = delete;
		auto operator=(github_application&&) -> github_application&      = delete;

		void show();
		void begin_shutdown();
		void request_quit(int aExitCode = 0);

		/// \brief Selects the device-authorization path once, as a reviewer
		/// would.
		///
		/// \details The public entry the RFC-BPR-0016 zero-request evidence
		/// runs through. It dispatches the same typed intent the Qt panel
		/// dispatches and returns without waiting; the outcome is read from
		/// `status()`. On an unregistered platform the capability gate refuses
		/// before a request is constructed; on a registered platform a real
		/// flow begins under the recorded distributable client.
		void select_device_authorization();

		/// \brief Cancels the in-flight device-authorization flow, if any.
		///
		/// \details The same typed intent the Qt panel dispatches for the
		/// user's cancel. The device probe uses it to end a registered
		/// platform's real flow inside its run budget; on an unregistered
		/// platform there is never a live flow to cancel.
		void cancel_device_authorization();

		/// \brief Asks the composed browser capability to open the address.
		///
		/// \details The same request the Qt panel's "Open browser" action
		/// makes. It changes no core state: the host revalidates the retained
		/// address against the compiled authority, invokes the capability only
		/// if that passes, and republishes what happened. Every outcome —
		/// including a refused address and an absent capability — leaves the
		/// flow exactly where it was, so this can be called on a live grant
		/// without risking it.
		void open_device_verification_address();

		/// \brief Activates the signed-out preview once, as a reviewer would.
		///
		/// \details RFC-BPR-0017's zero-request evidence runs through here. It
		/// asks the host to enter the mode for one profile and returns without
		/// waiting; the answer is read from `status()`. Where the shipped
		/// selection composed no credential-free read authority the host
		/// refuses `public_preview_unavailable` before the core is asked
		/// anything, and nothing of any kind is sent.
		///
		/// \param aEnterprise Selects the first enterprise profile instead of
		/// GitHub.com, which is refused by the core policy rather than by the
		/// composition. Does nothing when no enterprise profile is composed.
		void activate_public_preview(bool aEnterprise = false);

		/// \brief Offers the mode one mutation and one triage intent.
		///
		/// \details Both are refused by the core policy, and the refusal is
		/// the whole of the answer: nothing is queued, no transport is asked
		/// for, and no request is built, so a run that offers them and then
		/// reports zero requests has shown where the refusal happened. Asking
		/// costs nothing outside the process, which is what makes it safe to
		/// do in the middle of a read.
		void attempt_public_preview_writes();

		/// The compatibility accessor names the initial session only.
		/// Throws after that session retires; it never selects a sibling.
		[[nodiscard]] auto window() -> qt::workbench_window&;
		[[nodiscard]] auto windows() const -> std::vector<core::bpr_window_context>;
		[[nodiscard]] auto window(core::bpr_window_context aContext) noexcept -> qt::workbench_window*;
		[[nodiscard]] auto status(core::bpr_window_context aContext) const noexcept -> std::optional<github_application_status>;
		[[nodiscard]] auto status() const noexcept -> github_application_status;
		/// \brief Which cleanup refused, where, and what the platform said.
		///
		/// \details [[BPR-0875]]. The flag `status()` carries has three sources in
		/// a window's shutdown (the durable layout write, the layout erase and
		/// the MCP snapshot removal) and the profile folds in two more. Thirteen
		/// Windows rows reported the bare flag, so which one fired needed a
		/// bisect rather than a run. The first refusal is named here as
		/// `<source>:<path-or-window>:<error>`; empty means nothing refused. An
		/// accessor rather than a field on `github_application_status`, because
		/// that record crosses a seam by value and must keep its layout.
		[[nodiscard]] auto shutdown_cleanup_detail() const -> std::string;

		/// \brief Reports what the signed-out live-candidate transport
		/// forwarded, in the order it forwarded it.
		///
		/// \details [[BPR-0316]]. Kept off `status()` because it is a growing
		/// record rather than a fact of the moment, and the status record is
		/// copied on every tick of an automated run. Empty in every
		/// composition that did not build the counting transport, which is
		/// every shipped one.
		[[nodiscard]] auto public_preview_wire_operations() const
			-> std::vector<public_preview_wire_operation_record>;

	private:
		class implementation;
		explicit github_application(
			std::shared_ptr<github_profile_runtime> aProfile) noexcept;

		friend class detail::github_application_fixture_access;
		friend class github_application_startup;
		friend class profile_desktop;
		[[nodiscard]] auto desktop_registry_root() const -> std::filesystem::path;
		[[nodiscard]] auto desktop_update_recovery_root() const -> std::filesystem::path;
		[[nodiscard]] auto desktop_update_drafts() const -> std::vector<application::release_update_saved_draft>;
		[[nodiscard]] auto desktop_update_ready() const noexcept -> bool;
		void               desktop_update_freeze(bool aFreeze);
		[[nodiscard]] auto desktop_release_account() -> std::optional<release_update_account>;
		/// \brief Borrow the admitted runtime's bounded local activation endpoint.
		[[nodiscard]] auto desktop_activation_server() const -> std::shared_ptr<application::profile_activation_server>;
		/// \brief Borrow the admitted runtime's separate versioned tour command endpoint and its owner-lane handler.
		[[nodiscard]] auto desktop_tour_owner_server() const -> std::shared_ptr<application::tour_owner_server>;
		/// \brief Routes one admitted tour command on the decision lane: to the live window draft when the
		/// desktop holds it, otherwise to the profile store. `aRespond` runs exactly once, on any lane.
		void desktop_tour_owner_route(application::tour_channel_bytes aRequest, std::function<void(application::tour_channel_bytes)> aRespond);
		/// \brief Completes routed commands waiting for a live draft to become durable; decision lane.
		void                      desktop_tour_owner_tick();
		[[nodiscard]] auto        desktop_scratch() const -> std::shared_ptr<void>;
		void                      set_desktop_quit_handler(std::function<void(int)> aHandler);
		void                      set_maintenance_retirement(release_maintenance_retirement aRetire);
		[[nodiscard]] auto        take_maintenance() -> std::unique_ptr<release_maintenance_controls>;
		[[nodiscard]] static auto start_profile(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>;
		[[nodiscard]] auto        open_window(core::bpr_window_context aContext) -> bool;
		enum class window_retirement { forced,
									   explicit_close,
									   orderly_quit };
		void               close_window(core::bpr_window_context aContext, window_retirement aMode = window_retirement::forced);
		[[nodiscard]] auto configure_desktop_layouts(const std::vector<core::bpr_window_context>& aWindows) -> bool;
		void               focus_window(core::bpr_window_context aContext);
		[[nodiscard]] auto window_quiescent(core::bpr_window_context aContext) const noexcept -> bool;
		[[nodiscard]] auto profile_shutdown_complete() const noexcept -> bool;
		[[nodiscard]] auto profile_shutdown_cleanup_failed() const noexcept -> bool;
		/// \brief Which of the profile's cleanups refused first, or empty.
		///
		/// \details [[BPR-0875]]. The flag above becomes the desktop's exit 1, and
		/// five sources can raise it, so the one that did is named here.
		[[nodiscard]] auto profile_shutdown_cleanup_detail() const -> std::string;
		void               set_window_admission(std::function<bool(core::bpr_window_context)> aAdmission);
		void               set_profile_label(std::string aLabel);

		/// [[BPR-1168]]. Runs one forwarded cycle command on the profile's cycle-owner lane.
		void desktop_cycle_owner_route(application::tour_channel_bytes aRequest, application::cycle_owner_request aCycle, std::function<void(application::tour_channel_bytes)> aRespond);
		/// What the open windows hold of a cycle's round draft that the store does not;
		/// with `aSave`, unsaved text is saved as Return would. Decision lane.
		[[nodiscard]] auto cycle_draft_editors(core::review_cycle_id aCycle, bool aSave) -> core::cycle_draft_editor;
		/// Windows showing a cycle a forwarded command wrote read it again. Decision lane.
		void cycle_written(core::review_cycle_id aCycle);

		// Declared before `Implementation` only for readability; what matters is
		// that the destructor body clears `Liveness` before shutdown begins and
		// `SessionCloseLifetime` after the shutdown drain, both of which precede
		// member destruction regardless of order. `SessionCloseLifetime` guards
		// only the window-session close callbacks, which the drain itself must
		// still deliver (BPR-0827).
		std::shared_ptr<const int>                                                                       Liveness             = std::make_shared<const int>(0);
		std::shared_ptr<const int>                                                                       SessionCloseLifetime = std::make_shared<const int>(0);
		std::shared_ptr<github_profile_runtime>                                                          Profile;
		std::map<core::bpr_window_context, std::shared_ptr<implementation>, detail::window_context_less> Sessions;
		std::shared_ptr<implementation>                                                                  Implementation;
		bool                                                                                             InitialRetired = false;
	};
}

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
