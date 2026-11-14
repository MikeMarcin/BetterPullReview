/// \file
/// \brief RFC-BPR-0108 as amended by the owner, 2026-11-13. A local draft, and
/// a pull request opened by its author on their worktree, is shown in this
/// window's own review surface: the Files tree, the Search tab, the diff
/// viewer with its syntax colours, Overview, the drawers and the status bar
/// are the pull request's. The owner: "augment the real PR window, do not
/// rebuild it." The draft adds only what `local_draft_parts` names, each in a
/// slot of the window's.

#include "bpr/core/local_presentation.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string_view aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto shown_candidate(const application::local_draft_workbench_projection& aView) -> const core::local_draft_candidate*
		{
			if (!aView.Draft) {
				return nullptr;
			}
			const auto found = std::ranges::find(aView.Draft->Candidates, aView.Shown, &core::local_draft_candidate::Ordinal);
			return found == aView.Draft->Candidates.end() ? nullptr : &*found;
		}

		auto operation_of(const core::draft_change aChange) -> application::overview_file_operation
		{
			switch (aChange) {
			case core::draft_change::added:    return application::overview_file_operation::added;
			case core::draft_change::modified: return application::overview_file_operation::modified;
			case core::draft_change::deleted:  return application::overview_file_operation::removed;
			case core::draft_change::mode:     return application::overview_file_operation::type_changed;
			}
			return application::overview_file_operation::modified;
		}

		/// The Overview the pull request would have, from the draft alone.
		auto draft_overview(const application::local_draft_workbench_projection& aView) -> application::pull_request_overview_projection
		{
			auto overview = application::pull_request_overview_projection{ };
			if (!aView.Draft) {
				return overview;
			}
			const auto& draft       = *aView.Draft;
			overview.Owner          = draft.Target.Target.Repository.Owner;
			overview.RepositoryName = draft.Target.Target.Repository.Name;
			overview.Number         = core::pull_request_number{ draft.Association ? static_cast<std::uint32_t>(draft.Association->Number) : 0U };
			overview.Title          = draft.Title;
			overview.Body           = draft.Preparation.Description.Body;
			overview.BaseRef        = draft.Target.Target.Branch;
			overview.HeadRef        = draft.Branch;
			overview.Draft          = true;
			// No count: a draft's commits are the worktree's, not a pull request's.
			overview.AuthorLogin = "This draft";
			if (const auto* const shown = shown_candidate(aView); shown != nullptr) {
				for (const auto& change : shown->Changes) {
					const auto slash = change.Path.rfind('/');
					overview.Files.push_back({
						.Path      = change.Path,
						.Folder    = slash == std::string::npos ? std::string{ } : change.Path.substr(0U, slash),
						.Name      = slash == std::string::npos ? change.Path : change.Path.substr(slash + 1U),
						.Operation = operation_of(change.Change),
					});
				}
			}
			return overview;
		}

		/// The subject a draft's file identities are derived from.
		auto draft_subject(const application::local_draft_workbench_projection& aView) -> std::string
		{
			return aView.Draft ? aView.Draft->Id.Value : std::string{ };
		}
	}

	void workbench_window::show_local_draft(const QString& aTitle)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		if (LocalCycleShown) {
			leave_local_cycle();
		}
		const auto entering = !LocalDraftShown;
		LocalCycleTitle     = aTitle;
		LocalDraftShown     = true;
		HomeRequested       = false;
		if (entering) {
			// What a pull request open behind the draft shows comes back on leaving.
			OverviewBehindDraft            = PresentedOverview;
			DescriptionEditableBehindDraft = OverviewPage != nullptr && OverviewPage->description_editable();
			FilesBehindDraft               = PublishedFiles;
			// Another draft opens on Review, in two panes (X23 as amended).
			if (Modality != nullptr) {
				Modality->adopt(review_modality::review);
			}
			if (ReviewViewPicker != nullptr) {
				ReviewViewPicker->set_view(review_view::two_panes);
			}
		}
		// Discuss and Threads wait until a pull request exists.
		if (Modality != nullptr) {
			for (const auto waiting : { review_modality::discuss, review_modality::threads }) {
				if (auto* const action = Modality->action(waiting); action != nullptr) {
					if (!action->property("bprLocalDraftTip").isValid()) {
						action->setProperty("bprLocalDraftTip", action->toolTip());
					}
					action->setEnabled(false);
					action->setToolTip(QStringLiteral("Waits until the pull request exists."));
				}
			}
		}
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
		apply_local_draft_surface();
		if (entering && LocalDraftObserver) {
			LocalDraftObserver(true);
		}
	}

	void workbench_window::show_local_publication()
	{
		if (!LocalDraftShown) {
			return;
		}
		choose_modality(review_modality::overview);
		if (OverviewPage != nullptr && OverviewPage->status_drawer() != nullptr) {
			OverviewPage->status_drawer()->show_tab(overview_drawer_tab::status);
		}
	}

	void workbench_window::set_local_draft_observer(std::function<void(bool)> aObserver)
	{
		LocalDraftObserver = std::move(aObserver);
	}

	void workbench_window::leave_local_draft()
	{
		if (!LocalDraftShown) {
			return;
		}
		LocalDraftShown   = false;
		LocalChangesShown = false;
		if (LocalDraftObserver) {
			LocalDraftObserver(false);
		}
		LocalShown.reset();
		LocalFiles.clear();
		LocalCycleTitle.clear();
		if (Modality != nullptr) {
			for (const auto waiting : { review_modality::discuss, review_modality::threads }) {
				if (auto* const action = Modality->action(waiting); action != nullptr) {
					action->setEnabled(true);
					if (const auto tip = action->property("bprLocalDraftTip"); tip.isValid()) {
						action->setToolTip(tip.toString());
						action->setProperty("bprLocalDraftTip", QVariant{ });
					}
				}
			}
		}
		apply_local_draft_surface();
		if (OverviewPage != nullptr) {
			OverviewPage->close_description_editor();
			OverviewPage->set_description_editable(DescriptionEditableBehindDraft);
		}
		// The pull request behind the draft, as it was.
		apply_file_rows(FilesBehindDraft);
		FilesBehindDraft.clear();
		if (OverviewBehindDraft) {
			PresentedOverview.reset();
			publish(*OverviewBehindDraft);
			OverviewBehindDraft.reset();
		} else if (OverviewPage != nullptr) {
			OverviewPage->clear();
			PresentedOverview.reset();
		}
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->clear();
		}
		if (HasGithubPullRequest && Cursor && Cursor->File && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *Cursor->File });
		}
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
		update_action_state();
		update_status_text();
	}

	void workbench_window::present_local_draft(const application::local_draft_workbench_projection& aProjection)
	{
		if (LocalDraft == nullptr) {
			return;
		}
		const auto previousDraft = LocalDraft->projection().Draft ? LocalDraft->projection().Draft->Id.Value : std::string{ };
		LocalDraft->present(aProjection);
		if (!LocalDraftShown) {
			return;
		}
		// The Files tree holds the shown candidate's files, as it holds a pull request's.
		const auto  subject = draft_subject(aProjection);
		auto        files   = std::vector<std::string>{ };
		const auto* shown   = shown_candidate(aProjection);
		if (shown != nullptr) {
			for (const auto& change : shown->Changes) {
				files.push_back(change.Path);
			}
		}
		if (files != LocalFiles || previousDraft != subject) {
			LocalFiles = files;
			auto rows  = std::vector<application::workbench_file_projection>{ };
			rows.reserve(files.size());
			for (const auto& path : files) {
				rows.push_back({ .File = core::local_presentation_file(subject, path), .Path = path });
			}
			ApplyingLocalRows = true;
			apply_file_rows(rows);
			ApplyingLocalRows = false;
		}
		// Overview: the pull request this draft would be.
		if (OverviewPage != nullptr) {
			auto overview = draft_overview(aProjection);
			if (!PresentedOverview || *PresentedOverview != overview) {
				ApplyingLocalRows = true;
				publish(overview);
				ApplyingLocalRows = false;
			}
		}
		// The file selected is the one the draft shows.
		if (FilesTree != nullptr && aProjection.File && !LocalChangesShown) {
			ApplyingFileSelection = true;
			FilesTree->select_file(core::local_presentation_file(subject, aProjection.File->Path));
			const auto index = Drawers[0].Panel->contents()->index_for_file(core::local_presentation_file(subject, aProjection.File->Path));
			Drawers[0].Panel->contents_view()->setCurrentIndex(index);
			ApplyingFileSelection = false;
		}
		apply_local_comparison();
		apply_local_draft_surface();
	}

	void workbench_window::present_local_changes(const application::local_changes_projection& aProjection)
	{
		if (LocalDraft == nullptr) {
			return;
		}
		const auto previousEdit = LocalDraft->edit_document();
		LocalDraft->present_local(aProjection);
		if (!LocalDraftShown) {
			return;
		}
		apply_local_comparison();
		apply_local_draft_surface();
		// The Edit view's text is coloured by the same definitions as the panes.
		if (const auto document = LocalDraft->edit_document(); LocalSyntaxDispatch && !document.empty() && document != previousEdit) {
			LocalSyntaxDispatch(LocalDraft->edit_path(), document);
		}
	}

	void workbench_window::apply_local_comparison()
	{
		if (LocalDraft == nullptr || !LocalDraftShown) {
			return;
		}
		const auto& local = LocalDraft->local_projection();
		const auto& draft = LocalDraft->projection();
		LocalChangesShown = local.Active && FileSearchTabs != nullptr && FileSearchTabs->currentIndex() == local_changes_tab();
		if (LocalChangesShown) {
			present_local_comparison(local.File ? local.File->Presented : nullptr);
		} else {
			present_local_comparison(draft.File ? draft.File->Presented : nullptr);
		}
	}

	void workbench_window::present_local_comparison(const std::shared_ptr<const application::local_presentation>& aPresentation)
	{
		if (ComparisonSurface == nullptr || aPresentation == LocalShown) {
			return;
		}
		LocalShown = aPresentation;
		if (!aPresentation) {
			return;
		}
		const auto path = text_of(aPresentation->Path);
		ComparisonSurface->set_loading(aPresentation->Context, path);
		if (aPresentation->Result && aPresentation->Index) {
			ComparisonSurface->publish({ .Context = aPresentation->Context, .Result = aPresentation->Result, .Index = aPresentation->Index });
			apply_local_reveal();
			// The panes' colours, by the same classifier as a pull request's.
			if (LocalSyntaxDispatch) {
				for (const auto& side : { aPresentation->Left, aPresentation->Right }) {
					if (side) {
						ComparisonSurface->await_syntax(side == aPresentation->Left ? core::diff_side::left : core::diff_side::right);
						LocalSyntaxDispatch(aPresentation->Path, *side);
					}
				}
			}
		} else {
			ComparisonSurface->fail({ .Context = aPresentation->Context, .Reason = aPresentation->Reason == core::diff_reason_code::none ? core::diff_reason_code::host_failure : aPresentation->Reason });
		}
		update_status_text();
	}

	auto workbench_window::local_comparison() const noexcept -> const std::shared_ptr<const application::local_presentation>&
	{
		return LocalShown;
	}

	void workbench_window::set_local_syntax_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		LocalSyntaxDispatch = std::move(aDispatch);
	}

	void workbench_window::adopt_local_syntax(const std::string& aDocument, const application::shared_token_spans& aSpans)
	{
		if (!LocalDraftShown || !aSpans) {
			return;
		}
		if (LocalShown && ComparisonSurface != nullptr && ComparisonSurface->current_context() && *ComparisonSurface->current_context() == LocalShown->Context) {
			if (LocalShown->Left && *LocalShown->Left == aDocument) {
				static_cast<void>(ComparisonSurface->adopt_token_spans(core::diff_side::left, aSpans));
			}
			if (LocalShown->Right && *LocalShown->Right == aDocument) {
				static_cast<void>(ComparisonSurface->adopt_token_spans(core::diff_side::right, aSpans));
			}
		}
		if (LocalDraft != nullptr) {
			LocalDraft->adopt_edit_syntax(aDocument, aSpans);
		}
	}

	auto workbench_window::file_search_tabs() const noexcept -> QTabWidget*
	{
		return FileSearchTabs;
	}

	auto workbench_window::open_local_file(const QString& aPath) -> bool
	{
		if (!LocalDraftShown || LocalDraft == nullptr || Drawers[0].Panel->contents() == nullptr) {
			return false;
		}
		const auto subject = LocalDraft->projection().Draft ? LocalDraft->projection().Draft->Id.Value : std::string{ };
		const auto bytes   = aPath.toUtf8();
		const auto index   = Drawers[0].Panel->contents()->index_for_file(core::local_presentation_file(subject, std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }));
		if (!index.isValid()) {
			return false;
		}
		if (Drawers[0].Panel->contents_view()->currentIndex() == index) {
			dispatch_file_selection(index);
		} else {
			Drawers[0].Panel->contents_view()->setCurrentIndex(index);
		}
		return true;
	}

	auto workbench_window::local_changes_tab() const -> int
	{
		if (FileSearchTabs == nullptr || LocalDraft == nullptr) {
			return -1;
		}
		return FileSearchTabs->indexOf(LocalDraft->local_changes());
	}

	void workbench_window::step_local_file(const bool aNext)
	{
		if (LocalFiles.empty() || LocalDraft == nullptr) {
			return;
		}
		const auto& shown   = LocalDraft->projection().File;
		const auto  current = shown ? std::ranges::find(LocalFiles, shown->Path) : LocalFiles.end();
		auto        index   = current == LocalFiles.end() ? (aNext ? std::size_t{ 0U } : LocalFiles.size() - 1U) : static_cast<std::size_t>(std::distance(LocalFiles.begin(), current));
		if (current != LocalFiles.end()) {
			if (aNext && index + 1U < LocalFiles.size()) {
				++index;
			} else if (!aNext && index > 0U) {
				--index;
			}
		}
		if (FileSearchTabs != nullptr && FileSearchTabs->currentIndex() == local_changes_tab()) {
			FileSearchTabs->setCurrentIndex(0);
		}
		LocalDraft->open_file(LocalFiles[index], ReviewViewPicker != nullptr && ReviewViewPicker->view() == review_view::edit);
	}

	auto workbench_window::local_draft_status() const -> QString
	{
		if (LocalDraft == nullptr) {
			return { };
		}
		const auto& shown = LocalDraft->projection().File;
		const auto  found = shown ? std::ranges::find(LocalFiles, shown->Path) : LocalFiles.end();
		const auto  file  = found == LocalFiles.end() ? 0 : static_cast<int>(std::distance(LocalFiles.begin(), found)) + 1;
		return LocalDraft->status_text(file, static_cast<int>(LocalFiles.size()));
	}

	void workbench_window::apply_local_draft_surface()
	{
		if (LocalDraft == nullptr) {
			return;
		}
		const auto local  = LocalDraftShown;
		const auto author = local && LocalDraft->author();
		// The review bar: the candidate and branch where Updates stands, the
		// steps where Compose review stands; what a pull request's review has
		// and a draft lacks waits.
		if (LocalBarLeadingAction != nullptr) {
			LocalBarLeadingAction->setVisible(local);
		}
		if (LocalBarTrailingAction != nullptr) {
			LocalBarTrailingAction->setVisible(local);
		}
		if (ReviewBar != nullptr) {
			for (auto* const action : ReviewBar->actions()) {
				const auto name    = action->objectName();
				const auto pullish = name == QLatin1String{ "reviewBarUpdatesAction" } || name == QLatin1String{ "reviewBarProgressGapAction" }
					|| name == QLatin1String{ "reviewBarComparingAction" } || name == QLatin1String{ "reviewBarComparisonDividerAction" }
					|| name == QLatin1String{ "workbenchProgressTrackAction" } || action == ComposeReviewAction || action == CommentAction || action == HeaderSourceAction;
				if (pullish) {
					action->setVisible(!local);
				}
			}
			if (ReviewChipAction != nullptr && local) {
				ReviewChipAction->setVisible(false);
			}
		}
		// Author state's third view, and why it may be refused.
		if (ReviewViewPicker != nullptr) {
			ReviewViewPicker->set_author(author);
			ReviewViewPicker->set_edit_refusal(author ? LocalDraft->edit_refusal() : QString{ });
			if (!author && ReviewViewPicker->view() == review_view::edit) {
				ReviewViewPicker->set_view(review_view::two_panes);
			}
			if (local) {
				for (const auto view : { review_view::two_panes, review_view::one_pane }) {
					ReviewViewPicker->segment(view)->setEnabled(true);
				}
				ReviewViewPicker->segment(review_view::two_panes)->setToolTip(QStringLiteral("Two panes"));
				ReviewViewPicker->segment(review_view::one_pane)->setToolTip(QStringLiteral("One pane"));
			}
		}
		// Local Changes is a tab, always, in author state.
		if (FileSearchTabs != nullptr) {
			const auto tab = local_changes_tab();
			if (tab >= 0) {
				FileSearchTabs->setTabVisible(tab, author);
				if (!author && FileSearchTabs->currentIndex() == tab) {
					FileSearchTabs->setCurrentIndex(0);
				}
			}
		}
		if (auto* const footer = LocalDraft->files_footer(); footer != nullptr) {
			footer->setVisible(local && LocalDraft->projection().Draft.has_value());
		}
		if (LocalNoticeHolder != nullptr) {
			LocalNoticeHolder->setVisible(local && LocalDraft->notice_row()->isVisibleTo(LocalNoticeHolder));
		}
		// The tray: Comments and Tour hold the draft's. RFC-BPR-0127: the
		// draft's notes are its Comments, until the pull request has its own.
		if (TrayTabs != nullptr) {
			if (local && TrayTabs->currentIndex() == tray_comments_tab) {
				TrayTabs->setCurrentIndex(tray_tour_tab);
			}
			if (TrayPages != nullptr) {
				const auto index = TrayTabs->currentIndex();
				if (local && index == tray_comments_tab) {
					TrayPages->setCurrentWidget(LocalDraft->notes_page());
				} else if (index >= 0 && index < TrayPages->count()) {
					TrayPages->setCurrentIndex(index);
				}
			}
			if (CommentsTools != nullptr) {
				CommentsTools->setVisible(!local && TrayTabs->currentIndex() == tray_comments_tab);
			}
		}
		// Overview: the Status tab carries the publish steps; the description
		// is Overview's own, editable as a pull request's author's is.
		if (OverviewPage != nullptr) {
			if (auto* const drawer = OverviewPage->status_drawer(); drawer != nullptr) {
				drawer->set_status_page(local && LocalDraft->projection().Draft ? LocalDraft->status_page() : nullptr);
				drawer->set_conversation_tabs_shown(!local);
			}
			if (local) {
				OverviewPage->set_description_editable(LocalDraft->projection().Draft.has_value());
			}
		}
		// The centre: the Edit view's page covers the comparison in Review.
		if (CentreStack != nullptr && LocalEditHolder != nullptr) {
			const auto editing = author && ReviewViewPicker != nullptr && ReviewViewPicker->view() == review_view::edit && current_modality() == review_modality::review;
			if (editing) {
				CentreStack->setCurrentWidget(LocalEditHolder);
			} else if (CentreStack->currentWidget() == LocalEditHolder || local) {
				CentreStack->setCurrentWidget(current_modality() == review_modality::overview && local ? static_cast<QWidget*>(OverviewPage) : ComparisonPage);
			}
		}
		// Explain this file asks about a pull request's file; a draft's waits.
		refresh_ask_controls();
		if (EpochSelectors != nullptr) {
			EpochSelectors->setVisible(!local);
		}
		update_strip_positions();
		update_action_state();
		update_status_text();
		apply_drawer_layout();
	}
}

namespace bpr::qt {
	void workbench_window::build_local_draft_slots()
	{
		LocalDraft          = new local_draft_parts{ this };
		LocalDraft->Changed = [this] {
			if (LocalDraftShown) {
				update_status_text();
			}
		};
		// The review bar: the candidate and branch lead it, the publishing
		// steps stand where Compose review does.
		if (ReviewBar != nullptr) {
			QAction* updates = nullptr;
			for (auto* const action : ReviewBar->actions()) {
				if (action->objectName() == QLatin1String{ "reviewBarUpdatesAction" }) {
					updates = action;
				}
			}
			LocalBarLeadingAction = ReviewBar->insertWidget(updates, LocalDraft->bar_leading());
			LocalBarLeadingAction->setObjectName(QStringLiteral("reviewBarLocalDraftAction"));
			LocalBarLeadingAction->setVisible(false);
			LocalBarTrailingAction = ReviewBar->insertWidget(ReviewChipAction, LocalDraft->bar_trailing());
			LocalBarTrailingAction->setObjectName(QStringLiteral("reviewBarLocalPublishAction"));
			LocalBarTrailingAction->setVisible(false);
		}
		// The Files tab keeps its tree; the working files wait under it.
		if (FileSearchTabs != nullptr) {
			if (auto* const files = FileSearchTabs->widget(0); files != nullptr) {
				if (auto* const column = qobject_cast<QBoxLayout*>(files->layout()); column != nullptr) {
					column->addWidget(LocalDraft->files_footer());
				}
			}
			LocalDraft->files_footer()->hide();
			const auto tab = FileSearchTabs->addTab(LocalDraft->local_changes(), tr("Changes"));
			FileSearchTabs->setTabToolTip(tab, tr("Stage, commit and push."));
			FileSearchTabs->setTabVisible(tab, false);
			connect(FileSearchTabs, &QTabWidget::currentChanged, this, [this] {
				if (LocalDraftShown) {
					apply_local_comparison();
					update_status_text();
				}
			});
		}
		// A recovery or a target choice, above the centre, only when there is one.
		if (CentreStack != nullptr) {
			LocalNoticeHolder = LocalDraft->notice_row();
			if (auto* const centre = qobject_cast<QBoxLayout*>(CentreStack->parentWidget()->layout()); centre != nullptr) {
				centre->insertWidget(centre->indexOf(CentreStack), LocalNoticeHolder);
			}
			LocalEditHolder = LocalDraft->edit_page();
			CentreStack->addWidget(LocalEditHolder);
		}
		// The tray's Comments shows the draft's notes while it shows; its Tour
		// is the window's own Tour panel, which the composition gives the
		// draft's tour.
		if (TrayPages != nullptr && TrayTabs != nullptr) {
			TrayPages->addWidget(LocalDraft->notes_page());
			connect(TrayTabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
				if (LocalDraftShown && aIndex == tray_comments_tab) {
					TrayPages->setCurrentWidget(LocalDraft->notes_page());
					if (CommentsTools != nullptr) {
						CommentsTools->hide();
					}
				}
			});
		}
		// A stop's lines are the viewer's selection, on the candidate's side.
		LocalDraft->set_comparison_links(
			[this]() -> std::optional<local_draft_lines> {
				if (LocalChangesShown || !LocalShown || ComparisonSurface == nullptr || !ComparisonSurface->current_result()) {
					return std::nullopt;
				}
				auto first = std::optional<std::uint32_t>{ };
				auto last  = std::optional<std::uint32_t>{ };
				if (const auto range = ComparisonSurface->selected_source_range(); range && range->first.Side == core::diff_side::right && range->second.Side == core::diff_side::right) {
					first = range->first.Line + 1U;
					last  = range->second.Line + 1U;
				} else if (const auto& selected = ComparisonSurface->selection(); selected && selected->PreferredLine.Side == core::diff_side::right) {
					first = selected->PreferredLine.Line + 1U;
					last  = first;
				}
				if (!first) {
					return std::nullopt;
				}
				return local_draft_lines{ .Path = LocalShown->Path, .First = std::min(*first, *last), .Last = std::max(*first, *last) };
			},
			[this](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) {
				if (current_modality() != review_modality::review) {
					choose_modality(review_modality::review);
				}
				if (aFirst == 0U) {
					return;
				}
				// The file may still be loading: its lines are selected when it shows.
				LocalReveal = local_reveal{ .Path = aPath, .First = aFirst, .Last = std::max(aFirst, aLast) };
				apply_local_reveal();
			});
	}

	void workbench_window::apply_local_reveal()
	{
		if (!LocalReveal || !LocalShown || LocalShown->Path != LocalReveal->Path || ComparisonSurface == nullptr || !ComparisonSurface->current_result()) {
			return;
		}
		const auto digest = ComparisonSurface->current_result()->Alignment.Digest;
		static_cast<void>(ComparisonSurface->restore_source_range({ .Comparison = digest, .Side = core::diff_side::right, .Line = LocalReveal->First - 1U },
																  { .Comparison = digest, .Side = core::diff_side::right, .Line = LocalReveal->Last - 1U }));
		LocalReveal.reset();
	}

	void workbench_window::set_local_change_dispatch(std::function<void(const std::string&, core::local_change_hunk, bool)> aDispatch)
	{
		LocalChangeDispatch = std::move(aDispatch);
	}

	void workbench_window::add_local_change_items(QMenu& aMenu, const core::diff_side aSide, const std::uint32_t aLine)
	{
		if (!LocalChangesShown || LocalDraft == nullptr || !LocalChangeDispatch) {
			return;
		}
		const auto& file = LocalDraft->local_projection().File;
		if (!file || file->Comparison != core::local_comparison::uncommitted) {
			return;
		}
		// The band whose lines hold the one clicked, on its side.
		const auto band = std::ranges::find_if(file->Bands, [aSide, aLine](const core::local_change_band& aBand) {
			const auto begin = aSide == core::diff_side::left ? aBand.Shown.LeftBegin : aBand.Shown.RightBegin;
			const auto end   = aSide == core::diff_side::left ? aBand.Shown.LeftEnd : aBand.Shown.RightEnd;
			return aLine >= begin && aLine < std::max(end, begin + 1U);
		});
		if (band == file->Bands.end()) {
			return;
		}
		aMenu.addSeparator();
		const auto path = file->Path;
		if (band->Unstaged && band->Stage) {
			auto* const stage = aMenu.addAction(tr("Stage Change"));
			stage->setObjectName(QStringLiteral("localStageChangeAction"));
			connect(stage, &QAction::triggered, this, [this, path, hunk = *band->Stage] {
				if (LocalChangeDispatch) {
					LocalChangeDispatch(path, hunk, true);
				}
			});
		}
		if (band->Staged) {
			auto* const unstage = aMenu.addAction(tr("Unstage Change"));
			unstage->setObjectName(QStringLiteral("localUnstageChangeAction"));
			connect(unstage, &QAction::triggered, this, [this, path, hunk = band->Unstage.value_or(band->Shown)] {
				if (LocalChangeDispatch) {
					LocalChangeDispatch(path, hunk, false);
				}
			});
		}
	}

	namespace {
		/// Escape in a Home list clears Home's selection.
		class home_escape_filter final : public QObject
		{
		public:
			home_escape_filter(QObject* aParent, std::function<void()> aClear)
				: QObject{ aParent }
				, Clear{ std::move(aClear) }
			{
			}

		protected:
			auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Escape && static_cast<QKeyEvent*>(aEvent)->modifiers() == Qt::NoModifier) {
					auto* const view = qobject_cast<QAbstractItemView*>(aWatched);
					if (view != nullptr && view->selectionModel() != nullptr && view->selectionModel()->hasSelection()) {
						Clear();
						return true;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			std::function<void()> Clear;
		};

		auto home_views(QWidget* const aHome) -> QList<QAbstractItemView*>
		{
			return aHome == nullptr ? QList<QAbstractItemView*>{ } : aHome->findChildren<QAbstractItemView*>();
		}
	}

	void workbench_window::clear_home_selection(const QAbstractItemView* const aKept)
	{
		// Each list keeps its current row for the keyboard; none shows it selected.
		for (auto* const view : home_views(HomePage)) {
			if (view != aKept) {
				home_hide_selection(*view);
			}
		}
	}

	auto workbench_window::home_selected_rows() const -> int
	{
		auto rows = 0;
		for (auto* const view : home_views(HomePage)) {
			if (view->isVisibleTo(HomePage) && view->selectionModel() != nullptr) {
				rows += static_cast<int>(view->selectionModel()->selectedIndexes().size());
			}
		}
		return rows;
	}

	void workbench_window::build_home_selection()
	{
		// In preparation joins Home's one selection (claim_home_selection);
		// Escape in any Home list clears the selection it shows.
		auto* const escape = new home_escape_filter{ this, [this] { clear_home_selection(nullptr); } };
		const auto  adopt  = [escape](QAbstractItemView* const aView) {
			if (aView != nullptr && !aView->property("bprHomeEscape").toBool()) {
				aView->setProperty("bprHomeEscape", true);
				aView->installEventFilter(escape);
			}
		};
		if (HomeDraftsList != nullptr) {
			connect(HomeDraftsList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
				if (HomeSelection != home_selection_list::drafts) {
					home_hide_selection(*HomeDraftsList);
				}
			});
		}
		for (auto* const view : home_views(HomePage)) {
			adopt(view);
		}
		connect(qApp, &QApplication::focusChanged, this, [this, adopt](QWidget*, QWidget* const aNow) {
			if (HomePage != nullptr && aNow != nullptr && HomePage->isAncestorOf(aNow)) {
				adopt(qobject_cast<QAbstractItemView*>(aNow));
			}
		});
	}
}
