#include "bpr/application/review_queue_notification.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	auto notification_title(const queue_notification_request& aRequest)
		-> std::string
	{
		// Repository and number, and deliberately not the pull request's title.
		// A title is the obvious thing to add and routinely names the customer,
		// the incident, or the unreleased feature.
		return aRequest.Ref.Repository + " #"
			+ std::to_string(aRequest.Ref.Number);
	}

	auto notification_body(const queue_notification_request& aRequest)
		-> std::string
	{
		auto body = std::string{ core::explanation(aRequest.Bucket) };
		if (aRequest.UnreadComments > 0U) {
			// A count says there is something to read without saying what.
			body += " " + std::to_string(aRequest.UnreadComments)
				+ (aRequest.UnreadComments == 1U ? " unread comment."
												 : " unread comments.");
		}
		return body;
	}

	auto notification_target(const queue_notification_request& aRequest)
		-> queue_notification_target
	{
		return queue_notification_target{ .Ref = aRequest.Ref };
	}

	auto warrants_notification(const core::attention_bucket aBucket) noexcept
		-> bool
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
		case core::attention_bucket::changed_since_my_review:
		case core::attention_bucket::new_comments_or_author_replies:
		case core::attention_bucket::fixes_awaiting_verification:
		case core::attention_bucket::observed_with_new_activity:
			return true;

		// Waiting on somebody else is not something to interrupt the reviewer
		// about: there is nothing for them to do, and a notification would be
		// asking them to look at their own patience.
		case core::attention_bucket::authored_awaiting_reviewer_action:
		// Nothing is asked of anyone. A tool that notified about this would
		// teach the reviewer to ignore its notifications, after which the ones
		// that matter are ignored too.
		case core::attention_bucket::quiet:
		// [[BPR-0989]]. Not knowing is not news.
		case core::attention_bucket::detail_unknown:
			return false;
		}
		return false;
	}

	auto notification_active(const core::queue_item& aItem, const core::home_draft_placement aDrafts) noexcept -> bool
	{
		// [[BPR-1683]]. The reviewer said this one is not asking for
		// their attention; interrupting them about it would be the opposite.
		// [[BPR-2330]]. Nor is a draft nobody has asked them to read yet.
		return !aItem.Ignored && warrants_notification(aItem.Bucket) && !core::draft_waits_quietly(aItem, aDrafts);
	}

	auto step_queue_notifications(const std::span<const queue_notification_key> aPrevious, const queue_session_id& aSession,
								  const std::span<const core::queue_item> aItems, const core::home_draft_placement aDrafts)
		-> queue_notification_step
	{
		auto step = queue_notification_step{ };
		step.Active.reserve(aItems.size());
		for (std::size_t index = 0U; index < aItems.size(); ++index) {
			const auto& item = aItems[index];
			if (!notification_active(item, aDrafts)) {
				continue;
			}
			auto key = queue_notification_key{ .Session = aSession, .Ref = item.Ref, .Bucket = item.Bucket };
			if (!std::ranges::binary_search(aPrevious, key) && !std::ranges::contains(step.Active, key)) {
				step.Due.push_back(index);
			}
			step.Active.push_back(std::move(key));
		}
		std::ranges::sort(step.Active);
		const auto unique = std::ranges::unique(step.Active);
		step.Active.erase(unique.begin(), unique.end());
		return step;
	}

	auto notification_for(
		const core::queue_item& aItem, const std::uint32_t aUnreadComments, const core::home_draft_placement aDrafts)
		-> std::optional<queue_notification_request>
	{
		if (!notification_active(aItem, aDrafts)) {
			return std::nullopt;
		}
		return queue_notification_request{
			.Ref            = aItem.Ref,
			.Bucket         = aItem.Bucket,
			.UnreadComments = aUnreadComments,
		};
	}
} // namespace bpr::application
