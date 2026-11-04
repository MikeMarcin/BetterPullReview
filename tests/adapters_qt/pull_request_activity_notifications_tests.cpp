// [[BPR-1850]]. An activity notification is posted through the tray, its
// click reaches the host and opens the pull request in a window, a tray shared
// with the review queue's sink routes a click to whichever posted last, the
// one timer is armed as the host asks, and Settings shows the switch.
#include "bpr/qt/pull_request_activity_notifications.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>

#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string_view>
#include <utility>
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

	void process_events()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	class inline_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void drain()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}
		std::deque<std::function<void()>> Tasks;
	};

	class scripted final : public application::pull_request_activity_port
	{
	public:
		void poll_activity(application::pull_request_activity_request, application::pull_request_activity_completion aCompletion) override
		{
			auto answer = std::move(Script.front());
			Script.pop_front();
			aCompletion(std::move(answer));
		}
		std::deque<application::pull_request_activity_answer> Script;
	};

	class no_store final : public application::pull_request_activity_store_port
	{
	public:
		auto store(const core::activity_memory&) -> bool override { return true; }
	};

	auto answer(std::vector<core::activity_thread> aThreads) -> application::pull_request_activity_answer
	{
		auto value              = application::pull_request_activity_answer{ };
		value.Poll.Outcome      = core::activity_poll_outcome::changed;
		value.Poll.PollInterval = 60U;
		value.Threads           = std::move(aThreads);
		value.Complete          = true;
		return value;
	}

	auto session() -> application::personal_discovery_session
	{
		auto value                            = application::personal_discovery_session{ };
		value.Queue                           = { .Host = "github.com", .Account = "me", .Generation = 1U, .ViewerAccount = core::github_account_id{ 42U } };
		value.Context.ApplicationSession      = core::application_session_id{ 1U };
		value.Context.Profile                 = core::host_profile_id{ 2U };
		value.Context.IdentityGeneration      = core::github_identity_generation{ 1U };
		value.ProfileAuthority.Authentication = application::github_authentication_transport::github_cli;
		return value;
	}

	void test_posted_click_opens_the_pull_request()
	{
		auto shown = std::vector<std::pair<QString, QString>>{ };
		auto sink  = qt::pull_request_activity_notification_sink{ [&](QSystemTrayIcon&, const QString& aTitle, const QString& aBody) { shown.emplace_back(aTitle, aBody); } };
		auto host  = std::unique_ptr<application::pull_request_activity_host>{ };
		auto timer = qt::pull_request_activity_timer{ [&] { host->tick(); } };

		auto intents = std::vector<application::github_intent>{ };
		auto window  = qt::workbench_window{ { }, { }, { }, { }, [&](application::github_intent aIntent) { intents.push_back(std::move(aIntent)); } };
		window.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::ready, .Session = { .Host = "github.com", .Account = "me", .Generation = 1U } });
		process_events();

		auto effects   = inline_executor{ };
		auto decisions = inline_executor{ };
		auto provider  = scripted{ };
		auto store     = no_store{ };
		auto now       = std::int64_t{ 1793707000 };
		host           = std::make_unique<application::pull_request_activity_host>(provider, store, effects, decisions, timer, sink, [&] { return now; }, [&] { auto focused = std::vector<core::pull_request_ref>{ }; if (auto ref = window.focused_pull_request()) { focused.push_back(*ref); } return focused; }, core::activity_memory_read{ });
		sink.set_activation([&] {
			const auto target = host->activate();
			expect(window.open_notified_pull_request(target.PullRequest), "the click's pull request opens in the window");
		});

		provider.Script.push_back(answer({ }));
		host->adopt(session());
		decisions.drain();
		effects.drain();
		decisions.drain();
		expect(timer.timer()->isActive() && timer.timer()->isSingleShot() && timer.timer()->interval() == 60'000, "the one timer is armed for the poll interval");
		expect(shown.empty(), "the baseline posts nothing");

		provider.Script.push_back(answer({ core::activity_thread{ .Id = "7", .UpdatedAt = 1793707100, .Unread = true, .LastReadAt = std::nullopt, .Reason = core::activity_reason::author, .Repository = "octo/widgets", .Number = 42, .Anchor = { .Kind = core::activity_anchor_kind::review, .Id = 5U } } }));
		now += 60;
		// The timer is coarse, so its shortest wait is up to a second: run the
		// event loop until it fires, bounded.
		timer.timer()->setInterval(0);
		timer.timer()->start();
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
		while (shown.empty() && std::chrono::steady_clock::now() < deadline) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
			decisions.drain();
			effects.drain();
		}
		expect(shown.size() == 1U && shown[0] == std::pair<QString, QString>{ QStringLiteral("octo/widgets #42"), QStringLiteral("New review.") } && sink.posted() == 1U,
			   "the timer's poll posts one notification through the tray");

		emit sink.tray_icon()->messageClicked();
		process_events();
		expect(intents.size() == 1U && std::holds_alternative<core::open_pull_request_intent>(intents[0]), "the click asks for the pull request");
		const auto& open = std::get<core::open_pull_request_intent>(intents[0]);
		expect(open.Repository == core::repository_locator{ .Owner = "octo", .Name = "widgets" } && open.Number == core::pull_request_number{ 42U }, "the notified one");
		host->shutdown();
	}

	void test_a_shared_tray_routes_to_the_last_poster()
	{
		auto tray      = QSystemTrayIcon{ };
		auto activity  = qt::pull_request_activity_notification_sink{ tray };
		auto queue     = qt::system_tray_notification_sink{ tray };
		auto activated = 0;
		auto queued    = 0;
		activity.set_activation([&] { ++activated; });
		queue.set_activation_sink([&](application::queue_notification_target) { ++queued; });
		static_cast<void>(queue.request_permission());

		static_cast<void>(activity.post("octo/widgets #1", "New comment."));
		emit tray.messageClicked();
		expect(activated == 1 && queued == 0, "a click after an activity post is the activity sink's");

		static_cast<void>(queue.deliver({ .Title = "octo/widgets #2", .Body = "Review requested.", .Target = { } }));
		emit tray.messageClicked();
		expect(activated == 1 && queued == 1, "after the queue sink posts, the click is the queue's");
	}

	void test_settings_switch()
	{
		auto window = qt::workbench_window{ };
		expect(window.activity_notifications_switch() != nullptr && !window.activity_notifications_switch()->isEnabled(), "the switch is there, disabled without a dispatch");
		auto told = std::vector<bool>{ };
		window.set_activity_notifications_dispatch([&](const bool aEnabled) { told.push_back(aEnabled); });
		window.present_activity_notifications(true, QStringLiteral("Checking GitHub every minute."));
		expect(window.activity_notifications_switch()->isEnabled() && window.activity_notifications_switch()->isChecked() && told.empty(), "presenting tells nothing back");
		expect(window.activity_notifications_status()->text() == QStringLiteral("Checking GitHub every minute."), "the status line says what it does");
		expect(window.activity_notifications_info() != nullptr && window.activity_notifications_info()->toolTip().contains(QStringLiteral("nothing is marked read on GitHub"))
				   && !window.activity_notifications_info()->accessibleName().isEmpty(),
			   "the full explanation is behind the info button, named for assistive technology");
		window.activity_notifications_switch()->click();
		expect(told == std::vector<bool>{ false }, "the reviewer's click is dispatched");
		// For the owner's review of the wording: the Notifications page as shown.
		if (const auto render = qEnvironmentVariable("BPR_ACTIVITY_SETTINGS_RENDER"); !render.isEmpty()) {
			window.present_activity_notifications(true, QStringLiteral("Checking GitHub every minute."));
			window.settings()->resize(900, 720);
			window.settings()->present(qt::settings_page::notifications);
			window.settings()->show();
			process_events();
			expect(window.settings()->grab().save(render), "the Notifications page renders");
		}
	}
} // namespace

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	test_posted_click_opens_the_pull_request();
	test_a_shared_tray_routes_to_the_last_poster();
	test_settings_switch();
	std::cout << "pull-request-activity-notifications:pass\n";
	return EXIT_SUCCESS;
}
