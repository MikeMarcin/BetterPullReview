#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/review_cycle_share_page.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

/// [[BPR-1168]]. A real desktop owner with a local cycle open, driven by another
/// process through the shipped `bpr cycle`. This process is the desktop
/// reviewer: on command it resumes the cycle, writes a note, composes the next
/// round, types or composes its goal and reports what its window shows, so the
/// course can check what each interface shows of the other's work.
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
	auto wait_for(const std::function<bool()>& aCondition, const int aMilliseconds = 30000) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		return aCondition();
	}
	/// \brief Whether the owner's command endpoint is bound, without taking it.
	/// \details On Windows the endpoint is a named pipe. `std::filesystem::exists`
	/// opens a pipe as a client: it can take the owner's only free instance, and
	/// a busy pipe makes it throw (ERROR_PIPE_BUSY), which ended this course in
	/// terminate. `WaitNamedPipeW` answers whether the pipe exists and connects
	/// nothing; a pipe that exists but is busy still counts as bound.
	auto endpoint_bound(const std::filesystem::path& aEndpoint) -> bool
	{
#if defined(_WIN32)
		return ::WaitNamedPipeW(aEndpoint.c_str(), 1) != FALSE || ::GetLastError() == ERROR_SEM_TIMEOUT;
#else
		std::error_code code;
		return std::filesystem::exists(aEndpoint, code);
#endif
	}
	auto read_command(const std::filesystem::path& aFile) -> std::string
	{
		std::ifstream     in{ aFile };
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
	void say(const std::string& aLine)
	{
		std::cout << aLine << '\n'
				  << std::flush;
	}
	auto utf8(const QString& aText) -> std::string
	{
		const auto bytes = aText.toUtf8();
		return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
	}
	auto idle(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.projection().Activity == application::review_cycle_activity::idle;
	}
	auto notes_line(const qt::review_cycle_view& aPage) -> std::string
	{
		auto line = "NOTES " + std::to_string(aPage.projection().Notes.size());
		for (const auto& note : aPage.projection().Notes) {
			line += "|" + core::cycle_wire_name(note.Item.Id) + ":" + note.Body.substr(0, note.Body.find('\n'));
		}
		return line;
	}
	/// The round draft as the window shows it: revision, goal, the Goal field and each listed note.
	auto share_line(const qt::review_cycle_view& aPage) -> std::string
	{
		const auto& share = aPage.projection().Share;
		if (!share) {
			return "SHARE none";
		}
		auto* const page  = aPage.share_page();
		auto        items = std::string{ };
		for (int row = 0; row < page->items()->count(); ++row) {
			const auto* item = page->items()->item(row);
			items += (items.empty() ? "" : ",") + core::cycle_wire_name(core::cycle_feedback_item_id{ item->data(Qt::UserRole + 1).toULongLong() }) + ":" + (item->checkState() == Qt::Checked ? "1" : "0");
		}
		return "SHARE revision=" + std::to_string(share->Draft.Revision) + " goal=" + share->Selection.Goal + " field=" + utf8(page->goal()->text()) + " modified=" + (page->goal()->isModified() ? "1" : "0")
			+ " frozen=" + (share->Frozen ? "1" : "0") + " items=" + items;
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
	expect(wait_for([&] { return terminal; }), "startup terminal");
	expect(wait_for([&] { return !desktop->windows(profile).empty(); }), "owner window");
	auto& window = *desktop->window(desktop->windows(profile).front());
	auto* page   = window.local_cycle();
	expect(page != nullptr, "the window composes the local cycle page");
	const auto endpoint = adapters::sqlite::tour_owner_endpoint(root);
	expect(endpoint.has_value() && wait_for([&] { return endpoint_bound(*endpoint); }), "the owner's command endpoint is bound");
	say("OWNER_ENDPOINT " + endpoint->string());
	say("OWNER_READY");
	QElapsedTimer course;
	course.start();
	const auto step = [&] {
		const auto command = signal.string() + ".command";
		if (!std::filesystem::exists(command)) {
			return std::filesystem::exists(signal);
		}
		const auto text = read_command(command);
		std::filesystem::remove(command);
		std::istringstream words{ text };
		std::string        verb;
		words >> verb;
		std::string argument;
		std::getline(words >> std::ws, argument);
		auto* const share = page->share_page();
		if (verb == "resume") {
			// Home's Review Cycles section, as a reviewer resumes a cycle.
			window.choose_home(true);
			const auto listed = wait_for([&] { return window.home_cycles_list()->count() >= 1; });
			if (!listed) {
				say("RESUME_NOTHING_LISTED");
				return false;
			}
			emit       window.home_cycles_list()->itemActivated(window.home_cycles_list()->item(0));
			const auto open = wait_for([&] { return window.showing_local_cycle() && page->projection().Cycle && page->projection().Source && idle(*page); });
			say(open ? "RESUMED " + page->projection().Cycle->Title : std::string{ "RESUME_FAILED" });
		} else if (verb == "resume-title") {
			// [[BPR-1169]]. The reviewer moves the window to another cycle, by its title in Home.
			window.choose_home(true);
			wait_for([&] { return window.home_cycles_list()->count() >= 2; });
			auto found = false;
			for (int row = 0; row < window.home_cycles_list()->count() && !found; ++row) {
				auto* item = window.home_cycles_list()->item(row);
				if (item->text().contains(QString::fromStdString(argument))) {
					emit window.home_cycles_list()->itemActivated(item);
					found = true;
				}
			}
			const auto open = found && wait_for([&] { return window.showing_local_cycle() && page->projection().Cycle && page->projection().Cycle->Title == argument && idle(*page); });
			say(open ? "FOCUSED " + core::cycle_wire_name(page->projection().Cycle->Subject.Cycle) : std::string{ "FOCUS_FAILED" });
		} else if (verb == "notes") {
			say(notes_line(*page));
		} else if (verb == "wait-notes") {
			const auto count = static_cast<std::size_t>(std::stoul(argument));
			wait_for([&] { return page->projection().Notes.size() >= count && idle(*page); });
			say(notes_line(*page));
		} else if (verb == "type-note") {
			// A note on the first line of the shown file, typed and not saved.
			page->source()->select_bytes(0U, 1U);
			QApplication::processEvents();
			page->comment_action()->trigger();
			page->comment_editor()->setFocus();
			page->comment_editor()->setPlainText(QString::fromStdString(argument));
			say(page->comment_editor()->isVisible() ? "NOTE_TYPED" : "NOTE_NOT_OPEN");
		} else if (verb == "note-text") {
			say("NOTE_TEXT " + utf8(page->comment_editor()->toPlainText()) + " visible=" + (page->comment_editor()->isVisible() ? "1" : "0"));
		} else if (verb == "save-note") {
			page->findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
			wait_for([&] { return idle(*page); });
			say("NOTE_SAVED " + page->projection().Message);
		} else if (verb == "compose") {
			page->compose_action()->trigger();
			const auto shown = wait_for([&] { return page->projection().Share && idle(*page); });
			say(shown ? "COMPOSED" : "COMPOSE_FAILED");
		} else if (verb == "share") {
			say(share_line(*page));
		} else if (verb == "wait-share") {
			// Until the shown draft reaches a revision, or the Goal field shows a text.
			wait_for([&] {
				const auto& shown = page->projection().Share;
				if (!shown || !idle(*page)) {
					return false;
				}
				if (argument == "frozen") {
					return shown->Frozen;
				}
				return argument.starts_with("revision=") ? shown->Draft.Revision >= std::stoull(argument.substr(9)) : utf8(share->goal()->text()) == argument.substr(5);
			});
			say(share_line(*page));
		} else if (verb == "goal") {
			// Typed into Goal and not yet entered: committed text the store does not have.
			share->goal()->setFocus();
			share->goal()->end(false);
			share->goal()->insert(QString::fromStdString(argument));
			say(share->goal()->isModified() ? "GOAL_TYPED" : "GOAL_NOT_MODIFIED");
		} else if (verb == "goal-compose") {
			share->goal()->setFocus();
			QInputMethodEvent event{ QString::fromStdString(argument), { } };
			QApplication::sendEvent(share->goal(), &event);
			say(share->draft_editor() == core::cycle_draft_editor::composing ? "COMPOSING" : "NOT_COMPOSING");
		} else if (verb == "goal-commit") {
			QInputMethodEvent event;
			event.setCommitString(QString::fromStdString(argument));
			QApplication::sendEvent(share->goal(), &event);
			say(share->draft_editor() == core::cycle_draft_editor::composing ? "STILL_COMPOSING" : "COMMITTED");
		} else if (verb == "toggle") {
			// The reviewer's own check box on one listed note.
			for (int row = 0; row < share->items()->count(); ++row) {
				auto* item = share->items()->item(row);
				if (core::cycle_wire_name(core::cycle_feedback_item_id{ item->data(Qt::UserRole + 1).toULongLong() }) == argument) {
					item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
				}
			}
			wait_for([&] { return idle(*page); });
			say("TOGGLED");
		} else if (verb == "focus") {
			const auto* focused = QApplication::focusWidget();
			say(std::string{ "FOCUS " } + (focused == share->goal() ? "goal" : focused == page->comment_editor() ? "note"
											   : focused                                                         ? "other"
																												 : "none"));
		}
		return std::filesystem::exists(signal);
	};
	while (!step() && course.elapsed() < 240000) {
		QApplication::processEvents();
		QThread::msleep(1);
	}
	expect(std::filesystem::exists(signal), "the handoff course ends");
	desktop->quit();
	expect(wait_for([&] { return desktop->is_quiescent(); }), "orderly drain");
	expect(desktop->exit_code() == 0, "owner orderly drain");
	say("OWNER_RELEASED");
	return EXIT_SUCCESS;
}
