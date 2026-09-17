#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"

#include <QObject>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QAction;

namespace bpr::qt {
	/// \brief Records directly observable session-presentation activity.
	struct github_session_panel_statistics
	{
		std::size_t CommandUpdates    = 0;
		std::size_t StatusUpdates     = 0;
		std::size_t IdentityUpdates   = 0;
		std::size_t CapabilityUpdates = 0;
		std::size_t SessionUpdates    = 0;
		std::size_t DiagnosticUpdates = 0;
	};

	/// \brief What the session presentation needs from the review content beside it.
	///
	/// \details Two facts, named rather than reached for. `HasPullRequest` is
	/// read because the status wording says something different while a pull
	/// request is on screen; `PresentationChanged` is called because a status
	/// or diagnostic change can alter which view state applies.
	///
	/// They exist because the seam is not as clean as it looks. See the note on
	/// `github_session_panel` below.
	struct github_session_environment
	{
		std::function<bool()> HasPullRequest;
		std::function<void()> PresentationChanged;
	};

	/// \brief The GitHub session as the window presents it: status, diagnostic,
	/// identity and the host a Connect or a new sign-in names.
	///
	/// \details Extracted from `github_window` by BPR-0195 so that BPR-0191 could
	/// move the session chrome into the workbench shell without also moving the
	/// pull-request content in the same commit.
	///
	/// **This does not implement `github_projection_port`, which BPR-0195 asked
	/// for.** A presentation update cannot be split into a chrome half and a
	/// content half that apply independently: inside one transition the order is
	/// commands, identity, capabilities, *clear the pull request*, status,
	/// diagnostics, and `apply_status` reads whether a pull request is on screen,
	/// so a content mutation sits between two session applications. The composing
	/// window keeps the routing and calls the appliers below in the order it
	/// already used, so that sequence is not re-derived anywhere.
	///
	/// [[BPR-0771]]. No longer a widget. RFC-BPR-0032 took the host picker, the
	/// locator, the status banner and the public-preview strip off Accounts &
	/// hosts: a host is chosen by its card's Connect, a pull request is named in
	/// the Home bar, a load refusal is said under that bar, a session failure is
	/// said in its card, and Refresh is the strip's. What remains here is the one
	/// decided state those surfaces read, so no two of them can disagree about
	/// the status, the diagnostic or which host is meant. It holds nothing beyond
	/// what the projections told it and the host last chosen.
	class github_session_panel final : public QObject
	{
	public:
		github_session_panel(
			std::vector<application::github_profile_option> aProfiles,
			application::github_intent_dispatch             aDispatch,
			QAction*                                        aConnectAction,
			QAction*                                        aOpenAction,
			QAction*                                        aRefreshAction,
			github_session_environment                      aEnvironment,
			QObject*                                        aParent = nullptr);

		void add_profile(application::github_profile_option aProfile);

		/// \brief Applies one command projection if it names a session command.
		///
		/// \returns Whether this model owns the command. The composing window
		/// handles the rest, so a command belonging to neither is a compile-time
		/// omission rather than a silently dropped projection.
		auto apply_command(const core::github_command_projection& aCommand) -> bool;
		void apply_identity(const core::github_identity_delta& aIdentity);
		void apply_status(core::github_status aStatus);
		void apply_diagnostic(
			core::github_reason_code     aReason,
			std::optional<std::uint32_t> aRetryAfter = std::nullopt,
			const std::optional<
				application::github_rate_limit_projection>& aRateLimit        = std::nullopt,
			bool                                            aHostDetail       = false,
			application::github_bootstrap_failure_stage     aBootstrapStage   = application::github_bootstrap_failure_stage::none,
			application::github_capability_diagnostic       aCapabilityDetail = application::github_capability_diagnostic::none);
		void apply_session(
			const application::github_session_projection& aSession);
		void apply_capabilities(const core::github_capabilities& aCapabilities);
		void apply_rate_limits(
			const application::github_rate_limits_projection& aLimits);
		/// \brief Adopts one public-preview presentation.
		///
		/// \details `aNowUnixSeconds` is the instant the composition measured;
		/// nothing here reads a clock.
		void apply_public_preview(
			const application::public_preview_presentation& aPresentation,
			std::uint64_t                                   aNowUnixSeconds);

		/// \brief Names the host the next Connect or sign-in means.
		///
		/// \details [[BPR-0771]]. What the picker's current row used to be. A
		/// profile this model was never given is ignored rather than guessed at.
		void select_profile(core::host_profile_id aProfile);
		void update_connect_enabled();

		[[nodiscard]] auto profiles() const noexcept
			-> const std::vector<application::github_profile_option>&;
		[[nodiscard]] auto profile_option(core::host_profile_id aProfile) const
			-> const application::github_profile_option*;
		/// \brief The profile the accepted identity names, if any.
		[[nodiscard]] auto active_profile() const noexcept
			-> std::optional<core::host_profile_id>;
		/// \brief The host the next Connect or sign-in names.
		[[nodiscard]] auto selected_profile() const
			-> std::optional<core::host_profile_id>;
		/// \brief The accepted session, while there is one.
		[[nodiscard]] auto session() const noexcept
			-> const std::optional<application::github_session_projection>&;
		[[nodiscard]] auto capabilities() const noexcept
			-> const core::github_capabilities&;
		[[nodiscard]] auto rate_limits() const noexcept
			-> const application::github_rate_limits_projection&;
		/// \brief Whether the core currently admits a Connect.
		[[nodiscard]] auto connect_enabled() const noexcept -> bool;
		/// \brief What a Connect does now, or why the core refuses one.
		[[nodiscard]] auto connect_description() const -> QString;
		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool;
		[[nodiscard]] auto public_preview() const noexcept
			-> const application::public_preview_presentation&;
		[[nodiscard]] auto public_preview_now() const noexcept -> std::uint64_t;
		/// \brief The refusal that ended the last signed-out read, empty when none.
		[[nodiscard]] auto public_preview_refusal_text() const -> QString;
		/// \brief Whether a diagnostic is currently being presented.
		///
		/// \details Distinct from `diagnostic_text`, which reports the last text
		/// whether or not it is presented. Both callers exist and they want
		/// different answers.
		[[nodiscard]] auto showing_diagnostic() const -> bool;
		/// \brief The status and any presented diagnostic, as one sentence.
		[[nodiscard]] auto summary_text() const -> QString;
		[[nodiscard]] auto status_text() const -> std::string;
		[[nodiscard]] auto diagnostic_text() const -> std::string;
		/// \brief The hostname of the active host, or of the selected one while
		/// none is active; empty when there is neither.
		[[nodiscard]] auto active_host_text() const -> std::string;
		/// \brief The account as the strip names it: "Connecting…", "Not
		/// authenticated", or the display name and login.
		[[nodiscard]] auto active_account_text() const -> std::string;
		[[nodiscard]] auto status() const noexcept -> core::github_status;
		[[nodiscard]] auto statistics() const noexcept
			-> github_session_panel_statistics;

	private:
		void clear_account(const QString& aAccountText);

		application::github_intent_dispatch             Dispatch;
		std::vector<application::github_profile_option> Profiles;
		github_session_environment                      Environment;

		QAction* ConnectAction = nullptr;
		QAction* OpenAction    = nullptr;
		QAction* RefreshAction = nullptr;

		std::optional<core::host_profile_id>                  ActiveProfile;
		std::optional<core::host_profile_id>                  SelectedProfile;
		std::optional<application::github_session_projection> Session;
		core::github_capabilities                             Capabilities;
		application::github_rate_limits_projection            RateLimits;
		application::public_preview_presentation              PublicPreview;
		std::uint64_t                                         PublicPreviewNow = 0;
		QString                                               AccountText;
		QString                                               StatusText;
		QString                                               DiagnosticText;
		std::optional<core::github_reason_code>               LastDiagnosticReason;
		bool                                                  LastDiagnosticHasHostDetail = false;
		bool                                                  ConnectCommandEnabled       = true;
		core::github_reason_code                              ConnectDisabledReason       = core::github_reason_code::none;
		core::github_status                                   CurrentStatus               = core::github_status::no_profile;

		github_session_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP
