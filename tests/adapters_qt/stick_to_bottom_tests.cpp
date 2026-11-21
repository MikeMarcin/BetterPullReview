/// \file
/// \brief [[BPR-2296]] (owner, 2026-11-19: "The the agent tab and any similar
/// session transcript window should Auto scroll to the bottom if you're at
/// the bottom when anything new gets added below", and "If tool rows grow and
/// then contract at the bottom it should stay at its lowest auto scroll value
/// it shouldn't ping pong up and down").
///
/// \details A transcript at its end follows what is appended and what grows
/// below; one scrolled up, by a wheel or the bar, stays where it is; while a
/// turn runs, a row that grows, shrinks and grows again never moves the view
/// back up, and the turn's end lets the held height go. Checked on a bare
/// scroll area with the nested scroll latch installed, on the Agent tab as a
/// turn streams, and on an item's side chat as rounds arrive.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/nested_scroll_latch.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <cstdlib>
#include <iostream>
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

	/// \brief Events for `aMilliseconds`: the Agent tab paces its answer.
	void wait(const int aMilliseconds)
	{
		auto loop = QEventLoop{ };
		QTimer::singleShot(aMilliseconds, &loop, &QEventLoop::quit);
		loop.exec();
		settle();
	}

	[[nodiscard]] auto at_end(const QScrollArea& aArea) -> bool
	{
		const auto* const bar = aArea.verticalScrollBar();
		return bar->maximum() > 0 && bar->value() == bar->maximum();
	}

	[[nodiscard]] auto where(const QScrollArea& aArea) -> std::string
	{
		return std::to_string(aArea.verticalScrollBar()->value()) + " of " + std::to_string(aArea.verticalScrollBar()->maximum());
	}

	/// \brief A reader's mouse-wheel notch up over the transcript.
	void wheel_up(QScrollArea& aArea)
	{
		auto* const viewport = aArea.viewport();
		const auto  centre   = QPointF{ viewport->rect().center() };
		auto        event    = QWheelEvent{ centre, viewport->mapToGlobal(centre), QPoint{ 0, 30 }, QPoint{ 0, 120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(viewport, &event);
		settle();
	}

	/// \brief Every value the bar takes, in order.
	class bar_values final
	{
	public:
		explicit bar_values(const QScrollArea& aArea)
		{
			QObject::connect(aArea.verticalScrollBar(), &QScrollBar::valueChanged, &Guard, [this](const int aValue) { Values.push_back(aValue); });
		}

		[[nodiscard]] auto never_fell() const -> bool
		{
			for (std::size_t index = 1U; index < Values.size(); ++index) {
				if (Values[index] < Values[index - 1U]) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto said() const -> std::string
		{
			auto words = std::string{ };
			for (const auto value : Values) {
				words += std::to_string(value) + " ";
			}
			return words;
		}

	private:
		QObject          Guard;
		std::vector<int> Values;
	};

	/// \brief A transcript like the views': a column of turns over a stretch.
	struct transcript
	{
		QWidget      Outer;
		QScrollArea* Area   = nullptr;
		QVBoxLayout* Column = nullptr;

		transcript()
		{
			auto* const outer = new QVBoxLayout{ &Outer };
			Area              = new QScrollArea{ &Outer };
			Area->setWidgetResizable(true);
			auto* const content = new QWidget{ Area };
			Column              = new QVBoxLayout{ content };
			Column->addStretch(1);
			Area->setWidget(content);
			outer->addWidget(Area);
			qt::stick_to_bottom::attach(*Area);
			Outer.resize(320, 240);
			Outer.show();
			settle();
		}

		auto append(const int aHeight) -> QWidget*
		{
			auto* const row = new QLabel{ QStringLiteral("A turn"), Area->widget() };
			row->setFixedHeight(aHeight);
			Column->insertWidget(Column->count() - 1, row);
			settle();
			return row;
		}
	};

	void test_a_transcript_at_its_end_follows_and_one_scrolled_up_stays()
	{
		qt::nested_scroll_latch::install();
		transcript chat;
		auto&      follow = qt::stick_to_bottom::attach(*chat.Area);
		expect(&follow == qt::stick_to_bottom::of(*chat.Area), "one follower per transcript");
		for (int turn = 0; turn < 6; ++turn) {
			static_cast<void>(chat.append(80));
		}
		expect(at_end(*chat.Area), "appended turns keep a transcript at its end at its end: " + where(*chat.Area));

		// A row that grows below, as streamed text or a disclosure does.
		auto* const row = chat.append(40);
		row->setFixedHeight(260);
		settle();
		expect(at_end(*chat.Area), "a row growing at the end keeps the transcript at its end: " + where(*chat.Area));

		// Within the slack of the end still counts as the end.
		auto* const bar = chat.Area->verticalScrollBar();
		bar->setValue(bar->maximum() - 40);
		bar->setValue(bar->maximum() - qt::stick_to_bottom::slack + 2);
		static_cast<void>(chat.append(60));
		expect(at_end(*chat.Area), "a transcript a few pixels from its end follows: " + where(*chat.Area));

		// Scrolled up by a wheel: nothing appended or grown moves it.
		wheel_up(*chat.Area);
		const auto readAt = bar->value();
		expect(readAt < bar->maximum() && !follow.pinned(), "a wheel up lets the end go: " + where(*chat.Area));
		static_cast<void>(chat.append(120));
		row->setFixedHeight(400);
		settle();
		expect(bar->value() == readAt, "a transcript scrolled up stays where the reader put it: " + where(*chat.Area));

		// A one-pixel nudge up by the bar lets go too.
		bar->setValue(bar->maximum());
		expect(follow.pinned(), "back at the end, it follows again");
		bar->setValue(bar->maximum() - 1);
		static_cast<void>(chat.append(50));
		expect(!follow.pinned() && bar->value() < bar->maximum() - 40, "a nudge up is not pulled back: " + where(*chat.Area));

		// Back at the end, by the reader, it follows again.
		bar->setValue(bar->maximum());
		static_cast<void>(chat.append(50));
		expect(at_end(*chat.Area), "back at the end, appended turns follow: " + where(*chat.Area));
		expect(qt::nested_scroll_latch::owner() == nullptr, "no gesture was claimed by following");
	}

	void test_tool_rows_that_grow_and_shrink_never_move_the_view_back_up()
	{
		transcript chat;
		auto&      follow = qt::stick_to_bottom::attach(*chat.Area);
		for (int turn = 0; turn < 4; ++turn) {
			static_cast<void>(chat.append(80));
		}
		follow.set_turn_running(true);
		auto* const tool = chat.append(30);
		auto        seen = bar_values{ *chat.Area };
		for (const auto height : { 200, 60, 240, 30, 300, 120, 320 }) {
			tool->setFixedHeight(height);
			settle();
			expect(follow.pinned(), "the transcript stays pinned at height " + std::to_string(height));
		}
		expect(seen.never_fell(), "grow, shrink, grow: the scroll value never decreases: " + seen.said());
		expect(at_end(*chat.Area), "it ends at its end: " + where(*chat.Area));
		expect(follow.floor() > 0, "the tallest height is held while the turn runs");

		// The turn's end lets the held height go; the transcript meets its end.
		tool->setFixedHeight(40);
		settle();
		follow.set_turn_running(false);
		settle();
		expect(follow.floor() == 0 && at_end(*chat.Area), "the turn's end settles at the real end: " + where(*chat.Area));
		expect(chat.Area->widget()->height() < 4 * 80 + 320, "and the room held under the row is gone");
	}

	void open_review(qt::workbench_window& aWindow)
	{
		aWindow.resize(1'440, 900);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 520U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(2);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::claude_stream_json_profile_id;
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		profile.Protocol    = core::agent_protocol::claude_stream_json;
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto* const selector = aWindow.agent_panel()->profile_selector();
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(profile.Id.value())));
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	void test_the_agent_tab_follows_a_streaming_turn()
	{
		qt::workbench_window window;
		open_review(window);
		auto* const tab    = window.agent_tab();
		auto* const scroll = tab->findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		expect(scroll != nullptr && qt::stick_to_bottom::of(*scroll) != nullptr, "the Agent tab's conversation follows its end");
		auto identity                 = application::agent_invocation_projection_identity{ };
		identity.ApplicationSession   = core::application_session_id{ 1U };
		identity.Session.Id           = core::agent_session_id{ 9U };
		identity.InvocationGeneration = 1U;
		window.agent_panel()->admit(identity);
		window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
		settle();
		expect(tab->state() == qt::agent_tab_state::working, "the turn runs");
		auto       sequence = std::uint64_t{ 0U };
		const auto stream   = [&](const int aParagraphs) {
			for (int paragraph = 0; paragraph < aParagraphs; ++paragraph) {
				// One long paragraph an event, well short of a report's forty lines.
				auto text = std::string{ };
				for (int sentence = 0; sentence < 8; ++sentence) {
					text += "The loop ends when the queue drains, and each pass reads one more line of the change. ";
				}
				window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = ++sequence, .Text = text });
				wait(120);
			}
		};
		stream(8);
		expect(at_end(*scroll), "streamed text keeps the tab at its end: " + where(*scroll));

		// A tool row below that grows, shrinks and grows: never back up.
		auto* const thread = tab->findChild<QWidget*>(QStringLiteral("agentThread"));
		expect(thread != nullptr && thread->layout() != nullptr, "the thread is there");
		auto* const tool = new QLabel{ QStringLiteral("Ran: cmake --build"), thread };
		tool->setFixedHeight(24);
		thread->layout()->addWidget(tool);
		settle();
		auto seen = bar_values{ *scroll };
		for (const auto height : { 180, 40, 220, 24, 260 }) {
			tool->setFixedHeight(height);
			settle();
		}
		expect(seen.never_fell(), "a tool row growing and shrinking never moves the tab back up: " + seen.said());
		expect(at_end(*scroll), "the tab is at its end after the tool row: " + where(*scroll));

		// Scrolled up: the stream does not move it.
		wheel_up(*scroll);
		wheel_up(*scroll);
		const auto readAt = scroll->verticalScrollBar()->value();
		expect(readAt < scroll->verticalScrollBar()->maximum(), "the reader scrolled up: " + where(*scroll));
		stream(3);
		tool->setFixedHeight(320);
		settle();
		expect(scroll->verticalScrollBar()->value() == readAt, "a tab scrolled up stays where the reader put it: " + where(*scroll));

		// Back at the end, it follows again, to the turn's end.
		scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
		stream(2);
		expect(at_end(*scroll), "back at the end, the stream is followed: " + where(*scroll));
		window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
		settle();
		expect(tab->state() != qt::agent_tab_state::working && at_end(*scroll), "the turn's end leaves the tab at its end: " + where(*scroll));
		window.hide();
	}

	void test_the_item_side_chat_follows_new_rounds()
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:7:0";
		subject.Path  = "src/engine/renderer.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "A zero height still reaches the swap chain.";
		subject.Text  = "issue (blocking): clamp both.";
		auto state    = core::open_follow_up(subject);
		auto page     = QWidget{ };
		auto chat     = new qt::item_side_chat{ &page };
		page.resize(380, 420);
		chat->resize(380, 420);
		chat->present(state, QStringLiteral("Fixture agent"), { });
		page.show();
		chat->set_column_open(true);
		settle();
		auto* const scroll = chat->findChild<QScrollArea*>(QStringLiteral("itemSideChatScroll"));
		expect(scroll != nullptr && qt::stick_to_bottom::of(*scroll) != nullptr, "the item's side chat follows its end");
		auto       round = 0;
		const auto ask   = [&] {
			const auto question = "Why is round " + std::to_string(++round) + " blocking?";
			static_cast<void>(core::ask_follow_up(state, question));
			chat->present(state, QStringLiteral("Fixture agent"), { });
			settle();
			static_cast<void>(core::settle_follow_up(state, state.Pending, std::string{ "Because a zero height reaches the swap chain, and the renderer asserts when the window is minimised. Clamp both sides before the swap." }));
			chat->present(state, QStringLiteral("Fixture agent"), { });
			settle();
		};
		for (int each = 0; each < 6; ++each) {
			ask();
		}
		expect(at_end(*scroll), "new rounds keep the chat at its end: " + where(*scroll));

		wheel_up(*scroll);
		const auto readAt = scroll->verticalScrollBar()->value();
		expect(readAt < scroll->verticalScrollBar()->maximum(), "the reader scrolled up: " + where(*scroll));
		ask();
		expect(scroll->verticalScrollBar()->value() == readAt, "a chat scrolled up stays where the reader put it: " + where(*scroll));

		scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
		ask();
		expect(at_end(*scroll), "back at the end, the next round is followed: " + where(*scroll));
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_a_transcript_at_its_end_follows_and_one_scrolled_up_stays();
	test_tool_rows_that_grow_and_shrink_never_move_the_view_back_up();
	test_the_agent_tab_follows_a_streaming_turn();
	test_the_item_side_chat_follows_new_rounds();
	std::cout << "stick-to-bottom:pass\n";
	return EXIT_SUCCESS;
}
