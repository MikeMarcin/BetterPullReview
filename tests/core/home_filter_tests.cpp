#include "bpr/core/home_filter.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1865]]. Home's filter: number forms, the owner's tier order,
/// fields, multi-term queries, the matched characters, ties and Unicode
/// titles.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "home filter failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto row(const std::int64_t aNumber, std::string aTitle, std::string aRepository = "octo-org/app", std::string aAuthor = "contributor-a")
		-> home_filter_candidate
	{
		return { .Number = aNumber, .Title = std::move(aTitle), .Repository = std::move(aRepository), .Author = std::move(aAuthor) };
	}

	[[nodiscard]] auto match(const std::string_view aQuery, const home_filter_candidate& aCandidate) -> home_filter_match
	{
		auto result = match_home_filter(aQuery, aCandidate);
		expect(result.has_value(), std::string{ "expected a match for " } + std::string{ aQuery });
		return *result;
	}

	[[nodiscard]] auto hit(const home_filter_match& aMatch, const home_filter_field aField) -> const home_filter_hit*
	{
		for (const auto& candidate : aMatch.Hits) {
			if (candidate.Field == aField) {
				return &candidate;
			}
		}
		return nullptr;
	}

	/// The ranked rows' numbers, best first.
	[[nodiscard]] auto ranked(const std::string_view aQuery, const std::vector<home_filter_candidate>& aRows) -> std::vector<std::int64_t>
	{
		auto result = std::vector<std::int64_t>{ };
		for (const auto& entry : rank_home_filter(aQuery, aRows)) {
			result.push_back(aRows[entry.Index].Number);
		}
		return result;
	}

	void test_number_forms()
	{
		const auto pull = row(123, "Teach the queue to wait");
		for (const auto query : { "123", "#123", " #123 " }) {
			const auto found = match(query, pull);
			expect(found.Tier == home_filter_tier::number, std::string{ "the exact number is the number tier: " } + query);
			const auto* const number = hit(found, home_filter_field::number);
			expect(number != nullptr && number->Spans == std::vector{ fuzzy_match_span{ .Begin = 0U, .Length = 3U } }, "every digit of the number is marked");
		}
		expect(match("12", pull).Tier == home_filter_tier::prefix, "the start of the number is a prefix");
		expect(hit(match("12", pull), home_filter_field::number)->Spans == std::vector{ fuzzy_match_span{ .Begin = 0U, .Length = 2U } }, "only the typed digits are marked");
		expect(!match_home_filter("23", pull).has_value(), "digits inside the number are not a match");
		expect(!match_home_filter("#1234", pull).has_value(), "a longer number is not a match");
		expect(!match_home_filter("#12x", pull).has_value() || match("#12x", pull).Tier != home_filter_tier::number, "a hash before letters is not a number");

		// "name#12" names the repository and the number together.
		const auto named = match("app#123", pull);
		expect(named.Tier == home_filter_tier::number || named.Tier == home_filter_tier::prefix, "a repository and its number together");
		expect(hit(named, home_filter_field::repository) != nullptr && hit(named, home_filter_field::number) != nullptr, "both are marked");
		expect(!match_home_filter("tools#123", pull).has_value(), "another repository's number is not this one");
		expect(match("octo-org/app#123", pull).Tier <= home_filter_tier::prefix, "owner/name#number, as the Open bar takes it");

		// "#123" is the number only; a bare 2024 may be in a title.
		const auto dated = row(7, "Release notes for 2024");
		expect(!match_home_filter("#2024", dated).has_value(), "a hashed number never matches a title");
		expect(match("2024", dated).Tier == home_filter_tier::word_start, "a bare number matches a title's word");
		expect(!match_home_filter("123", row(0, "Untitled")).has_value(), "a row with no number has none to match");
	}

	void test_exact_number_outranks_every_text_match()
	{
		const auto rows = std::vector{
			row(1123, "Raise the 123 limit"),
			row(12, "One hundred twenty-three"),
			row(123, "Unrelated title"),
			row(1230, "Another"),
		};
		const auto order = ranked("123", rows);
		expect(!order.empty() && order.front() == 123, "the exact number first");
		expect(order.size() == 3U && order[1] == 1230 && order[2] == 1123, "then the number's prefix, then a word in a title");
		const auto hashed = ranked("#12", rows);
		expect(hashed == std::vector<std::int64_t>{ 12, 123, 1230 }, "#12: the exact number, then the shorter number that starts with it");
	}

	void test_tiers_follow_the_owner_order()
	{
		expect(match("fix", row(1, "Fix the login page")).Tier == home_filter_tier::prefix, "a title's start is a prefix");
		expect(match("login", row(1, "Fix the login page")).Tier == home_filter_tier::word_start, "a word's start");
		expect(match("ogin", row(1, "Fix the login page")).Tier == home_filter_tier::substring, "a run inside a word");
		expect(match("flp", row(1, "Fix the login page")).Tier == home_filter_tier::subsequence, "scattered letters");
		const auto rows = std::vector{
			row(1, "Measure the flp rate"), // a word start
			row(2, "Fix the login page"), // scattered
			row(3, "flp: drop old reader"), // a prefix
			row(4, "Unrelated"),
		};
		expect(ranked("flp", rows) == std::vector<std::int64_t>{ 3, 1, 2 }, "prefix, then word start, then scattered letters");
		expect(home_filter_tier::number < home_filter_tier::prefix && home_filter_tier::prefix < home_filter_tier::word_start
				   && home_filter_tier::word_start < home_filter_tier::substring && home_filter_tier::substring < home_filter_tier::subsequence
				   && home_filter_tier::subsequence < home_filter_tier::typo,
			   "the tiers are in ranking order");
	}

	void test_every_field_is_searched()
	{
		auto pull   = row(5, "Speed up the cache", "acme-tools/build", "MikeMarcin");
		pull.Branch = "feature/warm-cache";
		pull.Labels = { "performance", "needs-review" };
		expect(hit(match("acme", pull), home_filter_field::repository) != nullptr, "the repository's owner");
		expect(match("build", pull).Tier == home_filter_tier::prefix, "the repository's name alone is a prefix");
		expect(hit(match("build", pull), home_filter_field::repository)->Spans == std::vector{ fuzzy_match_span{ .Begin = 11U, .Length = 5U } }, "marked where the name stands in owner/name");
		expect(hit(match("acme-tools/build", pull), home_filter_field::repository) != nullptr, "owner/name whole");
		expect(hit(match("mike", pull), home_filter_field::author) != nullptr, "the author");
		expect(hit(match("warm", pull), home_filter_field::branch) != nullptr, "the branch");
		const auto label = match("perf", pull);
		expect(hit(label, home_filter_field::label) != nullptr && hit(label, home_filter_field::label)->Label == 0U, "a label, by its index");
		expect(match("needs", pull).Hits.front().Label == 1U, "the second label");
		expect(!match_home_filter("zebra", pull).has_value(), "nothing that is in no field");
	}

	void test_the_title_wins_a_tie_with_another_field()
	{
		const auto title  = row(1, "Octocat tools", "someone/else", "nobody");
		const auto author = row(2, "Dashboard", "someone/else", "octocat tools");
		const auto rows   = std::vector{ author, title };
		expect(ranked("octo", rows) == std::vector<std::int64_t>{ 1, 2 }, "the same prefix in the title ranks above the author's");
		expect(match("octo", title).Hits.size() == 1U && match("octo", title).Hits.front().Field == home_filter_field::title, "the title's hit is the one marked");
	}

	void test_terms_may_match_different_fields()
	{
		const auto pull  = row(42, "Fix login redirect", "octo-org/app", "alice");
		const auto found = match("app login", pull);
		expect(found.Tier == home_filter_tier::word_start, "the weakest term's tier: login is a word start, app a prefix of the name");
		expect(hit(found, home_filter_field::repository) != nullptr && hit(found, home_filter_field::title) != nullptr, "both fields are marked");
		expect(!match_home_filter("app zebra", pull).has_value(), "every term must match");
		expect(match("alice 42", pull).Tier == home_filter_tier::prefix, "an author and the exact number");
		// Terms that run together in the title rank as that run.
		const auto run = match("fix login", pull);
		expect(run.Tier == home_filter_tier::prefix, "two words that start the title are its prefix");
		expect(hit(run, home_filter_field::title)->Spans == std::vector{ fuzzy_match_span{ .Begin = 0U, .Length = 9U } }, "marked as one run");
	}

	void test_marked_runs_are_merged_and_ascending()
	{
		const auto        found = match("login fix", row(1, "Fix login redirect"));
		const auto* const title = hit(found, home_filter_field::title);
		expect(title != nullptr && title->Spans == std::vector{ fuzzy_match_span{ .Begin = 0U, .Length = 3U }, fuzzy_match_span{ .Begin = 4U, .Length = 5U } }, "two runs, ascending");
		const auto twice = match("log login", row(1, "Fix login redirect"));
		expect(hit(twice, home_filter_field::title)->Spans == std::vector{ fuzzy_match_span{ .Begin = 4U, .Length = 5U } }, "overlapping runs are merged");
	}

	void test_ties_keep_their_input_order()
	{
		const auto rows = std::vector{ row(30, "Cache warm"), row(10, "Cache warm"), row(20, "Cache warm") };
		expect(ranked("cache", rows) == std::vector<std::int64_t>{ 30, 10, 20 }, "equal matches keep the order they were given in");
		expect(ranked("cache", rows) == ranked("cache", rows), "the same query ranks the same way every time");
		const auto reversed = std::vector{ rows[2], rows[1], rows[0] };
		expect(ranked("cache", reversed) == std::vector<std::int64_t>{ 20, 10, 30 }, "and follows the input's order, not the numbers'");
	}

	void test_unicode_titles()
	{
		const auto accented = row(1, "Résumé parsing for Zoë's café");
		expect(match("resume", accented).Tier == home_filter_tier::prefix, "diacritics fold: resume finds Résumé");
		const auto spans = hit(match("cafe", accented), home_filter_field::title)->Spans;
		expect(spans.size() == 1U && spans.front().Begin == std::string{ "Résumé parsing for Zoë's " }.size() && spans.front().Length == std::string{ "café" }.size(),
			   "the marked run is the source's bytes, é and all");
		expect(match("ZOË", accented).Tier == home_filter_tier::word_start, "case folds beyond ASCII");
		const auto japanese = row(2, "設定画面の修正");
		expect(match("画面", japanese).Tier == home_filter_tier::substring, "a run of CJK characters");
		const auto wide = row(3, "Ｆｕｌｌ width title");
		expect(match("full", wide).Tier == home_filter_tier::prefix, "full-width letters fold");
		const auto emoji = row(4, "🚀 Launch the rocket");
		expect(match_home_filter("launch", emoji).has_value(), "a title that begins with an emoji");
		expect(!match_home_filter("rocket ship", emoji).has_value(), "every term, in Unicode titles too");
	}

	void test_typos_are_a_rescue()
	{
		const auto rows = std::vector{ row(1, "Speed up discovery"), row(2, "Refresh the queue") };
		expect(ranked("discovrey", rows) == std::vector<std::int64_t>{ 1 }, "a typo finds the row when nothing else matched");
		const auto mixed = std::vector{ row(1, "Speed up discovery"), row(2, "discover more") };
		const auto order = ranked("discovery", mixed);
		expect(order == std::vector<std::int64_t>{ 1 }, "a typo is dropped while anything matched without one");
	}

	void test_a_blank_query_matches_nothing()
	{
		expect(!home_filter_active("") && !home_filter_active("  \t") && home_filter_active(" a "), "a blank query is no filter");
		expect(!match_home_filter("   ", row(1, "Anything")).has_value(), "and matches nothing");
		expect(rank_home_filter("", std::vector{ row(1, "Anything") }).empty(), "and ranks nothing");
		expect(!match_home_filter(std::string(5'000U, 'a'), row(1, "aaaa")).has_value(), "an overlong query matches nothing");
	}

	void test_a_large_list_ranks_quickly_and_completely()
	{
		auto rows = std::vector<home_filter_candidate>{ };
		for (auto number = 1; number <= 2'000; ++number) {
			rows.push_back(row(number, "Pull request number " + std::to_string(number) + " about caching", number % 2 == 0 ? "octo-org/app" : "acme/tools"));
		}
		const auto found = rank_home_filter("1500", rows);
		expect(!found.empty() && rows[found.front().Index].Number == 1500, "the exact number first in two thousand rows");
		expect(rank_home_filter("tools cach", rows).size() == 1'000U, "every row of one repository with a word in its title");
	}
} // namespace

auto main() -> int
{
	test_number_forms();
	test_exact_number_outranks_every_text_match();
	test_tiers_follow_the_owner_order();
	test_every_field_is_searched();
	test_the_title_wins_a_tie_with_another_field();
	test_terms_may_match_different_fields();
	test_marked_runs_are_merged_and_ascending();
	test_ties_keep_their_input_order();
	test_unicode_titles();
	test_typos_are_a_rescue();
	test_a_blank_query_matches_nothing();
	test_a_large_list_ranks_quickly_and_completely();
	std::cout << "home filter tests passed\n";
	return EXIT_SUCCESS;
}
