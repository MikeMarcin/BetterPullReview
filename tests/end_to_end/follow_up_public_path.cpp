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
/// - A review's finding card has Ask follow-up: two rounds, each carrying the
///   ones before; Update suggested comment is seen first and replaces nothing
///   until Use; then the card shows it, the report shows it beside the
///   finding and offers Ask follow-up there (the same chat, beside the
///   report), and Draft comment drafts the updated text.
///
/// - [[BPR-2322]]: with Verbose on, every question, proposal and Use is an
///   `agent.side_chat` line in the diagnostic log, and an anonymized export
///   of that log holds none of the words asked, answered, proposed or quoted.
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
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
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
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
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

		// A review's finding card: Ask follow-up, two rounds, Update suggested comment.
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
		expect(shown.size() == 2U, "the review has two finding cards");
		// The tonemap finding: its card has no suggested comment of its own
		// (the thread's reply holds line 12 of the renderer meanwhile).
		auto* const card      = shown[1];
		auto* const askButton = card->findChild<QPushButton*>(QStringLiteral("agentFindingFollowUp"));
		expect(askButton != nullptr && askButton->isVisibleTo(tab), "the card offers Ask follow-up");
		const auto comment = [&card] {
			const auto* const view = dynamic_cast<qt::markdown_body_view*>(card->findChild<QWidget*>(QStringLiteral("agentFindingComment")));
			return view != nullptr ? view->original_body() : std::string{ "<no comment view>" };
		};
		const auto original = comment();
		askButton->click();
		wait([&] { return followUp->subject_text() == QStringLiteral("Suggested comment at tonemap.frag:5") && followUp->isVisible(); }, "the side chat about the finding: " + followUp->subject_text().toStdString());
		expect(followUp->item_text() == QStringLiteral("The tonemap pass ignores exposure."), "with no suggested comment, it quotes the claim: " + followUp->item_text().toStdString());
		expect(original.empty() && !card->findChild<QWidget*>(QStringLiteral("agentFindingSuggestion"))->isVisibleTo(card), "the card shows no suggested comment yet");
		ask(*followUp, QStringLiteral("I don't follow. Why is this blocking?"));
		ask(*followUp, QStringLiteral("Can it be softer?"));
		expect(followUp->answers().back().startsWith(QStringLiteral("Fixture follow-up: I read the item and 1 earlier rounds.")), "the second round carried the first: " + followUp->answers().back().toStdString());
		propose(*followUp, QStringLiteral("Update suggested comment"));
		const auto updated = QStringLiteral("issue (blocking): clamp the height as well as the width. A minimised window reports a height of zero, and the swap chain asserts on it.");
		expect(followUp->proposal_text() == updated, "the proposal is the rewritten comment: " + followUp->proposal_text().toStdString());
		expect(comment() == original, "the card's comment is unchanged before Use");
		render(*followUp, QStringLiteral("follow-up-update-suggestion"));
		followUp->use_button()->click();
		expect(followUp->status_text().startsWith(QStringLiteral("Suggested comment updated.")), "it says the comment was updated: " + followUp->status_text().toStdString());
		wait([&] { return comment().find("clamp the height as well as the width") != std::string::npos && card->findChild<QWidget*>(QStringLiteral("agentFindingSuggestion"))->isVisibleTo(card); }, "the card shows the updated comment: " + comment());

		// The report: the update beside the finding, the same conversation, and Draft comment drafts it.
		auto* const openRow = tab->turn_report_card(before)->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(openRow != nullptr, "the card has its Open report row");
		openRow->click();
		auto* const report = window.report_view();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens");
		QApplication::processEvents();
		const auto& issue = report->findings().at(1);
		expect(issue.Card == std::optional<std::size_t>{ 1U }, "the report's second finding is the tonemap card");
		auto page = report->dressed_view()->toPlainText();
		expect(page.contains(QStringLiteral("Ask follow-up")) && page.contains(QStringLiteral("Suggested comment, updated:")) && page.contains(QStringLiteral("clamp the height as well as the width")),
			   "the report offers Ask follow-up and shows the updated comment beside the finding:\n" + page.toStdString());
		render(*report, QStringLiteral("follow-up-report"));
		followUp->close_button()->click();
		expect(!followUp->isVisible(), "Close closes the chat");
		expect(report->press(issue.Outline, qt::report_link::ask), "Ask follow-up is pressed on the report");
		wait([&] { return followUp->isVisible() && followUp->answers().size() == 2; }, "the report's finding is the card's: the same chat, its two rounds");
		expect(window.report_shown() && report->isVisible() && followUp->mapTo(&window, QPoint{ 0, 0 }).x() >= report->mapTo(&window, QPoint{ report->width(), 0 }).x() && onlyTheWindow(),
			   "the chat stands beside the report, in the window");
		render(window, QStringLiteral("side-chat-report"));
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

		// [[BPR-2322]]. The log says what was asked and applied, and the
		// anonymized export none of the words.
		{
			const auto capture = control->flush_for_export(false);
			expect(capture.has_value(), "the log is flushed for export");
			const auto logs = tests::read_diagnostic_logs(*dataRoot);
			for (const auto* const line : { "action=side_chat_ask failure=none", "action=side_chat_propose failure=none", "action=side_chat_apply failure=none", "apply=update_suggestion", "apply=update_draft", "apply=draft_reply",
											"action=side_chat_apply failure=refused", "refusal=target_changed", "item=thread", "item=draft", "item=suggested_comment" }) {
				expect(logs.find(line) != std::string::npos, std::string{ "the log has a side chat line with " } + line);
			}
			const auto document = application::prepare_diagnostic_export(logs, core::release_maintenance_revision{ 1U }, { .Policy = core::diagnostic_export_policy::anonymized }, *capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
			expect(document && document->Descriptor.Records > 0U && document->Bytes.find("\"op\":\"agent.side_chat\"") != std::string::npos && document->Bytes.find("side_chat_apply") != std::string::npos,
				   "the anonymized export keeps the side chat lines");
			// Words the reviewer asked, the agent answered or proposed, and
			// the items quoted: none reaches the log or the export.
			for (const auto* const marker : { "Is this clear enough for the author", "Is the reviewer right", "Why is this blocking", "Can it be softer", "Fixture follow-up: I read the item", "does a zero height still reach",
											  "Thanks for checking", "clamp the height as well as the width", "The tonemap pass ignores exposure", "this seems wrong" }) {
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
