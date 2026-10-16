#include "diff_intraline.hpp"

#include "bpr/core/utf8.hpp"

#include <dtl/dtl.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// [[BPR-1481]] One of the two core sources that name dtl, with the Review
// Cycle suggestion hunks (architecture_boundaries).
namespace bpr::core::detail {
	namespace {
		static_assert(intraline_dtl_cells_limit < dtl::MAX_CORDINATES_SIZE,
					  "dtl must never reach its restart past MAX_CORDINATES_SIZE path points");

		/// Element comparisons between two step increments, so the meter's
		/// stop check stays out of dtl's inner loop.
		inline constexpr std::uint64_t comparison_batch = 4'096U;

		/// What the comparator shares with its caller: dtl copies the
		/// comparator, so its counts live here.
		struct comparison_state
		{
			diff_step_meter* Meter   = nullptr;
			std::uint64_t    Pending = 0U;
			bool             Stopped = false;
		};

		/// Counts dtl's comparisons through the engine's meter. Once a stop is
		/// requested, every pair compares equal: dtl's snakes then run to the
		/// grid's edge and it finishes in one more pass, with a result that is
		/// dropped. The stop is caught here, so it never unwinds through dtl's
		/// plain `new[]`.
		class metered_compare final : public dtl::Compare<std::uint32_t>
		{
		public:
			metered_compare() = default;

			explicit metered_compare(comparison_state* const aState) noexcept
				: State{ aState }
			{
			}

			[[nodiscard]] auto impl(const std::uint32_t& aLeft, const std::uint32_t& aRight) const -> bool override
			{
				if (State->Stopped) {
					return true;
				}
				if (++State->Pending >= comparison_batch) {
					const auto pending = State->Pending;
					State->Pending     = 0U;
					try {
						State->Meter->tick(pending);
					} catch (const diff_compute_stopped&) {
						State->Stopped = true;
						return true;
					}
				}
				return aLeft == aRight;
			}

		private:
			comparison_state* State = nullptr;
		};

		/// [[BPR-1537]] How the tokeniser sees one scalar.
		enum class scalar_class : std::uint8_t {
			space,
			upper,
			lower,
			digit,
			connector,
			other,
		};

		[[nodiscard]] auto classify(const char32_t aValue) noexcept -> scalar_class
		{
			if (aValue < 0x80U) {
				if (aValue >= U'A' && aValue <= U'Z') {
					return scalar_class::upper;
				}
				if (aValue >= U'a' && aValue <= U'z') {
					return scalar_class::lower;
				}
				if (aValue >= U'0' && aValue <= U'9') {
					return scalar_class::digit;
				}
				if (aValue == U'_') {
					return scalar_class::connector;
				}
			}
			switch (utf8::classify_word(aValue)) {
			case utf8::word_class::space: return scalar_class::space;
			case utf8::word_class::word:  return aValue < 0x80U ? scalar_class::other : scalar_class::lower;
			case utf8::word_class::other: return scalar_class::other;
			}
			return scalar_class::other;
		}

		/// One scalar of a line being tokenised: its class, where it ends,
		/// and whether it is a wide (East Asian) character, which is a token
		/// of its own.
		struct scanned_scalar
		{
			scalar_class Class = scalar_class::other;
			std::size_t  End   = 0U;
			bool         Wide  = false;
		};

		[[nodiscard]] auto scan(const std::string_view aLine, const std::size_t aOffset) -> std::optional<scanned_scalar>
		{
			auto       end   = aOffset;
			const auto value = utf8::decode(aLine, end);
			if (!value) {
				return std::nullopt;
			}
			auto result = scanned_scalar{ .Class = classify(*value), .End = end };
			if (result.Class == scalar_class::lower && *value >= 0x80U) {
				const auto cluster = utf8::grapheme_end(aLine, aOffset);
				result.Wide        = utf8::grapheme_width(aLine.substr(aOffset, cluster - aOffset)) == 2U;
			}
			return result;
		}

		/// Scalars in bytes [aBegin, aEnd) of a well-formed line.
		[[nodiscard]] auto count_scalars(const std::string_view aLine, const std::size_t aBegin, const std::size_t aEnd) -> std::uint64_t
		{
			auto count = std::uint64_t{ 0U };
			for (auto offset = aBegin; offset < aEnd;) {
				const auto before = offset;
				if (!utf8::decode(aLine, offset)) {
					offset = before + 1U;
				}
				++count;
			}
			return count;
		}

		/// Whether bytes [aBegin, aEnd) are one ASCII operator character.
		[[nodiscard]] auto operator_scalar(const std::string_view aLine, const std::size_t aBegin, const std::size_t aEnd) noexcept -> bool
		{
			constexpr auto operators = std::string_view{ "=!<>&|+-*/%^~:?" };
			return aEnd == aBegin + 1U && aEnd <= aLine.size() && operators.find(aLine[aBegin]) != std::string_view::npos;
		}

		[[nodiscard]] constexpr auto joins_words(const intraline_token_kind aKind) noexcept -> bool
		{
			return aKind == intraline_token_kind::word || aKind == intraline_token_kind::connector;
		}

		/// [[BPR-1537]] One step of the edit script, in token indices of both
		/// lines: an equal run, or a change replacing [LeftBegin, LeftEnd)
		/// with [RightBegin, RightEnd).
		struct script_step
		{
			bool        Equal      = false;
			std::size_t LeftBegin  = 0U;
			std::size_t LeftEnd    = 0U;
			std::size_t RightBegin = 0U;
			std::size_t RightEnd   = 0U;
		};

		/// The scalars of each prefix of a line's tokens, so a run's length is
		/// one subtraction.
		[[nodiscard]] auto prefix_scalars(const std::vector<intraline_token>& aTokens, diff_step_meter& aMeter) -> std::vector<std::uint64_t>
		{
			auto result = std::vector<std::uint64_t>(aTokens.size() + 1U, 0U);
			for (std::size_t index = 0U; index < aTokens.size(); ++index) {
				result[index + 1U] = result[index] + aTokens[index].Scalars;
				aMeter.tick(1U);
			}
			return result;
		}

		/// Whether tokens [aBegin, aEnd) sit inside one word: word tokens
		/// touch the run on both sides, and the run starts and ends with one.
		[[nodiscard]] auto inside_word(const std::vector<intraline_token>& aTokens, const std::size_t aBegin, const std::size_t aEnd) noexcept -> bool
		{
			return aBegin > 0U && aEnd < aTokens.size() && aBegin < aEnd && joins_words(aTokens[aBegin - 1U].Kind) && joins_words(aTokens[aBegin].Kind)
				&& joins_words(aTokens[aEnd - 1U].Kind) && joins_words(aTokens[aEnd].Kind);
		}

		/// Counts aSteps steps of work already done, in increments no larger
		/// than the engine's polls expect, so one long line never delays a stop.
		auto count_steps(diff_step_meter& aMeter, std::uint64_t aSteps) -> void
		{
			constexpr auto increment = std::uint64_t{ 1'024U };
			for (; aSteps > increment; aSteps -= increment) {
				aMeter.tick(increment);
			}
			aMeter.tick(aSteps);
		}

		/// The fragment of a word an equality may give to a neighbouring
		/// change: at most this many scalars.
		inline constexpr std::uint64_t word_fragment_scalars = 2U;
	} // namespace

	auto intraline_tokens(const std::string_view aLine) -> std::vector<intraline_token>
	{
		auto result = std::vector<intraline_token>{ };
		auto offset = std::size_t{ 0U };
		while (offset < aLine.size()) {
			const auto begin   = offset;
			const auto current = scan(aLine, offset);
			if (!current) {
				// Never for a validated line; a stray byte is a token of its own.
				result.push_back(intraline_token{ .Begin = begin, .End = begin + 1U, .Scalars = 1U, .Kind = intraline_token_kind::other });
				offset = begin + 1U;
				continue;
			}
			const auto same = [&aLine](std::size_t& aOffset, const scalar_class aClass) {
				while (aOffset < aLine.size()) {
					const auto next = scan(aLine, aOffset);
					if (!next || next->Class != aClass || next->Wide) {
						return;
					}
					aOffset = next->End;
				}
			};
			auto kind = intraline_token_kind::word;
			offset    = current->End;
			switch (current->Class) {
			case scalar_class::space:
				kind = intraline_token_kind::space;
				same(offset, scalar_class::space);
				break;
			case scalar_class::digit: same(offset, scalar_class::digit); break;
			case scalar_class::lower:
				if (current->Wide) {
					kind   = intraline_token_kind::other;
					offset = utf8::grapheme_end(aLine, begin);
				} else {
					same(offset, scalar_class::lower);
				}
				break;
			case scalar_class::upper: {
				// "Word" is one part; "HTTPServer" is "HTTP" and "Server".
				auto last = begin;
				while (offset < aLine.size()) {
					const auto next = scan(aLine, offset);
					if (!next || next->Class != scalar_class::upper) {
						break;
					}
					last   = offset;
					offset = next->End;
				}
				const auto next = offset < aLine.size() ? scan(aLine, offset) : std::nullopt;
				if (next && next->Class == scalar_class::lower && !next->Wide) {
					if (last == begin) {
						same(offset, scalar_class::lower);
					} else {
						offset = last;
					}
				}
				break;
			}
			case scalar_class::connector: kind = intraline_token_kind::connector; break;
			case scalar_class::other:
				kind   = intraline_token_kind::other;
				offset = utf8::grapheme_end(aLine, begin);
				// An operator is one token: "!=" beside "==", "<=" beside "<".
				if (operator_scalar(aLine, begin, offset)) {
					while (offset < aLine.size() && operator_scalar(aLine, offset, utf8::grapheme_end(aLine, offset))) {
						++offset;
					}
				}
				break;
			}
			result.push_back(intraline_token{ .Begin = begin, .End = offset, .Scalars = count_scalars(aLine, begin, offset), .Kind = kind });
		}
		return result;
	}

	auto prepare_intraline_pair(const std::string_view aLeft, const std::string_view aRight, diff_step_meter& aMeter) -> intraline_pair
	{
		auto result = intraline_pair{ .Left = aLeft, .Right = aRight, .LeftTokens = intraline_tokens(aLeft), .RightTokens = intraline_tokens(aRight) };
		// Equal tokens get equal values, numbered in order of first appearance.
		auto       values = std::map<std::string_view, std::uint32_t>{ };
		const auto number = [&values, &aMeter](const std::string_view aLine, const std::vector<intraline_token>& aTokens) {
			auto numbered = std::vector<std::uint32_t>{ };
			numbered.reserve(aTokens.size());
			for (const auto& token : aTokens) {
				const auto text = aLine.substr(static_cast<std::size_t>(token.Begin), static_cast<std::size_t>(token.End - token.Begin));
				numbered.push_back(values.try_emplace(text, static_cast<std::uint32_t>(values.size())).first->second);
				// Its share of tokenising, then numbering it.
				aMeter.tick(16U + 4U * text.size());
			}
			return numbered;
		};
		result.LeftValues  = number(aLeft, result.LeftTokens);
		result.RightValues = number(aRight, result.RightTokens);
		const auto shorter = std::min(result.LeftValues.size(), result.RightValues.size());
		while (result.Prefix < shorter && result.LeftValues[result.Prefix] == result.RightValues[result.Prefix]) {
			++result.Prefix;
			aMeter.tick(1U);
		}
		while (result.Suffix < shorter - result.Prefix
			   && result.LeftValues[result.LeftValues.size() - 1U - result.Suffix] == result.RightValues[result.RightValues.size() - 1U - result.Suffix]) {
			++result.Suffix;
			aMeter.tick(1U);
		}
		return result;
	}

	auto intraline_common_elements(const std::span<const std::uint32_t> aLeft,
								   const std::span<const std::uint32_t> aRight,
								   diff_step_meter&                     aMeter)
		-> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
	{
		if (aLeft.empty() || aRight.empty()) {
			return std::vector<std::pair<std::size_t, std::size_t>>{ };
		}
		const auto rows    = static_cast<std::uint64_t>(aLeft.size()) + 1U;
		const auto columns = static_cast<std::uint64_t>(aRight.size()) + 1U;
		if (rows > intraline_dtl_cells_limit || columns > intraline_dtl_cells_limit / rows) {
			return std::nullopt;
		}
		using sequence = std::vector<std::uint32_t>;
		auto state     = comparison_state{ .Meter = &aMeter };
		auto diff      = dtl::Diff<std::uint32_t, sequence, metered_compare>{
			sequence{ aLeft.begin(), aLeft.end() },
			sequence{ aRight.begin(), aRight.end() },
			metered_compare{ &state },
		};
		diff.compose();
		if (state.Stopped) {
			throw diff_compute_stopped{ };
		}
		count_steps(aMeter, state.Pending + aLeft.size() + aRight.size());
		const auto script = diff.getSes().getSequence();
		auto       result = std::vector<std::pair<std::size_t, std::size_t>>{ };
		for (const auto& [value, info] : script) {
			static_cast<void>(value);
			if (info.type != dtl::SES_COMMON) {
				continue;
			}
			// A defect in dtl must never become a span: a common element out
			// of order, out of range or unequal gives the pair the matrix table.
			const auto left  = static_cast<std::size_t>(info.beforeIdx - 1);
			const auto right = static_cast<std::size_t>(info.afterIdx - 1);
			const auto after = result.empty() || (left > result.back().first && right > result.back().second);
			if (info.beforeIdx < 1 || info.afterIdx < 1 || left >= aLeft.size() || right >= aRight.size() || !after || aLeft[left] != aRight[right]) {
				return std::nullopt;
			}
			result.emplace_back(left, right);
			aMeter.tick(1U);
		}
		return result;
	}

	auto intraline_regions(const intraline_pair&                                      aPair,
						   const std::span<const std::pair<std::size_t, std::size_t>> aMiddleMatches,
						   diff_step_meter&                                           aMeter) -> std::vector<intraline_region>
	{
		const auto& leftTokens  = aPair.LeftTokens;
		const auto& rightTokens = aPair.RightTokens;
		// Every loop below counts a step per token or script step it visits.

		// The whole lines' common tokens: the prefix, dtl's middle, the suffix.
		auto matches = std::vector<std::pair<std::size_t, std::size_t>>{ };
		matches.reserve(aPair.Prefix + aMiddleMatches.size() + aPair.Suffix);
		for (std::size_t index = 0U; index < aPair.Prefix; ++index) {
			matches.emplace_back(index, index);
			aMeter.tick(1U);
		}
		for (const auto& [left, right] : aMiddleMatches) {
			matches.emplace_back(aPair.Prefix + left, aPair.Prefix + right);
			aMeter.tick(1U);
		}
		for (std::size_t index = aPair.Suffix; index > 0U; --index) {
			matches.emplace_back(leftTokens.size() - index, rightTokens.size() - index);
			aMeter.tick(1U);
		}

		// The edit script: equal runs and the changes between them.
		auto script = std::vector<script_step>{ };
		{
			auto left  = std::size_t{ 0U };
			auto right = std::size_t{ 0U };
			for (std::size_t index = 0U; index < matches.size();) {
				const auto [first, second] = matches[index];
				if (first != left || second != right) {
					script.push_back({ .Equal = false, .LeftBegin = left, .LeftEnd = first, .RightBegin = right, .RightEnd = second });
				}
				auto last = index;
				while (last + 1U < matches.size() && matches[last + 1U].first == matches[last].first + 1U && matches[last + 1U].second == matches[last].second + 1U) {
					++last;
					aMeter.tick(1U);
				}
				left  = matches[last].first + 1U;
				right = matches[last].second + 1U;
				script.push_back({ .Equal = true, .LeftBegin = first, .LeftEnd = left, .RightBegin = second, .RightEnd = right });
				index = last + 1U;
			}
			if (left != leftTokens.size() || right != rightTokens.size()) {
				script.push_back({ .Equal = false, .LeftBegin = left, .LeftEnd = leftTokens.size(), .RightBegin = right, .RightEnd = rightTokens.size() });
			}
		}

		const auto leftScalars  = prefix_scalars(leftTokens, aMeter);
		const auto rightScalars = prefix_scalars(rightTokens, aMeter);
		const auto leftLength   = [&leftScalars](const script_step& aStep) { return leftScalars[aStep.LeftEnd] - leftScalars[aStep.LeftBegin]; };
		const auto rightLength  = [&rightScalars](const script_step& aStep) { return rightScalars[aStep.RightEnd] - rightScalars[aStep.RightBegin]; };
		const auto larger       = [&](const script_step& aStep) { return std::max(leftLength(aStep), rightLength(aStep)); };

		// Semantic cleanup: an equality between two changes joins them when it
		// is no longer than the larger side of either change, as
		// diff-match-patch's semantic cleanup decides, or when it lies inside
		// one word on both lines, and both lines change beside it. Each join
		// can only enlarge the change at the end, so checking backwards from
		// there keeps the whole script clean.
		auto cleaned = std::vector<script_step>{ };
		cleaned.reserve(script.size());
		for (const auto& step : script) {
			cleaned.push_back(step);
			aMeter.tick(1U);
			while (cleaned.size() >= 3U) {
				const auto& next     = cleaned[cleaned.size() - 1U];
				const auto& equal    = cleaned[cleaned.size() - 2U];
				const auto& previous = cleaned[cleaned.size() - 3U];
				if (next.Equal || !equal.Equal || previous.Equal) {
					break;
				}
				// Only when both lines change beside it: insertions around a
				// word on one line never mark that word on the other.
				const auto bothLines = (leftLength(previous) + leftLength(next) > 0U) && (rightLength(previous) + rightLength(next) > 0U);
				const auto length    = leftLength(equal);
				const auto brief     = length <= larger(previous) && length <= larger(next);
				const auto inside    = inside_word(leftTokens, equal.LeftBegin, equal.LeftEnd) && inside_word(rightTokens, equal.RightBegin, equal.RightEnd);
				if (!bothLines || (!brief && !inside)) {
					break;
				}
				const auto joined = script_step{ .Equal = false, .LeftBegin = previous.LeftBegin, .LeftEnd = next.LeftEnd, .RightBegin = previous.RightBegin, .RightEnd = next.RightEnd };
				cleaned.resize(cleaned.size() - 3U);
				cleaned.push_back(joined);
				aMeter.tick(1U);
			}
		}

		// A change that replaces part of a word on both lines takes the rest of
		// the word when the rest is at most two scalars ("6dc2…" beside
		// "6e0c…", "size_t" beside "uint64_t"), so a word is not split for a
		// letter or two.
		for (std::size_t index = 0U; index < cleaned.size(); ++index) {
			auto& change = cleaned[index];
			aMeter.tick(1U);
			if (change.Equal) {
				continue;
			}
			// A word replaced on both lines, not one only extended.
			const auto replaced = change.LeftBegin < change.LeftEnd && change.RightBegin < change.RightEnd;
			if (index > 0U) {
				auto&      equal = cleaned[index - 1U];
				const auto glued = replaced && change.LeftBegin > equal.LeftBegin && joins_words(leftTokens[change.LeftBegin - 1U].Kind) && joins_words(leftTokens[change.LeftBegin].Kind)
					&& joins_words(rightTokens[change.RightBegin - 1U].Kind) && joins_words(rightTokens[change.RightBegin].Kind);
				auto count = std::size_t{ 0U };
				while (glued && equal.LeftEnd - count > equal.LeftBegin && joins_words(leftTokens[equal.LeftEnd - count - 1U].Kind)) {
					++count;
					aMeter.tick(1U);
				}
				if (count > 0U && leftScalars[equal.LeftEnd] - leftScalars[equal.LeftEnd - count] <= word_fragment_scalars) {
					equal.LeftEnd -= count;
					equal.RightEnd -= count;
					change.LeftBegin -= count;
					change.RightBegin -= count;
				}
			}
			if (index + 1U < cleaned.size()) {
				auto&      equal = cleaned[index + 1U];
				const auto glued = replaced && change.LeftEnd < equal.LeftEnd && joins_words(leftTokens[change.LeftEnd - 1U].Kind) && joins_words(leftTokens[change.LeftEnd].Kind)
					&& joins_words(rightTokens[change.RightEnd - 1U].Kind) && joins_words(rightTokens[change.RightEnd].Kind);
				auto count = std::size_t{ 0U };
				while (glued && equal.LeftBegin + count < equal.LeftEnd && joins_words(leftTokens[equal.LeftBegin + count].Kind)) {
					++count;
					aMeter.tick(1U);
				}
				if (count > 0U && leftScalars[equal.LeftBegin + count] - leftScalars[equal.LeftBegin] <= word_fragment_scalars) {
					equal.LeftBegin += count;
					equal.RightBegin += count;
					change.LeftEnd += count;
					change.RightEnd += count;
				}
			}
		}
		// Equalities given away entirely leave changes side by side: join them.
		auto changes = std::vector<script_step>{ };
		for (const auto& step : cleaned) {
			aMeter.tick(1U);
			if (step.Equal) {
				continue;
			}
			if (!changes.empty() && changes.back().LeftEnd == step.LeftBegin && changes.back().RightEnd == step.RightBegin) {
				changes.back().LeftEnd  = step.LeftEnd;
				changes.back().RightEnd = step.RightEnd;
			} else {
				changes.push_back(step);
			}
		}

		const auto leftAt  = [&](const std::size_t aToken) { return aToken < leftTokens.size() ? leftTokens[aToken].Begin : static_cast<std::uint64_t>(aPair.Left.size()); };
		const auto rightAt = [&](const std::size_t aToken) { return aToken < rightTokens.size() ? rightTokens[aToken].Begin : static_cast<std::uint64_t>(aPair.Right.size()); };
		auto       result  = std::vector<intraline_region>{ };
		if (changes.empty()) {
			return result;
		}

		// A rewritten line: when the words the lines keep are under
		// intraline_rewrite_permille of the shorter line's non-space scalars,
		// a few shared words would only scatter the emphasis, so it runs from
		// the first change to the last.
		const auto nonSpace = [&aMeter](const std::vector<intraline_token>& aTokens, const std::size_t aBegin, const std::size_t aEnd) {
			auto count = std::uint64_t{ 0U };
			for (auto index = aBegin; index < aEnd; ++index) {
				count += aTokens[index].Kind == intraline_token_kind::space ? 0U : aTokens[index].Scalars;
				aMeter.tick(1U);
			}
			return count;
		};
		auto kept = std::uint64_t{ 0U };
		for (const auto& step : cleaned) {
			if (step.Equal) {
				kept += nonSpace(leftTokens, step.LeftBegin, step.LeftEnd);
			}
		}
		const auto shorter = std::min(nonSpace(leftTokens, 0U, leftTokens.size()), nonSpace(rightTokens, 0U, rightTokens.size()));
		if (shorter > 0U && kept * 1'000U < intraline_rewrite_permille * shorter) {
			result.push_back({ .LeftBegin = leftAt(changes.front().LeftBegin), .LeftEnd = leftAt(changes.back().LeftEnd), .RightBegin = rightAt(changes.front().RightBegin), .RightEnd = rightAt(changes.back().RightEnd) });
			return result;
		}

		for (const auto& change : changes) {
			auto region = intraline_region{ .LeftBegin = leftAt(change.LeftBegin), .LeftEnd = leftAt(change.LeftEnd), .RightBegin = rightAt(change.RightBegin), .RightEnd = rightAt(change.RightEnd) };
			// Spacing that only grew or shrank is emphasised where it changed,
			// not across the whole run: spaces and tabs common to both ends of
			// the run are left plain.
			const auto spacing = nonSpace(leftTokens, change.LeftBegin, change.LeftEnd) == 0U && nonSpace(rightTokens, change.RightBegin, change.RightEnd) == 0U;
			if (spacing) {
				const auto blank = [](const char aByte) { return aByte == ' ' || aByte == '\t'; };
				while (region.LeftBegin < region.LeftEnd && region.RightBegin < region.RightEnd && aPair.Left[region.LeftBegin] == aPair.Right[region.RightBegin] && blank(aPair.Left[region.LeftBegin])) {
					++region.LeftBegin;
					++region.RightBegin;
					aMeter.tick(1U);
				}
				while (region.LeftBegin < region.LeftEnd && region.RightBegin < region.RightEnd && aPair.Left[region.LeftEnd - 1U] == aPair.Right[region.RightEnd - 1U] && blank(aPair.Left[region.LeftEnd - 1U])) {
					--region.LeftEnd;
					--region.RightEnd;
					aMeter.tick(1U);
				}
			}
			result.push_back(region);
		}
		return result;
	}
} // namespace bpr::core::detail
