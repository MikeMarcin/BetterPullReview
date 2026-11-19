#include "bpr/core/agent_queue.hpp"

#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto bytes_of(const queued_input& aInput) noexcept -> std::size_t
		{
			auto threads = std::size_t{ 0U };
			for (const auto& thread : aInput.Threads) {
				threads += thread.size() + 1U;
			}
			const auto prompt = aInput.Prompt ? aInput.Prompt->Id.size() + aInput.Prompt->Label.size() + aInput.Prompt->Expansion.Text.size() : std::size_t{ 0U };
			return aInput.QuickAsk.size() + aInput.Scope.size() + aInput.File.size() + threads + aInput.Label.size() + aInput.Text.size() + prompt;
		}

		/// One decimal field and its `;`.
		void put_number(std::string& aTo, const std::size_t aValue)
		{
			aTo += std::to_string(aValue);
			aTo += ';';
		}

		[[nodiscard]] auto take_number(std::string_view& aFrom) -> std::optional<std::size_t>
		{
			const auto end = aFrom.find(';');
			if (end == std::string_view::npos || end == 0U || end > 9U) {
				return std::nullopt;
			}
			auto value = std::size_t{ 0U };
			for (const auto digit : aFrom.substr(0U, end)) {
				if (digit < '0' || digit > '9') {
					return std::nullopt;
				}
				value = value * 10U + static_cast<std::size_t>(digit - '0');
			}
			aFrom.remove_prefix(end + 1U);
			return value;
		}

		[[nodiscard]] auto take_text(std::string_view& aFrom) -> std::optional<std::string>
		{
			const auto size = take_number(aFrom);
			if (!size || *size > aFrom.size()) {
				return std::nullopt;
			}
			auto text = std::string{ aFrom.substr(0U, *size) };
			aFrom.remove_prefix(*size);
			return text;
		}

		constexpr auto queued_prompt_version = std::string_view{ "p1;" };
		/// [[BPR-2248]]. With the commits an offer about selected commits
		/// asks about, after its texts; `p1;` is read as before.
		constexpr auto queued_prompt_commits_version = std::string_view{ "p2;" };

		// [[BPR-2274]]. A queued prompt keeps the numbers it was written with
		// before the tour's Problem and Solution chat went: place 3 was that
		// chat and subject 5 its part. An offer queued there is not read.
		constexpr auto     retired_wire_place   = std::size_t{ 3U };
		constexpr auto     retired_wire_subject = std::size_t{ 5U };
		[[nodiscard]] auto wire_place(const prompt_place aPlace) noexcept -> std::size_t
		{
			const auto value = static_cast<std::size_t>(aPlace);
			return value >= retired_wire_place ? value + 1U : value;
		}
		[[nodiscard]] auto wire_subject(const prompt_subject aSubject) noexcept -> std::size_t
		{
			const auto value = static_cast<std::size_t>(aSubject);
			return value >= retired_wire_subject ? value + 1U : value;
		}
		[[nodiscard]] auto place_of_wire(const std::optional<std::size_t> aWire) noexcept -> std::optional<prompt_place>
		{
			if (!aWire || *aWire == retired_wire_place || *aWire > prompt_place_count) {
				return std::nullopt;
			}
			return static_cast<prompt_place>(*aWire > retired_wire_place ? *aWire - 1U : *aWire);
		}
		[[nodiscard]] auto subject_of_wire(const std::optional<std::size_t> aWire) noexcept -> std::optional<prompt_subject>
		{
			if (!aWire || *aWire == retired_wire_subject || *aWire > prompt_subject_count) {
				return std::nullopt;
			}
			return static_cast<prompt_subject>(*aWire > retired_wire_subject ? *aWire - 1U : *aWire);
		}
		constexpr auto queued_claim_version = std::string_view{ "c1;" };

		/// One unsigned 64-bit decimal field and its `;`.
		[[nodiscard]] auto take_wide(std::string_view& aFrom) -> std::optional<std::uint64_t>
		{
			const auto end = aFrom.find(';');
			if (end == std::string_view::npos || end == 0U || end > 20U) {
				return std::nullopt;
			}
			constexpr auto most  = std::numeric_limits<std::uint64_t>::max();
			auto           value = std::uint64_t{ 0U };
			for (const auto digit : aFrom.substr(0U, end)) {
				if (digit < '0' || digit > '9') {
					return std::nullopt;
				}
				const auto next = static_cast<std::uint64_t>(digit - '0');
				if (value > (most - next) / 10U) {
					return std::nullopt;
				}
				value = value * 10U + next;
			}
			aFrom.remove_prefix(end + 1U);
			return value;
		}

		/// Written but not settled: a correction or release the agent may
		/// hold.
		[[nodiscard]] auto in_flight(const queued_entry& aEntry) noexcept -> bool
		{
			return aEntry.State == queued_state::sending || aEntry.State == queued_state::sent;
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

	auto stable_name(const queued_route aRoute) noexcept -> std::string_view
	{
		switch (aRoute) {
		case queued_route::release: return "release";
		case queued_route::steer:   return "steer";
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
		case queue_refusal::not_saved:     return "not-saved";
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
		if (aInput.Text.size() > maximum_queued_text_bytes || !utf8::valid(aInput.Text) || !utf8::valid(aInput.Label)
			|| (aInput.Prompt && (aInput.Prompt->Expansion.Text.size() > maximum_queued_text_bytes || !utf8::valid(aInput.Prompt->Expansion.Text)))) {
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
		if (std::ranges::any_of(aQueue.Entries, [](const queued_entry& aEntry) { return in_flight(aEntry) || aEntry.State == queued_state::delivery_unknown; })) {
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
		found->Claim = queued_claim{ };
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto claim(agent_queue& aQueue, const std::uint64_t aId, queued_claim aClaim, const std::uint64_t aFrom) -> queue_refusal
	{
		if (aFrom != aQueue.Revision) {
			return queue_refusal::stale;
		}
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (!unsent(*found) || (found->Held && aClaim.Route == queued_route::release)) {
			return queue_refusal::not_editable;
		}
		aClaim.Receipt.clear();
		found->State = queued_state::sending;
		found->Held  = false;
		found->Claim = std::move(aClaim);
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto bind_run(agent_queue& aQueue, const std::uint64_t aId, const std::uint64_t aRun) -> queue_refusal
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		if (found->State != queued_state::sending || !found->Claim || found->Claim->Route != queued_route::release) {
			return queue_refusal::not_editable;
		}
		found->Claim->Run = aRun;
		++aQueue.Revision;
		return queue_refusal::none;
	}

	auto acknowledge(agent_queue& aQueue, const std::uint64_t aId, std::string aReceipt) -> queue_refusal
	{
		const auto found = find(aQueue, aId);
		if (found == aQueue.Entries.end()) {
			return queue_refusal::unknown_entry;
		}
		// A late or second acknowledgment changes nothing: one write, one
		// receipt, on the record it came from.
		if (found->State != queued_state::sending || !found->Claim || found->Claim->Route != queued_route::steer || aReceipt.empty()) {
			return queue_refusal::not_editable;
		}
		// An identity too long to keep is not kept: the run's end then cannot
		// name it, and it settles as unknown rather than as taken.
		if (aReceipt.size() > maximum_claim_receipt_bytes || !utf8::valid(aReceipt)) {
			aReceipt.clear();
		}
		found->State          = queued_state::sent;
		found->Claim->Receipt = std::move(aReceipt);
		++aQueue.Revision;
		return queue_refusal::none;
	}

	void settle_steers(agent_queue& aQueue, const std::uint64_t aRun, const std::vector<steered_settlement>& aEndings)
	{
		auto taken   = std::vector<std::uint64_t>{ };
		auto back    = std::vector<std::uint64_t>{ };
		auto unknown = false;
		for (auto& entry : aQueue.Entries) {
			if (entry.State != queued_state::sent || !entry.Claim || entry.Claim->Route != queued_route::steer || entry.Claim->Run != aRun) {
				continue;
			}
			const auto ending = std::ranges::find(aEndings, entry.Claim->Receipt, &steered_settlement::Receipt);
			if (ending != aEndings.end() && ending->Ending == steered_ending::consumed) {
				taken.push_back(entry.Id);
			} else if (ending != aEndings.end() && ending->Ending == steered_ending::not_taken) {
				back.push_back(entry.Id);
			} else {
				// Written and never named by the run's end: kept, never resent.
				entry.State = queued_state::delivery_unknown;
				unknown     = true;
			}
		}
		std::erase_if(aQueue.Entries, [&taken](const queued_entry& aEntry) { return std::ranges::find(taken, aEntry.Id) != taken.end(); });
		for (const auto id : back) {
			find(aQueue, id)->State = queued_state::sending;
			static_cast<void>(return_unsent(aQueue, id));
		}
		if (unknown) {
			aQueue.Paused = queue_pause::delivery_unknown;
		}
		if (unknown || !taken.empty() || !back.empty()) {
			++aQueue.Revision;
		}
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
		entry.Claim.reset();
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
		case queued_finish::refused:     return "refused";
		case queued_finish::unknown:     return "unknown";
		}
		return "unknown";
	}

	auto pause_for(const queued_finish aFinish) noexcept -> queue_pause
	{
		switch (aFinish) {
		case queued_finish::went:
		case queued_finish::not_taken:   return queue_pause::none;
		case queued_finish::failed:
		case queued_finish::refused:     return queue_pause::failed;
		case queued_finish::usage_limit: return queue_pause::usage_limit;
		case queued_finish::stopped:     return queue_pause::stopped;
		case queued_finish::unknown:     return queue_pause::delivery_unknown;
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
		if (aFinish == queued_finish::refused) {
			// Nothing reached the agent: it goes back first, and the queue
			// pauses so a refusal that would recur is not released again.
			const auto returned = return_unsent(aQueue, aId);
			if (aQueue.Paused == queue_pause::none) {
				aQueue.Paused = pause_for(aFinish);
				++aQueue.Revision;
			}
			return returned;
		}
		if (aFinish == queued_finish::unknown) {
			// [[BPR-2152]]. Kept, with its claim, and never resent: only the
			// reviewer discards it. Unknown delivery outranks a pause in force.
			found->State  = queued_state::delivery_unknown;
			aQueue.Paused = queue_pause::delivery_unknown;
			++aQueue.Revision;
			return queue_refusal::none;
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
			if (in_flight(entry)) {
				entry.State = queued_state::delivery_unknown;
			}
			entry.Held = false;
		}
		if (!aQueue.Entries.empty()) {
			aQueue.Paused = queue_pause::restarted;
		}
	}

	auto stable_name(const follow_up aFollowUp) noexcept -> std::string_view
	{
		switch (aFollowUp) {
		case follow_up::send:  return "send";
		case follow_up::queue: return "queue";
		case follow_up::steer: return "steer";
		}
		return "unknown";
	}

	auto decide_follow_up(const follow_up_facts& aFacts) noexcept -> follow_up
	{
		if (aFacts.Empty || (!aFacts.TurnShown && !aFacts.QueueHolds)) {
			return follow_up::send;
		}
		return aFacts.SteerByDefault != aFacts.Opposite ? follow_up::steer : follow_up::queue;
	}

	auto holds_messages(const agent_queue& aQueue) noexcept -> bool { return !aQueue.Entries.empty(); }

	namespace {
		[[nodiscard]] auto kind_prefix(const queued_session_kind aKind) noexcept -> std::string_view
		{
			switch (aKind) {
			case queued_session_kind::agent_tab:              return "agent-tab:";
			case queued_session_kind::tour_feedback:          return "tour-feedback:";
			case queued_session_kind::tour_part:              return "tour-part:";
			case queued_session_kind::local_draft_discussion: return "local-draft:";
			}
			return "agent-tab:";
		}

		[[nodiscard]] auto all_digits(const std::string_view aText) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= 18U && std::ranges::all_of(aText, [](const char aDigit) { return aDigit >= '0' && aDigit <= '9'; });
		}

		[[nodiscard]] auto number_of(const std::string_view aDigits) noexcept -> std::uint64_t
		{
			auto number = std::uint64_t{ 0U };
			for (const auto digit : aDigits) {
				number = number * 10U + static_cast<std::uint64_t>(digit - '0');
			}
			return number;
		}

		/// A part name or a draft identity: printable ASCII with no `/`, `#` or space.
		[[nodiscard]] auto plain_name(const std::string_view aText) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= 128U && std::ranges::all_of(aText, [](const char aChar) { return aChar > ' ' && aChar < 127 && aChar != '/' && aChar != '#'; });
		}
	} // namespace

	auto session_key(const queued_session& aSession) -> std::string
	{
		if (aSession.Kind == queued_session_kind::local_draft_discussion) {
			if (!plain_name(aSession.Draft) || !tour_part_thread(aSession.Part)) {
				return { };
			}
			return std::string{ kind_prefix(aSession.Kind) } + aSession.Draft + "/" + aSession.Part;
		}
		if (aSession.Host.empty() || aSession.Owner.empty() || aSession.Repository.empty() || aSession.Number == 0U) {
			return { };
		}
		auto key = std::string{ kind_prefix(aSession.Kind) } + aSession.Host + "/" + aSession.Owner + "/" + aSession.Repository + "#" + std::to_string(aSession.Number);
		if (aSession.Kind == queued_session_kind::tour_part) {
			if (!tour_part_thread(aSession.Part)) {
				return { };
			}
			key += "/" + aSession.Part;
		}
		return key;
	}

	auto parse_session_key(const std::string_view aKey) -> std::optional<queued_session>
	{
		auto session = queued_session{ };
		auto rest    = aKey;
		auto matched = false;
		for (const auto kind : { queued_session_kind::agent_tab, queued_session_kind::tour_feedback, queued_session_kind::tour_part, queued_session_kind::local_draft_discussion }) {
			if (rest.starts_with(kind_prefix(kind))) {
				session.Kind = kind;
				rest.remove_prefix(kind_prefix(kind).size());
				matched = true;
				break;
			}
		}
		if (!matched) {
			return std::nullopt;
		}
		if (session.Kind == queued_session_kind::local_draft_discussion) {
			const auto slash = rest.find('/');
			if (slash == std::string_view::npos) {
				return std::nullopt;
			}
			session.Draft = std::string{ rest.substr(0U, slash) };
			session.Part  = std::string{ rest.substr(slash + 1U) };
			return session_key(session) == aKey ? std::optional{ session } : std::nullopt;
		}
		const auto hash = rest.rfind('#');
		if (hash == std::string_view::npos) {
			return std::nullopt;
		}
		const auto path   = rest.substr(0U, hash);
		auto       digits = rest.substr(hash + 1U);
		if (session.Kind == queued_session_kind::tour_part) {
			const auto slash = digits.find('/');
			if (slash == std::string_view::npos) {
				return std::nullopt;
			}
			session.Part = std::string{ digits.substr(slash + 1U) };
			digits       = digits.substr(0U, slash);
		}
		const auto first  = path.find('/');
		const auto second = first == std::string_view::npos ? std::string_view::npos : path.find('/', first + 1U);
		if (second == std::string_view::npos || path.find('/', second + 1U) != std::string_view::npos || !all_digits(digits)) {
			return std::nullopt;
		}
		session.Host       = std::string{ path.substr(0U, first) };
		session.Owner      = std::string{ path.substr(first + 1U, second - first - 1U) };
		session.Repository = std::string{ path.substr(second + 1U) };
		session.Number     = number_of(digits);
		if (session_key(session) != aKey) {
			return std::nullopt;
		}
		return session;
	}

	auto with_kind(queued_session aSession, const queued_session_kind aKind) -> queued_session
	{
		aSession.Kind = aKind;
		if (aKind != queued_session_kind::tour_part && aKind != queued_session_kind::local_draft_discussion) {
			aSession.Part.clear();
		}
		if (aKind != queued_session_kind::local_draft_discussion) {
			aSession.Draft.clear();
		}
		return aSession;
	}

	auto tour_part_name(const tour_thread& aThread) -> std::string
	{
		switch (aThread.Kind) {
		case tour_thread_kind::tour: return "tour";
		case tour_thread_kind::stop: return "stop-" + std::to_string(aThread.Stop.value());
		}
		return "tour";
	}

	auto tour_part_thread(const std::string_view aPart) -> std::optional<tour_thread>
	{
		if (aPart == "tour") {
			return tour_thread{ };
		}
		constexpr auto stop = std::string_view{ "stop-" };
		if (aPart.starts_with(stop) && all_digits(aPart.substr(stop.size())) && (aPart.size() == stop.size() + 1U || aPart[stop.size()] != '0')) {
			return tour_thread{ .Kind = tour_thread_kind::stop, .Stop = tour_stop_id{ number_of(aPart.substr(stop.size())) } };
		}
		return std::nullopt;
	}

	auto tour_part_session(queued_session aPullRequest, const tour_thread& aThread) -> queued_session
	{
		aPullRequest      = with_kind(std::move(aPullRequest), queued_session_kind::tour_part);
		aPullRequest.Part = tour_part_name(aThread);
		return aPullRequest;
	}

	auto local_draft_session(std::string aDraft, const tour_thread& aThread) -> queued_session
	{
		return queued_session{ .Kind = queued_session_kind::local_draft_discussion, .Part = tour_part_name(aThread), .Draft = std::move(aDraft) };
	}

	auto encode_queued_prompt(const prompt_offer& aOffer) -> std::string
	{
		auto text = std::string{ aOffer.Commits.empty() ? queued_prompt_version : queued_prompt_commits_version };
		put_number(text, wire_place(aOffer.Place));
		put_number(text, wire_subject(aOffer.Subject));
		put_number(text, static_cast<std::size_t>(aOffer.Kind));
		put_number(text, aOffer.Expansion.Cursor ? *aOffer.Expansion.Cursor + 1U : 0U);
		put_number(text, aOffer.Expansion.Values.size());
		for (const auto& value : aOffer.Expansion.Values) {
			put_number(text, value.Begin);
			put_number(text, value.End);
		}
		for (const auto* const field : { &aOffer.Id, &aOffer.Label, &aOffer.Expansion.Text }) {
			put_number(text, field->size());
			text += *field;
		}
		if (!aOffer.Commits.empty()) {
			put_number(text, aOffer.Commits.size());
			for (const auto& commit : aOffer.Commits) {
				put_number(text, commit.size());
				text += commit;
			}
		}
		return text;
	}

	auto decode_queued_prompt(std::string_view aText) -> std::optional<prompt_offer>
	{
		const auto withCommits = aText.starts_with(queued_prompt_commits_version);
		if (!withCommits && !aText.starts_with(queued_prompt_version)) {
			return std::nullopt;
		}
		aText.remove_prefix(queued_prompt_version.size());
		const auto place   = place_of_wire(take_number(aText));
		const auto subject = subject_of_wire(take_number(aText));
		const auto kind    = take_number(aText);
		const auto cursor  = take_number(aText);
		const auto values  = take_number(aText);
		if (!place || !subject || !kind
			|| *kind >= prompt_ask_count || !cursor || !values || *values > aText.size()) {
			return std::nullopt;
		}
		auto offer    = prompt_offer{ };
		offer.Place   = *place;
		offer.Subject = *subject;
		offer.Kind    = static_cast<prompt_ask>(*kind);
		for (auto index = std::size_t{ 0U }; index < *values; ++index) {
			const auto begin = take_number(aText);
			const auto end   = take_number(aText);
			if (!begin || !end || *end < *begin) {
				return std::nullopt;
			}
			offer.Expansion.Values.push_back({ .Begin = *begin, .End = *end });
		}
		auto id    = take_text(aText);
		auto label = take_text(aText);
		auto body  = take_text(aText);
		if (withCommits) {
			const auto count = take_number(aText);
			if (!count || *count == 0U || *count > aText.size()) {
				return std::nullopt;
			}
			for (auto index = std::size_t{ 0U }; index < *count; ++index) {
				auto commit = take_text(aText);
				if (!commit || commit->size() != 40U || !std::ranges::all_of(*commit, [](const char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); })) {
					return std::nullopt;
				}
				offer.Commits.push_back(std::move(*commit));
			}
		}
		if (!id || !label || !body || !aText.empty() || !utf8::valid(*body) || !utf8::valid(*label)) {
			return std::nullopt;
		}
		offer.Id             = std::move(*id);
		offer.Label          = std::move(*label);
		offer.Expansion.Text = std::move(*body);
		if (*cursor > 0U) {
			offer.Expansion.Cursor = *cursor - 1U;
		}
		const auto size = offer.Expansion.Text.size();
		if ((offer.Expansion.Cursor && *offer.Expansion.Cursor > size) || std::ranges::any_of(offer.Expansion.Values, [size](const prompt_value_span& aSpan) { return aSpan.End > size; })) {
			return std::nullopt;
		}
		return offer;
	}

	auto encode_queued_claim(const queued_claim& aClaim) -> std::string
	{
		auto text = std::string{ queued_claim_version };
		text += std::to_string(static_cast<unsigned>(aClaim.Route));
		text += ';';
		text += std::to_string(aClaim.Run);
		text += ';';
		text += std::to_string(aClaim.Conversation);
		text += ';';
		put_number(text, aClaim.Receipt.size());
		text += aClaim.Receipt;
		return text;
	}

	auto decode_queued_claim(std::string_view aText) -> std::optional<queued_claim>
	{
		if (!aText.starts_with(queued_claim_version)) {
			return std::nullopt;
		}
		aText.remove_prefix(queued_claim_version.size());
		const auto route        = take_number(aText);
		const auto run          = take_wide(aText);
		const auto conversation = take_wide(aText);
		auto       receipt      = take_text(aText);
		if (!route || *route > static_cast<std::size_t>(queued_route::steer) || !run || !conversation || !receipt || !aText.empty() || receipt->size() > maximum_claim_receipt_bytes || !utf8::valid(*receipt)) {
			return std::nullopt;
		}
		return queued_claim{ .Route = static_cast<queued_route>(*route), .Run = *run, .Conversation = *conversation, .Receipt = std::move(*receipt) };
	}
} // namespace bpr::core
