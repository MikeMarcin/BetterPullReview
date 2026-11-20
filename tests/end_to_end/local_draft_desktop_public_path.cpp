#include "../git_contract/windows_system_root.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "local_draft_open_diagnostic.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

/// \file
/// \brief RFC-BPR-0108 ([[BPR-2021]]) through the real workbench window and
/// the real local-draft composition, on real Git repositories: Home's
/// Prepare Pull Request… signed out to the proposed diff with source, target
/// and Local draft identity, without changing the checkout; In preparation;
/// including a working file and refreshing; a note; a restart; the harness
/// handoff resuming the same draft; no Review Cycles entry; a prepared
/// pull request's mark; discard. Renders are written for the owner's look.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
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

	auto identity_text(qt::workbench_window& aWindow) -> QString
	{
		auto* const label = aWindow.findChild<QLabel*>(QStringLiteral("workbenchStripIdentity"));
		return label != nullptr && label->isVisible() ? label->text() : QString{ };
	}

	void commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	/// A primary clone whose origin names GitHub, main moved after the branch.
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
		commit(aGit, seed, "docs/main-only.md", "main moved\n", "main moves");
		aGit(seed, { "push", "--quiet", hosted.string(), "main" });
		aGit(primary, { "fetch", "--quiet", hosted.string(), "+refs/heads/main:refs/remotes/origin/main" });
		aGit(primary, { "checkout", "--quiet", "-b", "feature/search-results" });
		commit(aGit, primary, "src/search.cpp", "int search()\n{\n\treturn 2;\n}\n", "search");
		commit(aGit, primary, "src/engine.cpp", "int engine()\n{\n\treturn search() + 1;\n}\n", "engine uses search");
		return primary;
	}

	auto checkout_state(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::filesystem::path& aScratch) -> std::string
	{
		return read_file(aCheckout / ".git/HEAD") + read_file(aCheckout / ".git/index")
			+ aGit.output(aCheckout, { "--no-optional-locks", "status", "--porcelain=v2", "--untracked-files=all" }, aScratch / "status.txt")
			+ aGit.output(aCheckout, { "for-each-ref", "--format=%(refname) %(objectname)" }, aScratch / "refs.txt");
	}

	/// The paths the window's own Files drawer lists, in order.
	auto file_rows(qt::workbench_window& aWindow) -> QStringList
	{
		auto        rows  = QStringList{ };
		auto* const model = aWindow.files_drawer()->contents();
		for (int row = 0; row < model->rowCount(); ++row) {
			rows << model->data(model->index(row, 0), Qt::DisplayRole).toString();
		}
		return rows;
	}

	/// Both sides of the comparison the window's diff viewer shows, as text.
	auto shown_text(qt::workbench_window& aWindow) -> QString
	{
		const auto& result = aWindow.comparison_surface()->current_result();
		if (result == nullptr) {
			return { };
		}
		auto text = QString{ };
		for (const auto& document : { result->LeftDocument, result->RightDocument }) {
			if (document) {
				text += QString::fromUtf8(reinterpret_cast<const char*>(document->Bytes.data()), static_cast<qsizetype>(document->Bytes.size())) + QLatin1Char{ '\n' };
			}
		}
		return text;
	}

	auto list_texts(QListWidget& aList) -> QStringList
	{
		auto texts = QStringList{ };
		for (int row = 0; row < aList.count(); ++row) {
			texts << aList.item(row)->text();
		}
		return texts;
	}

	struct session
	{
		application::manual_executor              Lanes;
		std::unique_ptr<qt::workbench_window>     Window;
		std::unique_ptr<app::local_draft_desktop> Drafts;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 800);
			Window->show();
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, bpr::tests::git_contract::windows_system_root(), Lanes, Lanes, app::local_draft_syntax{ }, &Lanes);
			Drafts->set_folder_chooser([aChosen] { return aChosen; });
			Drafts->view()->set_discard_question([] { return true; });
			settle(Lanes);
		}

		~session()
		{
			Drafts->shutdown();
			settle(Lanes);
		}

		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;
	};
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "local-draft-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-worktrees" };
	const auto scratch       = std::filesystem::canonical(scratchRoot.path());
	const auto worktree      = make_worktree(*git, scratch);
	const auto chosen        = QString::fromStdString(worktree.string());
	const auto before        = checkout_state(*git, worktree, scratch);
	auto       draftId       = core::local_draft_id{ };
	{
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		// AC06: the author entry leads Home, signed out. [[BPR-2308]]: Review
		// Cycles are retired, so neither Home nor File offers Open Project.
		auto* const prepare = window.home_prepare_button();
		auto* const author  = window.findChild<QWidget*>(QStringLiteral("homeAuthor"));
		expect(window.showing_home() && prepare != nullptr && prepare->isVisible() && prepare->isEnabled(), "AC06: Prepare Pull Request… on Home, signed out");
		expect(!window.findChild<QWidget*>(QStringLiteral("homeInPreparation"))->isVisible(), "no In preparation list before a draft");
		expect(author != nullptr && author->isVisible(), "AC06: the author section shows on Home");
		expect(window.findChild<QWidget*>(QStringLiteral("homeReviewCycles")) == nullptr && window.findChild<QToolButton*>(QStringLiteral("homeOpenProjectButton")) == nullptr
				   && window.findChild<QAction*>(QStringLiteral("openProjectAction")) == nullptr,
			   "Review Cycles are retired: no Home section and no Open Project…");
		capture(window, QStringLiteral("local-draft-home-empty"));

		// AC01: the first click reaches the proposed diff.
		prepare->click();
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(window.showing_local_draft() && page != nullptr && window.comparison_surface()->isVisible(), "AC01: the draft is shown in the pull request window's own review surface");
		const auto& view = page->projection();
		if (!view.Draft || view.Outcome != application::local_draft_open_outcome::created) {
			bpr::tests::end_to_end::print_local_draft_open(view);
		}
		expect(view.Draft && view.Outcome == application::local_draft_open_outcome::created, "AC01: a new local draft");
		draftId = view.Draft->Id;
		expect(page->identity()->text() == QStringLiteral("feature/search-results into acme/engine main"), "AC01: source and target identity");
		expect(identity_text(window).contains(QStringLiteral("Local draft")), "AC01: the title row badges it Local draft");
		expect(page->candidates()->toolTip().contains(QStringLiteral("from merge base")), "AC01: the exact base is disclosed");
		expect(file_rows(window) == QStringList{ QStringLiteral("src/engine.cpp"), QStringLiteral("src/search.cpp") }, "AC01: only the branch's changes, in the Files drawer; main's later change is not shown");
		expect(shown_text(window).contains(QStringLiteral("\treturn 1;")) && shown_text(window).contains(QStringLiteral("\treturn search() + 1;")),
			   "AC01: the first useful screen is the exact proposed diff, in the pull request's diff viewer");
		expect(page->working()->count() == 0, "a clean worktree has nothing uncommitted");
		expect(checkout_state(*git, worktree, scratch) == before, "AC01: the checkout is unchanged");
		capture(window, QStringLiteral("local-draft-changes"));

		// AC02: a working edit and an untracked file are listed apart.
		write_file(worktree / "src/search.cpp", "int search()\n{\n\treturn 4;\n}\n");
		write_file(worktree / "notes/plan.md", "plan\n");
		page->close_button()->click();
		settle(s.Lanes);
		expect(window.showing_home() && window.home_drafts_list()->isVisible() && window.home_drafts_list()->count() == 1
				   && window.home_drafts_list()->item(0)->text().endsWith(QStringLiteral("Local draft")),
			   "AC06, X03: In preparation lists the draft as Local draft");
		capture(window, QStringLiteral("local-draft-home-in-preparation"));
		emit window.home_drafts_list()->itemDoubleClicked(window.home_drafts_list()->item(0));
		settle(s.Lanes);
		expect(window.showing_local_draft() && page->projection().Draft->Id == draftId, "AC08: In preparation resumes the same draft");
		expect(window.current_modality() == qt::review_modality::author && !shown_text(window).isEmpty(), "[[BPR-2196]]: a resumed draft opens in Author, no chat");
		expect(list_texts(*page->working()) == QStringList{ QStringLiteral("notes/plan.md"), QStringLiteral("src/search.cpp") } && !page->refresh_button()->isEnabled(),
			   "AC02: dirty and untracked files are listed and nothing is captured unasked");
		page->working()->item(1)->setCheckState(Qt::Checked);
		settle(s.Lanes);
		expect(page->refresh_button()->isEnabled(), "including a file offers Refresh");
		page->refresh_button()->click();
		settle(s.Lanes);
		expect(page->projection().Draft->Candidates.size() == 2U && page->candidate_actions().size() == 2 && page->candidates()->isEnabled(), "AC04: a successor candidate; the first stays");
		// The candidate is read in Review, its Files tree; Author shows This update.
		window.choose_modality(qt::review_modality::review, core::workbench_modality_entry::choose);
		settle(s.Lanes);
		expect(window.open_local_file(QStringLiteral("src/search.cpp")), "the included file is in Files");
		settle(s.Lanes);
		expect(shown_text(window).contains(QStringLiteral("\treturn 4;")), "AC02: the included working bytes");
		page->note_editor()->setText(QStringLiteral("Check the cancellation path."));
		emit page->note_editor()->returnPressed();
		settle(s.Lanes);
		expect(page->notes()->count() == 1 && page->notes()->item(0)->text().contains(QStringLiteral("cancellation")), "a note on the selected file");
		// RFC-BPR-0127 ([[BPR-2161]], Q8). The draft's notes read as Local
		// comments, marked with the computer; their record is unchanged.
		expect(page->notes()->accessibleName() == QStringLiteral("Local comments") && page->note_editor()->placeholderText() == QStringLiteral("Add a local comment")
				   && page->note_editor()->accessibleName() == QStringLiteral("New local comment") && !page->notes()->item(0)->icon().isNull()
				   && page->notes()->item(0)->toolTip() == QStringLiteral("Your local comment. Only on this computer.")
				   && page->notes()->item(0)->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Local comment, only on this computer"),
			   "BPR-2161: the draft's notes are called Local comments, with the computer");
		std::cout << "local-draft: vocabulary=" << page->notes()->accessibleName().toStdString() << " | " << page->note_editor()->placeholderText().toStdString() << " | " << page->notes()->item(0)->toolTip().toStdString() << '\n';
		capture(window, QStringLiteral("local-draft-candidate-two"));
		// The tray's Comments shows them while the draft shows.
		window.tray_tabs()->setCurrentIndex(0);
		settle(s.Lanes);
		expect(page->notes_page()->isVisible(), "the tray's Comments shows the draft's Local comments");
		capture(window, QStringLiteral("local-draft-local-comments"));
	}

	{
		// AC04, AC05: after a restart, the harness handoff resumes the same draft.
		auto  s      = session{ profileRoot.path(), gitExecutable, chosen };
		auto& window = *s.Window;
		expect(window.home_drafts_list()->count() == 1, "AC04: the draft survives a restart");
		s.Drafts->open_location(worktree.string(), std::string{ "From the harness: focus on search." });
		settle(s.Lanes);
		auto* const page = window.local_draft();
		expect(window.showing_local_draft() && page->projection().Draft && page->projection().Draft->Id == draftId, "AC05: the handoff reaches the same draft, not a new one");
		expect(page->projection().Draft->Candidates.size() == 2U && page->notes()->count() == 2, "AC04, AC05: candidates and notes survive; the context is a note");
		expect(window.home_drafts_list()->count() == 1, "AC05: no duplicate draft");

		// AC08 (X02): once associated, the draft leaves In preparation and
		// marks its pull request's row.
		auto store      = adapters::sqlite::open_local_draft_store(profileRoot.path());
		auto draft      = store->find(draftId);
		auto associated = core::associate_draft(*draft, draft->Version, { .Host = "github.com", .Owner = "acme", .Name = "engine", .Number = 7, .HeadRef = "feature/search-results" },
												"main", 5'000);
		expect(associated && store->put(*associated, draft->Version, { }).has_value(), "an association recorded");
		page->close_button()->click();
		s.Drafts->host().refresh_home();
		settle(s.Lanes);
		expect(window.home_drafts_list()->count() == 0 && !window.findChild<QWidget*>(QStringLiteral("homeInPreparation"))->isVisible(),
			   "AC08: an associated draft is not listed In preparation");
		expect(window.home_list() != nullptr && window.home_list()->prepared().contains(QStringLiteral("acme/engine#7")), "AC08: its pull request's row carries the mark");

		// AC07: discarding removes only the private preparation.
		s.Drafts->host().resume(draftId);
		settle(s.Lanes);
		const auto worktreeBefore = checkout_state(*git, worktree, scratch);
		page->discard_button()->click();
		settle(s.Lanes);
		expect(window.showing_home() && !store->find(draftId).has_value(), "AC07: the draft is discarded");
		expect(checkout_state(*git, worktree, scratch) == worktreeBefore, "AC07: the worktree and its branches are untouched");
		expect(read_file(worktree / "src/search.cpp") == "int search()\n{\n\treturn 4;\n}\n", "AC07: working edits are untouched");
	}
	std::cout << "local-draft-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
