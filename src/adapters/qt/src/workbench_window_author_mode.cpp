/// \file
/// \brief [[BPR-2196]]. Author, the author's mode (RFC-BPR-0112 as the owner
/// reshaped it on 2026-11-15, design `local-draft-workflow-v2`).
///
/// \details Author is the fifth mode, on Command-5, offered only when the
/// viewer owns what the window shows. It holds the author's bench that the
/// accepted design moved out of Review: This update (the Changes tab) on the
/// left, the selected file or the Edit view in the centre, and a drawer that
/// opens on Workflow, the author's checklist, with the one handoff at the
/// right end of the review bar. The core decides whether Author is offered,
/// the opening mode, the handoff and the checklist
/// (`bpr/core/author_mode.hpp`); this file presents them.

#include "bpr/core/author_mode.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/local_changes_panel.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/local_draft_workflow_area.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/view_picker.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "drawer_tab_bar.hpp"
#include "workbench_window_support.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>

namespace bpr::qt {
	namespace {
		/// Author's drawer tabs, in order: Workflow first (the owner, v001:
		/// "it should become the first tab and become active when we enter
		/// that mode"). There is no Notes tab (owner, v002).
		constexpr int author_workflow_tab = 0;
		constexpr int author_tour_tab     = 1;
		constexpr int author_agent_tab    = 2;

		[[nodiscard]] auto mode_of(const review_modality aModality) noexcept -> core::author_mode
		{
			switch (aModality) {
			case review_modality::overview: return core::author_mode::overview;
			case review_modality::review:   return core::author_mode::review;
			case review_modality::discuss:  return core::author_mode::discuss;
			case review_modality::threads:  return core::author_mode::threads;
			case review_modality::author:   return core::author_mode::author;
			}
			return core::author_mode::review;
		}

		[[nodiscard]] auto modality_of(const core::author_mode aMode) noexcept -> review_modality
		{
			switch (aMode) {
			case core::author_mode::overview: return review_modality::overview;
			case core::author_mode::review:   return review_modality::review;
			case core::author_mode::discuss:  return review_modality::discuss;
			case core::author_mode::threads:  return review_modality::threads;
			case core::author_mode::author:   return review_modality::author;
			}
			return review_modality::review;
		}

		[[nodiscard]] auto local_linked(const local_draft_parts& aParts) -> bool
		{
			const auto& draft = aParts.projection().Draft;
			return draft && (draft->Association.has_value() || (draft->Publication && draft->Publication->Journal.Phase == core::local_publication_phase::linked));
		}
	} // namespace

	auto workbench_window::author_case() const -> std::optional<core::author_case>
	{
		if (LocalDraftShown && LocalDraft != nullptr && LocalDraft->projection().Draft) {
			return core::author_case_of({ .LocalDraft = true, .Linked = local_linked(*LocalDraft) });
		}
		if (LocalDraftShown) {
			return std::nullopt;
		}
		return PullRequestAuthorCase;
	}

	void workbench_window::set_pull_request_author_case(const std::optional<core::author_case> aCase)
	{
		const auto key = overview_review_key();
		if (PullRequestAuthorCase == aCase && AuthorOpenedFor == key) {
			return;
		}
		PullRequestAuthorCase = aCase;
		apply_author_offer();
		// The first time a pull request is known to be the viewer's, it opens
		// where the core says (a draft of theirs in Author), unless the
		// window was left in another mode for it.
		if (aCase && !LocalDraftShown && !key.isEmpty() && AuthorOpenedFor != key) {
			AuthorOpenedFor = key;
			const auto left = ModeLeftIn.find(key);
			const auto mode = core::opening_mode(aCase, left == ModeLeftIn.end() ? std::nullopt : std::optional{ mode_of(left->second) }, mode_of(current_modality()));
			if (mode != mode_of(current_modality())) {
				enter_opening_modality(modality_of(mode));
				return;
			}
		}
		present_author_mode();
	}

	void workbench_window::set_worktree_chooser(std::function<void()> aChoose)
	{
		WorktreeChooser = std::move(aChoose);
	}

	auto workbench_window::author_tabs() const noexcept -> QTabBar*
	{
		return AuthorTabs;
	}

	auto workbench_window::author_key() const -> QString
	{
		if (LocalDraftShown && LocalDraft != nullptr && LocalDraft->projection().Draft) {
			return QStringLiteral("draft:") + QString::fromStdString(LocalDraft->projection().Draft->Id.Value);
		}
		return overview_review_key();
	}

	void workbench_window::note_mode_left_in(const review_modality aModality)
	{
		if (const auto key = author_key(); !key.isEmpty()) {
			ModeLeftIn[key] = aModality;
		}
	}

	auto workbench_window::local_opening_modality() -> review_modality
	{
		const auto key  = author_key();
		const auto left = ModeLeftIn.find(key);
		const auto mode = core::opening_mode(author_case(), left == ModeLeftIn.end() ? std::nullopt : std::optional{ mode_of(left->second) }, core::author_mode::overview);
		return modality_of(mode);
	}

	void workbench_window::open_local_draft_mode()
	{
		// Once per draft shown: where the core says it opens (Author, or the
		// mode it was left in).
		if (!LocalDraftShown || LocalDraft == nullptr || !LocalDraft->projection().Draft || showing_home()) {
			return;
		}
		const auto key = author_key();
		if (key == LocalOpenedFor) {
			return;
		}
		LocalOpenedFor = key;
		choose_modality(local_opening_modality());
	}

	void workbench_window::apply_author_offer()
	{
		if (Modality == nullptr) {
			return;
		}
		const auto offered = author_case().has_value();
		Modality->set_author_offered(offered);
		if (!offered && Modality->current() == review_modality::author) {
			choose_modality(LocalDraftShown ? review_modality::review : review_modality::overview);
		}
		apply_strip_width();
	}

	void workbench_window::build_author_slots()
	{
		if (TrayHeader == nullptr || TrayPages == nullptr || LocalDraft == nullptr) {
			return;
		}
		// The drawer: a header of its own over the tray's pages, shown in
		// Author in place of the tray's.
		auto* const shell = TrayHeader->parentWidget();
		AuthorHeader      = new QWidget{ shell };
		AuthorHeader->setObjectName(QStringLiteral("workbenchAuthorHeader"));
		auto* const headerRow = new QHBoxLayout{ AuthorHeader };
		headerRow->setContentsMargins(TrayHeader->contentsMargins());
		auto* const tabs = new tray_tab_bar{ AuthorHeader };
		tabs->setObjectName(QStringLiteral("workbenchAuthorTabs"));
		tabs->setAccessibleName(QStringLiteral("Author drawer"));
		configure_drawer_tabs(*tabs);
		tabs->addTab(tr("Workflow"));
		tabs->addTab(tr("Tour"));
		tabs->addTab(drawer_icon(drawer_glyph::agent, palette()), tr("Agent"));
		tabs->setTabToolTip(author_workflow_tab, tr("Your checklist before the handoff: the workflow's steps and getting the pull request onto GitHub."));
		tabs->setTabToolTip(author_tour_tab, tr("Your tour, stop by stop. Nothing in it is published until you publish it."));
		tabs->setTabToolTip(author_agent_tab, tr("Agent conversations. Nothing here is published."));
		AuthorTabs = tabs;
		headerRow->addWidget(tabs, 0, Qt::AlignBottom | Qt::AlignLeft);
		headerRow->addStretch(1);
		AuthorHeader->hide();
		if (auto* const column = qobject_cast<QBoxLayout*>(shell->layout()); column != nullptr) {
			column->insertWidget(column->indexOf(TrayHeader) + 1, AuthorHeader);
		}
		AuthorWorkflowHolder = new QWidget{ shell };
		AuthorWorkflowHolder->setObjectName(QStringLiteral("workbenchAuthorWorkflow"));
		auto* const holding = new QVBoxLayout{ AuthorWorkflowHolder };
		holding->setContentsMargins(0, 0, 0, 0);
		holding->addWidget(LocalDraft->workflow_page(), 1);
		LocalDraft->workflow_page()->show();
		TrayPages->addWidget(AuthorWorkflowHolder);
		connect(tabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			if (AuthorApplying || current_modality() != review_modality::author) {
				return;
			}
			show_author_tab(aIndex);
			remember_author_mode();
		});
		// A tray tab chosen another way (Command-I's Agent) shows in Author's tabs.
		connect(TrayTabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			if (AuthorApplying || AuthorTabs == nullptr || current_modality() != review_modality::author) {
				return;
			}
			const auto tab = aIndex == tray_tour_tab ? author_tour_tab : aIndex == tray_findings_tab ? author_agent_tab
																									 : -1;
			if (tab >= 0 && AuthorTabs->currentIndex() != tab) {
				const auto blocker = QSignalBlocker{ AuthorTabs };
				AuthorTabs->setCurrentIndex(tab);
			}
		});

		// The left drawer: without a worktree, This update is the one ask.
		if (FileSearchTabs != nullptr) {
			AuthorAskPage = new QWidget{ FileSearchTabs };
			AuthorAskPage->setObjectName(QStringLiteral("workbenchAuthorAsk"));
			auto* const ask = new QVBoxLayout{ AuthorAskPage };
			ask->setContentsMargins(12, 12, 12, 12);
			ask->setSpacing(10);
			auto* const words = new QLabel{ tr("Author works in your worktree: the folder where this pull request's branch is checked out. Choose it once, and This update shows what would go."), AuthorAskPage };
			words->setObjectName(QStringLiteral("workbenchAuthorAskText"));
			words->setWordWrap(true);
			ask->addWidget(words);
			auto* const choose = new QPushButton{ tr("Choose Worktree…"), AuthorAskPage };
			choose->setObjectName(QStringLiteral("workbenchAuthorChooseWorktree"));
			connect(choose, &QPushButton::clicked, this, [this] {
				if (WorktreeChooser) {
					WorktreeChooser();
				}
			});
			ask->addWidget(choose, 0, Qt::AlignLeft);
			ask->addStretch(1);
			const auto tab = FileSearchTabs->addTab(AuthorAskPage, tr("This update"));
			FileSearchTabs->setTabVisible(tab, false);
		}
		LocalDraft->workflow()->set_worktree_dispatch([this] {
			if (WorktreeChooser) {
				WorktreeChooser();
			}
		});
		LocalDraft->HandedOff = [this](const core::author_handoff aHandoff) {
			if (aHandoff == core::author_handoff::ready_for_review && DraftStateDispatch && DraftStateOffer == std::optional{ core::draft_state_change::ready_for_review }) {
				DraftStateDispatch(core::draft_state_change::ready_for_review);
			} else if (aHandoff == core::author_handoff::publish_update && LocalDraft->local_changes()->push_button()->isEnabled()) {
				// RFC-BPR-0124's replies are not built: the update is the push.
				LocalDraft->local_changes()->push_button()->click();
			}
		};
	}

	void workbench_window::show_author_tab(const int aIndex)
	{
		if (TrayPages == nullptr || TrayTabs == nullptr) {
			return;
		}
		AuthorApplying = true;
		if (aIndex == author_workflow_tab) {
			TrayPages->setCurrentWidget(AuthorWorkflowHolder);
			if (CommentsTools != nullptr) {
				CommentsTools->hide();
			}
			if (TourTools != nullptr) {
				TourTools->hide();
			}
		} else {
			const auto tray = aIndex == author_tour_tab ? tray_tour_tab : tray_findings_tab;
			if (TrayTabs->currentIndex() != tray) {
				TrayTabs->setCurrentIndex(tray);
			}
			TrayPages->setCurrentIndex(tray);
		}
		AuthorApplying = false;
	}

	void workbench_window::remember_author_mode()
	{
		const auto key = author_key();
		if (key.isEmpty() || AuthorTabs == nullptr) {
			return;
		}
		auto& memory = AuthorMemory[key];
		memory.Tab   = AuthorTabs->currentIndex();
		if (ReviewViewPicker != nullptr) {
			memory.View = ReviewViewPicker->view();
		}
		if (LocalDraft != nullptr && LocalDraft->local_projection().Active) {
			memory.Comparison = LocalDraft->local_projection().Comparison;
			memory.File       = LocalDraft->local_projection().File ? LocalDraft->local_projection().File->Path : std::string{ };
		}
	}

	void workbench_window::enter_author_mode()
	{
		// The drawers Author needs: This update on the left, Workflow on the right.
		if (Dispatch) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::files, .State = core::workbench_drawer_state::expanded });
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		const auto key    = author_key();
		const auto found  = AuthorMemory.find(key);
		const auto memory = found == AuthorMemory.end() ? std::optional<author_memory>{ } : std::optional{ found->second };
		if (AuthorTabs != nullptr) {
			const auto blocker = QSignalBlocker{ AuthorTabs };
			// The first entry opens the drawer on Workflow; later ones where it was.
			AuthorTabs->setCurrentIndex(memory ? memory->Tab : author_workflow_tab);
		}
		if (memory && LocalDraft != nullptr && LocalDraft->local_projection().Active) {
			const auto& local = LocalDraft->local_projection();
			if (memory->Comparison && local.Comparison != *memory->Comparison) {
				if (auto* const pick = LocalDraft->local_changes()->pick(*memory->Comparison); pick != nullptr) {
					pick->click();
				}
			}
			if (memory->View) {
				pick_author_view(*memory->View);
			}
			if (!memory->File.empty() && (!local.File || local.File->Path != memory->File)) {
				static_cast<void>(LocalDraft->local_changes()->select_file(memory->File));
			}
		} else {
			pick_author_file();
		}
	}

	void workbench_window::pick_author_file()
	{
		// Once per draft, the first time This update can show a file: the
		// first that would go. With nothing uncommitted, This update is the
		// whole pull request, so the first screen is the proposed diff.
		if (LocalDraft == nullptr || !LocalDraft->local_projection().Active || LocalDraft->local_projection().File) {
			return;
		}
		const auto key = author_key();
		if (AuthorPicking || key.isEmpty() || key == AuthorFilePickedFor || AuthorMemory.contains(key)) {
			return;
		}
		// A pick answers synchronously with another projection: it must not re-enter.
		AuthorPicking     = true;
		const auto& local = LocalDraft->local_projection();
		if (AuthorComparisonPickedFor != key && local.Comparison == core::local_comparison::uncommitted && local.Status.Staged.empty() && local.Status.Unstaged.empty()
			&& !local.WholePullRequest.empty()) {
			AuthorComparisonPickedFor = key;
			if (auto* const pick = LocalDraft->local_changes()->pick(core::local_comparison::whole_pull_request); pick != nullptr && pick->isEnabled()) {
				// The file is picked when the whole pull request's list arrives.
				pick->click();
				AuthorPicking = false;
				return;
			}
		}
		// Picked once a file is listed to pick; until then, the next projection tries again.
		if (LocalDraft->local_changes()->select_file({ })) {
			AuthorFilePickedFor = key;
			// A file being changed opens in Edit; the whole pull request's
			// keeps the comparison.
			if (LocalDraft->local_projection().Comparison == core::local_comparison::uncommitted) {
				pick_author_view(review_view::edit);
			}
		}
		AuthorPicking = false;
	}

	void workbench_window::pick_author_view(const review_view aView)
	{
		QTimer::singleShot(0, this, [this, aView] {
			if (!AuthorShown || ReviewViewPicker == nullptr || ReviewViewPicker->view() == aView) {
				return;
			}
			if (auto* const segment = ReviewViewPicker->segment(aView); segment != nullptr && segment->isEnabled() && segment->isVisible()) {
				segment->click();
			}
		});
	}

	void workbench_window::present_author_mode()
	{
		if (LocalDraft == nullptr) {
			return;
		}
		const auto shownCase = author_case();
		if (!shownCase) {
			return;
		}
		const auto  local     = LocalDraftShown && LocalDraft->projection().Draft.has_value();
		const auto& view      = LocalDraft->projection();
		const auto  external  = local && view.Draft->Workflow && view.Draft->Workflow->Publisher == core::workflow_publisher::external;
		auto        presented = local_draft_author_mode{
			.Case    = *shownCase,
			.Handoff = core::author_handoff_of(*shownCase, external ? core::workflow_publisher::external : core::workflow_publisher::built_in),
		};
		presented.Checklist = core::decide_author_checklist({ .Case = *shownCase, .Draft = local ? &*view.Draft : nullptr, .Branch = view.Publishing.Branch });
		switch (presented.Handoff) {
		case core::author_handoff::ready_for_review:
			presented.HandoffEnabled = DraftStateOffer == std::optional{ core::draft_state_change::ready_for_review };
			break;
		case core::author_handoff::publish_update:
			presented.HandoffEnabled = local && LocalDraft->local_projection().Active && LocalDraft->local_changes()->push_button()->isEnabled();
			break;
		case core::author_handoff::create_draft_pull_request:
		case core::author_handoff::hand_off_plan:
			break;
		}
		if (presented.Case == LocalDraft->author_mode().Case && presented.Handoff == LocalDraft->author_mode().Handoff && presented.HandoffEnabled == LocalDraft->author_mode().HandoffEnabled
			&& presented.Checklist == LocalDraft->author_mode().Checklist) {
			return;
		}
		LocalDraft->present_author_mode(presented);
	}

	void workbench_window::apply_author_surface()
	{
		if (LocalDraft == nullptr || AuthorTabs == nullptr || AuthorApplying) {
			return;
		}
		AuthorApplying       = true;
		const auto authoring = current_modality() == review_modality::author && author_case().has_value() && !showing_home();
		const auto author    = authoring && LocalDraftShown && LocalDraft->author();
		// A pull request with no worktree asks for one; a draft read without
		// the author's Git keeps its candidate's Files.
		const auto asking = authoring && !LocalDraftShown;
		const auto files  = authoring && LocalDraftShown && !author;
		AuthorShown       = authoring;
		// The bar's right end: the one handoff, in Author only. On a pull
		// request the reviewer's own controls (progress, Compose review and
		// its chip) step aside: the author does not review their own work.
		if (LocalBarTrailingAction != nullptr) {
			LocalBarTrailingAction->setVisible(authoring);
		}
		if (!LocalDraftShown && ReviewBar != nullptr) {
			for (auto* const action : ReviewBar->actions()) {
				const auto name     = action->objectName();
				const auto reviewer = name == QLatin1String{ "reviewBarProgressGapAction" } || name == QLatin1String{ "workbenchProgressTrackAction" } || action == ComposeReviewAction;
				if (reviewer && action->isVisible() == authoring) {
					action->setVisible(!authoring);
				}
			}
			if (ReviewChipAction != nullptr && authoring) {
				ReviewChipAction->setVisible(false);
			} else if (ReviewChipAction != nullptr && !authoring) {
				update_review_chip();
			}
		}
		// The left drawer: This update in Author, Files and Search elsewhere.
		if (FileSearchTabs != nullptr) {
			const auto changes = local_changes_tab();
			const auto ask     = AuthorAskPage == nullptr ? -1 : FileSearchTabs->indexOf(AuthorAskPage);
			if (changes >= 0) {
				FileSearchTabs->setTabText(changes, tr("This update"));
				FileSearchTabs->setTabToolTip(changes, tr("The files that would go: stage, commit and push."));
			}
			for (auto index = 0; index < FileSearchTabs->count(); ++index) {
				const auto own   = index == changes || index == ask;
				const auto shown = authoring ? (index == changes && author) || (index == ask && asking) || (index == 0 && files) : !own;
				FileSearchTabs->setTabVisible(index, shown);
			}
			const auto wanted  = authoring ? (author ? changes : asking ? ask
																		: 0)
										   : 0;
			const auto current = FileSearchTabs->currentIndex();
			const auto stray   = authoring ? current != wanted : current == changes || current == ask;
			if (wanted >= 0 && stray) {
				FileSearchTabs->setCurrentIndex(wanted);
			}
		}
		// The drawer: Author's own tabs over the tray's pages.
		if (TrayHeader != nullptr && AuthorHeader != nullptr) {
			TrayHeader->setVisible(!authoring);
			AuthorHeader->setVisible(authoring);
		}
		AuthorApplying = false;
		if (authoring) {
			show_author_tab(AuthorTabs->currentIndex());
		} else if (TrayPages != nullptr && TrayTabs != nullptr && TrayPages->currentWidget() == AuthorWorkflowHolder) {
			const auto index = TrayTabs->currentIndex();
			if (LocalDraftShown && index == tray_comments_tab) {
				TrayPages->setCurrentWidget(LocalDraft->notes_page());
			} else {
				TrayPages->setCurrentIndex(index);
			}
			if (CommentsTools != nullptr) {
				CommentsTools->setVisible(!LocalDraftShown && index == tray_comments_tab);
			}
			if (TourTools != nullptr) {
				TourTools->setVisible(index == tray_tour_tab);
			}
		}
		// Overview: a local draft has no Status tab until its pull request exists.
		if (OverviewPage != nullptr && OverviewPage->status_drawer() != nullptr) {
			const auto unlinked = LocalDraftShown && !local_linked(*LocalDraft);
			OverviewPage->status_drawer()->set_status_tabs_shown(!unlinked);
		}
		present_author_mode();
	}
} // namespace bpr::qt
