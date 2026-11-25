/// \file
/// \brief The tour's side chat at its narrowest and two wider widths (owner,
/// attended review 2026-11-24: "What happened to my tour window? Those
/// buttons are unreadable and thats not even the narrowest width."). In every
/// state the column can reach, each button and label shown lies inside the
/// column, no button is narrower than the words it shows, and none lies over
/// another. The header is one row: what the chat is about, Show in code,
/// History and Close, the subject ending in an ellipsis where it does not fit.
/// \details With `BPR_TOUR_CHAT_RENDER_DIR` naming a folder, each state is
/// saved there at each width before it is checked, as
/// `<tag>-<state>-<width>.png` (`BPR_TOUR_CHAT_RENDER_TAG`, else "tour-chat").
#include "../support/review_tour_fixture.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionButton>
#include <QToolButton>

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	auto failures = 0;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++failures;
		}
	}

	void settle()
	{
		for (int round = 0; round < 8; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
	}

	auto name_of(const QWidget& aWidget) -> std::string
	{
		auto name = aWidget.objectName().toStdString();
		if (const auto* const button = qobject_cast<const QAbstractButton*>(&aWidget); button != nullptr && !button->text().isEmpty()) {
			name += " \"" + button->text().toStdString() + "\"";
		}
		return name.empty() ? std::string{ aWidget.metaObject()->className() } : name;
	}

	/// Every button and label the column shows now, with where it is in the
	/// column. One scrolled out of the transcript's view is not shown.
	struct shown_control
	{
		QWidget* Widget = nullptr;
		QRect    Place;
	};

	auto shown_controls(QWidget& aChat) -> std::vector<shown_control>
	{
		auto found = std::vector<shown_control>{ };
		for (auto* const widget : aChat.findChildren<QWidget*>()) {
			const auto isButton = qobject_cast<QAbstractButton*>(widget) != nullptr;
			const auto isLabel  = qobject_cast<QLabel*>(widget) != nullptr;
			if ((!isButton && !isLabel) || !widget->isVisibleTo(&aChat) || widget->visibleRegion().isEmpty()) {
				continue;
			}
			// Where the control is drawn. A native push button's widget is
			// larger than its face (the style keeps room around it), so two
			// such widgets may overlap where their faces do not.
			auto face = widget->rect();
			if (auto* const push = qobject_cast<QPushButton*>(widget); push != nullptr) {
				auto option = QStyleOptionButton{ };
				option.initFrom(push);
				if (const auto item = push->style()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, push); item.isValid()) {
					face = item;
				}
			}
			found.push_back({ widget, face.translated(widget->mapTo(&aChat, QPoint{ 0, 0 })) });
		}
		return found;
	}

	/// The checks of one state at one width.
	void check_fit(QWidget& aChat, const std::string& aState)
	{
		const auto where    = aState + " at " + std::to_string(aChat.width()) + ": ";
		const auto controls = shown_controls(aChat);
		for (const auto& each : controls) {
			expect(each.Place.left() >= 0 && each.Place.right() < aChat.width(),
				   where + name_of(*each.Widget) + " lies outside the column (" + std::to_string(each.Place.left()) + ".." + std::to_string(each.Place.right()) + ")");
			// A button that shows words; an icon tool keeps the size its row gives it.
			const auto* const tool = qobject_cast<const QToolButton*>(each.Widget);
			if (const auto* const button = qobject_cast<const QAbstractButton*>(each.Widget); button != nullptr && !button->text().isEmpty() && (tool == nullptr || tool->toolButtonStyle() != Qt::ToolButtonIconOnly)) {
				expect(each.Widget->width() >= each.Widget->minimumSizeHint().width(),
					   where + name_of(*each.Widget) + " is narrower than what it shows (" + std::to_string(each.Widget->width()) + " of " + std::to_string(each.Widget->minimumSizeHint().width()) + ")");
				expect(each.Widget->height() >= each.Widget->minimumSizeHint().height(), where + name_of(*each.Widget) + " is shorter than what it shows");
			}
		}
		for (std::size_t first = 0; first < controls.size(); ++first) {
			for (std::size_t second = first + 1U; second < controls.size(); ++second) {
				const auto& one = controls[first];
				const auto& two = controls[second];
				if (one.Widget->isAncestorOf(two.Widget) || two.Widget->isAncestorOf(one.Widget)) {
					continue;
				}
				const auto box = [](const QRect& aRect) { return "(" + std::to_string(aRect.left()) + "," + std::to_string(aRect.top()) + " " + std::to_string(aRect.width()) + "x" + std::to_string(aRect.height()) + ")"; };
				expect(!one.Place.intersects(two.Place), where + name_of(*one.Widget) + " " + box(one.Place) + " lies over " + name_of(*two.Widget) + " " + box(two.Place));
			}
		}
		if (auto* const scroll = aChat.findChild<QScrollArea*>(QStringLiteral("tourSideChatScroll")); scroll != nullptr) {
			expect(scroll->horizontalScrollBar()->maximum() == 0, where + "the transcript pans sideways by " + std::to_string(scroll->horizontalScrollBar()->maximum()));
		}
	}

	auto render_prefix() -> QString
	{
		const auto folder = qEnvironmentVariable("BPR_TOUR_CHAT_RENDER_DIR");
		if (folder.isEmpty()) {
			return { };
		}
		const auto tag = qEnvironmentVariable("BPR_TOUR_CHAT_RENDER_TAG");
		return folder + QLatin1Char{ '/' } + (tag.isEmpty() ? QStringLiteral("tour-chat") : tag) + QLatin1Char{ '-' };
	}

	auto history() -> application::native_agent_history_projection
	{
		auto view     = application::native_agent_history_projection{ };
		view.Selected = "b";
		auto newest   = application::native_agent_history_entry{ };
		newest.Id     = "c";
		newest.Title  = "Does anything else read the frame before it is cleared, and where?";
		newest.Turns  = 2U;
		auto open     = application::native_agent_history_entry{ };
		open.Id       = "b";
		open.Title    = "Clearing order";
		open.Name     = "Clearing order";
		open.Turns    = 1U;
		open.Cost     = core::agent_cost{ 40000000U };
		auto oldest   = application::native_agent_history_entry{ };
		oldest.Id     = "a";
		oldest.Title  = "Why clear first?";
		oldest.Turns  = 4U;
		view.Entries  = { newest, open, oldest };
		return view;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = render_prefix();

	auto draft                = tests::review_tour_fixture();
	draft.Stops.front().Title = "Clear the frame before each view presents";
	draft.Stops.front().Body  = "Each view now clears between begin and present.";
	{
		auto second  = draft.Stops.front();
		second.Id    = core::tour_stop_id{ 2 };
		second.Title = "Where the renderer presents each frame after clearing it";
		second.Body  = "Each view now clears between begin and present.";
		draft.Stops.push_back(second);
		draft.LastStopId = 2;
	}
	const auto stopThread = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 2 } };
	const auto tourThread = core::tour_thread{ };
	const auto shared     = std::make_shared<const core::review_tour>(draft);

	qt::review_tour_editor editor;
	editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
	editor.set_tour_ask_dispatch([](const qt::tour_ask&) { });
	editor.set_tour_session_dispatch([] { }, [] { }, [](std::size_t, core::tour_proposal_decision, const QString&) { });
	editor.set_tour_ask_more_dispatch([](core::tour_agent_run_id) { });
	editor.resize(420, 900);
	editor.show();
	editor.present({ .Draft = shared });
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	settle();
	auto* const chat = editor.side_chat();
	chat->set_history_dispatch([](const core::tour_thread&, std::optional<std::string>) { });
	chat->set_queue_dispatch([](const core::tour_thread&) { return qt::agent_pending_dispatch{ }; });
	chat->resize(420, 760);
	chat->show();

	// The states. Each leaves the chat as the next one expects it.
	const auto answered = [&](const core::tour_thread& aThread) {
		auto       session = core::tour_agent_session{ };
		const auto said    = core::begin_tour_ask(session, aThread, std::nullopt, "Why does the renderer clear before it presents?");
		static_cast<void>(core::answer_tour_ask(session, *said, "Because the frame is kept between views: without the clear, the second view draws over the first one's pixels.", { }));
		static_cast<void>(core::cost_tour_run(session, *said, core::agent_cost{ 40000000U }, core::agent_cost{ 40000000U }));
		return session;
	};
	struct state
	{
		const char*           Name;
		std::function<void()> Enter;
	};
	const auto states = std::array{
		state{ "stop", [&] {
				  editor.set_tour_host_idle(true);
				  editor.set_tour_running(std::nullopt, false, false);
				  editor.present_tour_session(answered(stopThread), std::nullopt);
				  chat->open(stopThread);
				  chat->present_native_history(history());
				  chat->present_revision_continuation(std::nullopt, { }, false);
				  chat->composer()->clear();
			  } },
		state{ "stop-moved-head", [&] {
				  auto moved     = core::agent_continuation_offer{ };
				  moved.Required = true;
				  chat->present_revision_continuation(moved, { }, false);
			  } },
		state{ "stop-replay", [&] {
				  chat->present_revision_continuation(std::nullopt, { }, false);
				  auto view   = history();
				  view.Replay = true;
				  chat->present_native_history(view);
			  } },
		state{ "stop-queue", [&] {
				  chat->present_native_history(history());
				  auto       session = answered(stopThread);
				  const auto running = core::begin_tour_ask(session, stopThread, std::nullopt, "Take your time and explain this stop.");
				  static_cast<void>(running);
				  editor.set_tour_host_idle(false);
				  editor.set_tour_running(std::nullopt, false, true);
				  editor.present_tour_session(session, std::nullopt);
				  chat->present_queue(stopThread, { qt::agent_pending_row{ .Id = 1U, .Label = QStringLiteral("Now the next stop."), .Text = QStringLiteral("Now the next stop.") } }, QString{ }, false);
				  chat->composer()->setPlainText(QStringLiteral("And the one after that."));
			  } },
		state{ "stop-suggestion", [&] {
				  chat->present_queue(stopThread, { }, QString{ }, false);
				  chat->composer()->clear();
				  editor.set_tour_host_idle(true);
				  editor.set_tour_running(std::nullopt, false, false);
				  auto       session  = core::tour_agent_session{ };
				  const auto enhanced = core::begin_tour_ask(session, stopThread, std::nullopt, "Enhance the writing of this stop.", true);
				  for (auto& made : session.Runs) {
					  if (made.Id == *enhanced) {
						  made.Fresh = true;
					  }
				  }
				  auto suggested        = core::tour_ask_answer{ };
				  suggested.Reply       = "Tightened the stop.";
				  suggested.StopTexts   = { { 2U, "Each view clears between begin and present, so two never share a frame." } };
				  const auto suggestion = core::propose_tour_changes(*enhanced, suggested, { }, draft);
				  static_cast<void>(core::answer_tour_ask(session, *enhanced, suggested.Reply, core::tally_of(suggestion)));
				  editor.present_tour_session(session, suggestion);
				  chat->open(stopThread);
			  } },
		state{ "tour-revision", [&] {
				  auto       session       = core::tour_agent_session{ };
				  const auto turn          = core::begin_tour_ask(session, tourThread, std::nullopt, "Drop the first stop.");
				  auto       revised       = core::tour_ask_answer{ };
				  revised.Reply            = "Dropped stop 1.";
				  revised.Revision         = core::tour_revision{ };
				  revised.Revision->Remove = { 1U };
				  const auto revision      = core::propose_tour_changes(*turn, revised, { }, draft);
				  static_cast<void>(core::answer_tour_ask(session, *turn, revised.Reply, core::tally_of(revision)));
				  editor.present_tour_session(session, revision);
				  chat->open(tourThread);
			  } },
		state{ "tour-plan-check", [&] {
				  editor.present_tour_session(answered(tourThread), std::nullopt);
				  chat->open(tourThread);
				  chat->set_check_dispatch({ .Plan = [](const QString&) { } });
				  chat->set_check_state(true, true, true, QString{ });
			  } },
	};

	for (const auto& each : states) {
		each.Enter();
		for (const auto width : { 300, 340, 420 }) {
			chat->resize(width, 760);
			settle();
			if (!prefix.isEmpty()) {
				expect(chat->grab().save(prefix + QString::fromLatin1(each.Name) + QLatin1Char{ '-' } + QString::number(width) + QStringLiteral(".png")), "render saved");
			}
			check_fit(*chat, each.Name);
		}
	}

	// The History menu, as the chat opens it.
	chat->set_check_dispatch({ });
	states[0].Enter();
	chat->resize(300, 760);
	settle();
	if (auto* const menu = chat->history_button() != nullptr ? chat->history_button()->menu() : nullptr; menu != nullptr) {
		menu->popup(chat->mapToGlobal(QPoint{ 10, 40 }));
		settle();
		if (!prefix.isEmpty()) {
			expect(menu->grab().save(prefix + QStringLiteral("history-menu.png")), "render saved");
		}
		menu->close();
	} else {
		expect(false, "the chat has a History menu");
	}

	if (failures != 0) {
		std::cerr << failures << " checks failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "tour-side-chat-width:pass\n";
	return EXIT_SUCCESS;
}
