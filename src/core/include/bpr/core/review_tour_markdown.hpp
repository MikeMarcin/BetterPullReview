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
	/// \brief Destinations are already admitted by the host; the codec additionally refuses Markdown injection.
	[[nodiscard]] auto render_tour_index(std::span<const tour_markdown_link> aEntries, std::string_view aNewline = "\n") -> std::optional<std::string>;
	[[nodiscard]] auto render_tour_stop(std::string_view aTitle, std::size_t aOrdinal, std::string_view aNarrative,
										std::span<const tour_markdown_link> aNavigation, std::string_view aPinnedSource = { }, std::string_view aNewline = "\n") -> std::optional<std::string>;
}
#endif
