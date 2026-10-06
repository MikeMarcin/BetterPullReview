#ifndef BPR_CORE_FUZZY_MATCH_HPP
#define BPR_CORE_FUZZY_MATCH_HPP
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1289]]. A deterministic ranked fuzzy matcher for short labels.
///
/// \details Ported from the owner's own matchers, which the owner licensed
/// under BetterPullReview's licence (2026-10-05):
///
/// - the tiers, subsequence weights and Damerau-Levenshtein typo rescue of
///   `gct::strings::fuzzy_matcher` (private repository `containers`,
///   `source/gct/strings/fuzzy_match.hpp` at `a7454c4`);
/// - the matched ranges for highlighting of Entole's `score_candidate`
///   (private repository `aito`,
///   `code/entole_developer_tools/modules/completion/src/Completion.cpp` at
///   `962b48774`);
/// - the case, diacritic and width folding, the substring tier, spans on the
///   source text and the boundary anchor for scattered letters of
///   AllTheThings' `FuzzyMatcher` (`MikeMarcin/AllTheThings`,
///   `Sources/ATTCore/FuzzyMatcher.swift` at `f755aeb`, MIT);
/// - the consonant-signature spelling-variant test of FindInMedia's
///   `is_caption_spelling_variant` (`MikeMarcin/FindInMedia`,
///   `crates/fim-db/src/lib.rs` at `f1943cb`, MIT).
///
/// What was taken from each, and what was left, is recorded in
/// `docs/development/ported-code-provenance.md`.
namespace bpr::core {
	/// \brief How a query matched; the order is the ranking order.
	enum class fuzzy_match_kind : std::uint8_t {
		exact,
		prefix,
		word_prefix,
		substring,
		subsequence,
		typo,
	};

	/// \brief How far a match had to reach for the query: a literal run of it
	/// (exact to substring), its letters scattered, or a typo.
	enum class fuzzy_match_reach : std::uint8_t {
		literal,
		scattered,
		typo,
	};

	/// \brief The reach of a kind of match.
	[[nodiscard]] constexpr auto fuzzy_reach(const fuzzy_match_kind aKind) noexcept -> fuzzy_match_reach
	{
		return aKind == fuzzy_match_kind::typo       ? fuzzy_match_reach::typo
			: aKind == fuzzy_match_kind::subsequence ? fuzzy_match_reach::scattered
													 : fuzzy_match_reach::literal;
	}

	/// \brief A run of the candidate that the query matched, in UTF-8 bytes of
	/// the candidate as given (before folding), for highlighting.
	struct fuzzy_match_span
	{
		std::size_t Begin                                             = 0U;
		std::size_t Length                                            = 0U;
		auto        operator==(const fuzzy_match_span&) const -> bool = default;
	};

	/// \brief One accepted match. A higher score ranks first.
	struct fuzzy_match_result
	{
		std::int32_t     Score = 0;
		fuzzy_match_kind Kind  = fuzzy_match_kind::exact;
		/// The matched runs, ascending, merged and non-overlapping.
		std::vector<fuzzy_match_span> Spans;
		auto                          operator==(const fuzzy_match_result&) const -> bool = default;
	};

	/// \brief One ranked candidate, by its index in the input.
	struct fuzzy_rank_entry
	{
		std::size_t        Index = 0U;
		fuzzy_match_result Match;
		auto               operator==(const fuzzy_rank_entry&) const -> bool = default;
	};

	/// Scores of the whole-query tiers and the per-character weights.
	inline constexpr std::int32_t fuzzy_exact_score          = 200'000;
	inline constexpr std::int32_t fuzzy_prefix_score         = 160'000;
	inline constexpr std::int32_t fuzzy_word_prefix_score    = 140'000;
	inline constexpr std::int32_t fuzzy_substring_score      = 120'000;
	inline constexpr std::int32_t fuzzy_character_score      = 200;
	inline constexpr std::int32_t fuzzy_soft_boundary_bonus  = 300;
	inline constexpr std::int32_t fuzzy_word_start_bonus     = 500;
	inline constexpr std::int32_t fuzzy_label_start_bonus    = 500;
	inline constexpr std::int32_t fuzzy_adjacent_bonus       = 180;
	inline constexpr std::int32_t fuzzy_skipped_penalty      = 15;
	inline constexpr std::int32_t fuzzy_late_penalty         = 2;
	inline constexpr std::int32_t fuzzy_typo_edit_penalty    = 2'500;
	inline constexpr std::size_t  fuzzy_typo_minimum_length  = 3U;
	inline constexpr std::size_t  fuzzy_maximum_input_length = 512U;

	/// \brief Folds UTF-8 text for matching: lower case, Latin diacritics and
	/// full-width forms removed, combining marks dropped, whitespace runs
	/// collapsed to one space and trimmed. Invalid UTF-8 becomes U+FFFD.
	[[nodiscard]] auto fold_search_text(std::string_view aText) -> std::u32string;

	/// \brief Matches a query against one candidate.
	///
	/// \details The whole folded query tiers first: equal to the candidate,
	/// then its prefix, then the prefix of one of its words (a word starts
	/// after white space, after punctuation or at a camel hump), then a run
	/// inside a word. Otherwise every whitespace-separated term must match on
	/// its own, as the best-scoring subsequence whose first letter starts a
	/// word (word starts, soft boundaries and adjacent letters score; skipped
	/// and late letters cost) or as a run anywhere; or, for a term of three or
	/// more letters with neither, within one Damerau-Levenshtein edit of a
	/// word or word prefix (two edits for terms longer than four) that keeps
	/// the term's first letter or its consonants. An empty query, or an input
	/// longer than `fuzzy_maximum_input_length` code points, matches nothing.
	[[nodiscard]] auto fuzzy_match(std::string_view aQuery, std::string_view aCandidate)
		-> std::optional<fuzzy_match_result>;

	/// \brief The matching candidates, highest score first; equal scores keep
	/// their input order. Only the nearest reach any candidate made is listed:
	/// scattered letters are a rescue for when no candidate holds the query as
	/// a run, and typos for when no candidate holds its letters at all.
	[[nodiscard]] auto fuzzy_rank(std::string_view aQuery, std::span<const std::string_view> aCandidates)
		-> std::vector<fuzzy_rank_entry>;
} // namespace bpr::core

#endif // BPR_CORE_FUZZY_MATCH_HPP
