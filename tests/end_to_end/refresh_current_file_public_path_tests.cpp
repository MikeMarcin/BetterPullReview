// [[BPR-1851]]. A pull request's refresh brings the file on screen up to date
// first, through the composed application on a fixture network that holds
// every read as long as `gh` takes for one (the owner: "Is there anyway to
// prioritize refreshing the current file on a PR refresh? ... Less waiting
// around for loading"). The reviewer reads a file in Review; someone replies
// on its thread; Refresh. The reply is on the file's conversation before the
// refresh's remaining reads end, the comparison never leaves the screen, and
// the file is not washed over once it is up to date. The course prints when
// the file was fresh, when the refresh ended, and how the review looked
// meanwhile.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>

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

	void mark(std::optional<qint64>& aSlot, const bool aNow, const QElapsedTimer& aClock)
	{
		if (!aSlot && aNow) {
			aSlot = aClock.elapsed();
		}
	}

	auto text(const std::optional<qint64>& aSlot) -> std::string
	{
		return aSlot ? std::to_string(*aSlot) : std::string{ "-" };
	}

	[[nodiscard]] auto washed(const core::pending_look aLook) -> bool
	{
		return aLook == core::pending_look::refreshing || aLook == core::pending_look::refreshing_slow;
	}

	void open_reply(qt::conversation_cards_view& aCards, const QString& aReply)
	{
		auto thread = std::string{ };
		auto body   = QString{ };
		for (const auto& card : aCards.cards()) {
			for (const auto& post : card.Posts) {
				if (post.Id == aReply.toStdString()) {
					thread = card.Id;
					body   = post.Body;
				}
			}
		}
		expect(!thread.empty() && !body.isEmpty(), "the rendered conversation holds the new reply");
		if (aCards.open_thread_id() != thread) {
			const auto point = QPointF{ aCards.note_rect(thread).center() };
			expect(aCards.viewport()->rect().contains(point.toPoint()), "the reply's thread can be clicked in the viewport");
			const auto global  = aCards.viewport()->mapToGlobal(point);
			auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(aCards.viewport(), &press);
			QApplication::sendEvent(aCards.viewport(), &release);
		}
		wait_for([&] {
			if (aCards.open_thread_id() != thread || aCards.open_card() == nullptr) {
				return false;
			}
			for (auto* const widget : aCards.open_card()->findChildren<QWidget*>()) {
				if (auto* const reader = dynamic_cast<qt::markdown_body_view*>(widget);
					reader != nullptr && reader->isVisible() && reader->rendered_view()->toPlainText().contains(body)) {
					return true;
				}
			}
			return false;
		},
				 "the new reply's text can be read while the catalogue is held", 5'000);
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    arguments         = QApplication::arguments();
	const auto    roundTripArgument = arguments.indexOf(QStringLiteral("--round-trip-ms"));
	const auto    roundTrip         = roundTripArgument >= 0 ? arguments.value(roundTripArgument + 1).toUInt() : 120U;
	const auto    measureOnly       = arguments.contains(QStringLiteral("--measure"));
	const auto    discussion        = arguments.contains(QStringLiteral("--discussion"));
	const auto    roundsArgument    = arguments.indexOf(QStringLiteral("--rounds"));
	const auto    rounds            = roundsArgument >= 0 ? std::max(1, arguments.value(roundsArgument + 1).toInt()) : 1;
	const auto    renderArgument    = arguments.indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory   = renderArgument >= 0 ? arguments.value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private current-file refresh course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                     = std::make_shared<app::detail::github_read_delay_fixture>();
	auto server                    = std::make_shared<app::detail::review_submission_fixture_state>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay              = delay;
	if (discussion) {
		options.ReviewSubmissionServer = server;
	}
	// [[BPR-1890]]. Hold the independent catalogue until the new reply and
	// its remaining-loading plate are visible through the real window.
	std::atomic<bool> holdFiles{ false };
	std::atomic<bool> filesHeld{ false };
	options.BeforeFilesPage = [&](const std::stop_token aStop) {
		if (holdFiles.load()) {
			filesHeld = true;
			while (holdFiles.load() && !aStop.stop_requested()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
		}
	};
	options.PersonalDiscovery = app::detail::github_personal_discovery_fixture{
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

	// The reviewer reads the first file in Review.
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
	const auto  shown   = [&] { return surface->current_result() != nullptr && surface->current_context().has_value(); };
	wait_for([&] { return shown() && !surface->geometry_pending(); }, "the file's comparison shows");
	auto* const conversation = window->conversation_panel()->model();
	expect(conversation != nullptr && conversation->rowCount() > 0, "the file's conversation is listed");
	// [[BPR-1887]]. Navigation must not be painted over a line-aligned note.
	const auto checkConversationLink = [&] {
		auto* const cards        = window->conversation_panel()->cards();
		auto* const link         = cards->pull_request_comments_chip();
		const auto  linkRect     = QRect{ link->mapTo(window, QPoint{ }), link->size() };
		const auto  commentsRect = QRect{ cards->viewport()->mapTo(window, QPoint{ }), cards->viewport()->size() };
		expect(link->isVisible() && !linkRect.intersects(commentsRect), "conversation navigation has layout space outside the comment viewport");
		expect(link->width() >= link->sizeHint().width(), "the conversation link and its unread count fit in the toolbar");
	};
	checkConversationLink();

	auto*      refresh = window->github_refresh_action();
	const auto settle  = [&](const char* aMessage) {
		wait_for([&] { return window->pull_request_refresh().Phase != core::refresh_phase::refreshing && refresh->isEnabled(); }, aMessage);
	};
	for (auto round = 1; round <= rounds; ++round) {
		{
			const std::scoped_lock lock{ delay->ScriptMutex };
			delay->RefreshScript = { outcome::new_reply };
			delay->ScriptNext    = 0U;
		}
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			delay->ReadLog.clear();
		}
		const auto reply = discussion ? QStringLiteral("issue-comment:PRIC_refresh_discussion_%1").arg(round)
									  : QStringLiteral("review-comment:PRRC_refresh_reply_%1").arg(delay->RepliesAdded.load() + 1U);
		if (discussion) {
			const auto comment = QJsonObject{
				{ "id", 5000 + round },
				{ "node_id", QStringLiteral("PRIC_refresh_discussion_%1").arg(round) },
				{ "body", QStringLiteral("A PR-wide comment posted while you were reading (%1).").arg(round) },
				{ "user", QJsonObject{ { "node_id", "U_other" }, { "login", "other-reviewer" }, { "type", "User" } } },
			};
			const std::scoped_lock lock{ server->Mutex };
			server->ConversationComments = QJsonDocument{ QJsonArray{ comment } }.toJson(QJsonDocument::Compact).toStdString();
		}
		// Review's tree is scoped to its file. The cards retain the PR-wide
		// posts that the conversation link opens, even while Review is shown.
		const auto hasReply = [&] {
			return discussion ? std::ranges::any_of(window->conversation_panel()->cards()->retained(), [&](const auto& aPost) { return aPost.Id == reply.toStdString(); })
							  : holds_post(*conversation, reply);
		};
		expect(!hasReply(), "the reply is not there before the refresh");
		delay->EveryReadMilliseconds = roundTrip;
		auto fresh                   = std::optional<qint64>{ };
		auto settled                 = std::optional<qint64>{ };
		auto washedAfterFresh        = std::optional<qint64>{ };
		auto firstWash               = std::optional<qint64>{ };
		auto plate                   = std::optional<qint64>{ };
		auto blanks                  = 0;
		auto washedRendered          = false;
		auto clock                   = QElapsedTimer{ };
		clock.start();
		const auto heldContext = surface->current_context();
		const auto heldScroll  = surface->shared_scrollbar()->value();
		filesHeld              = false;
		holdFiles              = !measureOnly;
		auto releasedFiles     = measureOnly;
		refresh->trigger();
		while ((!settled || !fresh) && clock.elapsed() < 20'000) {
			QApplication::processEvents();
			const auto look = window->pending_veils().Content->look();
			mark(fresh, hasReply(), clock);
			mark(firstWash, washed(look), clock);
			if (fresh && washed(look)) {
				mark(washedAfterFresh, true, clock);
			}
			const auto plated = plate.has_value();
			mark(plate, fresh && !settled && look == core::pending_look::partial && window->pending_veils().Content->shown_text().startsWith(QStringLiteral("Refreshing remaining details")),
				 clock);
			// Renders for a look: the washed review before its file is fresh,
			// and the plate once it is.
			if (!renderDirectory.isEmpty() && round == 1) {
				if (!washedRendered && washed(look) && !fresh && clock.elapsed() > 1'000) {
					washedRendered = window->grab().save(renderDirectory + QStringLiteral("/refresh-washed.png"));
				}
				if (!plated && plate) {
					static_cast<void>(window->grab().save(renderDirectory + QStringLiteral("/refresh-file-fresh-rest-coming.png")));
				}
			}
			if (!shown()) {
				++blanks;
			}
			if (!releasedFiles && filesHeld && (plate || clock.elapsed() > 10'000)) {
				expect(fresh && plate, "the reply and remaining-loading plate appear while the changed-file read is held");
				expect(surface->current_context() == heldContext && surface->shared_scrollbar()->value() == heldScroll,
					   "early conversation preserves the exact comparison and reading position");
				if (discussion) {
					window->conversation_panel()->cards()->pull_request_comments_chip()->click();
					wait_for([&] { return window->showing_overview() && window->conversation_panel()->cards()->pull_request_comments_shown(); },
							 "the updated PR discussion opens through the Review link while the catalogue is held");
				}
				open_reply(*window->conversation_panel()->cards(), reply);
				expect(window->pending_veils().Content->look() == core::pending_look::partial,
					   "opening the reply retains the remaining-loading plate");
				if (!renderDirectory.isEmpty() && round == 1) {
					expect(window->grab().save(renderDirectory + QStringLiteral("/refresh-reply-readable.png")), "the readable reply is rendered before releasing the catalogue");
				}
				if (discussion) {
					window->choose_modality(qt::review_modality::review);
					wait_for([&] { return surface->isVisible() && shown() && !surface->geometry_pending(); }, "Review remains readable while the catalogue is held");
					expect(surface->current_context() == heldContext && surface->shared_scrollbar()->value() == heldScroll,
						   "reading the early PR comment retains the comparison and reading position");
				}
				holdFiles     = false;
				releasedFiles = true;
			}
			mark(settled, window->pull_request_refresh().Phase != core::refresh_phase::refreshing, clock);
			QThread::msleep(1);
		}
		holdFiles = false;
		expect(releasedFiles, "the refresh reaches its independent catalogue read");
		settle("the refresh settles");
		delay->EveryReadMilliseconds = 0U;
		auto reads                   = std::size_t{ 0U };
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			reads       = delay->ReadLog.size();
			auto counts = std::map<std::string, int>{ };
			for (const auto& read : delay->ReadLog) {
				++counts[read.Operation];
			}
			std::cout << "refresh-current-file:reads";
			for (const auto& [name, count] : counts) {
				std::cout << ' ' << name << '=' << count;
			}
			std::cout << '\n';
		}
		std::cout << (discussion ? "refresh-discussion:timing round=" : "refresh-current-file:timing round=") << round << " round-trip-ms=" << roundTrip << (discussion ? " conversation-fresh=" : " file-fresh=") << text(fresh)
				  << " refresh-settled=" << text(settled) << " first-wash=" << text(firstWash) << " washed-after-fresh=" << text(washedAfterFresh) << " plate=" << text(plate)
				  << " blank-samples=" << blanks << " reads=" << reads << '\n';
		expect(window->pull_request_refresh().Phase == core::refresh_phase::updated, "the refresh answered");
		expect(hasReply(), "the reply is in the conversation once the refresh ends");
		checkConversationLink();
		if (measureOnly || roundTrip == 0U) {
			continue;
		}
		expect(blanks == 0, "the file's comparison never leaves the screen while it is refreshed");
		expect(fresh && settled && *fresh + static_cast<qint64>(roundTrip) < *settled, "the reply is on the file's conversation before the refresh's remaining reads end");
		expect(!washedAfterFresh, "once the file is up to date it is not washed over");
		expect(plate.has_value(), "meanwhile the review's foot says the rest of the pull request is still coming");
	}

	// The relocated link follows the view's visibility and remains clear
	// at a narrow window size, before navigating by keyboard.
	auto* const cards      = window->conversation_panel()->cards();
	auto* const link       = cards->pull_request_comments_chip();
	auto* const loadedPage = cards->parentWidget();
	loadedPage->hide();
	QApplication::processEvents();
	expect(!cards->isVisible() && !link->isVisible(), "the toolbar link follows a hidden comment page");
	loadedPage->show();
	window->resize(960, 640);
	QApplication::processEvents();
	checkConversationLink();
	if (!renderDirectory.isEmpty()) {
		expect(window->grab().save(renderDirectory + QStringLiteral("/review-narrow.png")), "the narrow Review render is saved");
	}
	expect(link->focusPolicy() == Qt::StrongFocus, "conversation navigation remains keyboard reachable");
	link->setFocus(Qt::TabFocusReason);
	auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(link, &press);
	QApplication::sendEvent(link, &release);
	wait_for([&] { return window->current_modality() == qt::review_modality::overview && cards->pull_request_comments_shown(); },
			 "Space on the conversation link opens Overview's Comments");
	expect(!link->isVisible(), "Overview does not show navigation back to itself");

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << (measureOnly ? "refresh-current-file-public-path:measured\n" : "refresh-current-file-public-path:pass\n");
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "refresh-current-file-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
