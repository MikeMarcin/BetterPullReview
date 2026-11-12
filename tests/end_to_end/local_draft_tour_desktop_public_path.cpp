#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/local_diff_view.hpp"
#include "bpr/qt/local_draft_preparation_area.hpp"
#include "bpr/qt/local_draft_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTextBlock>
#include <QToolButton>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0110 ([[BPR-2026]], [[BPR-2027]]) through the real
/// workbench window and the real local-draft composition, on a real Git
/// worktree, signed out, with a scripted agent runner in place of an
/// installed agent: the description and the tour written by hand on an
/// uncommitted candidate; a stop added from lines selected in Changes and
/// accepted with Save; arrival reveals its lines; a whole-change question
/// whose request holds the captured candidate and never an unselected live
/// edit; its proposals beside the tour, one accepted into the tour and one
/// rejected, a stop outside the candidate refused; the exact preview; an accepted refresh that leaves only the
/// changed stop Changed since accepted and starts a new session with the
/// earlier one readable; and a restart. Renders are written for the owner.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 8; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_LOCAL_DRAFT_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered window");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
		std::cout << "RENDER " << aName.toStdString() << ' ' << image.width() << 'x' << image.height() << '\n';
	}

	void commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	auto make_worktree(const git_runner& aGit, const std::filesystem::path& aRoot) -> std::filesystem::path
	{
		const auto seed    = aRoot / "seed";
		const auto hosted  = aRoot / "hosted.git";
		const auto primary = aRoot / "engine";
		std::filesystem::create_directories(seed);
		aGit(seed, { "init", "--quiet" });
		commit(aGit, seed, "README.md", "engine\n", "initial");
		commit(aGit, seed, "src/helper.cpp", "int helper()\n{\n\treturn 0;\n}\n", "helper");
		aGit(aRoot, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
		aGit(aRoot, { "clone", "--quiet", hosted.string(), primary.string() });
		aGit(primary, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
		aGit(primary, { "checkout", "--quiet", "-b", "feature/cancel" });
		commit(aGit, primary, "src/search.cpp", "int search()\n{\n\treturn 2;\n}\n", "search");
		return primary;
	}

	constexpr auto cancel_source = "int search()\n{\n\treturn 2;\n}\n\nvoid cancel()\n{\n\tstop_worker();\n\tjoin_worker();\n}\n";

	/// Selects the diff rows whose new-side lines are [aFirst, aLast].
	void select_new_lines(qt::local_draft_view& aPage, const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		const auto& rows  = aPage.projection().File->Rows;
		auto        first = -1;
		auto        last  = -1;
		for (std::size_t row = 0U; row < rows.size(); ++row) {
			if (rows[row].RightLine && *rows[row].RightLine >= aFirst && *rows[row].RightLine <= aLast) {
				first = first < 0 ? static_cast<int>(row) : first;
				last  = static_cast<int>(row);
			}
		}
		expect(first >= 0, "the lines are in the diff");
		auto* const document = aPage.diff()->document();
		auto        cursor   = QTextCursor{ document->findBlockByNumber(first) };
		cursor.setPosition(document->findBlockByNumber(last).position() + document->findBlockByNumber(last).length() - 1, QTextCursor::KeepAnchor);
		aPage.diff()->setTextCursor(cursor);
	}

	struct session
	{
		application::manual_executor               Lanes;
		std::unique_ptr<qt::workbench_window>      Window;
		std::unique_ptr<app::review_cycle_desktop> Cycles;
		std::unique_ptr<app::local_draft_desktop>  Drafts;
		std::vector<std::string>                   Prompts;
		std::vector<application::local_draft_agent_run> Runs;
		std::string                                Answer;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 860);
			Window->show();
			Cycles = std::make_unique<app::review_cycle_desktop>(*Window, aProfile, core::bpr_profile_id{ 1 }, Lanes, Lanes, app::review_cycle_syntax{ }, &Lanes);
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, std::nullopt, Lanes, Lanes);
			Drafts->set_folder_chooser([aChosen] { return aChosen; });
			// The scripted agent: it records what it was sent and answers.
			Drafts->set_agent(
				[this](const application::local_draft_agent_run& aRun, std::stop_token) {
					Prompts.push_back(aRun.Prompt);
					Runs.push_back(aRun);
					// Like Claude: a run that resumes keeps its session's identity.
					return application::local_draft_agent_answer{ .Answered   = true,
																  .Text       = Answer,
																  .Handle     = aRun.Handle.empty() ? "session-" + std::to_string(Runs.size()) : aRun.Handle,
																  .CostMicros = 23000U };
				},
				&Lanes,
				[] {
					return std::optional{ app::local_draft_desktop::agent_choice{ .Identity           = "agent-claude-1",
																				  .Name               = "Claude",
																				  .Profile            = 1U,
																				  .Description        = "DESCRIPTION-PROMPT",
																				  .DescriptionVersion = "local-description-v1",
																				  .Discussion         = "DISCUSSION-PROMPT",
																				  .DiscussionVersion  = "local-discussion-v1" } };
				});
			settle(Lanes);
		}

		~session()
		{
			Drafts->shutdown();
			Cycles->shutdown();
			settle(Lanes);
		}

		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;
	};

	auto row_of(QListWidget& aList, const QString& aStart) -> QListWidgetItem*
	{
		for (int row = 0; row < aList.count(); ++row) {
			if (aList.item(row)->text().startsWith(aStart)) {
				return aList.item(row);
			}
		}
		return nullptr;
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "local-draft-tour-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-tour-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-tour-worktrees" };
	const auto scratch       = std::filesystem::canonical(scratchRoot.path());
	const auto worktree      = make_worktree(*git, scratch);
	const auto chosen        = QString::fromStdString(worktree.string());
	auto       draftId       = core::local_draft_id{ };
	{
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		window.home_prepare_button()->click();
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page != nullptr && page->projection().Draft, "a local draft, signed out");
		draftId = page->projection().Draft->Id;
		expect(page->areas()->count() == 3 && page->areas()->tabText(0) == QStringLiteral("Status") && page->areas()->tabText(1) == QStringLiteral("Notes") && page->areas()->tabText(2) == QStringLiteral("Tour")
				   && page->description()->isAncestorOf(page->description()->body()) && !page->overview(),
			   "mockup A: the drawer has Status, Notes and Tour; Description is Overview's; no Evidence; the draft opens on Review");
		expect(page->candidates()->minimumWidth() >= page->candidates()->fontMetrics().horizontalAdvance(QStringLiteral("Candidate 32 of 32")),
			   "the candidate chooser holds its longest words from the start");

		// AC01: an uncommitted candidate, with a live helper edit left out.
		write_file(worktree / "src/search.cpp", cancel_source);
		write_file(worktree / "src/helper.cpp", "int helper()\n{\n\treturn 0;\n}\n");
		page->close_button()->click();
		settle(s.Lanes);
		emit window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		for (int row = 0; row < page->working()->count(); ++row) {
			auto* const item = page->working()->item(row);
			item->setCheckState(item->text() == QStringLiteral("src/search.cpp") ? Qt::Checked : Qt::Unchecked);
			settle(s.Lanes);
		}
		page->refresh_button()->click();
		settle(s.Lanes);
		const auto* latest = core::latest_candidate(*page->projection().Draft);
		expect(latest->Ordinal == 2U && latest->Included == std::vector<std::string>{ "src/search.cpp" }, "AC01: candidate 2 holds the uncommitted file");

		// AC01: the description and the tour's brief, by hand.
		page->set_overview(true);
		settle(s.Lanes);
		auto* const description = page->description();
		description->body()->setPlainText(QStringLiteral("<!-- team-template v3 -->\n## Summary\nCancel stops the search worker.\n- [x] I read the diff\n"));
		description->body()->document()->setModified(true); // as typing does
		description->save_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Description.Body.starts_with("<!-- team-template v3 -->"), "AC01: the description is saved");
		page->areas()->setCurrentIndex(2);
		settle(s.Lanes);
		auto* const tour = page->tour();
		tour->problem()->setPlainText(QStringLiteral("Searches kept running after Cancel."));
		tour->solution()->setPlainText(QStringLiteral("Cancel stops and joins the worker."));
		tour->save_brief_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Problem == "Searches kept running after Cancel."
				   && page->projection().Draft->Preparation.Description.Body.starts_with("<!-- team-template v3 -->"),
			   "AC05: the brief never writes the description");

		// AC08: a stop on lines selected in Changes; Save accepts it.
		page->set_overview(false);
		page->changes()->setCurrentItem(page->changes()->item(0));
		settle(s.Lanes);
		expect(page->projection().File && page->projection().File->Path == "src/search.cpp", "the changed file is shown");
		select_new_lines(*page, 6U, 10U);
		page->areas()->setCurrentIndex(2);
		tour->add_stop_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Stops.size() == 1U, "AC01: a stop is added");
		expect(page->projection().Draft->Preparation.Tour.Stops[0].Source.Excerpt == "void cancel()\n{\n\tstop_worker();\n\tjoin_worker();\n}\n",
			   "AC01: it binds the candidate's exact bytes, no commit identity");
		expect(tour->head()->text() == QStringLiteral("1 stop, 1 to accept"), "AC08: the head states the stops to accept");
		expect(!tour->findChildren<QPushButton*>().empty()
				   && std::ranges::none_of(tour->findChildren<QPushButton*>(), [](const QPushButton* aButton) { return aButton->text().contains(QStringLiteral("Publish")); }),
			   "AC08: no Publish action in the Tour area");
		tour->stops()->setCurrentRow(0);
		tour->stop_title()->setText(QStringLiteral("Cancel stops and joins"));
		tour->stop_text()->setPlainText(QStringLiteral("Cancel now joins the worker, so no search outlives it."));
		tour->save_button()->click();
		settle(s.Lanes);
		expect(tour->head()->text() == QStringLiteral("1 stop, all accepted"), "AC08: Save accepts the stop");
		// Arriving at a stop reveals its lines in Changes.
		tour->stops()->setCurrentRow(0);
		tour->show_button()->click();
		settle(s.Lanes);
		expect(!page->overview() && page->diff()->textCursor().selectedText().contains(QStringLiteral("stop_worker")) && page->review_diff()->selected_lines().has_value(),
			   "AC08: arrival reveals the stop's lines in Review");
		capture(window, QStringLiteral("local-draft-tour-arrival"));

		// AC03: a whole-change question. The request carries the
		// candidate; the live helper edit, never captured, cannot reach it.
		write_file(worktree / "src/helper.cpp", "int helper()\n{\n\treturn LIVE_HELPER_EDIT;\n}\n");
		s.Answer = "The join can block if the worker waits on the UI.\n\n```bpr-proposals\n"
				   "[{\"target\":\"problem\",\"text\":\"A cancelled search kept its worker running.\"},"
				   "{\"target\":\"problem\",\"text\":\"Searches never stopped.\"},"
				   "{\"target\":\"new_stop\",\"path\":\"src/helper.cpp\",\"first\":1,\"last\":3,\"title\":\"Helper\",\"text\":\"The live helper.\"},"
				   "{\"target\":\"solution\",\"text\":\"Cancel stops the worker and waits for it.\"}]\n```";
		page->areas()->setCurrentIndex(2);
		tour->stops()->setCurrentRow(-1);
		tour->stops()->clearSelection();
		settle(s.Lanes);
		page->diff()->setTextCursor(QTextCursor{ page->diff()->document() });
		auto* const chat = tour->chat();
		expect(qt::agent_run_control(*chat->ask_button()), "Ask wears the agent accent");
		chat->composer()->setText(QStringLiteral("What could break?"));
		chat->ask_button()->click();
		settle(s.Lanes);
		expect(s.Prompts.size() == 1U, "the agent was asked once");
		const auto& prompt = s.Prompts[0];
		expect(s.Runs[0].Instructions.starts_with("DISCUSSION-PROMPT") && prompt.find("DISCUSSION-PROMPT") == std::string::npos && prompt.find("stop_worker();") != std::string::npos,
			   "AC03: the request carries the candidate's captured bytes; [[BPR-2140]] the purpose's text travels as its instructions");
		expect(s.Runs[0].Path == core::agent_run_path::local_draft_discussion && s.Runs[0].Handle.empty() && !s.Runs[0].Draft.empty(),
			   "[[BPR-2138]] the discussion's first turn starts its session");
		expect(prompt.find("LIVE_HELPER_EDIT") == std::string::npos && prompt.find("src/helper.cpp") == std::string::npos, "AC03: an unselected live helper edit never enters");
		const auto& prep = page->projection().Draft->Preparation;
		expect(prep.Conversations.size() == 1U && prep.Conversations[0].Turns.size() == 2U && prep.Proposals.size() == 4U, "the turn, the answer and its proposals are kept");
		expect(chat->transcript()->text().contains(QStringLiteral("The join can block")) && chat->session_line()->text().contains(QStringLiteral("Claude"))
				   && chat->session_line()->text().contains(QStringLiteral("$0.02")),
			   "AC08: the transcript and the session line with its cost");
		expect(chat->proposals()->count() == 4, "the proposals sit beside the tour");
		expect(prep.Conversations[0].Handle == "session-1", "[[BPR-2138]] the session the agent kept is recorded with the conversation");
		// [[BPR-2138]]. The next turn resumes that session: no transcript
		// replay, no files again, only the question.
		{
			const auto keep = s.Answer;
			s.Answer        = "It waits for the worker.";
			chat->composer()->setText(QStringLiteral("And the join?"));
			chat->ask_button()->click();
			settle(s.Lanes);
			expect(s.Runs.size() == 2U && s.Runs[1].Path == core::agent_run_path::local_draft_discussion && s.Runs[1].Handle == "session-1"
					   && s.Prompts[1].find("Earlier in this conversation") == std::string::npos && s.Prompts[1].find("stop_worker();") == std::string::npos
					   && s.Prompts[1].find("And the join?") != std::string::npos,
				   "[[BPR-2138]] the follow-up resumes the native session instead of replaying the transcript");
			expect(page->projection().Draft->Preparation.Conversations.size() == 1U && page->projection().Draft->Preparation.Conversations[0].Turns.size() == 4U,
				   "the follow-up is in the same conversation");
			s.Answer = keep;
		}
		auto* const outside = row_of(*chat->proposals(), QStringLiteral("New stop"));
		chat->proposals()->setCurrentItem(outside);
		expect(!chat->accept_button()->isEnabled(), "AC03: a stop outside the candidate cannot be accepted as written");
		chat->proposals()->setCurrentItem(row_of(*chat->proposals(), QStringLiteral("Problem: Searches never")));
		chat->reject_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Problem == "Searches kept running after Cancel.", "a rejected proposal changes nothing");
		chat->proposals()->setCurrentItem(row_of(*chat->proposals(), QStringLiteral("Problem: A cancelled")));
		chat->accept_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Problem == "A cancelled search kept its worker running.", "the accepted proposal is in the tour");
		capture(window, QStringLiteral("local-draft-tour-proposals"));
		// A late proposal cannot overwrite a later author edit.
		tour->solution()->setPlainText(QStringLiteral("Cancel stops and joins the worker, on any thread."));
		tour->save_brief_button()->click();
		settle(s.Lanes);
		chat->proposals()->setCurrentItem(row_of(*chat->proposals(), QStringLiteral("Solution")));
		expect(!chat->accept_button()->isEnabled(), "AC03: a stale proposal is readable and cannot be accepted");

		// AC02, AC05: the exact preview carries the description byte for byte
		// and the tour; nothing private.
		page->set_overview(true);
		settle(s.Lanes);
		const auto preview = description->preview_text();
		expect(preview.contains(QStringLiteral("<!-- team-template v3 -->")) && preview.contains(QStringLiteral("Cancel stops and joins")) && preview.contains(QStringLiteral("link pending"))
				   && preview.contains(QStringLiteral("A cancelled search kept its worker running.")) && !preview.contains(QStringLiteral("The join can block")),
			   "AC02: the preview carries the description and tour; the conversation stays private");
		capture(window, QStringLiteral("local-draft-description"));

		// AC03, AC08: an accepted refresh that changes the stop's lines.
		write_file(worktree / "src/search.cpp", "int search()\n{\n\treturn 2;\n}\n\nvoid cancel()\n{\n\tstop_worker(now);\n\tjoin_worker();\n}\n");
		page->close_button()->click();
		settle(s.Lanes);
		emit window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		page->refresh_button()->click();
		settle(s.Lanes);
		expect(core::latest_candidate(*page->projection().Draft)->Ordinal == 3U, "candidate 3");
		page->areas()->setCurrentIndex(2);
		settle(s.Lanes);
		const auto& moved = page->projection().Draft->Preparation.Tour.Stops[0];
		expect(core::decide_local_stop(moved).Reason == core::local_stop_reason::changed && moved.Source.Candidate == 2U,
			   "AC08: the changed stop is Changed since accepted: changed, its original material kept");
		expect(tour->head()->text() == QStringLiteral("1 stop, 1 to accept"), "the head counts it");
		chat->composer()->setText(QStringLiteral("Anything else?"));
		s.Answer = "No.";
		chat->ask_button()->click();
		settle(s.Lanes);
		const auto& conversations = page->projection().Draft->Preparation.Conversations;
		expect(conversations.size() == 2U && conversations[1].Candidate == 3U && conversations[0].Turns.size() == 4U, "AC03: a new session on the successor; the earlier stays");
		expect(s.Prompts[2].find("Earlier in this conversation") == std::string::npos && s.Runs[2].Handle.empty(), "the new session replays nothing from the earlier candidate and resumes nothing");
		chat->session_menu()->menu()->aboutToShow();
		auto* const earlier = chat->session_menu()->menu()->actions().last();
		expect(earlier->text().startsWith(QStringLiteral("Candidate 2")), "the Session menu lists the earlier conversation as history");
		earlier->trigger();
		expect(chat->transcript()->text().contains(QStringLiteral("What could break?")) && chat->session_line()->text().contains(QStringLiteral("earlier")),
			   "the earlier conversation is readable against its own candidate");
		capture(window, QStringLiteral("local-draft-tour-refreshed"));
	}
	{
		// AC04: everything resumes after a restart with no agent process and
		// no account; the draft opens on Changes.
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		emit  window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page->projection().Draft && page->projection().Draft->Id == draftId && !page->overview() && page->areas()->currentIndex() == 0, "X23 as amended: a resumed draft opens on Review, with no chat");
		const auto& prep = page->projection().Draft->Preparation;
		expect(prep.Description.Body.starts_with("<!-- team-template v3 -->") && prep.Tour.Stops.size() == 1U && prep.Conversations.size() == 2U && prep.Proposals.size() == 4U,
			   "AC04: description, tour, conversations and proposals resume");
		page->areas()->setCurrentIndex(2);
		settle(s.Lanes);
		expect(page->tour()->chat()->transcript()->text().contains(QStringLiteral("Anything else?")), "AC04: the open conversation resumes without a process");
		capture(window, QStringLiteral("local-draft-tour-restarted"));
	}
	{
		// AC09 (X19): the pull request tour's description option starts on for
		// an ordinary pull request and off for one made from a local draft.
		auto editor = qt::review_tour_editor{ };
		expect(editor.description_publication() == core::update_description_by_default(false), "AC09: the existing default stands");
		editor.set_description_publication_default(core::update_description_by_default(true));
		expect(!editor.description_publication(), "AC09: off for a pull request associated with a local draft");
	}
	std::cout << "local-draft-tour-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-tour-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
