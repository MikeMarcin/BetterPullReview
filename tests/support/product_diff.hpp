#ifndef BPR_TESTS_SUPPORT_PRODUCT_DIFF_HPP
#define BPR_TESTS_SUPPORT_PRODUCT_DIFF_HPP

// [[BPR-1485]]. The diff the product computes: the core engine with the
// application's line matcher port, behind which is the linked xdiff adapter
// (Myers with the indent heuristic). The core has no matcher of its own, so a
// test that wants the product's alignment computes it here. Link the
// `bpr_test_product_diff` target.

#include "bpr/adapters/xdiff/xdiff_line_matcher.hpp"
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_compute_components.hpp"

#include <memory>

namespace bpr::tests {
	/// \brief The product's line matcher, shared by every call.
	[[nodiscard]] inline auto product_line_matcher() -> const std::shared_ptr<application::diff_line_matcher_port>&
	{
		static const auto matcher = adapters::xdiff::make_xdiff_line_matcher();
		return matcher;
	}

	[[nodiscard]] inline auto product_file_diff(const core::diff_compute_input& aInput) -> core::diff_compute_outcome
	{
		auto matching = application::diff_line_matching{ *product_line_matcher() };
		return core::compute_file_diff(aInput, matching);
	}

	[[nodiscard]] inline auto product_material_diff(const core::material_diff_input& aInput) -> core::material_diff_outcome
	{
		auto matching = application::diff_line_matching{ *product_line_matcher() };
		return core::compute_material_diff(aInput, matching);
	}
} // namespace bpr::tests

#endif // BPR_TESTS_SUPPORT_PRODUCT_DIFF_HPP
