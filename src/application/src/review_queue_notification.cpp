#include "bpr/application/review_queue_notification.hpp"

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

	auto notification_for(
		const core::queue_item& aItem, const std::uint32_t aUnreadComments)
		-> std::optional<queue_notification_request>
	{
		// [[BPR-1683]]. The reviewer said this one is not asking for
		// their attention; interrupting them about it would be the opposite.
		if (aItem.Ignored || !warrants_notification(aItem.Bucket)) {
			return std::nullopt;
		}
		return queue_notification_request{
			.Ref            = aItem.Ref,
			.Bucket         = aItem.Bucket,
			.UnreadComments = aUnreadComments,
		};
	}
} // namespace bpr::application
