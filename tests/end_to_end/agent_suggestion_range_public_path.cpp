/// \file
/// \brief [[BPR-2298]] (owner, 2026-11-19: "the comments themselves are only
/// on one line instead of the entire block that needs to be replaced by the
/// suggestion"). Through the shipped window on nested-folders-v1, with the
/// suggestions fixture agent as the only Codex: a finding that names one line
/// while its suggestion block rewrites that line and carries the next one is
/// drafted from its card on both lines, its card says so, and Add single
/// comment publishes it as one multi-line comment, `start_line` 12 to `line`
/// 13 on `RIGHT`, so GitHub applying the suggestion changes the width and
/// writes no line twice. The fixture GitHub records what it received.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/core/agent_suggestion_range.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
	}

	/// The Agent tab's setup finds the fixture `codex` and its Check passes,
	/// as a person's does.
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
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the fixture codex");
			check()->click();
			wait(ready, "Check it works passes on the fixture codex");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		wait([selector] { return selector->findData(QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value())) >= 0; }, "the fixture codex is offered");
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value())));
	}

	[[nodiscard]] auto card_with(const qt::agent_tab_view& aTab, const std::size_t aTurn, const QString& aClaim) -> QWidget*
	{
		for (auto* const card : aTab.turn_cards(aTurn)) {
			for (auto* const widget : card->findChildren<QWidget*>(QStringLiteral("agentFindingClaim"))) {
				const auto* const text = dynamic_cast<const qt::markdown_body_view*>(widget);
				if (text != nullptr && text->original_body().find(aClaim.toStdString()) != std::string::npos) {
					return card;
				}
			}
		}
		return nullptr;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 2) {
		std::cerr << "usage: agent_suggestion_range_public_path CODEX\n";
		return EXIT_FAILURE;
	}
	const auto    stub = std::filesystem::path{ argv[1] };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                 = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.CodexAgentExecutable = stub;
		options.CodexSearch          = app::detail::claude_search_environment{ .PathVariable = stub.parent_path().string() };
		auto terminal                = false;
		auto startup                 = access::start(app::detail::github_public_path_fixture::nested_folders_v1,
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
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return !window.showing_home() && window.showing_overview() && access::agent_invocation_evidence(*application).Quiescent; }, "the pull request opens on Overview");
		agent_ready(window);
		wait([&] { return window.open_file_in_review(path) && window.active_document_path() == path && window.comparison_surface() != nullptr && window.comparison_surface()->current_result() != nullptr; },
			 "the file opens in Review");
		auto* const surface = window.comparison_surface();
		auto* const tab     = window.agent_tab();
		const auto  before  = access::review_mutation_evidence(*application);

		// The agent names line 12 only; its block rewrites 12 and carries 13.
		const auto turn = tab->turn_count();
		wait([&] {
			if (tab->turn_count() == turn && tab->state() != qt::agent_tab_state::working) {
				window.agent_tab()->question_editor()->setPlainText(QStringLiteral("suggestion-range probe: review resize"));
				tab->ask_button()->click();
			}
			return tab->turn_count() == turn + 1U;
		},
			 "the probe is sent");
		wait([&] { return tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent && !tab->turn_answer(turn).isEmpty(); }, "the probe is answered");
		auto* const card = card_with(*tab, turn, QStringLiteral("accepts a negative width"));
		expect(card != nullptr, "the finding is a card under the answer: cards=" + std::to_string(tab->turn_cards(turn).size()));
		tab->refresh();
		auto* const draft = card->findChild<QAbstractButton*>(QStringLiteral("agentFindingDraftComment"));
		expect(draft != nullptr && draft->isEnabled(), "the card offers Draft");
		draft->click();
		auto* const state = card->findChild<QLabel*>(QStringLiteral("agentFindingState"));
		wait([&] { return state->text().startsWith(QStringLiteral("Drafted")) || state->text().startsWith(QStringLiteral("Not")); }, "Draft drafts the finding");
		std::cout << "suggestion-range: card=\"" << state->text().toStdString() << "\"\n";
		expect(state->text().startsWith(QStringLiteral("Drafted on lines 12–13, the lines its suggestion replaces.")), "the card says the draft is on lines 12 to 13");
		auto* const editor = window.review_editor();
		wait([&] { const auto& shown = editor->shown_draft(); return shown && !shown->Body.empty(); }, "the draft is shown");
		const auto& shown = *editor->shown_draft();
		std::cout << "suggestion-range: draft anchor=" << shown.Anchor.Line << " start=" << (shown.StartLine ? std::to_string(*shown.StartLine) : std::string{ "-" }) << '\n';
		expect(shown.Anchor.Side == core::diff_side::right && shown.Anchor.Line == 12U && shown.StartLine == std::optional<std::uint32_t>{ 11U },
			   "the draft is one range from line 12 to line 13, anchored on its last line");
		expect(shown.Body.find("```suggestion\n\tWidth  = std::max(aWidth, 0);\n\tHeight = aHeight;\n```") != std::string::npos, "the draft carries the agent's suggestion block as written");

		// Add single comment, as the person would.
		expect(editor->publish_action()->isEnabled(), "Publish is offered");
		editor->publish_action()->trigger();
		wait([&] { return access::review_mutation_evidence(*application).NativePosts == before.NativePosts + 1U; }, "the comment is written once");
		const auto sent = access::review_mutation_evidence(*application);
		std::cout << "suggestion-range: published side=" << sent.LastSide << " line=" << sent.LastLine.value_or(0U) << " start=" << sent.LastStartLine.value_or(0U)
				  << " startSide=" << sent.LastStartSide.value_or("-") << '\n';
		expect(sent.LastPath == path.toStdString() && !sent.LastFile && sent.LastSide == "RIGHT" && sent.LastLine == std::optional<std::uint32_t>{ 13U }
				   && sent.LastStartLine == std::optional<std::uint32_t>{ 12U } && sent.LastStartSide == std::optional<std::string>{ "RIGHT" },
			   "GitHub receives start_line 12 to line 13, both RIGHT");

		// What GitHub makes of the file when the suggestion is applied on the
		// lines it received: the width clamped, nothing written twice.
		const auto& head  = *surface->current_result()->RightDocument;
		const auto  lines = core::suggestion_source_lines(head, 0U, static_cast<std::uint32_t>(head.Lines.size()) - 1U);
		const auto  block = core::single_suggestion_lines(sent.LastBody);
		expect(lines.has_value() && block.has_value(), "the head and the published block are read");
		auto applied = std::vector<std::string>(lines->begin(), lines->begin() + (*sent.LastStartLine - 1U));
		applied.insert(applied.end(), block->begin(), block->end());
		applied.insert(applied.end(), lines->begin() + *sent.LastLine, lines->end());
		auto expected = *lines;
		expected[11]  = "\tWidth  = std::max(aWidth, 0);";
		expect(applied == expected, "applying the suggestion on the published lines clamps the width and writes no line twice");
		std::cout << "suggestion-range: applied=pass\n";
		shutdown(*application);
		std::cout << "suggestion-range:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "suggestion-range: " << aError.what() << '\n';
		if (application) {
			auto& window = application->window();
			std::cerr << "agent-panel: " << window.agent_panel()->status_text().toStdString() << "\nturns: " << window.agent_tab()->turn_count() << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
