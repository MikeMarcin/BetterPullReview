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
		show(aSession);
		const auto waiting = queued_count();
		for (const auto& [token, viewer] : Viewers) {
			if (viewer.Waiting) {
				viewer.Waiting(waiting);
			}
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

	auto agent_queue_host::withdraw(const std::string& aSession, const std::uint64_t aId, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::withdraw(queue, aId, aSeen.value_or(queue.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::resume(const std::string& aSession, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::resume(queue, aSeen.value_or(queue.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::reorder(const std::string& aSession, const std::uint64_t aId, const int aBy, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::reorder(queue, aId, aBy, aSeen.value_or(queue.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::hold(const std::string& aSession, const std::uint64_t aId, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::hold(queue, aId, aSeen.value_or(queue.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
		}
		return refused;
	}

	auto agent_queue_host::release_hold(const std::string& aSession, const std::uint64_t aId, std::optional<std::string> aText) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::release_hold(queue, aId, std::move(aText), queue.Revision);
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
		start(aSession, aAgent, [this, &aSession](const core::queued_entry& aEntry) { return Dependencies.Release && Dependencies.Release(aSession, aEntry); }, std::nullopt);
	}

	void agent_queue_host::start(const std::string& aSession, const core::queue_agent aAgent, const std::function<bool(const core::queued_entry&)>& aRelease, const std::optional<std::uint64_t> aViewer)
	{
		auto&      queue = Queues[aSession];
		const auto next  = core::next_release(queue, aAgent);
		if (!next || core::mark_sending(queue, *next) != core::queue_refusal::none) {
			return;
		}
		Sending[aSession] = *next;
		if (aViewer) {
			Dispatchers[aSession] = *aViewer;
		}
		commit(aSession);
		const auto entry = std::ranges::find(Queues[aSession].Entries, *next, &core::queued_entry::Id);
		// A copy: the release may change the queue it came from.
		const auto released = entry != Queues[aSession].Entries.end() ? std::optional{ *entry } : std::nullopt;
		const auto started  = released && aRelease(*released);
		if (!started) {
			// Proven before any write: it stays unsent, and the queue pauses so
			// the reviewer sees why.
			Sending.erase(aSession);
			Dispatchers.erase(aSession);
			const auto again = std::ranges::find(Queues[aSession].Entries, *next, &core::queued_entry::Id);
			if (again != Queues[aSession].Entries.end()) {
				again->State = core::queued_state::queued;
			}
			core::pause(Queues[aSession], core::queue_pause::failed);
			commit(aSession);
		}
	}

	auto agent_queue_host::attach(agent_queue_viewer aViewer) -> std::uint64_t
	{
		const auto token = ++LastViewer;
		Viewers.emplace(token, std::move(aViewer));
		return token;
	}

	void agent_queue_host::detach(const std::uint64_t aViewer)
	{
		if (Viewers.erase(aViewer) == 0U) {
			return;
		}
		auto ended = std::vector<std::string>{ };
		for (const auto& [session, viewer] : Dispatchers) {
			if (viewer == aViewer) {
				ended.push_back(session);
			}
		}
		for (const auto& session : ended) {
			// Q10: closing a window neither pauses nor hands over the queue.
			// The turn it ran ended with it, after its message went.
			settle(session, core::queue_pause::none);
			drain(session);
		}
	}

	void agent_queue_host::show(const std::string& aSession)
	{
		const auto found = Queues.find(aSession);
		if (found == Queues.end()) {
			return;
		}
		// A copy of the tokens: a window may close while it is shown.
		auto tokens = std::vector<std::uint64_t>{ };
		for (const auto& [token, viewer] : Viewers) {
			tokens.push_back(token);
		}
		for (const auto token : tokens) {
			const auto viewer = Viewers.find(token);
			if (viewer != Viewers.end() && viewer->second.Changed && viewer->second.Shows && viewer->second.Shows(aSession)) {
				viewer->second.Changed(aSession, Queues[aSession]);
			}
		}
	}

	void agent_queue_host::drain(const std::string& aSession)
	{
		auto free = std::optional<std::uint64_t>{ };
		auto any  = core::queue_agent::not_set_up;
		for (const auto& [token, viewer] : Viewers) {
			if (!viewer.Shows || !viewer.Shows(aSession) || !viewer.Agent) {
				continue;
			}
			const auto agent = viewer.Agent(aSession);
			if (agent == core::queue_agent::busy) {
				// One conversation runs one turn at a time, whichever window
				// started it.
				any = core::queue_agent::busy;
				break;
			}
			if (agent == core::queue_agent::idle && !free) {
				free = token;
				any  = core::queue_agent::idle;
			}
		}
		if (any == core::queue_agent::busy || !free) {
			// Nothing starts; a queue with no window showing it waits for one.
			return;
		}
		const auto token = *free;
		start(aSession, any, [this, token, &aSession](const core::queued_entry& aEntry) {
			const auto viewer = Viewers.find(token);
			return viewer != Viewers.end() && viewer->second.Release && viewer->second.Release(aSession, aEntry); }, token);
	}

	auto agent_queue_host::shown(const std::string& aSession) const -> bool
	{
		return std::ranges::any_of(Viewers, [&aSession](const auto& aViewer) { return aViewer.second.Shows && aViewer.second.Shows(aSession); });
	}

	auto agent_queue_host::dispatcher(const std::string& aSession) const -> std::optional<std::uint64_t>
	{
		const auto found = Dispatchers.find(aSession);
		return found == Dispatchers.end() ? std::nullopt : std::optional{ found->second };
	}

	void agent_queue_host::settle(const std::string& aSession, const core::queue_pause aOutcome)
	{
		const auto found = Sending.find(aSession);
		if (found == Sending.end()) {
			return;
		}
		core::settle(Queues[aSession], found->second, aOutcome);
		Sending.erase(found);
		Dispatchers.erase(aSession);
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
