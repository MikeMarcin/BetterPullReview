#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::application {
	enum class github_profile_product : std::uint8_t {
		github_com,
		github_enterprise_server,
	};

	enum class github_profile_transport : std::uint8_t {
		github_cli,
		native_https,
	};

	/// \brief Describes the github profile option record.
	struct github_profile_option
	{
		core::host_profile_id    Profile{ };
		std::string              Label{ };
		std::string              Hostname{ };
		github_profile_product   Product   = github_profile_product::github_com;
		github_profile_transport Transport = github_profile_transport::native_https;

		auto operator<=>(const github_profile_option&) const = default;
	};

	using github_intent = std::variant<
		core::deactivate_github_identity_intent,
		core::activate_host_profile_intent,
		core::open_pull_request_intent,
		core::refresh_pull_request_intent>;
	using github_intent_dispatch = std::function<void(github_intent)>;

	enum class github_rate_limit_resource_projection : std::uint8_t {
		core,
		search,
		graphql,
	};

	/// \brief Describes the github rate limit projection record.
	struct github_rate_limit_projection
	{
		github_rate_limit_resource_projection Resource         = github_rate_limit_resource_projection::core;
		std::uint64_t                         Limit            = 0;
		std::uint64_t                         Remaining        = 0;
		std::uint64_t                         ResetUnixSeconds = 0;
		bool                                  SecondaryLimited = false;

		auto operator<=>(const github_rate_limit_projection&) const = default;
	};

	/// \brief Describes the github rate limits projection record.
	struct github_rate_limits_projection
	{
		std::optional<github_rate_limit_projection> Core    = std::nullopt;
		std::optional<github_rate_limit_projection> Search  = std::nullopt;
		std::optional<github_rate_limit_projection> Graphql = std::nullopt;

		auto operator<=>(const github_rate_limits_projection&) const = default;
	};

	/// \brief Describes the github transition projection record.
	struct github_transition_projection
	{
		core::github_changed_domain                     Changed = core::github_changed_domain::none;
		std::vector<core::github_command_projection>    Commands{ };
		std::optional<core::github_status_delta>        Status       = std::nullopt;
		std::optional<core::github_identity_delta>      Identity     = std::nullopt;
		std::optional<core::github_capability_delta>    Capabilities = std::nullopt;
		std::optional<core::pull_request_summary_delta> PullRequest  = std::nullopt;
		std::vector<core::github_diagnostic>            Diagnostics{ };

		auto operator<=>(const github_transition_projection&) const = default;
	};

	/// \brief Describes the github session projection record.
	struct github_session_projection
	{
		core::host_profile_id         Profile{ };
		core::git_host_id             Host{ };
		core::github_account_id       Account{ };
		std::string                   Login{ };
		std::string                   DisplayName{ };
		core::github_capabilities     Capabilities{ };
		core::capability_revision     CapabilityRevision{ };
		github_rate_limits_projection RateLimits{ };

		auto operator<=>(const github_session_projection&) const = default;
	};

	enum class github_pull_request_state_projection : std::uint8_t {
		open,
		closed,
		merged,
	};

	enum class github_review_state_projection : std::uint8_t {
		none,
		pending,
		commented,
		approved,
		changes_requested,
		dismissed,
	};

	enum class github_conversation_kind : std::uint8_t {
		review,
		review_thread,
		review_comment,
		issue_comment,
	};

	enum class github_conversation_diff_side : std::uint8_t {
		left,
		right,
	};

	enum class github_conversation_anchor_availability : std::uint8_t {
		current,
		selected_comparison_mismatch,
		selected_line_out_of_bounds,
		outdated_revision,
		file_level,
		missing_side,
		missing_current_line,
	};

	/// \brief Describes the github conversation projection record.
	struct github_conversation_projection
	{
		std::string                                  Id{ };
		std::optional<std::string>                   ParentId = std::nullopt;
		github_conversation_kind                     Kind     = github_conversation_kind::issue_comment;
		std::string                                  AuthorLogin{ };
		std::string                                  Body{ };
		std::string                                  Path{ };
		std::optional<std::uint32_t>                 Line         = std::nullopt;
		std::optional<std::uint32_t>                 OriginalLine = std::nullopt;
		std::optional<github_conversation_diff_side> DiffSide     = std::nullopt;
		github_conversation_anchor_availability      AnchorAvailability
			= github_conversation_anchor_availability::missing_current_line;
		github_review_state_projection ReviewState = github_review_state_projection::none;
		bool                           Resolved    = false;
		bool                           Outdated    = false;
		bool                           Deleted     = false;

		auto operator<=>(const github_conversation_projection&) const = default;
	};

	/// \brief Describes the github pull request projection record.
	struct github_pull_request_projection
	{
		core::host_profile_id                       Profile{ };
		core::git_host_id                           Host{ };
		core::github_account_id                     Account{ };
		core::provider_snapshot_id                  Snapshot{ };
		core::repository_id                         Repository{ };
		core::pull_request_id                       PullRequest{ };
		std::string                                 Owner{ };
		std::string                                 RepositoryName{ };
		core::pull_request_number                   Number{ };
		std::string                                 Title{ };
		std::string                                 Body{ };
		github_pull_request_state_projection        State = github_pull_request_state_projection::open;
		bool                                        Draft = false;
		std::string                                 AuthorLogin{ };
		std::string                                 BaseRef{ };
		std::string                                 HeadRef{ };
		std::string                                 BaseOid{ };
		std::string                                 HeadOid{ };
		std::size_t                                 FileCount              = 0;
		std::size_t                                 CommitCount            = 0;
		std::size_t                                 ReviewCount            = 0;
		std::size_t                                 ReviewThreadCount      = 0;
		std::size_t                                 IssueCommentCount      = 0;
		bool                                        ReviewThreadsAvailable = false;
		core::github_capabilities                   Capabilities{ };
		core::capability_revision                   CapabilityRevision{ };
		github_rate_limits_projection               RateLimits{ };
		std::vector<github_conversation_projection> Conversations{ };

		auto operator<=>(const github_pull_request_projection&) const = default;
	};

	/// \brief Describes the github host diagnostic projection record.
	struct github_host_diagnostic_projection
	{
		core::github_reason_code                    Reason            = core::github_reason_code::none;
		std::optional<std::uint32_t>                RetryAfterSeconds = std::nullopt;
		github_rate_limits_projection               RateLimits{ };
		std::optional<github_rate_limit_projection> RateLimitCause = std::nullopt;

		auto operator<=>(const github_host_diagnostic_projection&) const = default;
	};

	/// \brief Describes the github presentation update record.
	struct github_presentation_update
	{
		std::optional<github_transition_projection>    Transition  = std::nullopt;
		std::optional<github_session_projection>       Session     = std::nullopt;
		std::optional<github_pull_request_projection>  PullRequest = std::nullopt;
		std::vector<github_host_diagnostic_projection> HostDiagnostics{ };

		auto operator<=>(const github_presentation_update&) const = default;
	};

	/// \brief Provides the github projection port contract.
	class github_projection_port
	{
	public:
		virtual ~github_projection_port() = default;

		// Delivery is synchronous but observational: exceptions are contained by
		// the host and cannot veto accepted state or cleanup. These records contain
		// no provider requests, credentials, transports, or provider read models.
		virtual void publish(const github_presentation_update& aUpdate) = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP
