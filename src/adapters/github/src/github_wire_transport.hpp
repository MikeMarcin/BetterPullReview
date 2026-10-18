#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_TRANSPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_TRANSPORT_HPP

#include "github_capability_probe.hpp"
#include "github_insights.hpp"
#include "github_read_contract.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <stop_token>
#include <variant>

namespace bpr::adapters::github::detail {
	/// \brief The request a wire exchange belongs to.
	///
	/// \details [[BPR-0988]]. A repository context is a personal discovery
	/// search: it binds the same account authority a pull request read does and
	/// names no pull request.
	using github_wire_parent_context = std::variant<core::github_session_request_context,
													core::pull_request_request_context,
													core::repository_request_context>;

	struct github_wire_correlation
	{
		github_wire_parent_context                  Parent;
		application::github_profile_revision        ProfileRevision;
		core::git_host_id                           Host;
		read_operation                              Operation = read_operation::get_viewer_identity;
		std::uint32_t                               Page      = 1U;
		std::optional<core::application_session_id> AuthenticationSession;

		auto operator==(const github_wire_correlation&) const -> bool = default;
	};

	struct github_wire_request
	{
		github_wire_correlation Correlation;
		wire_request            Wire;

		auto operator==(const github_wire_request&) const -> bool = default;
	};

	struct github_wire_success
	{
		github_wire_correlation Correlation;
		wire_response           Response;

		auto operator==(const github_wire_success&) const -> bool = default;
	};

	struct github_wire_error
	{
		github_wire_correlation            Correlation;
		application::github_provider_error Error;

		auto operator==(const github_wire_error&) const -> bool = default;
	};

	struct github_wire_cancelled
	{
		github_wire_correlation Correlation;

		auto operator==(const github_wire_cancelled&) const -> bool = default;
	};

	using github_wire_completion = std::variant<github_wire_success, github_wire_error, github_wire_cancelled>;

	struct github_capability_probe_wire_correlation
	{
		std::uint8_t                         Part = 0U;
		core::github_session_request_context Session;
		application::github_profile_revision ProfileRevision;
		core::git_host_id                    Host;
		core::github_account_id              ObservedAccount;

		auto operator==(const github_capability_probe_wire_correlation&) const
			-> bool = default;
	};

	struct github_capability_probe_wire_request
	{
		github_capability_probe_wire_correlation Correlation;
		capability_probe_request                 Wire;

		auto operator==(const github_capability_probe_wire_request&) const
			-> bool = default;
	};

	struct github_capability_probe_wire_success
	{
		github_capability_probe_wire_correlation Correlation;
		capability_probe_response                Response;
		application::diagnostic_summary          Summary{ };

		auto operator==(const github_capability_probe_wire_success&) const
			-> bool = default;
	};

	struct github_capability_probe_wire_error
	{
		github_capability_probe_wire_correlation Correlation;
		application::github_provider_error       Error{
			.Failure           = application::github_provider_failure::unavailable,
			.RetryAfterSeconds = std::nullopt,
			.RateLimits        = { },
			.RateLimitCause    = std::nullopt,
		};

		application::github_capability_diagnostic CapabilityDetail = application::github_capability_diagnostic::unavailable_detail;

		auto operator==(const github_capability_probe_wire_error&) const
			-> bool = default;
	};

	struct github_capability_probe_wire_cancelled
	{
		github_capability_probe_wire_correlation Correlation;

		auto operator==(const github_capability_probe_wire_cancelled&) const
			-> bool = default;
	};

	using github_capability_probe_wire_completion = std::variant<github_capability_probe_wire_success,
																 github_capability_probe_wire_error,
																 github_capability_probe_wire_cancelled>;

	class github_wire_transport_port;
	using github_wire_operation = std::expected<std::shared_ptr<github_wire_transport_port>, application::github_provider_error>;

	class github_wire_transport_port
	{
	public:
		github_wire_transport_port()                                  = default;
		github_wire_transport_port(const github_wire_transport_port&) = delete;
		auto operator=(const github_wire_transport_port&)
			-> github_wire_transport_port&                       = delete;
		github_wire_transport_port(github_wire_transport_port&&) = delete;
		auto operator=(github_wire_transport_port&&)
			-> github_wire_transport_port&    = delete;
		virtual ~github_wire_transport_port() = default;
		virtual void       set_insights_channel(std::shared_ptr<github_insights_channel> aChannel) { Insights = std::move(aChannel); }
		[[nodiscard]] auto insights_channel() const -> const std::shared_ptr<github_insights_channel>& { return Insights; }

	private:
		std::shared_ptr<github_insights_channel> Insights;

	public:
		/// \brief Fresh operation authority; a null owner means this transport has no scoped credential.
		[[nodiscard]] virtual auto begin_operation(std::stop_token) -> github_wire_operation { return std::shared_ptr<github_wire_transport_port>{ }; }
		[[nodiscard]] virtual auto strict_cli() const noexcept -> bool { return false; }

		[[nodiscard]] virtual auto perform(const github_wire_request& aRequest,
										   std::stop_token            aStop)
			-> github_wire_completion = 0;

		[[nodiscard]] virtual auto perform_capability_probe(
			const github_capability_probe_wire_request& aRequest,
			std::stop_token                             aStop) -> github_capability_probe_wire_completion
		{
			if (aStop.stop_requested()) {
				return github_capability_probe_wire_cancelled{
					.Correlation = aRequest.Correlation,
				};
			}
			return github_capability_probe_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = {
					.Failure           = application::github_provider_failure::unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}
	};
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_TRANSPORT_HPP
