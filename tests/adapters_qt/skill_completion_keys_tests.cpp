/// \file
/// \brief Owner request, 2026-11-12: "I want to be able to Tab-complete
/// skills when that little picker popup comes up."
///
/// \details The Agent tab's question field in a real workbench window, with
/// keys delivered where the platform plugin hands them to Qt
/// (`QWindowSystemInterface`), so the shortcut map, the window and every
/// filter on the field see them as they do in the application:
/// - Tab with the list open inserts the highlighted skill, as Return and a
///   click do, closes the list and leaves the keyboard in the field;
/// - Tab with a row other than the first highlighted inserts that row;
/// - Shift+Tab with the list open is not taken by the list: the field's own
///   Shift+Tab runs and inserts nothing (a field whose Tab moves focus moves
///   the keyboard back, and the list closes with it);
/// - Tab with the list closed does what it did before: the field types a tab.

#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QTextCursor>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <cstdlib>
#include <iostream>
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
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	/// One key pressed and released, delivered as the platform plugin does.
	void platform_key(QWidget& aWindow, const int aKey, const Qt::KeyboardModifiers aModifiers, const QString& aText)
	{
		auto* const target = aWindow.windowHandle();
		expect(target != nullptr, "a key has a window to go to");
		QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(target, QEvent::KeyPress, aKey, aModifiers, aText);
		QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(target, QEvent::KeyRelease, aKey, aModifiers, aText);
		settle();
	}

	[[nodiscard]] auto skills() -> std::vector<core::skill_entry>
	{
		return { { .Name = "fixture-technical-writer", .Description = "Short paragraphs.", .Source = core::skill_source::personal, .Origin = "~/.claude/skills" },
				 { .Name = "fixture-tester", .Description = "Writes tests.", .Source = core::skill_source::personal, .Origin = "~/.claude/skills" } };
	}

	/// \brief A window in Review with the Agent tab open and skills offered.
	void open_agent_tab(qt::workbench_window& aWindow, qt::appearance_profile& aProfile)
	{
		aWindow.resize(1'600, 900);
		aProfile.apply_to(aWindow);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 600U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(2);
		settle();
		auto profile        = core::agent_profile{ };
		profile.Id          = core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U);
		profile.Protocol    = core::agent_protocol::claude_stream_json;
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		// A chat to ask in, so the field takes typing as it does in use.
		const auto binding = core::agent_revision_binding{ .Host = core::git_host_id{ 1 }, .Repository = core::repository_id{ 2 }, .PullRequest = core::pull_request_id{ 3 }, .BaseOid = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"), .HeadOid = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"), .LeftEpoch = core::epoch_ordinal{ 1 }, .RightEpoch = core::epoch_ordinal{ 2 } };
		const auto session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = profile.Id, .Binding = binding };
		aWindow.side_chat()->set_dispatch([](const qt::side_chat_intent&) { });
		aWindow.side_chat()->present(qt::side_chat_projection{
			.Session        = session,
			.Continuations  = core::offer_agent_continuations(session, binding),
			.Chat           = { .Scope = { .Kind = core::agent_chat_scope_kind::pull_request } },
			.PrivateContext = { .Session = session.Id },
		});
		settle();
		aWindow.agent_tab()->refresh();
		settle();
		aWindow.agent_tab()->present_skills(skills());
		aWindow.activateWindow();
		settle();
	}

	[[nodiscard]] auto composer_of(qt::workbench_window& aWindow) -> QPlainTextEdit*
	{
		auto* const completion = aWindow.agent_tab()->skill_completion();
		expect(completion != nullptr, "the Agent tab's question field completes skills");
		auto* const composer = qobject_cast<QPlainTextEdit*>(completion->parent());
		expect(composer != nullptr && composer->isVisible() && !composer->isReadOnly(), "the question field shows and takes typing");
		return composer;
	}

	/// \brief The field holding \p aText with the caret at its end, focused.
	void type_into(QPlainTextEdit& aComposer, const QString& aText)
	{
		aComposer.setFocus(Qt::OtherFocusReason);
		aComposer.clear();
		settle();
		aComposer.insertPlainText(aText);
		auto cursor = aComposer.textCursor();
		cursor.movePosition(QTextCursor::End);
		aComposer.setTextCursor(cursor);
		settle();
	}

	void test_tab_inserts_the_highlighted_skill()
	{
		auto                 profile = qt::appearance_profile{ qt::default_appearance_settings() };
		qt::workbench_window window;
		open_agent_tab(window, profile);
		auto* const composer   = composer_of(window);
		auto* const completion = window.agent_tab()->skill_completion();
		type_into(*composer, QStringLiteral("Review this as /fixture-tech"));
		expect(composer->hasFocus(), "the question field has the keyboard");
		expect(completion->rows().size() == 1 && !completion->popup()->isHidden(), "typing a skill's prefix opens the list");
		expect(completion->popup()->accessibleDescription().contains(QStringLiteral("Tab")), "the list says Tab inserts");
		platform_key(window, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
		expect(composer->toPlainText() == QStringLiteral("Review this as /fixture-technical-writer "), "Tab inserts the highlighted skill and a space");
		expect(completion->popup()->isHidden() && completion->rows().isEmpty(), "Tab closes the list");
		expect(composer->hasFocus(), "the keyboard stays in the question field");
		expect(composer->textCursor().position() == composer->toPlainText().size(), "the caret is after the inserted name");

		// With two matches, Down then Tab inserts the second.
		type_into(*composer, QStringLiteral("Use /fixture"));
		expect(completion->rows().size() == 2, "a shared prefix lists both skills");
		platform_key(window, Qt::Key_Down, Qt::NoModifier, QString{ });
		platform_key(window, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
		expect(composer->toPlainText() == QStringLiteral("Use /fixture-tester "), "Tab inserts the row Down chose");
		expect(completion->popup()->isHidden() && composer->hasFocus(), "the list closes and the field keeps the keyboard");

		// Return and Tab insert the same thing.
		type_into(*composer, QStringLiteral("Use /fixture"));
		platform_key(window, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		const auto byReturn = composer->toPlainText();
		type_into(*composer, QStringLiteral("Use /fixture"));
		platform_key(window, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
		expect(composer->toPlainText() == byReturn && byReturn == QStringLiteral("Use /fixture-technical-writer "), "Tab inserts what Return inserts");
		window.hide();
	}

	void test_shift_tab_is_the_fields_own()
	{
		auto                 profile = qt::appearance_profile{ qt::default_appearance_settings() };
		qt::workbench_window window;
		open_agent_tab(window, profile);
		auto* const composer   = composer_of(window);
		auto* const completion = window.agent_tab()->skill_completion();
		// What Shift+Tab does in the field with no list open.
		type_into(*composer, QStringLiteral("No skill here"));
		expect(completion->popup()->isHidden(), "plain words open no list");
		platform_key(window, Qt::Key_Backtab, Qt::ShiftModifier, QString{ });
		const auto leavesWithoutList = !composer->hasFocus();
		const auto landedWithoutList = QApplication::focusWidget();
		expect(composer->toPlainText() == QStringLiteral("No skill here"), "Shift+Tab inserts nothing without the list");

		type_into(*composer, QStringLiteral("Use /fixture"));
		expect(!completion->popup()->isHidden(), "the list is open");
		platform_key(window, Qt::Key_Backtab, Qt::ShiftModifier, QString{ });
		expect(composer->toPlainText() == QStringLiteral("Use /fixture"), "Shift+Tab inserts nothing with the list open");
		expect(!composer->hasFocus() == leavesWithoutList && QApplication::focusWidget() == landedWithoutList, "Shift+Tab moves the keyboard as it does without the list");
		if (leavesWithoutList) {
			expect(completion->popup()->isHidden(), "the list closes when the keyboard leaves the field");
		}
		window.hide();
	}

	void test_tab_without_the_list_is_unchanged()
	{
		auto                 profile = qt::appearance_profile{ qt::default_appearance_settings() };
		qt::workbench_window window;
		open_agent_tab(window, profile);
		auto* const composer   = composer_of(window);
		auto* const completion = window.agent_tab()->skill_completion();
		type_into(*composer, QStringLiteral("No skill here"));
		expect(completion->popup()->isHidden(), "plain words open no list");
		const auto tabMovesFocus = composer->tabChangesFocus();
		platform_key(window, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
		if (tabMovesFocus) {
			expect(!composer->hasFocus() && composer->toPlainText() == QStringLiteral("No skill here"), "Tab moves the keyboard on, as before");
		} else {
			expect(composer->hasFocus() && composer->toPlainText() == QStringLiteral("No skill here\t"), "Tab types a tab in the field, as before");
		}
		window.hide();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_tab_inserts_the_highlighted_skill();
	test_shift_tab_is_the_fields_own();
	test_tab_without_the_list_is_unchanged();
	std::cout << "skill-completion-keys:pass\n";
	return EXIT_SUCCESS;
}
