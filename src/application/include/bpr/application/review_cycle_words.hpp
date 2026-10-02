#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORDS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORDS_HPP

#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/workspace_source_port.hpp"

#include <cstdint>
#include <string>

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
	/// \brief A count with thousands separators: `16,777,216`.
	[[nodiscard]] auto grouped_count(std::uint64_t aValue) -> std::string;
	/// \brief An intelligible default title for a cycle opened at a chosen
	/// location: the folder's or file's own name.
	[[nodiscard]] auto default_cycle_title(const std::string& aLocation) -> std::string;
}

#endif
