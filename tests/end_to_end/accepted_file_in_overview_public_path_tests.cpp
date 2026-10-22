// [[BPR-1339]]. A file accepted in Review shows as accepted on Overview, through
// the real composition, the ledger host and the Git/cache actor, on the
// constructed nested-folders pull request served by the fake GitHub server.
//
// The owner, 2026-10-08: "I accepted a file in the review but it still shows as
// unread in the overview", and the rule: "Rule gap, marking it read updates it
// to read. Accepted is stronger than read or unread". The course accepts one
// file by pointer in the private progress control's menu and another by its
// key, Down and Return, without reading either, marks a third read, and then
// looks at Overview: each accepted row says accepted, the read row says read,
// and the summary counts the accepted files as done. Revoking one acceptance
// returns that row to its read state and the count with it.
//
// [[BPR-1701]]. The review bar's checked Read and Seen steps uncheck as
// its Accepted step does: Read off leaves a file seen and Overview counts it
// not read; Seen off an accepted file leaves it accepted and done.
//
// [[BPR-1702]]. A read file the reviewer leaves returns to the Files
// drawer, so Enter on its Overview row opens it again.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	/// \brief The Overview row of a file, at its first column.
	[[nodiscard]] auto overview_row(const qt::overview_file_model& aModel, const QString& aPath, const QModelIndex& aParent = { })
		-> QModelIndex
	{
		for (auto row = 0; row < aModel.rowCount(aParent); ++row) {
			const auto index = aModel.index(row, 0, aParent);
			if (aModel.rowCount(index) > 0) {
				if (const auto found = overview_row(aModel, aPath, index); found.isValid()) {
					return found;
				}
			} else if (index.data(qt::overview_file_model::path_role).toString() == aPath) {
				return index;
			}
		}
		return { };
	}

	/// \brief What Overview's read column says for a file, as a screen reader hears it.
	[[nodiscard]] auto overview_says(const qt::workbench_window& aWindow, const QString& aPath) -> QString
	{
		const auto* const model = aWindow.overview_page()->file_model();
		const auto        index = overview_row(*model, aPath);
		return index.isValid() ? index.siblingAtColumn(qt::overview_file_model::read_column).data(Qt::AccessibleTextRole).toString() : QString{ };
	}

	[[nodiscard]] auto overview_mark(const qt::workbench_window& aWindow, const QString& aPath) -> int
	{
		const auto* const model = aWindow.overview_page()->file_model();
		const auto        index = overview_row(*model, aPath);
		return index.isValid() ? index.data(qt::overview_file_model::progress_role).toInt() : -2;
	}

	/// \brief The pull request path of the file the comparison shows, from
	/// the presented Overview's rows.
	[[nodiscard]] auto current_path(const qt::workbench_window& aWindow) -> QString
	{
		const auto& context    = aWindow.comparison_surface()->current_context();
		const auto& projection = aWindow.overview_page()->projection();
		if (!context || !projection) {
			return { };
		}
		for (const auto& row : projection->Files) {
			if (core::file_identity_of(row.Path) == context->Key.File) {
				return QString::fromStdString(row.Path);
			}
		}
		return { };
	}

	/// \brief Opens the private progress menu by `aOpen` and runs `aStep` on
	/// it once it shows, whether or not the menu waits in a loop of its own.
	void with_progress_menu(qt::workbench_window& aWindow, const std::function<void()>& aOpen, const std::function<void(QMenu&)>& aStep, const char* const aMessage)
	{
		auto context  = QObject{ };
		auto ran      = false;
		auto attempts = 0;
		auto poll     = std::function<void()>{ };
		poll          = [&] {
			if (auto* const popup = qobject_cast<QMenu*>(QApplication::activePopupWidget()); popup == aWindow.review_controls_menu()) {
				ran = true;
				aStep(*popup);
			} else if (++attempts < 250) {
				QTimer::singleShot(20, &context, poll);
			}
		};
		QTimer::singleShot(0, &context, poll);
		aOpen();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!ran && attempts < 250 && elapsed.elapsed() < 10'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(ran, aMessage);
	}

	void click_action(QMenu& aMenu, QAction* const aAction)
	{
		const auto centre = QPointF{ aMenu.actionGeometry(aAction).center() };
		const auto global = QPointF{ aMenu.mapToGlobal(centre.toPoint()) };
		auto       press  = QMouseEvent{ QEvent::MouseButtonPress, centre, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       lift   = QMouseEvent{ QEvent::MouseButtonRelease, centre, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aMenu, &press);
		QApplication::sendEvent(&aMenu, &lift);
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_ACCEPTED_OVERVIEW_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "render directory created");
		expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "composed window render saved");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-accepted-overview-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::nested_folders_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");
	auto* const page  = window.overview_page();
	const auto  total = page->projection()->Progress.FilesTotal;
	expect(total >= 3U, "the pull request has at least three files");
	const auto filesRead = [&] { return page->value_label(qt::overview_value::files_read)->text(); };
	const auto doneOf    = [&](const std::size_t aDone) { return QStringLiteral("%1 of %2").arg(aDone).arg(total); };
	const auto notRead   = [&](const std::size_t aDone) { return QStringLiteral(", %1 not read").arg(total - aDone); };
	expect(filesRead() == doneOf(0U) && page->files_heading()->text().endsWith(notRead(0U)), "nothing is read or accepted on a first visit");

	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");
	auto* const accept = window.accept_action();
	auto* const read   = window.read_action();

	// File one: Accept through the control's menu, by pointer.
	window.next_file_action()->trigger();
	wait_for([&] { return !current_path(window).isEmpty() && accept->isEnabled() && !accept->isChecked(); }, "the first file opens and offers Accept");
	const auto first = current_path(window);
	// [[BPR-1506]]. Opening the file is enough for Seen, without claiming Read.
	wait_for([&] { return access::review_ledger_settled(*application) && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: seen"); },
			 "opening the first file automatically persists and presents Seen");
	render(window, QStringLiteral("review-automatic-seen"));
	expect(!read->isChecked(), "the first file is not read");
	read->trigger();
	wait_for([&] { return read->isChecked() && access::review_ledger_settled(*application); }, "the reader explicitly marks the file Read");
	// [[BPR-1701]]. The track's checked Read and Seen steps uncheck, as
	// its Accepted step does: Read off leaves the file seen, Seen off leaves
	// it unread.
	auto* const readStep = window.findChild<QToolButton*>(QStringLiteral("workbenchProgressReadStep"));
	auto* const seenStep = window.findChild<QToolButton*>(QStringLiteral("workbenchProgressSeenStep"));
	expect(readStep != nullptr && seenStep != nullptr && readStep->isChecked() && seenStep->isChecked() && readStep->isEnabled(),
		   "the read file's Read and Seen steps are checked and clickable");
	readStep->click();
	wait_for([&] { return access::review_ledger_settled(*application) && !readStep->isChecked() && seenStep->isChecked()
					   && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: seen"); },
			 "clicking the checked Read step leaves the file seen");
	seenStep->click();
	wait_for([&] { return access::review_ledger_settled(*application) && !seenStep->isChecked()
					   && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: unread"); },
			 "clicking the checked Seen step marks the file unseen");
	for (auto turns = 0; turns < 50; ++turns) {
		QApplication::processEvents();
		QThread::msleep(2);
	}
	expect(access::review_ledger_settled(*application) && !seenStep->isChecked() && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: unread"),
		   "explicit Mark unseen is retained while the file stays open");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview(); }, "the reader leaves the comparison");
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return current_path(window) == first && access::review_ledger_settled(*application) && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: seen"); },
			 "opening the comparison again automatically records Seen");
	with_progress_menu(window, [&] { window.review_controls_button()->showMenu(); }, [&](QMenu& aMenu) { click_action(aMenu, accept); }, "the private progress control opens its menu");
	wait_for([&] { return accept->isChecked() && accept->text() == QStringLiteral("Revoke &acceptance"); }, "the control's Accept accepts the first file");
	expect(!read->isChecked(), "accepting the first file does not mark it read");

	// File two: Accept through the control's key and the menu's keys.
	window.next_file_action()->trigger();
	wait_for([&] { return current_path(window) != first && !current_path(window).isEmpty() && accept->isEnabled() && !accept->isChecked(); }, "the second file opens and offers Accept");
	const auto second = current_path(window);
	// A shortcut reaches only the active window, as a reviewer's key does.
	window.activateWindow();
	wait_for([&] { return QGuiApplication::focusWindow() == window.windowHandle(); }, "the window is active for the key");
	with_progress_menu(window, [&] { tests::window_system_key(window, Qt::Key_4, Qt::AltModifier | Qt::ShiftModifier); }, [&](QMenu& aMenu) {
						   // Down to Accept and Return: the menu's keys on every
						   // platform (macOS has no menu mnemonics).
						   for (auto presses = 0; presses < 8 && aMenu.activeAction() != accept; ++presses) {
							   tests::window_system_key(aMenu, Qt::Key_Down);
						   }
						   if (aMenu.activeAction() == accept) {
							   tests::window_system_key(aMenu, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
						   } else {
							   aMenu.close();
						   } }, "the private progress key opens the menu");
	wait_for([&] { return accept->isChecked(); }, "the key, Down and Return accept the second file");

	// File three: Read, the control the owner's rule compares with.
	window.next_file_action()->trigger();
	wait_for([&] { return current_path(window) != second && !current_path(window).isEmpty() && read->isEnabled() && !read->isChecked(); }, "the third file opens and offers Read");
	const auto third = current_path(window);
	read->trigger();
	wait_for([&] { return read->isChecked(); }, "Read marks the third file read");

	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview(); }, "Overview shows again");
	wait_for([&] { return overview_says(window, first) == QStringLiteral("accepted") && overview_says(window, second) == QStringLiteral("accepted"); },
			 "Overview says accepted for each accepted file");
	expect(overview_says(window, third) == QStringLiteral("read"), "Overview says read for the file marked read");
	expect(overview_mark(window, first) == static_cast<int>(core::file_progress_mark::accepted)
			   && overview_mark(window, second) == static_cast<int>(core::file_progress_mark::accepted)
			   && overview_mark(window, third) == static_cast<int>(core::file_progress_mark::read),
		   "Overview's rows carry the core's progress mark");
	const auto* const files = window.files_tree()->tree_model();
	const auto        drawn = [&](const QString& aPath) {
		const auto index = files->index_for_file(core::file_identity_of(aPath.toStdString()));
		return index.isValid() ? index.data(qt::files_tree_model::progress_role).toInt() : -2;
	};
	// The first file sits at the root, so its Files row is listed with every
	// folder closed.
	expect(drawn(first) == static_cast<int>(core::file_progress_mark::accepted), "the Files drawer carries the same mark");
	expect(filesRead() == doneOf(3U) && page->files_heading()->text().endsWith(notRead(3U)),
		   "the summary counts the two accepted files and the read one as done");
	const auto& entry = page->projection()->EntryPath;
	expect(entry.has_value() && QString::fromStdString(*entry) != first && QString::fromStdString(*entry) != second && QString::fromStdString(*entry) != third,
		   "Continue review skips the accepted and read files");
	render(window, QStringLiteral("overview-accepted"));

	// Revoking acceptance returns the second file to its read state. Enter on
	// its Overview row opens it in Review.
	const auto secondRow = overview_row(*page->file_model(), second);
	page->files_view()->scrollTo(secondRow);
	page->files_view()->setCurrentIndex(secondRow);
	page->files_view()->setFocus(Qt::OtherFocusReason);
	tests::window_system_key(*page->files_view(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	wait_for([&] { return !window.showing_overview() && current_path(window) == second && accept->isEnabled() && accept->isChecked(); },
			 "Enter on the second file's row opens it in Review, accepted");
	accept->trigger();
	wait_for([&] { return !accept->isChecked(); }, "Revoke acceptance revokes it");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview() && overview_says(window, second) != QStringLiteral("accepted"); }, "Overview follows the revocation");
	expect(overview_says(window, second) == QStringLiteral("unread") || overview_says(window, second) == QStringLiteral("seen"),
		   "the revoked file shows its read state again");
	expect(filesRead() == doneOf(2U) && page->files_heading()->text().endsWith(notRead(2U)), "the summary counts one fewer done");

	// [[BPR-1702]]. A read file leaves the Files drawer's order only
	// while it is the file shown. Opening the second file left the read
	// third one, so the drawer lists it again and Enter on its Overview row
	// opens it. Before, it stayed out of the drawer and Enter left Review on
	// the second file.
	const auto* const flat   = window.files_drawer()->contents();
	const auto        listed = [&] {
		return static_cast<std::size_t>(flat->rowCount()) == total && files->file_count() == total
			&& flat->index_for_file(core::file_identity_of(third.toStdString())).isValid()
			&& files->index_for_file(core::file_identity_of(third.toStdString())).isValid();
	};
	wait_for(listed, "the Files drawer lists the read third file once another file is open");
	// [[BPR-1701]]. Enter on a file's Overview row opens it in Review. The
	// track's checked Read step leaves the read third file seen and its Read
	// step checks it read again; Overview counts each.
	const auto openFile = [&](const QString& aPath) {
		const auto row = overview_row(*page->file_model(), aPath);
		page->files_view()->scrollTo(row);
		page->files_view()->setCurrentIndex(row);
		page->files_view()->setFocus(Qt::OtherFocusReason);
		tests::window_system_key(*page->files_view(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	};
	const auto reviewSettled = [&](const QString& aPath) {
		return !window.showing_overview() && current_path(window) == aPath && access::review_ledger_settled(*application) && readStep->isEnabled();
	};
	openFile(third);
	wait_for([&] { return reviewSettled(third) && readStep->isChecked() && seenStep->isChecked(); }, "Enter on the read third file's row opens it in Review");
	readStep->click();
	wait_for([&] { return access::review_ledger_settled(*application) && !readStep->isChecked() && seenStep->isChecked()
					   && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: seen"); },
			 "the checked Read step unchecks the third file and leaves it seen");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview() && overview_says(window, third) == QStringLiteral("seen"); }, "Overview says seen for the file taken back from read");
	expect(overview_mark(window, third) == static_cast<int>(core::file_progress_mark::seen) && filesRead() == doneOf(1U)
			   && page->files_heading()->text().endsWith(notRead(1U)),
		   "the summary counts the unchecked file as not read");
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return reviewSettled(third) && !readStep->isChecked(); }, "Review returns to the seen third file");
	readStep->click();
	wait_for([&] { return access::review_ledger_settled(*application) && readStep->isChecked() && seenStep->isChecked(); }, "the Read step checks the third file read");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview() && overview_says(window, third) == QStringLiteral("read"); }, "Overview says read for the file the step checked");
	expect(filesRead() == doneOf(2U) && page->files_heading()->text().endsWith(notRead(2U)), "the summary counts the checked file as done");

	// Unchecking the accepted first file's Seen step keeps it accepted and done.
	openFile(first);
	wait_for([&] { return reviewSettled(first) && accept->isChecked() && seenStep->isChecked(); },
			 "the accepted first file opens in Review, seen");
	seenStep->click();
	wait_for([&] { return access::review_ledger_settled(*application) && !seenStep->isChecked() && accept->isChecked()
					   && window.review_controls_button()->accessibleName() == QStringLiteral("Private progress: accepted"); },
			 "the Seen step unchecks the accepted file and it stays accepted");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview(); }, "Overview shows after Seen is unchecked");
	expect(overview_says(window, first) == QStringLiteral("accepted") && drawn(first) == static_cast<int>(core::file_progress_mark::accepted)
			   && filesRead() == doneOf(2U) && page->files_heading()->text().endsWith(notRead(2U)),
		   "an accepted file unchecked from Seen still shows accepted and counts as done");
	wait_for(listed, "the Files drawer lists the read third file again once the first file is open");
	render(window, QStringLiteral("overview-unchecked"));

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "composed course shuts down and drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "composed private course root cleaned");
	std::cout << "ACCEPTED_OVERVIEW_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " files=" << total << '\n';
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
