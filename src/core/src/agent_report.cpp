#include "bpr/core/agent_report.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto lines_of(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto start = std::size_t{ 0 };
			while (start <= aText.size()) {
				const auto end = std::min(aText.find('\n', start), aText.size());
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

	auto report_card_of(const std::string_view aAnswer) -> report_card
	{
		auto       card    = report_card{ };
		const auto lines   = lines_of(aAnswer);
		const auto outline = report_outline(aAnswer);
		card.Sections      = static_cast<std::uint32_t>(outline.size());
		card.Lines         = static_cast<std::uint32_t>(lines.size());
		auto from          = std::size_t{ 0 };
		if (!outline.empty()) {
			card.Title = outline.front().Title;
			from       = outline.front().Line + 1U;
		} else {
			while (from < lines.size() && trimmed(lines[from]).empty()) {
				++from;
			}
			if (from < lines.size()) {
				card.Title = std::string{ trimmed(lines[from]) };
				++from;
			}
		}
		auto kept   = std::size_t{ 0 };
		auto fenced = false;
		for (auto index = from; index < lines.size() && kept < report_card_lead_lines; ++index) {
			const auto line = lines[index];
			if (fence(line)) {
				fenced = !fenced;
				continue;
			}
			if (fenced || trimmed(line).empty() || heading(line)) {
				continue;
			}
			if (!card.Lead.empty()) {
				card.Lead += '\n';
			}
			card.Lead += std::string{ trimmed(line) };
			++kept;
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
