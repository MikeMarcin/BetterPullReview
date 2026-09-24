#include "bpr/qt/github_session_panel.hpp"

#include "bpr/qt/usage_ring.hpp"

#include "github_presentation_text.hpp"

#include <QAction>

#include <algorithm>
#include <ranges>
#include <utility>

namespace bpr::qt {
	using namespace detail;

	namespace {
		[[nodiscard]] auto capability_detail_text(
			const application::github_capability_diagnostic aDetail) -> QString
		{
			switch (application::bounded_capability_diagnostic(aDetail)) {
			case application::github_capability_diagnostic::none:
			case application::github_capability_diagnostic::unavailable_detail:
				return QStringLiteral("No more specific capability refusal detail is available.");
			case application::github_capability_diagnostic::transport_content_encoding:
				return QStringLiteral("The response used an unsupported content encoding.");
			case application::github_capability_diagnostic::transport_duplicate_metadata:
				return QStringLiteral("The response contained duplicate HTTP metadata.");
			case application::github_capability_diagnostic::transport_invalid_metadata:
				return QStringLiteral("The response contained invalid HTTP metadata.");
			case application::github_capability_diagnostic::transport_missing_rate_tuple:
				return QStringLiteral("The response omitted required rate-limit metadata.");
			case application::github_capability_diagnostic::transport_partial_rate_tuple:
				return QStringLiteral("The response contained incomplete rate-limit metadata.");
			case application::github_capability_diagnostic::transport_wrong_rate_resource:
				return QStringLiteral("The response named an unexpected rate-limit resource.");
			case application::github_capability_diagnostic::transport_invalid_content_type:
				return QStringLiteral("The response did not declare a supported JSON content type.");
			case application::github_capability_diagnostic::transport_header_limit:
				return QStringLiteral("The response exceeded an HTTP header bound.");
			case application::github_capability_diagnostic::transport_body_limit:
				return QStringLiteral("The transport response exceeded a body bound.");
			case application::github_capability_diagnostic::transport_body_length:
				return QStringLiteral("The response body length disagreed with its metadata.");
			case application::github_capability_diagnostic::normalizer_invalid_limits:
				return QStringLiteral("The capability parser limits were invalid.");
			case application::github_capability_diagnostic::normalizer_transport_unavailable:
				return QStringLiteral("The capability parser received an unavailable transport result.");
			case application::github_capability_diagnostic::normalizer_rate_metadata:
				return QStringLiteral("The capability parser rejected the rate-limit metadata.");
			case application::github_capability_diagnostic::normalizer_body_limit:
				return QStringLiteral("The capability response exceeded the parser byte bound.");
			case application::github_capability_diagnostic::json_limits:
				return QStringLiteral("The capability JSON exceeded a structural bound.");
			case application::github_capability_diagnostic::json_shape:
				return QStringLiteral("The capability response had invalid JSON structure.");
			case application::github_capability_diagnostic::graphql_errors:
				return QStringLiteral("The capability response contained a GraphQL error envelope.");
			case application::github_capability_diagnostic::graphql_errors_not_array:
				return QStringLiteral("The capability errors field was not an array.");
			case application::github_capability_diagnostic::graphql_errors_empty:
				return QStringLiteral("The capability response contained an empty errors array.");
			case application::github_capability_diagnostic::graphql_errors_malformed:
				return QStringLiteral("The capability response contained a malformed error entry.");
			case application::github_capability_diagnostic::graphql_errors_mixed:
				return QStringLiteral("The capability response contained mixed or unclassified error codes.");
			case application::github_capability_diagnostic::graphql_code_forbidden:
				return QStringLiteral("The capability response reported GraphQL error code FORBIDDEN.");
			case application::github_capability_diagnostic::graphql_code_unauthorized:
				return QStringLiteral("The capability response reported GraphQL error code UNAUTHORIZED.");
			case application::github_capability_diagnostic::graphql_code_not_found:
				return QStringLiteral("The capability response reported GraphQL error code NOT_FOUND.");
			case application::github_capability_diagnostic::graphql_code_max_node_limit_exceeded:
				return QStringLiteral("The capability response reported GraphQL error code MAX_NODE_LIMIT_EXCEEDED.");
			case application::github_capability_diagnostic::graphql_code_internal:
				return QStringLiteral("The capability response reported GraphQL error code INTERNAL.");
			case application::github_capability_diagnostic::graphql_code_service_unavailable:
				return QStringLiteral("The capability response reported GraphQL error code SERVICE_UNAVAILABLE.");
			case application::github_capability_diagnostic::graphql_code_parse_failed:
				return QStringLiteral("The capability response reported GraphQL error code GRAPHQL_PARSE_FAILED.");
			case application::github_capability_diagnostic::graphql_code_validation_failed:
				return QStringLiteral("The capability response reported GraphQL error code GRAPHQL_VALIDATION_FAILED.");
			case application::github_capability_diagnostic::graphql_code_undefined_field:
				return QStringLiteral("The capability response reported GraphQL error code undefinedField.");
			case application::github_capability_diagnostic::graphql_code_argument_not_accepted:
				return QStringLiteral("The capability response reported GraphQL error code argumentNotAccepted.");
			case application::github_capability_diagnostic::graphql_code_undefined_type:
				return QStringLiteral("The capability response reported GraphQL error code undefinedType.");
			case application::github_capability_diagnostic::graphql_code_selection_mismatch:
				return QStringLiteral("The capability response reported GraphQL error code selectionMismatch.");
			case application::github_capability_diagnostic::graphql_code_variable_mismatch:
				return QStringLiteral("The capability response reported GraphQL error code variableMismatch.");
			case application::github_capability_diagnostic::graphql_code_variable_not_used:
				return QStringLiteral("The capability response reported GraphQL error code variableNotUsed.");
			case application::github_capability_diagnostic::graphql_code_variable_requires_valid_type:
				return QStringLiteral("The capability response reported GraphQL error code variableRequiresValidType.");
			case application::github_capability_diagnostic::graphql_code_variable_not_defined:
				return QStringLiteral("The capability response reported GraphQL error code variableNotDefined.");
			case application::github_capability_diagnostic::graphql_data:
				return QStringLiteral("The capability response lacked an object data envelope.");
			case application::github_capability_diagnostic::graphql_aliases:
				return QStringLiteral("The capability response did not contain the exact expected aliases.");
			case application::github_capability_diagnostic::schema_limits:
				return QStringLiteral("The capability schema exceeded a selected schema bound.");
			case application::github_capability_diagnostic::schema_shape:
				return QStringLiteral("The capability response had an invalid schema shape.");
			case application::github_capability_diagnostic::normalization_allocation:
				return QStringLiteral("The capability parser could not allocate bounded working storage.");
			case application::github_capability_diagnostic::normalization_unexpected:
				return QStringLiteral("The capability parser encountered an unexpected local failure.");
			case application::github_capability_diagnostic::provider_request_compilation:
				return QStringLiteral("The capability provider could not compile the selected request.");
			case application::github_capability_diagnostic::provider_correlation:
				return QStringLiteral("The capability provider received an inconsistent response correlation.");
			case application::github_capability_diagnostic::provider_metadata:
				return QStringLiteral("The capability provider rejected inconsistent response metadata.");
			case application::github_capability_diagnostic::provider_rate_tuple:
				return QStringLiteral("The capability provider rejected the required rate-limit tuple.");
			case application::github_capability_diagnostic::provider_retry_metadata:
				return QStringLiteral("The capability provider rejected inconsistent retry metadata.");
			case application::github_capability_diagnostic::provider_observation:
				return QStringLiteral("The capability provider received an invalid normalized observation.");
			case application::github_capability_diagnostic::host_epoch:
				return QStringLiteral("The local capability observation counter was exhausted.");
			case application::github_capability_diagnostic::host_cache_lookup:
				return QStringLiteral("The local capability cache lookup was refused.");
			case application::github_capability_diagnostic::host_correlation:
				return QStringLiteral("The application received an inconsistent capability request correlation.");
			case application::github_capability_diagnostic::host_provider_error:
				return QStringLiteral("The application rejected an invalid capability error record.");
			case application::github_capability_diagnostic::host_rate_state:
				return QStringLiteral("The application rejected the capability rate-limit state.");
			case application::github_capability_diagnostic::host_cache_stage:
				return QStringLiteral("The application could not stage the capability observation.");
			}
			return QStringLiteral("No more specific capability refusal detail is available.");
		}
	}

	github_session_panel::github_session_panel(
		std::vector<application::github_profile_option> aProfiles,
		application::github_intent_dispatch             aDispatch,
		QAction* const                                  aConnectAction,
		QAction* const                                  aOpenAction,
		QAction* const                                  aRefreshAction,
		github_session_environment                      aEnvironment,
		QObject* const                                  aParent)
		: QObject{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, Profiles{ std::move(aProfiles) }
		, Environment{ std::move(aEnvironment) }
		, ConnectAction{ aConnectAction }
		, OpenAction{ aOpenAction }
		, RefreshAction{ aRefreshAction }
		, AccountText{ QStringLiteral("Not authenticated") }
		, StatusText{ QStringLiteral("No GitHub host is connected.") }
	{
		setObjectName(QStringLiteral("githubSessionPresentation"));
		// The picker this replaced opened on its first row.
		if (!Profiles.empty()) {
			SelectedProfile = Profiles.front().Profile;
		}
		if (ConnectAction != nullptr) {
			connect(ConnectAction, &QAction::triggered, this, [this] {
				if (const auto profile = selected_profile();
					profile.has_value() && Dispatch) {
					Dispatch(application::github_intent{
						core::activate_host_profile_intent{
							.Profile = *profile,
						},
					});
				}
			});
		}
		if (RefreshAction != nullptr) {
			connect(RefreshAction, &QAction::triggered, this, [this] {
				if (Dispatch) {
					Dispatch(application::github_intent{
						core::refresh_pull_request_intent{ },
					});
				}
			});
		}
		update_connect_enabled();
	}

	void github_session_panel::add_profile(application::github_profile_option aProfile)
	{
		if (std::ranges::any_of(Profiles, [&](const auto& aExisting) { return aExisting.Profile == aProfile.Profile; })) {
			return;
		}
		if (!SelectedProfile.has_value()) {
			SelectedProfile = aProfile.Profile;
		}
		Profiles.push_back(std::move(aProfile));
		update_connect_enabled();
	}

	auto github_session_panel::apply_command(
		const core::github_command_projection& aCommand) -> bool
	{
		switch (aCommand.Id) {
		case core::github_command_id::activate_host_profile:
			ConnectCommandEnabled = aCommand.Enabled;
			ConnectDisabledReason = aCommand.DisabledReason;
			update_connect_enabled();
			break;
		case core::github_command_id::open_pull_request:
			if (OpenAction != nullptr) {
				apply_action_state(
					OpenAction,
					nullptr,
					aCommand.Enabled,
					aCommand.DisabledReason,
					QStringLiteral("Open the specified pull request"));
			}
			break;
		case core::github_command_id::refresh_pull_request:
			if (RefreshAction != nullptr) {
				apply_action_state(
					RefreshAction,
					nullptr,
					aCommand.Enabled,
					aCommand.DisabledReason,
					QStringLiteral("Refresh the current pull request"));
			}
			break;
		case core::github_command_id::show_review_threads:
			// The conversation view is the composing window's, so is its
			// command. Reported unhandled rather than ignored.
			return false;
		}
		++Statistics.CommandUpdates;
		return true;
	}

	void github_session_panel::apply_identity(
		const core::github_identity_delta& aIdentity)
	{
		ActiveProfile = aIdentity.Profile;
		// An accepted identity names the host it is on, so the host the next
		// Connect means follows it, exactly as the picker's row did.
		if (aIdentity.Profile.has_value() && profile_option(*aIdentity.Profile) != nullptr) {
			SelectedProfile = aIdentity.Profile;
		}
		clear_account(QStringLiteral("Not authenticated"));
		update_connect_enabled();
		++Statistics.IdentityUpdates;
	}

	void github_session_panel::apply_capabilities(
		const core::github_capabilities& aCapabilities)
	{
		Capabilities = aCapabilities;
		++Statistics.CapabilityUpdates;
	}

	void github_session_panel::apply_status(const core::github_status aStatus)
	{
		CurrentStatus = aStatus;
		KeptGrantReason.reset();
		const auto hasPullRequest
			= Environment.HasPullRequest && Environment.HasPullRequest();
		if (aStatus == core::github_status::loading_pull_request && hasPullRequest) {
			StatusText = QStringLiteral("Refreshing the pull request.");
		} else if (
			aStatus == core::github_status::pull_request_failed
			&& hasPullRequest) {
			StatusText = QStringLiteral(
				"Refresh failed. Showing the last accepted pull request.");
		} else if (
			aStatus == core::github_status::pull_request_cancelled
			&& hasPullRequest) {
			StatusText = QStringLiteral(
				"Refresh canceled. Showing the last accepted pull request.");
		} else {
			StatusText = github_status_text(aStatus);
		}
		switch (aStatus) {
		case core::github_status::no_profile:
			clear_account(QStringLiteral("Not authenticated"));
			[[fallthrough]];
		case core::github_status::session_ready:
		case core::github_status::loading_pull_request:
		case core::github_status::pull_request_ready:
			DiagnosticText.clear();
			LastDiagnosticReason.reset();
			LastDiagnosticHasHostDetail = false;
			break;
		case core::github_status::establishing_session:
			clear_account(QStringLiteral("Connecting…"));
			DiagnosticText.clear();
			LastDiagnosticReason.reset();
			LastDiagnosticHasHostDetail = false;
			break;
		case core::github_status::session_failed:
		case core::github_status::session_cancelled:
			clear_account(QStringLiteral("Not authenticated"));
			break;
		case core::github_status::pull_request_failed:
		case core::github_status::pull_request_cancelled:
			break;
		}
		if (Environment.PresentationChanged) {
			Environment.PresentationChanged();
		}
		++Statistics.StatusUpdates;
	}

	void github_session_panel::apply_diagnostic(
		const core::github_reason_code                                  aReason,
		const std::optional<std::uint32_t>                              aRetryAfter,
		const std::optional<application::github_rate_limit_projection>& aRateLimit,
		const bool                                                      aHostDetail,
		const application::github_bootstrap_failure_stage               aBootstrapStage,
		const application::github_capability_diagnostic                 aCapabilityDetail)
	{
		if (
			!aHostDetail && LastDiagnosticHasHostDetail
			&& LastDiagnosticReason == aReason) {
			return;
		}
		auto message = reason_text(aReason, aRetryAfter, aRateLimit);
		if (!message.isEmpty()) {
			switch (aBootstrapStage) {
			case application::github_bootstrap_failure_stage::none:
				break;
			case application::github_bootstrap_failure_stage::local_enrollment:
				message.prepend(QStringLiteral("Local enrollment: "));
				break;
			case application::github_bootstrap_failure_stage::account_identity:
				message.prepend(QStringLiteral("Account identity observation: "));
				break;
			case application::github_bootstrap_failure_stage::capability_discovery:
				message.prepend(QStringLiteral("Capability discovery: "));
				message.append(QStringLiteral(" "));
				message.append(capability_detail_text(aCapabilityDetail));
				break;
			case application::github_bootstrap_failure_stage::session_acceptance:
				message.prepend(QStringLiteral("Session acceptance: "));
				break;
			}
		}
		DiagnosticText              = message;
		LastDiagnosticReason        = message.isEmpty() ? std::nullopt : std::optional{ aReason };
		LastDiagnosticHasHostDetail = !message.isEmpty() && aHostDetail;
		if (Environment.PresentationChanged) {
			Environment.PresentationChanged();
		}
		++Statistics.DiagnosticUpdates;
	}

	void github_session_panel::apply_session(
		const application::github_session_projection& aSession)
	{
		Session = aSession;
		if (aSession.DisplayName.empty()) {
			AccountText = aSession.Login.empty()
				? QStringLiteral("Authenticated account")
				: QStringLiteral("@%1").arg(text(aSession.Login));
		} else if (aSession.Login.empty()) {
			AccountText = text(aSession.DisplayName);
		} else {
			AccountText = QStringLiteral("%1 (@%2)")
							  .arg(text(aSession.DisplayName), text(aSession.Login));
		}
		apply_capabilities(aSession.Capabilities);
		apply_rate_limits(aSession.RateLimits);
		++Statistics.SessionUpdates;
	}

	void github_session_panel::apply_rate_limits(
		const application::github_rate_limits_projection& aLimits)
	{
		RateLimits = aLimits;
	}

	void github_session_panel::apply_public_preview(
		const application::public_preview_presentation& aPresentation,
		const std::uint64_t                             aNowUnixSeconds)
	{
		PublicPreview    = aPresentation;
		PublicPreviewNow = aNowUnixSeconds;
	}

	void github_session_panel::select_profile(const core::host_profile_id aProfile)
	{
		if (profile_option(aProfile) == nullptr) {
			return;
		}
		SelectedProfile = aProfile;
		update_connect_enabled();
	}

	void github_session_panel::update_connect_enabled()
	{
		if (ConnectAction == nullptr) {
			return;
		}
		if (!selected_profile().has_value()) {
			ConnectAction->setEnabled(false);
			ConnectAction->setProperty(
				"githubDisabledReason",
				static_cast<int>(core::github_reason_code::invalid_profile));
			const auto description = QStringLiteral("No GitHub host profiles are configured.");
			ConnectAction->setToolTip(description);
			ConnectAction->setStatusTip(description);
			return;
		}
		apply_action_state(
			ConnectAction,
			nullptr,
			ConnectCommandEnabled,
			ConnectDisabledReason,
			QStringLiteral("Connect the selected GitHub host profile"));
	}

	void github_session_panel::clear_account(const QString& aAccountText)
	{
		Session.reset();
		AccountText = aAccountText;
		RateLimits  = { };
	}

	auto github_session_panel::profiles() const noexcept
		-> const std::vector<application::github_profile_option>&
	{
		return Profiles;
	}

	auto github_session_panel::active_profile() const noexcept
		-> std::optional<core::host_profile_id>
	{
		return ActiveProfile;
	}

	auto github_session_panel::selected_profile() const
		-> std::optional<core::host_profile_id>
	{
		if (SelectedProfile.has_value() && profile_option(*SelectedProfile) != nullptr) {
			return SelectedProfile;
		}
		return std::nullopt;
	}

	auto github_session_panel::session() const noexcept
		-> const std::optional<application::github_session_projection>&
	{
		return Session;
	}

	auto github_session_panel::capabilities() const noexcept
		-> const core::github_capabilities&
	{
		return Capabilities;
	}

	auto github_session_panel::rate_limits() const noexcept
		-> const application::github_rate_limits_projection&
	{
		return RateLimits;
	}

	auto github_session_panel::connect_enabled() const noexcept -> bool
	{
		return ConnectCommandEnabled && SelectedProfile.has_value();
	}

	auto github_session_panel::connect_description() const -> QString
	{
		if (!selected_profile().has_value()) {
			return QStringLiteral("No GitHub host profiles are configured.");
		}
		return ConnectCommandEnabled
			? QStringLiteral("Connect the selected GitHub host profile")
			: reason_text(ConnectDisabledReason, std::nullopt, std::nullopt);
	}

	auto github_session_panel::public_preview_signed_out() const noexcept -> bool
	{
		return PublicPreview.Mode == application::public_preview_mode::signed_out;
	}

	auto github_session_panel::public_preview() const noexcept
		-> const application::public_preview_presentation&
	{
		return PublicPreview;
	}

	auto github_session_panel::public_preview_now() const noexcept -> std::uint64_t
	{
		return PublicPreviewNow;
	}

	auto github_session_panel::public_preview_refusal_text() const -> QString
	{
		if (!PublicPreview.Refusal.has_value()) {
			return { };
		}
		return bpr::qt::public_preview_refusal_text(
			*PublicPreview.Refusal,
			PublicPreview.NowUnixSeconds != 0U
				? PublicPreview.NowUnixSeconds
				: PublicPreviewNow);
	}

	auto github_session_panel::showing_diagnostic() const -> bool
	{
		return !DiagnosticText.isEmpty();
	}

	auto github_session_panel::summary_text() const -> QString
	{
		auto summary = StatusText;
		if (showing_diagnostic()) {
			summary.append(QStringLiteral(" %1").arg(DiagnosticText));
		}
		return summary;
	}

	auto github_session_panel::status_text() const -> std::string
	{
		return StatusText.toUtf8().toStdString();
	}

	auto github_session_panel::diagnostic_text() const -> std::string
	{
		return DiagnosticText.toUtf8().toStdString();
	}

	auto github_session_panel::active_host_text() const -> std::string
	{
		const auto  profile = ActiveProfile.has_value() ? ActiveProfile : SelectedProfile;
		const auto* option  = profile.has_value() ? profile_option(*profile) : nullptr;
		return option != nullptr ? option->Hostname : std::string{ };
	}

	auto github_session_panel::active_account_text() const -> std::string
	{
		return AccountText.toUtf8().toStdString();
	}

	auto github_session_panel::status() const noexcept -> core::github_status
	{
		return CurrentStatus;
	}

	void github_session_panel::apply_kept_grant(const core::github_reason_code aReason)
	{
		if (CurrentStatus != core::github_status::session_failed) {
			return;
		}
		KeptGrantReason = aReason;
	}

	auto github_session_panel::kept_grant_reason() const noexcept
		-> std::optional<core::github_reason_code>
	{
		return KeptGrantReason;
	}

	auto github_session_panel::statistics() const noexcept
		-> github_session_panel_statistics
	{
		return Statistics;
	}

	auto github_session_panel::profile_option(
		const core::host_profile_id aProfile) const
		-> const application::github_profile_option*
	{
		const auto found = std::ranges::find_if(
			Profiles,
			[aProfile](const application::github_profile_option& aOption) {
				return aOption.Profile == aProfile;
			});
		return found == Profiles.end() ? nullptr : &*found;
	}
} // namespace bpr::qt
