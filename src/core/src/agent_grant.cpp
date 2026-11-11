#include "bpr/core/agent_grant.hpp"

#include <string>

namespace bpr::core {
	auto resolve_agent_grant(const agent_grant_facts& aFacts) noexcept -> agent_grant
	{
		auto grant = agent_grant{ };
		if (!aFacts.Program) {
			return grant;
		}
		grant.Applies = true;
		grant.Scratch = aFacts.ScratchChoice.value_or(aFacts.ScratchStarts);
		if (!aFacts.Builds) {
			grant.Builds = agent_builds_state::off;
		} else if (aFacts.OwnCheckout) {
			grant.Builds = agent_builds_state::own_checkout;
		} else if (!aFacts.BuildAnswer) {
			grant.Builds = agent_builds_state::unasked;
		} else {
			grant.Builds = *aFacts.BuildAnswer ? agent_builds_state::allowed : agent_builds_state::declined;
		}
		return grant;
	}

	auto grant_level(const agent_grant& aGrant) noexcept -> agent_grant_level
	{
		if (builds_granted(aGrant)) {
			return agent_grant_level::builds_and_tests;
		}
		return scratch_granted(aGrant) ? agent_grant_level::scratch_folder : agent_grant_level::reads_only;
	}

	auto stable_name(const agent_grant_level aLevel) noexcept -> std::string_view
	{
		switch (aLevel) {
		case agent_grant_level::reads_only:       return "reads_only";
		case agent_grant_level::scratch_folder:   return "scratch_folder";
		case agent_grant_level::builds_and_tests: return "builds_and_tests";
		}
		return "unknown";
	}

	auto stable_name(const agent_builds_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case agent_builds_state::off:          return "off";
		case agent_builds_state::own_checkout: return "own_checkout";
		case agent_builds_state::unasked:      return "unasked";
		case agent_builds_state::declined:     return "declined";
		case agent_builds_state::allowed:      return "allowed";
		}
		return "unknown";
	}

	auto agent_grant_line(const agent_grant_level aLevel) -> std::string_view
	{
		switch (aLevel) {
		case agent_grant_level::reads_only:       return "Reads only";
		case agent_grant_level::scratch_folder:   return "Writes only in its scratch folder";
		case agent_grant_level::builds_and_tests: return "Builds and tests in its disposable checkout";
		}
		return "Reads only";
	}

	auto agent_builds_line(const agent_builds_state aState, const std::string_view aAgent) -> std::string
	{
		switch (aState) {
		case agent_builds_state::off:          return "Builds and tests are off for " + std::string{ aAgent };
		case agent_builds_state::own_checkout: return "Builds and tests are unavailable in your own checkout";
		case agent_builds_state::unasked:      return "Builds and tests: asks at your next question";
		case agent_builds_state::declined:     return "Builds and tests declined for this pull request";
		case agent_builds_state::allowed:      return "Builds and tests in its disposable checkout";
		}
		return { };
	}

	auto agent_grant_enforcement(const agent_grant& aGrant, const std::optional<agent_program> aProgram) -> std::string
	{
		if (!aProgram || !aGrant.Applies || grant_level(aGrant) == agent_grant_level::reads_only) {
			return "BetterPullReview gives this agent no write or run allowance.";
		}
		if (*aProgram == agent_program::codex) {
			return "Codex's sandbox enforces this, network off. Its scripts run with your account's rights inside it.";
		}
		return "A permission rule Claude Code follows, not enforced. Its scripts run with your account's rights.";
	}

	auto agent_scratch_instruction(const std::string_view aPath, const bool aOverLimit) -> std::string
	{
		auto text = std::string{ "Your scratch folder for this session is " } + std::string{ aPath }
			+ ". You may create, change and run files there, and nowhere else; the review's workspace is for reading. It holds at most 256 MiB.";
		if (aOverLimit) {
			text += " It is full: this turn you may not write there. Ask the reviewer to make room in it.";
		}
		return text;
	}

	auto agent_builds_instruction(const std::string_view aWorkspace) -> std::string
	{
		return "The reviewer allowed building and running tests in this pull request's disposable checkout, " + std::string{ aWorkspace }
		+ ". You may run its build and test commands there and write build output inside it. Do not change its tracked files.";
	}
} // namespace bpr::core
