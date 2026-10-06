#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/tour_owner_service.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

/// [[BPR-1062]]. A real desktop owner with one pull request open, driven by another process through the
/// shipped CLI. This process is the desktop author: it types, composes and reads its own editors on
/// command, so the course can check what each presentation shows of the other's work.
namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	auto read_command(const std::filesystem::path& aFile) -> std::string
	{
		std::ifstream     in{ aFile };
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
	void say(const std::string& aLine) { std::cout << aLine << '\n'
												   << std::flush; }
	// The author's own gesture: pick a file and a source range in the comparison, then add a stop there.
	void select_file(qt::workbench_window& aWindow, const std::string& aPath)
	{
		auto*       files = aWindow.files_drawer()->contents_view();
		QModelIndex selected;
		wait_for([&] {
			for (int row = 0; row < files->model()->rowCount(); ++row) {
				const auto index = files->model()->index(row, 0);
				if (index.data(Qt::DisplayRole).toString().toStdString() == aPath) {
					selected = index;
					return true;
				}
			}
			return false;
		},
				 "the fixture file is listed");
		files->selectionModel()->setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for([&] {
			auto* surface = aWindow.comparison_surface();
			return surface->current_result() && surface->current_context() && surface->current_context()->Key.File == core::file_identity_of(aPath) && surface->current_semantic_index() && !surface->geometry_pending();
		},
				 "the comparison shows the selected file");
	}
	void select_range(qt::workbench_window& aWindow, int aFirst, int aLast)
	{
		auto* surface = aWindow.comparison_surface();
		expect(surface->select_conversation_line(core::diff_side::right, static_cast<std::uint32_t>(aFirst)), "the source line exists");
		auto* pane = surface->right_pane();
		pane->setFocus();
		auto cursor = pane->textCursor();
		cursor.setPosition(pane->document()->findBlockByNumber(aFirst).position());
		cursor.setPosition(pane->document()->findBlockByNumber(aLast).position() + 1, QTextCursor::KeepAnchor);
		pane->setTextCursor(cursor);
		expect(surface->selected_source_range().has_value(), "the selection is an exact source range");
	}
	auto outline_ids(qt::review_tour_editor& aEditor) -> std::string
	{
		std::string result;
		for (const auto& stop : aEditor.shown().Draft->Stops) {
			result += (result.empty() ? "" : ",") + std::to_string(stop.Id.value());
		}
		return result;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	expect(aArgc == 4, "root profile signal arguments");
	const auto root             = std::filesystem::path{ aArgv[1] };
	const auto profile          = core::bpr_profile_id{ std::stoull(aArgv[2]) };
	const auto signal           = std::filesystem::path{ aArgv[3] };
	auto       options          = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = root;
	options.Profile             = profile;
	bool terminal               = false;
	auto desktop                = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
		terminal = true;
		expect(static_cast<bool>(aResult), "owner startup succeeds");
	});
	wait_for([&] { return terminal; }, "startup terminal");
	wait_for([&] { return !desktop->windows(profile).empty(); }, "owner window");
	auto& window = *desktop->window(desktop->windows(profile).front());
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
	window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.prepare_tour_action()->isEnabled() && !window.tour_editor()->shown().Loading; }, "the opened pull request composes its tour host");
	window.prepare_tour_action()->trigger();
	QApplication::processEvents();
	auto& editor = *window.tour_editor();
	wait_for([&] { return editor.shown().Draft != nullptr; }, "a live draft");
	application::tour_saved_target target;
	target.Owner = editor.shown().Draft->Owner;
	target.Draft = editor.shown().Draft->Id;
	std::cout << "TOUR_LIVE " << app::tour_draft_token(target) << "\n"
			  << std::flush;
	window.activateWindow();
	auto* problem = editor.problem_editor();
	problem->setFocus();
	wait_for([&] { return problem->hasFocus(); }, "the author's caret is in the problem field");
	say("OWNER_READY");
	QElapsedTimer course;
	course.start();
	QElapsedTimer unreadable;
	const auto    step = [&] {
		const auto command = signal.string() + ".command";
		if (!std::filesystem::exists(command)) {
			return std::filesystem::exists(signal);
		}
		const auto text = read_command(command);
		if (text.empty()) {
			// On Windows a command renamed into place a moment ago can refuse the
			// open while a scan holds it, and the read comes back empty. It stays
			// where it is and is read again, within a bound ([[BPR-1184]]).
			if (!unreadable.isValid()) {
				unreadable.start();
			}
			expect(unreadable.elapsed() <= 2000, "the course's command is readable");
			return false;
		}
		unreadable.invalidate();
		std::filesystem::remove(command);
		std::istringstream words{ text };
		std::string        verb;
		words >> verb;
		std::string argument;
		std::getline(words >> std::ws, argument);
		auto* solution = editor.solution_editor();
		if (verb == "type") {
			problem->setFocus();
			problem->moveCursor(QTextCursor::End);
			problem->insertPlainText(QString::fromStdString(argument));
			say("TYPED");
		} else if (verb == "caret") {
			auto cursor = problem->textCursor();
			cursor.setPosition(std::stoi(argument));
			problem->setTextCursor(cursor);
			say("CARET_SET");
		} else if (verb == "compose") {
			problem->setFocus();
			QInputMethodEvent event{ QString::fromStdString(argument), { } };
			QApplication::sendEvent(problem, &event);
			say(editor.composing() ? "COMPOSING" : "NOT_COMPOSING");
		} else if (verb == "commit") {
			QInputMethodEvent event;
			event.setCommitString(QString::fromStdString(argument));
			QApplication::sendEvent(problem, &event);
			say(editor.composing() ? "STILL_COMPOSING" : "COMMITTED");
		} else if (verb == "problem") {
			say("PROBLEM " + problem->toPlainText().toStdString());
		} else if (verb == "solution") {
			say("SOLUTION " + solution->toPlainText().toStdString());
		} else if (verb == "state") {
			const auto cursor = problem->textCursor();
			say("STATE caret=" + std::to_string(cursor.position()) + " anchor=" + std::to_string(cursor.anchor()) + " focus=" + (problem->hasFocus() ? "problem" : (QApplication::focusWidget() ? "other" : "none")) + " saved=" + (editor.shown().SavedRevision == editor.shown().Draft->Revision ? "yes" : "no"));
		} else if (verb == "stops") {
			const auto count = static_cast<std::size_t>(std::stoul(argument));
			if (editor.shown().Draft->Stops.empty()) {
				select_file(window, "review.cpp");
				select_range(window, 2, 4);
				window.add_tour_stop_action()->trigger();
				editor.stop_title()->insert(QStringLiteral("Why this boundary matters"));
				editor.stop_body()->insertPlainText(QStringLiteral("Review the invariant and its limits."));
			}
			while (editor.shown().Draft->Stops.size() < count) {
				const auto before = editor.shown().Draft->Stops.size();
				editor.findChild<QAction*>(QStringLiteral("tourDuplicate"))->trigger();
				expect(editor.shown().Draft->Stops.size() == before + 1, "duplicate admits another stop");
				QApplication::processEvents();
			}
			editor.show_stops();
			editor.outline()->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
			QApplication::processEvents();
			say("STOPS " + outline_ids(editor));
		} else if (verb == "outline") {
			const auto* chosen = editor.selected_stop();
			say("OUTLINE " + outline_ids(editor) + " selected=" + (chosen ? std::to_string(chosen->Id.value()) : "none"));
		} else if (verb == "undo") {
			editor.trigger(qt::tour_editor_action::undo_structure);
			say("UNDO_TRIGGERED");
		} else if (verb == "pane") {
			// [[BPR-1227]]. What the author's own comparison shows on these right-side lines, as hex
			// so a line break cannot end the answer early.
			std::istringstream range{ argument };
			int                first = 0, last = 0;
			range >> first >> last;
			const auto* document = window.comparison_surface()->right_pane()->document();
			std::string shown;
			for (int line = first; line <= last; ++line) {
				shown += document->findBlockByNumber(line).text().toStdString() + "\n";
			}
			std::string hex;
			for (const auto byte : shown) {
				constexpr const char* digits = "0123456789abcdef";
				hex += digits[(static_cast<unsigned char>(byte) >> 4U) & 0xfU];
				hex += digits[static_cast<unsigned char>(byte) & 0xfU];
			}
			say("PANE " + hex);
		}
		return std::filesystem::exists(signal);
	};
	while (!step() && course.elapsed() < 180000) {
		QApplication::processEvents();
		QThread::msleep(1);
	}
	expect(std::filesystem::exists(signal), "the handoff course ends");
	desktop->quit();
	wait_for([&] { return desktop->is_quiescent(); }, "orderly drain");
	expect(desktop->exit_code() == 0, "owner orderly drain");
	say("OWNER_RELEASED");
	return EXIT_SUCCESS;
}
