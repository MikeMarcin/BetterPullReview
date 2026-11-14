#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0128 ([[BPR-2166]], [[BPR-2167]]). A long agent answer is a
/// report: a card where it was asked, read in a file tab with an outline.
/// And wherever commits are listed for an answer, a merge of the base branch
/// is one line and the rest are grouped by area. Pure decisions over text
/// and records the window already holds.
namespace bpr::core {
	/// \brief Q7 (default): an answer is a report by its shape, not a
	/// setting: at least this many section headings...
	inline constexpr std::size_t report_minimum_headings = 2U;
	/// \brief ...or longer than this many lines (more than the Agent tab
	/// shows without scrolling far).
	inline constexpr std::size_t report_minimum_lines = 40U;

	/// \brief Whether an answer is read as a report (Q7, by its shape).
	[[nodiscard]] auto is_report_answer(std::string_view aAnswer) -> bool;

	/// \brief One heading of a report's outline.
	struct report_heading
	{
		/// 1 for `#`, 2 for `##`, and so on.
		std::uint32_t Level = 1U;
		std::string   Title;
		/// The zero-based line of the answer the heading is on.
		std::uint32_t Line = 0U;

		auto operator<=>(const report_heading&) const = default;
	};

	/// \brief The ATX headings of a Markdown answer, in order, outside fenced
	/// code.
	[[nodiscard]] auto report_outline(std::string_view aAnswer) -> std::vector<report_heading>;

	/// \brief What a report's card shows where it was asked.
	struct report_card
	{
		/// The first heading, or the answer's first line.
		std::string Title;
		std::uint32_t Sections = 0U;
		std::uint32_t Lines    = 0U;
		/// The first lines of prose after the title, at most
		/// `report_card_lead_lines`, Markdown kept.
		std::string Lead;

		auto operator<=>(const report_card&) const = default;
	};
	inline constexpr std::size_t report_card_lead_lines = 3U;
	[[nodiscard]] auto           report_card_of(std::string_view aAnswer) -> report_card;

	/// \brief One commit as a listing reads it.
	struct listed_commit
	{
		std::string              Oid;
		std::string              Headline;
		std::vector<std::string> Parents;
		/// The paths its first-parent diff changes, when known.
		std::optional<std::vector<std::string>> Paths;

		auto operator<=>(const listed_commit&) const = default;
	};

	/// \brief A merge of the base branch, as one line.
	struct listed_merge
	{
		std::string Oid;
		/// The base branch's name, as the merge's headline says it.
		std::string Base;
		/// The commits it brought in, when the host counted them.
		std::optional<std::uint32_t> Brought;
		/// Whether it changed any of the pull request's files; none when its
		/// paths are not known.
		std::optional<bool> ChangedPullRequestFiles;

		auto operator<=>(const listed_merge&) const = default;
	};

	/// \brief The other commits of one area: the top folder most of their
	/// paths share, or "" for the root and unknown paths.
	struct listed_area
	{
		std::string                Area;
		std::vector<listed_commit> Commits;

		auto operator<=>(const listed_area&) const = default;
	};

	struct commit_listing
	{
		std::vector<listed_merge> Merges;
		std::vector<listed_area>  Areas;

		auto operator<=>(const commit_listing&) const = default;
	};

	/// \brief Whether `aCommit` merges the base branch `aBase` into the pull
	/// request: two or more parents, and its headline says so ("Merge branch
	/// 'main'", "Merge remote-tracking branch 'origin/main'", "Merge main
	/// into ..."), or its second parent is not one of `aPullRequestCommits`.
	[[nodiscard]] auto merges_base(const listed_commit& aCommit, std::string_view aBase, const std::vector<std::string>& aPullRequestCommits) -> bool;

	/// \brief Folds a listing: each merge of the base branch is one line, and
	/// the other commits are grouped by area, areas in first-seen order and
	/// commits in their order. `aBrought` counts, by merge oid, the commits a
	/// merge brought in where the host knows them.
	[[nodiscard]] auto fold_commit_listing(const std::vector<listed_commit>& aCommits, std::string_view aBase, const std::vector<std::string>& aPullRequestFiles,
										   const std::vector<std::pair<std::string, std::uint32_t>>& aBrought = { }) -> commit_listing;

	/// \brief The listing as Markdown for an answer's material: "Merged main
	/// (24 commits), changed this pull request's files" lines, then each
	/// area's commits.
	[[nodiscard]] auto render_commit_listing(const commit_listing& aListing) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP
