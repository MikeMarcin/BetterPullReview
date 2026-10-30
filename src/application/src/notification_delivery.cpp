#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <utility>

namespace bpr::application {
	review_queue_notifier::review_queue_notifier(
		notification_sink_port&              aSink,
		notification_activation_sink         aActivations,
		std::shared_ptr<diagnostic_log_port> aLog)
		: Sink{ aSink }
		, Activations{ std::move(aActivations) }
		, Log{ std::move(aLog) }
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
			const auto asked = operation_stopwatch{ };
			allowed          = Sink.request_permission();
			// [[BPR-1831]]. The reviewer's answer, as a grant or a refusal.
			record_operation(Log.get(), { .Action = core::operation_action::notification_permission, .Failure = allowed == notification_permission::granted || allowed == notification_permission::platform_managed ? core::operation_failure::none : core::operation_failure::permission_refused, .DurationMilliseconds = asked.milliseconds() });
		}
		if (allowed != notification_permission::granted
			&& allowed != notification_permission::platform_managed) {
			// A refusal is a normal outcome, not an error to retry. Nothing is
			// delivered, nothing is reported, and the reviewer is not asked
			// again — the state stays whatever the platform says it is, so a
			// later call reaches `permission()` and finds `refused` still.
			++Statistics.SuppressedByRefusal;
			record_operation(Log.get(), { .Action = core::operation_action::notification_deliver, .Failure = core::operation_failure::permission_refused });
			return false;
		}

		// Composed from the request alone. Every word comes from a fixed table
		// the core owns; nothing here can reach the pull request, the ledger,
		// or the diff, because this class holds none of them.
		const auto handed     = operation_stopwatch{ };
		const auto submission = Sink.deliver(delivered_notification{
			.Title  = notification_title(aRequest),
			.Body   = notification_body(aRequest),
			.Target = notification_target(aRequest),
		});
		if (submission
			== notification_submission_result::suppressed_unattributable_activation) {
			++Statistics.SuppressedUnattributableActivation;
			record_operation(Log.get(), { .Action = core::operation_action::notification_deliver, .Failure = core::operation_failure::unattributable_activation, .DurationMilliseconds = handed.milliseconds() });
			return false;
		}
		++Statistics.Submitted;
		record_operation(Log.get(), { .Action = core::operation_action::notification_deliver, .DurationMilliseconds = handed.milliseconds() });
		return true;
	}

	void review_queue_notifier::activate(
		const queue_notification_target& aTarget)
	{
		++Statistics.Activations;
		record_operation(Log.get(), { .Action = core::operation_action::notification_activate });
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
