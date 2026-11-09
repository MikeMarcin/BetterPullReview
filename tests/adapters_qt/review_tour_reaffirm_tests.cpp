// [[BPR-1900]] follow-up, owner 2026-11-07: "Why did my tour draft have this
// warning at every stop" and "there's this button 'Reaffirm explanation for
// current head' which doesnt seem to do anything". A stop drafted at the head
// the tour observes shows no head-changed warning; one whose head moved on
// does, as a Head changed chip by its title, never in its text. A click asks
// "Still accurate?" in place; Confirm acts on the stop whose card it is on,
// with no other stop selected first and whatever else the route has selected.
// Where confirming can't apply it says what is missing, and a refusal is said
// on the card. `--render PREFIX` saves light and dark pictures of the chip;
// `--look` leaves the window open to try it by hand.
#include "../support/review_tour_fixture.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	[[nodiscard]] auto written(const std::uint64_t aId, const core::git_commit_oid& aAffirmed) -> core::tour_stop
	{
		auto value           = tests::review_tour_fixture().Stops.front();
		value.Id             = core::tour_stop_id{ aId };
		value.Title          = aId == 1U ? "Where the cache is filled" : "Where it is emptied";
		value.Body           = aId == 1U ? "The parser fills the cache **once**, on first use. Later reads borrow the cached table instead of parsing the source again, so a hot path that used to parse on every call now parses once per reset."
										 : "`reset()` empties the cache, so the next read parses again. Writers take the same lock as readers, so no reader ever sees a half-built table.";
		value.Preparation    = core::tour_preparation::editing;
		value.ReaffirmedHead = aAffirmed;
		return value;
	}
	[[nodiscard]] auto drafted(const std::uint64_t aId, const core::git_commit_oid& aReadHead) -> core::tour_stop
	{
		auto value = core::tour_stop_of({ .Path = "src/item.cpp", .FirstLine = 4U, .LastLine = 6U, .Title = aId == 3U ? "Where a read takes the lock" : "Where the lock is released", .Explanation = "Every read takes the table's lock before it looks at the cache, and releases it before it returns the borrowed table. The agent drafted this stop; its lines are still to be confirmed in the code." }, aReadHead);
		value.Id   = core::tour_stop_id{ aId };
		return value;
	}
	/// The application's own light and dark themes, as the window derives them.
	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		const auto before = aEditor.palette();
		for (const auto& [suffix, appearance] : { std::pair{ "light", core::appearance_mode::light }, std::pair{ "dark", core::appearance_mode::dark } }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(appearance);
			aEditor.setPalette(profile.derive(QApplication::palette()));
			QApplication::processEvents();
			expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + "-" + suffix + ".png")), "render saved");
		}
		aEditor.setPalette(before);
		QApplication::processEvents();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   look   = argc > 1 && std::string_view{ argv[1] } == "--look";
	const auto   prefix = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };
	const auto   before = *core::parse_git_commit_oid("1111111111111111111111111111111111111111");
	const auto   after  = *core::parse_git_commit_oid("2222222222222222222222222222222222222222");

	// The head moved from `before` to `after`. Stops 1 and 2 were written at
	// `before`; stop 3 was drafted at `before`, its lines not yet confirmed;
	// stop 4 was drafted at `after`, the head the tour observes.
	auto draft         = tests::review_tour_fixture();
	draft.ObservedHead = after;
	draft.Stops        = { written(1, before), written(2, before), drafted(3, before), drafted(4, after) };
	draft.LastStopId   = 4;
	expect(core::validate(draft) == core::tour_refusal::none, "the fixture tour is valid");
	auto refusal = core::tour_refusal::none;

	qt::review_tour_editor editor;
	const auto             show = [&] { editor.present({ .Draft = std::make_shared<const core::review_tour>(draft), .Refusal = refusal, .SavedRevision = draft.Revision }); };
	editor.set_dispatch(
		[&](const core::tour_command aCommand) {
			auto outcome = core::apply(draft, aCommand);
			refusal      = outcome.Refusal;
			if (outcome.Refusal == core::tour_refusal::none) {
				draft = std::move(outcome.Accepted);
			}
			show();
		},
		[](qt::tour_editor_action) { });
	editor.resize(420, 820);
	editor.show();
	show();
	editor.show_stops();
	QApplication::processEvents();
	if (look) {
		editor.setWindowTitle(QStringLiteral("Head changed chip: stops 1 and 2 were written before the head moved, 3 was drafted before it, 4 after"));
		return QApplication::exec();
	}

	auto* const chip     = editor.head_chip();
	auto* const confirm  = editor.head_confirm();
	auto* const question = editor.head_question();
	auto* const yes      = editor.head_confirm_button();
	auto* const edit     = editor.head_edit_button();
	auto* const source   = editor.findChild<QLabel*>(QStringLiteral("tourStopSource"));
	expect(chip != nullptr && confirm != nullptr && question != nullptr && yes != nullptr && edit != nullptr && source != nullptr, "the card's controls exist");
	expect(editor.findChild<QPushButton*>(QStringLiteral("tourReaffirm")) == nullptr, "the old Reaffirm button is gone");
	const auto warned  = [&] { return chip->isVisibleTo(&editor); };
	const auto stopAt  = [&](const std::uint64_t aId) { return *std::ranges::find(draft.Stops, core::tour_stop_id{ aId }, &core::tour_stop::Id); };
	const auto current = [&](const std::uint64_t aId) { return stopAt(aId).ReaffirmedHead == draft.ObservedHead; };
	const auto clean   = [&] { return !source->text().contains(QStringLiteral("head changed"), Qt::CaseInsensitive) && !source->text().contains(QStringLiteral("reaffirm"), Qt::CaseInsensitive) && !source->toolTip().contains(QStringLiteral("Head or surrounding")) && !editor.stop_body()->toPlainText().contains(QStringLiteral("head changed"), Qt::CaseInsensitive); };

	// No stop chosen: the card opens on stop 1, stale. Its chip, not its text
	// or source line, says so; the first click asks, the second confirms.
	expect(editor.selected_stop() != nullptr && editor.selected_stop()->Id == core::tour_stop_id{ 1 }, "the card opens on the first stop");
	expect(warned() && chip->text() == QStringLiteral("Head changed") && !confirm->isVisibleTo(&editor) && clean(), "a stale stop wears the chip, its text and source line untouched");
	render(editor, prefix, "head-chip");
	chip->click();
	QApplication::processEvents();
	render(editor, prefix, "head-chip-open");
	expect(confirm->isVisibleTo(&editor) && question->text() == QStringLiteral("The head moved since you wrote this. Still accurate?") && yes->isVisibleTo(&editor) && edit->isVisibleTo(&editor), "the chip asks in place, with Confirm and Edit");
	expect(!current(1), "asking changes nothing");
	yes->click();
	QApplication::processEvents();
	expect(refusal == core::tour_refusal::none && current(1) && !current(2), "Confirm marks exactly the card's stop current");
	expect(!warned() && !confirm->isVisibleTo(&editor), "the chip and its question go from that card");

	// Stop 2's card, with stop 4 selected on the route as well: the card's stop
	// is the one confirmed. Its chip opens closed.
	editor.go_to_stop(core::tour_stop_id{ 2 });
	QApplication::processEvents();
	editor.outline()->item(3)->setSelected(true);
	expect(editor.selected_stop()->Id == core::tour_stop_id{ 2 } && warned() && !confirm->isVisibleTo(&editor), "stop 2's card is stale, its chip closed");
	chip->click();
	yes->click();
	QApplication::processEvents();
	expect(current(2) && !current(3) && stopAt(4).ReaffirmedHead == after && !warned(), "only stop 2 is confirmed; the other stops are as they were");
	expect(stopAt(2).Preparation != core::tour_preparation::ready, "confirming an explanation does not make the stop Ready");

	// Stop 3, drafted before the head moved and its lines not confirmed: the
	// chip asks for the lines instead, with no Confirm.
	editor.go_to_stop(core::tour_stop_id{ 3 });
	QApplication::processEvents();
	chip->click();
	QApplication::processEvents();
	render(editor, prefix, "head-chip-lines");
	expect(warned() && confirm->isVisibleTo(&editor) && !yes->isVisibleTo(&editor) && edit->isVisibleTo(&editor) && question->text().contains(QStringLiteral("lines aren't confirmed yet")), "a drafted stop from the old head asks for its lines first");
	// Were Confirm to reach the core anyway, its refusal is said on the card.
	yes->show();
	yes->click();
	QApplication::processEvents();
	expect(refusal == core::tour_refusal::incomplete && !current(3), "the core refuses it");
	expect(confirm->isVisibleTo(&editor) && question->text().contains(QStringLiteral("can't be confirmed yet")), "the refusal is said on the card, not silently");
	// Edit opens the stop's explanation with the keyboard in it.
	edit->click();
	QApplication::processEvents();
	expect(editor.stop_view().Mode == core::tour_stop_view_mode::edit && !confirm->isVisibleTo(&editor) && warned(), "Edit opens the explanation and closes the question; the chip stays");

	// Stop 4, drafted at the head the tour observes: no chip.
	editor.go_to_stop(core::tour_stop_id{ 4 });
	QApplication::processEvents();
	expect(!warned() && !confirm->isVisibleTo(&editor) && clean(), "a stop drafted at the observed head has no head-changed chip");
	expect(stopAt(4).Preparation != core::tour_preparation::ready, "a drafted stop is not Ready until its lines are confirmed");

	// The pull request's head really moves on: stop 4 asks again.
	const auto later = *core::parse_git_commit_oid("3333333333333333333333333333333333333333");
	auto       moved = core::apply(draft, { .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = draft.Revision, .Kind = core::tour_command_kind::observe_head, .Head = later });
	expect(moved.Refusal == core::tour_refusal::none, "the head moves");
	draft = std::move(moved.Accepted);
	show();
	QApplication::processEvents();
	expect(editor.selected_stop()->Id == core::tour_stop_id{ 4 } && warned(), "after a real head change the drafted stop warns again");

	// Confirmed replacement and missing prose are not source warnings.
	draft.Stops.front().Mapped  = draft.Stops.front().Original;
	draft.Stops.front().Mapping = core::tour_mapping::author_selected;
	draft.Stops.front().Body.clear();
	show();
	editor.go_to_stop(draft.Stops.front().Id);
	QApplication::processEvents();
	expect(!source->text().contains(QStringLiteral("Source needs attention")) && !source->text().contains(QStringLiteral("Lines not confirmed")), "a confirmed author replacement stays confirmed when prose is empty");
	render(editor, prefix, "confirmed-replacement");
	draft.Stops.front().Mapping = core::tour_mapping::ambiguous;
	show();
	QApplication::processEvents();
	expect(source->text().contains(QStringLiteral("Source needs attention")) && !source->text().contains(QStringLiteral("Lines not confirmed")), "ambiguous mapping warns separately from retained original confirmation");
	render(editor, prefix, "ambiguous-mapping");
	std::cout << "review-tour-reaffirm:pass\n";
	return EXIT_SUCCESS;
}
