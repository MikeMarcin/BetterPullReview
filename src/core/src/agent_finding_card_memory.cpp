#include "bpr/core/agent_finding_card_memory.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <span>

namespace bpr::core {
	auto digest_agent_findings(const std::string_view aPayload) noexcept -> agent_findings_digest
	{
		return sha256({ reinterpret_cast<const std::uint8_t*>(aPayload.data()), aPayload.size() });
	}

	auto stable_name(const agent_finding_card_disposition aDisposition) noexcept -> std::string_view
	{
		switch (aDisposition) {
		case agent_finding_card_disposition::open:      return "open";
		case agent_finding_card_disposition::drafted:   return "drafted";
		case agent_finding_card_disposition::noted:     return "noted";
		case agent_finding_card_disposition::dismissed: return "dismissed";
		case agent_finding_card_disposition::refused:   return "refused";
		}
		return "open";
	}

	auto normalized(agent_finding_card_record aRecord) -> agent_finding_card_record
	{
		if (aRecord.State != agent_finding_card_disposition::refused) {
			aRecord.Why.clear();
		} else if (aRecord.Why.size() > maximum_finding_card_reason_bytes) {
			auto size = maximum_finding_card_reason_bytes;
			while (size > 0U && (static_cast<unsigned char>(aRecord.Why[size]) & 0xC0U) == 0x80U) {
				--size;
			}
			aRecord.Why.resize(size);
		}
		if (aRecord.State != agent_finding_card_disposition::noted) {
			aRecord.Note = 0U;
		}
		return aRecord;
	}

	auto restored_finding_session(const agent_findings_digest& aFindings) noexcept -> std::uint64_t
	{
		auto session = std::uint64_t{ 0U };
		for (std::size_t index = 0U; index < sizeof(session); ++index) {
			session = (session << 8U) | aFindings[index];
		}
		return session | (std::uint64_t{ 1U } << 63U);
	}

	auto agent_finding_card_memory::remember(agent_finding_card_record aRecord) -> bool
	{
		aRecord        = normalized(std::move(aRecord));
		const auto key = std::pair{ aRecord.Findings, aRecord.Ordinal };
		if (!Adopted) {
			Touched.insert(key);
		}
		const auto found = Records.find(key);
		if (aRecord.State == agent_finding_card_disposition::open) {
			if (found == Records.end()) {
				return false;
			}
			Records.erase(found);
			return true;
		}
		if (found != Records.end() && found->second.Record == aRecord) {
			return false;
		}
		Records.insert_or_assign(key, entry{ .Record = std::move(aRecord), .Changed = NextChange++ });
		trim();
		return true;
	}

	void agent_finding_card_memory::adopt(const std::vector<agent_finding_card_record>& aRecords)
	{
		// What was remembered before the store answered is newer than all of
		// it: the stored records take the oldest places, in their order.
		const auto stored = static_cast<std::uint64_t>(aRecords.size());
		for (auto& [key, kept] : Records) {
			static_cast<void>(key);
			kept.Changed += stored;
		}
		auto place = std::uint64_t{ 0U };
		for (const auto& read : aRecords) {
			auto       record = normalized(read);
			const auto key    = std::pair{ record.Findings, record.Ordinal };
			const auto at     = place++;
			if (record.State == agent_finding_card_disposition::open || Touched.contains(key)) {
				continue;
			}
			Records.insert_or_assign(key, entry{ .Record = std::move(record), .Changed = at });
		}
		NextChange += stored;
		Touched.clear();
		Adopted = true;
		trim();
	}

	auto agent_finding_card_memory::recall(const agent_findings_digest& aFindings, const std::uint32_t aOrdinal) const -> agent_finding_card_record
	{
		const auto found = Records.find(std::pair{ aFindings, aOrdinal });
		return found != Records.end() ? found->second.Record : agent_finding_card_record{ .Findings = aFindings, .Ordinal = aOrdinal };
	}

	auto agent_finding_card_memory::records() const -> std::vector<agent_finding_card_record>
	{
		auto ordered = std::vector<const entry*>{ };
		ordered.reserve(Records.size());
		for (const auto& [key, kept] : Records) {
			static_cast<void>(key);
			ordered.push_back(&kept);
		}
		std::ranges::sort(ordered, { }, &entry::Changed);
		auto result = std::vector<agent_finding_card_record>{ };
		result.reserve(ordered.size());
		for (const auto* const kept : ordered) {
			result.push_back(kept->Record);
		}
		return result;
	}

	void agent_finding_card_memory::trim()
	{
		while (Records.size() > maximum_remembered_finding_cards) {
			const auto oldest = std::ranges::min_element(Records, { }, [](const auto& aKept) { return aKept.second.Changed; });
			Records.erase(oldest);
		}
	}
}
