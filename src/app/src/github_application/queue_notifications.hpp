#ifndef BPR_APP_GITHUB_APPLICATION_QUEUE_NOTIFICATIONS_HPP
#define BPR_APP_GITHUB_APPLICATION_QUEUE_NOTIFICATIONS_HPP
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <atomic>
#include <compare>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/notification_delivery.hpp"
#endif

// Review-queue observation, refresh and notification transitions the
// profile runtime composes.
namespace bpr::app {
	[[nodiscard]] auto queue_observation_for(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot)
		-> core::pull_request_observation;

	/// [[BPR-0990]]. Discovery's clock: wall seconds, read only to decide
	/// freshness and backoff and to say when a refresh succeeded.
	class system_discovery_clock final : public application::personal_discovery_clock_port
	{
	public:
		system_discovery_clock() = default;
		/// [[BPR-1848]]. A course's offset, added on every reading.
		explicit system_discovery_clock(std::shared_ptr<const std::atomic<std::int64_t>> aOffset);

		[[nodiscard]] auto now_seconds() const -> std::int64_t override;
		/// [[BPR-1680]]. Monotonic, for a refresh's logged duration only.
		[[nodiscard]] auto elapsed_milliseconds() const -> std::int64_t override;

	private:
		std::shared_ptr<const std::atomic<std::int64_t>> Offset;
	};

	class composition_queue_refresh final
		: public application::queue_refresh_effect_port
	{
	public:
		void retain(
			application::queue_session_id     aSession,
			core::pull_request_observation    aObservation,
			application::queue_refresh_status aStatus
			= application::queue_refresh_status::changed);

		void clear();

		auto refresh(
			const application::queue_refresh_request& aRequest,
			std::stop_token                           aStop) -> application::queue_refresh_completion override;

	private:
		std::mutex                                           Mutex;
		std::optional<application::queue_refresh_completion> Retained;
	};

	struct queue_notification_transition
	{
		application::queue_session_id Session;
		core::pull_request_ref        Ref;
		core::attention_bucket        Bucket
			= core::attention_bucket::quiet;

		auto operator<=>(const queue_notification_transition&) const = default;
	};

	struct queue_activity_baseline
	{
		application::queue_session_id Session;
		core::pull_request_ref        Ref;
	};

	struct queue_activity_fact
	{
		application::queue_session_id    Session;
		core::pull_request_ref           Ref;
		core::queue_activity_observation Activity;
		/// The observation the queue was refreshed from, kept so a later
		/// change in what I have seen can refresh it again ([[BPR-0659]]).
		core::pull_request_observation Observation;
	};
#if defined(BPR_BUILD_TESTING)
	class fixture_notification_sink final
		: public application::notification_sink_port
	{
	public:
		[[nodiscard]] auto permission()
			-> application::notification_permission override;

		[[nodiscard]] auto request_permission()
			-> application::notification_permission override;

		[[nodiscard]] auto deliver(
			const application::delivered_notification& aPayload)
			-> application::notification_submission_result override;

		void set_activation_sink(
			application::notification_activation_sink aSink);

		[[nodiscard]] auto activate() -> bool;

		[[nodiscard]] auto submitted_payload() const
			-> std::optional<application::delivered_notification>;

	private:
		application::notification_activation_sink          Activation;
		std::optional<application::delivered_notification> Payload;
		bool                                               ActivationConsumed = false;
	};
#endif
}
#endif
