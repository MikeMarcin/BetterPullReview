#include "bpr/core/agent_run_policy.hpp"

namespace bpr::core {
	auto stable_name(const agent_session_mode aMode) noexcept -> std::string_view
	{
		switch (aMode) {
		case agent_session_mode::fresh:   return "fresh";
		case agent_session_mode::resumed: return "resumed";
		}
		return { };
	}

	auto stable_name(const agent_run_path aPath) noexcept -> std::string_view
	{
		switch (aPath) {
		case agent_run_path::enhance_writing:               return "enhance-writing";
		case agent_run_path::tour_enhance:                  return "tour-enhance";
		case agent_run_path::tour_field_draft:              return "tour-field-draft";
		case agent_run_path::review_feedback_hook:          return "review-feedback-hook";
		case agent_run_path::draft_tour_first_draft:        return "draft-tour-first-draft";
		case agent_run_path::local_description_first_draft: return "local-description-first-draft";
		case agent_run_path::local_commit_message:          return "local-commit-message";
		case agent_run_path::agent_check:                   return "agent-check";
		case agent_run_path::assistance_first_answer:       return "assistance-first-answer";
		case agent_run_path::agent_tab:                     return "agent-tab";
		case agent_run_path::tour_side_chat:                return "tour-side-chat";
		case agent_run_path::tour_conversation:             return "tour-conversation";
		case agent_run_path::local_draft_discussion:        return "local-draft-discussion";
		case agent_run_path::review_cycle_round:            return "review-cycle-round";
		case agent_run_path::self_review_loop:              return "self-review-loop";
		case agent_run_path::address_feedback_run:          return "address-feedback-run";
		}
		return { };
	}

	auto stable_name(const agent_mcp_servers aServers) noexcept -> std::string_view
	{
		switch (aServers) {
		case agent_mcp_servers::off: return "off";
		case agent_mcp_servers::own: return "own";
		}
		return { };
	}

	auto parse_agent_mcp_servers(const std::string_view aName) noexcept -> std::optional<agent_mcp_servers>
	{
		if (aName == "off") {
			return agent_mcp_servers::off;
		}
		if (aName == "own") {
			return agent_mcp_servers::own;
		}
		return std::nullopt;
	}

	auto stable_name(const agent_instruction_delivery aDelivery) noexcept -> std::string_view
	{
		switch (aDelivery) {
		case agent_instruction_delivery::system_prompt: return "system-prompt";
		case agent_instruction_delivery::message:       return "message";
		}
		return { };
	}

	auto parse_agent_instruction_delivery(const std::string_view aName) noexcept -> std::optional<agent_instruction_delivery>
	{
		if (aName == "system-prompt") {
			return agent_instruction_delivery::system_prompt;
		}
		if (aName == "message") {
			return agent_instruction_delivery::message;
		}
		return std::nullopt;
	}
} // namespace bpr::core
