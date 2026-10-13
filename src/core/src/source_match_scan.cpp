#include "source_matcher_internal.hpp"

#include <new>

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
				if (value == U'\r' || value == U'\n') {
					result.Text.push_back('\n');
					if (value == U'\r' && offset < aSource.size() && aSource[offset] == '\n') {
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
		auto in_class(const character_class& aClass, char32_t aValue, budget& aWork) -> bool
		{
			const auto contains = [&](char32_t aScalar) {
				aWork.tick();
				if ((aClass.Categories & (1U << category(aScalar))) != 0U) {
					return true;
				}
				for (const auto& range : aClass.Ranges) {
					aWork.tick();
					if (aScalar >= range.Begin && aScalar <= range.End) {
						return true;
					}
				}
				return false;
			};
			auto matched = contains(aValue);
			if (aClass.Fold && !matched) {
				const auto folded = fold(aValue);
				matched           = contains(folded);
				auto it           = std::ranges::lower_bound(source_unicode::reverse_folds, folded, { }, &source_unicode::fold_pair::From);
				while (!matched && it != source_unicode::reverse_folds.end() && it->From == folded) {
					matched = contains(it->To);
					++it;
				}
			}
			return matched != aClass.Negated;
		}
		struct vm_thread
		{
			std::uint32_t Pc;
			std::uint64_t Begin;
		};
		class machine
		{
		public:
			machine(const source_match_program::implementation& aProgram, std::string_view aText, budget& aWork)
				: Program(aProgram)
				, Text(aText)
				, Work(aWork)
			{
				Current.reserve(Program.Code.size());
				Next.reserve(Program.Code.size());
				Stack.reserve(Program.Code.size() * 2U);
				Seen.resize(Program.Code.size());
			}
			auto next(std::size_t aBegin, std::size_t aEnd) -> std::optional<byte_span>
			{
				Current.clear();
				auto candidate = std::optional<byte_span>{ };
				for (auto position = aBegin;;) {
					++Generation;
					for (const auto& thread : Current) {
						Seen[thread.Pc] = Generation;
					}
					if (!candidate) {
						add(Current, { Program.Start, position }, position);
					}
					Next.clear();
					++Generation;
					auto       after = position;
					const auto value = position < aEnd ? scalar(Text, after) : char32_t{ 0U };
					for (const auto& thread : Current) {
						Work.tick();
						const auto& item = Program.Code[thread.Pc];
						if (item.Op == opcode::accept) {
							const auto span = byte_span{ thread.Begin, position };
							if (!Program.Options.WholeWord || whole_word(Text, span)) {
								candidate = span;
								break;
							}
							continue;
						}
						if (position == aEnd) {
							continue;
						}
						const auto matches = item.Op == opcode::character ? (item.Flag ? fold(value) : value) == item.Value
							: item.Op == opcode::any                      ? (item.Flag || value != U'\n')
																		  : item.Op == opcode::character_class && in_class(Program.Classes[static_cast<std::size_t>(item.Value)], value, Work);
						if (matches) {
							add(Next, { item.Next, thread.Begin }, after);
						}
					}
					if (candidate && Next.empty()) {
						return candidate;
					}
					if (position == aEnd) {
						return candidate;
					}
					Current.swap(Next);
					position = after;
				}
			}

		private:
			const source_match_program::implementation& Program;
			std::string_view                            Text;
			budget&                                     Work;
			std::vector<vm_thread>                      Current, Next, Stack;
			std::vector<std::uint64_t>                  Seen;
			std::uint64_t                               Generation = 0U;
			auto                                        assertion(const instruction& aItem, std::size_t aPosition) const -> bool
			{
				switch (aItem.Op) {
				case opcode::text_begin:   return aPosition == 0U;
				case opcode::text_end:     return aPosition == Text.size();
				case opcode::begin:        return aPosition == 0U || (aItem.Flag && previous(Text, aPosition) == U'\n');
				case opcode::end:          return aPosition == Text.size() || (aItem.Flag && following(Text, aPosition) == U'\n');
				case opcode::boundary:     return ascii_word(previous(Text, aPosition)) != ascii_word(following(Text, aPosition));
				case opcode::not_boundary: return ascii_word(previous(Text, aPosition)) == ascii_word(following(Text, aPosition));
				default:                   return false;
				}
			}
			void add(std::vector<vm_thread>& aList, vm_thread aThread, std::size_t aPosition)
			{
				Stack.clear();
				Stack.push_back(aThread);
				while (!Stack.empty()) {
					Work.tick();
					const auto thread = Stack.back();
					Stack.pop_back();
					if (Seen[thread.Pc] == Generation) {
						continue;
					}
					Seen[thread.Pc]  = Generation;
					const auto& item = Program.Code[thread.Pc];
					if (item.Op == opcode::split) {
						Stack.push_back({ item.Other, thread.Begin });
						Stack.push_back({ item.Next, thread.Begin });
					} else if (item.Op == opcode::character || item.Op == opcode::any || item.Op == opcode::character_class || item.Op == opcode::accept) {
						aList.push_back(thread);
					} else if (assertion(item, aPosition)) {
						Stack.push_back({ item.Next, thread.Begin });
					}
				}
			}
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
		void literal_scan(const source_match_program::implementation& aProgram, const logical_document& aDocument, byte_span aBounds, source_match_scan& aResult, budget& aWork)
		{
			auto prefix    = std::size_t{ 0U };
			auto ordinal   = std::size_t{ 0U };
			auto positions = std::vector<std::size_t>(aProgram.Literal.size(), 0U);
			for (auto offset = static_cast<std::size_t>(aBounds.Begin); offset < aBounds.End;) {
				const auto begin                      = offset;
				const auto value                      = scalar(aDocument.Text, offset);
				const auto folded                     = aProgram.Options.MatchCase ? value : fold(value);
				positions[ordinal % positions.size()] = begin;
				++ordinal;
				while (prefix > 0U && folded != aProgram.Literal[prefix]) {
					aWork.tick();
					prefix = aProgram.Prefix[prefix - 1U];
				}
				aWork.tick();
				if (folded == aProgram.Literal[prefix]) {
					++prefix;
				}
				if (prefix != aProgram.Literal.size()) {
					continue;
				}
				const auto span = byte_span{ positions[(ordinal - prefix) % positions.size()], offset };
				if (!aProgram.Options.WholeWord || whole_word(aDocument.Text, span)) {
					if (!append(aResult, span, aDocument, aProgram.Limits)) {
						return;
					}
					prefix = 0U;
				} else {
					prefix = aProgram.Prefix[prefix - 1U];
				}
			}
		}
		void regex_scan(const source_match_program::implementation& aProgram, const logical_document& aDocument, byte_span aBounds, source_match_scan& aResult, budget& aWork, bool aIncludeEnd)
		{
			auto vm     = machine{ aProgram, aDocument.Text, aWork };
			auto cursor = static_cast<std::size_t>(aBounds.Begin);
			while (cursor <= aBounds.End) {
				const auto match = vm.next(cursor, static_cast<std::size_t>(aBounds.End));
				if (!match || (match->empty() && match->End == aBounds.End && !aIncludeEnd) || !append(aResult, *match, aDocument, aProgram.Limits)) {
					return;
				}
				cursor = static_cast<std::size_t>(match->End);
				if (match->empty()) {
					if (cursor == aBounds.End) {
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
			const auto owned       = program.OwnedBytes + aSource.size() + 1U + program.Limits.MaximumLines * sizeof(newline_extra) + resultBytes + program.Limits.MaximumInstructions * 128U;
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
				const auto logical = byte_span{ document.logical(aRange.Begin), document.logical(aRange.End) };
				if (program.Options.RegularExpression) {
					regex_scan(program, document, logical, result, aWork, aRange.End == bounds.End);
				} else {
					literal_scan(program, document, logical, result, aWork);
				}
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
	auto source_match_preview(std::string_view aSource, byte_span aSpan) -> std::string
	{
		if (aSpan.Begin > aSpan.End || aSpan.End > aSource.size()) {
			return { };
		}
		if ((aSpan.Begin < aSource.size() && (static_cast<unsigned char>(aSource[static_cast<std::size_t>(aSpan.Begin)]) & 0xC0U) == 0x80U)
			|| (aSpan.End < aSource.size() && (static_cast<unsigned char>(aSource[static_cast<std::size_t>(aSpan.End)]) & 0xC0U) == 0x80U)) {
			return { };
		}
		try {
			auto begin = static_cast<std::size_t>(aSpan.Begin);
			for (auto count = 0U; count < 80U && begin > 0U; ++count) {
				if (aSource[begin - 1U] == '\n' || aSource[begin - 1U] == '\r') {
					break;
				}
				--begin;
				while ((static_cast<unsigned char>(aSource[begin]) & 0xC0U) == 0x80U) {
					--begin;
				}
			}
			auto end = begin;
			for (auto count = 0U; count < 240U && end < aSource.size(); ++count) {
				(void)source_matching::scalar(aSource, end);
			}
			return std::string{ aSource.substr(begin, end - begin) };
		} catch (const source_matching::stopped&) {
			return { };
		}
	}
}
