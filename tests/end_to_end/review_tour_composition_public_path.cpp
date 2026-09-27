#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;
	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::default_queue_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}
	void open(app::github_application& aApp)
	{
		aApp.show();
		auto& window = aApp.window();
		window.github_connect_action()->trigger();
		wait([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
		window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		window.home_open_action()->trigger();
		wait([&] { return window.prepare_tour_action()->isEnabled() && !window.tour_editor()->shown().Loading; }, "ordinary PR open composes tour host");
		window.prepare_tour_action()->trigger();
		QApplication::processEvents();
		expect(window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()) == QStringLiteral("Tour") && window.tour_editor()->isVisible(), "Prepare review tour opens the composed Tour tray");
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait([&] { return aApp.status().ShutdownComplete; }, "tour saves drain during application shutdown");
	}
	void saved(qt::review_tour_editor& aEditor)
	{
		wait([&] { const auto& p = aEditor.shown(); return p.Draft && !p.Saving && p.SavedRevision == p.Draft->Revision; }, "tour saved through admitted SQLite store");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir data;
	const auto    dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		return EXIT_FAILURE;
	}
	auto server = std::make_shared<app::detail::review_submission_fixture_state>();
	auto make   = [&] {
		std::unique_ptr<app::github_application> started;
		bool                                     terminal = false;
		auto                                     startup  = access::start(app::detail::github_public_path_fixture::github_com,
																		  app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot, .ReviewSubmissionServer = server },
																		  [&](auto aResult) { terminal = true; if (aResult) { started = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(started != nullptr, "startup succeeds");
		return started;
	};
	auto application = make();
	try {
		expect(!application->window().prepare_tour_action()->isEnabled() && application->window().tour_editor()->problem_editor()->isReadOnly(), "no PR has no invented writable draft");
		open(*application);
		auto& window = application->window();
		auto& editor = *window.tour_editor();
		expect(editor.problem_editor()->toPlainText().isEmpty() && editor.solution_editor()->toPlainText().isEmpty(), "new draft is empty, coaching is not source");
		const auto mutationBefore = access::review_mutation_evidence(*application);
		const auto agentBefore    = access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations;
		expect(agentBefore == 0U, "no agent configured or launched for authoring");
		const auto problem = QStringLiteral("## Before\n\nLiteral <details><summary>why</summary>\n\n```cpp\nreturn 1;\n```\n</details>\n\n| A | B |\n| - | - |\n| x | y |\n");
		editor.problem_editor()->insertPlainText(problem);
		saved(editor);
		editor.problem_editor()->undo();
		expect(editor.problem_editor()->toPlainText().isEmpty() && editor.shown().Draft->Problem.empty(), "native undo survives asynchronous save acknowledgement");
		editor.problem_editor()->redo();
		expect(editor.shown().Draft->Problem == problem.toStdString(), "native redo restores exact Markdown source");
		editor.roadmap()->setCurrentIndex(1);
		editor.solution_editor()->setFocus();
		QInputMethodEvent preedit{ QStringLiteral("に"), { } };
		QApplication::sendEvent(editor.solution_editor(), &preedit);
		expect(editor.shown().Draft->Solution.empty(), "IME preedit is not committed into the draft");
		QInputMethodEvent commit;
		commit.setCommitString(QStringLiteral("日本語 — "));
		QApplication::sendEvent(editor.solution_editor(), &commit);
		QApplication::clipboard()->setText(QStringLiteral("**central approach**\n\n[read more](https://example.invalid/)"));
		editor.solution_editor()->paste();
		const auto solution = editor.solution_editor()->toPlainText();
		expect(solution.startsWith(QStringLiteral("日本語 — **central")), "native IME commit and clipboard preserve Unicode Markdown");
		saved(editor);
		const auto accepted = *editor.shown().Draft;
		editor.roadmap()->setCurrentIndex(editor.roadmap()->count() - 1);
		expect(editor.preview()->original_body().find(accepted.Problem) != std::string::npos && editor.preview()->original_body().find(accepted.Solution) != std::string::npos, "preview retains raw source without rendering feedback into draft");
		editor.preview()->show_source(true);
		expect(*editor.shown().Draft == accepted, "preview Source toggle is read-only");
		editor.roadmap()->setCurrentIndex(0);
		expect(editor.problem_editor()->toPlainText() == problem, "free section revisit preserves text");
		if (argc > 1) {
			window.resize(1280, 900);
			QApplication::processEvents();
			expect(window.grab().save(QString::fromLocal8Bit(argv[1])), "composed editor render");
		}
		const auto mutationAfter = access::review_mutation_evidence(*application);
		expect(mutationAfter.Submissions == mutationBefore.Submissions && mutationAfter.StoredComments == mutationBefore.StoredComments && mutationAfter.Replies == mutationBefore.Replies
				   && access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0U,
			   "entry, editing, preview and section navigation perform zero remote mutations or agent launches");
		shutdown(*application);
		application.reset();
		application = make();
		open(*application);
		auto& restored = *application->window().tour_editor();
		expect(*restored.shown().Draft == accepted && restored.problem_editor()->toPlainText() == problem && restored.solution_editor()->toPlainText() == solution, "actual application reconstruction restores exact tour from admitted SQLite root");
		// Failures are already exercised at the real host/store seam. Verify the
		// native surface keeps the accepted editor and exposes explicit recovery.
		auto failure    = restored.shown();
		failure.Failure = application::tour_store_failure::unavailable;
		restored.present(failure);
		expect(restored.problem_editor()->toPlainText() == problem && restored.findChild<QLabel*>(QStringLiteral("tourSaveStatus"))->text().contains(QStringLiteral("not yet durable")), "local failure preserves source and explains durability");
		restored.present({ });
		expect(restored.problem_editor()->isReadOnly() && restored.problem_editor()->toPlainText().isEmpty(), "unavailable owner clears prior account text");
		restored.present(failure);
		// A missing product wire must be detectable: typing cannot masquerade as
		// durable success solely because a native editor displays the candidate.
		restored.set_dispatch({ }, { });
		restored.problem_editor()->insertPlainText(QStringLiteral("unsaved wire negative control"));
		expect(restored.problem_editor()->toPlainText().toStdString() != restored.shown().Draft->Problem, "removed dispatch wire is detected by accepted-state oracle");
		shutdown(*application);
		std::cout << "review-tour-composition:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "review-tour-composition:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
