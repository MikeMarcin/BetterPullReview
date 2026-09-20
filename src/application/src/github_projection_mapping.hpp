#ifndef BPR_SRC_APPLICATION_SRC_GITHUB_PROJECTION_MAPPING_HPP
#define BPR_SRC_APPLICATION_SRC_GITHUB_PROJECTION_MAPPING_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_read_model.hpp"

namespace bpr::application {
	struct github_pull_request_overview_observation;
	[[nodiscard]] auto project_github_loading_overview(const github_pull_request_overview_observation& aObservation)
		-> std::optional<github_loading_overview_projection>;
	struct github_host_diagnostic
	{
		core::github_reason_code                Reason = core::github_reason_code::none;
		std::optional<std::uint32_t>            RetryAfterSeconds;
		github_rate_limit_state                 RateLimits;
		std::optional<github_rate_limit_record> RateLimitCause;
		github_capability_diagnostic            CapabilityDetail = github_capability_diagnostic::none;

		auto operator<=>(const github_host_diagnostic&) const = default;
	};

	[[nodiscard]] auto project_github_transition(
		const core::github_transition_output& aOutput)
		-> github_transition_projection;
	[[nodiscard]] auto project_github_session(
		const github_session_read_model& aSession) -> github_session_projection;
	/// \brief [[BPR-0660]]. The session with the grant facts the host decided.
	[[nodiscard]] auto project_github_session(
		const github_session_read_model&       aSession,
		std::optional<github_grant_projection> aGrant) -> github_session_projection;
	[[nodiscard]] auto project_github_pull_request(
		const github_pull_request_snapshot& aSnapshot)
		-> github_pull_request_projection;
	[[nodiscard]] auto project_github_host_diagnostic(
		const github_host_diagnostic& aDiagnostic)
		-> github_host_diagnostic_projection;
}

#endif // BPR_SRC_APPLICATION_SRC_GITHUB_PROJECTION_MAPPING_HPP
