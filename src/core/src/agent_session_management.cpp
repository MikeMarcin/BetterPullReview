#include "bpr/core/agent_session_management.hpp"

#include "bpr/core/utf8.hpp"

namespace bpr::core {
	namespace {
		/// ASCII white space and control characters; a name is one line.
		[[nodiscard]] constexpr auto separator(const char aCharacter) noexcept -> bool
		{
			const auto value = static_cast<unsigned char>(aCharacter);
			return value <= 0x20U || value == 0x7FU;
		}
	}

	auto tidy_agent_session_name(const std::string_view aTyped) -> std::expected<std::string, agent_session_name_refusal>
	{
		if (aTyped.find('\0') != std::string_view::npos || !utf8::valid(aTyped)) {
			return std::unexpected(agent_session_name_refusal::not_text);
		}
		auto tidied  = std::string{ };
		auto pending = false;
		for (const auto character : aTyped) {
			if (separator(character)) {
				pending = !tidied.empty();
				continue;
			}
			if (pending) {
				tidied.push_back(' ');
				pending = false;
			}
			tidied.push_back(character);
			if (tidied.size() > maximum_agent_session_name_bytes) {
				return std::unexpected(agent_session_name_refusal::too_long);
			}
		}
		return tidied;
	}

	auto agent_session_title(const std::string_view aName, const std::string_view aFirstQuestion) -> std::string
	{
		if (!aName.empty()) {
			return std::string{ aName };
		}
		return aFirstQuestion.empty() ? std::string{ "New session" } : std::string{ aFirstQuestion };
	}

	auto plan_agent_session_deletion(const agent_session_deletion_facts& aFacts) noexcept -> agent_session_deletion_plan
	{
		if (!aFacts.Found) {
			return { .Refusal = agent_session_deletion_refusal::not_found };
		}
		if (aFacts.Running) {
			return { .Refusal = agent_session_deletion_refusal::running };
		}
		return { .RemoveWorkspace = aFacts.HoldsWorkspace && aFacts.OtherHolders == 0U, .WorkspaceShared = aFacts.HoldsWorkspace && aFacts.OtherHolders != 0U };
	}
}
