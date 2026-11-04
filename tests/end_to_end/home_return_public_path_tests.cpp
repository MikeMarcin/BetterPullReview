// [[BPR-1848]]. Coming back to Home from a pull request shows a usable Home at
// once and keeps it usable while any refresh runs, through the composed
// application on a fixture network that holds every account read as long as
// `gh` takes for one (the owner: "I hate how slow the home refresh is every
// time I finish with a PR and return there"). The course returns three ways:
// Back to Home while the list is fresh, Back to Home once it has aged past the
// freshness window, and Close review. It prints, for each, when Home was
// usable, how long its list was washed, when it settled and what it read, and
// it asserts that the list is never reset or emptied on the way.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/pending_presentation.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemModel>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <chrono>
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
	using access = app::detail::github_application_fixture_access;

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

	/// What the queue's model did while Home came back.
	struct model_events
	{
		int Resets = 0, Removed = 0, Inserted = 0, Moved = 0, Changed = 0;
	};

	/// One return to Home, in milliseconds from the gesture.
	struct return_times
	{
		std::optional<qint64> Shown, Usable, Settled;
		qint64                Washed    = 0;
		std::size_t           Reads     = 0U;
		std::uint64_t         Refreshes = 0U;
		model_events          Model;
		bool                  Blocked = false;
	};

	auto text(const std::optional<qint64>& aSlot) -> std::string
	{
		return aSlot ? std::to_string(*aSlot) : std::string{ "-" };
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto arguments         = QApplication::arguments();
	const auto roundTripArgument = arguments.indexOf(QStringLiteral("--round-trip-ms"));
	const auto roundTrip         = roundTripArgument >= 0 ? arguments.value(roundTripArgument + 1).toUInt() : 120U;
	// Only prints what each return cost, for a build before the change.
	const auto    measureOnly = arguments.contains(QStringLiteral("--measure"));
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private home-return course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                         = std::make_shared<app::detail::github_read_delay_fixture>();
	auto offset                        = std::make_shared<std::atomic<std::int64_t>>(0);
	auto options                       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot        = root;
	options.RegisteredDeviceCourse     = true;
	options.IndependentWindowReads     = true;
	options.RepeatReadSessions         = true;
	options.SharedDeviceVault          = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay                  = delay;
	options.DiscoveryWallOffsetSeconds = offset;
	// The reviewer reviews each pull request while it is open, as the
	// recording's own read of it already says; GitHub's answers to Home agree
	// from then on.
	auto reviewedFirst        = std::make_shared<std::atomic<bool>>(false);
	auto reviewedSecond       = std::make_shared<std::atomic<bool>>(false);
	options.PersonalDiscovery = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" }, .ReviewedByViewer = reviewedFirst },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" }, .ReviewedByViewer = reviewedSecond },
		},
	};
	// More rows, for a Home the size of a working reviewer's.
	const auto extraArgument = arguments.indexOf(QStringLiteral("--extra-rows"));
	const auto extraRows     = extraArgument >= 0 ? arguments.value(extraArgument + 1).toUInt() : 0U;
	for (auto number = 19U; number < 19U + extraRows; ++number) {
		options.PersonalDiscovery->OpenPullRequests.push_back({ .Number = number, .Author = "contributor-c", .RequestedUsers = { "octo-reviewer" } });
	}
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
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
	const auto first  = QStringLiteral("octo-org/review-repository#17");
	const auto second = QStringLiteral("octo-org/review-repository#18");
	wait_for([&] { return row_of(*window, first).View != nullptr && row_of(*window, second).View != nullptr; }, "the pull requests are listed");
	const auto discovery = [&] { return access::personal_discovery_evidence(*owner); };
	const auto settled   = [&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; };
	wait_for(settled, "Home's first refresh settles");

	auto* const veil  = window->pending_veils().HomeList;
	auto* const model = window->queue_landing();
	expect(veil != nullptr && model != nullptr, "Home's list and its veil are composed");
	auto events = model_events{ };
	QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, [&] { ++events.Resets; });
	QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, [&](const QModelIndex&, int aFirst, int aLast) { events.Removed += aLast - aFirst + 1; });
	QObject::connect(model, &QAbstractItemModel::rowsInserted, [&](const QModelIndex&, int aFirst, int aLast) { events.Inserted += aLast - aFirst + 1; });
	auto gestureAt = std::chrono::steady_clock::now();
	QObject::connect(model, &QAbstractItemModel::rowsMoved, [&](const QModelIndex&, int aFirst, int, const QModelIndex&, int aTo) {
		++events.Moved;
		if (measureOnly) {
			const auto moved = model->index(aTo > aFirst ? aTo - 1 : aTo, 0).data(qt::home_row_role::identity).toString().toStdString();
			std::cout << "home-return:moved " << moved << " row " << aFirst << " to " << aTo << " at "
					  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gestureAt).count() << " ms\n";
		}
	});
	QObject::connect(model, &QAbstractItemModel::dataChanged, [&] { ++events.Changed; });

	auto* const overview         = window->overview_page();
	delay->EveryReadEverywhere   = true;
	delay->EveryReadMilliseconds = roundTrip;
	const auto openReview        = [&](const QString& aIdentity, std::atomic<bool>& aReviewed) {
		wait_for([&] { return window->showing_home() && row_of(*window, aIdentity).View != nullptr; }, "Home lists the pull request before it is opened");
		double_click(row_of(*window, aIdentity));
		wait_for([&] { return !window->showing_home() && overview->projection().has_value(); }, "the review is accepted", 120'000);
		// The review's own reads, its tail included, are done before the
		// reviewer goes back: what is measured is what Home does.
		wait_for([&] { return !access::window_read_progress(*owner).Reading; }, "the review's reads end", 120'000);
		aReviewed = true;
	};
	const auto bucketOf = [&](const QString& aIdentity) -> std::optional<core::attention_bucket> {
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (index.data(qt::home_row_role::identity).toString() == aIdentity) {
				return model->item_at(index)->Bucket;
			}
		}
		return std::nullopt;
	};
	// Home is usable once it is shown with both rows, nothing blocks or washes
	// them, and a row can be opened; it is settled once discovery is at rest.
	const auto returns = [&](const char* aName, const std::function<void()>& aGesture) {
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			delay->ReadLog.clear();
		}
		events            = { };
		auto       times  = return_times{ };
		const auto before = discovery().Statistics.Refreshes;
		auto       clock  = QElapsedTimer{ };
		auto       wash   = QElapsedTimer{ };
		auto       washed = false;
		clock.start();
		gestureAt = std::chrono::steady_clock::now();
		aGesture();
		// Settled, then a further two round trips so a refresh that begins
		// late is still seen.
		auto quietSince = std::optional<qint64>{ };
		while (clock.elapsed() < 120'000) {
			QApplication::processEvents();
			const auto now     = clock.elapsed();
			const auto shown   = window->showing_home();
			const auto look    = veil->look();
			const auto veiled  = veil->isVisible() && core::pending_veiled(look);
			const auto blocked = veil->isVisible() && (look == core::pending_look::placeholder || look == core::pending_look::loading || look == core::pending_look::loading_slow || look == core::pending_look::failed);
			const auto listed  = row_of(*window, first).View != nullptr && row_of(*window, second).View != nullptr;
			times.Blocked      = times.Blocked || (shown && blocked);
			if (shown && !times.Shown) {
				times.Shown = now;
			}
			if (shown && listed && !veiled && !blocked && !times.Usable) {
				times.Usable = now;
			}
			if (veiled && !washed) {
				wash.start();
				washed = true;
			} else if (!veiled && washed) {
				times.Washed += wash.elapsed();
				washed = false;
			}
			if (shown && settled()) {
				if (!quietSince) {
					quietSince = now;
				}
				if (now - *quietSince > 2 * static_cast<qint64>(std::max(roundTrip, 100U))) {
					times.Settled = quietSince;
					break;
				}
			} else {
				quietSince.reset();
			}
			QThread::msleep(1);
		}
		if (washed) {
			times.Washed += wash.elapsed();
		}
		times.Model     = events;
		times.Refreshes = discovery().Statistics.Refreshes - before;
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			times.Reads = delay->ReadLog.size();
			auto counts = std::map<std::string, int>{ };
			for (const auto& read : delay->ReadLog) {
				++counts[read.Operation];
			}
			std::cout << "home-return:reads " << aName << ':';
			for (const auto& [name, count] : counts) {
				std::cout << ' ' << name << '=' << count;
			}
			std::cout << '\n';
			if (measureOnly) {
				for (const auto& read : delay->ReadLog) {
					std::cout << "home-return:read " << aName << ' ' << read.Operation << " at "
							  << std::chrono::duration_cast<std::chrono::milliseconds>(read.Began - gestureAt).count() << " ms\n";
				}
			}
		}
		std::cout << "home-return:timing " << aName << " round-trip-ms=" << roundTrip << " shown=" << text(times.Shown)
				  << " usable=" << text(times.Usable) << " washed-ms=" << times.Washed << " settled=" << text(times.Settled)
				  << " refreshes=" << times.Refreshes << " reads=" << times.Reads << " resets=" << times.Model.Resets
				  << " removed=" << times.Model.Removed << " inserted=" << times.Model.Inserted << " moved=" << times.Model.Moved
				  << " changed=" << times.Model.Changed << '\n';
		return times;
	};

	const auto census = [&](const char* aWhen) {
		if (!measureOnly) {
			return;
		}
		std::cout << "home-return:queue " << aWhen << ':';
		for (auto row = 0; row < model->rowCount(); ++row) {
			if (const auto item = model->item_at(model->index(row, 0))) {
				std::cout << " #" << item->Ref.Number << "=b" << static_cast<int>(item->Bucket) << (item->Membership.Requested ? "q" : "") << "role"
						  << static_cast<int>(item->Membership.Role);
			}
		}
		std::cout << '\n';
	};
	census("settled");
	// 1. Back to Home a moment after the list was refreshed.
	openReview(first, *reviewedFirst);
	census("reviewing #17");
	const auto returnReads = discovery().Statistics.ReturnReads;
	const auto fresh       = returns("fresh-back-to-home", [&] { window->choose_home(true); });
	census("back from #17");
	// 2. Back to Home after long enough in the review that the list has aged
	// past its freshness window.
	openReview(second, *reviewedSecond);
	offset->fetch_add(application::personal_discovery_freshness_seconds + 60);
	const auto stale      = returns("stale-back-to-home", [&] { window->choose_home(true); });
	const auto leftSecond = bucketOf(second);
	// 3. Close review, from Home over the open review.
	const auto closed   = returns("close-review", [&] { window->close_review(); });
	const auto searches = [&] {
		const std::scoped_lock lock{ delay->ReadLogMutex };
		return std::ranges::count_if(delay->ReadLog, [](const auto& aRead) { return aRead.Operation.find("Search") != std::string::npos; });
	};

	delay->EveryReadMilliseconds = 0U;
	delay->EveryReadEverywhere   = false;
	if (!measureOnly) {
		for (const auto* const times : { &fresh, &stale, &closed }) {
			expect(times->Shown && *times->Shown < 250, "Home is shown within a frame or two of the gesture");
			expect(times->Usable && *times->Usable <= *times->Shown + 50, "Home's rows are usable as soon as it is shown");
			expect(times->Washed == 0, "Home's list is never washed over while it refreshes");
			expect(!times->Blocked, "Home's list is never blocked while it refreshes");
			expect(times->Model.Resets == 0 && times->Model.Removed == 0, "returning never resets or empties Home's list");
		}
		expect(fresh.Refreshes == 0U && discovery().Statistics.ReturnReads == returnReads + 1U && fresh.Reads > 0U && fresh.Reads <= 2U,
			   "a fresh list is not refreshed on return: only the pull request left is read");
		expect(bucketOf(first) && *bucketOf(first) != core::attention_bucket::initial_review_requested,
			   "the row left says what the reviewer did there once Home settles");
		expect(closed.Refreshes == 0U && closed.Reads == 0U && searches() == 0, "closing a review from Home reads nothing more");
		expect(stale.Refreshes == 1U && stale.Settled.has_value(), "an aged list is refreshed once on return, and settles");
		expect(leftSecond && *leftSecond != core::attention_bucket::initial_review_requested, "the aged list's refresh agrees with the row left");
	}

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << (measureOnly ? "home-return-public-path:measured\n" : "home-return-public-path:pass\n");
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-return-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
