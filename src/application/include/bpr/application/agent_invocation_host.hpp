#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/agent_profile.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::application {
	struct agent_invocation_request
	{
		core::application_session_id        ApplicationSession;
		core::pull_request_authority        Authority;
		core::agent_profile_id              Profile;
		agent_context_observation           Context;
		bool                                ExistingCheckoutChosen = false;
		std::optional<checkout_observation> ExistingCheckout;

		auto operator<=>(const agent_invocation_request&) const = default;
	};

	/// \brief Exact identity admitted for one invocation's UI projections.
	struct agent_invocation_projection_identity
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		core::agent_session          Session;
		std::uint64_t                InvocationGeneration = 0U;

		auto operator<=>(const agent_invocation_projection_identity&) const
			= default;
	};

	class agent_invocation_projection_port
	{
	public:
		virtual ~agent_invocation_projection_port() = default;

		virtual void offer(std::span<const core::agent_profile> aProfiles) = 0;
		virtual void refuse(std::string_view aReason)                      = 0;
		virtual void admit(
			const agent_invocation_projection_identity& aIdentity) = 0;
		virtual void start(
			const agent_invocation_projection_identity& aIdentity,
			const prepared_workspace&                   aWorkspace) = 0;
		virtual void stream(
			const agent_invocation_projection_identity& aIdentity,
			const agent_stream_event&                   aEvent) = 0;
		virtual void finish(
			const agent_invocation_projection_identity& aIdentity,
			const agent_run_result&                     aResult) = 0;
	};

	struct agent_invocation_host_statistics
	{
		std::uint64_t CoreDecisions            = 0U;
		std::uint64_t WorkspaceStarts          = 0U;
		std::uint64_t ProcessStarts            = 0U;
		std::uint64_t WorkspaceCleanups        = 0U;
		std::uint64_t WorkspaceCleanupFailures = 0U;

		auto operator<=>(const agent_invocation_host_statistics&) const = default;
	};

	/// \brief Composes core legality, exact context, workspace and child effects.
	class agent_invocation_host final
	{
	public:
		agent_invocation_host(
			std::vector<core::agent_profile>     aProfiles,
			std::vector<core::agent_host_policy> aPolicies,
			std::filesystem::path                aWorkspaceParent,
			agent_workspace_port&                aWorkspaces,
			agent_process_port&                  aProcesses,
			environment_reader                   aEnvironment,
			effect_executor&                     aEffects,
			decision_executor&                   aDecisionLane,
			agent_invocation_projection_port&    aProjection);
		~agent_invocation_host();

		agent_invocation_host(const agent_invocation_host&) = delete;
		auto operator=(const agent_invocation_host&)
			-> agent_invocation_host& = delete;

		void observe(
			core::application_session_id aApplicationSession,
			core::pull_request_authority aAuthority,
			core::agent_revision_binding aBinding);
		void clear();
		void dispatch(agent_invocation_request aRequest);
		void cancel();
		void shutdown();
		void retry_cleanup();

		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto statistics() const noexcept
			-> agent_invocation_host_statistics;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP
