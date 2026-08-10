#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QThread>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	template <typename Predicate>
	[[nodiscard]] auto wait_until(Predicate aPredicate) -> bool
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 20'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			if (aPredicate()) {
				return true;
			}
			QThread::msleep(5);
		}
		return false;
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		return wait_until([&aApplication] {
			return aApplication.status().ShutdownComplete;
		});
	}

	auto fail(app::github_application& aApplication, const std::string_view aMessage)
		-> int
	{
		std::cerr << "queue-composition-failed:" << aMessage << '\n';
		(void)shutdown(aApplication);
		return EXIT_FAILURE;
	}

	[[nodiscard]] auto fixture_preferences()
		-> application::queue_presentation_preferences
	{
		return {
			.Role           = core::github_role::none,
			.ObserverReason = core::observer_rule_kind::repository,
			.Bucket         = core::attention_bucket::observed_with_new_activity,
			.Repository     = "google/UIforETW",
			.Quiet          = application::queue_quiet_filter::attention,
			.Sort           = application::queue_sort_order::pull_request_number,
		};
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3) {
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (course != "seed" && course != "reopen") {
		return EXIT_FAILURE;
	}
	auto application = app::detail::github_application_fixture_access::make(
		app::detail::github_public_path_fixture::uiforetw_74,
		{ .ApplicationDataRoot = std::filesystem::path{ argv[2] } });
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	if (!wait_until([&window] {
			return window.github_status_text() == "GitHub account connected."
				&& window.github_open_action()->isEnabled();
		})) {
		return fail(*application, "connect");
	}
	window.session_panel()->owner_editor()->setText(QStringLiteral("google"));
	window.session_panel()->repository_editor()->setText(
		QStringLiteral("UIforETW"));
	window.session_panel()->pull_request_editor()->setValue(74);
	window.github_open_action()->trigger();

	const auto rule = core::observer_rule{
		.Kind    = core::observer_rule_kind::repository,
		.Pattern = "google/UIforETW",
	};
	if (course == "seed") {
		window.queue_rule_pattern_editor()->setText(
			QString::fromStdString(rule.Pattern));
		if (!wait_until([&window] {
				return window.add_queue_rule_action()->isEnabled();
			})) {
			return fail(*application, "queue-ready");
		}
		window.add_queue_rule_action()->trigger();
		if (!wait_until([&window, &rule] {
				if (!window.remove_queue_rule_action()->isEnabled()
					|| window.queue_landing()->rowCount() != 1) {
					return false;
				}
				const auto item = window.queue_landing()->item_at(
					window.queue_landing()->index(0));
				return item.has_value() && item->Membership.MatchedRule
					&& item->Membership.Rule == rule;
			})) {
			return fail(*application, "add-match-persist");
		}
		const auto item = window.queue_landing()->item_at(
			window.queue_landing()->index(0));
		if (!item.has_value() || !item->Membership.MatchedRule
			|| item->Membership.Rule != rule) {
			return fail(*application, "exact-reason");
		}
		const auto updates = window.statistics().QueueUpdates;
		if (item->Bucket
			!= core::attention_bucket::observed_with_new_activity) {
			return fail(*application, "exact-attention-bucket");
		}
		const auto preferences = fixture_preferences();
		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(
				static_cast<int>(core::github_role::none)));
		window.queue_observer_filter()->setCurrentIndex(
			window.queue_observer_filter()->findData(
				static_cast<int>(core::observer_rule_kind::repository)));
		window.queue_bucket_filter()->setCurrentIndex(
			window.queue_bucket_filter()->findData(
				static_cast<int>(
					core::attention_bucket::observed_with_new_activity)));
		window.queue_repository_filter()->setCurrentIndex(
			window.queue_repository_filter()->findData(
				QStringLiteral("google/UIforETW")));
		window.queue_quiet_filter()->setCurrentIndex(
			window.queue_quiet_filter()->findData(
				static_cast<int>(application::queue_quiet_filter::attention)));
		window.queue_sort_selector()->setCurrentIndex(
			window.queue_sort_selector()->findData(
				static_cast<int>(application::queue_sort_order::pull_request_number)));
		if (!wait_until([&window, &preferences, updates] {
				return window.statistics().QueueUpdates > updates
					&& window.queue_filter_model()->preferences() == preferences
					&& window.queue_filter_model()->rowCount() == 1;
			})) {
			return fail(*application, "preference-gesture-persist");
		}
	} else {
		if (!wait_until([&window] {
				return window.remove_queue_rule_action()->isEnabled()
					&& window.queue_landing()->rowCount() == 1
					&& window.queue_filter_model()->rowCount() == 1;
			})) {
			return fail(*application, "restore-match");
		}
		const auto preferences = fixture_preferences();
		if (window.queue_filter_model()->preferences() != preferences) {
			return fail(*application, "restore-preferences");
		}
		window.remove_queue_rule_action()->trigger();
		if (!wait_until([&window] {
				return !window.remove_queue_rule_action()->isEnabled()
					&& window.queue_landing()->rowCount() == 0
					&& window.queue_filter_model()->rowCount() == 0;
			})) {
			return fail(*application, "remove-empty");
		}
	}
	const auto mutation = app::detail::github_application_fixture_access::
		review_mutation_evidence(*application);
	if (mutation.Submissions != 0U || mutation.StoredComments != 0U) {
		return fail(*application, "provider-mutation");
	}
	if (!shutdown(*application)) {
		return EXIT_FAILURE;
	}
	std::cout << "QUEUE_COMPOSITION_" << (course == "seed" ? "SEED" : "REOPEN")
			  << "_OK\n";
	return EXIT_SUCCESS;
}
