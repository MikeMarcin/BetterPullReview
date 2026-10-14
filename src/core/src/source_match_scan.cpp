#include "source_matcher_internal.hpp"

#include <algorithm>
#include <new>
#include <optional>

namespace bpr::core::source_matching {
	namespace {
		struct newline_extra
		{
			std::size_t LogicalAfter;
			std::size_t OriginalAfter;
		};
		struct logical_document
		{
			std::string                Text;
			std::vector<newline_extra> Extras;
			auto                       original(std::uint64_t aOffset) const -> std::uint64_t
			{
				return aOffset + static_cast<std::uint64_t>(std::ranges::upper_bound(Extras, aOffset, { }, &newline_extra::LogicalAfter) - Extras.begin());
			}
			auto logical(std::uint64_t aOffset) const -> std::uint64_t
			{
				return aOffset - static_cast<std::uint64_t>(std::ranges::upper_bound(Extras, aOffset, { }, &newline_extra::OriginalAfter) - Extras.begin());
			}
		};
		auto boundary(std::string_view aText, std::uint64_t aOffset) -> bool
		{
			if (aOffset > aText.size()) {
				return false;
			}
			if (aOffset == aText.size()) {
				return true;
			}
			if ((static_cast<unsigned char>(aText[static_cast<std::size_t>(aOffset)]) & 0xC0U) == 0x80U) {
				return false;
			}
			return aOffset == 0U || aText[static_cast<std::size_t>(aOffset) - 1U] != '\r' || aText[static_cast<std::size_t>(aOffset)] != '\n';
		}
		auto prepare_document(std::string_view aSource, const source_match_limits& aLimits, budget& aWork) -> logical_document
		{
			if (aSource.size() > aLimits.MaximumBytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			auto result = logical_document{ };
			result.Text.reserve(aSource.size());
			result.Extras.reserve(std::min(aLimits.MaximumLines, aSource.size()));
			auto lines = aSource.empty() ? 0U : 1U;
			for (auto offset = std::size_t{ 0U }; offset < aSource.size();) {
				const auto begin = offset;
				const auto value = scalar(aSource, offset);
				aWork.tick();
				// [[BPR-1432]]. CRLF reads as LF; a lone CR is content, as Git counts lines.
				if (value == U'\n' || (value == U'\r' && offset < aSource.size() && aSource[offset] == '\n')) {
					result.Text.push_back('\n');
					if (value == U'\r') {
						++offset;
						result.Extras.push_back({ result.Text.size(), offset });
					}
					if (offset < aSource.size() && ++lines > aLimits.MaximumLines) {
						throw stopped{ source_search_status::resource_limit };
					}
				} else {
					result.Text.append(aSource.substr(begin, offset - begin));
				}
			}
			return result;
		}
		/// \brief Whole Word over the actual document: Unicode letters, marks,
		/// numbers and underscore are word scalars; neighbours outside fixed
		/// bounds still count.
		class word_rule
		{
		public:
			word_rule(const source_match_program::implementation& aProgram, std::string_view aText, budget& aWork)
				: Program(aProgram)
				, Text(aText)
				, Work(aWork)
			{ }
			[[nodiscard]] auto word_at(std::size_t aOffset) const -> bool
			{
				if (aOffset >= Text.size()) {
					return false;
				}
				auto end = aOffset;
				(void)scalar(Text, end);
				Work.poll();
				return Program.Word->find(Text, aOffset, end, true).has_value();
			}
			[[nodiscard]] auto word_before(std::size_t aOffset) const -> bool { return aOffset != 0U && word_at(previous_start(Text, aOffset)); }
			[[nodiscard]] auto whole(byte_span aSpan) const -> bool { return !word_before(static_cast<std::size_t>(aSpan.Begin)) && !word_at(static_cast<std::size_t>(aSpan.End)); }
			/// \brief The first start after `aStart`, by `aEnd`, whose previous scalar is not a word scalar.
			[[nodiscard]] auto next_start(std::size_t aStart, std::size_t aEnd) const -> std::optional<std::size_t>
			{
				if (aStart >= aEnd) {
					return std::nullopt;
				}
				auto after = aStart;
				(void)scalar(Text, after);
				if (!word_at(aStart)) {
					return after;
				}
				Work.poll();
				const auto other = Program.NonWord->find(Text, after, aEnd, false);
				if (!other) {
					return std::nullopt;
				}
				auto next = static_cast<std::size_t>(other->Begin);
				(void)scalar(Text, next);
				return next;
			}

		private:
			const source_match_program::implementation& Program;
			std::string_view                            Text;
			budget&                                     Work;
		};
		auto append(source_match_scan& aResult, byte_span aLogical, const logical_document& aDocument, const source_match_limits& aLimits) -> bool
		{
			if (aResult.Matches.size() == aLimits.MaximumMatches) {
				aResult.AdditionalMatchObserved = true;
				aResult.Status                  = source_search_status::result_limit;
				return false;
			}
			if (aResult.Matches.size() == aResult.Matches.capacity()) {
				aResult.Status = source_search_status::resource_limit;
				return false;
			}
			aResult.Matches.push_back({ aDocument.original(aLogical.Begin), aDocument.original(aLogical.End) });
			return true;
		}
		/// \brief Nonoverlapping leftmost-first matches; a zero-width match
		/// advances one scalar, and the range's end is a terminal only once.
		/// \details [[BPR-1431]]. Whole Word filters each engine match by its
		/// actual neighbours. A refused match resumes the search at the next
		/// start whose previous scalar is not a word scalar, so a refused earlier
		/// start can never hide a later whole word.
		void scan_range(const source_match_program::implementation& aProgram, const logical_document& aDocument, byte_span aBounds, source_match_scan& aResult, budget& aWork, bool aIncludeEnd)
		{
			const auto end    = static_cast<std::size_t>(aBounds.End);
			const auto words  = word_rule{ aProgram, aDocument.Text, aWork };
			auto       cursor = static_cast<std::size_t>(aBounds.Begin);
			while (cursor <= end) {
				aWork.poll();
				const auto match = aProgram.Query->find(aDocument.Text, cursor, end, false);
				if (!match || match->Begin < cursor || match->End > end || !boundary(aDocument.Text, match->Begin) || !boundary(aDocument.Text, match->End)) {
					return;
				}
				if (aProgram.Options.WholeWord && !words.whole(*match)) {
					const auto next = words.next_start(static_cast<std::size_t>(match->Begin), end);
					if (!next) {
						return;
					}
					cursor = *next;
					continue;
				}
				if ((match->empty() && match->End == end && !aIncludeEnd) || !append(aResult, *match, aDocument, aProgram.Limits)) {
					return;
				}
				cursor = static_cast<std::size_t>(match->End);
				if (match->empty()) {
					if (cursor == end) {
						return;
					}
					(void)scalar(aDocument.Text, cursor);
				}
			}
		}
	}
	auto scan_with_budget(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds, budget& aWork, std::span<const byte_span> aRanges) -> source_match_scan
	{
		const auto& program = source_match_access::get(aProgram);
		auto        result  = source_match_scan{ };
		const auto  initial = aWork.Steps;
		try {
			const auto bounds = aBounds.value_or(byte_span{ 0U, aSource.size() });
			if (bounds.Begin > bounds.End || !boundary(aSource, bounds.Begin) || !boundary(aSource, bounds.End)) {
				throw stopped{ source_search_status::invalid_input };
			}
			const auto capacity    = std::min(static_cast<std::size_t>(program.Limits.MaximumMatches), program.Limits.MaximumResultBytes / sizeof(byte_span));
			const auto resultBytes = capacity * sizeof(byte_span);
			const auto owned       = program.OwnedBytes + aSource.size() + 1U + program.Limits.MaximumLines * sizeof(newline_extra) + resultBytes;
			if (owned > program.Limits.MaximumOwnedBytes || resultBytes > program.Limits.MaximumResultBytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			result.PeakOwnedBytes = owned;
			result.Matches.reserve(capacity);
			const auto document = prepare_document(aSource, program.Limits, aWork);
			const auto scan     = [&](byte_span aRange) {
				if (aRange.Begin > aRange.End || !boundary(aSource, aRange.Begin) || !boundary(aSource, aRange.End)) {
					throw stopped{ source_search_status::invalid_input };
				}
				aRange.Begin = std::max(aRange.Begin, bounds.Begin);
				aRange.End   = std::min(aRange.End, bounds.End);
				if (aRange.Begin > aRange.End) {
					return;
				}
				scan_range(program, document, byte_span{ document.logical(aRange.Begin), document.logical(aRange.End) }, result, aWork, aRange.End == bounds.End);
			};
			if (aRanges.empty()) {
				scan(bounds);
			} else {
				for (const auto& range : aRanges) {
					scan(range);
					if (result.Status != source_search_status::complete) {
						break;
					}
				}
			}

		} catch (const stopped& stop) {
			result.Status = stop.Status;
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		result.Steps = aWork.Steps - initial;
		return result;
	}
}
namespace bpr::core {
	auto scan_source_match(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds, diff_compute_stop* aStop) -> source_match_scan
	{
		try {
			auto work = source_matching::budget{ source_match_access::get(aProgram).Limits.MaximumScanSteps, aStop };
			return source_matching::scan_with_budget(aProgram, aSource, aBounds, work);
		} catch (const source_matching::stopped& stop) {
			auto result   = source_match_scan{ };
			result.Status = stop.Status;
			return result;
		}
	}
	namespace {
		/// The preview's byte bounds in the source, or nothing for a malformed span.
		auto preview_bounds(std::string_view aSource, byte_span aSpan) -> std::optional<byte_span>
		{
			if (aSpan.Begin > aSpan.End || aSpan.End > aSource.size()) {
				return std::nullopt;
			}
			if ((aSpan.Begin < aSource.size() && (static_cast<unsigned char>(aSource[static_cast<std::size_t>(aSpan.Begin)]) & 0xC0U) == 0x80U)
				|| (aSpan.End < aSource.size() && (static_cast<unsigned char>(aSource[static_cast<std::size_t>(aSpan.End)]) & 0xC0U) == 0x80U)) {
				return std::nullopt;
			}
			try {
				auto begin = static_cast<std::size_t>(aSpan.Begin);
				for (auto count = 0U; count < 80U && begin > 0U; ++count) {
					if (aSource[begin - 1U] == '\n') {
						break;
					}
					begin = source_matching::previous_start(aSource, begin);
				}
				auto end = begin;
				for (auto count = 0U; count < 240U && end < aSource.size(); ++count) {
					(void)source_matching::scalar(aSource, end);
				}
				return byte_span{ begin, end };
			} catch (const source_matching::stopped&) {
				return std::nullopt;
			}
		}
	}
	auto source_match_preview(std::string_view aSource, byte_span aSpan) -> std::string
	{
		const auto bounds = preview_bounds(aSource, aSpan);
		return bounds ? std::string{ aSource.substr(static_cast<std::size_t>(bounds->Begin), static_cast<std::size_t>(bounds->End - bounds->Begin)) } : std::string{ };
	}
	auto source_match_preview_span(std::string_view aSource, byte_span aSpan) -> byte_span
	{
		const auto bounds = preview_bounds(aSource, aSpan);
		if (!bounds || bounds->End == bounds->Begin) {
			return { };
		}
		const auto begin = std::min(aSpan.Begin, bounds->End) - bounds->Begin;
		const auto end   = std::min(aSpan.End, bounds->End) - bounds->Begin;
		return { begin, end };
	}
}
