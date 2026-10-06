#include "bpr/core/fuzzy_match.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>

namespace bpr::core {
	namespace {
		inline constexpr char32_t replacement_character = U'�';

		/// \brief One folded code point and where it stands in its label.
		struct folded_character
		{
			char32_t Value     = 0;
			bool     WordStart = false;
			bool     SoftStart = false;
		};

		/// Decodes UTF-8, replacing each invalid or truncated sequence.
		auto decode(std::string_view aText) -> std::u32string
		{
			auto result = std::u32string{ };
			result.reserve(aText.size());
			for (std::size_t index = 0U; index < aText.size();) {
				const auto lead = static_cast<unsigned char>(aText[index]);
				auto       size = std::size_t{ 0U };
				auto       code = char32_t{ 0 };
				if (lead < 0x80U) {
					size = 1U;
					code = lead;
				} else if ((lead & 0xE0U) == 0xC0U) {
					size = 2U;
					code = lead & 0x1FU;
				} else if ((lead & 0xF0U) == 0xE0U) {
					size = 3U;
					code = lead & 0x0FU;
				} else if ((lead & 0xF8U) == 0xF0U) {
					size = 4U;
					code = lead & 0x07U;
				}
				auto valid = size != 0U && index + size <= aText.size();
				for (std::size_t offset = 1U; valid && offset < size; ++offset) {
					const auto next = static_cast<unsigned char>(aText[index + offset]);
					valid           = (next & 0xC0U) == 0x80U;
					code            = (code << 6U) | (next & 0x3FU);
				}
				constexpr std::array<char32_t, 5> smallest{ 0, 0, 0x80, 0x800, 0x10000 };
				if (valid && (code < smallest[size] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))) {
					valid = false;
				}
				if (!valid) {
					result.push_back(replacement_character);
					++index;
					continue;
				}
				result.push_back(code);
				index += size;
			}
			return result;
		}

		[[nodiscard]] constexpr auto whitespace(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter == U' ' || aCharacter == U'\t' || aCharacter == U'\n' || aCharacter == U'\r' || aCharacter == U'\f'
				|| aCharacter == U'\v' || aCharacter == U' ' || aCharacter == U' ' || aCharacter == U' '
				|| aCharacter == U'　';
		}

		[[nodiscard]] constexpr auto combining_mark(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter >= 0x0300 && aCharacter <= 0x036F;
		}

		[[nodiscard]] constexpr auto ascii_upper(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter >= U'A' && aCharacter <= U'Z';
		}

		[[nodiscard]] constexpr auto ascii_lower(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter >= U'a' && aCharacter <= U'z';
		}

		/// The base letters of U+00C0 to U+00FF; `*` keeps the character.
		constexpr std::u32string_view latin_one = U"aaaaaaxceeeeiiiidnooooo*ouuuuyts"
												  U"aaaaaaxceeeeiiiidnooooo*ouuuuyty";

		/// \brief Appends the folded form of one code point.
		void fold_into(const char32_t aCharacter, std::u32string& aOut)
		{
			if (aCharacter < 0x80) {
				aOut.push_back(ascii_upper(aCharacter) ? aCharacter + (U'a' - U'A') : aCharacter);
				return;
			}
			if (aCharacter >= 0xC0 && aCharacter <= 0xFF) {
				const auto base = latin_one[aCharacter - 0xC0];
				if (aCharacter == 0xC6 || aCharacter == 0xE6) {
					aOut.append(U"ae");
				} else if (aCharacter == 0xDE || aCharacter == 0xFE) {
					aOut.append(U"th");
				} else if (aCharacter == 0xDF) {
					aOut.append(U"ss");
				} else if (base == U'*') {
					aOut.push_back(aCharacter);
				} else {
					aOut.push_back(base);
				}
				return;
			}
			if (aCharacter >= 0x100 && aCharacter <= 0x17F) {
				struct latin_range
				{
					char32_t            Last;
					std::u32string_view Base;
				};
				// Latin Extended-A, in ranges of one base letter.
				constexpr std::array<latin_range, 24> ranges{ {
					{ 0x105, U"a" },
					{ 0x10D, U"c" },
					{ 0x111, U"d" },
					{ 0x11B, U"e" },
					{ 0x123, U"g" },
					{ 0x127, U"h" },
					{ 0x131, U"i" },
					{ 0x133, U"ij" },
					{ 0x135, U"j" },
					{ 0x138, U"k" },
					{ 0x142, U"l" },
					{ 0x14B, U"n" },
					{ 0x151, U"o" },
					{ 0x153, U"oe" },
					{ 0x159, U"r" },
					{ 0x161, U"s" },
					{ 0x167, U"t" },
					{ 0x173, U"u" },
					{ 0x175, U"w" },
					{ 0x178, U"y" },
					{ 0x17E, U"z" },
					{ 0x17F, U"s" },
					{ 0x17F, U"s" },
					{ 0x17F, U"s" },
				} };
				const auto                            range = std::ranges::find_if(ranges, [aCharacter](const latin_range& aRange) {
					return aCharacter <= aRange.Last;
				});
				aOut.append(range->Base);
				return;
			}
			if (aCharacter >= 0xFF01 && aCharacter <= 0xFF5E) {
				fold_into(aCharacter - 0xFF01 + 0x21, aOut);
				return;
			}
			if (aCharacter >= 0x0391 && aCharacter <= 0x03A9 && aCharacter != 0x03A2) {
				aOut.push_back(aCharacter + 0x20);
				return;
			}
			if (aCharacter >= 0x0410 && aCharacter <= 0x042F) {
				aOut.push_back(aCharacter + 0x20);
				return;
			}
			if (aCharacter >= 0x0400 && aCharacter <= 0x040F) {
				aOut.push_back(aCharacter + 0x50);
				return;
			}
			aOut.push_back(aCharacter);
		}

		[[nodiscard]] auto letter_or_digit(const char32_t aCharacter) noexcept -> bool
		{
			return (aCharacter >= U'0' && aCharacter <= U'9') || ascii_lower(aCharacter) || ascii_upper(aCharacter) || aCharacter >= 0xC0;
		}

		/// \brief Folds a label and marks each character's word and soft starts.
		///
		/// \details A word starts at the beginning and after white space. A soft
		/// start follows other punctuation, or is a camel hump (an upper-case
		/// letter after a lower-case one), read before folding.
		auto fold_label(std::string_view aText) -> std::vector<folded_character>
		{
			const auto decoded = decode(aText);
			auto       result  = std::vector<folded_character>{ };
			result.reserve(decoded.size());
			auto previous = char32_t{ U' ' };
			auto folded   = std::u32string{ };
			for (const auto character : decoded) {
				if (combining_mark(character)) {
					continue;
				}
				if (whitespace(character)) {
					if (!result.empty() && result.back().Value != U' ') {
						result.push_back({ .Value = U' ' });
					}
					previous = U' ';
					continue;
				}
				const auto wordStart = whitespace(previous);
				const auto softStart = !wordStart
					&& ((!letter_or_digit(previous)) || (ascii_upper(character) && ascii_lower(previous)));
				folded.clear();
				fold_into(character, folded);
				for (std::size_t index = 0U; index < folded.size(); ++index) {
					result.push_back({ .Value = folded[index], .WordStart = wordStart && index == 0U, .SoftStart = softStart && index == 0U });
				}
				previous = character;
			}
			if (!result.empty() && result.back().Value == U' ') {
				result.pop_back();
			}
			return result;
		}

		[[nodiscard]] auto values(const std::vector<folded_character>& aLabel) -> std::u32string
		{
			auto result = std::u32string{ };
			result.reserve(aLabel.size());
			for (const auto& character : aLabel) {
				result.push_back(character.Value);
			}
			return result;
		}

		[[nodiscard]] auto terms(const std::u32string& aQuery) -> std::vector<std::u32string>
		{
			auto result = std::vector<std::u32string>{ };
			auto start  = std::size_t{ 0U };
			while (start < aQuery.size()) {
				auto end = aQuery.find(U' ', start);
				if (end == std::u32string::npos) {
					end = aQuery.size();
				}
				if (end > start) {
					result.push_back(aQuery.substr(start, end - start));
				}
				start = end + 1U;
			}
			return result;
		}

		inline constexpr std::int32_t unmatched = std::numeric_limits<std::int32_t>::min() / 4;

		/// \brief The best subsequence score of one term in a label, if any.
		///
		/// \details Each term letter is placed at a label position after the
		/// previous one; the dynamic programme keeps, for each position, the
		/// best score of the term so far ending there.
		auto subsequence_score(const std::u32string& aTerm, const std::vector<folded_character>& aLabel)
			-> std::optional<std::int32_t>
		{
			const auto width = aLabel.size();
			if (aTerm.empty() || aTerm.size() > width) {
				return std::nullopt;
			}
			const auto bonus = [&aLabel](const std::size_t aPosition) {
				return aLabel[aPosition].WordStart ? fuzzy_word_start_bonus
					: aLabel[aPosition].SoftStart  ? fuzzy_soft_boundary_bonus
												   : 0;
			};
			auto previous = std::vector<std::int32_t>(width, unmatched);
			auto current  = std::vector<std::int32_t>(width, unmatched);
			for (std::size_t position = 0U; position < width; ++position) {
				if (aLabel[position].Value == aTerm[0]) {
					previous[position] = fuzzy_character_score + bonus(position)
						- (fuzzy_late_start_penalty * static_cast<std::int32_t>(position));
				}
			}
			for (std::size_t letter = 1U; letter < aTerm.size(); ++letter) {
				std::ranges::fill(current, unmatched);
				// The best earlier placement two or more positions back, with the
				// skipped letters it implies already charged: score + 15 * k.
				auto distant = unmatched;
				for (std::size_t position = 1U; position < width; ++position) {
					if (position >= 2U && previous[position - 2U] != unmatched) {
						distant = std::max(distant, previous[position - 2U] + (fuzzy_skipped_penalty * static_cast<std::int32_t>(position - 2U)));
					}
					if (aLabel[position].Value != aTerm[letter]) {
						continue;
					}
					auto best = unmatched;
					if (previous[position - 1U] != unmatched) {
						best = previous[position - 1U] + fuzzy_adjacent_bonus;
					}
					if (distant != unmatched) {
						best = std::max(best, distant - (fuzzy_skipped_penalty * static_cast<std::int32_t>(position - 1U)));
					}
					if (best != unmatched) {
						current[position] = best + fuzzy_character_score + bonus(position);
					}
				}
				std::swap(previous, current);
			}
			const auto best = std::ranges::max(previous);
			return best == unmatched ? std::nullopt : std::optional{ best };
		}

		/// \brief The unrestricted Damerau-Levenshtein distance, stopping early
		/// once it must exceed `aLimit`.
		auto edit_distance(const std::u32string_view aLeft, const std::u32string_view aRight, const std::size_t aLimit)
			-> std::size_t
		{
			const auto rows    = aLeft.size() + 2U;
			const auto columns = aRight.size() + 2U;
			const auto maximum = aLeft.size() + aRight.size();
			if ((aLeft.size() > aRight.size() ? aLeft.size() - aRight.size() : aRight.size() - aLeft.size()) > aLimit) {
				return aLimit + 1U;
			}
			auto       table = std::vector<std::size_t>(rows * columns, 0U);
			const auto at    = [&table, columns](const std::size_t aRow, const std::size_t aColumn) -> std::size_t& {
				return table[(aRow * columns) + aColumn];
			};
			at(0U, 0U) = maximum;
			for (std::size_t row = 0U; row <= aLeft.size(); ++row) {
				at(row + 1U, 0U) = maximum;
				at(row + 1U, 1U) = row;
			}
			for (std::size_t column = 0U; column <= aRight.size(); ++column) {
				at(0U, column + 1U) = maximum;
				at(1U, column + 1U) = column;
			}
			auto lastRow = std::map<char32_t, std::size_t>{ };
			for (std::size_t row = 1U; row <= aLeft.size(); ++row) {
				auto lastMatchColumn = std::size_t{ 0U };
				for (std::size_t column = 1U; column <= aRight.size(); ++column) {
					const auto found       = lastRow.find(aRight[column - 1U]);
					const auto matchRow    = found == lastRow.end() ? std::size_t{ 0U } : found->second;
					const auto matchColumn = lastMatchColumn;
					const auto cost        = aLeft[row - 1U] == aRight[column - 1U] ? std::size_t{ 0U } : std::size_t{ 1U };
					if (cost == 0U) {
						lastMatchColumn = column;
					}
					at(row + 1U, column + 1U) = std::min({
						at(row, column) + cost,
						at(row + 1U, column) + 1U,
						at(row, column + 1U) + 1U,
						at(matchRow, matchColumn) + (row - matchRow - 1U) + 1U + (column - matchColumn - 1U),
					});
				}
				lastRow[aLeft[row - 1U]] = row;
			}
			return at(aLeft.size() + 1U, aRight.size() + 1U);
		}

		/// \brief The fewest edits from a term to a word of the label or a word
		/// prefix of nearly its length, if within the term's allowance.
		auto typo_edits(const std::u32string& aTerm, const std::u32string& aLabel) -> std::optional<std::size_t>
		{
			if (aTerm.size() < fuzzy_typo_minimum_length) {
				return std::nullopt;
			}
			const auto allowed = aTerm.size() <= 4U ? std::size_t{ 1U } : std::size_t{ 2U };
			auto       best    = allowed + 1U;
			for (const auto& word : terms(aLabel)) {
				// A typo keeps the word's first letter; without that, any short
				// word is a few edits from many unrelated ones.
				if (word.empty() || word.front() != aTerm.front()) {
					continue;
				}
				const auto view     = std::u32string_view{ word };
				best                = std::min(best, edit_distance(aTerm, view, allowed));
				const auto shortest = aTerm.size() > allowed ? aTerm.size() - allowed : std::size_t{ 1U };
				for (auto length = shortest; length < word.size() && length <= aTerm.size() + allowed; ++length) {
					best = std::min(best, edit_distance(aTerm, view.substr(0U, length), allowed));
				}
			}
			return best <= allowed ? std::optional{ best } : std::nullopt;
		}
	} // namespace

	auto fold_search_text(const std::string_view aText) -> std::u32string
	{
		return values(fold_label(aText));
	}

	auto fuzzy_match(const std::string_view aQuery, const std::string_view aCandidate) -> std::optional<fuzzy_match_result>
	{
		if (aQuery.size() > fuzzy_maximum_input_length * 4U || aCandidate.size() > fuzzy_maximum_input_length * 4U) {
			return std::nullopt;
		}
		const auto query = fold_search_text(aQuery);
		const auto label = fold_label(aCandidate);
		if (query.empty() || label.empty() || query.size() > fuzzy_maximum_input_length || label.size() > fuzzy_maximum_input_length) {
			return std::nullopt;
		}
		const auto text = values(label);
		if (text == query) {
			return fuzzy_match_result{ .Score = fuzzy_exact_score, .Kind = fuzzy_match_kind::exact };
		}
		// A shorter label ranks first within a tier, and an earlier word first
		// among word prefixes.
		const auto excess = static_cast<std::int32_t>(text.size() - std::min(text.size(), query.size()));
		if (text.starts_with(query)) {
			return fuzzy_match_result{ .Score = fuzzy_prefix_score - excess, .Kind = fuzzy_match_kind::prefix };
		}
		for (auto position = text.find(query); position != std::u32string::npos; position = text.find(query, position + 1U)) {
			if (label[position].WordStart) {
				return fuzzy_match_result{ .Score = fuzzy_word_prefix_score - (static_cast<std::int32_t>(position) * 10) - excess,
										   .Kind  = fuzzy_match_kind::word_prefix };
			}
		}
		auto total = std::int32_t{ 0 };
		auto kind  = fuzzy_match_kind::subsequence;
		for (const auto& term : terms(query)) {
			if (const auto score = subsequence_score(term, label); score.has_value()) {
				total += *score;
				continue;
			}
			const auto edits = typo_edits(term, text);
			if (!edits.has_value()) {
				return std::nullopt;
			}
			total += (fuzzy_character_score * static_cast<std::int32_t>(term.size()))
				- (fuzzy_typo_edit_penalty * static_cast<std::int32_t>(*edits));
			kind = fuzzy_match_kind::typo;
		}
		return fuzzy_match_result{ .Score = total, .Kind = kind };
	}

	auto fuzzy_rank(const std::string_view aQuery, const std::span<const std::string_view> aCandidates)
		-> std::vector<fuzzy_rank_entry>
	{
		auto result = std::vector<fuzzy_rank_entry>{ };
		for (std::size_t index = 0U; index < aCandidates.size(); ++index) {
			if (const auto match = fuzzy_match(aQuery, aCandidates[index]); match.has_value()) {
				result.push_back({ .Index = index, .Match = *match });
			}
		}
		// Typo tolerance is a rescue: it answers only when nothing matched.
		if (std::ranges::any_of(result, [](const fuzzy_rank_entry& aEntry) { return aEntry.Match.Kind != fuzzy_match_kind::typo; })) {
			std::erase_if(result, [](const fuzzy_rank_entry& aEntry) { return aEntry.Match.Kind == fuzzy_match_kind::typo; });
		}
		std::ranges::stable_sort(result, [](const fuzzy_rank_entry& aLeft, const fuzzy_rank_entry& aRight) {
			return aLeft.Match.Score > aRight.Match.Score;
		});
		return result;
	}
} // namespace bpr::core
