#include "bpr/core/review_cycle_comparison.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <span>

namespace bpr::core {
	namespace {
		auto allocated(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		}

		auto reference(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> cycle_source_revision_ref
		{
			return { .Cycle = aCycle.Subject.Cycle, .Id = aRevision };
		}

		auto excluded(const cycle_manifest& aManifest, const std::string_view aPath) -> const manifest_exclusion*
		{
			const auto found = std::ranges::lower_bound(aManifest.Exclusions, aPath, { }, [](const manifest_exclusion& aExclusion) { return std::string_view{ aExclusion.Path }; });
			return found != aManifest.Exclusions.end() && found->Path == aPath ? &*found : nullptr;
		}

		/// An excluded or unlisted directory at or above the path hides it.
		auto hidden(const cycle_manifest& aManifest, const std::string_view aPath) -> bool
		{
			for (auto end = aPath.find('/');; end = aPath.find('/', end + 1)) {
				const auto  ancestor  = aPath.substr(0, end);
				const auto  isSelf    = end == std::string_view::npos;
				const auto* exclusion = excluded(aManifest, ancestor);
				if (exclusion != nullptr && (isSelf || exclusion->Directory)) {
					return true;
				}
				if (!isSelf) {
					const auto* entry = find_entry(aManifest, ancestor);
					if (entry != nullptr && entry->Availability != manifest_availability::captured
						&& (entry->Kind == manifest_entry_kind::directory || entry->Kind == manifest_entry_kind::gitlink)) {
						return true;
					}
				}
				if (isSelf) {
					return false;
				}
			}
		}

		auto side(const cycle_manifest& aManifest, const std::string_view aPath) -> cycle_path_side
		{
			const auto scope = cycle_scope_specification{ .Version = aManifest.ScopeVersion, .Paths = aManifest.Scope };
			auto       view  = cycle_path_side{ .State = classify_path(scope, aManifest.Coverage, aPath, observe_path(aManifest, aPath)) };
			if (const auto* entry = find_entry(aManifest, aPath)) {
				view.Kind    = entry->Kind;
				view.Mode    = entry->Mode;
				view.Content = entry->Content;
				view.Size    = entry->Size;
				view.Text    = entry->Profile.Kind;
			}
			return view;
		}

		/// One sorted list of paths, read from a position.
		class path_cursor
		{
		public:
			template <typename TRange, typename TProjection>
			path_cursor(const TRange& aRange, TProjection aProjection, const std::optional<std::string>& aAfter)
			{
				Paths.reserve(aRange.size());
				for (const auto& item : aRange) {
					const auto& path = aProjection(item);
					if (!aAfter || *aAfter < path) {
						Paths.emplace_back(path);
					}
				}
			}
			[[nodiscard]] auto done() const noexcept -> bool
			{
				return Position == Paths.size();
			}
			[[nodiscard]] auto current() const noexcept -> std::string_view
			{
				return Paths[Position];
			}
			void skip(const std::string_view aPath) noexcept
			{
				if (!done() && current() == aPath) {
					++Position;
				}
			}

		private:
			std::vector<std::string_view> Paths;
			std::size_t                   Position = 0;
		};

		void write_u64(std::vector<std::uint8_t>& aOut, const std::uint64_t aValue)
		{
			for (auto shift = 56; shift >= 0; shift -= 8) {
				aOut.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
			}
		}

		void write_digest(std::vector<std::uint8_t>& aOut, const std::optional<cycle_material_digest>& aDigest)
		{
			aOut.push_back(aDigest ? 1U : 0U);
			if (aDigest) {
				aOut.insert(aOut.end(), aDigest->bytes().begin(), aDigest->bytes().end());
			}
		}
	}

	auto explicit_view(const cycle_comparison_selector& aSelector) noexcept -> bool
	{
		if (const auto* round = std::get_if<this_round_view>(&aSelector)) {
			return round->Result.has_value();
		}
		return std::holds_alternative<full_source_view>(aSelector) || std::holds_alternative<revision_pair_view>(aSelector);
	}

	auto resolve_comparison(const review_cycle_state& aCycle, const cycle_comparison_selector& aSelector)
		-> std::expected<cycle_comparison_pair, cycle_comparison_error>
	{
		const auto latest = [&]() -> std::expected<cycle_source_revision_ref, cycle_comparison_error> {
			if (!aCycle.Latest) {
				return std::unexpected(cycle_comparison_error::uncaptured);
			}
			return aCycle.Latest->Id;
		};
		const auto named = [&](const cycle_source_revision_id aRevision) -> std::expected<cycle_source_revision_ref, cycle_comparison_error> {
			if (!allocated(aCycle, aRevision)) {
				return std::unexpected(cycle_comparison_error::unknown_revision);
			}
			return reference(aCycle, aRevision);
		};
		const auto pair = [](const std::optional<cycle_source_revision_ref>& aLeft, const std::expected<cycle_source_revision_ref, cycle_comparison_error>& aRight)
			-> std::expected<cycle_comparison_pair, cycle_comparison_error> {
			if (!aRight) {
				return std::unexpected(aRight.error());
			}
			return cycle_comparison_pair{ .Left = aLeft, .Right = *aRight };
		};
		if (const auto* full = std::get_if<full_source_view>(&aSelector)) {
			return pair(std::nullopt, named(full->Revision));
		}
		if (const auto* exact = std::get_if<revision_pair_view>(&aSelector)) {
			const auto left = named(exact->Left);
			return left ? pair(*left, named(exact->Right)) : std::unexpected(left.error());
		}
		if (std::holds_alternative<all_cycle_view>(aSelector)) {
			return aCycle.Initial ? pair(aCycle.Initial->Id, latest()) : std::unexpected(cycle_comparison_error::uncaptured);
		}
		if (const auto* since = std::get_if<since_checkpoint_view>(&aSelector)) {
			const auto left = named(since->Checkpoint);
			return left ? pair(*left, latest()) : std::unexpected(left.error());
		}
		const auto& round = std::get<this_round_view>(aSelector);
		const auto  start = named(round.Start);
		if (!start) {
			return std::unexpected(start.error());
		}
		return pair(*start, round.Result ? named(*round.Result) : latest());
	}

	auto open_comparison(const review_cycle_state& aCycle, const cycle_comparison_selector& aSelector)
		-> std::expected<cycle_comparison_view, cycle_comparison_error>
	{
		const auto resolved = resolve_comparison(aCycle, aSelector);
		if (!resolved) {
			return std::unexpected(resolved.error());
		}
		return cycle_comparison_view{ .Selector = aSelector, .Pair = *resolved };
	}

	auto newer_available(const cycle_comparison_view& aView, const review_cycle_state& aCycle) -> bool
	{
		if (!aCycle.Latest) {
			return false;
		}
		if (explicit_view(aView.Selector)) {
			return aCycle.Latest->Id.Id > aView.Pair.Right.Id;
		}
		const auto resolved = resolve_comparison(aCycle, aView.Selector);
		return resolved && *resolved != aView.Pair;
	}

	auto refresh_comparison(const cycle_comparison_view& aView, const review_cycle_state& aCycle) -> cycle_comparison_view
	{
		if (explicit_view(aView.Selector)) {
			return aView;
		}
		const auto resolved = resolve_comparison(aCycle, aView.Selector);
		return resolved ? cycle_comparison_view{ .Selector = aView.Selector, .Pair = *resolved } : aView;
	}

	auto observe_path(const cycle_manifest& aManifest, const std::string_view aPath) -> cycle_path_observation
	{
		auto observation = cycle_path_observation{ };
		if (const auto* entry = find_entry(aManifest, aPath)) {
			observation.Listed      = true;
			observation.Unavailable = entry->Availability != manifest_availability::captured;
			return observation;
		}
		observation.ExplicitlyDeleted = std::ranges::binary_search(aManifest.Deletions, aPath, { }, [](const std::string& aDeleted) { return std::string_view{ aDeleted }; });
		observation.Unavailable       = !observation.ExplicitlyDeleted && hidden(aManifest, aPath);
		return observation;
	}

	auto compare_manifest_path(const cycle_manifest* aLeft, const cycle_manifest& aRight, const std::string_view aPath) -> cycle_path_comparison
	{
		auto item = cycle_path_comparison{ .Path = std::string{ aPath }, .Right = side(aRight, aPath) };
		if (aLeft != nullptr) {
			item.Left        = side(*aLeft, aPath);
			item.Change      = compare_path(item.Left->State, item.Left->Content, item.Right.State, item.Right.Content);
			const auto both  = item.Left->State == cycle_path_state::present && item.Right.State == cycle_path_state::present;
			item.KindChanged = both && item.Left->Kind != item.Right.Kind;
			item.ModeChanged = both && !item.KindChanged && item.Left->Mode != item.Right.Mode;
		}
		return item;
	}

	auto compare_manifests(const cycle_manifest* aLeft, const cycle_manifest& aRight, const std::optional<std::string>& aAfter, const std::size_t aLimit,
						   const bool aChangedOnly) -> cycle_comparison_page
	{
		const auto limit   = std::clamp<std::size_t>(aLimit, 1U, maximum_cycle_comparison_page);
		const auto path    = [](const manifest_entry& aEntry) -> const std::string& { return aEntry.Path; };
		auto       cursors = std::vector<path_cursor>{ };
		cursors.emplace_back(aRight.Entries, path, aAfter);
		cursors.emplace_back(aRight.Deletions, std::identity{ }, aAfter);
		if (aLeft != nullptr) {
			cursors.emplace_back(aLeft->Entries, path, aAfter);
			cursors.emplace_back(aLeft->Deletions, std::identity{ }, aAfter);
		}
		const auto nextPath = [&cursors]() -> std::optional<std::string_view> {
			auto lowest = std::optional<std::string_view>{ };
			for (const auto& cursor : cursors) {
				if (!cursor.done() && (!lowest || cursor.current() < *lowest)) {
					lowest = cursor.current();
				}
			}
			return lowest;
		};
		auto page = cycle_comparison_page{ };
		while (const auto current = nextPath()) {
			if (page.Entries.size() == limit) {
				page.Next = page.Entries.back().Path;
				break;
			}
			const auto key = std::string{ *current };
			for (auto& cursor : cursors) {
				cursor.skip(key);
			}
			auto item = compare_manifest_path(aLeft, aRight, key);
			if (!aChangedOnly || aLeft == nullptr || item.changed()) {
				page.Entries.push_back(std::move(item));
			}
		}
		return page;
	}

	auto cycle_file_comparison(const local_cycle_subject& aSubject, const cycle_comparison_pair& aPair, const std::string_view aPath,
							   const std::optional<cycle_material_digest>& aLeft, const std::optional<cycle_material_digest>& aRight,
							   const diff_options& aOptions) -> std::optional<material_comparison_key>
	{
		const auto options = options_digest(aOptions);
		const auto left    = aPair.Left ? aLeft : std::nullopt;
		if ((!left && !aRight) || !options || !valid(aSubject)) {
			return std::nullopt;
		}
		constexpr auto domain = std::string_view{ "BPR-cycle-file-comparison-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0);
		bytes.insert(bytes.end(), aSubject.Authority.Incarnation.bytes().begin(), aSubject.Authority.Incarnation.bytes().end());
		write_u64(bytes, aSubject.Authority.Profile.value());
		write_u64(bytes, aSubject.Cycle.value());
		write_u64(bytes, aSubject.Generation);
		write_u64(bytes, aPair.Left ? aPair.Left->Id.value() : 0U);
		write_u64(bytes, aPair.Right.Id.value());
		write_u64(bytes, aPath.size());
		bytes.insert(bytes.end(), aPath.begin(), aPath.end());
		write_digest(bytes, left);
		write_digest(bytes, aRight);
		bytes.insert(bytes.end(), options->bytes().begin(), options->bytes().end());
		write_u64(bytes, supported_diff_algorithm_revision.value());
		return material_comparison_key{
			.Comparison   = file_comparison_digest{ sha256(bytes) },
			.LeftPresent  = left.has_value(),
			.RightPresent = aRight.has_value(),
			.Options      = *options,
			.Algorithm    = supported_diff_algorithm_revision,
		};
	}

	auto request_file(const cycle_file_view_state& aState, const cycle_file_view_key& aKey) -> std::pair<cycle_file_view_state, std::uint64_t>
	{
		auto       next = aState;
		const auto id   = next.NextRequest++;
		next.Pending    = std::pair{ id, aKey };
		return { std::move(next), id };
	}

	auto complete_file(const cycle_file_view_state& aState, const std::uint64_t aRequest, const bool aAvailable)
		-> std::pair<cycle_file_view_state, cycle_file_view_result>
	{
		if (!aState.Pending || aState.Pending->first != aRequest) {
			return { aState, cycle_file_view_result::stale };
		}
		auto next = aState;
		if (aAvailable) {
			next.Shown = std::move(next.Pending->second);
		}
		next.Pending.reset();
		return { std::move(next), aAvailable ? cycle_file_view_result::shown : cycle_file_view_result::kept };
	}

	auto cancel_file(const cycle_file_view_state& aState) -> cycle_file_view_state
	{
		auto next = aState;
		next.Pending.reset();
		return next;
	}
}
