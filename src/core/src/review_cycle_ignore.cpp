#include "bpr/core/review_cycle_ignore.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

namespace bpr::core {
	namespace {
		/// \brief How one attempt ended. Besides a plain mismatch, an attempt
		/// can end because the text ran out with pattern left, or because a
		/// single `*` reached a `/` it may not cross. Neither changes for an
		/// enclosing `*` that starts later, so it stops at once.
		/// \details [[BPR-0970]]. Without these stops the backtracking was
		/// exponential in the number of `*`: the parser fuzz target found a
		/// 6 KiB `.gitignore` line and path that took 13 seconds. They are
		/// Git's own (`WM_ABORT_ALL`, `WM_ABORT_TO_STARSTAR` in wildmatch.c).
		enum class attempt : std::uint8_t {
			matched,
			unmatched,
			abort_all,
			abort_to_double_star,
		};

		// Git's ASCII-only character classes (`sane_ctype`).
		auto is_upper(const unsigned char aValue) -> bool
		{
			return aValue >= 'A' && aValue <= 'Z';
		}
		auto is_lower(const unsigned char aValue) -> bool
		{
			return aValue >= 'a' && aValue <= 'z';
		}
		auto is_digit(const unsigned char aValue) -> bool
		{
			return aValue >= '0' && aValue <= '9';
		}
		auto is_alpha(const unsigned char aValue) -> bool
		{
			return is_upper(aValue) || is_lower(aValue);
		}
		auto is_space(const unsigned char aValue) -> bool
		{
			return aValue == ' ' || aValue == '\t' || aValue == '\n' || aValue == '\r';
		}
		auto is_print(const unsigned char aValue) -> bool
		{
			return aValue >= 0x20U && aValue < 0x7FU;
		}
		auto is_punct(const unsigned char aValue) -> bool
		{
			return is_print(aValue) && aValue != ' ' && !is_alpha(aValue) && !is_digit(aValue);
		}
		auto to_lower(const unsigned char aValue) -> unsigned char
		{
			return is_upper(aValue) ? static_cast<unsigned char>(aValue - 'A' + 'a') : aValue;
		}
		auto to_upper(const unsigned char aValue) -> unsigned char
		{
			return is_lower(aValue) ? static_cast<unsigned char>(aValue - 'a' + 'A') : aValue;
		}
		auto is_glob_special(const char aValue) -> bool
		{
			return aValue == '*' || aValue == '?' || aValue == '[' || aValue == '\\';
		}

		/// A POSIX class name between `[:` and `:]`; nothing for an unknown one.
		auto in_class(const std::string_view aName, const unsigned char aValue, const bool aCaseFold) -> std::optional<bool>
		{
			if (aName == "alnum") {
				return is_alpha(aValue) || is_digit(aValue);
			}
			if (aName == "alpha") {
				return is_alpha(aValue);
			}
			if (aName == "blank") {
				return aValue == ' ' || aValue == '\t';
			}
			if (aName == "cntrl") {
				return aValue < 0x20U || aValue == 0x7FU;
			}
			if (aName == "digit") {
				return is_digit(aValue);
			}
			if (aName == "graph") {
				return is_print(aValue) && aValue != ' ';
			}
			if (aName == "lower") {
				return is_lower(aValue);
			}
			if (aName == "print") {
				return is_print(aValue);
			}
			if (aName == "punct") {
				return is_punct(aValue);
			}
			if (aName == "space") {
				return is_space(aValue);
			}
			if (aName == "upper") {
				return is_upper(aValue) || (aCaseFold && is_lower(aValue));
			}
			if (aName == "xdigit") {
				return is_digit(aValue) || (aValue >= 'a' && aValue <= 'f') || (aValue >= 'A' && aValue <= 'F');
			}
			return std::nullopt;
		}

		struct bracket_result
		{
			attempt     Outcome = attempt::unmatched;
			std::size_t Next    = 0;
		};

		/// One `[...]` expression whose `[` is at `aStart`, against the already
		/// folded text byte `aText`. Git's rules: the first member is literal
		/// (even `]`), `-` between two members makes a range, `\` escapes, and
		/// a class resets the range start.
		auto bracket(const std::string_view aPattern, const std::size_t aStart, const unsigned char aText, const bool aCaseFold) -> bracket_result
		{
			auto       index   = aStart + 1;
			auto       at      = [&aPattern](const std::size_t aIndex) -> unsigned char { return aIndex < aPattern.size() ? static_cast<unsigned char>(aPattern[aIndex]) : 0U; };
			auto       here    = at(index);
			const auto negated = here == '!' || here == '^';
			if (negated) {
				here = at(++index);
			}
			auto previous = static_cast<unsigned char>(0U);
			auto matched  = false;
			for (;;) {
				if (index >= aPattern.size() || here == 0U) {
					return { .Outcome = attempt::abort_all };
				}
				if (here == '\\') {
					here = at(++index);
					if (index >= aPattern.size() || here == 0U) {
						return { .Outcome = attempt::abort_all };
					}
					matched = matched || aText == here;
				} else if (here == '-' && previous != 0U && at(index + 1) != 0U && at(index + 1) != ']') {
					here = at(++index);
					if (here == '\\') {
						here = at(++index);
						if (index >= aPattern.size() || here == 0U) {
							return { .Outcome = attempt::abort_all };
						}
					}
					if (aText <= here && aText >= previous) {
						matched = true;
					} else if (aCaseFold && is_lower(aText)) {
						const auto upper = to_upper(aText);
						matched          = matched || (upper <= here && upper >= previous);
					}
					here = 0U;
				} else if (here == '[' && at(index + 1) == ':') {
					const auto nameStart = index + 2;
					auto       close     = nameStart;
					while (close < aPattern.size() && aPattern[close] != ']' && aPattern[close] != '\0') {
						++close;
					}
					if (close >= aPattern.size() || aPattern[close] == '\0') {
						return { .Outcome = attempt::abort_all };
					}
					if (close == nameStart || aPattern[close - 1] != ':') {
						// No `:]` before the first `]`: the `[` is an ordinary member.
						here    = '[';
						matched = matched || aText == here;
					} else {
						const auto inside = in_class(aPattern.substr(nameStart, close - 1 - nameStart), aText, aCaseFold);
						if (!inside) {
							return { .Outcome = attempt::abort_all };
						}
						matched = matched || *inside;
						index   = close;
						here    = 0U;
					}
				} else {
					matched = matched || aText == here;
				}
				previous = here;
				here     = at(++index);
				if (here == ']') {
					break;
				}
			}
			return { .Outcome = matched != negated && aText != '/' ? attempt::matched : attempt::unmatched, .Next = index + 1 };
		}

		/// Where a run of non-star pattern bytes stopped: at a result, or at
		/// the first `*` of a star group.
		struct linear_step
		{
			bool        Star    = false;
			attempt     Outcome = attempt::unmatched;
			std::size_t Pattern = 0;
			std::size_t Text    = 0;
		};

		class wildmatch
		{
		public:
			wildmatch(const std::string_view aPattern, const std::string_view aText, const bool aCaseFold)
				: Pattern{ aPattern }
				, Text{ aText }
				, CaseFold{ aCaseFold }
			{
			}

			/// Git's `dowild` with `WM_PATHNAME`, with its recursion at each
			/// star group kept on an explicit stack: a pattern of many star
			/// groups cannot exhaust a worker thread's stack.
			auto run() -> attempt
			{
				auto patternIndex = std::size_t{ 0 };
				auto textIndex    = std::size_t{ 0 };
				for (;;) {
					auto step = linear(patternIndex, textIndex);
					auto next = step.Star ? star(step.Pattern, step.Text) : std::optional<attempt>{ step.Outcome };
					while (next) {
						if (Frames.empty()) {
							return *next;
						}
						next = resume(*next);
					}
					patternIndex = Start.first;
					textIndex    = Start.second;
				}
			}

		private:
			struct frame
			{
				/// A `**/` that first tries matching no directory at all.
				bool        Shortcut   = false;
				bool        MatchSlash = false;
				std::size_t Next       = 0;
				std::size_t Text       = 0;
			};

			[[nodiscard]] auto fold(const char aValue) const -> unsigned char
			{
				const auto value = static_cast<unsigned char>(aValue);
				return CaseFold ? to_lower(value) : value;
			}

			auto linear(std::size_t aPatternIndex, std::size_t aTextIndex) -> linear_step
			{
				for (; aPatternIndex < Pattern.size(); ++aPatternIndex, ++aTextIndex) {
					const auto raw = Pattern[aPatternIndex];
					if (aTextIndex >= Text.size() && raw != '*') {
						return { .Outcome = attempt::abort_all };
					}
					const auto textByte = aTextIndex < Text.size() ? fold(Text[aTextIndex]) : static_cast<unsigned char>(0U);
					switch (fold(raw)) {
					case '\\':
						// The escaped byte is compared as written, never folded.
						++aPatternIndex;
						if (aPatternIndex >= Pattern.size() || textByte != static_cast<unsigned char>(Pattern[aPatternIndex])) {
							return { .Outcome = attempt::unmatched };
						}
						continue;
					case '?':
						if (textByte == '/') {
							return { .Outcome = attempt::unmatched };
						}
						continue;
					case '[': {
						const auto result = bracket(Pattern, aPatternIndex, textByte, CaseFold);
						if (result.Outcome != attempt::matched) {
							return { .Outcome = result.Outcome };
						}
						aPatternIndex = result.Next - 1;
						continue;
					}
					case '*':
						return { .Star = true, .Pattern = aPatternIndex, .Text = aTextIndex };
					default:
						if (textByte != fold(raw)) {
							return { .Outcome = attempt::unmatched };
						}
						continue;
					}
				}
				return { .Outcome = aTextIndex == Text.size() ? attempt::matched : attempt::unmatched };
			}

			/// The end of a run of `*` starting at `aIndex`.
			[[nodiscard]] auto stars_end(std::size_t aIndex) const -> std::size_t
			{
				while (aIndex < Pattern.size() && Pattern[aIndex] == '*') {
					++aIndex;
				}
				return aIndex;
			}

			/// A star group at `aPatternIndex`. Either a result, or nothing with
			/// `Start` set to the next linear attempt.
			auto star(const std::size_t aPatternIndex, std::size_t aTextIndex) -> std::optional<attempt>
			{
				auto next       = aPatternIndex + 1;
				auto matchSlash = false;
				if (next < Pattern.size() && Pattern[next] == '*') {
					const auto segmentStart = aPatternIndex == 0 || Pattern[aPatternIndex - 1] == '/';
					next                    = stars_end(next);
					const auto segmentEnd   = next == Pattern.size() || Pattern[next] == '/' || (Pattern[next] == '\\' && next + 1 < Pattern.size() && Pattern[next + 1] == '/');
					if (segmentStart && segmentEnd) {
						matchSlash = true;
						if (next < Pattern.size() && Pattern[next] == '/') {
							// `**/**/` means `**/`; fold repeats so the shortcut
							// below never chains without consuming text.
							while (next + 2 < Pattern.size() && Pattern[next + 1] == '*' && Pattern[next + 2] == '*') {
								const auto after = stars_end(next + 1);
								if (after != Pattern.size() && Pattern[after] != '/') {
									break;
								}
								next = after;
								if (after == Pattern.size()) {
									break;
								}
							}
						}
						if (next < Pattern.size() && Pattern[next] == '/') {
							// `**/` may also stand for no directory at all.
							Frames.push_back({ .Shortcut = true, .MatchSlash = true, .Next = next, .Text = aTextIndex });
							Start = { next + 1, aTextIndex };
							return std::nullopt;
						}
					}
				}
				return loop(next, aTextIndex, matchSlash);
			}

			/// The star loop for the pattern after a star group.
			auto loop(const std::size_t aNext, const std::size_t aTextIndex, const bool aMatchSlash) -> std::optional<attempt>
			{
				if (aNext == Pattern.size()) {
					// A trailing `**` matches everything; a trailing `*` only what
					// has no further `/`.
					return aMatchSlash || Text.find('/', aTextIndex) == std::string_view::npos ? attempt::matched : attempt::unmatched;
				}
				if (!aMatchSlash && Pattern[aNext] == '/') {
					// One star then a slash: the star is the rest of this segment.
					const auto slash = Text.find('/', aTextIndex);
					if (slash == std::string_view::npos) {
						return attempt::unmatched;
					}
					Start = { aNext, slash };
					return std::nullopt;
				}
				Frames.push_back({ .MatchSlash = aMatchSlash, .Next = aNext, .Text = aTextIndex });
				return attempt_from_top();
			}

			/// Starts the next inner attempt of the top star loop, or ends it.
			auto attempt_from_top() -> std::optional<attempt>
			{
				auto& top = Frames.back();
				if (top.Text >= Text.size()) {
					Frames.pop_back();
					return attempt::abort_all;
				}
				if (!is_glob_special(Pattern[top.Next])) {
					// Skip ahead to the next byte that could start the rest.
					const auto wanted = fold(Pattern[top.Next]);
					while (top.Text < Text.size() && (top.MatchSlash || Text[top.Text] != '/') && fold(Text[top.Text]) != wanted) {
						++top.Text;
					}
					if (top.Text >= Text.size() || fold(Text[top.Text]) != wanted) {
						// No later start of any enclosing star can find it either:
						// the text ran out, or this single star met a `/`.
						const auto exhausted = top.Text >= Text.size();
						Frames.pop_back();
						return exhausted ? attempt::abort_all : attempt::abort_to_double_star;
					}
				}
				Start = { top.Next, top.Text };
				return std::nullopt;
			}

			/// Hands an inner result to the frame that started it.
			auto resume(const attempt aInner) -> std::optional<attempt>
			{
				const auto top = Frames.back();
				if (top.Shortcut) {
					Frames.pop_back();
					if (aInner == attempt::matched) {
						return attempt::matched;
					}
					return loop(top.Next, top.Text, true);
				}
				if (aInner != attempt::unmatched) {
					if (!top.MatchSlash || aInner != attempt::abort_to_double_star) {
						Frames.pop_back();
						return aInner;
					}
				} else if (!top.MatchSlash && Text[top.Text] == '/') {
					Frames.pop_back();
					return attempt::abort_to_double_star;
				}
				++Frames.back().Text;
				return attempt_from_top();
			}

			std::string_view                    Pattern;
			std::string_view                    Text;
			bool                                CaseFold = false;
			std::vector<frame>                  Frames;
			std::pair<std::size_t, std::size_t> Start;
		};

		/// Git's `trim_trailing_spaces`: unescaped trailing spaces go; a
		/// backslash escapes the byte after it.
		auto trim_trailing_spaces(const std::string_view aLine) -> std::string_view
		{
			auto lastSpace = std::optional<std::size_t>{ };
			for (std::size_t index = 0; index < aLine.size(); ++index) {
				if (aLine[index] == ' ') {
					if (!lastSpace) {
						lastSpace = index;
					}
					continue;
				}
				if (aLine[index] == '\\') {
					++index;
					if (index >= aLine.size()) {
						return aLine;
					}
				}
				lastSpace.reset();
			}
			return lastSpace ? aLine.substr(0, *lastSpace) : aLine;
		}

		/// Splits a rule file as Git does: a UTF-8 byte order mark is skipped,
		/// lines end at LF (dropping one CR before it), and a NUL ends a line's
		/// pattern because Git reads it as a C string.
		template <typename TVisit>
		void each_rule_line(std::string_view aContents, TVisit aVisit)
		{
			if (aContents.starts_with("\xEF\xBB\xBF")) {
				aContents.remove_prefix(3);
			}
			while (!aContents.empty()) {
				const auto end  = aContents.find('\n');
				auto       line = aContents.substr(0, end);
				aContents       = end == std::string_view::npos ? std::string_view{ } : aContents.substr(end + 1);
				if (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1);
				}
				aVisit(line);
			}
		}

		auto cut_at_nul(const std::string_view aText) -> std::string_view
		{
			return aText.substr(0, aText.find('\0'));
		}

		/// True when `aPath` lies beneath the directory `aBase`; `aRelative`
		/// receives the rest. The base comparison folds case under
		/// `core.ignoreCase`, as Git's `fspathncmp` does.
		auto beneath(const std::string_view aBase, const std::string_view aPath, const bool aIgnoreCase, std::string_view& aRelative) -> bool
		{
			if (aBase.empty()) {
				aRelative = aPath;
				return true;
			}
			if (aPath.size() <= aBase.size() || aPath[aBase.size()] != '/') {
				return false;
			}
			const auto head = aPath.substr(0, aBase.size());
			const auto same = aIgnoreCase ? std::ranges::equal(head, aBase, [](const char aA, const char aB) {
				return to_lower(static_cast<unsigned char>(aA)) == to_lower(static_cast<unsigned char>(aB));
			})
										  : head == aBase;
			if (!same) {
				return false;
			}
			aRelative = aPath.substr(aBase.size() + 1);
			return true;
		}

		/// A pattern as Git's `parse_path_pattern` leaves it, after any `!`.
		struct parsed_pattern
		{
			std::string_view Pattern;
			bool             DirectoryOnly = false;
			bool             Anchored      = false;
		};

		auto parse_path_pattern(std::string_view aPattern) -> parsed_pattern
		{
			auto parsed = parsed_pattern{ };
			if (!aPattern.empty() && aPattern.back() == '/') {
				parsed.DirectoryOnly = true;
				aPattern.remove_suffix(1);
			}
			parsed.Anchored = aPattern.find('/') != std::string_view::npos;
			// match_pathname drops one leading slash.
			if (parsed.Anchored && aPattern.front() == '/') {
				aPattern.remove_prefix(1);
			}
			parsed.Pattern = aPattern;
			return parsed;
		}

		/// The subject a rule's pattern is matched against.
		using byte_set = std::array<std::uint64_t, 4>;

		void insert(byte_set& aSet, const unsigned char aValue)
		{
			aSet.at(aValue / 64U) |= std::uint64_t{ 1 } << (aValue % 64U);
		}

		/// The bytes of `aText`, folded to lower case under `aCaseFold`.
		auto bytes_of(const std::string_view aText, const bool aCaseFold) -> byte_set
		{
			auto set = byte_set{ };
			for (const auto character : aText) {
				const auto value = static_cast<unsigned char>(character);
				insert(set, aCaseFold ? to_lower(value) : value);
			}
			return set;
		}

		/// \brief Bytes a pattern's literals demand of any subject it matches:
		/// every byte outside a wildcard, up to the first bracket expression
		/// (whose extent the matcher alone decides). A rule whose required
		/// bytes a subject lacks cannot match it, so it is skipped without
		/// running the matcher (RFC-BPR-0083 H6: a 1 MiB rule file).
		/// \details Under case folding a plain literal is required folded; an
		/// escaped byte is required as written, because the matcher compares
		/// it unfolded with the folded text.
		auto required_bytes(const std::string_view aPattern, const bool aCaseFold) -> byte_set
		{
			auto set = byte_set{ };
			for (std::size_t index = 0; index < aPattern.size(); ++index) {
				const auto value = static_cast<unsigned char>(aPattern[index]);
				if (value == '[') {
					break;
				}
				if (value == '*' || value == '?') {
					continue;
				}
				if (value == '\\') {
					if (++index < aPattern.size()) {
						insert(set, static_cast<unsigned char>(aPattern[index]));
					}
					continue;
				}
				insert(set, aCaseFold ? to_lower(value) : value);
			}
			return set;
		}

		auto contains_all(const byte_set& aHave, const byte_set& aNeed) -> bool
		{
			for (std::size_t word = 0; word < aNeed.size(); ++word) {
				if ((aNeed.at(word) & ~aHave.at(word)) != 0U) {
					return false;
				}
			}
			return true;
		}

		/// One rule's pattern against a path below its base, as Git's
		/// `match_basename` and `match_pathname` do. A pattern without a `/`
		/// matches the last segment. An anchored pattern's literal part, up to
		/// its first wildcard, is compared on its own and the rest is a pattern
		/// of its own: so a `**` right after the literal part starts a segment,
		/// and `config/local**/*.pem` ignores `config/local/a/key.pem` as Git does.
		auto pattern_matches(const std::string_view aPattern, const bool aAnchored, const std::string_view aRelative, const bool aCaseFold) -> bool
		{
			if (!aAnchored) {
				return wildcard_match(aPattern, aRelative.substr(aRelative.find_last_of('/') + 1), aCaseFold);
			}
			const auto prefix = std::min(aPattern.find_first_of("*?[\\"), aPattern.size());
			if (prefix > aRelative.size()) {
				return false;
			}
			const auto same = std::ranges::equal(aPattern.substr(0, prefix), aRelative.substr(0, prefix), [aCaseFold](const char aA, const char aB) {
				const auto left  = static_cast<unsigned char>(aA);
				const auto right = static_cast<unsigned char>(aB);
				return aCaseFold ? to_lower(left) == to_lower(right) : left == right;
			});
			if (!same) {
				return false;
			}
			if (prefix == aPattern.size()) {
				return prefix == aRelative.size();
			}
			return wildcard_match(aPattern.substr(prefix), aRelative.substr(prefix), aCaseFold);
		}

		constexpr auto default_names = std::array<std::string_view, 22>{
			// Version control and tool state.
			".git",
			".hg",
			".svn",
			".bzr",
			"_darcs",
			".jj",
			".DS_Store",
			"Thumbs.db",
			// Caches that are regenerated, never reviewed.
			"__pycache__",
			".pytest_cache",
			".mypy_cache",
			".tox",
			".venv",
			"node_modules",
			// Common credential stores.
			".ssh",
			".gnupg",
			".aws",
			".azure",
			".kube",
			".netrc",
			".git-credentials",
			".pypirc",
		};

		/// Git's longest attributes line; a longer one is ignored.
		constexpr std::size_t maximum_attribute_line = 2048U;

		auto valid_attribute_name(const std::string_view aName) -> bool
		{
			if (aName.empty() || aName.front() == '-' || aName.starts_with("builtin_")) {
				return false;
			}
			return std::ranges::all_of(aName, [](const char aCharacter) {
				const auto value = static_cast<unsigned char>(aCharacter);
				return aCharacter == '-' || aCharacter == '.' || aCharacter == '_' || is_digit(value) || is_alpha(value);
			});
		}

		/// Git's `unquote_c_style` for a quoted attributes pattern. Nothing when
		/// the quoting is malformed, in which case Git reads the pattern as an
		/// ordinary blank-delimited word.
		auto unquote(const std::string_view aText, std::size_t& aEnd) -> std::optional<std::string>
		{
			auto result = std::string{ };
			for (auto index = std::size_t{ 1 }; index < aText.size(); ++index) {
				const auto character = aText[index];
				if (character == '"') {
					aEnd = index + 1;
					return result;
				}
				if (character != '\\') {
					result += character;
					continue;
				}
				if (++index >= aText.size()) {
					return std::nullopt;
				}
				switch (aText[index]) {
				case 'a':
					result += '\a';
					break;
				case 'b':
					result += '\b';
					break;
				case 'f':
					result += '\f';
					break;
				case 'n':
					result += '\n';
					break;
				case 'r':
					result += '\r';
					break;
				case 't':
					result += '\t';
					break;
				case 'v':
					result += '\v';
					break;
				case '\\':
				case '"':
					result += aText[index];
					break;
				default: {
					// Three octal digits, the first 0-3.
					if (index + 2 >= aText.size() || aText[index] < '0' || aText[index] > '3') {
						return std::nullopt;
					}
					auto value = 0;
					for (int digit = 0; digit < 3; ++digit) {
						const auto octal = aText[index + static_cast<std::size_t>(digit)];
						if (octal < '0' || octal > '7') {
							return std::nullopt;
						}
						value = (value * 8) + (octal - '0');
					}
					result += static_cast<char>(value);
					index += 2;
					break;
				}
				}
			}
			return std::nullopt;
		}
	}

	auto wildcard_match(const std::string_view aPattern, const std::string_view aPath, const bool aCaseFold) -> bool
	{
		// Git reads the pattern as a C string.
		return wildmatch{ cut_at_nul(aPattern), aPath, aCaseFold }.run() == attempt::matched;
	}

	auto workspace_ignore_rules::add(const std::string_view aBase, const std::string_view aContents) -> bool
	{
		auto parsed = std::vector<rule>{ };
		each_rule_line(aContents, [&](std::string_view aLine) {
			// A comment is recognised before trailing spaces are trimmed.
			if (aLine.empty() || aLine.front() == '#') {
				return;
			}
			aLine        = trim_trailing_spaces(cut_at_nul(aLine));
			auto current = rule{ .Base = std::string{ aBase } };
			if (!aLine.empty() && aLine.front() == '!') {
				current.Negated = true;
				aLine.remove_prefix(1);
			}
			const auto pattern    = parse_path_pattern(aLine);
			current.DirectoryOnly = pattern.DirectoryOnly;
			current.Anchored      = pattern.Anchored;
			current.Pattern       = std::string{ pattern.Pattern };
			current.Required      = required_bytes(current.Pattern, IgnoreCase);
			// An empty pattern can match nothing.
			if (!current.Pattern.empty()) {
				parsed.push_back(std::move(current));
			}
		});
		if (Rules.size() + parsed.size() > maximum_ignore_rules) {
			return false;
		}
		std::ranges::move(parsed, std::back_inserter(Rules));
		return true;
	}

	auto workspace_ignore_rules::ignored(const std::string_view aPath, const bool aDirectory) const -> bool
	{
		// A rule's subject is the last segment or the path below its base; both
		// lie within these byte sets.
		const auto pathBytes = bytes_of(aPath, IgnoreCase);
		const auto nameBytes = bytes_of(aPath.substr(aPath.find_last_of('/') + 1), IgnoreCase);
		for (const auto& current : std::views::reverse(Rules)) {
			if ((current.DirectoryOnly && !aDirectory) || !contains_all(current.Anchored ? pathBytes : nameBytes, current.Required)) {
				continue;
			}
			auto relative = std::string_view{ };
			if (!beneath(current.Base, aPath, IgnoreCase, relative)) {
				continue;
			}
			if (pattern_matches(current.Pattern, current.Anchored, relative, IgnoreCase)) {
				return !current.Negated;
			}
		}
		return false;
	}

	auto workspace_attribute_rules::parse(const std::string_view aBase, const std::string_view aContents, const bool aMacros, std::vector<rule>& aRules,
										  std::vector<std::pair<std::string, std::vector<assignment>>>& aDefinitions) const -> bool
	{
		constexpr auto blank = std::string_view{ " \t\r\n" };
		auto           added = std::size_t{ 0 };
		each_rule_line(aContents, [&](std::string_view aLine) {
			// Git reads each line as a C string: a NUL ends that line only.
			aLine            = cut_at_nul(aLine);
			const auto first = aLine.find_first_not_of(blank);
			if (first == std::string_view::npos || aLine[first] == '#' || aLine.size() >= maximum_attribute_line) {
				return;
			}
			auto rest    = aLine.substr(first);
			auto pattern = std::string{ };
			auto end     = std::size_t{ 0 };
			if (rest.front() == '"') {
				if (auto unquoted = unquote(rest, end)) {
					// The unquoted pattern is read as a C string too.
					pattern = std::string{ cut_at_nul(*unquoted) };
				}
			}
			if (end == 0) {
				end     = std::min(rest.find_first_of(blank), rest.size());
				pattern = std::string{ rest.substr(0, end) };
			}
			rest = rest.substr(end);
			// `[attr]name` defines a macro, only where macros are allowed.
			constexpr auto prefix  = std::string_view{ "[attr]" };
			auto           isMacro = false;
			if (pattern.size() > prefix.size() && pattern.starts_with(prefix)) {
				if (!aMacros) {
					return;
				}
				isMacro = true;
				// The macro name ends at the first blank, even inside quotes.
				pattern = pattern.substr(prefix.size());
				pattern = pattern.substr(0, std::min(pattern.find_first_of(blank), pattern.size()));
				if (!valid_attribute_name(pattern)) {
					return;
				}
			}
			auto assignments = std::vector<assignment>{ };
			for (;;) {
				const auto start = rest.find_first_not_of(blank);
				if (start == std::string_view::npos) {
					break;
				}
				rest              = rest.substr(start);
				const auto token  = rest.substr(0, std::min(rest.find_first_of(blank), rest.size()));
				rest              = rest.substr(token.size());
				auto       name   = token;
				auto       state  = git_attribute_state{ .Kind = git_attribute_state::kind::set };
				const auto equals = token.find('=');
				if (token.front() == '-' || token.front() == '!') {
					state.Kind = token.front() == '-' ? git_attribute_state::kind::unset : git_attribute_state::kind::unspecified;
					name       = token.substr(1, equals == std::string_view::npos ? std::string_view::npos : equals - 1);
				} else if (equals != std::string_view::npos) {
					state = { .Kind = git_attribute_state::kind::value, .Value = std::string{ token.substr(equals + 1) } };
					name  = token.substr(0, equals);
				}
				// One invalid name makes Git drop the whole line.
				if (!valid_attribute_name(name)) {
					return;
				}
				assignments.push_back({ .Name = std::string{ name }, .State = std::move(state) });
			}
			added += assignments.size() + 1;
			if (isMacro) {
				aDefinitions.emplace_back(std::move(pattern), std::move(assignments));
				return;
			}
			auto view = std::string_view{ pattern };
			// Git ignores negative attribute patterns.
			if (view.starts_with('!')) {
				return;
			}
			const auto path = parse_path_pattern(view);
			if (path.Pattern.empty()) {
				return;
			}
			aRules.push_back({ .Base          = std::string{ aBase },
							   .Pattern       = std::string{ path.Pattern },
							   .Anchored      = path.Anchored,
							   .DirectoryOnly = path.DirectoryOnly,
							   .Assignments   = std::move(assignments),
							   .Required      = required_bytes(path.Pattern, IgnoreCase) });
		});
		return Rules.size() + InfoRules.size() + Assignments + added <= maximum_ignore_rules * 4U;
	}

	auto workspace_attribute_rules::add(const std::string_view aBase, const std::string_view aContents) -> bool
	{
		auto rules  = std::vector<rule>{ };
		auto macros = std::vector<std::pair<std::string, std::vector<assignment>>>{ };
		if (!parse(aBase, aContents, aBase.empty(), rules, macros)) {
			return false;
		}
		for (const auto& current : rules) {
			Assignments += current.Assignments.size();
		}
		std::ranges::move(rules, std::back_inserter(Rules));
		for (auto& [name, assignments] : macros) {
			RootMacros.insert_or_assign(std::move(name), std::move(assignments));
		}
		return true;
	}

	auto workspace_attribute_rules::add_info(const std::string_view aContents) -> bool
	{
		auto rules  = std::vector<rule>{ };
		auto macros = std::vector<std::pair<std::string, std::vector<assignment>>>{ };
		if (!parse({ }, aContents, true, rules, macros)) {
			return false;
		}
		for (const auto& current : rules) {
			Assignments += current.Assignments.size();
		}
		std::ranges::move(rules, std::back_inserter(InfoRules));
		for (auto& [name, assignments] : macros) {
			InfoMacros.insert_or_assign(std::move(name), std::move(assignments));
		}
		return true;
	}

	auto workspace_attribute_rules::matches(const rule& aRule, const std::string_view aPath) const -> bool
	{
		// A directory pattern never applies to a file or a directory's contents.
		if (aRule.DirectoryOnly || !contains_all(bytes_of(aRule.Anchored ? aPath : aPath.substr(aPath.find_last_of('/') + 1), IgnoreCase), aRule.Required)) {
			return false;
		}
		auto relative = std::string_view{ };
		return beneath(aRule.Base, aPath, IgnoreCase, relative) && pattern_matches(aRule.Pattern, aRule.Anchored, relative, IgnoreCase);
	}

	auto workspace_attribute_rules::definition(const std::string_view aName) const -> const std::vector<assignment>*
	{
		// info/attributes outranks the root file; within one, the later line.
		for (const auto* table : { &InfoMacros, &RootMacros }) {
			if (const auto found = table->find(aName); found != table->end()) {
				return &found->second;
			}
		}
		return nullptr;
	}

	void workspace_attribute_rules::fill(const std::vector<assignment>& aAssignments, assigned_states& aAssigned) const
	{
		// Within a line the rightmost assignment wins; a set macro expands in
		// place, before the rest of the line, filling only what is still
		// unassigned. The expansion runs on an explicit stack, so a long chain
		// of macros cannot exhaust a worker thread's stack.
		auto pending = std::vector<std::pair<const std::vector<assignment>*, std::size_t>>{ { &aAssignments, aAssignments.size() } };
		while (!pending.empty()) {
			auto& [list, remaining] = pending.back();
			if (remaining == 0) {
				pending.pop_back();
				continue;
			}
			const auto& item = (*list)[--remaining];
			if (!aAssigned.emplace(item.Name, &item.State).second || item.State.Kind != git_attribute_state::kind::set) {
				continue;
			}
			if (const auto* expansion = definition(item.Name)) {
				pending.emplace_back(expansion, expansion->size());
			}
		}
	}

	auto workspace_attribute_rules::attribute(const std::string_view aPath, const std::string_view aName) const -> git_attribute_state
	{
		auto assigned = assigned_states{ };
		for (const auto* list : { &InfoRules, &Rules }) {
			for (const auto& current : std::views::reverse(*list)) {
				if (!matches(current, aPath)) {
					continue;
				}
				fill(current.Assignments, assigned);
				if (const auto found = assigned.find(aName); found != assigned.end()) {
					return *found->second;
				}
			}
		}
		return { };
	}

	auto workspace_attribute_rules::filter(const std::string_view aPath) const -> git_attribute_state
	{
		return attribute(aPath, "filter");
	}

	auto workspace_attribute_rules::linguist_generated(const std::string_view aPath) const -> git_attribute_state
	{
		return attribute(aPath, "linguist-generated");
	}

	auto default_exclusion_names() noexcept -> std::span<const std::string_view>
	{
		return default_names;
	}

	auto excluded_by_default(const std::string_view aName) noexcept -> bool
	{
		return std::ranges::find(default_names, aName) != default_names.end();
	}
}
