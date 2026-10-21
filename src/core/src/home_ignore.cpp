#include "bpr/core/home_ignore.hpp"

#include <algorithm>

namespace bpr::core {
	auto name(const home_ignore_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case home_ignore_change::ignored:
			return "ignored";
		case home_ignore_change::unignored:
			return "unignored";
		case home_ignore_change::pruned_closed:
			return "pruned, closed";
		case home_ignore_change::pruned_left:
			break;
		}
		return "pruned, left Home";
	}

	auto ignorable(const pull_request_ref& aRef) noexcept -> bool
	{
		return !aRef.Host.empty() && !aRef.Account.empty() && !aRef.Repository.empty() && aRef.Number > 0;
	}

	home_ignore_list::home_ignore_list(const std::span<const pull_request_ref> aRefs)
	{
		Refs.reserve(std::min(aRefs.size(), maximum_home_ignores));
		for (const auto& ref : aRefs) {
			if (ignorable(ref)) {
				Refs.push_back(ref);
			}
		}
		std::ranges::sort(Refs);
		const auto duplicates = std::ranges::unique(Refs);
		Refs.erase(duplicates.begin(), duplicates.end());
		if (Refs.size() > maximum_home_ignores) {
			Refs.resize(maximum_home_ignores);
		}
	}

	auto home_ignore_list::contains(const pull_request_ref& aRef) const -> bool
	{
		return std::ranges::binary_search(Refs, aRef);
	}

	auto home_ignore_list::refs() const noexcept -> std::span<const pull_request_ref>
	{
		return Refs;
	}

	auto home_ignore_list::size() const noexcept -> std::size_t
	{
		return Refs.size();
	}

	auto home_ignore_list::apply(const home_ignore_intent& aIntent, const home_tracking aTracking)
		-> std::optional<home_ignore_delta>
	{
		const auto found   = std::ranges::lower_bound(Refs, aIntent.Ref);
		const auto present = found != Refs.end() && *found == aIntent.Ref;
		if (aIntent.Action == home_ignore_action::unignore) {
			if (!present) {
				return std::nullopt;
			}
			Refs.erase(found);
			return home_ignore_delta{ .Ref = aIntent.Ref, .Change = home_ignore_change::unignored };
		}
		// Only a row on Home can be ignored: a closed one is already off it,
		// and an ignore with nothing to hide would only wait to be pruned.
		if (present || aTracking != home_tracking::open || !ignorable(aIntent.Ref) || Refs.size() >= maximum_home_ignores) {
			return std::nullopt;
		}
		Refs.insert(found, aIntent.Ref);
		return home_ignore_delta{ .Ref = aIntent.Ref, .Change = home_ignore_change::ignored };
	}

	auto home_ignore_list::observe(const std::span<const home_tracking_observation> aObservations)
		-> std::vector<home_ignore_delta>
	{
		auto deltas = std::vector<home_ignore_delta>{ };
		for (const auto& observation : aObservations) {
			if (observation.Tracking != home_tracking::open && contains(observation.Ref)) {
				deltas.push_back({ .Ref    = observation.Ref,
								   .Change = observation.Tracking == home_tracking::closed ? home_ignore_change::pruned_closed : home_ignore_change::pruned_left });
			}
		}
		// `pruned_closed` orders before `pruned_left`, so the first delta of a
		// pull request is the closed one whenever any observation said closed.
		std::ranges::sort(deltas);
		const auto repeated = std::ranges::unique(deltas, { }, &home_ignore_delta::Ref);
		deltas.erase(repeated.begin(), repeated.end());
		const auto pruned = std::ranges::remove_if(Refs, [&deltas](const pull_request_ref& aRef) {
			return std::ranges::binary_search(deltas, aRef, { }, &home_ignore_delta::Ref);
		});
		Refs.erase(pruned.begin(), pruned.end());
		return deltas;
	}
} // namespace bpr::core
