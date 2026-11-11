// Owner report 2026-11-10, after BPR-2052. What was done with each finding
// card of a stored answer, kept by its block's digest and its place.

#include "bpr/core/agent_finding_card_memory.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "agent finding card memory failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto card(const agent_findings_digest& aFindings, const std::uint32_t aOrdinal, const agent_finding_card_disposition aState) -> agent_finding_card_record
	{
		return { .Findings = aFindings, .Ordinal = aOrdinal, .State = aState };
	}

	void test_digest_and_session()
	{
		const auto one = digest_agent_findings("{\"findings\":[]}");
		expect(one == digest_agent_findings("{\"findings\":[]}"), "equal blocks have equal digests");
		expect(one != digest_agent_findings("{\"findings\":[ ]}"), "the block as written is what is digested");
		expect(restored_finding_session(one) == restored_finding_session(one), "a block restores under one identity");
		expect(is_restored_finding_session(restored_finding_session(one)) && !is_restored_finding_session(9U), "a restored identity is never a session's");
	}

	void test_remember_recall_and_forget()
	{
		const auto block  = digest_agent_findings("block");
		auto       memory = agent_finding_card_memory{ };
		expect(memory.recall(block, 1U).State == agent_finding_card_disposition::open, "a card nothing was done with is open");
		expect(!memory.remember(card(block, 1U, agent_finding_card_disposition::open)), "an open card is no record");
		expect(memory.remember(card(block, 1U, agent_finding_card_disposition::drafted)), "a drafted card is kept");
		expect(!memory.remember(card(block, 1U, agent_finding_card_disposition::drafted)), "the same again changes nothing");
		expect(memory.recall(block, 1U).State == agent_finding_card_disposition::drafted && memory.recall(block, 2U).State == agent_finding_card_disposition::open, "each card by its place");
		expect(memory.remember(card(block, 1U, agent_finding_card_disposition::open)) && memory.size() == 0U, "undoing forgets it");
	}

	void test_normalized()
	{
		auto refused    = card({ }, 0U, agent_finding_card_disposition::refused);
		refused.Why     = std::string(maximum_finding_card_reason_bytes - 1U, 'x') + "\xC3\xA9";
		refused.Note    = 7U;
		const auto kept = normalized(refused);
		expect(kept.Why.size() == maximum_finding_card_reason_bytes - 1U && kept.Note == 0U, "a reason is cut at a character boundary and a refusal names no note");
		auto drafted = card({ }, 0U, agent_finding_card_disposition::drafted);
		drafted.Why  = "stale";
		expect(normalized(drafted).Why.empty(), "only a refusal keeps a reason");
	}

	void test_adopt_keeps_newer_and_bounds()
	{
		const auto block  = digest_agent_findings("block");
		auto       memory = agent_finding_card_memory{ };
		static_cast<void>(memory.remember(card(block, 0U, agent_finding_card_disposition::open)));
		static_cast<void>(memory.remember(card(block, 1U, agent_finding_card_disposition::dismissed)));
		memory.adopt({ card(block, 0U, agent_finding_card_disposition::drafted), card(block, 1U, agent_finding_card_disposition::drafted), card(block, 2U, agent_finding_card_disposition::noted) });
		expect(memory.recall(block, 0U).State == agent_finding_card_disposition::open, "a card undone before the store answered stays open");
		expect(memory.recall(block, 1U).State == agent_finding_card_disposition::dismissed, "a card changed before the store answered keeps its change");
		expect(memory.recall(block, 2U).State == agent_finding_card_disposition::noted, "a stored card is adopted");
		const auto ordered = memory.records();
		expect(ordered.size() == 2U && ordered.front().Ordinal == 2U && ordered.back().Ordinal == 1U, "what changed here is newer than what was stored");

		auto bounded = agent_finding_card_memory{ };
		for (std::uint32_t ordinal = 0U; ordinal <= maximum_remembered_finding_cards; ++ordinal) {
			static_cast<void>(bounded.remember(card(block, ordinal, agent_finding_card_disposition::drafted)));
		}
		expect(bounded.size() == maximum_remembered_finding_cards && bounded.recall(block, 0U).State == agent_finding_card_disposition::open, "the least recently changed card goes first");
	}
}

auto main() -> int
{
	test_digest_and_session();
	test_remember_recall_and_forget();
	test_normalized();
	test_adopt_keeps_newer_and_bounds();
	std::cout << "agent-finding-card-memory:pass\n";
	return EXIT_SUCCESS;
}
