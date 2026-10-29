#include "bpr/core/markdown_blob_snippet.hpp"

#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <system_error>

namespace bpr::core {
	namespace {
		auto ascii_lower(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const char aChar) {
				return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar;
			});
			return aText;
		}

		auto ascii_lower_view(const std::string_view aText) -> std::string { return ascii_lower(std::string{ aText }); }

		auto blank(const std::string_view aLine) -> bool
		{
			return std::ranges::all_of(aLine, [](const char aChar) { return aChar == ' ' || aChar == '\t' || aChar == '\r'; });
		}

		/// The line without trailing spaces, tabs and CR.
		auto trimmed_end(std::string_view aLine) -> std::string_view
		{
			while (!aLine.empty() && (aLine.back() == ' ' || aLine.back() == '\t' || aLine.back() == '\r')) {
				aLine.remove_suffix(1);
			}
			return aLine;
		}

		/// Up to three spaces of indentation, consumed; nothing at four or a tab.
		auto unindented(std::string_view aLine) -> std::optional<std::string_view>
		{
			auto spaces = std::size_t{ 0U };
			while (spaces < aLine.size() && aLine[spaces] == ' ') {
				++spaces;
			}
			if (spaces > 3U || (spaces < aLine.size() && aLine[spaces] == '\t')) {
				return std::nullopt;
			}
			return aLine.substr(spaces);
		}

		struct fence
		{
			char        Char   = '`';
			std::size_t Length = 0U;
		};

		/// A fence opening or closing at the line's start: three or more of
		/// one backtick or tilde.
		auto fence_of(const std::string_view aLine) -> std::optional<fence>
		{
			const auto text = unindented(aLine);
			if (!text || text->empty() || (text->front() != '`' && text->front() != '~')) {
				return std::nullopt;
			}
			const auto ch     = text->front();
			auto       length = std::size_t{ 0U };
			while (length < text->size() && (*text)[length] == ch) {
				++length;
			}
			if (length < 3U || (ch == '`' && text->substr(length).find('`') != std::string_view::npos)) {
				return std::nullopt;
			}
			return fence{ .Char = ch, .Length = length };
		}

		auto closes(const std::string_view aLine, const fence& aOpen) -> bool
		{
			const auto found = fence_of(aLine);
			if (!found || found->Char != aOpen.Char || found->Length < aOpen.Length) {
				return false;
			}
			const auto text = *unindented(aLine);
			return blank(text.substr(found->Length));
		}

		/// What ends an HTML block a line opens: a terminator it must hold,
		/// or a blank line when empty.
		auto html_end(const std::string_view aText) -> std::optional<std::string_view>
		{
			if (!aText.starts_with('<')) {
				return std::nullopt;
			}
			const auto lower = ascii_lower_view(aText.substr(0, std::min<std::size_t>(aText.size(), 12U)));
			if (lower.starts_with("<!--")) {
				return std::string_view{ "-->" };
			}
			if (lower.starts_with("<?")) {
				return std::string_view{ "?>" };
			}
			if (lower.starts_with("<![cdata[")) {
				return std::string_view{ "]]>" };
			}
			if (lower.starts_with("<!")) {
				return std::string_view{ ">" };
			}
			struct raw_tag
			{
				std::string_view Open;
				std::string_view Close;
			};
			// The closing tags are matched without regard to case.
			for (const auto& tag : { raw_tag{ "<pre", "</pre>" }, raw_tag{ "<script", "</script>" }, raw_tag{ "<style", "</style>" }, raw_tag{ "<textarea", "</textarea>" } }) {
				const auto size = tag.Open.size();
				if (lower.starts_with(tag.Open) && (lower.size() == size || lower[size] == '>' || lower[size] == ' ' || lower[size] == '\t')) {
					return tag.Close;
				}
			}
			return std::string_view{ };
		}

		/// A setext heading's underline: `=` or `-` only, after up to three spaces.
		auto underline(const std::string_view aLine) -> bool
		{
			const auto text = unindented(aLine);
			if (!text) {
				return false;
			}
			const auto body = trimmed_end(*text);
			return !body.empty() && (body.front() == '=' || body.front() == '-')
				&& std::ranges::all_of(body, [&body](const char aChar) { return aChar == body.front(); });
		}

		struct line
		{
			std::size_t      Begin = 0U;
			std::string_view Text;
		};

		auto lines_of(const std::string_view aBody) -> std::vector<line>
		{
			auto result = std::vector<line>{ };
			auto begin  = std::size_t{ 0U };
			while (begin <= aBody.size()) {
				const auto end  = aBody.find('\n', begin);
				const auto stop = end == std::string_view::npos ? aBody.size() : end;
				auto       text = aBody.substr(begin, stop - begin);
				if (text.ends_with('\r')) {
					text.remove_suffix(1);
				}
				result.push_back({ .Begin = begin, .Text = text });
				if (end == std::string_view::npos) {
					break;
				}
				begin = end + 1U;
			}
			return result;
		}

		auto take_number(std::string_view aText) -> std::optional<std::uint32_t>
		{
			if (aText.empty() || aText.size() > 9U || (aText.size() > 1U && aText.front() == '0')) {
				return std::nullopt;
			}
			auto       value  = std::uint32_t{ 0U };
			const auto parsed = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			return parsed.ec == std::errc{ } && parsed.ptr == aText.data() + aText.size() ? std::optional{ value } : std::nullopt;
		}
	} // namespace

	auto markdown_standalone_permalinks(const std::string_view aBody) -> std::vector<markdown_standalone_permalink>
	{
		auto found = std::vector<markdown_standalone_permalink>{ };
		if (aBody.find(markdown_blob_snippet_mark) != std::string_view::npos) {
			return found;
		}
		const auto rows = lines_of(aBody);
		auto       open = std::optional<fence>{ };
		auto       html = std::optional<std::string>{ };
		for (std::size_t index = 0U; index < rows.size() && found.size() < markdown_blob_snippets_per_body; ++index) {
			const auto text = rows[index].Text;
			if (open) {
				if (closes(text, *open)) {
					open.reset();
				}
				continue;
			}
			if (html) {
				if (html->empty() ? blank(text) : ascii_lower_view(text).find(*html) != std::string::npos) {
					html.reset();
				}
				continue;
			}
			if (const auto opened = fence_of(text)) {
				open = opened;
				continue;
			}
			const auto shown     = trimmed_end(text);
			const auto angled    = shown.size() > 2U && shown.starts_with('<') && shown.ends_with('>');
			const auto inner     = angled ? shown.substr(1, shown.size() - 2U) : shown;
			auto       permalink = inner.empty() ? std::nullopt : recognize_markdown_blob_permalink(inner);
			if (!permalink) {
				if (const auto ends = unindented(text).and_then(html_end)) {
					// The block may end on the line that opens it.
					if (ends->empty() || ascii_lower_view(text).find(*ends, 1U) == std::string::npos) {
						html = std::string{ *ends };
					}
				}
				continue;
			}
			// Only a line that starts at its first column: indented, it may
			// belong to a list item or be code. Underlined, it is a heading.
			if (text.front() == ' ' || text.front() == '\t' || (index + 1U < rows.size() && underline(rows[index + 1U].Text))) {
				continue;
			}
			found.push_back({
				.Begin     = rows[index].Begin,
				.End       = rows[index].Begin + text.size(),
				.Address   = std::string{ inner },
				.Permalink = std::move(*permalink),
			});
		}
		return found;
	}

	auto markdown_blob_permalink_in_repository(const markdown_blob_permalink& aPermalink, const markdown_repository& aRepository) -> bool
	{
		return markdown_blob_permalink_in_repository(aPermalink, aRepository.Host, aRepository.Owner, aRepository.Repository);
	}

	auto markdown_blob_snippet_range_shown(const markdown_blob_permalink& aPermalink) -> bool
	{
		return aPermalink.FirstLine >= 1U && aPermalink.LastLine >= aPermalink.FirstLine
			&& aPermalink.LastLine - aPermalink.FirstLine < markdown_blob_snippet_maximum_lines;
	}

	auto markdown_blob_snippet_lines(const std::string_view aFile, const markdown_blob_permalink& aPermalink) -> std::optional<std::vector<std::string>>
	{
		if (!markdown_blob_snippet_range_shown(aPermalink) || aFile.size() > markdown_blob_snippet_file_bytes || aFile.find('\0') != std::string_view::npos) {
			return std::nullopt;
		}
		auto result = std::vector<std::string>{ };
		auto bytes  = std::size_t{ 0U };
		auto number = std::uint32_t{ 1U };
		auto begin  = std::size_t{ 0U };
		while (begin < aFile.size() && number <= aPermalink.LastLine) {
			const auto end  = aFile.find('\n', begin);
			const auto stop = end == std::string_view::npos ? aFile.size() : end;
			if (number >= aPermalink.FirstLine) {
				auto text = aFile.substr(begin, stop - begin);
				if (text.ends_with('\r')) {
					text.remove_suffix(1);
				}
				bytes += text.size() + 1U;
				if (bytes > markdown_blob_snippet_bytes || !utf8::valid(text)) {
					return std::nullopt;
				}
				result.emplace_back(text);
			}
			++number;
			begin = end == std::string_view::npos ? aFile.size() : end + 1U;
		}
		if (number <= aPermalink.LastLine) {
			return std::nullopt;
		}
		return result;
	}

	auto markdown_blob_permalink_source_key(const markdown_blob_permalink& aPermalink) -> std::string
	{
		return ascii_lower(aPermalink.Host) + '\n' + ascii_lower(aPermalink.Owner) + '\n' + ascii_lower(aPermalink.Repository) + '\n' + aPermalink.Commit + '\n' + aPermalink.Path;
	}

	auto markdown_blob_permalink_same_lines(const markdown_blob_permalink& aLeft, const markdown_blob_permalink& aRight) -> bool
	{
		return aLeft.FirstLine == aRight.FirstLine && aLeft.LastLine == aRight.LastLine && markdown_blob_permalink_source_key(aLeft) == markdown_blob_permalink_source_key(aRight);
	}

	auto markdown_blob_snippet_title(const markdown_blob_permalink& aPermalink) -> std::string
	{
		return aPermalink.Repository + '/' + aPermalink.Path;
	}

	auto markdown_blob_snippet_range(const markdown_blob_permalink& aPermalink) -> std::string
	{
		if (aPermalink.FirstLine == aPermalink.LastLine) {
			return "Line " + std::to_string(aPermalink.FirstLine);
		}
		return "Lines " + std::to_string(aPermalink.FirstLine) + " to " + std::to_string(aPermalink.LastLine);
	}

	auto markdown_blob_snippet_commit(const markdown_blob_permalink& aPermalink) -> std::string
	{
		return aPermalink.Commit.substr(0, 7);
	}

	auto markdown_blob_permalink_commit_address(const std::string_view aAddress) -> std::string
	{
		const auto permalink = recognize_markdown_blob_permalink(aAddress);
		if (!permalink) {
			return { };
		}
		// The scheme and authority as written: recognition admitted them.
		const auto scheme    = aAddress.find("://");
		const auto authority = aAddress.find('/', scheme + 3U);
		return std::string{ aAddress.substr(0, authority) } + '/' + permalink->Owner + '/' + permalink->Repository + "/commit/" + permalink->Commit;
	}

	auto markdown_blob_permalink_label(const markdown_blob_permalink& aPermalink, const bool aOwnRepository) -> std::string
	{
		auto label = markdown_blob_permalink_label(aPermalink);
		return aOwnRepository ? label.substr(aPermalink.Owner.size() + 1U) : label;
	}

	auto markdown_with_blob_snippets(const std::string_view aBody, const std::span<const markdown_blob_snippet_placement> aPlacements) -> std::string
	{
		auto result = std::string{ };
		auto done   = std::size_t{ 0U };
		for (const auto& placement : aPlacements) {
			if (placement.Begin < done || placement.End < placement.Begin || placement.End > aBody.size() || placement.Lines.empty()) {
				continue;
			}
			auto run     = std::size_t{ 0U };
			auto longest = std::size_t{ 0U };
			for (const auto& text : placement.Lines) {
				for (const auto ch : text) {
					run     = ch == '`' ? run + 1U : 0U;
					longest = std::max(longest, run);
				}
				run = 0U;
			}
			const auto fence = std::string(std::max<std::size_t>(3U, longest + 1U), '`');
			result.append(aBody.substr(done, placement.Begin - done));
			result += '\n';
			result += fence;
			result += markdown_blob_snippet_mark;
			result += std::to_string(placement.Ordinal);
			result += '\n';
			for (const auto& text : placement.Lines) {
				result += text;
				result += '\n';
			}
			result += fence;
			result += '\n';
			done = placement.End;
		}
		result.append(aBody.substr(done));
		return result;
	}

	auto markdown_blob_snippet_ordinal(const std::string_view aInfo) -> std::optional<std::uint32_t>
	{
		if (!aInfo.starts_with(markdown_blob_snippet_mark)) {
			return std::nullopt;
		}
		return take_number(aInfo.substr(markdown_blob_snippet_mark.size()));
	}
} // namespace bpr::core
