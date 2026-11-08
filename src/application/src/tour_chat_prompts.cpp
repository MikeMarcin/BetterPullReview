#include "bpr/application/tour_chat_prompts.hpp"

#include <array>

namespace bpr::application {
	namespace {
		constexpr auto stop_prompts = std::array{
			tour_chat_prompt{ .Text = "What does this code do?" },
			tour_chat_prompt{ .Text = "Why does this change matter?" },
			tour_chat_prompt{ .Text = "Rewrite this explanation more simply" },
			tour_chat_prompt{ .Text = "Is this the right place for this stop?" },
		};
		constexpr auto part_prompts = std::array{
			tour_chat_prompt{ .Text = "Make this clearer" },
			tour_chat_prompt{ .Text = "What's missing?" },
			tour_chat_prompt{ .Text = "Make it shorter" },
		};
		constexpr auto tour_prompts = std::array{
			tour_chat_prompt{ .Text = "Too many stops?" },
			tour_chat_prompt{ .Text = "Suggest a better order" },
			tour_chat_prompt{ .Text = "Focus on ", .Finish = true },
			tour_chat_prompt{ .Text = "What's not covered?" },
		};
	} // namespace

	auto tour_chat_prompts(const core::tour_thread_kind aKind) noexcept -> std::span<const tour_chat_prompt>
	{
		switch (aKind) {
		case core::tour_thread_kind::stop:     return stop_prompts;
		case core::tour_thread_kind::problem:
		case core::tour_thread_kind::solution: return part_prompts;
		case core::tour_thread_kind::tour:     return tour_prompts;
		}
		return { };
	}
}
