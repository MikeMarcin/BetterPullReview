// [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the sidechat is so
// decoupled from the tour when it's really about the tour." In a real window:
// a part's chat opens inside the Tour tab, beside the tour, and the drawer
// widens to hold both; nothing of it is outside the tab. Switching parts
// switches the chat. On Comments the chat is gone, the drawer is narrow again
// and the Tour tab counts the parts with chats; back on Tour it is as it was.
// Close gives the narrow drawer back. With BPR_TOUR_CHAT_RENDER set to a
// folder, each state is drawn there, light and dark.
#include "../support/review_tour_fixture.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QListWidget>
#include <QMouseEvent>
#include <QSplitter>
#include <QTabBar>
#include <QToolButton>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

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
		for (int pass = 0; pass < 6; ++pass) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
	}

	/// One left click: press and release, as a person's.
	void click(QWidget* aWidget)
	{
		const auto at      = aWidget->rect().center();
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ at }, aWidget->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ at }, aWidget->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
	}

	[[nodiscard]] auto drawer_width(qt::workbench_window& aWindow) -> int
	{
		// The conversations drawer is the central splitter's last widget.
		return aWindow.central_splitter()->sizes().value(3);
	}

	void draw(qt::workbench_window& aWindow, const QString& aName, const QString& aTheme)
	{
		const auto root = qEnvironmentVariable("BPR_TOUR_CHAT_RENDER");
		if (!root.isEmpty()) {
			settle();
			static_cast<void>(aWindow.grab().save(root + QStringLiteral("/") + aName + QStringLiteral("-") + aTheme + QStringLiteral(".png"), "PNG"));
		}
	}

	void run(const core::appearance_mode aMode, const QString& aTheme)
	{
		qt::workbench_window window;
		auto                 settings = qt::default_appearance_settings();
		settings.Mode                 = aMode;
		auto profile                  = qt::appearance_profile{ settings };
		window.resize(1'600, 940);
		profile.apply_to(window);
		window.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 400U };
		window.publish(opened);
		settle();
		window.choose_modality(qt::review_modality::review);
		settle();

		// A tour of two stops, with a chat about the Solution and one about
		// the first stop already in its session.
		auto* const editor = window.tour_editor();
		editor->set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
		editor->set_tour_ask_dispatch([](const qt::tour_ask&) { });
		auto tour       = tests::review_tour_fixture();
		tour.Problem    = "Two views drew over each other when they cleared in the same frame.";
		tour.Solution   = "Each view now clears between begin and present.";
		auto second     = tour.Stops.front();
		second.Id       = core::tour_stop_id{ 2 };
		second.Title    = "Where it presents";
		tour.LastStopId = 2;
		tour.Stops.push_back(second);
		auto presentation     = application::tour_presentation{ };
		presentation.Draft    = std::make_shared<const core::review_tour>(tour);
		presentation.Selected = core::tour_stop_id{ 1 };
		editor->present(presentation);
		editor->set_agent_readiness(qt::agent_setup_readiness::ready);
		const auto stopOne  = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } };
		const auto solution = core::tour_thread{ .Kind = core::tour_thread_kind::solution };
		auto       session  = core::tour_agent_session{ };
		for (const auto& [thread, question, reply] : { std::tuple{ stopOne, "Why does clear() come first?", "Because `begin_frame()` hands back the kept frame (src/item.cpp:4)." },
													   std::tuple{ stopOne, "Say that in the stop, shorter.", "Clear before `begin_frame()`: it returns the kept frame." },
													   std::tuple{ solution, "Is \"kept frame\" the right term?", "The code says retained; either reads." } }) {
			const auto run = core::begin_tour_ask(session, thread, std::nullopt, question);
			static_cast<void>(core::answer_tour_ask(session, *run, reply, { }));
			static_cast<void>(core::time_tour_run(session, *run, 2U));
		}
		editor->present_tour_session(session, std::nullopt);
		window.open_review_tour();
		editor->show_stops();
		editor->go_to_stop(core::tour_stop_id{ 1 });
		settle();
		auto* const chat  = editor->side_chat();
		auto* const tabs  = window.tray_tabs();
		auto* const split = window.findChild<QSplitter*>(QStringLiteral("workbenchTourChatSplit"));
		auto* const badge = static_cast<qt::drawer_count_pill*>(window.findChild<QWidget*>(QStringLiteral("workbenchTrayTourCount")));
		expect(split != nullptr && badge != nullptr, "the Tour tab has its two columns and its count");
		expect(!chat->isVisible() && drawer_width(window) <= 420, "no chat until one is asked for; the drawer is its narrow self");
		expect(badge->isVisible() && badge->text() == QStringLiteral("2"), "the Tour tab counts the two parts with chats");
		draw(window, QStringLiteral("tour-chat-closed"), aTheme);

		// AC1. A part's chat opens inside the Tour tab, beside the tour.
		auto* const stopGlyph = editor->findChild<QToolButton*>(QStringLiteral("tourStopChat"));
		expect(stopGlyph != nullptr && stopGlyph->isVisible() && stopGlyph->text() == QStringLiteral("2"), "the open stop's glyph counts its two turns");
		click(stopGlyph);
		settle();
		expect(chat->isVisible() && chat->parentWidget() == split && split->isAncestorOf(chat) && tabs->currentIndex() == 2, "the stop's chat is the Tour tab's second column");
		expect(window.findChildren<QWidget*>(QStringLiteral("tourSideChat")).size() == 1, "and there is no chat anywhere else");
		expect(chat->current() == stopOne && chat->turn_count() == 2U && stopGlyph->isChecked(), "it is the stop's chat, and the stop is marked");
		auto* const route = editor->findChild<QListWidget*>(QStringLiteral("tourOutline"));
		expect(route != nullptr && route->item(0)->data(Qt::UserRole + 10).toBool() && !route->item(1)->data(Qt::UserRole + 10).toBool(), "the route marks the stop under discussion and no other");
		expect(drawer_width(window) >= 700, "the drawer widens to hold the tour and its chat: " + std::to_string(drawer_width(window)));
		expect(chat->x() > split->widget(0)->x() + split->widget(0)->width() - 2, "the chat is beside the tour, to its right");
		draw(window, QStringLiteral("tour-chat-stop"), aTheme);

		// AC2. Switching parts switches the chat: the route's next stop.
		editor->go_to_stop(core::tour_stop_id{ 2 });
		settle();
		expect(chat->isVisible() && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 2 } } && chat->turn_count() == 0U, "going to the next stop brings its chat");
		expect(route->item(1)->data(Qt::UserRole + 10).toBool() && route->item(0)->data(Qt::UserRole + 9).toInt() == 2, "the route marks it; the first stop keeps its count");
		editor->go_to_stop(core::tour_stop_id{ 1 });
		settle();
		expect(chat->current() == stopOne && chat->turn_count() == 2U, "and back again, the first stop's chat as it was");

		// AC3. Leaving the Tour tab hides the chat; the drawer narrows; the
		// count stays on the Tour tab. Returning restores it.
		tabs->setCurrentIndex(0);
		settle();
		expect(!chat->isVisible() && drawer_width(window) <= 420 && badge->isVisible() && badge->text() == QStringLiteral("2"), "on Comments: no chat, the narrow drawer, the count on Tour");
		draw(window, QStringLiteral("tour-chat-comments"), aTheme);
		tabs->setCurrentIndex(2);
		settle();
		expect(chat->isVisible() && chat->current() == stopOne && drawer_width(window) >= 700, "back on Tour, the chat as it was, and the room for it");

		// A drag while the chat shows is the chat's width; the narrow width
		// stays the drawer's own.
		auto* const central = window.central_splitter();
		central->move_handle(3, central->width() - 680 - central->handleWidth());
		settle();
		const auto dragged = drawer_width(window);
		tabs->setCurrentIndex(0);
		settle();
		expect(drawer_width(window) <= 420, "the narrow drawer is still its own width");
		tabs->setCurrentIndex(2);
		settle();
		expect(std::abs(drawer_width(window) - dragged) <= 4, "the chat's width is remembered: " + std::to_string(drawer_width(window)) + " for " + std::to_string(dragged));

		// The Solution's chat, from its glyph.
		editor->open_step(core::tour_step::solution);
		settle();
		auto* const solutionGlyph = editor->findChild<QToolButton*>(QStringLiteral("tourSolutionChat"));
		click(solutionGlyph);
		expect(chat->current() == solution && solutionGlyph->isChecked() && !route->item(0)->data(Qt::UserRole + 10).toBool(), "the Solution's glyph brings its chat and marks it");
		draw(window, QStringLiteral("tour-chat-solution"), aTheme);

		// Close gives the narrow drawer back; every chat is kept.
		click(chat->close_button());
		expect(!chat->isVisible() && !chat->column_open() && drawer_width(window) <= 420 && badge->text() == QStringLiteral("2"), "closed: the narrow drawer, and the chats still counted");
	}
}

int main(int argc, char** argv)
{
	QApplication application{ argc, argv };
	run(core::appearance_mode::dark, QStringLiteral("dark"));
	run(core::appearance_mode::light, QStringLiteral("light"));
	std::cout << "tour-chat-in-tab:pass\n";
	return EXIT_SUCCESS;
}
