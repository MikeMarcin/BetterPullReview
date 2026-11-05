// [[BPR-1867]]. The open pull request picks up new activity by itself,
// through the composed application on the fixture network. The owner,
// 2026-11-04: with a pull request open, another user commented; the activity
// poll noticed, but the pull request did not show the comment "on its own; I
// had to manually refresh".
//
// The reviewer reads a file in Review. What an activity poll hands the
// profile (the activity host's own hand-off; a fixture profile signs in
// natively, which GitHub refuses the notification list, so it never polls) is
// handed in here:
//
// - activity on another pull request reads nothing;
// - activity on the shown one, spelt in another case, reads it again by
//   itself: an automatic refresh, never the reviewer's, that brings the reply
//   posted meanwhile onto the file's conversation, keeps the file selected
//   and scrolled where it was, and never leaves the comparison;
// - while the reviewer writes in a multi-line editor the reread waits, and it
//   runs once they stop, the text they wrote untouched;
// - coming back in front reads nothing while the update is current, and reads
//   again once it is five minutes old (discovery's clock moved on).
//
// It prints the reads each reread made.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using access  = app::detail::github_application_fixture_access;
	using outcome = app::detail::github_read_delay_fixture::refresh_outcome;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aTimeoutMs = 60000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	/// Runs the event loop this long, whatever happens.
	void pump(const int aMs)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	struct found_row
	{
		QListView*  View = nullptr;
		QModelIndex Index;
	};

	[[nodiscard]] auto row_of(qt::workbench_window& aWindow, const QString& aIdentity) -> found_row
	{
		for (const auto group : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const view = aWindow.home_list()->state_view(group);
			for (auto row = 0; view != nullptr && view->model() != nullptr && row < view->model()->rowCount(); ++row) {
				const auto index = view->model()->index(row, 0);
				if (index.data(qt::home_row_role::identity).toString() == aIdentity) {
					return { view, index };
				}
			}
		}
		return { };
	}

	void double_click(const found_row& aRow)
	{
		const auto point   = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		const auto global  = aRow.View->viewport()->mapToGlobal(point);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &press);
		QApplication::sendEvent(aRow.View->viewport(), &release);
		QApplication::sendEvent(aRow.View->viewport(), &doubled);
		QApplication::sendEvent(aRow.View->viewport(), &release);
	}

	/// Whether the conversation model holds a post with this identity, at any depth.
	[[nodiscard]] auto holds_post(const QAbstractItemModel& aModel, const QString& aIdentity, const QModelIndex& aParent = { }) -> bool
	{
		for (auto row = 0; row < aModel.rowCount(aParent); ++row) {
			const auto index = aModel.index(row, 0, aParent);
			if (index.data(qt::github_conversation_model::identity_role).toString() == aIdentity || holds_post(aModel, aIdentity, index)) {
				return true;
			}
		}
		return false;
	}

	/// A visible multi-line editor the reviewer can write in.
	[[nodiscard]] auto writable_editor(qt::workbench_window& aWindow) -> QWidget*
	{
		for (auto* const plain : aWindow.findChildren<QPlainTextEdit*>()) {
			if (plain->isVisible() && plain->isEnabled() && !plain->isReadOnly()) {
				return plain;
			}
		}
		for (auto* const rich : aWindow.findChildren<QTextEdit*>()) {
			if (rich->isVisible() && rich->isEnabled() && !rich->isReadOnly()) {
				return rich;
			}
		}
		return nullptr;
	}

	[[nodiscard]] auto editor_text(QWidget* aEditor) -> QString
	{
		if (auto* const plain = qobject_cast<QPlainTextEdit*>(aEditor)) {
			return plain->toPlainText();
		}
		if (auto* const rich = qobject_cast<QTextEdit*>(aEditor)) {
			return rich->toPlainText();
		}
		return { };
	}

	void insert_text(QWidget* aEditor, const QString& aText)
	{
		if (auto* const plain = qobject_cast<QPlainTextEdit*>(aEditor)) {
			plain->insertPlainText(aText);
		} else if (auto* const rich = qobject_cast<QTextEdit*>(aEditor)) {
			rich->insertPlainText(aText);
		}
	}

	struct reads
	{
		std::size_t Count = 0U;
		std::string Words;
	};

	[[nodiscard]] auto take_reads(app::detail::github_read_delay_fixture& aDelay) -> reads
	{
		const std::scoped_lock lock{ aDelay.ReadLogMutex };
		auto                   counts = std::map<std::string, int>{ };
		for (const auto& read : aDelay.ReadLog) {
			++counts[read.Operation];
		}
		auto words = std::string{ };
		for (const auto& [name, count] : counts) {
			words += ' ' + name + '=' + std::to_string(count);
		}
		auto taken = reads{ .Count = aDelay.ReadLog.size(), .Words = words };
		aDelay.ReadLog.clear();
		return taken;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private open pull request activity course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                         = std::make_shared<app::detail::github_read_delay_fixture>();
	auto clockOffset                   = std::make_shared<std::atomic<std::int64_t>>(0);
	auto options                       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot        = root;
	options.RegisteredDeviceCourse     = true;
	options.IndependentWindowReads     = true;
	options.RepeatReadSessions         = true;
	options.SharedDeviceVault          = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay                  = delay;
	options.DiscoveryWallOffsetSeconds = clockOffset;
	options.PersonalDiscovery          = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = { { .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } } },
	};
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "the window is admitted");
	auto* const window = owner->window(owner->windows().front());
	window->resize(1280, 800);
	window->activateWindow();

	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	const auto served = QStringLiteral("octo-org/review-repository#17");
	wait_for([&] { return row_of(*window, served).View != nullptr; }, "the pull request is listed");
	double_click(row_of(*window, served));
	wait_for([&] { return !window->showing_home() && window->github_refresh_action()->isEnabled() && window->pull_request_refresh().UpdatedAt.has_value(); },
			 "the pull request opens, up to date");
	const auto shown = window->shown_pull_request();
	expect(shown.has_value() && shown->Repository == "octo-org/review-repository" && shown->Number == 17, "the window names the pull request it shows");

	// The reviewer reads the first file in Review, scrolled into it.
	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*owner, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");
	auto* const drawer = window->files_drawer()->contents_view();
	wait_for([&] { return drawer->model() != nullptr && drawer->model()->rowCount() > 0; }, "the Files drawer lists the files");
	auto file = QModelIndex{ };
	for (auto row = 0; row < drawer->model()->rowCount() && !file.isValid(); ++row) {
		const auto index = drawer->model()->index(row, 0);
		if (!drawer->model()->hasChildren(index)) {
			file = index;
		}
	}
	expect(file.isValid(), "the drawer names a file");
	drawer->selectionModel()->setCurrentIndex(file, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto* const surface = window->comparison_surface();
	const auto  showing = [&] { return surface->current_result() != nullptr && surface->current_context().has_value(); };
	wait_for([&] { return showing() && !surface->geometry_pending(); }, "the file's comparison shows");
	auto* const conversation = window->conversation_panel()->model();
	expect(conversation != nullptr && conversation->rowCount() > 0, "the file's conversation is listed");
	auto* bar = static_cast<QScrollBar*>(nullptr);
	for (auto* const candidate : surface->findChildren<QScrollBar*>()) {
		if (candidate->orientation() == Qt::Vertical && candidate->isVisible() && (bar == nullptr || candidate->maximum() > bar->maximum())) {
			bar = candidate;
		}
	}
	if (bar != nullptr) {
		bar->setValue(bar->maximum() / 2);
	}
	const auto scrolled = bar != nullptr ? bar->value() : -1;
	pump(200);
	static_cast<void>(take_reads(*delay));
	const auto selected = [&] { return access::epoch_refresh_identity(*owner).SelectedFile; };
	const auto chosen   = selected();
	expect(chosen.has_value(), "the review holds the file chosen");

	// Activity on another pull request reads nothing.
	const auto refreshes = delay->RefreshesSeen.load();
	access::activity_arrived(*owner, { { .Host = shown->Host, .Account = shown->Account, .Repository = shown->Repository, .Number = 18 } });
	pump(500);
	expect(!access::reread_owed(*owner), "activity on another pull request owes nothing");
	expect(delay->RefreshesSeen.load() == refreshes && window->pull_request_refresh().Phase != core::refresh_phase::refreshing, "and reads nothing");

	// Activity on the shown one, as GitHub spells it, reads it again by itself.
	{
		const std::scoped_lock lock{ delay->ScriptMutex };
		delay->RefreshScript = { outcome::new_reply };
		delay->ScriptNext    = 0U;
	}
	auto reply = QStringLiteral("review-comment:PRRC_refresh_reply_%1").arg(delay->RepliesAdded.load() + 1U);
	expect(!holds_post(*conversation, reply), "the reply is not there before the activity");
	auto spelt       = *shown;
	spelt.Repository = "Octo-Org/Review-Repository";
	spelt.Host       = "GitHub.com";
	access::activity_arrived(*owner, { spelt });
	auto blanks    = 0;
	auto automatic = false;
	auto clock     = QElapsedTimer{ };
	clock.start();
	while (clock.elapsed() < 20'000 && (access::reread_owed(*owner) || window->pull_request_refresh().Phase == core::refresh_phase::refreshing || !holds_post(*conversation, reply))) {
		QApplication::processEvents();
		if (!showing()) {
			++blanks;
		}
		if (window->pull_request_refresh().Phase == core::refresh_phase::refreshing) {
			automatic = automatic || window->pull_request_refresh().Trigger == core::refresh_trigger::automatic;
		}
		QThread::msleep(1);
	}
	const auto first = take_reads(*delay);
	std::cout << "open-pull-request-activity:activity-reread ms=" << clock.elapsed() << " reads=" << first.Count << first.Words << '\n';
	expect(delay->RefreshesSeen.load() == refreshes + 1U, "activity on the shown pull request reads it again, once");
	expect(automatic && window->pull_request_refresh().Trigger == core::refresh_trigger::automatic, "as the application's own update, not the reviewer's Refresh");
	expect(window->pull_request_refresh().Phase == core::refresh_phase::updated && window->pull_request_refresh().Changes.NewComments == 1U, "which says what it brought");
	expect(holds_post(*conversation, reply), "the reply posted meanwhile is on the file's conversation");
	expect(blanks == 0 && showing(), "the comparison never leaves the screen");
	// The refreshed review chooses the same file again once its files are read.
	wait_for([&] { return selected() == chosen && !surface->geometry_pending(); }, "the same file is chosen again and its comparison settles", 20'000);
	if (bar != nullptr) {
		std::cout << "open-pull-request-activity:scroll before=" << scrolled << " after=" << bar->value() << '\n';
		expect(bar->value() == scrolled, "and scrolled where it was");
	}

	// While the reviewer writes, the reread waits; it runs once they stop.
	const auto draftBody = std::string{ "BPR-1867: a new comment, being written." };
	wait_for([&] {
		const auto seeded = access::seed_attended_comment_draft(*owner, core::diff_side::right, draftBody);
		expect(seeded != app::detail::fixture_file_reveal_result::failed, "a new comment can be written on a head line");
		return seeded == app::detail::fixture_file_reveal_result::shown;
	},
			 "a new inline draft holds its text");
	auto* const cards = window->conversation_panel()->cards();
	wait_for([&] { return cards->writing_draft(); }, "the new comment is written in the drawer");
	auto* const editor = writable_editor(*window);
	if (editor == nullptr) {
		std::cout << "open-pull-request-activity:writing skipped: no visible multi-line editor in Review\n";
	} else {
		std::cout << "open-pull-request-activity:writing in " << editor->metaObject()->className() << ' ' << editor->objectName().toStdString() << '\n';
		window->activateWindow();
		editor->setFocus(Qt::OtherFocusReason);
		wait_for([&] { return window->composing(); }, "the window says the reviewer is writing");
		const auto words = QStringLiteral("Half a thought, typed while someone else commented.");
		insert_text(editor, words);
		{
			const std::scoped_lock lock{ delay->ScriptMutex };
			delay->RefreshScript = { outcome::new_reply };
			delay->ScriptNext    = 0U;
		}
		reply                = QStringLiteral("review-comment:PRRC_refresh_reply_%1").arg(delay->RepliesAdded.load() + 1U);
		const auto writingAt = delay->RefreshesSeen.load();
		access::activity_arrived(*owner, { *shown });
		pump(3'000);
		std::cout << "open-pull-request-activity:writing owed=" << access::reread_owed(*owner) << " reads=" << (delay->RefreshesSeen.load() - writingAt) << " composing=" << window->composing()
				  << " focus=" << (QApplication::focusWidget() != nullptr ? QApplication::focusWidget()->objectName().toStdString() : std::string{ "none" }) << '\n';
		expect(access::reread_owed(*owner) && delay->RefreshesSeen.load() == writingAt, "while the reviewer writes the reread waits, still owed");
		expect(editor_text(editor).contains(words) && window->composing(), "and the writing is untouched");
		// The reviewer clicks away into the code.
		surface->setFocus(Qt::MouseFocusReason);
		if (window->composing()) {
			editor->clearFocus();
		}
		wait_for([&] { return !window->composing(); }, "the reviewer stops writing");
		wait_for([&] { return delay->RefreshesSeen.load() == writingAt + 1U && !access::reread_owed(*owner) && window->pull_request_refresh().Phase == core::refresh_phase::updated; },
				 "the reread runs once the reviewer stops", 20'000);
		wait_for([&] { return holds_post(*conversation, reply); }, "and brings the next reply");
		const auto later = take_reads(*delay);
		std::cout << "open-pull-request-activity:writing-reread reads=" << later.Count << later.Words << '\n';
		wait_for([&] { return cards->writing_draft(); }, "the comment is still being written in the drawer");
		auto* const after = writable_editor(*window);
		expect(after != nullptr && editor_text(after).contains(words), "the text written is still there");
	}

	// Coming back in front reads nothing while the update is current.
	QWidget elsewhere;
	elsewhere.resize(200, 100);
	elsewhere.show();
	const auto come_back = [&] {
		elsewhere.activateWindow();
		wait_for([&] { return !window->isActiveWindow(); }, "another window comes in front");
		window->activateWindow();
		wait_for([&] { return window->isActiveWindow(); }, "the review window comes back in front");
		pump(300);
	};
	// Not writing: a return's reread would wait for that too.
	std::cout << "open-pull-request-activity:before-return composing=" << window->composing() << " owed=" << access::reread_owed(*owner) << '\n';
	surface->setFocus(Qt::MouseFocusReason);
	if (auto* const writing = window->composing() ? QApplication::focusWidget() : nullptr; writing != nullptr) {
		writing->clearFocus();
	}
	expect(!window->composing(), "the reviewer is reading, not writing");
	const auto beforeReturn = delay->RefreshesSeen.load();
	come_back();
	expect(!access::reread_owed(*owner) && delay->RefreshesSeen.load() == beforeReturn, "coming back to a current pull request reads nothing");
	// Five minutes later by discovery's clock, it reads it again by itself.
	clockOffset->store(static_cast<std::int64_t>(core::refresh_return_seconds) + 1);
	come_back();
	wait_for([&] { return delay->RefreshesSeen.load() == beforeReturn + 1U && !access::reread_owed(*owner) && window->pull_request_refresh().Phase == core::refresh_phase::updated; },
			 "coming back to an aged pull request reads it again", 20'000);
	expect(window->pull_request_refresh().Trigger == core::refresh_trigger::automatic, "as the application's own update");
	expect(showing(), "keeping the file on screen");
	wait_for([&] { return selected() == chosen; }, "and the same file chosen", 20'000);
	const auto returned = take_reads(*delay);
	std::cout << "open-pull-request-activity:return-reread reads=" << returned.Count << returned.Words << '\n';
	come_back();
	expect(delay->RefreshesSeen.load() == beforeReturn + 1U, "and once read, coming back again reads nothing");
	elsewhere.hide();

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "open-pull-request-activity-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "open-pull-request-activity-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
