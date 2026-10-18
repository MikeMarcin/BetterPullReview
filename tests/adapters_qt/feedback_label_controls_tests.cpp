/// \file
/// \brief [[BPR-1083]]. Proves the selected authoring controls in the real
/// comment editor: the label chip and its menu, the decoration toggles, ⌘L
/// and, since [[BPR-1569]], ⌘0 as Actual Size even in the open menu (RFC-BPR-0048
/// R09, R10; `docs/ux/conventional-comments-feedback.md`).
/// [[BPR-1565]] removed the leading `:xx:` codes and their completion (owner,
/// 2026-10-16): a typed code stays the text it is.
/// \details Every check drives `review_draft_editor` as composed, with the
/// revisioned dispatch the workbench gives it, and reads the commands it
/// emits. A label chosen with a control must leave the editor exactly as the
/// same text typed by hand does: one ordinary whole-body edit.

#include "../support/window_system_keys.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/reference_page.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/review_draft_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
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
#include <QPoint>
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
		expect(chip->toolTip() == QStringLiteral("Label (%1)").arg(QKeySequence{ Qt::CTRL | Qt::Key_L }.toString(QKeySequence::NativeText)),
			   "its tooltip names ⌘L and no typed code");
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
					   && button->text() == qt::feedback_label_text(label) && button->accessibleDescription() == qt::feedback_label_description(label) && !button->icon().isNull(),
				   "each grid entry shows its mark and name, no code, and its catalog description as a tooltip");
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
		using mark_weight = qt::feedback_mark_weight;
		auto f            = fixture{ "Fix it." };
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
		expect(f.text() == "issue (blocking): Fix it." && blocking->isChecked() && f.controls()->state().Emphasis == qt::feedback_mark_emphasis{ .Weight = mark_weight::blocking },
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
				   && f.controls()->state().Emphasis == qt::feedback_mark_emphasis{ .Weight = mark_weight::non_blocking, .IfMinor = true },
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
		expect(overrides(f.menu()->label_button(core::feedback_label::issue), Qt::Key_0), "inside the open menu ⌘0 is claimed, to pass it to the window");
		{
			// [[BPR-1336]]. ⌘⌥0 is never Plain ([[BPR-1535]]: the window binds
			// it to nothing now that Actual Size is ⌘0).
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
		expect(f.text() == "question (blocking): Fix it." && !f.menu()->isVisible(), "[[BPR-1569]] ⌘0 in the menu closes it and leaves the label");
		expect(!overrides(f.body(), Qt::Key_0), "outside the menu ⌘0 stays the window's Actual Size shortcut");
		f.controls()->open_menu();
		settle();
		f.menu()->plain_button()->click();
		settle();
		expect(f.text() == "Fix it." && !f.menu()->isVisible(), "Plain comment, clicked, makes the comment plain and closes the menu");
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
		const auto expectAnchored = [](QWidget& aOwner, const QRect& aAnchor, const QString& aText, const char* const aWhat) {
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
		expectAnchored(*f.controls()->chip(), f.controls()->chip()->rect(), f.controls()->chip()->toolTip(), "chip");
		f.controls()->open_menu();
		settle();
		auto* const praise = f.menu()->label_button(core::feedback_label::praise);
		expectAnchored(*praise, praise->rect(), qt::feedback_label_description(core::feedback_label::praise), "menu entry");
		f.menu()->close();
		settle();
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

	/// \brief [[BPR-1566]]. About Conventional Comments is the menu's last
	/// item: shown only where a window bound it, named for assistive
	/// technology, reached by arrow keys from a plain or a labelled comment
	/// and by Tab, and on activation it names the page once, closes the menu
	/// and changes nothing in the comment.
	void test_about_conventional_comments_is_a_way_out_not_a_label()
	{
		using page = core::reference_page;
		// Unbound: no item, and the arrow keys stop where they always did.
		{
			auto f = fixture{ "Subject." };
			key(f.body(), Qt::Key_L, Qt::ControlModifier);
			settle();
			auto* const about = f.menu()->about_button();
			expect(about != nullptr && about->isHidden() && !f.controls()->reference_offered(), "an unbound menu offers no About item");
			f.menu()->label_button(core::feedback_label::typo)->setFocus(Qt::TabFocusReason);
			key(f.menu(), Qt::Key_Down);
			expect(f.menu()->focusWidget() == f.menu()->label_button(core::feedback_label::typo), "unbound, Down from the last grid row of a plain comment stays there, as before");
			key(f.menu(), Qt::Key_Escape);
			settle();
		}
		auto f     = fixture{ "Subject." };
		auto asked = std::vector<page>{ };
		f.controls()->set_reference_dispatch([&asked](const page aPage) { asked.push_back(aPage); });
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		auto* const menu  = f.menu();
		auto* const about = menu->about_button();
		auto* const rule  = menu->findChild<QFrame*>(QStringLiteral("feedbackLabelMenuAboutRule"));
		expect(menu->isVisible() && about->isVisible() && rule != nullptr && rule->isVisible(), "a bound menu ends with a rule and About Conventional Comments");
		expect(about->text() == QStringLiteral("About Conventional Comments") && about->accessibleName() == QStringLiteral("About Conventional Comments")
				   && about->accessibleDescription() == QStringLiteral("Where these labels come from and how to use them. Opens conventionalcomments.org in your browser.")
				   && about->toolTip().contains(QStringLiteral("conventionalcomments.org")) && !about->isCheckable() && about->icon().isNull(),
			   "it says where it goes, for the eye and for assistive technology, and is not a choice");
		const auto bottom = [menu](const QWidget* aWidget) { return aWidget->mapTo(menu, QPoint{ 0, 0 }).y(); };
		for (QWidget* const above : std::array<QWidget*, 4>{ menu->plain_button(), menu->label_button(core::feedback_label::typo), menu->custom_field(), rule }) {
			expect(bottom(above) < bottom(about), "it is the last item, under the rule");
		}
		expect(about->palette().color(QPalette::ButtonText) == qt::readable_link_colour(menu->palette()), "it is drawn in the link colour of the palette on screen");
		expect((about->focusPolicy() & Qt::TabFocus) != 0, "Tab reaches it");
		save_render(*menu, "menu-about-light");

		// Arrow keys on a plain comment: from the grid's last row, past the
		// disabled decorations and field, to About; and back up to the grid.
		menu->label_button(core::feedback_label::typo)->setFocus(Qt::TabFocusReason);
		key(menu, Qt::Key_Down);
		expect(menu->focusWidget() == about, "on a plain comment Down from the grid's last row reaches About");
		key(menu, Qt::Key_Down);
		expect(menu->focusWidget() == about, "Down stops on the last item");
		key(menu, Qt::Key_Up);
		expect(menu->focusWidget() == menu->label_button(core::feedback_label::typo), "Up from About returns to the grid's last row");
		key(menu, Qt::Key_Down);
		const auto commands = f.Commands.size();
		const auto edits    = f.controls()->statistics().Edits;
		key(menu, Qt::Key_Return);
		settle();
		expect(asked == std::vector<page>{ page::conventional_comments }, "Return on About names the Conventional Comments page, once");
		expect(!menu->isVisible() && f.text() == "Subject." && f.Commands.size() == commands && f.controls()->statistics().Edits == edits && !f.controls()->state().Label.has_value(),
			   "the menu closes and the comment, its label and its draft are untouched");
		expect(f.Window.focusWidget() == f.body() && f.controls()->statistics().References == 1U, "focus is back in the comment");

		// A labelled comment: Down from the field reaches About, Up returns.
		f.controls()->apply({ .Change = core::feedback_label_change::set, .Label = core::feedback_label::issue });
		settle();
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		menu->custom_field()->setFocus(Qt::TabFocusReason);
		key(menu, Qt::Key_Down);
		expect(menu->focusWidget() == about, "on a labelled comment Down from the custom field reaches About");
		key(menu, Qt::Key_Up);
		expect(menu->focusWidget() == menu->custom_field(), "and Up goes back to the field");
		// [[BPR-1569]]. ⌘0 with About focused closes the menu, asks for no
		// page and leaves the label; Plain is chosen by its item.
		about->setFocus(Qt::TabFocusReason);
		key(menu, Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(f.text() == "issue: Subject." && !menu->isVisible() && asked.size() == 1U, "⌘0 with About focused closes the menu, keeps the label and asks for no page");
		f.controls()->apply({ .Change = core::feedback_label_change::remove });
		settle();

		// A click is the same request and still no choice.
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		about->click();
		settle();
		expect(asked == std::vector<page>{ page::conventional_comments, page::conventional_comments } && !menu->isVisible() && f.text() == "Subject.",
			   "a click names the page again, closes the menu and changes nothing");

		// Unbinding takes the item away again.
		f.controls()->set_reference_dispatch({ });
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		expect(about->isHidden() && rule->isHidden(), "unbound again, the item and its rule are gone");
		key(menu, Qt::Key_Escape);
		settle();

		// Review renders, in the built-in Light and Dark palettes, of a
		// labelled comment's open menu. Written only when asked for.
		if (!qEnvironmentVariable("BPR_FEEDBACK_RENDER_DIR").isEmpty()) {
			f.controls()->set_reference_dispatch([](page) { });
			const auto original = QApplication::palette();
			for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "menu-about-light" }, std::pair{ core::appearance_mode::dark, "menu-about-dark" } }) {
				auto profile = qt::appearance_profile{ };
				static_cast<void>(profile.select_mode(mode));
				QApplication::setPalette(profile.derive(original));
				f.controls()->set_theme(qt::default_feedback_mark_theme(QApplication::palette()));
				static_cast<void>(f.controls()->apply({ .Change = core::feedback_label_change::set, .Label = core::feedback_label::suggestion }));
				settle();
				key(f.body(), Qt::Key_L, Qt::ControlModifier);
				settle();
				about->setFocus(Qt::TabFocusReason);
				settle();
				save_render(*menu, name);
				key(menu, Qt::Key_Escape);
				settle();
			}
			QApplication::setPalette(original);
			settle();
		}
	}

	/// \brief [[BPR-1569]]. The owner, 2026-10-16: Plain needs no key of its
	/// own, and ⌘0 is Actual Size with the label menu open as everywhere else.
	/// Keys arrive as the window system delivers them, through the shortcut map.
	void test_command_0_is_actual_size_in_the_open_menu()
	{
		auto  f          = fixture{ "issue: Subject." };
		auto* actualSize = new QAction{ QStringLiteral("Actual Size"), &f.Window };
		actualSize->setShortcut(QKeySequence{ QStringLiteral("Ctrl+0") });
		actualSize->setShortcutContext(Qt::WindowShortcut);
		f.Window.addAction(actualSize);
		auto sized = 0;
		QObject::connect(actualSize, &QAction::triggered, &f.Window, [&sized] { ++sized; });
		const auto commands = f.Commands.size();

		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		auto* const menu = f.menu();
		expect(menu->isVisible() && menu->plain_button()->text() == QStringLiteral("Plain comment") && !menu->plain_button()->toolTip().contains(QKeySequence{ Qt::CTRL | Qt::Key_0 }.toString(QKeySequence::NativeText))
				   && menu->plain_button()->accessibleDescription() == QStringLiteral("Removes the label"),
			   "Plain comment names no key");
		tests::window_system_key(*menu, Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(sized == 1 && !menu->isVisible(), "⌘0 with the menu open is the window's Actual Size, once, and the menu closes");
		expect(f.text() == "issue: Subject." && f.controls()->state().Label == core::feedback_label::issue && f.Commands.size() == commands,
			   "negative control for the removed key: ⌘0 never picks Plain");

		// On a plain comment, and with the window's action disabled: nothing
		// else happens, and Plain is not chosen either way.
		f.controls()->apply({ .Change = core::feedback_label_change::remove });
		settle();
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		tests::window_system_key(*menu, Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(sized == 2 && !menu->isVisible() && f.text() == "Subject.", "on a plain comment ⌘0 is Actual Size too");
		actualSize->setEnabled(false);
		f.controls()->apply({ .Change = core::feedback_label_change::set, .Label = core::feedback_label::note });
		settle();
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		tests::window_system_key(*menu, Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(sized == 2 && !menu->isVisible() && f.text() == "note: Subject.", "with Actual Size unavailable ⌘0 does nothing but close the menu");
		actualSize->setEnabled(true);

		// Outside the menu ⌘0 stays the window's, and Plain is still a
		// normal item: arrow keys and Return reach it.
		// A window shortcut answers only in the active window, as a hand
		// pressing the key in it has.
		f.Window.activateWindow();
		for (auto round = 0; round < 50 && !f.Window.isActiveWindow(); ++round) {
			settle();
		}
		expect(f.Window.isActiveWindow(), "the editor's window is active for its keys");
		f.body()->setFocus(Qt::OtherFocusReason);
		tests::window_system_key(*f.body(), Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(sized == 3 && f.text() == "note: Subject.", "outside the menu ⌘0 is Actual Size");
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		for (auto step = 0; step < 6; ++step) {
			key(menu, Qt::Key_Up);
		}
		key(menu, Qt::Key_Return);
		settle();
		expect(f.text() == "Subject." && sized == 3, "Plain is reached by the arrow keys and Return");

		// With the window active, so its shortcut could also answer, ⌘0 in
		// the open menu is still Actual Size exactly once.
		f.controls()->apply({ .Change = core::feedback_label_change::set, .Label = core::feedback_label::praise });
		f.Window.activateWindow();
		for (auto round = 0; round < 50 && !f.Window.isActiveWindow(); ++round) {
			settle();
		}
		key(f.body(), Qt::Key_L, Qt::ControlModifier);
		settle();
		tests::window_system_key(*menu, Qt::Key_0, Qt::ControlModifier);
		settle();
		expect(sized == 4 && !menu->isVisible() && f.controls()->state().Label == core::feedback_label::praise,
			   "with the window active ⌘0 in the open menu is Actual Size once, and keeps the label");
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

	/// \brief Whether any label completion list is on screen. The controls
	/// had one until [[BPR-1565]]; none may appear now.
	[[nodiscard]] auto any_list_shown() -> bool
	{
		const auto widgets = QApplication::topLevelWidgets();
		return std::ranges::any_of(widgets, [](const QWidget* const aWidget) {
			return aWidget->isVisible() && (qobject_cast<const QListWidget*>(aWidget) != nullptr || aWidget->findChild<QListWidget*>() != nullptr);
		});
	}

	/// \brief [[BPR-1565]]. The owner, 2026-10-16: "I think we can get rid of
	/// our custom shortcodes for the conventional comment tags... they don't
	/// add much over just typing it out yourself and the dropdown picker is
	/// convenient enough." Negative control for the removed behaviour: every
	/// former code, and a colon alone, stays the text typed, at the start or
	/// anywhere, opens no list and makes no control edit.
	void test_typed_codes_stay_text()
	{
		auto f = fixture{ "Fix it." };
		f.body()->setFocus();
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":"));
		settle();
		expect(f.text() == ":Fix it." && !any_list_shown(), "a colon alone at the start opens nothing");
		key(f.body(), Qt::Key_Tab);
		settle();
		expect(f.text().starts_with(':') && !f.controls()->state().Label.has_value(), "and Tab takes no label");

		for (const auto* const code : { ":qu:", ":is:", ":su:", ":ni:", ":pr:", ":zz:", ":+1:" }) {
			f.body()->setPlainText(QStringLiteral("Fix it."));
			settle();
			const auto before = f.Commands.size();
			caret_at(f.body(), 0);
			type(f.body(), QString::fromLatin1(code));
			settle();
			expect(f.text() == std::string{ code } + "Fix it." && !f.controls()->state().Label.has_value() && !any_list_shown(),
				   std::string{ code } + " typed at the start stays text, with no label and no list");
			expect(f.Commands.size() == before + 4U && f.Commands.back().Command.Body == std::string{ code } + "Fix it.",
				   "each keystroke is one ordinary edit, and the draft holds the code as typed");
		}
		f.body()->setPlainText(QStringLiteral("question (blocking): Fix it."));
		caret_at(f.body(), 0);
		type(f.body(), QStringLiteral(":is:"));
		settle();
		expect(f.text() == ":is:question (blocking): Fix it.", "a code in front of a label no longer replaces it");
		f.body()->setPlainText(QStringLiteral("Fix it "));
		caret_at(f.body(), 7);
		type(f.body(), QStringLiteral(":is: :+1:"));
		settle();
		expect(f.text() == "Fix it :is: :+1:", "codes and GitHub's own :emoji: elsewhere are text too");
		expect(f.controls()->statistics().Edits == 0U, "no control edit was made");

		f.body()->setPlainText(QStringLiteral("question: typed in full"));
		settle();
		expect(f.controls()->state().Label == core::feedback_label::question, "a label typed in full is still recognized");
	}

	/// \brief Found by BPR-1083's composed label-authoring course, with the
	/// `:xx:` codes [[BPR-1565]] removed: a control edit can follow a typed
	/// edit that is still with the draft owner. It used to go on the revision
	/// that edit was based on and was refused as "Draft changed in another
	/// window". It now waits for the answer and goes on the revision accepted;
	/// with no answer it goes as before.
	void test_a_label_chosen_while_typing_waits_for_the_unanswered_edit()
	{
		auto f = fixture{ "Fix it" };
		caret_at(f.body(), 6);
		f.Answering = false;
		type(f.body(), QStringLiteral("!"));
		settle();
		f.controls()->open_menu();
		settle();
		f.menu()->label_button(core::feedback_label::question)->click();
		settle();
		expect(f.text() == "question: Fix it!" && f.Unanswered.size() == 1U && f.Unanswered.front().Command.Body == "Fix it!",
			   "the typed edit is with the owner and the label is on screen");
		const auto sent    = f.Commands.size();
		const auto pending = f.Unanswered.front();
		f.Unanswered.clear();
		f.Answering = true;
		f.answer(pending);
		settle();
		expect(f.Commands.size() == sent + 1U && f.Commands.back().Command.Body == "question: Fix it!"
				   && f.Commands.back().ExpectedRevision == pending.ExpectedRevision + 1U && f.Latest.Draft.Body == "question: Fix it!"
				   && f.text() == "question: Fix it!" && f.Editor->mutation_status_label()->text().isEmpty(),
			   "the label then goes on the accepted revision, with no conflict and nothing undone");

		auto silent = fixture{ "Fix it" };
		caret_at(silent.body(), 6);
		silent.Answering = false;
		type(silent.body(), QStringLiteral("!"));
		settle();
		silent.controls()->open_menu();
		settle();
		silent.menu()->label_button(core::feedback_label::question)->click();
		settle();
		const auto stale = silent.Unanswered.front().ExpectedRevision;
		auto       clock = QElapsedTimer{ };
		clock.start();
		while (silent.Unanswered.size() < 2U && clock.elapsed() < 2'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
		}
		expect(silent.Unanswered.size() == 2U && silent.Unanswered.back().Command.Body == "question: Fix it!"
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
	test_typed_codes_stay_text();
	test_a_label_chosen_while_typing_waits_for_the_unanswered_edit();
	test_the_chip_keeps_its_size_when_narrow();
	test_a_small_gap_between_the_mark_and_the_word();
	test_about_conventional_comments_is_a_way_out_not_a_label();
	test_command_0_is_actual_size_in_the_open_menu();
	std::cout << "feedback-label-controls-tests:pass\n";
	return EXIT_SUCCESS;
}
