#include "bpr/core/comparison_preparation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	using positions = std::vector<std::size_t>;

	void a_small_review_is_prepared_whole_current_then_next_then_the_rest()
	{
		expect(prepared_comparison_order(3U, 0U) == positions{ 0U, 1U, 2U },
			   "three files from the first: current, next, then the last");
		expect(prepared_comparison_order(3U, 1U) == positions{ 1U, 2U, 0U },
			   "three files from the middle: current, next, then the one before");
		expect(prepared_comparison_order(5U, 4U) == positions{ 4U, 3U, 2U, 1U, 0U },
			   "at the last file the rest come nearest first");
		expect(prepared_comparison_order(4U, std::nullopt) == positions{ 0U, 1U, 2U, 3U },
			   "no cursor starts at the first file");
		const auto whole  = prepared_comparison_order(prepared_comparison_window, 7U);
		auto       sorted = whole;
		std::ranges::sort(sorted);
		expect(whole.size() == prepared_comparison_window && whole.front() == 7U && whole[1] == 8U
				   && std::ranges::adjacent_find(sorted) == sorted.end() && sorted.back() == prepared_comparison_window - 1U,
			   "a review exactly the window's size is prepared whole, each file once");
	}

	void a_large_review_is_prepared_as_a_bounded_window_around_the_cursor()
	{
		const auto middle = prepared_comparison_order(515U, 200U);
		expect(middle.size() == prepared_comparison_window, "the window bounds a 515-file review");
		expect(middle.front() == 200U && middle[1] == 201U, "current first, next second");
		// Sixteen: the current file, three before it and twelve after it.
		auto expected = positions{ 200U };
		for (std::size_t step = 1U; step <= 12U; ++step) {
			expected.push_back(200U + step);
		}
		for (std::size_t step = 1U; step <= 3U; ++step) {
			expected.push_back(200U - step);
		}
		expect(middle == expected, "a quarter of the window looks back, the rest looks forward");
		const auto nearEnd = prepared_comparison_order(515U, 512U);
		expect(nearEnd.size() == prepared_comparison_window && nearEnd.front() == 512U && nearEnd[1] == 513U
				   && nearEnd[2] == 514U && nearEnd[3] == 511U && nearEnd.back() == 499U,
			   "near the end the window spends what forward cannot use on files before");
		const auto start = prepared_comparison_order(515U, 0U);
		expect(start.size() == prepared_comparison_window && start.back() == 15U,
			   "at the start everything goes forward");
		expect(prepared_comparison_order(515U, 200U, 4U) == positions{ 200U, 201U, 202U, 203U },
			   "a narrow window keeps only the current and following files");
	}

	void refusals_yield_nothing()
	{
		expect(prepared_comparison_order(0U, std::nullopt).empty(), "an empty review prepares nothing");
		expect(prepared_comparison_order(3U, 3U).empty(), "a cursor outside the order prepares nothing");
		expect(prepared_comparison_order(3U, 0U, 0U).empty(), "a zero window prepares nothing");
		expect(prepared_comparison_order(100U, 0U, maximum_prepared_comparison_window + 1U).empty(),
			   "an oversized window is refused rather than clamped");
		expect(prepared_comparison_order(100U, 0U, maximum_prepared_comparison_window).size()
				   == maximum_prepared_comparison_window,
			   "the largest window is honoured exactly");
	}
} // namespace

int main()
{
	a_small_review_is_prepared_whole_current_then_next_then_the_rest();
	a_large_review_is_prepared_as_a_bounded_window_around_the_cursor();
	refusals_yield_nothing();
	return EXIT_SUCCESS;
}
