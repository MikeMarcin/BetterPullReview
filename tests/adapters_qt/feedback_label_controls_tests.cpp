/// \file
/// \brief [[BPR-1083]]. Proves the selected authoring controls in the real
/// comment editor: the label chip and its menu, the decoration toggles, ⌘L
/// and ⌘0, and the leading `:xx:` shortcodes (RFC-BPR-0048 R09, R10;
/// `docs/ux/conventional-comments-feedback.md`).
/// \details Every check drives `review_draft_editor` as composed, with the
/// revisioned dispatch the workbench gives it, and reads the commands it
/// emits. A label chosen with a control must leave the editor exactly as the
/// same text typed by hand does: one ordinary whole-body edit.

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/review_draft_editor.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto a_draft(std::string aBody, const core::review_draft_state aState = core::review_draft_state::drafting) -> core::review_comment_draft
	{
		auto comparison = std::array<std::uint8_t, 32>{ };
		comparison.fill(2U);
		return core::review_comment_draft{
			.Id     = core::review_draft_id{ 7U },
			.Anchor = core::source_line_ref{ .Comparison = core::file_comparison_digest{ comparison }, .Side = core::diff_side::right, .Line = 4U },
			.Body   = std::move(aBody),
			.State  = aState,
		};
	}

	/// \brief One composed editor and the commands it emits.
	struct fixture
	{
		QWidget                                            Window;
		qt::review_draft_editor*                           Editor = nullptr;
		std::vector<core::revisioned_review_draft_command> Commands;
		core::revisioned_review_draft                      Accepted;

		explicit fixture(std::string aBody, const core::review_draft_state aState = core::review_draft_state::drafting)
			: Accepted{ a_draft(std::move(aBody), aState), 3U, core::review_draft_scope_id{ 1U } }
		{
			auto* const layout = new QVBoxLayout{ &Window };
			Editor             = new qt::review_draft_editor{ [](auto) { }, [](auto) { }, &Window };
			layout->addWidget(Editor);
			Editor->set_revisioned_dispatch([this](core::revisioned_review_draft_command aCommand) { Commands.push_back(std::move(aCommand)); });
			Editor->present(Accepted);
			Window.resize(900, 600);
			Window.show();
			settle();
		}

		[[nodiscard]] auto body() const -> QPlainTextEdit* { return Editor->body_editor(); }
		[[nodiscard]] auto controls() const -> qt::feedback_label_controls* { return Editor->label_controls(); }
		[[nodiscard]] auto text() const -> std::string { return body()->toPlainText().toStdString(); }
		[[nodiscard]] auto menu() const -> qt::feedback_label_menu* { return controls()->menu(); }
	};

	void key(QWidget* const aTarget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier, const QString& aText = { })
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
		QApplication::sendEvent(aTarget, &press);
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers, aText };
		QApplication::sendEvent(aTarget, &release);
	}

	/// \brief Types characters one by one, as a person does.
	void type(QPlainTextEdit* const aBody, const QString& aText)
	{
		for (const auto character : aText) {
			key(aBody, character == QLatin1Char{ ':' } ? Qt::Key_Colon : Qt::Key_A, Qt::NoModifier, QString{ character });
		}
	}

	[[nodiscard]] auto overrides(QWidget* const aTarget, const int aKey) -> bool
	{
		auto event = QKeyEvent{ QEvent::ShortcutOverride, aKey, Qt::ControlModifier };
		event.ignore();
		QApplication::sendEvent(aTarget, &event);
		return event.isAccepted();
	}

	void caret_at(QPlainTextEdit* const aBody, const int aPosition)
	{
		auto cursor = aBody->textCursor();
		cursor.setPosition(aPosition);
		aBody->setTextCursor(cursor);
	}

	void test_the_chip_and_menu_edit_once_and_undo_once()
	{
		auto        f    = fixture{ "LightScale ignores the view." };
		auto* const chip = f.controls()->chip();
		expect(chip->text() == QStringLiteral("Plain ▾") && chip->accessibleName() == QStringLiteral("Label: Plain comment"),
			   "a plain comment's chip reads Plain");
		expect(chip->toolTip() == QStringLiteral("Label (%1), or type a code such as :qu: at the start").arg(QKeySequence{ Qt::CTRL | Qt::Key_L }.toString(QKeySequence::NativeText)),
			   "its tooltip names ⌘L and the shortcodes");
		auto* next = f.body()->nextInFocusChain();
		while (next != f.body() && (f.body()->isAncestorOf(next) || !(next->focusPolicy() & Qt::TabFocus) || !next->isVisibleTo(&f.Window))) {
			next = next->nextInFocusChain();
		}
		expect(next == chip, "the chip is the tab stop after the body");

		chip->click();
		settle();
		expect(f.menu()->isVisible() && f.controls()->statistics().MenuOpens == 1U, "clicking the chip opens the label menu");
		expect(f.menu()->plain_button()->isChecked() && f.menu()->plain_button()->accessibleName() == QStringLiteral("Plain comment"),
			   "Plain comment comes first and is the current choice");
		for (const auto label : { core::feedback_label::praise, core::feedback_label::issue, core::feedback_label::quibble }) {
			const auto* const button = f.menu()->label_button(label);
			expect(button != nullptr && button->accessibleName() == qt::feedback_label_text(label) && button->toolTip() == qt::feedback_label_description(label)
					   && button->text().contains(QStringLiteral(":%1:").arg(QString::fromLatin1(qt::feedback_shortcode(label).data(), 2))) && !button->icon().isNull(),
				   "each grid entry shows its mark, name and code, and its catalog description as a tooltip");
		}
		expect(!f.menu()->blocking_button()->isEnabled(), "the decorations wait for a label");

		f.menu()->label_button(core::feedback_label::issue)->click();
		settle();
		expect(f.text() == "issue: LightScale ignores the view.", "choosing issue writes the header before the subject");
		expect(!f.menu()->isVisible(), "the menu closes");
		expect(f.body()->textCursor().position() == 7, "the caret is after the prefix");
		expect(f.Commands.size() == 1U, "one choice is one draft edit");
		const auto plan = core::plan_feedback_draft_edit(f.Accepted, core::feedback_draft_basis_of(f.Accepted), { .Change = core::feedback_label_change::set, .Label = core::feedback_label::issue });
		expect(plan.Command.has_value() && f.Commands.front().Command.Body == plan.Command->Command.Body
				   && f.Commands.front().ExpectedRevision == plan.Command->ExpectedRevision && f.Commands.front().Scope == plan.Command->Scope
				   && f.Commands.front().Command.Kind == core::review_draft_command_kind::edit,
			   "and it is the command the core plans for that label: the whole body at the shown revision");
		expect(chip->text() == QStringLiteral("issue ▾") && chip->accessibleName() == QStringLiteral("Label: issue"), "the chip shows the label");
		const auto selections = f.body()->extraSelections();
		expect(selections.size() == 1 && selections.front().cursor.selectionStart() == 0 && selections.front().cursor.selectionEnd() == 7,
			   "the label is the highlighted words at the start of the field");

		f.body()->undo();
		settle();
		expect(f.text() == "LightScale ignores the view.", "one undo step restores the comment");
	}

	void test_decorations_and_plain()
	{
		auto f = fixture{ "Fix it." };
		f.controls()->chip()->click();
		settle();
		f.menu()->label_button(core::feedback_label::issue)->click();
		settle();
		auto* const blocking = f.controls()->blocking_toggle();
		expect(blocking->isVisible() && !blocking->isChecked() && blocking->accessibleName() == QStringLiteral("blocking"),
			   "with a label the three decorations sit beside the chip as toggles");
		blocking->click();
		settle();
		expect(f.text() == "issue (blocking): Fix it." && blocking->isChecked() && f.controls()->state().Expectation == core::feedback_expectation::blocking,
			   "blocking adds (blocking)");
		f.controls()->non_blocking_toggle()->click();
		settle();
		expect(f.text() == "issue (non-blocking): Fix it." && !blocking->isChecked() && f.controls()->non_blocking_toggle()->isChecked(),
			   "non-blocking leaves exactly that one");
		f.controls()->non_blocking_toggle()->click();
		settle();
		expect(f.text() == "issue: Fix it.", "pressing a written decoration removes it");

		f.controls()->open_menu();
		settle();
		f.menu()->if_minor_button()->click();
		settle();
		expect(f.text() == "issue (if-minor): Fix it." && f.menu()->isVisible() && f.menu()->if_minor_button()->isChecked(),
			   "the menu's Adds row decorates and stays open");
		f.menu()->label_button(core::feedback_label::question)->click();
		settle();
		expect(f.text() == "question (if-minor): Fix it.", "a new label keeps the decorations");

		f.controls()->open_menu();
		settle();
		expect(overrides(f.menu()->label_button(core::feedback_label::issue), Qt::Key_0), "inside the open menu ⌘0 is the menu's own key");
		key(f.menu(), Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(f.text() == "Fix it." && !f.menu()->isVisible(), "⌘0 in the menu makes the comment plain and closes it");
		expect(!overrides(f.body(), Qt::Key_0), "outside the menu ⌘0 stays the window's Home shortcut");
		expect(f.Commands.size() == 7U, "each change was one draft edit");
	}

	void test_command_l_belongs_to_the_focused_editor()
	{
		auto f     = fixture{ "Body." };
		auto other = QLineEdit{ };
		expect(overrides(f.body(), Qt::Key_L), "while the body has focus the editor claims ⌘L");
		expect(!overrides(&other, Qt::Key_L) && !overrides(f.controls()->chip(), Qt::Key_L), "anywhere else ⌘L is left to Home's Open bar");
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		expect(f.menu()->isVisible() && f.body()->toPlainText() == QStringLiteral("Body."), "⌘L in the body opens the label menu and types nothing");
		f.menu()->close();
		settle();
		auto locked = fixture{ "Publishing.", core::review_draft_state::publishing };
		expect(!overrides(locked.body(), Qt::Key_L) && !locked.controls()->chip()->isEnabled(), "a draft that is not editable claims nothing and offers no chip");
	}

	void test_every_label_and_plain_are_reachable_by_keyboard()
	{
		auto f = fixture{ "Subject." };
		for (auto index = std::size_t{ 0U }; index < core::feedback_label_count; ++index) {
			key(f.body(), Qt::Key_L, Qt::ControlModifier);
			settle();
			expect(f.menu()->isVisible(), "⌘L opens the menu");
			// From the current choice back to Plain, then down into the grid.
			for (auto step = 0; step < 6; ++step) {
				key(f.menu(), Qt::Key_Up);
			}
			for (auto step = 0; step < 3; ++step) {
				key(f.menu(), Qt::Key_Left);
			}
			key(f.menu(), Qt::Key_Down);
			for (auto row = std::size_t{ 0U }; row < index / 3U; ++row) {
				key(f.menu(), Qt::Key_Down);
			}
			for (auto column = std::size_t{ 0U }; column < index % 3U; ++column) {
				key(f.menu(), Qt::Key_Right);
			}
			key(f.menu(), Qt::Key_Return);
			settle();
			const auto label = static_cast<core::feedback_label>(static_cast<std::uint8_t>(index));
			expect(f.controls()->state().Label == label && !f.menu()->isVisible(), "arrow keys and Return reach " + std::string{ core::stable_name(label) });
		}
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		for (auto step = 0; step < 6; ++step) {
			key(f.menu(), Qt::Key_Up);
		}
		key(f.menu(), Qt::Key_Return);
		settle();
		expect(f.text() == "Subject.", "and Plain comment, from the top of the menu");
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		key(f.menu(), Qt::Key_Escape);
		settle();
		expect(!f.menu()->isVisible() && f.text() == "Subject.", "Esc closes the menu and changes nothing");
	}

	void test_controls_and_typed_syntax_are_the_same_draft()
	{
		auto chosen = fixture{ "Read the light scale from the view." };
		chosen.controls()->open_menu();
		settle();
		chosen.menu()->label_button(core::feedback_label::suggestion)->click();
		settle();
		chosen.controls()->blocking_toggle()->click();
		settle();
		auto typed = fixture{ "Read the light scale from the view." };
		typed.body()->setPlainText(QStringLiteral("suggestion (blocking): Read the light scale from the view."));
		settle();
		expect(chosen.Commands.back().Command.Body == typed.Commands.back().Command.Body, "the chip and the typed header leave the same body");
		const auto left  = core::recognize_feedback_prefix(chosen.Commands.back().Command.Body);
		const auto right = core::recognize_feedback_prefix(typed.Commands.back().Command.Body);
		expect(left.Status == core::feedback_prefix_status::recognized && left.Label == right.Label && left.Expectation == right.Expectation && left.Subject == right.Subject,
			   "and the same projected intent");
		expect(typed.controls()->state().Label == core::feedback_label::suggestion && typed.controls()->blocking_toggle()->isChecked(),
			   "a header typed by hand shows on the chip and toggles too");
	}

	void test_a_label_comes_first_on_an_empty_comment()
	{
		auto f = fixture{ "" };
		f.controls()->open_menu();
		settle();
		f.menu()->label_button(core::feedback_label::issue)->click();
		settle();
		expect(f.text() == "issue: " && f.controls()->state().AwaitingSubject, "an empty comment takes the label alone");
		expect(f.controls()->chip()->text() == QStringLiteral("issue ▾") && f.controls()->chip()->accessibleDescription().contains(QStringLiteral("waiting for its subject")),
			   "the chip shows the label and says it is waiting for its subject");
		f.controls()->blocking_toggle()->click();
		settle();
		f.controls()->open_menu();
		settle();
		f.menu()->label_button(core::feedback_label::question)->click();
		settle();
		expect(f.text() == "question (blocking): ", "before a subject the label can still change");
		type(f.body(), QStringLiteral("x"));
		settle();
		expect(f.controls()->state().Label == core::feedback_label::question && !f.controls()->state().AwaitingSubject, "once a subject follows it is an ordinary label");
		f.body()->setPlainText(QStringLiteral("question (blocking): "));
		f.controls()->open_menu();
		settle();
		f.menu()->plain_button()->click();
		settle();
		expect(f.text().empty(), "and Plain removes it again");
	}

	void test_refusals_change_nothing()
	{
		auto f = fixture{ "issue (blocking: broken header" };
		expect(!f.controls()->state().Label.has_value(), "a malformed header shows Plain");
		const auto before = f.Commands.size();
		f.controls()->open_menu();
		settle();
		f.menu()->label_button(core::feedback_label::question)->click();
		settle();
		expect(f.text() == "issue (blocking: broken header" && f.Commands.size() == before
				   && f.controls()->last_refusal() == core::feedback_edit_refusal::unsupported_header
				   && f.Editor->mutation_status_text().contains(QStringLiteral("Nothing was changed")),
			   "malformed leading syntax is never deleted, and the refusal is said");
		auto quoted = fixture{ "> quoted text" };
		quoted.controls()->open_menu();
		settle();
		quoted.menu()->label_button(core::feedback_label::issue)->click();
		settle();
		expect(quoted.text() == "> quoted text" && quoted.controls()->last_refusal() == core::feedback_edit_refusal::restructuring_required,
			   "a label is not forced in front of a quote");
	}

	void test_shortcodes_complete_and_replace_at_the_start()
	{
		auto f = fixture{ "Fix it." };
		f.Editor->set_label_helper(true);
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":q"));
		settle();
		auto* const completion = f.controls()->completion();
		expect(completion->isVisible() && f.controls()->completion_labels() == std::vector{ core::feedback_label::question, core::feedback_label::quibble },
			   "a colon and one letter open completion, codes first");
		key(f.body(), Qt::Key_Down);
		expect(completion->currentRow() == 1, "Down moves to quibble");
		key(f.body(), Qt::Key_Up);
		expect(completion->currentRow() == 0, "Up moves back to question");
		const auto typedCommands = f.Commands.size();
		key(f.body(), Qt::Key_Tab);
		settle();
		expect(f.text() == "question: Fix it." && !completion->isVisible() && f.Commands.size() == typedCommands + 1U,
			   "Tab takes the highlighted label in one draft edit");
		f.body()->undo();
		settle();
		expect(f.text() == ":qFix it.", "⌘Z right after restores the literal code");

		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":is:"));
		settle();
		expect(f.text() == "issue: Fix it.", "the closing colon of an exact code replaces it at once");
		f.body()->undo();
		settle();
		expect(f.text() == ":is:Fix it.", "and one undo restores the literal :is:");

		f.body()->setPlainText(QStringLiteral("question (blocking): Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":is:"));
		settle();
		expect(f.text() == "issue (blocking): Fix it.", "a code in front of a label replaces that label and never adds a second prefix");

		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":q"));
		settle();
		key(f.body(), Qt::Key_Escape);
		settle();
		expect(!completion->isVisible() && f.text() == ":qFix it.", "Esc leaves the text alone");

		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":zz:"));
		settle();
		expect(f.text() == ":zz:Fix it." && !completion->isVisible(), "an unknown code leaves the text alone");

		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		f.body()->insertPlainText(QStringLiteral(":qu:"));
		settle();
		expect(f.text() == ":qu:Fix it." && !completion->isVisible(), "pasted text leaves the text alone");

		f.body()->setPlainText(QStringLiteral("Fix it "));
		caret_at(f.body(), 7);
		type(f.body(), QStringLiteral(":is:"));
		settle();
		expect(f.text() == "Fix it :is:", "a code that is not at the very start, like GitHub's own :emoji:, is never touched");
		expect(f.controls()->statistics().Shortcodes == 3U, "three codes were replaced");

		auto off = fixture{ "Fix it." };
		caret_at(off.body(), 0);
		type(off.body(), QStringLiteral(":qu:"));
		settle();
		expect(off.text() == ":qu:Fix it." && !off.controls()->completion()->isVisible() && !off.controls()->helper(),
			   "with the helper off, which is the default, :qu: is plain text");
		off.body()->setPlainText(QStringLiteral("question: typed in full"));
		settle();
		expect(off.controls()->state().Label == core::feedback_label::question, "a label typed in full counts either way");
	}

	void test_the_chip_keeps_its_size_when_narrow()
	{
		auto       f    = fixture{ "suggestion (blocking): Narrow." };
		const auto wide = f.controls()->chip()->size();
		f.Window.resize(320, 600);
		settle();
		expect(f.controls()->chip()->size() == wide && f.controls()->chip()->width() >= f.controls()->chip()->sizeHint().width(),
			   "the chip does not change size at narrow widths");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_chip_and_menu_edit_once_and_undo_once();
	test_decorations_and_plain();
	test_command_l_belongs_to_the_focused_editor();
	test_every_label_and_plain_are_reachable_by_keyboard();
	test_controls_and_typed_syntax_are_the_same_draft();
	test_a_label_comes_first_on_an_empty_comment();
	test_refusals_change_nothing();
	test_shortcodes_complete_and_replace_at_the_start();
	test_the_chip_keeps_its_size_when_narrow();
	std::cout << "feedback-label-controls-tests:pass\n";
	return EXIT_SUCCESS;
}
