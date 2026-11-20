#include "bpr/core/agent_run_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <vector>

namespace bpr::core {
	namespace {
		/// An argument Claude reads as an option rather than as a value.
		[[nodiscard]] auto is_option(const std::string_view aArgument) noexcept -> bool { return aArgument.size() > 1U && aArgument.front() == '-'; }

		/// `aName` itself, or its `aName=value` spelling.
		[[nodiscard]] auto names(const std::string_view aArgument, const std::string_view aName) noexcept -> bool
		{
			return aArgument.starts_with(aName) && (aArgument.size() == aName.size() || aArgument[aName.size()] == '=');
		}

		[[nodiscard]] auto is_variadic(const std::string_view aArgument) noexcept -> bool
		{
			return std::ranges::any_of(claude_variadic_options, [aArgument](const std::string_view aName) { return names(aArgument, aName); });
		}

		/// Every value given to `aName`: an `=` spelling's own, then each
		/// argument up to the next option. A single-value option's later
		/// arguments are the prompt's, which this app never passes.
		[[nodiscard]] auto values_of(const std::span<const std::string> aArguments, const std::string_view aName) -> std::vector<std::string_view>
		{
			auto values = std::vector<std::string_view>{ };
			auto taking = false;
			for (const std::string_view argument : aArguments) {
				if (is_option(argument)) {
					taking = names(argument, aName);
					if (taking && argument.size() > aName.size()) {
						values.push_back(argument.substr(aName.size() + 1U));
					}
				} else if (taking) {
					values.push_back(argument);
				}
			}
			return values;
		}

		/// Permission rules are separated by commas or spaces outside a
		/// rule's parentheses (`Bash(git log:*)` is one rule).
		void split_rules(const std::string_view aValue, std::vector<std::string_view>& aRules)
		{
			auto depth = 0;
			auto start = std::size_t{ 0U };
			for (auto at = std::size_t{ 0U }; at < aValue.size(); ++at) {
				const auto character = aValue[at];
				if (character == '(') {
					++depth;
				} else if (character == ')' && depth > 0) {
					--depth;
				} else if (depth == 0 && (character == ',' || character == ' ')) {
					if (at > start) {
						aRules.push_back(aValue.substr(start, at - start));
					}
					start = at + 1U;
				}
			}
			if (start < aValue.size()) {
				aRules.push_back(aValue.substr(start));
			}
		}

		[[nodiscard]] auto allowed_rules(const std::span<const std::string> aArguments) -> std::vector<std::string_view>
		{
			auto rules = std::vector<std::string_view>{ };
			for (const auto name : { std::string_view{ "--allowedTools" }, std::string_view{ "--allowed-tools" } }) {
				for (const auto value : values_of(aArguments, name)) {
					split_rules(value, rules);
				}
			}
			return rules;
		}

		/// Every `Write(PATH)` rule has its `Edit(PATH)`.
		[[nodiscard]] auto writes_have_edits(const std::span<const std::string> aArguments) -> bool
		{
			const auto rules = allowed_rules(aArguments);
			return std::ranges::all_of(rules, [&rules](const std::string_view aRule) {
				return !aRule.starts_with("Write(") || std::ranges::find(rules, "Edit(" + std::string{ aRule.substr(std::string_view{ "Write(" }.size()) }) != rules.end();
			});
		}

		/// Each source a `--setting-sources` names, comma-separated.
		[[nodiscard]] auto only_user_sources(const std::string_view aValue) -> bool
		{
			auto rules = std::vector<std::string_view>{ };
			split_rules(aValue, rules);
			return std::ranges::all_of(rules, [](const std::string_view aSource) { return aSource == "user"; });
		}

		[[nodiscard]] auto mcp_off(const std::span<const std::string> aArguments) -> bool
		{
			const auto configurations = values_of(aArguments, "--mcp-config");
			return std::ranges::find(aArguments, "--strict-mcp-config") != aArguments.end() && !configurations.empty()
				&& std::ranges::all_of(configurations, [](const std::string_view aValue) { return aValue == claude_empty_mcp_configuration; });
		}
	} // namespace

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
		case agent_run_path::follow_up:                     return "follow-up";
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

	auto stable_name(const claude_argument_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case claude_argument_problem::no_print_mode:           return "no-print-mode";
		case claude_argument_problem::instructions_file:       return "instructions-file";
		case claude_argument_problem::open_variadic:           return "open-variadic";
		case claude_argument_problem::write_rule_without_edit: return "write-rule-without-edit";
		case claude_argument_problem::project_settings:        return "project-settings";
		case claude_argument_problem::settings_not_user_only:  return "settings-not-user-only";
		case claude_argument_problem::mcp_not_off:             return "mcp-not-off";
		}
		return { };
	}

	auto inspect_claude_arguments(const std::span<const std::string> aArguments, const claude_argument_expectation aExpected) -> std::optional<claude_argument_problem>
	{
		const auto print = std::ranges::find_if(aArguments, [](const std::string_view aArgument) { return aArgument == "-p" || aArgument == "--print"; });
		if (print == aArguments.end()) {
			return claude_argument_problem::no_print_mode;
		}
		if (std::ranges::any_of(aArguments, [](const std::string_view aArgument) { return names(aArgument, "--append-system-prompt-file") || names(aArgument, "--system-prompt-file"); })) {
			return claude_argument_problem::instructions_file;
		}
		// RFC-BPR-0104 AC11 (amended 2026-11-19). Wherever it stands, an
		// option that takes several values is followed by another option, so
		// the last option given never takes several: one left open would take
		// a prompt, or whatever a wrapper adds after these arguments.
		const auto backwards = aArguments | std::views::reverse;
		const auto last      = std::ranges::find_if(backwards, [](const std::string_view aArgument) { return is_option(aArgument); });
		if (last != backwards.end() && is_variadic(*last)) {
			return claude_argument_problem::open_variadic;
		}
		if (!writes_have_edits(aArguments)) {
			return claude_argument_problem::write_rule_without_edit;
		}
		const auto sources = values_of(aArguments, "--setting-sources");
		if (!std::ranges::all_of(sources, only_user_sources)) {
			return claude_argument_problem::project_settings;
		}
		if (aExpected.UserSettingsOnly && std::ranges::find(sources, "user") == sources.end()) {
			return claude_argument_problem::settings_not_user_only;
		}
		if (aExpected.McpOff && !mcp_off(aArguments)) {
			return claude_argument_problem::mcp_not_off;
		}
		return std::nullopt;
	}
} // namespace bpr::core
