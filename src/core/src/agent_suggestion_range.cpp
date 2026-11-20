#include "bpr/core/agent_suggestion_range.hpp"

#include "bpr/core/suggestion_draft.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

namespace bpr::core {
	namespace {
		/// \brief A line that says something: one letter, digit or non-ASCII
		/// byte. A brace, a bracket or a blank line alone never moves a range.
		[[nodiscard]] auto substantive(const std::string_view aLine) noexcept -> bool
		{
			return std::ranges::any_of(aLine, [](const char aByte) {
				const auto byte = static_cast<unsigned char>(aByte);
				return byte >= 0x80U || (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
			});
		}

		/// \brief The head lines the caller supplied, by one-based line.
		struct head_window
		{
			std::span<const std::string> Lines;
			std::uint32_t                First = 1U;
			std::uint32_t                Count = 0U;

			[[nodiscard]] auto holds(const std::uint32_t aLine) const noexcept -> bool
			{
				return aLine >= First && aLine >= 1U && aLine <= Count && aLine - First < Lines.size();
			}
			[[nodiscard]] auto at(const std::uint32_t aLine) const -> const std::string&
			{
				return Lines[aLine - First];
			}
		};

		/// \brief How many of the block's first lines the head holds, in
		/// order, from `aStart`.
		[[nodiscard]] auto prefix_from(const std::span<const std::string> aBlock, const head_window& aHead, const std::uint32_t aStart) -> std::uint32_t
		{
			auto matched = std::uint32_t{ 0U };
			while (matched < aBlock.size() && aHead.holds(aStart + matched) && aHead.at(aStart + matched) == aBlock[matched]) {
				++matched;
			}
			return matched;
		}

		/// \brief How many of the block's last lines the head holds, in
		/// order, ending at `aEnd`.
		[[nodiscard]] auto suffix_to(const std::span<const std::string> aBlock, const head_window& aHead, const std::uint32_t aEnd) -> std::uint32_t
		{
			auto matched = std::uint32_t{ 0U };
			while (matched < aBlock.size() && matched < aEnd && aHead.holds(aEnd - matched) && aHead.at(aEnd - matched) == aBlock[aBlock.size() - 1U - matched]) {
				++matched;
			}
			return matched;
		}

		/// \brief One window the block could replace, and how many of its
		/// lines the block keeps at its two ends.
		struct window_fit
		{
			std::uint32_t First  = 0U;
			std::uint32_t Last   = 0U;
			std::uint32_t Prefix = 0U;
			std::uint32_t Suffix = 0U;

			[[nodiscard]] auto kept() const noexcept -> std::uint32_t { return Prefix + Suffix; }
			[[nodiscard]] auto size() const noexcept -> std::uint32_t { return Last - First + 1U; }
		};

		/// \brief The window's fit: the prefix and suffix the block keeps,
		/// never counting a line twice.
		[[nodiscard]] auto fit_of(const std::uint32_t aFirst, const std::uint32_t aLast, const std::uint32_t aPrefix, const std::uint32_t aSuffix, const std::uint32_t aBlockLines) -> window_fit
		{
			const auto bound  = std::min(aBlockLines, aLast - aFirst + 1U);
			const auto prefix = std::min(aPrefix, bound);
			return { .First = aFirst, .Last = aLast, .Prefix = prefix, .Suffix = std::min(aSuffix, bound - prefix) };
		}

		/// \brief Whether the window grown past the agent's lines is
		/// explained: each added end is a line the block keeps, and one kept
		/// added line is substantive.
		[[nodiscard]] auto explained(const window_fit& aFit, const std::uint32_t aFirst, const std::uint32_t aLast, const head_window& aHead) -> bool
		{
			if ((aFit.First < aFirst && aFit.Prefix < 1U) || (aFit.Last > aLast && aFit.Suffix < 1U)) {
				return false;
			}
			const auto keptAdded = [&](const std::uint32_t aLine) {
				const auto kept = aLine < aFit.First + aFit.Prefix || aLine > aFit.Last - aFit.Suffix;
				return (aLine < aFirst || aLine > aLast) && kept && substantive(aHead.at(aLine));
			};
			for (auto line = aFit.First; line <= aFit.Last; ++line) {
				if (keptAdded(line)) {
					return true;
				}
			}
			return false;
		}

		/// \brief The best window around the agent's lines the block
		/// explains better than those lines: most lines kept, then the
		/// smallest window, then the earliest.
		[[nodiscard]] auto best_window(const std::span<const std::string> aBlock, const head_window& aHead, const std::uint32_t aFirst, const std::uint32_t aLast, const std::uint32_t aStatedKept)
			-> std::optional<window_fit>
		{
			const auto blockLines = static_cast<std::uint32_t>(aBlock.size());
			const auto lowest     = aFirst > blockLines ? aFirst - blockLines : 1U;
			const auto highest    = std::min(aHead.Count, aLast + blockLines);
			auto       prefixes   = std::vector<std::uint32_t>{ };
			auto       suffixes   = std::vector<std::uint32_t>{ };
			for (auto start = lowest; start <= aFirst; ++start) {
				prefixes.push_back(prefix_from(aBlock, aHead, start));
			}
			for (auto end = aLast; end <= highest; ++end) {
				suffixes.push_back(suffix_to(aBlock, aHead, end));
			}
			auto best = std::optional<window_fit>{ };
			for (auto start = lowest; start <= aFirst; ++start) {
				for (auto end = aLast; end <= highest; ++end) {
					const auto added = (aFirst - start) + (end - aLast);
					if (added == 0U || added > blockLines || !aHead.holds(start) || !aHead.holds(end)) {
						continue;
					}
					const auto fit = fit_of(start, end, prefixes[start - lowest], suffixes[end - aLast], blockLines);
					if (fit.kept() <= aStatedKept || !explained(fit, aFirst, aLast, aHead)) {
						continue;
					}
					if (!best || fit.kept() > best->kept() || (fit.kept() == best->kept() && fit.size() < best->size())) {
						best = fit;
					}
				}
			}
			return best;
		}

		/// \brief A line's indentation and the identifier it starts with:
		/// `\tWidth  = aWidth;` is (`\t`, `Width`). Empty word for a line
		/// that starts with anything else.
		[[nodiscard]] auto leading_word(const std::string_view aLine) noexcept -> std::pair<std::string_view, std::string_view>
		{
			const auto indent = std::min(aLine.find_first_not_of(" \t"), aLine.size());
			auto       end    = indent;
			while (end < aLine.size()) {
				const auto byte = static_cast<unsigned char>(aLine[end]);
				if (!(byte == '_' || (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z'))) {
					break;
				}
				++end;
			}
			return { aLine.substr(0U, indent), aLine.substr(indent, end - indent) };
		}

		/// \brief The block's line rewrites the head's: the same indentation
		/// and the same leading identifier (`\tWidth  = std::max(aWidth, 0);`
		/// for `\tWidth  = aWidth;`).
		[[nodiscard]] auto alike(const std::string_view aBlock, const std::string_view aHead) noexcept -> bool
		{
			const auto block = leading_word(aBlock);
			return !block.second.empty() && block == leading_word(aHead);
		}

		/// \brief The window of exactly the block's length around the agent's
		/// lines that the block rewrites line for line: each line the same or
		/// alike (`alike`), and a line beside the agent's that says something
		/// so. Most lines the same, then the earliest.
		[[nodiscard]] auto rewritten_window(const std::span<const std::string> aBlock, const head_window& aHead, const std::uint32_t aFirst, const std::uint32_t aLast) -> std::optional<window_fit>
		{
			const auto blockLines = static_cast<std::uint32_t>(aBlock.size());
			const auto lowest     = aLast >= blockLines ? aLast - blockLines + 1U : 1U;
			auto       best       = std::optional<window_fit>{ };
			auto       bestSame   = std::uint32_t{ 0U };
			for (auto start = lowest; start <= aFirst; ++start) {
				const auto end = start + blockLines - 1U;
				if (!aHead.holds(start) || !aHead.holds(end)) {
					continue;
				}
				auto same     = std::uint32_t{ 0U };
				auto rewrites = true;
				auto evidence = false;
				for (auto offset = std::uint32_t{ 0U }; offset < blockLines && rewrites; ++offset) {
					const auto  line   = start + offset;
					const auto& head   = aHead.at(line);
					const auto  equal  = head == aBlock[offset];
					const auto  akin   = !equal && alike(aBlock[offset], head);
					const auto  beside = line < aFirst || line > aLast;
					rewrites           = equal || akin;
					same += equal ? 1U : 0U;
					evidence = evidence || (beside && (akin || (equal && substantive(head))));
				}
				if (rewrites && evidence && (!best || same > bestSame)) {
					best     = window_fit{ .First = start, .Last = end };
					bestSame = same;
				}
			}
			return best;
		}
	} // namespace

	auto stable_name(const agent_suggestion_range_reading aReading) noexcept -> std::string_view
	{
		switch (aReading) {
		case agent_suggestion_range_reading::no_suggestion:    return "no_suggestion";
		case agent_suggestion_range_reading::kept:             return "kept";
		case agent_suggestion_range_reading::widened:          return "widened";
		case agent_suggestion_range_reading::unverified:       return "unverified";
		case agent_suggestion_range_reading::outside_document: return "outside_document";
		}
		return "unknown";
	}

	auto single_suggestion_lines(const std::string_view aComment) -> std::optional<std::vector<std::string>>
	{
		const auto blocks = suggestion_blocks(aComment);
		if (blocks.size() != 1U) {
			return std::nullopt;
		}
		auto lines = std::vector<std::string>{ };
		auto text  = blocks.front();
		while (!text.empty()) {
			const auto end  = text.find('\n');
			auto       line = text.substr(0U, end);
			if (line.ends_with('\r')) {
				line.remove_suffix(1U);
			}
			lines.emplace_back(line);
			text = end == std::string_view::npos ? std::string_view{ } : text.substr(end + 1U);
		}
		return lines;
	}

	auto decide_agent_suggestion_range(const std::string_view aComment, const std::uint32_t aFirst, const std::uint32_t aLast, const std::span<const std::string> aHead, const std::uint32_t aHeadFirst,
									   const std::uint32_t aHeadLineCount) -> agent_suggestion_range
	{
		auto       result = agent_suggestion_range{ .Reading = agent_suggestion_range_reading::no_suggestion, .First = aFirst, .Last = aLast };
		const auto block  = single_suggestion_lines(aComment);
		if (!block) {
			return result;
		}
		result.SuggestedLines = static_cast<std::uint32_t>(std::min<std::size_t>(block->size(), 0xFFFF'FFFFU));
		const auto head       = head_window{ .Lines = aHead, .First = aHeadFirst, .Count = aHeadLineCount };
		if (aFirst == 0U || aLast < aFirst || !head.holds(aFirst) || !head.holds(aLast)) {
			result.Reading = agent_suggestion_range_reading::outside_document;
			return result;
		}
		const auto stated = aLast - aFirst + 1U;
		if (result.SuggestedLines <= stated) {
			result.Reading = agent_suggestion_range_reading::kept;
			return result;
		}
		if (result.SuggestedLines > maximum_checked_suggestion_lines) {
			result.Reading = agent_suggestion_range_reading::unverified;
			return result;
		}
		const auto statedPrefix = prefix_from(*block, head, aFirst);
		const auto statedSuffix = suffix_to(*block, head, aLast);
		const auto statedFit    = fit_of(aFirst, aLast, statedPrefix, statedSuffix, result.SuggestedLines);
		// Verbatim lines beside the agent's first; then a block that
		// rewrites, line for line, as many lines as it holds.
		auto widened = best_window(*block, head, aFirst, aLast, statedFit.kept());
		if (!widened) {
			widened = rewritten_window(*block, head, aFirst, aLast);
		}
		if (!widened) {
			// A block that keeps every one of the agent's lines at its start
			// or end only adds lines after or before them: the range is right.
			result.Reading = statedPrefix >= stated || statedSuffix >= stated ? agent_suggestion_range_reading::kept : agent_suggestion_range_reading::unverified;
			return result;
		}
		result.Reading = agent_suggestion_range_reading::widened;
		result.First   = widened->First;
		result.Last    = widened->Last;
		return result;
	}

	auto decide_agent_suggestion_range(const std::string_view aComment, const std::uint32_t aFirst, const std::uint32_t aLast, const decoded_text_document& aHead) -> agent_suggestion_range
	{
		const auto count      = static_cast<std::uint32_t>(std::min<std::size_t>(aHead.Lines.size(), 0xFFFF'FFFFU));
		const auto block      = single_suggestion_lines(aComment);
		const auto blockLines = block ? static_cast<std::uint32_t>(std::min<std::size_t>(block->size(), maximum_checked_suggestion_lines)) : 0U;
		if (aFirst == 0U || aLast < aFirst || aLast > count) {
			return decide_agent_suggestion_range(aComment, aFirst, aLast, { }, 1U, count);
		}
		const auto from  = aFirst > blockLines ? aFirst - blockLines : 1U;
		const auto to    = std::min(count, aLast + blockLines);
		const auto lines = suggestion_source_lines(aHead, from - 1U, to - 1U);
		if (!lines) {
			return decide_agent_suggestion_range(aComment, aFirst, aLast, { }, 1U, count);
		}
		return decide_agent_suggestion_range(aComment, aFirst, aLast, *lines, from, count);
	}
} // namespace bpr::core
