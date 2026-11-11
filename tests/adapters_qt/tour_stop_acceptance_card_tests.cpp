// [[BPR-2013]], [[BPR-2014]], RFC-BPR-0106. The tour stop in the Tour tab:
// every way of arriving at a stop shows its code (AC01), a stop at rest offers
// only reading and going, and what changes it appears only while it is
// edited (AC06), and the author accepts a stop the way a file is accepted,
// with the ring green or yellow from what was accepted and Accept, with its
// one-line reason, only while it is yellow (AC02, AC03).
//
// The fixture is one stop in each state after a push: 1 Accepted, 2 Changed
// since accepted (its lines), 3 Changed since accepted (a link in its text),
// 4 Not accepted yet (drafted by the agent). `--render PREFIX` saves light and
// dark pictures of each; `--look` leaves the window open for a person.
#include "../support/review_tour_fixture.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
#include "bpr/qt/appearance_profile.hpp"
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
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (int round = 0; round < 6; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 60) {
			QApplication::processEvents(QEventLoop::AllEvents, 10);
		}
	}
	void click(QWidget* aWidget, const QPoint& aAt)
	{
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
	}
	void key(QWidget* aWidget, const int aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
	}

	const auto before = *core::parse_git_commit_oid("1111111111111111111111111111111111111111");
	const auto after  = *core::parse_git_commit_oid("2222222222222222222222222222222222222222");

	[[nodiscard]] auto written(const std::uint64_t aId, std::string aTitle, std::string aBody, const std::uint32_t aFirst) -> core::tour_stop
	{
		auto value                 = tests::review_tour_fixture().Stops.front();
		value.Id                   = core::tour_stop_id{ aId };
		value.Title                = std::move(aTitle);
		value.Body                 = std::move(aBody);
		value.Original->First.Line = aFirst;
		value.Original->Last.Line  = aFirst + 5U;
		value.Original->Path       = "src/renderer.cpp";
		value.Related.clear();
		value.Acceptance.reset();
		return value;
	}

	/// The fixture after a push: what was accepted at `before`, checked at `after`.
	[[nodiscard]] auto pushed() -> core::review_tour
	{
		auto       draft = tests::review_tour_fixture();
		const auto files = std::vector<std::string>{ "src/renderer.cpp", "src/frame_pool.cpp" };
		draft.Stops      = {
			written(1, "Where the change starts", "The draw loop clears the target first now. `begin_frame()` hands back the retained frame, so clearing after it would erase what was kept.", 2U),
			written(2, "The pool hands back the last frame", "The pool keeps one frame and hands it back on the next draw.", 20U),
			written(3, "Marking layers dirty", "A layer is dirty until it is painted (`src/frame_pool.cpp:41-47`), and only dirty layers are drawn.", 40U),
		};
		for (auto& stop : draft.Stops) {
			stop.Acceptance = core::accept_tour_stop(draft, stop, files);
		}
		auto drafted = core::tour_stop_of({ .Path = "src/renderer_test.cpp", .FirstLine = 40U, .LastLine = 70U, .Title = "What the test pins down", .Explanation = "An idle second frame paints no layers. The test fails if `dirty_layers()` ever returns a clean layer." }, after);
		drafted.Id   = core::tour_stop_id{ 4 };
		draft.Stops.push_back(drafted);
		draft.LastStopId = 4;
		auto moved       = core::apply(draft, { .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = draft.Revision, .Kind = core::tour_command_kind::observe_head, .Head = after });
		expect(moved.Refusal == core::tour_refusal::none, "the head moves");
		draft = std::move(moved.Accepted);
		// What the comparison of `before` against `after` found.
		for (auto& stop : draft.Stops) {
			if (!stop.Acceptance) {
				continue;
			}
			for (auto& material : stop.Acceptance->Material) {
				const auto changed = (stop.Id.value() == 2U && !material.Reference) || (stop.Id.value() == 3U && material.Reference);
				material.CheckedAt = after;
				material.Verdict   = changed ? core::tour_material_verdict::changed : core::tour_material_verdict::unchanged;
			}
		}
		// The comparison mapped stop 2's changed lines to the new head, as a
		// push that edits lines in place does.
		auto& changed          = draft.Stops[1];
		changed.Mapped         = *changed.Original;
		changed.Mapped->Commit = after;
		changed.Mapped->Blob   = std::nullopt;
		changed.Mapping        = core::tour_mapping::changed_material;
		expect(core::validate(draft) == core::tour_refusal::none, "the fixture tour is valid");
		return draft;
	}

	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		const auto saved = aEditor.palette();
		for (const auto& [suffix, appearance] : { std::pair{ "light", core::appearance_mode::light }, std::pair{ "dark", core::appearance_mode::dark } }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(appearance);
			aEditor.setPalette(profile.derive(QApplication::palette()));
			settle();
			expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + "-" + suffix + ".png")), "render saved");
		}
		aEditor.setPalette(saved);
		settle();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   look   = argc > 1 && std::string_view{ argv[1] } == "--look";
	const auto   prefix = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };

	auto draft   = pushed();
	auto refusal = core::tour_refusal::none;
	auto actions = std::vector<qt::tour_editor_action>{ };
	auto sent    = std::vector<core::tour_command_kind>{ };

	qt::review_tour_editor editor;
	const auto             show = [&] { editor.present({ .Draft = std::make_shared<const core::review_tour>(draft), .Refusal = refusal, .SavedRevision = draft.Revision }); };
	editor.set_dispatch(
		[&](const core::tour_command aCommand) {
			sent.push_back(aCommand.Kind);
			auto outcome = core::apply(draft, aCommand);
			refusal      = outcome.Refusal;
			if (outcome.Refusal == core::tour_refusal::none) {
				draft = std::move(outcome.Accepted);
			}
			show();
		},
		[&](const qt::tour_editor_action aAction) { actions.push_back(aAction); });
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	editor.resize(440, 900);
	editor.show();
	show();
	editor.show_stops();
	settle();
	if (look) {
		editor.setWindowTitle(QStringLiteral("Tour stops after a push: 1 Accepted, 2 its lines changed, 3 a link changed, 4 not accepted yet"));
		return QApplication::exec();
	}

	auto* const outline = editor.outline();
	auto* const source  = editor.findChild<QLabel*>(QStringLiteral("tourStopSource"));
	auto* const accept  = editor.accept_button();
	auto* const reason  = editor.accept_reason();
	expect(outline != nullptr && outline->count() == 4 && source != nullptr && accept != nullptr && reason != nullptr, "the route and the card's parts exist");
	const auto navigations = [&] { return std::ranges::count(actions, qt::tour_editor_action::navigate_stop); };
	const auto shown       = [&](const char* aName) {
		auto* const widget = editor.findChild<QWidget*>(QString::fromLatin1(aName));
		return widget != nullptr && widget->isVisibleTo(&editor);
	};
	const auto stopAt = [&](const std::uint64_t aId) { return *std::ranges::find(draft.Stops, core::tour_stop_id{ aId }, &core::tour_stop::Id); };
	const auto state  = [&](const std::uint64_t aId) { return core::decide_tour_stop_acceptance(draft, stopAt(aId)).State; };

	// AC02. Each state from the core, and its ring.
	expect(state(1) == core::tour_acceptance_state::accepted && state(2) == core::tour_acceptance_state::changed && state(3) == core::tour_acceptance_state::changed && state(4) == core::tour_acceptance_state::not_accepted, "the fixture holds one stop in each state");
	const auto ring = [&](const int aRow) { return static_cast<core::tour_acceptance_state>(outline->item(aRow)->data(Qt::UserRole + 1).toInt()); };
	expect(ring(0) == core::tour_acceptance_state::accepted && ring(1) == core::tour_acceptance_state::changed && ring(2) == core::tour_acceptance_state::changed && ring(3) == core::tour_acceptance_state::not_accepted, "each ring draws its stop's state");
	expect(outline->item(0)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Accepted")) && outline->item(1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Changed since accepted")) && outline->item(3)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Not accepted yet")), "each row says its state in Review's words");
	expect(!outline->item(3)->text().contains(QStringLiteral("Suggested")), "no Suggested word");

	// AC06 and AC07. Gone from the application.
	for (const auto* name : { "tourReturn", "tourComplete", "tourHeadChip", "tourHeadConfirm", "tourSuggestOutline", "tourSuggestedOutline", "tourInsertOutline" }) {
		expect(editor.findChild<QWidget*>(QString::fromLatin1(name)) == nullptr, std::string{ name } + " is gone");
	}
	for (auto* const button : editor.findChildren<QAbstractButton*>()) {
		const auto text = button->text();
		expect(!text.contains(QStringLiteral("Ready, next")) && !text.contains(QStringLiteral("Show in code")) && !text.contains(QStringLiteral("Suggest an outline")) && !text.contains(QStringLiteral("Head changed")), "no button reads " + text.toStdString());
	}

	// AC06. Stop 1 at rest, accepted: its title, ring, lines link, text,
	// Edit and its chat; nothing that changes it, and no Accept.
	editor.go_to_stop(core::tour_stop_id{ 1 });
	settle();
	render(editor, prefix, "stop-accepted");
	expect(shown("tourStopReadTitle") && shown("tourStopEdit") && shown("tourStopReader") && shown("tourStopSource") && source->text().contains(QStringLiteral("<a ")), "at rest: title, Edit, text and the file-and-lines link");
	for (const auto* name : { "tourStopTitle", "tourStopBody", "tourUseSelectedLines", "tourStopMagic", "tourStopRemove", "tourStopCancel", "tourStopSave", "tourStopMore", "tourAddRelated", "tourRelatedSources" }) {
		expect(!shown(name), std::string{ name } + " is not offered at rest");
	}
	expect(!accept->isVisibleTo(&editor) && !reason->isVisibleTo(&editor), "an accepted stop asks for nothing");

	// Each yellow stop at rest: Accept, with its reason.
	const auto yellow = [&](const std::uint64_t aId, const QString& aReason, const char* aName) {
		editor.go_to_stop(core::tour_stop_id{ aId });
		settle();
		render(editor, prefix, aName);
		expect(accept->isVisibleTo(&editor) && accept->isEnabled() && accept->text() == QStringLiteral("Accept") && reason->isVisibleTo(&editor) && reason->text() == aReason, "stop " + std::to_string(aId) + " offers Accept, saying: " + reason->text().toStdString());
		for (const auto* name : { "tourStopRemove", "tourStopCancel", "tourStopSave", "tourUseSelectedLines", "tourStopMagic" }) {
			expect(!shown(name), std::string{ name } + " is not offered at rest on a yellow stop");
		}
	};
	yellow(2, QStringLiteral("The code under this stop changed in the last push."), "stop-changed-lines");
	yellow(3, QStringLiteral("src/frame_pool.cpp:41-47, which this stop links to, changed."), "stop-changed-link");
	yellow(4, QStringLiteral("Not accepted yet."), "stop-not-accepted");

	// AC06. Edited: the fields, Use selected lines, Enhance, Remove, Cancel and Save.
	editor.begin_stop_edit();
	settle();
	render(editor, prefix, "stop-editing");
	for (const auto* name : { "tourStopTitle", "tourStopBody", "tourUseSelectedLines", "tourStopMagic", "tourStopRemove", "tourStopCancel", "tourStopSave" }) {
		expect(shown(name), std::string{ name } + " is offered while the stop is edited");
	}
	expect(!accept->isVisibleTo(&editor), "while it is edited Save is the card's action");

	// AC06. Every operation offered before is still reachable: the stop's
	// context menu holds the route's operations and Mark not accepted.
	auto texts = QStringList{ };
	for (auto* const item : editor.stop_menu()->actions()) {
		if (!item->isSeparator()) {
			texts << item->text();
		}
	}
	for (const auto& wanted : { QStringLiteral("Edit"), QStringLiteral("Open side chat about this stop"), QStringLiteral("Insert a stop after"), QStringLiteral("Duplicate"), QStringLiteral("Merge with next"), QStringLiteral("Move up"), QStringLiteral("Move down"), QStringLiteral("Mark not accepted"), QStringLiteral("Remove stop") }) {
		expect(texts.contains(wanted), "the stop's context menu offers " + wanted.toStdString());
	}
	expect(editor.findChild<QAction*>(QStringLiteral("tourRetarget")) != nullptr, "Use selected lines keeps its action and key");

	// AC03. Save of an edit accepts the stop.
	editor.stop_body()->setPlainText(QString::fromStdString(stopAt(4).Body + " It runs in CI."));
	settle();
	expect(state(4) == core::tour_acceptance_state::not_accepted, "typing accepts nothing");
	editor.findChild<QPushButton*>(QStringLiteral("tourStopSave"))->click();
	settle();
	expect(state(4) == core::tour_acceptance_state::accepted && !accept->isVisibleTo(&editor), "Save accepts the stop it saved");
	render(editor, prefix, "stop-saved");

	// AC03. Accept on the current stop, which moves nowhere.
	editor.go_to_stop(core::tour_stop_id{ 2 });
	settle();
	accept->click();
	settle();
	expect(state(2) == core::tour_acceptance_state::accepted && editor.selected_stop()->Id == core::tour_stop_id{ 2 } && !accept->isVisibleTo(&editor), "Accept accepts the current stop and stays on it");

	// AC03. Opening a stop and clicking its ring accept nothing.
	const auto accepts = std::ranges::count(sent, core::tour_command_kind::accept);
	editor.go_to_stop(core::tour_stop_id{ 3 });
	settle();
	editor.stop_collapse_button()->click();
	settle();
	expect(editor.stop_collapsed(), "the ring closes the stop");
	const auto rect = outline->visualItemRect(outline->item(2));
	const auto was  = navigations();
	click(outline->viewport(), QPoint{ rect.left() + 120, rect.top() + 12 });
	expect(!editor.stop_collapsed() && std::ranges::count(sent, core::tour_command_kind::accept) == accepts && state(3) == core::tour_acceptance_state::changed, "opening the stop and its ring accept nothing");

	// AC01. Choosing the row already current shows its code too.
	expect(navigations() == was + 1, "choosing the row that is already current shows its code");

	// AC01. Every other arrival: another row, the keyboard, a link to a
	// stop (a notice's, a proposal's, Overview's) even to the current one,
	// Next changed stop's path, and the stop's file-and-lines link.
	auto       count = navigations();
	const auto other = outline->visualItemRect(outline->item(0));
	click(outline->viewport(), QPoint{ other.left() + 120, other.top() + 12 });
	expect(navigations() == count + 1 && editor.selected_stop()->Id == core::tour_stop_id{ 1 }, "choosing another row shows its code");
	count = navigations();
	outline->setFocus();
	key(outline, Qt::Key_Down);
	expect(navigations() == count + 1 && editor.selected_stop()->Id == core::tour_stop_id{ 2 }, "the stop keys show its code");
	count = navigations();
	editor.go_to_stop(core::tour_stop_id{ 2 });
	settle();
	expect(navigations() == count + 1, "a link to the current stop shows its code");
	count = navigations();
	editor.go_to_stop(core::tour_stop_id{ 4 });
	settle();
	expect(navigations() == count + 1, "a link to another stop shows its code");
	count = navigations();
	emit source->linkActivated(QStringLiteral("stop"));
	settle();
	expect(navigations() == count + 1, "the file-and-lines link brings the lines back");

	// Mark not accepted turns a stop yellow again.
	auto* const mark = editor.findChild<QAction*>(QStringLiteral("tourMarkNotAccepted"));
	expect(mark != nullptr, "Mark not accepted exists");
	mark->trigger();
	settle();
	expect(state(4) == core::tour_acceptance_state::not_accepted && accept->isVisibleTo(&editor), "Mark not accepted");

	// Critical review finding 3. Its lines changed and none are mapped at the
	// new head: Accept cannot record them, so it asks for its lines there
	// rather than turning green with nothing to compare.
	auto& unmapped = *std::ranges::find(draft.Stops, core::tour_stop_id{ 3 }, &core::tour_stop::Id);
	for (auto& material : unmapped.Acceptance->Material) {
		material.Verdict = material.Reference ? core::tour_material_verdict::unchanged : core::tour_material_verdict::missing;
	}
	show();
	editor.go_to_stop(core::tour_stop_id{ 3 });
	settle();
	expect(accept->isVisibleTo(&editor) && !accept->isEnabled() && reason->text() == QStringLiteral("The lines under this stop are no longer in the pull request. Choose its lines at this head to accept it."), "a stop whose lines are gone at the new head asks for its lines there: " + reason->text().toStdString());
	const auto acceptsBefore = std::ranges::count(sent, core::tour_command_kind::accept);
	editor.accept_stop(core::tour_stop_id{ 3 });
	settle();
	expect(std::ranges::count(sent, core::tour_command_kind::accept) == acceptsBefore + 1 && state(3) == core::tour_acceptance_state::changed, "an Accept that reaches the core anyway is refused");
	std::cout << "tour-stop-card:pass\n";
	return EXIT_SUCCESS;
}
