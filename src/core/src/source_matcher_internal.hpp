#ifndef BPR_SRC_CORE_SRC_SOURCE_MATCHER_INTERNAL_HPP
#define BPR_SRC_CORE_SRC_SOURCE_MATCHER_INTERNAL_HPP
#include "bpr/core/source_matcher.hpp"
#include "source_search_unicode_data.hpp"
#include <algorithm>
#include <stdexcept>
namespace bpr::core::source_matching {
	struct stopped
	{
		source_search_status Status;
		std::size_t          Offset = 0U;
	};
	struct budget
	{
		std::uint64_t      Steps = 0U;
		std::uint64_t      Maximum;
		diff_compute_stop* Stop;
		explicit budget(std::uint64_t aMaximum, diff_compute_stop* aStop)
			: Maximum(aMaximum)
			, Stop(aStop)
		{
			if (Stop && Stop->stop_requested(0U)) {
				throw stopped{ source_search_status::cancelled };
			}
		}
		void tick()
		{
			if (Steps >= Maximum) {
				throw stopped{ source_search_status::resource_limit };
			}
			++Steps;
			if ((Steps % 1024U) == 0U && Stop && Stop->stop_requested(Steps)) {
				throw stopped{ source_search_status::cancelled };
			}
		}
	};
	inline auto limits_valid(const source_match_limits& aLimits) -> bool
	{
		const auto upper = source_match_limits{ };
		return aLimits.MaximumBytes > 0U && aLimits.MaximumBytes <= upper.MaximumBytes
			&& aLimits.MaximumLines > 0U && aLimits.MaximumLines <= upper.MaximumLines
			&& aLimits.MaximumInstructions > 0U && aLimits.MaximumInstructions <= upper.MaximumInstructions
			&& aLimits.MaximumOwnedBytes > 0U && aLimits.MaximumOwnedBytes <= upper.MaximumOwnedBytes
			&& aLimits.MaximumResultBytes > 0U && aLimits.MaximumResultBytes <= upper.MaximumResultBytes
			&& aLimits.MaximumMatches > 0U && aLimits.MaximumMatches <= upper.MaximumMatches
			&& aLimits.MaximumCompileSteps > 0U && aLimits.MaximumCompileSteps <= upper.MaximumCompileSteps
			&& aLimits.MaximumScanSteps > 0U && aLimits.MaximumScanSteps <= upper.MaximumScanSteps;
	}
	inline auto scalar(std::string_view aText, std::size_t& aOffset) -> char32_t
	{
		const auto first = static_cast<unsigned char>(aText[aOffset++]);
		if (first < 0x80U) {
			if (first == 0U) {
				throw stopped{ source_search_status::invalid_input };
			}
			return first;
		}
		const auto length = first >= 0xF0U ? 4U : first >= 0xE0U ? 3U
																 : 2U;
		if (first < 0xC2U || first > 0xF4U || aOffset + length - 1U > aText.size()) {
			throw stopped{ source_search_status::invalid_input };
		}
		auto value = static_cast<char32_t>(first & (0x7FU >> length));
		for (auto index = 1U; index < length; ++index) {
			const auto next = static_cast<unsigned char>(aText[aOffset++]);
			if ((next & 0xC0U) != 0x80U) {
				throw stopped{ source_search_status::invalid_input };
			}
			value = (value << 6U) | (next & 0x3FU);
		}
		if (value < (length == 2U ? 0x80U : length == 3U ? 0x800U
														 : 0x10000U)
			|| value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
			throw stopped{ source_search_status::invalid_input };
		}
		return value;
	}
	inline auto fold(char32_t aValue) -> char32_t
	{
		const auto found = std::ranges::lower_bound(source_unicode::folds, aValue, { }, &source_unicode::fold_pair::From);
		return found != source_unicode::folds.end() && found->From == aValue ? found->To : aValue;
	}
	inline auto category(char32_t aValue) -> std::uint8_t
	{
		const auto found = std::ranges::upper_bound(source_unicode::categories, aValue, { }, &source_unicode::category_range::Begin);
		return found != source_unicode::categories.begin() && aValue <= std::prev(found)->End ? std::prev(found)->Category : 29U;
	}
	inline auto word(char32_t aValue) -> bool { return aValue == U'_' || category(aValue) <= 10U; }
	inline auto ascii_word(char32_t aValue) -> bool { return aValue == U'_' || (aValue >= U'a' && aValue <= U'z') || (aValue >= U'A' && aValue <= U'Z') || (aValue >= U'0' && aValue <= U'9'); }
	inline auto previous(std::string_view aText, std::size_t aOffset) -> char32_t
	{
		if (aOffset == 0U) {
			return 0U;
		}
		--aOffset;
		while ((static_cast<unsigned char>(aText[aOffset]) & 0xC0U) == 0x80U) {
			--aOffset;
		}
		return scalar(aText, aOffset);
	}
	inline auto following(std::string_view aText, std::size_t aOffset) -> char32_t { return aOffset == aText.size() ? 0U : scalar(aText, aOffset); }
	inline auto whole_word(std::string_view aText, byte_span aSpan) -> bool { return !word(previous(aText, static_cast<std::size_t>(aSpan.Begin))) && !word(following(aText, static_cast<std::size_t>(aSpan.End))); }
	struct character_range
	{
		char32_t Begin;
		char32_t End;
	};
	struct character_class
	{
		std::vector<character_range> Ranges;
		std::uint32_t                Categories = 0U;
		bool                         Negated    = false;
		bool                         Fold       = false;
	};
	enum class opcode : std::uint8_t { character,
									   any,
									   character_class,
									   split,
									   begin,
									   end,
									   text_begin,
									   text_end,
									   boundary,
									   not_boundary,
									   accept };
	struct instruction
	{
		opcode        Op    = opcode::accept;
		std::uint32_t Next  = 0U;
		std::uint32_t Other = 0U;
		char32_t      Value = 0U;
		bool          Flag  = false;
	};
	auto scan_with_budget(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds, budget& aWork, std::span<const byte_span> aRanges = { }) -> source_match_scan;
	auto compile_pattern(std::string_view aQuery, source_search_options aOptions, source_match_limits aLimits, diff_compute_stop* aStop, bool aGlob = false) -> source_match_compile;
}
namespace bpr::core {
	struct source_match_program::implementation
	{
		source_match_limits                           Limits;
		source_search_options                         Options;
		std::vector<source_matching::instruction>     Code;
		std::vector<source_matching::character_class> Classes;
		std::vector<char32_t>                         Literal;
		std::vector<std::size_t>                      Prefix;
		std::uint32_t                                 Start      = 0U;
		std::size_t                                   OwnedBytes = 0U;
	};
	struct source_match_access
	{
		static auto make(std::shared_ptr<const source_match_program::implementation> aProgram) -> source_match_program { return source_match_program{ std::move(aProgram) }; }
		static auto get(const source_match_program& aProgram) -> const source_match_program::implementation& { return *aProgram.Program; }
	};
}
#endif
