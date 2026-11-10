#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_COST_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_COST_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1999]], RFC-BPR-0101. An agent's reported spend, in
	/// billionths of a US dollar, so a session's turns sum exactly.
	using agent_cost = std::uint64_t;

	/// The most one report may say: a billion dollars.
	inline constexpr agent_cost maximum_agent_cost = 1'000'000'000ULL * 1'000'000'000ULL;

	/// \brief Reads a runtime's figure: a plain non-negative decimal of
	/// dollars ("0.4123567", "12"). Digits past the ninth decimal place are
	/// rounded half up. Anything else, or past the bound, is no figure.
	[[nodiscard]] auto parse_agent_cost(std::string_view aDecimal) -> std::optional<agent_cost>;

	/// \brief Dollars and cents, rounded half up: `$0.41`; a nonzero amount
	/// under half a cent is `under $0.01`.
	[[nodiscard]] auto agent_cost_words(agent_cost aCost) -> std::string;

	/// \brief What one conversation has cost so far.
	///
	/// \details `Reported` is the runtime's own last figure for the native
	/// session this conversation continues. Claude Code 2.1.319 reports
	/// `total_cost_usd` cumulatively over a resumed session (established on
	/// 2026-11-10: three `--resume` turns reported 0.00279231, 0.00549827 and
	/// 0.00631917, the sums of each turn's own usage; see
	/// `docs/verification/RFC-BPR-0101.md`). `Total` is the sum of the turn
	/// costs this conversation knows; `Known` says any turn reported one.
	struct agent_cost_ledger
	{
		std::optional<agent_cost> Reported;
		agent_cost                Total = 0U;
		bool                      Known = false;

		auto operator<=>(const agent_cost_ledger&) const = default;
	};

	/// \brief One turn's cost, and the ledger after it.
	struct agent_turn_cost
	{
		std::optional<agent_cost> Turn;
		agent_cost_ledger         Ledger;

		auto operator<=>(const agent_turn_cost&) const = default;
	};

	/// \brief Accounts one turn's reported figure.
	///
	/// \details Pure. With no figure the turn has no cost and the ledger is
	/// unchanged. A turn that continued the runtime's own session
	/// (`aContinued`) cost the difference from the session's previous figure;
	/// with no previous figure the difference is unknown, so the turn shows
	/// none rather than the session's whole spend. A figure below the
	/// previous one means the runtime started counting again: it is the
	/// turn's own. A turn that started a session cost its figure. The ledger's
	/// total is always the sum of the turn costs it accounted.
	[[nodiscard]] auto account_agent_turn_cost(const agent_cost_ledger& aLedger, std::optional<agent_cost> aReported, bool aContinued) -> agent_turn_cost;
} // namespace bpr::core

#endif
