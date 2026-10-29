#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		auto decimal(std::string_view aText) -> bool
		{
			return !aText.empty() && aText.size() <= 20U && aText.front() != '0' && std::ranges::all_of(aText, [](unsigned char aByte) { return aByte >= '0' && aByte <= '9'; });
		}
		/// `…/owner/repository/pull/N`: the page both tabs hang from. The host
		/// admits the URL (its scheme and host) before it reaches the codec;
		/// the core reads only its path.
		auto pull_request_page(std::string_view aPrefix) -> bool
		{
			const auto pull = aPrefix.rfind("/pull/");
			return pull != std::string_view::npos && pull != 0U && decimal(aPrefix.substr(pull + 6U));
		}
		struct markdown_line
		{
			std::size_t      Begin;
			std::size_t      End;
			std::string_view Text;
			bool             Top;
		};
		auto trim(std::string_view aText) -> std::string_view
		{
			const auto first = aText.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return { };
			}
			return aText.substr(first, aText.find_last_not_of(" \t\r\n") - first + 1);
		}
		// URI syntax is inert Markdown here. The host separately admits destinations.
		auto absolute_link(std::string_view aText) -> bool
		{
			const auto colon = aText.find(':');
			if (colon < 2 || colon > 32 || colon == std::string_view::npos) {
				return false;
			}
			return std::all_of(aText.begin(), aText.begin() + static_cast<std::ptrdiff_t>(colon), [](unsigned char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '+' || aByte == '-' || aByte == '.'; });
		}

		auto lines(std::string_view aRaw) -> std::vector<markdown_line>
		{
			std::vector<markdown_line> result;
			char                       fence       = 0;
			std::size_t                fenceLength = 0;
			bool                       html        = false;
			std::string_view           htmlClose;
			bool                       comment = false;
			bool                       list    = false;
			for (std::size_t at = 0; at < aRaw.size();) {
				const auto lf        = aRaw.find('\n', at);
				const auto end       = lf == std::string_view::npos ? aRaw.size() : lf + 1;
				const auto text      = aRaw.substr(at, (lf == std::string_view::npos ? end : lf) - at);
				const auto clean     = trim(text);
				const auto indent    = text.find_first_not_of(' ');
				const bool container = !clean.empty() && (clean.front() == '>' || text.front() == '\t' || (indent != std::string_view::npos && indent >= 4) || (list && indent != 0));
				bool       top       = !fence && !html && !comment && !container;
				if (!container && !html && !comment && clean.size() >= 3 && (clean.front() == '`' || clean.front() == '~')) {
					const auto count  = clean.find_first_not_of(clean.front());
					const auto length = count == std::string_view::npos ? clean.size() : count;
					if (length >= 3 && !fence) {
						fence       = clean.front();
						fenceLength = length;
						top         = false;
					} else if (fence == clean.front() && length >= fenceLength && trim(clean.substr(length)).empty()) {
						fence = 0;
						top   = false;
					}
				}
				if (!fence && !container) {
					if (clean.starts_with("<!--")) {
						comment = true;
						top     = false;
					}
					if (comment && clean.find("-->") != std::string_view::npos) {
						comment = false;
						top     = false;
					}
					// Conservatively opaque through the next blank line (CommonMark HTML blocks),
					// and through explicit details close for disclosures containing blank lines.
					if (!comment && clean.starts_with('<') && !absolute_link(clean.substr(1))) {
						html = true;
						top  = false;
						if (clean.starts_with("<details")) {
							htmlClose = "</details>";
						}
						if (clean.starts_with("<script")) {
							htmlClose = "</script>";
						}
						if (clean.starts_with("<style")) {
							htmlClose = "</style>";
						}
						if (clean.starts_with("<pre")) {
							htmlClose = "</pre>";
						}
					}
					if (html && ((!htmlClose.empty() && clean.find(htmlClose) != std::string_view::npos) || (htmlClose.empty() && (clean.empty() || clean.starts_with("</"))))) {
						html      = false;
						htmlClose = { };
						top       = false;
					}
				}
				result.push_back({ at, end, text, top });
				if (top && !clean.empty()) {
					list = std::isdigit(static_cast<unsigned char>(clean.front())) || clean.starts_with("- ") || clean.starts_with("* ") || clean.starts_with("+ ");
				}
				at = end;
			}
			return result;
		}
		auto safe_destination(std::string_view aText) -> bool
		{
			return !aText.empty() && aText.size() <= maximum_tour_path_bytes
				&& std::none_of(aText.begin(), aText.end(), [](unsigned char aByte) { return aByte <= 0x20 || aByte == 0x7f || aByte == '(' || aByte == ')' || aByte == '<' || aByte == '>' || aByte == '\\'; });
		}
		auto read_link(std::string_view aText, std::size_t& aAt) -> std::optional<tour_markdown_link>
		{
			if (aAt >= aText.size()) {
				return { };
			}
			const auto opening = aText[aAt];
			++aAt;
			if (opening != '[') {
				return { };
			}
			std::string label;
			while (aAt < aText.size() && aText[aAt] != ']') {
				if (aText[aAt] == '[') {
					return { };
				}
				if (aText[aAt] == '\\') {
					if (++aAt == aText.size()) {
						return { };
					}
				}
				label += aText[aAt++];
			}
			if (aAt + 2 > aText.size() || aText.substr(aAt, 2) != "](") {
				return { };
			}
			aAt += 2;
			const auto end = aText.find(')', aAt);
			if (end == std::string_view::npos) {
				return { };
			}
			const auto destination = aText.substr(aAt, end - aAt);
			if (!safe_destination(destination)) {
				return { };
			}
			aAt = end + 1;
			return tour_markdown_link{ std::move(label), std::string{ destination } };
		}
		auto links(std::string_view aText) -> std::vector<tour_markdown_link>
		{
			std::vector<tour_markdown_link> result;
			for (std::size_t at = 0; at < aText.size();) {
				if (aText[at] == '\\') {
					at = std::min(at + 2, aText.size());
					continue;
				}
				if (aText[at] == '`') {
					const auto end = aText.find('`', at + 1);
					at             = end == std::string_view::npos ? aText.size() : end + 1;
					continue;
				}
				if (aText[at] != '[') {
					++at;
					continue;
				}
				if (auto link = read_link(aText, at)) {
					result.push_back(std::move(*link));
				}
			}
			return result;
		}
		auto ordered_prefix(std::string_view aText) -> std::optional<std::size_t>
		{
			std::size_t at = 0;
			while (at < aText.size() && at < 9 && std::isdigit(static_cast<unsigned char>(aText[at]))) {
				++at;
			}
			if (at == 0 || at + 1 >= aText.size() || (aText[at] != '.' && aText[at] != ')') || aText[at + 1] != ' ') {
				return { };
			}
			return at + 2;
		}
		auto heading(std::string_view aText) -> std::pair<std::size_t, std::string_view>
		{
			std::size_t level = 0;
			while (level < aText.size() && aText[level] == '#') {
				++level;
			}
			if (!level || level > 6 || level == aText.size() || aText[level] != ' ') {
				return { };
			}
			auto       title = trim(aText.substr(level));
			const auto close = title.find_last_not_of('#');
			if (close != std::string_view::npos && close + 1 < title.size() && title[close] == ' ') {
				title = trim(title.substr(0, close));
			}
			return { level, title };
		}
		auto boundary(std::string_view aRaw, std::size_t aAt) -> bool
		{
			return aAt <= aRaw.size() && (aAt == aRaw.size() || (static_cast<unsigned char>(aRaw[aAt]) & 0xc0) != 0x80);
		}
		auto navigation(std::string_view aText) -> bool
		{
			std::size_t              at = 0;
			std::vector<std::string> labels;
			while (at < aText.size()) {
				const auto link = read_link(aText, at);
				if (!link) {
					return false;
				}
				labels.push_back(link->Label);
				if (at == aText.size()) {
					break;
				}
				if (aText.substr(at, 4) != " · ") {
					return false;
				}
				at += 4;
			}
			return labels == std::vector<std::string>{ "Tour overview" } || labels == std::vector<std::string>{ "Previous", "Tour overview" }
			|| labels == std::vector<std::string>{ "Tour overview", "Next" } || labels == std::vector<std::string>{ "Previous", "Tour overview", "Next" };
		}
	}

	auto map_tour_markdown(std::string_view aRaw) -> tour_markdown_map
	{
		tour_markdown_map result;
		if (aRaw.size() > maximum_tour_field_bytes) {
			result.Bounded = false;
			return result;
		}
		const auto                 rows = lines(aRaw);
		std::optional<std::size_t> section;
		std::optional<std::size_t> list;
		for (const auto& row : rows) {
			const auto text = trim(row.Text);
			if (!row.Top) {
				if (list && !text.empty()) {
					result.Lists[*list].MappingNeeded = true;
				}
				continue;
			}
			const auto [level, name] = heading(text);
			if (section && level && level <= 2) {
				result.Sections[*section].Content.End = row.Begin;
				section.reset();
			}
			if (level == 2 && (name == "Problem" || name == "Solution")) {
				result.Sections.push_back({ std::string{ name }, { row.End, aRaw.size() } });
				section = result.Sections.size() - 1;
			}
			const auto prefix = ordered_prefix(text);
			if (prefix) {
				if (!list) {
					result.Lists.push_back({ { row.Begin, row.End }, { }, false });
					list = result.Lists.size() - 1;
				}
				auto& target      = result.Lists[*list];
				target.Source.End = row.End;
				auto       at     = *prefix;
				const auto entry  = read_link(text, at);
				if (entry && trim(text.substr(at)).empty()) {
					target.Entries.push_back(*entry);
				} else {
					target.MappingNeeded = true;
					target.Entries.push_back({ std::string{ text.substr(*prefix) }, { } });
				}
			} else if (!text.empty()) {
				list.reset();
			}
			const auto found = links(text);
			result.Links.insert(result.Links.end(), found.begin(), found.end());
			if (result.Sections.size() + result.Lists.size() + result.Links.size() > 1024) {
				return { .Bounded = false };
			}
		}
		return result;
	}

	auto map_tour_stop(std::string_view aRaw) -> tour_stop_markdown
	{
		tour_stop_markdown result;
		if (aRaw.size() > maximum_tour_field_bytes) {
			return result;
		}
		const auto                 rows         = lines(aRaw);
		constexpr auto             prefix       = std::string_view{ "### PR Tour · " };
		constexpr auto             legacyPrefix = std::string_view{ "### Author's tour · " };
		std::optional<std::size_t> heading;
		std::size_t                prefixSize = 0;
		for (std::size_t index = 0; index < rows.size(); ++index) {
			const auto text = trim(rows[index].Text);
			if (rows[index].Top && (text.starts_with(prefix) || text.starts_with(legacyPrefix))) {
				if (heading) {
					return result;
				}
				heading    = index;
				prefixSize = text.starts_with(prefix) ? prefix.size() : legacyPrefix.size();
			}
		}
		if (!heading) {
			return result;
		}
		const auto first = *heading;
		const auto text  = trim(rows[first].Text);
		const auto dash  = text.find(" — ", prefixSize);
		if (dash == std::string_view::npos || dash == prefixSize
			|| !std::all_of(text.begin() + static_cast<std::ptrdiff_t>(prefixSize), text.begin() + static_cast<std::ptrdiff_t>(dash), [](unsigned char aByte) { return std::isdigit(aByte); })) {
			return result;
		}
		result.Heading   = tour_text_span{ rows[first].Begin, rows[first].End };
		result.Title     = std::string{ text.substr(dash + std::string_view{ " — " }.size()) };
		std::size_t last = rows.size();
		while (last > first + 1 && trim(rows[last - 1].Text).empty()) {
			--last;
		}
		// [[BPR-1785]]. A stop published before the note has the links alone.
		const bool noted = last > first + 3 && rows[last - 1].Top && trim(rows[last - 1].Text) == tour_navigation_note;
		const auto links = noted ? last - 2 : last - 1;
		if (links > first + 1 && rows[links].Top && trim(rows[links - 1].Text).empty() && navigation(trim(rows[links].Text))) {
			result.Footer    = tour_text_span{ rows[links].Begin, rows[last - 1].End };
			result.Narrative = tour_text_span{ rows[first].End, rows[links].Begin };
		}
		return result;
	}

	auto edit_tour_markdown(std::string_view aRaw, std::span<const tour_text_edit> aEdits) -> std::optional<std::string>
	{
		if (aRaw.size() > maximum_tour_field_bytes || aEdits.size() > 1024) {
			return { };
		}
		auto edits = std::vector<tour_text_edit>{ aEdits.begin(), aEdits.end() };
		std::ranges::sort(edits, { }, [](const auto& aEdit) { return aEdit.Span.Begin; });
		std::string result;
		std::size_t at = 0;
		for (const auto& edit : edits) {
			if (edit.Span.Begin < at || edit.Span.End < edit.Span.Begin || !boundary(aRaw, edit.Span.Begin) || !boundary(aRaw, edit.Span.End)
				|| aRaw.substr(edit.Span.Begin, edit.Span.End - edit.Span.Begin) != edit.Expected || edit.Replacement.size() > maximum_tour_field_bytes) {
				return { };
			}
			result.append(aRaw.substr(at, edit.Span.Begin - at));
			result += edit.Replacement;
			at = edit.Span.End;
			if (result.size() > maximum_tour_field_bytes) {
				return { };
			}
		}
		result.append(aRaw.substr(at));
		return result.size() <= maximum_tour_field_bytes ? std::optional{ std::move(result) } : std::nullopt;
	}

	auto insert_tour_brief(std::string_view aRaw, std::size_t aAt, std::string_view aProblem, std::string_view aSolution, std::string_view aNewline) -> std::optional<std::string>
	{
		if ((aNewline != "\n" && aNewline != "\r\n") || aProblem.size() > maximum_tour_field_bytes || aSolution.size() > maximum_tour_field_bytes) {
			return { };
		}
		const auto           nl   = std::string{ aNewline };
		const auto           body = "## Problem" + nl + std::string{ aProblem } + nl + nl + "## Solution" + nl + std::string{ aSolution } + nl + nl;
		const tour_text_edit edit{ { aAt, aAt }, { }, body };
		return edit_tour_markdown(aRaw, std::span{ &edit, 1U });
	}

	auto tour_markdown_label(std::string_view aLabel) -> std::string
	{
		std::string result;
		for (char character : aLabel) {
			const auto byte = static_cast<unsigned char>(character);
			if (byte < 0x20 || byte == 0x7f) {
				result += ' ';
				continue;
			}
			if (std::string_view{ "\\`*_{}[]()<>!#|&" }.find(static_cast<char>(byte)) != std::string_view::npos) {
				result += '\\';
			}
			result += static_cast<char>(byte);
		}
		return result;
	}

	auto tour_stop_link(std::string_view aWebUrl) -> std::string
	{
		constexpr auto conversation = std::string_view{ "#discussion_r" };
		const auto     at           = aWebUrl.find(conversation);
		if (at == std::string_view::npos || !pull_request_page(aWebUrl.substr(0, at)) || !decimal(aWebUrl.substr(at + conversation.size()))) {
			return std::string{ aWebUrl };
		}
		return std::string{ aWebUrl.substr(0, at) } + "/files#r" + std::string{ aWebUrl.substr(at + conversation.size()) };
	}

	auto tour_link_identity(std::string_view aLink) -> std::string
	{
		constexpr auto files = std::string_view{ "/files#r" };
		const auto     at    = aLink.find(files);
		if (at == std::string_view::npos || !pull_request_page(aLink.substr(0, at)) || !decimal(aLink.substr(at + files.size()))) {
			return std::string{ aLink };
		}
		return std::string{ aLink.substr(0, at) } + "#discussion_r" + std::string{ aLink.substr(at + files.size()) };
	}

	auto render_tour_index(std::span<const tour_markdown_link> aEntries, std::string_view aNewline) -> std::optional<std::string>
	{
		if (aEntries.empty() || aEntries.size() > maximum_tour_stops || (aNewline != "\n" && aNewline != "\r\n")) {
			return { };
		}
		std::string result = "## Review tour — Start here" + std::string{ aNewline } + std::string{ aNewline };
		for (const auto& entry : aEntries) {
			if (!safe_destination(entry.Destination)) {
				return { };
			}
			result += "1. [" + tour_markdown_label(entry.Label) + "](" + entry.Destination + ")" + std::string{ aNewline };
			if (result.size() > maximum_tour_field_bytes) {
				return { };
			}
		}
		return result;
	}

	auto render_tour_stop(std::string_view aTitle, std::size_t aOrdinal, std::string_view aNarrative,
						  std::span<const tour_markdown_link> aNavigation, std::string_view aPinnedSource, std::string_view aNewline) -> std::optional<std::string>
	{
		if (!aOrdinal || aOrdinal > maximum_tour_stops || aTitle.empty() || aNarrative.empty() || aTitle.size() > maximum_tour_field_bytes || aNarrative.size() > maximum_tour_field_bytes
			|| (aNewline != "\n" && aNewline != "\r\n") || (!aPinnedSource.empty() && !safe_destination(aPinnedSource))) {
			return { };
		}
		std::string footer;
		for (const auto& link : aNavigation) {
			if (!safe_destination(link.Destination)) {
				return { };
			}
			if (!footer.empty()) {
				footer += " · ";
			}
			footer += "[" + tour_markdown_label(link.Label) + "](" + link.Destination + ")";
		}
		if (!navigation(footer)) {
			return { };
		}
		const auto nl = std::string{ aNewline };
		if (aNavigation.size() > 1) {
			footer += nl + std::string{ tour_navigation_note };
		}
		std::string result = aPinnedSource.empty() ? "" : std::string{ aPinnedSource } + nl + nl;
		result += "### PR Tour · " + std::to_string(aOrdinal) + " — " + tour_markdown_label(aTitle) + nl + nl + std::string{ aNarrative } + nl + nl + footer + nl;
		return result.size() <= maximum_tour_field_bytes ? std::optional{ std::move(result) } : std::nullopt;
	}

	auto with_tour_navigation_note(std::string_view aRaw) -> std::string
	{
		const auto stop = map_tour_stop(aRaw);
		if (!stop.Footer) {
			return std::string{ aRaw };
		}
		const auto footer = aRaw.substr(stop.Footer->Begin, stop.Footer->End - stop.Footer->Begin);
		const auto lf     = footer.find('\n');
		const auto line   = trim(footer.substr(0, lf));
		if (lf != std::string_view::npos && lf + 1 < footer.size()) {
			return std::string{ aRaw };
		}
		if (line.find("](") == line.rfind("](")) {
			return std::string{ aRaw };
		}
		const auto  nl     = std::string{ lf != 0 && lf != std::string_view::npos && footer[lf - 1] == '\r' ? "\r\n" : "\n" };
		std::string result = std::string{ aRaw.substr(0, stop.Footer->End) };
		if (lf == std::string_view::npos) {
			result += nl;
		}
		result += std::string{ tour_navigation_note } + nl + std::string{ aRaw.substr(stop.Footer->End) };
		return result;
	}

	namespace {
		/// \brief A row without its carriage return, as both newline styles write it.
		auto bare(std::string_view aText) -> std::string_view
		{
			return aText.ends_with('\r') ? aText.substr(0, aText.size() - 1) : aText;
		}
		/// \brief The inverse of `tour_markdown_label`: a backslash before one of
		/// the bytes it escapes is dropped; any other byte is kept.
		auto unescaped_label(std::string_view aLabel) -> std::string
		{
			constexpr auto escaped = std::string_view{ "\\`*_{}[]()<>!#|&" };
			std::string    result;
			result.reserve(aLabel.size());
			for (std::size_t at = 0; at < aLabel.size(); ++at) {
				if (aLabel[at] == '\\' && at + 1 < aLabel.size() && escaped.find(aLabel[at + 1]) != std::string_view::npos) {
					++at;
				}
				result += aLabel[at];
			}
			return result;
		}
		/// \brief The published heading's number and title, or none.
		auto published_heading(std::string_view aText) -> std::optional<std::pair<std::size_t, std::string>>
		{
			constexpr auto prefix       = std::string_view{ "### PR Tour · " };
			constexpr auto legacyPrefix = std::string_view{ "### Author's tour · " };
			constexpr auto dash         = std::string_view{ " — " };
			auto           size         = std::size_t{ 0 };
			if (aText.starts_with(prefix)) {
				size = prefix.size();
			} else if (aText.starts_with(legacyPrefix)) {
				size = legacyPrefix.size();
			} else {
				return { };
			}
			const auto split = aText.find(dash, size);
			if (split == std::string_view::npos || split - size > 3U || !decimal(aText.substr(size, split - size))) {
				return { };
			}
			auto ordinal = std::size_t{ 0 };
			for (const auto digit : aText.substr(size, split - size)) {
				ordinal = ordinal * 10U + static_cast<std::size_t>(digit - '0');
			}
			auto title = unescaped_label(trim(aText.substr(split + dash.size())));
			if (ordinal == 0U || ordinal > maximum_tour_stops || trim(title).empty()) {
				return { };
			}
			return std::pair{ ordinal, std::move(title) };
		}
	}

	auto recognize_published_tour_stop(const std::string_view aBody) -> std::optional<published_tour_stop>
	{
		if (aBody.size() > maximum_tour_field_bytes) {
			return { };
		}
		const auto rows   = lines(aBody);
		auto       result = published_tour_stop{ };
		// The pinned source, alone on the first line, then a blank line.
		auto heading = std::size_t{ 0 };
		if (!rows.empty() && !published_heading(bare(rows[0].Text))) {
			const auto source = bare(rows[0].Text);
			if (rows.size() < 2U || !rows[0].Top || !safe_destination(source) || !absolute_link(source) || !bare(rows[1].Text).empty()) {
				return { };
			}
			result.PinnedSource = tour_text_span{ rows[0].Begin, rows[0].Begin + source.size() };
			heading             = 2U;
		}
		// The heading, a blank line, the narrative, a blank line and the
		// navigation: five rows at least.
		if (heading + 5U > rows.size() || !rows[heading].Top || !bare(rows[heading + 1U].Text).empty()) {
			return { };
		}
		auto parsed = published_heading(bare(rows[heading].Text));
		if (!parsed) {
			return { };
		}
		result.Ordinal = parsed->first;
		result.Title   = std::move(parsed->second);
		// Trailing blank lines only, after the navigation and its note.
		auto last = rows.size();
		while (last > heading && trim(rows[last - 1U].Text).empty()) {
			--last;
		}
		if (last < heading + 5U) {
			return { };
		}
		auto links = last - 1U;
		if (bare(rows[links].Text) == tour_navigation_note) {
			if (!rows[links].Top) {
				return { };
			}
			--links;
		}
		const auto footer = bare(rows[links].Text);
		if (links < heading + 4U || !rows[links].Top || !trim(rows[links - 1U].Text).empty() || !navigation(footer)) {
			return { };
		}
		auto at = std::size_t{ 0 };
		while (at < footer.size()) {
			auto link = read_link(footer, at);
			if (!link) {
				return { };
			}
			if (link->Label == "Previous") {
				result.Previous = std::move(link->Destination);
			} else if (link->Label == "Next") {
				result.Next = std::move(link->Destination);
			} else {
				result.Overview = std::move(link->Destination);
			}
			if (at < footer.size()) {
				at += std::string_view{ " · " }.size();
			}
		}
		// A first stop has nothing before it.
		if (result.Ordinal == 1U && result.Previous.has_value()) {
			return { };
		}
		// The narrative: the rows between the heading's blank line and the
		// navigation's, without the blank rows around it.
		auto first = heading + 2U;
		auto end   = links - 1U;
		while (first < end && trim(rows[first].Text).empty()) {
			++first;
		}
		while (end > first && trim(rows[end - 1U].Text).empty()) {
			--end;
		}
		if (first >= end) {
			return { };
		}
		result.Narrative = tour_text_span{ rows[first].Begin, rows[end - 1U].Begin + bare(rows[end - 1U].Text).size() };
		return result;
	}

	auto published_tour_stop_counts(const std::span<const published_tour_stop> aStops) -> std::vector<std::optional<std::size_t>>
	{
		auto result  = std::vector<std::optional<std::size_t>>(aStops.size());
		auto visited = std::vector<bool>(aStops.size(), false);
		for (std::size_t index = 0; index < aStops.size(); ++index) {
			if (visited[index]) {
				continue;
			}
			auto tour = std::vector<std::size_t>{ };
			for (std::size_t other = index; other < aStops.size(); ++other) {
				if (!visited[other] && aStops[other].Overview == aStops[index].Overview) {
					visited[other] = true;
					tour.push_back(other);
				}
			}
			const auto count = tour.size();
			auto       seen  = std::vector<bool>(count + 1U, false);
			const auto whole = std::ranges::all_of(tour, [&](const std::size_t aMember) {
				const auto& stop = aStops[aMember];
				if (stop.Ordinal == 0U || stop.Ordinal > count || seen[stop.Ordinal] || stop.Previous.has_value() != (stop.Ordinal > 1U)
					|| stop.Next.has_value() != (stop.Ordinal < count)) {
					return false;
				}
				seen[stop.Ordinal] = true;
				return true;
			});
			if (whole) {
				for (const auto member : tour) {
					result[member] = count;
				}
			}
		}
		return result;
	}

	auto published_tour_stop_neighbours(const std::span<const published_tour_stop> aStops) -> std::vector<published_tour_stop_links>
	{
		// The stop at each tour's number; a number given twice holds none.
		auto at = std::map<std::pair<std::string_view, std::size_t>, std::optional<std::size_t>>{ };
		for (std::size_t index = 0; index < aStops.size(); ++index) {
			const auto [place, added] = at.try_emplace(std::pair{ std::string_view{ aStops[index].Overview }, aStops[index].Ordinal }, index);
			if (!added) {
				place->second.reset();
			}
		}
		const auto find = [&at](const published_tour_stop& aStop, const std::size_t aOrdinal) -> std::optional<std::size_t> {
			const auto place = at.find(std::pair{ std::string_view{ aStop.Overview }, aOrdinal });
			return place != at.end() ? place->second : std::nullopt;
		};
		auto result = std::vector<published_tour_stop_links>(aStops.size());
		for (std::size_t index = 0; index < aStops.size(); ++index) {
			const auto& stop = aStops[index];
			if (stop.Previous.has_value() && stop.Ordinal > 1U) {
				result[index].Previous = find(stop, stop.Ordinal - 1U);
			}
			if (stop.Next.has_value()) {
				result[index].Next = find(stop, stop.Ordinal + 1U);
			}
		}
		return result;
	}

	auto find_published_tour_stop(const review_tour& aTour, const std::string_view aPost, const published_tour_stop& aStop) -> std::optional<tour_stop_id>
	{
		const auto named = [aPost](const tour_remote_binding& aRemote) {
			auto prefix = std::string_view{ };
			if (aRemote.Kind == tour_remote_kind::conversation_comment) {
				prefix = "issue-comment:";
			} else if (aRemote.Kind == tour_remote_kind::line_comment || aRemote.Kind == tour_remote_kind::file_comment) {
				prefix = "review-comment:";
			}
			return !prefix.empty() && !aRemote.Entity.empty() && aPost.starts_with(prefix) && aPost.substr(prefix.size()) == aRemote.Entity;
		};
		for (const auto& stop : aTour.Stops) {
			if (stop.Remote && named(*stop.Remote)) {
				return stop.Id;
			}
		}
		if (aStop.Ordinal >= 1U && aStop.Ordinal <= aTour.Stops.size()) {
			const auto& stop = aTour.Stops[aStop.Ordinal - 1U];
			if (stop.Remote && stop.Title == aStop.Title) {
				return stop.Id;
			}
		}
		return { };
	}
}
