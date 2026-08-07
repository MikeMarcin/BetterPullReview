/// \file
/// \brief Proves a queue notification reaches the platform, and what it carries.

#include "bpr/application/notification_delivery.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief Stands in for the desktop, and records exactly what it was given.
	class fake_platform final : public application::notification_sink_port
	{
	public:
		explicit fake_platform(
			const application::notification_permission aInitial)
			: Current{ aInitial }
		{
		}

		[[nodiscard]] auto permission()
			-> application::notification_permission override
		{
			++Reads;
			return Current;
		}

		[[nodiscard]] auto request_permission()
			-> application::notification_permission override
		{
			++Asks;
			Current = Answer;
			return Current;
		}

		void deliver(const application::delivered_notification& aPayload) override
		{
			Payloads.push_back(aPayload);
		}

		application::notification_permission Current;
		application::notification_permission Answer
			= application::notification_permission::granted;
		std::vector<application::delivered_notification> Payloads;
		std::size_t                                      Reads = 0U;
		std::size_t                                      Asks  = 0U;
	};

	[[nodiscard]] auto reference() -> core::pull_request_ref
	{
		return {
			.Host       = "github.com",
			.Account    = "octo-org",
			.Repository = "review-repository",
			.Number     = 336,
		};
	}

	[[nodiscard]] auto loud_request(const std::uint32_t aUnread)
		-> application::queue_notification_request
	{
		return {
			.Ref            = reference(),
			.Bucket         = core::attention_bucket::new_comments_or_author_replies,
			.UnreadComments = aUnread,
		};
	}

	/// \brief The payload must be exactly what the request produced.
	void test_the_payload_is_asserted_whole()
	{
		auto platform = fake_platform{
			application::notification_permission::granted
		};
		auto       notifier = application::review_queue_notifier{ platform };
		const auto request  = loud_request(3U);

		expect(notifier.deliver(request), "a loud bucket is delivered");
		expect(platform.Payloads.size() == 1U, "exactly one payload reached the platform");

		// Whole, not searched. A test that only proved some string absent would
		// pass against a payload carrying a different one.
		const auto expected = application::delivered_notification{
			.Title  = application::notification_title(request),
			.Body   = application::notification_body(request),
			.Target = application::notification_target(request),
		};
		expect(
			platform.Payloads.front() == expected,
			"the delivered payload equals what the request's own functions "
			"produce, field for field");
		expect(
			platform.Payloads.front().Target.Ref == request.Ref,
			"the payload's target names the review the request named");
		expect(
			!expected.Title.empty() && !expected.Body.empty(),
			"the comparison above is not satisfied by two empty strings");
	}

	/// \brief A refusal delivers nothing, reports nothing, and does not re-ask.
	void test_a_refusal_is_a_normal_outcome()
	{
		auto platform = fake_platform{
			application::notification_permission::refused
		};
		auto notifier = application::review_queue_notifier{ platform };

		for (auto attempt = 0U; attempt < 5U; ++attempt) {
			expect(
				!notifier.deliver(loud_request(1U)),
				"a refused reviewer is not notified");
		}
		expect(platform.Payloads.empty(), "nothing reached the platform");
		expect(
			platform.Asks == 0U,
			"a reviewer who declined is never asked again, however many "
			"notifications would otherwise have been delivered");
		expect(
			notifier.statistics().SuppressedByRefusal == 5U
				&& notifier.statistics().Delivered == 0U,
			"the refusals are counted as suppressions rather than as failures");
	}

	/// \brief An undetermined permission is asked once, then honoured.
	void test_permission_is_asked_once()
	{
		auto platform = fake_platform{
			application::notification_permission::not_determined
		};
		platform.Answer = application::notification_permission::refused;
		auto notifier   = application::review_queue_notifier{ platform };

		expect(!notifier.deliver(loud_request(1U)), "a declined prompt delivers nothing");
		expect(platform.Asks == 1U, "the reviewer was asked");
		expect(!notifier.deliver(loud_request(2U)), "the second is still not delivered");
		expect(
			platform.Asks == 1U,
			"and the reviewer was not asked a second time, because the "
			"platform now reports the answer they gave");
	}

	/// \brief A quiet bucket asks nothing of anyone.
	void test_a_quiet_bucket_is_not_delivered()
	{
		auto platform = fake_platform{
			application::notification_permission::granted
		};
		auto notifier = application::review_queue_notifier{ platform };
		expect(
			!application::warrants_notification(core::attention_bucket::quiet),
			"the core says a quiet bucket warrants nothing, which is what the "
			"assertion below relies on");
		expect(
			!notifier.deliver({
				.Ref    = reference(),
				.Bucket = core::attention_bucket::quiet,
			}),
			"a quiet bucket is not delivered");
		expect(
			platform.Payloads.empty() && platform.Reads == 0U,
			"and the platform is not even consulted, so a quiet queue cannot "
			"raise a permission prompt");
	}

	/// \brief Activation names the review and selects nothing.
	void test_activation_reports_the_target()
	{
		auto platform = fake_platform{
			application::notification_permission::granted
		};
		auto chosen   = std::optional<application::queue_notification_target>{ };
		auto notifier = application::review_queue_notifier{
			platform,
			[&chosen](application::queue_notification_target aTarget) {
				chosen = aTarget;
			}
		};
		const auto request = loud_request(1U);
		expect(notifier.deliver(request), "the notification is delivered");
		notifier.activate(platform.Payloads.front().Target);
		expect(
			chosen.has_value() && chosen->Ref == request.Ref,
			"activation reports the review the notification named");
		expect(
			notifier.statistics().Activations == 1U,
			"the activation is counted");
	}
} // namespace

auto main() -> int
{
	test_the_payload_is_asserted_whole();
	test_a_refusal_is_a_normal_outcome();
	test_permission_is_asked_once();
	test_a_quiet_bucket_is_not_delivered();
	test_activation_reports_the_target();
	std::cout << "notification-delivery-tests:pass\n";
	return EXIT_SUCCESS;
}
