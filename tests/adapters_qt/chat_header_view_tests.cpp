/// \file
/// \brief RFC-BPR-0130, header A as the owner amended it. The one header row
/// the Agent tab, the tour's side chat, a local draft's discussion and the
/// side chat about one review item wear: what the chat is about on one line
/// that ends in an ellipsis where it does not fit, its place under it, then
/// Show, History where the chat keeps sessions or Copy conversation where it
/// keeps none, and Close. Core decides which of them a chat has; no row holds
/// a New session button. Held at the narrowest drawer and the narrowest
/// column, where nothing may be cut or leave the row.
#include "bpr/qt/chat_header_view.hpp"

#include "bpr/core/chat_header.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 6; ++round) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto names() -> qt::chat_header_view::names
	{
		return {
			.Row     = QStringLiteral("chatHeader"),
			.Subject = QStringLiteral("chatSubject"),
			.Place   = QStringLiteral("chatPlace"),
			.Show    = QStringLiteral("chatShow"),
			.History = QStringLiteral("chatHistory"),
			.Copy    = QStringLiteral("chatCopy"),
			.Close   = QStringLiteral("chatClose"),
		};
	}

	/// Every part the row shows lies inside it, none over another, each
	/// button as wide as its words.
	void expect_fits(const qt::chat_header_view& aRow, const std::string& aWhere)
	{
		auto shown = QList<const QWidget*>{ };
		for (const auto* const part : aRow.findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
			if (part->isVisibleTo(&aRow)) {
				shown.push_back(part);
			}
		}
		for (const auto* const part : shown) {
			expect(aRow.rect().contains(part->geometry()), aWhere + ": " + part->objectName().toStdString() + " lies inside the row");
			if (const auto* const button = qobject_cast<const QAbstractButton*>(part); button != nullptr && !button->text().isEmpty()) {
				expect(button->width() >= button->fontMetrics().horizontalAdvance(button->text()), aWhere + ": " + button->objectName().toStdString() + " is as wide as its words");
			}
			for (const auto* const other : shown) {
				if (other != part && other->objectName() != QStringLiteral("chatPlace") && part->objectName() != QStringLiteral("chatPlace")) {
					expect(!part->geometry().intersects(other->geometry()), aWhere + ": " + part->objectName().toStdString() + " is not drawn over " + other->objectName().toStdString());
				}
			}
		}
		expect(aRow.sizeHint().width() <= aRow.width() || aRow.minimumSizeHint().width() <= aRow.width(), aWhere + ": the row asks for no more than it has");
	}
}

auto main(int aArgumentCount, char** aArguments) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	auto application = QApplication{ aArgumentCount, aArguments };

	auto  host   = QWidget{ };
	auto* column = new QVBoxLayout{ &host };
	column->setContentsMargins(10, 8, 10, 10);
	auto* const row = new qt::chat_header_view{ names(), &host };
	column->addWidget(row);
	column->addStretch(1);
	auto shows  = 0;
	auto copies = 0;
	auto closes = 0;
	row->set_dispatch({ .Show = [&shows] { ++shows; }, .CopyConversation = [&copies] { ++copies; }, .Close = [&closes] { ++closes; } });
	row->set_tips({ .Show = QStringLiteral("Show its lines in the code"), .Close = QStringLiteral("Close the chat. It is kept.") });
	host.resize(420, 200);
	host.show();
	settle();

	// The Agent tab: the pull request's title and History.
	const auto title = std::string{ "#2368 Queued prompt rows leave the pending list once their turn begins, at every drawer width" };
	row->present(core::plan_chat_header({ .Kind = core::chat_kind::agent_tab, .Subject = title, .RevisionRead = "3f9c2ab11111" }));
	settle();
	expect(row->subject()->text() == QString::fromStdString(title) && row->history_tool()->isVisibleTo(&host) && !row->copy_tool()->isVisibleTo(&host) && !row->close_tool()->isVisibleTo(&host)
			   && !row->show_button()->isVisibleTo(&host) && !row->place()->isVisibleTo(&host),
		   "the Agent tab's row is its title and History");
	expect(row->history_tool()->accessibleName() == QStringLiteral("History") && row->history_tool()->toolTip() == QStringLiteral("History: this chat's sessions, New session and every run"),
		   "History says what it holds: " + row->history_tool()->toolTip().toStdString());
	for (const auto* const button : row->findChildren<QAbstractButton*>()) {
		expect(!button->text().contains(QStringLiteral("New session")) && !button->accessibleName().contains(QStringLiteral("New session")) && !button->objectName().contains(QStringLiteral("NewSession")),
			   "no row holds a New session button");
	}
	for (const auto width : { 280, 300, 340, 420 }) {
		host.resize(width, 200);
		settle();
		const auto where = "agent tab at " + std::to_string(width);
		expect_fits(*row, where);
		expect(row->title_cut() && row->shown_title().endsWith(QChar{ 0x2026 }) && row->subject()->toolTip() == QString::fromStdString(title) && row->subject()->accessibleName() == QString::fromStdString(title),
			   where + ": a title too long for its row ends in an ellipsis, with the whole title in its tooltip: " + row->shown_title().toStdString());
		expect(row->subject()->height() < 2 * row->subject()->fontMetrics().lineSpacing(), where + ": the title keeps one line");
	}
	row->present(core::plan_chat_header({ .Kind = core::chat_kind::agent_tab, .Subject = "#7 Fix", .SessionName = "The queue's races" }));
	settle();
	expect(row->subject()->text() == QStringLiteral("The queue's races") && !row->title_cut() && row->subject()->toolTip() == QStringLiteral("The queue's races, in #7 Fix"),
		   "a named session is the title, and its tooltip the pull request it is in: " + row->subject()->toolTip().toStdString());

	// A tour part: the part and its place, Show in code, History, Close.
	const auto part = core::plan_chat_header(
		{ .Kind = core::chat_kind::tour_part, .Subject = "Stop 2, The claim is stored before the row is removed", .Place = "src/application/src/agent_queue_host.cpp, lines 212 to 218", .CanShow = true });
	host.resize(420, 200);
	row->present(part);
	settle();
	expect(row->show_button()->isVisibleTo(&host) && row->show_button()->text() == QStringLiteral("Show in code") && row->show_button()->toolTip() == QStringLiteral("Show its lines in the code")
			   && row->history_tool()->isVisibleTo(&host) && row->close_tool()->isVisibleTo(&host) && !row->copy_tool()->isVisibleTo(&host) && row->place()->isVisibleTo(&host),
		   "a tour part's row: the part, its place, Show in code, History, close");
	expect(row->history_tool()->toolTip() == QStringLiteral("History: this chat's sessions, and New session") && row->close_tool()->toolTip() == QStringLiteral("Close the chat. It is kept.")
			   && row->close_tool()->accessibleName() == QStringLiteral("Close the chat"),
		   "its tools are named, and closing says what it keeps");
	expect(row->subject()->geometry().right() < row->show_button()->geometry().left() && row->show_button()->geometry().right() < row->history_tool()->geometry().left()
			   && row->history_tool()->geometry().right() < row->close_tool()->geometry().left() && row->place()->geometry().top() > row->subject()->geometry().top(),
		   "left to right: the part over its place, Show in code, History, close");
	for (const auto width : { 300, 340, 420 }) {
		host.resize(width, 200);
		settle();
		const auto where = "tour part at " + std::to_string(width);
		expect_fits(*row, where);
		expect(row->subject()->height() < 2 * row->subject()->fontMetrics().lineSpacing() && row->place()->height() < 2 * row->place()->fontMetrics().lineSpacing(), where + ": the part and its place keep one line each");
		expect(row->place()->toolTip() == QString::fromStdString(part.Place), where + ": a place cut short is whole in its tooltip");
	}
	// At 300 the full label can still fit with the native Windows font.
	host.resize(240, 200);
	settle();
	expect(row->show_button()->text() == QStringLiteral("Show"), "where the full label would crowd the subject, Show in code reads Show: " + row->show_button()->text().toStdString());
	expect_fits(*row, "the tour part's compact Show label");
	host.resize(520, 200);
	settle();
	expect(row->show_button()->text() == QStringLiteral("Show in code"), "and is said whole where the part beside it keeps its room");
	row->show_button()->click();
	row->close_tool()->click();
	expect(shows == 1 && closes == 1 && copies == 0, "Show and close reach the chat");

	// The whole tour: nothing to show.
	row->present(core::plan_chat_header({ .Kind = core::chat_kind::whole_tour, .Subject = "The whole tour", .CanShow = true }));
	settle();
	expect(!row->show_button()->isVisibleTo(&host) && !row->place()->isVisibleTo(&host) && row->history_tool()->isVisibleTo(&host) && row->close_tool()->isVisibleTo(&host), "the whole tour's row: its name, History, close");

	// An item's chat keeps sessions and offers Copy from History.
	host.resize(300, 200);
	row->present(core::plan_chat_header({ .Kind = core::chat_kind::review_item, .Subject = "Suggested comment", .Place = "agent_queue_host.cpp, lines 212 to 218", .CanShow = true }));
	settle();
	expect(row->history_tool()->isVisibleTo(&host) && !row->copy_tool()->isVisibleTo(&host) && row->close_tool()->isVisibleTo(&host) && row->show_button()->isVisibleTo(&host),
		   "an item's chat shows Show, History and close");
	expect(row->history_tool()->toolTip() == QStringLiteral("History: this chat's sessions, and New session"), "item History names its sessions and New session");
	expect(row->show_button()->text() == QStringLiteral("Show"), "the item chat says Show in the narrowest column");
	expect_fits(*row, "an item's chat at 300");

	// A local draft's discussion is one conversation, with Copy in the row.
	host.resize(240, 200);
	row->present(core::plan_chat_header({ .Kind = core::chat_kind::local_draft_discussion, .Subject = "Suggested comment", .Place = "agent_queue_host.cpp, lines 212 to 218", .CanShow = true, .Cost = core::agent_cost{ 140'000'000U } }));
	settle();
	expect(row->copy_tool()->isVisibleTo(&host) && !row->history_tool()->isVisibleTo(&host) && row->close_tool()->isVisibleTo(&host) && row->show_button()->isVisibleTo(&host),
		   "a local draft's discussion shows Show, Copy conversation and close");
	expect(row->copy_tool()->accessibleName() == QStringLiteral("Copy conversation") && row->copy_tool()->toolTip() == QStringLiteral("Copy this conversation as text. This conversation has cost $0.14 so far."),
		   "Copy says what the local draft's discussion has cost");
	expect(row->show_button()->text() == QStringLiteral("Show"), "the local draft discussion says Show in the narrowest column");
	expect_fits(*row, "a local draft's discussion with the compact Show label");
	row->copy_tool()->click();
	expect(copies == 1, "Copy conversation reaches the local draft's discussion");

	if (const auto root = qEnvironmentVariable("BPR_CHAT_HEADER_RENDER_DIR"); !root.isEmpty()) {
		host.resize(300, 120);
		row->present(part);
		settle();
		expect(host.grab().save(QStringLiteral("%1/chat-header-tour-part-300.png").arg(root), "PNG"), "the render is saved");
	}

	std::cout << "chat-header-view-tests:pass\n";
	return EXIT_SUCCESS;
}
