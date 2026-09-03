#include "bpr/application/mcp_context_helper_supervisor.hpp"

#include <mutex>
#include <utility>

namespace bpr::application {
	namespace {
		/// The two names `adapters::mcp::mcp_environment_allowlist` declares.
		/// Written here rather than included because the application layer
		/// links no adapter; the composition contract compares the two lists
		/// element for element so a rename fails rather than diverges.
		inline constexpr std::string_view snapshot_root_name
			= "BPR_MCP_SNAPSHOT_ROOT";
		inline constexpr std::string_view pinned_binding_name
			= "BPR_MCP_PINNED_BINDING";
	} // namespace

	struct mcp_context_helper_supervisor::implementation
	{
		implementation(
			std::filesystem::path aHelper,
			agent_process_port&   aProcesses,
			effect_executor&      aEffects,
			decision_executor&    aDecisionLane)
			: Helper{ std::move(aHelper) }
			, Processes{ aProcesses }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
		{
		}

		std::filesystem::path Helper;
		agent_process_port&   Processes;
		effect_executor&      Effects;
		decision_executor&    DecisionLane;

		mutable std::mutex               Mutex;
		std::optional<mcp_helper_launch> Launch;
		std::string                      Answer;
		bool                             Running = false;
		bool                             Retired = false;
		mcp_helper_supervisor_statistics Statistics;
	};

	auto name(const mcp_helper_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case mcp_helper_refusal::none:
			return "none";
		case mcp_helper_refusal::no_helper:
			return "no_helper";
		case mcp_helper_refusal::not_pinned:
			return "not_pinned";
		case mcp_helper_refusal::network_location:
			return "network_location";
		case mcp_helper_refusal::already_running:
			return "already_running";
		}
		return { };
	}

	mcp_context_helper_supervisor::mcp_context_helper_supervisor(
		std::filesystem::path aHelperExecutable,
		agent_process_port&   aProcesses,
		effect_executor&      aEffects,
		decision_executor&    aDecisionLane)
		: Implementation{ std::make_shared<implementation>(
			  std::move(aHelperExecutable),
			  aProcesses,
			  aEffects,
			  aDecisionLane) }
	{
	}

	mcp_context_helper_supervisor::~mcp_context_helper_supervisor()
	{
		{
			const auto lock         = std::scoped_lock{ Implementation->Mutex };
			Implementation->Retired = true;
		}
		stop();
	}

	auto mcp_context_helper_supervisor::environment_names() noexcept
		-> std::array<std::string_view, 2U>
	{
		return { snapshot_root_name, pinned_binding_name };
	}

	auto mcp_context_helper_supervisor::names_network_location(
		const std::string_view aPath) noexcept -> bool
	{
		// The same two shapes `adapters::mcp::names_uri_scheme` and
		// `adapters::mcp::names_network_authority` refuse, and the contract
		// sweeps both implementations over one list of inputs.
		return aPath.find("://") != std::string_view::npos
			|| aPath.starts_with("//") || aPath.starts_with("\\\\");
	}

	void mcp_context_helper_supervisor::pin(
		std::filesystem::path aSnapshotRoot, std::string aPinnedBinding)
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		++Implementation->Statistics.Pins;
		Implementation->Launch = mcp_helper_launch{
			.Executable       = Implementation->Helper.string(),
			.WorkingDirectory = aSnapshotRoot.string(),
			.Environment      = {
				std::pair{
					std::string{ snapshot_root_name },
					aSnapshotRoot.string() },
				std::pair{
					std::string{ pinned_binding_name },
					std::move(aPinnedBinding) },
			},
		};
	}

	void mcp_context_helper_supervisor::unpin()
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		Implementation->Launch.reset();
	}

	auto mcp_context_helper_supervisor::describe() const
		-> std::optional<mcp_helper_launch>
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		return Implementation->Launch;
	}

	auto mcp_context_helper_supervisor::start(std::string aRequest)
		-> mcp_helper_refusal
	{
		auto launch = mcp_helper_launch{ };
		{
			const auto lock    = std::scoped_lock{ Implementation->Mutex };
			const auto refusal = Implementation->Helper.empty()
				? mcp_helper_refusal::no_helper
				: !Implementation->Launch.has_value()
				? mcp_helper_refusal::not_pinned
				: names_network_location(Implementation->Launch->Executable)
					|| names_network_location(
						Implementation->Launch->WorkingDirectory)
				? mcp_helper_refusal::network_location
				: Implementation->Running ? mcp_helper_refusal::already_running
										  : mcp_helper_refusal::none;
			if (refusal != mcp_helper_refusal::none) {
				++Implementation->Statistics.Refusals;
				return refusal;
			}
			Implementation->Running = true;
			Implementation->Answer.clear();
			++Implementation->Statistics.Starts;
			launch = *Implementation->Launch;
		}
		auto request = agent_launch_request{
			.Protocol         = core::agent_protocol::external_terminal,
			.Session          = core::agent_session_id{ },
			.Executable       = launch.Executable,
			.Arguments        = { },
			.WorkingDirectory = launch.WorkingDirectory,
			.Environment      = launch.Environment,
			.Input            = std::move(aRequest),
		};
		Implementation->Effects.post(
			[implementation = Implementation, launchRequest = std::move(request)]() {
				auto result = implementation->Processes.run(launchRequest, 15000);
				implementation->DecisionLane.post(
					[implementation, runResult = std::move(result)]() {
						const auto lock
							= std::scoped_lock{ implementation->Mutex };
						implementation->Running = false;
						++implementation->Statistics.Completions;
						for (const auto& event : runResult.Events) {
							implementation->Answer += event.Text;
							implementation->Answer += '\n';
						}
					});
			});
		return mcp_helper_refusal::none;
	}

	void mcp_context_helper_supervisor::stop() noexcept
	{
		{
			const auto lock = std::scoped_lock{ Implementation->Mutex };
			if (!Implementation->Running) {
				return;
			}
			++Implementation->Statistics.Stops;
		}
		Implementation->Processes.cancel();
	}

	auto mcp_context_helper_supervisor::running() const noexcept -> bool
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		return Implementation->Running;
	}

	auto mcp_context_helper_supervisor::answer() const -> std::string
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		return Implementation->Answer;
	}

	auto mcp_context_helper_supervisor::statistics() const noexcept
		-> mcp_helper_supervisor_statistics
	{
		const auto lock = std::scoped_lock{ Implementation->Mutex };
		return Implementation->Statistics;
	}
} // namespace bpr::application
