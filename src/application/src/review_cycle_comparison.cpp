#include "bpr/application/review_cycle_comparison.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace bpr::application {
	review_cycle_comparison_host::review_cycle_comparison_host(review_cycle_store& aStore, const core::local_cycle_authority aAuthority,
															   std::shared_ptr<diff_line_matcher_port> aMatcher) noexcept
		: Store{ aStore }
		, Authority{ aAuthority }
		, Bridge{ nullptr, &aStore, aAuthority }
		, Matcher{ std::move(aMatcher) }
	{
	}

	auto review_cycle_comparison_host::manifest(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision, std::stop_token aStop)
		-> std::expected<std::shared_ptr<const core::cycle_manifest>, review_material_status>
	{
		if (aSubject.Authority != Authority || !core::valid_cycle_id(aRevision)) {
			return std::unexpected(review_material_status::wrong_authority);
		}
		const auto cached = std::ranges::find_if(Cache, [&](const cached_manifest& aEntry) { return aEntry.Cycle == aSubject.Cycle && aEntry.Revision == aRevision; });
		if (cached != Cache.end()) {
			return cached->Manifest;
		}
		const auto before = aRevision.value() == 1U ? std::nullopt : std::optional{ core::cycle_source_revision_id{ aRevision.value() - 1U } };
		const auto page   = Store.revisions(aSubject, before, aStop);
		if (!page) {
			return std::unexpected(page.error().Failure == cycle_store_failure::cancelled ? review_material_status::cancelled : review_material_status::missing);
		}
		if (page->Revisions.empty() || page->Revisions.front().Revision.Id.Id != aRevision) {
			return std::unexpected(review_material_status::missing);
		}
		const auto& stored = page->Revisions.front();
		auto        bytes  = Bridge.materialize(cycle_material_request{ .Subject = aSubject, .Revision = aRevision, .Object = stored.Manifest, .MaximumBytes = maximum_cycle_object_bytes }, aStop);
		if (bytes.Status != review_material_status::available) {
			return std::unexpected(bytes.Status);
		}
		auto decoded = core::decode_manifest(bytes.Bytes);
		if (!decoded || core::observe_manifest(*decoded, stored.Manifest.Digest, stored.Revision.Capture.WorkspaceVersion) != stored.Revision.Capture) {
			return std::unexpected(review_material_status::corrupt);
		}
		auto shared = std::make_shared<const core::cycle_manifest>(std::move(*decoded));
		if (Cache.size() == cached_manifests) {
			Cache.erase(Cache.begin());
		}
		Cache.push_back({ .Cycle = aSubject.Cycle, .Revision = aRevision, .Manifest = shared });
		return shared;
	}

	auto review_cycle_comparison_host::page(const core::local_cycle_subject& aSubject, const core::cycle_comparison_pair& aPair, const std::optional<std::string>& aAfter,
											const std::size_t aLimit, const bool aChangedOnly, std::stop_token aStop) -> cycle_comparison_page_observation
	{
		const auto right = manifest(aSubject, aPair.Right.Id, aStop);
		if (!right) {
			return { .Status = right.error() };
		}
		auto left = std::shared_ptr<const core::cycle_manifest>{ };
		if (aPair.Left) {
			auto loaded = manifest(aSubject, aPair.Left->Id, aStop);
			if (!loaded) {
				return { .Status = loaded.error() };
			}
			left = std::move(*loaded);
		}
		if (aStop.stop_requested()) {
			return { .Status = review_material_status::cancelled };
		}
		return { .Status = review_material_status::available, .Page = core::compare_manifests(left.get(), **right, aAfter, aLimit, aChangedOnly) };
	}

	auto review_cycle_comparison_host::content(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_ref& aRevision, const core::cycle_path_side& aSide,
											   const std::uint64_t aMaximumBytes, std::stop_token aStop) -> review_material_observation
	{
		if (!aSide.Content) {
			return { .Status = review_material_status::missing };
		}
		return Bridge.materialize(cycle_material_request{ .Subject = aSubject, .Revision = aRevision.Id, .Object = { .Digest = *aSide.Content, .Size = aSide.Size }, .MaximumBytes = aMaximumBytes },
								  std::move(aStop));
	}

	auto review_cycle_comparison_host::file(const core::local_cycle_subject& aSubject, const core::cycle_comparison_pair& aPair, const std::string_view aPath,
											const core::diff_options& aOptions, std::stop_token aStop) -> cycle_file_observation
	{
		const auto right = manifest(aSubject, aPair.Right.Id, aStop);
		if (!right) {
			return { .Status = right.error() };
		}
		auto left = std::shared_ptr<const core::cycle_manifest>{ };
		if (aPair.Left) {
			auto loaded = manifest(aSubject, aPair.Left->Id, aStop);
			if (!loaded) {
				return { .Status = loaded.error() };
			}
			left = std::move(*loaded);
		}
		auto        observation = cycle_file_observation{ .Path = core::compare_manifest_path(left.get(), **right, aPath) };
		const auto& path        = *observation.Path;
		// A diff needs each side present or known absent; an unavailable or
		// unknown side is never shown as an empty file.
		const auto comparable = [](const core::cycle_path_side& aSide) {
			return aSide.State == core::cycle_path_state::present || aSide.State == core::cycle_path_state::absent || aSide.State == core::cycle_path_state::deleted;
		};
		// [[BPR-1184]]. Bytes over the capture's file budget were never kept:
		// said as too large, not as missing.
		const auto overBudget = [&](const core::cycle_manifest* aManifest) {
			const auto* entry = aManifest != nullptr ? core::find_entry(*aManifest, aPath) : nullptr;
			return entry != nullptr && entry->Availability == core::manifest_availability::too_large;
		};
		if (!aPair.Left) {
			// Full source: one retained side, read on its own.
			auto bytes         = content(aSubject, aPair.Right, path.Right, maximum_cycle_object_bytes, aStop);
			observation.Status = bytes.Status == review_material_status::missing && overBudget(right->get()) ? review_material_status::too_large : bytes.Status;
			observation.Source = std::move(bytes.Bytes);
			return observation;
		}
		if (!comparable(path.Right) || !path.Left || !comparable(*path.Left)) {
			observation.Status = overBudget(right->get()) || overBudget(left.get()) ? review_material_status::too_large : review_material_status::missing;
			return observation;
		}
		const auto limits = core::diff_compute_limits{ };
		auto       before = path.Left && path.Left->State == core::cycle_path_state::present ? std::optional{ content(aSubject, *aPair.Left, *path.Left, limits.MaximumBytesPerSide, aStop) } : std::nullopt;
		auto       after  = path.Right.State == core::cycle_path_state::present ? std::optional{ content(aSubject, aPair.Right, path.Right, limits.MaximumBytesPerSide, aStop) } : std::nullopt;
		for (const auto* side : { &before, &after }) {
			if (*side && (*side)->Status != review_material_status::available) {
				observation.Status = (*side)->Status;
				return observation;
			}
		}
		const auto key = core::cycle_file_comparison(aSubject, aPair, aPath, before ? path.Left->Content : std::nullopt, after ? path.Right.Content : std::nullopt, aOptions);
		if (!key) {
			// Neither side holds readable content: the path comparison is all there is.
			observation.Status = review_material_status::missing;
			return observation;
		}
		const auto spanOf = [](const std::optional<review_material_observation>& aSide) -> std::optional<std::span<const std::uint8_t>> {
			return aSide ? std::optional{ std::span<const std::uint8_t>{ aSide->Bytes } } : std::nullopt;
		};
		// [[BPR-1485]] The host's line matcher, abandoned on a stop.
		auto check    = stop_token_check{ aStop };
		auto matching = abandonable_diff_line_matching{ Matcher, aStop };
		auto run      = core::compute_material_diff({ .Key = *key, .Options = aOptions, .LeftBytes = spanOf(before), .RightBytes = spanOf(after), .Limits = limits },
													core::diff_compute_control{ .Stop = &check }, matching);
		if (!run.Outcome.has_value() || aStop.stop_requested()) {
			observation.Status = review_material_status::cancelled;
			return observation;
		}
		observation.Reason = run.Outcome->Reason;
		observation.Diff   = std::move(run.Outcome->Result);
		observation.Status = review_material_status::available;
		return observation;
	}
}
