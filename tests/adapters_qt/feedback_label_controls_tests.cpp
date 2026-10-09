/// \file
/// \brief [[BPR-1083]]. Proves the selected authoring controls in the real
/// comment editor: the label chip and its menu, the decoration toggles, ⌘L
/// and ⌘0, and the leading `:xx:` shortcodes (RFC-BPR-0048 R09, R10;
/// `docs/ux/conventional-comments-feedback.md`).
/// \details Every check drives `review_draft_editor` as composed, with the
/// revisioned dispatch the workbench gives it, and reads the commands it
/// emits. A label chosen with a control must leave the editor exactly as the
/// same text typed by hand does: one ordinary whole-body edit.

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/review_draft_editor.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHelpEvent>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
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
		/// The owner's latest answer. Like the composed host's shared drafts,
		/// it answers each edit at once unless `Answering` is false, when the
		/// edit waits in `Unanswered` for `answer()`.
		core::revisioned_review_draft                      Latest;
		bool                                               Answering = true;
		std::vector<core::revisioned_review_draft_command> Unanswered;

		explicit fixture(std::string aBody, const core::review_draft_state aState = core::review_draft_state::drafting)
			: Accepted{ a_draft(std::move(aBody), aState), 3U, core::review_draft_scope_id{ 1U } }
			, Latest{ Accepted }
		{
			auto* const layout = new QVBoxLayout{ &Window };
			Editor             = new qt::review_draft_editor{ [](auto) { }, [](auto) { }, &Window };
			layout->addWidget(Editor);
			Editor->set_revisioned_dispatch([this](core::revisioned_review_draft_command aCommand) {
				Commands.push_back(aCommand);
				if (Answering) {
					answer(aCommand);
				} else {
					Unanswered.push_back(std::move(aCommand));
				}
			});
			Editor->present(Accepted);
			Window.resize(900, 600);
			Window.show();
			settle();
		}

		/// \brief The owner accepts one edit made on its latest revision.
		void answer(const core::revisioned_review_draft_command& aCommand)
		{
			if (aCommand.ExpectedRevision != Latest.Revision) {
				Editor->present_conflict({ .Accepted = Latest, .Refusal = core::revisioned_draft_refusal::conflict });
				return;
			}
			auto next       = Latest;
			next.Draft.Body = aCommand.Command.Body;
			next.Revision   = Latest.Revision + 1U;
			Latest          = next;
			Editor->acknowledge(aCommand, next);
		}

		[[nodiscard]] auto body() const -> QPlainTextEdit* { return Editor->body_editor(); }
		[[nodiscard]] auto controls() const -> qt::feedback_label_controls* { return Editor->label_controls(); }
		[[nodiscard]] auto text() const -> std::string { return body()->toPlainText().toStdString(); }
		[[nodiscard]] auto menu() const -> qt::feedback_label_menu* { return controls()->menu(); }
	};

	void save_render(QWidget& aWidget, const char* const aName)
	{
		const auto folder = qEnvironmentVariable("BPR_FEEDBACK_RENDER_DIR");
		if (!folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + QString::fromLatin1(aName) + QStringLiteral(".png")));
		}
	}

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
		save_render(*f.menu(), "menu");
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
		using W = qt::feedback_mark_weight;
		auto f  = fixture{ "Fix it." };
		f.controls()->chip()->click();
		settle();
		f.menu()->label_button(core::feedback_label::issue)->click();
		settle();
		auto* const blocking    = f.controls()->blocking_toggle();
		auto* const nonBlocking = f.controls()->non_blocking_toggle();
		auto* const ifMinor     = f.controls()->if_minor_toggle();
		expect(blocking->isVisible() && !blocking->isChecked() && blocking->accessibleName() == QStringLiteral("blocking"),
			   "with a label the three decorations sit beside the chip as toggles");
		blocking->click();
		settle();
		expect(f.text() == "issue (blocking): Fix it." && blocking->isChecked() && f.controls()->state().Emphasis == qt::feedback_mark_emphasis{ .Weight = W::blocking },
			   "blocking adds (blocking)");
		nonBlocking->click();
		settle();
		expect(f.text() == "issue (non-blocking): Fix it." && !blocking->isChecked() && nonBlocking->isChecked(),
			   "non-blocking takes blocking's place: a label carries one weight");
		// [[BPR-1083]] v004 (owner, decorations-v003: "multiple decorators can
		// be applied to a comment. like (non-blocking, if-minor)").
		ifMinor->click();
		settle();
		expect(f.text() == "issue (non-blocking, if-minor): Fix it." && nonBlocking->isChecked() && ifMinor->isChecked()
				   && f.controls()->state().Emphasis == qt::feedback_mark_emphasis{ .Weight = W::non_blocking, .IfMinor = true },
			   "if-minor combines with the weight");
		nonBlocking->click();
		settle();
		expect(f.text() == "issue (if-minor): Fix it." && ifMinor->isChecked(), "pressing a written decoration takes out that one alone");

		f.controls()->open_menu();
		settle();
		f.menu()->blocking_button()->click();
		settle();
		expect(f.text() == "issue (if-minor, blocking): Fix it." && f.menu()->isVisible() && f.menu()->blocking_button()->isChecked() && f.menu()->if_minor_button()->isChecked(),
			   "the menu's Adds row combines decorations and stays open");
		// Any custom decoration, written as typed.
		auto* const field = f.menu()->custom_field();
		expect(field->isEnabled() && f.menu()->custom_buttons().empty(), "the menu takes a custom decoration once there is a label");
		field->setFocus();
		for (const auto character : QStringLiteral("two words")) {
			key(field, character == QLatin1Char{ ' ' } ? Qt::Key_Space : Qt::Key_A, Qt::NoModifier, QString{ character });
		}
		expect(field->text() == QStringLiteral("twowords"), "a decoration is one word: a space cannot be typed");
		field->clear();
		for (const auto character : QStringLiteral("Security")) {
			key(field, Qt::Key_A, Qt::NoModifier, QString{ character });
		}
		key(field, Qt::Key_Return);
		settle();
		expect(f.text() == "issue (if-minor, blocking, Security): Fix it." && field->text().isEmpty() && f.menu()->isVisible(), "Return writes the custom decoration as typed, after the others");
		const auto custom = f.menu()->custom_buttons();
		expect(custom.size() == 1U && custom.front()->text() == QStringLiteral("Security") && custom.front()->isChecked() && f.controls()->state().Custom == QStringList{ QStringLiteral("Security") },
			   "a written custom decoration shows as a checked toggle");
		f.menu()->if_minor_button()->click();
		settle();
		expect(f.text() == "issue (blocking, Security): Fix it.", "if-minor comes out and the custom one stays as written");
		f.menu()->custom_buttons().front()->click();
		settle();
		expect(f.text() == "issue (blocking): Fix it." && f.menu()->custom_buttons().empty(), "unchecking a custom decoration takes it out");
		f.menu()->label_button(core::feedback_label::question)->click();
		settle();
		expect(f.text() == "question (blocking): Fix it.", "a new label keeps the decorations");

		f.controls()->open_menu();
		settle();
		expect(overrides(f.menu()->label_button(core::feedback_label::issue), Qt::Key_0), "inside the open menu ⌘0 is the menu's own key");
		{
			// [[BPR-1336]]. ⌘⌥0 is the window's Actual Size, never Plain.
			auto actualSize = QKeyEvent{ QEvent::ShortcutOverride, Qt::Key_0, Qt::ControlModifier | Qt::AltModifier };
			actualSize.ignore();
			QApplication::sendEvent(f.menu()->label_button(core::feedback_label::issue), &actualSize);
			expect(!actualSize.isAccepted(), "inside the open menu ⌘⌥0 is not the menu's key");
			key(f.menu(), Qt::Key_0, Qt::ControlModifier | Qt::AltModifier);
			settle();
			expect(f.text() == "question (blocking): Fix it." && f.menu()->isVisible(), "⌘⌥0 in the menu leaves the label and the menu as they were");
		}
		key(f.menu(), Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(f.text() == "Fix it." && !f.menu()->isVisible(), "⌘0 in the menu makes the comment plain and closes it");
		expect(!overrides(f.body(), Qt::Key_0), "outside the menu ⌘0 stays the window's Home shortcut");
		expect(f.Commands.size() == 11U, "each change was one draft edit");

		// A header typed by hand with several and custom decorations shows on
		// the toggles and in the menu exactly as written.
		auto typed = fixture{ "nitpick (Security, if-minor, perf_2): spacing" };
		expect(typed.controls()->state().Custom == QStringList{ QStringLiteral("Security"), QStringLiteral("perf_2") } && typed.controls()->if_minor_toggle()->isChecked()
				   && !typed.controls()->blocking_toggle()->isChecked() && typed.controls()->state().Emphasis == qt::feedback_mark_emphasis{ .IfMinor = true },
			   "typed custom decorations are read as written, in order");
		typed.controls()->open_menu();
		settle();
		save_render(*typed.menu(), "menu-custom");
		expect(typed.menu()->custom_buttons().size() == 2U, "each custom decoration has its toggle");
		typed.menu()->custom_buttons().front()->click();
		settle();
		expect(typed.text() == "nitpick (if-minor, perf_2): spacing", "taking one out keeps the rest exactly as written");
		typed.menu()->close();
	}

	/// \brief [[BPR-1083]] v004. The controls' tooltips sit right next to
	/// what they name (owner, preview-tooltips-v003: "The tooltip feels
	/// strangely far away.").
	void test_control_tooltips_sit_next_to_their_marks()
	{
		const auto expect_anchored = [](QWidget& aOwner, const QRect& aAnchor, const QString& aText, const char* const aWhat) {
			qt::hide_anchored_tooltip();
			QHelpEvent help{ QEvent::ToolTip, aAnchor.center(), aOwner.mapToGlobal(aAnchor.center()) };
			QApplication::sendEvent(&aOwner, &help);
			const auto* const tip   = qobject_cast<const QLabel*>(qt::anchored_tooltip());
			const auto        frame = QRect{ aOwner.mapToGlobal(aAnchor.topLeft()), aAnchor.size() };
			const auto        ok    = tip != nullptr && tip->text() == aText
				&& (tip->geometry().left() == frame.left() || (tip->geometry().left() < frame.left() && tip->geometry().right() >= frame.right() && tip->geometry() == qt::anchored_tooltip_geometry(frame, tip->size(), aOwner.screen()->availableGeometry())))
				&& (tip->geometry().top() == frame.bottom() + 1 + qt::anchored_tooltip_gap || tip->geometry().bottom() + 1 + qt::anchored_tooltip_gap == frame.top());
			if (!ok) {
				std::cerr << aWhat << ": " << (tip != nullptr ? tip->text().toStdString() : std::string{ "no tooltip" }) << '\n';
			}
			expect(ok, "the tooltip sits right below or above what it names, left edges aligned");
			qt::hide_anchored_tooltip();
		};
		auto f = fixture{ "issue: Fix it." };
		expect_anchored(*f.controls()->chip(), f.controls()->chip()->rect(), f.controls()->chip()->toolTip(), "chip");
		f.controls()->open_menu();
		settle();
		auto* const praise = f.menu()->label_button(core::feedback_label::praise);
		expect_anchored(*praise, praise->rect(), qt::feedback_label_description(core::feedback_label::praise), "menu entry");
		f.menu()->close();
		settle();
		f.controls()->set_helper(true);
		f.body()->clear();
		f.body()->setFocus();
		type(f.body(), QStringLiteral(":"));
		settle();
		auto* const list = f.controls()->completion();
		expect(list->isVisible() && list->count() == 12, "a colon opens completion");
		const auto item = list->visualItemRect(list->item(1));
		expect_anchored(*list->viewport(), item, qt::feedback_label_description(core::feedback_label::nitpick), "completion row");
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
		save_render(chosen.Window, "editor");
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
		auto* const completion = f.controls()->completion();
		// The owner, shortcodes-v002: "just typing : should be enough to open
		// suggestions." A colon alone at the start offers all twelve.
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":"));
		settle();
		auto everyLabel = std::vector<core::feedback_label>{ };
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			everyLabel.push_back(static_cast<core::feedback_label>(index));
		}
		expect(completion->isVisible() && f.controls()->completion_labels() == everyLabel && completion->currentRow() == 0,
			   "a colon alone at the start opens completion with every label, in catalog order");
		save_render(*completion, "completion-colon");
		key(f.body(), Qt::Key_Down);
		key(f.body(), Qt::Key_Down);
		key(f.body(), Qt::Key_Down);
		key(f.body(), Qt::Key_Tab);
		settle();
		expect(f.text() == "issue: Fix it." && !completion->isVisible(), "arrows and Tab take a label from the colon alone in one edit");
		f.body()->undo();
		settle();
		expect(f.text() == ":Fix it.", "and one undo restores the colon");
		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":"));
		settle();
		key(f.body(), Qt::Key_Escape);
		settle();
		expect(!completion->isVisible() && f.text() == ":Fix it.", "Esc after a colon alone leaves the text alone");
		f.body()->setPlainText(QStringLiteral("Fix it "));
		caret_at(f.body(), 7);
		type(f.body(), QStringLiteral(":"));
		settle();
		expect(!completion->isVisible(), "a colon anywhere but the start opens nothing");

		f.body()->setPlainText(QStringLiteral("Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":q"));
		settle();
		save_render(*completion, "completion");
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
		expect(f.controls()->statistics().Shortcodes == 4U, "four codes were replaced");

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

	/// \brief Found by BPR-1083's composed label-authoring course: the
	/// replacement of `:qu:` follows its closing colon at once, while the
	/// colon's own edit can still be with the draft owner. It used to go on
	/// the revision that edit was based on and was refused as "Draft changed in
	/// another window". It now waits for the answer and goes on the revision
	/// accepted; with no answer it goes as before.
	void test_a_shortcode_waits_for_the_unanswered_colon()
	{
		auto f = fixture{ "Fix it." };
		f.Editor->set_label_helper(true);
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":qu"));
		settle();
		f.Answering = false;
		type(f.body(), QStringLiteral(":"));
		settle();
		expect(f.text() == "question: Fix it." && f.Unanswered.size() == 1U && f.Unanswered.front().Command.Body == ":qu:Fix it.",
			   "the closing colon's edit is with the owner and the replacement is on screen");
		const auto sent    = f.Commands.size();
		const auto pending = f.Unanswered.front();
		f.Unanswered.clear();
		f.Answering = true;
		f.answer(pending);
		settle();
		expect(f.Commands.size() == sent + 1U && f.Commands.back().Command.Body == "question: Fix it."
				   && f.Commands.back().ExpectedRevision == pending.ExpectedRevision + 1U && f.Latest.Draft.Body == "question: Fix it."
				   && f.text() == "question: Fix it." && f.Editor->mutation_status_label()->text().isEmpty(),
			   "the replacement then goes on the accepted revision, with no conflict and nothing undone");

		auto silent = fixture{ "Fix it." };
		silent.Editor->set_label_helper(true);
		caret_at(silent.body(), 0);
		type(silent.body(), QStringLiteral(":qu"));
		settle();
		silent.Answering = false;
		type(silent.body(), QStringLiteral(":"));
		settle();
		const auto stale = silent.Unanswered.front().ExpectedRevision;
		auto       clock = QElapsedTimer{ };
		clock.start();
		while (silent.Unanswered.size() < 2U && clock.elapsed() < 2'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
		}
		expect(silent.Unanswered.size() == 2U && silent.Unanswered.back().Command.Body == "question: Fix it."
				   && silent.Unanswered.back().ExpectedRevision == stale,
			   "with no answer, the waiting change still goes, as it did before");
	}

	/// \brief The first and last columns where two renders of one widget differ.
	[[nodiscard]] auto differing_columns(const QImage& aShown, const QImage& aWithout) -> std::pair<int, int>
	{
		auto first = -1;
		auto last  = -1;
		for (auto x = 0; x < std::min(aShown.width(), aWithout.width()); ++x) {
			for (auto y = 0; y < std::min(aShown.height(), aWithout.height()); ++y) {
				const auto a = aShown.pixelColor(x, y);
				const auto b = aWithout.pixelColor(x, y);
				if (std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue()) + std::abs(a.alpha() - b.alpha()) > 24) {
					first = first < 0 ? x : first;
					last  = x;
					break;
				}
			}
		}
		return { first, last };
	}

	/// \brief [[BPR-1346]]. The logical gap between the mark's last ink and
	/// the word's first ink on one tool button. The mark's ink is where the
	/// button drawn with a blank icon differs; the word's is where the button
	/// drawn with transparent text differs, so the style's frame and fill
	/// cancel out on every platform.
	[[nodiscard]] auto mark_to_word_gap(QToolButton& aButton) -> qreal
	{
		const auto shown = aButton.grab().toImage();
		const auto icon  = aButton.icon();
		auto       blank = icon.pixmap(aButton.iconSize(), aButton.devicePixelRatioF());
		blank.fill(Qt::transparent);
		aButton.setIcon(QIcon{ blank });
		const auto noMark = aButton.grab().toImage();
		aButton.setIcon(icon);
		const auto palette = aButton.palette();
		auto       hidden  = palette;
		for (const auto role : { QPalette::ButtonText, QPalette::WindowText, QPalette::Text, QPalette::BrightText, QPalette::HighlightedText }) {
			hidden.setColor(role, Qt::transparent);
		}
		aButton.setPalette(hidden);
		const auto noWord = aButton.grab().toImage();
		aButton.setPalette(palette);
		const auto [markFirst, markLast] = differing_columns(shown, noMark);
		const auto [wordFirst, wordLast] = differing_columns(shown, noWord);
		expect(markFirst >= 0 && wordFirst >= 0 && wordLast > wordFirst, "the button draws both a mark and a word");
		return static_cast<qreal>(wordFirst - markLast - 1) / shown.devicePixelRatio();
	}

	/// \brief [[BPR-1346]]. A small gap between each label's mark and its word
	/// (owner, BPR-1091 v002: "Can we add like a tiny space between the icon
	/// and the word?"): on Plain, on every label in the picker and on the
	/// review bar's chip.
	void test_a_small_gap_between_the_mark_and_the_word()
	{
		constexpr auto least = qreal{ 4.0 };
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
			const auto* const name    = mode == core::appearance_mode::dark ? "dark" : "light";
			auto              f       = fixture{ "question: Is it reported?" };
			auto              profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(f.Window);
			profile.apply_to(*f.menu());
			f.controls()->set_theme(qt::default_feedback_mark_theme(f.Window.palette()));
			settle();
			save_render(f.Window, (std::string{ "chip-gap-" } + name).c_str());
			const auto chip = mark_to_word_gap(*f.controls()->chip());
			f.controls()->open_menu();
			settle();
			save_render(*f.menu(), (std::string{ "menu-gap-" } + name).c_str());
			const auto plain = mark_to_word_gap(*f.menu()->plain_button());
			auto       gaps  = std::vector<qreal>{ };
			for (auto index = std::size_t{ 0U }; index < core::feedback_label_count; ++index) {
				gaps.push_back(mark_to_word_gap(*f.menu()->label_button(static_cast<core::feedback_label>(static_cast<std::uint8_t>(index)))));
			}
			f.menu()->close();
			settle();
			std::cout << name << " mark-to-word gap: chip " << chip << ", Plain " << plain << ", praise " << gaps.front() << '\n';
			expect(chip >= least, "the chip leaves a small gap between its mark and its word");
			expect(plain >= least, "Plain leaves a small gap between its mark and its word");
			// Each word's first letter has its own side bearing, so the gaps
			// agree within a logical pixel rather than exactly.
			const auto [narrowest, widest] = std::ranges::minmax(gaps);
			expect(narrowest >= least && widest - narrowest <= 1.0, "every label button leaves the same small gap between its mark and its word");
			expect(chip <= least + 4.0 && plain <= least + 4.0, "the gap stays small");
		}
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
	test_control_tooltips_sit_next_to_their_marks();
	test_command_l_belongs_to_the_focused_editor();
	test_every_label_and_plain_are_reachable_by_keyboard();
	test_controls_and_typed_syntax_are_the_same_draft();
	test_a_label_comes_first_on_an_empty_comment();
	test_refusals_change_nothing();
	test_shortcodes_complete_and_replace_at_the_start();
	test_a_shortcode_waits_for_the_unanswered_colon();
	test_the_chip_keeps_its_size_when_narrow();
	test_a_small_gap_between_the_mark_and_the_word();
	std::cout << "feedback-label-controls-tests:pass\n";
	return EXIT_SUCCESS;
}
