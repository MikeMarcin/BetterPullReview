#include "bpr/core/fuzzy_match.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1289]]. The ranked fuzzy matcher: its tiers, word-start and
/// camel bonuses, folding, multi-term queries, the spans it highlights, the
/// rules for scattered letters, its rescues and two ranking corpora of
/// Settings labels, the second the labels the Settings pages show.

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

	[[nodiscard]] auto spans(const std::string_view aQuery, const std::string_view aCandidate) -> std::vector<fuzzy_match_span>
	{
		const auto match = fuzzy_match(aQuery, aCandidate);
		expect(match.has_value(), "expected a match");
		return match->Spans;
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
		// MSVC without /utf-8 reads this file in the ANSI code page, so a literal
		// U+FFFD written as UTF-8 bytes would become three other characters;
		// the universal character names mean the same on every compiler.
		expect(fold_search_text("\xFF\xFE") == U"\uFFFD\uFFFD", "invalid UTF-8 becomes replacement characters");
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
		expect(kind("eppearance", "Appearance") == fuzzy_match_kind::typo, "a changed first letter is a typo when the consonants hold");
		expect(!fuzzy_match("kolour", "Colours").has_value(), "a changed first letter and changed consonants are no match");
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
	void test_spans_mark_the_matched_source_text()
	{
		using spans_t = std::vector<fuzzy_match_span>;
		expect(spans("theme", "Theme") == spans_t{ { 0U, 5U } }, "an exact match covers the label");
		expect(spans("code", "Code font") == spans_t{ { 0U, 4U } }, "a prefix covers the query's letters");
		expect(spans("font", "Code font") == spans_t{ { 5U, 4U } }, "a word prefix covers its word's start");
		expect(spans("pear", "Appearance") == spans_t{ { 2U, 4U } }, "a run covers the run");
		expect(spans("lh", "Line height") == spans_t{ { 0U, 1U }, { 5U, 1U } }, "scattered letters are spans of their own");
		expect(spans("syn key", "Syntax keyword") == spans_t{ { 0U, 3U }, { 7U, 3U } }, "each term marks its own letters");
		expect(spans("resume", "R\xC3\xA9sum\xC3\xA9") == spans_t{ { 0U, 8U } }, "a folded letter covers its source bytes");
		expect(spans("cafe", "cafe\xCC\x81") == spans_t{ { 0U, 6U } }, "a dropped combining mark is covered with its letter");
		expect(spans("strasse", "Stra\xC3\x9F"
								"e")
				   == spans_t{ { 0U, 7U } },
			   "an expanded letter is covered once");
		expect(spans("thmee", "Theme") == spans_t{ { 0U, 5U } }, "a typo covers the word it was matched to");
	}

	void test_a_run_inside_a_word_ranks_between_word_starts_and_letters()
	{
		expect(kind("pear", "Appearance") == fuzzy_match_kind::substring, "a run inside a word is a substring");
		expect(score("theme", "Dark theme") > score("heme", "Theme"), "a word's start ranks above a run inside a word");
		expect(score("heme", "Theme") > score("thm", "Theme"), "a run ranks above scattered letters");
		expect(score("heme", "Theme") > score("heme", "The scheme"), "an earlier, shorter run ranks first");
	}

	void test_scattered_letters_start_words()
	{
		expect(!fuzzy_match("tfcn", "Notifications").has_value(), "scattered letters start at a word, not inside one");
		expect(kind("ntfc", "Notifications") == fuzzy_match_kind::subsequence, "letters may be skipped inside the word");
		expect(kind("ilab", "Diff in-line added background") == fuzzy_match_kind::subsequence, "an acronym crosses soft boundaries");
		expect(kind("lig", "Line height") == fuzzy_match_kind::typo, "another word is entered at its start, so a letter inside it is no subsequence");
		const auto cache = fuzzy_match("cache", "Cancel history maintenance");
		expect(!cache.has_value() || cache->Kind == fuzzy_match_kind::typo, "a term's letters are not picked out of unrelated words");
	}

	void test_the_label_start_and_a_late_end_weigh_as_in_containers()
	{
		// "rv": r at the label's start, then v starting the next word, against
		// the same two letters two characters later.
		expect(score("rv", "Review velocity") - score("rv", "A review velocity") == fuzzy_label_start_bonus + (2 * fuzzy_late_penalty),
			   "the first letter at the label's start earns the start bonus, and each later end letter costs");
		expect(score("rv", "Review velocity") == (fuzzy_character_score + fuzzy_word_start_bonus + fuzzy_label_start_bonus) + (fuzzy_character_score + fuzzy_word_start_bonus) - (6 * fuzzy_skipped_penalty) - (7 * fuzzy_late_penalty),
			   "a subsequence scores containers' weights");
	}

	void test_rescues_answer_only_when_nothing_nearer_matched()
	{
		expect(fuzzy_reach(fuzzy_match_kind::exact) == fuzzy_match_reach::literal && fuzzy_reach(fuzzy_match_kind::substring) == fuzzy_match_reach::literal
				   && fuzzy_reach(fuzzy_match_kind::subsequence) == fuzzy_match_reach::scattered
				   && fuzzy_reach(fuzzy_match_kind::typo) == fuzzy_match_reach::typo,
			   "exact to substring is literal, a subsequence scattered and a typo a typo");
		const auto labels = std::vector<std::string_view>{ "Review preparation writing hook", "Appearance" };
		expect(ranked("pear", labels) == std::vector<std::string_view>{ "Appearance" }, "a literal run hides scattered letters");
		expect(ranked("rpw", labels) == std::vector<std::string_view>{ "Review preparation writing hook" }, "with no run, scattered letters answer");
		expect(ranked("apearance", labels) == std::vector<std::string_view>{ "Appearance" }, "with neither, a typo answers");
	}

	/// \brief The labels the Settings pages show (BPR-1289, read from the
	/// composed window on 2026-10-06), and what a reviewer types for them.
	void test_live_settings_labels_rank_as_expected()
	{
		const auto labels = std::vector<std::string_view>{
			"General",
			"Accounts & hosts",
			"Reviews & queue",
			"A pull request opens in",
			"Comments drawer opens on",
			"Continue review enters",
			"Mark a file read",
			"On return, compare",
			"Observer rules",
			"Observer rule pattern",
			"Saved observer rules",
			"Remove selected rule",
			"Queue defaults",
			"Reset queue defaults",
			"Repository",
			"Repository to save",
			"Save repository",
			"Saved repositories",
			"Remove saved repository",
			"Label marks",
			"Reset",
			"Appearance",
			"Theme",
			"Type",
			"Colours for the theme on screen",
			"Interface font",
			"Interface size",
			"Code font",
			"Code size",
			"Code weight",
			"Line height",
			"Tab width",
			"White space",
			"Ligatures",
			"Bold syntax emphasis",
			"Italic syntax emphasis",
			"Code line number text",
			"Code selection background",
			"Diff added background",
			"Diff in-line added background",
			"Syntax keyword",
			"Syntax comment",
			"Label typo",
			"Dif",
			"Dif motion",
			"Show Dif mascot",
			"Reset all",
			"Notifications",
			"Enable review notifications",
			"Pause notifications",
			"Reset notification preferences",
			"Agents & context",
			"Private draft",
			"Review preparation writing hook",
			"Test saved hook (no publishing)",
			"Storage & privacy",
			"Cancel history maintenance",
			"Delete repository cache",
			"Local application data location",
		};
		using names = std::vector<std::string_view>;
		expect(ranked("appear", labels) == names{ "Appearance" }, "appear finds the page, as in the owner's reference");
		expect(ranked("theme", labels) == names{ "Theme", "Colours for the theme on screen" }, "theme finds its section, then the colours");
		expect(ranked("lig", labels) == names{ "Ligatures" }, "lig finds Ligatures, not a scattered Line height");
		expect(ranked("tab", labels) == names{ "Tab width" }, "tab finds Tab width alone");
		expect(ranked("cache", labels) == names{ "Delete repository cache" }, "cache finds the cache, not Cancel history");
		expect(ranked("pear", labels) == names{ "Appearance" }, "a run inside a word finds its word");
		expect(ranked("syn key", labels) == names{ "Syntax keyword" }, "two word starts find the setting");
		expect(ranked("lnum", labels) == names{ "Code line number text" }, "initials and a word's start find the setting");
		expect(ranked("ilab", labels) == names{ "Diff in-line added background" }, "an acronym crosses a hyphen");
		expect(ranked("whitespace", labels) == names{ "White space" }, "a closed-up compound finds its two words");
		expect(ranked("dif mas", labels) == names{ "Show Dif mascot" }, "a two-word run finds the setting");
		expect(ranked("diff add", labels).front() == "Diff added background", "the shorter of two word-start runs ranks first");
		expect(ranked("color", labels) == names{ "Colours for the theme on screen" }, "an American spelling finds the British label");
		expect(ranked("eppearance", labels) == names{ "Appearance" }, "a vowel slip in the first letter is rescued");
		expect(ranked("notif", labels) == names{ "Notifications", "Pause notifications", "Reset notification preferences", "Enable review notifications" },
			   "a prefix ranks first, then word starts by their position");
		expect(ranked("reset", labels) == names{ "Reset", "Reset all", "Reset queue defaults", "Reset notification preferences" },
			   "reset finds the resets, and Remove selected rule's scattered letters do not join them");
		expect(ranked("repo", labels).size() == 6U && ranked("repo", labels).front() == "Repository", "repo finds every repository setting, the shortest first");
		expect(ranked("zzzq", labels).empty(), "nothing near the query is no result");
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
	test_spans_mark_the_matched_source_text();
	test_a_run_inside_a_word_ranks_between_word_starts_and_letters();
	test_scattered_letters_start_words();
	test_the_label_start_and_a_late_end_weigh_as_in_containers();
	test_rescues_answer_only_when_nothing_nearer_matched();
	test_live_settings_labels_rank_as_expected();
	std::cout << "fuzzy-match-tests:pass\n";
	return EXIT_SUCCESS;
}
