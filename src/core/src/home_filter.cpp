#include "bpr/core/home_filter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <ranges>
#include <utility>

namespace bpr::core {
	namespace {
		/// The longest query, in bytes, worth matching: four bytes for each of
		/// the code points `fuzzy_match` accepts.
		inline constexpr std::size_t maximum_query_bytes = fuzzy_maximum_input_length * 4U;

		[[nodiscard]] constexpr auto blank(const char aCharacter) noexcept -> bool
		{
			return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\n' || aCharacter == '\r' || aCharacter == '\f'
				|| aCharacter == '\v';
		}

		[[nodiscard]] auto digits_only(const std::string_view aText) noexcept -> bool
		{
			return !aText.empty() && std::ranges::all_of(aText, [](const char aCharacter) { return aCharacter >= '0' && aCharacter <= '9'; });
		}

		/// \brief One term of a query, and the fields it may match.
		struct query_term
		{
			/// Matched against the texts; empty for a number-only term.
			std::string_view Text;
			/// Compared with the number; empty for a term that is not one.
			std::string_view Digits;
			/// "#12": the number only.
			bool NumberOnly = false;
			/// The "name" of "name#12": the repository only.
			bool RepositoryOnly = false;
		};

		[[nodiscard]] auto split_terms(const std::string_view aQuery) -> std::vector<query_term>
		{
			auto terms = std::vector<query_term>{ };
			auto at    = std::size_t{ 0U };
			while (at < aQuery.size()) {
				while (at < aQuery.size() && blank(aQuery[at])) {
					++at;
				}
				const auto begin = at;
				while (at < aQuery.size() && !blank(aQuery[at])) {
					++at;
				}
				if (begin == at) {
					break;
				}
				const auto token = aQuery.substr(begin, at - begin);
				const auto hash  = token.rfind('#');
				if (digits_only(token)) {
					terms.push_back({ .Text = token, .Digits = token });
				} else if (hash != std::string_view::npos && digits_only(token.substr(hash + 1U))) {
					if (hash > 0U) {
						terms.push_back({ .Text = token.substr(0U, hash), .RepositoryOnly = true });
					}
					terms.push_back({ .Digits = token.substr(hash + 1U), .NumberOnly = true });
				} else {
					terms.push_back({ .Text = token });
				}
			}
			return terms;
		}

		[[nodiscard]] constexpr auto tier_of(const fuzzy_match_kind aKind) noexcept -> home_filter_tier
		{
			switch (aKind) {
			case fuzzy_match_kind::exact:
			case fuzzy_match_kind::prefix:      return home_filter_tier::prefix;
			case fuzzy_match_kind::word_prefix: return home_filter_tier::word_start;
			case fuzzy_match_kind::substring:   return home_filter_tier::substring;
			case fuzzy_match_kind::subsequence: return home_filter_tier::subsequence;
			case fuzzy_match_kind::typo:        break;
			}
			return home_filter_tier::typo;
		}

		/// \brief Where one term matched best.
		struct term_hit
		{
			home_filter_tier              Tier  = home_filter_tier::typo;
			std::int32_t                  Score = 0;
			home_filter_field             Field = home_filter_field::title;
			std::size_t                   Label = 0U;
			std::vector<fuzzy_match_span> Spans;
		};

		/// \brief A lower tier first, then a higher score. An equal one keeps
		/// the hit found first, which is the earlier field.
		[[nodiscard]] auto better(const term_hit& aCandidate, const std::optional<term_hit>& aBest) -> bool
		{
			return !aBest.has_value() || aCandidate.Tier < aBest->Tier || (aCandidate.Tier == aBest->Tier && aCandidate.Score > aBest->Score);
		}

		void consider(std::optional<term_hit>& aBest, std::optional<term_hit> aCandidate)
		{
			if (aCandidate.has_value() && better(*aCandidate, aBest)) {
				aBest = std::move(aCandidate);
			}
		}

		[[nodiscard]] auto text_hit(
			const std::string_view  aTerm,
			const home_filter_field aField,
			const std::size_t       aLabel,
			const std::string_view  aText,
			const std::int32_t      aPenalty,
			const std::size_t       aOffset = 0U) -> std::optional<term_hit>
		{
			if (aText.empty()) {
				return std::nullopt;
			}
			auto match = fuzzy_match(aTerm, aText);
			if (!match.has_value()) {
				return std::nullopt;
			}
			for (auto& span : match->Spans) {
				span.Begin += aOffset;
			}
			return term_hit{ .Tier = tier_of(match->Kind), .Score = match->Score - aPenalty, .Field = aField, .Label = aLabel, .Spans = std::move(match->Spans) };
		}

		[[nodiscard]] auto number_hit(const std::string_view aDigits, const std::int64_t aNumber) -> std::optional<term_hit>
		{
			if (aNumber <= 0 || aDigits.empty()) {
				return std::nullopt;
			}
			auto buffer             = std::array<char, 24U>{ };
			const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), aNumber);
			if (error != std::errc{ }) {
				return std::nullopt;
			}
			const auto number = std::string_view{ buffer.data(), static_cast<std::size_t>(end - buffer.data()) };
			if (!number.starts_with(aDigits)) {
				return std::nullopt;
			}
			const auto spans = std::vector{ fuzzy_match_span{ .Begin = 0U, .Length = aDigits.size() } };
			if (number.size() == aDigits.size()) {
				return term_hit{ .Tier = home_filter_tier::number, .Score = fuzzy_exact_score, .Field = home_filter_field::number, .Spans = spans };
			}
			// A shorter number that starts with the digits is the nearer one.
			const auto rest = static_cast<std::int32_t>(number.size() - aDigits.size());
			return term_hit{ .Tier = home_filter_tier::prefix, .Score = fuzzy_prefix_score - (rest * fuzzy_character_score), .Field = home_filter_field::number, .Spans = spans };
		}

		[[nodiscard]] auto repository_hit(const std::string_view aTerm, const std::string_view aRepository) -> std::optional<term_hit>
		{
			auto best = text_hit(aTerm, home_filter_field::repository, 0U, aRepository, home_filter_repository_penalty);
			// The name alone, so a term can be its prefix: "app" starts "octo/app".
			if (const auto slash = aRepository.rfind('/'); slash != std::string_view::npos && slash + 1U < aRepository.size()) {
				consider(best, text_hit(aTerm, home_filter_field::repository, 0U, aRepository.substr(slash + 1U), home_filter_repository_penalty, slash + 1U));
			}
			return best;
		}

		[[nodiscard]] auto term_match(const query_term& aTerm, const home_filter_candidate& aCandidate) -> std::optional<term_hit>
		{
			auto best = std::optional<term_hit>{ };
			if (!aTerm.Digits.empty()) {
				consider(best, number_hit(aTerm.Digits, aCandidate.Number));
			}
			if (aTerm.NumberOnly || aTerm.Text.empty()) {
				return best;
			}
			if (aTerm.RepositoryOnly) {
				consider(best, repository_hit(aTerm.Text, aCandidate.Repository));
				return best;
			}
			consider(best, text_hit(aTerm.Text, home_filter_field::title, 0U, aCandidate.Title, 0));
			consider(best, repository_hit(aTerm.Text, aCandidate.Repository));
			consider(best, text_hit(aTerm.Text, home_filter_field::author, 0U, aCandidate.Author, home_filter_author_penalty));
			consider(best, text_hit(aTerm.Text, home_filter_field::branch, 0U, aCandidate.Branch, home_filter_branch_penalty));
			for (std::size_t label = 0U; label < aCandidate.Labels.size(); ++label) {
				consider(best, text_hit(aTerm.Text, home_filter_field::label, label, aCandidate.Labels[label], home_filter_label_penalty));
			}
			return best;
		}

		/// \brief Sorts and merges runs that overlap or touch.
		[[nodiscard]] auto merged(std::vector<fuzzy_match_span> aSpans) -> std::vector<fuzzy_match_span>
		{
			std::ranges::sort(aSpans, [](const fuzzy_match_span& aLeft, const fuzzy_match_span& aRight) {
				return aLeft.Begin != aRight.Begin ? aLeft.Begin < aRight.Begin : aLeft.Length < aRight.Length;
			});
			auto result = std::vector<fuzzy_match_span>{ };
			for (const auto& span : aSpans) {
				if (span.Length == 0U) {
					continue;
				}
				if (!result.empty() && span.Begin <= result.back().Begin + result.back().Length) {
					const auto end       = std::max(result.back().Begin + result.back().Length, span.Begin + span.Length);
					result.back().Length = end - result.back().Begin;
				} else {
					result.push_back(span);
				}
			}
			return result;
		}

		/// \brief The hits of every term, one per field, in field order.
		[[nodiscard]] auto gathered(std::vector<term_hit> aHits) -> std::vector<home_filter_hit>
		{
			std::ranges::stable_sort(aHits, [](const term_hit& aLeft, const term_hit& aRight) {
				return aLeft.Field != aRight.Field ? aLeft.Field < aRight.Field : aLeft.Label < aRight.Label;
			});
			auto result = std::vector<home_filter_hit>{ };
			for (auto& hit : aHits) {
				if (result.empty() || result.back().Field != hit.Field || result.back().Label != hit.Label) {
					result.push_back({ .Field = hit.Field, .Label = hit.Label });
				}
				result.back().Spans.insert(result.back().Spans.end(), hit.Spans.begin(), hit.Spans.end());
			}
			for (auto& hit : result) {
				hit.Spans = merged(std::move(hit.Spans));
			}
			return result;
		}
	} // namespace

	auto home_filter_active(const std::string_view aQuery) noexcept -> bool
	{
		return std::ranges::any_of(aQuery, [](const char aCharacter) { return !blank(aCharacter); });
	}

	auto match_home_filter(const std::string_view aQuery, const home_filter_candidate& aCandidate) -> std::optional<home_filter_match>
	{
		if (aQuery.size() > maximum_query_bytes) {
			return std::nullopt;
		}
		const auto terms = split_terms(aQuery);
		if (terms.empty()) {
			return std::nullopt;
		}
		auto hits  = std::vector<term_hit>{ };
		auto tier  = home_filter_tier::number;
		auto total = std::int64_t{ 0 };
		for (const auto& term : terms) {
			auto hit = term_match(term, aCandidate);
			if (!hit.has_value()) {
				return std::nullopt;
			}
			tier = std::max(tier, hit->Tier);
			total += hit->Score;
			hits.push_back(std::move(*hit));
		}
		auto result = home_filter_match{ .Tier = tier, .Score = static_cast<std::int32_t>(total / static_cast<std::int64_t>(terms.size())), .Hits = gathered(std::move(hits)) };
		// Several plain terms that run together in the title are that run: "fix
		// login" is a prefix of "Fix login page", not two word starts.
		const auto plain = terms.size() > 1U && std::ranges::none_of(terms, [](const query_term& aTerm) {
							   return aTerm.NumberOnly || aTerm.RepositoryOnly;
						   });
		if (plain && !aCandidate.Title.empty()) {
			if (auto whole = fuzzy_match(aQuery, aCandidate.Title);
				whole.has_value() && fuzzy_reach(whole->Kind) == fuzzy_match_reach::literal && tier_of(whole->Kind) <= result.Tier) {
				result = home_filter_match{
					.Tier  = tier_of(whole->Kind),
					.Score = whole->Score,
					.Hits  = { home_filter_hit{ .Field = home_filter_field::title, .Spans = merged(std::move(whole->Spans)) } },
				};
			}
		}
		return result;
	}

	auto rank_home_filter(const std::string_view aQuery, const std::span<const home_filter_candidate> aCandidates)
		-> std::vector<home_filter_entry>
	{
		auto result = std::vector<home_filter_entry>{ };
		if (!home_filter_active(aQuery)) {
			return result;
		}
		auto nearer = false;
		for (std::size_t index = 0U; index < aCandidates.size(); ++index) {
			if (auto match = match_home_filter(aQuery, aCandidates[index]); match.has_value()) {
				nearer = nearer || match->Tier != home_filter_tier::typo;
				result.push_back({ .Index = index, .Match = std::move(*match) });
			}
		}
		if (nearer) {
			std::erase_if(result, [](const home_filter_entry& aEntry) { return aEntry.Match.Tier == home_filter_tier::typo; });
		}
		std::ranges::stable_sort(result, [](const home_filter_entry& aLeft, const home_filter_entry& aRight) {
			return aLeft.Match.Tier != aRight.Match.Tier ? aLeft.Match.Tier < aRight.Match.Tier : aLeft.Match.Score > aRight.Match.Score;
		});
		return result;
	}
} // namespace bpr::core
