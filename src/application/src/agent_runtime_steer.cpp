#include "bpr/application/agent_runtime_steer.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr auto goes_next = std::string_view{ " The message goes next, right after this answer." };
		constexpr auto quick_ask = std::string_view{ "A quick ask is written when it is sent, so it can't steer." };

		/// Gives an unsent correction back to its session's queue, first in
		/// line, under a new identity: it left the queue when it was handed
		/// off.
		void give_back(agent_queue_host& aQueue, const std::string& aSession, core::queued_input aInput)
		{
			if (aQueue.enqueue(aSession, std::move(aInput)) != core::queue_refusal::none) {
				return;
			}
			if (const auto* const queue = aQueue.queue(aSession); queue != nullptr && !queue->Entries.empty()) {
				static_cast<void>(aQueue.reorder(aSession, queue->Entries.back().Id, -static_cast<int>(core::maximum_queued_inputs)));
			}
		}
	} // namespace

	auto agent_steer_goes_next_words(const core::agent_steer_support aSupport) -> std::string
	{
		return std::string{ core::agent_steer_unavailable_words(aSupport) } + std::string{ goes_next };
	}

	auto agent_runtime_steer(agent_runtime_steer_binding aBinding)
		-> std::function<std::optional<std::string>(const std::string& aSession, const core::queued_entry& aEntry)>
	{
		return [binding = std::move(aBinding)](const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> {
			if (!aEntry.Input.QuickAsk.empty() || std::ranges::all_of(aEntry.Input.Text, [](const char aByte) { return aByte == ' ' || aByte == '\t' || aByte == '\r' || aByte == '\n'; })) {
				return std::string{ quick_ask } + std::string{ goes_next };
			}
			const auto shown = binding.Shown ? binding.Shown() : std::nullopt;
			if (!binding.Support || !binding.Steer || !shown) {
				return agent_steer_goes_next_words(core::agent_steer_support::not_running);
			}
			if (const auto support = binding.Support(*shown); support != core::agent_steer_support::available) {
				return agent_steer_goes_next_words(support);
			}
			binding.Steer(*shown, aEntry.Input.Text, [binding, aSession, input = aEntry.Input](const agent_steer_receipt& aReceipt) {
				if (aReceipt.Disposition == core::agent_steer_disposition::sent || binding.Queue == nullptr) {
					return;
				}
				give_back(*binding.Queue, aSession, input);
				if (binding.Returned) {
					const auto why = aReceipt.Disposition == core::agent_steer_disposition::refused ? std::string{ "The agent refused the correction for this turn." } + std::string{ goes_next }
																									: agent_steer_goes_next_words(aReceipt.Support);
					binding.Returned(aSession, why);
				}
			});
			return std::nullopt;
		};
	}
} // namespace bpr::application
