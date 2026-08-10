#include "bpr/application/notification_delivery.hpp"

#include <utility>

namespace bpr::application {
	review_queue_notifier::review_queue_notifier(
		notification_sink_port&      aSink,
		notification_activation_sink aActivations)
		: Sink{ aSink }
		, Activations{ std::move(aActivations) }
	{
	}

	auto review_queue_notifier::deliver(
		const queue_notification_request& aRequest) -> bool
	{
		// The bucket decides first, and it decides for the reviewer's sake: a
		// tool that notified about a quiet queue would teach them to ignore its
		// notifications, after which the ones that matter are ignored too.
		if (!warrants_notification(aRequest.Bucket)) {
			++Statistics.SuppressedByBucket;
			return false;
		}

		auto allowed = Sink.permission();
		if (allowed == notification_permission::not_determined) {
			++Statistics.PermissionRequests;
			allowed = Sink.request_permission();
		}
		if (allowed != notification_permission::granted) {
			// A refusal is a normal outcome, not an error to retry. Nothing is
			// delivered, nothing is reported, and the reviewer is not asked
			// again — the state stays whatever the platform says it is, so a
			// later call reaches `permission()` and finds `refused` still.
			++Statistics.SuppressedByRefusal;
			return false;
		}

		// Composed from the request alone. Every word comes from a fixed table
		// the core owns; nothing here can reach the pull request, the ledger,
		// or the diff, because this class holds none of them.
		const auto submission = Sink.deliver(delivered_notification{
			.Title  = notification_title(aRequest),
			.Body   = notification_body(aRequest),
			.Target = notification_target(aRequest),
		});
		if (submission
			== notification_submission_result::suppressed_unattributable_activation) {
			++Statistics.SuppressedUnattributableActivation;
			return false;
		}
		++Statistics.Submitted;
		return true;
	}

	void review_queue_notifier::activate(
		const queue_notification_target& aTarget)
	{
		++Statistics.Activations;
		if (Activations) {
			Activations(aTarget);
		}
	}

	auto review_queue_notifier::statistics() const noexcept
		-> notification_delivery_statistics
	{
		return Statistics;
	}
} // namespace bpr::application
