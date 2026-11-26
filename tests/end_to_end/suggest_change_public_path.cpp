/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change through the shipped
/// window, on nested-folders-v1 with the scoped writing stub as the installed
/// Claude. The draft comment's words choose the stub's reply.
///
/// Courses, by the first argument:
///
/// - `suggest STUB`: a draft comment on a head line in a changed region
///   offers Suggest the Change on its field's right-click menu, and a line the
///   draft is not on offers nothing; opening the pull request and writing the
///   comment launch no agent. A press sends one fresh, tool-free call through
///   the comment workshop's seventh operation with exactly the draft's lines
///   and its own prompt as the system prompt, as an Agent tab turn. An
///   applying reply opens in the head pane, and the draft is unchanged until
///   Keep change adds the block with its words as written; Undo restores the
///   draft exactly. A reply whose stated text is one byte off and a reply
///   that is a sentence are refused with their reasons, never opened in the
///   code, and the reply reads as text. Nothing reaches GitHub.
/// - `suggest STUB --attended`: the applied change, then the window stays open
///   for a person to look; closing it ends the course.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/suggest_change_control.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	constexpr auto changed_path = std::string_view{ "src/engine/renderer.cpp" };

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

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
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

	[[nodiscard]] auto card_child_action(const qt::workbench_window& aWindow, const QString& aName) -> QAction*
	{
		auto* const card = aWindow.conversation_panel()->cards()->open_card();
		return card != nullptr ? card->findChild<QAction*>(aName) : nullptr;
	}

	template <typename T>
	[[nodiscard]] auto card_child(const qt::workbench_window& aWindow, const QString& aName) -> T*
	{
		auto* const card = aWindow.conversation_panel()->cards()->open_card();
		return card != nullptr ? card->findChild<T*>(aName) : nullptr;
	}

	[[nodiscard]] auto phase(const qt::workbench_window& aWindow) -> qt::agent_suggestion_phase { return aWindow.agent_suggestion_presentation_now().Phase; }

	[[nodiscard]] auto body(const qt::workbench_window& aWindow) -> std::string { return aWindow.review_editor()->body_editor()->toPlainText().toStdString(); }
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 3 || std::string_view{ argv[1] } != "suggest") {
		std::cerr << "usage: suggest_change_public_path suggest STUB [--attended]\n";
		return EXIT_FAILURE;
	}
	const auto    stub     = std::filesystem::path{ argv[2] };
	const auto    attended = argc > 3 && std::string_view{ argv[3] } == "--attended";
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
	std::unique_ptr<app::github_application> application;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.ClaudeAgentExecutable = stub;
		options.ClaudeSearch          = search;
		auto terminal                 = false;
		auto startup                  = access::start(app::detail::github_public_path_fixture::nested_folders_v1,
													  std::move(options), [&](auto aResult) { terminal = true; if (aResult) { application = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto launchesAt = [&] { return access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations; };
		const auto path       = QString::fromUtf8(changed_path.data(), static_cast<qsizetype>(changed_path.size()));
		wait([&] { return window.open_file_in_review(path) && window.active_document_path() == path; }, "the file opens in Review");
		agent_ready(window);
		auto* const tab = window.agent_tab();

		// A head line the pull request adds: in a changed region.
		wait([&] { return window.comparison_surface()->current_result() != nullptr; }, "the comparison loads");
		const auto  result  = window.comparison_surface()->current_result();
		const auto& segment = std::ranges::find_if(result->Alignment.Segments, [](const core::alignment_segment& aSegment) {
			return (aSegment.Kind == core::alignment_kind::right_only || aSegment.Kind == core::alignment_kind::replacement) && aSegment.Right.Begin < aSegment.Right.End;
		});
		expect(segment != result->Alignment.Segments.end(), "the file has a line the pull request adds");
		const auto line   = segment->Right.Begin;
		auto*      editor = window.review_editor();
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, line), "the line can be selected");
		wait([&] { return editor->shown_draft() && editor->shown_draft()->Anchor.Line == line && editor->shown_draft()->Anchor.Side == core::diff_side::right; }, "a draft is offered on the line");
		window.add_comment_action()->trigger();
		wait([&] { return window.conversation_panel()->cards()->writing_draft() && window.conversation_panel()->cards()->draft_editor() != nullptr; }, "the draft is written in the drawer");
		const auto mutation      = access::review_mutation_evidence(*application);
		const auto setupLaunches = launchesAt();
		const auto turnsBefore   = tab->turn_count();

		// AC01. Offered on the draft's field menu, where its lines take a suggestion.
		auto* const field = window.conversation_panel()->cards()->draft_editor();
		field->setPlainText(QStringLiteral("STUB_APPLY: note this line."));
		wait([&] { return editor->shown_draft() && editor->shown_draft()->Body == "STUB_APPLY: note this line." && !editor->rejected_candidate(); }, "the words are the draft's");
		auto* const suggest = card_child_action(window, QStringLiteral("threadDraftSuggestChange"));
		expect(suggest != nullptr && suggest->isVisible() && suggest->isEnabled() && window.agent_suggestion_presentation_now().Offered, "Suggest the Change is offered on the draft");
		expect(launchesAt() == setupLaunches, "writing a comment launches no agent");
		const auto before = body(window);

		// AC02, AC05. The press: one call, its own prompt, exactly the lines.
		suggest->trigger();
		wait([&] { return phase(window) == qt::agent_suggestion_phase::proposed || phase(window) == qt::agent_suggestion_phase::refused; }, "the agent answers");
		expect(phase(window) == qt::agent_suggestion_phase::proposed, "the applying reply is accepted: " + window.agent_suggestion_presentation_now().Line.toStdString());
		expect(launchesAt() == setupLaunches + 1U, "one press, one call");
		expect(tab->turn_count() == turnsBefore + 1U && tab->turn_suggest_change(tab->turn_count() - 1U) == path, "the press is a turn in the Agent tab's history, about the file");
		auto* const head = window.comparison_surface()->head_editor();
		expect(head != nullptr, "it opens in the head pane");
		const auto& lines = head->model().state().Lines;
		expect(line < lines.size() && lines[line].Content.find("SUGGEST_ECHO system=1") != std::string::npos, "the agent's line is in the code, and its prompt was the system prompt");
		expect(body(window) == before, "the draft comment is unchanged until it is kept");
		const auto plan = window.agent_suggestion_presentation_now();
		std::cout << "suggest-change: proposed line=" << line + 1U << " panel=\"" << plan.Line.toStdString() << "\"\n";

		if (attended) {
			window.setWindowTitle(QStringLiteral("Suggest the change — synthetic data — close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "suggest-change-attended:ready\n"
					  << std::flush;
			const auto exit = QApplication::exec();
			shutdown(*application);
			return exit;
		}

		// AC04. Keep change adds the block; the words stay as written.
		auto* const first = card_child<QAbstractButton>(window, QStringLiteral("suggestChangeFirst"));
		expect(first != nullptr && first->isVisible() && first->text() == QStringLiteral("Keep change"), "Keep change is offered");
		first->click();
		wait([&] { return core::split_suggestion_body(body(window)).has_value() && !editor->rejected_candidate(); }, "the block is in the draft");
		const auto split = core::split_suggestion_body(body(window));
		expect(split->Prose == "STUB_APPLY: note this line." && !split->Block.empty() && split->Block.front().find("SUGGEST_ECHO") != std::string::npos,
			   "the draft holds its words as written and the agent's block");
		expect(phase(window) == qt::agent_suggestion_phase::kept && first->text() == QStringLiteral("Undo"), "Undo takes Keep change's place");
		// One Undo restores the draft as it was.
		first->click();
		wait([&] { return body(window) == before && !editor->rejected_candidate(); }, "Undo restores the draft exactly");
		expect(phase(window) == qt::agent_suggestion_phase::idle, "and the panel closes");

		// AC03. A stated current text one byte off is refused, never shown.
		const auto refusedCourse = [&](const QString& aWords, const QString& aReason) {
			window.conversation_panel()->cards()->draft_editor()->setPlainText(aWords);
			wait([&] { return editor->shown_draft() && QString::fromStdString(editor->shown_draft()->Body) == aWords && !editor->rejected_candidate(); }, "the words are the draft's");
			const auto launches = launchesAt();
			auto*      again    = card_child_action(window, QStringLiteral("threadDraftSuggestChange"));
			expect(again != nullptr && again->isEnabled(), "Suggest the Change is offered again");
			again->trigger();
			wait([&] { return phase(window) == qt::agent_suggestion_phase::refused; }, "the reply is refused");
			const auto shown = window.agent_suggestion_presentation_now();
			expect(launchesAt() == launches + 1U, "one call");
			expect(shown.Line.contains(aReason), "the panel says which check failed: " + shown.Line.toStdString());
			auto* const head2 = window.comparison_surface()->head_editor();
			expect(head2 == nullptr || head2->model().state().Lines[line].Content.find("SUGGEST_ECHO") == std::string::npos, "it is never shown as a suggestion");
			expect(QString::fromStdString(body(window)) == aWords, "the draft is unchanged");
			auto* const read = card_child<QAbstractButton>(window, QStringLiteral("suggestChangeFirst"));
			auto* const view = card_child<QPlainTextEdit>(window, QStringLiteral("suggestChangeReply"));
			expect(read != nullptr && read->text() == QStringLiteral("Show reply") && view != nullptr, "the reply can be read");
			read->click();
			QApplication::processEvents();
			expect(view->isVisible() && view->toPlainText() == shown.Reply && !shown.Reply.isEmpty(), "as plain text");
			card_child<QAbstractButton>(window, QStringLiteral("suggestChangeSecond"))->click();
			QApplication::processEvents();
			expect(phase(window) == qt::agent_suggestion_phase::idle, "Dismiss closes the panel");
			std::cout << "suggest-change: refused \"" << shown.Line.toStdString() << "\"\n";
		};
		refusedCourse(QStringLiteral("STUB_WRONG_BYTE: note this line."), QStringLiteral("differs from the head"));
		refusedCourse(QStringLiteral("STUB_PROSE: note this line."), QStringLiteral("was not a change to these lines"));

		// AC04. Nothing reached GitHub.
		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations, "nothing reached GitHub");
		shutdown(*application);
		std::cout << "suggest-change:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "suggest-change: " << aError.what() << '\n';
		if (application) {
			auto& window = application->window();
			std::cerr << "github-status: " << window.github_status_text() << "\nagent-panel: " << window.agent_panel()->status_text().toStdString()
					  << "\npanel: " << window.agent_suggestion_presentation_now().Line.toStdString() << "\nreply: " << window.agent_suggestion_presentation_now().Reply.toStdString() << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
