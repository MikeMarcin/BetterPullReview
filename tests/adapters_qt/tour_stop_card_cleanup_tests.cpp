// [[BPR-1936]], owner 2026-11-08, on the author's Tour tab: the stop card
// collapses by its number or its header's empty space (stops, Problem and
// Solution alike) without moving the tour; it edits as the overview's
// description does (the title wrapped and whole, no mode label, Cancel and
// Save at the foot); Use selected lines lives only in the editor; Ready, next
// is never a permanently disabled primary button; no file chip sits on the
// rail and the open card has no empty band at its top; and every stop can be
// removed, from its menu and from its card's trash. Pass a path prefix to save
// renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/review_tour_editor.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait(const int aMilliseconds)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < aMilliseconds) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
	}
	void settle()
	{
		for (int round = 0; round < 8; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
		wait(120);
		QApplication::processEvents();
	}
	void click(QWidget* aWidget, const QPoint& aAt)
	{
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
	}
	void press(QWidget* aTarget, const Qt::Key aKey)
	{
		auto down = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(aTarget, &down);
		settle();
	}
	[[nodiscard]] auto stop(const std::uint64_t aId, std::string aTitle, std::string aBody, std::string aPath) -> core::tour_stop
	{
		auto value                 = tests::review_tour_fixture().Stops.front();
		value.Id                   = core::tour_stop_id{ aId };
		value.Title                = std::move(aTitle);
		value.Body                 = std::move(aBody);
		value.Preparation          = core::tour_preparation::editing;
		value.Related              = { };
		value.Original->Path       = std::move(aPath);
		value.Original->First.Line = static_cast<std::uint32_t>(aId * 10U);
		value.Original->Last.Line  = static_cast<std::uint32_t>(aId * 10U + 2U);
		return value;
	}
	[[nodiscard]] auto card_of(qt::review_tour_editor& aEditor) -> QWidget*
	{
		auto* const outline = aEditor.outline();
		return outline->currentItem() != nullptr ? outline->itemWidget(outline->currentItem()) : nullptr;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString{ };

	const auto longTitle = std::string{ "Why the snapshot is taken before the reader lock is released, and what a concurrent delete can still observe afterwards" };
	auto       draft     = tests::review_tour_fixture();
	draft.Stops          = { stop(1, "Where the cache is filled", "The parser fills the cache once.", "src/cache.cpp"),
							 stop(2, longTitle, "A reset empties the cache so the next read parses again.", "src/renderer.cpp"),
							 stop(3, "Where it is read", "Readers borrow the cached copy.", "src/main.cpp"),
							 stop(4, "Where it is reset", "Reset clears the cache.", "src/main.cpp") };
	draft.LastStopId     = 4;
	auto selection       = std::uint64_t{ 1 };
	auto selected        = core::tour_stop_id{ 2 };
	auto refusal         = core::tour_refusal::none;

	auto        scroll = QScrollArea{ };
	auto* const holder = new QWidget{ };
	auto* const column = new QVBoxLayout{ holder };
	auto* const top    = new QWidget{ holder };
	top->setFixedHeight(500);
	column->addWidget(top);
	auto* const editor = new qt::review_tour_editor{ holder };
	column->addWidget(editor);
	// Room below, as a long drawer has, so a shorter tour need not scroll back.
	auto* const below = new QWidget{ holder };
	below->setFixedHeight(1200);
	column->addWidget(below);
	scroll.setWidget(holder);
	scroll.setWidgetResizable(true);
	scroll.resize(440, 900);
	const auto show = [&] { editor->present({ .Draft = std::make_shared<const core::review_tour>(draft), .Refusal = refusal, .Selected = selected, .SelectionRevision = selection, .SavedRevision = draft.Revision }); };
	editor->set_dispatch(
		[&](const core::tour_command aCommand) {
			auto outcome = core::apply(draft, aCommand);
			refusal      = outcome.Refusal;
			if (outcome.Refusal == core::tour_refusal::none) {
				draft = std::move(outcome.Accepted);
				if (outcome.Selected.value() != 0U && outcome.Selected != selected) {
					selected = outcome.Selected;
					++selection;
				}
			}
			show();
		},
		[](qt::tour_editor_action) { });
	scroll.show();
	show();
	editor->show_stops();
	settle();
	auto* const outline = editor->outline();
	auto* const bar     = scroll.verticalScrollBar();

	// AC6. The open card starts at its row's top: its title at the ring's
	// height, no band above it.
	auto* card = card_of(*editor);
	expect(card != nullptr && card->isVisible(), "stop 2 opens in place on the route");
	auto* const readTitle = editor->findChild<QLabel*>(QStringLiteral("tourStopReadTitle"));
	const auto  titleTop  = readTitle->mapTo(card, QPoint{ 0, 0 }).y();
	expect(titleTop <= 14, "the open card has no empty space above its title (title at " + std::to_string(titleTop) + ")");
	expect(card->geometry().top() == outline->visualItemRect(outline->currentItem()).top(), "the card sits at its row's top");
	// AC1. No file chip between stops: rows in a new file are as tall as rows in the same file.
	expect(outline->sizeHintForRow(2) == outline->sizeHintForRow(3), "a stop entering a new file has no divider band (" + std::to_string(outline->sizeHintForRow(2)) + " vs " + std::to_string(outline->sizeHintForRow(3)) + ")");
	if (!prefix.isEmpty()) {
		expect(scroll.grab().save(prefix + QStringLiteral("card-read.png")), "render saved");
	}

	// AC3. Ready, next shows only when it can act.
	auto* const complete = editor->findChild<QPushButton*>(QStringLiteral("tourComplete"));
	expect(complete->isVisible() && complete->isEnabled() && complete->isDefault(), "a stop with confirmed lines, not yet Ready, offers Ready, next");
	{
		auto unconfirmed = draft;
		unconfirmed.Stops[1].Original.reset();
		unconfirmed.Stops[1].Mapping     = core::tour_mapping::unavailable;
		unconfirmed.Stops[1].Destination = core::tour_code_destination{ .Commit = draft.Head, .Path = "src/renderer.cpp", .Side = core::diff_side::right, .FirstLine = 3U, .LastLine = 5U };
		std::swap(draft, unconfirmed);
		show();
		settle();
		auto* const source = editor->findChild<QLabel*>(QStringLiteral("tourStopSource"));
		expect(!complete->isVisible() && editor->findChild<QPushButton*>(QStringLiteral("tourReturn"))->isDefault(), "unconfirmed lines: no disabled Ready, next; Show in code leads");
		expect(source->text().contains(QStringLiteral("Lines not confirmed")), "the location says the lines are not confirmed: " + source->text().toStdString());
		std::swap(draft, unconfirmed);
		show();
		settle();
	}

	// AC4. Use selected lines only while the stop is written.
	auto* const useLines = editor->findChild<QPushButton*>(QStringLiteral("tourUseSelectedLines"));
	expect(useLines != nullptr && !useLines->isVisible() && !editor->findChild<QAction*>(QStringLiteral("tourRetarget"))->isVisible(), "Use selected lines is not offered on a read stop, nor in its menus");

	// AC5. Editing: the pencil goes, the title stays wrapped and whole, no mode
	// label, Cancel and Save at the foot.
	editor->begin_stop_edit();
	settle();
	auto* const title  = editor->stop_title();
	auto* const save   = editor->findChild<QPushButton*>(QStringLiteral("tourStopSave"));
	auto* const cancel = editor->findChild<QPushButton*>(QStringLiteral("tourStopCancel"));
	expect(editor->stop_view().Mode == core::tour_stop_view_mode::edit && !editor->findChild<QToolButton*>(QStringLiteral("tourStopEdit"))->isVisible(), "editing hides the pencil");
	expect(title->isVisible() && title->toPlainText() == QString::fromStdString(longTitle), "the title editor holds the whole title");
	expect(title->lineWrapMode() == QPlainTextEdit::WidgetWidth && title->height() >= 2 * title->fontMetrics().lineSpacing(), "the long title wraps onto more lines while written (" + std::to_string(title->height()) + "px)");
	expect(title->document()->size().height() >= 2.0, "its lines are all shown");
	expect(editor->findChild<QLabel*>(QStringLiteral("tourStopState")) == nullptr && editor->findChild<QPushButton*>(QStringLiteral("tourStopDone")) == nullptr, "no Editing label and no Done beside the title");
	const auto titleBox    = QRect{ title->mapTo(card, QPoint{ 0, 0 }), title->size() };
	const auto fieldsWidth = editor->findChild<QWidget*>(QStringLiteral("tourStopFields"))->width();
	expect(titleBox.width() * 10 >= fieldsWidth * 7, "the title keeps most of the row (" + std::to_string(titleBox.width()) + " of " + std::to_string(fieldsWidth) + ")");
	for (auto* const control : card->findChildren<QAbstractButton*>()) {
		if (control->isVisible() && control->objectName() != QStringLiteral("tourStopCollapse")) {
			const auto box = QRect{ control->mapTo(card, QPoint{ 0, 0 }), control->size() };
			expect(!box.intersects(titleBox), "nothing covers the title being written: " + control->objectName().toStdString());
		}
	}
	expect(save->isVisible() && cancel->isVisible() && save->isDefault() && cancel->x() < save->x() && save->mapTo(card, QPoint{ }).y() > editor->stop_body()->mapTo(card, QPoint{ }).y(), "Cancel and Save at the card's foot, Save the default");
	expect(useLines->isVisible() && useLines->mapTo(card, QPoint{ }).y() > editor->stop_body()->mapTo(card, QPoint{ }).y(), "written, Use selected lines is a quiet link under the explanation");
	{
		auto* const retarget = editor->findChild<QAction*>(QStringLiteral("tourRetarget"));
		expect(retarget != nullptr && retarget->isVisible(), "and the menus offer it while written");
	}
	expect(!complete->isVisible(), "and no Ready, next while written");
	if (!prefix.isEmpty()) {
		expect(scroll.grab().save(prefix + QStringLiteral("card-edit.png")), "render saved");
	}
	save->click();
	settle();
	expect(editor->stop_view().Mode == core::tour_stop_view_mode::read, "Save reads the stop again");

	// AC2. The number collapses and expands the card, by mouse and key, and the
	// tour stays where it was.
	bar->setValue(std::min(bar->maximum(), 420));
	settle();
	const auto before = bar->value();
	card              = card_of(*editor);
	auto* ring        = editor->stop_collapse_button();
	expect(ring != nullptr && ring->isVisible() && !ring->accessibleName().isEmpty(), "the card's number is a named button");
	click(ring, ring->rect().center());
	expect(editor->stop_collapsed() && card_of(*editor) == nullptr && editor->selected_stop()->Id == core::tour_stop_id{ 2 }, "a click on the number collapses the card; the stop stays current");
	expect(std::abs(bar->value() - before) <= 4, "collapsing keeps the tour where it was (" + std::to_string(before) + " to " + std::to_string(bar->value()) + ")");
	const auto row = outline->visualItemRect(outline->item(1));
	click(outline->viewport(), QPoint{ 22 + 11, row.top() + 8 + 11 });
	expect(!editor->stop_collapsed() && card_of(*editor) != nullptr, "a click on its number opens it again");
	expect(std::abs(bar->value() - before) <= 4, "and the tour still stays (" + std::to_string(bar->value()) + ")");
	card = card_of(*editor);
	click(card, QPoint{ card->width() - 60, 10 });
	expect(editor->stop_collapsed(), "a click in the header's empty space collapses it too");
	outline->setFocus(Qt::OtherFocusReason);
	press(outline, Qt::Key_Space);
	expect(!editor->stop_collapsed(), "Space on the route opens it");
	ring = editor->stop_collapse_button();
	ring->setFocus(Qt::OtherFocusReason);
	press(ring, Qt::Key_Return);
	expect(editor->stop_collapsed(), "Return on the number collapses it");
	outline->setCurrentRow(2, QItemSelectionModel::ClearAndSelect);
	settle();
	expect(!editor->stop_collapsed() && card_of(*editor) != nullptr, "another stop opens expanded");
	// Problem and Solution collapse by their headers.
	editor->open_step(core::tour_step::problem);
	settle();
	auto* const problemRow = editor->findChild<QAbstractButton*>(QStringLiteral("tourProblemRow"));
	expect(editor->problem_editor()->isVisible(), "Problem opens");
	problemRow->click();
	settle();
	expect(!editor->problem_editor()->isVisible() && editor->step_collapsed(), "its header collapses it");
	problemRow->click();
	settle();
	expect(editor->problem_editor()->isVisible() && !editor->step_collapsed(), "and opens it again");
	editor->open_step(core::tour_step::solution);
	settle();
	auto* const solutionRow = editor->findChild<QAbstractButton*>(QStringLiteral("tourSolutionRow"));
	solutionRow->click();
	settle();
	expect(!editor->solution_editor()->isVisible(), "Solution collapses by its header too");
	editor->show_stops();
	settle();
	expect(!editor->step_collapsed() && outline->isVisible(), "showing the stops opens them");

	// AC9. Remove is in every stop's menu, enabled, and the trash removes its own stop.
	auto* const menu   = editor->findChild<QMenu*>(QStringLiteral("tourStopMenu"));
	auto*       remove = editor->findChild<QAction*>(QStringLiteral("tourRemove"));
	for (int index = 0; index < outline->count(); ++index) {
		outline->clearSelection();
		const auto at = outline->visualItemRect(outline->item(index)).center();
		Q_EMIT outline->customContextMenuRequested(at);
		settle();
		expect(menu->isVisible() && menu->actions().contains(remove) && remove->isEnabled(), "stop " + std::to_string(index + 1) + "'s menu offers Remove");
		menu->hide();
	}
	// With nothing selected (a stop opened by the host), Remove still acts on the open stop.
	selected = core::tour_stop_id{ 3 };
	++selection;
	show();
	settle();
	outline->clearSelection();
	editor->findChild<QToolButton*>(QStringLiteral("tourStopMore"))->click();
	settle();
	expect(remove->isEnabled(), "Remove is offered with no row selected");
	if (auto* const more = editor->findChild<QMenu*>(QStringLiteral("tourStopMoreMenu")); more != nullptr) {
		more->hide();
	}
	// After a refused command (Ready refused), Remove stays.
	refusal = core::tour_refusal::incomplete;
	show();
	settle();
	expect(remove->isEnabled() && editor->stop_remove_button()->isEnabled(), "a refused command does not take Remove away");
	refusal = core::tour_refusal::none;
	show();
	settle();
	auto* const trash    = editor->stop_remove_button();
	auto* const fields   = editor->findChild<QWidget*>(QStringLiteral("tourStopFields"));
	auto* const showCode = editor->findChild<QPushButton*>(QStringLiteral("tourReturn"));
	// v002, owner: the trash takes no room from the title.
	expect(trash != nullptr && !trash->isVisible(), "a read stop's card has no trash by its title");
	const auto showRead = showCode->mapTo(fields, QPoint{ 0, 0 }).x();
	editor->begin_stop_edit();
	settle();
	{
		// v003, owner: "show in code shouldnt move, it should go on the right
		// of that and left of cancel".
		auto* const cancelButton = editor->findChild<QPushButton*>(QStringLiteral("tourStopCancel"));
		auto* const saveButton   = editor->findChild<QPushButton*>(QStringLiteral("tourStopSave"));
		const auto  trashAt      = trash->mapTo(fields, QPoint{ 0, 0 });
		const auto  cancelAt     = cancelButton->mapTo(fields, QPoint{ 0, 0 });
		const auto  showAt       = showCode->mapTo(fields, QPoint{ 0, 0 });
		expect(trash->isVisible() && trash->accessibleName() == QStringLiteral("Remove stop") && trash->toolTip() == QStringLiteral("Remove stop (Shift-click to skip confirmation)"), "written, the trash is named Remove stop and its tooltip tells of Shift");
		expect(showAt.x() == showRead, "Show in code keeps its place between read and written (" + std::to_string(showRead) + " to " + std::to_string(showAt.x()) + ")");
		expect(std::abs((trashAt.y() + trash->height() / 2) - (cancelAt.y() + cancelButton->height() / 2)) <= 4 && trashAt.x() > showAt.x() + showCode->width() && trashAt.x() + trash->width() < cancelAt.x() && cancelAt.x() < saveButton->mapTo(fields, QPoint{ 0, 0 }).x(),
			   "the trash is right of Show in code and left of Cancel, then Save");
		const auto titleRight = editor->stop_title()->mapTo(fields, QPoint{ editor->stop_title()->width(), 0 }).x();
		expect(titleRight >= fields->width() - 50, "the title has the row but for the chat toggle (" + std::to_string(titleRight) + " of " + std::to_string(fields->width()) + ")");
		// Owner: red only while Shift is held, saying a click will not ask.
		editor->stop_body()->setFocus(Qt::OtherFocusReason);
		settle();
		expect(!trash->property("danger").toBool(), "the trash is drawn as the other icons are");
		auto shiftDown = QKeyEvent{ QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier };
		QApplication::sendEvent(editor->stop_body(), &shiftDown);
		settle();
		expect(trash->property("danger").toBool(), "Shift held with the keyboard in the card turns it red");
		if (!prefix.isEmpty()) {
			expect(card_of(*editor)->grab().save(prefix + QStringLiteral("card-edit-shift.png")), "render saved");
		}
		auto shiftUp = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier };
		QApplication::sendEvent(editor->stop_body(), &shiftUp);
		settle();
		expect(!trash->property("danger").toBool(), "and back when Shift is released");
		if (!prefix.isEmpty()) {
			expect(card_of(*editor)->grab().save(prefix + QStringLiteral("card-edit-whole.png")), "render saved");
		}
	}
	auto*      confirm = editor->findChild<QWidget*>(QStringLiteral("tourStopRemoveConfirm"));
	const auto count   = draft.Stops.size();
	// A click asks, in place, and removes nothing.
	click(trash, trash->rect().center());
	expect(confirm->isVisible() && draft.Stops.size() == count, "a click on the trash asks first and removes nothing");
	expect(std::abs(showCode->mapTo(fields, QPoint{ 0, 0 }).x() - showRead) == 0, "asking moves nothing on the foot's row");
	if (!prefix.isEmpty()) {
		expect(card_of(*editor)->grab().save(prefix + QStringLiteral("card-edit-confirm.png")), "render saved");
	}
	editor->findChild<QPushButton*>(QStringLiteral("tourStopRemoveKeep"))->click();
	settle();
	expect(!confirm->isVisible() && draft.Stops.size() == count, "Keep keeps the stop");
	click(trash, trash->rect().center());
	editor->findChild<QPushButton*>(QStringLiteral("tourStopRemoveYes"))->click();
	settle();
	expect(draft.Stops.size() == count - 1U && std::ranges::find(draft.Stops, core::tour_stop_id{ 3 }, &core::tour_stop::Id) == draft.Stops.end(), "confirming removes exactly its own stop");
	expect(std::ranges::find(draft.Stops, core::tour_stop_id{ 2 }, &core::tour_stop::Id) != draft.Stops.end() && std::ranges::find(draft.Stops, core::tour_stop_id{ 4 }, &core::tour_stop::Id) != draft.Stops.end(), "and no other");
	// A Shift-click removes at once.
	const auto shiftTarget = editor->selected_stop()->Id;
	editor->begin_stop_edit();
	settle();
	expect(trash->isVisible(), "the next stop, written, has its trash");
	{
		const auto at      = trash->rect().center();
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ at }, trash->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ at }, trash->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier };
		QApplication::sendEvent(trash, &press);
		QApplication::sendEvent(trash, &release);
		settle();
	}
	expect(draft.Stops.size() == count - 2U && std::ranges::find(draft.Stops, shiftTarget, &core::tour_stop::Id) == draft.Stops.end() && !confirm->isVisible(), "a Shift-click removes the stop without asking");

	std::cout << "tour-stop-card-cleanup:pass\n";
	return EXIT_SUCCESS;
}
