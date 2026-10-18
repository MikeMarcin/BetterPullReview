// [[BPR-1592]]. RFC-BPR-0085 D3 and AC05: Notes are margin notes. Each note
// hangs at its line beside the code and opens in place, drawn dashed with a
// lock; notes on the pull request, the whole file and other files wait in the
// edge chips; an outdated note follows the placed ones and says why; N writes
// a note on the selected line; Draft comment is offered where the note can
// draft one; nothing is lost while it is written.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
namespace {
	using namespace bpr;

	void expect(const bool aValue, const std::string& aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	void key(QWidget* aTarget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(aTarget, &press);
		QApplication::processEvents();
	}

	void click(QWidget* aTarget, const QPoint aAt)
	{
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aAt }, aTarget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aAt }, aTarget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aTarget, &press);
		QApplication::sendEvent(aTarget, &release);
		QApplication::processEvents();
	}
}

int main(int argc, char** argv)
{
	QApplication app{ argc, argv };
	const auto   root = argc == 2 ? QString::fromLocal8Bit(argv[1]) : QString{ };

	// The comparison shown: renderer.cpp, 200 lines a side, each line 20
	// pixels down the code, scrolled by `scroll`.
	auto shown           = core::file_alignment{ };
	shown.Key.File       = core::file_identity_of("src/engine/renderer.cpp");
	shown.Key.LeftBlob   = blob(10U);
	shown.Key.RightBlob  = blob(20U);
	shown.Digest         = identity<core::file_comparison_digest>(5U);
	shown.LeftLineCount  = 200U;
	shown.RightLineCount = 200U;
	auto       scroll    = 0;
	auto       selected  = std::optional<qt::private_note_line>{ };
	auto       revealed  = std::vector<std::pair<core::diff_side, std::uint32_t>>{ };
	auto       away      = std::vector<std::uint64_t>{ };
	auto       marked    = 0;
	const auto file      = qt::private_note_target{ shown.Key.File, shown.Digest };
	const auto on        = [&](const core::diff_side aSide, const std::uint32_t aLine) {
		auto target = file;
		target.Line = core::note_line_anchor_at(shown, aSide, aLine);
		return target;
	};

	for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
		const auto name     = std::string{ mode == core::appearance_mode::light ? "light" : "dark" };
		auto       settings = qt::default_appearance_settings();
		settings.Mode       = mode;
		auto profile        = qt::appearance_profile{ settings };
		auto panel          = qt::private_notes_panel{ };
		profile.apply_to(panel);
		panel.resize(380, 720);
		auto intents = std::vector<qt::private_note_intent>{ };
		panel.set_dispatch([&](auto aIntent) { intents.push_back(std::move(aIntent)); });
		panel.set_line_locator([&](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 20 - scroll; });
		panel.set_comparison_source([&]() -> const core::file_alignment* { return &shown; });
		panel.set_selected_line_source([&] { return selected; });
		panel.set_line_dispatch([&](const core::diff_side aSide, const std::uint32_t aLine) { revealed.emplace_back(aSide, aLine); });
		panel.set_elsewhere_dispatch([&](const qt::private_note_item& aNote) { away.push_back(aNote.Id); });
		panel.set_marks_observer([&] { ++marked; });
		panel.set_colours(QColor{ 0x38, 0x8B, 0xFD }, QColor{ 0xD2, 0x99, 0x22 });
		panel.show();

		// Empty: say how to write one.
		panel.present({ .Scope = 7, .Available = true, .CurrentFile = file, .CurrentFileLabel = "src/engine/renderer.cpp" });
		QApplication::processEvents();
		expect(panel.empty_state()->isVisible() && panel.empty_state()->sentence().contains(QStringLiteral("press N")) && panel.empty_state()->action_button() != nullptr,
			   name + ": with no notes the tab says how to write one, with one action");

		// Notes at their lines, and the rest at the edges.
		auto changed       = on(core::diff_side::right, 30U);
		changed.Line->Blob = blob(99U);
		changed.Comparison = identity<core::file_comparison_digest>(4U);
		auto other         = qt::private_note_target{ core::file_identity_of("src/engine/renderer.h"), identity<core::file_comparison_digest>(9U) };
		other.Line         = core::note_line_anchor{ .Side = core::diff_side::right, .Line = 11U, .Blob = blob(40U) };
		auto notes         = qt::private_notes_projection{
			.Scope            = 7,
			.Available        = true,
			.CurrentFile      = file,
			.CurrentFileLabel = "src/engine/renderer.cpp",
			.Notes            = {
				{ .Id = 1, .Revision = 3, .Attachment = on(core::diff_side::right, 10U), .Label = "src/engine/renderer.cpp", .Text = "Check what device_pixel_ratio() returns on Wayland with fractional scaling before asking." },
				{ .Id = 2, .Revision = 1, .Attachment = on(core::diff_side::right, 12U), .Label = "src/engine/renderer.cpp", .Text = "Retry budget: compare with how submit() handles a full queue. Probably fine." },
				{ .Id = 3, .Revision = 1, .Attachment = std::nullopt, .Label = "Pull request", .Text = "Before approving: run the soak test with the retry budget at 1." },
				{ .Id = 4, .Revision = 1, .Attachment = file, .Label = "src/engine/renderer.cpp", .Text = "The new clamp at 4.0 changes behaviour on 5K displays. Worth a test?" },
				{ .Id = 5, .Revision = 1, .Attachment = other, .Label = "src/engine/renderer.h", .Text = "Should Renderer be non-copyable now?" },
				{ .Id = 6, .Revision = 2, .Attachment = changed, .Label = "src/engine/renderer.cpp", .Text = "Is the factor worth caching per frame?" },
			},
		};
		panel.present(notes);
		QApplication::processEvents();
		const auto first  = panel.note_rect(1U);
		const auto second = panel.note_rect(2U);
		const auto stale  = panel.note_rect(6U);
		expect(!first.isEmpty() && first.top() == 10 * 20 - 4 && !second.isEmpty() && second.top() >= first.bottom() + 6,
			   name + ": a note hangs at its line, and the next one below it never overlaps");
		expect(!stale.isEmpty() && stale.top() > second.bottom() && panel.placement(6U).Kind == core::note_placement_kind::outdated,
			   name + ": an outdated note follows the placed ones");
		expect(panel.note_rect(3U).isEmpty() && panel.note_rect(4U).isEmpty() && panel.note_rect(5U).isEmpty(), name + ": pull-request, whole-file and other-file notes are not at a line");
		auto chipWords = QStringList{ };
		for (const auto* chip : panel.edge_chips()) {
			chipWords.push_back(chip->text());
		}
		expect(chipWords.contains(QStringLiteral("1 on the pull request")) && chipWords.contains(QStringLiteral("1 on the whole file")) && chipWords.contains(QStringLiteral("1 on other files")),
			   name + ": the notes elsewhere wait in the edge chips: " + chipWords.join(QStringLiteral(", ")).toStdString());
		const auto marks = panel.line_marks(core::diff_side::right);
		expect(marks.size() == 2U && marks[0].Line == 10U && marks[1].Line == 12U && !marks[0].Open && panel.line_marks(core::diff_side::left).empty() && marked > 0,
			   name + ": the gutter marks each placed note's line, and not the outdated one's");
		const auto violations = qt::drawer_principle_violations(panel);
		expect(violations.isEmpty(), name + ": Notes keeps RFC-BPR-0085's principles: " + violations.join(QStringLiteral("; ")).toStdString());
		if (!root.isEmpty()) {
			expect(panel.grab().save(QStringLiteral("%1/notes-%2.png").arg(root, QString::fromStdString(name)), "PNG"), "the margin render is saved");
		}

		// Scrolled: a note whose line is above the view waits in a chip.
		scroll = 230;
		panel.refresh_geometry();
		QApplication::processEvents();
		auto lineChip = static_cast<QAbstractButton*>(nullptr);
		for (auto* chip : panel.edge_chips()) {
			if (chip->text() == QStringLiteral("11")) {
				lineChip = chip;
			}
		}
		expect(lineChip != nullptr && panel.note_rect(1U).isEmpty(), name + ": a note above the code in view waits in an edge chip with its line");
		revealed.clear();
		lineChip->click();
		QApplication::processEvents();
		expect(panel.open_note_id() == 1U && !revealed.empty() && revealed.back() == std::pair{ core::diff_side::right, 10U }, name + ": its chip opens the note and brings its line into view");
		panel.close_note();
		scroll = 0;
		panel.refresh_geometry();

		// Open in place, write, and save with ⌘↩.
		click(&panel, panel.note_rect(1U).center());
		auto* const card = panel.open_card();
		expect(card != nullptr && card->isVisible() && panel.open_note_id() == 1U && panel.body_editor()->toPlainText().startsWith(QStringLiteral("Check what")) && panel.body_editor()->hasFocus()
				   && card->geometry().top() <= 10 * 20 && card->geometry().bottom() >= 10 * 20,
			   name + ": a note opens in place beside its line, to write, with the keyboard in it");
		expect(panel.line_marks(core::diff_side::right)[0].Open, name + ": its line is washed while it is open");
		if (!root.isEmpty()) {
			expect(panel.grab().save(QStringLiteral("%1/notes-open-%2.png").arg(root, QString::fromStdString(name)), "PNG"), "the open render is saved");
		}
		panel.body_editor()->setPlainText(QStringLiteral("Edited privately"));
		key(panel.body_editor(), Qt::Key_Return, Qt::ControlModifier);
		expect(!intents.empty() && intents.back().Action == qt::private_note_action::save && intents.back().Id == 1U && intents.back().Revision == 3U && intents.back().Attachment == notes.Notes[0].Attachment
				   && intents.back().Text == "Edited privately",
			   name + ": ⌘↩ saves the note with its revision and its line");
		panel.refuse(QStringLiteral("Store unavailable"));
		expect(panel.body_editor()->toPlainText() == QStringLiteral("Edited privately") && panel.status_label()->isVisible(), name + ": a refusal keeps the text and says why");
		panel.present(notes);
		expect(panel.body_editor()->toPlainText() == QStringLiteral("Edited privately"), name + ": a refresh keeps unsaved text");
		panel.open_note(2U);
		expect(panel.open_note_id() == 1U && panel.body_editor()->toPlainText() == QStringLiteral("Edited privately"), name + ": another note cannot open over unsaved text");

		// Esc asks once, then discards.
		const auto before = intents.size();
		key(panel.body_editor(), Qt::Key_Escape);
		expect(panel.open_note_id() == 1U && panel.status_label()->text().contains(QStringLiteral("Esc again")) && intents.size() == before, name + ": Esc with unsaved changes says so and keeps them");
		key(panel.body_editor(), Qt::Key_Escape);
		expect(intents.back().Action == qt::private_note_action::discard && panel.open_note_id() == 0U, name + ": Esc again discards the changes and closes the note");

		// Draft comment: at its line for a placed note; not for an outdated one.
		panel.open_note(1U);
		expect(panel.promote_action()->isEnabled() && panel.promote_action()->text() == QStringLiteral("Draft comment"), name + ": a note at its line can draft a comment there");
		panel.promote_action()->trigger();
		expect(intents.back().Action == qt::private_note_action::promote && intents.back().Id == 1U && intents.back().Attachment->Line.has_value(), name + ": Draft comment asks for exactly that note");
		panel.close_note();
		panel.open_note(6U);
		expect(!panel.promote_action()->isEnabled() && panel.promote_action()->toolTip().contains(QStringLiteral("not in this comparison")), name + ": an outdated note has nowhere to draft a comment");
		panel.close_note();
		panel.open_note(3U);
		expect(panel.elsewhere_shown() && !panel.promote_action()->isEnabled(), name + ": a pull-request note opens among the notes elsewhere, and waits for Comment's target");
		panel.set_promotion_target(true, { });
		expect(panel.promote_action()->isEnabled(), name + ": with Comment's target it can draft a comment");
		panel.erase_action()->trigger();
		expect(intents.back().Action == qt::private_note_action::erase && intents.back().Id == 3U && intents.back().Revision == 1U, name + ": Delete uses the accepted revision");
		panel.close_note();
		if (!root.isEmpty()) {
			expect(panel.grab().save(QStringLiteral("%1/notes-elsewhere-%2.png").arg(root, QString::fromStdString(name)), "PNG"), "the elsewhere render is saved");
		}
		// Another file's note goes to its file.
		click(&panel, panel.note_rect(5U).center());
		expect(!away.empty() && away.back() == 5U && !panel.elsewhere_shown(), name + ": another file's note goes to its file");
		panel.close_note();

		// N: a new note on the selected line.
		selected = qt::private_note_line{ .Side = core::diff_side::right, .Line = 30U };
		panel.refresh_geometry();
		expect(panel.new_line_action()->isEnabled(), name + ": N is offered on a selected line");
		panel.setFocus();
		key(&panel, Qt::Key_N);
		expect(panel.writing() && panel.open_card() != nullptr && panel.open_card()->geometry().top() <= 30 * 20 && panel.open_card()->geometry().bottom() >= 30 * 20 && panel.body_editor()->hasFocus(),
			   name + ": N opens a new note at the selected line");
		panel.body_editor()->setPlainText(QStringLiteral("Ask whether to cache the factor per frame."));
		panel.save_action()->trigger();
		expect(intents.back().Action == qt::private_note_action::save && intents.back().Id == 0U && intents.back().Attachment == on(core::diff_side::right, 30U)
				   && intents.back().Attachment->Line->Blob == blob(20U),
			   name + ": the new note keeps its side, line and blob");
		panel.acknowledge(7U);
		expect(!panel.writing() && panel.open_card() == nullptr, name + ": once saved the note closes");
		selected.reset();
		panel.refresh_geometry();
		panel.setFocus();
		key(&panel, Qt::Key_N);
		expect(!panel.writing() && panel.notice_label()->isVisible() && panel.notice_label()->text().contains(QStringLiteral("Select a line")), name + ": N without a selected line says why");

		// Keyboard: Down then Return opens the first note.
		panel.setFocus();
		key(&panel, Qt::Key_Down);
		key(&panel, Qt::Key_Return);
		expect(panel.open_note_id() == 1U, name + ": Down and Return open a note from the keyboard");
		panel.close_note();

		// Another review starts clean.
		panel.open_note(1U);
		panel.body_editor()->setPlainText(QStringLiteral("Unsaved old review"));
		panel.present({ .Scope = 8, .Available = true });
		expect(panel.body_editor()->toPlainText().isEmpty() && !panel.writing() && panel.note_rect(1U).isEmpty(), name + ": a different review cannot inherit the prior note");
	}
	std::cout << "private-notes-panel-tests:pass\n";
}
