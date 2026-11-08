#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_CHAT_PROMPTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_CHAT_PROMPTS_HPP

#include "bpr/core/tour_agent_session.hpp"

#include <span>
#include <string_view>

/// \file
/// \brief [[BPR-1934]]. The suggested prompts an empty tour chat offers.
///
/// \details The owner, 2026-11-07: "Tour side chat should get suggestions like
/// the agent tab has." Each chat kind has its own short list, kept here in one
/// place rather than in the chat's widget, so Agent settings can later make
/// them editable. The chat presents them with the Agent tab's suggestion
/// component ([[BPR-1877]]).
namespace bpr::application {
	/// \brief One suggested prompt. `Text` is sent as written. A prompt the
	/// reviewer finishes (`Finish`, "Focus on …") is put in the composer
	/// instead, with `Text` there to be completed.
	struct tour_chat_prompt
	{
		std::string_view Text;
		bool             Finish = false;
	};

	/// \brief The prompts for a chat about `aKind`: a stop, the Problem or
	/// the Solution, or the whole tour. In the order they are offered.
	[[nodiscard]] auto tour_chat_prompts(core::tour_thread_kind aKind) noexcept -> std::span<const tour_chat_prompt>;
}

#endif
