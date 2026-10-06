#include "bpr/core/fuzzy_match.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1289]]. The ranked fuzzy matcher: its tiers, word-start and
/// camel bonuses, folding, multi-term queries, typo rescue and a ranking
/// corpus of Settings labels.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "fuzzy match failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto score(const std::string_view aQuery, const std::string_view aCandidate) -> std::int32_t
	{
		const auto match = fuzzy_match(aQuery, aCandidate);
		expect(match.has_value(), "expected a match");
		return match->Score;
	}

	[[nodiscard]] auto kind(const std::string_view aQuery, const std::string_view aCandidate) -> fuzzy_match_kind
	{
		const auto match = fuzzy_match(aQuery, aCandidate);
		expect(match.has_value(), "expected a match");
		return match->Kind;
	}

	/// The ranked labels' order, as their texts.
	[[nodiscard]] auto ranked(const std::string_view aQuery, const std::vector<std::string_view>& aCandidates)
		-> std::vector<std::string_view>
	{
		auto result = std::vector<std::string_view>{ };
		for (const auto& entry : fuzzy_rank(aQuery, aCandidates)) {
			result.push_back(aCandidates[entry.Index]);
		}
		return result;
	}

	void test_tiers_rank_exact_then_prefix_then_word_start_then_letters()
	{
		expect(kind("theme", "Theme") == fuzzy_match_kind::exact, "an equal label is exact, ignoring case");
		expect(kind("theme", "Theme colours") == fuzzy_match_kind::prefix, "a leading match is a prefix");
		expect(kind("theme", "Dark theme") == fuzzy_match_kind::word_prefix, "a later word's start is a word prefix");
		expect(kind("thm", "Theme") == fuzzy_match_kind::subsequence, "scattered letters are a subsequence");
		expect(score("theme", "Theme") > score("theme", "Theme colours") && score("theme", "Theme colours") > score("theme", "Dark theme")
				   && score("theme", "Dark theme") > score("thm", "Theme"),
			   "exact ranks above prefix, prefix above a word's start, and that above letters");
		expect(score("theme", "Theme mode") > score("theme", "Theme colours"), "a shorter label ranks first within a tier");
		expect(score("theme", "Dark theme") > score("theme", "Use the dark theme"), "an earlier word ranks first among word prefixes");
		expect(!fuzzy_match("theme", "Notifications").has_value(), "a label with neither the letters nor a near word is no match");
	}

	void test_word_starts_and_camel_humps_outrank_scattered_letters()
	{
		expect(score("ls", "Line spacing") > score("ls", "Lens"), "word starts outrank a scattered letter");
		expect(score("ls", "lineSpacing") > score("ls", "Lens"), "a camel hump counts as a boundary");
		expect(score("ls", "line_spacing") > score("ls", "Lens"), "punctuation starts a soft boundary");
		expect(score("ls", "Line spacing") > score("ls", "lineSpacing"), "a word start outranks a camel hump");
		expect(score("comp", "Compose review") > score("comp", "Show computed colour"), "a match at the start outranks a later one");
		expect(score("cr", "Compose review") > score("cr", "Cursor velocity"), "a word start outranks a nearer letter inside a word");
	}

	void test_folding_ignores_case_diacritics_and_width()
	{
		expect(kind("resume", "Résumé") == fuzzy_match_kind::exact, "diacritics fold away");
		expect(kind("RESUME", "résumé") == fuzzy_match_kind::exact, "case folds away");
		expect(kind("strasse", "Straße") == fuzzy_match_kind::exact, "sharp s folds to ss");
		expect(kind("theme", "\xEF\xBC\xB4\xEF\xBD\x88\xEF\xBD\x85\xEF\xBD\x8D\xEF\xBD\x85") == fuzzy_match_kind::exact, "full-width letters fold to ASCII");
		expect(kind("cafe", "cafe\xCC\x81") == fuzzy_match_kind::exact, "a combining accent is dropped");
		expect(fold_search_text("  Dark   theme  ") == U"dark theme", "white space collapses and trims");
		expect(fold_search_text("\xFF\xFE") == U"��", "invalid UTF-8 becomes replacement characters");
	}

	void test_every_term_must_match()
	{
		expect(fuzzy_match("font size", "Size of the code font").has_value(), "terms match in any order");
		expect(!fuzzy_match("font zebra", "Size of the code font").has_value(), "every term must match");
		expect(kind("code font", "Code font size") == fuzzy_match_kind::prefix, "the whole query tiers before its terms");
	}

	void test_typos_are_rescued_below_letters()
	{
		expect(kind("thmee", "Theme") == fuzzy_match_kind::typo, "a transposition is one edit");
		expect(kind("appearence", "Appearance") == fuzzy_match_kind::typo, "a substitution is one edit");
		expect(kind("notifcations", "Notifications") == fuzzy_match_kind::subsequence, "a dropped letter is still a subsequence");
		expect(fuzzy_match("apparence", "Appearance").has_value(), "two edits are allowed beyond four letters");
		expect(!fuzzy_match("tz", "Theme").has_value(), "a term under three letters has no typo rescue");
		expect(!fuzzy_match("xyzzy", "Theme").has_value(), "a distant term is no match");
		expect(score("theme", "Theme colours") > score("thmee", "Theme"), "a typo ranks below every direct match");
		expect(score("thm", "Theme") > score("thmee", "Theme"), "a typo ranks below a subsequence");
	}

	void test_empty_and_oversized_inputs_match_nothing()
	{
		expect(!fuzzy_match("", "Theme").has_value(), "an empty query matches nothing");
		expect(!fuzzy_match("   ", "Theme").has_value(), "a blank query matches nothing");
		expect(!fuzzy_match("theme", "").has_value(), "an empty label matches nothing");
		expect(!fuzzy_match(std::string(600U, 'a'), std::string(600U, 'a')).has_value(), "an oversized input matches nothing");
	}

	void test_settings_corpus_ranks_stably()
	{
		const auto corpus = std::vector<std::string_view>{
			"Appearance",
			"Theme",
			"Interface font",
			"Code font",
			"Code font size",
			"Line spacing",
			"Show line numbers",
			"Notifications",
			"Pause notifications",
			"Record local diagnostics",
			"Automatically check for updates",
			"A pull request opens in",
			"Accounts & hosts",
			"Agents & context",
			"Private draft",
			"Private notes",
		};
		expect(ranked("appear", corpus) == std::vector<std::string_view>{ "Appearance" }, "appear finds Appearance alone");
		expect(ranked("font", corpus) == std::vector<std::string_view>{ "Code font", "Code font size", "Interface font" },
			   "word prefixes order by position, then by length");
		expect(ranked("notif", corpus) == std::vector<std::string_view>{ "Notifications", "Pause notifications" },
			   "a prefix ranks above a later word's start");
		const auto privateOrder = ranked("private", corpus);
		expect(privateOrder == std::vector<std::string_view>{ "Private draft", "Private notes" }, "equal scores keep their input order");
		expect(ranked("lnum", corpus).front() == "Show line numbers", "word-start letters find a setting by its initials");
		expect(ranked("updtes", corpus).front() == "Automatically check for updates", "a dropped letter still finds the setting");
		expect(ranked("thmee", corpus) == std::vector<std::string_view>{ "Theme" }, "with no direct match a typo is rescued");
		expect(ranked("notif", corpus).size() == 2U, "with direct matches no typo is listed");
		expect(!fuzzy_match("font", "Agents & context").has_value(), "a typo keeps the word's first letter");
		const auto first  = fuzzy_rank("co", corpus);
		const auto second = fuzzy_rank("co", corpus);
		expect(first == second && !first.empty(), "ranking is deterministic");
	}
} // namespace

auto main() -> int
{
	test_tiers_rank_exact_then_prefix_then_word_start_then_letters();
	test_word_starts_and_camel_humps_outrank_scattered_letters();
	test_folding_ignores_case_diacritics_and_width();
	test_every_term_must_match();
	test_typos_are_rescued_below_letters();
	test_empty_and_oversized_inputs_match_nothing();
	test_settings_corpus_ranks_stably();
	std::cout << "fuzzy-match-tests:pass\n";
	return EXIT_SUCCESS;
}
