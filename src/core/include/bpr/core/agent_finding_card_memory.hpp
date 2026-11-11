#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_CARD_MEMORY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_CARD_MEMORY_HPP

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Owner report 2026-11-10, after BPR-2052: a review answer's finding
/// cards came back while the application ran but not after a restart. What a
/// reviewer did with each card of a stored answer, kept by the answer's
/// `bpr-findings` block, so a restored thread makes its cards again as they
/// were: a drafted one says Drafted once and is not offered again.
/// \details The stored answer keeps the block as the agent wrote it, so the
/// block's SHA-256 names its cards across restarts and across the native
/// history and the kept thread alike; a card is that block and its ordinal.
/// Nothing here reads or writes a store. The host keeps the records and
/// performs the writes this memory says are due.
namespace bpr::core {
	/// \brief The SHA-256 of one findings block's JSON text, exactly as
	/// `split_agent_answer` returned it in `Payload`.
	using agent_findings_digest = std::array<std::uint8_t, 32>;

	[[nodiscard]] auto digest_agent_findings(std::string_view aPayload) noexcept -> agent_findings_digest;

	/// \brief What became of one card, as it is kept. A card being worked on
	/// is not kept: after a restart it is open again.
	enum class agent_finding_card_disposition : std::uint8_t {
		/// Still to be acted on. Kept as no record at all.
		open,
		/// Added as a private draft comment; not offered again.
		drafted,
		/// Saved as a private note, with the note it made.
		noted,
		dismissed,
		/// The last action could not be done; the card stays open and says why.
		refused,
	};
	inline constexpr std::size_t agent_finding_card_disposition_count = 5U;
	static_assert(static_cast<std::size_t>(agent_finding_card_disposition::refused) + 1U == agent_finding_card_disposition_count,
				  "agent_finding_card_disposition_count must cover every disposition");

	[[nodiscard]] auto stable_name(agent_finding_card_disposition aDisposition) noexcept -> std::string_view;

	/// The longest refusal reason kept, cut at a UTF-8 character boundary.
	inline constexpr std::size_t maximum_finding_card_reason_bytes = 1024U;
	/// The most cards whose dispositions are kept; the least recently changed
	/// go first.
	inline constexpr std::size_t maximum_remembered_finding_cards = 4096U;

	/// \brief One card's kept disposition.
	struct agent_finding_card_record
	{
		agent_findings_digest          Findings{ };
		std::uint32_t                  Ordinal = 0U;
		agent_finding_card_disposition State   = agent_finding_card_disposition::open;
		/// Why it was refused; empty for every other disposition.
		std::string Why;
		/// The private note Save as note made; 0 unless `noted`.
		std::uint64_t Note = 0U;

		auto operator<=>(const agent_finding_card_record&) const = default;
	};

	/// \brief The record as it is kept: the reason only with a refusal, bounded,
	/// and the note only with `noted`.
	[[nodiscard]] auto normalized(agent_finding_card_record aRecord) -> agent_finding_card_record;

	/// \brief The delivery identity a restored card carries, derived from its
	/// block so the same answer restores under the same identity. Its top bit
	/// is set, which no session the application admits has.
	[[nodiscard]] auto           restored_finding_session(const agent_findings_digest& aFindings) noexcept -> std::uint64_t;
	[[nodiscard]] constexpr auto is_restored_finding_session(const std::uint64_t aSession) noexcept -> bool { return (aSession >> 63U) != 0U; }

	/// \brief The dispositions of the cards of stored answers, bounded to
	/// `maximum_remembered_finding_cards`.
	/// \details Deterministic: equal sequences of calls give equal contents.
	/// `remember` says whether the kept state changed, which is when the host
	/// writes it; an open card is no record.
	class agent_finding_card_memory
	{
	public:
		/// \brief Keeps \p aRecord (normalized), or forgets the card when it is
		/// open. \returns Whether anything kept changed.
		[[nodiscard]] auto remember(agent_finding_card_record aRecord) -> bool;
		/// \brief Records read back from a store, oldest first. A card already
		/// remembered here before is newer and is not replaced, and stays
		/// newer than every record adopted.
		void adopt(const std::vector<agent_finding_card_record>& aRecords);
		/// \brief The card's disposition; `open` with nothing else when none is kept.
		[[nodiscard]] auto recall(const agent_findings_digest& aFindings, std::uint32_t aOrdinal) const -> agent_finding_card_record;
		/// \brief Every kept record, least recently changed first.
		[[nodiscard]] auto records() const -> std::vector<agent_finding_card_record>;
		[[nodiscard]] auto size() const noexcept -> std::size_t { return Records.size(); }

	private:
		using key = std::pair<agent_findings_digest, std::uint32_t>;
		struct entry
		{
			agent_finding_card_record Record;
			std::uint64_t             Changed = 0U;
		};
		void                 trim();
		std::uint64_t        NextChange = 0U;
		bool                 Adopted    = false;
		std::map<key, entry> Records;
		/// Cards remembered here before `adopt`; a store's older answer for
		/// them is not adopted.
		std::set<key> Touched;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_CARD_MEMORY_HPP
