#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_NOTIFICATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_NOTIFICATION_HPP

#include "bpr/core/review_queue_attention.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief What BetterPullReview asks the operating system to show.
	///
	/// \details **There is no field here a comment body or a source line could
	/// occupy, and that absence is the guarantee.**
	///
	/// The operating system's notification store outlives the application, is
	/// readable by other software, and on some systems is synchronised to the
	/// user's other devices. A diff line or a quoted review comment placed
	/// there has left this product's control permanently, and no later change
	/// of mind retrieves it.
	///
	/// The alternative — carrying the text and redacting it on the way out — is
	/// worse in the way that matters. A redaction step is a place a mistake can
	/// live, and the mistake is silent: the notification looks right to whoever
	/// wrote the code and wrong only to the person whose repository content is
	/// now in a system database.
	///
	/// So the request names the review, says which bucket it is in, and counts
	/// what is waiting. Nothing it carries is repository content.
	struct queue_notification_request
	{
		core::pull_request_ref Ref;
		core::attention_bucket Bucket = core::attention_bucket::quiet;

		// How many unread comments, if the bucket is about comments. A count is
		// not content: it says there is something to read without saying what.
		std::uint32_t UnreadComments = 0U;

		auto operator<=>(const queue_notification_request&) const = default;
	};

	/// \brief The title a notification carries.
	///
	/// \details Repository and number. A pull request title would be the
	/// obvious thing to add and is exactly what must not be there: titles
	/// routinely name the customer, the incident, or the unreleased feature.
	[[nodiscard]] auto notification_title(
		const queue_notification_request& aRequest) -> std::string;

	/// \brief The body a notification carries.
	///
	/// \details The bucket's own sentence, and a count when there is one. Every
	/// word of it comes from a fixed table in the core; none of it is copied
	/// from the pull request.
	[[nodiscard]] auto notification_body(
		const queue_notification_request& aRequest) -> std::string;

	/// \brief Where following the notification goes.
	///
	/// \details A local reference, not a URL. A deep link that carried a
	/// `https://` address would open a browser, which is the one thing a native
	/// review tool exists to avoid; and a link the operating system can hand to
	/// any handler is one another application can claim.
	struct queue_notification_target
	{
		core::pull_request_ref Ref;

		auto operator<=>(const queue_notification_target&) const = default;
	};

	/// \brief The target a notification resolves to.
	[[nodiscard]] auto notification_target(
		const queue_notification_request& aRequest)
		-> queue_notification_target;

	/// \brief Whether a bucket is worth interrupting the reviewer for.
	///
	/// \details Not every bucket is. `quiet` asks nothing of anyone, and a tool
	/// that notified about it would teach the reviewer to ignore its
	/// notifications — after which the ones that matter are ignored too.
	[[nodiscard]] auto warrants_notification(
		core::attention_bucket aBucket) noexcept -> bool;

	/// \brief Builds the request for one queue item, if it warrants one.
	[[nodiscard]] auto notification_for(
		const core::queue_item& aItem,
		std::uint32_t           aUnreadComments)
		-> std::optional<queue_notification_request>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_NOTIFICATION_HPP
