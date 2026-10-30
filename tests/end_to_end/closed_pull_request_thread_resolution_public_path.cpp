// [[BPR-1826]]. A closed or merged pull request's threads resolve and
// unresolve through the composed application, as GitHub allows: the fixture
// reads the pull request closed (or merged), a write connection is composed
// for it that resolves threads and offers nothing else, and Resolve, then
// Unresolve, on the thread's open card in Discuss reach the mutation port and
// come back in the thread's state.

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

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	using fixture_access = app::detail::github_application_fixture_access;
	using tests::expect;

	constexpr auto thread_identity = std::string_view{ "review-thread:PRRT_fixture_1" };

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void run(const app::detail::github_public_path_fixture aFixture, const application::github_pull_request_state aState)
	{
		const auto root             = tests::make_scratch_directory("bpr-closed-thread-resolution");
		const auto cleanup          = tests::scratch_guard{ root };
		auto       owner            = std::unique_ptr<app::github_application>{ };
		auto       options          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = root / "data";
		options.RepeatReadSessions  = true;
		options.ResolvesThreads     = true;
		// Every other change is served, so the course shows the closed pull
		// request's connection offers none of them.
		options.ServesReactions   = true;
		options.EditsDescription  = true;
		options.EditsComments     = true;
		options.RequestsReviewers = true;
		options.PullRequestState  = aState;
		auto       ready          = false;
		auto       startup        = fixture_access::start(aFixture, options, [&](auto aResult) {
			ready = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		const auto stop           = [&] {
			if (owner) {
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
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready && window.github_refresh_action()->isEnabled(); },
					 "the closed pull request's read completes");
			wait_for([&] { return fixture_access::thread_resolution_evidence(*owner).HostComposed; }, "a write connection is composed for the closed pull request");
			const auto composed = fixture_access::thread_resolution_evidence(*owner);
			expect(!composed.BindingOpen, "its binding says the pull request is not open");
			expect(composed.ResolvesThreads, "it resolves threads");
			expect(!composed.Reacts && !composed.EditsDescription && !composed.EditsComments && !composed.RequestsReviewers,
				   "and offers no reaction, description or comment edit, or reviewer request, though the provider serves them");

			auto* const cards = window.conversation_panel() != nullptr ? window.conversation_panel()->cards() : nullptr;
			auto* const model = window.conversation_model();
			expect(cards != nullptr && model != nullptr, "the conversation surfaces exist");
			expect(!cards->comment_edit_offered(), "Edit is not offered on a comment of the closed pull request");
			window.choose_modality(qt::review_modality::discuss);
			QCoreApplication::processEvents();
			wait_for([&] { return model->index_for_id(thread_identity).isValid() && cards->resolution_refusal() == core::thread_resolution_refusal::none; },
					 "the thread is listed and nothing refuses its resolution");
			const auto press = [&](const QString& aText) {
				cards->open_thread(std::string{ thread_identity });
				QCoreApplication::processEvents();
				auto* const card   = cards->open_card();
				auto* const button = card != nullptr ? card->findChild<QAbstractButton*>(QStringLiteral("threadResolve")) : nullptr;
				if (button == nullptr || !button->isEnabled() || button->accessibleName() != aText) {
					return false;
				}
				button->click();
				return true;
			};
			const auto resolved = [&] {
				const auto index = model->index_for_id(thread_identity);
				return index.isValid() && index.data(qt::github_conversation_model::resolved_role).toBool();
			};
			expect(!resolved(), "the thread is read open");
			expect(press(QStringLiteral("Resolve conversation")), "the open card offers Resolve conversation on the closed pull request");
			wait_for([&] { return fixture_access::thread_resolution_evidence(*owner).Completions == 1U && resolved(); }, "the thread resolves through the port");
			expect(fixture_access::review_mutation_evidence(*owner).Resolutions == 1U, "exactly one resolution was asked of the provider");
			// Pressed once: the condition is asked again after the wait.
			auto reopenPressed = false;
			wait_for([&] { return reopenPressed = reopenPressed || press(QStringLiteral("Unresolve conversation")); }, "the open card offers Unresolve conversation");
			wait_for([&] { return fixture_access::thread_resolution_evidence(*owner).Completions == 2U && !resolved(); }, "the thread reopens through the port");
			const auto settled = fixture_access::thread_resolution_evidence(*owner);
			expect(settled.Refusals == 0U && settled.Failures == 0U && settled.Gestures == 2U, "neither gesture was refused or failed");
			const auto mutations = fixture_access::review_mutation_evidence(*owner);
			expect(mutations.Resolutions == 2U && mutations.Submissions == 0U && mutations.Replies == 0U && mutations.Reactions == 0U
					   && mutations.DescriptionEdits == 0U && mutations.CommentEdits == 0U && mutations.ReviewerWrites == 0U,
				   "the provider was asked two resolutions and nothing else");
			stop();
		} catch (...) {
			stop();
			throw;
		}
	}
} // namespace

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
	for (const auto fixture : { app::detail::github_public_path_fixture::github_com, app::detail::github_public_path_fixture::ghes_3_18 }) {
		for (const auto state : { application::github_pull_request_state::closed, application::github_pull_request_state::merged }) {
			run(fixture, state);
		}
	}
	std::cout << "closed-thread-resolution-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
