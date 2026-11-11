#include "bpr/core/agent_reference.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace bpr::core {
	namespace {
		constexpr auto en_dash = std::string_view{ "\xE2\x80\x93" };

		[[nodiscard]] constexpr auto digit(const char aCharacter) noexcept -> bool { return aCharacter >= '0' && aCharacter <= '9'; }

		[[nodiscard]] constexpr auto letter(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z');
		}

		/// A byte of a word: ASCII letters, digits and `_`, and every byte of
		/// a multi-byte UTF-8 character, which is a letter of some script.
		[[nodiscard]] constexpr auto word(const char aCharacter) noexcept -> bool
		{
			return letter(aCharacter) || digit(aCharacter) || aCharacter == '_' || static_cast<unsigned char>(aCharacter) >= 0x80U;
		}

		[[nodiscard]] constexpr auto path_byte(const char aCharacter) noexcept -> bool
		{
			return word(aCharacter) || aCharacter == '.' || aCharacter == '/' || aCharacter == '-' || aCharacter == '+';
		}

		/// What may not stand right before a path: anything that would make it
		/// the tail of a longer word or path.
		[[nodiscard]] constexpr auto path_joined(const char aCharacter) noexcept -> bool
		{
			return word(aCharacter) || aCharacter == '.' || aCharacter == '/' || aCharacter == '\\' || aCharacter == '-';
		}

		/// A path as the forms admit it: segments between slashes, none empty,
		/// the last with a name and an extension of letters, digits, `_` or `+`.
		[[nodiscard]] auto valid_path(const std::string_view aPath) noexcept -> bool
		{
			if (aPath.empty() || aPath.front() == '/' || aPath.back() == '/' || aPath.find("//") != std::string_view::npos) {
				return false;
			}
			const auto slash = aPath.rfind('/');
			const auto name  = slash == std::string_view::npos ? aPath : aPath.substr(slash + 1U);
			const auto dot   = name.rfind('.');
			if (dot == std::string_view::npos || dot == 0U || dot + 1U == name.size() || !(word(name.front()) || name.front() == '-')) {
				return false;
			}
			// An extension has a letter: `1.2:3` is a version, not a file.
			const auto extension = name.substr(dot + 1U);
			return std::ranges::any_of(extension, letter)
				&& std::ranges::all_of(extension, [](const char aCharacter) { return letter(aCharacter) || digit(aCharacter) || aCharacter == '_' || aCharacter == '+'; });
		}

		/// Reads the text from a position, by the forms' small vocabulary.
		class reader
		{
		public:
			reader(const std::string_view aText, const std::size_t aAt) noexcept
				: Text{ aText }
				, At{ aAt }
			{
			}

			[[nodiscard]] auto at() const noexcept -> std::size_t { return At; }

			auto literal(const std::string_view aWords) noexcept -> bool
			{
				if (Text.substr(At).starts_with(aWords)) {
					At += aWords.size();
					return true;
				}
				return false;
			}

			auto spaces() noexcept -> std::size_t
			{
				const auto from = At;
				while (At < Text.size() && (Text[At] == ' ' || Text[At] == '\t')) {
					++At;
				}
				return At - from;
			}

			/// A one-based line number, at most nine digits.
			auto number() noexcept -> std::optional<std::uint32_t>
			{
				auto value = std::uint64_t{ 0U };
				auto count = std::size_t{ 0U };
				while (At < Text.size() && digit(Text[At]) && count < 10U) {
					value = value * 10U + static_cast<std::uint64_t>(Text[At] - '0');
					++At;
					++count;
				}
				if (count == 0U || count > 9U || value == 0U || (At < Text.size() && digit(Text[At]))) {
					return std::nullopt;
				}
				return static_cast<std::uint32_t>(value);
			}

			/// A hyphen or an en dash, with spaces around it or not.
			auto dash() noexcept -> bool
			{
				const auto from = At;
				spaces();
				if (literal("-") || literal(en_dash)) {
					spaces();
					return true;
				}
				At = from;
				return false;
			}

			/// "to" between spaces.
			auto to() noexcept -> bool
			{
				const auto from = At;
				if (spaces() > 0U && literal("to") && spaces() > 0U) {
					return true;
				}
				At = from;
				return false;
			}

			[[nodiscard]] auto next() const noexcept -> char { return At < Text.size() ? Text[At] : '\0'; }

			void reset(const std::size_t aAt) noexcept { At = aAt; }

		private:
			std::string_view Text;
			std::size_t      At = 0U;
		};

		struct lines
		{
			std::uint32_t First  = 0U;
			std::uint32_t Last   = 0U;
			bool          Before = false;
			std::size_t   End    = 0U;
		};

		/// " before the change", taken when it is there.
		void before_the_change(reader& aRead, lines& aLines) noexcept
		{
			const auto from = aRead.at();
			if (aRead.spaces() > 0U && aRead.literal("before the change")) {
				aLines.Before = true;
				aLines.End    = aRead.at();
				return;
			}
			aRead.reset(from);
		}

		/// "line 12", "lines 12-34", "lines 12 to 34", the word first.
		[[nodiscard]] auto worded(reader& aRead) noexcept -> std::optional<lines>
		{
			const auto from = aRead.at();
			if (!(aRead.literal("lines") || aRead.literal("line") || aRead.literal("Lines") || aRead.literal("Line")) || aRead.spaces() == 0U) {
				aRead.reset(from);
				return std::nullopt;
			}
			const auto first = aRead.number();
			if (!first) {
				aRead.reset(from);
				return std::nullopt;
			}
			auto found = lines{ .First = *first, .Last = *first, .End = aRead.at() };
			if (const auto range = aRead.at(); aRead.dash() || aRead.to()) {
				if (const auto last = aRead.number()) {
					found.Last = *last;
					found.End  = aRead.at();
				} else {
					aRead.reset(range);
				}
			}
			aRead.reset(found.End);
			return found;
		}

		/// The lines that follow a path, in any of the forms.
		[[nodiscard]] auto after_path(const std::string_view aText, const std::size_t aAt) noexcept -> std::optional<lines>
		{
			auto read  = reader{ aText, aAt };
			auto found = std::optional<lines>{ };
			if (read.literal(":")) {
				// `:12`, `:12-34`, `:12:5`.
				if (const auto first = read.number()) {
					found = lines{ .First = *first, .Last = *first, .End = read.at() };
					if (const auto range = read.at(); read.literal("-") || read.literal(en_dash)) {
						if (const auto last = read.number()) {
							found->Last = *last;
							found->End  = read.at();
						} else {
							read.reset(range);
						}
					} else if (read.literal(":") && read.number()) {
						found->End = read.at();
					}
					read.reset(found->End);
					if (letter(read.next())) {
						return std::nullopt;
					}
				}
			} else if (read.literal("#L")) {
				// `#L12`, `#L12-L34`.
				if (const auto first = read.number()) {
					found = lines{ .First = *first, .Last = *first, .End = read.at() };
					if (const auto range = read.at(); read.literal("-")) {
						static_cast<void>(read.literal("L"));
						if (const auto last = read.number()) {
							found->Last = *last;
							found->End  = read.at();
						} else {
							read.reset(range);
						}
					}
					read.reset(found->End);
					if (letter(read.next())) {
						return std::nullopt;
					}
				}
			} else if (read.spaces() > 0U) {
				// ` line 12`, ` (lines 12–34)`.
				const auto open = read.literal("(");
				found           = worded(read);
				if (found && open) {
					if (read.literal(")")) {
						found->End = read.at();
					} else {
						found.reset();
					}
				}
			}
			if (!found) {
				return std::nullopt;
			}
			read.reset(found->End);
			before_the_change(read, *found);
			found->Last = std::max(found->First, found->Last);
			return found;
		}

		[[nodiscard]] auto overlaps(const std::vector<agent_reference>& aFound, const std::size_t aBegin, const std::size_t aEnd) noexcept -> bool
		{
			return std::ranges::any_of(aFound, [&](const agent_reference& aKnown) { return aBegin < aKnown.Begin + aKnown.Length && aKnown.Begin < aEnd; });
		}
	} // namespace

	auto find_agent_references(const std::string_view aText, const bool aBare) -> std::vector<agent_reference>
	{
		auto found = std::vector<agent_reference>{ };
		for (auto at = std::size_t{ 0U }; at < aText.size();) {
			if (!path_byte(aText[at]) || aText[at] == '/' || (at > 0U && path_joined(aText[at - 1U]))) {
				++at;
				continue;
			}
			auto end = at;
			while (end < aText.size() && path_byte(aText[end])) {
				++end;
			}
			// A sentence's full stop, or a dash after the path, is not its own.
			auto last = end;
			while (last > at && (aText[last - 1U] == '.' || aText[last - 1U] == '-' || aText[last - 1U] == '/')) {
				--last;
			}
			// Emphasis by underscores is not part of the path.
			auto first = at;
			while (first < last && aText[first] == '_') {
				++first;
			}
			const auto path = aText.substr(first, last - first);
			if (valid_path(path)) {
				if (const auto lines = after_path(aText, last)) {
					found.push_back({ .Begin = first, .Length = lines->End - first, .Path = std::string{ path }, .First = lines->First, .Last = lines->Last, .Before = lines->Before });
					at = lines->End;
					continue;
				}
			}
			at = end;
		}
		if (aBare) {
			for (auto at = std::size_t{ 0U }; at < aText.size(); ++at) {
				if ((aText[at] != 'l' && aText[at] != 'L') || (at > 0U && (path_joined(aText[at - 1U])))) {
					continue;
				}
				auto read  = reader{ aText, at };
				auto lines = worded(read);
				if (!lines) {
					continue;
				}
				read.reset(lines->End);
				before_the_change(read, *lines);
				lines->Last = std::max(lines->First, lines->Last);
				if (!overlaps(found, at, lines->End)) {
					found.push_back({ .Begin = at, .Length = lines->End - at, .First = lines->First, .Last = lines->Last, .Before = lines->Before });
				}
				at = lines->End - 1U;
			}
		}
		std::ranges::sort(found, { }, &agent_reference::Begin);
		return found;
	}

	auto resolve_agent_reference(const std::string_view aPath, const agent_reference_files& aFiles) -> std::vector<std::string>
	{
		if (aPath.empty()) {
			return { };
		}
		auto       written = std::string{ aPath };
		const auto among   = [&written](const std::vector<std::string>& aList) {
			if (std::ranges::find(aList, written) != aList.end()) {
				return std::vector<std::string>{ written };
			}
			auto       matches = std::vector<std::string>{ };
			const auto suffix  = "/" + written;
			for (const auto& file : aList) {
				if (file.size() > suffix.size() && file.ends_with(suffix)) {
					matches.push_back(file);
				}
			}
			std::ranges::sort(matches);
			matches.erase(std::ranges::unique(matches).begin(), matches.end());
			return matches;
		};
		if (auto changed = among(aFiles.Changed); !changed.empty()) {
			return changed;
		}
		if (!aFiles.UnchangedOpenable) {
			return { };
		}
		return among(aFiles.Head);
	}

	namespace {
		struct markdown_span
		{
			std::size_t Begin = 0U;
			std::size_t End   = 0U;
		};

		[[nodiscard]] auto inside(const std::vector<markdown_span>& aSpans, const std::size_t aBegin, const std::size_t aEnd) noexcept -> bool
		{
			return std::ranges::any_of(aSpans, [&](const markdown_span& aSpan) { return aBegin < aSpan.End && aSpan.Begin < aEnd; });
		}

		/// Inline code spans of one line: a backtick run to the next run of the
		/// same length.
		[[nodiscard]] auto code_spans(const std::string_view aLine) -> std::vector<markdown_span>
		{
			auto spans = std::vector<markdown_span>{ };
			for (auto at = std::size_t{ 0U }; at < aLine.size();) {
				if (aLine[at] != '`') {
					++at;
					continue;
				}
				auto run = std::size_t{ 0U };
				while (at + run < aLine.size() && aLine[at + run] == '`') {
					++run;
				}
				auto close = at + run;
				auto shut  = std::optional<std::size_t>{ };
				while (close < aLine.size()) {
					if (aLine[close] != '`') {
						++close;
						continue;
					}
					auto length = std::size_t{ 0U };
					while (close + length < aLine.size() && aLine[close + length] == '`') {
						++length;
					}
					if (length == run) {
						shut = close + length;
						break;
					}
					close += length;
				}
				if (!shut) {
					at += run;
					continue;
				}
				spans.push_back({ at, *shut });
				at = *shut;
			}
			return spans;
		}

		/// Existing links of one line: `[text](target)`, `<target>` and bare
		/// web addresses.
		[[nodiscard]] auto link_spans(const std::string_view aLine) -> std::vector<markdown_span>
		{
			auto spans = std::vector<markdown_span>{ };
			for (auto at = std::size_t{ 0U }; at < aLine.size(); ++at) {
				if (aLine[at] == '[') {
					const auto close = aLine.find("](", at);
					const auto end   = close == std::string_view::npos ? close : aLine.find(')', close);
					if (end != std::string_view::npos && aLine.substr(at, close - at).find(']') == std::string_view::npos) {
						spans.push_back({ at, end + 1U });
						at = end;
					}
				} else if (aLine[at] == '<') {
					const auto end = aLine.find('>', at);
					if (end != std::string_view::npos && aLine.substr(at, end - at).find(' ') == std::string_view::npos) {
						spans.push_back({ at, end + 1U });
						at = end;
					}
				} else if (aLine.substr(at).starts_with("http://") || aLine.substr(at).starts_with("https://")) {
					auto end = at;
					while (end < aLine.size() && aLine[end] != ' ' && aLine[end] != '\t') {
						++end;
					}
					spans.push_back({ at, end });
					at = end;
				}
			}
			return spans;
		}

		/// A fence's marker when the line opens or closes one: up to three
		/// spaces, then three or more backticks or tildes.
		[[nodiscard]] auto fence_of(const std::string_view aLine) noexcept -> std::string_view
		{
			auto indent = std::size_t{ 0U };
			while (indent < aLine.size() && aLine[indent] == ' ') {
				++indent;
			}
			if (indent > 3U || indent == aLine.size() || (aLine[indent] != '`' && aLine[indent] != '~')) {
				return { };
			}
			auto run = std::size_t{ 0U };
			while (indent + run < aLine.size() && aLine[indent + run] == aLine[indent]) {
				++run;
			}
			return run >= 3U ? aLine.substr(indent, run) : std::string_view{ };
		}

		[[nodiscard]] auto trimmed(const std::string_view aText) noexcept -> std::string_view
		{
			auto begin = std::size_t{ 0U };
			auto end   = aText.size();
			while (begin < end && aText[begin] == ' ') {
				++begin;
			}
			while (end > begin && aText[end - 1U] == ' ') {
				--end;
			}
			return aText.substr(begin, end - begin);
		}
	} // namespace

	auto agent_markdown_references(const std::string_view aMarkdown, const bool aBare) -> std::vector<agent_markdown_reference>
	{
		auto found = std::vector<agent_markdown_reference>{ };
		auto fence = std::string_view{ };
		for (auto start = std::size_t{ 0U }; start <= aMarkdown.size();) {
			auto end = aMarkdown.find('\n', start);
			if (end == std::string_view::npos) {
				end = aMarkdown.size();
			}
			const auto line   = aMarkdown.substr(start, end - start);
			const auto marker = fence_of(line);
			const auto alone  = [&](const std::size_t aBegin, const std::size_t aLength) { return trimmed(line).size() == aLength && line.find_first_not_of(" \t\r") == aBegin; };
			if (!fence.empty()) {
				if (!marker.empty() && marker.front() == fence.front() && marker.size() >= fence.size() && trimmed(line) == marker) {
					fence = { };
				}
			} else if (!marker.empty()) {
				fence = marker;
			} else {
				const auto code     = code_spans(line);
				const auto existing = link_spans(line);
				// A code span whose whole content is one reference: the
				// reference covers the span.
				for (const auto& span : code) {
					if (inside(existing, span.Begin, span.End)) {
						continue;
					}
					auto run = std::size_t{ 0U };
					while (span.Begin + run < span.End && line[span.Begin + run] == '`') {
						++run;
					}
					const auto content = trimmed(line.substr(span.Begin + run, span.End - span.Begin - 2U * run));
					const auto inner   = find_agent_references(content, aBare);
					if (inner.size() == 1U && inner.front().Begin == 0U && inner.front().Length == content.size()) {
						auto reference   = inner.front();
						reference.Begin  = start + span.Begin;
						reference.Length = span.End - span.Begin;
						found.push_back({ .Reference = std::move(reference), .Code = true, .Alone = alone(span.Begin, span.End - span.Begin) });
					}
				}
				for (auto reference : find_agent_references(line, aBare)) {
					if (inside(code, reference.Begin, reference.Begin + reference.Length) || inside(existing, reference.Begin, reference.Begin + reference.Length)) {
						continue;
					}
					const auto lone = alone(reference.Begin, reference.Length);
					reference.Begin += start;
					found.push_back({ .Reference = std::move(reference), .Code = false, .Alone = lone });
				}
			}
			if (end == aMarkdown.size()) {
				break;
			}
			start = end + 1U;
		}
		std::ranges::sort(found, { }, [](const agent_markdown_reference& aFound) { return aFound.Reference.Begin; });
		return found;
	}

	auto agent_markdown_reference_links(const std::string_view aMarkdown, const agent_reference_files& aFiles, const std::string_view aContextFile) -> std::vector<agent_reference_link>
	{
		auto       links   = std::vector<agent_reference_link>{ };
		const auto context = aContextFile.empty() ? std::vector<std::string>{ } : resolve_agent_reference(aContextFile, aFiles);
		for (const auto& each : agent_markdown_references(aMarkdown, !aContextFile.empty())) {
			const auto& reference = each.Reference;
			auto        paths     = reference.Path.empty() ? context : resolve_agent_reference(reference.Path, aFiles);
			if (!paths.empty()) {
				links.push_back(agent_reference_link{ .Begin = reference.Begin, .Length = reference.Length, .Paths = std::move(paths), .First = reference.First, .Last = reference.Last, .Before = reference.Before });
			}
		}
		return links;
	}
} // namespace bpr::core
