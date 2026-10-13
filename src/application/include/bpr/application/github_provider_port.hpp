#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROVIDER_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROVIDER_PORT_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_capability_diagnostic.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/github_read_progress.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace bpr::application {
	inline constexpr std::uint8_t maximum_github_provider_attempts = 3U;

	/// \brief Provides the github provider attempt contract.
	class github_provider_attempt
	{
	public:
		constexpr github_provider_attempt() = default;
		explicit constexpr github_provider_attempt(
			const std::uint8_t aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::uint8_t
		{
			return Value;
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Value != 0U && Value <= maximum_github_provider_attempts;
		}

		auto operator<=>(const github_provider_attempt&) const = default;

	private:
		std::uint8_t Value = 0U;
	};

	/// \brief Provides the github account transport lease contract.
	class github_account_transport_lease
	{
	public:
		virtual ~github_account_transport_lease() = default;
	};

	using github_account_transport = std::shared_ptr<github_account_transport_lease>;

	enum class github_provider_failure : std::uint8_t {
		authentication,
		permission,
		network,
		api_version,
		unsupported_capability,
		rate_limit,
		certificate,
		redirect,
		timeout,
		process_startup,
		cli_unavailable,
		vault,
		malformed_response,
		response_limit,
		unavailable,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_provider_failure aFailure) noexcept -> bool
	{
		switch (aFailure) {
		case github_provider_failure::authentication:
		case github_provider_failure::permission:
		case github_provider_failure::network:
		case github_provider_failure::api_version:
		case github_provider_failure::unsupported_capability:
		case github_provider_failure::rate_limit:
		case github_provider_failure::certificate:
		case github_provider_failure::redirect:
		case github_provider_failure::timeout:
		case github_provider_failure::process_startup:
		case github_provider_failure::cli_unavailable:
		case github_provider_failure::vault:
		case github_provider_failure::malformed_response:
		case github_provider_failure::response_limit:
		case github_provider_failure::unavailable:
			return true;
		}
		return false;
	}

	/// \brief Describes the github provider error record.
	struct github_provider_error
	{
		github_provider_failure                 Failure = github_provider_failure::network;
		std::optional<std::uint32_t>            RetryAfterSeconds;
		github_rate_limit_state                 RateLimits;
		std::optional<github_rate_limit_record> RateLimitCause;
		std::uint16_t                           HttpStatus             = 0;
		diagnostic_transport_failure_source     TransportFailureSource = diagnostic_transport_failure_source::none;
		/// [[BPR-0644]]. An authentication failure whose cause is known: the
		/// stored credential answered `/user` as an account other than the one
		/// it is bound to. Only an `authentication` failure carries it.
		bool AccountMismatch = false;
		/// The selected CLI credential could not be acquired; no downstream request was launched.
		bool CliCredentialUnavailable = false;

		auto operator<=>(const github_provider_error&) const = default;
	};

	inline constexpr std::uint32_t maximum_github_retry_after_seconds = 86'400U;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_provider_error& aError) noexcept -> bool
	{
		if (!valid(aError.Failure)) {
			return false;
		}
		if ((aError.AccountMismatch || aError.CliCredentialUnavailable) && aError.Failure != github_provider_failure::authentication) {
			return false;
		}
		if (aError.HttpStatus != 0U && (aError.HttpStatus < 100U || aError.HttpStatus > 599U)) {
			return false;
		}
		switch (aError.TransportFailureSource) {
		case diagnostic_transport_failure_source::none:
		case diagnostic_transport_failure_source::native_transport:
			if (aError.HttpStatus != 0U) {
				return false;
			}
			break;
		case diagnostic_transport_failure_source::http_response:
			if (aError.HttpStatus == 0U) {
				return false;
			}
			break;
		case diagnostic_transport_failure_source::network_reply:
			break;
		default:
			return false;
		}
		if (!valid(aError.RateLimits)) {
			return false;
		}
		if (
			(
				aError.RateLimitCause.has_value()
				&& (aError.Failure != github_provider_failure::rate_limit
					|| !valid(*aError.RateLimitCause)
					|| !is_limited(*aError.RateLimitCause)
					|| find(
						   aError.RateLimits,
						   aError.RateLimitCause->Resource)
						== nullptr
					|| *find(
						   aError.RateLimits,
						   aError.RateLimitCause->Resource)
						!= *aError.RateLimitCause))
			|| (aError.Failure == github_provider_failure::rate_limit
				&& !aError.RateLimitCause.has_value())) {
			return false;
		}
		if (
			is_secondary_limited(aError.RateLimits)
			&& (!aError.RateLimitCause.has_value()
				|| !aError.RateLimitCause->SecondaryLimited
				|| (aError.RateLimits.Core.has_value()
					&& aError.RateLimits.Core->SecondaryLimited
					&& *aError.RateLimits.Core != *aError.RateLimitCause)
				|| (aError.RateLimits.Search.has_value()
					&& aError.RateLimits.Search->SecondaryLimited
					&& *aError.RateLimits.Search != *aError.RateLimitCause)
				|| (aError.RateLimits.Graphql.has_value()
					&& aError.RateLimits.Graphql->SecondaryLimited
					&& *aError.RateLimits.Graphql != *aError.RateLimitCause))) {
			return false;
		}
		return !aError.RetryAfterSeconds.has_value()
			|| (*aError.RetryAfterSeconds
					<= maximum_github_retry_after_seconds
				&& (aError.Failure == github_provider_failure::rate_limit
					|| aError.Failure
						== github_provider_failure::unavailable));
	}

	/// \brief Describes the github profile request record.
	struct github_profile_request
	{
		core::github_session_request_context Context;
		github_provider_attempt              Attempt{ 1U };

		auto operator<=>(const github_profile_request&) const = default;
	};

	/// \brief Describes the github profile success record.
	struct github_profile_success
	{
		github_profile_request   Request;
		github_profile_authority Authority;

		auto operator<=>(const github_profile_success&) const = default;
	};

	/// \brief Describes the github profile error record.
	struct github_profile_error
	{
		github_profile_request Request;
		github_provider_error  Error;

		auto operator<=>(const github_profile_error&) const = default;
	};

	/// \brief Describes the github profile cancelled record.
	struct github_profile_cancelled
	{
		github_profile_request Request;

		auto operator<=>(const github_profile_cancelled&) const = default;
	};

	using github_profile_completion = std::variant<
		github_profile_success,
		github_profile_error,
		github_profile_cancelled>;
	using github_profile_completion_handler = std::function<void(github_profile_completion)>;

	/// \brief Describes the github viewer request record.
	struct github_viewer_request
	{
		core::github_session_request_context Context;
		github_profile_authority             Authority;
		github_provider_attempt              Attempt{ 1U };

		auto operator<=>(const github_viewer_request&) const = default;
	};

	/// \brief Describes the github viewer success record.
	struct github_viewer_success
	{
		github_viewer_request    Request;
		github_viewer_record     Viewer;
		github_rate_limit_state  RateLimits;
		github_account_transport AccountTransport;

		auto operator<=>(const github_viewer_success&) const = default;
	};

	/// \brief Describes the github viewer error record.
	struct github_viewer_error
	{
		github_viewer_request Request;
		github_provider_error Error;

		auto operator<=>(const github_viewer_error&) const = default;
	};

	/// \brief Describes the github viewer cancelled record.
	struct github_viewer_cancelled
	{
		github_viewer_request Request;

		auto operator<=>(const github_viewer_cancelled&) const = default;
	};

	using github_viewer_completion = std::variant<
		github_viewer_success,
		github_viewer_error,
		github_viewer_cancelled>;
	using github_viewer_completion_handler = std::function<void(github_viewer_completion)>;

	/// \brief Describes the github bootstrap credential record.
	struct github_bootstrap_credential
	{
		github_credential_material Credential;
	};

	using github_viewer_authentication = std::variant<
		std::monostate,
		github_bootstrap_credential,
		github_credential_retrieve_success>;

	/// \brief Describes the github viewer submission record.
	struct github_viewer_submission
	{
		github_viewer_request        Request;
		github_viewer_authentication Authentication;
	};

	/// \brief Describes the github capability request record.
	struct github_capability_request
	{
		core::github_session_request_context Context;
		github_profile_authority             ProfileAuthority;
		github_viewer_record                 Viewer;
		github_account_transport             AccountTransport;
		github_provider_attempt              Attempt{ 1U };

		auto operator<=>(const github_capability_request&) const = default;
	};

	/// \brief Describes the github capability success record.
	struct github_capability_success
	{
		github_capability_request                                        Request;
		std::array<github_capability_fact, github_capability_fact_count> Facts;
		std::array<
			github_schema_capability_fact,
			github_schema_capability_fact_count>
								SchemaFacts;
		github_rate_limit_state RateLimits;

		auto operator<=>(const github_capability_success&) const = default;
	};

	/// \brief Describes the github capability error record.
	struct github_capability_error
	{
		github_capability_request    Request;
		github_provider_error        Error;
		github_capability_diagnostic CapabilityDetail = github_capability_diagnostic::unavailable_detail;

		auto operator<=>(const github_capability_error&) const = default;
	};

	/// \brief Describes the github capability cancelled record.
	struct github_capability_cancelled
	{
		github_capability_request Request;

		auto operator<=>(const github_capability_cancelled&) const = default;
	};

	using github_capability_completion = std::variant<
		github_capability_success,
		github_capability_error,
		github_capability_cancelled>;
	using github_capability_completion_handler = std::function<void(github_capability_completion)>;

	/// \brief Describes the github pull request provider request record.
	struct github_pull_request_provider_request
	{
		core::pull_request_request_context Context;
		github_profile_authority           ProfileAuthority;
		github_viewer_record               Viewer;
		github_capability_cache_entry      CapabilityEvidence;
		github_account_transport           AccountTransport;
		github_provider_attempt            Attempt{ 1U };
		// The mode the host issued this read in. Last and defaulted so every
		// request written before the mode existed still means the
		// authenticated read it meant. A provider bound to one authority
		// refuses a request naming the other rather than answering it.
		core::github_access_mode Mode{ };

		/// Trusted accepted profile session that owns the opaque credential lease.
		/// Absence preserves the original same-session contract; Context remains window-local.
		std::optional<core::application_session_id> AuthenticationSession;

		auto operator<=>(const github_pull_request_provider_request&) const = default;
	};

	/// \brief Describes the github pull request success record.
	struct github_pull_request_success
	{
		github_pull_request_provider_request      Request;
		github_pull_request_snapshot              Snapshot;
		std::optional<diagnostic_paged_operation> LastReadOperation = std::nullopt;

		auto operator<=>(const github_pull_request_success&) const = default;
	};

	/// \brief Describes the github pull request error record.
	struct github_pull_request_error
	{
		github_pull_request_provider_request        Request;
		github_provider_error                       Error;
		std::optional<diagnostic_paged_operation>   LastReadOperation  = std::nullopt;
		std::optional<github_read_validation_error> ValidationError    = std::nullopt;
		std::optional<github_read_validation_error> RelationshipDetail = std::nullopt;

		auto operator<=>(const github_pull_request_error&) const = default;
	};

	/// \brief Describes the github pull request cancelled record.
	struct github_pull_request_cancelled
	{
		github_pull_request_provider_request      Request;
		std::optional<diagnostic_paged_operation> LastReadOperation = std::nullopt;

		auto operator<=>(const github_pull_request_cancelled&) const = default;
	};

	using github_pull_request_completion = std::variant<
		github_pull_request_success,
		github_pull_request_error,
		github_pull_request_cancelled>;
	using github_pull_request_completion_handler = std::function<void(github_pull_request_completion)>;
	using github_pull_request_progress_handler   = std::function<void(core::pull_request_request_context, github_read_progress)>;

	/// Validated metadata, not an accepted snapshot or authority to select files.
	struct github_pull_request_overview_observation
	{
		core::pull_request_request_context Context;
		github_repository_record           Repository;
		github_pull_request_record         PullRequest;
		std::size_t                        ReportedFiles = 0U;
	};
	using github_pull_request_overview_handler = std::function<void(github_pull_request_overview_observation)>;

	// Submission and cancellation return without waiting for a worker. A
	// submission that returns normally has retained the operation and invokes its
	// supplied completion exactly once; a submission that throws has retained
	// nothing and never invokes it. Cancellation is advisory and does not suppress
	// that one terminal. A completion may be invoked before return, but the
	// decision executor still enqueues its application turn; an implementation
	// must not join or otherwise wait for that turn.
	/// \brief Provides the github profile port contract.
	class github_profile_port
	{
	public:
		virtual ~github_profile_port() = default;
		virtual void resolve_profile(
			github_profile_request            aRequest,
			github_profile_completion_handler aCompletion)           = 0;
		virtual void cancel_profile(github_profile_request aRequest) = 0;
	};

	/// \brief Provides the github viewer port contract.
	class github_viewer_port
	{
	public:
		virtual ~github_viewer_port() = default;
		virtual void observe_viewer(
			github_viewer_submission         aSubmission,
			github_viewer_completion_handler aCompletion)          = 0;
		virtual void cancel_viewer(github_viewer_request aRequest) = 0;
	};

	/// \brief Provides the github capability port contract.
	class github_capability_port
	{
	public:
		virtual ~github_capability_port() = default;
		virtual void probe_capabilities(
			github_capability_request            aRequest,
			github_capability_completion_handler aCompletion)                = 0;
		virtual void cancel_capabilities(github_capability_request aRequest) = 0;
	};

	/// \brief Provides the github pull request port contract.
	class github_pull_request_port
	{
	public:
		virtual ~github_pull_request_port() = default;
		virtual void fetch_pull_request(
			github_pull_request_provider_request   aRequest,
			github_pull_request_completion_handler aCompletion) = 0;
		/// \brief Reports optional bounded progress before the single terminal.
		/// Progress grants no partial-snapshot authority. Implementations without
		/// page observations retain the ordinary asynchronous read contract.
		virtual void fetch_pull_request_with_progress(
			github_pull_request_provider_request   aRequest,
			github_pull_request_completion_handler aCompletion,
			github_pull_request_progress_handler   aProgress)
		{
			static_cast<void>(aProgress);
			fetch_pull_request(std::move(aRequest), std::move(aCompletion));
		}
		virtual void cancel_pull_request(
			github_pull_request_provider_request aRequest) = 0;
		/// One optional metadata observation before the single terminal. This is
		/// independent of snapshot acceptance; older ports may omit it entirely.
		virtual void fetch_pull_request_with_observations(
			github_pull_request_provider_request   aRequest,
			github_pull_request_completion_handler aCompletion,
			github_pull_request_progress_handler   aProgress,
			github_pull_request_overview_handler   aOverview)
		{
			static_cast<void>(aOverview);
			fetch_pull_request_with_progress(std::move(aRequest), std::move(aCompletion), std::move(aProgress));
		}
	};

	/// \brief Provides the github provider port contract.
	class github_provider_port
		: public github_profile_port,
		  public github_viewer_port,
		  public github_capability_port,
		  public github_pull_request_port
	{
	public:
		~github_provider_port() override = default;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROVIDER_PORT_HPP
