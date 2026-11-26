#include "bpr/core/agent_cost.hpp"

namespace bpr::core {
	auto parse_agent_cost(const std::string_view aDecimal) -> std::optional<agent_cost>
	{
		constexpr auto places = 9U;
		const auto     dot    = aDecimal.find('.');
		const auto     whole  = aDecimal.substr(0U, dot);
		const auto     part   = dot == std::string_view::npos ? std::string_view{ } : aDecimal.substr(dot + 1U);
		const auto     digits = [](const std::string_view aText) {
			for (const auto character : aText) {
				if (character < '0' || character > '9') {
					return false;
				}
			}
			return true;
		};
		if (whole.empty() || whole.size() > 10U || !digits(whole) || !digits(part) || (dot != std::string_view::npos && part.empty()) || part.size() > 64U) {
			return std::nullopt;
		}
		auto dollars = agent_cost{ 0U };
		for (const auto character : whole) {
			dollars = dollars * 10U + static_cast<agent_cost>(character - '0');
		}
		auto fraction = agent_cost{ 0U };
		for (auto index = 0U; index < places; ++index) {
			fraction = fraction * 10U + (index < part.size() ? static_cast<agent_cost>(part[index] - '0') : 0U);
		}
		// Half up at the ninth place.
		if (part.size() > places && part[places] >= '5') {
			++fraction;
		}
		const auto cost = dollars * 1'000'000'000ULL + fraction;
		if (cost > maximum_agent_cost) {
			return std::nullopt;
		}
		return cost;
	}

	auto agent_cost_words(const agent_cost aCost) -> std::string
	{
		constexpr auto cent = agent_cost{ 10'000'000U };
		if (aCost != 0U && aCost < cent / 2U) {
			return "< $0.01";
		}
		const auto cents   = (aCost + cent / 2U) / cent;
		const auto dollars = cents / 100U;
		const auto rest    = cents % 100U;
		return "$" + std::to_string(dollars) + "." + (rest < 10U ? "0" : "") + std::to_string(rest);
	}

	auto account_agent_turn_cost(const agent_cost_ledger& aLedger, const std::optional<agent_cost> aReported, const bool aContinued) -> agent_turn_cost
	{
		auto result = agent_turn_cost{ .Ledger = aLedger };
		if (!aReported) {
			return result;
		}
		if (!aContinued || (aLedger.Reported && *aReported < *aLedger.Reported)) {
			result.Turn = aReported;
		} else if (aLedger.Reported) {
			result.Turn = *aReported - *aLedger.Reported;
		}
		result.Ledger.Reported = aReported;
		if (result.Turn) {
			result.Ledger.Total += *result.Turn;
			result.Ledger.Known = true;
		}
		return result;
	}
} // namespace bpr::core
