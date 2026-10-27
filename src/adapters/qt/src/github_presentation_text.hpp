#ifndef BPR_SRC_ADAPTERS_QT_SRC_GITHUB_PRESENTATION_TEXT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_GITHUB_PRESENTATION_TEXT_HPP

/// \file
/// \brief The reviewer-facing text of one GitHub session, in one place.
///
/// \details Extracted verbatim by BPR-0195 so that the session chrome and the
/// pull-request content can live in separate widgets and still say the same
/// words. Every function here was an anonymous-namespace helper of
/// `github_window.cpp`; none changed, because this slice preserves behaviour
/// and a formatter that drifted during the move would be the one defect the
/// untouched tests could not distinguish from a real one.
///
/// Internal to the Qt adapter. It is not published from `include/` because
/// nothing outside the adapter should be able to depend on how a status reads.

#include "bpr/application/github_projection.hpp"

#include <QAction>
#include <QLabel>
#include <QString>
#include <QToolButton>
#include <QVariant>

#include <cstdint>
#include <optional>
#include <string>

namespace bpr::qt::detail {
	[[nodiscard]] inline auto text(const std::string& aValue) -> QString
	{
		return QString::fromUtf8(
			aValue.data(), static_cast<qsizetype>(aValue.size()));
	}

	[[nodiscard]] inline auto valid(
		const application::github_profile_product aProduct) noexcept -> bool
	{
		switch (aProduct) {
		case application::github_profile_product::github_com:
		case application::github_profile_product::github_enterprise_server:
			return true;
		}
		return false;
	}

	[[nodiscard]] inline auto valid(
		const application::github_profile_transport aTransport) noexcept -> bool
	{
		switch (aTransport) {
		case application::github_profile_transport::github_cli:
		case application::github_profile_transport::native_https:
			return true;
		}
		return false;
	}

	[[nodiscard]] inline auto valid(
		const application::github_profile_option& aOption) noexcept -> bool
	{
		return aOption.Profile.value() != 0U && !aOption.Label.empty()
			&& !aOption.Hostname.empty() && valid(aOption.Product)
			&& valid(aOption.Transport);
	}

	[[nodiscard]] inline auto product_text(
		const application::github_profile_product aProduct) -> QString
	{
		switch (aProduct) {
		case application::github_profile_product::github_com:
			return QStringLiteral("GitHub.com");
		case application::github_profile_product::github_enterprise_server:
			return QStringLiteral("GitHub Enterprise Server");
		}
		return QStringLiteral("Unknown GitHub product");
	}

	[[nodiscard]] inline auto transport_text(
		const application::github_profile_transport aTransport) -> QString
	{
		switch (aTransport) {
		case application::github_profile_transport::github_cli:
			return QStringLiteral("GitHub CLI");
		case application::github_profile_transport::native_https:
			return QStringLiteral("Native HTTPS");
		}
		return QStringLiteral("Unknown transport");
	}

	// [[BPR-1791]]. What gh may write and whether full diffs run, which differ
	// by platform: Windows and Linux have no write denial for gh, and gh's
	// Git-object acquisition is qualified on macOS only.
	[[nodiscard]] inline auto cli_platform_confinement_text() -> QString
	{
#if defined(__APPLE__)
		return QStringLiteral(
			"On macOS, CLI file writes are denied except for an owned temporary object store. "
			"Full diffs may run Git and gh's shell credential helper with the same operation credential. "
			"They require gh 2.96 or later and Git 2.49 or later, checked locally, with a 32 MiB per-file limit and supervised temporary-store cutoff. ");
#else
		return QStringLiteral(
			"On this system CLI file writes are not denied; each call's private configuration is removed when it ends. "
			"Full diffs through GitHub CLI are not yet available on this system. GitHub CLI reads require gh 2.96 or later. ");
#endif
	}

	// [[BPR-0772]]. The card's header names the host, so the warning says
	// "this server" and the host appears once on the card (RFC-BPR-0032 A4).
	[[nodiscard]] inline auto cli_trust_warning_text() -> QString
	{
		return QStringLiteral(
				   "Connect uses the chosen account's existing gh sign-in on this server without saving its token. "
				   "Each operation retrieves a transient credential and validates that exact account; changing gh's active account does not redirect it. "
				   "This connection is reused until you sign out or replace it. "
				   "You trust the configured CLI executable, CA certificates, redirects and child-process behavior. "
				   "Calls use private account configuration, excluding ambient socket routing and CLI environment overrides. ")
			+ cli_platform_confinement_text()
			+ QStringLiteral(
				   "Owned token buffers are cleared after children finish; system/child memory, swap and crash dumps remain outside exact erasure guarantees. "
				   "Read trust does not authorize publishing; ordinary review, comment and tour gestures supply exact-operation authority.");
	}

	[[nodiscard]] inline auto github_status_text(
		const core::github_status aStatus) -> QString
	{
		switch (aStatus) {
		case core::github_status::no_profile:
			return QStringLiteral("No GitHub host is connected.");
		case core::github_status::establishing_session:
			return QStringLiteral("Connecting to the selected GitHub host.");
		case core::github_status::session_ready:
			return QStringLiteral("GitHub account connected.");
		case core::github_status::session_failed:
			return QStringLiteral("The GitHub connection failed.");
		case core::github_status::session_cancelled:
			return QStringLiteral("The GitHub connection was canceled.");
		case core::github_status::loading_pull_request:
			return QStringLiteral("Loading the pull request.");
		case core::github_status::pull_request_ready:
			return QStringLiteral("Pull request ready.");
		case core::github_status::pull_request_failed:
			return QStringLiteral("The pull request could not be loaded.");
		case core::github_status::pull_request_cancelled:
			return QStringLiteral("Pull request loading was canceled.");
		}
		return QStringLiteral("GitHub status is unavailable.");
	}

	[[nodiscard]] inline auto resource_text(
		const application::github_rate_limit_resource_projection aResource)
		-> QString
	{
		switch (aResource) {
		case application::github_rate_limit_resource_projection::core:
			return QStringLiteral("GitHub API");
		case application::github_rate_limit_resource_projection::search:
			return QStringLiteral("GitHub search");
		case application::github_rate_limit_resource_projection::graphql:
			return QStringLiteral("GitHub GraphQL API");
		}
		return QStringLiteral("GitHub API");
	}

	[[nodiscard]] inline auto reason_text(
		const core::github_reason_code     aReason,
		const std::optional<std::uint32_t> aRetryAfter,
		const std::optional<application::github_rate_limit_projection>&
			aRateLimit) -> QString
	{
		QString result;
		switch (aReason) {
		case core::github_reason_code::none:
			return { };
		case core::github_reason_code::invalid_profile:
			result = QStringLiteral("The selected GitHub host profile is invalid.");
			break;
		case core::github_reason_code::session_in_progress:
			result = QStringLiteral(
				"Wait for the GitHub connection attempt to finish.");
			break;
		case core::github_reason_code::session_required:
			result = QStringLiteral(
				"Connect a GitHub host profile before opening a pull request.");
			break;
		case core::github_reason_code::invalid_repository:
			result = QStringLiteral(
				"Enter a valid repository owner and repository name.");
			break;
		case core::github_reason_code::invalid_pull_request_number:
			result = QStringLiteral("Enter a valid pull request number.");
			break;
		case core::github_reason_code::pull_request_load_in_progress:
			result = QStringLiteral(
				"Wait for the current pull request load to finish.");
			break;
		case core::github_reason_code::no_active_pull_request:
			result = QStringLiteral("Open a pull request before refreshing it.");
			break;
		case core::github_reason_code::capability_unknown:
			result = QStringLiteral(
				"GitHub review-thread support has not been confirmed.");
			break;
		case core::github_reason_code::capability_unsupported:
			result = QStringLiteral(
				"The selected GitHub host does not support review threads.");
			break;
		case core::github_reason_code::stale_completion:
			result = QStringLiteral("An outdated GitHub result was safely ignored.");
			break;
		case core::github_reason_code::completion_identity_mismatch:
			result = QStringLiteral(
				"GitHub returned a result for a different identity.");
			break;
		case core::github_reason_code::authentication_failed:
			result = QStringLiteral(
				"Authentication failed for the selected GitHub host profile.");
			break;
		case core::github_reason_code::permission_denied:
			result = QStringLiteral(
				"This app's GitHub connection cannot read this pull request. "
				"Your browser sign-in may have different access.");
			break;
		case core::github_reason_code::network_unavailable:
			result = QStringLiteral(
				"The GitHub host could not be reached. Check the network "
				"connection and try again.");
			break;
		case core::github_reason_code::api_version_unsupported:
			result = QStringLiteral(
				"The selected host does not support the required GitHub API "
				"version.");
			break;
		case core::github_reason_code::capability_unavailable:
			result = QStringLiteral(
				"Required GitHub capability information is unavailable.");
			break;
		case core::github_reason_code::rate_limited:
			result = aRateLimit.has_value()
				? QStringLiteral("%1 rate limit reached.")
					  .arg(resource_text(aRateLimit->Resource))
				: QStringLiteral("GitHub rate limit reached.");
			if (aRateLimit.has_value() && aRateLimit->SecondaryLimited) {
				result = QStringLiteral(
							 "%1 temporarily limited this request.")
							 .arg(resource_text(aRateLimit->Resource));
			}
			break;
		case core::github_reason_code::malformed_response:
			result = QStringLiteral(
				"GitHub returned data that BetterPullReview could not safely use.");
			break;
		case core::github_reason_code::response_too_large:
			result = QStringLiteral(
				"The GitHub response exceeded BetterPullReview's safe limits.");
			break;
		case core::github_reason_code::vault_unavailable:
			result = QStringLiteral(
				"The operating system credential store is unavailable.");
			break;
		case core::github_reason_code::certificate_rejected:
			result = QStringLiteral(
				"The GitHub host certificate was rejected.");
			break;
		case core::github_reason_code::redirect_rejected:
			result = QStringLiteral(
				"A redirect to an untrusted destination was blocked.");
			break;
		case core::github_reason_code::timed_out:
			result = QStringLiteral("The GitHub request timed out.");
			break;
		case core::github_reason_code::cli_unavailable:
			result = QStringLiteral("The selected GitHub CLI connection is unavailable.");
			break;
		case core::github_reason_code::cancelled:
			result = QStringLiteral("The GitHub request was canceled.");
			break;
		}

		if (aRetryAfter.has_value()) {
			result.append(
				QStringLiteral(" Try again in %1 seconds.")
					.arg(QString::number(*aRetryAfter)));
		}
		return result;
	}

	[[nodiscard]] inline auto capability_text(
		const core::github_capability_state aState) -> QString
	{
		switch (aState) {
		case core::github_capability_state::supported:
			return QStringLiteral("Supported");
		case core::github_capability_state::unsupported:
			return QStringLiteral("Unsupported");
		case core::github_capability_state::unknown:
			return QStringLiteral("Not yet known");
		}
		return QStringLiteral("Not yet known");
	}

	[[nodiscard]] inline auto capabilities_text(
		const core::github_capabilities& aCapabilities) -> QString
	{
		return QStringLiteral(
				   "Review threads: %1\n"
				   "Viewed-file changes: %2\n"
				   "Resolve review threads: %3\n"
				   "Reopen review threads: %4")
			.arg(
				capability_text(aCapabilities.ReviewThreads),
				capability_text(aCapabilities.ViewedFileMutations),
				capability_text(aCapabilities.ResolveThread),
				capability_text(aCapabilities.UnresolveThread));
	}

	[[nodiscard]] inline auto rate_limit_line(
		const QString&                                   aLabel,
		const application::github_rate_limit_projection& aLimit) -> QString
	{
		if (aLimit.SecondaryLimited) {
			return QStringLiteral("%1: Temporarily limited").arg(aLabel);
		}
		return QStringLiteral("%1: %2 of %3 requests remaining")
			.arg(
				aLabel,
				QString::number(aLimit.Remaining),
				QString::number(aLimit.Limit));
	}

	[[nodiscard]] inline auto rate_limits_text(
		const application::github_rate_limits_projection& aLimits) -> QString
	{
		QStringList lines;
		if (aLimits.Core.has_value()) {
			lines.push_back(rate_limit_line(QStringLiteral("API"), *aLimits.Core));
		}
		if (aLimits.Search.has_value()) {
			lines.push_back(
				rate_limit_line(QStringLiteral("Search"), *aLimits.Search));
		}
		if (aLimits.Graphql.has_value()) {
			lines.push_back(
				rate_limit_line(QStringLiteral("GraphQL"), *aLimits.Graphql));
		}
		return lines.isEmpty()
			? QStringLiteral("Rate limits: Not reported")
			: lines.join(QLatin1Char('\n'));
	}

	[[nodiscard]] inline auto pull_request_state_text(
		const application::github_pull_request_state_projection aState,
		const bool                                              aDraft) -> QString
	{
		QString result;
		switch (aState) {
		case application::github_pull_request_state_projection::open:
			result = QStringLiteral("Open");
			break;
		case application::github_pull_request_state_projection::closed:
			result = QStringLiteral("Closed");
			break;
		case application::github_pull_request_state_projection::merged:
			result = QStringLiteral("Merged");
			break;
		}
		if (aDraft) {
			result.append(QStringLiteral(" draft"));
		}
		return result;
	}

	[[nodiscard]] inline auto review_thread_availability_text(
		const bool                          aAvailable,
		const std::size_t                   aCount,
		const core::github_capability_state aCapability) -> QString
	{
		if (aAvailable) {
			if (aCount == 0U) {
				return QStringLiteral(
					"Review threads are available; this pull request has none.");
			}
			if (aCount == 1U) {
				return QStringLiteral("1 review thread available.");
			}
			return QStringLiteral("%1 review threads available.")
				.arg(QString::number(aCount));
		}

		switch (aCapability) {
		case core::github_capability_state::unsupported:
			return QStringLiteral(
				"Review threads are unavailable because this host does not "
				"support them.");
		case core::github_capability_state::unknown:
			return QStringLiteral(
				"Review threads are unavailable because support has not been "
				"confirmed.");
		case core::github_capability_state::supported:
			return QStringLiteral(
				"Review threads are unavailable for this pull request.");
		}
		return QStringLiteral(
			"Review threads are unavailable for this pull request.");
	}

	inline void set_plain_text_label(QLabel* const aLabel)
	{
		aLabel->setTextFormat(Qt::PlainText);
		aLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	}

	inline void apply_action_state(
		QAction* const                 aAction,
		QToolButton* const             aButton,
		const bool                     aEnabled,
		const core::github_reason_code aReason,
		const QString&                 aEnabledDescription)
	{
		aAction->setEnabled(aEnabled);
		const auto description = aEnabled
			? aEnabledDescription
			: reason_text(aReason, std::nullopt, std::nullopt);
		aAction->setToolTip(description);
		aAction->setStatusTip(description);
		aAction->setProperty(
			"githubDisabledReason", static_cast<int>(aEnabled ? core::github_reason_code::none : aReason));
		if (aButton != nullptr) {
			aButton->setAccessibleDescription(description);
		}
	}
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_GITHUB_PRESENTATION_TEXT_HPP
