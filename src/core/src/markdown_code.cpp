#include "bpr/core/markdown_code.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		constexpr auto tab_width = std::size_t{ 4U };

		[[nodiscard]] constexpr auto space_or_tab(const char aChar) noexcept -> bool
		{
			return aChar == ' ' || aChar == '\t';
		}

		[[nodiscard]] auto blank(const std::string_view aText) noexcept -> bool
		{
			return std::ranges::all_of(aText, [](const char aChar) { return space_or_tab(aChar) || aChar == '\r'; });
		}

		/// \brief A line's text from a byte offset, and the column that offset
		/// stands at, so a tab counts to the next multiple of four.
		struct line_cursor
		{
			std::string_view Text;
			std::size_t      At     = 0U;
			std::size_t      Column = 0U;

			[[nodiscard]] auto rest() const noexcept -> std::string_view { return Text.substr(At); }
			[[nodiscard]] auto done() const noexcept -> bool { return At >= Text.size(); }
			[[nodiscard]] auto peek() const noexcept -> char { return done() ? '\0' : Text[At]; }

			/// \brief The columns of whitespace from here.
			[[nodiscard]] auto indentation() const noexcept -> std::size_t
			{
				auto column = Column;
				for (auto at = At; at < Text.size() && space_or_tab(Text[at]); ++at) {
					column = Text[at] == '\t' ? column + tab_width - column % tab_width : column + 1U;
				}
				return column - Column;
			}

			/// \brief Consumes whitespace until `aColumns` more columns are
			/// passed or none is left; a tab that crosses the bound is taken.
			void consume_columns(const std::size_t aColumns) noexcept
			{
				const auto target = Column + aColumns;
				while (Column < target && At < Text.size() && space_or_tab(Text[At])) {
					Column = Text[At] == '\t' ? Column + tab_width - Column % tab_width : Column + 1U;
					++At;
				}
			}

			void consume(const std::size_t aBytes) noexcept
			{
				At += aBytes;
				Column += aBytes;
			}
		};

		struct container
		{
			bool        Quote  = false;
			std::size_t Indent = 0U; ///< An item's content column, from where the item's line starts.
		};

		struct fence_state
		{
			char        Character = '`';
			std::size_t Count     = 0U;
			std::size_t Indent    = 0U;
			std::size_t Depth     = 0U;
		};

		/// \brief Consumes one block quote marker: up to three spaces, `>`
		/// and one following space or tab.
		[[nodiscard]] auto quote_marker(line_cursor& aLine) -> bool
		{
			if (aLine.indentation() > 3U) {
				return false;
			}
			auto probe = aLine;
			probe.consume_columns(3U);
			if (probe.peek() != '>') {
				return false;
			}
			probe.consume(1U);
			if (space_or_tab(probe.peek())) {
				probe.consume_columns(1U);
			}
			aLine = probe;
			return true;
		}

		[[nodiscard]] auto thematic_break(const std::string_view aRest) -> bool
		{
			if (aRest.empty() || (aRest[0] != '-' && aRest[0] != '*' && aRest[0] != '_')) {
				return false;
			}
			auto count = std::size_t{ 0U };
			for (const auto character : aRest) {
				if (character == aRest[0]) {
					++count;
				} else if (!space_or_tab(character) && character != '\r') {
					return false;
				}
			}
			return count >= 3U;
		}

		/// \brief Consumes one list item marker and the spaces after it, and
		/// returns the item's content column counted from the line position
		/// before it; none when the line starts no item.
		[[nodiscard]] auto list_marker(line_cursor& aLine) -> std::optional<std::size_t>
		{
			if (aLine.indentation() > 3U) {
				return std::nullopt;
			}
			const auto start = aLine.Column;
			auto       probe = aLine;
			probe.consume_columns(3U);
			const auto rest = probe.rest();
			if (rest.empty() || thematic_break(rest)) {
				return std::nullopt;
			}
			auto width = std::size_t{ 0U };
			if (rest[0] == '-' || rest[0] == '+' || rest[0] == '*') {
				width = 1U;
			} else {
				while (width < rest.size() && width < 9U && rest[width] >= '0' && rest[width] <= '9') {
					++width;
				}
				if (width == 0U || width == rest.size() || (rest[width] != '.' && rest[width] != ')')) {
					return std::nullopt;
				}
				++width;
			}
			if (width < rest.size() && !space_or_tab(rest[width])) {
				return std::nullopt;
			}
			probe.consume(width);
			if (probe.done() || blank(probe.rest())) {
				// An item that starts blank: its content is one column past the marker.
				const auto content = probe.Column + 1U - start;
				probe.consume_columns(probe.indentation());
				aLine = probe;
				return content;
			}
			const auto spaces = probe.indentation();
			probe.consume_columns(spaces >= 1U && spaces <= 4U ? spaces : 1U);
			aLine = probe;
			return aLine.Column - start;
		}

		[[nodiscard]] auto fence_opening(const line_cursor& aLine) -> std::optional<std::pair<fence_state, std::string_view>>
		{
			const auto indent = aLine.indentation();
			if (indent > 3U) {
				return std::nullopt;
			}
			auto probe = aLine;
			probe.consume_columns(3U);
			const auto rest = probe.rest();
			if (rest.empty() || (rest[0] != '`' && rest[0] != '~')) {
				return std::nullopt;
			}
			auto count = std::size_t{ 0U };
			while (count < rest.size() && rest[count] == rest[0]) {
				++count;
			}
			auto info = rest.substr(count);
			if (count < 3U || (rest[0] == '`' && info.find('`') != std::string_view::npos)) {
				return std::nullopt;
			}
			while (!info.empty() && (space_or_tab(info.front()))) {
				info.remove_prefix(1U);
			}
			while (!info.empty() && (space_or_tab(info.back()) || info.back() == '\r')) {
				info.remove_suffix(1U);
			}
			return std::pair{ fence_state{ .Character = rest[0], .Count = count, .Indent = indent }, info };
		}

		[[nodiscard]] auto fence_closes(const line_cursor& aLine, const fence_state& aFence) -> bool
		{
			if (aLine.indentation() > 3U) {
				return false;
			}
			auto probe = aLine;
			probe.consume_columns(3U);
			const auto rest  = probe.rest();
			auto       count = std::size_t{ 0U };
			while (count < rest.size() && rest[count] == aFence.Character) {
				++count;
			}
			return count >= aFence.Count && blank(rest.substr(count));
		}

		/// \brief Removes up to `aSpaces` leading spaces, as CommonMark removes
		/// a fence's own indentation from its content.
		[[nodiscard]] auto content_line(std::string_view aText, const std::size_t aSpaces) -> std::string
		{
			auto removed = std::size_t{ 0U };
			while (removed < aSpaces && removed < aText.size() && aText[removed] == ' ') {
				++removed;
			}
			aText.remove_prefix(removed);
			if (aText.ends_with('\r')) {
				aText.remove_suffix(1U);
			}
			return std::string{ aText };
		}

		[[nodiscard]] auto lower(const std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			for (auto& character : result) {
				if (character >= 'A' && character <= 'Z') {
					character = static_cast<char>(character - 'A' + 'a');
				}
			}
			return result;
		}

		struct language_entry
		{
			std::string_view Alias;
			std::string_view Name;
			std::string_view Selector;
		};

		/// \brief GitHub's Linguist names and aliases for the languages its
		/// Markdown highlights, each with a file name the packaged syntax
		/// definitions select that language by. Verified against GitHub's
		/// Markdown API (`tests/fixtures/markdown/code-blocks-v1/languages.md`).
		/// A language the packaged definitions do not cover resolves to plain
		/// text there and stays plain.
		constexpr auto languages = std::to_array<language_entry>({
			{ "adoc", "AsciiDoc", "code.adoc" },
			{ "asciidoc", "AsciiDoc", "code.adoc" },
			{ "asm", "Assembly", "code.asm" },
			{ "assembly", "Assembly", "code.asm" },
			{ "awk", "Awk", "code.awk" },
			{ "bash", "Shell", "code.sh" },
			{ "bat", "Batchfile", "code.bat" },
			{ "batch", "Batchfile", "code.bat" },
			{ "batchfile", "Batchfile", "code.bat" },
			{ "c", "C", "code.c" },
			{ "c#", "C#", "code.cs" },
			{ "c++", "C++", "code.cpp" },
			{ "cc", "C++", "code.cpp" },
			{ "cjs", "JavaScript", "code.js" },
			{ "clj", "Clojure", "code.clj" },
			{ "clojure", "Clojure", "code.clj" },
			{ "cmake", "CMake", "CMakeLists.txt" },
			{ "cmd", "Batchfile", "code.bat" },
			{ "containerfile", "Dockerfile", "Dockerfile" },
			{ "cpp", "C++", "code.cpp" },
			{ "cs", "C#", "code.cs" },
			{ "csharp", "C#", "code.cs" },
			{ "css", "CSS", "code.css" },
			{ "csv", "CSV", "code.csv" },
			{ "cts", "TypeScript", "code.ts" },
			{ "cxx", "C++", "code.cpp" },
			{ "dart", "Dart", "code.dart" },
			{ "django", "Jinja", "code.jinja" },
			{ "dockerfile", "Dockerfile", "Dockerfile" },
			{ "dosbatch", "Batchfile", "code.bat" },
			{ "elixir", "Elixir", "code.ex" },
			{ "erl", "Erlang", "code.erl" },
			{ "erlang", "Erlang", "code.erl" },
			{ "ex", "Elixir", "code.ex" },
			{ "exs", "Elixir", "code.ex" },
			{ "f#", "F#", "code.fs" },
			{ "fish", "fish", "code.fish" },
			{ "fsharp", "F#", "code.fs" },
			{ "glsl", "GLSL", "code.glsl" },
			{ "go", "Go", "code.go" },
			{ "golang", "Go", "code.go" },
			{ "gql", "GraphQL", "code.graphql" },
			{ "gradle", "Gradle", "build.gradle" },
			{ "graphql", "GraphQL", "code.graphql" },
			{ "groovy", "Groovy", "code.groovy" },
			{ "h", "C", "code.c" },
			{ "h++", "C++", "code.cpp" },
			{ "haskell", "Haskell", "code.hs" },
			{ "hcl", "HCL", "code.tf" },
			{ "hh", "C++", "code.cpp" },
			{ "hlsl", "HLSL", "code.hlsl" },
			{ "hpp", "C++", "code.cpp" },
			{ "hs", "Haskell", "code.hs" },
			{ "htm", "HTML", "code.html" },
			{ "html", "HTML", "code.html" },
			{ "http", "HTTP", "code.http" },
			{ "hxx", "C++", "code.cpp" },
			{ "ini", "INI", "code.ini" },
			{ "java", "Java", "code.java" },
			{ "javascript", "JavaScript", "code.js" },
			{ "jinja", "Jinja", "code.jinja" },
			{ "jinja2", "Jinja", "code.jinja" },
			{ "js", "JavaScript", "code.js" },
			{ "json", "JSON", "code.json" },
			{ "json5", "JavaScript", "code.js" },
			{ "jsonc", "JSON with Comments", "code.jsonc" },
			{ "jsx", "JavaScript", "code.jsx" },
			{ "kotlin", "Kotlin", "code.kt" },
			{ "kt", "Kotlin", "code.kt" },
			{ "kts", "Kotlin", "code.kts" },
			{ "latex", "TeX", "code.tex" },
			{ "lua", "Lua", "code.lua" },
			{ "make", "Makefile", "Makefile" },
			{ "makefile", "Makefile", "Makefile" },
			{ "markdown", "Markdown", "code.md" },
			{ "md", "Markdown", "code.md" },
			{ "mf", "Makefile", "Makefile" },
			{ "mjs", "JavaScript", "code.js" },
			{ "ml", "OCaml", "code.ml" },
			{ "mts", "TypeScript", "code.ts" },
			{ "nasm", "Assembly", "code.asm" },
			{ "nginx", "Nginx", "nginx.conf" },
			{ "nim", "Nim", "code.nim" },
			{ "node", "JavaScript", "code.js" },
			{ "nu", "Nushell", "code.nu" },
			{ "nushell", "Nushell", "code.nu" },
			{ "objc", "Objective-C", "code.m" },
			{ "objective-c", "Objective-C", "code.m" },
			{ "objectivec", "Objective-C", "code.m" },
			{ "ocaml", "OCaml", "code.ml" },
			{ "perl", "Perl", "code.pl" },
			{ "php", "PHP", "code.php" },
			{ "pl", "Perl", "code.pl" },
			{ "posh", "PowerShell", "code.ps1" },
			{ "powershell", "PowerShell", "code.ps1" },
			{ "proto", "Protocol Buffer", "code.proto" },
			{ "protobuf", "Protocol Buffer", "code.proto" },
			{ "ps1", "PowerShell", "code.ps1" },
			{ "pwsh", "PowerShell", "code.ps1" },
			{ "py", "Python", "code.py" },
			{ "python", "Python", "code.py" },
			{ "python3", "Python", "code.py" },
			{ "r", "R", "code.r" },
			{ "rake", "Ruby", "code.rb" },
			{ "rb", "Ruby", "code.rb" },
			{ "regex", "Regular Expression", "code.regex" },
			{ "restructuredtext", "reStructuredText", "code.rst" },
			{ "rs", "Rust", "code.rs" },
			{ "rst", "reStructuredText", "code.rst" },
			{ "ruby", "Ruby", "code.rb" },
			{ "rust", "Rust", "code.rs" },
			{ "scala", "Scala", "code.scala" },
			{ "scss", "SCSS", "code.scss" },
			{ "sed", "sed", "code.sed" },
			{ "sh", "Shell", "code.sh" },
			{ "shell", "Shell", "code.sh" },
			{ "shell-script", "Shell", "code.sh" },
			{ "sql", "SQL", "code.sql" },
			{ "svelte", "Svelte", "code.svelte" },
			{ "svg", "SVG", "code.svg" },
			{ "swift", "Swift", "code.swift" },
			{ "terraform", "HCL", "code.tf" },
			{ "tex", "TeX", "code.tex" },
			{ "tf", "HCL", "code.tf" },
			{ "toml", "TOML", "code.toml" },
			{ "ts", "TypeScript", "code.ts" },
			{ "tsv", "TSV", "code.tsv" },
			{ "tsx", "TSX", "code.tsx" },
			{ "twig", "Twig", "code.twig" },
			{ "typescript", "TypeScript", "code.ts" },
			{ "vim", "Vim Script", "code.vim" },
			{ "viml", "Vim Script", "code.vim" },
			{ "vimscript", "Vim Script", "code.vim" },
			{ "vue", "Vue", "code.vue" },
			{ "wasm", "WebAssembly", "code.wat" },
			{ "wast", "WebAssembly", "code.wat" },
			{ "wat", "WebAssembly", "code.wat" },
			{ "xml", "XML", "code.xml" },
			{ "xsd", "XML", "code.xsd" },
			{ "xsl", "XML", "code.xsl" },
			{ "yaml", "YAML", "code.yaml" },
			{ "yml", "YAML", "code.yaml" },
			{ "zig", "Zig", "code.zig" },
			{ "zsh", "Shell", "code.sh" },
		});

		static_assert(std::ranges::is_sorted(languages, { }, &language_entry::Alias), "the language table is searched by alias");

		[[nodiscard]] auto starts(const std::string_view aText, const std::string_view aPrefix) noexcept -> bool
		{
			return aText.substr(0U, aPrefix.size()) == aPrefix;
		}

		[[nodiscard]] auto digits(const std::string_view aText, std::size_t& aAt) noexcept -> bool
		{
			const auto from = aAt;
			while (aAt < aText.size() && aText[aAt] >= '0' && aText[aAt] <= '9') {
				++aAt;
			}
			return aAt > from;
		}

		/// `^\d+(,\d+)*(a|d|c)\d+(,\d+)*$`
		[[nodiscard]] auto normal_range(const std::string_view aLine) noexcept -> bool
		{
			auto       at      = std::size_t{ 0U };
			const auto numbers = [&] {
				if (!digits(aLine, at)) {
					return false;
				}
				while (at < aLine.size() && aLine[at] == ',') {
					++at;
					if (!digits(aLine, at)) {
						return false;
					}
				}
				return true;
			};
			if (!numbers() || at >= aLine.size() || (aLine[at] != 'a' && aLine[at] != 'd' && aLine[at] != 'c')) {
				return false;
			}
			++at;
			return numbers() && at == aLine.size();
		}

		/// `^diff (-|\S+\s+\S+).*$`
		[[nodiscard]] auto command_header(const std::string_view aLine) noexcept -> bool
		{
			if (!starts(aLine, "diff ")) {
				return false;
			}
			const auto rest = aLine.substr(5U);
			if (starts(rest, "-")) {
				return true;
			}
			const auto space = [](const char aChar) { return aChar == ' ' || aChar == '\t' || aChar == '\f' || aChar == '\v'; };
			auto       at    = std::size_t{ 0U };
			const auto first = at;
			while (at < rest.size() && !space(rest[at])) {
				++at;
			}
			if (at == first || at == rest.size()) {
				return false;
			}
			while (at < rest.size() && space(rest[at])) {
				++at;
			}
			return at < rest.size();
		}

		[[nodiscard]] auto word_byte(const char aChar) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aChar);
			return (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_' || byte >= 0x80U;
		}

		/// \brief Splits a line into words, runs of white space and single
		/// other characters, by their byte ranges.
		[[nodiscard]] auto tokens(const std::string_view aText) -> std::vector<std::pair<std::size_t, std::size_t>>
		{
			auto result = std::vector<std::pair<std::size_t, std::size_t>>{ };
			for (auto at = std::size_t{ 0U }; at < aText.size();) {
				auto end = at + 1U;
				if (word_byte(aText[at])) {
					while (end < aText.size() && word_byte(aText[end])) {
						++end;
					}
				} else if (space_or_tab(aText[at])) {
					while (end < aText.size() && space_or_tab(aText[end])) {
						++end;
					}
				}
				result.emplace_back(at, end);
				at = end;
			}
			return result;
		}

		/// \brief The changed middle of two lines, word by word: what is left
		/// once the words both start with and the words both end with are set
		/// aside.
		void mark_changed_words(markdown_suggestion_row& aRemoved, markdown_suggestion_row& aAdded)
		{
			if (aRemoved.Text == aAdded.Text) {
				return;
			}
			const auto left  = tokens(aRemoved.Text);
			const auto right = tokens(aAdded.Text);
			const auto same  = [&](const std::pair<std::size_t, std::size_t> aLeft, const std::pair<std::size_t, std::size_t> aRight) {
				return std::string_view{ aRemoved.Text }.substr(aLeft.first, aLeft.second - aLeft.first)
					== std::string_view{ aAdded.Text }.substr(aRight.first, aRight.second - aRight.first);
			};
			auto prefix = std::size_t{ 0U };
			while (prefix < left.size() && prefix < right.size() && same(left[prefix], right[prefix])) {
				++prefix;
			}
			auto suffix = std::size_t{ 0U };
			while (suffix < left.size() - prefix && suffix < right.size() - prefix && same(left[left.size() - 1U - suffix], right[right.size() - 1U - suffix])) {
				++suffix;
			}
			const auto range = [](const std::vector<std::pair<std::size_t, std::size_t>>& aTokens, const std::size_t aPrefix, const std::size_t aSuffix, const std::size_t aSize) {
				const auto begin = aPrefix < aTokens.size() ? aTokens[aPrefix].first : aSize;
				const auto end   = aSuffix > 0U ? aTokens[aTokens.size() - aSuffix].first : aSize;
				return std::pair{ begin, std::max(begin, end) };
			};
			std::tie(aRemoved.ChangedBegin, aRemoved.ChangedEnd) = range(left, prefix, suffix, aRemoved.Text.size());
			std::tie(aAdded.ChangedBegin, aAdded.ChangedEnd)     = range(right, prefix, suffix, aAdded.Text.size());
		}
	} // namespace

	auto markdown_fenced_code::text() const -> std::string
	{
		auto result = std::string{ };
		for (std::size_t index = 0U; index < Lines.size(); ++index) {
			if (index > 0U) {
				result += '\n';
			}
			result += Lines[index];
		}
		return result;
	}

	auto scan_markdown_fenced_code(const std::string_view aBody) -> std::vector<markdown_fenced_code>
	{
		auto       blocks     = std::vector<markdown_fenced_code>{ };
		auto       containers = std::vector<container>{ };
		auto       fence      = std::optional<fence_state>{ };
		const auto close      = [&](const bool aClosed) {
			blocks.back().Closed = aClosed;
			fence.reset();
		};
		for (auto start = std::size_t{ 0U }; start <= aBody.size();) {
			auto       end  = aBody.find('\n', start);
			const auto last = end == std::string_view::npos;
			if (last) {
				end = aBody.size();
			}
			auto line = line_cursor{ .Text = aBody.substr(start, end - start) };
			start     = end + 1U;
			if (line.Text.ends_with('\r')) {
				line.Text.remove_suffix(1U);
			}
			const auto empty = blank(line.Text);
			// The containers this line continues.
			auto matched = std::size_t{ 0U };
			for (const auto& open : containers) {
				if (open.Quote) {
					if (!quote_marker(line)) {
						break;
					}
				} else if (blank(line.rest()) || line.indentation() >= open.Indent) {
					// An item continues on a blank line or one indented to its content.
					line.consume_columns(open.Indent);
				} else {
					break;
				}
				++matched;
			}
			if (fence) {
				if (matched < fence->Depth) {
					// The container holding the fence ended, and the fence with it.
					close(false);
				} else if (fence_closes(line, *fence)) {
					close(true);
					containers.resize(matched);
					if (last) {
						break;
					}
					continue;
				} else {
					blocks.back().Lines.push_back(content_line(line.rest(), fence->Indent));
					if (last) {
						break;
					}
					continue;
				}
			}
			containers.resize(matched);
			if (!empty) {
				while (containers.size() < markdown_code_container_depth) {
					if (quote_marker(line)) {
						containers.push_back({ .Quote = true });
					} else if (const auto indent = list_marker(line)) {
						containers.push_back({ .Quote = false, .Indent = *indent });
					} else {
						break;
					}
				}
				if (const auto opening = fence_opening(line); opening && blocks.size() < markdown_code_blocks_per_body) {
					fence         = opening->first;
					fence->Depth  = containers.size();
					auto block    = markdown_fenced_code{ };
					block.Ordinal = static_cast<std::uint32_t>(blocks.size());
					block.Info    = std::string{ opening->second };
					blocks.push_back(std::move(block));
				}
			}
			if (last) {
				break;
			}
		}
		if (fence) {
			close(false);
		}
		return blocks;
	}

	auto markdown_code_language_of(const std::string_view aInfo) -> markdown_code_language
	{
		auto word = aInfo;
		while (!word.empty() && space_or_tab(word.front())) {
			word.remove_prefix(1U);
		}
		word = word.substr(0U, std::min({ word.find(' '), word.find('\t'), word.find(','), word.size() }));
		if (word == "suggestion") {
			return { .Kind = markdown_code_kind::suggestion, .Name = "Suggestion" };
		}
		auto token = lower(word);
		if (token.starts_with('.')) {
			token.erase(0U, 1U);
		}
		if (token == "diff" || token == "patch" || token == "udiff") {
			return { .Kind = markdown_code_kind::diff, .Name = "Diff" };
		}
		const auto found = std::ranges::lower_bound(languages, std::string_view{ token }, { }, &language_entry::Alias);
		if (token.empty() || found == languages.end() || found->Alias != token) {
			return { };
		}
		return { .Kind = markdown_code_kind::language, .Name = found->Name, .Selector = found->Selector };
	}

	auto markdown_diff_marks(const std::string_view aLine) -> std::vector<markdown_diff_mark>
	{
		const auto whole = [&aLine](const markdown_diff_mark_kind aKind) {
			return std::vector<markdown_diff_mark>{ { .Begin = 0U, .Length = aLine.size(), .Kind = aKind } };
		};
		using enum markdown_diff_mark_kind;
		if (aLine == "---" || aLine == std::string(15U, '*') || aLine == std::string(67U, '=')) {
			return whole(separator);
		}
		if (normal_range(aLine)) {
			return whole(range);
		}
		if (starts(aLine, "@@")) {
			// `^(@@)\s*(.+?)\s*(@@)`: the first `@@` that leaves a character
			// between the two.
			if (const auto close = aLine.find("@@", 3U); close != std::string_view::npos) {
				return { { .Begin = 0U, .Length = close + 2U, .Kind = range } };
			}
		}
		if (aLine.size() >= 10U && ((starts(aLine, "--- ") && aLine.ends_with(" ----")) || (starts(aLine, "*** ") && aLine.ends_with(" ****")))) {
			return whole(range);
		}
		if (starts(aLine, "diff --git a/") || command_header(aLine)) {
			return whole(header);
		}
		if ((starts(aLine, "--- ") || starts(aLine, "*** ")) && aLine.size() > 4U) {
			return whole(deleted);
		}
		if (starts(aLine, "==== ")) {
			// `^(={4}) .+(?= - )`, then the to-file half ` - … ====`.
			if (const auto split = aLine.rfind(" - "); split != std::string_view::npos && split >= 6U) {
				auto marks = std::vector<markdown_diff_mark>{ { .Begin = 0U, .Length = split, .Kind = deleted } };
				if (aLine.ends_with("====") && aLine.size() >= split + 8U) {
					marks.push_back({ .Begin = split, .Length = aLine.size() - split, .Kind = inserted });
				}
				return marks;
			}
		}
		if (starts(aLine, "+++ ") && aLine.size() > 4U) {
			return whole(inserted);
		}
		if (aLine == ">" || starts(aLine, "> ") || starts(aLine, "+")) {
			return whole(inserted);
		}
		if (starts(aLine, "!")) {
			return whole(changed);
		}
		if (aLine == "<" || starts(aLine, "< ") || starts(aLine, "-")) {
			return whole(deleted);
		}
		return { };
	}

	auto markdown_suggestion_rows(const std::span<const std::string> aSuggested, const std::optional<markdown_suggestion_context>& aContext)
		-> std::vector<markdown_suggestion_row>
	{
		auto rows = std::vector<markdown_suggestion_row>{ };
		if (aContext) {
			rows.reserve(aContext->Lines.size() + aSuggested.size());
			for (std::size_t index = 0U; index < aContext->Lines.size(); ++index) {
				auto row   = markdown_suggestion_row{ };
				row.Number = aContext->FirstLine + static_cast<std::uint32_t>(index);
				row.Text   = aContext->Lines[index];
				rows.push_back(std::move(row));
			}
		}
		const auto removed = rows.size();
		for (std::size_t index = 0U; index < aSuggested.size(); ++index) {
			auto row  = markdown_suggestion_row{ };
			row.Added = true;
			if (aContext) {
				row.Number = aContext->FirstLine + static_cast<std::uint32_t>(index);
			}
			row.Text = aSuggested[index];
			rows.push_back(std::move(row));
		}
		if (aContext && removed == aSuggested.size()) {
			for (std::size_t index = 0U; index < removed; ++index) {
				mark_changed_words(rows[index], rows[removed + index]);
			}
		}
		return rows;
	}
} // namespace bpr::core
