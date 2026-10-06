#include "bpr/core/fuzzy_match.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>

namespace bpr::core {
	namespace {
		inline constexpr char32_t     replacement_character = 0xFFFD;
		inline constexpr std::int32_t unmatched             = std::numeric_limits<std::int32_t>::min() / 4;

		/// \brief One decoded code point and the UTF-8 bytes it came from.
		struct decoded_character
		{
			char32_t    Value = 0;
			std::size_t Begin = 0U;
			std::size_t End   = 0U;
		};

		/// \brief One folded code point, where it stands in its label and the
		/// bytes of the source text it stands for.
		struct folded_character
		{
			char32_t    Value       = 0;
			bool        WordStart   = false;
			bool        SoftStart   = false;
			std::size_t SourceBegin = 0U;
			std::size_t SourceEnd   = 0U;
		};

		using folded_label = std::vector<folded_character>;

		/// \brief Where one term matched: its score and the folded positions of
		/// its letters.
		struct term_match
		{
			std::int32_t             Score = unmatched;
			std::vector<std::size_t> Positions;
		};

		/// \brief One word of a folded label: where it starts and how long it is.
		struct label_word
		{
			std::size_t Start  = 0U;
			std::size_t Length = 0U;
		};

		/// \brief Decodes one UTF-8 sequence at `aIndex`, or nothing if invalid.
		auto decode_one(const std::string_view aText, const std::size_t aIndex) -> std::optional<decoded_character>
		{
			const auto lead = static_cast<unsigned char>(aText[aIndex]);
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
			if (size == 0U || aIndex + size > aText.size()) {
				return std::nullopt;
			}
			for (std::size_t offset = 1U; offset < size; ++offset) {
				const auto next = static_cast<unsigned char>(aText[aIndex + offset]);
				if ((next & 0xC0U) != 0x80U) {
					return std::nullopt;
				}
				code = (code << 6U) | (next & 0x3FU);
			}
			constexpr std::array<char32_t, 5> smallest{ 0, 0, 0x80, 0x800, 0x10000 };
			if (code < smallest[size] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
				return std::nullopt;
			}
			return decoded_character{ .Value = code, .Begin = aIndex, .End = aIndex + size };
		}

		/// Decodes UTF-8, replacing each invalid or truncated byte.
		auto decode(const std::string_view aText) -> std::vector<decoded_character>
		{
			auto result = std::vector<decoded_character>{ };
			result.reserve(aText.size());
			for (std::size_t index = 0U; index < aText.size();) {
				const auto decoded = decode_one(aText, index);
				result.push_back(decoded.value_or(decoded_character{ .Value = replacement_character, .Begin = index, .End = index + 1U }));
				index = result.back().End;
			}
			return result;
		}

		[[nodiscard]] constexpr auto whitespace(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter == U' ' || (aCharacter >= 0x09 && aCharacter <= 0x0D) || aCharacter == 0xA0 || aCharacter == 0x1680
				|| (aCharacter >= 0x2000 && aCharacter <= 0x200A) || aCharacter == 0x2028 || aCharacter == 0x2029 || aCharacter == 0x202F
				|| aCharacter == 0x205F || aCharacter == 0x3000;
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

		[[nodiscard]] constexpr auto ascii_digit(const char32_t aCharacter) noexcept -> bool
		{
			return aCharacter >= U'0' && aCharacter <= U'9';
		}

		[[nodiscard]] constexpr auto letter_or_digit(const char32_t aCharacter) noexcept -> bool
		{
			return ascii_digit(aCharacter) || ascii_lower(aCharacter) || ascii_upper(aCharacter) || aCharacter >= 0xC0;
		}

		/// The base letters of U+00C0 to U+00FF; `*` keeps the character.
		constexpr std::u32string_view latin_one = U"aaaaaaxceeeeiiiidnooooo*ouuuuyts"
												  U"aaaaaaxceeeeiiiidnooooo*ouuuuyty";

		/// \brief Latin Extended-A in runs of one base letter: the last code
		/// point of each run and the letters it folds to.
		struct latin_run
		{
			char32_t            Last;
			std::u32string_view Base;
		};

		constexpr std::array<latin_run, 22> latin_extended_a{ {
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
		} };

		/// \brief Appends the folded form of one code point: AllTheThings'
		/// case-, diacritic- and width-insensitive folding, for the scripts a
		/// Settings label uses, without a Unicode library.
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
				} else {
					aOut.push_back(base == U'*' ? aCharacter : base);
				}
				return;
			}
			if (aCharacter >= 0x100 && aCharacter <= 0x17F) {
				const auto run = std::ranges::find_if(latin_extended_a, [aCharacter](const latin_run& aRun) {
					return aCharacter <= aRun.Last;
				});
				aOut.append(run->Base);
				return;
			}
			if (aCharacter >= 0xFF01 && aCharacter <= 0xFF5E) {
				fold_into(aCharacter - 0xFF01 + 0x21, aOut);
				return;
			}
			if ((aCharacter >= 0x0391 && aCharacter <= 0x03A9 && aCharacter != 0x03A2) || (aCharacter >= 0x0410 && aCharacter <= 0x042F)) {
				aOut.push_back(aCharacter + 0x20);
				return;
			}
			if (aCharacter >= 0x0400 && aCharacter <= 0x040F) {
				aOut.push_back(aCharacter + 0x50);
				return;
			}
			aOut.push_back(aCharacter);
		}

		/// \brief Folds a label and marks each character's word and soft starts.
		///
		/// \details A word starts at the beginning and after white space. A soft
		/// start follows other punctuation, or is a camel hump (an upper-case
		/// letter after a lower-case one), read before folding. Each folded
		/// character keeps the source bytes it stands for; a dropped combining
		/// mark joins the character before it, so highlighting covers both.
		auto fold_label(const std::string_view aText) -> folded_label
		{
			auto result   = folded_label{ };
			auto previous = char32_t{ U' ' };
			auto folded   = std::u32string{ };
			result.reserve(aText.size());
			for (const auto& character : decode(aText)) {
				if (combining_mark(character.Value)) {
					if (!result.empty() && result.back().Value != U' ') {
						result.back().SourceEnd = character.End;
					}
					continue;
				}
				if (whitespace(character.Value)) {
					if (!result.empty() && result.back().Value != U' ') {
						result.push_back({ .Value = U' ', .SourceBegin = character.Begin, .SourceEnd = character.End });
					}
					previous = U' ';
					continue;
				}
				const auto wordStart = whitespace(previous);
				const auto softStart = !wordStart
					&& (!letter_or_digit(previous) || (ascii_upper(character.Value) && ascii_lower(previous)));
				folded.clear();
				fold_into(character.Value, folded);
				for (std::size_t index = 0U; index < folded.size(); ++index) {
					result.push_back({ .Value       = folded[index],
									   .WordStart   = wordStart && index == 0U,
									   .SoftStart   = softStart && index == 0U,
									   .SourceBegin = character.Begin,
									   .SourceEnd   = character.End });
				}
				previous = character.Value;
			}
			if (!result.empty() && result.back().Value == U' ') {
				result.pop_back();
			}
			return result;
		}

		[[nodiscard]] auto values(const folded_label& aLabel) -> std::u32string
		{
			auto result = std::u32string{ };
			result.reserve(aLabel.size());
			for (const auto& character : aLabel) {
				result.push_back(character.Value);
			}
			return result;
		}

		/// \brief The space-separated words of folded text, by position.
		[[nodiscard]] auto words(const std::u32string_view aText) -> std::vector<label_word>
		{
			auto result = std::vector<label_word>{ };
			auto start  = std::size_t{ 0U };
			while (start < aText.size()) {
				const auto end = std::min(aText.find(U' ', start), aText.size());
				if (end > start) {
					result.push_back({ .Start = start, .Length = end - start });
				}
				start = end + 1U;
			}
			return result;
		}

		[[nodiscard]] auto boundary(const folded_character& aCharacter) noexcept -> bool
		{
			return aCharacter.WordStart || aCharacter.SoftStart;
		}

		/// \brief What a matched letter earns where it stands: containers'
		/// segment bonus at a word start, its soft bonus at punctuation or a
		/// camel hump, and the match base.
		[[nodiscard]] auto letter_score(const folded_label& aLabel, const std::size_t aPosition) noexcept -> std::int32_t
		{
			const auto& character = aLabel[aPosition];
			return fuzzy_character_score
				+ (character.WordStart       ? fuzzy_word_start_bonus
					   : character.SoftStart ? fuzzy_soft_boundary_bonus
											 : 0);
		}

		[[nodiscard]] auto late(const std::size_t aPosition) noexcept -> std::int32_t
		{
			return fuzzy_late_penalty * static_cast<std::int32_t>(aPosition);
		}

		[[nodiscard]] auto run_positions(const std::size_t aBegin, const std::size_t aLength) -> std::vector<std::size_t>
		{
			auto result = std::vector<std::size_t>(aLength);
			for (std::size_t index = 0U; index < aLength; ++index) {
				result[index] = aBegin + index;
			}
			return result;
		}

		/// \brief The source byte runs that folded positions stand for, merged.
		[[nodiscard]] auto source_spans(const folded_label& aLabel, std::vector<std::size_t> aPositions) -> std::vector<fuzzy_match_span>
		{
			std::ranges::sort(aPositions);
			auto result = std::vector<fuzzy_match_span>{ };
			for (const auto position : aPositions) {
				const auto& character = aLabel[position];
				if (!result.empty() && result.back().Begin + result.back().Length >= character.SourceBegin) {
					const auto end       = std::max(result.back().Begin + result.back().Length, character.SourceEnd);
					result.back().Length = end - result.back().Begin;
					continue;
				}
				result.push_back({ .Begin = character.SourceBegin, .Length = character.SourceEnd - character.SourceBegin });
			}
			return result;
		}

		/// \brief The best earlier placement of the previous letter, charged for
		/// its position.
		struct placement
		{
			std::int32_t Score    = unmatched;
			std::size_t  Position = 0U;

			void offer(const std::int32_t aScore, const std::size_t aPosition) noexcept
			{
				if (aScore > Score) {
					Score    = aScore;
					Position = aPosition;
				}
			}
		};

		/// \brief The best subsequence of one term whose first letter starts a
		/// word or follows a soft boundary.
		///
		/// \details containers' dynamic programme: each term letter is placed at
		/// a label position after the previous one, keeping for each position
		/// the best score of the term so far ending there and where it came
		/// from. A letter adjacent to the previous one earns the adjacent bonus;
		/// otherwise each skipped letter costs. The first letter at the label's
		/// start earns the label-start bonus, and a later end costs a little.
		/// AllTheThings' anchors (an acronym's letters start words; a short
		/// token's letters stay compact) become two rules here: the first
		/// letter starts a word or follows a soft boundary, and a letter in
		/// another word than the one before it starts its word or follows a
		/// soft boundary. Skipping letters inside a word is free of the rule,
		/// so a dropped letter still matches, but a term's letters are not
		/// picked out of the middle of unrelated words.
		auto anchored_subsequence(const std::u32string_view aTerm, const folded_label& aLabel) -> std::optional<term_match>
		{
			const auto width  = aLabel.size();
			const auto length = aTerm.size();
			if (length == 0U || length > width) {
				return std::nullopt;
			}
			auto previous = std::vector<std::int32_t>(width, unmatched);
			auto current  = std::vector<std::int32_t>(width, unmatched);
			auto from     = std::vector<std::size_t>(length * width, 0U);
			for (std::size_t position = 0U; position < width; ++position) {
				if (aLabel[position].Value == aTerm[0] && boundary(aLabel[position])) {
					previous[position] = letter_score(aLabel, position) + (position == 0U ? fuzzy_label_start_bonus : 0);
				}
			}
			for (std::size_t letter = 1U; letter < length; ++letter) {
				std::ranges::fill(current, unmatched);
				// The best earlier placement anywhere, and the best in the
				// current word, with the skipped letters each implies charged
				// when it is used: score + 15 * k.
				auto anywhere = placement{ };
				auto inWord   = placement{ };
				for (std::size_t position = 1U; position < width; ++position) {
					const auto before = previous[position - 1U];
					if (aLabel[position - 1U].Value == U' ') {
						inWord = placement{ };
					}
					if (before != unmatched) {
						const auto charged = before + (fuzzy_skipped_penalty * static_cast<std::int32_t>(position - 1U));
						anywhere.offer(charged, position - 1U);
						inWord.offer(charged, position - 1U);
					}
					// Letters may be skipped inside a word; reaching another word
					// lands on its start.
					const auto& reach = boundary(aLabel[position]) ? anywhere : inWord;
					if (aLabel[position].Value != aTerm[letter] || reach.Score == unmatched) {
						continue;
					}
					const auto earned = letter_score(aLabel, position);
					auto       best   = reach.Score - (fuzzy_skipped_penalty * static_cast<std::int32_t>(position - 1U)) + earned;
					auto       source = reach.Position;
					if (before != unmatched && before + earned + fuzzy_adjacent_bonus > best) {
						best   = before + earned + fuzzy_adjacent_bonus;
						source = position - 1U;
					}
					current[position]                 = best;
					from[(letter * width) + position] = source;
				}
				std::swap(previous, current);
			}
			auto best = term_match{ };
			auto end  = std::size_t{ 0U };
			for (std::size_t position = 0U; position < width; ++position) {
				if (previous[position] != unmatched && previous[position] - late(position) > best.Score) {
					best.Score = previous[position] - late(position);
					end        = position;
				}
			}
			if (best.Score == unmatched) {
				return std::nullopt;
			}
			best.Positions.resize(length);
			best.Positions[length - 1U] = end;
			for (auto letter = length - 1U; letter > 0U; --letter) {
				best.Positions[letter - 1U] = from[(letter * width) + best.Positions[letter]];
			}
			return best;
		}

		/// \brief The best run of a term's letters anywhere in the label: the
		/// subsequence score of its letters placed side by side.
		auto contiguous_run(const std::u32string_view aTerm, const folded_label& aLabel, const std::u32string_view aText)
			-> std::optional<term_match>
		{
			auto best = std::optional<term_match>{ };
			for (auto position = aText.find(aTerm); position != std::u32string_view::npos; position = aText.find(aTerm, position + 1U)) {
				auto score = (position == 0U ? fuzzy_label_start_bonus : 0) + (fuzzy_adjacent_bonus * static_cast<std::int32_t>(aTerm.size() - 1U))
					- late(position + aTerm.size() - 1U);
				for (std::size_t index = 0U; index < aTerm.size(); ++index) {
					score += letter_score(aLabel, position + index);
				}
				if (!best.has_value() || score > best->Score) {
					best = term_match{ .Score = score, .Positions = run_positions(position, aTerm.size()) };
				}
			}
			return best;
		}

		/// \brief The unrestricted Damerau-Levenshtein distance (Lowrance and
		/// Wagner), as containers computes it, stopping early once it must
		/// exceed `aLimit`.
		auto edit_distance(const std::u32string_view aLeft, const std::u32string_view aRight, const std::size_t aLimit)
			-> std::size_t
		{
			const auto difference = aLeft.size() > aRight.size() ? aLeft.size() - aRight.size() : aRight.size() - aLeft.size();
			if (difference > aLimit) {
				return aLimit + 1U;
			}
			const auto columns = aRight.size() + 2U;
			const auto maximum = aLeft.size() + aRight.size();
			auto       table   = std::vector<std::size_t>((aLeft.size() + 2U) * columns, 0U);
			const auto at      = [&table, columns](const std::size_t aRow, const std::size_t aColumn) -> std::size_t& {
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
			return std::min(at(aLeft.size() + 1U, aRight.size() + 1U), aLimit + 1U);
		}

		/// \brief FindInMedia's consonant signature: the ASCII letters and
		/// digits other than a, e, i, o, u and y.
		[[nodiscard]] auto consonants(const std::u32string_view aText) -> std::u32string
		{
			auto result = std::u32string{ };
			for (const auto character : aText) {
				if ((ascii_lower(character) || ascii_digit(character)) && std::u32string_view{ U"aeiouy" }.find(character) == std::u32string_view::npos) {
					result.push_back(character);
				}
			}
			return result;
		}

		/// \brief FindInMedia's test that a word may have a spelling variant:
		/// 3 to 12 ASCII letters and digits with at least two consonants.
		[[nodiscard]] auto spelling_variant_word(const std::u32string_view aText) -> bool
		{
			return aText.size() >= 3U && aText.size() <= 12U
				&& std::ranges::all_of(aText, [](const char32_t aCharacter) { return ascii_lower(aCharacter) || ascii_digit(aCharacter); })
				&& consonants(aText).size() >= 2U;
		}

		/// \brief Whether a typo may join a term to a window of a word: it keeps
		/// the term's first letter, or both are spelling variants of one
		/// another by FindInMedia's rule (the same consonants, so only vowels
		/// moved). Without either, a short term is a few edits from many
		/// unrelated words.
		[[nodiscard]] auto plausible_typo(const std::u32string_view aTerm, const std::u32string_view aWindow) -> bool
		{
			if (aWindow.front() == aTerm.front()) {
				return true;
			}
			return spelling_variant_word(aTerm) && spelling_variant_word(aWindow) && consonants(aTerm) == consonants(aWindow);
		}

		/// \brief The nearest word or word prefix to a term, within the term's
		/// edit allowance: containers' typo rescue over each word's prefix
		/// windows.
		auto typo_match(const std::u32string_view aTerm, const std::u32string_view aText) -> std::optional<term_match>
		{
			if (aTerm.size() < fuzzy_typo_minimum_length) {
				return std::nullopt;
			}
			const auto allowed  = aTerm.size() <= 4U ? std::size_t{ 1U } : std::size_t{ 2U };
			const auto shortest = aTerm.size() - allowed;
			// Fewest edits, then the window nearest the term's length, then the
			// earliest word.
			auto bestKey   = std::array<std::size_t, 3>{ allowed + 1U, 0U, 0U };
			auto bestStart = std::size_t{ 0U };
			auto bestSize  = std::size_t{ 0U };
			for (const auto& word : words(aText)) {
				const auto longest = std::min(word.Length, aTerm.size() + allowed);
				for (auto size = std::min(shortest, word.Length); size <= longest; ++size) {
					const auto window = aText.substr(word.Start, size);
					if (window.empty() || !plausible_typo(aTerm, window)) {
						continue;
					}
					const auto edits  = edit_distance(aTerm, window, allowed);
					const auto spread = size > aTerm.size() ? size - aTerm.size() : aTerm.size() - size;
					const auto key    = std::array<std::size_t, 3>{ edits, spread, word.Start };
					if (edits <= allowed && key < bestKey) {
						bestKey   = key;
						bestStart = word.Start;
						bestSize  = size;
					}
				}
			}
			if (bestKey[0] > allowed) {
				return std::nullopt;
			}
			return term_match{ .Score = (fuzzy_character_score * static_cast<std::int32_t>(aTerm.size()))
								   - (fuzzy_typo_edit_penalty * static_cast<std::int32_t>(bestKey[0])) - late(bestStart),
							   .Positions = run_positions(bestStart, bestSize) };
		}

		/// \brief One term's best direct match: an anchored subsequence or a run.
		auto direct_term_match(const std::u32string_view aTerm, const folded_label& aLabel, const std::u32string_view aText)
			-> std::optional<term_match>
		{
			auto anchored = anchored_subsequence(aTerm, aLabel);
			auto run      = contiguous_run(aTerm, aLabel, aText);
			if (anchored.has_value() && (!run.has_value() || anchored->Score >= run->Score)) {
				return anchored;
			}
			return run;
		}

		/// \brief The whole-query tiers: exact, prefix, a word's prefix, then a
		/// run inside a word. A shorter label ranks first within a tier, and an
		/// earlier word first among word prefixes and runs.
		auto whole_query_match(const std::u32string_view aQuery, const folded_label& aLabel, const std::u32string_view aText)
			-> std::optional<fuzzy_match_result>
		{
			const auto spans = [&aLabel, &aQuery](const std::size_t aPosition) {
				return source_spans(aLabel, run_positions(aPosition, aQuery.size()));
			};
			if (aText == aQuery) {
				return fuzzy_match_result{ .Score = fuzzy_exact_score, .Kind = fuzzy_match_kind::exact, .Spans = spans(0U) };
			}
			const auto excess = static_cast<std::int32_t>(aText.size() - std::min(aText.size(), aQuery.size()));
			if (aText.starts_with(aQuery)) {
				return fuzzy_match_result{ .Score = fuzzy_prefix_score - excess, .Kind = fuzzy_match_kind::prefix, .Spans = spans(0U) };
			}
			auto inside = std::u32string_view::npos;
			for (auto position = aText.find(aQuery); position != std::u32string_view::npos; position = aText.find(aQuery, position + 1U)) {
				if (boundary(aLabel[position])) {
					return fuzzy_match_result{ .Score = fuzzy_word_prefix_score - late(position) - excess,
											   .Kind  = fuzzy_match_kind::word_prefix,
											   .Spans = spans(position) };
				}
				inside = std::min(inside, position);
			}
			if (inside != std::u32string_view::npos) {
				return fuzzy_match_result{ .Score = fuzzy_substring_score - late(inside) - excess,
										   .Kind  = fuzzy_match_kind::substring,
										   .Spans = spans(inside) };
			}
			return std::nullopt;
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
		if (auto whole = whole_query_match(query, label, text); whole.has_value()) {
			return whole;
		}
		auto total     = std::int32_t{ 0 };
		auto kind      = fuzzy_match_kind::subsequence;
		auto positions = std::vector<std::size_t>{ };
		for (const auto& word : words(query)) {
			const auto term  = std::u32string_view{ query }.substr(word.Start, word.Length);
			auto       match = direct_term_match(term, label, text);
			if (!match.has_value()) {
				match = typo_match(term, text);
				kind  = fuzzy_match_kind::typo;
			}
			if (!match.has_value()) {
				return std::nullopt;
			}
			total += match->Score;
			positions.insert(positions.end(), match->Positions.begin(), match->Positions.end());
		}
		return fuzzy_match_result{ .Score = total, .Kind = kind, .Spans = source_spans(label, std::move(positions)) };
	}

	auto fuzzy_rank(const std::string_view aQuery, const std::span<const std::string_view> aCandidates)
		-> std::vector<fuzzy_rank_entry>
	{
		auto result = std::vector<fuzzy_rank_entry>{ };
		for (std::size_t index = 0U; index < aCandidates.size(); ++index) {
			if (auto match = fuzzy_match(aQuery, aCandidates[index]); match.has_value()) {
				result.push_back({ .Index = index, .Match = std::move(*match) });
			}
		}
		// Scattered letters and typos are rescues: each answers only when
		// nothing nearer matched.
		const auto reach = [](const fuzzy_rank_entry& aEntry) { return fuzzy_reach(aEntry.Match.Kind); };
		if (!result.empty()) {
			const auto nearest = reach(std::ranges::min(result, { }, reach));
			std::erase_if(result, [&](const fuzzy_rank_entry& aEntry) { return reach(aEntry) != nearest; });
		}
		std::ranges::stable_sort(result, [](const fuzzy_rank_entry& aLeft, const fuzzy_rank_entry& aRight) {
			return aLeft.Match.Score > aRight.Match.Score;
		});
		return result;
	}
} // namespace bpr::core
