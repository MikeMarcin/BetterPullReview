#ifndef BPR_GH_CLI_GIT_SMART_TRANSPORT_HPP
#define BPR_GH_CLI_GIT_SMART_TRANSPORT_HPP

#include "gh_cli_transport.hpp"

#include <functional>

namespace bpr::adapters::github::detail {
	/// \brief Acquires exact commit graphs through gh and serves local pack bytes.
	/// \details Ref replies represent the accepted API snapshot, not an additional
	/// remote-ref freshness observation. The account is re-observed before/after
	/// acquisition. ConfigurationCurrent retains the original CLI lease baseline.
	[[nodiscard]] auto make_gh_cli_git_smart_transport(
		const validated_github_host_profile&          aProfile,
		const validated_gh_cli_configuration&         aConfiguration,
		const std::filesystem::path&                  aGit,
		core::github_session_request_context          aSession,
		const application::git_smart_request_context& aContext,
		std::function<bool()>                         aConfigurationCurrent,
		std::shared_ptr<github_insights_channel>      aInsights = { })
		-> application::git_smart_transport_result<application::git_smart_transport>;
}

#endif
