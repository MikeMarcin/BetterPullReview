#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NOTIFICATION_DELIVERY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NOTIFICATION_DELIVERY_HPP

#include "bpr/application/review_queue_notification.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <string>

namespace bpr::application {
	/// \brief What the adapter knows about notification delivery authority.
	///
	/// \details A refusal is not the absence of a grant. A reviewer who declined
	/// has made a choice; a tool that treats that as "not yet asked" and asks
	/// again has overridden it. `platform_managed` is distinct from both: the
	/// adapter may submit, but cannot observe or claim the reviewer's answer.
	enum class notification_permission : std::uint8_t {
		not_determined,
		granted,
		/// \brief The platform owns permission and exposes no readable answer.
		platform_managed,
		refused,
	};

	/// \brief Exactly what reaches the operating system.
	///
	/// \details Three fields, all derived from `queue_notification_request` by
	/// functions the core owns. This type is the last place the payload can be
	/// inspected before it leaves, which is why the suite asserts it whole
	/// rather than searching it for the absence of a bad string: a test that
	/// only proves one string is missing passes against a payload carrying a
	/// different one.
	struct delivered_notification
	{
		std::string               Title;
		std::string               Body;
		queue_notification_target Target;

		auto operator<=>(const delivered_notification&) const = default;
	};

	/// \brief Reports whether an identityless platform can safely attribute activation.
	enum class notification_submission_result : std::uint8_t {
		submitted,
		suppressed_unattributable_activation,
	};

	/// \brief The platform's notification surface.
	///
	/// \details Deliberately narrow. An implementation can report what the
	/// reviewer has allowed, ask once, and either submit one payload or report
	/// that activation could not be attributed safely.
	class notification_sink_port
	{
	public:
		virtual ~notification_sink_port() = default;

		[[nodiscard]] virtual auto permission() -> notification_permission = 0;
		/// \brief Asks the reviewer once.
		///
		/// \details Called only from `not_determined`. An implementation that
		/// re-asks from `refused` would be overriding an answer already given.
		[[nodiscard]] virtual auto request_permission()
			-> notification_permission = 0;
		[[nodiscard]] virtual auto deliver(
			const delivered_notification& aPayload)
			-> notification_submission_result = 0;
	};

	/// \brief Receives the review a reviewer chose from a notification.
	using notification_activation_sink
		= std::function<void(queue_notification_target)>;

	/// \brief Records directly observable submission activity.
	struct notification_delivery_statistics
	{
		std::size_t Submitted                          = 0U;
		std::size_t SuppressedByBucket                 = 0U;
		std::size_t SuppressedByRefusal                = 0U;
		std::size_t SuppressedUnattributableActivation = 0U;
		std::size_t PermissionRequests                 = 0U;
		std::size_t Activations                        = 0U;

		auto operator<=>(const notification_delivery_statistics&) const = default;
	};

	/// \brief Hands a queue notification to the platform, and routes the way back.
	///
	/// \details **This class holds a sink and a callback and nothing else.**
	/// That is the guarantee, and it is structural rather than reviewed: it has
	/// no pull request record, no review ledger, and no diff, so there is no
	/// path from here to repository content even for someone trying to
	/// "improve" the wording. The payload is composed from the request's own
	/// fields by `notification_title`, `notification_body`, and
	/// `notification_target`.
	///
	/// It selects nothing. An activation becomes a target handed to the
	/// composition, which is what decides that a review is opened — the same
	/// separation every other surface in this product keeps.
	class review_queue_notifier final
	{
	public:
		explicit review_queue_notifier(
			notification_sink_port&      aSink,
			notification_activation_sink aActivations = { });

		/// \brief Delivers one request, if the reviewer allows it.
		///
		/// \returns Whether the platform was asked to show anything.
		auto deliver(const queue_notification_request& aRequest) -> bool;

		/// \brief Reports that the reviewer chose a submitted notification.
		void activate(const queue_notification_target& aTarget);

		[[nodiscard]] auto statistics() const noexcept
			-> notification_delivery_statistics;

	private:
		notification_sink_port&          Sink;
		notification_activation_sink     Activations;
		notification_delivery_statistics Statistics;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NOTIFICATION_DELIVERY_HPP
