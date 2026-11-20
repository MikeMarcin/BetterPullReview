#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP

#include "bpr/core/defect_workflow.hpp"

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

	/// \brief [[BPR-2246]]. What decides whether a turn's reply wears a
	/// Report attachment.
	struct report_attachment_facts
	{
		/// The ask answers as a report whatever its shape (Explain this file,
		/// What changed since your review, Was this addressed? on several
		/// threads).
		bool AlwaysReport = false;
		/// The turn has ended.
		bool Ended = false;
		/// It ended with the agent's answer.
		bool Answered = false;
	};

	/// \brief Whether a turn's reply is shown as a report: an answer of a
	/// report's shape, or any answer to an ask that always reports. A turn
	/// that ended without an answer never is: its text says why it stopped,
	/// and it shows in the thread, not as a page repeating the error.
	[[nodiscard]] auto shows_report_attachment(const report_attachment_facts& aFacts, std::string_view aAnswer) -> bool;

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

	/// \brief docs/design/report-templates-v1 and v2 (owner, 2026-11-14: B,
	/// Dressed, in Qt's rich text). What a verdict or label means, for its
	/// colour: what still needs work, what is half done or asked, what is
	/// done or good, or nothing in particular. [[BPR-2236]]: wherever such a
	/// thing is shown, the Agent tab or the report page, its tone names one
	/// of the three theme roles `appearance_role::report_verdict_*`.
	enum class report_tone : std::uint8_t {
		neutral,
		open,
		partial,
		done,
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
		/// [[BPR-2236]]: without the `**Verdict:**` line, which is `Verdict`.
		std::string Summary;
		/// [[BPR-2236]]. The verdict the summary opens with, and its tone, as
		/// `dressed_report` reads it; empty when there is none.
		std::string Verdict;
		report_tone VerdictTone = report_tone::neutral;
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
	/// commits in their order. `aCommits` is the slice listed (the commits
	/// since the review); `aPullRequestCommits` every commit of the pull
	/// request, which decides whether a merge's second parent is the pull
	/// request's own ([[BPR-2184]], review N7). `aBrought` counts, by merge
	/// oid, the commits a merge brought in where the host knows them.
	[[nodiscard]] auto fold_commit_listing(const std::vector<listed_commit>& aCommits, const std::vector<std::string>& aPullRequestCommits, std::string_view aBase,
										   const std::vector<std::string>&                           aPullRequestFiles,
										   const std::vector<std::pair<std::string, std::uint32_t>>& aBrought = { }) -> commit_listing;

	/// \brief One commit's own change: from its first parent to it.
	struct commit_change_step
	{
		std::string Parent;
		std::string Commit;
		std::string Headline;

		auto operator<=>(const commit_change_step&) const = default;
	};

	/// \brief [[BPR-2247]]. The changes a selection of commits carries as
	/// the pull request's own work: each commit of `aSelection` that is not a
	/// merge of the base branch (`merges_base`), as its first parent to it, in
	/// the selection's order.
	///
	/// \details A merge of the base is left out whole: its first-parent change
	/// is the base's changes. A merge of anything else keeps its first-parent
	/// change, and a commit with no parent is left out. Any selection may be
	/// asked: the commits since a review, or commits the reviewer chose.
	[[nodiscard]] auto own_change_steps(const std::vector<listed_commit>& aSelection, const std::vector<std::string>& aPullRequestCommits, std::string_view aBase)
		-> std::vector<commit_change_step>;

	/// \brief [[BPR-2184]] (review N3). The merges of the base branch in the
	/// slice whose brought-in commits are worth a compare each: the newest
	/// first, at most `maximum_counted_merges`, each with its two parents.
	inline constexpr std::size_t maximum_counted_merges = 8U;
	struct merge_to_count
	{
		std::string Merge;
		std::string First;
		std::string Second;

		auto operator<=>(const merge_to_count&) const = default;
	};
	[[nodiscard]] auto merges_to_count(const std::vector<listed_commit>& aCommits, const std::vector<std::string>& aPullRequestCommits, std::string_view aBase) -> std::vector<merge_to_count>;

	/// \brief The listing as Markdown for an answer's material: "Merged main
	/// (24 commits), changed this pull request's files" lines, then each
	/// area's commits.
	[[nodiscard]] auto render_commit_listing(const commit_listing& aListing) -> std::string;

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
		std::string Place;
		std::string PlaceLink;
		/// [[BPR-2219]]. What followed the place on its line, when the item
		/// names more than one ("`a.py:5`, `b.py:21-22`"): the others,
		/// Markdown kept. The first is the item's place.
		std::string                OtherPlaces;
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
		/// [[BPR-2331]]. The menu row of the first item it counts, where the
		/// Agent tab's summary opens the report.
		std::uint32_t First = 0U;

		auto operator<=>(const dressed_count&) const = default;
	};

	/// \brief A report read by the shared convention: what the app dresses.
	/// Only what is recognised is dressed; the rest shows as written.
	struct dressed_report
	{
		std::string Title;
		/// [[BPR-2219]]. The review's verdict, the `**Verdict:**` line that
		/// opens its summary ("Needs changes", "Ready with comments", "Ready
		/// to merge"), and its colour; empty when the report gives none.
		std::string                  Verdict;
		report_tone                  VerdictTone = report_tone::neutral;
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

	/// \brief [[BPR-2236]]. The tone a finding's severity wears, on its card
	/// in the Agent tab as its label does on the report page: critical and
	/// major need work, minor is half way, trivial is nothing in particular.
	[[nodiscard]] auto report_tone_of(defect_severity aSeverity) noexcept -> report_tone;

	/// \brief Reads `aAnswer` by the report convention: the title (the
	/// heading it opens with, else `aTitle`), the summary, each section and
	/// its items, rows, code and suggestions, and the tally. The
	/// machine-read blocks (`bpr-claims`, `bpr-findings`) are left out. No
	/// sections: an answer that does not follow the convention, which is
	/// shown as written.
	[[nodiscard]] auto dress_report(std::string_view aAnswer, std::string_view aTitle = { }) -> dressed_report;

	/// \brief A place's path and its lines: `src/a.cpp:12` or
	/// `src/a.cpp:12-14`; none when it is not one. Of several lines written
	/// with commas (`src/a.cpp:30,71`), the first.
	struct report_place
	{
		std::string   Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;

		auto operator<=>(const report_place&) const = default;
	};
	[[nodiscard]] auto read_report_place(std::string_view aPlace) -> std::optional<report_place>;

	/// \brief [[BPR-2219]] (owner, 2026-11-16: "it reads much more like an
	/// agent transcript than an actual report"). An agent's answer is every
	/// word it said in the turn, so what it said while it worked ("Let me
	/// read the diff first.") stands before the report it then wrote.
	struct report_answer
	{
		/// What stands before the report's title: what the agent said while
		/// it worked. Empty when the answer opens with its report.
		std::string Notes;
		/// The report: from its title on. The whole answer when it has no
		/// title to start at.
		std::string Report;

		auto operator<=>(const report_answer&) const = default;
	};
	/// \brief Splits an answer at its report's title: the first level-one
	/// heading outside fenced code that a section heading follows. With no
	/// such heading, or nothing before it, the answer is the report. A
	/// level-one mark glued to a sentence's last mark ("...the review.# Review
	/// of #21": text blocks joined with nothing between them) that a heading
	/// follows is the title too. Linear; no effects.
	[[nodiscard]] auto read_report_answer(std::string_view aAnswer) -> report_answer;

	/// \brief [[BPR-2219]] (owner, 2026-11-16: "we lost the most useful
	/// feature of the agent integration: it no longer gives the ability to
	/// draft a comment from the agent response"). What a report's finding
	/// offers to keep, RFC-BPR-0127's one keep action.
	enum class report_keep : std::uint8_t {
		/// Nothing to keep: not a finding.
		none,
		/// Draft comment: a private draft at the finding's lines.
		draft_comment,
		/// Keep local: a Local comment on the finding's file, or on the pull
		/// request when it names none. No line takes a draft.
		keep_local,
	};
	[[nodiscard]] auto stable_name(report_keep aKeep) noexcept -> std::string_view;

	/// \brief One finding of a report: an item headed with a Conventional
	/// Comments label, or one that proposes a change; and, in a review (a
	/// report titled Review of ..., or one that gives a verdict), every item
	/// that names a place, so a prompt edited to another severity scheme
	/// still drafts.
	struct report_finding
	{
		/// The item's row in the report's menu.
		std::uint32_t Outline = 0U;
		/// Where it is: empty and zero when the item names no place; zero
		/// lines when it names only a file.
		std::string   Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		report_keep   Keep  = report_keep::none;
		/// What the draft or the Local comment starts from: the label and the
		/// finding in a few words, why, and the proposed change as a
		/// suggestion GitHub can apply.
		std::string Comment;
		/// [[BPR-2231]]. The finding card it is: the position in
		/// `report_findings`' `aSuggested` of the card at the same place.
		/// None when no card is at its place.
		std::optional<std::size_t> Card;

		auto operator<=>(const report_finding&) const = default;
	};

	/// \brief The findings of a report, in its order, each with its keep
	/// action (`decide_keep`'s rule: a line takes a draft, anything else is
	/// kept local) and the comment it starts from. `aSuggested` are the
	/// answer's finding cards, by place (`path`, first line, last line, the
	/// comment the agent suggested, possibly empty): a finding at the same
	/// file whose lines overlap is that card ([[BPR-2231]]: `Card`), a card
	/// with a comment before one without, each card one finding's; and it
	/// takes the agent's own comment, which follows the reviewer's comment
	/// format, in place of the one made from the item.
	struct report_suggested_comment
	{
		std::string   Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		std::string   Comment;

		auto operator<=>(const report_suggested_comment&) const = default;
	};
	[[nodiscard]] auto report_findings(const dressed_report& aReport, const std::vector<report_suggested_comment>& aSuggested = { }) -> std::vector<report_finding>;

	/// \brief [[BPR-2331]] (owner, 2026-11-20: "let's just keep in the report
	/// when they're part of the report and remove them from the agent's
	/// tab"). Which of `aCards` finding cards the report holds: those a
	/// finding of `aFindings` is (`report_finding::Card`). The Agent tab shows
	/// only the others.
	[[nodiscard]] auto report_held_cards(const std::vector<report_finding>& aFindings, std::size_t aCards) -> std::vector<bool>;

	/// \brief The report's summary as a comment on the pull request: its
	/// verdict, its summary and one line a finding with its place. Kept
	/// local (no line takes it); empty when the report has no summary.
	[[nodiscard]] auto report_summary_comment(const dressed_report& aReport) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REPORT_HPP
