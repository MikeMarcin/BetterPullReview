/// \file
/// \brief [[BPR-2337]] (owner, attended review b64bbc238073ff3b665048b611ef1c60:
/// "side chat is staying open when i switch off that tab. it should be part
/// of that tab. The send button on both the side chats (report and tour)
/// should be positioned more like the agent tab."). The side chat about one
/// review item belongs to the document tab it was opened in: another tab
/// hides it, its own shows it again with its words and width, and closing
/// its tab closes it. In the item's side chat and the tour's, Send stands in
/// the composer's field as the Agent tab's does: under the rule, at the right
/// end of the row, the same size and the same insets.
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>

#include <cstdint>
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
		for (int pass = 0; pass < 6; ++pass) {
			QApplication::processEvents();
		}
	}

	void wait(const int aMilliseconds)
	{
		auto loop = QEventLoop{ };
		QTimer::singleShot(aMilliseconds, &loop, &QEventLoop::quit);
		loop.exec();
		settle();
	}

	void publish_drawer(qt::workbench_window& aWindow, const std::uint32_t aConversationsWidth)
	{
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = aConversationsWidth };
		aWindow.publish(opened);
		settle();
	}

	/// A shown window in review, its Agent tab in front with an agent chosen.
	void open_review(qt::workbench_window& aWindow, const std::uint32_t aDrawer)
	{
		aWindow.resize(1'440, 900);
		aWindow.show();
		publish_drawer(aWindow, aDrawer);
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

	/// The window's tabs: `aReport` a report's tab (id 1) when set, then
	/// file tabs by id; `aActive` the active one.
	void publish_tabs(qt::workbench_window& aWindow, const bool aReport, const std::vector<std::uint64_t>& aFiles, const std::uint64_t aActive)
	{
		auto tabs = application::document_tabs_projection{ };
		if (aReport) {
			auto entry   = application::document_tab_entry{ };
			entry.Id     = core::document_tab_id{ 1U };
			entry.Report = "turn:1";
			entry.Active = aActive == 1U;
			tabs.Tabs.push_back(entry);
		}
		for (const auto file : aFiles) {
			auto entry   = application::document_tab_entry{ };
			entry.Id     = core::document_tab_id{ file };
			entry.Active = aActive == file;
			tabs.Tabs.push_back(entry);
		}
		aWindow.publish(tabs);
		settle();
	}

	auto suggestion() -> core::follow_up_subject
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:1:0";
		subject.Path  = "src/a.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "Placeholder claim.";
		subject.Text  = "issue: placeholder suggested comment.";
		return subject;
	}

	void test_the_side_chat_belongs_to_its_tab()
	{
		qt::workbench_window window;
		// The narrowest drawer, so the page beside the chat lets it widen.
		open_review(window, 280U);
		auto report     = qt::agent_report_document{ };
		report.Turn     = 1U;
		report.Title    = QStringLiteral("Review of #21");
		report.Markdown = QStringLiteral("# Review of #21\n\n**Verdict:** Needs changes\n\nPlaceholder summary.\n");
		window.open_report("turn:1", report);
		publish_tabs(window, true, { 2U }, 1U);
		expect(window.report_shown(), "the report's tab is in front");

		auto* const chat   = window.item_side_chat();
		auto        closed = 0;
		chat->set_intents({ .Closed = [&closed] { ++closed; } });
		chat->present(core::open_follow_up(suggestion()), QStringLiteral("Fixture agent"), { });
		chat->set_column_open(true);
		settle();
		// A width of the reviewer's own, inside what the page allows.
		const auto chosen = static_cast<std::uint32_t>(qt::item_side_chat::minimum_column_width + ((chat->maximum_width() - qt::item_side_chat::minimum_column_width) / 2));
		chat->set_preferred_width(chosen);
		wait(50);
		expect(chat->isVisible() && window.item_side_chat_tab() == std::optional{ core::document_tab_id{ 1U } }, "opened beside the report, the chat is the report tab's");
		chat->composer()->setPlainText(QStringLiteral("Placeholder question being written"));
		const auto width = chat->width();
		expect(width == static_cast<int>(chosen) && width > qt::item_side_chat::minimum_column_width, "the chat stands at its width: " + std::to_string(width) + " for " + std::to_string(chosen));

		// Another tab: the chat hides, and is kept.
		publish_tabs(window, true, { 2U }, 2U);
		expect(!chat->isVisible() && chat->column_open() && !chat->tab_shown() && closed == 0, "switching to another tab hides the chat without closing it");
		// Its own tab again: as it was.
		publish_tabs(window, true, { 2U }, 1U);
		wait(50);
		expect(chat->isVisible() && chat->tab_shown(), "returning to the report's tab shows the chat again");
		expect(chat->width() == width, "with its width: " + std::to_string(chat->width()));
		expect(chat->composer()->toPlainText() == QStringLiteral("Placeholder question being written") && chat->chip_text() == QStringLiteral("issue: placeholder suggested comment."),
			   "with its conversation and the words being written");
		expect(closed == 0, "nothing was closed");

		// Opened again from another tab, it is that tab's.
		publish_tabs(window, true, { 2U }, 2U);
		chat->set_column_open(true);
		settle();
		expect(chat->isVisible() && window.item_side_chat_tab() == std::optional{ core::document_tab_id{ 2U } }, "asked about from the file's tab, the chat is the file tab's");
		publish_tabs(window, true, { 2U }, 1U);
		expect(!chat->isVisible() && chat->column_open(), "and the report's tab does not show it");
		publish_tabs(window, true, { 2U }, 2U);
		expect(chat->isVisible(), "its own tab does");

		// Closing its tab closes it, once.
		publish_tabs(window, true, { }, 1U);
		expect(!chat->isVisible() && !chat->column_open() && closed == 1 && !window.item_side_chat_tab(), "closing the chat's tab closes the chat");
		publish_tabs(window, true, { 3U }, 3U);
		expect(!chat->isVisible() && !chat->column_open() && closed == 1, "a later tab does not bring it back");

		// Opened where no tab shows it, the tab its opening brings up is its.
		publish_tabs(window, false, { }, 0U);
		chat->set_column_open(true);
		settle();
		expect(!window.item_side_chat_tab(), "with no tab active the chat waits for its tab");
		publish_tabs(window, false, { 4U }, 4U);
		expect(chat->isVisible() && window.item_side_chat_tab() == std::optional{ core::document_tab_id{ 4U } }, "the tab brought up for it is its");
		publish_tabs(window, false, { 4U, 5U }, 5U);
		expect(!chat->isVisible() && chat->column_open(), "and another tab hides it");

		// Its own Close closes it wherever it is.
		publish_tabs(window, false, { 4U, 5U }, 4U);
		chat->close_button()->click();
		settle();
		expect(!chat->column_open() && closed == 2 && !window.item_side_chat_tab(), "Close closes it and it belongs to no tab");
		publish_tabs(window, false, { 4U, 5U }, 5U);
		publish_tabs(window, false, { 4U, 5U }, 4U);
		expect(!chat->isVisible(), "a closed chat stays closed across tab switches");
		window.hide();
	}

	struct placed
	{
		QSize Size;
		/// From the field's right edge to Send's.
		int Right = 0;
		/// From the rule's bottom to Send's top, and Send's bottom to the
		/// field's.
		int UnderRule = 0;
		int Bottom    = 0;
	};

	auto place_of(QPushButton& aSend, const std::string& aWhere) -> placed
	{
		auto* const field = aSend.parentWidget();
		auto* const rule  = field != nullptr ? field->findChild<QWidget*>(QStringLiteral("composerRule")) : nullptr;
		expect(field != nullptr && rule != nullptr && rule->parentWidget() == field, aWhere + ": Send is in a composer field with its rule");
		const auto send = aSend.geometry();
		expect(aSend.isVisible() && send.top() > rule->geometry().bottom(), aWhere + ": Send is shown under the rule");
		const auto place = placed{ .Size = send.size(), .Right = field->width() - (send.right() + 1), .UnderRule = send.top() - (rule->geometry().bottom() + 1), .Bottom = field->height() - (send.bottom() + 1) };
		std::cout << aWhere << ": Send " << place.Size.width() << "x" << place.Size.height() << ", right " << place.Right << ", under the rule " << place.UnderRule << ", bottom " << place.Bottom << '\n';
		return place;
	}

	/// With BPR_SIDE_CHAT_SEND_RENDER set to a folder, `aWidget` is saved
	/// there as `aName`.png: the renders a look carries.
	void render(QWidget& aWidget, const QString& aName)
	{
		if (const auto root = qEnvironmentVariable("BPR_SIDE_CHAT_SEND_RENDER"); !root.isEmpty()) {
			static_cast<void>(aWidget.grab().save(root + QStringLiteral("/") + aName + QStringLiteral(".png")));
		}
	}

	void expect_placed_as(const placed& aPlace, const placed& aAgent, const std::string& aWhere)
	{
		expect(aPlace.Size == aAgent.Size, aWhere + ": Send is the Agent tab's size");
		expect(aPlace.Right == aAgent.Right, aWhere + ": Send ends the row at the Agent tab's right inset");
		expect(aPlace.UnderRule == aAgent.UnderRule && aPlace.Bottom == aAgent.Bottom, aWhere + ": Send stands under the rule and above the field's edge as the Agent tab's does");
	}

	void test_send_stands_as_the_agent_tabs()
	{
		qt::workbench_window window;
		// A drawer wide enough that the agent picker shares Send's row.
		open_review(window, 640U);
		auto* const ask = window.agent_tab()->findChild<QPushButton*>(QStringLiteral("agentAsk"));
		expect(ask != nullptr && ask->isVisible(), "the Agent tab's Send shows");
		const auto agent = place_of(*ask, "Agent tab");
		render(*ask->parentWidget(), QStringLiteral("agent-composer"));
		expect(ask->text().isEmpty() && !ask->icon().isNull(), "the Agent tab's Send is its arrow");

		auto report     = qt::agent_report_document{ };
		report.Turn     = 1U;
		report.Title    = QStringLiteral("Review of #21");
		report.Markdown = QStringLiteral("# Review of #21\n\nPlaceholder summary.\n");
		window.open_report("turn:1", report);
		publish_tabs(window, true, { }, 1U);
		auto* const item = window.item_side_chat();
		item->present(core::open_follow_up(suggestion()), QStringLiteral("Fixture agent"), { });
		item->set_column_open(true);
		wait(50);
		auto* const itemSend = item->send_button();
		expect(itemSend->parentWidget()->objectName() == QStringLiteral("itemSideChatField"), "the report's side chat keeps Send in its composer's field");
		expect(itemSend->text().isEmpty() && !itemSend->icon().isNull() && itemSend->accessibleName() == QStringLiteral("Send"), "its Send is the arrow, named Send");
		expect_placed_as(place_of(*itemSend, "Report side chat"), agent, "Report side chat");
		render(*item, QStringLiteral("report-side-chat"));

		QWidget page;
		page.resize(380, 640);
		auto* const tour = new qt::tour_side_chat{ &page };
		tour->setGeometry(0, 0, 380, 640);
		tour->show();
		page.show();
		wait(50);
		auto* const tourSend = tour->send_button();
		expect(tourSend->parentWidget() == tour->composer_box(), "the tour's side chat keeps Send in its composer's field");
		expect(tourSend->text().isEmpty() && !tourSend->icon().isNull(), "its Send is the arrow");
		expect_placed_as(place_of(*tourSend, "Tour side chat"), agent, "Tour side chat");
		render(*tour, QStringLiteral("tour-side-chat"));
		page.hide();
		window.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_side_chat_belongs_to_its_tab();
	test_send_stands_as_the_agent_tabs();
	std::cout << "side-chat-tab-and-send:pass\n";
	return EXIT_SUCCESS;
}
