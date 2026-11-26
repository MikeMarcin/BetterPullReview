/// \file
/// \brief [[BPR-2221]]. Every prompt condition reads the window's real state,
/// through the shipped window on returning-review-v1 (viewer `jiayin`).
///
/// Own prompts, each with one condition and Hide while unmet, are placed on
/// the pull request's, a file's and a thread's Ask menus. Each is listed
/// exactly where its condition holds on the fixture: replies since my
/// comment where `tbutler` replied after `jiayin` (PRRT_cc_1) and not where
/// `jiayin` commented last (PRRT_cc_3) or never (PRRT_cc_7); new replies
/// since the comment where any followed; a file with an open thread; the pull
/// request not mine, not a draft, no check failing, pushes since my review
/// and the repository's name. Then the state changes through the window and
/// the menus, built fresh, follow it: Resolve on PRRT_cc_7 (slider.cpp's only
/// thread) hides "thread is unresolved" there and "file has unresolved
/// threads" on slider.cpp, and shows their "is not"; marking renderer.cpp
/// read hides "I haven't reviewed this file" on it. No agent is asked.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/prompts_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QMenu>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
	}

	/// One own prompt: its condition, what it is about, and the menu it is on.
	struct conditioned
	{
		std::string                 Name;
		core::prompt_fact           Fact     = core::prompt_fact::draft;
		bool                        Negated  = false;
		core::prompt_subject        Subject  = core::prompt_subject::pull_request;
		core::prompt_surface        Surface  = core::prompt_surface::pull_request_menu;
		std::vector<std::string>    Patterns = { };
		core::prompt_activity_value Activity = core::prompt_activity_value::replies_or_pushes;
		std::string                 Id;
	};

	[[nodiscard]] auto scope_of(const core::prompt_surface aSurface) -> QString
	{
		return aSurface == core::prompt_surface::thread_menu ? QStringLiteral("thread") : aSurface == core::prompt_surface::file_menu ? QStringLiteral("file")
																																	  : QStringLiteral("pull-request");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options            = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.EpochPhase      = app::detail::github_epoch_fixture_phase::reuse;
		options.ResolvesThreads = true;
		auto terminal           = false;
		auto startup            = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](auto aResult) {
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
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto renderer = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.what_changed_action()->isEnabled(); }, "the returning pull request's commits are read");
		wait([&] { return window.go_to_file_line(renderer, 12U, false) && window.active_document_path() == renderer; }, "renderer.cpp opens");

		using enum core::prompt_fact;
		using surface = core::prompt_surface;
		using subject = core::prompt_subject;
		auto prompts  = std::vector<conditioned>{
			{ .Name = "Not mine", .Fact = author_is_me, .Negated = true },
			{ .Name = "Mine", .Fact = author_is_me },
			{ .Name = "Not a draft", .Fact = draft, .Negated = true },
			{ .Name = "A draft", .Fact = draft },
			{ .Name = "No check failing", .Fact = checks_failing, .Negated = true },
			{ .Name = "A check failing", .Fact = checks_failing },
			{ .Name = "Pushed since my review", .Fact = pushes_since_review },
			{ .Name = "This repository", .Fact = repository, .Patterns = { "bpr-fixtures/*" } },
			{ .Name = "Another repository", .Fact = repository, .Patterns = { "elsewhere/*" } },
			{ .Name = "Has open threads", .Fact = file_unresolved_threads, .Subject = subject::open_file, .Surface = surface::file_menu },
			{ .Name = "No open threads", .Fact = file_unresolved_threads, .Negated = true, .Subject = subject::open_file, .Surface = surface::file_menu },
			{ .Name = "Not reviewed yet", .Fact = file_unreviewed, .Subject = subject::open_file, .Surface = surface::file_menu },
			{ .Name = "Reviewed already", .Fact = file_unreviewed, .Negated = true, .Subject = subject::open_file, .Surface = surface::file_menu },
			{ .Name = "Markdown", .Fact = path, .Subject = subject::open_file, .Surface = surface::file_menu, .Patterns = { "*.md" } },
			{ .Name = "Unresolved", .Fact = thread_unresolved, .Subject = subject::conversation, .Surface = surface::thread_menu },
			{ .Name = "Resolved", .Fact = thread_unresolved, .Negated = true, .Subject = subject::conversation, .Surface = surface::thread_menu },
			{ .Name = "Replied to", .Fact = thread_activity, .Subject = subject::conversation, .Surface = surface::thread_menu, .Activity = core::prompt_activity_value::replies },
			{ .Name = "No replies", .Fact = thread_activity, .Negated = true, .Subject = subject::conversation, .Surface = surface::thread_menu, .Activity = core::prompt_activity_value::replies },
			{ .Name = "Replies since mine", .Fact = replies_since_mine, .Subject = subject::conversation, .Surface = surface::thread_menu },
			{ .Name = "None since mine", .Fact = replies_since_mine, .Negated = true, .Subject = subject::conversation, .Surface = surface::thread_menu },
			{ .Name = "Takes replies", .Fact = takes_replies, .Subject = subject::conversation, .Surface = surface::thread_menu },
		};
		auto* const page = window.prompts();
		expect(page != nullptr, "the Prompts page is composed");
		for (auto& prompt : prompts) {
			prompt.Id = page->new_prompt();
			expect(!prompt.Id.empty(), "a new prompt for " + prompt.Name);
			page->edit_selected([&prompt](core::prompt_macro& aMacro) {
				aMacro.Body.Name       = prompt.Name;
				aMacro.Body.Template   = "Ask about it: " + prompt.Name;
				aMacro.Body.Subject    = prompt.Subject;
				aMacro.Place           = prompt.Subject == subject::conversation ? core::prompt_place::conversation : core::prompt_place::agent_tab;
				aMacro.Body.Conditions = { core::prompt_condition{ .Fact = prompt.Fact, .Negated = prompt.Negated, .Patterns = prompt.Patterns, .Activity = prompt.Activity } };
				aMacro.Body.WhenUnmet  = core::prompt_when_unmet::hide;
			});
			const auto refused = page->place(prompt.Id, prompt.Surface);
			expect(refused == core::placement_refusal::none, prompt.Name + " is placed on its menu: " + page->last_refusal().toStdString());
		}

		// A menu built as the window builds it, for a file or a thread.
		const auto listed = [&](const conditioned& aPrompt, const QString& aFile, const QString& aThread) {
			auto request   = window.ask_request(aPrompt.Surface == surface::thread_menu     ? core::quick_ask_anchor::thread
													: aPrompt.Surface == surface::file_menu ? core::quick_ask_anchor::file
																							: core::quick_ask_anchor::review_bar);
			request.File   = aFile;
			request.Thread = aThread;
			QMenu menu;
			window.fill_quick_ask(menu, request);
			return menu.findChild<QAction*>(QStringLiteral("quickAsk_%1_%2").arg(QString::fromStdString(aPrompt.Id), scope_of(aPrompt.Surface))) != nullptr;
		};
		const auto named = [&](const std::string& aName) -> const conditioned& {
			for (const auto& prompt : prompts) {
				if (prompt.Name == aName) {
					return prompt;
				}
			}
			throw std::runtime_error{ "no prompt " + aName };
		};
		auto       failures = std::string{ };
		const auto check    = [&](const std::string& aName, const QString& aFile, const QString& aThread, const bool aShown) {
			if (listed(named(aName), aFile, aThread) != aShown) {
				failures += "\n  " + aName + " on " + (aThread.isEmpty() ? aFile : aThread).toStdString() + (aShown ? " is not listed" : " is listed");
			}
		};
		const auto settle = [&](const std::string& aWhat) {
			if (!failures.empty()) {
				throw std::runtime_error{ aWhat + ":" + failures };
			}
		};
		const auto thread = [](const char* aId) { return QStringLiteral("review-thread:") + QString::fromLatin1(aId); };
		const auto slider = QStringLiteral("src/ui/widgets/slider.cpp");
		const auto readme = QStringLiteral("README.md");

		// The pull request: fixture-author's, open, no check runs, a push since
		// my review, in bpr-fixtures.
		wait([&] { return listed(named("Not mine"), renderer, { }); }, "the pull request's facts are read");
		for (const auto& [name, shown] : std::vector<std::pair<std::string, bool>>{
				 { "Not mine", true }, { "Mine", false }, { "Not a draft", true }, { "A draft", false }, { "No check failing", true }, { "A check failing", false }, { "Pushed since my review", true }, { "This repository", true }, { "Another repository", false } }) {
			check(name, renderer, { }, shown);
		}
		settle("the pull request's menu");

		// A file.
		check("Has open threads", renderer, { }, true);
		check("No open threads", renderer, { }, false);
		check("Has open threads", QStringLiteral("docs/guide.md"), { }, false);
		check("No open threads", QStringLiteral("docs/guide.md"), { }, true);
		check("Markdown", readme, { }, true);
		check("Markdown", renderer, { }, false);
		wait([&] { return listed(named("Not reviewed yet"), renderer, { }); }, "the review's marks are read: renderer.cpp is not reviewed");
		check("Reviewed already", renderer, { }, false);
		settle("a file's menu");

		// A thread.
		for (const auto* const id : { "PRRT_cc_1", "PRRT_cc_3", "PRRT_cc_7" }) {
			check("Unresolved", { }, thread(id), true);
			check("Resolved", { }, thread(id), false);
			check("Takes replies", { }, thread(id), true);
		}
		check("Replied to", { }, thread("PRRT_cc_1"), true);
		check("No replies", { }, thread("PRRT_cc_1"), false);
		check("Replied to", { }, thread("PRRT_cc_7"), false);
		check("No replies", { }, thread("PRRT_cc_7"), true);
		// tbutler replied after jiayin's last comment on cc_1 and cc_4; jiayin
		// commented last on cc_3, and never on cc_7.
		check("Replies since mine", { }, thread("PRRT_cc_1"), true);
		check("Replies since mine", { }, thread("PRRT_cc_4"), true);
		check("None since mine", { }, thread("PRRT_cc_1"), false);
		check("Replies since mine", { }, thread("PRRT_cc_3"), false);
		check("None since mine", { }, thread("PRRT_cc_3"), true);
		check("Replies since mine", { }, thread("PRRT_cc_7"), false);
		check("None since mine", { }, thread("PRRT_cc_7"), true);
		settle("a thread's menu");

		// Resolve slider.cpp's only thread through its card.
		auto* const cards = window.conversation_panel()->cards();
		window.choose_modality(qt::review_modality::discuss);
		QApplication::processEvents();
		const auto cc7     = std::string{ "review-thread:PRRT_cc_7" };
		auto       pressed = false;
		wait([&] {
			if (pressed) {
				return true;
			}
			cards->open_thread(cc7);
			QApplication::processEvents();
			auto* const card   = cards->open_card();
			auto* const button = card != nullptr ? card->findChild<QAbstractButton*>(QStringLiteral("threadResolve")) : nullptr;
			if (button == nullptr || !button->isEnabled() || button->accessibleName() != QStringLiteral("Resolve conversation")) {
				return false;
			}
			button->click();
			pressed = true;
			return true;
		},
			 "PRRT_cc_7's card offers Resolve conversation");
		wait([&] { return !listed(named("Unresolved"), { }, thread("PRRT_cc_7")); }, "resolved, thread is unresolved no longer holds on it");
		check("Resolved", { }, thread("PRRT_cc_7"), true);
		check("Unresolved", { }, thread("PRRT_cc_1"), true);
		check("Has open threads", slider, { }, false);
		check("No open threads", slider, { }, true);
		check("Has open threads", renderer, { }, true);
		settle("after the resolve");
		expect(access::review_mutation_evidence(*application).Resolutions == 1U, "one resolution was asked of the fixture GitHub");

		// Mark renderer.cpp reviewed (read), as its toggle does.
		window.choose_modality(qt::review_modality::review);
		wait([&] { return window.go_to_file_line(renderer, 12U, false) && window.active_document_path() == renderer; }, "renderer.cpp is the selected file");
		auto* const toggle = window.findChild<QAction*>(QStringLiteral("toggleSelectedFileReadAction"));
		expect(toggle != nullptr, "the window has the mark-read toggle");
		wait([&] { return toggle->isEnabled(); }, "the mark-read toggle is offered on renderer.cpp");
		toggle->trigger();
		wait([&] { return !listed(named("Not reviewed yet"), renderer, { }); }, "marked reviewed, I haven't reviewed this file no longer holds on it");
		check("Reviewed already", renderer, { }, true);
		check("Not reviewed yet", readme, { }, true);
		settle("after marking renderer.cpp reviewed");

		const auto mutations = access::review_mutation_evidence(*application);
		expect(mutations.Submissions == 0U && mutations.Replies == 0U, "nothing but the resolution reached GitHub");
		shutdown(*application);
		std::cout << "prompt-facts:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "prompt-facts: " << aError.what() << '\n';
		if (application) {
			std::cerr << "github-status: " << application->window().github_status_text() << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
