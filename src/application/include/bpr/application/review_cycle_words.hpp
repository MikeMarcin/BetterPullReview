#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORDS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORDS_HPP

#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/workspace_source_port.hpp"

#include <cstdint>
#include <string>
#include <string_view>

/// \file
/// \brief The plain words a local Review Cycle's frontends say about an
/// outcome (BPR-1167): the desktop's status line and the terminal's message
/// say the same thing about the same result. Each is empty when there is
/// nothing to say.
namespace bpr::application {
	[[nodiscard]] auto cycle_store_words(const cycle_store_error& aError) -> std::string;
	[[nodiscard]] auto cycle_workspace_words(workspace_failure aFailure) -> std::string;
	[[nodiscard]] auto cycle_capture_words(const cycle_capture_outcome& aOutcome) -> std::string;
	[[nodiscard]] auto cycle_response_words(const cycle_response_result& aResult) -> std::string;
	[[nodiscard]] auto cycle_export_words(cycle_export_failure aFailure) -> std::string;
	/// \brief [[BPR-1279]]. Whether a refused capture's way forward is an
	/// explicit rebind: another root answers at the cycle's location, or
	/// nothing does.
	[[nodiscard]] auto cycle_capture_offers_rebind(const cycle_capture_outcome& aOutcome) noexcept -> bool;
	/// \brief How a rebind candidate compares with the latest capture.
	[[nodiscard]] auto cycle_material_check_words(const core::workspace_material_check& aCheck, bool aCaptured) -> std::string;
	/// \brief Why a rebind is not offered; `aOther` names the cycle bound to
	/// the candidate, for `bound_elsewhere`.
	[[nodiscard]] auto cycle_rebind_words(core::workspace_rebind_problem aProblem, std::string_view aOther = { }) -> std::string;
	/// \brief Why a rebind candidate could not be checked; empty when it was.
	[[nodiscard]] auto cycle_rebind_check_words(const cycle_material_check_outcome& aOutcome) -> std::string;
	/// \brief A count with thousands separators: `16,777,216`.
	[[nodiscard]] auto grouped_count(std::uint64_t aValue) -> std::string;
	/// \brief An intelligible default title for a cycle opened at a chosen
	/// location: the folder's or file's own name.
	[[nodiscard]] auto default_cycle_title(const std::string& aLocation) -> std::string;
}

#endif
