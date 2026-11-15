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

	/// \brief One section the opened report's menu lists.
	struct report_section
	{
		std::string Title;
		/// 0 for a section, 1 for a sub-section and so on, from the shallowest
		/// heading after the title.
		std::uint32_t Depth = 0U;
		/// The zero-based line of the answer the heading is on.
		std::uint32_t Line = 0U;

		auto operator<=>(const report_section&) const = default;
	};

	/// \brief What a report's card and its opened tab show
	/// (docs/design/report-card-v4/SPEC.md).
	struct report_card
	{
		/// The heading the answer opens with; else the title given; else the
		/// answer's first line.
		std::string Title;
		/// The sections after the title at their shallowest level.
		std::uint32_t Sections = 0U;
		/// What stands between the title and the first section, Markdown
		/// kept; with no prose there, the first paragraph after the title.
		std::string Summary;
		/// Every heading after the title, in order.
		std::vector<report_section> Outline;

		auto operator<=>(const report_card&) const = default;
	};
	/// \details `aTitle`: Q4, the title of an answer that is a report by what
	/// was asked (Explain this file, What changed); used when it has no
	/// heading, its lead then starting at its first line.
	[[nodiscard]] auto report_card_of(std::string_view aAnswer, std::string_view aTitle = { }) -> report_card;

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

	/// \brief docs/design/report-templates-v1 and v2 (owner, 2026-11-14: B,
	/// Dressed, in Qt's rich text). What a verdict or label means, for its
	/// colour: what still needs work, what is half done or asked, what is
	/// done or good, or nothing in particular.
	enum class report_tone : std::uint8_t {
		neutral,
		open,
		partial,
		done,
	};

	/// \brief A short row: a list item, its bold label first where the
	/// template gives one, then its place in backticks, then what to know.
	struct dressed_row
	{
		std::string Label;
		report_tone Tone = report_tone::neutral;
		std::string Place;
		/// Where the place's link goes, when the answer made it one
		/// (`[`place`](link)`).
		std::string PlaceLink;
		/// The rest of the row, Markdown kept.
		std::string Text;

		auto operator<=>(const dressed_row&) const = default;
	};

	enum class dressed_block_kind : std::uint8_t {
		/// A paragraph, Markdown kept.
		paragraph,
		/// A quotation of the comment.
		quote,
		/// A run of rows.
		rows,
		/// Fenced code that names its language.
		code,
		/// A fenced `suggestion`: the lines as they should read.
		suggestion,
		/// A `**Check:**` line.
		check,
	};

	struct dressed_block
	{
		dressed_block_kind Kind = dressed_block_kind::paragraph;
		/// A paragraph's, a quote's or a check's text, Markdown kept.
		std::string              Text;
		std::string              Language;
		std::vector<std::string> Lines;
		/// The first line code is numbered from: the item's place, when the
		/// code follows one.
		std::optional<std::uint32_t> FirstLine;
		/// A suggestion's: the lines it replaces, from the item's code above
		/// it when that code holds them.
		std::vector<std::string> Replaced;
		std::vector<dressed_row> Rows;

		auto operator<=>(const dressed_block&) const = default;
	};

	/// \brief A level-three heading and what is under it.
	struct dressed_item
	{
		/// The verdict or Conventional Comments label the item carries
		/// ("Partly", "issue (blocking)"), or none.
		std::string Label;
		report_tone Tone = report_tone::neutral;
		/// The heading without its label, Markdown kept.
		std::string Heading;
		/// The line that is only a place: `path:line` or `path:first-last`.
		std::string                Place;
		std::string                PlaceLink;
		std::vector<dressed_block> Body;
		/// Its row in the report's menu, counted from 1 (0 is the summary).
		std::uint32_t Outline = 0U;

		auto operator<=>(const dressed_item&) const = default;
	};

	/// \brief A level-two heading and what is under it.
	struct dressed_section
	{
		std::string Name;
		/// A section named for a verdict ("Not addressed", "Likely") carries
		/// its colour.
		report_tone Tone = report_tone::neutral;
		/// What stands before its first item.
		std::vector<dressed_block> Intro;
		std::vector<dressed_item>  Items;
		std::uint32_t              Outline = 0U;

		auto operator<=>(const dressed_section&) const = default;
	};

	/// \brief One count of the tally under the summary.
	struct dressed_count
	{
		std::string   Word;
		std::uint32_t Count = 0U;
		report_tone   Tone  = report_tone::neutral;

		auto operator<=>(const dressed_count&) const = default;
	};

	/// \brief A report read by the shared convention: what the app dresses.
	/// Only what is recognised is dressed; the rest shows as written.
	struct dressed_report
	{
		std::string                  Title;
		std::string                  Summary;
		std::vector<dressed_section> Sections;
		/// By verdict or label, in the order they first appear; else by the
		/// areas a section's items group rows into. Empty when neither.
		std::vector<dressed_count> Tally;

		auto operator<=>(const dressed_report&) const = default;
	};

	/// \brief The colour a verdict, section name or Conventional Comments
	/// label means; neutral for anything else.
	[[nodiscard]] auto report_tone_of(std::string_view aWord) -> report_tone;

	/// \brief Reads `aAnswer` by the report convention: the title (the
	/// heading it opens with, else `aTitle`), the summary, each section and
	/// its items, rows, code and suggestions, and the tally. The
	/// machine-read blocks (`bpr-claims`, `bpr-findings`) are left out. No
	/// sections: an answer that does not follow the convention, which is
	/// shown as written.
	[[nodiscard]] auto dress_report(std::string_view aAnswer, std::string_view aTitle = { }) -> dressed_report;

	/// \brief A place's path and its lines: `src/a.cpp:12` or
	/// `src/a.cpp:12-14`; none when it is not one.
	struct report_place
	{
		std::string   Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;

		auto operator<=>(const report_place&) const = default;
	};
	[[nodiscard]] auto read_report_place(std::string_view aPlace) -> std::optional<report_place>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP
