#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QSpinBox>
#include <QThread>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
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

	[[nodiscard]] auto fixture_ref() -> core::pull_request_ref
	{
		return {
			.Host       = "github.com",
			.Account    = "octo-reviewer",
			.Repository = "octo-org/review-repository",
			.Number     = 17,
		};
	}

	[[nodiscard]] auto fixture_preferences()
		-> application::queue_presentation_preferences
	{
		return {
			.Role           = core::github_role::none,
			.ObserverReason = core::observer_rule_kind::repository,
			.Repository     = "octo-org/review-repository",
			.Quiet          = application::queue_quiet_filter::all,
			.Sort           = application::queue_sort_order::pull_request_number,
		};
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3) {
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (course != "seed" && course != "reopen" && course != "request-cut"
		&& course != "activation-cut") {
		return EXIT_FAILURE;
	}
	const auto refreshCourse = course != "reopen";
	auto       application   = app::detail::github_application_fixture_access::make(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot              = std::filesystem::path{ argv[2] },
			.EpochPhase                       = refreshCourse
				? app::detail::github_epoch_fixture_phase::seed
				: app::detail::github_epoch_fixture_phase::reuse,
			.UseGrantedNotificationSink       = true,
			.DisableNotificationRequestWire   = course == "request-cut",
			.DisableNotificationSelectionWire = course == "activation-cut",
		});
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	if (!wait_until([&window] {
			return window.github_status_text() == "GitHub account connected."
				&& window.github_open_action()->isEnabled();
		})) {
		return fail(*application, "connect");
	}
	window.session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
	window.session_panel()->repository_editor()->setText(
		QStringLiteral("review-repository"));
	window.session_panel()->pull_request_editor()->setValue(17);
	window.github_open_action()->trigger();

	const auto rule = core::observer_rule{
		.Kind    = core::observer_rule_kind::repository,
		.Pattern = "octo-org/review-repository",
	};
	if (refreshCourse) {
		window.queue_rule_pattern_editor()->setText(
			QString::fromStdString(rule.Pattern));
		if (!wait_until([&window] {
				return window.add_queue_rule_action()->isEnabled();
			})) {
			return fail(*application, "queue-ready");
		}
		window.add_queue_rule_action()->trigger();
	}
	if (!wait_until([&window, &rule] {
			if (!window.remove_queue_rule_action()->isEnabled()
				|| window.queue_landing()->rowCount() != 1) {
				return false;
			}
			const auto item = window.queue_landing()->item_at(
				window.queue_landing()->index(0));
			return item.has_value() && item->Ref == fixture_ref()
				&& item->Membership.Role == core::github_role::none
				&& item->Membership.MatchedRule
				&& item->Membership.Rule == rule
				&& item->Bucket == core::attention_bucket::quiet;
		})) {
		return fail(*application, "first-head-is-exact-and-quiet");
	}
	const auto initialEvidence = app::detail::github_application_fixture_access::
		queue_notification_evidence(*application);
	if (initialEvidence.RequestsAttempted != 0U
		|| initialEvidence.Submitted != 0U
		|| initialEvidence.LastSubmitted.has_value()) {
		return fail(*application, "first-head-requested-nothing");
	}

	if (refreshCourse) {
		const auto initialRef = fixture_ref();
		window.github_refresh_action()->trigger();
		if (!wait_until([&window, &initialRef] {
				const auto item = window.queue_landing()->item_at(
					window.queue_landing()->index(0));
				return item.has_value() && item->Ref == initialRef
					&& item->Bucket
					== core::attention_bucket::changed_since_my_review;
			})) {
			return fail(*application, "same-ref-head-refresh-bucket-transition");
		}
		const auto item = window.queue_landing()->item_at(
			window.queue_landing()->index(0));
		if (!item.has_value() || item->Ref != initialRef) {
			return fail(*application, "exact-ref-changed-during-refresh");
		}

		if (course == "request-cut") {
			const auto evidence = app::detail::github_application_fixture_access::
				queue_notification_evidence(*application);
			if (evidence.RequestsAttempted != 0U || evidence.Submitted != 0U
				|| evidence.LastSubmitted.has_value()) {
				return fail(*application, "request-cut-not-cut");
			}
		} else {
			if (!wait_until([&application] {
					const auto evidence = app::detail::github_application_fixture_access::
						queue_notification_evidence(*application);
					return evidence.RequestsAttempted == 1U
						&& evidence.Submitted == 1U
						&& evidence.LastSubmitted.has_value();
				})) {
				return fail(*application, "notification-submission");
			}
			const auto evidence = app::detail::github_application_fixture_access::
				queue_notification_evidence(*application);
			const auto expected = application::delivered_notification{
				.Title  = "octo-org/review-repository #17",
				.Body   = std::string{ core::explanation(item->Bucket) },
				.Target = application::queue_notification_target{ .Ref = initialRef },
			};
			if (evidence.LastSubmitted != std::optional{ expected }
				|| evidence.SuppressedUnattributableActivation != 0U
				|| application->status().NotificationRequestsAttempted != 1U
				|| application->status().NotificationRequestsSubmitted != 1U) {
				return fail(*application, "exact-metadata-only-payload");
			}
			if (!app::detail::github_application_fixture_access::
					activate_fixture_notification(*application)) {
				return fail(*application, "platform-activation");
			}
			if (app::detail::github_application_fixture_access::
					activate_fixture_notification(*application)) {
				return fail(*application, "duplicate-platform-activation");
			}
			if (course == "activation-cut") {
				if (!wait_until([&application] {
						const auto cut = app::detail::github_application_fixture_access::
							queue_notification_evidence(*application);
						return cut.Activations == 1U;
					})
					|| app::detail::github_application_fixture_access::
							queue_notification_evidence(*application)
								.Selections
						!= 0U) {
					return fail(*application, "activation-cut-not-cut");
				}
			} else if (!wait_until([&application, &window, &initialRef] {
						   const auto selected = window.queue_filter_model()->item_at(
							   window.queue_view()->currentIndex());
						   return application->status().NotificationActivations == 1U
							   && application->status().NotificationSelections == 1U
							   && selected.has_value()
							   && selected->Ref == initialRef
							   && QApplication::focusWidget() == window.queue_view();
					   })) {
				return fail(*application, "activation-selection");
			}
		}

		if (course == "seed") {
			const auto preferences = fixture_preferences();
			window.queue_role_filter()->setCurrentIndex(
				window.queue_role_filter()->findData(
					static_cast<int>(core::github_role::none)));
			window.queue_observer_filter()->setCurrentIndex(
				window.queue_observer_filter()->findData(
					static_cast<int>(core::observer_rule_kind::repository)));
			window.queue_repository_filter()->setCurrentIndex(
				window.queue_repository_filter()->findData(
					QStringLiteral("octo-org/review-repository")));
			window.queue_sort_selector()->setCurrentIndex(
				window.queue_sort_selector()->findData(
					static_cast<int>(application::queue_sort_order::pull_request_number)));
			if (!wait_until([&application, &window, &preferences] {
					return window.queue_filter_model()->preferences() == preferences
						&& window.queue_filter_model()->rowCount() == 1
						&& app::detail::github_application_fixture_access::
							   queue_notification_evidence(*application)
								   .QueueQuiescent;
				})) {
				return fail(*application, "preference-gesture-persist");
			}
		}
	} else {
		if (window.queue_filter_model()->preferences() != fixture_preferences()) {
			return fail(*application, "restore-preferences");
		}
		const auto evidence = app::detail::github_application_fixture_access::
			queue_notification_evidence(*application);
		if (evidence.RequestsAttempted != 0U || evidence.Submitted != 0U
			|| evidence.LastSubmitted.has_value()) {
			return fail(*application, "reopen-fabricated-delivery");
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
	if (course == "seed") {
		std::cout << "QUEUE_COMPOSITION_SEED_OK\n";
	} else if (course == "reopen") {
		std::cout << "QUEUE_COMPOSITION_REOPEN_OK\n";
	} else if (course == "request-cut") {
		std::cout << "QUEUE_COMPOSITION_REQUEST_CUT_OK\n";
	} else {
		std::cout << "QUEUE_COMPOSITION_ACTIVATION_CUT_OK\n";
	}
	return EXIT_SUCCESS;
}
