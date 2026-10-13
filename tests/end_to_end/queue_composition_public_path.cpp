#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include "public_path_poll.hpp"

#include "bpr/qt/home_list.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	using e2e::default_queue_budget;
	using e2e::wait_terms;

	// The course's own budget, unchanged. `wait_until` and `fail` used to be
	// copies of the agent course's, differing only in whether the budget was a
	// parameter; both copies are gone (BPR-0381).
	template <typename Predicate>
	[[nodiscard]] auto wait_until_reporting(wait_terms& aTerms, Predicate aPredicate) -> bool
	{
		return e2e::wait_until(aTerms, aPredicate, default_queue_budget);
	}

	template <typename Predicate>
	[[nodiscard]] auto wait_until(Predicate aPredicate) -> bool
	{
		return e2e::wait_until_plain(aPredicate, default_queue_budget);
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		return wait_until([&aApplication] {
			return aApplication.status().ShutdownComplete;
		});
	}

	/// \brief Reports a failed condition without shutting anything down.
	///
	/// \details Separate from `fail` because one site is reached after
	/// `begin_shutdown` has already been called, where `fail`'s own shutdown wait
	/// would be a second one. That site used to print nothing at all.
	void report(const std::string_view aReason, const std::string_view aState = { })
	{
		std::cerr << "queue-composition-failed:" << aReason;
		if (!aState.empty()) {
			std::cerr << ' ' << aState;
		}
		std::cerr << '\n';
	}

	auto fail(
		app::github_application& aApplication,
		const std::string_view   aMessage,
		const std::string_view   aState = { }) -> int
	{
		report(aMessage, aState);
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
			.Role           = core::github_role::reviewer,
			.ObserverReason = core::observer_rule_kind::repository,
			.Repository     = "octo-org/review-repository",
			.Quiet          = application::queue_quiet_filter::all,
			.Sort           = application::queue_sort_order::pull_request_number,
			.Notifications  = { .Enabled = true, .Paused = true },
		};
	}

	// The bounded wait cannot distinguish a wrong first head from one that is
	// merely late, and BPR-0344's ext4 failure collapsed both into one string.
	// The timeout report therefore carries the observed terminal state: an
	// exact-but-late row names a durability-latency mechanism, while a wrong
	// field names a content mechanism.
	/// \brief Records the queue's first row into a wait report.
	///
	/// \details Was `first_head_state`, a hand-rolled string built only for the
	/// one call site below. The values it chose were right; what it lacked was
	/// which conjunct failed, a bound, and any reuse. BPR-0381.
	void note_first_head(
		wait_terms&                aTerms,
		qt::workbench_window&      aWindow,
		const core::observer_rule& aRule)
	{
		aTerms.note(
			"rows", static_cast<std::uint64_t>(aWindow.queue_landing()->rowCount()));
		if (aWindow.queue_landing()->rowCount() != 1) {
			return;
		}
		const auto item = aWindow.queue_landing()->item_at(
			aWindow.queue_landing()->index(0));
		if (!item.has_value()) {
			aTerms.note("item-present", false);
			return;
		}
		// The ref goes through the identifier channel, which bounds and sanitizes
		// it. Fixture refs are synthetic, but the rule has to hold by
		// construction rather than by that.
		auto ref = item->Ref.Host;
		ref += '/';
		ref += item->Ref.Account;
		ref += '/';
		ref += item->Ref.Repository;
		ref += '#';
		ref += std::to_string(item->Ref.Number);
		aTerms.note_identifier("ref", ref);
		aTerms.note_name("role", core::name(item->Membership.Role));
		aTerms.note("matched-rule", item->Membership.MatchedRule);
		aTerms.note("rule-exact", item->Membership.Rule == aRule);
		aTerms.note_name("bucket", core::name(item->Bucket));
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
	const auto dataRoot = std::filesystem::path{ argv[2] };
	if (!adapters::sqlite::make_storage_root_owner_private(dataRoot)) {
		std::cerr << "queue-composition-failed:private-test-root\n";
		return EXIT_FAILURE;
	}
	const auto                               refreshCourse = course != "reopen";
	std::unique_ptr<app::github_application> application;
	auto                                     startupTerminal = false;
	auto                                     startup         = app::detail::github_application_fixture_access::start(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot              = dataRoot,
			.EpochPhase                       = refreshCourse
				? app::detail::github_epoch_fixture_phase::seed
				: app::detail::github_epoch_fixture_phase::reuse,
			.UseGrantedNotificationSink       = true,
			.DisableNotificationRequestWire   = course == "request-cut",
			.DisableNotificationSelectionWire = course == "activation-cut",
		},
		[&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
	auto startupTerms = wait_terms{ };
	if (!wait_until_reporting(startupTerms, [&](wait_terms& aTerms) {
			return aTerms.require("startup-terminal", startupTerminal && startup && startup->is_quiescent());
		})
		|| !application) {
		std::cerr << "application-startup-failed:" << startupTerms.render() << '\n';
		return EXIT_FAILURE;
	}
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	auto connected = wait_terms{ };
	if (!wait_until_reporting(connected, [&window](wait_terms& aTerms) {
			aTerms.note_identifier("status", window.github_status_text());
			return aTerms.require(
					   "status-is-connected",
					   window.github_status_text() == "GitHub account connected.")
				&& aTerms.require(
					"open-enabled", window.github_open_action()->isEnabled());
		})) {
		return fail(*application, "connect", connected.render());
	}
	// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
	window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
	window.home_open_action()->trigger();

	const auto rule = core::observer_rule{
		.Kind    = core::observer_rule_kind::repository,
		.Pattern = "octo-org/review-repository",
	};
	if (refreshCourse) {
		window.queue_rule_pattern_editor()->setText(
			QString::fromStdString(rule.Pattern));
		auto queueReady = wait_terms{ };
		if (!wait_until_reporting(queueReady, [&window](wait_terms& aTerms) {
				return aTerms.require(
					"add-enabled", window.add_queue_rule_action()->isEnabled());
			})) {
			return fail(*application, "queue-ready", queueReady.render());
		}
		window.add_queue_rule_action()->trigger();
	}
	auto firstHead = wait_terms{ };
	if (!wait_until_reporting(firstHead, [&window, &rule, refreshCourse](wait_terms& aTerms) {
			note_first_head(aTerms, window, rule);
			if (!aTerms.require(
					"remove-enabled",
					window.remove_queue_rule_action()->isEnabled())
				|| !aTerms.require(
					"row-count-is-one",
					window.queue_landing()->rowCount() == 1)) {
				return false;
			}
			const auto item = window.queue_landing()->item_at(
				window.queue_landing()->index(0));
			return aTerms.require("item-present", item.has_value())
				&& aTerms.require("ref-is-the-fixture", item->Ref == fixture_ref())
				&& aTerms.require(
					"role-is-none",
					item->Membership.Role == core::github_role::none)
				&& aTerms.require("matched-a-rule", item->Membership.MatchedRule)
				&& aTerms.require("rule-is-exact", item->Membership.Rule == rule)
				&& aTerms.require(
					"bucket-matches-accepted-review",
					item->Bucket == (refreshCourse ? core::attention_bucket::quiet : core::attention_bucket::changed_since_my_review));
		})) {
		return fail(
			*application, "first-head-is-exact-and-quiet", firstHead.render());
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
		auto refreshed = wait_terms{ };
		if (!wait_until_reporting(
				refreshed, [&window, &initialRef](wait_terms& aTerms) {
					const auto item = window.queue_landing()->item_at(
						window.queue_landing()->index(0));
					aTerms.note(
						"rows",
						static_cast<std::uint64_t>(
							window.queue_landing()->rowCount()));
					if (item.has_value()) {
						aTerms.note_name("bucket", core::name(item->Bucket));
					}
					return aTerms.require("item-present", item.has_value())
						&& aTerms.require("ref-unchanged", item->Ref == initialRef)
						&& aTerms.require(
							"bucket-is-changed-since-my-review",
							item->Bucket
								== core::attention_bucket::changed_since_my_review);
				})) {
			return fail(
				*application,
				"same-ref-head-refresh-bucket-transition",
				refreshed.render());
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
			auto submitted = wait_terms{ };
			if (!wait_until_reporting(submitted, [&application](wait_terms& aTerms) {
					const auto evidence = app::detail::github_application_fixture_access::
						queue_notification_evidence(*application);
					return aTerms.require(
							   "requests-attempted-is-one",
							   evidence.RequestsAttempted == 1U)
						&& aTerms.require("submitted-is-one", evidence.Submitted == 1U)
						&& aTerms.require(
							"last-submitted-present",
							evidence.LastSubmitted.has_value());
				})) {
				return fail(
					*application, "notification-submission", submitted.render());
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
				auto activationCut = wait_terms{ };
				if (!wait_until_reporting(
						activationCut,
						[&application](wait_terms& aTerms) {
							const auto cut = app::detail::
								github_application_fixture_access::
									queue_notification_evidence(*application);
							aTerms.note(
								"selections",
								static_cast<std::uint64_t>(cut.Selections));
							return aTerms.require(
								"activations-is-one", cut.Activations == 1U);
						})
					|| app::detail::github_application_fixture_access::
							queue_notification_evidence(*application)
								.Selections
						!= 0U) {
					return fail(
						*application,
						"activation-cut-not-cut",
						activationCut.render());
				}
			} else if (auto selection = wait_terms{ };
					   !wait_until_reporting(
						   selection,
						   [&application, &window, &initialRef](wait_terms& aTerms) {
							   const auto selected
								   = window.selected_queue_item();
							   const auto status = application->status();
							   aTerms.note(
								   "activations",
								   static_cast<std::uint64_t>(
									   status.NotificationActivations));
							   aTerms.note(
								   "selections",
								   static_cast<std::uint64_t>(
									   status.NotificationSelections));
							   return aTerms.require(
										  "activations-is-one",
										  status.NotificationActivations == 1U)
								   && aTerms.require(
									   "selections-is-one",
									   status.NotificationSelections == 1U)
								   && aTerms.require(
									   "row-selected", selected.has_value())
								   && aTerms.require(
									   "selected-ref-matches",
									   selected->Ref == initialRef)
								   && aTerms.require(
									   "queue-view-focused",
									   QApplication::focusWidget()
										   == window.home_list()->current_view());
						   })) {
				return fail(
					*application, "activation-selection", selection.render());
			}
		}

		if (course == "seed") {
			auto* const preview = window.findChild<QPushButton*>(QStringLiteral("notificationPreview"));
			auto* const paused  = window.findChild<QCheckBox*>(QStringLiteral("notificationsPaused"));
			if (preview == nullptr || paused == nullptr || !preview->isEnabled()) {
				return fail(*application, "notification-settings-missing");
			}
			preview->click();
			const auto previewEvidence = app::detail::github_application_fixture_access::queue_notification_evidence(*application);
			if (previewEvidence.Submitted != 1U || previewEvidence.SuppressedUnattributableActivation != 1U) {
				return fail(*application, "notification-preview-refusal-wire");
			}
			paused->click();
			const auto preferences = fixture_preferences();
			window.queue_role_filter()->setCurrentIndex(
				window.queue_role_filter()->findData(
					static_cast<int>(core::github_role::reviewer)));
			window.queue_observer_filter()->setCurrentIndex(
				window.queue_observer_filter()->findData(
					static_cast<int>(core::observer_rule_kind::repository)));
			window.queue_repository_filter()->setCurrentIndex(
				window.queue_repository_filter()->findData(
					QStringLiteral("octo-org/review-repository")));
			window.queue_sort_selector()->setCurrentIndex(
				window.queue_sort_selector()->findData(
					static_cast<int>(application::queue_sort_order::pull_request_number)));
			auto persisted = wait_terms{ };
			if (!wait_until_reporting(
					persisted,
					[&application, &window, &preferences](wait_terms& aTerms) {
						aTerms.note(
							"filtered-rows",
							static_cast<std::uint64_t>(
								window.queue_filter_model()->rowCount()));
						return aTerms.require(
								   "preferences-match",
								   window.queue_filter_model()->preferences()
									   == preferences)
							&& aTerms.require(
								"filtered-rows-is-one",
								window.queue_filter_model()->rowCount() == 1)
							&& aTerms.require(
								"queue-quiescent",
								app::detail::github_application_fixture_access::
									queue_notification_evidence(*application)
										.QueueQuiescent);
					})) {
				return fail(
					*application, "preference-gesture-persist", persisted.render());
			}
			if (!paused->isChecked() || preview->isEnabled()) {
				return fail(*application, "paused-preview-enabled");
			}
			preview->click();
			const auto pausedEvidence = app::detail::github_application_fixture_access::queue_notification_evidence(*application);
			if (pausedEvidence.Submitted != 1U || pausedEvidence.SuppressedUnattributableActivation != 1U) {
				return fail(*application, "paused-preview-delivered");
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
		auto emptied = wait_terms{ };
		if (!wait_until_reporting(emptied, [&window](wait_terms& aTerms) {
				return aTerms.require(
						   "remove-disabled",
						   !window.remove_queue_rule_action()->isEnabled())
					&& aTerms.require(
						"landing-empty", window.queue_landing()->rowCount() == 0)
					&& aTerms.require(
						"filtered-empty",
						window.queue_filter_model()->rowCount() == 0);
			})) {
			return fail(*application, "remove-empty", emptied.render());
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
