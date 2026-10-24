// [[BPR-1736]] (owner, 2026-10-22, D1 to D4): the learned comment-anywhere
// fact. A line is natively placeable within three lines of a change, or
// anywhere once GitHub accepted such a comment for the key; the fact is an
// explicit input, its lifetime 14 days on GitHub.com and 30 on GitHub
// Enterprise Server, and only an observed answer sets it. The diff is
// BPR-Test #125's: an 80-line file whose line 10 changed, so lines 7 to 13 are
// the hunk and line 14 is the first line past it.
#include "bpr/core/off_hunk_fact.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace {
	using namespace bpr;
	using core::diff_side;
	using core::github_comment_route;
	using core::github_diff_placement;
	using fact    = core::off_hunk_creation_capability;
	using product = core::github_server_product;

	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(1);
		}
	}

	// Zero-based: line 9 changed, so 6 to 12 are in the hunk and 13 is out.
	auto pr125() -> core::line_correspondence
	{
		return { .LeftLines = 80U, .RightLines = 80U, .Unchanged = { { .Left = 0U, .Right = 0U, .Length = 9U }, { .Left = 10U, .Right = 10U, .Length = 70U } } };
	}

	constexpr auto day = std::int64_t{ 24 } * 60 * 60 * 1000;

	void placement_with_each_fact()
	{
		const auto lines = std::optional{ pr125() };
		const auto place = [&lines](const std::uint32_t aLine, const std::optional<std::uint32_t> aStart = std::nullopt) {
			return core::place_on_github_diff(lines, diff_side::right, aLine, aStart);
		};
		expect(place(12U) == github_diff_placement::in_diff && place(13U) == github_diff_placement::outside_diff && place(59U) == github_diff_placement::outside_diff,
			   "BPR-Test #125: line 13 is the last context line, 14 and 60 are outside");
		for (const auto learned : { fact::unknown, fact::supported, fact::unsupported }) {
			expect(core::route_github_comment(place(12U), learned) == github_comment_route::native, "a line in the hunk is native whatever was learned");
			expect(core::route_github_comment(github_diff_placement::unknown, learned) == github_comment_route::native, "no merge base read: native as today, nothing inferred");
			expect(!core::holds_review_comment(place(12U), learned) && !core::holds_review_comment(github_diff_placement::unknown, learned), "and is never held");
		}
		expect(core::route_github_comment(place(13U), fact::supported) == github_comment_route::native_off_hunk, "supported: line 14 is native");
		expect(core::route_github_comment(place(13U), fact::unsupported) == github_comment_route::off_hunk_file, "unsupported: a file comment, no native attempt");
		expect(core::route_github_comment(place(13U), fact::unknown) == github_comment_route::off_hunk_unknown, "unknown: one native attempt teaches the fact");
		expect(!core::holds_review_comment(place(59U), fact::supported), "a review carries line 60 only once GitHub accepted such a comment");
		expect(core::holds_review_comment(place(59U), fact::unsupported) && core::holds_review_comment(place(59U), fact::unknown), "otherwise it is held: a review never probes");
		const auto range = place(20U, 10U);
		expect(range == github_diff_placement::range_outside_diff, "a range leaving the hunk");
		expect(core::route_github_comment(range, fact::supported) == github_comment_route::native_off_hunk, "is native once supported");
		expect(core::route_github_comment(range, fact::unsupported) == github_comment_route::refused && core::route_github_comment(range, fact::unknown) == github_comment_route::refused,
			   "and otherwise refused: a range has no file comment");
		expect(core::holds_review_comment(range, fact::unknown) && !core::holds_review_comment(range, fact::supported), "and held in a review until supported");
		expect(core::route_github_comment(static_cast<github_diff_placement>(9U), fact::supported) == github_comment_route::refused, "an invalid placement is refused");
	}

	void equal_inputs_equal_routes()
	{
		const auto lines = std::optional{ pr125() };
		for (std::uint32_t line = 0U; line < 80U; ++line) {
			for (const auto learned : { fact::unknown, fact::supported, fact::unsupported }) {
				const auto first  = core::route_github_comment(core::place_on_github_diff(lines, diff_side::right, line, std::nullopt), learned);
				const auto second = core::route_github_comment(core::place_on_github_diff(lines, diff_side::right, line, std::nullopt), learned);
				expect(first == second, "the same line and fact route the same way");
			}
		}
	}

	void lifetime()
	{
		const auto refused  = core::learn_off_hunk_fact(core::off_hunk_answer::refused_naming_line, 1'000 * day);
		const auto accepted = core::learn_off_hunk_fact(core::off_hunk_answer::created, 1'000 * day);
		expect(refused && refused->State == fact::unsupported && refused->Source == core::off_hunk_fact_source::observed_refusal, "a refusal naming the line teaches unsupported");
		expect(accepted && accepted->State == fact::supported && accepted->Source == core::off_hunk_fact_source::observed_acceptance, "a created comment teaches supported");
		expect(!core::learn_off_hunk_fact(core::off_hunk_answer::unclear, 1'000 * day), "an unclear answer teaches nothing");
		expect(!core::learn_off_hunk_fact(core::off_hunk_answer::created, -1), "nor an answer at an invalid time");
		const auto at = [&](const product aProduct, const std::int64_t aAge) { return core::current_off_hunk_fact(aProduct, refused, 1'000 * day + aAge); };
		expect(at(product::github_com, 0) == fact::unsupported && at(product::github_com, 14 * day - 1) == fact::unsupported, "GitHub.com keeps it for 14 days");
		expect(at(product::github_com, 14 * day) == fact::unknown, "and forgets it at 14");
		expect(at(product::github_enterprise_server, 29 * day) == fact::unsupported && at(product::github_enterprise_server, 30 * day) == fact::unknown, "GitHub Enterprise Server keeps it for 30");
		expect(at(product::github_com, -1) == fact::unknown, "a fact observed after now is not trusted");
		expect(core::current_off_hunk_fact(product::github_com, std::nullopt, 0) == fact::unknown, "no observation: unknown");
		expect(core::current_off_hunk_fact(static_cast<product>(7U), refused, 1'000 * day) == fact::unknown, "an unknown product keeps nothing");
		const auto mixed = core::off_hunk_fact_observation{ .State = fact::supported, .Source = core::off_hunk_fact_source::observed_refusal, .ObservedUtcMs = 0 };
		expect(!core::valid(mixed) && core::current_off_hunk_fact(product::github_com, mixed, 1) == fact::unknown, "support never comes from a refusal");
		expect(!core::valid(core::off_hunk_fact_observation{ .State = fact::unknown, .Source = core::off_hunk_fact_source::observed_refusal }), "unknown is never kept");
	}

	void newest_wins()
	{
		const auto accepted = core::learn_off_hunk_fact(core::off_hunk_answer::created, 10);
		const auto refused  = core::learn_off_hunk_fact(core::off_hunk_answer::refused_naming_line, 20);
		expect(core::newer_off_hunk_fact(accepted, refused) == refused && core::newer_off_hunk_fact(refused, accepted) == refused, "a refusal after an acceptance demotes at once");
		const auto tie = core::learn_off_hunk_fact(core::off_hunk_answer::created, 20);
		expect(core::newer_off_hunk_fact(tie, refused) == refused && core::newer_off_hunk_fact(refused, tie) == refused, "a tie is the refusal");
		expect(core::newer_off_hunk_fact(accepted, std::nullopt) == accepted && core::newer_off_hunk_fact(std::nullopt, refused) == refused, "one alone is itself");
		expect(!core::newer_off_hunk_fact(std::nullopt, std::nullopt), "none of either is none");
	}

	void versions()
	{
		expect(core::valid_github_server_version(product::github_com, ""), "GitHub.com has no version");
		expect(!core::valid_github_server_version(product::github_com, "3.20.8"), "and is never keyed by one");
		for (const auto* good : { "3.20", "3.20.8", "3.22.2", "3.23.1", "10.0.0.1" }) {
			expect(core::valid_github_server_version(product::github_enterprise_server, good), "a GitHub Enterprise Server installed_version");
		}
		for (const auto* bad : { "", "3", ".3.20", "3..20", "3.20.", "3.20.8-rc1", "v3.20", "3.20.8.1.2", "3.123456", " 3.20" }) {
			expect(!core::valid_github_server_version(product::github_enterprise_server, bad), "an unparsable version keys nothing");
		}
		expect(!core::valid_github_server_version(static_cast<product>(0U), ""), "an unknown product keys nothing");
	}
}

auto main() -> int
{
	placement_with_each_fact();
	equal_inputs_equal_routes();
	lifetime();
	newest_wins();
	versions();
	std::cout << "core_off_hunk_fact:pass\n";
	return 0;
}
