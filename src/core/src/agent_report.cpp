#include "bpr/core/agent_report.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto lines_of(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto start = std::size_t{ 0 };
			while (start <= aText.size()) {
				const auto end  = std::min(aText.find('\n', start), aText.size());
				auto       line = aText.substr(start, end - start);
				if (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1U);
				}
				lines.push_back(line);
				if (end == aText.size()) {
					break;
				}
				start = end + 1U;
			}
			while (!lines.empty() && lines.back().empty()) {
				lines.pop_back();
			}
			return lines;
		}

		[[nodiscard]] auto trimmed(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		[[nodiscard]] auto fence(const std::string_view aLine) -> bool
		{
			const auto line = trimmed(aLine);
			return line.starts_with("```") || line.starts_with("~~~");
		}

		/// An ATX heading: up to three spaces, one to six `#`, then a space.
		[[nodiscard]] auto heading(const std::string_view aLine) -> std::optional<std::pair<std::uint32_t, std::string>>
		{
			auto line   = aLine;
			auto indent = 0U;
			while (!line.empty() && line.front() == ' ' && indent < 4U) {
				line.remove_prefix(1U);
				++indent;
			}
			if (indent > 3U) {
				return std::nullopt;
			}
			auto level = 0U;
			while (!line.empty() && line.front() == '#' && level < 7U) {
				line.remove_prefix(1U);
				++level;
			}
			if (level == 0U || level > 6U || (!line.empty() && line.front() != ' ' && line.front() != '\t')) {
				return std::nullopt;
			}
			auto title = trimmed(line);
			while (!title.empty() && title.back() == '#') {
				title.remove_suffix(1U);
			}
			title = trimmed(title);
			if (title.empty()) {
				return std::nullopt;
			}
			return std::pair{ level, std::string{ title } };
		}

		[[nodiscard]] auto ascii_lower(std::string_view aText) -> std::string
		{
			auto lower = std::string{ aText };
			for (auto& character : lower) {
				if (character >= 'A' && character <= 'Z') {
					character = static_cast<char>(character - 'A' + 'a');
				}
			}
			return lower;
		}

		/// The top folder most of a commit's paths share; "" for the root or
		/// paths not known.
		[[nodiscard]] auto area_of(const listed_commit& aCommit) -> std::string
		{
			if (!aCommit.Paths || aCommit.Paths->empty()) {
				return { };
			}
			auto counts = std::map<std::string, std::size_t>{ };
			for (const auto& path : *aCommit.Paths) {
				const auto slash = path.find('/');
				++counts[slash == std::string::npos ? std::string{ } : path.substr(0U, slash)];
			}
			auto best = counts.begin();
			for (auto each = counts.begin(); each != counts.end(); ++each) {
				if (each->second > best->second) {
					best = each;
				}
			}
			return best->first;
		}
	} // namespace

	auto report_outline(const std::string_view aAnswer) -> std::vector<report_heading>
	{
		auto outline = std::vector<report_heading>{ };
		auto fenced  = false;
		auto number  = std::uint32_t{ 0 };
		for (const auto line : lines_of(aAnswer)) {
			if (fence(line)) {
				fenced = !fenced;
			} else if (!fenced) {
				if (auto found = heading(line)) {
					outline.push_back({ .Level = found->first, .Title = std::move(found->second), .Line = number });
				}
			}
			++number;
		}
		return outline;
	}

	auto is_report_answer(const std::string_view aAnswer) -> bool
	{
		return report_outline(aAnswer).size() >= report_minimum_headings || lines_of(aAnswer).size() > report_minimum_lines;
	}

	auto report_card_of(const std::string_view aAnswer, const std::string_view aTitle) -> report_card
	{
		auto       card    = report_card{ };
		const auto lines   = lines_of(aAnswer);
		const auto outline = report_outline(aAnswer);
		auto       from    = std::size_t{ 0 };
		while (from < lines.size() && trimmed(lines[from]).empty()) {
			++from;
		}
		auto sections = outline;
		if (!outline.empty() && outline.front().Line == from) {
			card.Title = outline.front().Title;
			sections.erase(sections.begin());
			++from;
		} else if (!aTitle.empty()) {
			card.Title = std::string{ aTitle };
		} else if (from < lines.size()) {
			card.Title = std::string{ trimmed(lines[from]) };
			++from;
		}
		auto shallowest = std::uint32_t{ 0 };
		for (const auto& each : sections) {
			shallowest = shallowest == 0U ? each.Level : std::min(shallowest, each.Level);
		}
		for (const auto& each : sections) {
			card.Outline.push_back({ .Title = each.Title, .Depth = each.Level - shallowest, .Line = each.Line });
			card.Sections += each.Level == shallowest ? 1U : 0U;
		}
		// The summary: the prose before the first section; with none there,
		// the first paragraph after the title wherever it stands.
		const auto first     = sections.empty() ? lines.size() : static_cast<std::size_t>(sections.front().Line);
		const auto paragraph = [&lines](std::size_t aFrom, const std::size_t aTo, const bool aWhole) {
			auto text   = std::string{ };
			auto fenced = false;
			for (auto index = aFrom; index < aTo; ++index) {
				const auto line = lines[index];
				if (fence(line)) {
					fenced = !fenced;
				}
				if (!fenced && !fence(line) && heading(line)) {
					if (!text.empty()) {
						break;
					}
					continue;
				}
				if (trimmed(line).empty() && !fenced) {
					if (!text.empty() && !aWhole) {
						break;
					}
					if (!text.empty()) {
						text += '\n';
					}
					continue;
				}
				if (!text.empty()) {
					text += '\n';
				}
				text += std::string{ line };
			}
			while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
				text.pop_back();
			}
			return text;
		};
		card.Summary = sections.empty() ? paragraph(from, lines.size(), false) : paragraph(from, first, true);
		if (card.Summary.empty()) {
			card.Summary = paragraph(from, lines.size(), false);
		}
		return card;
	}

	auto merges_base(const listed_commit& aCommit, const std::string_view aBase, const std::vector<std::string>& aPullRequestCommits) -> bool
	{
		if (aCommit.Parents.size() < 2U) {
			return false;
		}
		const auto headline = ascii_lower(aCommit.Headline);
		const auto base     = ascii_lower(aBase);
		for (const auto& said : { "merge branch '" + base + "'", "merge remote-tracking branch 'origin/" + base + "'", "merge branch '" + base + "' of ", "merge " + base + " into ",
								  "merge origin/" + base + " into " }) {
			if (headline.starts_with(said)) {
				return true;
			}
		}
		return std::ranges::none_of(aPullRequestCommits, [&aCommit](const std::string& aOid) { return aOid == aCommit.Parents[1]; });
	}

	auto fold_commit_listing(const std::vector<listed_commit>& aCommits, const std::string_view aBase, const std::vector<std::string>& aPullRequestFiles,
							 const std::vector<std::pair<std::string, std::uint32_t>>& aBrought) -> commit_listing
	{
		auto listing = commit_listing{ };
		auto oids    = std::vector<std::string>{ };
		for (const auto& commit : aCommits) {
			oids.push_back(commit.Oid);
		}
		for (const auto& commit : aCommits) {
			if (merges_base(commit, aBase, oids)) {
				auto merge = listed_merge{ .Oid = commit.Oid, .Base = std::string{ aBase } };
				if (const auto counted = std::ranges::find(aBrought, commit.Oid, &std::pair<std::string, std::uint32_t>::first); counted != aBrought.end()) {
					merge.Brought = counted->second;
				}
				if (commit.Paths) {
					merge.ChangedPullRequestFiles = std::ranges::any_of(*commit.Paths, [&aPullRequestFiles](const std::string& aPath) {
						return std::ranges::find(aPullRequestFiles, aPath) != aPullRequestFiles.end();
					});
				}
				listing.Merges.push_back(std::move(merge));
				continue;
			}
			const auto area  = area_of(commit);
			auto       found = std::ranges::find(listing.Areas, area, &listed_area::Area);
			if (found == listing.Areas.end()) {
				listing.Areas.push_back({ .Area = area });
				found = std::prev(listing.Areas.end());
			}
			found->Commits.push_back(commit);
		}
		return listing;
	}

	auto render_commit_listing(const commit_listing& aListing) -> std::string
	{
		auto text = std::string{ };
		for (const auto& merge : aListing.Merges) {
			text += "- Merged " + merge.Base;
			if (merge.Brought) {
				text += " (" + std::to_string(*merge.Brought) + (*merge.Brought == 1U ? " commit)" : " commits)");
			}
			if (merge.ChangedPullRequestFiles) {
				text += *merge.ChangedPullRequestFiles ? ", changed this pull request's files" : ", changed none of this pull request's files";
			}
			text += " (" + merge.Oid.substr(0U, 7U) + ")\n";
		}
		for (const auto& area : aListing.Areas) {
			text += "\n" + (area.Area.empty() ? std::string{ "Other" } : area.Area + "/") + "\n";
			for (const auto& commit : area.Commits) {
				text += "- " + commit.Oid.substr(0U, 7U) + " " + commit.Headline + "\n";
			}
		}
		return text;
	}
} // namespace bpr::core

namespace bpr::core {
	namespace {
		[[nodiscard]] auto lower_ascii(std::string_view aText) -> std::string
		{
			auto text = std::string{ aText };
			std::ranges::transform(text, text.begin(), [](const char aChar) { return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar - 'A' + 'a') : aChar; });
			return text;
		}

		[[nodiscard]] auto strip(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// The Conventional Comments labels a review's items are headed with.
		constexpr auto conventional_labels = std::array<std::string_view, 12>{ "praise", "nitpick", "suggestion", "issue", "todo", "question", "thought", "chore", "note", "typo", "polish", "quibble" };

		/// A heading that opens with a label: `issue (blocking): The rest`.
		/// Answers the label as written and the rest.
		[[nodiscard]] auto labelled(const std::string_view aHeading) -> std::optional<std::pair<std::string, std::string>>
		{
			const auto colon = aHeading.find(": ");
			if (colon == std::string_view::npos || colon == 0U) {
				return std::nullopt;
			}
			const auto label = aHeading.substr(0U, colon);
			const auto word  = lower_ascii(strip(label.substr(0U, label.find(' '))));
			if (std::ranges::find(conventional_labels, std::string_view{ word }) == conventional_labels.end()) {
				return std::nullopt;
			}
			const auto rest = label.substr(word.size());
			if (!strip(rest).empty() && !(strip(rest).front() == '(' && strip(rest).back() == ')')) {
				return std::nullopt;
			}
			return std::pair{ std::string{ strip(label) }, std::string{ strip(aHeading.substr(colon + 2U)) } };
		}

		/// The words that name a verdict, whatever their colour.
		[[nodiscard]] auto verdict_word(const std::string_view aWord) -> bool
		{
			static constexpr auto words = std::array<std::string_view, 14>{ "not addressed", "partly", "cannot tell", "addressed", "likely", "possible", "unlikely", "not checked",
																			"must fix", "worth a look", "good", "lines changed", "replied", "unchanged" };
			return std::ranges::find(words, std::string_view{ lower_ascii(strip(aWord)) }) != words.end();
		}

		[[nodiscard]] auto fence_info(const std::string_view aLine) -> std::optional<std::string>
		{
			const auto line = strip(aLine);
			if (!line.starts_with("```") && !line.starts_with("~~~")) {
				return std::nullopt;
			}
			auto info = line.substr(3U);
			while (!info.empty() && (info.front() == '`' || info.front() == '~')) {
				info.remove_prefix(1U);
			}
			return std::string{ strip(info) };
		}

		struct read_place
		{
			std::string      Place;
			std::string      Link;
			std::string_view Rest;
		};

		/// A place in backticks, or a path that looks like one, alone or made
		/// a link (`[`place`](link)`), and what follows it.
		[[nodiscard]] auto backticked(std::string_view aText) -> std::optional<read_place>
		{
			auto link = std::string_view{ };
			auto rest = std::string_view{ };
			if (aText.starts_with("[`")) {
				const auto close = aText.find("`](");
				const auto end   = close == std::string_view::npos ? close : aText.find(')', close + 3U);
				if (end == std::string_view::npos) {
					return std::nullopt;
				}
				link  = aText.substr(close + 3U, end - close - 3U);
				rest  = aText.substr(end + 1U);
				aText = aText.substr(1U, close);
			} else if (aText.starts_with('`')) {
				const auto close = aText.find('`', 1U);
				if (close == std::string_view::npos) {
					return std::nullopt;
				}
				rest  = aText.substr(close + 1U);
				aText = aText.substr(0U, close + 1U);
			} else {
				return std::nullopt;
			}
			if (aText.size() < 3U || !aText.ends_with('`')) {
				return std::nullopt;
			}
			const auto inside = aText.substr(1U, aText.size() - 2U);
			const auto path   = inside.find_first_of(" \t") == std::string_view::npos && (inside.find('/') != std::string_view::npos || inside.find('.') != std::string_view::npos);
			if (!read_report_place(inside) && !path) {
				return std::nullopt;
			}
			return read_place{ .Place = std::string{ inside }, .Link = std::string{ link }, .Rest = strip(rest) };
		}

		[[nodiscard]] auto row_of(std::string_view aText) -> dressed_row
		{
			auto row = dressed_row{ };
			aText    = strip(aText);
			if (aText.starts_with("**")) {
				if (const auto close = aText.find("**", 2U); close != std::string_view::npos) {
					auto label = strip(aText.substr(2U, close - 2U));
					if (label.ends_with(':')) {
						label.remove_suffix(1U);
					}
					row.Label = std::string{ strip(label) };
					row.Tone  = report_tone_of(row.Label);
					aText     = strip(aText.substr(close + 2U));
					if (aText.starts_with(':')) {
						aText = strip(aText.substr(1U));
					}
				}
			}
			if (const auto place = backticked(aText)) {
				row.Place     = place->Place;
				row.PlaceLink = place->Link;
				aText         = place->Rest;
			}
			row.Text = std::string{ aText };
			return row;
		}
	} // namespace

	auto report_tone_of(const std::string_view aWord) -> report_tone
	{
		auto word = lower_ascii(strip(aWord));
		if (const auto open = word.find(" ("); open != std::string::npos) {
			word.resize(open);
		}
		static constexpr auto open    = std::array<std::string_view, 5>{ "not addressed", "likely", "must fix", "issue", "todo" };
		static constexpr auto partial = std::array<std::string_view, 7>{ "partly", "possible", "worth a look", "lines changed", "question", "suggestion", "thought" };
		static constexpr auto done    = std::array<std::string_view, 4>{ "addressed", "unlikely", "good", "praise" };
		if (std::ranges::find(open, std::string_view{ word }) != open.end()) {
			return report_tone::open;
		}
		if (std::ranges::find(partial, std::string_view{ word }) != partial.end()) {
			return report_tone::partial;
		}
		if (std::ranges::find(done, std::string_view{ word }) != done.end()) {
			return report_tone::done;
		}
		return report_tone::neutral;
	}

	auto read_report_place(const std::string_view aPlace) -> std::optional<report_place>
	{
		const auto place = strip(aPlace);
		const auto colon = place.rfind(':');
		if (colon == std::string_view::npos || colon == 0U || colon + 1U == place.size() || place.find_first_of(" \t") != std::string_view::npos) {
			return std::nullopt;
		}
		const auto number = [](const std::string_view aText) -> std::optional<std::uint32_t> {
			if (aText.empty() || aText.size() > 9U || !std::ranges::all_of(aText, [](const char aChar) { return aChar >= '0' && aChar <= '9'; })) {
				return std::nullopt;
			}
			auto value = std::uint32_t{ 0U };
			for (const auto digit : aText) {
				value = value * 10U + static_cast<std::uint32_t>(digit - '0');
			}
			return value == 0U ? std::nullopt : std::optional{ value };
		};
		auto lines = place.substr(colon + 1U);
		// A range is written with a hyphen or an en dash.
		auto dash = lines.find('-');
		auto skip = std::size_t{ 1U };
		if (const auto en = lines.find("\xE2\x80\x93"); dash == std::string_view::npos && en != std::string_view::npos) {
			dash = en;
			skip = 3U;
		}
		const auto first = number(lines.substr(0U, dash));
		const auto last  = dash == std::string_view::npos ? first : number(lines.substr(dash + skip));
		if (!first || !last || *last < *first) {
			return std::nullopt;
		}
		return report_place{ .Path = std::string{ place.substr(0U, colon) }, .First = *first, .Last = *last };
	}

	auto dress_report(const std::string_view aAnswer, const std::string_view aTitle) -> dressed_report
	{
		auto report = dressed_report{ };
		auto lines  = std::vector<std::string_view>{ };
		{
			// The machine-read blocks after a report are not shown.
			auto skipping = false;
			auto inside   = false;
			for (const auto line : lines_of(aAnswer)) {
				const auto info = fence_info(line);
				if (skipping) {
					skipping = !info.has_value();
					continue;
				}
				if (info && !inside && info->starts_with("bpr-")) {
					skipping = true;
					continue;
				}
				if (info) {
					inside = inside ? !info->empty() : true;
				}
				lines.push_back(line);
			}
			while (!lines.empty() && strip(lines.back()).empty()) {
				lines.pop_back();
			}
		}
		auto at = std::size_t{ 0U };
		while (at < lines.size() && strip(lines[at]).empty()) {
			++at;
		}
		if (at < lines.size()) {
			if (const auto found = heading(lines[at])) {
				report.Title = found->second;
				++at;
			}
		}
		if (report.Title.empty()) {
			report.Title = std::string{ aTitle };
		}
		// The section level: the shallowest heading after the title.
		auto sectionLevel = std::uint32_t{ 0U };
		{
			auto fenced = false;
			for (auto index = at; index < lines.size(); ++index) {
				if (fence_info(lines[index])) {
					fenced = !fenced;
				} else if (const auto found = heading(lines[index]); found && !fenced) {
					sectionLevel = sectionLevel == 0U ? found->first : std::min(sectionLevel, found->first);
				}
			}
		}
		auto outline = std::uint32_t{ 0U };
		auto blocks  = std::vector<dressed_block>{ };
		// Where the blocks being read go: the summary, a section's intro, or
		// an item's body.
		const auto place_blocks = [&report](std::vector<dressed_block>& aBlocks) {
			if (report.Sections.empty()) {
				for (const auto& block : aBlocks) {
					if (!block.Text.empty()) {
						report.Summary += (report.Summary.empty() ? "" : "\n\n") + block.Text;
					}
				}
			} else if (auto& section = report.Sections.back(); section.Items.empty()) {
				section.Intro.insert(section.Intro.end(), aBlocks.begin(), aBlocks.end());
			} else {
				auto& item = section.Items.back();
				for (auto& block : aBlocks) {
					// The line that is only a place, first under the heading.
					if (item.Place.empty() && item.Body.empty() && block.Kind == dressed_block_kind::paragraph) {
						if (const auto place = backticked(block.Text); place && place->Rest.empty()) {
							item.Place     = place->Place;
							item.PlaceLink = place->Link;
							continue;
						}
					}
					item.Body.push_back(std::move(block));
				}
			}
			aBlocks.clear();
		};
		auto       paragraph     = std::string{ };
		auto       kind          = dressed_block_kind::paragraph;
		const auto end_paragraph = [&] {
			if (!paragraph.empty()) {
				blocks.push_back({ .Kind = kind, .Text = std::move(paragraph) });
				paragraph.clear();
			}
			kind = dressed_block_kind::paragraph;
		};
		for (; at < lines.size(); ++at) {
			const auto line = lines[at];
			const auto bare = strip(line);
			if (const auto info = fence_info(line)) {
				end_paragraph();
				auto code = dressed_block{ .Kind = *info == "suggestion" ? dressed_block_kind::suggestion : dressed_block_kind::code, .Language = *info };
				for (++at; at < lines.size() && !(fence_info(lines[at]) && fence_info(lines[at])->empty()); ++at) {
					code.Lines.emplace_back(lines[at]);
				}
				blocks.push_back(std::move(code));
				continue;
			}
			if (const auto found = heading(line); found && sectionLevel != 0U && found->first <= sectionLevel + 1U) {
				end_paragraph();
				place_blocks(blocks);
				++outline;
				if (found->first == sectionLevel) {
					report.Sections.push_back({ .Name = found->second, .Tone = report_tone_of(found->second), .Outline = outline });
					continue;
				}
				if (report.Sections.empty()) {
					report.Sections.push_back({ });
				}
				auto& section = report.Sections.back();
				auto  item    = dressed_item{ .Heading = found->second, .Outline = outline };
				if (const auto label = labelled(found->second)) {
					item.Label   = label->first;
					item.Heading = label->second;
					item.Tone    = report_tone_of(label->first);
				} else if (verdict_word(section.Name)) {
					item.Label = section.Name;
					item.Tone  = section.Tone;
				}
				section.Items.push_back(std::move(item));
				continue;
			}
			if (const auto deeper = heading(line)) {
				// A deeper heading counts in the menu; it reads as a line.
				++outline;
				end_paragraph();
				blocks.push_back({ .Text = "**" + deeper->second + "**" });
				continue;
			}
			if (bare.empty()) {
				end_paragraph();
				continue;
			}
			if (bare.starts_with("- ") || bare.starts_with("* ")) {
				end_paragraph();
				if (blocks.empty() || blocks.back().Kind != dressed_block_kind::rows) {
					blocks.push_back({ .Kind = dressed_block_kind::rows });
				}
				blocks.back().Rows.push_back(row_of(bare.substr(2U)));
				continue;
			}
			if (!blocks.empty() && blocks.back().Kind == dressed_block_kind::rows && paragraph.empty() && (line.starts_with("  ") || line.starts_with('\t'))) {
				blocks.back().Rows.back().Text += " " + std::string{ bare };
				continue;
			}
			if (bare.starts_with('>')) {
				if (kind != dressed_block_kind::quote) {
					end_paragraph();
					kind = dressed_block_kind::quote;
				}
				paragraph += (paragraph.empty() ? "" : " ") + std::string{ strip(bare.substr(1U)) };
				continue;
			}
			if (kind == dressed_block_kind::quote) {
				end_paragraph();
			}
			if (paragraph.empty() && bare.starts_with("**Check:**")) {
				kind      = dressed_block_kind::check;
				paragraph = std::string{ strip(bare.substr(10U)) };
				continue;
			}
			paragraph += (paragraph.empty() ? "" : "\n") + std::string{ bare };
		}
		end_paragraph();
		place_blocks(blocks);
		// Code is numbered from the item's place, or from the first row's
		// with lines; a suggestion replaces the lines the item names, read
		// from the code above it where that code holds them.
		auto labels = std::vector<dressed_count>{ };
		auto areas  = std::vector<dressed_count>{ };
		for (auto& section : report.Sections) {
			for (auto& item : section.Items) {
				auto from = read_report_place(item.Place);
				for (const auto& block : item.Body) {
					if (from) {
						break;
					}
					for (const auto& row : block.Rows) {
						if (const auto place = read_report_place(row.Place)) {
							from = place;
							break;
						}
					}
				}
				const dressed_block* code = nullptr;
				auto                 rows = std::uint32_t{ 0U };
				for (auto& block : item.Body) {
					rows += static_cast<std::uint32_t>(block.Rows.size());
					if (block.Kind == dressed_block_kind::code) {
						block.FirstLine = from ? std::optional{ from->First } : std::nullopt;
						code            = &block;
					} else if (block.Kind == dressed_block_kind::suggestion && from) {
						block.FirstLine = from->First;
						if (code != nullptr && code->FirstLine && *code->FirstLine <= from->First && from->Last < *code->FirstLine + code->Lines.size()) {
							block.Replaced.assign(code->Lines.begin() + (from->First - *code->FirstLine), code->Lines.begin() + (from->Last - *code->FirstLine + 1U));
						}
					}
				}
				if (!item.Label.empty()) {
					auto word = lower_ascii(item.Label);
					if (const auto open = word.find(" ("); open != std::string::npos) {
						word.resize(open);
					}
					if (auto found = std::ranges::find(labels, word, &dressed_count::Word); found != labels.end()) {
						++found->Count;
					} else {
						labels.push_back({ .Word = word, .Count = 1U, .Tone = item.Tone });
					}
				} else if (item.Place.empty() && rows > 0U) {
					areas.push_back({ .Word = lower_ascii(item.Heading), .Count = rows });
				}
			}
		}
		report.Tally = labels.empty() ? std::move(areas) : std::move(labels);
		return report;
	}
} // namespace bpr::core
