#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QApplication>
#include <QByteArrayView>
#include <QChar>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethods>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief The drawer's row for one projected file.
		[[nodiscard]] auto drawer_row_for(const application::workbench_file_projection& aFile) -> workbench_drawer_row
		{
			auto identity = QString{ };
			identity.reserve(
				static_cast<qsizetype>(aFile.File.bytes().size()) * qsizetype{ 2 });
			for (const auto byte : aFile.File.bytes()) {
				identity.append(QString::number(byte, 16).rightJustified(2, QLatin1Char{ '0' }));
			}
			const auto path = QString::fromUtf8(aFile.Path.data(), static_cast<qsizetype>(aFile.Path.size()));
			return workbench_drawer_row{
				.Identity = std::move(identity),
				.Title    = path,
				.Detail   = path,
				.File     = aFile.File,
			};
		}
	}

	void workbench_window::present_repository_addition(const application::repository_addition_result& aResult)
	{
		if (RepositoryPendingGesture == 0U || aResult.Gesture != RepositoryPendingGesture || aResult.Destination != RepositoryDisplayedDestination) {
			return;
		}
		using state      = core::repository_addition_status;
		RepositorySaving = aResult.Status == state::saving;
		update_repository_destination();
		QString notice;
		switch (aResult.Status) {
		case state::saving: notice = tr("Adding repository…"); break;
		case state::saved:  {
			notice = aResult.Target ? tr("Added %1/%2.").arg(QString::fromStdString(aResult.Target->Repository.Owner), QString::fromStdString(aResult.Target->Repository.Name)) : tr("Repository added.");
			statusBar()->showMessage(notice, 6000);
			// The draft card closes into the list, as a kept server becomes its
			// card; the page says what was added and the keyboard returns to
			// Add repository.
			const auto onPage        = RepositoryDraft->isAncestorOf(QApplication::focusWidget());
			RepositoryPendingGesture = 0U;
			RepositorySaving         = false;
			RepositoryDraftOpen      = false;
			SavedRepositoryName->clear();
			repository_addition_notice({ });
			RepositoryPrepareButton->hide();
			update_repository_destination();
			RepositoriesConfirmation->setText(notice);
			RepositoriesConfirmation->show();
			QAccessibleAnnouncementEvent event{ RepositoriesConfirmation, notice };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
			if (onPage) {
				AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
			}
			return;
		}
		case state::already_added:       notice = tr("This repository is already added."); break;
		case state::invalid_input:       notice = tr("Enter owner/name or a complete HTTPS repository link without a pull-request path, query or fragment."); break;
		case state::sign_in_required:    notice = tr("Sign in to the repository's host, then return here and choose Add."); break;
		case state::destination_changed: notice = tr("This repository needs a different destination. Choose its account or explicitly set up its server, then return here and choose Add."); break;
		case state::unavailable:         notice = tr("The repository could not be saved locally. Your input is kept; choose Add to retry."); break;
		case state::busy:                notice = tr("Another repository change is being saved. Try Add again when it finishes."); break;
		case state::collection_limit:    notice = tr("The saved repository limit has been reached. Remove a repository before adding another."); break;
		}
		if (RepositoryDraftOpen) {
			repository_addition_notice(notice);
			RepositoryPrepareButton->setVisible(aResult.Target.has_value() && (aResult.Status == state::sign_in_required || aResult.Status == state::destination_changed));
		} else if (!RepositorySaving) {
			statusBar()->showMessage(notice, 6000);
		}
	}

	auto workbench_window::saved_repositories_editor() const noexcept -> QWidget*
	{
		return SavedRepositoriesEditor;
	}

	auto workbench_window::saved_repository_list() const noexcept -> QListWidget*
	{
		return SavedRepositoryList;
	}

	auto workbench_window::saved_repositories_empty() const noexcept -> QWidget*
	{
		return SavedRepositoriesEmpty;
	}

	auto workbench_window::home_repository_onboarding() const noexcept -> QWidget*
	{
		return HomeOnboarding;
	}

	auto workbench_window::home_repository_onboarding_button() const noexcept -> QPushButton*
	{
		return HomeOnboardingButton;
	}

	auto workbench_window::saved_repository_name() const noexcept -> QLineEdit*
	{
		return SavedRepositoryName;
	}

	auto workbench_window::add_saved_repository_action() const noexcept -> QAction*
	{
		return AddSavedRepository;
	}

	auto workbench_window::remove_saved_repository_action() const noexcept -> QAction*
	{
		return RemoveSavedRepository;
	}

	void workbench_window::dispatch_saved_repository(const application::saved_repository_action aAction)
	{
		if (!SavedRepositoryDispatch || !QueueReady) {
			return;
		}
		auto repository = application::saved_repository{
			.Host    = QueueProjection.Session.Host,
			.Account = QueueProjection.Session.Account,
		};
		if (aAction == application::saved_repository_action::add) {
			repository.Repository = SavedRepositoryName->text().trimmed().toStdString();
		} else {
			// The row names the exact accepted record of the displayed account.
			const auto* const item = SavedRepositoryList != nullptr ? SavedRepositoryList->currentItem() : nullptr;
			if (item == nullptr) {
				return;
			}
			repository = application::saved_repository{
				.Host       = item->data(Qt::UserRole + 1).toString().toStdString(),
				.Account    = item->data(Qt::UserRole + 2).toString().toStdString(),
				.Repository = item->data(Qt::UserRole).toString().toStdString(),
			};
			if (std::ranges::find(QueueProjection.SavedRepositories, repository) == QueueProjection.SavedRepositories.end()) {
				return;
			}
		}
		if (!application::valid(repository)) {
			return;
		}
		SavedRepositoryDispatch(application::saved_repository_intent{
			.Action     = aAction,
			.Repository = std::move(repository),
		});
	}

	void workbench_window::update_saved_repository_actions()
	{
		if (AddSavedRepository == nullptr || RemoveSavedRepository == nullptr) {
			return;
		}
		const auto account = QueueReady && static_cast<bool>(SavedRepositoryDispatch)
			&& !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		AddSavedRepository->setEnabled(static_cast<bool>(RepositoryAdditionDispatch));
		RemoveSavedRepository->setEnabled(
			account && SavedRepositoryList != nullptr && SavedRepositoryList->currentItem() != nullptr);
		AddSavedRepository->setToolTip(tr("Find my pull requests and review requests in a saved repository; nothing changes on GitHub."));
		update_repository_destination();
		RemoveSavedRepository->setToolTip(account ? QStringLiteral("Stop finding my pull requests in the selected repository; nothing changes on GitHub.")
												  : QStringLiteral("Sign in to remove a saved repository."));
		if (AddSavedRepositoryButton != nullptr) {
			AddSavedRepositoryButton->setEnabled(AddSavedRepository->isEnabled());
			AddSavedRepositoryButton->setToolTip(AddSavedRepository->toolTip());
		}
		if (RemoveSavedRepositoryButton != nullptr) {
			RemoveSavedRepositoryButton->setEnabled(RemoveSavedRepository->isEnabled());
			RemoveSavedRepositoryButton->setToolTip(RemoveSavedRepository->toolTip());
		}
	}

	void workbench_window::publish(
		const application::workbench_projection& aProjection)
	{
		const auto previousAnchor = conversation_anchor_file();
		const auto heldRefresh    = RefreshConversationAnchor.has_value();
		const auto refreshGap     = HasGithubPullRequest && previousAnchor.has_value()
			&& LastWorkbenchContext.has_value() && aProjection.Context.Session.value() != 0U
			&& aProjection.Context.Session == LastWorkbenchContext->Session
			&& aProjection.Context.Generation.value() > LastWorkbenchContext->Generation.value()
			&& aProjection.Context.Request.value() == 0U
			&& aProjection.Files.has_value() && aProjection.Files->empty()
			&& aProjection.Selection.has_value() && !aProjection.Selection->Cursor.has_value();
		// [[BPR-0818]]. The hold ends with the gap. The observation's terminal
		// publishes the new file set and cursor, and nothing restores the file
		// selection after it, so holding past it scoped the drawer to a file
		// nobody had selected and hid every review summary and issue comment.
		// The widening that ends the hold runs on its own event-loop turn:
		// re-projecting a thousand-thread pull request takes tens of
		// milliseconds, and sharing a turn with the rest of the refresh held
		// input for longer than one frame budget allows.
		if (refreshGap) {
			RefreshConversationAnchor  = previousAnchor;
			RefreshConversationContext = aProjection.Context;
		} else if (aProjection.Selection.has_value() || aProjection.Files.has_value() || aProjection.FilesDelta.has_value() || (RefreshConversationContext.has_value() && aProjection.Context.Session != RefreshConversationContext->Session)) {
			RefreshConversationAnchor.reset();
			RefreshConversationContext.reset();
		}
		LastWorkbenchContext = aProjection.Context;

		if (aProjection.Commands) {
			Commands    = *aProjection.Commands;
			HasCommands = true;
			++Statistics.CommandUpdates;
		}
		if (aProjection.Files) {
			apply_file_rows(*aProjection.Files);
		}
		if (aProjection.FilesDelta) {
			apply_file_delta(*aProjection.FilesDelta);
		}
		if (aProjection.Selection) {
			Cursor = aProjection.Selection->Cursor;
			apply_file_selection();
		}
		if (ConversationPanel != nullptr && previousAnchor != conversation_anchor_file()) {
			if (heldRefresh && !RefreshConversationAnchor.has_value() && !conversation_anchor_file().has_value()) {
				schedule_conversation_anchor();
			} else {
				apply_conversation_anchor();
			}
		}
		if (aProjection.Drawers) {
			DrawerStates = *aProjection.Drawers;
			++Statistics.DrawerUpdates;
		}
		if (aProjection.DrawerRestore) {
			DrawerRestore = *aProjection.DrawerRestore;
		}
		if (aProjection.LineNumbers) {
			LineNumberState = *aProjection.LineNumbers;
		}
		if (aProjection.OnePane) {
			OnePaneState = *aProjection.OnePane;
		}
		if (aProjection.UpdatesLine) {
			UpdatesLineState = *aProjection.UpdatesLine;
		}
		if (aProjection.Layout) {
			LayoutWidths = *aProjection.Layout;
			// [[BPR-1588]]. The window's Status drawer as the host published it.
			if (aProjection.Layout->StatusDrawer) {
				OverviewPage->status_drawer()->adopt(*aProjection.Layout->StatusDrawer);
			}
			if (aProjection.Layout->SummaryFraction) {
				OverviewPage->set_summary_fraction(*aProjection.Layout->SummaryFraction);
			}
			// [[BPR-1334]]. This window's Line details height, once loaded.
			if (aProjection.Layout->LineDetailsHeight && ComparisonSurface != nullptr) {
				ComparisonSurface->set_line_details_height(*aProjection.Layout->LineDetailsHeight);
			}
			++Statistics.LayoutUpdates;
		}
		if (aProjection.Behaviour) {
			Behaviour          = *aProjection.Behaviour;
			BehaviourPublished = true;
			AskedBehaviour.reset();
			apply_behaviour_controls();
			apply_label_marks();
			// [[BPR-1089]]. The Overview's chips and where its count starts.
			if (OverviewPage != nullptr) {
				OverviewPage->set_label_columns(Behaviour.LabelColumns);
				OverviewPage->set_default_mode(Behaviour.OverviewCounts == application::review_overview_counts::all ? core::feedback_resolution_mode::all
																													: core::feedback_resolution_mode::unresolved);
			}
			apply_line_details();
			apply_markdown_images();
			apply_reading_width();
			ComparisonSurface->set_center_ribbon_width(Behaviour.CenterRibbonWidth);
			consider_mark_read_when_fully_seen();
			// A pull request that opened before the preferences landed takes
			// them now, once each.
			if (HasGithubPullRequest) {
				apply_first_comments_filter();
				if (!ReturnComparisonAsked) {
					ReturnComparisonPending = Behaviour.ReturnComparison
						!= application::review_return_comparison::whole_pr;
					honour_return_comparison();
				}
			}
		}
		if (aProjection.HomeView) {
			// [[BPR-0783]]. The window's own Home view, as the host holds it:
			// restored at load, and published again when a view was refused so
			// Home shows what was kept rather than what was asked for.
			adopt_home_view(*aProjection.HomeView);
		}
		if (aProjection.Geometry) {
			restore_geometry(*aProjection.Geometry);
		}
		// [[BPR-1295]]. The drawer that holds focus before it may be hidden.
		auto* const focusedDrawer = aProjection.Drawers ? drawer_holding_focus() : nullptr;
		if (aProjection.Drawers || aProjection.Layout || aProjection.Behaviour) {
			apply_drawer_layout();
		}
		keep_focus_out_of_hidden_drawers(focusedDrawer);
		if (aProjection.Drawers) {
			infer_modality_from_drawers();
		}
		Failure.reset();
		++Statistics.Publications;
		update_action_state();
		update_status_text();
		// [[BPR-0727]]. A refused gesture is explained in the status bar, where
		// the toggle's own status tip already speaks; the toggle stays enabled
		// and shows the state the core kept.
		if (aProjection.Refusal) {
			LastRefusal = *aProjection.Refusal;
			statusBar()->showMessage(drawer_reason_text(*aProjection.Refusal));
		}
		announce_update();
	}

	auto workbench_window::last_refusal() const noexcept -> core::workbench_reason_code
	{
		return LastRefusal;
	}

	void workbench_window::apply_file_delta(const application::workbench_files_delta& aDelta)
	{
		// [[BPR-0972]]. The drawer list and the tree each take the few moved
		// files; neither receives the order again.
		auto inserted = std::vector<std::pair<std::size_t, workbench_drawer_row>>{ };
		auto rows     = std::vector<workbench_drawer_row>{ };
		inserted.reserve(aDelta.Inserted.size());
		rows.reserve(aDelta.Inserted.size());
		for (const auto& [position, file] : aDelta.Inserted) {
			inserted.emplace_back(position, drawer_row_for(file));
			rows.push_back(inserted.back().second);
		}
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents()->apply_delta(aDelta.Removed, inserted);
		if (FilesTree != nullptr) {
			FilesTree->apply_delta(aDelta.Removed, rows);
			refresh_filter_notices();
		}
		FileRowsPresent       = Drawers[0].Panel->contents()->rowCount() > 0;
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
		if (!PendingReviewPath.isEmpty()) {
			QTimer::singleShot(0, this, [this] { open_pending_review_path(); });
		}
	}

	void workbench_window::apply_file_rows(
		const std::vector<application::workbench_file_projection>& aFiles)
	{
		OverviewFileFactsDirty = true;
		auto rows              = std::vector<workbench_drawer_row>{ };
		rows.reserve(aFiles.size());
		for (const auto& file : aFiles) {
			rows.push_back(drawer_row_for(file));
		}
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents()->apply(rows);
		if (FilesTree != nullptr) {
			FilesTree->apply(rows);
			refresh_filter_notices();
		}
		FileRowsPresent       = !rows.empty();
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
		if (!PendingReviewPath.isEmpty()) {
			QTimer::singleShot(0, this, [this] { open_pending_review_path(); });
		}
	}

	auto workbench_window::conversation_anchor_file() const
		-> std::optional<core::file_identity>
	{
		// File selection survives the asynchronous gap between clearing the
		// previous diff and receiving the next one. Widening to the whole PR
		// in that gap rebuilds unrelated threads on every file switch.
		if (RefreshConversationAnchor.has_value()) {
			return RefreshConversationAnchor;
		}
		if (Cursor.has_value()) {
			return Cursor->File;
		}
		// Standalone diff presentation has no workbench cursor projection.
		if (ComparisonSurface != nullptr && ComparisonSurface->current_context()) {
			return ComparisonSurface->current_context()->Key.File;
		}
		return std::nullopt;
	}

	void workbench_window::apply_file_selection()
	{
		if (Drawers[0].Panel->contents() == nullptr
			|| Drawers[0].Panel->contents_view() == nullptr) {
			return;
		}
		const auto index      = Cursor && Cursor->File
			? Drawers[0].Panel->contents()->index_for_file(*Cursor->File)
			: QModelIndex{ };
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents_view()->setCurrentIndex(index);
		if (FilesTree != nullptr) {
			FilesTree->select_file(Cursor ? Cursor->File : std::nullopt);
		}
		ApplyingFileSelection = false;
	}

	void workbench_window::dispatch_file_selection(const QModelIndex& aIndex)
	{
		if (ApplyingFileSelection || !Dispatch || Drawers[0].Panel->contents() == nullptr) {
			return;
		}
		const auto file = Drawers[0].Panel->contents()->file_at(aIndex);
		if (!file.has_value()) {
			return;
		}
		++Statistics.FileSelections;
		Dispatch(core::select_workbench_file_intent{ .File = *file });
	}

	void workbench_window::publish(
		const application::epoch_presentation_update& aUpdate)
	{
		// The composed control owns every review-history decision, so the update
		// reaches it unchanged and the shell only quotes the wording it produced.
		EpochSelectors->publish(aUpdate);
		honour_return_comparison();
		HasEpochState = true;
		++Statistics.EpochUpdates;
		// [[BPR-1130]]. The disclosure repeats the line's mark and statement.
		apply_updates_line();
		// [[BPR-1132]]. The tabs name their pairs by the line's dates.
		present_document_tabs();
		update_older_pair_notice();
		update_status_text();
		// Deliberately silent. The composed control already announced this exact
		// change, and a second announcement of the same fact is the duplicated
		// chatter a combined status update exists to prevent.
	}

	void workbench_window::fail(
		const application::workbench_failure_publication& aFailure)
	{
		// A failed effect changes no command legality. The host publishes a command
		// delta when legality actually changed, so the window reports the failure in
		// text and leaves enabled state exactly where the last projection put it.

		if (RefreshConversationAnchor.has_value() && RefreshConversationContext.has_value()
			&& aFailure.Context.Session == RefreshConversationContext->Session
			&& aFailure.Context.Generation == RefreshConversationContext->Generation) {
			RefreshConversationAnchor.reset();
			RefreshConversationContext.reset();
			apply_conversation_anchor();
		}
		Failure = aFailure.Failure;
		if (aFailure.Failure == application::workbench_host_failure::selected_file_missing_left
			|| aFailure.Failure == application::workbench_host_failure::selected_file_missing_right
			|| aFailure.Failure == application::workbench_host_failure::selected_file_missing_both
			// RFC-BPR-0089: a split file reads nothing, so the previous file's
			// diff must not stay on screen under its row.
			|| aFailure.Failure == application::workbench_host_failure::selected_file_split_by_commit_filter) {
			ComparisonSurface->clear(failure_text(aFailure.Failure));
		}
		++Statistics.Failures;
		update_status_text();
		announce_update();
	}

	void workbench_window::present_opened_file(const core::opened_file_wait                             aWait,
											   const QString&                                           aPath,
											   const bool                                               aHistory,
											   const std::optional<application::workbench_host_failure> aFailure)
	{
		if (ComparisonSurface == nullptr) {
			return;
		}
		switch (aWait) {
		case core::opened_file_wait::none:
			// Only a wait this said is taken back; a comparison on screen or
			// loading under its own request stays.
			if (!ComparisonSurface->waiting_path().isEmpty() && !ComparisonSurface->current_context().has_value()) {
				ComparisonSurface->clear();
			}
			return;
		case core::opened_file_wait::loading:
			ComparisonSurface->set_waiting(aPath, aHistory);
			return;
		case core::opened_file_wait::failed:
			// [[BPR-1857]]. The sentence names the reason the file cannot open.
			ComparisonSurface->set_waiting_failed(
				aPath, failure_text(aFailure.value_or(application::workbench_host_failure::selected_file_comparison_unavailable), EpochSelectors));
			return;
		}
	}

	void workbench_window::publish(
		const application::file_receipt_action_projection& aProjection)
	{
		Receipt = aProjection;
		if (ReviewEditor != nullptr) {
			ReviewEditor->present(aProjection.Receipt);
		}
		ReceiptFailure.reset();
		LedgerUnavailable = false;
		++Statistics.ReceiptUpdates;
		update_receipt_actions();
		consider_mark_read_when_fully_seen();
		update_status_text();
		announce_update();
	}

	void workbench_window::fail(
		const application::receipt_failure_publication& aFailure)
	{
		ReceiptFailure = aFailure;
		if (aFailure.Store == application::ledger_store_failure::unavailable) {
			LedgerUnavailable = true;
		}
		++Statistics.ReceiptFailures;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::clear_review_ledger(const bool aUnavailable)
	{
		Receipt.reset();
		ReceiptFailure.reset();
		LedgerUnavailable = aUnavailable;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::set_saved_layout_unavailable(const bool aUnavailable)
	{
		if (SavedLayoutUnavailable == aUnavailable) {
			return;
		}
		SavedLayoutUnavailable = aUnavailable;
		update_status_text();
		announce_update();
	}

	void workbench_window::set_agent_workspaces_unavailable(const bool aUnavailable)
	{
		set_agent_workspaces_unavailable(aUnavailable, AgentWorkspacesReason);
	}

	void workbench_window::set_agent_workspaces_unavailable(const bool aUnavailable, QString aReason)
	{
		if (AgentWorkspacesUnavailable == aUnavailable && AgentWorkspacesReason == aReason) {
			return;
		}
		AgentWorkspacesUnavailable = aUnavailable;
		AgentWorkspacesReason      = std::move(aReason);
		update_status_text();
		announce_update();
	}

	auto workbench_window::agent_workspaces_problem_text() const -> QString
	{
		if (!AgentWorkspacesUnavailable) {
			return { };
		}
		return AgentWorkspacesReason.isEmpty()
			? QStringLiteral("Disposable agent workspaces are unavailable: their folder needs repair or has reached its limit.")
			: AgentWorkspacesReason;
	}

	void workbench_window::set_dispatch(workbench_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void workbench_window::set_change_dispatch(
		workbench_change_dispatch aChangeDispatch)
	{
		ChangeDispatch = std::move(aChangeDispatch);
	}

	void workbench_window::set_width_dispatch(
		workbench_drawer_width_dispatch aWidthDispatch)
	{
		WidthDispatch = std::move(aWidthDispatch);
	}

	void workbench_window::set_receipt_dispatch(
		workbench_receipt_dispatch aReceiptDispatch)
	{
		ReceiptDispatch = std::move(aReceiptDispatch);
	}

	void workbench_window::set_review_submission_dispatch(review_submission_editor_dispatch aDispatch) { ReviewSubmissionDispatch = std::move(aDispatch); }

	auto workbench_window::review_submission_editor_view() const noexcept -> review_submission_editor* { return ReviewSubmissionEditor; }

	void workbench_window::set_review_draft_dispatch(
		workbench_review_draft_dispatch aDraftDispatch)
	{
		ReviewDraftDispatch = std::move(aDraftDispatch);
	}

	void workbench_window::set_defect_dispatch(
		workbench_defect_dispatch aDefectDispatch)
	{
		DefectDispatch = std::move(aDefectDispatch);
	}

	void workbench_window::set_agent_dispatch(agent_intent_dispatch aAgentDispatch)
	{
		AgentPanel->set_dispatch(std::move(aAgentDispatch));
	}

	void workbench_window::set_device_authorization_dispatch(
		application::device_authorization_gesture_dispatch aDispatch)
	{
		DeviceDispatch = std::move(aDispatch);
	}

	void workbench_window::set_defect_classification_dispatch(
		workbench_defect_classification_dispatch aClassificationDispatch)
	{
		DefectClassificationDispatch = std::move(aClassificationDispatch);
		update_defect_surface();
	}

	void workbench_window::present_defects(
		std::vector<core::defect_history> aDefects, std::map<core::defect_id, std::string> aThreads)
	{
		DefectThreads = std::move(aThreads);
		std::ranges::sort(aDefects, { }, [](const core::defect_history& aDefect) {
			return aDefect.Defect.Id.value();
		});
		auto retained = std::optional<core::defect_id>{ };
		if (ShownDefectIndex.has_value() && *ShownDefectIndex < Defects.size()) {
			retained = Defects[*ShownDefectIndex].Defect.Id;
		}
		Defects = std::move(aDefects);
		ShownDefectIndex.reset();
		if (retained.has_value()) {
			const auto found = std::ranges::find(
				Defects, *retained, [](const core::defect_history& aHistory) {
					return aHistory.Defect.Id;
				});
			if (found != Defects.end()) {
				ShownDefectIndex = static_cast<std::size_t>(
					std::distance(Defects.begin(), found));
			}
		}
		if (!ShownDefectIndex.has_value() && !Defects.empty()) {
			ShownDefectIndex = 0U;
		}
		++Statistics.DefectUpdates;
		update_defect_badges();
		update_defect_surface();
	}

	void workbench_window::clear_defects()
	{
		Defects.clear();
		DefectThreads.clear();
		PendingDefectDialog.reset();
		DefectDialog->hide();
		ShownDefectIndex.reset();
		++Statistics.DefectUpdates;
		update_defect_badges();
		update_defect_surface();
	}

	void workbench_window::classify_selected_conversation()
	{
		if (!DefectClassificationDispatch) {
			return;
		}
		auto classification = selected_conversation_classification();
		if (!classification.has_value()) {
			return;
		}
		++Statistics.DefectClassifications;
		DefectClassificationDispatch(std::move(*classification));
	}

	void workbench_window::navigate_defect(const int aDirection)
	{
		if (!ShownDefectIndex.has_value() || Defects.empty()) {
			return;
		}
		const auto current = static_cast<std::ptrdiff_t>(*ShownDefectIndex);
		const auto next    = current + aDirection;
		if (next < 0 || next >= static_cast<std::ptrdiff_t>(Defects.size())) {
			return;
		}
		ShownDefectIndex = static_cast<std::size_t>(next);
		++Statistics.DefectNavigation;
		update_defect_surface();
		if (const auto thread = DefectThreads.find(Defects[*ShownDefectIndex].Defect.Id); thread != DefectThreads.end()) {
			const auto identity = thread->second;
			ConversationPanel->set_anchor_line(std::nullopt);
			ConversationPanel->set_anchor_file(std::nullopt);
			auto index = ConversationPanel->model()->index_for_id(identity);
			if (!index.isValid()) {
				// [[BPR-1293]]. A defect's thread on a file opens in Review,
				// in place; one that is not on a file still goes to Discuss.
				if (reveal_conversation_in_review(identity)) {
					return;
				}
				choose_modality(review_modality::discuss);
				index = ConversationPanel->model()->index_for_id(identity);
			}
			if (index.isValid()) {
				ConversationPanel->choose(index);
			}
			ConversationPanel->cards()->set_current_thread(identity);
		}
	}

	auto workbench_window::chosen_conversation() const -> QModelIndex
	{
		// [[BPR-0985]]. The panel's default row shows a detail without moving
		// code. Revealing it again after every published comparison moved the
		// reader to the first thread of each file, and as a reading gesture it
		// superseded the saved reading a restart was restoring.
		return ConversationPanel->current_is_default() ? QModelIndex{ } : ConversationPanel->view()->currentIndex();
	}

	void workbench_window::synchronize_selected_conversation(
		const QModelIndex& aIndex)
	{
		// A PR-wide selection reads discussion. Only Open in code may move
		// the retained comparison; an off-file row must not reveal a same-number line.
		if (ComparisonSurface == nullptr
			|| (Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads))) {
			return;
		}
		auto root = aIndex;
		while (root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto context = ComparisonSurface->current_context();
		const auto path    = root.data(
									 github_conversation_model::path_role)
								 .toString();
		if (!context.has_value() || path.isEmpty()
			|| context->Key.File != core::file_identity_of(path.toStdString())) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		bool       validLine = false;
		const auto oneBased  = root.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = root.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto side = github_conversation_source_side(sideValue.toInt());
		if (!side.has_value()) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::missing_side);
			return;
		}
		const auto semantic = ConversationPanel->model()->semantic_anchor(root);
		if (semantic && semantic->Comparison != core::comparison_digest(context->Key)) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto endpointIsCurrent = *side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		if (!semantic && !endpointIsCurrent) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		if (!ComparisonSurface->reveal_conversation_line(
				*side, static_cast<std::uint32_t>(oneBased - 1U))) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::selected_line_out_of_bounds);
		}
	}

	void workbench_window::update_defect_badges()
	{
		auto badges = conversation_cards_view::defect_badges{ };
		for (const auto& history : Defects) {
			const auto thread = DefectThreads.find(history.Defect.Id);
			if (thread == DefectThreads.end()) {
				continue;
			}
			const auto label   = tr("%1 %2 · %3").arg(badge_for(history.Defect.State), QString::fromUtf8(core::name(history.Defect.Severity)), QString::fromUtf8(core::name(history.Defect.Category)));
			const auto context = core::context_available(history) ? tr("Shared context available at the retained semantic anchor.") : tr("Shared context unavailable; the original semantic anchor is retained.");
			badges.emplace(thread->second, std::pair{ label, explanation_for(history.Defect.State) + QLatin1Char{ ' ' } + context });
		}
		ConversationPanel->cards()->set_defect_badges(std::move(badges));
	}

	void workbench_window::show_defect_dialog(core::defect_id aDefect)
	{
		const auto at = std::ranges::find(Defects, aDefect, [](const auto& aHistory) { return aHistory.Defect.Id; });
		if (at == Defects.end()) {
			return;
		}
		ShownDefectIndex = static_cast<std::size_t>(std::distance(Defects.begin(), at));
		update_defect_surface();
		DefectDialog->show();
		DefectDialog->raise();
		DefectCard->severity_selector()->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::append_thread_defect_menu(QMenu& aMenu, const std::string& aThread)
	{
		auto selected = ConversationPanel->model()->index_for_id(aThread);
		if (!selected.isValid()) {
			ConversationPanel->set_anchor_line(std::nullopt);
			ConversationPanel->set_anchor_file(std::nullopt);
			selected = ConversationPanel->model()->index_for_id(aThread);
		}
		if (selected.isValid()) {
			ConversationPanel->choose(selected);
		}
		const auto binding = std::ranges::find_if(DefectThreads, [&aThread](const auto& aEntry) { return aEntry.second == aThread; });
		const auto history = binding == DefectThreads.end() ? Defects.end() : std::ranges::find(Defects, binding->first, [](const auto& aEntry) { return aEntry.Defect.Id; });
		if (history == Defects.end()) {
			auto* action = aMenu.addAction(tr("Classify as defect…"));
			action->setObjectName(QStringLiteral("threadClassifyDefect"));
			const auto classification = selected_conversation_classification();
			const auto offered        = classification && classification->Conversation == aThread && static_cast<bool>(DefectClassificationDispatch);
			action->setEnabled(offered);
			action->setToolTip(offered ? tr("Classify this thread locally, then choose its severity and category.") : tr("Open this thread's current code comparison before classifying it."));
			connect(action, &QAction::triggered, &aMenu, [this, classification, aThread] {
				if (classification && DefectClassificationDispatch) {
					PendingDefectDialog = aThread;
					++Statistics.DefectClassifications;
					DefectClassificationDispatch(*classification);
				}
			});
			return;
		}
		// The menu owns an exact history snapshot. A later selection cannot
		// retarget an action already offered for this thread.
		auto* snapshot = new defect_card_view{ [this](core::defect_command aCommand) {
												  ++Statistics.DefectIntents;
												  if (DefectDispatch) {
													  DefectDispatch(std::move(aCommand));
												  }
											  },
											   &aMenu };
		snapshot->present(*history);
		snapshot->hide();
		for (auto* action : { snapshot->verify_action(), snapshot->reopen_action(), snapshot->withdraw_action(), snapshot->accept_as_is_action() }) {
			aMenu.addAction(action);
		}
		auto* reclassify = aMenu.addAction(tr("Reclassify…"));
		reclassify->setObjectName(QStringLiteral("threadReclassifyDefect"));
		reclassify->setEnabled(snapshot->reclassify_action()->isEnabled());
		reclassify->setToolTip(snapshot->reclassify_action()->toolTip());
		connect(reclassify, &QAction::triggered, &aMenu, [this, id = history->Defect.Id] { show_defect_dialog(id); });
	}

	void workbench_window::update_defect_surface()
	{
		ClassifyDefect->setEnabled(
			static_cast<bool>(DefectClassificationDispatch)
			&& selected_conversation_classification().has_value());
		const auto validIndex = ShownDefectIndex.has_value()
			&& *ShownDefectIndex < Defects.size();
		PreviousDefect->setEnabled(validIndex && *ShownDefectIndex > 0U);
		NextDefect->setEnabled(
			validIndex && *ShownDefectIndex + 1U < Defects.size());
		if (DefectRow != nullptr) {
			DefectRow->setVisible(!Defects.empty());
		}
		DefectCountLabel->setText(
			validIndex
				? tr("Defects %1/%2")
					  .arg(*ShownDefectIndex + 1U)
					  .arg(Defects.size())
				: tr("Defects 0/0"));
		if (validIndex) {
			DefectCard->present(Defects[*ShownDefectIndex]);
		} else {
			DefectCard->setVisible(false);
			return;
		}
		DefectCard->setVisible(true);
		if (PendingDefectDialog) {
			const auto thread = std::ranges::find_if(DefectThreads, [this](const auto& aEntry) { return aEntry.second == *PendingDefectDialog; });
			if (thread != DefectThreads.end()) {
				const auto id = thread->first;
				PendingDefectDialog.reset();
				show_defect_dialog(id);
			}
		}
	}

	auto workbench_window::selected_conversation_classification() const
		-> std::optional<workbench_defect_classification>
	{
		if (ConversationPanel == nullptr || ConversationPanel->view() == nullptr
			|| ConversationPanel->model() == nullptr || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto current = ConversationPanel->view()->currentIndex();
		if (!current.isValid() || current.parent().isValid()
			|| current.data(github_conversation_model::kind_role).toInt()
				!= static_cast<int>(
					application::github_conversation_kind::review_thread)
			|| current
					.data(github_conversation_model::anchor_availability_role)
					.toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			return std::nullopt;
		}
		const auto context = ComparisonSurface->current_context();
		if (!context || context->Key.File != core::file_identity_of(current.data(github_conversation_model::path_role).toString().toStdString())) {
			return std::nullopt;
		}
		bool       validLine = false;
		const auto oneBased  = current.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = current.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			return std::nullopt;
		}
		const auto projectedSide = sideValue.toInt();
		if (projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::left)
			&& projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::right)) {
			return std::nullopt;
		}
		const auto  side              = projectedSide
				== static_cast<int>(
							  application::github_conversation_diff_side::left)
			? core::diff_side::left
			: core::diff_side::right;
		const auto  endpointIsCurrent = side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		const auto& alignment         = ComparisonSurface->current_result()->Alignment;
		const auto  line              = static_cast<std::uint32_t>(oneBased - 1U);
		const auto  lineCount         = side == core::diff_side::left
			? alignment.LeftLineCount
			: alignment.RightLineCount;
		const auto  identity          = ConversationPanel->model()->id_for_index(current);
		if (!endpointIsCurrent || line >= lineCount || identity.empty()) {
			return std::nullopt;
		}
		return workbench_defect_classification{
			.Conversation = identity,
			.Anchor       = {
				.Comparison = alignment.Digest,
				.Side       = side,
				.Line       = line,
			},
		};
	}

	auto workbench_window::primary_toolbar() const noexcept -> QToolBar*
	{
		return PrimaryToolbar;
	}

	auto workbench_window::modality() const noexcept -> modality_control*
	{
		return Modality;
	}

	auto workbench_window::current_modality() const noexcept -> review_modality
	{
		return Modality != nullptr ? Modality->current() : review_modality::review;
	}

	auto workbench_window::toggle_drawers_action() const noexcept -> QAction*
	{
		return ToggleDrawersAction;
	}

	void workbench_window::publish(const application::commit_filter_projection& aProjection)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->present_commit_filter(aProjection);
		}
		if (FilesCommitChip != nullptr) {
			FilesCommitChip->present(aProjection);
		}
		if (FilesGeneratedRow != nullptr) {
			FilesGeneratedRow->present(aProjection);
		}
		FilterProjection = aProjection;
	}

	void workbench_window::set_file_filter_dispatch(std::function<void(const std::vector<std::string>&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_file_filter_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_generated_dispatch(std::function<void(const application::generated_file_gesture&)> aDispatch)
	{
		GeneratedDispatch = aDispatch;
		if (OverviewPage != nullptr) {
			OverviewPage->set_generated_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_generated_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		if (FilesGeneratedRow != nullptr) {
			FilesGeneratedRow->set_open_dispatch(aDispatch);
		}
		if (OverviewPage != nullptr && OverviewPage->generated_row() != nullptr) {
			// Review first, then the file, as a pick in the Overview's tree does.
			OverviewPage->generated_row()->set_open_dispatch([this, dispatch = std::move(aDispatch)](const std::string& aPath) {
				if (!dispatch) {
					return;
				}
				ReviewEnteredFor = overview_review_key();
				choose_modality(review_modality::review);
				dispatch(aPath);
			});
		}
	}

	auto workbench_window::files_generated_row() const noexcept -> generated_files_row*
	{
		return FilesGeneratedRow;
	}

	void workbench_window::set_commit_filter_dispatch(std::function<void(const application::commit_filter_gesture&)> aDispatch)
	{
		CommitFilterDispatch = aDispatch;
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_filter_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_filter_shown_dispatch(std::function<void()> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_filter_shown_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_open_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_rematch_dispatch(std::function<void(const application::commit_filter_rematch_gesture&)> aDispatch)
	{
		if (FilesCommitChip != nullptr) {
			FilesCommitChip->set_rematch_dispatch(aDispatch);
		}
		if (OverviewPage != nullptr) {
			if (OverviewPage->commit_chip() != nullptr) {
				OverviewPage->commit_chip()->set_rematch_dispatch(aDispatch);
			}
			if (OverviewPage->status_drawer() != nullptr && OverviewPage->status_drawer()->commits() != nullptr) {
				OverviewPage->status_drawer()->commits()->set_rematch_dispatch(std::move(aDispatch));
			}
		}
	}

	void workbench_window::present_split_diff(std::optional<application::split_diff_projection> aProjection)
	{
		if (SplitStrip != nullptr) {
			SplitStrip->present(aProjection);
		}
	}

	void workbench_window::set_split_segment_dispatch(std::function<void(std::size_t)> aDispatch)
	{
		if (SplitStrip != nullptr) {
			SplitStrip->set_dispatch(std::move(aDispatch));
		}
	}

	auto workbench_window::split_strip() const noexcept -> split_diff_strip*
	{
		return SplitStrip;
	}

	void workbench_window::show_commits()
	{
		if (OverviewPage == nullptr) {
			return;
		}
		if (Modality == nullptr || Modality->current() != review_modality::overview) {
			choose_modality(review_modality::overview, core::workbench_modality_entry::enter);
		}
		OverviewPage->status_drawer()->show_commits();
	}

	auto workbench_window::files_commit_chip() const noexcept -> commit_filter_chip*
	{
		return FilesCommitChip;
	}

	void workbench_window::choose_modality(const review_modality aModality, const core::workbench_modality_entry aEntry)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		CompositionPage = nullptr;
		// [[BPR-1588]]. The owner's decision D1 (RFC-BPR-0085): with Start
		// review gone, the first entry into Review from the Overview of a pull
		// request this window has not reviewed yet lands where the button did,
		// at the first unseen change's file. Later entries keep the file.
		const auto reviewKey = overview_review_key();
		// Only the reviewer's own choice (Command-2 or the Review segment); a
		// link, Resume or a tour enters with its own target.
		const auto firstReview = aModality == review_modality::review && aEntry == core::workbench_modality_entry::choose && Modality != nullptr
			&& Modality->current() == review_modality::overview
			&& !reviewKey.isEmpty() && ReviewEnteredFor != reviewKey;
		// Only on a pull request the reviewer has not started (nothing read, no
		// earlier review), where Start review stood; once started, a restored
		// reading place is never replaced.
		const auto notStarted = OverviewPage != nullptr && OverviewPage->projection().has_value()
			&& OverviewPage->projection()->Entry == application::overview_entry_kind::start_review;
		const auto entryPath = firstReview && notStarted ? OverviewPage->entry_path() : std::nullopt;
		if (aModality == review_modality::review && !reviewKey.isEmpty()) {
			ReviewEnteredFor = reviewKey;
		}
		// [[BPR-1293]]. Threads goes back where it was entered from.
		if (Modality != nullptr && aModality == review_modality::threads && Modality->current() != review_modality::threads) {
			ThreadsReturn      = Modality->current();
			ThreadsReturnFocus = Finder != nullptr && Finder->isAncestorOf(QApplication::focusWidget()) ? nullptr : QApplication::focusWidget();
		}
		if (Modality != nullptr) {
			// [[BPR-1740]]. A reply refused in one modality says nothing
			// about the next.
			if (Modality->current() != aModality) {
				clear_reply_refusal();
			}
			Modality->adopt(aModality);
		}
		// [[BPR-0661]]. "Last used" is the modality chosen while a pull
		// request was open.
		if (HasGithubPullRequest) {
			LastUsedModality = aModality;
		}
		// [[BPR-1599]]. A choice made while the pull request loads is the
		// reviewer's, and its arrival keeps it.
		if (!EnteringOpeningModality && !HasGithubPullRequest && SessionPanel != nullptr
			&& SessionPanel->status() == core::github_status::loading_pull_request) {
			LoadingChoice = aModality;
		}
		ModalityChosen = true;
		apply_modality_surface();
		if (HomeRequested) {
			HomeRequested = false;
			update_home_landing();
		}
		// The core decides every drawer, as it decides a toggle. [[BPR-0675]]
		// made those decisions reachable without a comparison, which is when
		// a preset matters most.
		if (!Dispatch) {
			return;
		}
		// [[BPR-0727]]. The core retains the modality first, because Overview
		// carries a rule: while it is chosen, a drawer opened by hand is
		// refused, and choosing it collapses every drawer in the same
		// decision ([[BPR-0674]]). [[BPR-1295]]: Review's arrangement is the
		// core's too. It keeps the drawers Review last had in this window,
		// applies Review's preset on the window's first entry, and applies it
		// again when Review is chosen while it shows.
		Dispatch(core::set_workbench_modality_intent{
			.Modality = [aModality] {
				switch (aModality) {
				case review_modality::overview:
					return core::workbench_modality::overview;
				case review_modality::review:
					return core::workbench_modality::review;
				case review_modality::discuss:
					return core::workbench_modality::discuss;
				case review_modality::threads:
					return core::workbench_modality::threads;
				}
				return core::workbench_modality::review;
			}(),
			.Entry = aEntry,
		});
		if (entryPath) {
			if (const auto file = file_for_path(*entryPath); file.has_value()) {
				Dispatch(core::select_workbench_file_intent{ .File = *file });
			}
		}
		if (aModality != review_modality::discuss) {
			return;
		}
		// Discuss's preset stays two exact intents: the files drawer gives
		// the conversation the room, and the conversations panel, which
		// Discuss borrows as its page, is open. Repositories stay as they
		// were.
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::files,
			.State  = core::workbench_drawer_state::collapsed,
		});
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::conversations,
			.State  = core::workbench_drawer_state::expanded,
		});
	}

	void workbench_window::enter_opening_modality(const review_modality aModality)
	{
		// [[BPR-1599]]. Pressing Review while a pull request loads used to be
		// undone when it arrived: the arrival entered Overview over it, and
		// the reviewer had to choose Review again.
		const auto chosen       = LoadingChoice;
		EnteringOpeningModality = true;
		choose_modality(chosen.value_or(aModality));
		EnteringOpeningModality = false;
	}

	auto workbench_window::home_landing() const noexcept -> workbench_landing*
	{
		return HomeLanding;
	}

	auto workbench_window::dif() const noexcept -> dif_widget*
	{
		return Dif;
	}

	auto workbench_window::ai_tools_action() const noexcept -> QAction*
	{
		return AiToolsAction;
	}

	auto workbench_window::dif_workshop_slot() const noexcept -> QWidget*
	{
		return DifWorkshopSlot;
	}

	void workbench_window::adopt_dif(const core::dif_resolution aResolution)
	{
		DifResolution      = aResolution;
		DifResolutionReady = true;
		update_dif_presentation();
		apply_pending_appearance();
	}

	void workbench_window::observe_dif_context(
		const core::application_session_id aSession,
		core::pull_request_authority       aAuthority,
		core::agent_revision_binding       aBinding)
	{
		if (DifCues->observe(
				{ aSession, std::move(aAuthority), std::move(aBinding) })
			&& Dif != nullptr) {
			Dif->reset_contextual();
		}
	}

	void workbench_window::clear_dif_context()
	{
		DifCues->clear();
		if (Dif != nullptr) {
			Dif->reset_contextual();
		}
	}

	void workbench_window::play_dif_cue(std::optional<dif_clip_id> aCue)
	{
		if (aCue.has_value() && Dif != nullptr) {
			static_cast<void>(Dif->play(*aCue));
		}
	}

	void workbench_window::admit_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity)
	{
		static_cast<void>(DifCues->admit(aIdentity));
	}

	void workbench_window::start_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity)
	{
		play_dif_cue(DifCues->start(aIdentity, DifCueClock->elapsed()));
	}

	void workbench_window::present_dif_agent_delivery(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_finding_delivery&               aDelivery,
		const bool                                               aReplyPresented,
		const bool                                               aFindingsPresented)
	{
		DifCues->deliver(
			aIdentity, aDelivery, aReplyPresented, aFindingsPresented);
	}

	void workbench_window::finish_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_run_outcome                     aOutcome)
	{
		play_dif_cue(DifCues->finish(aIdentity, aOutcome, DifCueClock->elapsed()));
	}

	void workbench_window::present_dif_workshop(
		const core::agent_session&  aSession,
		const std::uint64_t         aActionGeneration,
		const core::review_draft_id aDraft,
		const std::uint64_t         aRevision,
		const std::string_view      aProposed)
	{
		play_dif_cue(DifCues->present_workshop(
			aSession,
			aActionGeneration,
			aDraft,
			aRevision,
			aProposed,
			DifCueClock->elapsed()));
	}

	void workbench_window::begin_dif_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		DifCues->begin_draft(aCommand);
	}

	void workbench_window::accept_dif_draft(
		const core::revisioned_review_draft_command& aCommand,
		const core::revisioned_review_draft&         aAccepted)
	{
		play_dif_cue(DifCues->accept_draft(
			aCommand, aAccepted, DifCueClock->elapsed()));
	}

	void workbench_window::cancel_dif_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		DifCues->cancel_draft(aCommand);
	}

	void workbench_window::present_dif_continuation(
		const core::agent_session&            aSession,
		const core::agent_continuation_offer& aOffer)
	{
		play_dif_cue(DifCues->present_continuation(
			aSession, aOffer, DifCueClock->elapsed()));
	}

	void workbench_window::publish(const application::overview_history_update& aUpdate)
	{
		if (OverviewPage == nullptr || !PresentedOverview) {
			return;
		}
		auto& since      = PresentedOverview->SinceLastReview;
		since.Known      = aUpdate.Known;
		since.ReviewedAt = aUpdate.ReviewedAt;
		since.Uploads    = aUpdate.Uploads;
		OverviewPage->apply_history(aUpdate);
	}

	void workbench_window::publish(const application::overview_receipt_update& aUpdate)
	{
		if (OverviewPage == nullptr || !PresentedOverview.has_value()) {
			return;
		}
		// The retained overview takes the same rows, so an identical complete
		// publication afterwards is still recognised as unchanged.
		for (const auto& row : aUpdate.Rows) {
			if (const auto found = PresentedOverviewRows.find(row.Path); found != PresentedOverviewRows.end()) {
				PresentedOverview->Files[found->second] = row;
			}
		}
		PresentedOverview->Progress  = aUpdate.Progress;
		PresentedOverview->Entry     = aUpdate.Entry;
		PresentedOverview->EntryPath = aUpdate.EntryPath;
		OverviewPage->apply_receipts(aUpdate);
		if (FilesTree != nullptr && !aUpdate.Rows.empty()) {
			ApplyingFileSelection = true;
			FilesTree->apply_overview_rows(aUpdate.Rows);
			ApplyingFileSelection = false;
		}
	}

	void workbench_window::publish(
		const application::pull_request_overview_projection& aProjection)
	{
		if (OverviewPage == nullptr) {
			return;
		}
		const auto unchanged = PresentedOverview.has_value() && *PresentedOverview == aProjection;
		if (!unchanged) {
			OverviewPage->present(aProjection);
			present_new_threads();
			// [[BPR-1781]]. Whether I wrote it decides the Tour tab.
			refresh_overview_tour();
			PresentedOverview = aProjection;
			PresentedOverviewRows.clear();
			PresentedOverviewRows.reserve(aProjection.Files.size());
			for (std::size_t row = 0U; row < aProjection.Files.size(); ++row) {
				PresentedOverviewRows.emplace(aProjection.Files[row].Path, row);
			}
		}
		if (FilesTree != nullptr && (!unchanged || OverviewFileFactsDirty)) {
			// [[BPR-0658]]. The tree draws the overview's per-file facts.
			ApplyingFileSelection = true;
			// [[BPR-0668]]. A comparison that could not be prepared leaves the
			// workbench with no file rows, and the drawer was empty beside a
			// pull request that plainly has files. The pull request's own file
			// set is enough to list them; opening one still needs the
			// comparison, and the status line says so.
			if (!FileRowsPresent) {
				auto rows = std::vector<workbench_drawer_row>{ };
				rows.reserve(aProjection.Files.size());
				for (const auto& file : aProjection.Files) {
					const auto path = QString::fromStdString(file.Path);
					rows.push_back(workbench_drawer_row{
						.Identity = path,
						.Title    = path,
						.Detail   = path,
						.File     = core::file_identity_of(file.Path),
					});
				}
				FilesTree->apply(rows);
			}
			FilesTree->apply_overview(aProjection.Files);
			refresh_filter_notices();
			OverviewFileFactsDirty = false;
			ApplyingFileSelection  = false;
		}
		apply_modality_surface();
		update_pending_panes();
	}

	void workbench_window::publish_review_requests(const application::pull_request_overview_projection& aProjection)
	{
		if (OverviewPage == nullptr || (PresentedOverview.has_value() && *PresentedOverview == aProjection)) {
			return;
		}
		// [[BPR-1849]]. Only People and the status change; anything else is
		// presented whole.
		if (!PresentedOverview.has_value() || !OverviewPage->present_review_requests(aProjection)) {
			publish(aProjection);
			return;
		}
		PresentedOverview->ReviewRequests     = aProjection.ReviewRequests;
		PresentedOverview->ReviewRequestsNote = aProjection.ReviewRequestsNote;
		PresentedOverview->Status             = aProjection.Status;
	}

	void workbench_window::publish_draft_state(const application::pull_request_overview_projection& aProjection)
	{
		if (OverviewPage == nullptr || (PresentedOverview.has_value() && *PresentedOverview == aProjection)) {
			return;
		}
		// [[BPR-1852]]. Only the draft state and Merge change; anything else is
		// presented whole.
		if (!PresentedOverview.has_value() || !OverviewPage->present_draft_state(aProjection)) {
			publish(aProjection);
			return;
		}
		PresentedOverview->Draft              = aProjection.Draft;
		PresentedOverview->DraftPending       = aProjection.DraftPending;
		PresentedOverview->MergeState         = aProjection.MergeState;
		PresentedOverview->MergeTone          = aProjection.MergeTone;
		PresentedOverview->RequirementDetails = aProjection.RequirementDetails;
	}

	void workbench_window::present_strip_draft(const bool aDraft)
	{
		if (IdentityDraft == aDraft) {
			return;
		}
		IdentityDraft = aDraft;
		apply_strip_width();
	}

	auto workbench_window::strip_draft_chip() const noexcept -> QWidget*
	{
		return StripDraftChip;
	}

	auto workbench_window::strip_draft_shown() const noexcept -> bool
	{
		return StripDraftChipAction != nullptr && StripDraftChipAction->isVisible();
	}

	void workbench_window::set_draft_state_offer(const std::optional<core::draft_state_change> aOffered)
	{
		DraftStateOffer = aOffered;
		if (OverviewPage != nullptr) {
			OverviewPage->set_draft_state_offer(aOffered);
		}
		if (DraftStateAction != nullptr) {
			const auto ready = aOffered == core::draft_state_change::ready_for_review;
			DraftStateAction->setText(ready ? tr("Mark &Ready for Review") : tr("Convert to &Draft"));
			DraftStateAction->setStatusTip(ready ? QStringLiteral("Mark this draft ready for review on GitHub") : QStringLiteral("Make this pull request a draft again on GitHub"));
			DraftStateAction->setVisible(aOffered.has_value());
			DraftStateAction->setEnabled(aOffered.has_value());
		}
	}

	void workbench_window::set_draft_state_dispatch(std::function<void(core::draft_state_change)> aDispatch)
	{
		DraftStateDispatch = aDispatch;
		if (OverviewPage != nullptr) {
			OverviewPage->set_draft_state_dispatch(std::move(aDispatch));
		}
	}

	auto workbench_window::draft_state_action() const noexcept -> QAction*
	{
		return DraftStateAction;
	}

	void workbench_window::publish(const application::overview_feedback_projection& aFeedback)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->present_feedback(aFeedback);
		}
	}

	void workbench_window::set_overview_feedback_dispatch(std::function<void(const application::overview_feedback_request&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_feedback_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_overview_drill_down_dispatch(std::function<void(const application::overview_drill_down&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_drill_down_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::list_conversations(const application::overview_drill_down_listing& aListing, const bool aReveal)
	{
		if (ConversationPanel == nullptr || Modality == nullptr) {
			return;
		}
		// The caption says which count it was: the label, the mode, where.
		const auto  anyColumns = Behaviour.LabelColumns.Count != 0U;
		const auto& down       = aListing.DrillDown;
		auto        where      = QString{ };
		switch (down.Row) {
		case application::overview_feedback_row_kind::file:
		case application::overview_feedback_row_kind::folder:
			where = down.Path.empty() ? QStringLiteral("/") : QString::fromStdString(down.Path);
			break;
		case application::overview_feedback_row_kind::remainder:
			where = QStringLiteral("Not on a file");
			break;
		case application::overview_feedback_row_kind::pull_request:
			where = QStringLiteral("Pull request");
			break;
		}
		auto parts = QStringList{ overview_count_name(down.Target, anyColumns) };
		if (down.Target.Kind != core::feedback_count_kind::uncertain) {
			parts.push_back(aListing.Mode == core::feedback_resolution_mode::unresolved ? QStringLiteral("open") : QStringLiteral("all"));
		}
		parts.push_back(where);
		const auto count       = static_cast<qulonglong>(aListing.Count);
		const auto caption     = QStringLiteral("%1 — %2 %3").arg(parts.join(QStringLiteral(" · ")), QString::number(count), count == 1U ? QStringLiteral("thread") : QStringLiteral("threads"));
		const auto description = QStringLiteral("Listed from the Overview: exactly the threads this count counted. Show every thread clears it.");
		if (aReveal) {
			if (OverviewPage != nullptr && OverviewPage->isAncestorOf(QApplication::focusWidget())) {
				OverviewReturnFocus = QApplication::focusWidget();
			}
			choose_home(false);
			choose_modality(review_modality::discuss);
		}
		ConversationPanel->list_conversations(aListing.Conversations, caption, description,
											  OverviewPage != nullptr ? OverviewPage->count_mark(down.Target, anyColumns) : QPixmap{ });
		if (aReveal) {
			ConversationPanel->cards()->setFocus(Qt::OtherFocusReason);
			QAccessibleAnnouncementEvent event{ ConversationPanel, QStringLiteral("Discuss lists %1").arg(caption) };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	auto workbench_window::conversation_listing_active() const -> bool
	{
		return ConversationPanel != nullptr && ConversationPanel->listing().has_value();
	}

	void workbench_window::clear_overview()
	{
		PresentedOverview.reset();
		PresentedOverviewRows.clear();
		OverviewFileFactsDirty = true;
		if (OverviewPage == nullptr) {
			return;
		}
		OverviewPage->clear();
		if (FilesTree != nullptr) {
			ApplyingFileSelection = true;
			FilesTree->apply_overview({ });
			ApplyingFileSelection = false;
		}
		apply_modality_surface();
	}

	auto workbench_window::overview_page() const noexcept -> pull_request_overview_page*
	{
		return OverviewPage;
	}

	auto workbench_window::showing_overview() const -> bool
	{
		return CentreStack != nullptr && CentreStack->currentWidget() == OverviewPage;
	}

	void workbench_window::apply_conversation_anchor()
	{
		if (ConversationAnchorTimer != nullptr) {
			ConversationAnchorTimer->stop();
		}
		if (ConversationPanel == nullptr) {
			return;
		}
		ApplyingConversationAnchor = true;
		ConversationPanel->set_anchor_file(conversation_anchor_file());
		ConversationPanel->set_anchor_line(std::nullopt);
		ApplyingConversationAnchor = false;
		refresh_cards();
	}

	void workbench_window::schedule_conversation_anchor()
	{
		// A zero-interval timer rather than a posted call: an overdue timer
		// fires before one registered after it, so input already waiting is
		// served before the drawer re-projects. A later publication that moves
		// the anchor applies it at once, which leaves this turn nothing to do.
		if (ConversationAnchorTimer == nullptr) {
			ConversationAnchorTimer = new QTimer{ this };
			ConversationAnchorTimer->setSingleShot(true);
			ConversationAnchorTimer->setInterval(0);
			connect(ConversationAnchorTimer, &QTimer::timeout, this, [this] {
				if (ConversationPanel != nullptr && ConversationPanel->anchor_file() != conversation_anchor_file()) {
					apply_conversation_anchor();
				}
			});
		}
		ConversationAnchorTimer->start();
	}
}
