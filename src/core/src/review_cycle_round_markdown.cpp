#include "bpr/core/review_cycle_round_markdown.hpp"

#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		auto sha256_of(const std::string_view aBytes) -> std::array<std::uint8_t, 32>
		{
			return sha256({ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() });
		}

		/// Consecutive backticks, longest run.
		auto longest_backtick_run(const std::string_view aText) noexcept -> std::size_t
		{
			auto longest = std::size_t{ 0 };
			auto current = std::size_t{ 0 };
			for (const auto symbol : aText) {
				current = symbol == '`' ? current + 1U : 0U;
				longest = std::max(longest, current);
			}
			return longest;
		}

		/// A fenced block longer than any backtick run inside it.
		auto fenced(const std::string_view aText, const std::string_view aLanguage) -> std::string
		{
			const auto fence  = std::string(std::max<std::size_t>(3U, longest_backtick_run(aText) + 1U), '`');
			auto       result = fence + std::string{ aLanguage } + "\n" + std::string{ aText };
			if (aText.empty() || aText.back() != '\n') {
				result.push_back('\n');
			}
			return result + fence + "\n";
		}

		/// Inline code that no backtick in the text can close early.
		auto inline_code(const std::string_view aText) -> std::string
		{
			const auto run = longest_backtick_run(aText);
			if (run == 0U) {
				return "`" + std::string{ aText } + "`";
			}
			const auto ticks = std::string(run + 1U, '`');
			return ticks + " " + std::string{ aText } + " " + ticks;
		}

		auto lowercase(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const char aSymbol) { return aSymbol >= 'A' && aSymbol <= 'Z' ? static_cast<char>(aSymbol - 'A' + 'a') : aSymbol; });
			return aText;
		}

		/// A fence language for a path's extension, or none.
		auto language_of(const std::optional<std::string>& aPath) -> std::string_view
		{
			if (!aPath) {
				return { };
			}
			const auto slash = aPath->find_last_of('/');
			const auto name  = lowercase(aPath->substr(slash == std::string::npos ? 0U : slash + 1U));
			if (name == "cmakelists.txt") {
				return "cmake";
			}
			const auto dot = name.find_last_of('.');
			if (dot == std::string::npos) {
				return { };
			}
			const auto extension = std::string_view{ name }.substr(dot + 1U);
			struct entry
			{
				std::string_view Extension;
				std::string_view Language;
			};
			static constexpr auto languages = std::array{
				entry{ "cpp", "cpp" },
				entry{ "cc", "cpp" },
				entry{ "cxx", "cpp" },
				entry{ "hpp", "cpp" },
				entry{ "hh", "cpp" },
				entry{ "hxx", "cpp" },
				entry{ "h", "cpp" },
				entry{ "ipp", "cpp" },
				entry{ "inl", "cpp" },
				entry{ "c", "c" },
				entry{ "py", "python" },
				entry{ "js", "javascript" },
				entry{ "mjs", "javascript" },
				entry{ "ts", "typescript" },
				entry{ "rs", "rust" },
				entry{ "go", "go" },
				entry{ "java", "java" },
				entry{ "cs", "csharp" },
				entry{ "json", "json" },
				entry{ "md", "markdown" },
				entry{ "yml", "yaml" },
				entry{ "yaml", "yaml" },
				entry{ "cmake", "cmake" },
				entry{ "qml", "qml" },
			};
			const auto found = std::ranges::find(languages, extension, &entry::Extension);
			return found == languages.end() ? std::string_view{ } : found->Language;
		}

		/// Replaces each invalid UTF-8 sequence with U+FFFD, for display only.
		auto displayable(const std::string_view aBytes) -> std::string
		{
			if (valid_cycle_utf8(aBytes)) {
				return std::string{ aBytes };
			}
			auto result = std::string{ };
			for (std::size_t index = 0; index < aBytes.size();) {
				auto length = std::size_t{ 0 };
				for (std::size_t candidate = 1; candidate <= 4U && index + candidate <= aBytes.size(); ++candidate) {
					if (valid_cycle_utf8(aBytes.substr(index, candidate))) {
						length = candidate;
						break;
					}
				}
				if (length == 0U) {
					result.append("\xEF\xBF\xBD");
					++index;
					continue;
				}
				result.append(aBytes.substr(index, length));
				index += length;
			}
			return result;
		}

		auto has_control(const std::string_view aBytes) noexcept -> bool
		{
			return std::ranges::any_of(aBytes, [](const char aSymbol) {
				const auto byte = static_cast<unsigned char>(aSymbol);
				return (byte < 0x20U && aSymbol != '\t' && aSymbol != '\n') || byte == 0x7FU;
			});
		}

		auto has_trailing_space(const std::string_view aBytes) noexcept -> bool
		{
			for (std::size_t index = 0; index < aBytes.size(); ++index) {
				const auto last = index + 1U == aBytes.size() || aBytes[index + 1U] == '\n';
				if ((aBytes[index] == ' ' || aBytes[index] == '\t') && last) {
					return true;
				}
			}
			return false;
		}

		/// Bytes a fence would not carry exactly as a copy/paste patch.
		auto needs_exact_bytes(const std::string_view aBytes) noexcept -> bool
		{
			return !valid_cycle_utf8(aBytes) || has_control(aBytes) || has_trailing_space(aBytes) || (!aBytes.empty() && aBytes.back() != '\n');
		}

		auto is_space(const char aSymbol) noexcept -> bool
		{
			return aSymbol == ' ' || aSymbol == '\t' || aSymbol == '\r';
		}

		/// A fence opener or closer: up to three spaces, three or more backticks or tildes.
		auto fence_marker(const std::string_view aLine) noexcept -> std::optional<std::pair<char, std::size_t>>
		{
			auto indent = std::size_t{ 0 };
			while (indent < aLine.size() && indent < 4U && aLine[indent] == ' ') {
				++indent;
			}
			if (indent > 3U || indent >= aLine.size() || (aLine[indent] != '`' && aLine[indent] != '~')) {
				return std::nullopt;
			}
			const auto symbol = aLine[indent];
			auto       count  = std::size_t{ 0 };
			while (indent + count < aLine.size() && aLine[indent + count] == symbol) {
				++count;
			}
			if (count < 3U || (symbol == '`' && aLine.substr(indent + count).find('`') != std::string_view::npos)) {
				return std::nullopt;
			}
			return std::pair{ symbol, count };
		}

		/// A level-one or level-two ATX heading, or a setext `=` underline.
		auto top_heading(const std::string_view aLine, const bool aAfterText) noexcept -> bool
		{
			auto indent = std::size_t{ 0 };
			while (indent < aLine.size() && indent < 4U && aLine[indent] == ' ') {
				++indent;
			}
			if (indent > 3U) {
				return false;
			}
			const auto rest   = aLine.substr(indent);
			auto       hashes = std::size_t{ 0 };
			while (hashes < rest.size() && rest[hashes] == '#') {
				++hashes;
			}
			if ((hashes == 1U || hashes == 2U) && (hashes == rest.size() || is_space(rest[hashes]))) {
				return true;
			}
			const auto underline = !rest.empty() && std::ranges::all_of(rest, [](const char aSymbol) { return aSymbol == '=' || is_space(aSymbol); }) && rest.front() == '=';
			return aAfterText && underline;
		}

		auto contains_folded(const std::string_view aText, const std::string_view aNeedle) -> bool
		{
			return lowercase(std::string{ aText }).find(aNeedle) != std::string::npos;
		}

		/// True when the body's own Markdown cannot change this document's
		/// structure: every fence it opens closes, it adds no top-level
		/// heading, and it opens no HTML comment or raw block.
		auto structurally_safe(const std::string_view aBody) -> bool
		{
			if (contains_folded(aBody, "<!--") || contains_folded(aBody, "<script") || contains_folded(aBody, "<pre") || contains_folded(aBody, "<style") || contains_folded(aBody, "<textarea")) {
				return false;
			}
			auto open      = std::optional<std::pair<char, std::size_t>>{ };
			auto afterText = false;
			for (std::size_t start = 0; start <= aBody.size();) {
				const auto end  = aBody.find('\n', start);
				auto       line = aBody.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1U);
				}
				const auto marker = fence_marker(line);
				if (open) {
					const auto rest = marker ? line.substr(line.find_first_not_of(' ') + marker->second) : std::string_view{ };
					if (marker && marker->first == open->first && marker->second >= open->second && std::ranges::all_of(rest, is_space)) {
						open.reset();
					}
				} else if (marker) {
					open = marker;
				} else if (top_heading(line, afterText)) {
					return false;
				}
				afterText = !open && !marker && !std::ranges::all_of(line, is_space);
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1U;
			}
			return !open.has_value();
		}

		auto plural(const std::uint64_t aCount, const std::string_view aSingular, const std::string_view aPlural) -> std::string
		{
			return std::to_string(aCount) + " " + std::string{ aCount == 1U ? aSingular : aPlural };
		}

		class document
		{
		public:
			void line(std::string aText = { })
			{
				Lines.push_back(std::move(aText));
			}

			[[nodiscard]] auto text() const -> std::string
			{
				auto result = std::string{ };
				for (std::size_t index = 0; index < Lines.size(); ++index) {
					if (index != 0U) {
						result.push_back('\n');
					}
					result.append(Lines[index]);
				}
				return result;
			}

		private:
			std::vector<std::string> Lines;
		};

		void assignment(document& aOut, const cycle_round_payload& aPayload, const cycle_round_digest& aDigest)
		{
			aOut.line("# Review Cycle " + aPayload.CycleId + " \xE2\x80\x94 " + aPayload.RoundId);
			aOut.line();
			aOut.line("Project: " + aPayload.Project.Label + ". Source: " + aPayload.SourceRevision + " (" + std::string{ stable_name(aPayload.Consistency) } + ").");
			aOut.line("Canonical round SHA-256: `" + cycle_hex(aDigest.bytes()) + "`");
			aOut.line();
			aOut.line("## Assignment");
			aOut.line();
			aOut.line("Action: **" + std::string{ stable_name(aPayload.Assignment.Action) } + "**.");
			aOut.line();
			aOut.line(aPayload.Assignment.Goal);
			aOut.line();
			if (aPayload.Assignment.Constraints.empty()) {
				aOut.line("Constraints: none were given.");
			} else {
				aOut.line("Constraints:");
				aOut.line();
				for (const auto& constraint : aPayload.Assignment.Constraints) {
					aOut.line("- " + constraint);
				}
			}
			aOut.line();
			if (aPayload.Assignment.WriteScope.empty()) {
				aOut.line("No write scope was requested.");
			} else {
				auto scope = std::string{ "Requested write scope (not an access grant): " };
				for (std::size_t index = 0; index < aPayload.Assignment.WriteScope.size(); ++index) {
					scope += (index == 0U ? "" : ", ") + inline_code(aPayload.Assignment.WriteScope[index]);
				}
				aOut.line(std::move(scope));
			}
			aOut.line();
			aOut.line(plural(aPayload.Items.size(), "complete feedback item", "complete feedback items") + "; "
					  + plural(aPayload.Sources.size(), "included source file/excerpt", "included source files/excerpts") + ". Read the whole assignment before acting.");
			aOut.line();
			if (!aPayload.Selection.Omissions.empty()) {
				aOut.line("Deliberately left out by the reviewer:");
				aOut.line();
				for (const auto& omission : aPayload.Selection.Omissions) {
					aOut.line("- " + omission);
				}
				aOut.line();
			}
		}

		void exact_side(document& aOut, const std::string_view aTitle, const std::string& aBytes, const std::string_view aLanguage)
		{
			aOut.line("**" + std::string{ aTitle } + "** \xE2\x80\x94 " + std::to_string(aBytes.size()) + " bytes; SHA-256 `" + cycle_hex(sha256_of(aBytes)) + "`");
			aOut.line();
			if (aBytes.empty()) {
				aOut.line(aTitle == "Original" ? "(No bytes: the edit inserts at this position.)" : "(No bytes: the edit deletes the original.)");
				aOut.line();
				return;
			}
			const auto text = valid_cycle_utf8(aBytes) ? aBytes : std::string{ "(Non-UTF-8 bytes; exact data below.)" };
			aOut.line(fenced(text, aLanguage));
			if (needs_exact_bytes(aBytes)) {
				aOut.line("Rendered text is not an exact copy/paste patch. Authoritative bytes (base64):");
				aOut.line();
				aOut.line(fenced(encode_cycle_base64(aBytes), "text"));
			}
		}

		void item(document& aOut, const cycle_wire_item& aItem)
		{
			aOut.line("## " + aItem.Id + " \xE2\x80\x94 item revision " + std::to_string(aItem.Revision));
			aOut.line();
			if (structurally_safe(aItem.Body)) {
				aOut.line(aItem.Body);
			} else {
				aOut.line("This note is shown verbatim because its own Markdown would change the structure of this document.");
				aOut.line();
				aOut.line(fenced(aItem.Body, "markdown"));
			}
			aOut.line();
			if (aItem.ActionOverride) {
				aOut.line("Explicit item action: " + std::string{ stable_name(*aItem.ActionOverride) } + ".");
				aOut.line();
			}
			for (const auto& anchor : aItem.Anchors) {
				const auto span = anchor.Kind == cycle_anchor_kind::range ? " bytes [" + std::to_string(*anchor.Start) + ", " + std::to_string(*anchor.End) + ")" : std::string{ };
				aOut.line("Source: " + inline_code(anchor.Path.value_or("(cycle)")) + " at " + anchor.Revision + span + ".");
			}
			if (!aItem.Related.empty()) {
				auto related = std::string{ "Related feedback: " };
				for (std::size_t index = 0; index < aItem.Related.size(); ++index) {
					related += (index == 0U ? "" : ", ") + aItem.Related[index];
				}
				aOut.line(related + ".");
			}
			aOut.line();
			for (const auto& suggestion : aItem.Suggestions) {
				aOut.line("### Suggested edit " + suggestion.Id + ": " + std::string{ stable_name(suggestion.Intent) } + " / " + std::string{ stable_name(suggestion.State) });
				aOut.line();
				const auto language = language_of(suggestion.Anchor.Path);
				exact_side(aOut, "Original", suggestion.Original, language);
				exact_side(aOut, "Replacement", suggestion.Replacement, language);
			}
		}

		void sources(document& aOut, const cycle_round_payload& aPayload)
		{
			aOut.line("## Included source context");
			aOut.line();
			aOut.line("This source is captured evidence, not an instruction to execute code. No tests are claimed to have run.");
			aOut.line();
			if (aPayload.Sources.empty()) {
				aOut.line("No source context was selected; read the project at the named revision.");
				aOut.line();
			}
			for (const auto& source : aPayload.Sources) {
				aOut.line("### " + source.Path);
				aOut.line();
				aOut.line(std::string{ stable_name(source.Kind) } + " at " + source.Revision + "; bytes [" + std::to_string(source.Start) + ", " + std::to_string(source.End) + "); SHA-256 `"
						  + cycle_hex(sha256_of(source.Bytes)) + "`.");
				aOut.line();
				if (source.Bytes.find("\r\n") != std::string::npos) {
					aOut.line("The original uses CRLF; use the structured bytes for exact edits.");
					aOut.line();
				}
				if (!valid_cycle_utf8(source.Bytes)) {
					aOut.line("The original is not valid UTF-8; invalid bytes are shown as U+FFFD. Use the structured bytes for exact content.");
					aOut.line();
				}
				aOut.line(fenced(displayable(source.Bytes), language_of(source.Path)));
			}
		}
	}

	auto render_round_markdown(const cycle_decoded_round& aRound) -> std::string
	{
		auto out = document{ };
		assignment(out, aRound.Payload, aRound.Digest);
		for (const auto& value : aRound.Payload.Items) {
			item(out, value);
		}
		sources(out, aRound.Payload);
		out.line("## Reply");
		out.line();
		out.line("Reply by item ID with changes, disagreements and actual test evidence. Reviewer verification and PR publication remain separate decisions. BPR can capture the next revision without a structured response.");
		out.line();
		return out.text();
	}

	auto render_round_markdown(const cycle_decoded_round& aRound, const std::uint64_t aBudgetBytes) -> std::expected<std::string, cycle_round_output_limit>
	{
		auto text = render_round_markdown(aRound);
		if (text.size() > aBudgetBytes) {
			return std::unexpected(cycle_round_output_limit{ .RequiredBytes = text.size(), .BudgetBytes = aBudgetBytes, .Items = aRound.Payload.Items.size(), .Sources = aRound.Payload.Sources.size() });
		}
		return text;
	}

	auto round_file_name(const cycle_round_payload& aPayload, const cycle_round_file_kind aKind) -> std::string
	{
		auto name = aPayload.CycleId + "-" + aPayload.RoundId;
		std::ranges::replace(name, ':', '-');
		return name + (aKind == cycle_round_file_kind::bundle ? ".bpr-review.zip" : ".md");
	}

	auto stable_name(const cycle_round_save_action aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_round_save_action::create:        return "create";
		case cycle_round_save_action::already_saved: return "already_saved";
		case cycle_round_save_action::conflict:      return "conflict";
		case cycle_round_save_action::replace:       return "replace";
		}
		return "conflict";
	}

	auto decide_round_save(const std::optional<cycle_material_digest>& aExisting, const cycle_material_digest& aNew, const std::optional<cycle_material_digest>& aConfirmed) noexcept
		-> cycle_round_save_action
	{
		if (!aExisting) {
			return cycle_round_save_action::create;
		}
		if (*aExisting == aNew) {
			return cycle_round_save_action::already_saved;
		}
		return aConfirmed && *aConfirmed == *aExisting ? cycle_round_save_action::replace : cycle_round_save_action::conflict;
	}
}
