#include "bpr/application/agent_queue_host.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace bpr::application {
	auto stable_name(const queued_admission_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case queued_admission_kind::started:  return "started";
		case queued_admission_kind::awaiting: return "awaiting";
		case queued_admission_kind::answered: return "answered";
		case queued_admission_kind::refused:  return "refused";
		}
		return "unknown";
	}

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

	agent_queue_host::~agent_queue_host() { *Alive = false; }

	auto agent_queue_host::commit(const std::string& aSession, const std::optional<std::uint64_t> aAdded) -> bool
	{
		auto kept = true;
		if (Dependencies.Store != nullptr && Dependencies.Effects != nullptr) {
			// The lane is serial, so the store sees each session's writes in
			// the order the queue changed.
			try {
				Dependencies.Effects->post([store = Dependencies.Store, lane = Dependencies.DecisionLane, alive = std::weak_ptr<bool>{ Alive }, this, aSession, aAdded, queue = Queues[aSession]] {
					if (store->store(aSession, queue) || !aAdded || lane == nullptr) {
						return;
					}
					try {
						lane->post([alive, this, aSession, added = *aAdded] {
							if (const auto live = alive.lock(); live && *live) {
								unsaved(aSession, added);
							}
						});
					} catch (...) {
						// A closed lane: the profile is going; nothing is shown.
					}
				});
			} catch (...) {
				// A closed lane takes no more writes; the queue stays as it is.
			}
		} else if (Dependencies.Store != nullptr) {
			kept = Dependencies.Store->store(aSession, Queues[aSession]);
		}
		if (!kept && aAdded) {
			return false;
		}
		show(aSession);
		notify_waiting();
		return kept;
	}

	void agent_queue_host::commit_then(const std::string& aSession, std::function<void(bool)> aThen)
	{
		if (Dependencies.Store != nullptr && Dependencies.Effects != nullptr && Dependencies.DecisionLane != nullptr) {
			// The serial lane keeps every earlier write of the session ahead of
			// this one; its verdict comes back to the decision lane.
			show(aSession);
			notify_waiting();
			try {
				Dependencies.Effects->post([store = Dependencies.Store, lane = Dependencies.DecisionLane, alive = std::weak_ptr<bool>{ Alive }, aSession, queue = Queues[aSession], then = std::move(aThen)] {
					const auto kept = store->store(aSession, queue);
					try {
						lane->post([alive, then, kept] {
							if (const auto live = alive.lock(); live && *live) {
								then(kept);
							}
						});
					} catch (...) {
						// A closed lane: the profile is going; nothing is written
						// to an agent, and the stored claim reads as uncertain.
					}
				});
			} catch (...) {
				// A closed lane takes no more writes: nothing was kept, so
				// nothing may be written to an agent.
				aThen(false);
			}
			return;
		}
		const auto kept = Dependencies.Store == nullptr || Dependencies.Store->store(aSession, Queues[aSession]);
		show(aSession);
		notify_waiting();
		aThen(kept);
	}

	void agent_queue_host::notify_waiting()
	{
		const auto waiting = waiting_count();
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

	/// AC05. The store refused the write that queued `aAdded`: a message
	/// still waiting leaves the queue and is handed back as it was asked. One
	/// already released went to the agent and stays as it is.
	void agent_queue_host::unsaved(const std::string& aSession, const std::uint64_t aAdded)
	{
		auto&      queue = Queues[aSession];
		const auto entry = std::ranges::find(queue.Entries, aAdded, &core::queued_entry::Id);
		if (entry == queue.Entries.end() || entry->State != core::queued_state::queued) {
			return;
		}
		const auto input = entry->Input;
		if (core::withdraw(queue, aAdded, queue.Revision) != core::queue_refusal::none) {
			return;
		}
		static_cast<void>(commit(aSession));
		auto tokens = std::vector<std::uint64_t>{ };
		for (const auto& [token, subscriber] : Subscribers) {
			tokens.push_back(token);
		}
		for (const auto token : tokens) {
			if (const auto found = Subscribers.find(token); found != Subscribers.end() && found->second.Unsaved) {
				found->second.Unsaved(aSession, input);
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
	/// queue allows it and its agent can take a turn. [[BPR-2152]] (AC06):
	/// the exact entry is claimed and stored before its turn starts.
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
		if (!next || core::claim(queue, *next, core::queued_claim{ .Conversation = Dependencies.Runner->bound(aSession).Conversation }, queue.Revision) != core::queue_refusal::none) {
			return;
		}
		Sending[aSession] = *next;
		commit_then(aSession, [this, aSession, id = *next](const bool aKept) { start_claimed(aSession, id, aKept); });
	}

	void agent_queue_host::start_claimed(const std::string& aSession, const std::uint64_t aId, const bool aKept)
	{
		const auto still = Sending.find(aSession);
		if (still == Sending.end() || still->second != aId) {
			return;
		}
		auto&      queue = Queues[aSession];
		const auto entry = std::ranges::find(queue.Entries, aId, &core::queued_entry::Id);
		if (entry == queue.Entries.end() || entry->State != core::queued_state::sending) {
			Sending.erase(still);
			return;
		}
		// Nothing was written yet: a claim not kept, a queue stopped (Stop
		// all, a lost access) or an agent taken while it was stored leaves it
		// unsent, first. Pause queue stops what comes after it, not the
		// message already shown as Sending.
		const auto recalled = queue.Paused != core::queue_pause::none && queue.Paused != core::queue_pause::reviewer;
		if (!aKept || recalled || Dependencies.Runner == nullptr || Dependencies.Runner->state(aSession) != core::queue_agent::idle) {
			Sending.erase(still);
			static_cast<void>(core::finish(queue, aId, aKept ? core::queued_finish::not_taken : core::queued_finish::refused));
			commit(aSession);
			return;
		}
		// A copy: starting the turn may change the queue it came from.
		const auto released = *entry;
		// A runner that answers `agent_changed` synchronously must not start
		// a second turn inside this one.
		Draining             = true;
		const auto admission = Dependencies.Runner->start(aSession, released);
		Draining             = false;
		// AC03: a started or awaiting release is settled by its own run's
		// retirement (`retired`), which may already have come.
		if (admission.Kind == queued_admission_kind::started || admission.Kind == queued_admission_kind::awaiting) {
			if (const auto now = Sending.find(aSession); now != Sending.end() && now->second == aId && core::bind_run(Queues[aSession], aId, admission.Run) == core::queue_refusal::none) {
				// The receipt: the claim names its run.
				commit(aSession);
			}
			return;
		}
		const auto after = Sending.find(aSession);
		if (after == Sending.end() || after->second != aId) {
			return;
		}
		Sending.erase(after);
		// Answered with no run, it went; refused, it goes back first and the
		// queue pauses so the reviewer sees why.
		static_cast<void>(core::finish(Queues[aSession], aId, admission.Kind == queued_admission_kind::answered ? core::queued_finish::went : core::queued_finish::refused));
		commit(aSession);
		if (admission.Kind == queued_admission_kind::answered) {
			drain(aSession);
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
		auto       before  = queue;
		const auto refused = core::enqueue(queue, std::move(aInput), queue.Revision);
		if (refused != core::queue_refusal::none) {
			return refused;
		}
		if (!commit(aSession, queue.LastId)) {
			// AC05: a store that refused it here queued nothing.
			Queues[aSession] = std::move(before);
			return core::queue_refusal::not_saved;
		}
		drain(aSession);
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

	auto agent_queue_host::steer(const std::string& aSession, const std::uint64_t aId, const std::optional<std::uint64_t> aSeen, agent_queue_steer_done aDone) -> core::queue_refusal
	{
		auto&      queue   = Queues[aSession];
		auto       bound   = Dependencies.Runner != nullptr ? Dependencies.Runner->bound(aSession) : core::queued_claim{ };
		const auto refused = core::claim(queue, aId, core::queued_claim{ .Route = core::queued_route::steer, .Run = bound.Run, .Conversation = bound.Conversation }, aSeen.value_or(queue.Revision));
		if (refused != core::queue_refusal::none) {
			return refused;
		}
		commit_then(aSession, [this, aSession, aId, done = std::move(aDone)](const bool aKept) { offer_claimed(aSession, aId, aKept, done); });
		return refused;
	}

	void agent_queue_host::offer_claimed(const std::string& aSession, const std::uint64_t aId, const bool aKept, const agent_queue_steer_done& aDone)
	{
		auto&      queue = Queues[aSession];
		const auto entry = std::ranges::find(queue.Entries, aId, &core::queued_entry::Id);
		if (entry == queue.Entries.end() || entry->State != core::queued_state::sending || !entry->Claim || entry->Claim->Route != core::queued_route::steer) {
			return;
		}
		auto why = std::optional<std::string>{ "This message couldn't be saved before steering, so it wasn't sent. It goes next." };
		if (aKept) {
			why = std::optional<std::string>{ "This message can't steer the running turn, so it goes next." };
			if (Dependencies.Runner != nullptr) {
				// A copy: the offer may answer at once and change the queue.
				const auto offered = *entry;
				why                = Dependencies.Runner->steer(aSession, offered);
			}
		}
		auto result = agent_queue_steer_result{ };
		if (!why) {
			// Handed to the turn: it stays Sending until the runtime says it
			// was written, then Sent until the run's end settles it.
			result.Sent = true;
		} else {
			// Nothing was written: it goes next, with its identity.
			const auto now = std::ranges::find(Queues[aSession].Entries, aId, &core::queued_entry::Id);
			if (now != Queues[aSession].Entries.end() && now->State == core::queued_state::sending && core::return_unsent(Queues[aSession], aId) == core::queue_refusal::none) {
				commit(aSession);
			}
			result.Why = std::move(*why);
		}
		if (aDone) {
			aDone(result);
		}
	}

	void agent_queue_host::steer_answered(const std::string& aSession, const std::uint64_t aId, const bool aWritten, std::string aReceipt)
	{
		const auto found = Queues.find(aSession);
		if (found == Queues.end()) {
			return;
		}
		const auto entry = std::ranges::find(found->second.Entries, aId, &core::queued_entry::Id);
		if (entry == found->second.Entries.end() || entry->State != core::queued_state::sending || !entry->Claim || entry->Claim->Route != core::queued_route::steer) {
			// A late answer, or one for a record already settled: nothing.
			return;
		}
		const auto refused = aWritten ? core::acknowledge(found->second, aId, std::move(aReceipt)) : core::return_unsent(found->second, aId);
		if (refused == core::queue_refusal::none) {
			commit(aSession);
			drain(aSession);
		}
	}

	void agent_queue_host::settle_steers(const std::string& aSession, const std::uint64_t aRun, const std::vector<core::steered_settlement>& aEndings)
	{
		const auto found = Queues.find(aSession);
		if (found == Queues.end()) {
			return;
		}
		const auto before = found->second.Revision;
		core::settle_steers(found->second, aRun, aEndings);
		if (found->second.Revision != before) {
			commit(aSession);
		}
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

	auto agent_queue_host::waiting_count() const -> std::size_t
	{
		auto count = std::size_t{ 0 };
		for (const auto& [session, queue] : Queues) {
			if (queue.Paused != core::queue_pause::none) {
				continue;
			}
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
