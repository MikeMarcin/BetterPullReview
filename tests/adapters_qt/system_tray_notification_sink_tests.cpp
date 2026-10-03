#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QApplication>
#include <QString>
#include <QSystemTrayIcon>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto payload(const std::int64_t aNumber)
		-> application::delivered_notification
	{
		return {
			.Title  = "octo-org/review-repository #" + std::to_string(aNumber),
			.Body   = "The code moved after you reviewed it.",
			.Target = {
				.Ref = {
					.Host       = "github.com",
					.Account    = "octo-reviewer",
					.Repository = "octo-org/review-repository",
					.Number     = aNumber,
				},
			},
		};
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         refused = bpr::qt::system_tray_notification_sink{
		bpr::qt::system_notification_display_boundary{ }
	};
	expect(
		refused.permission() == bpr::application::notification_permission::refused,
		"an absent display boundary is an explicit refusal");
	auto displayedTitle = QString{ };
	auto displayedBody  = QString{ };
	auto displays       = std::size_t{ 0U };
	auto sink           = bpr::qt::system_tray_notification_sink{
		[&displayedTitle, &displayedBody, &displays](
			QSystemTrayIcon& aTray,
			const QString&   aTitle,
			const QString&   aBody) {
			expect(!aTray.icon().isNull(), "the tray receives a non-null icon before use");
			displayedTitle = aTitle;
			displayedBody  = aBody;
			++displays;
		}
	};
	auto* const tray = sink.tray_icon();
	expect(tray != nullptr && !tray->icon().isNull(), "the sink owns a prepared tray");
	expect(
		sink.permission()
			== bpr::application::notification_permission::not_determined,
		"tray capability is not reported as an owner permission grant");
	expect(
		sink.request_permission()
			== bpr::application::notification_permission::platform_managed,
		"the first submission attempt leaves prompting to the operating system");
	expect(
		sink.permission()
			== bpr::application::notification_permission::platform_managed,
		"the unreadable owner permission is never relabelled as granted");
	auto activated
		= std::optional<bpr::application::queue_notification_target>{ };
	auto activationCount = std::size_t{ 0U };
	sink.set_activation_sink(
		[&activated, &activationCount](
			bpr::application::queue_notification_target aTarget) {
			activated = std::move(aTarget);
			++activationCount;
		});
	const auto payloadA = payload(17);
	const auto payloadB = payload(18);

	expect(
		sink.deliver(payloadA)
			== bpr::application::notification_submission_result::submitted,
		"A is submitted");
	expect(
		sink.deliver(payloadB)
			== bpr::application::notification_submission_result::
				suppressed_unattributable_activation,
		"B is suppressed before showMessage can replace A's identity");
	expect(
		displays == 1U
			&& displayedTitle == QString::fromStdString(payloadA.Title)
			&& displayedBody == QString::fromStdString(payloadA.Body),
		"the display boundary receives the exact metadata-only title and body once");
	tray->messageClicked();
	expect(
		activated == std::optional{ payloadA.Target } && activationCount == 1U,
		"the click routes exact immutable A");
	tray->messageClicked();
	expect(activationCount == 1U, "a duplicate or delayed click is a no-op");
	expect(
		sink.deliver(payloadB)
			== bpr::application::notification_submission_result::
				suppressed_unattributable_activation,
		"the sink never rearms after activation");
	expect(sink.submitted() == 1U, "only A reached the platform submission call");
	expect(
		sink.submitted_payload() == std::optional{ payloadA },
		"the exact first metadata-only payload remains observable");

	// [[BPR-1250]]. On macOS every profile's sink submits through the
	// application's one status item. A click carries no message identity, so it
	// is the latest submission's, and only that sink's.
	auto shared = QSystemTrayIcon{ };
	shared.setObjectName(QStringLiteral("menuBarStatusItem"));
	shared.setToolTip(QStringLiteral("the owner's tip"));
	auto earlierActivations = std::size_t{ 0U };
	auto laterActivations   = std::size_t{ 0U };
	auto laterTarget        = std::optional<bpr::application::queue_notification_target>{ };
	{
		auto earlier = bpr::qt::system_tray_notification_sink{ shared };
		auto later   = bpr::qt::system_tray_notification_sink{ shared };
		expect(earlier.tray_icon() == &shared && later.tray_icon() == &shared,
			   "both sinks submit through the one caller-owned item");
		expect(shared.objectName() == QStringLiteral("menuBarStatusItem")
				   && shared.toolTip() == QStringLiteral("the owner's tip"),
			   "a sink keeps the name and tip the item's owner gave it");
		earlier.set_activation_sink([&earlierActivations](auto) { ++earlierActivations; });
		later.set_activation_sink([&laterActivations, &laterTarget](auto aTarget) {
			laterTarget = std::move(aTarget);
			++laterActivations;
		});
		static_cast<void>(earlier.request_permission());
		static_cast<void>(later.request_permission());
		const auto earlierPayload = payload(21);
		const auto laterPayload   = payload(22);
		expect(earlier.deliver(earlierPayload) == bpr::application::notification_submission_result::submitted
				   && later.deliver(laterPayload) == bpr::application::notification_submission_result::submitted,
			   "each profile submits its first notification");
		shared.messageClicked();
		expect(laterActivations == 1U && laterTarget == std::optional{ laterPayload.Target },
			   "the click reaches the latest submission");
		expect(earlierActivations == 0U, "and no other sink sharing the item");
		shared.messageClicked();
		expect(laterActivations == 1U && earlierActivations == 0U, "a repeated click is still a no-op");
	}
	shared.messageClicked();
	expect(laterActivations == 1U && earlierActivations == 0U,
		   "a sink that is gone hears nothing from the item that outlives it");

	std::cout << "system-tray-notification-sink-tests:pass\n";
	return EXIT_SUCCESS;
}
