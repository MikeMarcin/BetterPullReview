/// \file
/// \brief [[BPR-2322]]. The side chat about one review item, which replaced
/// [[BPR-2286]]'s follow-up window, and the controls that open it: it is a
/// column, not a window; each press leaves as an intent; the reviewer's words
/// and the agent's answers are turns; a proposal waits in the chat beside
/// what it replaces and nothing is applied until Use; Send and the apply
/// action wear the Agent accent and Stop the ordinary roles; closing keeps
/// the chat. A report's finding offers Ask follow-up and shows its updated
/// suggested comment; a thread's Ask menu offers Ask about this. Nothing here
/// runs an agent or writes anywhere.
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/report_page.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "item-side-chat-tests: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// The child named `aName` the chat shows now: a chat drawn again deletes
	/// what it drew before, later; that is done first.
	template <typename Widget>
	auto shown(const QWidget& aParent, const QString& aName) -> Widget*
	{
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		return aParent.findChild<Widget*>(aName);
	}

	auto suggestion() -> core::follow_up_subject
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:7:0";
		subject.Path  = "src/engine/renderer.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "A zero height still reaches the swap chain.";
		subject.Text  = "issue (blocking): clamp both.";
		return subject;
	}

	struct pressed
	{
		std::vector<QString>                 Asked;
		std::vector<core::follow_up_apply>   Proposed;
		std::vector<std::uint64_t>           Used;
		std::vector<std::uint64_t>           Discarded;
		std::vector<core::follow_up_subject> Revealed;
		int                                  Stops  = 0;
		int                                  Closed = 0;
	};

	auto intents(pressed& aPressed) -> qt::item_side_chat_intents
	{
		return {
			.Ask = [&aPressed](const QString& aQuestion) { aPressed.Asked.push_back(aQuestion); },
			.Propose = [&aPressed](const core::follow_up_apply aApply) { aPressed.Proposed.push_back(aApply); },
			.Use = [&aPressed](const std::uint64_t aId) { aPressed.Used.push_back(aId); },
			.Discard = [&aPressed](const std::uint64_t aId) { aPressed.Discarded.push_back(aId); },
			.Stop = [&aPressed] { ++aPressed.Stops; },
			.Closed = [&aPressed] { ++aPressed.Closed; },
			.Reveal = [&aPressed](const core::follow_up_subject& aSubject) { aPressed.Revealed.push_back(aSubject); },
		};
	}

	/// Open, ask about the item, a running turn and its answer, Update
	/// suggested comment, Use and Discard, refusals, close and reopen.
	void test_the_chat_asks_about_an_item_and_revises_its_comment()
	{
		auto page = QWidget{ };
		auto chat = new qt::item_side_chat{ &page };
		auto done = pressed{ };
		chat->set_intents(intents(done));
		page.resize(420, 640);
		chat->resize(400, 620);
		page.show();
		QApplication::processEvents();
		expect(!chat->isWindow() && !chat->isVisible() && !chat->column_open(), "the chat is a column in its page, closed until an item is asked about");

		auto state = core::open_follow_up(suggestion());
		chat->present(state, QStringLiteral("Fixture agent"), { });
		chat->set_column_open(true);
		QApplication::processEvents();
		expect(chat->isVisible() && chat->column_open() && chat->composer()->hasFocus(), "opening shows the column with the keyboard in its composer");
		expect(chat->subject_text() == QStringLiteral("Suggested comment at renderer.cpp:12–13"), "the title names the item and its place: " + chat->subject_text().toStdString());
		expect(chat->item_text() == QStringLiteral("issue (blocking): clamp both."), "the item is quoted at the chat's top");
		expect(chat->chip_text() == QStringLiteral("issue (blocking): clamp both."), "the composer carries the item as its chip");
		expect(chat->findChild<QLabel*>(QStringLiteral("itemSideChatQuote"))->text() == QStringLiteral("A zero height still reaches the swap chain."), "under the title, the finding's claim");
		expect(chat->apply_button()->isVisibleTo(chat) && chat->apply_button()->text() == QStringLiteral("Update suggested comment"), "a suggestion offers Update suggested comment");
		expect(qt::agent_run_control(*chat->send_button()) && qt::agent_run_control(*chat->apply_button()), "Send and the apply action start runs: the Agent accent");
		expect(chat->proposal_card() == nullptr && chat->questions().isEmpty(), "no turns and no proposal yet");
		expect(shown<QLabel>(*chat, QStringLiteral("itemSideChatEmpty")) != nullptr, "an empty chat invites a question");
		chat->show_button()->click();
		expect(done.Revealed.size() == 1U && done.Revealed.front().Key == "finding:7:0", "Show asks for the item's place");

		// Return sends; Shift+Return is a new line; a blank question is not sent.
		chat->composer()->setPlainText(QStringLiteral("Why is this blocking?"));
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(chat->composer(), &enter);
		expect(done.Asked == std::vector{ QStringLiteral("Why is this blocking?") } && chat->composer()->toPlainText().isEmpty(), "Return sends and clears the composer");
		auto shiftEnter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier };
		QApplication::sendEvent(chat->composer(), &shiftEnter);
		expect(done.Asked.size() == 1U, "Shift+Return sends nothing");
		chat->send_button()->click();
		expect(done.Asked.size() == 1U && chat->status_text() == qt::item_chat_refusal_words(core::follow_up_refusal::empty_question), "a blank question is not sent, and it says so");

		// A run going: the question is a turn, the Agent tab's live line under
		// it, Send is Stop in the ordinary roles, and the apply action waits.
		static_cast<void>(core::ask_follow_up(state, "Why is this blocking?"));
		chat->present(state, QStringLiteral("Fixture agent"), { });
		expect(chat->running() && chat->questions() == QStringList{ QStringLiteral("Why is this blocking?") }, "the question shows at once, as the reviewer's turn");
		expect(chat->send_button()->text() == QStringLiteral("Stop") && !qt::agent_run_control(*chat->send_button()) && !chat->apply_button()->isEnabled(), "while it answers, Send is Stop in the ordinary roles and Update waits");
		auto* const live = dynamic_cast<qt::agent_run_status_line*>(shown<QLabel>(*chat, QStringLiteral("itemSideChatEnding")));
		expect(live != nullptr && live->live() && live->text().contains(QStringLiteral("Fixture agent")), "the running turn's line names the agent, as the Agent tab's does");
		chat->send_button()->click();
		expect(done.Stops == 1 && done.Asked.size() == 1U, "Stop stops; it asks nothing");
		static_cast<void>(core::settle_follow_up(state, state.Pending, std::string{ "It asserts on **minimise**." }));
		chat->present(state, QStringLiteral("Fixture agent"), { });
		QApplication::processEvents();
		expect(!chat->running() && chat->answers() == QStringList{ QStringLiteral("It asserts on **minimise**.") }, "the answer is the agent's turn, as Markdown");
		expect(chat->status_line(0U) != nullptr && !chat->status_line(0U)->live() && chat->status_line(0U)->text().startsWith(QStringLiteral("Fixture agent answered")), "the answer ends with who answered: " + (chat->status_line(0U) != nullptr ? chat->status_line(0U)->text().toStdString() : std::string{ }));
		expect(shown<QLabel>(*chat, QStringLiteral("itemSideChatEmpty")) == nullptr, "a chat with turns no longer invites");

		// Update: the proposal in the chat beside what it replaces; nothing
		// changes until Use.
		chat->apply_button()->click();
		expect(done.Proposed == std::vector{ core::follow_up_apply::update_suggestion }, "the apply action asks for the proposal");
		const auto step = core::propose_follow_up(state, core::follow_up_apply::update_suggestion, state.Subject.Text);
		chat->present(state, QStringLiteral("Fixture agent"), { });
		expect(chat->running() && chat->questions().size() == 2U && chat->questions().back() == QStringLiteral("Update suggested comment"), "the request for the update is a turn while it runs");
		static_cast<void>(core::settle_follow_up(state, step.Request->Id, std::string{ "issue (blocking): clamp the height too." }));
		chat->present(state, QStringLiteral("Fixture agent"), { });
		QApplication::processEvents();
		expect(chat->proposal_card() != nullptr && chat->proposal_card()->isVisibleTo(chat) && chat->proposal_text() == QStringLiteral("issue (blocking): clamp the height too."), "the proposal waits in the chat");
		expect(shown<QLabel>(*chat, QStringLiteral("itemSideChatProposalBefore"))->text().contains(QStringLiteral("issue (blocking): clamp both.")), "beside what it replaces");
		expect(chat->item_text() == QStringLiteral("issue (blocking): clamp both."), "the item is unchanged before Use");
		expect(chat->use_button() != nullptr && chat->use_button()->text() == QStringLiteral("Use this") && chat->use_button()->hasFocus(), "the keyboard is on Use");
		chat->discard_button()->click();
		chat->use_button()->click();
		expect(done.Discarded == std::vector{ state.Proposal->Id } && done.Used == std::vector{ state.Proposal->Id }, "Discard and Use name the proposal shown");

		// A refusal and a failure are said in words.
		static_cast<void>(core::apply_follow_up(state, state.Proposal->Id, "changed meanwhile"));
		chat->present(state, QStringLiteral("Fixture agent"), { });
		expect(chat->status_text() == qt::item_chat_refusal_words(core::follow_up_refusal::target_changed) && chat->status_text().contains(QStringLiteral("Nothing was replaced")), "a changed text is refused in words");
		chat->present(state, QStringLiteral("Fixture agent"), QStringLiteral("The agent did not answer (timeout)."));
		expect(chat->status_text() == QStringLiteral("The agent did not answer (timeout)."), "a run's failure is said as the host said it");

		// Copy chat: the item and the turns as text.
		chat->copy_chat_button()->click();
		const auto copied = QGuiApplication::clipboard()->text();
		expect(copied.contains(QStringLiteral("Suggested comment at renderer.cpp:12–13")) && copied.contains(QStringLiteral("**You:** Why is this blocking?")) && copied.contains(QStringLiteral("It asserts on **minimise**.")), "Copy chat copies the chat: " + copied.toStdString());

		// Close keeps the chat; opening the same item shows its turns again.
		chat->close_button()->click();
		expect(!chat->isVisible() && !chat->column_open() && done.Closed == 1, "Close closes the column and says so");
		chat->present(state, QStringLiteral("Fixture agent"), { });
		chat->set_column_open(true);
		expect(chat->isVisible() && chat->answers().size() == 1 && chat->questions().size() == 1, "reopened, the item's turns are there");
	}

	/// Ask about this on a draft and on a thread: Update draft, Draft reply.
	void test_a_draft_and_a_thread_offer_their_apply_action()
	{
		auto page = QWidget{ };
		auto chat = new qt::item_side_chat{ &page };
		auto done = pressed{ };
		chat->set_intents(intents(done));
		page.resize(420, 640);
		page.show();
		chat->set_column_open(true);

		auto draft  = suggestion();
		draft.Kind  = core::follow_up_subject_kind::draft;
		draft.Key   = "draft";
		draft.Title = { };
		draft.Text  = "this seems wrong?";
		auto state  = core::open_follow_up(draft);
		chat->present(state, { }, { });
		expect(chat->apply_button()->text() == QStringLiteral("Update draft") && chat->subject_text().startsWith(QStringLiteral("Your draft at")), "a draft offers Update draft");
		expect(chat->chip_text() == QStringLiteral("this seems wrong?"), "the draft is the composer's chip");
		const auto step = core::propose_follow_up(state, core::follow_up_apply::update_draft, "this seems wrong?");
		static_cast<void>(core::settle_follow_up(state, step.Request->Id, std::string{ "question: does a zero height reach the swap chain?" }));
		chat->present(state, { }, { });
		expect(chat->proposal_text() == QStringLiteral("question: does a zero height reach the swap chain?") && chat->use_button()->text() == QStringLiteral("Use this"), "Update draft proposes the draft");

		// A thread offers Draft reply; Use says where it goes.
		auto thread    = core::follow_up_subject{ };
		thread.Kind    = core::follow_up_subject_kind::thread;
		thread.Key     = "review-thread:1";
		thread.Path    = "src/engine/renderer.cpp";
		thread.First   = 12U;
		thread.Last    = 12U;
		thread.Replies = true;
		thread.Thread  = { { .Author = "octo-reviewer", .Body = "Should the height clamp too?" } };
		chat->composer()->setPlainText(QStringLiteral("left over"));
		auto talk = core::open_follow_up(thread);
		chat->present(talk, { }, { });
		expect(chat->subject_text() == QStringLiteral("Conversation at renderer.cpp:12") && chat->item_text() == QStringLiteral("octo-reviewer: Should the height clamp too?"), "a thread shows its posts");
		expect(chat->apply_button()->text() == QStringLiteral("Draft reply") && chat->answers().isEmpty() && chat->composer()->toPlainText().isEmpty() && chat->proposal_card() == nullptr, "another item: Draft reply, its own turns, a clear composer");
		const auto reply = core::propose_follow_up(talk, core::follow_up_apply::draft_reply, "");
		static_cast<void>(core::settle_follow_up(talk, reply.Request->Id, std::string{ "Thanks for checking. Yes, both." }));
		chat->present(talk, { }, { });
		expect(chat->use_button()->text() == QStringLiteral("Put in reply") && shown<QLabel>(*chat, QStringLiteral("itemSideChatProposalBefore")) == nullptr, "a reply into an empty composer: Put in reply, nothing it replaces");
		chat->use_button()->click();
		expect(done.Used == std::vector{ talk.Proposal->Id }, "Put in reply names the proposal");
		thread.Replies = false;
		chat->present(core::open_follow_up(thread), { }, { });
		expect(!chat->apply_button()->isVisibleTo(chat), "a thread that takes no reply offers no apply action");
	}

	/// The tour's chat is built from the same parts and still draws.
	void test_the_tour_chat_shares_the_parts()
	{
		// Each in a page, as the window places them: a column, not a window.
		auto        page = QWidget{ };
		auto* const tour = new qt::tour_side_chat{ &page };
		auto* const item = new qt::item_side_chat{ &page };
		tour->resize(380, 400);
		item->resize(380, 400);
		const auto tourImage = tour->grab().toImage();
		const auto itemImage = item->grab().toImage();
		expect(!tourImage.isNull() && !itemImage.isNull() && tourImage.pixelColor(0, 200) == itemImage.pixelColor(0, 200) && tourImage.pixelColor(0, 200) != page.palette().color(QPalette::Window),
			   "both chats draw the same seam, in the agent's accent, at their left edge");
		expect(dynamic_cast<qt::composer_field*>(tour->composer()->parentWidget()) != nullptr && dynamic_cast<qt::composer_field*>(item->composer()->parentWidget()) != nullptr, "both compose in the tour's composer field");
	}

	void test_a_report_finding_offers_ask_follow_up()
	{
		const auto report  = core::dress_report("# Review\n\n**Verdict:** Needs changes\n\nOne thing.\n\n## Must fix\n\n### issue (blocking): A zero height\n\n`src/engine/renderer.cpp:12-13`\n\nThe height is not clamped.\n");
		auto       actions = qt::report_page_actions{ .Findings = core::report_findings(report) };
		expect(!actions.Findings.empty(), "the report has a finding");
		const auto outline = actions.Findings.front().Outline;
		actions.Link       = [](const std::uint32_t aOutline, const qt::report_link aLink) { return QStringLiteral("action-%1-%2").arg(aOutline).arg(static_cast<int>(aLink)); };
		const auto askLink = QStringLiteral("action-%1-%2").arg(outline).arg(static_cast<int>(qt::report_link::ask));
		expect(!qt::report_page_html(report, { }, actions).contains(askLink), "without a side chat, no Ask follow-up");
		actions.FollowUp = true;
		const auto page  = qt::report_page_html(report, { }, actions);
		expect(page.contains(askLink) && page.contains(QStringLiteral("Ask follow-up")), "a finding offers Ask follow-up beside its keep action");
		expect(!page.contains(QStringLiteral("Suggested comment, updated")), "nothing updated yet");
		actions.Revised[outline] = QStringLiteral("issue (blocking): clamp <both>.");
		const auto revised       = qt::report_page_html(report, { }, actions);
		expect(revised.contains(QStringLiteral("Suggested comment, updated:")) && revised.contains(QStringLiteral("clamp &lt;both&gt;.")), "the updated comment shows, escaped");
	}

	void test_a_threads_ask_menu_offers_ask_about_this()
	{
		auto       followed = std::vector<QString>{ };
		auto       dispatch = qt::ask_menu_dispatch{ };
		const auto request  = qt::ask_menu_request{ .Anchor = core::quick_ask_anchor::thread, .File = { }, .Thread = QStringLiteral("review-thread:1"), .ThreadTitle = QStringLiteral("Thread on line 12") };
		{
			QMenu menu;
			qt::fill_ask_menu(menu, { }, request, dispatch);
			expect(menu.findChild<QAction*>(QStringLiteral("quickAskFollowUp")) == nullptr, "without a dispatch, no Ask about this");
		}
		dispatch.FollowUp = [&followed](const qt::ask_menu_request& aRequest) { followed.push_back(aRequest.Thread); };
		QMenu menu;
		qt::fill_ask_menu(menu, { }, request, dispatch);
		auto* const row = menu.findChild<QAction*>(QStringLiteral("quickAskFollowUp"));
		expect(row != nullptr && row->text() == QStringLiteral("Ask about this…"), "a thread's menu offers Ask about this");
		row->trigger();
		expect(followed == std::vector{ QStringLiteral("review-thread:1") }, "it names the thread");
		QMenu fileMenu;
		qt::fill_ask_menu(fileMenu, { }, qt::ask_menu_request{ .Anchor = core::quick_ask_anchor::file, .File = QStringLiteral("a.cpp"), .Thread = { }, .ThreadTitle = { } }, dispatch);
		expect(fileMenu.findChild<QAction*>(QStringLiteral("quickAskFollowUp")) == nullptr, "a file's menu has none");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_the_chat_asks_about_an_item_and_revises_its_comment();
	test_a_draft_and_a_thread_offer_their_apply_action();
	test_the_tour_chat_shares_the_parts();
	test_a_report_finding_offers_ask_follow_up();
	test_a_threads_ask_menu_offers_ask_about_this();
	std::cout << "item-side-chat-tests:pass\n";
	return EXIT_SUCCESS;
}
