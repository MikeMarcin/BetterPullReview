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
		auto cached_reader_matches(const github_state& aState, const pull_request_request_context& aRequest,
								   const cached_review_key& aExpected, const cached_review_overview& aOverview) noexcept -> bool
		{
			return !aState.AcceptedSnapshot && !aState.PendingSession
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
	auto valid_cached_threads(const std::vector<cached_review_thread>& aThreads) noexcept -> bool
	{
		if (aThreads.size() > maximum_cached_threads) {
			return false;
		}
		std::array<std::string_view, maximum_cached_threads + maximum_cached_thread_comments> ids;
		std::size_t                                                                           count = 0, comments = 0, bytes = 24;
		const auto                                                                            lineValid = [](auto aLine) { return !aLine || (*aLine > 0 && *aLine <= 2147483647U); };
		for (const auto& thread : aThreads) {
			if (thread.Id.empty() || !text_valid(thread.Id, 256) || thread.Path.empty() || !text_valid(thread.Path, 4096)
				|| thread.Side > cached_thread_side::right || !lineValid(thread.Line) || !lineValid(thread.OriginalLine)
				|| (thread.FileLevel && (thread.Line || thread.OriginalLine))
				|| thread.Comments.size() > maximum_cached_thread_comments - comments) {
				return false;
			}
			comments += thread.Comments.size();
			bytes += 88 + thread.Id.size() + thread.Path.size();
			ids[count++]            = thread.Id;
			const auto firstComment = count;
			for (const auto& comment : thread.Comments) {
				const auto end = ids.begin() + static_cast<std::ptrdiff_t>(count);
				if (comment.Id.empty() || !text_valid(comment.Id, 256) || !text_valid(comment.AuthorLogin, 256)
					|| !text_valid(comment.Body, 64 * 1024) || !lineValid(comment.Line) || !lineValid(comment.OriginalLine)
					|| (thread.FileLevel && (comment.Line || comment.OriginalLine))
					|| (comment.ReplyTo && std::find(ids.begin() + static_cast<std::ptrdiff_t>(firstComment), end, *comment.ReplyTo) == end)) {
					return false;
				}
				bytes += 96 + comment.Id.size() + comment.AuthorLogin.size() + comment.Body.size() + (comment.ReplyTo ? comment.ReplyTo->size() : 0);
				if (bytes > maximum_cached_threads_bytes) {
					return false;
				}
				ids[count++] = comment.Id;
			}
			if (bytes > maximum_cached_threads_bytes) {
				return false;
			}
		}
		const auto end = ids.begin() + static_cast<std::ptrdiff_t>(count);
		std::sort(ids.begin(), end);
		return std::adjacent_find(ids.begin(), end) == end;
	}
	auto valid_cached_discussion(const std::vector<cached_discussion_message>& aMessages, bool aReviews) noexcept -> bool
	{
		if (aMessages.size() > maximum_cached_discussion_messages) {
			return false;
		}
		std::array<std::string_view, maximum_cached_discussion_messages> ids;
		std::size_t                                                      bytes = 24, count = 0;
		for (const auto& message : aMessages) {
			if (message.Id.empty() || !text_valid(message.Id, 256) || !text_valid(message.AuthorLogin, 256)
				|| !text_valid(message.Body, 64 * 1024)
				|| (aReviews ? message.Kind < cached_discussion_kind::pending_review || message.Kind > cached_discussion_kind::dismissed_review
							 : message.Kind != cached_discussion_kind::general_comment || message.CommitOid.has_value())
				|| (message.CommitOid && !parse_git_commit_oid(*message.CommitOid))) {
				return false;
			}
			bytes += 56 + message.Id.size() + message.AuthorLogin.size() + message.Body.size() + (message.CommitOid ? message.CommitOid->size() : 0);
			if (bytes > maximum_cached_discussion_bytes) {
				return false;
			}
			ids[count++] = message.Id;
		}
		const auto end = ids.begin() + static_cast<std::ptrdiff_t>(count);
		std::sort(ids.begin(), end);
		return std::adjacent_find(ids.begin(), end) == end;
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
			&& (!aOverview.GeneralComments || valid_cached_discussion(*aOverview.GeneralComments, false))
			&& (!aOverview.ReviewSummaries || valid_cached_discussion(*aOverview.ReviewSummaries, true))
			&& (!aOverview.ReviewThreads || valid_cached_threads(*aOverview.ReviewThreads))
			&& (!aOverview.FileCatalogue || (aOverview.FileCatalogue->size() == aOverview.Files && valid_cached_catalogue(*aOverview.FileCatalogue)));
	}
	auto admit_cached_source(const github_state& aState, const pull_request_request_context& aRequest,
							 const cached_review_key& aExpected, const cached_review_overview& aOverview, std::string_view aPath) noexcept -> bool
	{
		if (aState.PendingPullRequest || aState.PullRequestGeneration != aRequest.Generation
			|| aState.ActiveLocator != aRequest.Repository || aState.ActiveNumber != aRequest.Number
			|| !aOverview.FileCatalogue || aPath.empty() || aPath.starts_with('/') || aPath.find('\\') != std::string_view::npos) {
			return false;
		}
		return cached_reader_matches(aState, aRequest, aExpected, aOverview)
			&& std::ranges::any_of(*aOverview.FileCatalogue, [aPath](const auto& aFile) { return aFile.Path == aPath; });
	}
	auto admit_cached_overview(const github_state& aState, const pull_request_request_context& aRequest, github_failure aFailure,
							   const cached_review_key& aExpected, const cached_review_overview& aOverview) noexcept -> bool
	{
		return aFailure == github_failure::network && aState.PendingPullRequest == aRequest
			&& cached_reader_matches(aState, aRequest, aExpected, aOverview);
	}
}
