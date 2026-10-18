#include "github_insights.hpp"
#include "github_paged_read.hpp"
#include <chrono>
namespace bpr::adapters::github::detail {
	github_insights_channel::github_insights_channel(std::shared_ptr<application::insights_observer> aObserver, const std::uint64_t aHost)
		: Observer{ std::move(aObserver) }
		, Scope{ aHost, 0 }
	{ }
	void github_insights_channel::bind(const std::uint64_t aAccount) noexcept
	{
		try {
			std::lock_guard lock{ Mutex };
			Scope.Account = aAccount;
			Bound         = true;
			for (auto& event : Pending) {
				event.Scope = Scope;
				Observer->record(std::move(event));
			}
			Pending.clear();
			if (Scope.Host != 0) {
				Observer->select(Scope);
			}
		} catch (...) { }
	}
	void github_insights_channel::observe(core::insights_event aEvent) noexcept
	{
		try {
			aEvent.UnixSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			std::lock_guard lock{ Mutex };
			aEvent.Scope = Scope;
			if (Bound) {
				Observer->record(std::move(aEvent));
			} else if (Pending.size() < 128) {
				Pending.push_back(std::move(aEvent));
			}
		} catch (...) { }
	}
	void github_insights_channel::request(const core::insights_resource aResource, const core::insights_category aCategory, const wire_response* aResponse, const std::uint64_t aCount) noexcept
	{
		core::insights_event event;
		event.Count    = aCount;
		event.Resource = aResource;
		event.Category = aCategory;
		if (aResponse) {
			const auto metadata = parse_github_response_metadata(*aResponse);
			if (metadata) {
				event.RetryAfterSeconds = metadata->RetryAfterSeconds;
				if (metadata->RateLimit) {
					const auto& reading    = *metadata->RateLimit;
					event.Limit            = reading.Limit;
					event.Remaining        = reading.Remaining;
					event.ResetUnixSeconds = static_cast<std::int64_t>(reading.ResetUnixSeconds);
					event.SecondaryLimited = reading.SecondaryLimited;
				}
				event.SecondaryLimited = event.SecondaryLimited || ((aResponse->Status == 403 || aResponse->Status == 429) && github_error_body_is_secondary_rate_limit(aResponse->Body, 65536));
			}
		}
		observe(std::move(event));
	}
	auto insights_category_for(const read_operation aOperation) noexcept -> core::insights_category
	{
		switch (aOperation) {
		case read_operation::search_personal_pull_requests:
		case read_operation::search_pull_requests:          return core::insights_category::inbox;
		case read_operation::get_repository:
		case read_operation::get_pull_request:
		case read_operation::list_pull_request_files:
		case read_operation::list_pull_request_commits:
		case read_operation::get_merge_base:
		case read_operation::list_checks:
		case read_operation::list_commit_statuses:
		case read_operation::get_merge_assessment:          return core::insights_category::opening;
		case read_operation::list_reviews:
		case read_operation::list_review_threads:
		case read_operation::list_issue_comments:
		case read_operation::list_review_comments:
		case read_operation::list_pull_request_timeline:
		case read_operation::resolve_body_images:           return core::insights_category::conversations;
		default:                                            return core::insights_category::other;
		}
	}
	auto insights_resource_for(const read_operation aOperation) noexcept -> core::insights_resource
	{
		return graphql_read(aOperation) ? core::insights_resource::graphql
										: (aOperation == read_operation::search_pull_requests || aOperation == read_operation::search_personal_pull_requests ? core::insights_resource::search : core::insights_resource::core);
	}
}
