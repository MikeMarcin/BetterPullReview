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
/// \details Implemented in BetterPullReview from the algorithm the card
/// records for the owner's `gct::strings::fuzzy_matcher` (private repository
/// `containers`, `source/gct/strings/fuzzy_match.hpp` at `a7454c4`) and the
/// text folding of the owner's AllTheThings `FuzzyMatcher.swift`
/// (`MikeMarcin/AllTheThings` at `f755aeb`, MIT). Neither source was copied:
/// the station that wrote this could not reach them, so the tiers and
/// weights follow the card's description of them. The owner licensed any
/// port of their code under BetterPullReview's licence (2026-10-05). The
/// provenance is recorded in `docs/development/ported-code-provenance.md`.
namespace bpr::core {
	/// \brief How a query matched; the order is the ranking order.
	enum class fuzzy_match_kind : std::uint8_t {
		exact,
		prefix,
		word_prefix,
		subsequence,
		typo,
	};

	/// \brief One accepted match. A higher score ranks first.
	struct fuzzy_match_result
	{
		std::int32_t     Score                                               = 0;
		fuzzy_match_kind Kind                                                = fuzzy_match_kind::exact;
		auto             operator==(const fuzzy_match_result&) const -> bool = default;
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
	inline constexpr std::int32_t fuzzy_character_score      = 200;
	inline constexpr std::int32_t fuzzy_soft_boundary_bonus  = 300;
	inline constexpr std::int32_t fuzzy_word_start_bonus     = 500;
	inline constexpr std::int32_t fuzzy_adjacent_bonus       = 180;
	inline constexpr std::int32_t fuzzy_skipped_penalty      = 15;
	inline constexpr std::int32_t fuzzy_late_start_penalty   = 2;
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
	/// then its prefix, then the prefix of one of its words. Otherwise every
	/// whitespace-separated term must match on its own, as the best-scoring
	/// subsequence (word starts, soft boundaries and adjacent letters score;
	/// skipped and late letters cost), or, for a term of three or more
	/// letters with no subsequence, within one Damerau-Levenshtein edit of a
	/// word or word prefix that starts with the term's first letter (two edits
	/// for terms longer than four). An empty query,
	/// or an input longer than `fuzzy_maximum_input_length` code points,
	/// matches nothing.
	[[nodiscard]] auto fuzzy_match(std::string_view aQuery, std::string_view aCandidate)
		-> std::optional<fuzzy_match_result>;

	/// \brief The matching candidates, highest score first; equal scores keep
	/// their input order. Typo matches are a rescue: they are listed only when
	/// no candidate matched directly.
	[[nodiscard]] auto fuzzy_rank(std::string_view aQuery, std::span<const std::string_view> aCandidates)
		-> std::vector<fuzzy_rank_entry>;
} // namespace bpr::core

#endif // BPR_CORE_FUZZY_MATCH_HPP
