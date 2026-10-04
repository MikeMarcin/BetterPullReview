#include "bpr/application/review_cycle_words.hpp"

#include "bpr/core/review_cycle.hpp"

#include <cstddef>
#include <filesystem>

namespace bpr::application {
	// [[BPR-1184]]. A refusal names the limit it met.
	static_assert(core::maximum_cycle_capture_entries == 250'000U, "the over-limit capture words name 250,000 entries");

	auto cycle_store_words(const cycle_store_error& aError) -> std::string
	{
		switch (aError.Failure) {
		case cycle_store_failure::quota:              return "The retained-material budget is full; nothing was evicted. Remove an old cycle to make room.";
		case cycle_store_failure::conflict:           return "The cycle changed while this was prepared. Try again.";
		case cycle_store_failure::object_unavailable: return "Retained material is missing; nothing was substituted.";
		case cycle_store_failure::corrupt:            return "Retained material is damaged; nothing was substituted.";
		case cycle_store_failure::refused:            return "The cycle refused this change.";
		case cycle_store_failure::schema_mismatch:    return "This profile's Review Cycles were saved by a different version of BetterPullReview; they were left as they are.";
		case cycle_store_failure::cancelled:          return { };
		default:                                      return "The local cycle store is unavailable.";
		}
	}

	auto cycle_workspace_words(const workspace_failure aFailure) -> std::string
	{
		switch (aFailure) {
		case workspace_failure::missing:           return "The chosen folder or file no longer exists.";
		case workspace_failure::permission_denied: return "The chosen folder or file cannot be read.";
		case workspace_failure::unsupported:       return "The chosen item is not a folder or a regular file.";
		default:                                   return "The chosen folder or file could not be opened.";
		}
	}

	auto cycle_capture_words(const cycle_capture_outcome& aOutcome) -> std::string
	{
		switch (aOutcome.Status) {
		case cycle_capture_status::captured:         return { };
		case cycle_capture_status::unstable:         return "Files kept changing while they were read; nothing was recorded. Capture again when the folder is quiet.";
		case cycle_capture_status::root_changed:     return "The chosen folder was replaced while it was read; nothing was recorded.";
		case cycle_capture_status::too_many_entries: return "The folder holds more than 250,000 entries, the most one capture keeps; nothing was recorded. Narrow the scope.";
		case cycle_capture_status::store:            return aOutcome.Store ? cycle_store_words(*aOutcome.Store) : "The local cycle store is unavailable.";
		case cycle_capture_status::workspace:        return aOutcome.Workspace ? cycle_workspace_words(*aOutcome.Workspace) : "The chosen folder could not be read.";
		case cycle_capture_status::not_bound:        return "The cycle has no folder to capture.";
		case cycle_capture_status::cancelled:        return { };
		}
		return { };
	}

	auto cycle_response_words(const cycle_response_result& aResult) -> std::string
	{
		if (aResult.Read) {
			return "The chosen file could not be read; nothing was added.";
		}
		if (aResult.Decode) {
			return "The file is not a response this build reads (" + std::string{ core::stable_name(aResult.Decode->Problem) } + "); nothing was added.";
		}
		if (!aResult.Refusal) {
			return { };
		}
		switch (aResult.Refusal->Problem) {
		case core::cycle_response_problem::wrong_cycle:       return "The response is for another cycle; nothing was added.";
		case core::cycle_response_problem::wrong_round:       return "The response is for another round; nothing was added.";
		case core::cycle_response_problem::wrong_digest:      return "The response names this round with other content than was frozen; nothing was added.";
		case core::cycle_response_problem::unknown_item:      return "The reply names an item this round does not hold; nothing was added.";
		case core::cycle_response_problem::stale_item:        return "The response answers another revision of an item; nothing was added.";
		case core::cycle_response_problem::empty:             return "A reply needs some text.";
		case core::cycle_response_problem::reused_identifier: return "A different response with the same identifier was already added; nothing was added.";
		}
		return { };
	}

	auto cycle_export_words(const cycle_export_failure aFailure) -> std::string
	{
		switch (aFailure) {
		case cycle_export_failure::exists:      return "Another file appeared there while saving; nothing was replaced.";
		case cycle_export_failure::changed:     return "The file there changed after you chose to replace it; nothing was replaced.";
		case cycle_export_failure::not_regular: return "That is a link, a folder or another special file; nothing was written.";
		case cycle_export_failure::too_large:   return "The file there is too large to compare; nothing was written.";
		case cycle_export_failure::cancelled:   return { };
		default:                                return "The file could not be written there; nothing was saved.";
		}
	}

	auto grouped_count(const std::uint64_t aValue) -> std::string
	{
		auto digits = std::to_string(aValue);
		for (auto at = static_cast<std::ptrdiff_t>(digits.size()) - 3; at > 0; at -= 3) {
			digits.insert(static_cast<std::size_t>(at), ",");
		}
		return digits;
	}

	auto default_cycle_title(const std::string& aLocation) -> std::string
	{
		auto path = std::filesystem::path{ aLocation };
		while (path.has_relative_path() && path.filename().empty()) {
			path = path.parent_path();
		}
		auto name = path.filename().string();
		if (name.empty()) {
			return "Local project";
		}
		return name.substr(0, core::maximum_cycle_title_bytes);
	}
}
