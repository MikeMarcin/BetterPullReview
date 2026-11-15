#include "bpr/application/agent_session_runner.hpp"

#include <utility>

namespace bpr::application {
	void agent_session_runner::bind(agent_queue_host& aHost) { Host = &aHost; }

	auto agent_session_runner::offer(agent_session_runtime aRuntime) -> std::uint64_t
	{
		const auto token = ++LastRuntime;
		Runtimes.emplace(token, std::move(aRuntime));
		return token;
	}

	void agent_session_runner::withdraw(const std::uint64_t aRuntime) { Runtimes.erase(aRuntime); }

	void agent_session_runner::retired(const std::uint64_t aRuntime, const std::string& aSession, const core::queue_pause aOutcome)
	{
		const auto found = Running.find(aSession);
		if (found == Running.end() || found->second != aRuntime) {
			return;
		}
		Running.erase(found);
		if (Host != nullptr) {
			Host->retired(aSession, aOutcome);
		}
	}

	void agent_session_runner::returned(const std::uint64_t aRuntime, const std::string& aSession)
	{
		const auto found = Running.find(aSession);
		if (found == Running.end() || found->second != aRuntime) {
			return;
		}
		Running.erase(found);
		if (Host != nullptr) {
			Host->returned(aSession);
		}
	}

	auto agent_session_runner::running(const std::string& aSession) const -> std::optional<std::uint64_t>
	{
		const auto found = Running.find(aSession);
		return found == Running.end() ? std::nullopt : std::optional{ found->second };
	}

	auto agent_session_runner::running_on(const std::uint64_t aRuntime) const -> std::vector<std::string>
	{
		auto sessions = std::vector<std::string>{ };
		for (const auto& [session, runtime] : Running) {
			if (runtime == aRuntime) {
				sessions.push_back(session);
			}
		}
		return sessions;
	}

	auto agent_session_runner::state(const std::string& aSession) -> core::queue_agent
	{
		if (Running.contains(aSession)) {
			return core::queue_agent::busy;
		}
		auto any = core::queue_agent::not_set_up;
		for (const auto& [token, runtime] : Runtimes) {
			if (!runtime.Runs || !runtime.Runs(aSession) || !runtime.State) {
				continue;
			}
			const auto agent = runtime.State(aSession);
			if (agent == core::queue_agent::busy) {
				// One conversation runs one turn at a time.
				return core::queue_agent::busy;
			}
			if (agent == core::queue_agent::idle) {
				any = core::queue_agent::idle;
			}
		}
		return any;
	}

	auto agent_session_runner::start(const std::string& aSession, const core::queued_entry& aEntry) -> bool
	{
		for (const auto& [token, runtime] : Runtimes) {
			if (runtime.Runs && runtime.Runs(aSession) && runtime.State && runtime.State(aSession) == core::queue_agent::idle && runtime.Start) {
				Running[aSession] = token;
				if (runtime.Start(aSession, aEntry)) {
					return true;
				}
				Running.erase(aSession);
				return false;
			}
		}
		return false;
	}

	auto agent_session_runner::steer(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string>
	{
		// The runtime running the session's turn, or one whose agent is busy
		// with a turn typed directly.
		for (const auto& [token, runtime] : Runtimes) {
			const auto running = Running.find(aSession);
			const auto ours    = running != Running.end() ? running->second == token : runtime.Runs && runtime.Runs(aSession) && runtime.State && runtime.State(aSession) == core::queue_agent::busy;
			if (ours && runtime.Steer) {
				return runtime.Steer(aSession, aEntry);
			}
		}
		// [[BPR-2153]] fills this in: no agent steers a running turn yet.
		return std::string{ "Steer isn't available for this agent yet, so the message goes next, right after this answer." };
	}
} // namespace bpr::application
