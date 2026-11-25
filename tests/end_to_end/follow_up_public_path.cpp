/// \file
/// \brief [[BPR-2286]] (owner, 2026-11-19: "I want to be able to easily ask a
/// follow-up and perhaps update a suggested comment from a report or agent tab
/// response ... I also want to ask about a comment from a comment or draft
/// composer, and either be able to optionally update the draft or draft a
/// reply to a thread after one or more rounds of conversation with the
/// agent"). [[BPR-2322]] (owner, 2026-11-20): the conversation is the item's
/// side chat, beside the report or the code, as the Tour tab's chat sits
/// beside the tour; no window of its own opens. Through the window, with the
/// fixture agent, on the fixture's pull request #21. No real agent, no
/// network, no cost; nothing is published.
///
/// - The draft composer's Ask about this opens the side chat about the draft;
///   Update draft shows the agent's text beside the draft, and Use puts it in
///   the field as one undoable edit. A draft changed meanwhile is not replaced.
/// - A thread's Ask menu has Ask about this…: a round, then Draft reply shows
///   the reply, and Put in reply puts it in the thread's reply composer.
/// - A review's findings are its report's ([[BPR-2331]]): the Agent tab shows
///   the report's summary and no finding card. The report's Ask follow-up on
///   a finding: two rounds, each carrying the ones before; Update suggested
///   comment is seen first and replaces nothing until Use; then the report
///   says under the finding that Draft comment drafts the update, opening
///   the chat again shows the same chat beside the report, and Draft comment
///   drafts the updated text.
///
/// - The owner, of the updated comment written out again under its finding:
///   "This updated comment section at the bottom is more confusing than
///   helpful. It's just restating the thing that's drawn above it." The
///   report never restates an updated comment: its Suggested change is the
///   finding's own, once, and one line under the keep row says what is so
///   (Draft comment drafts the update, or the draft differs from it). A
///   finding no follow-up updated shows its suggested comment's words and
///   not its suggestion a second time: the Suggested change above is that
///   suggestion, and the label says it goes with the words.
///
/// - The owner, of Go to draft: "How come when I clicked goto draft it didnt
///   switch to the comments tab? Then goto draft went to the comment but
///   didn't select it, it was still collapsed". From an updated finding,
///   with the draft closed and the right tray on the Agent tab, Go to draft
///   turns the tray to Comments and opens the draft in its composer.
///
/// - The draft gone to shows its first column: its words from their first
///   letter and its suggestion's line, wider than the drawer, from its
///   start. It opened panned sideways ("ssue: clamp both sides ..."). The
///   field has no sideways range at any width the drawer is dragged to; the
///   caret at the end of the long line moves the line's box, not the field.
///
/// - [[BPR-2370]] (owner: "Why does the report side chat not have access to
///   the code?"): each round runs in the pull request's workspace, as a tour
///   part's chat does: the fixture agent's answer, from the report's chat and
///   the draft's, says it found the pull request's file in its working folder.
///
/// - [[BPR-2322]]: with Verbose on, every question, proposal and Use is an
///   `agent.side_chat` line in the diagnostic log, and an anonymized export
///   of that log holds none of the words asked, answered, proposed or quoted.
///
/// - The report beside its side chat is narrow, and a keep row's sentence
///   stood a letter to a line beside its chips. Beside the chat every row is
///   inside the page and its sentence breaks only between words: a finding
///   offered, one drafted, and one whose draft differs from its update.
///
///     follow_up_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/diagnostic_files.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "report_page_rows.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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

	/// With `BPR_FOLLOW_UP_RENDER_ROOT` set, a widget is saved there as a PNG
	/// (the owner's looks).
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FOLLOW_UP_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}

	/// Asks `aQuestion` in the side chat and waits for its answer.
	void ask(qt::item_side_chat& aFollowUp, const QString& aQuestion)
	{
		const auto before = aFollowUp.answers().size();
		aFollowUp.composer()->setPlainText(aQuestion);
		aFollowUp.send_button()->click();
		if (!e2e::wait_until_plain([&] { return aFollowUp.answers().size() == before + 1 && !aFollowUp.running(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the follow-up answers \"" + aQuestion.toStdString() + "\"; it says: " + aFollowUp.status_text().toStdString() };
		}
	}

	void write_skill(const QDir& aHome, const QString& aName, const QByteArray& aGuidance)
	{
		const auto folder = QStringLiteral(".agents/skills/") + aName;
		expect(aHome.mkpath(folder), "create a fixture skill folder");
		auto       skill = QFile{ aHome.filePath(folder + QStringLiteral("/SKILL.md")) };
		const auto text  = "---\nname: " + aName.toUtf8() + "\ndescription: Guidance for an item-chat fixture.\n---\n" + aGuidance + '\n';
		expect(skill.open(QIODevice::WriteOnly) && skill.write(text) == text.size(), "write the fixture skill");
	}

	void composer_key(QPlainTextEdit& aComposer, const int aKey, const QString& aText = { }, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
		QApplication::sendEvent(&aComposer, &press);
		QApplication::processEvents();
	}

	void ask_with_skill(qt::item_side_chat& aFollowUp)
	{
		auto& composer = *aFollowUp.composer();
		expect(composer.hasFocus(), "Ask follow-up focuses the item chat's composer");
		auto* completion = static_cast<qt::skill_completer*>(nullptr);
		for (auto* const child : composer.children()) {
			if (auto* const completer = dynamic_cast<qt::skill_completer*>(child); completer != nullptr) {
				completion = completer;
				break;
			}
		}
		expect(completion != nullptr, "the item chat's composer has a skill picker");
		const auto question    = QStringLiteral("I don't follow. Why is this blocking? ");
		const auto prefix      = QStringLiteral("/fixture-item");
		const auto type_prefix = [&] {
			composer.setPlainText(question);
			composer.moveCursor(QTextCursor::End);
			for (const auto letter : prefix) {
				composer_key(composer, letter.toUpper().unicode(), QString{ letter });
			}
		};
		const auto rows = QStringList{
			QStringLiteral("/fixture-item-decoy (Personal, ~/.agents/skills)"),
			QStringLiteral("/fixture-item-style (Personal, ~/.agents/skills)"),
		};
		const auto selected = question + QStringLiteral("/fixture-item-style ");
		type_prefix();
		wait([&] { return completion->rows() == rows; }, "the slash prefix shows both discovered fixture skills and their source");
		render(*completion->popup(), QStringLiteral("side-chat-skill-popup"));
		composer_key(composer, Qt::Key_Down);
		composer_key(composer, Qt::Key_Tab, QStringLiteral("\t"));
		expect(composer.toPlainText() == selected && composer.hasFocus() && completion->popup()->isHidden(), "Tab inserts the highlighted skill and leaves the composer focused");

		type_prefix();
		composer_key(composer, Qt::Key_Down);
		composer_key(composer, Qt::Key_Return, QStringLiteral("\r"));
		expect(composer.toPlainText() == selected && !aFollowUp.running() && aFollowUp.questions().isEmpty(), "Return chooses the skill without sending");
		composer_key(composer, Qt::Key_Return, QStringLiteral("\r"), Qt::ShiftModifier);
		expect(composer.toPlainText() == selected + QLatin1Char('\n') && !aFollowUp.running(), "Shift+Return still makes a new line with the picker closed");
		composer_key(composer, Qt::Key_Return, QStringLiteral("\r"));
		wait([&] { return aFollowUp.answers().size() == 1 && !aFollowUp.running(); }, "the fixture agent answers the completed question");
		expect(aFollowUp.questions().front() == selected.trimmed(), "the transcript shows the typed question and completed reference");
		expect(aFollowUp.answers().front().endsWith(QStringLiteral(" Item-chat skill guidance received.")), "the request includes only the chosen skill's guidance in its inline fields");
	}

	/// Presses the side chat's apply action and waits for its proposal.
	void propose(qt::item_side_chat& aFollowUp, const QString& aAction)
	{
		expect(aFollowUp.apply_button()->isVisible() && aFollowUp.apply_button()->text() == aAction, "the follow-up offers " + aAction.toStdString() + "; it offers " + aFollowUp.apply_button()->text().toStdString());
		aFollowUp.apply_button()->click();
		if (!e2e::wait_until_plain([&] { return !aFollowUp.proposal_text().isEmpty() && !aFollowUp.running(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aAction.toStdString() + " proposes a text; it says: " + aFollowUp.status_text().toStdString() };
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "follow-up: no private data root\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto       options   = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		const auto skillHome = QDir{ data.filePath(QStringLiteral("fixture-skill-home")) };
		write_skill(skillHome, QStringLiteral("fixture-item-decoy"), QByteArray{ "FIXTURE_UNUSED_ITEM_CHAT_GUIDANCE" });
		write_skill(skillHome, QStringLiteral("fixture-item-style"), QByteArray{ "FIXTURE_SELECTED_ITEM_CHAT_GUIDANCE" });
		options.PersonalSkillHome   = std::filesystem::path{ skillHome.absolutePath().toStdU16String() };
		options.FixtureAgentSession = true;
		options.EpochPhase          = app::detail::github_epoch_fixture_phase::reuse;
		auto terminal               = false;
		// The returning review's #21: the same change, with review threads.
		auto startup = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		// [[BPR-2322]]. The owner's check runs with Verbose on.
		const auto control = std::dynamic_pointer_cast<application::diagnostic_control_port>(access::diagnostic_log(*application));
		expect(control != nullptr && control->configure({ .Enabled = true, .Detail = application::diagnostic_detail::verbose, .Privacy = application::diagnostic_privacy::anonymized }), "Verbose diagnostics are on");
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.open_file_in_review(path) && window.active_document_path() == path; }, "the file opens in Review");
		wait([&] { return window.comparison_surface() != nullptr && window.comparison_surface()->current_result() != nullptr; }, "the comparison is in");
		window.open_agent_question();
		auto* const tab = window.agent_tab();
		wait([&] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; }, "the fixture agent is set up");
		const auto mutation    = access::review_mutation_evidence(*application);
		const auto unpublished = [&] {
			const auto after = access::review_mutation_evidence(*application);
			return after.Submissions == mutation.Submissions && after.StoredComments == mutation.StoredComments && after.NativePosts == mutation.NativePosts && after.FilePosts == mutation.FilePosts;
		};
		auto* const cards    = window.conversation_panel()->cards();
		auto* const followUp = window.item_side_chat();
		expect(followUp != nullptr && !followUp->isWindow() && !followUp->isVisible(), "the item's side chat is a column of the window, closed until an item is asked about");
		// No window of its own: what is open is the review window alone.
		const auto onlyTheWindow = [&window] {
			return std::ranges::none_of(QApplication::topLevelWidgets(), [&window](const QWidget* aWidget) { return aWidget != &window && aWidget->isVisible() && aWidget->isWindow() && qobject_cast<const QMenu*>(aWidget) == nullptr; });
		};

		// The draft composer: Ask about this, Update draft, Use.
		{
			const auto  result  = window.comparison_surface()->current_result();
			const auto& segment = std::ranges::find_if(result->Alignment.Segments, [](const core::alignment_segment& aSegment) {
				return (aSegment.Kind == core::alignment_kind::right_only || aSegment.Kind == core::alignment_kind::replacement) && aSegment.Right.Begin < aSegment.Right.End;
			});
			expect(segment != result->Alignment.Segments.end(), "the file has a line the pull request adds");
			auto* const editor = window.review_editor();
			expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, segment->Right.Begin), "the line can be selected");
			wait([&] { return editor->shown_draft() && editor->shown_draft()->Anchor.Line == segment->Right.Begin; }, "a draft is offered on the line");
			window.add_comment_action()->trigger();
			wait([&] { return cards->writing_draft() && cards->draft_editor() != nullptr; }, "the draft is written in the drawer");
			auto* const field = cards->draft_editor();
			field->setPlainText(QStringLiteral("this seems wrong?"));
			wait([&] { return editor->shown_draft() && editor->shown_draft()->Body == "this seems wrong?"; }, "the words are the draft's");
			auto* const askAbout = cards->ask_about_control();
			expect(askAbout != nullptr && askAbout->isVisibleTo(&window) && askAbout->text() == QStringLiteral("Ask about this"), "the composer offers Ask about this beside Enhance");
			askAbout->click();
			QApplication::processEvents();
			expect(followUp->isVisible() && followUp->subject_text().startsWith(QStringLiteral("Your draft at renderer.cpp:")) && followUp->item_text() == QStringLiteral("this seems wrong?") && followUp->chip_text() == QStringLiteral("this seems wrong?"),
				   "the side chat about the draft opens: " + followUp->subject_text().toStdString());
			expect(onlyTheWindow(), "no window of its own opens");
			const auto surface = window.comparison_surface();
			expect(followUp->parentWidget() == surface->parentWidget() && followUp->mapTo(&window, QPoint{ 0, 0 }).x() >= surface->mapTo(&window, QPoint{ surface->width(), 0 }).x(), "the chat stands beside the code");
			ask(*followUp, QStringLiteral("Is this clear enough for the author?"));
			expect(followUp->answers().back().startsWith(QStringLiteral("Fixture follow-up: I read the item and 0 earlier rounds.")), "the fixture agent answered the round: " + followUp->answers().back().toStdString());
			// [[BPR-2370]]. The round ran in the pull request's workspace.
			expect(followUp->answers().back().contains(QStringLiteral("I read src/engine/renderer.cpp in the pull request's workspace.")), "the draft's round ran in the workspace: " + followUp->answers().back().toStdString());
			propose(*followUp, QStringLiteral("Update draft"));
			const auto proposed = followUp->proposal_text();
			expect(proposed == QStringLiteral("question: does a zero height still reach the swap chain here? A minimised window reports one."), "the proposal is the agent's draft: " + proposed.toStdString());
			expect(field->toPlainText() == QStringLiteral("this seems wrong?"), "nothing is replaced before Use");
			render(*followUp, QStringLiteral("follow-up-update-draft"));
			// Changed meanwhile: refused, nothing replaced.
			field->setPlainText(QStringLiteral("this seems wrong!"));
			followUp->use_button()->click();
			expect(field->toPlainText() == QStringLiteral("this seems wrong!") && followUp->status_text().contains(QStringLiteral("Nothing was replaced")), "a draft changed since is not replaced: " + followUp->status_text().toStdString());
			// Asked again over the draft as it is, Use replaces it in one edit.
			propose(*followUp, QStringLiteral("Update draft"));
			followUp->use_button()->click();
			expect(field->toPlainText() == proposed && followUp->status_text().startsWith(QStringLiteral("Draft updated.")), "Use puts the proposal in the field: " + field->toPlainText().toStdString());
			wait([&] { return editor->shown_draft() && editor->shown_draft()->Body == proposed.toStdString(); }, "the draft holds it");
			field->undo();
			expect(field->toPlainText() == QStringLiteral("this seems wrong!"), "Undo takes it back in one step: " + field->toPlainText().toStdString());
			field->redo();
			expect(field->toPlainText() == proposed, "Redo puts it back");
			expect(unpublished(), "nothing is published from the draft composer");
		}

		// A thread: Ask about this… in its Ask menu, Draft reply, Put in reply.
		{
			const auto isLine12 = [&path](const qt::thread_card& aCard) { return aCard.Path == path && aCard.Line == 12U && !aCard.Draft && !aCard.Posts.empty(); };
			if (!e2e::wait_until_plain([&] { return std::ranges::any_of(cards->cards(), isLine12); }, e2e::extended_agent_budget)) {
				auto listed = std::string{ };
				for (const auto& each : cards->cards()) {
					listed += each.Path.toStdString() + ":" + (each.Line ? std::to_string(*each.Line) : std::string{ "-" }) + (each.Draft ? " draft" : "") + "; ";
				}
				throw std::runtime_error{ "the line-12 thread is listed: " + listed };
			}
			const auto line12    = std::ranges::find_if(cards->cards(), isLine12);
			const auto identity  = line12->Id;
			auto*      threadAsk = static_cast<qt::ask_menu_button*>(nullptr);
			wait([&] {
				cards->open_thread(identity);
				QApplication::processEvents();
				threadAsk = static_cast<qt::ask_menu_button*>(cards->findChild<QToolButton*>(QStringLiteral("threadAsk")));
				return threadAsk != nullptr;
			},
				 "the open thread has Ask");
			threadAsk->refresh_menu();
			auto* const row = threadAsk->menu()->findChild<QAction*>(QStringLiteral("quickAskFollowUp"));
			expect(row != nullptr && row->isEnabled() && row->text() == QStringLiteral("Ask about this…"), "the thread's Ask menu offers Ask about this");
			row->trigger();
			wait([&] { return followUp->subject_text() == QStringLiteral("Conversation at renderer.cpp:12"); }, "the side chat about the thread: " + followUp->subject_text().toStdString());
			expect(followUp->answers().isEmpty() && !followUp->item_text().isEmpty(), "the thread's own chat, its posts quoted");
			ask(*followUp, QStringLiteral("Is the reviewer right?"));
			propose(*followUp, QStringLiteral("Draft reply"));
			const auto reply = followUp->proposal_text();
			expect(reply.startsWith(QStringLiteral("Thanks for checking.")), "the proposal is a reply: " + reply.toStdString());
			expect(followUp->use_button()->text() == QStringLiteral("Put in reply"), "Use says where it goes");
			render(*followUp, QStringLiteral("follow-up-draft-reply"));
			followUp->use_button()->click();
			wait([&] { return cards->reply_editor() != nullptr && cards->reply_editor()->toPlainText().contains(reply); }, "the reply is in the thread's reply composer");
			wait([&] { return followUp->status_text().startsWith(QStringLiteral("The reply is in the conversation's reply composer.")); }, "it says where it went: " + followUp->status_text().toStdString());
			expect(unpublished(), "nothing is published from the thread");
		}

		// A review's findings, held by its report ([[BPR-2331]]): the Agent
		// tab shows the report's summary and no finding cards; the report's
		// Ask follow-up, two rounds, Update suggested comment.
		window.open_agent_question();
		auto* const reviewAsk = window.review_ask_button();
		auto*       review    = static_cast<QAction*>(nullptr);
		wait([&] {
			reviewAsk->refresh_menu();
			review = reviewAsk->menu()->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.review-pull-request_pull-request"));
			return review != nullptr && review->isEnabled();
		},
			 "Review this pull request is offered");
		const auto before = tab->turn_count();
		review->trigger();
		wait([&] { return tab->turn_count() == before + 1U && tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value() && access::agent_invocation_evidence(*application).Quiescent; },
			 "the review is answered");
		const auto shown = tab->turn_cards(before);
		expect(shown.size() == 2U && std::ranges::all_of(shown, [](const QWidget* aCard) { return aCard->isHidden(); }), "the review's two findings are its report's, and no card shows in the Agent tab");
		auto* const summary = tab->turn_report_tally(before);
		expect(summary != nullptr && summary->isVisible() && !summary->rows().empty(), "the Agent tab shows the report's summary over its attachment");
		auto* const openRow = tab->turn_report_card(before)->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(openRow != nullptr, "the card has its Open report row");
		openRow->click();
		auto* const report = window.report_view();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens");
		QApplication::processEvents();
		const auto& issue = report->findings().at(1);
		expect(issue.Card == std::optional<std::size_t>{ 1U }, "the report's second finding is the tonemap card");
		const auto offered = report->dressed_view()->toPlainText();
		expect(offered.contains(QStringLiteral("Ask follow-up")) && offered.contains(QStringLiteral("Dismiss")) && !offered.contains(QStringLiteral("Suggested comment, updated:")),
			   "the report offers Ask follow-up and Dismiss on its findings:\n" + offered.toStdString());
		expect(report->press(issue.Outline, qt::report_link::ask), "Ask follow-up is pressed on the report");
		wait([&] { return followUp->subject_text() == QStringLiteral("Suggested comment at tonemap.frag:5") && followUp->isVisible(); }, "the side chat about the finding: " + followUp->subject_text().toStdString());
		expect(!followUp->item_text().isEmpty(), "it quotes the finding");
		ask_with_skill(*followUp);
		ask(*followUp, QStringLiteral("Can it be softer?"));
		expect(followUp->answers().back().startsWith(QStringLiteral("Fixture follow-up: I read the item and 1 earlier rounds.")), "the second round carried the first: " + followUp->answers().back().toStdString());
		expect(!followUp->answers().back().contains(QStringLiteral("Item-chat skill guidance received.")), "a later question does not carry the earlier question's skill guidance");
		// [[BPR-2370]]. The report's side chat reads the code: both its
		// rounds, the one with a skill too, ran in the pull request's
		// workspace.
		for (const auto& answered : followUp->answers()) {
			expect(answered.contains(QStringLiteral("I read src/engine/renderer.cpp in the pull request's workspace.")), "the report's round ran in the workspace: " + answered.toStdString());
		}
		propose(*followUp, QStringLiteral("Update suggested comment"));
		const auto updated = QStringLiteral("issue (blocking): clamp the height as well as the width. A minimised window reports a height of zero, and the swap chain asserts on it.");
		expect(followUp->proposal_text() == updated, "the proposal is the rewritten comment: " + followUp->proposal_text().toStdString());
		expect(!report->dressed_view()->toPlainText().contains(QStringLiteral("Suggested comment, updated:")), "the finding's comment is unchanged before Use");
		render(*followUp, QStringLiteral("follow-up-update-suggestion"));
		followUp->use_button()->click();
		expect(followUp->status_text().startsWith(QStringLiteral("Suggested comment updated.")), "it says the comment was updated: " + followUp->status_text().toStdString());

		// The report: one line under the finding says Draft comment drafts
		// the update, the comment itself is not written out again, and Draft
		// comment drafts it.
		const auto draftsTheUpdate = QStringLiteral("Draft comment drafts the comment as the follow-up updated it.");
		wait([&] { return report->dressed_view()->toPlainText().contains(draftsTheUpdate); }, "the finding says Draft comment drafts the update:\n" + report->dressed_view()->toPlainText().toStdString());
		{
			const auto page = report->dressed_view()->toPlainText();
			expect(page.count(draftsTheUpdate) == 1 && !page.contains(QStringLiteral("Suggested comment, updated")) && !page.contains(QStringLiteral("clamp the height as well as the width")),
				   "the updated comment is not restated under its finding:\n" + page.toStdString());
			// A finding no follow-up updated keeps its suggested comment's
			// words. Its suggestion is the Suggested change drawn above
			// them, once: the page does not write its lines out again in a
			// box under the words, and says the change goes with them.
			const auto suggestedLine = QStringLiteral("const auto height = std::max(aSize.Height, 1);");
			auto       changes       = 0;
			for (const auto* const view : report->code_views()) {
				changes += view->rendered_view()->toPlainText().contains(suggestedLine) ? 1 : 0;
			}
			expect(page.contains(QStringLiteral("Suggested comment, with the suggested change above:")) && page.contains(QStringLiteral("Clamp both.")),
				   "a finding no follow-up updated still shows its suggested comment's words, and says its change is the one above:\n" + page.toStdString());
			expect(changes == 1 && !page.contains(suggestedLine), "its suggestion shows once, as the Suggested change, not again under the words: in " + std::to_string(changes) + " code boxes, and the page:\n" + page.toStdString());
		}
		render(*report, QStringLiteral("follow-up-report"));
		render(*report->dressed_view(), QStringLiteral("follow-up-report-page"));
		followUp->close_button()->click();
		expect(!followUp->isVisible(), "Close closes the chat");
		expect(report->press(issue.Outline, qt::report_link::ask), "Ask follow-up is pressed on the report again");
		wait([&] { return followUp->isVisible() && followUp->answers().size() == 2; }, "the same chat, its two rounds");
		expect(window.report_shown() && report->isVisible() && followUp->mapTo(&window, QPoint{ 0, 0 }).x() >= report->mapTo(&window, QPoint{ report->width(), 0 }).x() && onlyTheWindow(),
			   "the chat stands beside the report, in the window");
		render(window, QStringLiteral("side-chat-report"));
		// Beside its side chat the report is narrow: its rows stay inside the
		// page and their sentences break between words, not a letter a line.
		expect(e2e::report_rows_wrong(*report).empty(), "beside its side chat the report's rows are whole, " + std::to_string(report->column()->width()) + " px wide: " + e2e::report_rows_wrong(*report));
		expect(report->press(issue.Outline, qt::report_link::keep), "Draft comment is pressed");
		wait([&] {
			const auto phase = report->keep_state(issue.Outline).Phase;
			return phase == qt::report_keep_phase::kept || phase == qt::report_keep_phase::refused;
		},
			 "Draft comment is answered");
		expect(report->keep_state(issue.Outline).Phase == qt::report_keep_phase::kept, "the finding is drafted: " + report->keep_state(issue.Outline).Why.toStdString());
		const auto tonemap = QStringLiteral("src/engine/shaders/post/tonemap.frag");
		const auto isDraft = [&tonemap](const qt::thread_card& aCard) { return aCard.Draft && (aCard.Path == tonemap || aCard.Path.isEmpty()) && (aCard.Line == 5U || aCard.StartLine == 5U || aCard.FirstLine == 5U); };
		if (!e2e::wait_until_plain([&] { return std::ranges::any_of(cards->cards(), isDraft); }, e2e::extended_agent_budget)) {
			auto       listed = std::string{ };
			const auto number = [](const std::optional<std::uint32_t>& aLine) { return aLine ? std::to_string(*aLine) : std::string{ "-" }; };
			for (const auto& each : cards->cards()) {
				listed += each.Path.toStdString() + ":" + number(each.Line) + "/" + number(each.StartLine) + "/" + number(each.FirstLine) + (each.Draft ? " draft" : "") + "; ";
			}
			throw std::runtime_error{ "a draft stands at the finding's line: " + listed };
		}
		const auto drafted = std::ranges::find_if(cards->cards(), isDraft);
		cards->open_thread(drafted->Id);
		wait([&] { return cards->field_text(drafted->Id).value_or(QString{ }).contains(QStringLiteral("clamp the height as well as the width")); },
			 "the draft holds the updated comment: " + cards->field_text(drafted->Id).value_or(QStringLiteral("<not open>")).toStdString());
		expect(unpublished(), "nothing is published");

		// [[BPR-2325]]. The draft deleted, the finding offers Draft comment
		// again, and drafts the updated text again.
		{
			auto* remove = static_cast<QAbstractButton*>(nullptr);
			wait([&] {
				if (!cards->writing_draft()) {
					cards->open_thread(drafted->Id);
				}
				QApplication::processEvents();
				remove = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadDeleteDraft")) : nullptr;
				return remove != nullptr && remove->isEnabled() && remove->isVisibleTo(&window);
			},
				 "the draft's Delete is offered");
			remove->click();
			QApplication::processEvents();
			auto* const question = window.delete_draft_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteDraftConfirmationConfirm")) : nullptr;
			expect(yes != nullptr && question->isVisible(), "Delete asks first");
			yes->click();
			wait([&] { return std::ranges::none_of(cards->cards(), isDraft); }, "the draft is deleted");
			wait([&] { return report->keep_state(issue.Outline).Phase == qt::report_keep_phase::offered; }, "the report's finding no longer says Drafted");
			const auto again = report->dressed_view()->toPlainText();
			expect(again.contains(QStringLiteral("Draft comment")), "the report offers Draft comment again");
			expect(report->press(issue.Outline, qt::report_link::keep), "Draft comment is pressed again");
			wait([&] { return report->keep_state(issue.Outline).Phase == qt::report_keep_phase::kept; }, "the finding is drafted again: " + report->keep_state(issue.Outline).Why.toStdString());
			wait([&] { return std::ranges::any_of(cards->cards(), isDraft); }, "a draft stands at the finding's line again");
			const auto redrafted = std::ranges::find_if(cards->cards(), isDraft);
			cards->open_thread(redrafted->Id);
			wait([&] { return cards->field_text(redrafted->Id).value_or(QString{ }).contains(QStringLiteral("clamp the height as well as the width")); }, "with the finding's updated text");
			expect(unpublished(), "nothing is published");
		}

		// [[BPR-2376]]. A drafted finding with a suggested change: an update
		// whose comment carries another suggestion changes the finding's
		// Suggested change and its draft with it; a draft the reviewer changed
		// since is left as it is, the finding says so and offers Update draft.
		{
			const auto& fix = report->findings().at(0);
			expect(fix.Path == "src/engine/renderer.cpp" && fix.First == 12U && fix.Last == 13U && fix.Comment.find("```suggestion") != std::string::npos, "the report's first finding is the renderer's suggested change");
			const auto oldLine   = QStringLiteral("Target.resize(extent{ width, height });");
			const auto newLine   = QStringLiteral("Target.resize(clamped);");
			const auto thirdLine = QStringLiteral("Target.resize(extent{ std::max(aSize.Width, 1), std::max(aSize.Height, 1) });");
			// The page's code boxes, a suggested change among them, lie over
			// the page in their own readers.
			const auto codeText = [&] {
				auto text = QString{ };
				for (const auto* const view : report->code_views()) {
					text += view->rendered_view()->toPlainText() + QStringLiteral("\n---\n");
				}
				return text;
			};
			const auto codeShows    = [&](const QString& aLine) { return codeText().contains(aLine); };
			const auto renderer     = QStringLiteral("src/engine/renderer.cpp");
			const auto isFixDraft   = [&renderer](const qt::thread_card& aCard) { return aCard.Draft && (aCard.Path == renderer || aCard.Path.isEmpty()) && aCard.Line == 13U; };
			const auto fixDraftText = [&]() -> QString {
				const auto found = std::ranges::find_if(cards->cards(), isFixDraft);
				if (found == cards->cards().end()) {
					return QStringLiteral("<no draft>");
				}
				cards->open_thread(found->Id);
				QApplication::processEvents();
				return cards->field_text(found->Id).value_or(QStringLiteral("<not open>"));
			};
			expect(codeShows(oldLine), "the finding's Suggested change shows its replacement lines: " + codeText().toStdString());
			expect(report->press(fix.Outline, qt::report_link::keep), "Draft comment is pressed on the suggested change");
			wait([&] { return report->keep_state(fix.Outline).Phase == qt::report_keep_phase::kept; }, "the suggested change is drafted: " + report->keep_state(fix.Outline).Why.toStdString());
			if (!e2e::wait_until_plain([&] { return fixDraftText().contains(oldLine); }, e2e::extended_agent_budget)) {
				auto       listed = std::string{ };
				const auto number = [](const std::optional<std::uint32_t>& aLine) { return aLine ? std::to_string(*aLine) : std::string{ "-" }; };
				for (const auto& each : cards->cards()) {
					listed += each.Path.toStdString() + ":" + number(each.Line) + "/" + number(each.StartLine) + "/" + number(each.FirstLine) + (each.Draft ? " draft" : "") + "; ";
				}
				throw std::runtime_error{ "its draft holds the suggestion: " + fixDraftText().toStdString() + " among " + listed };
			}

			// Unchanged since it was drafted: the update replaces it too.
			expect(report->press(fix.Outline, qt::report_link::ask), "Ask follow-up on the drafted finding");
			wait([&] { return followUp->isVisible() && followUp->subject_text() == QStringLiteral("Suggested comment at renderer.cpp:12–13"); }, "the side chat about the drafted finding: " + followUp->subject_text().toStdString());
			propose(*followUp, QStringLiteral("Update suggested comment"));
			expect(followUp->proposal_text().contains(newLine), "the proposal carries another suggestion: " + followUp->proposal_text().toStdString());
			followUp->use_button()->click();
			if (!e2e::wait_until_plain([&] { return codeShows(newLine) && !codeShows(oldLine); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the finding's Suggested change shows the updated comment's lines, not the old replacement: " + codeText().toStdString() };
			}
			{
				// Once, in the Suggested change: the page under it writes out
				// neither the update nor the comment it replaced.
				const auto page = report->dressed_view()->toPlainText();
				expect(!page.contains(oldLine) && !page.contains(newLine) && !page.contains(QStringLiteral("Suggested comment, updated")) && !page.contains(QStringLiteral("clamp both sides")),
					   "the updated comment is not restated under the drafted finding:\n" + page.toStdString());
			}
			wait([&] { return fixDraftText().contains(newLine) && !fixDraftText().contains(oldLine); }, "the unchanged draft follows the updated comment: " + fixDraftText().toStdString());
			expect(followUp->status_text().startsWith(QStringLiteral("Suggested comment and its draft updated.")), "it says both were updated: " + followUp->status_text().toStdString());
			render(*report->dressed_view(), QStringLiteral("follow-up-draft-updated-page"));
			expect(!report->dressed_view()->toPlainText().contains(QStringLiteral("Update draft")), "nothing to update: the draft is the comment");

			// Changed since: left as it is, and offered.
			const auto edited = fixDraftText() + QStringLiteral("\n\nAlso worth a test.");
			auto*      field  = cards->draft_editor();
			expect(field != nullptr, "the draft is open in the drawer");
			field->setPlainText(edited);
			wait([&] { return window.review_editor()->shown_draft() && window.review_editor()->shown_draft()->Body == edited.toStdString(); }, "the reviewer's change is the draft's");
			expect(report->press(fix.Outline, qt::report_link::ask), "Ask follow-up on the finding again");
			wait([&] { return followUp->isVisible(); }, "the side chat opens again");
			propose(*followUp, QStringLiteral("Update suggested comment"));
			expect(followUp->proposal_text().contains(thirdLine), "the proposal carries a third suggestion: " + followUp->proposal_text().toStdString());
			followUp->use_button()->click();
			if (!e2e::wait_until_plain([&] { return codeShows(thirdLine) && !codeShows(newLine); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the finding's Suggested change shows the third suggestion and not the second: " + codeText().toStdString() };
			}
			expect(!report->dressed_view()->toPlainText().contains(thirdLine), "the third suggestion shows once, in the Suggested change, not again under the finding");
			expect(followUp->status_text().contains(QStringLiteral("left as it is")), "it says the changed draft was left: " + followUp->status_text().toStdString());
			expect(fixDraftText() == edited, "the reviewer's draft is not replaced: " + fixDraftText().toStdString());
			wait([&] { return report->dressed_view()->toPlainText().contains(QStringLiteral("Update draft")); }, "the finding offers Update draft:\n" + report->dressed_view()->toPlainText().toStdString());
			expect(report->dressed_view()->toPlainText().contains(QStringLiteral("The draft differs from the updated suggested comment")), "and says why");
			render(*report, QStringLiteral("follow-up-draft-differs"));
			render(*report->dressed_view(), QStringLiteral("follow-up-draft-differs-page"));
			render(window, QStringLiteral("side-chat-report-draft-differs"));
			expect(followUp->isVisible() && e2e::report_rows_wrong(*report).empty(),
				   "beside its side chat the rows of a drafted finding and of one whose draft differs are whole, " + std::to_string(report->column()->width()) + " px wide: " + e2e::report_rows_wrong(*report));
			expect(report->press(fix.Outline, qt::report_link::update_draft), "Update draft is pressed");
			wait([&] { return fixDraftText().contains(thirdLine) && !fixDraftText().contains(QStringLiteral("Also worth a test.")); }, "Update draft replaces the draft: " + fixDraftText().toStdString());
			wait([&] { return !report->dressed_view()->toPlainText().contains(QStringLiteral("Update draft")); }, "and is no longer offered");
			expect(!report->dressed_view()->toPlainText().contains(QStringLiteral("The draft differs")), "nor does the finding still say the draft differs");
			render(*report->dressed_view(), QStringLiteral("follow-up-draft-in-step-page"));
			expect(unpublished(), "nothing is published");

			// Go to draft from the updated finding: the draft closed and the
			// right tray on the Agent tab, as a reviewer who went on leaves
			// them. Comments comes in front and the draft opens.
			const auto  updatedDraft = fixDraftText();
			auto* const close        = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadClose")) : nullptr;
			expect(cards->writing_draft() && close != nullptr && close->isEnabled(), "the open draft offers Close");
			close->click();
			wait([&] { return !cards->writing_draft(); }, "the draft is left a closed note");
			window.open_agent_question();
			const auto trayShows = [&window] { return window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()); };
			wait([&] { return trayShows().startsWith(QStringLiteral("Agent")); }, "the right tray shows the Agent tab");
			wait([&] { return window.report_shown() && report->isVisible(); }, "the report is in front");
			// The window a person presses in is the active one; the Delete
			// question above left none active offscreen.
			window.activateWindow();
			wait([&] { return QApplication::activeWindow() == &window; }, "the window is the active one");
			expect(report->press(fix.Outline, qt::report_link::go), "Go to draft is pressed on the updated finding");
			wait([&] { return window.active_document_path() == renderer && !window.report_shown(); }, "Go to draft shows the file");
			if (!e2e::wait_until_plain([&] { return trayShows().startsWith(QStringLiteral("Comments")) && cards->writing_draft() && cards->draft_editor() != nullptr; }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "Go to draft turns the right tray to Comments and opens the draft; the tray shows " + trayShows().toStdString() + (cards->writing_draft() ? ", the draft open" : ", the draft closed") };
			}
			render(window, QStringLiteral("follow-up-go-to-draft"));
			expect(cards->draft_editor()->toPlainText() == updatedDraft && updatedDraft.contains(thirdLine), "the open draft is the finding's, holding the update: " + cards->draft_editor()->toPlainText().toStdString());
			// The draft gone to shows its first column: its words from
			// their first letter, and its suggestion's line, wider than the
			// drawer, from its start in its own box. The field has no
			// sideways range at any drawer width, so the caret at the end of
			// that line, where a suggestion's caret goes, cannot pan it.
			{
				auto* const editor   = cards->draft_editor();
				const auto  sideways = [editor] { return editor->horizontalScrollBar()->maximum() != 0 || editor->horizontalScrollBar()->value() != 0 || editor->cursorRect(QTextCursor{ editor->document() }).left() < 0; };
				const auto  placed   = [editor] {
					return "range " + std::to_string(editor->horizontalScrollBar()->maximum()) + ", at " + std::to_string(editor->horizontalScrollBar()->value()) + ", first letter at "
						+ std::to_string(editor->cursorRect(QTextCursor{ editor->document() }).left()) + " of " + std::to_string(editor->viewport()->width());
				};
				expect(!sideways(), "the draft gone to shows its words from their first letter: " + placed());
				auto* const box = editor->viewport()->findChild<QScrollBar*>(QStringLiteral("threadCodeBlockScroll"));
				expect(box != nullptr && box->isVisible() && box->maximum() > 0 && box->value() == 0,
					   "its suggestion's line is wider than the drawer and shows from its start: " + (box != nullptr ? "at " + std::to_string(box->value()) + " of " + std::to_string(box->maximum()) : std::string{ "no bar" }));
				// Its caret is where a suggestion's caret goes, at the end of
				// that line and so out of sight. A key that leaves it there
				// brings it into sight in its box, and the field stays.
				const auto code = editor->document()->findBlockByNumber(editor->document()->blockCount() - 2);
				expect(code.text() == thirdLine && editor->textCursor().block() == code && editor->textCursor().atBlockEnd(), "the caret is at the end of the suggestion's line: " + code.text().toStdString());
				const auto caretShows = [editor, box] {
					const auto at = editor->cursorRect();
					return box->maximum() > 0 && box->value() > 0 && at.left() >= 0 && at.right() <= editor->viewport()->width();
				};
				{
					const auto endOfLine = QKeySequence{ QKeySequence::MoveToEndOfLine }[0];
					auto       press     = QKeyEvent{ QEvent::KeyPress, endOfLine.key(), endOfLine.keyboardModifiers() };
					QApplication::sendEvent(editor, &press);
					QApplication::processEvents();
					expect(editor->textCursor().block() == code && editor->textCursor().atBlockEnd() && caretShows() && !sideways(), "the end-of-line key shows the caret in its box and leaves the field: box at " + std::to_string(box->value()) + ", " + placed());
				}
				// The drawer dragged through the widths a reviewer may leave
				// it at: the words wrap to each, and the caret sent to the
				// end of the long line moves the box, never the field.
				auto* const splitter = window.central_splitter();
				const auto  seam     = splitter->handle(3)->x();
				auto        widths   = std::vector<int>{ };
				for (auto wider = 0; wider <= 48; wider += 3) {
					splitter->move_handle(3, seam - wider);
					QApplication::processEvents();
					auto* const dragged = cards->draft_editor();
					expect(dragged == editor && cards->writing_draft(), "the draft stays open while its drawer is dragged");
					if (std::ranges::find(widths, editor->viewport()->width()) == widths.end()) {
						widths.push_back(editor->viewport()->width());
					}
					auto caret = editor->textCursor();
					caret.setPosition(0);
					editor->setTextCursor(caret);
					caret.setPosition(code.position() + code.length() - 1);
					editor->setTextCursor(caret);
					QApplication::processEvents();
					expect(!sideways(), "the drawer " + std::to_string(wider) + " px wider, with the caret at the end of the suggestion's line: " + placed());
					expect(caretShows(), "and the box scrolls to the caret the reviewer put there: box at " + std::to_string(box->value()) + " of " + std::to_string(box->maximum()));
				}
				expect(widths.size() > 8U, "the editor was drawn at each width: " + std::to_string(widths.size()) + " widths");
				splitter->move_handle(3, seam);
				QApplication::processEvents();
			}
			const auto* const focus = QApplication::focusWidget();
			expect(focus != nullptr && (focus == cards->open_card() || cards->open_card()->isAncestorOf(focus)),
				   "the draft holds the keyboard; it is with " + (focus != nullptr ? std::string{ focus->metaObject()->className() } + " " + focus->objectName().toStdString() : std::string{ "nothing" }));
			expect(unpublished(), "nothing is published by going to it");
		}

		// [[BPR-2322]]. The log says what was asked and applied, and the
		// anonymized export none of the words.
		{
			const auto capture = control->flush_for_export(false);
			expect(capture.has_value(), "the log is flushed for export");
			const auto logs = tests::read_diagnostic_logs(*dataRoot);
			for (const auto* const line : { "action=side_chat_ask failure=none", "action=side_chat_propose failure=none", "action=side_chat_apply failure=none", "apply=update_suggestion", "apply=update_draft", "apply=draft_reply",
											"action=side_chat_apply failure=refused", "refusal=target_changed", "item=thread", "item=draft", "item=suggested_comment",
											// [[BPR-2325]].
											"action=finding_draft_cleared failure=none process=none exit=none duration_ms=0 count=1" }) {
				expect(logs.find(line) != std::string::npos, std::string{ "the log has a side chat line with " } + line);
			}
			const auto document = application::prepare_diagnostic_export(logs, core::release_maintenance_revision{ 1U }, { .Policy = core::diagnostic_export_policy::anonymized }, *capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
			expect(document && document->Descriptor.Records > 0U && document->Bytes.find("\"op\":\"agent.side_chat\"") != std::string::npos && document->Bytes.find("side_chat_apply") != std::string::npos,
				   "the anonymized export keeps the side chat lines");
			// Words the reviewer asked, the agent answered or proposed, and
			// the items quoted: none reaches the log or the export.
			for (const auto* const marker : { "Is this clear enough for the author", "Is the reviewer right", "Why is this blocking", "Can it be softer", "Fixture follow-up: I read the item", "does a zero height still reach",
											  "Thanks for checking", "clamp the height as well as the width", "clamp both sides", "The tonemap pass ignores exposure", "this seems wrong" }) {
				expect(document->Bytes.find(marker) == std::string::npos, std::string{ "the anonymized export holds none of the words: " } + marker);
				expect(logs.find(marker) == std::string::npos, std::string{ "nor does the log: " } + marker);
			}
		}

		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << "follow-up:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "follow-up: " << aError.what() << '\n';
		if (application) {
			application->begin_shutdown();
			static_cast<void>(e2e::wait_until_plain([&] { return application->status().ShutdownComplete; }, e2e::extended_agent_budget));
		}
		return EXIT_FAILURE;
	}
}
