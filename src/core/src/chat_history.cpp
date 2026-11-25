#include "bpr/core/chat_history.hpp"

#include <array>

namespace bpr::core {
	auto decide_new_session(const chat_history_facts& aFacts) noexcept -> new_session_refusal
	{
		if (!aFacts.AgentReady) {
			return new_session_refusal::no_agent;
		}
		if (aFacts.Closed) {
			return new_session_refusal::closed;
		}
		if (aFacts.Loading) {
			return new_session_refusal::loading;
		}
		if (aFacts.Running) {
			return new_session_refusal::running;
		}
		if (aFacts.Busy) {
			return new_session_refusal::busy;
		}
		const auto untouched = aFacts.SavedTurns == 0U && aFacts.ShownTurns == 0U && !aFacts.Uncertain;
		const auto usable    = aFacts.NativeSupported || aFacts.CarriedOver;
		return untouched && usable && !aFacts.Pinned ? new_session_refusal::already_new : new_session_refusal::none;
	}

	auto plan_chat_history(const chat_history_facts& aFacts) -> chat_history_plan
	{
		using item      = chat_history_item;
		auto plan       = chat_history_plan{ };
		plan.NewSession = decide_new_session(aFacts);
		plan.CarryOver  = aFacts.AgentReady && !aFacts.Closed && !aFacts.Loading && !aFacts.Running && !aFacts.Busy;
		plan.Copy       = aFacts.ShownTurns != 0U;
		// Groups, a separator between each two that hold something.
		auto others = std::vector<item>{ };
		if (aFacts.Kind == chat_history_kind::tour_side_chat) {
			if (aFacts.OtherChats != 0U) {
				others.push_back(item::other_chats);
			}
			// The Agent tab copies each reply; it has no copy of the whole thread.
			others.push_back(item::copy_conversation);
		} else if (aFacts.EveryRun) {
			others.push_back(item::show_every_run);
		}
		const auto groups = std::array{
			std::vector{ item::new_session },
			std::vector{ item::sessions },
			std::vector{ item::carry_over, item::delete_all },
			others,
			aFacts.Kind == chat_history_kind::agent_tab && aFacts.ResetAgents ? std::vector{ item::reset_agents } : std::vector<item>{ },
			aFacts.RevisionKnown ? std::vector{ item::revision_read } : std::vector<item>{ },
		};
		for (const auto& group : groups) {
			if (group.empty()) {
				continue;
			}
			if (!plan.Items.empty()) {
				plan.Items.push_back(item::separator);
			}
			plan.Items.insert(plan.Items.end(), group.begin(), group.end());
		}
		return plan;
	}
}
