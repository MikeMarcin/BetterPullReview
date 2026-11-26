#include "../git_contract/windows_system_root.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "local_draft_open_diagnostic.hpp"

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
/// \brief RFC-BPR-0110 ([[BPR-2026]], [[BPR-2027]]) as amended by the owner
/// ("I thought we chose to stick with the PR view and just augment it")
/// through the real workbench window and the real local-draft composition,
/// on a real Git worktree, signed out, with a scripted agent runner in place
/// of an installed agent. The local draft's tour is the window's own Tour
/// panel and its description is Overview's: the description written in
/// Overview's editor; a stop
/// added from lines selected in Review and accepted with Save (the green
/// ring); no Publish in its head; arriving at the stop reveals its lines; a
/// whole-change question in the panel's side chat whose request holds the
/// captured candidate and never an unselected live edit, its cost and its
/// proposals in the panel, one accepted into the tour and a stale one
/// refused; the exact public preview without the conversation; an accepted
/// refresh that leaves only the changed stop Changed since accepted, with
/// the draft's own reason, and starts a new session with the earlier turns
/// readable; and a restart. The panel's gestures reach the draft through the
/// same desktop entry points the application composition routes them to.

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

	/// Selects the new-side lines [aFirst, aLast] in the window's own diff viewer, as a drag does.
	void select_new_lines(qt::workbench_window& aWindow, const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		auto* const viewer = aWindow.comparison_surface();
		expect(viewer->current_result() != nullptr, "the lines are in the diff");
		const auto digest = viewer->current_result()->Alignment.Digest;
		expect(viewer->restore_source_range({ .Comparison = digest, .Side = core::diff_side::right, .Line = aFirst - 1U }, { .Comparison = digest, .Side = core::diff_side::right, .Line = aLast - 1U }),
			   "the lines are selected");
	}

	struct session
	{
		application::manual_executor                    Lanes;
		std::unique_ptr<qt::workbench_window>           Window;
		std::unique_ptr<app::local_draft_desktop>       Drafts;
		std::vector<std::string>                        Prompts;
		std::vector<application::local_draft_agent_run> Runs;
		std::string                                     Answer;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 860);
			Window->show();
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, bpr::tests::git_contract::windows_system_root(), Lanes, Lanes, app::local_draft_syntax{ }, &Lanes);
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
			// The panel's gestures go to the draft as the application
			// composition routes them while a local draft shows.
			auto* const tour = Window->tour_editor();
			tour->set_dispatch([this](core::tour_command aCommand) { Drafts->tour_command(aCommand); }, [this](qt::tour_editor_action aAction) { Drafts->tour_action(aAction); });
			tour->set_tour_ask_dispatch([this](const qt::tour_ask& aAsk) { Drafts->tour_ask(aAsk); });
			tour->set_writing_dispatch([this](core::writing_hook aHook, qt::prose_capture) { Drafts->tour_writing(aHook); }, [this] { Drafts->tour_stop(); });
			tour->set_tour_session_dispatch([] { }, [this] { Drafts->tour_stop(); },
											[this](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { Drafts->tour_decide(aIndex, aDecision, aText); });
			Window->overview_page()->set_description_save_dispatch([this](const qt::overview_description_save& aSave) {
				Drafts->save_description(aSave.Body);
				Window->overview_page()->close_description_editor();
			});
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
			settle(Lanes);
			// The window outlives nothing it was lent: its gestures reach nothing now.
			Window->set_local_draft_observer({ });
			Window->tour_editor()->set_dispatch({ }, { });
			Window->tour_editor()->set_tour_ask_dispatch({ });
			Window->tour_editor()->set_writing_dispatch({ }, { });
			Window->tour_editor()->set_tour_session_dispatch({ }, { }, { });
			Window->overview_page()->set_description_save_dispatch({ });
		}

		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;
	};

	auto ring(qt::review_tour_editor& aTour, const std::size_t aIndex) -> core::tour_acceptance_state
	{
		const auto& shown = *aTour.shown().Draft;
		return core::decide_tour_stop_acceptance(shown, shown.Stops.at(aIndex)).State;
	}

	/// The open chat's turns and replies, as Copy chat gives them.
	auto chat_of(qt::review_tour_editor& aTour) -> QString { return aTour.side_chat()->chat_text(); }
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
		if (page != nullptr && !page->projection().Draft) {
			bpr::tests::end_to_end::print_local_draft_open(page->projection());
		}
		expect(page != nullptr && page->projection().Draft, "a local draft, signed out");
		draftId = page->projection().Draft->Id;
		// [[BPR-2196]]. The draft opens in Author; Review keeps the pull
		// request window's tray. RFC-BPR-0127: Notes is gone; the draft's
		// notes are its Comments.
		expect(window.current_modality() == qt::review_modality::author, "the draft opens in Author");
		window.choose_modality(qt::review_modality::review, core::workbench_modality_entry::choose);
		settle(s.Lanes);
		window.tray_tabs()->setCurrentIndex(0);
		expect(window.tray_tabs()->count() == 3 && window.tray_tabs()->isTabVisible(0) && window.tray_tabs()->accessibleTabName(0) == QStringLiteral("Comments")
				   && window.tray_tabs()->accessibleTabName(1) == QStringLiteral("Tour") && window.findChild<QWidget*>(QStringLiteral("localDraftNotesPage"))->isVisible()
				   && window.current_modality() == qt::review_modality::review,
			   "Review's tray holds Comments, the draft's notes, and Tour");
		auto* const tour = window.tour_editor();
		expect(tour->local_words(), "the Tour tab is the window's own Tour panel, showing the draft's tour");

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

		// AC01: the description in Overview's own editor.
		window.choose_modality(qt::review_modality::overview);
		settle(s.Lanes);
		auto* const overview = window.overview_page();
		expect(overview->description_editable() && overview->begin_description_edit(), "Overview offers the draft's description for editing");
		overview->description_editor()->setPlainText(QStringLiteral("<!-- team-template v3 -->\n## Summary\nCancel stops the search worker.\n- [x] I read the diff\n"));
		overview->description_save_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Description.Body.starts_with("<!-- team-template v3 -->"), "AC01: the description is saved");
		expect(overview->status_drawer()->status_page() == nullptr && !overview->status_drawer()->tabs()->isTabVisible(static_cast<int>(qt::overview_drawer_tab::status)),
			   "[[BPR-2196]]: a local draft has no Status tab until its pull request exists");
		capture(window, QStringLiteral("local-draft-overview-description"));

		// AC01, AC05: the Tour panel; [[BPR-2274]] a tour is only its stops
		// and never writes the description.
		window.choose_modality(qt::review_modality::review);
		window.tray_tabs()->setCurrentIndex(1);
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Description.Body.starts_with("<!-- team-template v3 -->"), "AC05: the tour never writes the description");

		// AC08: a stop on lines selected in Review; Save accepts it.
		expect(window.open_local_file(QStringLiteral("src/search.cpp")), "the file is in Files");
		settle(s.Lanes);
		expect(page->projection().File && page->projection().File->Path == "src/search.cpp", "the changed file is shown");
		select_new_lines(window, 6U, 10U);
		tour->show_stops();
		tour->trigger(qt::tour_editor_action::add_stop);
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Stops.size() == 1U, "AC01: a stop is added from the panel");
		expect(page->projection().Draft->Preparation.Tour.Stops[0].Source.Excerpt == "void cancel()\n{\n\tstop_worker();\n\tjoin_worker();\n}\n",
			   "AC01: it binds the candidate's exact bytes, no commit identity");
		expect(tour->head().Stops == 1U && tour->head().ToAccept == 1U && tour->head().Actions.empty(), "AC08: the head states the stop to accept, with no Publish");
		expect(ring(*tour, 0U) == core::tour_acceptance_state::not_accepted, "a new stop is yellow");
		const auto stopId = core::tour_stop_id{ page->projection().Draft->Preparation.Tour.Stops[0].Id };
		tour->go_to_stop(stopId);
		tour->begin_stop_edit();
		tour->stop_title()->setPlainText(QStringLiteral("Cancel stops and joins"));
		settle(s.Lanes);
		tour->stop_body()->setPlainText(QStringLiteral("Cancel now joins the worker, so no search outlives it."));
		settle(s.Lanes);
		expect(page->projection().Draft->Preparation.Tour.Stops[0].Title == "Cancel stops and joins" && !page->projection().Draft->Preparation.Tour.Stops[0].Accepted,
			   "typing saves the stop's text, and is not the sign-off");
		tour->save_stop_edit();
		settle(s.Lanes);
		expect(tour->head().ToAccept == 0U && ring(*tour, 0U) == core::tour_acceptance_state::accepted, "AC08: Save accepts the stop: the green ring");
		// Arriving at a stop reveals its lines in Review.
		static_cast<void>(window.comparison_surface()->clear_line_selection());
		tour->go_to_stop(stopId);
		settle(s.Lanes);
		const auto revealed = window.comparison_surface()->selected_source_range();
		expect(window.current_modality() == qt::review_modality::review && revealed && revealed->first.Line == 5U && revealed->second.Line == 9U,
			   "AC08: arrival reveals the stop's lines in Review's diff viewer");
		capture(window, QStringLiteral("local-draft-tour-arrival"));

		// AC03: a whole-change question in the panel's side chat. The request
		// carries the candidate; the live helper edit cannot reach it.
		write_file(worktree / "src/helper.cpp", "int helper()\n{\n\treturn LIVE_HELPER_EDIT;\n}\n");
		s.Answer = "The join can block if the worker waits on the UI.\n\n```bpr-proposals\n"
				   "[{\"target\":\"title\",\"text\":\"A cancelled search kept its worker running.\"},"
				   "{\"target\":\"title\",\"text\":\"Searches never stopped.\"},"
				   "{\"target\":\"new_stop\",\"path\":\"src/helper.cpp\",\"first\":1,\"last\":3,\"title\":\"Helper\",\"text\":\"The live helper.\"},"
				   "{\"target\":\"description\",\"text\":\"Cancel stops the worker and waits for it.\"}]\n```";
		// The agent is set up (the Agent tab's readiness, which this course has no setup for).
		tour->set_agent_readiness(qt::agent_setup_readiness::ready);
		tour->open_side_chat(core::tour_thread{ });
		settle(s.Lanes);
		tour->side_chat()->composer()->setPlainText(QStringLiteral("What could break?"));
		tour->side_chat()->send_button()->click();
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
		std::cout << "CHAT " << chat_of(*tour).toStdString() << '\n';
		expect(chat_of(*tour).contains(QStringLiteral("What could break?")) && chat_of(*tour).contains(QStringLiteral("The join can block")),
			   "AC08: the side chat shows the turn and its answer");
		// RFC-BPR-0130: a discussion keeps no sessions, so its header's Copy
		// conversation says what it has cost, where a session's row would.
		std::cout << "COST " << tour->side_chat()->copy_chat_button()->toolTip().toStdString() << '\n';
		expect(tour->side_chat()->copy_chat_button()->isVisibleTo(tour->side_chat()) && tour->side_chat()->copy_chat_button()->toolTip().contains(QStringLiteral("$0.02")), "AC08: the session's cost");
		expect(prep.Conversations[0].Handle == "session-1", "[[BPR-2138]] the session the agent kept is recorded with the conversation");
		{
			const auto keep = s.Answer;
			s.Answer        = "It waits for the worker.";
			tour->side_chat()->composer()->setPlainText(QStringLiteral("And the join?"));
			tour->side_chat()->send_button()->click();
			settle(s.Lanes);
			expect(s.Runs.size() == 2U && s.Runs[1].Handle == "session-1" && s.Prompts[1].find("stop_worker();") == std::string::npos && s.Prompts[1].find("And the join?") != std::string::npos,
				   "[[BPR-2138]] the follow-up resumes the native session instead of replaying the transcript");
			s.Answer = keep;
		}
		// The proposals wait; the stop outside the candidate is recorded and
		// never offered. [[BPR-2274]]: the title and description proposals
		// belong to Overview, not to the tour.
		expect(page->projection().Draft->Preparation.Proposals[2].State == core::local_proposal_state::outside_candidate, "AC03: a stop outside the candidate is recorded as such, never offered");
		capture(window, QStringLiteral("local-draft-tour-proposals"));
		// The owner, rejecting the local Overview on 2026-11-13: "the other
		// mode's drawers aren't being closed when going to overview." With
		// Review's Files drawer, its tray and the tour's chat open, Overview
		// shows none of them, and its own drawer lends nothing a draft lacks;
		// Review brings them back as they were.
		{
			auto* const files         = window.files_drawer();
			auto* const conversations = window.conversations_drawer();
			expect(files->isVisible() && conversations->isVisible() && tour->side_chat()->isVisible(), "Review shows its Files drawer, its tray and the tour's chat");
			window.choose_modality(qt::review_modality::overview);
			settle(s.Lanes);
			auto* const drawer = window.overview_page()->status_drawer();
			expect(window.current_modality() == qt::review_modality::overview && window.overview_page()->isVisible(), "Overview shows");
			expect(!files->isVisible() && !conversations->isVisible() && !tour->side_chat()->isVisible(), "Overview closes Review's drawers and the tour's chat");
			expect(!drawer->conversation_tabs_shown() && !drawer->tabs()->isTabVisible(static_cast<int>(qt::overview_drawer_tab::status)),
				   "Overview's own drawer has no Comments or Agent for a draft, and no Status until its pull request exists ([[BPR-2196]])");
			capture(window, QStringLiteral("local-draft-overview-drawers-closed"));
			window.choose_modality(qt::review_modality::review);
			settle(s.Lanes);
			expect(files->isVisible() && conversations->isVisible() && tour->side_chat()->isVisible(), "Review brings its drawers and the tour's chat back");
		}
		// AC02, AC05: the exact preview carries the description byte for byte
		// and the tour; nothing private.
		const auto preview = core::compose_local_publication(*page->projection().Draft);
		expect(preview.Body.starts_with("<!-- team-template v3 -->") && preview.Tour.find("src/search.cpp") != std::string::npos
				   && preview.Tour.find("The join can block") == std::string::npos && preview.Body.find("The join can block") == std::string::npos,
			   "AC02: the preview carries the description and tour; the conversation stays private");

		// AC03, AC08: an accepted refresh that changes the stop's lines.
		write_file(worktree / "src/search.cpp", "int search()\n{\n\treturn 2;\n}\n\nvoid cancel()\n{\n\tstop_worker(now);\n\tjoin_worker();\n}\n");
		page->close_button()->click();
		settle(s.Lanes);
		emit window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		page->refresh_button()->click();
		settle(s.Lanes);
		expect(core::latest_candidate(*page->projection().Draft)->Ordinal == 3U, "candidate 3");
		window.tray_tabs()->setCurrentIndex(1);
		settle(s.Lanes);
		const auto& moved = page->projection().Draft->Preparation.Tour.Stops[0];
		expect(core::decide_local_stop(moved).Reason == core::local_stop_reason::changed && moved.Source.Candidate == 2U, "AC08: the changed stop keeps its original material");
		expect(ring(*tour, 0U) == core::tour_acceptance_state::changed && tour->head().ToAccept == 1U, "AC08: its ring is yellow: Changed since accepted");
		tour->go_to_stop(stopId);
		settle(s.Lanes);
		std::cout << "REASON " << tour->accept_reason()->text().toStdString() << '\n';
		expect(tour->accept_reason()->text().startsWith(QStringLiteral("Changed since accepted: changed.")), "AC08: with the draft's own reason");
		// The agent is set up (the Agent tab's readiness, which this course has no setup for).
		tour->set_agent_readiness(qt::agent_setup_readiness::ready);
		tour->open_side_chat(core::tour_thread{ });
		tour->side_chat()->composer()->setPlainText(QStringLiteral("Anything else?"));
		s.Answer = "No.";
		tour->side_chat()->send_button()->click();
		settle(s.Lanes);
		const auto& conversations = page->projection().Draft->Preparation.Conversations;
		expect(conversations.size() == 2U && conversations[1].Candidate == 3U && conversations[0].Turns.size() == 4U, "AC03: a new session on the successor; the earlier stays");
		expect(s.Runs.back().Handle.empty() && s.Prompts.back().find("What could break?") == std::string::npos, "the new session replays nothing from the earlier candidate and resumes nothing");
		expect(chat_of(*tour).contains(QStringLiteral("What could break?")) && chat_of(*tour).contains(QStringLiteral("Anything else?")),
			   "the earlier candidate's turns stay readable above the new session's");
		capture(window, QStringLiteral("local-draft-tour-refreshed"));
	}
	{
		// AC04: everything resumes after a restart with no agent process and
		// no account.
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		emit  window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page->projection().Draft && page->projection().Draft->Id == draftId && window.current_modality() == qt::review_modality::author, "[[BPR-2196]]: a resumed draft opens in Author");
		const auto& prep = page->projection().Draft->Preparation;
		expect(prep.Description.Body.starts_with("<!-- team-template v3 -->") && prep.Tour.Stops.size() == 1U && prep.Conversations.size() == 2U && prep.Proposals.size() == 4U,
			   "AC04: description, tour, conversations and proposals resume");
		window.tray_tabs()->setCurrentIndex(1);
		settle(s.Lanes);
		auto* const tour = window.tour_editor();
		// The agent is set up (the Agent tab's readiness, which this course has no setup for).
		tour->set_agent_readiness(qt::agent_setup_readiness::ready);
		tour->open_side_chat(core::tour_thread{ });
		settle(s.Lanes);
		expect(tour->shown().Draft && tour->shown().Draft->Stops.size() == 1U && chat_of(*tour).contains(QStringLiteral("Anything else?")),
			   "AC04: the panel and the open conversation resume without a process");
		capture(window, QStringLiteral("local-draft-tour-restarted"));
	}
	std::cout << "local-draft-tour-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-tour-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
