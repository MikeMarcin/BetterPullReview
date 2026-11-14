#include "bpr/application/agent_queue_host.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace bpr::application {
	agent_queue_host::agent_queue_host(agent_queue_dependencies aDependencies)
		: Dependencies{ std::move(aDependencies) }
	{ }

	void agent_queue_host::load()
	{
		if (Dependencies.Store == nullptr) {
			return;
		}
		auto stored = Dependencies.Store->load();
		if (!stored) {
			return;
		}
		for (auto& [session, queue] : *stored) {
			// Q10 and AC04: a restored queue opens paused; nothing is sent
			// without the reviewer resuming it.
			core::restore(queue);
			Queues[session] = std::move(queue);
			if (Dependencies.Changed) {
				Dependencies.Changed(session, Queues[session]);
			}
		}
	}

	void agent_queue_host::commit(const std::string& aSession)
	{
		const auto& queue = Queues[aSession];
		if (Dependencies.Store != nullptr) {
			static_cast<void>(Dependencies.Store->store(aSession, queue));
		}
		if (Dependencies.Changed) {
			Dependencies.Changed(aSession, queue);
		}
	}

	auto agent_queue_host::enqueue(const std::string& aSession, core::queued_input aInput) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::enqueue(queue, std::move(aInput), queue.Revision);
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::withdraw(const std::string& aSession, const std::uint64_t aId) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::withdraw(queue, aId, queue.Revision);
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::resume(const std::string& aSession) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::resume(queue, queue.Revision);
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	void agent_queue_host::pause(const std::string& aSession, const core::queue_pause aWhy)
	{
		core::pause(Queues[aSession], aWhy);
		commit(aSession);
	}

	void agent_queue_host::pause_all(const core::queue_pause aWhy)
	{
		auto sessions = std::vector<std::string>{ };
		for (const auto& [session, queue] : Queues) {
			if (!queue.Entries.empty()) {
				sessions.push_back(session);
			}
		}
		for (const auto& session : sessions) {
			pause(session, aWhy);
		}
	}

	auto agent_queue_host::queued_count() const -> std::size_t
	{
		auto count = std::size_t{ 0 };
		for (const auto& [session, queue] : Queues) {
			count += static_cast<std::size_t>(std::ranges::count(queue.Entries, core::queued_state::queued, &core::queued_entry::State));
		}
		return count;
	}

	void agent_queue_host::observe(const std::string& aSession, const core::queue_agent aAgent)
	{
		auto&      queue = Queues[aSession];
		const auto next  = core::next_release(queue, aAgent);
		if (!next || core::mark_sending(queue, *next) != core::queue_refusal::none) {
			return;
		}
		Sending[aSession] = *next;
		commit(aSession);
		const auto entry   = std::ranges::find(queue.Entries, *next, &core::queued_entry::Id);
		const auto started = entry != queue.Entries.end() && Dependencies.Release && Dependencies.Release(aSession, *entry);
		if (!started) {
			// Proven before any write: it stays unsent, and the queue pauses so
			// the reviewer sees why.
			Sending.erase(aSession);
			const auto again = std::ranges::find(Queues[aSession].Entries, *next, &core::queued_entry::Id);
			if (again != Queues[aSession].Entries.end()) {
				again->State = core::queued_state::queued;
			}
			core::pause(Queues[aSession], core::queue_pause::failed);
			commit(aSession);
		}
	}

	void agent_queue_host::settle(const std::string& aSession, const core::queue_pause aOutcome)
	{
		const auto found = Sending.find(aSession);
		if (found == Sending.end()) {
			return;
		}
		core::settle(Queues[aSession], found->second, aOutcome);
		Sending.erase(found);
		commit(aSession);
	}

	auto agent_queue_host::queue(const std::string& aSession) const -> const core::agent_queue*
	{
		const auto found = Queues.find(aSession);
		return found == Queues.end() ? nullptr : &found->second;
	}

	auto agent_queue_host::sending(const std::string& aSession) const -> std::optional<std::uint64_t>
	{
		const auto found = Sending.find(aSession);
		return found == Sending.end() ? std::nullopt : std::optional{ found->second };
	}
} // namespace bpr::application
