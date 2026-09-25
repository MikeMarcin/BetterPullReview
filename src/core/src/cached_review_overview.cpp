#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_model.hpp"
#include <algorithm>

namespace bpr::core {
	namespace {
		auto text_valid(std::string_view aText, std::size_t aLimit) noexcept -> bool
		{
			return aText.size() <= aLimit && aText.find('\0') == std::string_view::npos;
		}
	}
	auto valid_cached_catalogue(const std::vector<cached_review_file>& aFiles) noexcept -> bool
	{
		if (aFiles.size() > maximum_cached_files) {
			return false;
		}
		std::array<std::string_view, maximum_cached_files> paths;
		std::size_t                                        bytes = 0, index = 0;
		for (const auto& file : aFiles) {
			if (file.Path.empty() || !text_valid(file.Path, 4096)
				|| (file.PreviousPath && (file.PreviousPath->empty() || !text_valid(*file.PreviousPath, 4096)))
				|| file.Change > cached_file_change::unchanged) {
				return false;
			}
			// Includes all length/presence/count fields in the durable encoding.
			bytes += 48 + file.Path.size() + (file.PreviousPath ? file.PreviousPath->size() : 0);
			if (bytes > maximum_cached_catalogue_bytes) {
				return false;
			}
			paths[index++] = file.Path;
		}
		auto end = paths.begin() + static_cast<std::ptrdiff_t>(aFiles.size());
		std::sort(paths.begin(), end);
		return std::adjacent_find(paths.begin(), end) == end;
	}
	auto valid(const cached_review_key& aKey) noexcept -> bool
	{
		return std::ranges::any_of(aKey.Authority, [](auto aByte) { return aByte != 0; })
			&& aKey.Profile.value() && aKey.Host.value() && aKey.Account.value()
			&& valid(aKey.Locator) && aKey.Number.valid();
	}
	auto valid(const cached_review_overview& aOverview) noexcept -> bool
	{
		return valid(aOverview.Key) && aOverview.Repository.value() && aOverview.PullRequest.value()
			&& parse_git_commit_oid(aOverview.BaseOid).has_value() && parse_git_commit_oid(aOverview.HeadOid).has_value()
			&& text_valid(aOverview.Title, 1024) && text_valid(aOverview.Body, 256 * 1024)
			&& text_valid(aOverview.AuthorLogin, 256) && text_valid(aOverview.BaseRef, 256)
			&& text_valid(aOverview.HeadRef, 256) && aOverview.State <= 2
			&& aOverview.Files <= 10000 && aOverview.Commits <= 1000000
			&& (!aOverview.FileCatalogue || (aOverview.FileCatalogue->size() == aOverview.Files && valid_cached_catalogue(*aOverview.FileCatalogue)));
	}
	auto admit_cached_overview(const github_state&                 aState,
							   const pull_request_request_context& aRequest, github_failure aFailure,
							   const cached_review_key& aExpected, const cached_review_overview& aOverview) noexcept -> bool
	{
		return aFailure == github_failure::network && aState.PendingPullRequest == aRequest
			&& !aState.AcceptedSnapshot && !aState.PendingSession
			&& aState.AccessMode == github_access_mode::authenticated
			&& aRequest.Mode == github_access_mode::authenticated
			&& aState.ApplicationSession == aRequest.ApplicationSession
			&& aState.IdentityGeneration == aRequest.IdentityGeneration
			&& aState.ActiveProfile == aExpected.Profile && aState.Host == aExpected.Host
			&& aState.Account == aExpected.Account && aRequest.Profile == aExpected.Profile
			&& aRequest.Host == aExpected.Host && aRequest.Account == aExpected.Account
			&& aRequest.Repository == aExpected.Locator && aRequest.Number == aExpected.Number
			&& aOverview.Key == aExpected && valid(aOverview)
			&& (!aRequest.ExpectedRepository || *aRequest.ExpectedRepository == aOverview.Repository)
			&& (!aRequest.ExpectedPullRequest || *aRequest.ExpectedPullRequest == aOverview.PullRequest);
	}
}
