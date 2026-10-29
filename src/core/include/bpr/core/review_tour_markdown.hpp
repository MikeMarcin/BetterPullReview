#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_MARKDOWN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_MARKDOWN_HPP

#include "bpr/core/review_tour.hpp"
#include <span>
#include <string_view>

namespace bpr::core {
	/// \brief Half-open UTF-8 byte span into an immutable raw Markdown baseline.
	struct tour_text_span
	{
		std::size_t Begin                                           = 0;
		std::size_t End                                             = 0;
		auto        operator==(const tour_text_span&) const -> bool = default;
	};
	struct tour_markdown_link
	{
		std::string Label;
		std::string Destination;
		auto        operator==(const tour_markdown_link&) const -> bool = default;
	};
	struct tour_markdown_section
	{
		std::string    Name;
		tour_text_span Content;
	};
	struct tour_markdown_list
	{
		tour_text_span                  Source;
		std::vector<tour_markdown_link> Entries;
		bool                            MappingNeeded = false;
	};
	/// \brief Conservative structural mapping; duplicate fields/lists stay separate for manual choice.
	struct tour_markdown_map
	{
		std::vector<tour_markdown_section> Sections;
		std::vector<tour_markdown_list>    Lists;
		std::vector<tour_markdown_link>    Links;
		bool                               Bounded = true;
	};
	struct tour_stop_markdown
	{
		std::optional<tour_text_span> Heading;
		std::optional<tour_text_span> Narrative;
		std::optional<tour_text_span> Footer;
		std::string                   Title;
	};
	struct tour_text_edit
	{
		tour_text_span Span;
		std::string    Expected;
		std::string    Replacement;
	};
	/// \brief Scan top-level sections/lists, excluding code, quotes, nested items and HTML regions.
	[[nodiscard]] auto map_tour_markdown(std::string_view aRaw) -> tour_markdown_map;
	/// \brief Recognize only the generated heading and an exact final navigation paragraph.
	[[nodiscard]] auto map_tour_stop(std::string_view aRaw) -> tour_stop_markdown;
	/// \brief Apply disjoint explicitly selected spans against their exact baseline; preserve every other byte.
	[[nodiscard]] auto edit_tour_markdown(std::string_view aRaw, std::span<const tour_text_edit> aEdits) -> std::optional<std::string>;
	/// \brief Preview insertion into an unstructured description at an explicitly chosen byte boundary.
	[[nodiscard]] auto insert_tour_brief(std::string_view aRaw, std::size_t aAt, std::string_view aProblem, std::string_view aSolution, std::string_view aNewline = "\n") -> std::optional<std::string>;
	[[nodiscard]] auto tour_markdown_label(std::string_view aLabel) -> std::string;
	/// \brief [[BPR-1222]]. Where a tour's index and Previous/Next take a
	/// reader: a review comment's Files changed anchor. GitHub returns a review
	/// comment as `…/pull/N#discussion_rID` (the Conversation tab); the Files
	/// changed page carries the same comment as `id="rID"`, so the link is
	/// `…/pull/N/files#rID`. Any other destination (a review, a conversation
	/// comment) is returned unchanged.
	[[nodiscard]] auto tour_stop_link(std::string_view aWebUrl) -> std::string;
	/// \brief The comment URL GitHub returned for a route link written in
	/// either form, so a tour published before [[BPR-1222]] still matches its
	/// comments. Anything else is returned unchanged.
	[[nodiscard]] auto tour_link_identity(std::string_view aLink) -> std::string;
	/// \brief Destinations are already admitted by the host; the codec additionally refuses Markdown injection.
	[[nodiscard]] auto render_tour_index(std::span<const tour_markdown_link> aEntries, std::string_view aNewline = "\n") -> std::optional<std::string>;
	/// \brief [[BPR-1785]]. The line under a stop's Previous or Next link: GitHub
	/// does not jump to an anchor inside a diff it hides as too large.
	inline constexpr std::string_view tour_navigation_note = "_If Next or Previous doesn't jump (GitHub hides large diffs), use the Tour overview._";
	/// \brief Adds the note under a published stop's links when they hold Previous
	/// or Next and it is missing; any other body comes back unchanged.
	[[nodiscard]] auto with_tour_navigation_note(std::string_view aRaw) -> std::string;
	[[nodiscard]] auto render_tour_stop(std::string_view aTitle, std::size_t aOrdinal, std::string_view aNarrative,
										std::span<const tour_markdown_link> aNavigation, std::string_view aPinnedSource = { }, std::string_view aNewline = "\n") -> std::optional<std::string>;

	/// \brief [[BPR-1806]]. A published review comment read as the tour stop
	/// `render_tour_stop` wrote: what a reader of the comment needs, with the
	/// website's scaffolding (pinned source, generated heading, Previous /
	/// Tour overview / Next links and their note) named so it can be set aside.
	struct published_tour_stop
	{
		/// The one-based stop number the heading carries.
		std::size_t Ordinal = 0;
		/// The heading's title with the publisher's Markdown escapes removed.
		std::string Title;
		/// The narrative, without the blank lines around it; never empty.
		tour_text_span Narrative;
		/// The pinned source line before the heading, when it has one.
		std::optional<tour_text_span> PinnedSource;
		/// The Tour overview link's destination, shared by a tour's stops.
		std::string                Overview;
		std::optional<std::string> Previous;
		std::optional<std::string> Next;
		auto                       operator==(const published_tour_stop&) const -> bool = default;
	};

	/// \brief [[BPR-1806]]. Recognizes exactly the envelope `render_tour_stop`
	/// publishes, in either newline style, and nothing looser: an optional
	/// pinned source alone on the first line and a blank line; the
	/// `### PR Tour · N — Title` heading (or the legacy `### Author's tour · `)
	/// on the next; a blank line; a non-empty narrative; a blank line; the
	/// navigation paragraph (Tour overview, after Previous unless the stop is
	/// the first, before Next when there is one); its optional note; and
	/// trailing blank lines only. `N` is a decimal without a leading zero, at
	/// most `maximum_tour_stops`. Any other body is not a tour stop.
	[[nodiscard]] auto recognize_published_tour_stop(std::string_view aBody) -> std::optional<published_tour_stop>;

	/// \brief [[BPR-1806]]. How many stops each recognized stop's tour has, in
	/// the order given, when the stops at hand prove it; none otherwise.
	/// \details Stops are one tour when they name the same Tour overview. A
	/// tour's count is known only when its stops are numbered 1 to M once
	/// each, every stop but the first has Previous and every stop but the last
	/// has Next: then each of them is one of M. A tour still being published
	/// (no Previous or Next yet), a missing stop, or a number given twice
	/// leaves its count unknown.
	[[nodiscard]] auto published_tour_stop_counts(std::span<const published_tour_stop> aStops) -> std::vector<std::optional<std::size_t>>;

	/// \brief [[BPR-1806]]. The stop of `aTour` a published comment is: the
	/// stop bound to that very comment (`review-comment:<id>` for a line
	/// comment, `issue-comment:<id>` for a conversation comment, as the
	/// conversation names its posts), else the bound stop at the comment's
	/// number when its title is the comment's; none when neither holds.
	[[nodiscard]] auto find_published_tour_stop(const review_tour& aTour, std::string_view aPost, const published_tour_stop& aStop) -> std::optional<tour_stop_id>;
}
#endif
