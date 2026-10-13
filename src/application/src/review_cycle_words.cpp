#include "bpr/application/review_cycle_words.hpp"

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_response.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

namespace bpr::application {
	// [[BPR-1184]]. A refusal names the limit it met.
	static_assert(core::maximum_cycle_capture_entries == 250'000U, "the over-limit capture words name 250,000 entries");
	static_assert(core::maximum_response_document_bytes == 16U * 1024U * 1024U, "the over-limit response words name 16 MiB");

	auto cycle_store_words(const cycle_store_error& aError) -> std::string
	{
		switch (aError.Failure) {
		case cycle_store_failure::quota:              return "The retained-material budget is full; nothing was evicted. Remove an old cycle to make room.";
		case cycle_store_failure::conflict:           return "The cycle changed while this was prepared. Try again.";
		case cycle_store_failure::object_unavailable: return "Retained material is missing; nothing was substituted.";
		case cycle_store_failure::corrupt:            return "Retained material is damaged; nothing was substituted.";
		case cycle_store_failure::refused:            return "The cycle refused this change.";
		case cycle_store_failure::schema_mismatch:    return "This profile's Review Cycles were saved by a different version of BetterPullReview; they were left as they are.";
		case cycle_store_failure::store_damaged:      return "This profile's Review Cycles store is damaged and cannot be read; it was left as it is. Restore it from a backup of the profile.";
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

	auto cycle_repository_words(const core::git_repository_problem& aProblem) -> std::string
	{
		const auto key = aProblem.Key.empty() ? std::string{ } : " (" + aProblem.Key + ")";
		switch (aProblem.Reason) {
		case core::git_repository_refusal::malformed_config: return "This checkout's Git config" + key + " has a line Git itself would refuse to read; nothing was read from the checkout.";
		case core::git_repository_refusal::format_version:   return "This checkout's Git repository format version" + key + " is one BetterPullReview does not read; nothing was read from the checkout.";
		case core::git_repository_refusal::unknown_extension:
			return "This checkout's Git repository uses an extension" + key + " that BetterPullReview does not read; nothing was read from the checkout.";
		case core::git_repository_refusal::version_zero_extension:
			return "This checkout's Git config sets an extension" + key + " that Git refuses in a version 0 repository; nothing was read from the checkout.";
		case core::git_repository_refusal::object_format: return "This checkout's Git config names an object format" + key + " Git does not know; nothing was read from the checkout.";
		case core::git_repository_refusal::ref_storage:   return "This checkout's Git config names a reference format" + key + " Git does not know; nothing was read from the checkout.";
		case core::git_repository_refusal::reftable:
			return "This checkout stores its branches in Git's reftable format, which BetterPullReview does not read yet; nothing was read from the checkout.";
		case core::git_repository_refusal::invalid_value:   return "This checkout's Git config gives a setting" + key + " a value Git itself refuses; nothing was read from the checkout.";
		case core::git_repository_refusal::index_format:    return "This checkout's Git index is in a form BetterPullReview does not read; nothing was read from the checkout.";
		case core::git_repository_refusal::index_checksum:  return "This checkout's Git index is damaged: its checksum does not match; nothing was read from the checkout.";
		case core::git_repository_refusal::index_extension: return "This checkout's Git index uses a required extension BetterPullReview does not read; nothing was read from the checkout.";
		case core::git_repository_refusal::rule_file_bound:
			return "This checkout's ignore or attributes file" + key + " is larger than BetterPullReview reads (1 MiB or 100,000 rules), so paths it ignores could be captured; nothing was recorded.";
		}
		return "This checkout's Git repository is one BetterPullReview does not read; nothing was read from the checkout.";
	}

	namespace {
		/// [[BPR-1183]]. The cycle's location answers with another root than the
		/// one it was bound to, found before reading or while reading. Capturing
		/// again reads the same location and is refused again, so the words say
		/// what can change that rather than to retry.
		auto cycle_root_changed_words(const std::optional<core::workspace_verification> aVerification) -> std::string
		{
			switch (aVerification.value_or(core::workspace_verification::replaced)) {
			case core::workspace_verification::repository_changed:
				return "The folder is the one this cycle captured, but its Git repository or worktree is not; nothing was recorded. This cycle captures only the checkout it was opened "
					   "on, unless you rebind it to this one.";
			case core::workspace_verification::kind_changed:
				return "What this cycle captured changed kind: a file became a folder or the reverse, or the checkout lost its repository; nothing was recorded.";
			default:
				// [[BPR-1279]]. The way forward is a confirmed rebind, or moving it back.
				return "Another folder now answers at this cycle's location: the one it captured was moved, replaced or recreated. Nothing was recorded, and capturing "
					   "again reads the same folder. Rebind the cycle to where its folder is now, or move the folder back; open the folder now there to review it as a "
					   "cycle of its own.";
			}
		}

		auto counted(const std::uint64_t aCount, const std::string_view aNoun) -> std::string
		{
			return grouped_count(aCount) + " " + std::string{ aNoun } + (aCount == 1U ? "" : "s");
		}
	}

	auto cycle_capture_offers_rebind(const cycle_capture_outcome& aOutcome) noexcept -> bool
	{
		return aOutcome.Status == cycle_capture_status::root_changed || (aOutcome.Status == cycle_capture_status::workspace && aOutcome.Workspace == workspace_failure::missing);
	}

	auto cycle_material_check_words(const core::workspace_material_check& aCheck, const bool aCaptured) -> std::string
	{
		if (!aCaptured) {
			return "Nothing is captured yet, so there is nothing to compare.";
		}
		return "Compared with the latest capture's " + counted(aCheck.Compared, "file") + ": " + grouped_count(aCheck.Matching) + " the same, " + grouped_count(aCheck.Differing)
			+ " changed, " + grouped_count(aCheck.Missing) + " missing and " + grouped_count(aCheck.Added) + " new.";
	}

	auto cycle_rebind_words(const core::workspace_rebind_problem aProblem, const std::string_view aOther) -> std::string
	{
		switch (aProblem) {
		case core::workspace_rebind_problem::none:      return { };
		case core::workspace_rebind_problem::unbound:   return "This cycle has no folder to rebind; open the folder as a cycle instead. Nothing was changed.";
		case core::workspace_rebind_problem::archived:  return "The cycle is archived, and an archived cycle is not rebound. Nothing was changed.";
		case core::workspace_rebind_problem::unchanged: return "The cycle is already bound to that folder, at that location. Nothing was changed.";
		case core::workspace_rebind_problem::kind_changed:
			return "A cycle that reviews a folder can be rebound only to a folder, and one that reviews a file only to a file. Nothing was changed.";
		case core::workspace_rebind_problem::file_scope: return "A single file cannot hold the paths this cycle's scope names. Nothing was changed.";
		case core::workspace_rebind_problem::bound_elsewhere:
			return "That folder is already the project of " + std::string{ aOther } + ", and a cycle never takes over another cycle's folder. Nothing was changed.";
		}
		return { };
	}

	auto cycle_rebind_check_words(const cycle_material_check_outcome& aOutcome) -> std::string
	{
		switch (aOutcome.Status) {
		case cycle_capture_status::captured: return { };
		case cycle_capture_status::workspace:
			if (aOutcome.Repository) {
				return cycle_repository_words(*aOutcome.Repository) + " Nothing was changed.";
			}
			return (aOutcome.Workspace ? cycle_workspace_words(*aOutcome.Workspace) : std::string{ "The chosen folder could not be read." }) + " Nothing was changed.";
		case cycle_capture_status::too_many_entries: return "The folder holds more than 250,000 entries, the most one capture keeps; nothing was changed.";
		case cycle_capture_status::store:            return aOutcome.Store ? cycle_store_words(*aOutcome.Store) : "The local cycle store is unavailable.";
		case cycle_capture_status::cancelled:        return { };
		default:                                     return "The chosen folder could not be compared; nothing was changed.";
		}
	}

	auto cycle_capture_words(const cycle_capture_outcome& aOutcome) -> std::string
	{
		switch (aOutcome.Status) {
		case cycle_capture_status::captured:         return { };
		case cycle_capture_status::unstable:         return "Files kept changing while they were read; nothing was recorded. Capture again when the folder is quiet.";
		case cycle_capture_status::root_changed:     return cycle_root_changed_words(aOutcome.Verification);
		case cycle_capture_status::too_many_entries: return "The folder holds more than 250,000 entries, the most one capture keeps; nothing was recorded. Narrow the scope.";
		case cycle_capture_status::store:            return aOutcome.Store ? cycle_store_words(*aOutcome.Store) : "The local cycle store is unavailable.";
		case cycle_capture_status::workspace:
			if (aOutcome.Workspace == workspace_failure::missing) {
				// [[BPR-1279]]. Moved away with nothing in its place.
				return "This cycle's folder is no longer at its location; nothing was recorded. Rebind the cycle to where the folder is now, or move it back.";
			}
			if (aOutcome.Repository) {
				return cycle_repository_words(*aOutcome.Repository);
			}
			return aOutcome.Workspace ? cycle_workspace_words(*aOutcome.Workspace) : "The chosen folder could not be read.";
		case cycle_capture_status::not_bound: return "The cycle has no folder to capture.";
		case cycle_capture_status::cancelled: return { };
		}
		return { };
	}

	auto cycle_response_words(const cycle_response_result& aResult) -> std::string
	{
		if (aResult.Read) {
			// [[BPR-1181]]. A bound or a link is said as such, not as an unreadable file.
			switch (*aResult.Read) {
			case cycle_export_failure::too_large:   return "The chosen file is larger than a response can be (16 MiB); nothing was read or added.";
			case cycle_export_failure::not_regular: return "The chosen file is a link, a folder or another special file, which is never followed or read; nothing was added.";
			default:                                return "The chosen file could not be read; nothing was added.";
			}
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
		case cycle_export_failure::read_only:   return "The file there is read-only; nothing was replaced.";
		case cycle_export_failure::in_use:      return "Another program has the file there open; nothing was replaced. Close it and save again.";
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
