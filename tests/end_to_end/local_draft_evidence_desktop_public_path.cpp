#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/local_draft_evidence.hpp"
#include "bpr/qt/local_draft_evidence_area.hpp"
#include "bpr/qt/local_draft_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0109 ([[BPR-2023]], [[BPR-2024]]) through the real
/// workbench window and the real local-draft composition, on a real Git
/// worktree, with no agent, account or network: the Evidence area's claim,
/// check, result and question; an import chosen in the window whose claims,
/// checks and questions arrive proposed and are accepted or rejected there;
/// a refused import that applies nothing; a kept artifact read back and a
/// link handed to the browser only as an address; currency across a
/// text-only candidate; the selection for later use; and a restart. No row
/// says verified or observed. Renders are written for the owner's look.

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
		commit(aGit, seed, "src/engine.cpp", "int engine()\n{\n\treturn 1;\n}\n", "engine");
		aGit(aRoot, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
		aGit(aRoot, { "clone", "--quiet", hosted.string(), primary.string() });
		aGit(primary, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
		aGit(primary, { "checkout", "--quiet", "-b", "feature/search-order" });
		commit(aGit, primary, "src/search.cpp", "int search()\n{\n\treturn 2;\n}\n", "search");
		commit(aGit, primary, "src/engine.cpp", "int engine()\n{\n\treturn search() + 1;\n}\n", "engine uses search");
		return primary;
	}

	/// Every row's text and status, for words no row may use.
	auto all_rows(QTreeWidget& aTree) -> QString
	{
		auto text = QString{ };
		for (auto it = QTreeWidgetItemIterator{ &aTree }; *it != nullptr; ++it) {
			text += (*it)->text(0) + QLatin1Char{ '|' } + (*it)->text(1) + QLatin1Char{ '\n' };
		}
		return text;
	}

	struct session
	{
		application::manual_executor               Lanes;
		std::unique_ptr<qt::workbench_window>      Window;
		std::unique_ptr<app::review_cycle_desktop> Cycles;
		std::unique_ptr<app::local_draft_desktop>  Drafts;
		QString                                    Document;
		std::vector<std::string>                   Opened;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 800);
			Window->show();
			Cycles = std::make_unique<app::review_cycle_desktop>(*Window, aProfile, core::bpr_profile_id{ 1 }, Lanes, Lanes, app::review_cycle_syntax{ }, &Lanes);
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, std::nullopt, Lanes, Lanes);
			Drafts->set_folder_chooser([aChosen] { return aChosen; });
			Drafts->set_evidence_chooser([this] { return Document; });
			Drafts->set_link_opener([this](const std::string_view aAddress) {
				Opened.emplace_back(aAddress);
				return true;
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

	auto import_document(const std::string& aHead) -> std::string
	{
		return std::string{ R"({"format":"bpr.evidence-import","version":1,"producer":{"kind":"tool","name":"order-bridge","version":"0.4"},)" }
		+ R"("claims":[{"key":"c1","kind":"bug_fix","text":"Equal scores keep their insertion order."}],)"
			+ R"("checks":[{"key":"k1","method":"model_check","title":"order.tla","command":"tlc order.tla","claims":["c1"],)"
			+ R"("formal":{"property":"StableOrder","subject":"order.tla model of the ranking","tool":"TLC 2.18","bounds":"4 results, 3 equal scores"}}],)"
			+ R"("results":[{"check":"k1","outcome":"pass","class":"reported","material":{"head":")" + aHead
			+ R"("},"artifacts":[{"name":"TLC output","path":"tlc.txt"},{"name":"CI run","url":"https://ci.example/runs/42"}]}],)"
			+ R"("questions":[{"text":"Should ties break by name instead?","claim":"c1"},{"text":"A note to self, not for reviewers."}]})";
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "local-draft-evidence-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-evidence-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-evidence-worktrees" };
	const auto scratch       = std::filesystem::canonical(scratchRoot.path());
	const auto worktree      = make_worktree(*git, scratch);
	const auto chosen        = QString::fromStdString(worktree.string());
	const auto head          = git->output(worktree, { "rev-parse", "HEAD" }, scratch / "head.txt");
	const auto imports       = scratch / "from-tools";
	std::filesystem::create_directories(imports);
	write_file(imports / "tlc.txt", "Model checking completed. No error has been found.\n");
	const auto document = import_document(head);
	write_file(imports / "order.json", document);
	write_file(imports / "observed.json", std::string{ document }.replace(document.find(R"("class":"reported")"), 18U, R"("class":"observed")"));
	auto draftId = core::local_draft_id{ };
	{
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		window.home_prepare_button()->click();
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page != nullptr && page->projection().Draft, "a local draft");
		draftId = page->projection().Draft->Id;
		expect(page->areas()->count() == 2 && page->areas()->currentIndex() == 0, "the page opens on Changes; Evidence is beside it");
		page->areas()->setCurrentIndex(1);
		settle(s.Lanes);
		auto* const evidence = page->evidence();
		expect(evidence->isVisible() && evidence->tree()->topLevelItemCount() == 0, "AC01: Evidence starts empty");

		// AC01: claim, check, result and question entered by the author, no agent.
		auto forms  = qt::local_draft_evidence_forms{ };
		forms.Claim = [](const application::local_draft_workbench_projection&) {
			return std::optional{ qt::local_draft_evidence_forms::claim{ .Kind = core::claim_kind::preserved_behavior, .Text = "Search results keep their order." } };
		};
		forms.Revision = [](const core::evidence_claim&) { return std::optional{ std::string{ "Search results keep their order across pages." } }; };
		forms.Check    = [](const application::local_draft_workbench_projection& aView) {
			return std::optional{ qt::local_draft_evidence_forms::check{
				.Method = core::check_method::test, .Title = "search_order_test", .Command = "ctest -R search_order", .Claims = { aView.Draft->Evidence.Claims.front().Id } } };
		};
		forms.Result = [](const application::local_draft_workbench_projection& aView) {
			return std::optional{ core::manual_result{ .Check       = aView.Draft->Evidence.Checks.front().Id,
													   .Outcome     = core::evidence_outcome::pass,
													   .Candidate   = aView.Shown,
													   .Inputs      = { "src/" },
													   .Environment = "macOS 26, Debug" } };
		};
		forms.Question = [](const application::local_draft_workbench_projection&) {
			return std::optional{ qt::local_draft_evidence_forms::question{ .Text = "Is page size part of the order?" } };
		};
		evidence->set_forms(forms);
		expect(!evidence->record_result_button()->isEnabled(), "no result before a check");
		evidence->add_claim_button()->click();
		settle(s.Lanes);
		evidence->add_check_button()->click();
		settle(s.Lanes);
		evidence->record_result_button()->click();
		settle(s.Lanes);
		evidence->add_question_button()->click();
		settle(s.Lanes);
		const auto& entered = page->projection().Draft->Evidence;
		expect(entered.Claims.size() == 1U && entered.Checks.size() == 1U && entered.Results.size() == 1U && entered.Questions.size() == 1U, "AC01: the four records are kept");
		const auto claimId  = entered.Claims[0].Id;
		auto*      claimRow = evidence->row_of(claimId);
		expect(claimRow != nullptr && claimRow->text(1) == QStringLiteral("1 passing"), "AC02: the claim counts its reported pass; it has no verified state");
		auto* const resultRow = evidence->row_of(entered.Results[0].Id);
		expect(resultRow->text(0) == QStringLiteral("Pass, reported by you") && resultRow->text(1) == QStringLiteral("This candidate"), "AC02, AC03: reported, on this candidate");
		// Revise the claim: the earlier wording stays in its history.
		evidence->tree()->setCurrentItem(claimRow);
		const auto revision = forms.Revision(entered.Claims[0]);
		page->evidence()->set_forms(forms);
		s.Drafts->host().revise_claim(claimId, *revision);
		settle(s.Lanes);
		expect(page->projection().Draft->Evidence.Claims[0].Earlier.size() == 1U, "AC01: a revision keeps the earlier wording");
		evidence->tree()->setCurrentItem(evidence->row_of(claimId));
		expect(evidence->detail()->toPlainText().contains(QStringLiteral("Earlier: Search results keep their order.")), "AC01: the history is readable");
		capture(window, QStringLiteral("local-draft-evidence-entered"));

		// AC05: a refused import applies nothing.
		const auto before = page->projection().Draft->Evidence;
		s.Document        = QString::fromStdString((imports / "observed.json").string());
		evidence->import_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Evidence == before && evidence->status()->text().startsWith(QStringLiteral("Import refused, nothing applied"))
				   && evidence->status()->text().contains(QStringLiteral("reported")),
			   "AC02, AC05: an import that says observed is refused whole, with its reason");

		// AC06: an import's claim, check and questions arrive proposed.
		s.Document = QString::fromStdString((imports / "order.json").string());
		evidence->import_button()->click();
		settle(s.Lanes);
		const auto& imported = page->projection().Draft->Evidence;
		expect(imported.Claims.size() == 2U && imported.Results.size() == 2U && imported.Questions.size() == 3U, "AC01: the import is applied whole");
		expect(evidence->status()->text().startsWith(QStringLiteral("Imported 1 claim(s), 1 check(s), 1 reported result(s) (1 bound to a candidate)")), "the import is reported");
		const auto proposedClaim = imported.Claims[1].Id;
		const auto modelCheck    = imported.Checks[1].Id;
		const auto keepQuestion  = imported.Questions[1].Id;
		const auto dropQuestion  = imported.Questions[2].Id;
		expect(evidence->row_of(proposedClaim)->text(1) == QStringLiteral("Proposed") && evidence->row_of(modelCheck)->text(1) == QStringLiteral("Proposed"),
			   "AC06: imported claims and checks are proposed");
		expect(!all_rows(*evidence->tree()).contains(QStringLiteral("erified")) && !all_rows(*evidence->tree()).contains(QStringLiteral("bserved")), "AC02: no row says verified or observed");

		// AC04: the model check's scope is its model and bounds.
		evidence->tree()->setCurrentItem(evidence->row_of(imported.Results[1].Id));
		settle(s.Lanes);
		const auto detail = evidence->detail()->toPlainText();
		expect(detail.contains(QStringLiteral("Concerns the model it analyzed, within its bounds; not the code.")) && detail.contains(QStringLiteral("reported by order-bridge 0.4")),
			   "AC04: a model check is shown as concerning its model, reported by its producer");
		expect(evidence->artifacts()->count() == 2, "the result's artifacts are listed");
		capture(window, QStringLiteral("local-draft-evidence-imported"));

		// AC01: follow the kept artifact; the link goes out only as an address.
		emit evidence->artifacts()->itemActivated(evidence->artifacts()->item(0));
		settle(s.Lanes);
		auto* const reader = evidence->artifact_reader();
		expect(reader != nullptr && reader->findChild<QPlainTextEdit*>()->toPlainText().contains(QStringLiteral("No error has been found")), "AC01: the kept artifact is read back");
		capture(*reader, QStringLiteral("local-draft-evidence-artifact"));
		reader->reject();
		settle(s.Lanes);
		expect(!page->projection().Artifact, "closing the reader closes the artifact");
		emit evidence->artifacts()->itemActivated(evidence->artifacts()->item(1));
		settle(s.Lanes);
		expect(s.Opened == std::vector<std::string>{ "https://ci.example/runs/42" }, "a link is handed to the browser, never fetched");

		// AC06: accept the claim and the check, reject one question, keep the other.
		evidence->tree()->setCurrentItem(evidence->row_of(proposedClaim));
		expect(evidence->accept_button()->isEnabled() && evidence->reject_button()->isEnabled(), "a proposed record can be accepted or rejected");
		evidence->accept_button()->click();
		settle(s.Lanes);
		evidence->tree()->setCurrentItem(evidence->row_of(modelCheck));
		evidence->accept_button()->click();
		settle(s.Lanes);
		evidence->tree()->setCurrentItem(evidence->row_of(dropQuestion));
		evidence->reject_button()->click();
		settle(s.Lanes);
		evidence->tree()->setCurrentItem(evidence->row_of(keepQuestion));
		evidence->accept_button()->click();
		settle(s.Lanes);
		const auto& decided = page->projection().Draft->Evidence;
		expect(decided.Claims[1].Disposition == core::evidence_disposition::accepted && decided.Checks[1].Disposition == core::evidence_disposition::accepted
				   && decided.Questions[2].Disposition == core::evidence_disposition::rejected && decided.Questions[1].Disposition == core::evidence_disposition::accepted,
			   "AC06: accepted and rejected through the window");
		expect(evidence->row_of(dropQuestion)->text(1) == QStringLiteral("Rejected") && evidence->row_of(keepQuestion)->text(1) == QStringLiteral("Open"),
			   "AC06: the rejected question stays in the history; the accepted one is open");
		expect(evidence->row_of(proposedClaim)->text(1) == QStringLiteral("1 passing"), "an accepted check's reported pass counts; it does not verify the claim");

		// AC06: select for later use.
		for (const auto id : { proposedClaim, decided.Results[1].Id, keepQuestion }) {
			evidence->row_of(id)->setCheckState(2, Qt::Checked);
			settle(s.Lanes);
		}
		const auto selection = core::selected_evidence(*page->projection().Draft);
		expect(selection.Claims.size() == 1U && selection.Results.size() == 1U && selection.Questions.size() == 1U && selection.Checks.size() == 1U,
			   "AC06: only what was selected is offered to a description or tour");
		expect(evidence->row_of(dropQuestion)->flags().testFlag(Qt::ItemIsUserCheckable) == false, "a rejected record cannot be selected");
		capture(window, QStringLiteral("local-draft-evidence-decided"));

		// AC03: a text-only candidate keeps the scoped code result; the model
		// check, with no recorded inputs, needs a recheck.
		page->areas()->setCurrentIndex(0);
		write_file(worktree / "README.md", "engine, with ordered search\n");
		page->close_button()->click();
		settle(s.Lanes);
		emit window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		page->working()->item(0)->setCheckState(Qt::Checked);
		settle(s.Lanes);
		page->refresh_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Candidates.size() == 2U && page->projection().Shown == 2U, "candidate 2 is text-only");
		page->areas()->setCurrentIndex(1);
		settle(s.Lanes);
		const auto& current = page->projection().Draft->Evidence;
		expect(evidence->row_of(current.Results[0].Id)->text(1) == QStringLiteral("Carried from candidate 1"), "AC03: an unchanged input scope carries the code result");
		expect(evidence->row_of(current.Results[1].Id)->text(1) == QStringLiteral("Recheck"), "AC03: without inputs, a later candidate needs a recheck");
		capture(window, QStringLiteral("local-draft-evidence-candidate-two"));
		page->candidates()->setCurrentIndex(0);
		settle(s.Lanes);
		expect(evidence->row_of(current.Results[1].Id)->text(1) == QStringLiteral("This candidate"), "AC03: the earlier candidate's history is inspectable");
	}

	{
		// AC01: the evidence history resumes after a restart; the draft opens on Changes.
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		emit  window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(page->projection().Draft && page->projection().Draft->Id == draftId && page->areas()->currentIndex() == 0, "X23: a resumed draft opens on Changes");
		const auto& kept = page->projection().Draft->Evidence;
		expect(kept.Claims.size() == 2U && kept.Results.size() == 2U && kept.Imports.size() == 1U && kept.Claims[0].Earlier.size() == 1U, "AC01: the evidence history resumes");
		page->areas()->setCurrentIndex(1);
		settle(s.Lanes);
		expect(page->evidence()->row_of(kept.Claims[1].Id)->checkState(2) == Qt::Checked, "the selection survives a restart");
	}
	std::cout << "local-draft-evidence-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-evidence-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
