#ifndef BPR_SRC_CORE_SRC_SOURCE_MATCHER_INTERNAL_HPP
#define BPR_SRC_CORE_SRC_SOURCE_MATCHER_INTERNAL_HPP
#include "bpr/core/source_matcher.hpp"
#include <algorithm>
#include <span>
#include <string>
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
		/// \brief Polls cancellation before work whose cost is not counted in steps.
		void poll()
		{
			tick();
			if (Stop && Stop->stop_requested(Steps)) {
				throw stopped{ source_search_status::cancelled };
			}
		}
	};
	inline auto limits_valid(const source_match_limits& aLimits) -> bool
	{
		const auto upper = source_match_limits{ };
		return aLimits.MaximumBytes > 0U && aLimits.MaximumBytes <= upper.MaximumBytes
			&& aLimits.MaximumLines > 0U && aLimits.MaximumLines <= upper.MaximumLines
			&& aLimits.MaximumOwnedBytes > 0U && aLimits.MaximumOwnedBytes <= upper.MaximumOwnedBytes
			&& aLimits.MaximumResultBytes > 0U && aLimits.MaximumResultBytes <= upper.MaximumResultBytes
			&& aLimits.MaximumMatches > 0U && aLimits.MaximumMatches <= upper.MaximumMatches
			&& aLimits.MaximumProgramBytes > 0U && aLimits.MaximumProgramBytes <= upper.MaximumProgramBytes
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
	/// \brief Start of the scalar ending at `aOffset`; `aOffset` is a scalar boundary.
	inline auto previous_start(std::string_view aText, const std::size_t aOffset) -> std::size_t
	{
		auto start = aOffset - 1U;
		for (; start > 0U && (static_cast<unsigned char>(aText[start]) & 0xC0U) == 0x80U; --start) {
		}
		return start;
	}
	auto scan_with_budget(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds, budget& aWork, std::span<const byte_span> aRanges = { }) -> source_match_scan;
}
namespace bpr::core {
	struct source_match_program::implementation
	{
		source_match_limits                 Limits;
		source_search_options               Options;
		std::shared_ptr<const source_regex> Query;
		/// Whole Word only: one Unicode word scalar ([\p{L}\p{M}\p{N}_]) and one other.
		std::shared_ptr<const source_regex> Word;
		std::shared_ptr<const source_regex> NonWord;
		std::size_t                         OwnedBytes = 0U;
	};
	struct source_match_access
	{
		static auto make(std::shared_ptr<const source_match_program::implementation> aProgram) -> source_match_program { return source_match_program{ std::move(aProgram) }; }
		static auto get(const source_match_program& aProgram) -> const source_match_program::implementation& { return *aProgram.Program; }
	};
}
#endif
