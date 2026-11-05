#ifndef BPR_CORE_HOME_FILTER_HPP
#define BPR_CORE_HOME_FILTER_HPP
#include "bpr/core/fuzzy_match.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1865]]. Home's filter: which pull requests a typed query
/// finds, how they rank and which characters it matched.
///
/// \details The owner, 2026-11-04: "I want to be able to fuzzy filter the home
/// screen to quickly find a PR." One pure function of the query and the rows'
/// own text. It reads no clock and no provider, keeps no state between calls,
/// and orders equal matches by their input order, so the same query over the
/// same rows always ranks them the same way. Each term is matched by
/// `fuzzy_match` ([[BPR-1289]]) on one field at a time; a number is compared
/// as digits.
namespace bpr::core {
	/// \brief The text of one Home row a query is matched against.
	struct home_filter_candidate
	{
		/// The pull request's number; zero for none.
		std::int64_t Number = 0;
		std::string  Title;
		/// "owner/name".
		std::string Repository;
		std::string Author;
		/// The head branch; empty where it is not known.
		std::string Branch;
		/// The labels; empty where none are known.
		std::vector<std::string> Labels;

		auto operator==(const home_filter_candidate&) const -> bool = default;
	};

	/// \brief Which of a candidate's texts a hit is in.
	enum class home_filter_field : std::uint8_t {
		/// The number's decimal digits, without "#".
		number,
		title,
		repository,
		author,
		branch,
		label,
	};

	/// \brief How well a candidate matched; the order is the ranking order.
	///
	/// \details The owner's order: an exact number, then a prefix or a word's
	/// start, then a run inside a word, then scattered letters. A typo is a
	/// rescue that `rank_home_filter` lists only when nothing matched better.
	enum class home_filter_tier : std::uint8_t {
		number,
		prefix,
		word_start,
		substring,
		subsequence,
		typo,
	};

	/// \brief The characters a query matched in one of a candidate's texts.
	struct home_filter_hit
	{
		home_filter_field Field = home_filter_field::title;
		/// Which label, for `home_filter_field::label`; zero otherwise.
		std::size_t Label = 0U;
		/// UTF-8 byte runs of that text as given (for `number`, of its
		/// digits), ascending, merged and non-overlapping.
		std::vector<fuzzy_match_span> Spans;

		auto operator==(const home_filter_hit&) const -> bool = default;
	};

	/// \brief One candidate's match.
	struct home_filter_match
	{
		/// The weakest tier any of the query's terms needed.
		home_filter_tier Tier = home_filter_tier::number;
		/// Higher ranks first within a tier.
		std::int32_t Score = 0;
		/// One per field matched, in field order, then label order.
		std::vector<home_filter_hit> Hits;

		auto operator==(const home_filter_match&) const -> bool = default;
	};

	/// \brief One ranked candidate, by its index in the input.
	struct home_filter_entry
	{
		std::size_t       Index = 0U;
		home_filter_match Match;

		auto operator==(const home_filter_entry&) const -> bool = default;
	};

	/// What a field's hit gives up against the title's, within a tier.
	inline constexpr std::int32_t home_filter_repository_penalty = 1'000;
	inline constexpr std::int32_t home_filter_author_penalty     = 2'000;
	inline constexpr std::int32_t home_filter_branch_penalty     = 3'000;
	inline constexpr std::int32_t home_filter_label_penalty      = 4'000;

	/// \brief Whether a query filters anything: it holds a character other
	/// than white space.
	[[nodiscard]] auto home_filter_active(std::string_view aQuery) noexcept -> bool;

	/// \brief Matches a query against one candidate.
	///
	/// \details The query's whitespace-separated terms must each match some
	/// field; different terms may match different fields ("app login" finds
	/// "Fix login" in octo/app). A term of digits, with or without a leading
	/// "#", equal to the number is an exact number; the start of it, a
	/// prefix. A bare number also matches the texts, as any term does; "#12"
	/// matches only the number. "name#12" is a repository term and "#12".
	/// Every other term is matched against the title, the repository (and its
	/// name alone, so "app" is a prefix of "octo/app"), the author, the branch
	/// and each label, and its best field wins: lower tier first, then higher
	/// score, the title before the other fields at equal strength. Several
	/// terms that run together in the title, as one prefix, word start or run,
	/// rank as that run. A blank query matches nothing.
	[[nodiscard]] auto match_home_filter(std::string_view aQuery, const home_filter_candidate& aCandidate)
		-> std::optional<home_filter_match>;

	/// \brief The candidates a query finds, best first.
	///
	/// \details Ordered by tier, then by score, highest first, then by input
	/// order, so equal matches keep the order they were given in. Typos are
	/// listed only when no candidate matched without one.
	[[nodiscard]] auto rank_home_filter(std::string_view aQuery, std::span<const home_filter_candidate> aCandidates)
		-> std::vector<home_filter_entry>;
} // namespace bpr::core

#endif // BPR_CORE_HOME_FILTER_HPP
