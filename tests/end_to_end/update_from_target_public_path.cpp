#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	void wait(const auto& aPredicate, const char* aMessage)
	{
		expect(e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget), aMessage);
	}

	enum class cold_press { status,
							single,
							repeated,
							closed,
							stopped };

	void exercise_cold(const cold_press aPress)
	{
		QTemporaryDir directory;
		const auto    root = e2e::private_data_root(directory);
		expect(root.has_value(), "private cold fixture root");
		auto options                      = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = root };
		options.FixtureAgentSession       = true;
		options.InlineDrawerMergeConflict = aPress == cold_press::status;
		auto application                  = std::unique_ptr<app::github_application>{ };
		auto finished                     = false;
		auto startup                      = access::start(app::detail::github_public_path_fixture::inline_drawer_v1, std::move(options), [&](auto aResult) {
			finished = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return finished && startup->is_quiescent(); }, "cold fixture startup settles");
		expect(application != nullptr, "cold fixture starts");
		struct held_workspace
		{
			std::function<void()> Release;
			void                  release()
			{
				if (Release) {
					std::exchange(Release, { })();
				}
			}
			~held_workspace() { release(); }
		};
		auto held = held_workspace{ access::hold_agent_workspace_effects(*application) };
		expect(static_cast<bool>(held.Release), "the Agent effect lane can be held");
		application->show();
		auto& window = application->window();
		window.resize(1280, 900);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "cold fixture account connects");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/inline-drawer#22"));
		window.home_open_action()->trigger();
		const auto* const page        = window.overview_page();
		auto* const       setup       = window.agent_setup();
		auto* const       tab         = window.agent_tab();
		const auto        statusPress = aPress == cold_press::status;
		auto* const       press       = page->findChild<QPushButton*>(statusPress ? QStringLiteral("overviewSuggestUpdateFromTarget") : QStringLiteral("overviewUpdateFromTarget"));
		expect(press != nullptr, "the product creates the selected update action");
		wait([&] {
			const auto& shown = setup->projection();
			return window.showing_overview() && press->isVisible() && press->isEnabled()
				&& !shown.Checking && shown.State.AgentCheck.State == core::setup_check_state::passed
				&& shown.State.WorkspaceCheck.State == core::setup_check_state::running;
		},
			 "the authored update action is visible while only the workspace check is running");
		if (statusPress) {
			wait([&] { return press->visibleRegion().contains(press->rect()); }, "the Status action fits in the visible drawer");
			const auto&       projection = *page->projection();
			const auto* const conflict   = page->conflicts_row();
			expect(projection.MergeConflict && projection.MergeConflict->Base == "main"
					   && page->status_drawer()->current_tab() == qt::overview_drawer_tab::status
					   && !page->status_section_header(qt::overview_status_section::attention)->isChecked(),
				   "the authored inline fixture opens Status with an accepted conflict on main and collapsed Merge details");
			expect(conflict && conflict->isVisible() && conflict->parentWidget() == press->parentWidget()
					   && conflict->geometry().bottom() < press->geometry().top() && conflict->nextInFocusChain() == press,
				   "the Status action follows the conflict in the drawer and in keyboard order");
			expect(qt::agent_run_control(*press) && press->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(page->palette()),
				   "the Status action wears the Agent accent");
		}
		expect(access::agent_chat_evidence(*application).Turns == 0U, "the cold fixture has no turns");
		press->click();
		expect(page->status_drawer()->current_tab() == qt::overview_drawer_tab::agent, "the first press opens the Overview Agent tab");
		if (aPress == cold_press::repeated) {
			press->click();
			press->click();
		} else if (aPress == cold_press::closed) {
			wait([&] { return access::epoch_refresh_identity(*application).AcceptedComparison.has_value(); }, "the held check reaches the accepted comparison");
			window.home_action()->trigger();
			auto* const close = window.home_landing()->close_button();
			wait([&] { return close->isVisible() && close->isEnabled(); }, "Home offers Close review");
			close->click();
			wait([&] { return !access::close_review(*application).ReadHoldsPullRequest; }, "the review closes while its workspace check is held");
		} else if (aPress == cold_press::stopped) {
			wait([&] { return access::epoch_refresh_identity(*application).AcceptedComparison.has_value(); }, "the held check reaches the accepted comparison");
			window.open_settings(qt::settings_page::agents_and_context);
			auto* const stop = window.agent_programs()->stop_check_button();
			wait([&] { return stop->isVisible() && stop->isEnabled(); }, "the Agents settings offer Stop");
			stop->click();
		}
		expect(access::agent_chat_evidence(*application).Turns == 0U, "holding the workspace check prevents a send");
		held.release();
		if (aPress == cold_press::closed || aPress == cold_press::stopped) {
			wait([&] {
				return access::agent_invocation_evidence(*application).Quiescent
					&& (aPress == cold_press::closed || setup->projection().State.WorkspaceCheck.State != core::setup_check_state::running);
			},
				 "the abandoned workspace work retires");
			QApplication::processEvents();
			expect(access::agent_chat_evidence(*application).Turns == 0U, "an abandoned press sends no turn when setup settles");
		} else {
			wait([&] {
				return tab->turn_count() > 0U && tab->turn_outcome(tab->turn_count() - 1U) == core::agent_turn_outcome::answered
					&& access::agent_invocation_evidence(*application).Quiescent;
			},
				 "the first update press sends a turn that the fixture answers");
			const auto chat = access::agent_chat_evidence(*application);
			expect(chat.Turns == 1U && chat.LastTurn && tab->turn_count() == 1U, "the cold press sends exactly one turn");
			const auto& prompt = chat.LastTurn->Message;
			expect(prompt.contains(R"("pull_request":"https://github.com/bpr-fixtures/inline-drawer/pull/22")")
					   && prompt.contains(R"("target_ref":"refs/heads/main")") && prompt.contains(R"("head_ref":"refs/heads/inline-drawer")")
					   && prompt.contains(R"("expected_head":"89df5e592e5db97a526a05a445735733395e4832")"),
				   "the deferred prompt names the reviewed fixture and its exact branches and head");
		}
		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "cold fixture shutdown completes");
	}

	void exercise(const bool aViewerAuthors)
	{
		QTemporaryDir directory;
		const auto    root = e2e::private_data_root(directory);
		expect(root.has_value(), "private fixture root");
		auto server                 = std::make_shared<app::detail::review_submission_fixture_state>();
		server->MergeAssessment     = R"({"data":{"repository":{"id":"R_stacked-merge-base","pullRequest":{"id":"PR_1572","headRefOid":"d160ec81ebe09b21894732b84f4f7ce3aea82258","baseRefOid":"67b31e41c9867713d769174f7d631fbddfc05a6e","baseRefName":"feature-a","mergeStateStatus":"DIRTY","reviewDecision":null,"autoMergeRequest":null}}}})";
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = root };
		options.FixtureAgentSession = true;
		if (!aViewerAuthors) {
			options.PullRequestAuthor = application::github_actor_record{ .Actor = application::github_actor_id{ "U_fixture-fork" }, .Login = "fixture-fork" };
		}
		options.ForkHeadRepository     = core::repository_locator{ "fixture-fork", "stacked-merge-base" };
		options.ReviewSubmissionServer = std::move(server);
		auto application               = std::unique_ptr<app::github_application>{ };
		auto finished                  = false;
		auto startup                   = access::start(app::detail::github_public_path_fixture::stacked_merge_base_v1, std::move(options), [&](auto aResult) {
			finished = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return finished && startup->is_quiescent(); }, "fixture startup settles");
		expect(application != nullptr, "fixture starts");
		application->show();
		auto& window = application->window();
		window.resize(1280, 900);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/stacked-merge-base#1572"));
		window.home_open_action()->trigger();
		const auto* page = window.overview_page();
		wait([&] { return window.showing_overview() && page->projection() && page->projection()->MergeConflict; }, "fixture reports a merge-blocking conflict");
		auto* overview   = page->findChild<QPushButton*>(QStringLiteral("overviewUpdateFromTarget"));
		auto* suggestion = page->findChild<QPushButton*>(QStringLiteral("overviewSuggestUpdateFromTarget"));
		expect(overview != nullptr && suggestion != nullptr, "the product creates both placements");
		if (aViewerAuthors) {
			wait([&] { return overview->isVisible() && suggestion->isVisible(); }, "both authored update controls are offered");
			auto* hooks = window.writing_hooks();
			hooks->select_hook(core::writing_hook::review_update_from_target);
			auto* instructions = hooks->findChild<QPlainTextEdit*>(QStringLiteral("writingHookInstructions"));
			expect(instructions->toPlainText().contains(QStringLiteral("fast-forward push")), "the prompt shows the shipped guidance");
			instructions->setPlainText(QStringLiteral("TEAM_UPDATE_GUIDANCE: run only the smoke checks."));
			hooks->findChild<QPushButton*>(QStringLiteral("writingHookSave"))->click();
			auto* source = hooks->findChild<QLabel*>(QStringLiteral("writingHookSource"));
			wait([&] { return source->text().contains(QStringLiteral("Changed from built-in")); }, "replacement guidance is accepted");
			window.open_agent_question();
			auto* tab = window.agent_tab();
			wait([&] { return tab->setup_readiness() == qt::agent_setup_readiness::ready && tab->ready_to_send() && access::agent_chat_evidence(*application).Session.has_value(); }, "fixture Agent conversation is ready");
			window.open_overview_tab(qt::overview_drawer_tab::status);
			expect(suggestion->isVisible(), "the status suggestion remains actionable");
			const auto before = access::agent_chat_evidence(*application).Turns;
			suggestion->click();
			wait([&] { return access::agent_chat_evidence(*application).Turns > before; }, "one press submits to the normal Agent turn");
			const auto chat = access::agent_chat_evidence(*application);
			expect(chat.Turns == before + 1U && chat.LastTurn.has_value(), "exactly one turn was dispatched");
			const auto& message = chat.LastTurn->Message;
			expect(message.contains(R"("target_repository":"https://github.com/bpr-fixtures/stacked-merge-base.git")") && message.contains(R"("target_ref":"refs/heads/feature-a")"), "the actual nondefault branch is the target");
			expect(message.contains(R"("head_repository":"https://github.com/fixture-fork/stacked-merge-base.git")") && message.contains(R"("head_ref":"refs/heads/feature-b")"), "the head's fork is the push destination");
			expect(message.contains(R"("expected_head":"d160ec81ebe09b21894732b84f4f7ce3aea82258")") && message.contains("TEAM_UPDATE_GUIDANCE") && !message.contains("resulting commit"), "the saved replacement rides with the observed head and replaces the default");
			wait([&] { return tab->turn_count() > 0U && tab->turn_outcome(tab->turn_count() - 1U) == core::agent_turn_outcome::answered && access::agent_invocation_evidence(*application).Quiescent; }, "the fixture agent answers and retires");
		} else {
			expect(overview->isHidden() && suggestion->isHidden(), "the reviewer gets neither action on the conflicting PR");
			overview->click();
			suggestion->click();
			expect(access::agent_chat_evidence(*application).Turns == 0U, "even a programmatic press submits no reviewer turn");
		}
		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "fixture shutdown completes");
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	std::cout << "qt-runtime:" << qVersion() << '\n';
	try {
		exercise_cold(cold_press::status);
		std::cout << "cold-status:first-press-passed\n"
				  << std::flush;
		exercise_cold(cold_press::single);
		std::cout << "cold-overview:first-press-passed\n"
				  << std::flush;
		exercise_cold(cold_press::repeated);
		std::cout << "cold-overview:repeated-presses-passed\n"
				  << std::flush;
		exercise_cold(cold_press::closed);
		std::cout << "cold-overview:closed-review-passed\n"
				  << std::flush;
		exercise_cold(cold_press::stopped);
		std::cout << "cold-overview:stopped-setup-passed\n"
				  << std::flush;
		exercise(false);
		exercise(true);
		std::cout << "update-from-target:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "update-from-target: " << aError.what() << '\n';
		return EXIT_FAILURE;
	}
}
