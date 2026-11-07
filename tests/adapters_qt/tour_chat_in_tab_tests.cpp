// [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the sidechat is so
// decoupled from the tour when it's really about the tour." In a real window:
// a part's chat opens inside the Tour tab, beside the tour, and the drawer
// widens to hold both; nothing of it is outside the tab. Switching parts
// switches the chat. On Comments the chat is gone, the drawer is narrow again
// and the Tour tab counts the parts with chats; back on Tour it is as it was.
// Close gives the narrow drawer back. With BPR_TOUR_CHAT_RENDER set to a
// folder, each state is drawn there, light and dark.
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QSplitter>
#include <QTabBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>

#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <set>
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

	/// [[BPR-1925]] v002. The window's own code colouring, composed as the
	/// application composes it: the packaged definitions, a host of its own,
	/// and every answer handed back through the window's relay.
	class held_texts final : public adapters::syntax::blob_source
	{
	public:
		explicit held_texts(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}
		auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override { return Texts.bytes(aBlob); }

	private:
		application::markdown_code_texts& Texts;
	};
	/// Queues work for `deliver`, or, once `Immediate`, runs it at once: a
	/// cached text then answers inside the request, as the application's
	/// does.
	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			if (Immediate) {
				aTask();
				return;
			}
			Tasks.push_back(std::move(aTask));
		}
		std::deque<std::function<void()>> Tasks;
		bool                              Immediate = false;
	};
	struct composed_syntax
	{
		application::markdown_code_texts                           Texts;
		held_texts                                                 Source{ Texts };
		adapters::syntax::packaged_definition_catalog              Catalog;
		adapters::syntax::packaged_syntax_classifier               Classifier{ Catalog, Source };
		manual_executor                                            Effects;
		manual_executor                                            Decisions;
		std::vector<application::markdown_code_syntax_publication> Published;
		/// Once set, each answer goes to the window the moment it is made.
		qt::workbench_window*                  Live = nullptr;
		application::markdown_code_syntax_host Host{ Classifier, Texts, Effects, Decisions,
													 [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) {
														 if (Live != nullptr) {
															 Live->apply_markdown_code_syntax(aPublication);
															 return;
														 }
														 Published.push_back(aPublication);
													 } };
		void deliver(qt::workbench_window& aWindow)
		{
			for (int round = 0; round < 4; ++round) {
				settle();
				while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
					for (auto* lane : { &Effects, &Decisions }) {
						while (!lane->Tasks.empty()) {
							auto task = std::move(lane->Tasks.front());
							lane->Tasks.pop_front();
							task();
						}
					}
				}
				for (const auto& publication : std::exchange(Published, { })) {
					aWindow.apply_markdown_code_syntax(publication);
				}
			}
			settle();
		}
	};
	/// The colours a reader draws `aText` in, by the formats of its block.
	[[nodiscard]] auto colours_of(qt::markdown_body_view& aReader, const QString& aText) -> std::set<QRgb>
	{
		auto seen = std::set<QRgb>{ };
		for (auto block = aReader.rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			const auto at = block.text().indexOf(aText);
			if (at < 0 || block.layout() == nullptr) {
				continue;
			}
			for (const auto& range : block.layout()->formats()) {
				if (range.start < at + aText.size() && range.start + range.length > at) {
					seen.insert(range.format.foreground().color().rgb());
				}
			}
		}
		return seen;
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
		auto syntax = composed_syntax{ };
		window.set_markdown_code_dispatch([&syntax](std::vector<application::markdown_code_syntax_request> aRequests) { syntax.Host.request(aRequests); });
		auto* const editor = window.tour_editor();
		editor->set_code_theme({ .Colors = qt::code_role_colors_of(profile.palette()), .Variant = profile.variant() });
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
													   std::tuple{ stopOne, "Say that in the stop, shorter.", "Clear before `begin_frame()`: it returns the kept frame.\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n" },
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
		// [[BPR-1925]] v002, owner on v001: the chat's code was plain again.
		// In the Tour tab's column, a reply's `cpp` block is in the diff
		// view's colours and its inline code in the part's language.
		const auto coloured = [&](const std::string& aWhen) {
			syntax.deliver(window);
			auto* const reply = chat->reply(1);
			expect(reply != nullptr && reply->window() == &window, "the reply is in the window's Tour tab " + aWhen);
			expect(colours_of(*reply, QStringLiteral("return *Table")).size() >= 2U, "the reply's cpp block is coloured " + aWhen);
			expect(colours_of(*reply, QStringLiteral("begin_frame()")).size() >= 2U, "the reply's inline code is coloured " + aWhen);
		};
		coloured("when it first shows");
		// The application's own lanes: a text it has classified answers
		// inside the request. A reader the chat makes again (a new
		// presentation rebuilds the turns) is coloured at once.
		syntax.Effects.Immediate   = true;
		syntax.Decisions.Immediate = true;
		syntax.Live                = &window;
		editor->present_tour_session(session, std::nullopt);
		coloured("when a cached answer comes inside the request");
		// An appearance applied again keeps them.
		profile.apply_to(window);
		settle();
		coloured("after the appearance is applied again");
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
		coloured("after leaving the Tour tab and coming back");
		editor->present_tour_session(session, std::nullopt);
		coloured("after the session is presented again");

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

		// [[BPR-1925]] v002, owner on v001: "I also want the chat button to
		// toggle it open and closed." Each part's toggle is at the right of
		// its header: one click opens its chat, a click on another part's
		// switches, a click on the open part's closes the column.
		editor->show_problem();
		settle();
		auto* const problemGlyph = editor->findChild<QToolButton*>(QStringLiteral("tourProblemChat"));
		auto* const problemRow   = editor->findChild<QWidget*>(QStringLiteral("tourProblemRow"));
		expect(problemGlyph != nullptr && problemGlyph->isVisible() && problemGlyph->parentWidget() == problemRow && solutionGlyph->parentWidget() == editor->findChild<QWidget*>(QStringLiteral("tourSolutionRow")), "the Problem's and the Solution's toggles are in their headers");
		expect(problemGlyph->x() > problemRow->width() / 2 && solutionGlyph->x() > solutionGlyph->parentWidget()->width() / 2, "at the headers' right");
		click(solutionGlyph);
		expect(chat->isVisible() && chat->current() == solution && solutionGlyph->isChecked() && !problemGlyph->isChecked() && drawer_width(window) >= 600, "the Solution's toggle opens its chat, pressed");
		click(problemGlyph);
		expect(chat->isVisible() && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::problem } && problemGlyph->isChecked() && !solutionGlyph->isChecked(), "the Problem's switches to its chat");
		click(problemGlyph);
		expect(!chat->isVisible() && !chat->column_open() && !problemGlyph->isChecked() && !solutionGlyph->isChecked() && drawer_width(window) <= 420, "the Problem's again closes the column");
		draw(window, QStringLiteral("tour-chat-toggles-off"), aTheme);
		// From the keyboard: Space on the focused toggle opens, Space again closes.
		solutionGlyph->setFocus(Qt::TabFocusReason);
		const auto key = [](QWidget* aWidget, const Qt::Key aKey) {
			auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, QStringLiteral(" ") };
			auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier, QStringLiteral(" ") };
			QApplication::sendEvent(aWidget, &press);
			QApplication::sendEvent(aWidget, &release);
			settle();
		};
		key(solutionGlyph, Qt::Key_Space);
		expect(chat->column_open() && chat->current() == solution && solutionGlyph->isChecked() && solutionGlyph->accessibleName() == QStringLiteral("Close the chat about the Solution"), "Space opens the Solution's chat; the toggle says it closes it");
		draw(window, QStringLiteral("tour-chat-toggles-on"), aTheme);
		solutionGlyph->setFocus(Qt::TabFocusReason);
		key(solutionGlyph, Qt::Key_Space);
		expect(!chat->column_open() && !solutionGlyph->isChecked() && solutionGlyph->accessibleName() == QStringLiteral("Open the chat about the Solution"), "Space again closes it");
		// A stop's toggle on the route: the next stop's opens its chat and goes
		// to it; the open stop's own toggle then closes the column.
		editor->show_stops();
		editor->go_to_stop(core::tour_stop_id{ 1 });
		settle();
		const auto rowRect = route->visualItemRect(route->item(1));
		const auto onGlyph = QPoint{ rowRect.right() - 20, rowRect.top() + 8 + 11 };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ onGlyph }, route->viewport()->mapToGlobal(QPointF{ onGlyph }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ onGlyph }, route->viewport()->mapToGlobal(QPointF{ onGlyph }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(route->viewport(), &press);
		QApplication::sendEvent(route->viewport(), &release);
		settle();
		expect(chat->column_open() && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 2 } } && route->item(1)->data(Qt::UserRole + 10).toBool(), "the next stop's toggle on the route opens its chat");
		auto* const openStopGlyph = editor->findChild<QToolButton*>(QStringLiteral("tourStopChat"));
		expect(openStopGlyph != nullptr && openStopGlyph->isVisible() && openStopGlyph->isChecked(), "and the stop it went to has its toggle pressed");
		click(openStopGlyph);
		expect(!chat->column_open() && !openStopGlyph->isChecked(), "which closes the column");
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
