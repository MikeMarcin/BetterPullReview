// BPR-0972: measured public work, including counterexamples to blanket scaling.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/repositories_drawer.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <new>
#include <thread>
#include <tuple>

// Counts interposed unaligned C++ scalar/array allocations across all threads.
// Qt/C-library malloc and aligned allocation calls are outside this observation.
namespace allocation_measurement {
	std::atomic<std::uint64_t> Calls{ 0U };
	std::atomic<std::uint64_t> Bytes{ 0U };
}
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new(const std::size_t aSize) -> void*
{
	if (auto* const block = std::malloc(aSize == 0U ? 1U : aSize); block != nullptr) {
		allocation_measurement::Calls.fetch_add(1U, std::memory_order_relaxed);
		allocation_measurement::Bytes.fetch_add(aSize, std::memory_order_relaxed);
		return block;
	}
	throw std::bad_alloc{ };
}
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new[](const std::size_t aSize) -> void* { return ::operator new(aSize); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock, std::size_t) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete[](void* aBlock) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete[](void* aBlock, std::size_t) noexcept { std::free(aBlock); }

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	struct gesture_measurement
	{
		std::uint64_t                         Calls   = allocation_measurement::Calls.load(std::memory_order_relaxed);
		std::uint64_t                         Bytes   = allocation_measurement::Bytes.load(std::memory_order_relaxed);
		std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
	};
	gesture_measurement Measurement;

	void report_allocations(const char* aGesture, const gesture_measurement& aBefore)
	{
		const auto after = gesture_measurement{ };
		std::cout << "allocation_gesture=" << aGesture << " process_unaligned_new_calls=" << after.Calls - aBefore.Calls
				  << " requested_bytes=" << after.Bytes - aBefore.Bytes
				  << " elapsed_us_including_settle=" << std::chrono::duration_cast<std::chrono::microseconds>(after.Started - aBefore.Started).count() << '\n';
	}

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { })
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}

	/// \brief Drains queued continuations, such as a window's own ledger
	/// notification, until the observed counters stop moving.
	void settle(const std::function<app::detail::application_scaling_evidence()>& aSample)
	{
		const auto signature = [&] {
			const auto sample = aSample();
			return std::tuple{ sample.OverviewBuilds, sample.OverviewReceiptVisits, sample.ConversationThreadsVisited, sample.Ledger.ReceiptLoads,
							   sample.Ledger.ReceiptCacheHits, sample.Ledger.Publications, sample.Workbench.AcceptedObservationRows, sample.Workbench.FileProjectionRows,
							   sample.OverviewReceiptUpdates };
		};
		auto          last   = signature();
		auto          stable = 0;
		QElapsedTimer elapsed;
		elapsed.start();
		while (stable < 50 && elapsed.elapsed() < 5000) {
			QApplication::processEvents();
			QThread::msleep(2);
			const auto next = signature();
			stable          = next == last ? stable + 1 : 0;
			last            = next;
		}
	}

	void report(const char* aGesture, const app::detail::application_scaling_evidence& aBefore,
				const app::detail::application_scaling_evidence& aAfter,
				const qt::files_tree_work_statistics& aTreeBefore, const qt::files_tree_work_statistics& aTreeAfter)
	{
		report_allocations(aGesture, Measurement);
		std::cout << "gesture=" << aGesture
				  << " file_projection_rows=" << aAfter.Workbench.FileProjectionRows - aBefore.Workbench.FileProjectionRows
				  << " projection_capacity_growths=" << aAfter.Workbench.FileProjectionCapacityGrowths - aBefore.Workbench.FileProjectionCapacityGrowths
				  << " file_position_visits=" << aAfter.Workbench.FilePositionVisits - aBefore.Workbench.FilePositionVisits
				  << " published_file_rows_shifted=" << aAfter.Workbench.PublishedFileRowsShifted - aBefore.Workbench.PublishedFileRowsShifted
				  << " lookup_comparisons=" << aAfter.Workbench.FileLookupComparisons - aBefore.Workbench.FileLookupComparisons
				  << " accepted_observation_rows=" << aAfter.Workbench.AcceptedObservationRows - aBefore.Workbench.AcceptedObservationRows
				  << " overview_builds=" << aAfter.OverviewBuilds - aBefore.OverviewBuilds
				  << " overview_receipt_visits=" << aAfter.OverviewReceiptVisits - aBefore.OverviewReceiptVisits
				  << " overview_receipt_updates=" << aAfter.OverviewReceiptUpdates - aBefore.OverviewReceiptUpdates
				  << " overview_receipt_rows=" << aAfter.OverviewReceiptRows - aBefore.OverviewReceiptRows
				  << " receipt_loads=" << aAfter.Ledger.ReceiptLoads - aBefore.Ledger.ReceiptLoads
				  << " receipt_cache_hits=" << aAfter.Ledger.ReceiptCacheHits - aBefore.Ledger.ReceiptCacheHits
				  << " persistence_requests=" << aAfter.Ledger.PersistenceRequests - aBefore.Ledger.PersistenceRequests
				  << " private_loads=" << aAfter.Ledger.PrivateLoads - aBefore.Ledger.PrivateLoads
				  << " private_writes=" << aAfter.Ledger.PrivateWrites - aBefore.Ledger.PrivateWrites
				  << " conversation_loads=" << aAfter.Ledger.ConversationLoads - aBefore.Ledger.ConversationLoads
				  << " overview_history_updates=" << aAfter.OverviewHistoryUpdates - aBefore.OverviewHistoryUpdates
				  << " conversation_thread_visits=" << aAfter.ConversationThreadsVisited - aBefore.ConversationThreadsVisited
				  << " conversation_post_visits=" << aAfter.ConversationPostsVisited - aBefore.ConversationPostsVisited
				  << " conversation_writes=" << aAfter.Ledger.ConversationWrites - aBefore.Ledger.ConversationWrites
				  << " queue_refreshes=" << aAfter.Queue.AppliedRefreshes - aBefore.Queue.AppliedRefreshes
				  << " queue_item_publications=" << aAfter.Queue.ItemPublications - aBefore.Queue.ItemPublications
				  << " queue_catalogue_visits=" << aAfter.Queue.Projection.RecordsVisited - aBefore.Queue.Projection.RecordsVisited
				  << " files_resets=" << aTreeAfter.ModelResets - aTreeBefore.ModelResets
				  << " files_changed_rows=" << aTreeAfter.ChangedRows - aTreeBefore.ChangedRows
				  << " files_overview_visits=" << aTreeAfter.OverviewRowsVisited - aTreeBefore.OverviewRowsVisited << '\n';
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	std::cout.setf(std::ios::unitbuf);
	const auto  calibrationBefore = allocation_measurement::Calls.load();
	auto* const calibrated        = ::operator new(37U);
	const auto  calibrationAfter  = allocation_measurement::Calls.load();
	::operator delete(calibrated);
	expect(calibrationAfter == calibrationBefore + 1U, "allocation counter detects an explicit allocation");
	expect(aArgc == 2, "installed Git executable required");
	const auto   git = std::filesystem::path{ aArgv[1] };
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	std::cout << "platform=" << QSysInfo::productType().toStdString() << " version=" << QSysInfo::productVersion().toStdString()
			  << " arch=" << QSysInfo::currentCpuArchitecture().toStdString() << " qt=" << qVersion()
			  << " backend=" << QGuiApplication::platformName().toStdString() << " files=515 threads=1000 large_source_lines=20000\n";
	QTemporaryDir temporary;
	const auto    root = e2e::private_data_root(temporary);
	expect(root.has_value(), "owner-private root");
	const auto vault = app::detail::make_shared_github_fixture_vault();
	for (unsigned round = 0; round != 2; ++round) {
		auto holdRead                   = std::atomic_bool{ false };
		auto readEntered                = std::atomic_bool{ false };
		auto readCancelled              = std::atomic_bool{ false };
		auto options                    = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot     = *root;
		options.GitExecutable           = git;
		options.RegisteredDeviceCourse  = true;
		options.PersistDeviceBindings   = true;
		options.RepeatReadSessions      = true;
		options.LargeReviewForeignPosts = true;
		options.LargeReviewRepeatReads  = true;
		options.SharedDeviceVault       = vault;
		options.BeforeFilesPage         = [&](std::stop_token aStop) {
			if (!holdRead.load()) {
				return;
			}
			readEntered = true;
			while (holdRead.load() && !aStop.stop_requested()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
			readCancelled = aStop.stop_requested();
		};
		app::github_application* owner    = nullptr;
		auto                     terminal = false;
		auto                     desktop  = access::start_desktop(app::detail::github_public_path_fixture::large_review_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
		wait_for([&] { return terminal; }, "startup terminal");
		if (owner == nullptr) {
			std::cerr << "startup_refusal=" << app::profile_startup_refusal_name() << " source=" << app::startup_failure_source() << '\n';
		}
		expect(owner != nullptr, "profile admitted");
		owner->show();
		wait_for([&] { return owner->windows().size() == 1U; }, "one window");
		auto* window = owner->window(owner->windows().front());
		window->resize(1500, 950);
		if (round == 0U) {
			window->home_landing()->primary_button()->click();
		}
		// The reopened process resumes the persisted synthetic device binding.
		wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "safe fixture account accepted");
		window->home_open_bar()->setText(QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
		wait_for([&] { return window->home_open_action()->isEnabled(); }, "Open enabled");
		window->home_open_action()->trigger();
		wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_ready && owner->status().EpochStatus == core::epoch_status::ready; }, "review and epoch ready", [&] { std::cerr << "review_status=" << window->session_panel()->status_text() << " reason=" << window->session_panel()->diagnostic_text()
																																																					 << " epoch=" << static_cast<int>(owner->status().EpochStatus) << " recording_failure=" << access::evidence(*owner).RecordingFailure << '\n'; });
		const auto snapshot = access::large_review_snapshot(*owner);
		expect(snapshot && snapshot->Files.size() == 515U && snapshot->ReviewThreads.size() == 1000U, "exact workload");
		if (round == 0U) {
			wait_for([&] { return window->follow_pull_request_action()->isEnabled(); }, "public Follow is enabled");
			window->follow_pull_request_action()->trigger();
			wait_for([&] { return window->follow_pull_request_action()->isChecked() && window->repositories_tree()->tree_model()->item_count() == 1U; }, "Follow durably enrolls the local queue item");
			const auto queueBefore        = access::scaling_evidence(*owner).Queue;
			const auto repositoriesBefore = window->repositories_tree()->tree_model()->work();
			const auto queueMeasurement   = gesture_measurement{ };
			expect(window->github_refresh_action()->isEnabled(), "public PR refresh is enabled for the retained queue item");
			window->github_refresh_action()->trigger();
			wait_for([&] { return access::scaling_evidence(*owner).Queue.AppliedRefreshes > queueBefore.AppliedRefreshes
							   && window->session_panel()->status() == core::github_status::pull_request_ready
							   && owner->status().EpochStatus == core::epoch_status::ready; }, "refreshed public review updates its retained queue item");
			const auto queueAfter        = access::scaling_evidence(*owner).Queue;
			const auto repositoriesAfter = window->repositories_tree()->tree_model()->work();
			report_allocations("queue-update-with-complete-PR-refresh", queueMeasurement);
			std::cout << "queue_public_changed=1 item_publications=" << queueAfter.ItemPublications - queueBefore.ItemPublications
					  << " catalogue_visits=" << queueAfter.Projection.RecordsVisited - queueBefore.Projection.RecordsVisited
					  << " repository_full_builds=" << repositoriesAfter.FullBuilds - repositoriesBefore.FullBuilds << '\n';
			expect(queueAfter.ItemPublications == queueBefore.ItemPublications + 1U
					   && queueAfter.Projection.RecordsVisited == queueBefore.Projection.RecordsVisited
					   && repositoriesAfter.FullBuilds == repositoriesBefore.FullBuilds,
				   "public refresh publishes one queue item and rebuilds no retained queue or repository catalogue");
		}
		window->modality()->action(qt::review_modality::read)->trigger();
		auto*      surface = window->comparison_surface();
		auto*      files   = window->files_drawer();
		const auto quiet   = [&] {
			const auto work = access::diff_work_evidence(*owner);
			return work.NavigationReady && work.WorkbenchOutstandingRequests == 0U && work.EpochOutstandingJobs == 0U && work.OutstandingJobs == 0U;
		};
		const auto select = [&](const char* aPath) {
			const auto file  = core::file_identity_of(aPath);
			const auto index = files->contents()->index_for_file(file);
			expect(index.isValid(), "actual file row exists");
			files->contents_view()->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			wait_for([&] { return surface->current_result() && surface->current_result()->Alignment.Key.File == file && surface->current_semantic_index() && !surface->geometry_pending() && quiet(); }, "exact selected material settles");
		};
		select("src/file_0031.cpp");
		auto* tree       = window->files_tree()->tree_model();
		auto  before     = access::scaling_evidence(*owner);
		auto  treeBefore = tree->work_statistics();
		Measurement      = gesture_measurement{ };
		if (round == 0U) {
			expect(window->epoch_selectors()->refresh_action()->isEnabled(), "public epoch refresh is enabled");
			const auto epochUpdates = window->epoch_selectors()->statistics().PublishedUpdates;
			window->epoch_selectors()->refresh_action()->trigger();
			wait_for([&] { return window->epoch_selectors()->statistics().PublishedUpdates > epochUpdates
							   && window->epoch_selectors()->status() == core::epoch_status::ready && quiet(); }, "same-head history refresh completes on the public decision lane",
					 [&] { std::cerr << "epoch_status=" << window->epoch_selectors()->status_text().toStdString() << " recording_failure=" << access::evidence(*owner).RecordingFailure << " quiet=" << quiet() << '\n'; });
			settle([&] { return access::scaling_evidence(*owner); });
			const auto epochAfter = access::scaling_evidence(*owner);
			report("epoch-refresh-unchanged", before, epochAfter, treeBefore, tree->work_statistics());
			expect(epochAfter.OverviewBuilds == before.OverviewBuilds && epochAfter.OverviewReceiptVisits == before.OverviewReceiptVisits
					   && epochAfter.Workbench.FileProjectionRows == before.Workbench.FileProjectionRows,
				   "unchanged history refresh reads no Overview files or receipts and rebuilds no file projection");
			before      = epochAfter;
			treeBefore  = tree->work_statistics();
			Measurement = gesture_measurement{ };
		}
		select("src/file_0033.cpp");
		settle([&] { return access::scaling_evidence(*owner); });
		report("select-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
		const auto selectedAfter = access::scaling_evidence(*owner);
		expect(selectedAfter.Workbench.FileProjectionRows == before.Workbench.FileProjectionRows
				   && selectedAfter.Workbench.AcceptedObservationRows == before.Workbench.AcceptedObservationRows
				   && selectedAfter.OverviewBuilds == before.OverviewBuilds
				   && selectedAfter.Ledger.ReceiptLoads == before.Ledger.ReceiptLoads + 1U
				   && tree->work_statistics().ModelResets == treeBefore.ModelResets,
			   "selection reads one receipt without catalogue observation, Overview reconstruction or model reset");
		if (round == 0U) {
			before      = access::scaling_evidence(*owner);
			treeBefore  = tree->work_statistics();
			Measurement = gesture_measurement{ };
			expect(window->seen_action()->isEnabled(), "Seen enabled");
			window->seen_action()->trigger();
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.PersistenceRequests > before.Ledger.PersistenceRequests && quiet(); }, "receipt durably accepted");
			settle([&] { return access::scaling_evidence(*owner); });
			report("seen-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
			const auto seenAfter = access::scaling_evidence(*owner);
			expect(seenAfter.OverviewBuilds == before.OverviewBuilds && seenAfter.OverviewReceiptRows == before.OverviewReceiptRows + 1U
					   && seenAfter.Ledger.ReceiptLoads == before.Ledger.ReceiptLoads && seenAfter.Ledger.PersistenceRequests == before.Ledger.PersistenceRequests + 1U
					   && tree->work_statistics().ModelResets == treeBefore.ModelResets,
				   "Seen writes and projects one receipt without rereads, Overview reconstruction or model reset");
			before                      = access::scaling_evidence(*owner);
			treeBefore                  = tree->work_statistics();
			Measurement                 = gesture_measurement{ };
			const auto selectedMaterial = surface->current_result();
			expect(window->read_action()->isEnabled(), "Mark read enabled for exact selected material");
			window->read_action()->trigger();
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.PersistenceRequests > before.Ledger.PersistenceRequests && quiet(); }, "Mark read durably accepted");
			settle([&] { return access::scaling_evidence(*owner); });
			const auto readAfter = access::scaling_evidence(*owner);
			report("mark-file-read", before, readAfter, treeBefore, tree->work_statistics());
			expect(readAfter.Workbench.TargetedActionabilityUpdates == before.Workbench.TargetedActionabilityUpdates + 1U
					   && readAfter.Workbench.TargetedActionabilityFiles == before.Workbench.TargetedActionabilityFiles + 1U
					   && readAfter.Workbench.AcceptedObservationRows == before.Workbench.AcceptedObservationRows
					   && readAfter.Workbench.FileProjectionRows == before.Workbench.FileProjectionRows,
				   "Mark read publishes one file delta without catalogue observation");
			expect(!files->contents()->index_for_file(core::file_identity_of("src/file_0033.cpp")).isValid()
					   && surface->current_result() == selectedMaterial,
				   "read file leaves actionable order while exact comparison stays readable");
			expect(readAfter.OverviewBuilds == before.OverviewBuilds && readAfter.OverviewReceiptRows == before.OverviewReceiptRows + 1U
					   && readAfter.Ledger.ReceiptLoads == before.Ledger.ReceiptLoads && readAfter.Ledger.PersistenceRequests == before.Ledger.PersistenceRequests + 1U
					   && tree->work_statistics().ModelResets == treeBefore.ModelResets,
				   "Mark read writes and projects one receipt without rereads, Overview reconstruction or model reset");
			auto* cards = window->conversation_panel()->cards();
			wait_for([&] { return !cards->cards().empty(); }, "selected file conversation cards");
			auto button = std::optional<QRect>{ };
			for (const auto& card : cards->cards()) {
				for (const auto& action : card.Actions) {
					if (action.first == qt::card_action::mark_read) {
						button = action.second;
						break;
					}
				}
				if (button) {
					break;
				}
			}
			if (!button) {
				for (const auto& card : cards->cards()) {
					std::cerr << "card=" << card.Id << " posts=" << card.Posts.size() << " unread=" << card.UnreadCount << " actions=" << card.Actions.size() << '\n';
				}
			}
			expect(button.has_value(), "real card offers Mark read");
			cards->verticalScrollBar()->setValue(button->top());
			const auto point = QPointF{ button->center() - QPoint{ 0, cards->verticalScrollBar()->value() } };
			before           = access::scaling_evidence(*owner);
			treeBefore       = tree->work_statistics();
			Measurement      = gesture_measurement{ };
			QMouseEvent press{ QEvent::MouseButtonPress, point, cards->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(cards->viewport(), &press);
			QMouseEvent release{ QEvent::MouseButtonRelease, point, cards->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(cards->viewport(), &release);
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.ConversationWrites > before.Ledger.ConversationWrites; }, "public card commits read cursor");
			settle([&] { return access::scaling_evidence(*owner); });
			report("conversation-read-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
			expect(access::scaling_evidence(*owner).ConversationThreadsVisited - before.ConversationThreadsVisited == 1U
					   && access::scaling_evidence(*owner).ConversationPostsVisited - before.ConversationPostsVisited == 1U
					   && access::scaling_evidence(*owner).OverviewBuilds == before.OverviewBuilds,
				   "one conversation command observes exactly its thread and post without overview work");

			// [[BPR-1211]] AC3, composed. The profile's review-ledger lane is
			// held, as slow or contended storage would hold it. The decision
			// lane keeps turning: a posted task runs, the reviewer changes
			// modality, and a receipt gesture is admitted but neither written
			// nor published until the store answers.
			settle([&] { return access::scaling_evidence(*owner); });
			const auto heldBefore  = access::scaling_evidence(*owner);
			const auto shownBefore = window->review_editor()->shown_receipt();
			expect(shownBefore.has_value() && shownBefore->Read == core::file_read_state::read && window->read_action()->isEnabled(),
				   "the read file offers Mark unread before the ledger is held");
			const auto releaseLedger = access::hold_review_ledger_effects(*owner);
			window->read_action()->trigger();
			QElapsedTimer laneTurn;
			laneTurn.start();
			auto turned = false;
			QTimer::singleShot(0, window, [&turned] { turned = true; });
			wait_for([&] { return turned; }, "the decision lane runs a posted task while ledger storage is held");
			const auto turnMs = laneTurn.elapsed();
			window->modality()->action(qt::review_modality::overview)->trigger();
			QApplication::processEvents();
			const auto overviewShown = window->current_modality() == qt::review_modality::overview;
			window->modality()->action(qt::review_modality::read)->trigger();
			QApplication::processEvents();
			const auto held = access::scaling_evidence(*owner);
			std::cout << "held_ledger lane_turn_ms=" << turnMs << " admitted=" << held.Ledger.Commands - heldBefore.Ledger.Commands
					  << " completions=" << held.Ledger.Completions - heldBefore.Ledger.Completions << '\n';
			expect(turnMs < 1000 && overviewShown && window->current_modality() == qt::review_modality::read,
				   "while the store is held the decision lane runs posted work and modality gestures at once");
			expect(held.Ledger.Commands == heldBefore.Ledger.Commands + 1U && held.Ledger.Completions == heldBefore.Ledger.Completions
					   && window->review_editor()->shown_receipt() == shownBefore,
				   "the held gesture is decided and queued, and nothing is written or published before the store answers");
			releaseLedger();
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.Completions > heldBefore.Ledger.Completions
							   && window->review_editor()->shown_receipt() && window->review_editor()->shown_receipt()->Read == core::file_read_state::unread
							   && quiet(); },
					 "the held gesture is durable, then published, once the store answers");
			settle([&] { return access::scaling_evidence(*owner); });
			const auto released = access::scaling_evidence(*owner);
			expect(released.Ledger.PersistenceRequests == heldBefore.Ledger.PersistenceRequests + 1U
					   && released.Ledger.StoreFailures == heldBefore.Ledger.StoreFailures,
				   "the released gesture was written exactly once");
		} else {
			expect(access::scaling_evidence(*owner).Ledger.MaterialReceiptHits > 0U, "same-root restart loads durable receipt for selected material");
			std::cout << "reopen=1 material_receipt_hits=" << access::scaling_evidence(*owner).Ledger.MaterialReceiptHits << '\n';
		}
		// The sibling's real provider worker remains occupied while the first
		// window changes source selection. Closing it revokes its exact authority.
		const auto first     = owner->windows().front();
		auto*      newWindow = window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		expect(newWindow && newWindow->isEnabled(), "New Window available");
		newWindow->trigger();
		wait_for([&] { return owner->windows().size() == 2U; }, "sibling created");
		const auto contexts = owner->windows();
		auto*      sibling  = owner->window(contexts.front() == first ? contexts.back() : contexts.front());
		holdRead            = true;
		sibling->home_open_bar()->setText(QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
		wait_for([&] { return sibling->home_open_action()->isEnabled(); }, "sibling Open enabled", [&] { std::cerr << "sibling_status=" << sibling->session_panel()->status_text() << '\n'; });
		sibling->home_open_action()->trigger();
		wait_for([&] { return readEntered.load(); }, "concurrent provider holds real files page", [&] { std::cerr << "sibling_status=" << sibling->session_panel()->status_text() << " sibling_reason=" << sibling->session_panel()->diagnostic_text() << " showing_home=" << sibling->showing_home() << " recording_failure=" << access::evidence(*owner).RecordingFailure << '\n'; });
		before      = access::scaling_evidence(*owner);
		treeBefore  = tree->work_statistics();
		Measurement = gesture_measurement{ };
		select("large/comparison.cpp");
		const auto sourceBefore      = surface->statistics();
		const auto semantic          = surface->current_semantic_index();
		const auto sourceMeasurement = gesture_measurement{ };
		surface->shared_scrollbar()->setValue(surface->shared_scrollbar()->maximum() / 2);
		QApplication::processEvents();
		expect(!window->grab().isNull(), "composed reading surface renders");
		const auto sourceAfter = surface->statistics();
		report_allocations("source-scroll", sourceMeasurement);
		std::cout << "source_scroll_lines=20000 index_adoptions=" << sourceAfter.SemanticIndexAdoptions - sourceBefore.SemanticIndexAdoptions
				  << " geometry_rebuilds=" << sourceAfter.GeometryRebuilds - sourceBefore.GeometryRebuilds
				  << " pane_measurements=" << sourceAfter.PaneMeasurements - sourceBefore.PaneMeasurements
				  << " alignment_segments_visited=" << sourceAfter.AlignmentSegmentsVisited - sourceBefore.AlignmentSegmentsVisited << '\n';
		expect(surface->current_semantic_index() == semantic && sourceAfter.PaneMeasurements == sourceBefore.PaneMeasurements
				   && sourceAfter.GeometryRebuilds == sourceBefore.GeometryRebuilds,
			   "scroll reuses accepted source geometry and index");
		select("src/file_0034.cpp");
		settle([&] { return access::scaling_evidence(*owner); });
		report("select-two-during-provider", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
		const auto exact = surface->current_result()->Alignment.Key;
		sibling->close();
		wait_for([&] { return readCancelled.load() && owner->windows().size() == 1U && quiet(); }, "cancelled sibling retires and source settles");
		holdRead = false;
		expect(surface->current_result()->Alignment.Key == exact, "cancelled provider cannot replace active source");
		expect(access::evidence(*owner).RecordingFailure == -1, "exact safe fixture transport");
		expect(access::review_mutation_evidence(*owner).Submissions == 0U, "zero remote submissions");
		std::cout << "round=" << round << " held_provider_cancelled=1 surviving_source_exact=1\n";
		desktop->quit();
		wait_for([&] { return desktop->is_quiescent(); }, "whole desktop closes and drains");
		desktop.reset();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QCoreApplication::processEvents();
	}
	expect(e2e::remove_course_root(temporary), "private course data removed");
	std::cout << "application-scaling:observed\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
