#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/local_draft_workflow_area.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/// \file
/// \brief RFC-BPR-0112 ([[BPR-2032]], [[BPR-2033]], [[BPR-2034]]) through the
/// real workbench window, the real local-draft composition and the profile's
/// real stores, on a real Git worktree, with the synthetic organization
/// companion (`tests/fixtures/workflow_companion`) run as the author's own
/// terminal would run it and a scripted pull request catalog for GitHub.
/// With no workflow the draft prepares as before; importing the
/// organization's configuration runs nothing, keeps builds off and leaves an
/// unrelated draft alone; selecting it is explicit; the companion's context,
/// question, proposal and check come back as reported results; a review
/// marker only the upstream tool writes, and a changed upstream rule, leave
/// the required check unmet and its publisher unhanded; the accepted plan is
/// handed to the one chosen publisher, a lost answer is reconciled by reading
/// GitHub, and a failed follow-up keeps its results and is retried only
/// after reconciliation, without making anything twice.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::write_file;

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

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 8; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	void await(application::manual_executor& aLanes, const std::function<bool()>& aDone, const std::string& aWhat)
	{
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{ 30 };
		while (!aDone()) {
			expect(std::chrono::steady_clock::now() < until, aWhat);
			settle(aLanes);
			std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
		}
	}

	void commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	/// The author's terminal: runs exactly the argument array the Workflow area names.
	void run_as_author(const std::vector<std::string>& aCommand)
	{
		expect(!aCommand.empty(), "a command is named");
		auto arguments = QStringList{ };
		for (std::size_t index = 1U; index < aCommand.size(); ++index) {
			arguments << QString::fromStdString(aCommand[index]);
		}
		auto process = QProcess{ };
		process.start(QString::fromStdString(aCommand.front()), arguments);
		expect(process.waitForFinished(30'000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
			   "the companion ran: " + process.readAllStandardError().toStdString());
	}

	[[nodiscard]] auto action_view(const qt::local_draft_parts& aPage, const std::string& aId) -> const application::local_draft_workflow_action_view&
	{
		for (const auto& view : aPage.projection().Workflow.Actions) {
			if (view.Action.Id == aId) {
				return view;
			}
		}
		throw std::runtime_error{ "no action " + aId };
	}

	[[nodiscard]] auto button(qt::local_draft_workflow_area& aArea, const std::string& aAction, const char* aName) -> QPushButton*
	{
		auto* const group = aArea.actions()->findChild<QWidget*>(QStringLiteral("localDraftWorkflowAction_") + QString::fromStdString(aAction));
		expect(group != nullptr, "the action's group: " + aAction);
		auto* const found = group->findChild<QPushButton*>(QString::fromLatin1(aName));
		expect(found != nullptr, std::string{ "the button " } + aName + " on " + aAction);
		return found;
	}

	void touch(const std::filesystem::path& aPath, const std::string& aText = { })
	{
		auto file = std::ofstream{ aPath };
		file << aText;
	}

	/// GitHub as the course scripts it: branch heads and pull requests in memory.
	class scripted_github final : public application::github_publication_port
	{
	public:
		std::mutex                                                Mutex;
		std::map<std::string, core::git_commit_oid>               Branches;
		std::vector<application::github_pull_request_observation> Pulls;
		std::atomic<int>                                          Creates{ 0 };
		std::atomic<bool>                                         LoseRequest{ false };
		std::atomic<bool>                                         Hold{ false };
		std::atomic<bool>                                         Holding{ false };

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token) -> application::github_publication_result<application::github_publication_capability> override
		{
			return application::github_publication_capability{ .Owner = aTarget.Owner, .Repository = aTarget.Repository, .CanPush = true, .DefaultBranch = "main" };
		}
		auto observe_branch(const application::github_publication_target&, const std::string& aBranch, std::stop_token)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			const auto lock  = std::scoped_lock{ Mutex };
			const auto found = Branches.find(aBranch);
			return found == Branches.end() ? std::nullopt : std::optional{ found->second };
		}
		auto observe_commit(const application::github_publication_target&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_tree(const application::github_publication_target&, const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_blob(const application::github_publication_target&, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t>, const core::git_entry_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_tree(const application::github_publication_target&, const core::git_tree_oid&, const std::vector<core::git_tree_entry>&, const std::vector<std::string>&,
						 const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_commit(const application::github_publication_target&, const core::git_commit_content&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_branch(const application::github_publication_target&, const std::string&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			++Creates;
			if (LoseRequest.exchange(false)) {
				return std::unexpected{ application::github_publication_error{ .Transmission = application::github_publication_transmission::indeterminate } };
			}
			Holding = true;
			while (Hold.load()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
			}
			Holding         = false;
			const auto lock = std::scoped_lock{ Mutex };
			auto       pull = application::github_pull_request_observation{ .Number         = 7U,
																			.NodeId         = "PR_7",
																			.Url            = "https://github.com/acme/engine/pull/7",
																			.State          = "open",
																			.Draft          = aDraft.Draft,
																			.Title          = aDraft.Title,
																			.HeadRef        = aDraft.Head,
																			.HeadOwner      = aTarget.Owner,
																			.HeadRepository = aTarget.Repository,
																			.HeadCommit     = Branches.at(aDraft.Head),
																			.BaseRef        = aDraft.Base,
																			.Author         = aTarget.Login };
			Pulls.push_back(pull);
			return pull;
		}
		auto find_pull_requests(const application::github_publication_target&, const std::string& aHead, const std::string& aBase, std::stop_token)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			const auto lock = std::scoped_lock{ Mutex };
			auto       out  = std::vector<application::github_pull_request_observation>{ };
			for (const auto& pull : Pulls) {
				if (pull.HeadRef == aHead && pull.BaseRef == aBase) {
					out.push_back(pull);
				}
			}
			return out;
		}
		auto observe_pull_request(const application::github_publication_target&, const std::uint64_t, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			return std::optional<application::github_pull_request_observation>{ };
		}
	};
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "local-draft-workflow-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	const auto fixture       = std::filesystem::canonical(std::filesystem::path{ aArgv[4] });
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-workflow-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-workflow-worktrees" };
	const auto root          = std::filesystem::canonical(scratchRoot.path());
	// The companion as installed on this computer: its own copy, whose
	// upstream folder the course plays.
	const auto installed = root / "companion";
	std::filesystem::copy(fixture, installed, std::filesystem::copy_options::recursive);
	const auto companion = installed / "synthetic_org_companion.py";
	const auto upstream  = installed / "upstream";
	const auto ran       = [&upstream] { return std::filesystem::exists(upstream / "published-plans.jsonl"); };

	const auto seed   = root / "seed";
	const auto hosted = root / "hosted.git";
	std::filesystem::create_directories(seed);
	(*git)(seed, { "init", "--quiet" });
	commit(*git, seed, "README.md", "engine\n", "initial");
	(*git)(root, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
	const auto clone = [&](const std::filesystem::path& aWhere, const std::string& aBranch) {
		(*git)(root, { "clone", "--quiet", hosted.string(), aWhere.string() });
		(*git)(aWhere, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
		(*git)(aWhere, { "checkout", "--quiet", "-b", aBranch });
		commit(*git, aWhere, "src/search.cpp", "int search() { return 2; }\n", "search");
	};
	const auto worktree  = root / "engine";
	const auto unrelated = root / "engine-other";
	clone(worktree, "feature/cancel");
	clone(unrelated, "feature/other");

	auto lanes  = application::manual_executor{ };
	auto window = std::make_unique<qt::workbench_window>();
	window->resize(1280, 900);
	window->show();
	auto cycles  = std::make_unique<app::review_cycle_desktop>(*window, profileRoot.path(), core::bpr_profile_id{ 1 }, lanes, lanes, app::review_cycle_syntax{ }, &lanes);
	auto drafts  = std::make_unique<app::local_draft_desktop>(*window, profileRoot.path(), gitExecutable, std::nullopt, lanes, lanes, app::local_draft_syntax{ }, &lanes);
	auto github  = std::make_shared<scripted_github>();
	auto copied  = std::string{ };
	auto chooser = worktree;
	drafts->set_folder_chooser([&chooser] { return QString::fromStdString(chooser.string()); });
	drafts->set_copier([&copied](const std::string& aText) { copied = aText; });
	drafts->set_publishing([github]() -> std::optional<application::github_publication_account> {
		return application::github_publication_account{ .Provider = github, .Account = core::github_account_id{ 7U }, .Login = "author", .Host = "github.com" };
	});
	settle(lanes);

	// AC01: no workflow anywhere; the unrelated draft prepares as before.
	chooser = unrelated;
	window->home_prepare_button()->click();
	settle(lanes);
	auto* const page = window->local_draft();
	expect(page != nullptr && page->projection().Draft && !page->projection().Draft->Workflow, "a draft opens with no workflow");
	auto* const area = page->workflow();
	expect(area->summary()->text().contains(QStringLiteral("No workflow is set up")) && area->chooser()->count() == 1, "AC01: nothing to select, and nothing needed");
	drafts->host().save_description("Other change", "Searches return two.\n");
	drafts->host().add_stop("src/search.cpp", 1U, 1U, "Search returns two", "The answer is two now.");
	settle(lanes);
	const auto other = page->projection().Draft->Id;
	expect(page->projection().Draft->Preparation.Description.Body == "Searches return two.\n" && page->projection().Draft->Preparation.Tour.Stops.size() == 1U,
		   "AC01: Description and Tour prepare with no extension");

	// AC02: the organization's configuration, imported through the profile's
	// own stores: nothing runs, builds stay off, the program is bound here.
	// The recipient has finished agent setup, with no agent added yet.
	expect(adapters::sqlite::make_agent_setup_store(profileRoot.path())->store(application::agent_setup_preferences{ .AgentsMoved = true }), "the recipient's setup");
	auto store  = adapters::sqlite::make_agent_configuration_store(profileRoot.path());
	auto before = store->load();
	expect(before.has_value(), "the profile's settings");
	auto bytes = std::string{ };
	{
		auto file = std::ifstream{ fixture / "synthetic-org.bpr-config.json", std::ios::binary };
		bytes.assign(std::istreambuf_iterator<char>{ file }, { });
	}
	auto preview    = application::agent_configuration_preview{ .Snapshot = *before };
	preview.Prompts = application::local_prompts(preview.Snapshot);
	preview.File    = core::decode_agent_configuration(bytes, preview.Prompts, application::prompt_aliases()).value();
	expect(preview.File.Workflows.size() == 1U && !preview.File.Workflows[0].Problem, "the shared workflow reads");
	const auto agents   = std::vector<core::agent_import_choice>{ { .Item = 0U, .Action = core::agent_import_action::add } };
	auto       programs = std::vector<core::workflow_program>{ };
	for (const auto* id : { "context", "template", "publish", "follow" }) {
		programs.push_back({ .Action = id, .Program = companion.string() });
	}
	auto workflowChoice      = std::vector<core::workflow_import_choice>{ { .Item = 0U, .Action = core::workflow_import_action::add, .Programs = programs } };
	workflowChoice[0].Agents = application::default_workflow_agents(preview, agents, 0U);
	auto applied             = core::apply_agent_configuration(preview.Snapshot.Agents.Agents, preview.Prompts, preview.File, agents, { }).value();
	auto after               = application::snapshot_after_import(preview.Snapshot, applied, applied.Replaced, [] { return std::string(32U, 'e'); });
	after.Workflows          = core::apply_workflow_choices(preview.Snapshot.Workflows, preview.File.Workflows, workflowChoice).value();
	expect(store->apply(*before, after) == application::agent_configuration_outcome::applied, "the import is stored");
	const auto stored = store->load().value();
	std::cout << "STORED workflows=" << stored.Workflows.size() << " agents=" << stored.Agents.Agents.size() << " builds=" << (stored.Agents.Agents.empty() ? -1 : int{ stored.Agents.Agents[0].BuildsAndTests }) << '\n';
	expect(stored.Workflows.size() == 1U && stored.Agents.Agents.size() == 1U && !stored.Agents.Agents[0].BuildsAndTests, "AC09: the file asks for builds; none are on without acceptance");
	expect(core::bound_agent(stored.Workflows[0], "describe")->AgentName == "Writer", "the agent action is bound to the imported agent");
	drafts->reload_workflows();
	settle(lanes);
	expect(!ran() && !std::filesystem::exists(upstream / "SELF_REVIEWED"), "AC02: importing ran nothing");
	expect(page->projection().Draft->Id == other && !page->projection().Draft->Workflow && area->chooser()->count() == 2, "AC01: the open, unrelated draft is unchanged; the workflow is only offered");

	// The draft that uses it: selected explicitly, and selecting runs nothing.
	chooser = worktree;
	window->home_prepare_button()->click();
	settle(lanes);
	expect(page->projection().Draft && page->projection().Draft->Id != other, "the second worktree's draft");
	drafts->host().save_description("Cancel searches promptly", "Searches kept running.\n");
	settle(lanes);
	area->chooser()->setCurrentIndex(1);
	emit area->chooser()->activated(1);
	settle(lanes);
	expect(page->projection().Draft->Workflow && page->projection().Draft->Workflow->Workflow.PortableId == "synthetic-org" && !ran(), "AC02: selected explicitly; nothing ran");
	expect(area->summary()->text().contains(QStringLiteral("From Synthetic Org.")) && area->summary()->text().contains(QStringLiteral("Required check “Check the pull request template”: Not run")),
		   "AC07: origin and the unmet required check are shown");

	// AC09: an agent action wears the agent accent; a program's does not.
	auto* const ask     = button(*area, "describe", "localDraftWorkflowAsk");
	auto* const handOff = button(*area, "context", "localDraftWorkflowHandOff");
	expect(qt::agent_run_control(*ask) && !qt::agent_run_control(*handOff), "AC09: the accent marks only the agent's action");
	std::cout << "SCOPE " << area->text().toStdString() << '\n';
	expect(area->text().contains(QStringLiteral("BetterPullReview does not start or restrict it")) && area->text().contains(QStringLiteral("read-only writing lane")),
		   "AC05, AC07: who acts and the effect scope, never a declaration called isolation");
	window->show_local_publication();
	settle(lanes);
	expect(area->isVisible(), "the Workflow area is in Overview's Status tab, above the publish steps");
	capture(*window, QStringLiteral("workflow-selected"));

	// AC03: context asks the author something; the check runs meanwhile.
	handOff->click();
	settle(lanes);
	const auto first = action_view(*page, "context");
	expect(first.Record && first.Record->State == core::workflow_action_state::handed_off && !first.Command.empty() && first.Command.front() == companion.string() && !ran(),
		   "handed off: BPR names the command and starts nothing");
	expect(std::filesystem::exists(first.RequestPath) && first.CommandLine.find("Searches kept running") == std::string::npos, "the request is a file; no draft text reaches the command");
	expect(area->text().contains(QStringLiteral("Sent candidate 1: the title, the description")), "AC07: the selected context sent is shown");
	run_as_author(first.Command);
	button(*area, "context", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(action_view(*page, "context").Record->State == core::workflow_action_state::awaiting_input && area->text().contains(QStringLiteral("Which ticket does this change close?")),
		   "AC03: the companion asks the author");
	button(*area, "template", "localDraftWorkflowHandOff")->click();
	settle(lanes);
	const auto checkInvocation = action_view(*page, "template").Record->Invocation;
	run_as_author(action_view(*page, "template").Command);
	button(*area, "template", "localDraftWorkflowRead")->click();
	settle(lanes);
	const auto marker = action_view(*page, "template");
	window->show_local_publication();
	settle(lanes);
	capture(*window, QStringLiteral("workflow-check-unmet"));
	expect(marker.Record->State == core::workflow_action_state::reported && !marker.Record->Limitations.empty()
			   && marker.Record->Limitations[0].Kind == core::workflow_limitation_kind::review_marker && page->projection().Workflow.Checks[0].Gap == core::check_gap::limitation,
		   "AC04: the review marker only the upstream tool writes leaves the check unmet");

	// The author answers; only the asking action runs again.
	auto* const answer = area->actions()->findChild<QLineEdit*>(QStringLiteral("localDraftWorkflowAnswer"));
	expect(answer != nullptr, "the answer field");
	answer->setText(QStringLiteral("ENG-42"));
	button(*area, "context", "localDraftWorkflowContinue")->click();
	settle(lanes);
	expect(action_view(*page, "template").Record->Invocation == checkInvocation, "AC03: continuing reran no other action");
	run_as_author(action_view(*page, "context").Command);
	// The author edits the description while the companion's proposal is out.
	button(*area, "context", "localDraftWorkflowRead")->click();
	settle(lanes);
	const auto& proposals = page->projection().Draft->Preparation.Proposals;
	expect(!proposals.empty() && proposals.back().Producer == "Synthetic Org pull request: Gather the ticket's context" && proposals.back().Text.find("Closes ENG-42") != std::string::npos,
		   "AC03: the companion's proposal is recorded, labelled with its origin");
	const auto proposal = proposals.back().Id;
	drafts->host().decide_proposal(proposal, true);
	settle(lanes);
	expect(page->projection().Draft->Preparation.Description.Body.find("## Why") != std::string::npos, "the accepted proposal is the description");

	// AC03, AC04: rerun the stale-marker check: still unmet, until the
	// upstream tool's own self-review writes its marker.
	touch(upstream / "SELF_REVIEWED");
	button(*area, "template", "localDraftWorkflowHandOff")->click();
	settle(lanes);
	run_as_author(action_view(*page, "template").Command);
	button(*area, "template", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(!page->projection().Workflow.Checks[0].Gap, "the repeated check is met, as reported");
	// A changed upstream rule, seen by a later answer, needs the check again.
	touch(upstream / "RULES.json", R"({"name":"synthetic-org-create-pr","required_sections":["## Why","## Testing"],"self_review_marker":"required","creation_dry_run":false,"ticket_prefix":"ENG"})");
	handOff->click();
	settle(lanes);
	run_as_author(action_view(*page, "context").Command);
	button(*area, "context", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(page->projection().Workflow.Checks[0].Gap == core::check_gap::upstream_changed, "AC04: a changed upstream rule is an honest unmet requirement");
	button(*area, "template", "localDraftWorkflowHandOff")->click();
	settle(lanes);
	run_as_author(action_view(*page, "template").Command);
	button(*area, "template", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(!page->projection().Workflow.Checks[0].Gap, "checked again against the new rules");

	// AC05: one publisher. The author chooses the organization's; the
	// built-in route stands aside and the exact accepted plan is handed off.
	{
		const auto lock = std::scoped_lock{ github->Mutex };
		github->Branches.emplace("feature/cancel", page->projection().Draft->Candidates.back().Head);
	}
	auto* const publisher = area->findChild<QComboBox*>(QStringLiteral("localDraftWorkflowPublisher"));
	publisher->setCurrentIndex(1);
	emit publisher->activated(1);
	settle(lanes);
	drafts->host().check_publication(false);
	await(lanes, [&] { return page->projection().Publishing.Checked && !page->projection().Publishing.Checking && !page->projection().Publishing.Digest.empty(); }, "the plan");
	const auto digest = page->projection().Publishing.Digest;
	drafts->host().create_pull_request(digest);
	await(lanes, [&] { return !page->projection().Publishing.Creating; }, "the built-in route answers");
	expect(github->Creates == 0 && page->projection().Publishing.Refusal == core::local_publication_refusal::external_publisher, "AC05: the built-in publisher stands aside");
	touch(upstream / "LOSE_ANSWER");
	button(*area, "publish", "localDraftWorkflowPublish")->click();
	settle(lanes);
	run_as_author(action_view(*page, "publish").Command);
	{
		auto log  = std::ifstream{ upstream / "published-plans.jsonl" };
		auto line = std::string{ };
		std::getline(log, line);
		std::cout << "PLAN " << line << '\n';
		expect(line.find("\"digest\": \"" + digest + "\"") != std::string::npos && line.find("\"title\": \"Cancel searches promptly\"") != std::string::npos
				   && line.find("Closes ENG-42") != std::string::npos,
			   "AC05: the publisher received the exact accepted plan");
	}
	// AC06: the answer was lost; nothing is handed off again before GitHub is read.
	button(*area, "publish", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(area->summary()->text().contains(QStringLiteral("No answer yet")), "no answer to read");
	button(*area, "publish", "localDraftWorkflowLost")->click();
	settle(lanes);
	drafts->host().hand_off_publication("publish", digest);
	settle(lanes);
	expect(page->projection().Workflow.Refusal == core::workflow_refusal::external_open, "AC06: no replay before reconciliation");
	button(*area, "publish", "localDraftWorkflowReconcile")->click();
	await(lanes, [&] { return !page->projection().Publishing.Creating; }, "GitHub is read");
	expect(!core::external_publication_open(*page->projection().Draft), "AC06: GitHub shows none; it may be handed off again");
	touch(upstream / "CREATED_PULL_REQUEST", "9");
	button(*area, "publish", "localDraftWorkflowPublish")->click();
	settle(lanes);
	run_as_author(action_view(*page, "publish").Command);
	{
		// The organization's tool created it on GitHub.
		const auto lock = std::scoped_lock{ github->Mutex };
		github->Pulls.push_back({ .Number = 9U, .NodeId = "PR_9", .Url = "https://github.com/acme/engine/pull/9", .State = "open", .Draft = true, .Title = "Cancel searches promptly", .HeadRef = "feature/cancel", .HeadOwner = "acme", .HeadRepository = "engine", .HeadCommit = github->Branches.at("feature/cancel"), .BaseRef = "main", .Author = "author" });
	}
	button(*area, "publish", "localDraftWorkflowRead")->click();
	settle(lanes);
	expect(!page->projection().Draft->Publication->Journal.Link && area->text().contains(QStringLiteral("not observed by BetterPullReview")), "AC09: a report alone links nothing and is not called observed");
	button(*area, "publish", "localDraftWorkflowReconcile")->click();
	await(lanes, [&] { return !page->projection().Publishing.Creating; }, "GitHub is read again");
	const auto& journal = page->projection().Draft->Publication->Journal;
	expect(journal.Phase == core::local_publication_phase::linked && journal.Link->Number == 9U && journal.Link->Adopted && github->Creates == 0,
		   "AC05, AC06: linked after reading GitHub, as one BPR did not create");

	// AC06: a follow-up fails partway; its results stay, and it is retried
	// only after reconciliation, carrying what was already made.
	button(*area, "follow", "localDraftWorkflowFollowUp")->click();
	settle(lanes);
	run_as_author(action_view(*page, "follow").Command);
	button(*area, "follow", "localDraftWorkflowRead")->click();
	settle(lanes);
	const auto failed = action_view(*page, "follow");
	expect(failed.Record->References.size() == 2U && failed.Record->References[1].State == core::workflow_reference_state::failed && journal.Phase == core::local_publication_phase::linked,
		   "AC06: the ticket was updated, the upload failed, the pull request stands");
	drafts->host().hand_off_follow_up("follow");
	settle(lanes);
	expect(page->projection().Workflow.Refusal == core::workflow_refusal::state, "AC06: no retry before reconciliation");
	button(*area, "follow", "localDraftWorkflowReconcileFollowUp")->click();
	settle(lanes);
	touch(upstream / "UPLOAD_OK");
	button(*area, "follow", "localDraftWorkflowFollowUp")->click();
	settle(lanes);
	run_as_author(action_view(*page, "follow").Command);
	button(*area, "follow", "localDraftWorkflowRead")->click();
	settle(lanes);
	const auto retried = action_view(*page, "follow");
	auto       tickets = 0;
	for (const auto& reference : retried.Record->References) {
		tickets += reference.Kind == core::workflow_reference_kind::ticket ? 1 : 0;
	}
	expect(tickets == 1 && retried.Record->References[1].State == core::workflow_reference_state::created, "AC06: the retry made only what was missing");
	window->show_local_publication();
	settle(lanes);
	capture(*window, QStringLiteral("workflow-follow-up"));

	// The unrelated draft never saw any of it.
	{
		auto       drafts_store   = adapters::sqlite::open_local_draft_store(profileRoot.path());
		const auto unrelatedDraft = drafts_store->find(other).value();
		expect(!unrelatedDraft.Workflow && unrelatedDraft.Preparation.Description.Body == "Searches return two.\n", "AC01: the unrelated draft does not depend on the workflow");
	}

	drafts->shutdown();
	cycles->shutdown();
	settle(lanes);
	std::cout << "local-draft-workflow-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-workflow-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
