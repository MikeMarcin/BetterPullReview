#include "bpr/core/review_cycle_retrieval.hpp"

#include "bpr/core/review_cycle_json.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>

namespace bpr::core {
	namespace {
		auto decimal(const std::string_view aText) -> std::optional<std::uint64_t>
		{
			auto value = std::uint64_t{ 0 };
			if (aText.empty() || (aText.size() > 1U && aText.front() == '0')) {
				return std::nullopt;
			}
			const auto parsed = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}

		/// Splits on `aSeparator` into exactly `aCount` parts, or nothing.
		auto split(std::string_view aText, const char aSeparator, const std::size_t aCount) -> std::optional<std::vector<std::string_view>>
		{
			auto parts = std::vector<std::string_view>{ };
			while (parts.size() + 1U < aCount) {
				const auto at = aText.find(aSeparator);
				if (at == std::string_view::npos) {
					return std::nullopt;
				}
				parts.push_back(aText.substr(0, at));
				aText.remove_prefix(at + 1U);
			}
			if (aText.find(aSeparator) != std::string_view::npos) {
				return std::nullopt;
			}
			parts.push_back(aText);
			return parts;
		}
	}

	auto measure_round(const cycle_round_payload& aPayload) -> std::optional<cycle_round_extent>
	{
		auto extent = cycle_round_extent{ .Items = aPayload.Items.size(), .Sources = aPayload.Sources.size() };
		for (const auto& item : aPayload.Items) {
			const auto canonical = encode_round_item(item);
			if (!canonical) {
				return std::nullopt;
			}
			extent.InstructionBytes += canonical->size();
			extent.Suggestions += item.Suggestions.size();
		}
		extent.InstructionBytes += aPayload.Assignment.Goal.size();
		for (const auto* texts : { &aPayload.Assignment.Constraints, &aPayload.Assignment.WriteScope }) {
			for (const auto& text : *texts) {
				extent.InstructionBytes += text.size();
			}
		}
		for (const auto& source : aPayload.Sources) {
			extent.SourceBytes += source.Bytes.size();
		}
		return extent;
	}

	auto encode_round_cursor(const cycle_round_cursor& aCursor) -> std::string
	{
		return cycle_wire_name(aCursor.Round) + "." + cycle_hex(aCursor.Digest.bytes()) + "." + std::to_string(aCursor.Next) + "." + std::to_string(aCursor.PageBytes);
	}

	auto parse_round_cursor(const std::string_view aText) -> std::optional<cycle_round_cursor>
	{
		if (aText.size() > 128U) {
			return std::nullopt;
		}
		const auto parts = split(aText, '.', 4U);
		if (!parts) {
			return std::nullopt;
		}
		const auto round  = parse_round_name((*parts)[0]);
		const auto digest = parse_cycle_sha256((*parts)[1]);
		const auto next   = decimal((*parts)[2]);
		const auto page   = decimal((*parts)[3]);
		if (!round || !digest || !next || !page) {
			return std::nullopt;
		}
		return cycle_round_cursor{ .Round = *round, .Digest = cycle_round_digest{ *digest }, .Next = *next, .PageBytes = *page };
	}

	auto stable_name(const cycle_retrieval_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_retrieval_problem::page_size:      return "page_size";
		case cycle_retrieval_problem::cursor:         return "cursor";
		case cycle_retrieval_problem::over_budget:    return "over_budget";
		case cycle_retrieval_problem::item_over_page: return "item_over_page";
		case cycle_retrieval_problem::unknown_source: return "unknown_source";
		case cycle_retrieval_problem::source_range:   return "source_range";
		case cycle_retrieval_problem::source_cursor:  return "source_cursor";
		}
		return "cursor";
	}

	auto page_round_items(const cycle_decoded_round& aRound, const cycle_round_id aId, const std::uint64_t aFrom, const std::uint64_t aPageBytes)
		-> std::expected<cycle_round_items_page, cycle_retrieval_problem>
	{
		if (aPageBytes < minimum_round_page_bytes || aPageBytes > maximum_round_page_bytes) {
			return std::unexpected(cycle_retrieval_problem::page_size);
		}
		const auto& items = aRound.Payload.Items;
		if (aFrom >= items.size()) {
			return std::unexpected(cycle_retrieval_problem::cursor);
		}
		auto page = cycle_round_items_page{ .First = aFrom };
		for (auto index = aFrom; index < items.size(); ++index) {
			auto canonical = encode_round_item(items[index]);
			if (!canonical) {
				return std::unexpected(cycle_retrieval_problem::item_over_page);
			}
			if (page.Items.empty() && canonical->size() > maximum_round_page_bytes) {
				// Splitting an item would hand over part of an instruction as if it were whole.
				return std::unexpected(cycle_retrieval_problem::item_over_page);
			}
			if (!page.Items.empty() && page.Bytes + canonical->size() > aPageBytes) {
				break;
			}
			page.Bytes += canonical->size();
			page.Items.push_back(std::move(*canonical));
		}
		const auto next = aFrom + page.Items.size();
		if (next < items.size()) {
			page.Next = cycle_round_cursor{ .Round = aId, .Digest = aRound.Digest, .Next = next, .PageBytes = aPageBytes };
		}
		return page;
	}

	auto admit_round_cursor(const cycle_round_cursor& aCursor, const cycle_round_id aRound, const cycle_round_digest& aDigest, const cycle_round_extent& aExtent) noexcept
		-> std::optional<cycle_retrieval_problem>
	{
		if (aCursor.Round != aRound || aCursor.Digest != aDigest || aCursor.Next == 0U || aCursor.Next >= aExtent.Items) {
			return cycle_retrieval_problem::cursor;
		}
		if (aCursor.PageBytes < minimum_round_page_bytes || aCursor.PageBytes > maximum_round_page_bytes) {
			return cycle_retrieval_problem::page_size;
		}
		return std::nullopt;
	}

	auto admit_round_budget(const cycle_round_extent& aExtent, const std::uint64_t aBudget) noexcept -> std::optional<cycle_retrieval_problem>
	{
		return aExtent.InstructionBytes > aBudget ? std::optional{ cycle_retrieval_problem::over_budget } : std::nullopt;
	}

	auto list_round_sources(const cycle_round_payload& aPayload, const std::uint64_t aFrom) -> std::expected<cycle_round_source_listing, cycle_retrieval_problem>
	{
		const auto total = static_cast<std::uint64_t>(aPayload.Sources.size());
		if (aFrom > total || (aFrom == total && aFrom != 0U)) {
			return std::unexpected(cycle_retrieval_problem::source_cursor);
		}
		const auto count = std::min<std::uint64_t>(total - aFrom, round_source_list_entries);
		auto       list  = cycle_round_source_listing{ .First = aFrom, .Count = count };
		if (aFrom + count < total) {
			list.Next = aFrom + count;
		}
		return list;
	}

	auto parse_source_range(const std::string_view aText) -> std::optional<cycle_source_range>
	{
		const auto parts = split(aText, ':', 2U);
		if (!parts) {
			return std::nullopt;
		}
		const auto start = decimal((*parts)[0]);
		const auto end   = decimal((*parts)[1]);
		if (!start || !end || *end <= *start) {
			return std::nullopt;
		}
		return cycle_source_range{ .Start = *start, .End = *end };
	}

	auto slice_round_source(const cycle_round_payload& aPayload, const std::string_view aSource, const std::optional<cycle_source_range>& aRange)
		-> std::expected<cycle_round_source_slice, cycle_retrieval_problem>
	{
		const auto found = std::ranges::find(aPayload.Sources, aSource, &cycle_wire_source::Id);
		if (found == aPayload.Sources.end()) {
			return std::unexpected(cycle_retrieval_problem::unknown_source);
		}
		const auto size  = static_cast<std::uint64_t>(found->Bytes.size());
		const auto asked = aRange.value_or(cycle_source_range{ .Start = 0U, .End = size });
		if (asked.End > size || asked.Start >= asked.End) {
			// An empty attachment has nothing to read, and says so.
			if (size == 0U && !aRange) {
				return cycle_round_source_slice{ .Source = static_cast<std::size_t>(std::distance(aPayload.Sources.begin(), found)) };
			}
			return std::unexpected(cycle_retrieval_problem::source_range);
		}
		auto slice = cycle_round_source_slice{
			.Source = static_cast<std::size_t>(std::distance(aPayload.Sources.begin(), found)),
			.Range  = { .Start = asked.Start, .End = std::min(asked.End, asked.Start + maximum_round_source_chunk_bytes) },
		};
		if (slice.Range.End < asked.End) {
			slice.Next = cycle_source_range{ .Start = slice.Range.End, .End = asked.End };
		}
		return slice;
	}
}
