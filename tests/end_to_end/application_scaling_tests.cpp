// BPR-0972: measured public work, including counterexamples to blanket scaling.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>
#include <tuple>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

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
		std::cout << "gesture=" << aGesture
				  << " file_projection_rows=" << aAfter.Workbench.FileProjectionRows - aBefore.Workbench.FileProjectionRows
				  << " projection_capacity_growths=" << aAfter.Workbench.FileProjectionCapacityGrowths - aBefore.Workbench.FileProjectionCapacityGrowths
				  << " lookup_comparisons=" << aAfter.Workbench.FileLookupComparisons - aBefore.Workbench.FileLookupComparisons
				  << " accepted_observation_rows=" << aAfter.Workbench.AcceptedObservationRows - aBefore.Workbench.AcceptedObservationRows
				  << " overview_builds=" << aAfter.OverviewBuilds - aBefore.OverviewBuilds
				  << " overview_receipt_visits=" << aAfter.OverviewReceiptVisits - aBefore.OverviewReceiptVisits
				  << " overview_receipt_updates=" << aAfter.OverviewReceiptUpdates - aBefore.OverviewReceiptUpdates
				  << " overview_receipt_rows=" << aAfter.OverviewReceiptRows - aBefore.OverviewReceiptRows
				  << " receipt_loads=" << aAfter.Ledger.ReceiptLoads - aBefore.Ledger.ReceiptLoads
				  << " receipt_cache_hits=" << aAfter.Ledger.ReceiptCacheHits - aBefore.Ledger.ReceiptCacheHits
				  << " persistence_requests=" << aAfter.Ledger.PersistenceRequests - aBefore.Ledger.PersistenceRequests
				  << " conversation_thread_visits=" << aAfter.ConversationThreadsVisited - aBefore.ConversationThreadsVisited
				  << " conversation_post_visits=" << aAfter.ConversationPostsVisited - aBefore.ConversationPostsVisited
				  << " conversation_writes=" << aAfter.Ledger.ConversationWrites - aBefore.Ledger.ConversationWrites
				  << " files_resets=" << aTreeAfter.ModelResets - aTreeBefore.ModelResets
				  << " files_changed_rows=" << aTreeAfter.ChangedRows - aTreeBefore.ChangedRows
				  << " files_overview_visits=" << aTreeAfter.OverviewRowsVisited - aTreeBefore.OverviewRowsVisited << '\n';
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	std::cout.setf(std::ios::unitbuf);
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
		select("src/file_0033.cpp");
		settle([&] { return access::scaling_evidence(*owner); });
		report("select-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
		expect(access::scaling_evidence(*owner).Workbench.FileProjectionRows == before.Workbench.FileProjectionRows, "selection builds no retained file projection");
		if (round == 0U) {
			before     = access::scaling_evidence(*owner);
			treeBefore = tree->work_statistics();
			expect(window->seen_action()->isEnabled(), "Seen enabled");
			window->seen_action()->trigger();
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.PersistenceRequests > before.Ledger.PersistenceRequests && quiet(); }, "receipt durably accepted");
			settle([&] { return access::scaling_evidence(*owner); });
			report("seen-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
			expect(tree->work_statistics().ModelResets == treeBefore.ModelResets, "one receipt preserves Files model identity");
			before                      = access::scaling_evidence(*owner);
			treeBefore                  = tree->work_statistics();
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
			expect(tree->work_statistics().ModelResets == treeBefore.ModelResets, "Mark read preserves model identity");
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
			QMouseEvent press{ QEvent::MouseButtonPress, point, cards->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(cards->viewport(), &press);
			QMouseEvent release{ QEvent::MouseButtonRelease, point, cards->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(cards->viewport(), &release);
			wait_for([&] { return access::scaling_evidence(*owner).Ledger.ConversationWrites > before.Ledger.ConversationWrites; }, "public card commits read cursor");
			settle([&] { return access::scaling_evidence(*owner); });
			report("conversation-read-one", before, access::scaling_evidence(*owner), treeBefore, tree->work_statistics());
			expect(access::scaling_evidence(*owner).ConversationThreadsVisited - before.ConversationThreadsVisited <= snapshot->ReviewThreads.size(), "one conversation command never duplicates the snapshot observation");
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
		before     = access::scaling_evidence(*owner);
		treeBefore = tree->work_statistics();
		select("large/comparison.cpp");
		const auto sourceBefore = surface->statistics();
		const auto semantic     = surface->current_semantic_index();
		surface->shared_scrollbar()->setValue(surface->shared_scrollbar()->maximum() / 2);
		QApplication::processEvents();
		expect(!window->grab().isNull(), "composed reading surface renders");
		const auto sourceAfter = surface->statistics();
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
