/// \file
/// \brief RFC-BPR-0125 milestone 2 ([[BPR-2145]]): the Agent tab's chips are
/// the core's offers.
///
/// \details Through the window's own Agent tab and the side chat panel it
/// sends with: the shipped chips and the requests they send equal today's
/// (AC05); a macro whose name is not its template sends its expansion,
/// Shift drafts it, typed words survive and `{cursor}` finishes in the field
/// (AC06); tooltip, accessible description and the sent turn's name and fold,
/// read back as a restart reads it, through the store (AC07); five chips and
/// More (AC08); values never bring a skill or a command (AC09); a drafted
/// macro keeps its subject and kind behind a removable chip (AC10).
///
/// With `BPR_PROMPT_RENDER_ROOT` set, the chips, a tooltip, More, the sent
/// turn and the drafted chip are saved there as PNGs.

#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/prompt_turn_name_host.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/prompt_macro.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	auto render_root() -> QString { return qEnvironmentVariable("BPR_PROMPT_RENDER_ROOT"); }

	void render(QWidget& aWidget, const QString& aName)
	{
		if (const auto root = render_root(); !root.isEmpty()) {
			const auto scheme = qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME", QStringLiteral("offscreen"));
			expect(aWidget.grab().save(root + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + scheme + QStringLiteral(".png"), "PNG"), "render saved: " + aName.toStdString());
		}
	}

	/// A tooltip as Qt draws one: its palette, frame and words.
	void render_tooltip(const QString& aText, const QString& aName)
	{
		if (render_root().isEmpty()) {
			return;
		}
		auto tip = QLabel{ aText };
		tip.setPalette(QToolTip::palette());
		tip.setFont(QToolTip::font());
		tip.setFrameShape(QFrame::Box);
		tip.setMargin(6);
		tip.setAutoFillBackground(true);
		tip.adjustSize();
		render(tip, aName);
	}

	void click(QPushButton& aButton, const Qt::KeyboardModifiers aModifiers)
	{
		const auto at      = QPointF{ aButton.rect().center() };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, at, at, Qt::LeftButton, Qt::LeftButton, aModifiers };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, at, at, Qt::LeftButton, Qt::NoButton, aModifiers };
		QCoreApplication::sendEvent(&aButton, &press);
		QCoreApplication::sendEvent(&aButton, &release);
		settle();
	}

	struct harness
	{
		harness()
		{
			Window.resize(1'280, 860);
			Window.show();
			auto opened   = application::workbench_projection{ };
			auto commands = core::workbench_command_delta{ };
			for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
				commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
			}
			opened.Commands = commands;
			opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
			opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 520U };
			Window.publish(opened);
			settle();
			Window.choose_modality(qt::review_modality::review);
			settle();
			Window.open_agent_action()->trigger();
			settle();
			auto profile        = core::agent_profile{ };
			profile.Id          = core::entry_profile_id(core::agent_protocol::codex_jsonl_exec, 1U);
			profile.Protocol    = core::agent_protocol::codex_jsonl_exec;
			profile.DisplayName = "Fixture agent";
			profile.Executable  = "/usr/bin/true";
			Window.agent_panel()->offer_profiles(std::vector{ profile });
			settle();
			const auto binding = core::agent_revision_binding{ .Host = core::git_host_id{ 1 }, .Repository = core::repository_id{ 2 }, .PullRequest = core::pull_request_id{ 3 }, .BaseOid = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"), .HeadOid = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"), .LeftEpoch = core::epoch_ordinal{ 1 }, .RightEpoch = core::epoch_ordinal{ 2 } };
			const auto session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = profile.Id, .Binding = binding };
			Window.side_chat()->set_dispatch([this](qt::side_chat_intent aIntent) {
				if (const auto* const send = std::get_if<qt::send_chat_message_intent>(&aIntent)) {
					Sends.push_back(*send);
				}
			});
			Window.side_chat()->present(qt::side_chat_projection{
				.Session        = session,
				.Continuations  = core::offer_agent_continuations(session, binding),
				.Chat           = { .Scope = { .Kind = core::agent_chat_scope_kind::pull_request } },
				.PrivateContext = { .Session = session.Id },
			});
			settle();
			tab().set_suggestion_context([this] { return Context; });
			tab().refresh();
			settle();
		}

		auto tab() -> qt::agent_tab_view& { return *Window.agent_tab(); }
		auto field() -> QPlainTextEdit& { return *Window.side_chat()->composer(); }

		void set_context(qt::agent_suggestion_context aContext)
		{
			Context = std::move(aContext);
			tab().refresh_suggestions();
			settle();
		}

		auto chip(const QString& aLabel) -> QPushButton*
		{
			for (auto* const button : tab().suggestion_buttons()) {
				if (button->property("agentPrompt").toString() == aLabel) {
					return button;
				}
			}
			return nullptr;
		}

		/// Admits the last send as a turn and ends it, as the host would.
		void answer(const QString& aReply)
		{
			auto identity                 = application::agent_invocation_projection_identity{ };
			identity.ApplicationSession   = core::application_session_id{ 1U };
			identity.Session.Id           = core::agent_session_id{ 9U };
			identity.InvocationGeneration = ++Generation;
			Window.agent_panel()->admit(identity);
			Window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
			settle();
			Window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
			static_cast<void>(aReply);
			settle();
		}

		qt::workbench_window                      Window;
		qt::agent_suggestion_context              Context;
		std::vector<qt::send_chat_message_intent> Sends;
		unsigned                                  Generation = 0U;
	};

	auto pull_request() -> core::prompt_pull_request
	{
		auto names   = core::prompt_pull_request{ };
		names.Number = 482U;
		names.Title  = "Faster renderer";
		names.Url    = "https://github.com/octo/app/pull/482";
		names.Author = "octo";
		names.Base   = "main";
		names.Head   = "fast-render";
		return names;
	}

	auto shipped() -> std::vector<core::prompt_macro> { return core::merge_prompt_macros(core::shipped_prompt_macros(), { }).macros(); }

	/// The reviewer's own macro in the Agent tab, after the built-ins.
	auto own(std::string aId, std::string aName, std::string aTemplate, const core::prompt_subject aSubject = core::prompt_subject::pull_request, const core::prompt_ask aAsk = core::prompt_ask::question) -> core::prompt_macro
	{
		auto macro          = core::prompt_macro{ };
		macro.Id            = std::move(aId);
		macro.Place         = core::prompt_place::agent_tab;
		macro.Body.Name     = std::move(aName);
		macro.Body.Template = std::move(aTemplate);
		macro.Body.Subject  = aSubject;
		macro.Body.Kind     = aAsk;
		return macro;
	}

	/// What today's widget sent for one chip: its words, and the subject the
	/// core decided from the subject and review flag the widget carried.
	auto today(const QString& aText, const std::optional<core::agent_suggested_subject> aSubject, const bool aReview, const qt::agent_suggestion_context& aContext, const bool aLinesAttached) -> std::pair<std::string, std::optional<core::agent_question>>
	{
		return { aText.toStdString(), core::decide_agent_question({ .LinesAttached = aLinesAttached, .LinesSelected = aContext.Lines.has_value(), .Suggested = aSubject, .Review = aReview, .OpenFile = aContext.OpenFile.toStdString() }) };
	}

	void test_ac05_shipped_chips_send_todays_requests(harness& aHarness)
	{
		auto& tab = aHarness.tab();
		expect(tab.suggestions() == QStringList{ QStringLiteral("Review this pull request"), QStringLiteral("What could this change break?") }, "AC05 with nothing open: today's two chips");
		auto context        = qt::agent_suggestion_context{ };
		context.OpenFile    = QStringLiteral("src/render/renderer.cpp");
		context.PullRequest = pull_request();
		context.Repository  = "octo/app";
		aHarness.set_context(context);
		expect(tab.suggestions() == QStringList{ QStringLiteral("Review this pull request"), QStringLiteral("Review src/render/renderer.cpp"), QStringLiteral("What could this change break?") }, "AC05 a file open adds Review <file>");
		render(*tab.suggestions_widget(), QStringLiteral("rfc0125-m2-chips-shipped"));
		context.Lines = std::pair{ 34U, 34U };
		aHarness.set_context(context);
		expect(tab.suggestions().back() == QStringLiteral("Explain src/render/renderer.cpp line 34"), "AC05 one line: Explain <file> line <n>");
		context.Lines = std::pair{ 34U, 36U };
		aHarness.set_context(context);
		expect(tab.suggestions().back() == QStringLiteral("Explain the selected block (src/render/renderer.cpp lines 34–36)"), "AC05 several: Explain the selected block (<file> lines a–b)");

		// Each built-in sends what today's widget sent: its words, its subject
		// and its entry point, with no skill text to read but its own.
		const auto expected = std::vector<std::tuple<QString, std::optional<core::agent_suggested_subject>, bool>>{
			{ QStringLiteral("Review this pull request"), core::agent_suggested_subject::pull_request, true },
			{ QStringLiteral("Review src/render/renderer.cpp"), core::agent_suggested_subject::open_file, true },
			{ QStringLiteral("What could this change break?"), core::agent_suggested_subject::pull_request, false },
			{ QStringLiteral("Explain the selected block (src/render/renderer.cpp lines 34–36)"), core::agent_suggested_subject::selected_lines, false },
		};
		for (const auto& [label, subject, review] : expected) {
			aHarness.field().clear();
			const auto sent = aHarness.Sends.size();
			auto*      chip = aHarness.chip(label);
			expect(chip != nullptr, "AC05 the chip is offered: " + label.toStdString());
			click(*chip, Qt::NoModifier);
			expect(aHarness.Sends.size() == sent + 1U, "AC05 a click sends: " + label.toStdString());
			const auto& send = aHarness.Sends.back();
			const auto  was  = today(label, subject, review, context, tab.lines_attached());
			expect(send.Text == was.first && send.Question == was.second && send.Action.entry_point() == was.second->EntryPoint, "AC05 the request equals today's, byte for byte: " + label.toStdString());
			expect(send.SkillText == std::optional<std::string>{ "" }, "AC05 a built-in's own text names no skill, as today");
		}

		// A conversation: today's four, the reply only where one can be written.
		auto chip    = qt::agent_conversation_chip{ };
		chip.Key     = "thread:1";
		chip.Text    = QStringLiteral("src/cache.cpp line 42");
		chip.Replies = true;
		tab.set_conversation_chip(chip);
		tab.refresh_suggestions();
		settle();
		expect(tab.suggestions() == QStringList{ QStringLiteral("Summarize this conversation"), QStringLiteral("Is the concern raised here right?"), QStringLiteral("Draft a reply to this conversation") }, "AC05 a thread's chips");
		aHarness.field().clear();
		click(*aHarness.chip(QStringLiteral("Draft a reply to this conversation")), Qt::NoModifier);
		expect(aHarness.Sends.back().Text == "Draft a reply to this conversation" && aHarness.Sends.back().Question && aHarness.Sends.back().Question->Subject == core::agent_question_subject::conversation
				   && aHarness.Sends.back().Question->EntryPoint == core::agent_entry_point::ask_about_conversation,
			   "AC05 a conversation chip asks about the conversation, as today");
		chip.Replies = false;
		tab.set_conversation_chip(chip);
		tab.refresh_suggestions();
		settle();
		expect(tab.suggestions().back() == QStringLiteral("Explain the code this note is on"), "AC05 a note offers no reply");
		tab.set_conversation_chip(std::nullopt);
		context.Lines.reset();
		aHarness.set_context(context);
	}

	void test_ac06_expansion_draft_and_finish(harness& aHarness)
	{
		auto& tab    = aHarness.tab();
		auto  macros = shipped();
		macros.push_back(own("own.security", "Security pass on {file.name}", "Check {file} for injection and unsafe deserialisation. Say which lines.", core::prompt_subject::open_file));
		macros.push_back(own("own.focus", "Ask about a part of {file.name}…", "In {file}, look only at {cursor} and say what could break.", core::prompt_subject::open_file));
		tab.present_prompt_macros(macros);
		settle();
		const auto name      = QStringLiteral("Security pass on renderer.cpp");
		const auto expansion = QStringLiteral("Check src/render/renderer.cpp for injection and unsafe deserialisation. Say which lines.");
		expect(tab.suggestions().contains(name), "AC06 the reviewer's macro is a chip under its name");
		aHarness.field().clear();
		click(*aHarness.chip(name), Qt::NoModifier);
		expect(aHarness.Sends.back().Text == expansion.toStdString() && aHarness.Sends.back().Question && aHarness.Sends.back().Question->Subject == core::agent_question_subject::file,
			   "AC06 a click sends the expansion, not the name, about the open file");
		const auto sent = aHarness.Sends.size();
		aHarness.field().clear();
		click(*aHarness.chip(name), Qt::ShiftModifier);
		expect(aHarness.Sends.size() == sent && aHarness.field().toPlainText() == expansion && aHarness.field().textCursor().position() == expansion.size(), "AC06 Shift-click puts the expansion in the field, caret at its end, and sends nothing");
		aHarness.field().clear();
		aHarness.field().setPlainText(QStringLiteral("my own words"));
		click(*aHarness.chip(name), Qt::NoModifier);
		expect(aHarness.Sends.size() == sent + 1U && aHarness.Sends.back().Text == expansion.toStdString() && aHarness.field().toPlainText() == QStringLiteral("my own words"), "AC06 typed words survive a click, as before");
		aHarness.field().clear();
		const auto focus = QStringLiteral("Ask about a part of renderer.cpp…");
		click(*aHarness.chip(focus), Qt::NoModifier);
		expect(aHarness.Sends.size() == sent + 1U && aHarness.field().toPlainText() == QStringLiteral("In src/render/renderer.cpp, look only at  and say what could break.")
				   && aHarness.field().textCursor().position() == QStringLiteral("In src/render/renderer.cpp, look only at ").size(),
			   "AC06 a {cursor} macro is put in the field by a plain click, the caret at the mark");
		aHarness.field().clear();
		tab.present_prompt_macros(shipped());
		settle();
	}

	void test_ac07_tooltip_turn_and_restart(harness& aHarness)
	{
		auto& tab          = aHarness.tab();
		auto  macros       = shipped();
		auto  longTemplate = std::string{ "Check {file} line by line:" };
		for (auto line = 1; line <= 14; ++line) {
			longTemplate += "\n" + std::to_string(line) + ". look for one more kind of mistake";
		}
		macros.push_back(own("own.long", "Long pass on {file.name}", longTemplate, core::prompt_subject::open_file));
		tab.present_prompt_macros(macros);
		settle();
		auto* const chip = aHarness.chip(QStringLiteral("Long pass on renderer.cpp"));
		expect(chip != nullptr, "AC07 the long macro is a chip");
		const auto expansion = chip->property("agentPromptExpansion").toString();
		expect(chip->toolTip().contains(expansion.section(QLatin1Char{ '\n' }, 0, 11)) && chip->toolTip().contains(QStringLiteral("and 3 more lines")) && chip->toolTip().endsWith(QStringLiteral("Click to send. Shift-click to edit before sending.")),
			   "AC07 the tooltip holds the expansion's first 12 lines and how to send or edit it");
		expect(chip->accessibleName() == QStringLiteral("Long pass on renderer.cpp") && chip->accessibleDescription().startsWith(expansion.section(QLatin1Char{ '\n' }, 0, 0))
				   && chip->accessibleDescription().contains(QStringLiteral("Click to send. Shift-click or ")),
			   "AC07 the accessible name is the chip's text; its description holds the expansion and how to send or edit it");
		render_tooltip(chip->toolTip(), QStringLiteral("rfc0125-m2-tooltip"));

		aHarness.field().clear();
		click(*chip, Qt::NoModifier);
		aHarness.answer(QStringLiteral("Lines 12 and 40 read user input unchecked."));
		const auto turn = tab.turn_count() - 1U;
		expect(tab.turn_name(turn) == QStringLiteral("Long pass on renderer.cpp") && tab.turn_question(turn) == expansion, "AC07 the turn shows the sent text under the macro's name");
		auto* const showAll = tab.turn_show_all(turn);
		auto* const shown   = tab.findChildren<QLabel*>(QStringLiteral("agentTurnQuestionText")).back();
		expect(showAll != nullptr && showAll->text() == QStringLiteral("Show all") && shown->text().count(QLatin1Char{ '\n' }) + shown->text().count(QStringLiteral("<br>")) == 5,
			   "AC07 folded past six lines, behind Show all");
		render(tab, QStringLiteral("rfc0125-m2-sent-turn-folded"));
		showAll->click();
		settle();
		expect(showAll->text() == QStringLiteral("Show less") && shown->text().count(QLatin1Char{ '\n' }) + shown->text().count(QStringLiteral("<br>")) == 14, "AC07 Show all shows every line");
		showAll->click();
		settle();

		// The kept thread, as the window keeps it, through the profile's store,
		// then read back by a new store and host as a restart reads it.
		const auto kept = tab.thread_turns();
		expect(!kept.empty() && kept.back().Name == QStringLiteral("Long pass on renderer.cpp"), "AC07 the kept turn keeps its name");
		auto root = QTemporaryDir{ };
		expect(root.isValid(), "a scratch root");
		{
			auto store        = adapters::sqlite::make_prompt_turn_name_store(root.path().toStdString());
			auto lane         = application::manual_executor{ };
			auto dependencies = application::prompt_turn_name_dependencies{ .Store = store.get(), .Effects = &lane, .Decision = &lane };
			auto host         = application::prompt_turn_name_host{ std::move(dependencies) };
			host.load();
			lane.drain();
			host.remember({ .Asked = kept.back().Question.toStdString(), .Name = kept.back().Name.toStdString() });
			lane.drain();
			host.shutdown();
		}
		auto store        = adapters::sqlite::make_prompt_turn_name_store(root.path().toStdString());
		auto lane         = application::manual_executor{ };
		auto dependencies = application::prompt_turn_name_dependencies{ .Store = store.get(), .Effects = &lane, .Decision = &lane };
		auto host         = application::prompt_turn_name_host{ std::move(dependencies) };
		host.load();
		lane.drain();
		const auto recalled = host.names().recall(kept.back().Question.toStdString());
		expect(recalled == std::optional<std::string>{ "Long pass on renderer.cpp" }, "AC07 the store gives the name back after a restart");
		auto restarted = kept;
		for (auto& each : restarted) {
			each.Name.clear();
		}
		restarted.back().Name = QString::fromStdString(*recalled);
		tab.present_thread(restarted);
		settle();
		expect(tab.turn_name(tab.turn_count() - 1U) == QStringLiteral("Long pass on renderer.cpp") && tab.turn_show_all(tab.turn_count() - 1U) != nullptr, "AC07 the thread read back shows the same name and fold");
		host.shutdown();
		tab.present_thread({ });
		tab.present_prompt_macros(shipped());
		settle();
	}

	void test_ac08_more_and_focus(harness& aHarness)
	{
		auto& tab    = aHarness.tab();
		auto  macros = shipped();
		for (const auto* const word : { "Naming", "Tests", "Errors", "Docs" }) {
			macros.push_back(own(std::string{ "own." } + word, std::string{ "Check " } + word, std::string{ "Check the " } + word + " in {repo} #{pr.number}."));
		}
		tab.present_prompt_macros(macros);
		settle();
		auto* const list = tab.suggestion_list();
		expect(tab.suggestions().size() == 5U && list->more().size() == 2U && list->more_button() != nullptr && list->more_button()->text() == QStringLiteral("More prompts (2)…"),
			   "AC08 with seven matching: five chips, then More prompts (2)…");
		expect(list->more_menu() != nullptr && list->more_menu()->actions().size() == 2 && list->more_menu()->actions().front()->text() == QStringLiteral("Check Errors"), "AC08 More's menu holds the rest, in order");
		render(*tab.suggestions_widget(), QStringLiteral("rfc0125-m2-chips-more"));
		list->more_button()->click();
		settle();
		render(*list->more_menu(), QStringLiteral("rfc0125-m2-more-menu"));
		const auto sent = aHarness.Sends.size();
		list->more_menu()->actions().front()->trigger();
		settle();
		expect(aHarness.Sends.size() == sent + 1U && aHarness.Sends.back().Text == "Check the Errors in octo/app #482.", "AC08 a More item sends as a chip does");
		list->more_menu()->hide();
		aHarness.field().clear();
		{
			auto press = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier };
			QCoreApplication::sendEvent(list->more_menu(), &press);
		}
		list->more_menu()->actions().at(1)->trigger();
		settle();
		expect(aHarness.Sends.size() == sent + 1U && aHarness.field().toPlainText() == QStringLiteral("Check the Docs in octo/app #482."), "AC08 and with Shift drafts as a chip does");
		aHarness.field().clear();

		// A refresh that changes no name leaves the keyboard on its chip.
		auto* const focused = tab.suggestion_buttons().at(2);
		aHarness.Window.activateWindow();
		focused->setFocus(Qt::TabFocusReason);
		settle();
		expect(focused->hasFocus(), "the third chip has the keyboard");
		auto context               = aHarness.Context;
		context.PullRequest->Title = "A new title";
		aHarness.set_context(context);
		expect(tab.suggestion_buttons().at(2) == focused && focused->hasFocus(), "AC08 a refresh that changes no name leaves focus on its chip");
		tab.present_prompt_macros(shipped());
		settle();
	}

	void test_ac09_values_cannot_command(harness& aHarness)
	{
		auto& tab                  = aHarness.tab();
		auto  context              = aHarness.Context;
		context.PullRequest->Title = "/some-skill please";
		aHarness.set_context(context);
		auto macros = shipped();
		macros.push_back(own("own.title", "Summarize the title", "Summarize {pr.title} /review-notes"));
		macros.push_back(own("own.model", "Model check", "/model {pr.title}"));
		tab.present_prompt_macros(macros);
		settle();
		aHarness.field().clear();
		auto* const titled = aHarness.chip(QStringLiteral("Summarize the title"));
		expect(titled != nullptr, "AC09 the titled macro is a chip");
		click(*titled, Qt::NoModifier);
		expect(aHarness.Sends.back().Text == "Summarize /some-skill please /review-notes" && aHarness.Sends.back().SkillText == std::optional<std::string>{ "/review-notes" },
			   "AC09 a title naming /some-skill brings in no skill; the template's own reference stays");
		const auto sent = aHarness.Sends.size();
		click(*aHarness.chip(QStringLiteral("Model check")), Qt::NoModifier);
		expect(aHarness.Sends.size() == sent + 1U && aHarness.Sends.back().Text == "/model /some-skill please" && aHarness.Sends.back().SkillText == std::optional<std::string>{ "/model" },
			   "AC09 an expansion beginning /model is sent as text and changes no model");
		auto* const notice = tab.findChild<QLabel*>(QStringLiteral("agentComposerNotice"));
		expect(notice == nullptr || !notice->isVisibleTo(&tab), "AC09 no model notice was raised");
		// The same words typed are still a command, as before.
		aHarness.field().setPlainText(QStringLiteral("/model gpt-x"));
		tab.ask_button()->click();
		settle();
		expect(aHarness.Sends.size() == sent + 1U, "AC09 a typed /model is still read as a command");
		aHarness.field().clear();
		tab.present_prompt_macros(shipped());
		settle();
	}

	void test_ac10_drafted_macro_keeps_subject(harness& aHarness)
	{
		auto& tab = aHarness.tab();
		aHarness.field().clear();
		click(*aHarness.chip(QStringLiteral("Review this pull request")), Qt::ShiftModifier);
		expect(tab.macro_chip()->isVisibleTo(&tab) && tab.macro_chip_text() == QStringLiteral("Review this pull request") && tab.macro_chip_close()->accessibleName() == QStringLiteral("Remove the suggested prompt"),
			   "AC10 a drafted macro shows its chip in the field");
		render(*tab.question_field(), QStringLiteral("rfc0125-m2-drafted-chip"));
		aHarness.field().moveCursor(QTextCursor::End);
		aHarness.field().insertPlainText(QStringLiteral(", focusing on the cache"));
		const auto sent = aHarness.Sends.size();
		tab.ask_button()->click();
		settle();
		expect(aHarness.Sends.size() == sent + 1U && aHarness.Sends.back().Text == "Review this pull request, focusing on the cache" && aHarness.Sends.back().Question
				   && aHarness.Sends.back().Question->EntryPoint == core::agent_entry_point::run_ai_review && !aHarness.Sends.back().SkillText,
			   "AC10 edited and sent, it keeps its subject and kind: a review");
		aHarness.answer(QStringLiteral("Reviewed."));
		expect(!tab.macro_chip()->isVisibleTo(&tab), "the chip goes with the sent question");
		expect(tab.turn_name(tab.turn_count() - 1U) == QStringLiteral("Review this pull request"), "the edited turn shows the prompt it was sent as");

		click(*aHarness.chip(QStringLiteral("Review this pull request")), Qt::ShiftModifier);
		aHarness.field().moveCursor(QTextCursor::End);
		aHarness.field().insertPlainText(QStringLiteral(", focusing on the cache"));
		tab.macro_chip_close()->click();
		settle();
		expect(!tab.macro_chip()->isVisibleTo(&tab), "AC10 the chip's close removes it");
		tab.ask_button()->click();
		settle();
		expect(aHarness.Sends.back().Text == "Review this pull request, focusing on the cache" && aHarness.Sends.back().Question
				   && aHarness.Sends.back().Question->EntryPoint == core::agent_entry_point::open_full_agent_session && aHarness.Sends.back().Question->Subject == core::agent_question_subject::pull_request,
			   "AC10 with the chip removed the same text is sent as a typed question");
		aHarness.answer(QStringLiteral("Answered."));
		click(*aHarness.chip(QStringLiteral("Review this pull request")), Qt::ShiftModifier);
		aHarness.field().clear();
		settle();
		expect(!tab.macro_chip()->isVisibleTo(&tab), "AC10 emptying the field removes the chip");
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	auto application = QApplication{ aArgc, aArgv };
	auto harnessed   = harness{ };
	test_ac05_shipped_chips_send_todays_requests(harnessed);
	test_ac06_expansion_draft_and_finish(harnessed);
	test_ac08_more_and_focus(harnessed);
	test_ac09_values_cannot_command(harnessed);
	test_ac10_drafted_macro_keeps_subject(harnessed);
	test_ac07_tooltip_turn_and_restart(harnessed);
	std::cout << "prompt-macro-chips:pass\n";
	return EXIT_SUCCESS;
}
