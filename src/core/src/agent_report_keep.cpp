#include "bpr/core/agent_report_keep.hpp"

#include "bpr/core/sha256.hpp"

#include <string>
#include <utility>

namespace bpr::core {
	auto digest_report_findings(const std::string_view aReport) noexcept -> agent_findings_digest
	{
		// A findings block is JSON and opens with `[` or `{`; this prefix
		// keeps a report's digest from ever being a block's.
		auto           hasher = sha256_stream{ };
		constexpr auto prefix = std::string_view{ "bpr-report-findings-v1\n" };
		hasher.update({ reinterpret_cast<const std::uint8_t*>(prefix.data()), prefix.size() });
		hasher.update({ reinterpret_cast<const std::uint8_t*>(aReport.data()), aReport.size() });
		return hasher.finish();
	}

	auto report_finding_key_of(const report_finding& aFinding, const std::span<const report_card_key> aCards, const agent_findings_digest& aReport) -> report_finding_key
	{
		if (aFinding.Card && *aFinding.Card < aCards.size()) {
			const auto& card = aCards[*aFinding.Card];
			return { .Findings = card.Findings, .Ordinal = card.Ordinal, .Card = true };
		}
		return { .Findings = aReport, .Ordinal = aFinding.Outline, .Card = false };
	}

	auto stable_name(const report_finding_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case report_finding_standing::open:       return "open";
		case report_finding_standing::drafted:    return "drafted";
		case report_finding_standing::kept_local: return "kept_local";
		case report_finding_standing::dismissed:  return "dismissed";
		case report_finding_standing::refused:    return "refused";
		}
		return "open";
	}

	auto report_finding_standing_of(const agent_finding_card_disposition aKept) noexcept -> report_finding_standing
	{
		switch (aKept) {
		case agent_finding_card_disposition::open:      return report_finding_standing::open;
		case agent_finding_card_disposition::drafted:   return report_finding_standing::drafted;
		case agent_finding_card_disposition::noted:     return report_finding_standing::kept_local;
		case agent_finding_card_disposition::dismissed: return report_finding_standing::dismissed;
		case agent_finding_card_disposition::refused:   return report_finding_standing::refused;
		}
		return report_finding_standing::open;
	}

	auto report_keep_record(const report_finding_key& aKey, const report_keep aKeep, const bool aMade, std::string aWhy, const std::uint64_t aNote) -> agent_finding_card_record
	{
		auto record = agent_finding_card_record{ .Findings = aKey.Findings, .Ordinal = aKey.Ordinal };
		if (!aMade) {
			record.State = agent_finding_card_disposition::refused;
			record.Why   = std::move(aWhy);
		} else if (aKeep == report_keep::draft_comment) {
			record.State = agent_finding_card_disposition::drafted;
		} else if (aKeep == report_keep::keep_local) {
			record.State = agent_finding_card_disposition::noted;
			record.Note  = aNote;
		}
		return normalized(std::move(record));
	}

	auto report_draft_all_candidates(const std::span<const report_finding> aFindings, const std::span<const report_draft_all_fact> aFacts) -> std::vector<draft_all_candidate>
	{
		auto cards = std::vector<draft_all_candidate>{ };
		cards.reserve(aFindings.size());
		for (std::size_t index = 0U; index < aFindings.size(); ++index) {
			const auto fact  = index < aFacts.size() ? aFacts[index] : report_draft_all_fact{ };
			auto       state = draft_all_card_state::open;
			if (fact.Working) {
				state = draft_all_card_state::working;
			} else {
				switch (fact.Standing) {
				case report_finding_standing::open:
				case report_finding_standing::refused:    state = draft_all_card_state::open; break;
				case report_finding_standing::drafted:    state = draft_all_card_state::drafted; break;
				case report_finding_standing::kept_local: state = draft_all_card_state::kept_local; break;
				case report_finding_standing::dismissed:  state = draft_all_card_state::dismissed; break;
				}
			}
			cards.push_back({ .State = state, .Suggested = aFindings[index].Keep == report_keep::draft_comment, .Placeable = fact.Placeable });
		}
		return cards;
	}
} // namespace bpr::core
