#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP

#include "bpr/application/notification_delivery.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace bpr::app {
	namespace detail {
		class github_application_fixture_access;
	}

	/// \brief Describes the github application config record.
	struct github_application_config
	{
		std::optional<std::string>           EnterpriseHostname  = std::nullopt;
		std::optional<std::filesystem::path> ApplicationDataRoot = std::nullopt;
		std::optional<std::filesystem::path> GitExecutable       = std::nullopt;
		std::optional<std::filesystem::path> ExistingCheckout    = std::nullopt;
		std::optional<std::filesystem::path> AttendedDesignatedLiveApproval
			= std::nullopt;
		std::filesystem::path RunningExecutable;
		std::string           AttendedSourceSha256;
		std::string           BuildConfiguration;
		bool                  BuildTesting = true;
	};

	/// \brief Initializes or deliberately resets the designated-live replay
	/// namespace below one explicit application-data root.
	///
	/// \details This is an operator operation, not an application-startup
	/// fallback. It returns no replay path or namespace identity, so neither can
	/// escape through the command line or diagnostics.
	[[nodiscard]] auto initialize_or_reset_designated_live_replay_state(
		const std::filesystem::path& aApplicationDataRoot) noexcept -> bool;

	/// \brief Describes the github application status record.
	struct github_application_status
	{
		bool Closing                       = false;
		bool ShutdownComplete              = false;
		bool ShutdownCleanupFailed         = false;
		bool AgentWorkspaceCleanupFailed   = false;
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
		std::string   AppearanceMode;
		std::uint32_t AppearanceTabWidth    = 0U;
		std::size_t   AppearanceDivergences = 0U;
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
	};

	/// \brief Provides the github application contract.
	class github_application final
	{
	public:
		explicit github_application(github_application_config aConfig = { });
		~github_application();

		/// \brief Expires the moment this application stops being usable.
		///
		/// \details `~implementation` pumps `processEvents` in its own shutdown
		/// loop, which delivers whatever timers were still armed when `exec()`
		/// returned. By then the owning `unique_ptr` has already been nulled —
		/// libc++'s `reset` assigns before invoking the deleter — so any queued
		/// callback that reaches back in through this class reads a member off
		/// a null pointer and the process segfaults.
		///
		/// Every poller that re-enters the application holds a weak reference
		/// to this and does nothing once it has expired. It is cleared in the
		/// destructor body, which runs before any member is destroyed, so the
		/// window between "unusable" and "says so" is empty.
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

		[[nodiscard]] auto window() noexcept -> qt::workbench_window&;
		[[nodiscard]] auto status() const noexcept -> github_application_status;

	private:
		class implementation;
		explicit github_application(
			std::unique_ptr<implementation> aImplementation) noexcept;

		friend class detail::github_application_fixture_access;

		// Declared before `Implementation` only for readability; what matters is
		// that the destructor body clears it, which precedes member destruction
		// regardless of order.
		std::shared_ptr<const int>      Liveness = std::make_shared<const int>(0);
		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
