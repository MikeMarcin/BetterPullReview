#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/application/local_check_copies.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/local_draft_check.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0120 ([[BPR-2189]]) through the real workbench window and
/// the real local-draft composition, on a real Git worktree, signed out, with
/// the fixture agent (`bpr_fixture_agent`) as the agent: a plan from no words
/// and from words, nothing run before Run; Run unavailable with its reason
/// for a grant that is off, another check running and no candidate; a run in
/// the candidate's check copy with the author's worktree, branch and index
/// byte for byte unchanged, a live edit and a new candidate captured during
/// it not reaching the copy; the report tab with its outcome word,
/// candidate, commands, out-of-plan command, failure output, a code
/// reference opening the candidate at its line, and what was not covered;
/// Failed, Stopped and Didn't run reports kept; a tracked file the run
/// changed listed; an older candidate said and Run again checking the
/// latest with the same plan; the prompt `local-draft.run-check` reaching
/// plans and runs and no question; reports surviving a restart and read by
/// a later question; nothing of a check in the description, the tour or the
/// public preview; the copies counted against the limit and removed with
/// the draft.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 12; ++round) {
			aLanes.drain();
			QApplication::processEvents();
			// Cards a rebuild let go are gone before the next look.
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
		const auto primary = aRoot / "parser";
		std::filesystem::create_directories(seed);
		aGit(seed, { "init", "--quiet" });
		commit(aGit, seed, "README.md", "parser\n", "initial");
		aGit(aRoot, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
		aGit(aRoot, { "clone", "--quiet", hosted.string(), primary.string() });
		aGit(primary, { "remote", "set-url", "origin", "https://github.com/acme/parser.git" });
		aGit(primary, { "checkout", "--quiet", "-b", "feature/tokens" });
		commit(aGit, primary, "src/parser.cpp", "int count()\n{\n\treturn 2;\n}\n", "count tokens");
		return primary;
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto in = std::ifstream{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{ } };
	}

	/// Every file under the worktree, its Git directory among them, by path.
	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files[std::filesystem::relative(entry.path(), aRoot).generic_string()] = read_file(entry.path());
			}
		}
		return files;
	}

	/// Runs the fixture agent as Claude Code's one-shot stream JSON, in the
	/// run's folder: its Bash commands and its answer.
	auto run_fixture(const QString& aAgent, const application::local_draft_agent_run& aRun, const std::stop_token aStop, const bool aStopAfterFirst)
		-> application::local_draft_agent_answer
	{
		auto process     = QProcess{ };
		auto environment = QProcessEnvironment::systemEnvironment();
		environment.insert(QStringLiteral("BPR_FIXTURE_AGENT_STEERABLE"), QStringLiteral("0"));
		environment.insert(QStringLiteral("BPR_FIXTURE_AGENT_PAUSE_MS"), QStringLiteral("0"));
		process.setProcessEnvironment(environment);
		process.setWorkingDirectory(QString::fromStdString(aRun.Folder));
		process.start(aAgent, { QStringLiteral("-p"), QStringLiteral("--output-format"), QStringLiteral("stream-json"), QStringLiteral("--append-system-prompt"), QString::fromStdString(aRun.Instructions) });
		auto message = QJsonObject{ { QStringLiteral("type"), QStringLiteral("user") },
									{ QStringLiteral("message"), QJsonObject{ { QStringLiteral("role"), QStringLiteral("user") }, { QStringLiteral("content"), QString::fromStdString(aRun.Prompt) } } } };
		process.write(QJsonDocument{ message }.toJson(QJsonDocument::Compact) + '\n');
		process.closeWriteChannel();
		expect(process.waitForFinished(30000), "the fixture agent finished");
		auto answer = application::local_draft_agent_answer{ };
		for (const auto& line : process.readAllStandardOutput().split('\n')) {
			const auto object = QJsonDocument::fromJson(line).object();
			if (object.value(QStringLiteral("type")).toString() == QStringLiteral("result")) {
				answer.Answered = true;
				answer.Text     = object.value(QStringLiteral("result")).toString().toStdString();
			}
			if (line.contains("\"name\":\"Bash\"")) {
				const auto content = object.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray();
				answer.Commands.push_back(content.first().toObject().value(QStringLiteral("input")).toObject().value(QStringLiteral("command")).toString().toStdString());
				if (aStopAfterFirst) {
					break;
				}
			}
		}
		if (aStopAfterFirst || aStop.stop_requested()) {
			answer.Answered = false;
			answer.Stopped  = true;
			answer.Text.clear();
			answer.Commands.resize(std::min<std::size_t>(answer.Commands.size(), 1U));
		}
		return answer;
	}

	struct session
	{
		application::manual_executor                                   Lanes;
		std::unique_ptr<qt::workbench_window>                          Window;
		std::unique_ptr<app::review_cycle_desktop>                     Cycles;
		std::unique_ptr<app::local_draft_desktop>                      Drafts;
		std::vector<application::local_draft_agent_run>                Runs;
		std::function<void(const application::local_draft_agent_run&)> During;
		bool                                                           StopAfterFirst = false;
		std::vector<std::string>                                       Reports;
		bool                                                           Unanswered  = false;
		bool                                                           BuildAnswer = true;
		std::uint64_t                                                  Limit       = 64U * 1024U * 1024U;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen, const QString& aAgent, const std::filesystem::path& aScratchRoot)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 860);
			Window->show();
			Cycles = std::make_unique<app::review_cycle_desktop>(*Window, aProfile, core::bpr_profile_id{ 1 }, Lanes, Lanes, app::review_cycle_syntax{ }, &Lanes);
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, std::nullopt, Lanes, Lanes, app::local_draft_syntax{ }, &Lanes);
			Drafts->set_folder_chooser([aChosen] { return aChosen; });
			Drafts->set_agent(
				[this, aAgent](const application::local_draft_agent_run& aRun, const std::stop_token aStop) {
					Runs.push_back(aRun);
					if (aRun.Check == application::local_check_mode::none) {
						return application::local_draft_agent_answer{ .Answered = true, .Text = "It looks fine.", .Handle = "session-" + std::to_string(Runs.size()) };
					}
					if (During) {
						std::exchange(During, { })(aRun);
					}
					if (std::exchange(Unanswered, false)) {
						return application::local_draft_agent_answer{ .Failure = "The agent did not answer (crashed)." };
					}
					return run_fixture(aAgent, aRun, aStop, std::exchange(StopAfterFirst, false));
				},
				&Lanes,
				[] {
					return std::optional{ app::local_draft_desktop::agent_choice{ .Identity          = "fixture-claude",
																				  .Name              = "Fixture agent",
																				  .Profile           = 1U,
																				  .Discussion        = "DISCUSSION-PROMPT",
																				  .DiscussionVersion = "local-discussion-v1",
																				  .Check             = "CHECK-PROMPT",
																				  .CheckVersion      = "local-draft-run-check-v1" } };
				});
			Drafts->set_check_copies(std::make_unique<application::local_check_copies>(aScratchRoot), [this] { return Limit; });
			Drafts->set_build_question([this] { return BuildAnswer; });
			auto* const tour = Window->tour_editor();
			tour->set_dispatch([this](core::tour_command aCommand) { Drafts->tour_command(aCommand); }, [this](qt::tour_editor_action aAction) { Drafts->tour_action(aAction); });
			tour->set_tour_ask_dispatch([this](const qt::tour_ask& aAsk) { Drafts->tour_ask(aAsk); });
			tour->set_writing_dispatch([this](core::writing_hook aHook, qt::prose_capture) { Drafts->tour_writing(aHook); }, [this] { Drafts->writing_stop(); });
			tour->set_tour_session_dispatch([] { }, [this] { Drafts->tour_stop(); }, [](std::size_t, core::tour_proposal_decision, const QString&) { });
			// The window's tab host, as the application composes it, for a report's tab.
			Window->set_document_tab_dispatch([this](const core::document_tab_intent& aIntent) {
				if (const auto* const report = std::get_if<core::open_report_tab_intent>(&aIntent)) {
					Reports.push_back(report->Report);
					Window->publish(application::document_tabs_projection{ .Tabs = { { .Id = core::document_tab_id{ Reports.size() }, .Active = true, .Report = report->Report } } });
				}
			});
			Window->report_view()->set_link_dispatch([this](const QString& aLink) { expect(Drafts->follow_check_reference(aLink.toStdString()), "a report's reference is the draft's"); });
			Window->set_local_draft_observer([this](const bool aShown) {
				if (aShown) {
					Drafts->present_tour();
				}
			});
			settle(Lanes);
		}

		~session()
		{
			Drafts->shutdown();
			Cycles->shutdown();
			settle(Lanes);
			Window->set_local_draft_observer({ });
			Window->report_view()->set_link_dispatch({ });
			Window->set_document_tab_dispatch({ });
			Window->tour_editor()->set_dispatch({ }, { });
			Window->tour_editor()->set_tour_ask_dispatch({ });
			Window->tour_editor()->set_writing_dispatch({ }, { });
			Window->tour_editor()->set_tour_session_dispatch({ }, { }, { });
		}

		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;

		[[nodiscard]] auto chat() const -> qt::tour_side_chat* { return Window->tour_editor()->side_chat(); }
		[[nodiscard]] auto draft() const -> const core::local_draft& { return *Window->local_draft()->projection().Draft; }

		/// The last card of `aName` in the open conversation.
		template <typename T = QWidget>
		[[nodiscard]] auto last(const QString& aName) const -> T*
		{
			const auto found = chat()->findChildren<T*>(aName);
			return found.isEmpty() ? nullptr : found.back();
		}

		void plan(const QString& aWords)
		{
			chat()->composer()->setPlainText(aWords);
			auto* const button = chat()->findChild<QPushButton*>(QStringLiteral("tourChatPlanCheck"));
			if (button == nullptr || !button->isVisible() || !button->isEnabled()) {
				std::cerr << "PLAN visible=" << (button != nullptr && button->isVisible()) << " hidden=" << (button != nullptr && button->isHidden()) << " enabled=" << (button != nullptr && button->isEnabled())
						  << " tip=" << (button != nullptr ? button->toolTip().toStdString() : std::string{ }) << '\n';
			}
			expect(button != nullptr && button->isVisible() && button->isEnabled(), "Plan a check… is offered in the whole-change conversation");
			button->click();
			settle(Lanes);
		}

		/// The latest agent turn's check.
		[[nodiscard]] auto latest_check() const -> core::local_check
		{
			for (auto conversation = draft().Preparation.Conversations.rbegin(); conversation != draft().Preparation.Conversations.rend(); ++conversation) {
				for (auto turn = conversation->Turns.rbegin(); turn != conversation->Turns.rend(); ++turn) {
					if (turn->Check) {
						return *turn->Check;
					}
				}
			}
			throw std::runtime_error{ "no check" };
		}
	};
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git || aArgc < 5) {
		std::cout << "local-draft-check-desktop-public-path:skipped (no Git or fixture agent)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	const auto agent         = QString::fromStdString(std::filesystem::canonical(std::filesystem::path{ aArgv[4] }).string());
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-check-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-check-worktrees" };
	auto       copiesRoot    = tests::sqlite_contract::scratch_root{ "local-draft-check-copies" };
	const auto scratch       = std::filesystem::canonical(scratchRoot.path());
	const auto copies        = std::filesystem::canonical(copiesRoot.path()) / "agent-scratch";
	const auto worktree      = make_worktree(*git, scratch);
	const auto chosen        = QString::fromStdString(worktree.string());
	{
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen, agent, copies };
		auto& window = *s.Window;
		window.home_prepare_button()->click();
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page != nullptr && page->projection().Draft, "a local draft, signed out");
		window.choose_modality(qt::review_modality::review, core::workbench_modality_entry::choose);
		settle(s.Lanes);
		window.tray_tabs()->setCurrentIndex(1);
		auto* const tour = window.tour_editor();
		tour->set_agent_readiness(qt::agent_setup_readiness::ready);
		tour->open_side_chat(core::tour_thread{ });
		settle(s.Lanes);

		// AC01: no words; the agent chooses, and nothing runs.
		const auto before = snapshot(worktree);
		s.plan({ });
		std::cerr << "RUNS " << s.Runs.size() << " FAILURE " << page->projection().AskFailure << '\n';
		expect(s.Runs.size() == 1U && s.Runs[0].Check == application::local_check_mode::plan && !s.Runs[0].Builds && s.Runs[0].Scratch.empty(), "AC01: a plan reads, with no grant");
		expect(s.Runs[0].Prompt.find("choose the checks that matter most") != std::string::npos, "AC01: with no words the agent chooses");
		expect(std::filesystem::path{ s.Runs[0].Folder }.parent_path().parent_path() == copies && !s.Runs[0].Folder.starts_with(worktree.string()), "AC02: the plan reads the check copy, not the worktree");
		auto plan = s.latest_check();
		expect(plan.Plan && plan.Plan->Steps.size() == 2U && plan.Plan->NeedsBuilds && plan.Plan->State == core::local_check_state::planned, "AC01: a plan card with its commands and grants");
		auto* card = s.last(QStringLiteral("localCheckPlanCard"));
		expect(card != nullptr && card->findChildren<QWidget*>(QStringLiteral("localCheckPlanCommand")).size() == 2 && card->findChild<QWidget*>(QStringLiteral("localCheckPlanShows")) != nullptr,
			   "AC01: the card names each command and what it shows");
		auto* run = card->findChild<QPushButton*>(QStringLiteral("localCheckRun"));
		expect(!run->isEnabled() && run->toolTip().contains(QStringLiteral("Allow build and run tests")), "AC01: Run is unavailable while a needed grant is off, naming the toggle");
		capture(window, QStringLiteral("local-check-plan"));
		capture(*s.chat(), QStringLiteral("local-check-plan-chat"));
		expect(card->minimumSizeHint().width() <= s.chat()->width() && card->width() <= s.chat()->width(), "the plan card fits the side chat's width: its toggles wrap rather than widen it");

		// The first build asks; No keeps it off.
		s.BuildAnswer = false;
		card->findChild<QCheckBox*>(QStringLiteral("localCheckBuilds"))->click();
		settle(s.Lanes);
		expect(!core::grants_for(s.draft().Preparation.CheckGrants, "fixture-claude").Builds, "declined: builds stay off");
		s.BuildAnswer = true;
		s.last(QStringLiteral("localCheckPlanCard"))->findChild<QCheckBox*>(QStringLiteral("localCheckBuilds"))->click();
		settle(s.Lanes);
		expect(core::grants_for(s.draft().Preparation.CheckGrants, "fixture-claude").Builds, "allowed for this local draft and agent");
		run = s.last(QStringLiteral("localCheckPlanCard"))->findChild<QPushButton*>(QStringLiteral("localCheckRun"));
		expect(run->isEnabled(), "AC01: with its grant on, Run is available");

		// AC02: the run is in the copy; the worktree is untouched.
		run->click();
		settle(s.Lanes);
		const auto& passing = s.Runs.back();
		expect(passing.Check == application::local_check_mode::run && passing.Builds && passing.Folder == s.Runs[0].Folder, "AC02: the run reuses the candidate's one check copy, with builds");
		expect(passing.Instructions.find("CHECK-PROMPT") != std::string::npos && s.Runs[0].Instructions.find("CHECK-PROMPT") != std::string::npos, "AC06: plans and runs are briefed by the check prompt");
		expect(snapshot(worktree) == before, "AC02: the worktree, branch and index are byte for byte unchanged");
		auto report = s.latest_check();
		expect(report.Report && report.Report->Outcome == core::local_check_outcome::passed && report.Report->Commands.size() == 2U && report.Report->Commands[1].Exit == 0,
			   "AC03: a Passed report with its commands and exit status");
		expect(s.latest_check().Report && s.draft().Preparation.Conversations.back().Turns.size() == 4U, "the plan and the run are turns of one conversation");
		auto* reportCard = s.last(QStringLiteral("localCheckReportCard"));
		expect(reportCard != nullptr && reportCard->findChild<QLabel*>(QStringLiteral("localCheckOutcome"))->text() == QStringLiteral("Passed"), "AC03: the report card leads with the outcome word");
		reportCard->findChild<QPushButton*>(QStringLiteral("localCheckOpen"))->click();
		settle(s.Lanes);
		auto shown = window.report_view()->document().Markdown;
		expect(window.report_view()->isVisible() && shown.startsWith(QStringLiteral("# Passed")) && shown.contains(QStringLiteral("Checked candidate 1")),
			   "AC03: the report opens as a file tab with its outcome and candidate");
		capture(window, QStringLiteral("local-check-passed-report"));
		capture(*s.chat(), QStringLiteral("local-check-passed-report-chat"));

		// AC01, AC02, AC04: a failing plan with a tracked file changed; a
		// live edit and a new candidate during its run.
		s.plan(QStringLiteral("run the failing parser case and touch the version"));
		expect(s.Runs.back().Prompt.find("The author asks: run the failing parser case") != std::string::npos, "AC01: the author's words reach the plan");
		run                 = s.last(QStringLiteral("localCheckPlanCard"))->findChild<QPushButton*>(QStringLiteral("localCheckRun"));
		const auto copyFile = std::filesystem::path{ s.Runs.back().Folder } / "README.md";
		s.During            = [&](const application::local_draft_agent_run& aRun) {
			expect(!s.chat()->findChildren<QPushButton*>(QStringLiteral("localCheckRun")).back()->isEnabled(), "AC01: Run is unavailable while another check runs");
			// The author commits on while the check runs: a new candidate.
			commit(*git, worktree, "src/parser.cpp", "int count()\n{\n\treturn 3;\n}\n", "count three");
			s.Drafts->host().refresh();
			expect(read_file(std::filesystem::path{ aRun.Folder } / "src/parser.cpp") == "int count()\n{\n\treturn 2;\n}\n", "AC02: the copy holds the candidate, not the live edit");
		};
		const auto beforeFailing = snapshot(worktree);
		run->click();
		settle(s.Lanes);
		report = s.latest_check();
		expect(report.Report->Outcome == core::local_check_outcome::failed && report.Report->Candidate == 1U, "AC04: a Failed report on the candidate it checked");
		expect(report.Report->Changed == std::vector<std::string>{ "README.md" }, "AC04: the tracked file the run changed in its copy is listed");
		expect(std::ranges::any_of(report.Report->Commands, [](const core::local_check_command& aCommand) { return aCommand.Command == "ls build" && aCommand.OutOfPlan; }),
			   "AC03: a command not in the plan is marked");
		expect(report.Report->Output.size() == 2U && !report.Report->References.empty(), "AC03: the failure's output and a code reference");
		auto edited              = beforeFailing;
		edited["src/parser.cpp"] = "int count()\n{\n\treturn 3;\n}\n";
		auto after               = snapshot(worktree);
		std::erase_if(edited, [](const auto& aEntry) { return aEntry.first.starts_with(".git/"); });
		std::erase_if(after, [](const auto& aEntry) { return aEntry.first.starts_with(".git/"); });
		expect(after == edited, "AC02: only the author's own commit changed the worktree");
		expect(!std::filesystem::exists(copyFile) && read_file(worktree / "README.md") == "parser\n", "Q4: candidate 1's copy, stamp and all, went once the check ended; the worktree's file is untouched");
		expect(core::latest_candidate(s.draft())->Ordinal == 2U, "a new candidate was captured during the run");
		reportCard = s.last(QStringLiteral("localCheckReportCard"));
		expect(reportCard->findChild<QLabel*>(QStringLiteral("localCheckCandidate"))->text() == QStringLiteral("Checked candidate 1; the draft is now at candidate 2."), "AC04: the report says the draft moved on");
		reportCard->findChild<QPushButton*>(QStringLiteral("localCheckOpen"))->click();
		settle(s.Lanes);
		shown = window.report_view()->document().Markdown;
		expect(shown.startsWith(QStringLiteral("# Failed")) && shown.contains(QStringLiteral("exited 8")) && shown.contains(QStringLiteral("(not in the plan)"))
				   && shown.contains(QStringLiteral("parser_tests: expected 3, got 2")) && shown.contains(QStringLiteral("not tested as written")) && shown.contains(QStringLiteral("## Not covered")),
			   "AC03: the failing report's commands, output, changed files and gaps");
		capture(window, QStringLiteral("local-check-failed-report"));
		capture(*s.chat(), QStringLiteral("local-check-failed-report-chat"));
		const auto reference = core::check_reference_link(report.Report->References.front());
		window.report_view()->follow(QString::fromStdString(reference));
		settle(s.Lanes);
		expect(page->projection().File && page->projection().File->Path == report.Report->References.front().Path && page->projection().Shown == 1U,
			   "AC03: a code reference opens the checked candidate at its file");

		// AC04: Run again checks the latest candidate with the same plan.
		const auto  copiesBefore = s.Runs.back().Folder;
		const auto  failingPlan  = report.Report->Plan.Steps;
		auto* const again        = s.last(QStringLiteral("localCheckReportCard"))->findChild<QPushButton*>(QStringLiteral("localCheckAgain"));
		expect(again != nullptr, "AC04: an older candidate's report offers Run again");
		again->click();
		settle(s.Lanes);
		report = s.latest_check();
		expect(report.Report->Candidate == 2U && report.Report->Plan.Steps == failingPlan && s.Runs.back().Folder != copiesBefore,
			   "AC04: Run again checks candidate 2 in its own copy");
		expect(!std::filesystem::exists(copiesBefore), "Q4: the older candidate's copy goes once no check uses it");
		expect(read_file(std::filesystem::path{ s.Runs.back().Folder } / "src/parser.cpp") == "int count()\n{\n\treturn 3;\n}\n", "the latest candidate's bytes");

		// AC04: Stopped and Didn't run are kept.
		s.plan(QStringLiteral("parser"));
		s.StopAfterFirst = true;
		s.last(QStringLiteral("localCheckPlanCard"))->findChild<QPushButton*>(QStringLiteral("localCheckRun"))->click();
		settle(s.Lanes);
		report = s.latest_check();
		expect(report.Report->Outcome == core::local_check_outcome::stopped && report.Report->Commands.size() == 1U, "AC04: a Stopped report keeps what ran");
		s.plan({ });
		s.Unanswered = true;
		s.last(QStringLiteral("localCheckPlanCard"))->findChild<QPushButton*>(QStringLiteral("localCheckRun"))->click();
		settle(s.Lanes);
		expect(s.latest_check().Report->Outcome == core::local_check_outcome::didnt_run, "AC04: an agent that ran nothing: Didn't run, kept");
		auto reports = 0;
		for (const auto& conversation : s.draft().Preparation.Conversations) {
			reports += static_cast<int>(std::ranges::count_if(conversation.Turns, [](const core::local_turn& aTurn) { return aTurn.Check && aTurn.Check->Report; }));
		}
		expect(reports == 5, "AC04: every report is kept: Passed, Failed, Run again, Stopped, Didn't run");

		// AC05: a later question reads the reports; nothing reaches the preview.
		const auto asked = s.Runs.size();
		s.chat()->composer()->setPlainText(QStringLiteral("Why did it fail?"));
		s.chat()->send_button()->click();
		settle(s.Lanes);
		expect(s.Runs.size() == asked + 1U && s.Runs.back().Check == application::local_check_mode::none, "a question is no check");
		expect(s.Runs.back().Prompt.find("# Stopped") != std::string::npos, "AC05: a follow-up question reads the reports");
		expect(s.Runs.back().Instructions.find("CHECK-PROMPT") == std::string::npos, "AC06: the check prompt reaches no other run");
		const auto preview = core::compose_local_publication(s.draft());
		expect(preview.Body.find("Passed") == std::string::npos && preview.Tour.find("Passed") == std::string::npos && preview.Body.find("parser_tests") == std::string::npos,
			   "AC05: nothing from a check reaches the description, the tour or the public preview");
	}
	{
		// AC05: after a restart the reports are there and open.
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen, agent, copies };
		auto& window = *s.Window;
		emit  window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		window.choose_modality(qt::review_modality::review, core::workbench_modality_entry::choose);
		settle(s.Lanes);
		window.tray_tabs()->setCurrentIndex(1);
		auto* const tour = window.tour_editor();
		tour->set_agent_readiness(qt::agent_setup_readiness::ready);
		tour->open_side_chat(core::tour_thread{ });
		settle(s.Lanes);
		const auto cards = s.chat()->findChildren<QWidget*>(QStringLiteral("localCheckReportCard"));
		expect(cards.size() >= 3, "AC05: reports survive a restart with the conversation");
		cards.front()->findChild<QPushButton*>(QStringLiteral("localCheckOpen"))->click();
		settle(s.Lanes);
		expect(window.report_view()->document().Markdown.startsWith(QStringLiteral("# Passed")), "AC05: a restored report opens");
		capture(window, QStringLiteral("local-check-restarted"));

		// AC05: copies count toward the limit and go with the draft.
		auto* const page = window.local_draft();
		s.Limit          = 8U;
		commit(*git, worktree, "src/parser.cpp", "int count()\n{\n\treturn 4;\n}\n", "count four");
		s.Drafts->host().refresh();
		settle(s.Lanes);
		s.plan({ });
		expect(page->projection().AskFailure.find("scratch folder limit") != std::string::npos, "AC05: a copy past the limit is refused with the setting to change");
		s.Limit = 64U * 1024U * 1024U;
		s.plan({ });
		const auto id = s.draft().Id.Value;
		expect(std::filesystem::exists(copies / application::local_check_copies::key(id, 3U)), "the latest candidate's copy exists");
		s.Drafts->host().discard();
		settle(s.Lanes);
		auto left = 0;
		for (const auto& entry : std::filesystem::directory_iterator{ copies }) {
			left += entry.path().filename().string().starts_with("check-" + id) ? 1 : 0;
		}
		expect(left == 0, "AC05: discarding the draft removes its check copies");
	}
	std::cout << "local-draft-check-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-check-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
