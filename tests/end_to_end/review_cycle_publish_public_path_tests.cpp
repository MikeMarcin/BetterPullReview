#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../git_contract/windows_system_root.hpp"
#include "../support/scratch_directory.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "../workspace_contract/simulated_github.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/review_cycle_association.hpp"
#include "bpr/qt/review_cycle_promote_sheet.hpp"
#include "bpr/qt/review_cycle_publish_sheet.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#if defined(Q_OS_MACOS)
	#include <qpa/qwindowsysteminterface.h>
#endif

/// \file
/// \brief [[BPR-1177]]. Publish as Pull Request in the application's own
/// profile desktop: the production composition, its window, its local cycle
/// page and the publication sheet, the shipped store and the product's
/// installed-Git catalog in the profile's own isolation root. GitHub is the
/// one substitute: the course gives the window's session an in-memory GitHub
/// behind the publication catalog's port (`simulated_github`), and the
/// fixture's recorded server answers the pull request read when the window
/// opens it. No live GitHub is contacted.
///
/// \details One launch. File > Open Project… on a Git checkout with starting
/// work and unrelated scratch, a private note, the cycle's improvements and
/// Capture revision, then Share notes' menu, Publish as Pull Request…: the
/// sheet keeps the base commit, the target and the cycle's captures apart and
/// lists what goes out and what stays; the reviewer names the repository,
/// writes Problem and Solution, chooses a draft, Review Plan, reads the exact
/// plan, Publish Pull Request; the sheet says pull request #17 is open and
/// the page's Updates line keeps it. A pull request made by hand is linked
/// only after it is read, as externally published. Open Pull Request hands
/// the address to the Home bar, and the window opens the pull request in the
/// existing workflow; the cycle resumes from Home with its history and both
/// links. Nothing private reached the server, and only the catalog's finite
/// writes were made. The sheet is rendered at each stage for review.
///
/// [[BPR-1273]]. A note on a published line joins the private note. Once
/// pull request #17 was opened (which starts its Review Tour on this
/// profile), Share notes' menu offers Add Notes to Review Tour…: the sheet
/// lists both notes and says where each stop would point, the line note is
/// added through the profile's tour owner with its exact line at the
/// published commit, adding it again changes nothing, and GitHub receives
/// nothing.
namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;
	using tests::workspace_contract::simulated_github;

	constexpr auto private_note = "PRIVATE-NOTE-c41d keep the parser pure";
	constexpr auto line_note    = "LINE-NOTE-7b2e return early on a null pointer";
	constexpr auto r2_parse     = std::string_view{ "#include \"validate.hpp\"\nint parse(const char* aText) { return aText && validate(aText) ? 0 : 1; }\n" };

	void say(const std::string& aLine)
	{
		std::cout << aLine << '\n'
				  << std::flush;
	}

	/// A step the window did not answer as the course expected.
	struct refused final : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	void require(const bool aCondition, const std::string& aWhat)
	{
		if (!aCondition) {
			throw refused{ aWhat };
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

	auto utf8(const QString& aText) -> std::string
	{
		const auto bytes = aText.toUtf8();
		return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
	}

	void reactivate(QWidget& aWindow)
	{
		if (aWindow.isActiveWindow()) {
			return;
		}
		aWindow.activateWindow();
		if (wait_for([&] { return aWindow.isActiveWindow(); }, 2000)) {
			return;
		}
#if defined(Q_OS_MACOS)
		QWindowSystemInterface::handleFocusWindowChanged(aWindow.windowHandle(), Qt::ActiveWindowFocusReason);
		wait_for([&] { return aWindow.isActiveWindow(); }, 3000);
#endif
	}

	void click(QWidget& aControl)
	{
		// A control in a scrolling part of a sheet is scrolled into view first, as a hand would.
		for (auto* parent = aControl.parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const area = qobject_cast<QScrollArea*>(parent)) {
				area->ensureWidgetVisible(&aControl);
				QApplication::processEvents();
				break;
			}
		}
		require(aControl.isVisible() && aControl.isEnabled(), "a visible, enabled " + utf8(aControl.objectName()));
		reactivate(*aControl.window());
		// A check box takes a click on its box or its words, not the empty row beside them.
		const auto at = qobject_cast<QCheckBox*>(&aControl) != nullptr ? QPointF{ 8.0, aControl.height() / 2.0 } : QRectF{ aControl.rect() }.center();
		tests::window_system_clicks(aControl, at, 1);
	}

	/// Types into `aField` one key per character, as the window system reports a hand's keys.
	void type(QWidget& aField, const QString& aText, const bool aClick = true)
	{
		if (aClick) {
			click(aField);
		}
		for (const auto character : aText) {
			const auto upper = character.toUpper().unicode();
			const auto key   = character == QLatin1Char(' ') ? int{ Qt::Key_Space } : int{ upper };
			const auto shift = character.isUpper() ? Qt::ShiftModifier : Qt::NoModifier;
			auto*      focus = QApplication::focusWidget();
			tests::window_system_key(focus != nullptr ? *focus : aField, key, shift, QString{ character });
		}
	}

	/// Replaces a line field's text, then leaves it as Tab would.
	void retype(QLineEdit& aField, const QString& aText)
	{
		click(aField);
		// Select All, as the platform's key does, then the new text replaces it.
		aField.selectAll();
		type(aField, aText, false);
		tests::window_system_key(aField, Qt::Key_Tab);
	}

	void answer_next_folder_dialog(const QString& aPath, bool& aAnswered)
	{
		auto* const timer = new QTimer{ };
		timer->setInterval(20);
		QObject::connect(timer, &QTimer::timeout, [timer, aPath, &aAnswered] {
			auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
			for (auto* const top : QApplication::topLevelWidgets()) {
				if (dialog == nullptr && top->isVisible()) {
					dialog = qobject_cast<QFileDialog*>(top);
				}
			}
			if (dialog == nullptr || !dialog->isVisible()) {
				return;
			}
			timer->stop();
			timer->deleteLater();
			dialog->setDirectory(aPath);
			dialog->selectFile(QString{ });
			static_cast<QDialog*>(dialog)->accept();
			aAnswered = true;
		});
		timer->start();
	}

	/// Opens a file from the drawer, by a double click on its row.
	void open_file(qt::workbench_window& aWindow, qt::review_cycle_view& aPage, const QString& aPath)
	{
		auto* const files = aPage.files();
		for (auto row = 0; row < files->count(); ++row) {
			auto* const item = files->item(row);
			if (item->data(Qt::UserRole).toString() != aPath) {
				continue;
			}
			const auto opened = [&] { return aPage.projection().Source && QString::fromStdString(aPage.projection().Source->Path) == aPath && aPage.source()->isVisible(); };
			for (auto attempt = 0; attempt < 2 && !opened(); ++attempt) {
				reactivate(aWindow);
				files->scrollToItem(item);
				QApplication::processEvents();
				tests::window_system_clicks(*files->viewport(), QRectF{ files->visualItemRect(item) }.center(), 2);
				wait_for(opened, 5000);
			}
			require(opened(), "the drawer opens " + utf8(aPath));
			return;
		}
		throw refused{ "the drawer does not list " + utf8(aPath) };
	}

	/// A drag across bytes [aStart, aEnd) of the code, as a hand selects.
	void select(qt::review_cycle_view& aPage, const std::uint64_t aStart, const std::uint64_t aEnd)
	{
		auto* const code = aPage.source();
		code->setFocus(Qt::MouseFocusReason);
		const auto from = code->cell_rectangle(code->model().point_of(aStart));
		const auto to   = code->cell_rectangle(code->model().point_of(aEnd));
		tests::window_system_mouse(*code->viewport(), QPointF{ from.left() + 1.0, from.center().y() }, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
		tests::window_system_mouse(*code->viewport(), QPointF{ to.left() + 1.0, to.center().y() }, QEvent::MouseMove, Qt::LeftButton);
		tests::window_system_mouse(*code->viewport(), QPointF{ to.left() + 1.0, to.center().y() }, QEvent::MouseButtonRelease, Qt::NoButton);
		require(code->selected_bytes() == std::pair{ aStart, aEnd }, "a drag selects the bytes");
	}

	auto promotion_settled(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.promote_sheet()->isVisible() && aPage.promote_sheet()->add_button()->isVisible() && !aPage.promote_sheet()->status()->text().endsWith(QStringLiteral("…"));
	}

	auto idle(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.projection().Activity == application::review_cycle_activity::idle;
	}

	auto sheet_settled(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.projection().Publication && !aPage.projection().Publication->Busy;
	}

	auto identity_text(qt::workbench_window& aWindow) -> QString
	{
		auto* const label = aWindow.findChild<QLabel*>(QStringLiteral("workbenchStripIdentity"));
		return label != nullptr && label->isVisible() ? label->text() : QString{ };
	}

	/// The sheet as rendered now, for the review artifacts; nothing depends on it.
	void render(QWidget& aSheet, const std::string& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_CYCLE_PUBLISH_RENDER_DIR");
		if (directory.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(directory);
		// From its top, as it opens.
		if (auto* const body = aSheet.findChild<QScrollArea*>(QStringLiteral("cyclePublishBody"))) {
			body->verticalScrollBar()->setValue(0);
			QApplication::processEvents();
		}
		const auto image = aSheet.grab().toImage();
		image.save(QDir{ directory }.filePath(QString::fromStdString(aName) + QStringLiteral(".png")));
	}

	auto contains(const std::string& aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string::npos;
	}

	void run(app::profile_desktop& aDesktop, const core::bpr_profile_id aProfile, const std::filesystem::path& aScratch, const std::filesystem::path& aGitPath)
	{
		const auto context = aDesktop.windows(aProfile).front();
		auto&      window  = *aDesktop.window(context);
		auto&      page    = *window.local_cycle();
		auto*      runtime = aDesktop.runtime(aProfile);
		require(runtime != nullptr, "the profile's runtime");
		const auto aGit = tests::workspace_contract::git_runner{ aGitPath.string() };

		// The window's session acts as the simulated account on GitHub.com.
		auto remote   = std::make_shared<simulated_github>();
		remote->Login = "octo-reviewer";
		access::set_cycle_publication_account(*runtime, context, [remote] {
			return std::optional{ application::github_publication_account{ .Provider = remote, .Account = core::github_account_id{ 1001U }, .Login = "octo-reviewer", .Host = "github.com" } };
		});

		// A checkout at its base, with starting work and unrelated scratch.
		const auto project = aScratch / "parser-project";
		std::filesystem::create_directories(project);
		aGit(project, { "init", "-q", "-b", "main" });
		tests::workspace_contract::write_file(project / "README.md", "# Parser\n");
		tests::workspace_contract::write_file(project / "src" / "parse.cpp", "int parse(const char* aText) { return check(aText); }\n");
		tests::workspace_contract::write_file(project / "src" / "old.cpp", "int old() { return 0; }\n");
		aGit(project, { "add", "." });
		aGit(project, { "commit", "-q", "-m", "base" });
		aGit(project, { "update-ref", "refs/remotes/origin/main", "HEAD" });
		tests::workspace_contract::write_file(project / "src" / "parse.cpp", "int parse(const char* aText) { return aText ? check(aText) : 0; }\n");
		tests::workspace_contract::write_file(project / "scratch.txt", "my own scratch, unrelated\n");

		// File > Open Project…, then a private note on the whole cycle.
		auto answered = false;
		answer_next_folder_dialog(QString::fromStdString(project.string()), answered);
		window.open_project_action()->trigger();
		require(answered, "File > Open Project… asked for a folder");
		reactivate(window);
		require(wait_for([&] { return window.showing_local_cycle() && page.projection().Cycle && !page.projection().Revisions.empty() && idle(page) && !page.projection().Files.empty(); }),
				"the folder opens as a cycle with its first capture");
		click(*page.findChild<QToolButton*>(QStringLiteral("cycleWholeNote")));
		require(page.comment_editor()->isVisible(), "Note on the cycle opens the note");
		type(*page.comment_editor(), QString::fromUtf8(private_note));
		click(*page.findChild<QPushButton*>(QStringLiteral("cycleCommentSave")));
		require(wait_for([&] { return idle(page) && page.projection().Notes.size() == 1U; }), "the note is saved");

		// The cycle's improvements, an addition and a deletion; Capture revision.
		tests::workspace_contract::write_file(project / "src" / "parse.cpp", "#include \"validate.hpp\"\nint parse(const char* aText) { return aText && validate(aText) ? 0 : 1; }\n");
		tests::workspace_contract::write_file(project / "src" / "validate.cpp", "bool validate(const char*) { return true; }\n");
		std::filesystem::remove(project / "src" / "old.cpp");
		auto* const capture = qobject_cast<QToolButton*>(page.review_bar()->widgetForAction(page.capture_action()));
		require(capture != nullptr, "Capture revision is in the review bar");
		click(*capture);
		require(wait_for([&] { return idle(page) && page.projection().Revisions.size() == 2U && page.projection().Message.empty(); }), "R2 captured");

		// [[BPR-1273]]. A note on a line the publication will carry: Return in the code notes the selection.
		open_file(window, page, QStringLiteral("src/parse.cpp"));
		const auto noted = r2_parse.find("validate(aText)");
		select(page, noted, noted + 15U);
		reactivate(window);
		tests::window_system_key(*page.source(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		require(page.comment_editor()->isVisible(), "Return opens a note on the selection");
		type(*page.comment_editor(), QString::fromUtf8(line_note));
		click(*page.findChild<QPushButton*>(QStringLiteral("cycleCommentSave")));
		require(wait_for([&] { return idle(page) && page.projection().Notes.size() == 2U; }), "the line note is saved");

		// Share notes' menu: Publish as Pull Request…
		require(page.publish_action()->isEnabled(), "Publish as Pull Request is offered");
		page.publish_action()->trigger();
		auto* const sheet = page.publish_sheet();
		require(wait_for([&] { return sheet->isVisible() && sheet_settled(page) && page.projection().Publication->Outgoing.has_value(); }), "the sheet opens over the code with what would go out");
		require(sheet->title()->text() == QStringLiteral("Publish R2 as a pull request"), "titled for the latest capture: " + utf8(sheet->title()->text()));
		require(sheet->account_words()->text() == QStringLiteral("As octo-reviewer on github.com."), "the account it acts as");
		require(sheet->cycle_words()->text().contains(QStringLiteral("from R1, the first capture, to R2, the latest")) && sheet->cycle_words()->text().contains(QStringLiteral("not from R1 to R2")),
				"the cycle's captures are not the pull request's change: " + utf8(sheet->cycle_words()->text()));
		require(sheet->base_words()->text().startsWith(QStringLiteral("The new commit's only parent: ")) && sheet->base_words()->text().contains(QStringLiteral("where HEAD points")),
				"the base commit named: " + utf8(sheet->base_words()->text()));
		require(sheet->private_words()->text().contains(QStringLiteral("R1 to R2, 2 notes, 0 rounds, 0 replies and your decisions. None of it is uploaded.")), "what stays on this computer: " + utf8(sheet->private_words()->text()));
		require(remote->writes() == 0 && !page.projection().Publication->Current, "opening it recorded and sent nothing");

		// The reviewer names where it goes and what the pull request says.
		retype(*sheet->target_field(), QStringLiteral("refs/remotes/origin/main"));
		require(wait_for([&] { return sheet_settled(page) && sheet->target_words()->text().contains(QStringLiteral("is at the same commit")); }), "the target is compared: " + utf8(sheet->target_words()->text()));
		const auto paths = [&] {
			auto text = std::string{ };
			for (auto row = 0; row < sheet->paths()->count(); ++row) {
				text += utf8(sheet->paths()->item(row)->data(Qt::UserRole + 2).toString()) + "\n";
			}
			return text;
		}();
		require(contains(paths, "src/parse.cpp\nGoes out changed: starting work and the cycle's improvement") && contains(paths, "src/validate.cpp\nGoes out added: the cycle's improvement") && contains(paths, "src/old.cpp\nGoes out deleted") && contains(paths, "scratch.txt\nStays out: starting work nothing in the cycle touched"), "each path in its own words:\n" + paths + [&] {
					auto facts = std::string{ };
					const auto& form = page.projection().Publication->Form;
					facts += "mode " + std::to_string(static_cast<int>(form.Mode)) + " overrides " + std::to_string(form.Overrides.size()) + " base " + form.Base + " target " + form.Target + "\n";
					for (const auto& override : form.Overrides) {
						facts += "  override " + override.Path + " " + std::to_string(static_cast<int>(override.Choice)) + "\n";
					}
					for (const auto& row : page.projection().Publication->Outgoing->Rows) {
						facts += row.Path + " proposed " + std::to_string(row.Proposed) + " selected " + std::to_string(row.Selected) + " out " + std::to_string(static_cast<int>(row.Outgoing)) + "\n";
					}
					return facts; }());
		render(*sheet, "publish-choosing");
		retype(*sheet->owner_field(), QStringLiteral("octo-org"));
		retype(*sheet->repository_field(), QStringLiteral("review-repository"));
		retype(*sheet->title_field(), QStringLiteral("Validate parser input"));
		type(*sheet->problem_field(), QStringLiteral("Parsing accepted a null pointer."));
		type(*sheet->solution_field(), QStringLiteral("Validate the text before checking it."));
		click(*sheet->draft_box());
		require(wait_for([&] { return sheet_settled(page) && page.projection().Publication->Form.Draft; }), "the draft choice is the form's");

		// Review Plan: a finite plan, nothing sent.
		click(*sheet->primary_button());
		require(wait_for([&] { return sheet_settled(page) && page.projection().Publication->Stage == application::review_cycle_publication_stage::planned; }),
				"Review Plan records a plan: " + utf8(sheet->status()->text()));
		const auto& planned = *page.projection().Publication;
		require(sheet->title()->text() == QStringLiteral("Publish this plan?") && sheet->primary_button()->text() == QStringLiteral("Publish Pull Request"), "the plan waits for confirmation");
		require(planned.Current->Plan.PullTitle == "Validate parser input" && planned.Current->Plan.Draft && planned.Current->Plan.PullBody == *core::publication_body("Parsing accepted a null pointer.", "Validate the text before checking it."),
				"the plan carries exactly what was typed: [" + planned.Current->Plan.PullTitle + "] [" + planned.Current->Plan.PullBody + "] " + std::to_string(planned.Current->Plan.Draft));
		click(*sheet->exact_plan_button());
		require(sheet->plan_text()->isVisible() && sheet->plan_text()->toPlainText().contains(QString::fromStdString(planned.Digest))
					&& sheet->plan_text()->toPlainText().contains(QStringLiteral("Private cycle history, notes and rounds are not uploaded.")),
				"the exact plan names its digest and what stays private");
		require(remote->writes() == 0, "a plan sends nothing");
		render(*sheet, "publish-planned");

		// Publish Pull Request: the server receives the plan's objects, branch and pull request.
		const auto commit = planned.Current->Plan.Commit;
		const auto parent = planned.Current->Plan.Parent;
		{
			// The base exists on the simulated server, as a pushed main would.
			const auto listed = [&] {
				auto made = adapters::git::make_installed_git_publication({ .GitExecutable = std::filesystem::canonical(aGitPath), .IsolationRoot = std::filesystem::canonical(aScratch / "lister"), .WindowsSystemRoot = tests::git_contract::windows_system_root() });
				require(made.has_value(), "a lister catalog");
				return (*made)->list_commit({ .Checkout = project.string(), .Algorithm = core::git_hash_algorithm::sha1 }, parent, { });
			}();
			require(listed.has_value(), "P listed for the simulated server");
			remote->seed(parent, listed->Tree, listed->Entries, "main");
		}
		// [[BPR-1178]]. The branch request never leaves this computer: the
		// sheet stops between sends with the plan waiting, says what GitHub
		// holds, and offers Publish Pull Request again or Cancel Publishing.
		remote->Faults["branch"] = tests::workspace_contract::simulated_fault::not_sent;
		click(*sheet->primary_button());
		require(wait_for([&] {
					return sheet_settled(page) && page.projection().Publication->Stage == application::review_cycle_publication_stage::planned && page.projection().Publication->Current
						&& !page.projection().Publication->Current->Journal.Effects.empty();
				}),
				"a send that never left stops between sends: " + utf8(sheet->status()->text()));
		require(sheet->title()->text() == QStringLiteral("Publish this plan?") && sheet->primary_button()->text() == QStringLiteral("Publish Pull Request") && sheet->primary_button()->isEnabled()
					&& sheet->cancel_publication_button()->isVisible(),
				"the stopped plan can be confirmed again or cancelled");
		require(sheet->status()->text().contains(QStringLiteral("GitHub holds the 4 steps already confirmed; nothing is undone."))
					&& sheet->summary()->text().contains(QStringLiteral("GitHub holds: blob, tree, commit.")) && !remote->Writes.contains("branch"),
				"it says what GitHub holds: " + utf8(sheet->status()->text()) + " / " + utf8(sheet->summary()->text()));
		render(*sheet, "publish-stopped-between-sends");
		click(*sheet->primary_button());
		require(wait_for([&] { return sheet_settled(page) && page.projection().Publication->Stage == application::review_cycle_publication_stage::linked; }),
				"published and linked: " + utf8(sheet->status()->text()));
		require(sheet->title()->text() == QStringLiteral("Pull request #17 is open") && sheet->primary_button()->text() == QStringLiteral("Open Pull Request"), "the sheet says it is open");
		require(page.projection().Links.size() == 1U && page.projection().Links[0].Origin == core::cycle_pull_origin::published && page.projection().Links[0].Head == commit, "the cycle is associated with it");
		require(page.updates_line()->text().contains(QStringLiteral("published as pull request #17 (octo-org/review-repository)")), "the page keeps it: " + utf8(page.updates_line()->text()));
		render(*sheet, "publish-linked");
		for (const auto& [operation, count] : remote->Writes) {
			require(operation == "blob" || operation == "tree" || operation == "commit" || operation == "branch" || operation == "pull", "only the catalog's finite writes: " + operation);
		}
		auto sent = std::string{ };
		for (const auto& [id, bytes] : remote->Blobs) {
			sent.append(bytes.begin(), bytes.end());
		}
		for (const auto& draft : remote->Drafts) {
			sent += draft.Title + draft.Body;
		}
		require(remote->Drafts.size() == 1U && remote->Drafts[0].Draft && !contains(sent, "PRIVATE-NOTE") && !contains(sent, "LINE-NOTE") && !contains(sent, "unrelated"), "nothing private or unselected reached GitHub");

		// A pull request published by hand is linked after a read of exactly it.
		auto theirs    = remote->Pulls.front();
		theirs.Number  = 21;
		theirs.NodeId  = "PR_21";
		theirs.Url     = "https://github.com/octo-org/review-repository/pull/21";
		theirs.HeadRef = "harden-by-hand";
		remote->Pulls.push_back(theirs);
		const auto writes = remote->writes();
		click(*sheet->link_button());
		require(sheet->link_field()->isVisible(), "Link Existing Pull Request asks for its address");
		type(*sheet->link_field(), QStringLiteral("https://github.com/octo-org/review-repository/pull/21"));
		tests::window_system_key(*sheet->link_field(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		require(wait_for([&] { return sheet_settled(page) && page.projection().Links.size() == 2U; }), "linked after it was read: " + utf8(sheet->status()->text()));
		require(page.projection().Links[1].Origin == core::cycle_pull_origin::external && sheet->status()->text().contains(QStringLiteral("as externally published. BetterPullReview did not publish it.")),
				"as externally published");
		require(page.updates_line()->text().contains(QStringLiteral("linked to externally published pull request #21")) && remote->writes() == writes, "the page says so, and nothing was written");
		// [[BPR-1273]]. Signed in, the pull request opened next starts its
		// Review Tour on this profile; a signed-out preview has none.
		window.github_connect_action()->trigger();
		require(wait_for([&] { return window.github_status_text() == "GitHub account connected."; }), "the fixture's account connects: " + window.github_status_text());
		reactivate(*sheet);
		click(*sheet->primary_button());
		require(wait_for([&] { return !window.showing_local_cycle(); }, 30000), "Open Pull Request hands the address to the Home bar");
		require(wait_for([&] { return identity_text(window).contains(QStringLiteral("#17")); }, 60000), "the window opens pull request #17 in the existing workflow: " + utf8(identity_text(window)));
		say("COURSE_NOTE the pull request opened: " + utf8(identity_text(window)));

		// Back to the cycle from Home: its history, and the link made by hand.
		window.choose_home(true);
		require(wait_for([&] { return window.showing_home() && window.home_cycles_list()->count() >= 1; }), "Home lists the cycle");
		auto* const list = window.home_cycles_list();
		auto*       item = list->item(0);
		reactivate(window);
		list->scrollToItem(item);
		QApplication::processEvents();
		tests::window_system_clicks(*list->viewport(), QRectF{ list->visualItemRect(item) }.center(), 2);
		if (!wait_for([&] { return window.showing_local_cycle(); }, 5000)) {
			say("COURSE_NOTE a double click on Home's cycle opened nothing; given once more");
			reactivate(window);
			tests::window_system_clicks(*list->viewport(), QRectF{ list->visualItemRect(item) }.center(), 2);
		}
		require(wait_for([&] { return window.showing_local_cycle() && page.projection().Cycle && idle(page) && page.projection().Links.size() == 2U; }),
				"the cycle resumes with both links (cycle shown " + std::to_string(window.showing_local_cycle()) + ", links " + std::to_string(page.projection().Links.size()) + ", message " + page.projection().Message + ")");
		// The notes are read after the cycle itself is shown.
		require(wait_for([&] { return idle(page) && page.projection().Revisions.size() == 2U && page.projection().Notes.size() == 2U && page.projection().Notes[0].Body == private_note; }),
				"R1, R2 and the notes are still there");
		page.publish_action()->trigger();
		require(wait_for([&] { return sheet->isVisible() && sheet_settled(page) && page.projection().Publication->Stage == application::review_cycle_publication_stage::linked; }), "the linked publication shows again");
		require(!sheet->save_patch_button()->isVisible() && sheet->primary_button()->text() == QStringLiteral("Open Pull Request"), "a linked sheet offers the pull request, not a patch");
		click(*sheet->close_button());
		require(wait_for([&] { return !page.projection().Publication; }), "closed");

		// [[BPR-1273]]. Share notes' menu: Add Notes to Review Tour… The pull
		// request #17 was opened above, so its tour exists on this profile.
		const auto before = remote->writes();
		const auto calls  = remote->Writes;
		reactivate(window);
		require(page.promote_action()->isEnabled(), "Add Notes to Review Tour is offered for a linked cycle");
		page.promote_action()->trigger();
		auto* const promote = page.promote_sheet();
		require(wait_for([&] { return promotion_settled(page); }), "the sheet opens and finds the tour: " + utf8(promote->status()->text()));
		require(promote->title()->text() == QStringLiteral("Add notes to the Review Tour of #17") && promote->notes()->count() == 2, "titled for pull request #17, with both notes: " + utf8(promote->title()->text()));
		const auto rows = [&] {
			auto text = std::string{ };
			for (auto row = 0; row < promote->notes()->count(); ++row) {
				text += utf8(promote->notes()->item(row)->data(Qt::UserRole + 2).toString()) + "\n";
			}
			return text;
		};
		say("COURSE_NOTE promotion rows:\n" + rows());
		require(contains(rows(), "The whole change: PRIVATE-NOTE-c41d keep the parser pure\nNo lines: the note is about the whole change.") && contains(rows(), "src/parse.cpp: LINE-NOTE-7b2e return early on a null pointer\nLine 2, exactly as the pull request's commit holds them."),
				"each note says where its stop would point:\n" + rows());
		require(promote->outgoing_words()->text().startsWith(QStringLiteral("Nothing now.")), "the ledger says nothing goes to GitHub now");
		render(*promote, "promote-choosing");
		// The line note is checked by its box, as a hand does.
		auto* const lineItem = promote->notes()->item(1);
		const auto  box      = promote->notes()->visualItemRect(lineItem);
		reactivate(*promote);
		tests::window_system_clicks(*promote->notes()->viewport(), QPointF{ box.left() + 10.0, box.top() + box.height() / 4.0 }, 1);
		if (!wait_for([&] { return lineItem->checkState() == Qt::Checked; }, 2000)) {
			say("COURSE_NOTE a click on the line note's box did not check it; given by the keyboard");
			promote->notes()->setCurrentItem(lineItem);
			tests::window_system_key(*promote->notes(), Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
		}
		require(wait_for([&] { return promote->add_button()->isEnabled() && promote->add_button()->text() == QStringLiteral("Add 1 Note"); }), "one note chosen: " + utf8(promote->add_button()->text()));
		click(*promote->add_button());
		require(wait_for([&] { return promote->status()->text().startsWith(QStringLiteral("Added 1 note to the Review Tour of #17.")); }), "the note is added: " + utf8(promote->status()->text()));
		require(promote->status()->text().contains(QStringLiteral("nothing was sent to GitHub")) && remote->writes() == before && remote->Writes == calls, "and GitHub received nothing");
		render(*promote, "promote-added");
		// The tour as its owner stored it: the note's words, its exact line at the published commit, waiting.
		{
			auto       tours   = adapters::sqlite::make_review_tour_store(aScratch / "data", aProfile);
			const auto targets = tours->targets({ });
			require(targets && targets->size() == 1U && targets->front().Number.value() == 17U, "the tour of #17");
			const auto state = tours->load_state(targets->front().Owner, targets->front().Draft, { });
			require(state && *state && (*state)->Draft.Stops.size() == 1U, "one stop in the tour");
			const auto& stop = (*state)->Draft.Stops.front();
			require(stop.Body == line_note && stop.Destination && stop.Destination->Commit == commit && stop.Destination->Path == "src/parse.cpp" && stop.Destination->FirstLine == 1U && stop.Destination->LastLine == 1U
						&& stop.Preparation == core::tour_preparation::editing && !core::ready(stop),
					"the stop keeps the note's words and its exact line at the published commit, and waits for the reviewer");
		}
		// Adding it again changes nothing.
		reactivate(*promote);
		tests::window_system_clicks(*promote->notes()->viewport(), QPointF{ box.left() + 10.0, box.top() + box.height() / 4.0 }, 1);
		if (!wait_for([&] { return lineItem->checkState() == Qt::Checked; }, 2000)) {
			promote->notes()->setCurrentItem(lineItem);
			tests::window_system_key(*promote->notes(), Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
		}
		require(wait_for([&] { return promote->add_button()->isEnabled(); }), "chosen again");
		click(*promote->add_button());
		require(wait_for([&] { return promote->status()->text().contains(QStringLiteral("1 note was already in the tour; nothing changed for it.")); }), "added again, nothing changes: " + utf8(promote->status()->text()));
		{
			auto       tours   = adapters::sqlite::make_review_tour_store(aScratch / "data", aProfile);
			const auto targets = tours->targets({ });
			const auto state   = tours->load_state(targets->front().Owner, targets->front().Draft, { });
			require(state && *state && (*state)->Draft.Stops.size() == 1U, "still one stop");
		}
		click(*promote->close_button());
		require(wait_for([&] { return !promote->isVisible() && page.promote_action()->isEnabled(); }), "Done closes it");
		require(remote->writes() == before, "promotion sent nothing to GitHub");
		say("REVIEW_CYCLE_PUBLISH_OK");
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	auto git = std::optional<std::filesystem::path>{ };
	for (int index = 1; index + 1 < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--git") {
			git = std::filesystem::path{ aArgv[index + 1] };
		}
	}
	if (!git) {
		std::cerr << "FAILED: the course needs the build's Git (--git)\n";
		return EXIT_FAILURE;
	}
	// The data root is the owner-private `data` child of the course's own scratch root.
	const auto scratch = std::filesystem::canonical(tests::make_scratch_directory("bpr-cycle-publish"));
	const auto data    = scratch / "data";
	if (!adapters::sqlite::create_storage_root_owner_private(data) || !adapters::sqlite::create_storage_root_owner_private(scratch / "lister")) {
		std::cerr << "FAILED: owner-private roots\n";
		return EXIT_FAILURE;
	}
	auto exit = EXIT_FAILURE;
	{
		const auto profile          = core::bpr_profile_id{ 1 };
		auto       options          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = data;
		options.Profile             = profile;
		// Open Pull Request opens #17 signed out, in public preview. Only
		// macOS ships it, so the course forces it against the fixture, as the
		// other composed public-preview courses do; otherwise Linux and
		// Windows stop at "Sign in to continue to github.com".
		options.PublicPreviewEnabledCourse = true;
		auto started                       = false;
		auto startedOk                     = false;
		auto desktop                       = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			started   = true;
			startedOk = static_cast<bool>(aResult);
		});
		if (!wait_for([&] { return started; }) || !startedOk || !wait_for([&] { return !desktop->windows(profile).empty(); })) {
			std::cerr << "FAILED: the desktop starts with a window\n";
		} else {
			try {
				run(*desktop, profile, scratch, *git);
				exit = EXIT_SUCCESS;
			} catch (const refused& aRefused) {
				std::cerr << "FAILED: " << aRefused.what() << '\n';
			}
		}
		desktop->quit();
		if (!wait_for([&] { return desktop->is_quiescent(); }) || desktop->exit_code() != 0) {
			std::cerr << "FAILED: orderly drain\n";
			exit = EXIT_FAILURE;
		}
	}
	std::error_code ignored;
	std::filesystem::remove_all(scratch, ignored);
	return exit;
}
