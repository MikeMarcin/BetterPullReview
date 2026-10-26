#include "bpr/core/home_discovery_plan.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto folded(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] auto same_text(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			return std::ranges::equal(aLeft, aRight, [](const char aOne, const char aOther) { return folded(aOne) == folded(aOther); });
		}

		[[nodiscard]] auto same_repository(const repository_locator& aLeft, const repository_locator& aRight) noexcept -> bool
		{
			return same_text(aLeft.Owner, aRight.Owner) && same_text(aLeft.Name, aRight.Name);
		}
	} // namespace

	auto discovery_known_pull_requests(const std::span<const discovery_retained_read> aReads) -> std::vector<discovery_retained_read>
	{
		auto known = std::vector<discovery_retained_read>{ };
		for (const auto& read : aReads) {
			if (read.Open && read.Complete && valid(read.Repository) && read.Number.value() != 0U) {
				known.push_back(read);
			}
		}
		const auto key = [](const discovery_retained_read& aRead) { return std::tie(aRead.Repository, aRead.Number); };
		std::ranges::stable_sort(known, [&key](const discovery_retained_read& aLeft, const discovery_retained_read& aRight) { return key(aLeft) < key(aRight); });
		const auto [first, last] = std::ranges::unique(known, [&key](const discovery_retained_read& aLeft, const discovery_retained_read& aRight) { return key(aLeft) == key(aRight); });
		known.erase(first, last);
		return known;
	}

	auto discovery_search_query(const discovery_search_reason aReason, const std::string_view aLogin, const std::span<const repository_locator> aRepositories) -> std::string
	{
		auto query = std::string{ "is:pr is:open " };
		query += aReason == discovery_search_reason::authored ? "author:" + std::string{ aLogin } : "user-review-requested:@me";
		for (const auto& repository : aRepositories) {
			query += " repo:";
			query += repository.Owner;
			query += '/';
			query += repository.Name;
		}
		return query;
	}

	auto plan_discovery_searches(const discovery_search_reason aReason, const std::string_view aLogin, const std::span<const repository_locator> aRepositories,
								 const std::span<const repository_locator> aAlone, const std::size_t aMaximumRepositories) -> std::vector<std::vector<repository_locator>>
	{
		auto       searches = std::vector<std::vector<repository_locator>>{ };
		auto       current  = std::vector<repository_locator>{ };
		const auto bound    = std::clamp<std::size_t>(aMaximumRepositories, 1U, maximum_discovery_search_repositories);
		const auto fits     = [&](const std::vector<repository_locator>& aSearch) {
			return aSearch.size() <= bound && discovery_search_query(aReason, aLogin, aSearch).size() <= maximum_discovery_search_query_bytes;
		};
		const auto flush = [&] {
			if (!current.empty()) {
				searches.push_back(std::move(current));
				current.clear();
			}
		};
		auto singles = std::vector<std::vector<repository_locator>>{ };
		for (const auto& repository : aRepositories) {
			if (std::ranges::find(aAlone, repository) != aAlone.end() || !fits({ repository })) {
				singles.push_back({ repository });
				continue;
			}
			// GitHub names a repository in any case, so a search never names
			// two that differ only in case: their rows could not be told apart.
			if (std::ranges::any_of(current, [&repository](const repository_locator& aOther) { return same_repository(aOther, repository); })) {
				flush();
			}
			current.push_back(repository);
			if (!fits(current)) {
				current.pop_back();
				flush();
				current.push_back(repository);
			}
		}
		flush();
		searches.insert(searches.end(), std::make_move_iterator(singles.begin()), std::make_move_iterator(singles.end()));
		return searches;
	}

	auto splits_discovery_search(const std::size_t aRepositories, const discovery_search_outcome aOutcome) noexcept -> bool
	{
		switch (aOutcome) {
		case discovery_search_outcome::refused:
		case discovery_search_outcome::result_limit:
		case discovery_search_outcome::detail_limit:       return aRepositories > 1U;
		case discovery_search_outcome::complete:
		case discovery_search_outcome::incomplete_results:
		case discovery_search_outcome::failed:             return false;
		}
		return false;
	}

	auto plan_retained_batches(const std::span<const discovery_retained_target> aTargets, const std::size_t aMaximum) -> std::vector<discovery_retained_batch>
	{
		const auto bound   = std::clamp<std::size_t>(aMaximum, 1U, maximum_discovery_batch_pull_requests);
		auto       grouped = std::vector<discovery_retained_batch>{ };
		for (const auto& target : aTargets) {
			auto group = std::ranges::find(grouped, target.Repository, &discovery_retained_batch::Repository);
			if (group == grouped.end()) {
				group = grouped.insert(grouped.end(), discovery_retained_batch{ .Repository = target.Repository, .Numbers = { } });
			}
			if (std::ranges::find(group->Numbers, target.Number) == group->Numbers.end()) {
				group->Numbers.push_back(target.Number);
			}
		}
		auto batches = std::vector<discovery_retained_batch>{ };
		for (auto& group : grouped) {
			for (std::size_t first = 0U; first < group.Numbers.size(); first += bound) {
				auto numbers = std::vector<pull_request_number>{ group.Numbers.begin() + static_cast<std::ptrdiff_t>(first),
																 group.Numbers.begin() + static_cast<std::ptrdiff_t>(std::min(first + bound, group.Numbers.size())) };
				std::ranges::sort(numbers);
				batches.push_back({ .Repository = group.Repository, .Numbers = std::move(numbers) });
			}
		}
		return batches;
	}

	auto keeps_reused_row(const discovery_search_reason aReason, const discovery_retained_read& aRead) noexcept -> bool
	{
		return aReason == discovery_search_reason::authored || aRead.RequestsViewer;
	}
} // namespace bpr::core
