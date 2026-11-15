#include "bpr/core/agent_queue.hpp"

#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto bytes_of(const queued_input& aInput) noexcept -> std::size_t
		{
			return aInput.QuickAsk.size() + aInput.Scope.size() + aInput.File.size() + aInput.Thread.size() + aInput.Label.size() + aInput.Text.size();
		}

		[[nodiscard]] auto unsent(const queued_entry& aEntry) noexcept -> bool { return aEntry.State == queued_state::queued; }

		[[nodiscard]] auto find(agent_queue& aQueue, const std::uint64_t aId) -> std::vector<queued_entry>::iterator
		{
			return std::ranges::find(aQueue.Entries, aId, &queued_entry::Id);
		}
	} // namespace

	auto stable_name(const queued_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case queued_state::queued:           return "queued";
		case queued_state::sending:          return "sending";
		case queued_state::sent:             return "sent";
		case queued_state::delivery_unknown: return "delivery-unknown";
		}
		return "unknown";
	}

	auto stable_name(const queue_pause aPause) noexcept -> std::string_view
	{
		switch (aPause) {
		case queue_pause::none:             return "none";
		case queue_pause::reviewer:         return "reviewer";
		case queue_pause::stopped:          return "stopped";
		case queue_pause::failed:           return "failed";
		case queue_pause::usage_limit:      return "usage-limit";
		case queue_pause::restarted:        return "restarted";
		case queue_pause::delivery_unknown: return "delivery-unknown";
		case queue_pause::changed_target:   return "changed-target";
		case queue_pause::lost_access:      return "lost-access";
		}
		return "unknown";
	}

	auto stable_name(const queue_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case queue_refusal::none:          return "none";
		case queue_refusal::empty:         return "empty";
		case queue_refusal::too_long:      return "too-long";
		case queue_refusal::full:          return "full";
		case queue_refusal::too_large:     return "too-large";
		case queue_refusal::stale:         return "stale";
		case queue_refusal::unknown_entry: return "unknown-entry";
		case queue_refusal::not_editable:  return "not-editable";
		}
		return "unknown";
	}

	auto queue_bytes(const agent_queue& aQueue) noexcept -> std::size_t
	{
		auto total = std::size_t{ 0 };
		for (const auto& entry : aQueue.Entries) {
			total += bytes_of(entry.Input);
		}
		return total;
	}

	auto enqueue(agent_queue& aQueue, queued_input aInput, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		if (aInput.Label.empty() && aInput.Text.empty()) {
			return queue_refusal::empty;
		}
		if (aInput.Text.size() > maximum_queued_text_bytes || !utf8::valid(aInput.Text) || !utf8::valid(aInput.Label)) {
			return queue_refusal::too_long;
		}
		if (aQueue.Entries.size() >= maximum_queued_inputs) {
			return queue_refusal::full;
		}
		if (queue_bytes(aQueue) + bytes_of(aInput) > maximum_queue_bytes) {
			return queue_refusal::too_large;
		}
		aQueue.Entries.push_back({ .Id = ++aQueue.LastId, .Input = std::move(aInput) });
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto withdraw(agent_queue& aQueue, const std::uint64_t aId, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		// A message being edited (in any window) is not removed under it.
		if (!unsent(*found) || found->Held) {
			return queue_refusal::not_editable;
		}
		aQueue.Entries.erase(found);
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto reorder(agent_queue& aQueue, const std::uint64_t aId, const int aBy, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (!unsent(*found)) {
			return queue_refusal::not_editable;
		}
		auto index  = std::distance(aQueue.Entries.begin(), found);
		auto target = index;
		for (auto steps = aBy; steps != 0;) {
			const auto next = target + (steps > 0 ? 1 : -1);
			if (next < 0 || next >= static_cast<std::ptrdiff_t>(aQueue.Entries.size()) || !unsent(aQueue.Entries[static_cast<std::size_t>(next)])) {
				break;
			}
			target = next;
			steps += steps > 0 ? -1 : 1;
		}
		if (target == index) {
			return queue_refusal::none;
		}
		auto moved = std::move(aQueue.Entries[static_cast<std::size_t>(index)]);
		aQueue.Entries.erase(aQueue.Entries.begin() + index);
		aQueue.Entries.insert(aQueue.Entries.begin() + target, std::move(moved));
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto hold(agent_queue& aQueue, const std::uint64_t aId, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (!unsent(*found) || found->Held) {
			return queue_refusal::not_editable;
		}
		// Q9 as refined (owner, 2026-11-13): held, not paused; nothing is
		// sent past it, and Save or Cancel lets the queue continue.
		found->Held = true;
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto release_hold(agent_queue& aQueue, const std::uint64_t aId, std::optional<std::string> aText, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end() || !found->Held) {
			return queue_refusal::unknown_entry;
		}
		if (aText) {
			if (aText->empty()) {
				return queue_refusal::empty;
			}
			if (aText->size() > maximum_queued_text_bytes || !utf8::valid(*aText)) {
				return queue_refusal::too_long;
			}
			if (queue_bytes(aQueue) - found->Input.Text.size() + aText->size() > maximum_queue_bytes) {
				return queue_refusal::too_large;
			}
			found->Input.Text = std::move(*aText);
		}
		found->Held = false;
		++aQueue.Revision;
		return queue_refusal::none;
	}

	void pause(agent_queue& aQueue, const queue_pause aWhy)
	{
		if (aWhy != queue_pause::none && aQueue.Paused != aWhy) {
			aQueue.Paused = aWhy;
			++aQueue.Revision;
		}
	}

	auto resume(agent_queue& aQueue, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto blocked = std::ranges::any_of(aQueue.Entries, [](const queued_entry& aEntry) { return aEntry.Held || aEntry.State == queued_state::delivery_unknown; });
		if (blocked) {
			return queue_refusal::not_editable;
		}
		if (aQueue.Paused != queue_pause::none) {
			aQueue.Paused = queue_pause::none;
			++aQueue.Revision;
		}
		return queue_refusal::none;
	}

	auto next_release(const agent_queue& aQueue, const queue_agent aAgent) -> std::optional<std::uint64_t>
	{
		if (aQueue.Paused != queue_pause::none || aAgent != queue_agent::idle) {
			return std::nullopt;
		}
		if (std::ranges::any_of(aQueue.Entries, [](const queued_entry& aEntry) { return aEntry.State == queued_state::sending || aEntry.State == queued_state::delivery_unknown; })) {
			return std::nullopt;
		}
		const auto first = std::ranges::find_if(aQueue.Entries, unsent);
		if (first == aQueue.Entries.end() || first->Held) {
			return std::nullopt;
		}
		return first->Id;
	}

	auto mark_sending(agent_queue& aQueue, const std::uint64_t aId) -> queue_refusal
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (!unsent(*found) || found->Held) {
			return queue_refusal::not_editable;
		}
		found->State = queued_state::sending;
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto return_unsent(agent_queue& aQueue, const std::uint64_t aId) -> queue_refusal
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (found->State != queued_state::sending) {
			return queue_refusal::not_editable;
		}
		auto entry  = *found;
		entry.State = queued_state::queued;
		aQueue.Entries.erase(found);
		const auto first = std::ranges::find_if(aQueue.Entries, [](const queued_entry& aEntry) { return aEntry.State == queued_state::queued; });
		aQueue.Entries.insert(first, std::move(entry));
		++aQueue.Revision;
		return queue_refusal::none;
	}

	void settle(agent_queue& aQueue, const std::uint64_t aId, const queue_pause aOutcome)
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return;
		}
		if (aOutcome == queue_pause::delivery_unknown) {
			found->State = queued_state::delivery_unknown;
		} else {
			aQueue.Entries.erase(found);
		}
		if (aOutcome != queue_pause::none) {
			aQueue.Paused = aOutcome;
		}
		++aQueue.Revision;
	}

	auto stable_name(const queued_finish aFinish) noexcept -> std::string_view
	{
		switch (aFinish) {
		case queued_finish::went:        return "went";
		case queued_finish::failed:      return "failed";
		case queued_finish::usage_limit: return "usage-limit";
		case queued_finish::stopped:     return "stopped";
		case queued_finish::not_taken:   return "not-taken";
		}
		return "unknown";
	}

	auto pause_for(const queued_finish aFinish) noexcept -> queue_pause
	{
		switch (aFinish) {
		case queued_finish::went:
		case queued_finish::not_taken:   return queue_pause::none;
		case queued_finish::failed:      return queue_pause::failed;
		case queued_finish::usage_limit: return queue_pause::usage_limit;
		case queued_finish::stopped:     return queue_pause::stopped;
		}
		return queue_pause::failed;
	}

	auto finish(agent_queue& aQueue, const std::uint64_t aId, const queued_finish aFinish) -> queue_refusal
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (found->State != queued_state::sending) {
			return queue_refusal::not_editable;
		}
		if (aFinish == queued_finish::not_taken) {
			return return_unsent(aQueue, aId);
		}
		aQueue.Entries.erase(found);
		if (aQueue.Paused == queue_pause::none) {
			aQueue.Paused = pause_for(aFinish);
		}
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto discard_uncertain(agent_queue& aQueue, const std::uint64_t aId, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (found->State != queued_state::delivery_unknown) {
			return queue_refusal::not_editable;
		}
		aQueue.Entries.erase(found);
		++aQueue.Revision;
		return queue_refusal::none;
	}

	void restore(agent_queue& aQueue)
	{
		for (auto& entry : aQueue.Entries) {
			if (entry.State == queued_state::sending) {
				entry.State = queued_state::delivery_unknown;
			}
			entry.Held = false;
		}
		if (!aQueue.Entries.empty()) {
			aQueue.Paused = queue_pause::restarted;
		}
	}
} // namespace bpr::core
