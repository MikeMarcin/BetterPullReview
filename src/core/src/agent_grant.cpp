#include "bpr/core/agent_grant.hpp"

#include <algorithm>
#include <string>

namespace bpr::core {
	auto resolve_agent_grant(const agent_grant_facts& aFacts) noexcept -> agent_grant
	{
		auto grant = agent_grant{ };
		if (!aFacts.Program) {
			return grant;
		}
		grant.Applies       = true;
		grant.Scratch       = aFacts.ScratchChoice.value_or(aFacts.ScratchStarts);
		grant.ScratchPlace  = aFacts.ScratchPlace;
		grant.ScratchFull   = aFacts.ScratchFull;
		grant.Refusal       = aFacts.Refusal;
		grant.ScratchUnmade = aFacts.ScratchUnmade;
		// Re-review N1a (2026-11-11). In the reviewer's own checkout Codex
		// also reads that checkout's `.codex/config.toml` once they trusted
		// it, which BPR does not read; BPR's profile would replace it.
		if (!grant.Refusal && aFacts.OwnCheckout && *aFacts.Program == agent_program::codex) {
			grant.Refusal = agent_grant_refusal::codex_own_checkout;
		}
		// Owner look e97c6a3d: the pull request's toggle (or its answer to
		// the question) rules; the agent's setting decides only whether an
		// unanswered pull request asks. Never in the reviewer's own checkout.
		if (aFacts.OwnCheckout && (aFacts.Builds || aFacts.BuildAnswer)) {
			grant.Builds = agent_builds_state::own_checkout;
		} else if (aFacts.BuildAnswer) {
			grant.Builds = *aFacts.BuildAnswer ? agent_builds_state::allowed : agent_builds_state::declined;
		} else if (aFacts.Builds) {
			grant.Builds = agent_builds_state::unasked;
		} else {
			grant.Builds = agent_builds_state::off;
		}
		return grant;
	}

	auto grant_level(const agent_grant& aGrant) noexcept -> agent_grant_level
	{
		if (builds_granted(aGrant)) {
			return agent_grant_level::builds_and_tests;
		}
		return scratch_writable(aGrant) ? agent_grant_level::scratch_folder : agent_grant_level::reads_only;
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

	auto stable_name(const agent_grant_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case agent_grant_refusal::codex_own_permissions:   return "codex_own_permissions";
		case agent_grant_refusal::codex_asks_approval:     return "codex_asks_approval";
		case agent_grant_refusal::codex_config_unreadable: return "codex_config_unreadable";
		case agent_grant_refusal::custom_arguments:        return "custom_arguments";
		case agent_grant_refusal::codex_own_checkout:      return "codex_own_checkout";
		}
		return "unknown";
	}

	auto agent_grant_refusal_reason(const agent_grant_refusal aRefusal) -> std::string_view
	{
		switch (aRefusal) {
		case agent_grant_refusal::codex_own_permissions:   return "Unavailable: your Codex configuration has its own permissions profile, and BetterPullReview's would replace it. Codex keeps yours.";
		case agent_grant_refusal::codex_asks_approval:     return "Unavailable: your Codex asks before it runs commands, and a run here cannot answer. BetterPullReview does not turn that off.";
		case agent_grant_refusal::codex_config_unreadable: return "Unavailable: your Codex configuration could not be read, or has a setting BetterPullReview cannot judge, so it passes nothing that might widen it.";
		case agent_grant_refusal::custom_arguments:        return "Unavailable: this agent's own arguments set its sandbox, approvals or profile. BetterPullReview passes nothing on top of them.";
		case agent_grant_refusal::codex_own_checkout:      return "Unavailable in your own checkout: Codex reads that checkout's own settings there, and BetterPullReview's would replace them. Codex keeps yours.";
		}
		return { };
	}

	auto agent_scratch_toggle_text(const agent_grant& aGrant) -> std::string
	{
		return scratch_granted(aGrant) && aGrant.ScratchFull ? "Use scratch folder (full)" : "Use scratch folder";
	}

	namespace {
		[[nodiscard]] auto scratch_enforcement(const agent_program aProgram) -> std::string_view
		{
			if (aProgram == agent_program::codex) {
				return "Codex's sandbox holds its writes to the folder, with the network off; a script it runs has your account's rights inside it.";
			}
			return "This is a permission rule Claude Code follows, not enforced by the system: a script it runs has your account's rights.";
		}
	} // namespace

	auto agent_scratch_tooltip(const agent_grant& aGrant, const std::optional<agent_program> aProgram, const std::uint32_t aLimitMiB) -> std::string
	{
		const auto limit = std::to_string(agent_scratch_limit_mib(aLimitMiB)) + " MiB";
		if (!aProgram || !aGrant.Applies) {
			return "This agent takes no allowance from BetterPullReview.";
		}
		if (aGrant.Refusal) {
			return std::string{ agent_grant_refusal_reason(*aGrant.Refusal) };
		}
		if (!aGrant.ScratchPlace) {
			return "No scratch folder can be made on this computer: its path must be a plain absolute path that a permission rule can name. What the agent may do is what its own settings allow.";
		}
		if (!aGrant.Scratch) {
			return "Off in this pull request. BetterPullReview gives the agent no write or run allowance; what it may do is what its own settings allow.";
		}
		// Re-review N3: the last question asked for a folder and got none.
		if (aGrant.ScratchUnmade) {
			return "The scratch folder could not be made for the last question, so that question had none. The next question tries again.";
		}
		// Re-review N6: a full folder is not watched, so Claude, whose
		// scripts are not confined, is given no run rule there either.
		if (aGrant.ScratchFull) {
			const auto may = *aProgram == agent_program::codex ? std::string_view{ "may run what is there but not write there" } : std::string_view{ "may read what is there but not write or run anything there" };
			return "The scratch folder is full: it holds its limit of " + limit + ". The agent " + std::string{ may }
			+ " until you make room. Open it, or clear scratch folders in Settings, Agents & context.";
		}
		return "The agent may create, change and run scripts in a folder of its own, outside the review's workspace, up to " + limit + ". "
			+ std::string{ scratch_enforcement(*aProgram) } + " The setting carries to the next question.";
	}

	auto agent_builds_tooltip(const agent_grant& aGrant, const std::optional<agent_program> aProgram) -> std::string
	{
		if (!aProgram || !aGrant.Applies) {
			return "This agent takes no allowance from BetterPullReview.";
		}
		if (aGrant.Refusal) {
			return std::string{ agent_grant_refusal_reason(*aGrant.Refusal) };
		}
		const auto consequence = std::string{ "Building runs this pull request's code on this computer with your account's rights." };
		const auto confinement = *aProgram == agent_program::codex
			? std::string{ " Codex's sandbox holds the build's writes to the disposable checkout and the temporary folder, with the network off." }
			: std::string{ " Claude Code may run the build and test programs (cmake, make, npm, cargo and others) with any arguments, in any folder, including commands that install or publish: a permission rule it follows, not enforced by the system." };
		switch (aGrant.Builds) {
		case agent_builds_state::own_checkout: return "Unavailable in your own checkout: " + std::string{ agent_builds_own_checkout_reason };
		case agent_builds_state::allowed:      return "The agent may build and run tests in this pull request's disposable checkout. " + consequence + confinement;
		case agent_builds_state::unasked:      return "Asks before the first question that would build in this pull request. " + consequence;
		case agent_builds_state::declined:
		case agent_builds_state::off:          return "Off in this pull request. Turn on to let the agent build and run tests in its disposable checkout. " + consequence + confinement;
		}
		return { };
	}

	namespace {
		[[nodiscard]] auto trimmed(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t' || aText.front() == '\r')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t' || aText.back() == '\r')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}
		/// A line up to its comment: a `#` outside a quoted string.
		[[nodiscard]] auto uncommented(const std::string_view aLine) -> std::string_view
		{
			auto quote = '\0';
			for (auto index = std::size_t{ 0U }; index < aLine.size(); ++index) {
				const auto character = aLine[index];
				if (quote != '\0') {
					if (character == '\\' && quote == '"') {
						++index;
					} else if (character == quote) {
						quote = '\0';
					}
				} else if (character == '"' || character == '\'') {
					quote = character;
				} else if (character == '#') {
					return aLine.substr(0U, index);
				}
			}
			return aLine;
		}
		/// The key without quotes or whitespace around its dots. Fourth
		/// review (2026-11-11): Codex trims every whitespace character, a
		/// newline, return, vertical tab or form feed too.
		[[nodiscard]] auto plain_key(const std::string_view aKey) -> std::string
		{
			auto key = std::string{ };
			for (const auto character : aKey) {
				if (character != '"' && character != '\'' && character != ' ' && character != '\t' && character != '\n' && character != '\r' && character != '\v' && character != '\f') {
					key += character;
				}
			}
			return key;
		}
		[[nodiscard]] auto names_permissions(const std::string_view aKey) -> bool
		{
			return aKey == "permissions" || aKey.starts_with("permissions.") || aKey.find(".permissions") != std::string_view::npos || aKey == "default_permissions"
				|| aKey.ends_with(".default_permissions");
		}
		[[nodiscard]] auto names_approval(const std::string_view aKey) -> bool
		{
			return aKey == "approval_policy" || aKey.ends_with(".approval_policy") || aKey.starts_with("approval_policy.") || aKey.find(".approval_policy.") != std::string_view::npos;
		}
		/// Whether one `key = value` setting (from a file or `-c`) refuses.
		[[nodiscard]] auto setting_refusal(const std::string& aKey, const std::string_view aValue) -> std::optional<agent_grant_refusal>
		{
			if (names_permissions(aKey)) {
				return agent_grant_refusal::codex_own_permissions;
			}
			if (names_approval(aKey)) {
				const auto value = trimmed(aValue);
				const auto known = value == "\"on-request\"" || value == "\"on-failure\"" || value == "\"never\"" || value == "'on-request'" || value == "'on-failure'" || value == "'never'"
					|| value == "on-request" || value == "on-failure" || value == "never";
				if (!known || (aKey != "approval_policy" && !aKey.ends_with(".approval_policy"))) {
					return agent_grant_refusal::codex_asks_approval;
				}
			}
			return std::nullopt;
		}
		/// Codex flags that take a value and leave permissions alone.
		[[nodiscard]] auto codex_value_flag(const std::string_view aName) -> bool
		{
			return aName == "--model" || aName == "--image" || aName == "--output-last-message" || aName == "--output-schema" || aName == "--color" || aName == "--local-provider";
		}
		/// Codex switches that leave permissions alone.
		[[nodiscard]] auto codex_switch(const std::string_view aName) -> bool
		{
			return aName == "--json" || aName == "--skip-git-repo-check" || aName == "--ephemeral" || aName == "--oss" || aName == "--strict-config" || aName == "--help" || aName == "--version";
		}
		/// Whether a `-c key=value` the agent passes itself refuses.
		/// Third round F2: a setting with no key, or none BPR can read,
		/// refuses too.
		[[nodiscard]] auto codex_setting_refused(const std::string_view aSetting) -> bool
		{
			const auto equals = aSetting.find('=');
			const auto key    = plain_key(aSetting.substr(0U, equals));
			return equals == std::string_view::npos || key.empty() || key.front() == '-' || names_permissions(key) || names_approval(key) || key.starts_with("sandbox")
				|| key.starts_with("profile") || key.starts_with("projects");
		}
	} // namespace

	auto codex_grant_refusal(const std::span<const std::string_view> aConfigs, const bool aUnreadable, const std::span<const std::string> aArguments) -> std::optional<agent_grant_refusal>
	{
		// The agent's own arguments first: a profile, sandbox, approval,
		// working folder or anything else there that BPR cannot judge is the
		// reviewer's, and BPR's would override or escape it. Re-review N1c
		// (2026-11-11): an allowlist, read as Codex's parser reads it, so an
		// attached value (`-pwork`) is the flag it is attached to.
		for (auto index = std::size_t{ 0U }; index < aArguments.size(); ++index) {
			const auto argument = std::string_view{ aArguments[index] };
			if (argument == "--") {
				break;
			}
			if (argument == "-" || !argument.starts_with('-')) {
				continue;
			}
			auto setting = std::optional<std::string_view>{ };
			if (argument.starts_with("--")) {
				const auto equals = argument.find('=');
				const auto name   = argument.substr(0U, equals);
				const auto valued = equals != std::string_view::npos;
				if (name == "--config") {
					setting = valued ? argument.substr(equals + 1U) : index + 1U < aArguments.size() ? std::string_view{ aArguments[++index] }
																									 : std::string_view{ };
				} else if (codex_value_flag(name)) {
					index += valued ? 0U : 1U;
					continue;
				} else if (!valued && codex_switch(name)) {
					continue;
				} else {
					return agent_grant_refusal::custom_arguments;
				}
			} else {
				// A cluster of short flags: the first that takes a value
				// takes the rest of the argument, or the next one.
				for (auto at = std::size_t{ 1U }; at < argument.size() && !setting; ++at) {
					const auto flag = argument[at];
					if (flag == 'h' || flag == 'V') {
						continue;
					}
					if (flag != 'c' && flag != 'm' && flag != 'i' && flag != 'o') {
						return agent_grant_refusal::custom_arguments;
					}
					// Third round F2: Codex's parser takes `-c=KEY=VALUE`
					// as `KEY=VALUE`, one `=` dropped; only a flag with
					// nothing attached takes the next argument.
					auto value = argument.substr(at + 1U);
					if (value.empty()) {
						value = index + 1U < aArguments.size() ? std::string_view{ aArguments[++index] } : std::string_view{ };
					} else if (value.front() == '=') {
						value.remove_prefix(1U);
					}
					if (flag != 'c') {
						break;
					}
					setting = value;
				}
			}
			if (setting && codex_setting_refused(*setting)) {
				return agent_grant_refusal::custom_arguments;
			}
		}
		if (aUnreadable) {
			return agent_grant_refusal::codex_config_unreadable;
		}
		auto refusal = std::optional<agent_grant_refusal>{ };
		for (const auto config : aConfigs) {
			auto table = std::string{ };
			auto rest  = config;
			// Re-review: a byte-order mark is not part of the first key.
			if (rest.starts_with("\xEF\xBB\xBF")) {
				rest.remove_prefix(3U);
			}
			while (!rest.empty()) {
				const auto end  = rest.find('\n');
				const auto line = trimmed(uncommented(rest.substr(0U, end)));
				rest            = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
				if (line.empty()) {
					continue;
				}
				if (line.front() == '[') {
					table = plain_key(line);
					std::erase(table, '[');
					std::erase(table, ']');
					if (names_permissions(table)) {
						return agent_grant_refusal::codex_own_permissions;
					}
					continue;
				}
				const auto equals = line.find('=');
				if (equals == std::string_view::npos) {
					continue;
				}
				auto key = table;
				if (!key.empty()) {
					key += '.';
				}
				key += plain_key(line.substr(0U, equals));
				const auto value = trimmed(line.substr(equals + 1U));
				const auto why   = setting_refusal(key, value);
				// Re-review: an inline table holds settings this line scan
				// cannot place. Under `profiles` (any profile may be the one
				// chosen), or naming a permission or an approval, it refuses.
				if (!why && value.starts_with('{')
					&& (key == "profiles" || key.starts_with("profiles.") || value.find("permissions") != std::string_view::npos || value.find("approval_policy") != std::string_view::npos)) {
					return agent_grant_refusal::codex_config_unreadable;
				}
				if (why == agent_grant_refusal::codex_own_permissions) {
					return why;
				}
				if (why && !refusal) {
					refusal = why;
				}
			}
		}
		return refusal;
	}

	auto codex_configuration_paths(const std::string_view aHome) -> std::optional<std::vector<std::string>>
	{
		if (aHome.empty() || aHome.front() != '/') {
			return std::nullopt;
		}
		auto home = std::string{ aHome };
		while (home.size() > 1U && home.back() == '/') {
			home.pop_back();
		}
		return std::vector<std::string>{ home + "/.codex/config.toml", "/etc/codex/config.toml", "/etc/codex/managed_config.toml" };
	}

	auto agent_scratch_instruction(const std::string_view aPath, const bool aOverLimit, const std::uint32_t aLimitMiB) -> std::string
	{
		auto text = std::string{ "Your scratch folder for this session is " } + std::string{ aPath }
			+ ". You may create, change and run files there, and nowhere else; the review's workspace is for reading. Run a file there with python3, python, node, ruby or perl and its path in the folder. It holds at most "
			+ std::to_string(agent_scratch_limit_mib(aLimitMiB)) + " MiB; a turn that passes that is stopped.";
		if (aOverLimit) {
			text += " It is full: this turn you may not write there or run files there; you may only read it. Ask the reviewer to make room in it.";
		}
		return text;
	}

	auto agent_builds_instruction(const std::string_view aWorkspace) -> std::string
	{
		return "The reviewer allowed building and running tests in this pull request's disposable checkout, " + std::string{ aWorkspace }
		+ ". You may run its build and test commands there and write build output inside it. Do not change its tracked files.";
	}
} // namespace bpr::core
