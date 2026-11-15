/// \file
/// \brief [[BPR-2153]], RFC-BPR-0126 AC05 through the window, on the
/// profile's session queue host. `sent`: the steerable fixture agent runs a
/// slow Agent tab turn; a follow-up sent with Follow-up behaviour set to
/// Steer goes into that running turn, which takes it after its current tool
/// call and answers with it; it leaves the queue, no second turn starts, and
/// the panel says it was steered. `unavailable`: the turn's agent names no
/// version; a queued message's Steer says why it can't steer, the message
/// goes next, and it runs as its own turn. No real agent, no network, no cost.
///
///     agent_steer_public_path FIXTURE_AGENT_FOLDER sent|unavailable

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// Types `aText` and presses Return, as a person sends a follow-up.
	void press_return(QPlainTextEdit& aComposer, const QString& aText)
	{
		aComposer.setPlainText(aText);
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(&aComposer, &press);
		QApplication::processEvents();
	}

	/// Lets the window run for `aMilliseconds`.
	void settle_for(const int aMilliseconds)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot || argc < 3) {
		std::cerr << "usage: agent_steer_public_path FIXTURE_AGENT_FOLDER sent|unavailable\n";
		return EXIT_FAILURE;
	}
	const auto                               unavailable = std::string_view{ argv[2] } == "unavailable";
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.FixtureAgentSession = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::nested_folders_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		wait([&] { return window.open_file_in_review(QStringLiteral("src/engine/renderer.cpp")); }, "a file opens");
		window.open_agent_question();
		auto* const tab = window.agent_tab();
		wait([tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; }, "the fixture agent is set up");

		// A slow turn: the fixture agent reads a file a second for twenty.
		// Unavailable: that turn's agent names no version, as an older Claude
		// Code might, so it can't be steered.
		auto* const chat  = access::side_chat_panel(*application);
		const auto  turns = tab->turn_count();
		chat->composer()->setPlainText(unavailable ? QStringLiteral("Take your time and read the renderer. FIXTURE_UNVERSIONED") : QStringLiteral("Take your time and read the renderer."));
		QApplication::processEvents();
		chat->send_action()->trigger();
		wait([tab] { return tab->state() == qt::agent_tab_state::working; }, "the turn runs");
		settle_for(2500);
		expect(tab->state() == qt::agent_tab_state::working, "the turn is still running when the follow-up is sent");
		const auto refusals = window.agent_panel()->refusal_count();

		if (!unavailable) {
			// Follow-up behaviour: Steer. Return sends the follow-up into the run.
			tab->set_follow_up_steer(true);
			press_return(*chat->composer(), QStringLiteral("Look at the swap chain instead."));
			wait([&] { return window.agent_panel()->refusal_count() > refusals; }, "the panel says what became of the follow-up");
			expect(window.agent_panel()->last_refusal().startsWith(QStringLiteral("Steered into the running turn")), "it was steered: " + window.agent_panel()->last_refusal().toStdString());
			wait([&] { return tab->turn_count() > turns && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent; }, "the steered turn ends");
			const auto answer = tab->turn_answer(tab->turn_count() - 1U);
			expect(answer.contains(QStringLiteral("Steered after step")) && answer.contains(QStringLiteral("Look at the swap chain instead.")), "the running turn took it after a tool call: " + answer.toStdString());
			// Nothing was left to send: no second turn starts.
			settle_for(1500);
			expect(tab->turn_count() == turns + 1U && access::agent_invocation_evidence(*application).Quiescent, "the steered message left the queue; no second turn ran");
		} else {
			// Queued while the turn runs, then the waiting message's Steer.
			press_return(*chat->composer(), QStringLiteral("Look at the swap chain instead."));
			auto* steer = static_cast<QPushButton*>(nullptr);
			wait([&] {
				steer = tab->findChild<QPushButton*>(QStringLiteral("agentQueueSteer"));
				return steer != nullptr;
			},
				 "the queued message shows its Steer");
			steer->click();
			wait([&] { return window.agent_panel()->refusal_count() > refusals; }, "the panel says what became of the Steer");
			const auto why = window.agent_panel()->last_refusal();
			// The reason is the agent's own: the version it hasn't named, or
			// (a fixture session without a kept agent session, today) no kept
			// session. Either way it is said, with where the message goes.
			const auto reason = why.left(why.indexOf(QStringLiteral(" The message goes next")));
			expect(why.endsWith(QStringLiteral("The message goes next, right after this answer.")) && !reason.isEmpty() && !why.startsWith(QStringLiteral("Steered")),
				   "it says why it can't steer and that the message goes next: " + why.toStdString());
			// It was never sent into the running turn: it runs as the next turn.
			wait([&] { return tab->turn_count() >= turns + 2U && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent; }, "the message runs next, as its own turn");
			expect(!tab->turn_answer(turns).contains(QStringLiteral("Steered after step")), "the running turn was not steered: " + tab->turn_answer(turns).toStdString());
		}
		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << (unavailable ? "agent-steer-unavailable:pass\n" : "agent-steer-sent:pass\n");
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "agent-steer: " << aError.what() << '\n';
		if (application) {
			const auto* const tab = application->window().agent_tab();
			std::cerr << "agent-panel: " << application->window().agent_panel()->status_text().toStdString() << "\nlast refusal: " << application->window().agent_panel()->last_refusal().toStdString() << "\nturns: " << tab->turn_count()
					  << "\nlast: " << (tab->turn_count() > 0U ? tab->turn_answer(tab->turn_count() - 1U).toStdString() : std::string{ }) << '\n';
			application->begin_shutdown();
		}
		return EXIT_FAILURE;
	}
}
