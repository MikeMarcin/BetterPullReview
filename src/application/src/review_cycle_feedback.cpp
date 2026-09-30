#include "bpr/application/review_cycle_feedback.hpp"

#include <string_view>
#include <variant>

namespace bpr::application {
	namespace {
		auto path_of(const core::cycle_feedback_target& aTarget) -> std::optional<std::string_view>
		{
			if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
				return file->Path;
			}
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&aTarget)) {
				return anchor->Path;
			}
			return std::nullopt;
		}

		auto failure_of(const review_material_status aStatus) -> cycle_store_error
		{
			switch (aStatus) {
			case review_material_status::missing:         return { .Failure = cycle_store_failure::object_unavailable };
			case review_material_status::corrupt:         return { .Failure = cycle_store_failure::corrupt };
			case review_material_status::cancelled:       return { .Failure = cycle_store_failure::cancelled };
			case review_material_status::too_large:       return { .Failure = cycle_store_failure::bounds };
			case review_material_status::wrong_authority: return { .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity };
			default:                                      return { .Failure = cycle_store_failure::unavailable };
			}
		}

		auto full_source(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision) -> core::cycle_comparison_pair
		{
			return { .Left = std::nullopt, .Right = { .Cycle = aSubject.Cycle, .Id = aRevision } };
		}
	}

	auto verify_feedback_targets(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const std::span<const core::cycle_feedback_target* const> aTargets,
								 std::stop_token aStop) -> cycle_store_result<void>
	{
		for (const auto* target : aTargets) {
			const auto path     = path_of(*target);
			const auto revision = core::source_revision_of(*target);
			if (!path || !revision) {
				continue;
			}
			const auto file = aHost.file(aSubject, full_source(aSubject, *revision), *path, { }, aStop);
			if (file.Status != review_material_status::available) {
				// A path the revision does not hold as a readable file is not a valid target.
				const auto held = file.Path && file.Path->Right.State == core::cycle_path_state::present;
				return std::unexpected(held || file.Status == review_material_status::cancelled ? failure_of(file.Status)
																								: cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
			}
			if (!core::verify_target(*target, file.Source)) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
			}
		}
		return { };
	}

	auto observe_feedback_target(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_feedback_target& aTarget,
								 const core::cycle_source_revision_id aAgainst, std::stop_token aStop) -> cycle_correspondence_observation
	{
		const auto path = path_of(aTarget);
		if (!path) {
			return { .Status = review_material_status::available };
		}
		const auto file = aHost.file(aSubject, full_source(aSubject, aAgainst), *path, { }, aStop);
		if (!file.Path) {
			return { .Status = file.Status };
		}
		const auto& side    = file.Path->Right;
		auto        current = core::cycle_current_source{ .Revision = aAgainst, .State = side.State, .Blob = side.Content };
		if (file.Status == review_material_status::available) {
			current.Bytes = std::span<const std::uint8_t>{ file.Source };
		}
		return { .Status = file.Status, .Correspondence = core::observe_correspondence(aTarget, current) };
	}
}
