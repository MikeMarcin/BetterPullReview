#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/scratch_directory.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>

namespace {
	using namespace bpr;
	using fixture_access = app::detail::github_application_fixture_access;
	using tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 12000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void run(const app::detail::github_public_path_fixture aFixture)
	{
		const auto root             = tests::make_scratch_directory("bpr-thread-resolution-refresh");
		const auto cleanup          = tests::scratch_guard{ root };
		auto       mutex            = std::mutex{ };
		auto       changed          = std::condition_variable_any{ };
		auto       holdRead         = false;
		auto       entered          = std::atomic_bool{ false };
		auto       owner            = std::unique_ptr<app::github_application>{ };
		auto       options          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = root / "data";
		options.RepeatReadSessions  = true;
		options.ResolvesThreads     = true;
		options.BeforeFilesPage     = [&](const std::stop_token aStop) {
			auto lock = std::unique_lock{ mutex };
			if (holdRead) {
				entered = true;
				changed.wait(lock, aStop, [&] { return !holdRead; });
			}
		};
		auto       ready        = false;
		auto       startup      = fixture_access::start(aFixture, options, [&](auto aResult) {
			ready = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		const auto releaseRead = [&] {
			{
				const auto lock = std::scoped_lock{ mutex };
				holdRead        = false;
			}
			changed.notify_all();
		};
		const auto stop = [&] {
			releaseRead();
			if (owner) {
				fixture_access::hold_review_mutation_terminals(*owner, false);
				owner->begin_shutdown();
				wait_for([&] { return owner->status().ShutdownComplete; }, "shutdown drains all owned effects and terminals");
				owner.reset();
				QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
				QCoreApplication::processEvents();
			}
		};
		try {
			wait_for([&] { return ready && startup->is_quiescent(); }, "fixture startup completes");
			expect(owner != nullptr, "fixture application starts");
			owner->show();
			auto& window = owner->window();
			window.resize(1280, 860);
			window.github_connect_action()->trigger();
			wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
			window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
			window.home_open_action()->trigger();
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready && window.github_refresh_action()->isEnabled(); }, "pull request read completes");
			const auto step = [&] {
				expect(fixture_access::exercise_threaded_reply(*owner) != app::detail::threaded_reply_probe_result::failed, "public threaded course advances");
			};
			wait_for([&] {
				step();
				return fixture_access::threaded_reply_diagnostic(*owner).find("phase=6 body=Agreed, will do. publish=1") != std::string::npos;
			},
					 "a public reply is published");
			wait_for([&] { return fixture_access::review_mutation_evidence(*owner).Replies == 1U
							   && window.conversation_model()->index_for_id("review-comment:fixture-comment-1").isValid() && window.github_refresh_action()->isEnabled(); }, "reply refresh completes before the controlled read");
			{
				const auto lock = std::scoped_lock{ mutex };
				holdRead        = true;
			}
			fixture_access::hold_review_mutation_terminals(*owner, true);
			step();
			wait_for([&] { return entered.load(); }, "second refresh is held at its files read");
			expect(!window.github_refresh_action()->isEnabled(), "a read really is in flight");
			step();
			wait_for([&] { return fixture_access::held_review_mutation_terminals(*owner) == 1U
							   && fixture_access::review_mutation_evidence(*owner).Resolutions == 1U; }, "Resolve is admitted and its provider answer is independently held");
			const auto pending = [&] {
				for (const auto& card : window.conversation_panel()->cards()->cards()) {
					if (card.Id == "review-thread:PRRT_fixture_1") {
						return card.ResolutionPending;
					}
				}
				return false;
			};
			expect(pending(), "the public card awaits its admitted resolution");
			releaseRead();
			wait_for([&] { return window.github_refresh_action()->isEnabled(); }, "the second refresh accepts while the mutation terminal is held");
			expect(pending(), "accepting the read retains the admitted pending change");
			fixture_access::hold_review_mutation_terminals(*owner, false);
			wait_for([&] {
				step();
				return fixture_access::threaded_reply_diagnostic(*owner).find("completions=1 refusals=0 failures=0 resolutions=1") != std::string::npos && !pending();
			},
					 "the original terminal settles exactly once across accepted refresh");
			wait_for([&] { return fixture_access::exercise_threaded_reply(*owner) == app::detail::threaded_reply_probe_result::passed; }, "the composed course reaches its final resolved-thread assertions");
			expect(fixture_access::review_mutation_evidence(*owner).Resolutions == 1U, "no second resolution write is sent");
			stop();
		} catch (...) {
			if (owner) {
				std::cerr << fixture_access::threaded_reply_diagnostic(*owner) << '\n';
			}
			stop();
			throw;
		}
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	auto application = QApplication{ aArgc, aArgv };
	auto consent     = QTimer{ };
	QObject::connect(&consent, &QTimer::timeout, [] {
		if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
			if (auto* button = dialog->button(QMessageBox::Yes)) {
				button->click();
			}
		}
	});
	consent.start(1);
	run(app::detail::github_public_path_fixture::github_com);
	run(app::detail::github_public_path_fixture::ghes_3_18);
	std::cout << "thread-resolution-refresh-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
