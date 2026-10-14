#ifndef BPR_SRC_ADAPTERS_XDIFF_INCLUDE_BPR_ADAPTERS_XDIFF_XDIFF_LINE_MATCHER_HPP
#define BPR_SRC_ADAPTERS_XDIFF_INCLUDE_BPR_ADAPTERS_XDIFF_XDIFF_LINE_MATCHER_HPP

#include "bpr/application/diff_line_matcher.hpp"

#include <memory>

/// \file
/// \brief [[BPR-1485]]. The application's line matcher: Git 2.56.0's xdiff,
/// Myers with the indent heuristic, linked dynamically as its own shared
/// library (RFC-BPR-0082, `l-myers-indent`). No xdiff type leaves the adapter.
namespace bpr::adapters::xdiff {
	/// \brief The product's line matcher. Stateless and safe to call from
	/// several threads at once.
	[[nodiscard]] auto make_xdiff_line_matcher() -> std::shared_ptr<application::diff_line_matcher_port>;
} // namespace bpr::adapters::xdiff

#endif // BPR_SRC_ADAPTERS_XDIFF_INCLUDE_BPR_ADAPTERS_XDIFF_XDIFF_LINE_MATCHER_HPP
