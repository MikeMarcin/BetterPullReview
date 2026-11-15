#include "bpr/application/agent_queue_host.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace bpr::application {
	agent_queue_host::agent_queue_host(agent_queue_dependencies aDependencies)
		: Dependencies{ aDependencies }
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
			// Q8 and AC04: a restored queue opens paused; nothing is sent
			// without the reviewer resuming it.
			core::restore(queue);
			Queues[session] = std::move(queue);
			show(session);
		}
	}

	void agent_queue_host::commit(const std::string& aSession)
	{
		if (Dependencies.Store != nullptr && Dependencies.Effects != nullptr) {
			// The lane is serial, so the store sees each session's writes in
			// the order the queue changed.
			try {
				Dependencies.Effects->post([store = Dependencies.Store, aSession, queue = Queues[aSession]] { static_cast<void>(store->store(aSession, queue)); });
			} catch (...) {
				// A closed lane takes no more writes; the queue stays as it is.
			}
		} else if (Dependencies.Store != nullptr) {
			static_cast<void>(Dependencies.Store->store(aSession, Queues[aSession]));
		}
		show(aSession);
		const auto waiting = queued_count();
		// A copy of the tokens: a subscriber may leave while it is told.
		auto tokens = std::vector<std::uint64_t>{ };
		for (const auto& [token, subscriber] : Subscribers) {
			tokens.push_back(token);
		}
		for (const auto token : tokens) {
			if (const auto found = Subscribers.find(token); found != Subscribers.end() && found->second.Waiting) {
				found->second.Waiting(waiting);
			}
		}
	}

	void agent_queue_host::show(const std::string& aSession)
	{
		const auto queue = Queues.find(aSession);
		if (queue == Queues.end()) {
			return;
		}
		auto tokens = std::vector<std::uint64_t>{ };
		for (const auto& [token, subscriber] : Subscribers) {
			tokens.push_back(token);
		}
		for (const auto token : tokens) {
			if (const auto found = Subscribers.find(token); found != Subscribers.end() && found->second.Changed) {
				found->second.Changed(aSession, queue->second);
			}
		}
	}

	/// The host's one decision: the session's next message goes when its
	/// queue allows it and its agent can take a turn.
	void agent_queue_host::drain(const std::string& aSession)
	{
		if (Dependencies.Runner == nullptr || Draining) {
			return;
		}
		auto& queue = Queues[aSession];
		if (Sending.contains(aSession)) {
			return;
		}
		const auto next = core::next_release(queue, Dependencies.Runner->state(aSession));
		if (!next || core::mark_sending(queue, *next) != core::queue_refusal::none) {
			return;
		}
		Sending[aSession] = *next;
		commit(aSession);
		const auto entry = std::ranges::find(Queues[aSession].Entries, *next, &core::queued_entry::Id);
		// A copy: starting the turn may change the queue it came from.
		const auto released = entry != Queues[aSession].Entries.end() ? std::optional{ *entry } : std::nullopt;
		// A runner that answers `agent_changed` synchronously must not start
		// a second turn inside this one.
		Draining           = true;
		const auto started = released && Dependencies.Runner->start(aSession, *released);
		Draining           = false;
		if (!started) {
			// Proven before any write: it stays unsent, and the queue pauses so
			// the reviewer sees why.
			Sending.erase(aSession);
			static_cast<void>(core::return_unsent(Queues[aSession], *next));
			core::pause(Queues[aSession], core::queue_pause::failed);
			commit(aSession);
		}
	}

	auto agent_queue_host::enqueue(const std::string& aSession, core::queued_input aInput) -> core::queue_refusal
	{
		// [[BPR-2181]]. A session not yet holding messages starts a queue only
		// while fewer than `maximum_queued_sessions` hold any.
		const auto found = Queues.find(aSession);
		if ((found == Queues.end() || found->second.Entries.empty())
			&& std::ranges::count_if(Queues, [](const auto& aQueue) { return !aQueue.second.Entries.empty(); }) >= static_cast<std::ptrdiff_t>(core::maximum_queued_sessions)) {
			return core::queue_refusal::full;
		}
		auto&      queue   = Queues[aSession];
		const auto refused = core::enqueue(queue, std::move(aInput), queue.Revision);
		if (refused == core::queue_refusal::none) {
			commit(aSession);
			drain(aSession);
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
			drain(aSession);
		}
		return refused;
	}

	void agent_queue_host::pause(const std::string& aSession, const core::queue_pause aWhy)
	{
		core::pause(Queues[aSession], aWhy);
		commit(aSession);
	}

	auto agent_queue_host::resume(const std::string& aSession, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		const auto refused = core::resume(queue, aSeen.value_or(queue.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
			drain(aSession);
		}
		return refused;
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

	auto agent_queue_host::steer(const std::string& aSession, const std::uint64_t aId, const std::optional<std::uint64_t> aSeen) -> agent_queue_steer_result
	{
		auto result    = agent_queue_steer_result{ };
		result.Refused = hold(aSession, aId, aSeen);
		if (result.Refused != core::queue_refusal::none) {
			return result;
		}
		const auto& entries = Queues[aSession].Entries;
		const auto  entry   = std::ranges::find(entries, aId, &core::queued_entry::Id);
		auto        offered = entry != entries.end() ? std::optional{ *entry } : std::nullopt;
		auto        why     = std::optional<std::string>{ "This message can't steer the running turn, so it goes next." };
		if (offered && Dependencies.Runner != nullptr) {
			why = Dependencies.Runner->steer(aSession, *offered);
		}
		static_cast<void>(release_hold(aSession, aId, std::nullopt));
		if (!why) {
			// It went into the running turn: it leaves the queue.
			result.Sent = withdraw(aSession, aId) == core::queue_refusal::none;
			return result;
		}
		static_cast<void>(reorder(aSession, aId, -static_cast<int>(core::maximum_queued_inputs)));
		result.Why = std::move(*why);
		return result;
	}

	void agent_queue_host::agent_changed(const std::string& aSession) { drain(aSession); }

	void agent_queue_host::retired(const std::string& aSession, const core::queued_finish aFinish)
	{
		const auto found = Sending.find(aSession);
		if (found != Sending.end()) {
			static_cast<void>(core::finish(Queues[aSession], found->second, aFinish));
			Sending.erase(found);
			commit(aSession);
		}
		drain(aSession);
	}

	void agent_queue_host::returned(const std::string& aSession) { retired(aSession, core::queued_finish::not_taken); }

	auto agent_queue_host::discard(const std::string& aSession, const std::uint64_t aId, const std::optional<std::uint64_t> aSeen) -> core::queue_refusal
	{
		const auto found = Queues.find(aSession);
		if (found == Queues.end()) {
			return core::queue_refusal::unknown_entry;
		}
		const auto refused = core::discard_uncertain(found->second, aId, aSeen.value_or(found->second.Revision));
		if (refused == core::queue_refusal::none) {
			commit(aSession);
			drain(aSession);
		}
		return refused;
	}

	auto agent_queue_host::subscribe(agent_queue_subscriber aSubscriber) -> std::uint64_t
	{
		const auto token = ++LastSubscriber;
		Subscribers.emplace(token, std::move(aSubscriber));
		return token;
	}

	void agent_queue_host::unsubscribe(const std::uint64_t aSubscriber) { Subscribers.erase(aSubscriber); }

	auto agent_queue_host::queued_count() const -> std::size_t
	{
		auto count = std::size_t{ 0 };
		for (const auto& [session, queue] : Queues) {
			count += static_cast<std::size_t>(std::ranges::count(queue.Entries, core::queued_state::queued, &core::queued_entry::State));
		}
		return count;
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
