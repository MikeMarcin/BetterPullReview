#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/header_source_navigation.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/source_search_panel.hpp"
#include "github_application/queue_notifications.hpp"

#include <QByteArray>
#include <QColor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPalette>
#include <QScopeGuard>
#include <QStyleHints>
#include <QTabBar>
#include <QTextStream>
#include <QWidget>

#include <algorithm>
#include <type_traits>
#include <variant>

namespace bpr::app {
	/// \brief Resolves once, then commits only in the exact originating review context.
	void github_application::implementation::dispatch_header_source()
	{
		if (Closing || !WorkbenchHost || !EpochHost || !TabHost || !LastAcceptedHandoff
			|| Window->showing_home() || WorkbenchHost->state().Modality != core::workbench_modality::review) {
			return;
		}
		const auto commands = core::project_workbench_commands(WorkbenchHost->state());
		if (!commands.Commands[static_cast<std::size_t>(core::workbench_command_id::go_to_header_source)].Enabled) {
			return;
		}
		const auto result = WorkbenchHost->header_source_target();
		if (result.Status != core::header_source_status::matched || !result.File) {
			Window->present_header_source_result(result);
			return;
		}
		const auto  generation    = WorkbenchHost->generation();
		const auto  selected      = WorkbenchHost->selected();
		const auto  sourceRequest = SelectedDiffRequest;
		const auto  authority     = workbench_authority(*LastAcceptedHandoff);
		const auto  active        = TabHost->state().Active;
		const auto* tab           = active ? core::find_tab(TabHost->state(), *active) : nullptr;
		const auto  view          = tab != nullptr ? std::optional{ tab->View } : std::nullopt;
		const auto  comparison    = EpochHost->state().AcceptedComparison;
		// A tab still restoring its pair cannot lend the visible newer pair to
		// this command. Once ready, the ordinary selection lane keeps its pair.
		if (!comparison || EpochHost->state().PendingComparison || (view && view->Comparison != *comparison)) {
			return;
		}
		DecisionLane.post([this, weak = std::weak_ptr<const int>{ WindowLifetime }, result, generation, selected, sourceRequest, authority, active, view, comparison] {
			if (weak.expired() || Closing || !current(DraftWindow) || !LastAcceptedHandoff
				|| workbench_authority(*LastAcceptedHandoff) != authority
				|| WorkbenchHost->generation() != generation || WorkbenchHost->selected() != selected
				|| SelectedDiffRequest != sourceRequest
				|| WorkbenchHost->state().Modality != core::workbench_modality::review || Window->showing_home()
				|| TabHost->state().Active != active || EpochHost->state().PendingComparison
				|| Window->document_tabs()->tab_at(Window->document_tabs()->tab_bar()->currentIndex()) != active
				|| EpochHost->state().AcceptedComparison != comparison || !WorkbenchHost->contains_file(*result.File)) {
				return;
			}
			const auto* now = active ? core::find_tab(TabHost->state(), *active) : nullptr;
			if ((now != nullptr) != view.has_value()
				|| (now != nullptr && (now->View.File != view->File || now->View.Comparison != view->Comparison))) {
				return;
			}
			dispatch_workbench_intent(core::select_workbench_file_intent{ *result.File });
			Window->present_header_source_result(result);
		});
	}

	void github_application::implementation::dispatch_workbench_intent(
		core::workbench_interaction_intent aIntent)
	{
		if (Closing || WorkbenchHost == nullptr) {
			return;
		}
		if (std::holds_alternative<core::select_workbench_file_intent>(aIntent) || std::holds_alternative<core::navigate_workbench_file_intent>(aIntent)) {
			AutoSeenPresentedRequest.reset();
			TourSourceRequest.reset();
			PendingTourSource.reset();
			cancel_review_navigation();
			cancel_restoration(false, true);
			abandon_tab_load();
			// A file gesture immediately after returning from Discuss wins
			// over the still-queued restoration of the previous tab view.
			TabModality = WorkbenchHost->state().Modality;
		}
		// [[BPR-1132]]. A modality change decides whether the comparison
		// shows the active tab's pair or the window's.
		if (std::holds_alternative<core::set_workbench_modality_intent>(aIntent)) {
			AutoSeenPresentedRequest.reset();
			// A modality entry reuses its admitted source request. It cannot
			// change whether that request came from ordinary or tour navigation.
			PendingTourSource.reset();
			schedule_tab_presentation();
		}
#if defined(BPR_BUILD_TESTING)
		++WorkbenchIntentDispatches;
#endif
		// Clicking the retained file after closing the final tab is still
		// an explicit open, even when the file cursor itself is unchanged.
		const auto* selected    = std::get_if<core::select_workbench_file_intent>(&aIntent);
		const auto  openFile    = selected != nullptr ? std::optional{ selected->File } : std::nullopt;
		const auto  fileGesture = selected != nullptr || std::holds_alternative<core::navigate_workbench_file_intent>(aIntent);
		WorkbenchHost->apply(std::move(aIntent));
		// [[BPR-1478]]. The active tab takes the chosen file on a later
		// turn. A refresh that lands first retires the workbench's
		// selection, so the tabs keep the choice: a reread presents it.
		if (fileGesture && TabHost && tab_modality(WorkbenchHost->state().Modality) && WorkbenchHost->selected()) {
			TabHost->observe_file_choice(*WorkbenchHost->selected());
		}
		if (openFile && TabHost && !TabHost->state().Active) {
			// The accepted epoch can have queued an observation behind this
			// gesture. Adopt it before opening the first tab synchronously.
			observe_tab_changes();
			if (!TabHost->state().Active) {
				TabHost->apply(core::open_in_active_tab_intent{ .File = *openFile });
			}
		}
	}

	void github_application::implementation::dispatch_workbench_change(
		const core::navigate_diff_change_intent aIntent)
	{
		if (Closing || Window == nullptr
			|| Window->comparison_surface() == nullptr) {
			return;
		}
#if defined(BPR_BUILD_TESTING)
		++WorkbenchChangeDispatches;
#endif
		// [[BPR-1532]]. First and Last change reach the end of the order.
		if (aIntent.Reach == core::change_navigation_reach::outermost) {
			static_cast<void>(Window->comparison_surface()->navigate_outermost_change(aIntent.Direction));
			return;
		}
		static_cast<void>(Window->comparison_surface()->navigate_change(
			aIntent.Direction, aIntent.Policy));
	}

	void github_application::implementation::dispatch_workbench_width(
		const core::workbench_drawer aDrawer,
		const std::uint32_t          aWidth)
	{
		if (Closing || WorkbenchHost == nullptr) {
			return;
		}
#if defined(BPR_BUILD_TESTING)
		++WorkbenchWidthDispatches;
#endif
		WorkbenchHost->resize(aDrawer, aWidth);
	}

	void github_application::implementation::dispatch_receipt(core::file_receipt_command aCommand)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			return;
		}
		// [[BPR-1211]]. Decided when it reaches the head of the ledger's
		// queue and published once durable; an answer the ledger did not
		// publish (the selection moved meanwhile) still moves the overview.
		ReviewLedgerHost->submit(aCommand, ledger_answer([this](const auto&) { refresh_overview_receipts(); }));
	}

	auto github_application::implementation::read_cached_source(application::cached_review_source_request aRequest, std::stop_token aStop, application::cached_review_source_completion aCompletion) -> bool
	{
		return !Closing && Profile->current_window(DraftWindow) && EpochHost
			&& EpochHost->read_cached_source(std::move(aRequest), aStop, std::move(aCompletion));
	}

	void github_application::implementation::prepare_runtime()
	{
		if (!Closing && Profile->current_window(DraftWindow) && EpochHost) {
			EpochHost->prepare_runtime();
		}
	}

	void github_application::implementation::publish(
		const application::epoch_presentation_update& aUpdate)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (EpochHost && EpochHost->state().Authority && LastAcceptedHandoff) {
			const auto& authority = *EpochHost->state().Authority;
			if (authority.Repository == LastAcceptedHandoff->Repository && authority.Account == LastAcceptedHandoff->Account) {
				const auto key   = core::lower_hex(authority.Cache.bytes());
				const auto label = LastAcceptedHandoff->Locator.Owner + "/" + LastAcceptedHandoff->Locator.Name;
				if (Profile->InsightsRepositoryLabels[key] != label) {
					Profile->InsightsRepositoryLabels[key] = label;
					Profile->InsightsEffects.post([root = ApplicationDataRoot, key, label] { static_cast<void>(adapters::sqlite::store_insights_repository_label(root, key, label)); });
				}
			}
		}

		if (DiffHost != nullptr && aUpdate.Catalog.has_value()
			&& aUpdate.Catalog->AvailabilityChangedTo == core::anchor_availability::pruned) {
			const auto& state          = DiffHost->state();
			const auto  visible        = state.Pending ? state.Pending : state.Current;
			auto        clearSelection = false;
			for (const auto& epoch : aUpdate.Catalog->AvailabilityChangedEpochs) {
				clearSelection = clearSelection || (visible && core::uses_epoch(visible->Key, epoch));
				DiffHost->invalidate(epoch);
			}
			// [[BPR-1344]]. A round may have started once the reviewer was
			// idle; nothing it prepared from a pruned epoch may be stored.
			cancel_preparation();
			DiffHost->prepare({ });
			if (clearSelection) {
				clear_diff_selection();
			}
		}
		if (aUpdate.Catalog.has_value()) {
			EpochRows.apply(*aUpdate.Catalog);
		}
		if (EpochHost && EpochHost->state().Authority && AgentReview) {
			const auto& authority = *EpochHost->state().Authority;
			if (authority.Host == AgentReview->Host && authority.Account == AgentReview->Account
				&& authority.Repository == AgentReview->Repository && authority.PullRequest == AgentReview->PullRequest) {
				auto label = QStringLiteral("%1 / %2/%3 #%4")
								 .arg(QString::fromStdString(AgentReview->Hostname), QString::fromStdString(AgentReview->Locator.Owner), QString::fromStdString(AgentReview->Locator.Name))
								 .arg(AgentReview->Number.value());
				if (QueueActivity && QueueActivity->Session.Generation == AgentReview->Generation.value()) {
					label += QStringLiteral(" — %1").arg(QString::fromStdString(QueueActivity->Ref.Account));
				}
				Window->epoch_selectors()->set_maintenance_scope(authority, std::move(label));
			}
		}
		Window->publish(aUpdate);
		if (PrFind) {
			PrFind->observe_scope();
		}
		// RFC-BPR-0089. A new comparison or head moves the filter's span,
		// and an ingestion that finished lets the trees be asked for.
		if (aUpdate.Transition.has_value()) {
			refresh_commit_filter();
		}
		const auto acceptedHead = EpochHost != nullptr && EpochHost->state().AcceptedRevision.has_value()
			? std::optional{ EpochHost->state().AcceptedRevision->Head }
			: std::nullopt;
		if (acceptedHead != OverviewHead || OverviewChangedPathsPending || !PublishedOverview) {
			publish_overview();
		} else if (aUpdate.Catalog) {
			const auto history = application::project_overview_history(EpochRows.rows(), OverviewReviewedHead);
			auto&      since   = PublishedOverview->SinceLastReview;
			since.Known        = history.Known;
			since.ReviewedAt   = history.ReviewedAt;
			since.Uploads      = history.Uploads;
			++OverviewHistoryUpdates;
			Window->publish(history);
		}
		refresh_agent_authority();
		const auto* comparison = aUpdate.Transition.has_value()
				&& aUpdate.Transition->Comparison.has_value()
			? &*aUpdate.Transition->Comparison
			: nullptr;
		if (WorkbenchHost != nullptr && comparison != nullptr
			&& comparison->Pair.has_value()
			&& comparison->AvailableForDiff) {
			WorkbenchHost->refresh_selected_diff();
			schedule_preparation();
		}
		retain_reading_position();
		schedule_reading_restoration();
		schedule_tab_presentation();
		// [[BPR-1871]]. The history's progress or failure moves the
		// opened file's wait.
		present_opened_file();
		schedule_link_location();
	}

	void github_application::implementation::clear_diff_selection()
	{
		const auto hadComparison = SelectedDiffPublication.has_value();
		RetryableReviewDraft.reset();
		if (ReviewMutationHost != nullptr
			&& PendingReviewMutation.has_value()) {
			DeferredReviewSelection.emplace(std::nullopt);
			ReviewMutationHost->cancel(*PendingReviewMutation);
		} else {
			adopt_review_selection(
				std::nullopt,
				qt::review_draft_editor_clear_reason::selection_invalidated);
		}
		SelectedDiffRequest.reset();
		SelectedDiffPublication.reset();
		AutoSeenPresentedRequest.reset();
		ReviewSourceMaterial.reset();
		present_split_strip();
		present_filtered_comment();
		// [[BPR-1387]]. Clearing a displayed source removes its permalink
		// placement. Initial PR setup has no source and already published
		// the host's conversations, so it owes no second full publication.
		if (hadComparison) {
			refresh_review_conversations();
		}
		if (ReviewLedgerHost) {
			ReviewLedgerHost->retain({ });
		}
		// [[BPR-1702]]. The order withholds only the retained receipt's
		// file, and only while that receipt says it needs nothing. Once the
		// receipt goes, the file's material is unknown and it is actionable
		// again, as a new observation would answer, so the host restores it
		// now. Without this a read file the reviewer left stayed out of the
		// Files drawer, and Enter on its Overview row opened nothing.
		auto released = std::optional<std::pair<application::workbench_pull_request_authority, core::file_identity>>{ };
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (ReviewLedgerReceipt.has_value() && ReviewLedgerAuthority.has_value()
				&& !core::actionable(core::reason_for(core::actionability_input{ .Receipt = *ReviewLedgerReceipt }))) {
				released.emplace(*ReviewLedgerAuthority, ReviewLedgerReceipt->File);
			}
			ReviewLedgerReceipt.reset();
		}
		if (released.has_value() && WorkbenchHost != nullptr) {
			WorkbenchHost->retain_actionability(released->first, WorkbenchReview.receipt_owner(released->second), true);
		}
		if (Window != nullptr) {
			Window->set_conversation_endpoint_placement(false, false);
		}
		if (DiffHost != nullptr) {
			DiffHost->cancel();
		}
		if (SyntaxBlobs != nullptr) {
			// A classification still in flight now has no comparison to
			// read for. Dropping the bindings makes it report unavailable
			// rather than materialize bytes for a file the reviewer has
			// left.
			SyntaxBlobs->clear_comparison();
		}
		if (AppearanceHost != nullptr) {
			// [[BPR-1558]]. The classifications those reads served go with
			// them. Left in flight, a request for the same blob made before
			// their failed terminals arrive would join one, and that pane
			// would stay plain with nothing left to ask again.
			AppearanceHost->cancel_classifications();
		}
		// The definitions go with them: nothing on screen can be adopted for
		// a selection that has been cleared, and keeping the choices would
		// only let a later publication colour a pane the reviewer has left.
		ClassifiedDefinitions.clear();
		if (Window != nullptr) {
			Window->comparison_surface()->clear();
		}
	}

	/// \brief [[BPR-1277]]. Whether `aRequest` asks again for exactly the
	/// diff on screen: the same authority, comparison context and file
	/// entry as the shown publication, whose key the surface presents.
	/// Only the request's context (its generation) may differ.
	auto github_application::implementation::shows_selected_diff(const application::workbench_selected_diff_request& aRequest) const -> bool
	{
		const auto* const surface = Window == nullptr ? nullptr : Window->comparison_surface();
		return SelectedDiffRequest.has_value() && SelectedDiffPublication.has_value() && surface != nullptr
			&& application::same_selected_diff(*SelectedDiffRequest, aRequest)
			&& application::same_selected_diff(SelectedDiffPublication->Request, aRequest)
			&& surface->current_result() && surface->current_context()
			&& surface->current_context()->Key == SelectedDiffPublication->Key;
	}

	/// \brief [[BPR-1305]]. Re-observe a kept tab under the new file
	/// snapshot without clearing its reading while the actor resolves it.
	/// The accepted same-head handoff and tab restoration bound this path;
	/// publication still has to prove the complete material key unchanged.
	auto github_application::implementation::revalidates_kept_tab(const application::workbench_selected_diff_request& aRequest) const -> bool
	{
		return RevalidatingKeptTabView && TabLoad && SelectedDiffPublication
			&& SelectedDiffPublication->Request.Diff.ApplicationSession == aRequest.Diff.ApplicationSession
			&& SelectedDiffPublication->Request.Diff.Authority == aRequest.Diff.Authority
			&& SelectedDiffPublication->Request.Diff.Comparison == aRequest.Diff.Comparison
			&& SelectedDiffPublication->Request.Diff.OptionsDigest == aRequest.Diff.OptionsDigest
			&& SelectedDiffPublication->Request.Diff.Algorithm == aRequest.Diff.Algorithm
			&& SelectedDiffPublication->Request.File == aRequest.File
			&& Window->comparison_surface()->current_result()
			&& Window->comparison_surface()->current_result()->Alignment.Key == SelectedDiffPublication->Key;
	}

	void github_application::implementation::begin(
		const application::workbench_selected_diff_request& aRequest)
	{
		if (Closing || EpochHost == nullptr
			|| EpochHost->context(aRequest.WorkbenchAuthority)
				!= std::optional{ aRequest.Diff }) {
			return;
		}
		// [[BPR-1527]]. A stop's source load is navigation, not Seen.
		// Bind the exclusion to its admitted request; a normal file gesture
		// clears it before dispatch, regardless of the visible Tour tray.
		const auto tourSource = PendingTourSource && TourOwner
			&& aRequest.File.File == core::file_identity_of(PendingTourSource->Path)
			&& aRequest.WorkbenchAuthority.Host == TourOwner->Host
			&& aRequest.WorkbenchAuthority.HostAuthority == TourOwner->HostAuthority
			&& aRequest.WorkbenchAuthority.Account == TourOwner->Account
			&& aRequest.WorkbenchAuthority.Repository == TourOwner->Repository
			&& aRequest.WorkbenchAuthority.PullRequest == TourOwner->PullRequest;
		if (tourSource || (TourSourceRequest && application::same_selected_diff(*TourSourceRequest, aRequest))) {
			TourSourceRequest = aRequest;
		} else {
			TourSourceRequest.reset();
		}
		// [[BPR-1277]]. An epoch refresh asks again for the selected file.
		// When that is exactly the diff on screen, the reviewer's selection,
		// the draft they are writing and any publish in flight stay: clearing
		// here dropped the draft, and the diff presented afresh put the
		// selection on its first change instead.
		if (shows_selected_diff(aRequest) || revalidates_kept_tab(aRequest)) {
			SelectedDiffRequest = aRequest;
#if defined(BPR_BUILD_TESTING)
			++SelectedDiffBegins;
			++SelectedDiffBeginsKept;
#endif
			return;
		}
		clear_diff_selection();
		SelectedDiffRequest = aRequest;
		if (Window != nullptr) {
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			Window->clear_review_ledger(!ReviewLedgerAvailable);
		}
		// [[BPR-1871]]. The file's comparison is being read: until it is
		// asked of the diff host, the comparison area says so by name
		// rather than "No file open".
		OpenedFileFailure.reset();
		OpenedFileFailed.reset();
		OpenedFileFailureWithheld = false;
		if (Window != nullptr) {
			Window->comparison_surface()->set_waiting(QString::fromStdString(aRequest.File.PresentationPath), false);
		}
		// The opened file's reads go next: a preparation round of other
		// files already running is stopped and starts again once the
		// reviewer's own work is done ([[BPR-1344]]).
		const auto prepared = std::ranges::any_of(PreparedWindow, [&aRequest](const application::workbench_selected_diff_request& aPrepared) {
			return aPrepared.File.File == aRequest.File.File;
		});
		if (core::opening_preempts_preparation(PreparationsInFlight.load(std::memory_order_acquire) != 0U, prepared)) {
			cancel_preparation();
			PreparationDeferred = true;
#if defined(BPR_BUILD_TESTING)
			++PreparationsPreempted;
#endif
		}
#if defined(BPR_BUILD_TESTING)
		++SelectedDiffBegins;
#endif
	}

	void github_application::implementation::publish(
		const application::workbench_selected_diff_publication& aPublication)
	{
		if (Closing || EpochHost == nullptr || WorkbenchHost == nullptr
			|| WorkbenchHost->generation()
				!= aPublication.Request.Context.Generation
			|| WorkbenchHost->selected()
				!= std::optional{ aPublication.Request.File.File }
			|| EpochHost->context(aPublication.Request.WorkbenchAuthority)
				!= std::optional{ aPublication.Request.Diff }) {
			return;
		}
		// [[BPR-1277]]. The same diff, already on screen, is adopted under
		// its new request without presenting it again. Different bytes under
		// a kept request are a new diff: the selection goes, as it would have.
		if (SelectedDiffPublication.has_value() && SelectedDiffRequest == std::optional{ aPublication.Request }
			&& (application::same_selected_diff(SelectedDiffPublication->Request, aPublication.Request)
				|| revalidates_kept_tab(aPublication.Request))) {
			if (SelectedDiffPublication->Key == aPublication.Key) {
				SelectedDiffPublication = aPublication;
				// [[BPR-1570]]. No new publication follows, so the base
				// numbering is checked against the adopted request here,
				// where its kept answer normally still holds.
				observe_base_numbering();
				// [[BPR-1707]]. So are a commit-filtered diff's line facts,
				// which the epoch host answers only under the current
				// request: before, the placement waited on the old answer
				// until another file was chosen and this one again.
				request_filtered_comment_lines();
				present_filtered_comment();
				present_review_comment_holds();
				observe_head_moves();
#if defined(BPR_BUILD_TESTING)
				++SelectedDiffPublications;
				LastSelectedDiff = aPublication;
#endif
				schedule_tab_presentation();
				return;
			}
			const auto request = aPublication.Request;
			clear_diff_selection();
			SelectedDiffRequest = request;
		}
		SelectedDiffPublication = aPublication;
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1753]], owner decision (a): Review <open file path>.
		Window->agent_tab()->refresh_suggestions();
		present_agent_pull_request();
#endif
		// RFC-BPR-0089 M4. A commit-filtered diff's threads are marked where
		// the core places them on it (`place_filtered_threads`).
		Window->set_thread_marks_suspended(false);
		present_split_strip();
		request_filtered_comment_lines();
		present_filtered_comment();
		// [[BPR-1735]]. GitHub's own diff can be read once a file is shown.
		present_review_comment_holds();
		observe_head_moves();
		request_diff(
			aPublication.Key,
			aPublication.Request.Diff.Comparison,
			aPublication.Request.Diff.Options,
			aPublication.Request.File.PresentationPath);
#if defined(BPR_BUILD_TESTING)
		++SelectedDiffPublications;
		LastSelectedDiff = aPublication;
#endif
	}

	void github_application::implementation::fail(
		const application::workbench_selected_diff_observation& aFailure)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		// [[BPR-1871]]. Only the opened file's current request fails it.
		const auto current = SelectedDiffRequest == std::optional{ aFailure.Request };
		if (SelectedDiffRequest == std::optional{ aFailure.Request }) {
			SelectedDiffRequest.reset();
			// RFC-BPR-0089 M3. A failed diff has no strip over it.
			Window->present_split_diff(std::nullopt);
			if (TabDriver && TabDriver->position().File == aFailure.Request.File.File
				&& TabDriver->position().Comparison == aFailure.Request.Diff.Comparison) {
				TabFailed = true;
				schedule_tab_presentation();
			}
		}
		if (ReadingRestoration) {
			ReadingFailed = true;
			schedule_reading_restoration();
		}
		const auto failure = [&aFailure] {
			switch (aFailure.Status) {
			case application::workbench_selected_diff_status::binary:
				return application::workbench_host_failure::selected_file_binary;
			case application::workbench_selected_diff_status::missing_left:
				return application::workbench_host_failure::selected_file_missing_left;
			case application::workbench_selected_diff_status::missing_right:
				return application::workbench_host_failure::selected_file_missing_right;
			case application::workbench_selected_diff_status::both_missing:
				return application::workbench_host_failure::selected_file_missing_both;
			case application::workbench_selected_diff_status::ambiguous_left:
			case application::workbench_selected_diff_status::ambiguous_right:
			case application::workbench_selected_diff_status::ambiguous_both:
				return application::workbench_host_failure::selected_file_ambiguous;
			case application::workbench_selected_diff_status::cancelled:
				return application::workbench_host_failure::selected_file_cancelled;
			case application::workbench_selected_diff_status::stale:
				return application::workbench_host_failure::selected_file_stale;
			case application::workbench_selected_diff_status::split_by_commit_filter:
				return application::workbench_host_failure::selected_file_split_by_commit_filter;
			case application::workbench_selected_diff_status::available:
			case application::workbench_selected_diff_status::failure:
				return application::workbench_host_failure::host_failure;
			}
			return application::workbench_host_failure::host_failure;
		}();
		Window->fail(application::workbench_failure_publication{
			.Context = aFailure.Request.Context,
			.Failure = failure,
		});
		// [[BPR-1871]]. The opened file's own read failed: its sentence
		// stands in its place.
		if (current && WorkbenchHost != nullptr && WorkbenchHost->selected() == std::optional{ aFailure.Request.File.File }) {
			auto publication          = application::workbench_failure_publication{ };
			publication.Context       = aFailure.Request.Context;
			publication.Failure       = failure;
			OpenedFileFailure         = publication;
			OpenedFileFailed          = aFailure.Request.File.File;
			OpenedFileFailureWithheld = false;
			present_opened_file();
		}
#if defined(BPR_BUILD_TESTING)
		++SelectedDiffFailures;
#endif
	}

	/// \brief Prepares the files around the cursor once this turn is over.
	///
	/// \details [[BPR-1327]]. Coalesced: every trigger in one turn (the file
	/// list, the cursor, an available comparison) leads to one look at the
	/// window, and an unchanged window starts nothing.
	void github_application::implementation::schedule_preparation()
	{
		if (Closing || PreparationScheduled || Window == nullptr) {
			return;
		}
#if defined(BPR_BUILD_TESTING)
		if (PreparationDisabled) {
			return;
		}
#endif
		PreparationScheduled = true;
		QTimer::singleShot(0, Window.get(), [this] {
			PreparationScheduled = false;
			// [[BPR-1344]]. Not while the reviewer waits on their own work:
			// what is in flight cannot be preempted, so preparation never
			// starts ahead of it. The reviewer's lane resumes it.
			if (reviewer_busy()) {
				PreparationDeferred = true;
				return;
			}
			start_preparation();
		});
	}

	/// \brief [[BPR-1344]]. Whether the reviewer has diff, workbench or
	/// epoch work outstanding (preparation's own epoch jobs excluded), or a
	/// classification of what is on screen still to arrive.
	auto github_application::implementation::reviewer_busy() const -> bool
	{
		return (DiffHost != nullptr && DiffHost->outstanding_jobs() != 0U)
			|| (WorkbenchHost != nullptr && WorkbenchHost->outstanding_requests() != 0U)
			|| (EpochHost != nullptr && EpochHost->active_reviewer_jobs() != 0U)
			|| (AppearanceHost != nullptr
				&& AppearanceHost->classification_terminals() != AppearanceHost->classification_submissions());
	}

	/// \brief [[BPR-1344]]. After each task of the reviewer's hosts, and after
	/// a syntax classification is adopted (its blob read is reviewer epoch
	/// work answered outside those hosts' lanes): a deferred round is
	/// scheduled again once the reviewer's work is done.
	void github_application::implementation::after_reviewer_decision()
	{
		if (!PreparationDeferred || Closing || reviewer_busy()) {
			return;
		}
		PreparationDeferred = false;
		schedule_preparation();
	}

	void github_application::implementation::cancel_preparation()
	{
		PreparationStop.request_stop();
		PreparationStop = std::stop_source{ };
		++PreparationGeneration;
		PreparedWindow.clear();
		// [[BPR-1344]]. A round deferred for the reviewer is cancelled with
		// the rest: an invalidated review (a prune, a cache delete) owes no
		// preparation until its next publication asks for one.
		PreparationDeferred = false;
	}

	void github_application::implementation::start_preparation()
	{
		if (Closing || EpochHost == nullptr || WorkbenchHost == nullptr || DiffHost == nullptr) {
			return;
		}
#if defined(BPR_BUILD_TESTING)
		if (PreparationDisabled) {
			return;
		}
#endif
		auto window = WorkbenchHost->preparation_window();
		// The selected file is already being opened by the reviewer's own
		// request; preparing it too would only compete with that.
		if (const auto& selected = WorkbenchHost->selected(); selected.has_value()) {
			std::erase_if(window, [&selected](const application::workbench_selected_diff_request& aRequest) {
				return aRequest.File.File == *selected;
			});
		}
		const auto same = [](const application::workbench_selected_diff_request& aLeft,
							 const application::workbench_selected_diff_request& aRight) {
			return aLeft.WorkbenchAuthority == aRight.WorkbenchAuthority && aLeft.Diff == aRight.Diff
				&& aLeft.File == aRight.File;
		};
		if (std::ranges::equal(window, PreparedWindow, same)) {
			return;
		}
		cancel_preparation();
		PreparedWindow = window;
		if (window.empty()) {
			DiffHost->prepare({ });
			return;
		}
		const auto  generation = PreparationGeneration;
		auto* const epoch      = EpochHost.get();
#if defined(BPR_BUILD_TESTING)
		// [[BPR-1871]]. A round started while the opened file is still
		// waiting for its own comparison would have gone ahead of it.
		if (WorkbenchHost->selected().has_value()
			&& !(SelectedDiffPublication.has_value() && SelectedDiffPublication->Request.File.File == *WorkbenchHost->selected())) {
			++PreparationsStartedBeforeOpenedFile;
		}
#endif
		PreparationsInFlight.fetch_add(1U, std::memory_order_acq_rel);
		try {
			PrepareEffects.post([this, epoch, generation, requests = std::move(window), stop = PreparationStop.get_token()] {
				auto found = application::epoch_prepared_comparisons{ };
				try {
					found = epoch->prepare(requests, stop);
				} catch (...) {
					found = { };
				}
				try {
					DecisionLane.post([this, generation, prepared = std::move(found)]() mutable {
						finish_preparation(generation, std::move(prepared));
						// Last: once this reaches zero the closing window may be destroyed.
						PreparationsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
					});
				} catch (...) {
					PreparationsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
				}
			});
		} catch (...) {
			PreparationsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
		}
	}

	void github_application::implementation::finish_preparation(const std::uint64_t aGeneration, application::epoch_prepared_comparisons aPrepared)
	{
		if (Closing || aGeneration != PreparationGeneration || DiffHost == nullptr) {
			return;
		}
		auto requests = std::vector<application::prepared_diff_request>{ };
		requests.reserve(aPrepared.Observations.size());
		for (const auto& observation : aPrepared.Observations) {
			if (const auto key = application::selected_diff_key(observation); key.has_value()) {
				requests.push_back(application::prepared_diff_request{
					.Key     = *key,
					.Options = observation.Request.Diff.Options,
					.Path    = observation.Request.File.PresentationPath,
				});
			}
		}
		DiffHost->prepare(std::move(requests));
#if defined(BPR_BUILD_TESTING)
		++PreparationRounds;
		PreparedResolutions += aPrepared.Observations.size();
		PreparedRemoteBlobs += aPrepared.RemoteBlobs;
		PreparedRemoteBatches += aPrepared.RemoteBatches;
#endif
	}

	/// \brief Requests one comparison and the classification of its blobs.
	///
	/// \details The path is not part of the comparison key and deliberately
	/// stays out of it: the key identifies a file by digest, and a second
	/// spelling of the same file inside it would be a second identity. It
	/// is carried alongside because choosing a syntax definition is the one
	/// thing that needs the name a file was given.
	void github_application::implementation::request_diff(
		core::file_comparison_key    aKey,
		const core::comparison_pair& aComparison,
		core::diff_options           aOptions,
		const std::string_view       aPath)
	{
		if (Closing || DiffHost == nullptr) {
			return;
		}
		// GitHub's current PR diff uses a merge-base coordinate on LEFT.
		// LEFT stays unplaced for the new comparison until
		// `observe_base_numbering` answers that its left document is the
		// version of the merge base GitHub named ([[BPR-1560]]: Whole PR's
		// left commit is that merge base) or the same blob ([[BPR-1212]]),
		// instead of applying a numeric line to a potentially unrelated blob.
		constexpr auto leftHasCurrentDiffRevision = false;
		const auto     rightIsCurrentHead         = conversation_endpoint_is_current(
			aComparison, core::diff_side::right);
		if (Window != nullptr) {
			Window->set_conversation_endpoint_placement(
				leftHasCurrentDiffRevision, rightIsCurrentHead);
		}
		DiffHost->request(std::move(aKey), std::move(aOptions), std::string{ aPath });
		if (!DiffHost->state().Pending.has_value()) {
			return;
		}
		const auto pending = *DiffHost->state().Pending;
		// Clears the previous comparison's spans before the new ones are
		// asked for, so a classification that lands first is not thrown
		// away by the surface it was meant for.
		Window->comparison_surface()->set_loading(pending, QString::fromUtf8(aPath.data(), static_cast<qsizetype>(aPath.size())));
		classify_comparison(pending, aPath);
	}

	/// \brief Asks the appearance host to classify both sides of one
	/// comparison.
	///
	/// \details Nothing here classifies. The host answers from its cache
	/// when the blob and definition are already known and reaches the
	/// classifier only when they are not, which is the whole reason the
	/// cache key carries no colour.
	void github_application::implementation::classify_comparison(
		const core::diff_request_context& aContext,
		const std::string_view            aPath)
	{
		if (AppearanceHost == nullptr || SyntaxBlobs == nullptr) {
			return;
		}
		SyntaxBlobs->register_comparison(aContext);
		const auto definition = SyntaxDefinitions->definition_for_path(aPath);
		for (const auto side :
			 { core::diff_side::left, core::diff_side::right }) {
			const auto& blob = side == core::diff_side::left
				? aContext.Key.LeftBlob
				: aContext.Key.RightBlob;
			if (!blob.has_value()) {
				continue;
			}
			// Retained so the display path can rebuild this exact cache key
			// without the file's name. The name is only needed to choose a
			// definition, and that choice has already been made here.
			retain_definition(*blob, definition);
			// [[BPR-1576]]. Said as coming before it is asked for: a cached
			// answer is adopted inside `classify` and settles it at once.
			if (Window != nullptr && Window->comparison_surface() != nullptr) {
				Window->comparison_surface()->await_syntax(side);
			}
			AppearanceHost->classify(application::token_span_cache_key{
				.Blob       = *blob,
				.Definition = definition,
			});
		}
	}

	/// \brief [[BPR-1714]]. Asks again for the C++ spans of the
	/// comparison on screen, under the known namespaces now in effect.
	///
	/// \details The panes keep the colours they have until the answer
	/// arrives, and say the colours are coming. A list in effect before
	/// answers from the cache at once; a new one is classified. Only C++
	/// reads the list, so no other language is asked again.
	void github_application::implementation::reclassify_shown_comparison()
	{
		if (Closing || Window == nullptr || AppearanceHost == nullptr || SyntaxBlobs == nullptr) {
			return;
		}
		auto* const surface = Window->comparison_surface();
		if (surface == nullptr || !surface->current_context().has_value()) {
			return;
		}
		const auto context = *surface->current_context();
		auto       wanted  = std::vector<std::pair<core::diff_side, application::token_span_cache_key>>{ };
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			const auto& blob = side == core::diff_side::left ? context.Key.LeftBlob : context.Key.RightBlob;
			if (!blob.has_value()) {
				continue;
			}
			const auto known = std::ranges::find_if(ClassifiedDefinitions, [&blob](const auto& aEntry) {
				return aEntry.first == *blob;
			});
			if (known == ClassifiedDefinitions.end() || known->second.Name != application::cpp_syntax_definition_name) {
				continue;
			}
			wanted.emplace_back(side, application::token_span_cache_key{ .Blob = *blob, .Definition = known->second });
		}
		if (wanted.empty()) {
			return;
		}
		// The reads end with each answer, so they are bound again.
		SyntaxBlobs->register_comparison(context);
		for (const auto& [side, key] : wanted) {
			surface->await_syntax(side);
		}
		for (auto& [side, key] : wanted) {
			AppearanceHost->classify(std::move(key));
		}
	}

	/// \brief Remembers which definition a blob was classified against.
	///
	/// \details Bounded and insert-if-absent for the same reason as the blob
	/// bindings: a blob is content-addressed, so the definition chosen for it
	/// once is the definition for it. BPR-0390.
	void github_application::implementation::retain_definition(
		const core::git_blob_oid&                     aBlob,
		const application::syntax_definition_version& aDefinition)
	{
		const auto known = std::ranges::find_if(
			ClassifiedDefinitions,
			[&aBlob](const auto& aEntry) { return aEntry.first == aBlob; });
		if (known != ClassifiedDefinitions.end()) {
			return;
		}
		if (ClassifiedDefinitions.size() >= MaximumClassifiedDefinitions) {
			ClassifiedDefinitions.erase(ClassifiedDefinitions.begin());
		}
		ClassifiedDefinitions.emplace_back(aBlob, aDefinition);
	}

	/// \brief Colours the comparison the panes are actually showing.
	///
	/// \details Adoption used to depend entirely on delivery order. A
	/// classification and a diff result are two unordered publications into
	/// one pane, `set_loading` clears spans on every new request, and
	/// `adopt_classification` drops anything whose blob is no longer the
	/// current comparison's — so a classification that arrived a moment too
	/// early was wiped and one that arrived a moment too late was refused.
	/// Measured on the recorded course, four of seven publications were
	/// dropped and both panes ended unclassified while every composition
	/// marker stayed green.
	///
	/// This asks the opposite question at the one moment it can be answered:
	/// the comparison is on screen now, so if its spans are already known,
	/// adopt them now. Guarded by what the surface itself reports rather than
	/// by the retained selection, because `composition_diff_projection`
	/// forwards every publication to this sink while `diff_workbench::publish`
	/// accepts only the current one. BPR-0390.
	void github_application::implementation::adopt_published_classification(
		const application::diff_publication& aPublication)
	{
		if (
			Closing || Window == nullptr || AppearanceHost == nullptr
			|| aPublication.Result == nullptr) {
			return;
		}
		auto* const surface = Window->comparison_surface();
		const auto& shown   = surface->current_result();
		if (!shown || shown->Alignment.Key != aPublication.Context.Key) {
			return;
		}
		for (const auto side :
			 { core::diff_side::left, core::diff_side::right }) {
			const auto& blob = side == core::diff_side::left
				? aPublication.Context.Key.LeftBlob
				: aPublication.Context.Key.RightBlob;
			if (!blob.has_value()) {
				continue;
			}
			const auto known = std::ranges::find_if(
				ClassifiedDefinitions,
				[&blob](const auto& aEntry) {
					return aEntry.first == *blob;
				});
			if (known == ClassifiedDefinitions.end()) {
				continue;
			}
			// A synchronous read of what the host already holds. The
			// submission in `classify_comparison` is deliberately kept as
			// well: `cached` does not refresh the retention order, so a
			// repeatedly displayed blob would become evictable if this
			// replaced it rather than joining it.
			auto spans = AppearanceHost->cached(
				application::token_span_cache_key{
					.Blob       = *blob,
					.Definition = known->second,
				});
			if (!spans) {
				continue;
			}
			if (surface->adopt_token_spans(side, std::move(spans))) {
				++DisplayAdoptedClassifications;
			}
		}
	}

	/// \brief Hands one published classification to every side it describes.
	///
	/// \details The side is decided by blob identity, which is the only
	/// thing that can decide it: a classification names a blob, and a
	/// comparison is the only thing that knows which side that blob is on.
	/// A classification for neither side belongs to a comparison the
	/// reviewer has already left, and is dropped.
	///
	/// [[BPR-1558]]. Both sides can name the same blob: a pure rename, or
	/// a file the two compared updates share. `classify_comparison` then
	/// asks once, the host coalesces the second request, and this one
	/// publication is the only answer either pane will get. Choosing the
	/// first matching side coloured the left pane and left the right plain
	/// until the reviewer came back to the file and the display's cache
	/// read gave both panes the spans.
	void github_application::implementation::adopt_classification(
		const application::token_span_publication& aSpans)
	{
		// A terminal has arrived for this blob, so whatever read it needed is
		// over and its binding can go. Released before the guards below,
		// because the binding's lifetime is the classification's rather than
		// this adoption's, and a dropped adoption still ends the read.
		// BPR-0390.
		//
		// [[BPR-1714]]. Except for an answer under a known-namespace
		// list no longer in effect: the request for the current list may
		// still need the same binding, and its own answer releases it.
		const auto stale = AppearanceHost != nullptr && AppearanceHost->classification_key(aSpans.Key) != aSpans.Key;
		if (SyntaxBlobs != nullptr && !stale) {
			SyntaxBlobs->release_blob(aSpans.Key.Blob);
		}
		// [[BPR-1344]]. The classifier's blob read was reviewer epoch work
		// answered outside the reviewer's lanes: once this adoption is done,
		// a deferred preparation round may start.
		const auto recheck = qScopeGuard([this] { after_reviewer_decision(); });
		if (Closing || Window == nullptr) {
			return;
		}
		// [[BPR-1382]]. The host retains the previous accepted diff while
		// its replacement loads. Cached spans arrive synchronously, before
		// the replacement diff does, and belong to the surface's loading
		// context. Keep them there through staged large-document adoption;
		// that later Qt turn has no second host publication to restore them.
		auto* const surface = Window->comparison_surface();
		const auto& context = surface->current_context();
		if (!context) {
			++DroppedClassifications;
			return;
		}
		// [[BPR-1714]]. Spans for a known-namespace list no longer in
		// effect are stale: an answer for the current list replaces them
		// or already has.
		if (stale) {
			++DroppedClassifications;
			return;
		}
		const auto blob  = std::optional{ aSpans.Key.Blob };
		const auto left  = context->Key.LeftBlob == blob;
		const auto right = context->Key.RightBlob == blob;
		if ((!left && !right) || !aSpans.Spans) {
			++DroppedClassifications;
			return;
		}
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			if (!(side == core::diff_side::left ? left : right)) {
				continue;
			}
			if (surface->adopt_token_spans(side, aSpans.Spans)) {
				++AdoptedClassifications;
			} else {
				++DroppedClassifications;
			}
		}
	}

	/// \brief Reads the operating system's appearance as a bounded fact.
	auto github_application::implementation::native_appearance_observation() noexcept
		-> core::appearance_observation
	{
		auto* const hints = QGuiApplication::styleHints();
		if (hints == nullptr) {
			return { };
		}
		auto observation = core::appearance_observation{ };
		switch (hints->colorScheme()) {
		case Qt::ColorScheme::Light:
			observation.Scheme = core::os_color_scheme::light;
			break;
		case Qt::ColorScheme::Dark:
			observation.Scheme = core::os_color_scheme::dark;
			break;
		case Qt::ColorScheme::Unknown:
			// [[BPR-0645]]. The platform palette is derived from the
			// same theme, so its window colour is the best remaining
			// evidence of the scheme when the hint has no answer. A
			// wrong guess here renders one scheme's palette on the
			// other's chrome, which is exactly the defect this avoids.
			observation.Scheme
				= QGuiApplication::palette().color(QPalette::Window).lightness()
					< 128
				? core::os_color_scheme::dark
				: core::os_color_scheme::light;
			break;
		}
		return observation;
	}

	/// \brief Reports a changed operating-system appearance to both owners.
	void github_application::implementation::observe_native_appearance()
	{
		if (Closing || !AppearanceProfile) {
			return;
		}
		const auto observation = native_appearance_observation();
		// Both, because they own different things: the host records the
		// observation against its durable settings, and the profile
		// re-resolves what presentation should look like. Telling only one
		// leaves them disagreeing about the same fact.
		if (AppearanceHost) {
			AppearanceHost->observe(observation);
		}
		AppearanceProfile->observe(observation);
	}

	/// \brief Applies the resolved appearance to the composed window.
	/// \brief Routes one settings gesture to the appearance host.
	///
	/// \details The host decides. It validates, refuses whole, persists
	/// what it accepted, and publishes; nothing here judges a value or
	/// touches presentation, so a gesture cannot change the screen without
	/// changing the record.
	void github_application::implementation::dispatch_appearance_intent(application::appearance_intent aIntent)
	{
		if (Closing || AppearanceHost == nullptr || !AppearanceProfile) {
			return;
		}
		// RFC-BPR-0034: a commit, a reset and an export act on the theme on
		// screen. The composition is the only place that holds both the
		// selected mode and the native observation the core resolves that
		// theme from, so it is the composition that names the variant on every
		// scoped gesture. Qt never derives it: a widget that guessed would bank
		// one theme's colours against another whenever the desktop had moved
		// since the gesture.
		const auto variant = AppearanceProfile->variant();
		std::visit(
			[this, variant](auto&& aValue) {
				using intent = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<
								  intent,
								  application::select_appearance_mode_intent>) {
					AppearanceHost->select(aValue.Mode);
				} else if constexpr (std::is_same_v<
										 intent,
										 application::apply_dif_preferences_intent>) {
					AppearanceHost->apply(aValue.Preferences);
				} else if constexpr (std::is_same_v<
										 intent,
										 application::apply_typography_intent>) {
					AppearanceHost->apply(std::move(aValue.Typography));
				} else if constexpr (std::is_same_v<intent, application::step_code_zoom_intent>) {
					AppearanceHost->step_code_zoom(aValue.Step);
				} else if constexpr (std::is_same_v<
										 intent,
										 application::preview_appearance_palette_intent>) {
					if (AppearanceProfile) {
						AppearanceProfile->preview_palette(std::move(aValue.Palette));
					}
				} else if constexpr (std::is_same_v<
										 intent,
										 application::commit_appearance_palette_intent>) {
					AppearanceHost->apply(std::move(aValue.Palette), variant);
				} else if constexpr (std::is_same_v<
										 intent,
										 application::cancel_appearance_palette_preview_intent>) {
					AppearanceHost->cancel_theme_import();
					if (AppearanceProfile) {
						AppearanceProfile->cancel_palette_preview();
					}
				} else if constexpr (std::is_same_v<intent, application::reset_appearance_intent>) {
					AppearanceHost->reset(aValue.Scope, variant);
				} else if constexpr (std::is_same_v<intent, application::apply_known_namespaces_intent>) {
					AppearanceHost->apply(std::move(aValue.Namespaces));
				} else if constexpr (std::is_same_v<intent, application::apply_avatar_style_intent>) {
					AppearanceHost->apply(aValue.Style);
				} else if constexpr (std::is_same_v<intent, application::export_appearance_theme_intent>) {
					// The theme on screen, built-in colours included. The
					// record holds only what a reviewer customised, so
					// exporting its slot alone would write a document of the
					// roles they never touched as transparent. That theme is
					// the document's base ([[BPR-1479]]).
					AppearanceHost->export_theme_file(std::move(aValue.Path), AppearanceProfile->palette_for(variant), variant);
				} else {
					AppearanceHost->import_theme_file(std::move(aValue.Path));
				}
			},
			std::move(aIntent));
	}

	/// \brief Puts the host's accepted settings into presentation.
	///
	/// \details Read off the host rather than off the intent that caused
	/// the change: an intent is what was asked for, and the host is the
	/// only thing that knows what was accepted. Offering the intent to
	/// presentation would show a refused value as though it had applied.
	///
	void github_application::implementation::adopt_appearance_settings(const bool aPaletteCommitted)
	{
		if (Closing || AppearanceHost == nullptr || !AppearanceProfile) {
			return;
		}
		const auto settings = AppearanceHost->settings();
		static_cast<void>(AppearanceProfile->select_mode(settings.Mode));
		// The whole per-variant table, adopted whole. Each theme's palette is
		// read against its own built-in profile, so a light customisation
		// cannot become the dark profile's overrides ([[BPR-0645]]) and a theme
		// with no customisation of its own renders its built-in palette
		// untouched ([[BPR-0216]]). This replaces the pair of branches the
		// single-palette record needed, where "nothing stored" could only mean
		// "restore every theme".
		static_cast<void>(AppearanceProfile->adopt_palettes(
			settings.Palettes,
			aPaletteCommitted ? qt::palette_preview_adoption::commit
							  : qt::palette_preview_adoption::retain));
		const auto selection
			= AppearanceProfile->select_typography(settings.Typography);
		if (!selection.accepted()) {
			// The host validated with the same core function, so this
			// cannot happen without the two disagreeing about legality.
			// Counted rather than ignored, because a silent divergence
			// here is exactly the kind of thing that stays invisible.
			++AppearanceDivergences;
		}
		if (Window != nullptr) {
			Window->adopt_dif(AppearanceHost->dif_resolution());
			// [[BPR-1807]]. What every face in the window is drawn from.
			Window->adopt_avatar_style(settings.Avatars);
			if (auto* const panel = Window->appearance_settings();
				panel != nullptr) {
				auto activePreview
					= std::optional<core::appearance_palette>{ };
				if (AppearanceProfile->previewing_palette()) {
					activePreview = AppearanceProfile->palette();
				}
				// Supplied, never guessed. The panel names the theme its export,
				// import and reset act on; the strip captions its System tile and
				// paints every tile in that theme's own colours; the Colours list
				// sets its values in the accepted code font; and the Type preview
				// repaints. All of it comes from the profile, which is the only
				// place holding the built-in palettes and the core's own
				// resolution. [[BPR-0790]].
				panel->adopt_appearance(*AppearanceProfile);
				panel->adopt(
					settings,
					AppearanceProfile->palette_for(AppearanceProfile->variant()),
					std::move(activePreview));
				panel->adopt_motion(AppearanceHost->dif_resolution());
			}
		}
	}

	void github_application::implementation::preview_imported_theme(const core::appearance_palette& aPalette)
	{
		if (Closing || !AppearanceProfile || Window == nullptr) {
			return;
		}
		AppearanceProfile->preview_palette(aPalette);
		if (auto* const panel = Window->appearance_settings(); panel != nullptr) {
			panel->preview_import(aPalette);
		}
	}

	void github_application::implementation::report_appearance_refusal(
		const application::appearance_failure_publication& aFailure)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		// [[BPR-1576]]. A classification that failed brings no colours:
		// the panes stop saying they are coming.
		if (const auto failure = aFailure.Failure; (failure == application::appearance_host_failure::classification_unavailable
													|| failure == application::appearance_host_failure::classification_limit
													|| failure == application::appearance_host_failure::invalid_classification_key)
			&& Window->comparison_surface() != nullptr) {
			Window->comparison_surface()->settle_syntax();
		}
		if (auto* const panel = Window->appearance_settings();
			panel != nullptr) {
			switch (aFailure.Failure) {
			case application::appearance_host_failure::settings_unavailable:
				if (!AppearancePersistenceAvailable) {
					panel->report_persistence_unavailable();
				} else {
					panel->report(std::span<const core::typography_rejection>{ });
				}
				break;
			case application::appearance_host_failure::settings_rejected:
				panel->report(aFailure.Rejections);
				break;
			case application::appearance_host_failure::theme_export_unavailable:
				panel->report_theme_export(false);
				break;
			case application::appearance_host_failure::theme_document_unavailable:
				panel->report_theme_document_unavailable();
				break;
			case application::appearance_host_failure::theme_import_rejected:
				panel->report(aFailure.ThemeRejections);
				break;
			case application::appearance_host_failure::palette_contrast_rejected:
				panel->report(aFailure.ContrastRejections);
				break;
			case application::appearance_host_failure::invalid_known_namespaces:
				panel->report(aFailure.NamespaceRejections);
				break;
			case application::appearance_host_failure::unknown_appearance_mode:
			case application::appearance_host_failure::invalid_dif_preferences:
			case application::appearance_host_failure::classification_unavailable:
			case application::appearance_host_failure::classification_limit:
			case application::appearance_host_failure::invalid_classification_key:
			case application::appearance_host_failure::host_failure:
				panel->report(std::span<const core::typography_rejection>{ });
				break;
			}
		}
	}

	void github_application::implementation::apply_appearance()
	{
		if (Closing || !AppearanceProfile || Window == nullptr) {
			return;
		}
		// Applied to the window, so every composed surface inherits it.
		// `derive` maps the core's semantic roles onto Qt's palette roles,
		// which is how `code_selection_background` becomes
		// `QPalette::Highlight` - the selection colour BPR-0117 fitted for
		// contrast and which, until now, nothing applied.
		AppearanceProfile->apply_to(*Window);
		Window->set_go_to_line_colours(AppearanceProfile->color(core::appearance_role::diff_intraline_removed_background),
									   AppearanceProfile->color(core::appearance_role::diff_intraline_added_background), AppearanceProfile->color(core::appearance_role::application_text));
		if (Profile->Insights && Profile->InsightsOwner == DraftWindow) {
			AppearanceProfile->apply_to(*Profile->Insights);
			Profile->Insights->set_chart_colors(AppearanceProfile->color(core::appearance_role::state_accepted), AppearanceProfile->color(core::appearance_role::state_unread));
		}
		// QPalette carries no role for "an added line", so the comparison
		// surface is given the semantic palette directly. Without this the
		// panes paint no line backgrounds at all, which is visibly wrong
		// rather than quietly wrong.
		if (auto* const workbench = Window->comparison_surface();
			workbench != nullptr) {
			// [[BPR-1336]]. The comparison takes the code font at its zoom.
			AppearanceProfile->apply_comparison_code_font_to(*workbench);
			workbench->adopt_appearance(*AppearanceProfile);
		}
		// [[BPR-1504]]. Find in Files' side tags and match wash.
		if (auto* const search = Window->pr_search(); search != nullptr) {
			search->adopt_appearance(*AppearanceProfile);
		}
		Window->adopt_code_zoom(AppearanceProfile->typography().Code);
		if (auto* const overview = Window->overview_page(); overview != nullptr) {
			overview->adopt_appearance(*AppearanceProfile);
		}
		// [[BPR-1919]]. Fenced code in the tour's rendered text, its
		// proposals and the agent's replies, in the panes' colours.
		if (auto* const tour = Window->tour_editor(); tour != nullptr) {
			tour->set_code_theme({ .Colors = qt::code_role_colors_of(AppearanceProfile->palette()), .Variant = AppearanceProfile->variant() });
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1753]], owner look v003. The agent's answers' code in
		// the panes' colours and code font.
		if (auto* const agent = Window->agent_tab(); agent != nullptr) {
			agent->adopt_appearance(*AppearanceProfile);
		}
#endif
		// [[BPR-1156]]. Suggest change paints its code in the panes' role colours.
		if (CycleDesktop) {
			CycleDesktop->adopt_appearance(*AppearanceProfile);
		}
		if (auto* const files = Window->files_tree(); files != nullptr) {
			files->adopt_appearance(*AppearanceProfile);
		}
		// [[BPR-0776]]. Home's three state colours for the resolved variant.
		if (auto* const home = Window->home_list(); home != nullptr) {
			home->set_state_colours(qt::home_state_colours_for(*AppearanceProfile));
		}
		// [[BPR-1357]]. Home's status band marks a problem in the theme's failure ink.
		Window->set_home_failure_ink(AppearanceProfile->color(core::appearance_role::state_defect));
		// [[BPR-1576]]. A high-contrast theme draws waits without a wash.
		Window->set_pending_high_contrast(AppearanceProfile->variant() == core::appearance_variant::high_contrast_light
										  || AppearanceProfile->variant() == core::appearance_variant::high_contrast_dark);
		// [[BPR-1083]]. The label tints of the theme on screen, for every
		// surface that draws a Conventional Comments mark.
		Window->set_label_tints(qt::feedback_label_tints_of(AppearanceProfile->palette()));
		// [[BPR-1085]]. Approve's dot takes the theme's attention amber.
		Window->set_attention_colour(AppearanceProfile->color(core::appearance_role::state_attention));
		// [[BPR-1331]] v003. The private progress marks in the theme's colours.
		Window->set_progress_mark_colours(qt::progress_mark_colours_for(*AppearanceProfile));
		if (auto* const conversations = Window->conversation_panel();
			conversations != nullptr && conversations->cards() != nullptr) {
			conversations->adopt_appearance(*AppearanceProfile);
			// [[BPR-0648]]. The accent for my posts and the thread marks'
			// amber; the diff's own marks take the same two colours.
			conversations->cards()->set_colours(
				AppearanceProfile->color(core::appearance_role::state_unread),
				QColor{ 0xD2, 0x99, 0x22 });
			// [[BPR-0649]]. The feed's code excerpt on the theme's own
			// code background and ink.
			conversations->cards()->set_code_colours(
				AppearanceProfile->color(core::appearance_role::code_background),
				AppearanceProfile->color(core::appearance_role::syntax_normal));
		}
		// [[BPR-1293]]. Threads' blue, amber and line numbers.
		if (auto* const finder = Window->finder(); finder != nullptr) {
			finder->set_colours(AppearanceProfile->color(core::appearance_role::state_unread), QColor{ 0xD2, 0x99, 0x22 });
			finder->set_code_font(AppearanceProfile->code_font());
		}

		// Read back off the window rather than off the profile. The claim
		// is that the palette reached the window; asking the profile what
		// it would have applied proves only that it decided something.
		const auto& applied   = Window->palette();
		AppliedSelectionColor = applied.color(QPalette::Highlight).name().toStdString();
		AppliedBaseColor      = applied.color(QPalette::Base).name().toStdString();

		// And whether those colours actually came from the profile. Without
		// this the evidence is only that *some* colours were reported, which
		// stays true when the window keeps the platform's own palette - a
		// break that removed the application entirely still produced
		// well-formed output. Compared against the profile's semantic roles,
		// so the answer is no when presentation is still the platform's.
		AppliedFromProfile = applied.color(QPalette::Highlight)
				== AppearanceProfile->color(
					core::appearance_role::code_selection_background)
			&& applied.color(QPalette::Base)
				== AppearanceProfile->color(
					core::appearance_role::code_background);
	}
}
