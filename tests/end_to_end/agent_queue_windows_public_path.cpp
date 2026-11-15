/// \file
/// \brief RFC-BPR-0126 AC04 ([[BPR-2151]]), Q10 as overridden by the owner:
/// "Queue must be tied to session not active UI windows". Two windows of one
/// profile on the same pull request (returning-review-v1, the scoped writing
/// stub as the installed Claude), through the desktop:
///
/// - messages queued in one window show in the other, in the same order;
/// - a message one window edits shows as being edited in the other, with no
///   controls there, and its edit shows in both on Save;
/// - a removal in one window shows in the other, and the other's later edit
///   on the removed message is refused and says so;
/// - a message typed in the second window joins the one queue;
/// - Resume in either window drains it one turn at a time; closing the window
///   running a turn neither pauses the queue nor stops it: the other window
///   runs what is left, in order.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
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

	void open_review(qt::workbench_window& aWindow, const std::string& aWhich)
	{
		if (!aWindow.github_open_action()->isEnabled()) {
			aWindow.github_connect_action()->trigger();
		}
		wait([&] { return aWindow.github_open_action()->isEnabled(); }, aWhich + " connected");
		aWindow.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		aWindow.home_open_action()->trigger();
		// The review bar shows once the pull request is read. (Only one window
		// holds the review ledger, so only the first offers What changed.)
		if (!e2e::wait_until_plain([&] { return aWindow.review_ask_button()->isVisible() && aWindow.github_open_action()->isEnabled(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aWhich + " shows the pull request: " + aWindow.status_text().toStdString() + " / " + aWindow.github_status_text() };
		}
	}

	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the stub claude");
			check()->click();
			wait(ready, "Check it works passes on the stub claude");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		for (const auto claude : { core::claude_stream_json_profile_id, core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U) }) {
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(claude.value())); index >= 0) {
				selector->setCurrentIndex(index);
				break;
			}
		}
	}

	[[nodiscard]] auto rows_of(qt::workbench_window& aWindow) -> QStringList
	{
		auto labels = QStringList{ };
		for (auto* const label : aWindow.agent_tab()->queue_list()->findChildren<QLabel*>(QStringLiteral("agentQueueLabel"))) {
			labels << label->text();
		}
		return labels;
	}

	[[nodiscard]] auto card_of(qt::workbench_window& aWindow, const QString& aLabel) -> QWidget*
	{
		for (auto* const card : aWindow.agent_tab()->queue_list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow"))) {
			auto* const label = card->findChild<QLabel*>(QStringLiteral("agentQueueLabel"));
			if (label != nullptr && label->text() == aLabel) {
				return card;
			}
		}
		return nullptr;
	}

	auto press_in(qt::workbench_window& aWindow, const QString& aName, const QString& aLabel = { }) -> bool
	{
		auto* const list = aWindow.agent_tab()->queue_list();
		auto* const card = aLabel.isEmpty() ? nullptr : card_of(aWindow, aLabel);
		auto* const in   = card != nullptr ? card : static_cast<QWidget*>(list);
		if (!aLabel.isEmpty() && card == nullptr) {
			return false;
		}
		auto* const button = in->findChild<QPushButton*>(aName);
		if (button == nullptr || !button->isVisibleTo(list)) {
			return false;
		}
		button->click();
		QApplication::processEvents();
		return true;
	}

	void type_and_return(qt::workbench_window& aWindow, const QString& aText)
	{
		auto* const composer = aWindow.side_chat()->composer();
		composer->setPlainText(aText);
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(composer, &press);
		QApplication::processEvents();
	}

	[[nodiscard]] auto questions_of(const qt::agent_tab_view& aTab) -> QStringList
	{
		auto asked = QStringList{ };
		for (std::size_t turn = 0U; turn < aTab.turn_count(); ++turn) {
			asked << aTab.turn_question(turn);
		}
		return asked;
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 2) {
		std::cerr << "usage: agent_queue_windows_public_path STUB\n";
		return EXIT_FAILURE;
	}
	const auto    stub = std::filesystem::path{ argv[1] };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	auto search = e2e::stub_claude_search(std::filesystem::path{ data.path().toStdString() } / "stub-claude-search", stub);
	if (!search) {
		std::cerr << "stub claude search refused\n";
		return EXIT_FAILURE;
	}
	app::github_application*              owner = nullptr;
	std::unique_ptr<app::profile_desktop> desktop;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.EpochPhase            = app::detail::github_epoch_fixture_phase::single;
		options.ClaudeAgentExecutable = stub;
		options.ClaudeSearch          = search;
		// The second window reads the pull request again.
		options.RepeatReadSessions = true;
		auto terminal              = false;
		desktop                    = access::start_desktop(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](app::profile_desktop_startup_result aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
		wait([&] { return terminal; }, "the profile owner starts");
		expect(owner != nullptr, "the profile owner is admitted");
		owner->show();
		wait([&] { return owner->windows().size() == 1U; }, "the first window is admitted");
		const auto firstContext = owner->windows().front();
		auto*      first        = owner->window(firstContext);
		first->resize(1440, 960);
		open_review(*first, "the first window");
		agent_ready(*first);

		// The second window, on the same pull request: the same session.
		first->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
		wait([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
		const auto contexts      = owner->windows();
		const auto secondContext = contexts.front() == firstContext ? contexts.back() : contexts.front();
		auto*      second        = owner->window(secondContext);
		second->resize(1440, 960);
		open_review(*second, "the second window");
		agent_ready(*second);
		const auto idle = [&] { return access::agent_invocation_evidence(*owner).Quiescent && first->agent_tab()->state() != qt::agent_tab_state::working && second->agent_tab()->state() != qt::agent_tab_state::working; };

		// Messages waiting in the first window, the queue paused.
		auto* const ask = first->review_ask_button();
		wait([&] { return ask->isVisible(); }, "the review bar's Ask shows");
		const auto quick = [&](const char* aId, const char* aScope) {
			ask->refresh_menu();
			auto* const action = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_%1_%2").arg(QString::fromLatin1(aId), QString::fromLatin1(aScope)));
			expect(action != nullptr, std::string{ "the Ask menu lists " } + aId);
			action->trigger();
			QApplication::processEvents();
		};
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return first->go_to_file_line(path, 12U, false) && first->active_document_path() == path; }, "the file opens");
		quick("agent-tab.explain-file", "file");
		quick("agent-tab.explain-file", "file");
		wait([&] { return first->agent_tab()->queue_list()->isVisible(); }, "a waiting message shows in the first window's pending list");
		expect(press_in(*first, QStringLiteral("agentQueuePauseResume")), "Pause queue in the first window");
		if (!e2e::wait_until_plain(idle, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the running ask finishes: " + first->agent_panel()->status_text().toStdString() + " / " + rows_of(*first).join(QLatin1Char('|')).toStdString() };
		}
		type_and_return(*first, QStringLiteral("First follow-up"));
		type_and_return(*first, QStringLiteral("Second follow-up"));
		const auto firstRows = rows_of(*first);
		expect(firstRows.contains(QStringLiteral("First follow-up")) && firstRows.contains(QStringLiteral("Second follow-up")), "typed in the first window, queued: " + firstRows.join(QLatin1Char('|')).toStdString());
		wait([&] { return rows_of(*second) == rows_of(*first) && second->agent_tab()->queue_list()->isVisible(); },
			 "the second window shows the same queue, in the same order: " + rows_of(*second).join(QLatin1Char('|')).toStdString());

		// An edit in the second window holds the message in both.
		expect(press_in(*second, QStringLiteral("agentQueueEdit"), QStringLiteral("First follow-up")), "Edit in the second window");
		wait([&] { return rows_of(*first).contains(QStringLiteral("First follow-up (being edited)")); }, "the first window shows it being edited: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
		auto* const heldThere = card_of(*first, QStringLiteral("First follow-up (being edited)"));
		expect(heldThere != nullptr && heldThere->findChild<QPushButton*>(QStringLiteral("agentQueueEdit")) == nullptr && heldThere->findChild<QPushButton*>(QStringLiteral("agentQueueRemove")) == nullptr,
			   "with no Edit or Remove there while it is edited");
		// With `BPR_QUEUE_WINDOWS_RENDER_ROOT` set, each window's Agent tab is
		// saved there (the owner's look; native with BPR_NATIVE_LOOK=1).
		if (const auto root = qEnvironmentVariable("BPR_QUEUE_WINDOWS_RENDER_ROOT"); !root.isEmpty()) {
			QApplication::processEvents();
			static_cast<void>(first->agent_tab()->grab().save(root + QStringLiteral("/first-window.png")));
			static_cast<void>(second->agent_tab()->grab().save(root + QStringLiteral("/second-window.png")));
		}
		auto* const field = second->agent_tab()->queue_list()->findChild<QLineEdit*>(QStringLiteral("agentQueueEditField"));
		expect(field != nullptr && field->text() == QStringLiteral("First follow-up"), "edited in place in the second window");
		field->setText(QStringLiteral("First follow-up, edited"));
		expect(press_in(*second, QStringLiteral("agentQueueSave")), "Save in the second window");
		wait([&] { return rows_of(*first).contains(QStringLiteral("First follow-up, edited")) && rows_of(*second) == rows_of(*first); }, "the edit shows in both");

		// A removal in the first window crossing the second's press on the
		// same message: the later press is refused and says so.
		auto* const removedCard = card_of(*second, QStringLiteral("Second follow-up"));
		auto* const firstCard   = card_of(*first, QStringLiteral("Second follow-up"));
		expect(removedCard != nullptr && firstCard != nullptr, "both windows show the message");
		const auto stale = QPointer<QPushButton>{ removedCard->findChild<QPushButton*>(QStringLiteral("agentQueueRemove")) };
		firstCard->findChild<QPushButton*>(QStringLiteral("agentQueueRemove"))->click();
		expect(!stale.isNull(), "the second window's press is still on its way");
		stale->click();
		QApplication::processEvents();
		expect(second->agent_panel()->status_text().contains(QStringLiteral("Another window changed this queue first")),
			   "the press that crossed the first window's removal is refused and says so: " + second->agent_panel()->status_text().toStdString());
		wait([&] { return !rows_of(*second).contains(QStringLiteral("Second follow-up")) && rows_of(*first) == rows_of(*second); }, "removed in both windows");

		// Typed in the second window, it joins the one queue.
		type_and_return(*second, QStringLiteral("Third follow-up"));
		wait([&] { return rows_of(*first).contains(QStringLiteral("Third follow-up")) && rows_of(*first) == rows_of(*second); }, "typed in the second window, it shows in the first");

		// Resume: the first window runs the next message; closing it while it
		// runs neither pauses nor stops the queue, and the second window runs
		// the rest in order.
		const auto before = second->agent_tab()->turn_count();
		expect(press_in(*second, QStringLiteral("agentQueuePauseResume")), "Resume in the second window");
		wait([&] { return first->agent_tab()->state() == qt::agent_tab_state::working || second->agent_tab()->state() == qt::agent_tab_state::working; }, "a turn starts");
		const auto ranInFirst = first->agent_tab()->state() == qt::agent_tab_state::working;
		expect(ranInFirst, "the first free window showing the session runs it");
		first->close();
		wait([&] { return owner->window(firstContext) == nullptr && owner->windows().size() == 1U; }, "the first window closes");
		wait([&] {
			const auto asked = questions_of(*second->agent_tab());
			return asked.contains(QStringLiteral("First follow-up, edited")) && asked.contains(QStringLiteral("Third follow-up")) && idle() && !second->agent_tab()->queue_list()->isVisible();
		},
			 "the second window runs what is left: " + questions_of(*second->agent_tab()).join(QLatin1Char('|')).toStdString());
		const auto asked = questions_of(*second->agent_tab());
		expect(second->agent_tab()->turn_count() >= before + 2U && asked.indexOf(QStringLiteral("First follow-up, edited")) < asked.indexOf(QStringLiteral("Third follow-up")),
			   "in the order the list showed: " + asked.join(QLatin1Char('|')).toStdString());

		second->findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
		wait([&] { return desktop->is_quiescent(); }, "Quit drains the last session and the profile's services");
		desktop.reset();
	} catch (const std::exception& aError) {
		std::cerr << "agent-queue-windows:fail: " << aError.what() << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "agent-queue-windows:pass\n";
	return EXIT_SUCCESS;
}
