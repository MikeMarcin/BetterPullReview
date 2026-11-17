#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_KEEP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_KEEP_HPP

#include "bpr/core/agent_draft_all.hpp"
#include "bpr/core/agent_finding_card_memory.hpp"
#include "bpr/core/agent_report.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2231]], [[BPR-2232]]. What became of a report's findings is
/// the turn's one state for each finding, kept as a finding card's is.
/// \details A finding at a finding card's place (`report_finding::Card`) is
/// that card: its state is the card's record, under the card's findings
/// block and ordinal, so Drafted from the report reads Drafted on the card
/// and the other way round. A finding no card carries is kept under the
/// report's own digest and its row in the report's menu. Both live in the
/// one `agent_finding_card_memory` the host stores. Draft all from the
/// report plans as the Agent tab's Draft all does, over these states. Pure.
namespace bpr::core {
	/// \brief Names the record of a report's findings no card carries: the
	/// SHA-256 of the report's text under a prefix of its own, so it never
	/// names a findings block.
	[[nodiscard]] auto digest_report_findings(std::string_view aReport) noexcept -> agent_findings_digest;

	/// \brief Where one finding's state is kept: a findings block's digest
	/// (a card's) or a report's (`digest_report_findings`), and the card's
	/// ordinal or the finding's row in the report's menu.
	struct report_finding_key
	{
		agent_findings_digest Findings{ };
		std::uint32_t         Ordinal = 0U;
		/// It is a finding card's.
		bool Card = false;

		auto operator<=>(const report_finding_key&) const = default;
	};

	/// \brief One finding card of the answer, as `report_findings` was given
	/// it (`aSuggested`, in the same order): its findings block and ordinal.
	struct report_card_key
	{
		agent_findings_digest Findings{ };
		std::uint32_t         Ordinal = 0U;

		auto operator<=>(const report_card_key&) const = default;
	};

	/// \brief The finding's key: its card's when it is one of `aCards`, else
	/// the report's (`aReport`, from `digest_report_findings`) at its row.
	[[nodiscard]] auto report_finding_key_of(const report_finding& aFinding, std::span<const report_card_key> aCards, const agent_findings_digest& aReport) -> report_finding_key;

	/// \brief What a report says of a finding, from its kept state.
	enum class report_finding_standing : std::uint8_t {
		/// Its keep action is offered: nothing is kept.
		open,
		/// A private draft at its lines (Drafted).
		drafted,
		/// A Local comment (Kept).
		kept_local,
		/// Its card was dismissed; the report still offers its keep action.
		dismissed,
		/// The last action could not be done, and why.
		refused,
	};
	[[nodiscard]] auto stable_name(report_finding_standing aStanding) noexcept -> std::string_view;
	[[nodiscard]] auto report_finding_standing_of(agent_finding_card_disposition aKept) noexcept -> report_finding_standing;
	/// \brief The record a finished keep action leaves: Draft comment drafts,
	/// Keep local keeps a Local comment (with its note), a refusal says why.
	[[nodiscard]] auto report_keep_record(const report_finding_key& aKey, report_keep aKeep, bool aMade, std::string aWhy = { }, std::uint64_t aNote = 0U) -> agent_finding_card_record;

	/// \brief One finding as Draft all from the report reads it.
	struct report_draft_all_fact
	{
		report_finding_standing Standing = report_finding_standing::open;
		/// An action on it is under way.
		bool Working = false;
		/// Its file is in the pull request at the head and its lines in it,
		/// as far as the host knows now.
		bool Placeable = false;

		auto operator==(const report_draft_all_fact&) const -> bool = default;
	};

	/// \brief [[BPR-2232]]. The report's findings as the Agent tab's Draft
	/// all reads its cards, in the report's order: a finding whose keep
	/// action is Draft comment is a suggested comment, drafted unless it is
	/// drafted, kept local, dismissed, under way or not placeable; a finding
	/// only Keep local can keep is neither drafted nor counted.
	/// `aFacts` is by finding, as long as `aFindings`.
	[[nodiscard]] auto report_draft_all_candidates(std::span<const report_finding> aFindings, std::span<const report_draft_all_fact> aFacts) -> std::vector<draft_all_candidate>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_KEEP_HPP
