#include "bpr/core/off_hunk_fact.hpp"

namespace bpr::core {
	auto valid_github_server_version(const github_server_product aProduct, const std::string_view aVersion) noexcept -> bool
	{
		if (aProduct == github_server_product::github_com) {
			return aVersion.empty();
		}
		if (aProduct != github_server_product::github_enterprise_server || aVersion.empty() || aVersion.size() > maximum_github_server_version_bytes) {
			return false;
		}
		auto parts  = std::size_t{ 1U };
		auto digits = std::size_t{ 0U };
		for (const auto character : aVersion) {
			if (character == '.') {
				if (digits == 0U) {
					return false;
				}
				++parts;
				digits = 0U;
			} else if (character >= '0' && character <= '9' && digits < 5U) {
				++digits;
			} else {
				return false;
			}
		}
		return digits != 0U && parts >= 2U && parts <= 4U;
	}

	auto valid(const off_hunk_fact_observation& aObservation) noexcept -> bool
	{
		return aObservation.ObservedUtcMs >= 0
			&& ((aObservation.State == off_hunk_creation_capability::supported && aObservation.Source == off_hunk_fact_source::observed_acceptance)
				|| (aObservation.State == off_hunk_creation_capability::unsupported && aObservation.Source == off_hunk_fact_source::observed_refusal));
	}

	auto current_off_hunk_fact(const github_server_product aProduct, const std::optional<off_hunk_fact_observation>& aObservation, const std::int64_t aNowUtcMs) noexcept
		-> off_hunk_creation_capability
	{
		if (!aObservation || !valid(*aObservation) || aNowUtcMs < aObservation->ObservedUtcMs) {
			return off_hunk_creation_capability::unknown;
		}
		const auto lifetime = aProduct == github_server_product::github_com ? off_hunk_fact_lifetime_github_com_ms
			: aProduct == github_server_product::github_enterprise_server   ? off_hunk_fact_lifetime_enterprise_ms
																			: std::int64_t{ 0 };
		return aNowUtcMs - aObservation->ObservedUtcMs < lifetime ? aObservation->State : off_hunk_creation_capability::unknown;
	}

	auto newer_off_hunk_fact(const std::optional<off_hunk_fact_observation>& aLeft, const std::optional<off_hunk_fact_observation>& aRight) noexcept
		-> std::optional<off_hunk_fact_observation>
	{
		const auto left  = aLeft && valid(*aLeft) ? aLeft : std::nullopt;
		const auto right = aRight && valid(*aRight) ? aRight : std::nullopt;
		if (!left || !right) {
			return left ? left : right;
		}
		if (left->ObservedUtcMs != right->ObservedUtcMs) {
			return left->ObservedUtcMs > right->ObservedUtcMs ? left : right;
		}
		return left->State == off_hunk_creation_capability::unsupported ? left : right;
	}

	auto learn_off_hunk_fact(const off_hunk_answer aAnswer, const std::int64_t aNowUtcMs) noexcept -> std::optional<off_hunk_fact_observation>
	{
		if (aNowUtcMs < 0) {
			return std::nullopt;
		}
		switch (aAnswer) {
		case off_hunk_answer::created:
			return off_hunk_fact_observation{ .State = off_hunk_creation_capability::supported, .Source = off_hunk_fact_source::observed_acceptance, .ObservedUtcMs = aNowUtcMs };
		case off_hunk_answer::refused_naming_line:
			return off_hunk_fact_observation{ .State = off_hunk_creation_capability::unsupported, .Source = off_hunk_fact_source::observed_refusal, .ObservedUtcMs = aNowUtcMs };
		case off_hunk_answer::unclear:
			break;
		}
		return std::nullopt;
	}

	auto route_github_comment(const github_diff_placement aPlacement, const off_hunk_creation_capability aFact) noexcept -> github_comment_route
	{
		switch (aPlacement) {
		case github_diff_placement::unknown:
		case github_diff_placement::in_diff:
			return github_comment_route::native;
		case github_diff_placement::outside_diff:
			return aFact == off_hunk_creation_capability::supported  ? github_comment_route::native_off_hunk
				: aFact == off_hunk_creation_capability::unsupported ? github_comment_route::off_hunk_file
																	 : github_comment_route::off_hunk_unknown;
		case github_diff_placement::range_outside_diff:
			return aFact == off_hunk_creation_capability::supported ? github_comment_route::native_off_hunk : github_comment_route::refused;
		}
		return github_comment_route::refused;
	}

	auto holds_review_comment(const github_diff_placement aPlacement, const off_hunk_creation_capability aFact) noexcept -> bool
	{
		const auto route = route_github_comment(aPlacement, aFact);
		return route != github_comment_route::native && route != github_comment_route::native_off_hunk;
	}
}
