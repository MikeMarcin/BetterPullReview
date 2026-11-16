#include "bpr/core/agent_steering.hpp"

#include <array>
#include <charconv>
#include <cstddef>

namespace bpr::core {
	namespace {
		/// Dotted decimal components, at most four; nullopt for anything else.
		[[nodiscard]] auto version_components(const std::string_view aVersion) -> std::optional<std::array<std::uint32_t, 4>>
		{
			auto result = std::array<std::uint32_t, 4>{ };
			auto rest   = aVersion.substr(0U, aVersion.find_first_of(" -+("));
			for (std::size_t index = 0U; index < result.size() && !rest.empty(); ++index) {
				const auto dot   = rest.find('.');
				const auto part  = rest.substr(0U, dot);
				auto       value = std::uint32_t{ 0U };
				const auto read  = std::from_chars(part.data(), part.data() + part.size(), value);
				if (part.empty() || read.ec != std::errc{ } || read.ptr != part.data() + part.size()) {
					return std::nullopt;
				}
				result[index] = value;
				rest          = dot == std::string_view::npos ? std::string_view{ } : rest.substr(dot + 1U);
				if (dot != std::string_view::npos && rest.empty()) {
					return std::nullopt;
				}
			}
			if (!rest.empty()) {
				return std::nullopt;
			}
			return result;
		}

		void append_hex(std::string& aOut, const std::uint64_t aValue, const std::size_t aDigits)
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			for (auto shift = aDigits; shift > 0U; --shift) {
				aOut += digits[(aValue >> ((shift - 1U) * 4U)) & 0xFU];
			}
		}
	} // namespace

	auto agent_steer_support_of(const agent_protocol aProtocol, const bool aConnected, const std::string_view aToolVersion) noexcept -> agent_steer_support
	{
		if (aProtocol == agent_protocol::codex_jsonl_exec || aProtocol == agent_protocol::external_terminal) {
			return agent_steer_support::protocol;
		}
		if (!aConnected) {
			return agent_steer_support::no_connection;
		}
		if (aProtocol == agent_protocol::codex_app_server) {
			return agent_steer_support::available;
		}
		if (aToolVersion.empty()) {
			return agent_steer_support::version_unknown;
		}
		const auto observed  = version_components(aToolVersion);
		const auto qualified = version_components(qualified_claude_steer_version);
		if (!observed || !qualified || *observed < *qualified) {
			return agent_steer_support::version;
		}
		return agent_steer_support::available;
	}

	auto agent_steer_unavailable_words(const agent_steer_support aSupport) noexcept -> std::string_view
	{
		switch (aSupport) {
		case agent_steer_support::available:       return { };
		case agent_steer_support::not_running:     return "Steer needs a running turn; this one has ended.";
		case agent_steer_support::no_connection:   return "This turn can't be steered: it isn't running on a kept agent session.";
		case agent_steer_support::protocol:        return "This agent can't be steered: its connection has no way to add to a running turn.";
		case agent_steer_support::version:         return "This agent's version can't be steered: Claude Code 2.1.319 or newer is needed.";
		case agent_steer_support::exhausted:       return "This turn has taken as many corrections as one turn can; queue the rest.";
		case agent_steer_support::version_unknown: return "Steer waits until the agent says which version it is; it hasn't yet.";
		case agent_steer_support::other_run:       return "That turn has ended; this message was for it, so it wasn't sent into another.";
		case agent_steer_support::empty_text:      return "There's nothing to steer with: the message is empty.";
		case agent_steer_support::text_too_long:   return "The message is over the 16 KiB limit, so it wasn't sent.";
		}
		return "Steer isn't available for this agent.";
	}

	auto agent_steer_identity(const std::uint64_t aNativeTurn, const std::uint32_t aOrdinal) -> std::string
	{
		// b5ee7e0? marks a BPR correction; the version nibble 4 and variant 8
		// keep it a well-formed UUID.
		auto result = std::string{ "b5ee7e0" };
		append_hex(result, aOrdinal & 0xFU, 1U);
		result += '-';
		append_hex(result, aNativeTurn >> 48U, 4U);
		result += "-4";
		append_hex(result, (aNativeTurn >> 36U) & 0xFFFU, 3U);
		result += "-8";
		append_hex(result, (aNativeTurn >> 24U) & 0xFFFU, 3U);
		result += '-';
		append_hex(result, ((aNativeTurn & 0xFFFFFFU) << 24U) | (aOrdinal & 0xFFFFFFU), 12U);
		return result;
	}

	auto read_run_cost_report(const agent_run_cost_reading& aReading, const std::optional<agent_cost> aReported) -> agent_run_cost_reading
	{
		auto result = aReading;
		if (!aReported) {
			return result;
		}
		++result.Reports;
		if (result.Unknown) {
			return result;
		}
		if (result.Figure && *aReported < *result.Figure) {
			result.Figure.reset();
			result.Unknown = true;
			return result;
		}
		result.Figure = aReported;
		return result;
	}

	void settle_steered_messages(std::vector<agent_steered_message>& aMessages, const std::vector<agent_steer_disposition>& aEndings) noexcept
	{
		for (std::size_t index = 0U; index < aMessages.size() && index < aEndings.size(); ++index) {
			aMessages[index].Disposition = aEndings[index];
		}
	}
} // namespace bpr::core
