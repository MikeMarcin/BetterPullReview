#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_WRITING_STYLE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_WRITING_STYLE_HPP

#include <string_view>

/// \file
/// \brief [[BPR-1915]]. The style section the built-in tour prompts carry.
///
/// \details The owner, 2026-11-06: "It also feels like the output of the
/// draft is entirely devoid of any formatting. It feels just like big walls
/// of text." Tour fields are GitHub-flavoured Markdown: the Tour tab's
/// preview, its stop reader, the proposal and the conversation render it
/// with the same reader, and publishing posts the same bytes to GitHub,
/// which renders them as GitHub does. This section asks for the structure
/// that renders in both places. It is one named piece, so a reviewer's own
/// writing guidance ([[BPR-1916]]) can stand in for it.
namespace bpr::application {
	/// \brief The style section: plain, precise and scannable Markdown, with
	/// the length bounds kept.
	[[nodiscard]] auto tour_writing_style() noexcept -> std::string_view;
}

#endif
