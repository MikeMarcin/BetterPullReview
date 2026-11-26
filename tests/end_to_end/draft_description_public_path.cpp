/// \file
/// \brief [[BPR-2280]], [[BPR-2281]]. Draft Description through the window,
/// on the github.com fixture pull request the viewer opened, with the fixture
/// agent and a description the fixture's GitHub serves and writes. No real
/// agent, no network, no cost.
///
/// - The pull request's Ask menu has no Draft a description.
/// - Author's Workflow tab shows the description with Draft Description
///   beside it; a press says it is drafting beside the field, then puts the
///   agent's description (Summary, What changed, How to test) in the editor
///   as one undoable edit, sending nothing.
/// - Save writes the description once through the fixture's GitHub, and the
///   next read shows it.
/// - Changed on GitHub meanwhile, a second save is refused: nothing written,
///   the text kept, a sentence saying why, and Reload edits from GitHub's.
///
///     draft_description_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/author_description_editor.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextCursor>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// With `BPR_DRAFT_DESCRIPTION_RENDER_ROOT` set, a widget is saved there
	/// as a PNG (the owner's looks).
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_DRAFT_DESCRIPTION_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "draft-description: no private data root\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.FixtureAgentSession = true;
		options.EditsDescription    = true;
		options.RepeatReadSessions  = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::github_com, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		window.home_open_action()->trigger();
		auto* const editor = window.author_description();
		expect(editor != nullptr, "Author has a description editor");
		if (!e2e::wait_until_plain([&] { return !window.showing_home() && window.author_case().has_value() && editor->editable() && editor->body().has_value(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the viewer's pull request offers Author and its description is editable: home=" + std::to_string(window.showing_home() ? 1 : 0)
									  + " author=" + std::to_string(window.author_case().has_value() ? 1 : 0) + " editable=" + std::to_string(editor->editable() ? 1 : 0)
									  + " body=" + std::to_string(editor->body().has_value() ? 1 : 0) };
		}
		window.open_agent_question();
		wait([&] { return window.agent_tab()->setup_readiness() == qt::agent_setup_readiness::ready; }, "the fixture agent is set up");

		// Not in the Ask menu ([[BPR-2281]], owner 2026-11-19).
		{
			QMenu menu;
			window.fill_quick_ask(menu, window.ask_request(core::quick_ask_anchor::review_bar));
			expect(menu.findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.draft-description_pull-request")) == nullptr, "the Ask menu has no Draft a description");
		}

		// Author, Workflow: Draft Description beside the description.
		window.choose_modality(qt::review_modality::author);
		QApplication::processEvents();
		auto* const draft = editor->draft_button();
		wait([&] { return editor->isVisibleTo(&window) && draft->isVisibleTo(&window) && draft->isEnabled(); }, "Draft Description is offered beside the description");
		const auto mutation = access::review_mutation_evidence(*application);
		draft->click();
		QApplication::processEvents();
		expect(editor->drafting() && draft->text() == QStringLiteral("Stop drafting") && editor->draft_status()->text().contains(QStringLiteral("is drafting the description")),
			   "it says it is drafting, beside the field");
		render(window, QStringLiteral("draft-description-running"));
		if (!e2e::wait_until_plain([&] { return !editor->drafting() && editor->editing(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the draft lands in the editor; status: " + editor->draft_status()->text().toStdString() };
		}
		const auto drafted = QString::fromStdString(editor->editor()->raw());
		expect(drafted.startsWith(QStringLiteral("## Summary")) && drafted.contains(QStringLiteral("## What changed")) && drafted.contains(QStringLiteral("## How to test"))
				   && !drafted.contains(QStringLiteral("Fixture agent")),
			   "the editor holds a title-free description in three parts:\n" + drafted.toStdString());
		expect(editor->original() == *editor->body(), "editing goes on from the description GitHub has");
		editor->editor()->undo();
		expect(editor->editor()->raw() == *editor->body(), "Undo takes the draft back in one step");
		editor->editor()->redo();
		expect(QString::fromStdString(editor->editor()->raw()) == drafted, "Redo puts it back");
		expect(access::review_mutation_evidence(*application).DescriptionEdits == mutation.DescriptionEdits, "nothing is sent before Save");
		render(window, QStringLiteral("draft-description-editor"));

		// Save: one write, and the next read shows it.
		const auto saved = editor->editor()->raw();
		editor->save_button()->click();
		wait([&] { return access::review_mutation_evidence(*application).DescriptionWrites == mutation.DescriptionWrites + 1U && !editor->editing(); }, "Save writes the description once");
		if (!e2e::wait_until_plain([&] { return editor->body() == std::optional{ saved }; }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the next read shows the saved description; it shows:\n" + editor->body().value_or("<none>") + "\noverview: "
									  + (window.overview_page()->projection() ? window.overview_page()->projection()->Body : std::string{ "<none>" }) };
		}
		expect(editor->reader()->original_body() == saved, "the description reads as saved");

		// Changed on GitHub meanwhile: refused, nothing written, Reload offered.
		expect(editor->begin_edit(), "the description opens again");
		auto end = editor->editor()->textCursor();
		end.movePosition(QTextCursor::End);
		editor->editor()->setTextCursor(end);
		editor->editor()->insertPlainText(QStringLiteral("\n\nTyped second."));
		access::stage_server_description(*application, "Changed on GitHub by someone else.");
		const auto writes = access::review_mutation_evidence(*application).DescriptionWrites;
		editor->save_button()->click();
		wait([&] { return editor->reload_button()->isVisible(); }, "a stale description is refused");
		expect(access::review_mutation_evidence(*application).DescriptionWrites == writes, "nothing was written over GitHub's description");
		expect(editor->editing() && editor->editor()->raw().ends_with("Typed second.") && editor->status()->text().contains(QStringLiteral("changed the description on GitHub"))
				   && editor->status()->text().contains(QStringLiteral("Nothing was saved")),
			   "the text is kept and a sentence says why: " + editor->status()->text().toStdString());
		render(window, QStringLiteral("draft-description-conflict"));
		editor->reload_button()->click();
		QApplication::processEvents();
		expect(editor->editor()->raw() == "Changed on GitHub by someone else." && editor->original() == editor->editor()->raw(), "Reload edits from GitHub's text");
		editor->cancel_button()->click();
		expect(!editor->editing() && access::review_mutation_evidence(*application).DescriptionWrites == writes, "Cancel sends nothing");

		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << "draft-description:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "draft-description: " << aError.what() << '\n';
		if (application) {
			application->begin_shutdown();
			static_cast<void>(e2e::wait_until_plain([&] { return application->status().ShutdownComplete; }, e2e::extended_agent_budget));
		}
		return EXIT_FAILURE;
	}
}
