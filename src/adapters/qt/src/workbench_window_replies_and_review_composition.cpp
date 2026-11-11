#include "bpr/core/overview_tour.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAction>
#include <QAnyStringView>
#include <QApplication>
#include <QBoxLayout>
#include <QChar>
#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTabBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// [[BPR-1212]]. The conversations narrow to a list beside Discuss's composer.
		constexpr int discuss_composer_list_width = 340;
	}

	void workbench_window::draft_thread_reply(const std::string& aIdentity, const QString& aText, std::function<void(bool)> aDone)
	{
		if (PendingReplyDone) {
			// An earlier one still waiting is replaced: it was not written.
			std::exchange(PendingReplyDone, { })(false);
		}
		PendingReplyText  = std::pair{ aIdentity, aText };
		PendingReplyDone  = std::move(aDone);
		PendingReplyTries = 50;
		reply_to_thread(aIdentity, true);
		write_pending_reply();
	}

	void workbench_window::open_thread_reply(const std::string& aIdentity) { reply_to_thread(aIdentity, true); }

	auto workbench_window::draft_pull_request_comment(const QString& aText) -> bool
	{
		open_overview_tab(overview_drawer_tab::comments);
		auto* const composer = ConversationPanel != nullptr ? ConversationPanel->new_comment_editor() : nullptr;
		if (!showing_overview() || composer == nullptr || ConversationPanel->new_comment()->isHidden() || composer->isReadOnly()) {
			return false;
		}
		// Written as if typed: the composer opens and keeps its own draft.
		const auto existing = composer->toPlainText();
		composer->setPlainText(existing.trimmed().isEmpty() ? aText : existing + QStringLiteral("\n\n") + aText);
		composer->moveCursor(QTextCursor::End);
		composer->setFocus(Qt::OtherFocusReason);
		return true;
	}

	void workbench_window::open_pull_request_comment()
	{
		open_overview_tab(overview_drawer_tab::comments);
		if (auto* const composer = ConversationPanel != nullptr ? ConversationPanel->new_comment_editor() : nullptr; composer != nullptr && showing_overview()) {
			composer->moveCursor(QTextCursor::End);
			composer->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::write_pending_reply()
	{
		if (!PendingReplyText.has_value() || ReviewEditor == nullptr) {
			return;
		}
		// The reply opens once its comparison is shown and its draft made;
		// the words are written into that draft as if typed, and looked for
		// again for five seconds at most.
		constexpr auto prefix = std::string_view{ "review-thread:" };
		const auto&    shown  = ReviewEditor->shown_draft();
		const auto     thread = std::string_view{ PendingReplyText->first }.starts_with(prefix) ? PendingReplyText->first.substr(prefix.size()) : PendingReplyText->first;
		if (shown.has_value() && shown->ReplyToThread == thread) {
			auto* const body     = ReviewEditor->body_editor();
			const auto  existing = body->toPlainText();
			body->setPlainText(existing.trimmed().isEmpty() ? PendingReplyText->second : existing + QStringLiteral("\n\n") + PendingReplyText->second);
			body->moveCursor(QTextCursor::End);
			body->setFocus(Qt::OtherFocusReason);
			PendingReplyText.reset();
			if (PendingReplyDone) {
				std::exchange(PendingReplyDone, { })(true);
			}
			return;
		}
		if (--PendingReplyTries <= 0) {
			QGuiApplication::clipboard()->setText(PendingReplyText->second);
			statusBar()->showMessage(tr("The reply could not be opened here. The agent's words are on the clipboard."), 8000);
			PendingReplyText.reset();
			if (PendingReplyDone) {
				std::exchange(PendingReplyDone, { })(false);
			}
			return;
		}
		QTimer::singleShot(100, this, [this] { write_pending_reply(); });
	}

	void workbench_window::reply_to_thread(const std::string& aIdentity, const bool aInPlace)
	{
		// [[BPR-0649]], [[BPR-0657]], [[BPR-1046]]. The thread becomes current
		// and its line is selected, which composes the draft on that line; only
		// then is the thread named, so that draft answers it and no draft on
		// another line can. While the thread's own file is still loading, the
		// reply waits for its comparison instead of refusing.
		DeferredReply.reset();
		if (ConversationPanel == nullptr) {
			return;
		}
		const auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (index.isValid()) {
			ConversationPanel->choose(index);
		}
		if (thread_comparison_loading(aIdentity)) {
			DeferredReply        = aIdentity;
			DeferredReplyInPlace = aInPlace;
			return;
		}
		begin_thread_reply(aIdentity, aInPlace);
	}

	void workbench_window::step_to_next_new_comment()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto& unread = ConversationPanel->cards()->unread_conversations_in_navigation_order();
		if (unread.empty()) {
			return;
		}
		// The first unread thread after the current one in file order, and
		// the first of all once the last is passed.
		const auto& current = ConversationPanel->cards()->current_thread();
		auto        next    = unread.front();
		if (const auto at = std::ranges::find(unread, current); at != unread.end()) {
			const auto following = std::next(at);
			next                 = following != unread.end() ? *following : unread.front();
		}
		go_to_conversation(next);
	}

	void workbench_window::go_to_conversation(const std::string& aIdentity)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto& next = aIdentity;
		if (const auto location = Finder->locate(next); location.has_value() && location->Path.isEmpty()) {
			// [[BPR-1781]]. A comment on the pull request is in its
			// conversation, Overview's Comments tab.
			if (Modality && (Modality->current() == review_modality::review || Modality->current() == review_modality::overview)) {
				open_pull_request_conversation(next);
			} else {
				static_cast<void>(move_thread(next, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
			}
			return;
		}
		// The conversation view's current index is what reveals the line, as a
		// press on a card does; the cards follow it.
		ConversationPanel->set_pull_request_conversation(false);
		const auto index = ConversationPanel->model()->index_for_id(next);
		if (index.isValid()) {
			ConversationPanel->view()->setCurrentIndex(index);
		}
		ConversationPanel->cards()->set_current_thread(next);
		// [[BPR-1290]]. A thread of this file opens in place.
		if (std::ranges::any_of(ConversationPanel->cards()->cards(), [&next](const thread_card& aCard) { return aCard.Id == next; })) {
			ConversationPanel->cards()->open_thread(next);
		}
	}

	void workbench_window::lend_conversation_tabs(const bool aToOverview)
	{
		// [[BPR-1781]]. One conversation panel, one notes panel and one agent
		// page: Overview's drawer holds them while Overview shows, and Review's
		// tray holds them otherwise. Moving the same objects, as Discuss
		// borrows the conversation panel and the Agent tab borrows its panels,
		// keeps every draft, open card, note and agent turn as it was; each
		// mode keeps its own place.
		auto* const drawer = OverviewPage != nullptr ? OverviewPage->status_drawer() : nullptr;
		if (drawer == nullptr || ConversationPanel == nullptr || PrivateNotes == nullptr || AgentBody == nullptr || ConversationTabsLent == aToOverview) {
			return;
		}
		ConversationTabsLent = aToOverview;
		const auto move      = [](QWidget* const aWidget, QWidget* const aHome) {
			if (aWidget->parentWidget() == aHome) {
				return;
			}
			static_cast<QBoxLayout*>(aHome->layout())->insertWidget(0, aWidget, 1);
			aWidget->show();
		};
		if (aToOverview) {
			// Discuss's panel stays Discuss's until it leaves.
			if (ConversationPanel->parentWidget() != DiscussPage) {
				move(ConversationPanel, drawer->holder(overview_drawer_tab::comments));
				ConversationPanel->set_pull_request_conversation(true);
			}
			ConversationPanel->set_new_comment_offered(ConversationPanel->parentWidget() != DiscussPage);
			move(PrivateNotes, drawer->holder(overview_drawer_tab::notes));
			PrivateNotes->set_pull_request_only(true);
			move(AgentBody, drawer->holder(overview_drawer_tab::agent));
		} else {
			if (ConversationPanel->parentWidget() == drawer->holder(overview_drawer_tab::comments)) {
				ConversationPanel->set_pull_request_conversation(false);
				move(ConversationPanel, ConversationHome);
			}
			ConversationPanel->set_new_comment_offered(false);
			PrivateNotes->set_pull_request_only(false);
			move(PrivateNotes, NotesHome);
			move(AgentBody, AgentHome);
		}
		// What the agent's prompts may name follows: the pull request on
		// Overview, the open file and lines beside the code.
		schedule_agent_suggestions();
	}

	void workbench_window::open_overview_tab(const overview_drawer_tab aTab)
	{
		if (OverviewPage == nullptr || OverviewPage->status_drawer() == nullptr) {
			return;
		}
		close_review_composition(false);
		close_review_sheet();
		choose_home(false);
		if (Modality != nullptr && Modality->current() != review_modality::overview) {
			choose_modality(review_modality::overview);
		}
		apply_modality_surface();
		OverviewPage->status_drawer()->show_tab(aTab);
	}

	void workbench_window::open_pull_request_conversation(const std::string& aIdentity)
	{
		open_overview_tab(overview_drawer_tab::comments);
		if (!showing_overview() || ConversationPanel == nullptr) {
			return;
		}
		if (!aIdentity.empty()) {
			// [[BPR-1808]]. Overview's own filters never hide what a link asked for.
			static_cast<void>(ConversationPanel->reveal_in_conversation(aIdentity));
			ConversationPanel->cards()->open_thread(aIdentity);
		} else {
			ConversationPanel->cards()->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::set_tour_start_dispatch(std::function<void()> aDispatch)
	{
		TourStartDispatch = std::move(aDispatch);
	}

	void workbench_window::refresh_overview_tour()
	{
		if (OverviewPage == nullptr || OverviewPage->status_drawer() == nullptr || TourEditor == nullptr) {
			return;
		}
		const auto& shown  = TourEditor->shown();
		const auto  author = OverviewPage->projection().has_value() && std::ranges::any_of(OverviewPage->projection()->Status.Roles.Roles, [](const core::review_role& aRole) { return aRole.Kind == core::review_role_kind::author; });
		const auto  tour   = core::decide_overview_tour(shown.Draft.get(), shown.Offered.get(), author);
		auto* const drawer = OverviewPage->status_drawer();
		if (drawer->tour_summary()->shown() != tour) {
			drawer->tour_summary()->present(tour);
		}
		drawer->set_tour_tab_shown(tour.Kind != core::overview_tour_kind::hidden);
	}

	void workbench_window::follow_overview_tour(const overview_tour_gesture aGesture)
	{
		// [[BPR-1781]]. The tab presents and routes: writing and following
		// are Review's, beside the code.
		switch (aGesture) {
		case overview_tour_gesture::prepare:
			if (PrepareTourAction != nullptr && PrepareTourAction->isEnabled()) {
				PrepareTourAction->trigger();
			} else {
				open_review_tour();
			}
			return;
		case overview_tour_gesture::continue_tour:
			open_review_tour();
			// Its own place: the stop it was on, or the first.
			if (const auto* const stop = TourEditor->selected_stop(); stop != nullptr) {
				TourEditor->go_to_stop(stop->Id);
			} else if (TourEditor->shown().Draft && !TourEditor->shown().Draft->Stops.empty()) {
				TourEditor->go_to_stop(TourEditor->shown().Draft->Stops.front().Id);
			}
			return;
		case overview_tour_gesture::start_tour:
			if (TourStartDispatch) {
				TourStartDispatch();
			} else {
				start_tour_at_first_stop();
			}
			return;
		}
	}

	void workbench_window::set_tour_stop_dispatch(std::function<void(const std::string&, const core::published_tour_stop&)> aDispatch)
	{
		TourStopDispatch = std::move(aDispatch);
	}

	void workbench_window::refresh_tour_open_offer()
	{
		if (TourEditor == nullptr || ConversationPanel == nullptr) {
			return;
		}
		// [[BPR-1806]]. Open in tour while there is a published tour to
		// follow: the window's own, or one GitHub offers for import.
		const auto& shown = TourEditor->shown();
		ConversationPanel->cards()->set_tour_open_offered((shown.Draft && shown.Draft->Remote && !shown.Draft->Stops.empty()) || shown.Offered != nullptr);
	}

	void workbench_window::start_tour_at_published_stop(const std::string& aPost, const core::published_tour_stop& aStop)
	{
		open_review_tour();
		if (!TourEditor->shown().Draft) {
			return;
		}
		if (const auto stop = core::find_published_tour_stop(*TourEditor->shown().Draft, aPost, aStop)) {
			TourEditor->go_to_stop(*stop);
		}
	}

	void workbench_window::start_tour_at_first_stop()
	{
		open_review_tour();
		if (TourEditor->shown().Draft && !TourEditor->shown().Draft->Stops.empty()) {
			TourEditor->go_to_stop(TourEditor->shown().Draft->Stops.front().Id);
		}
	}

	void workbench_window::apply_modality_surface()
	{
		present_compose_toggle();
		if (CentreStack == nullptr || OverviewPage == nullptr) {
			return;
		}
		// Composition covers the invoking page without replacing its mode or
		// selection. Discuss borrows the same panel used beside the code.
		const auto overview = Modality != nullptr
			&& Modality->current() == review_modality::overview
			&& (OverviewPage->presented() || HasCachedOverview || (SessionPanel != nullptr && SessionPanel->status() == core::github_status::loading_pull_request));
		// [[BPR-1212]]. A composer is Discuss's: it shows there, beside the
		// conversations, whatever surface opened it.
		const auto discuss         = (Modality != nullptr && Modality->current() == review_modality::discuss) || CompositionPage != nullptr;
		const auto threads         = Modality != nullptr && Modality->current() == review_modality::threads;
		const auto enteringDiscuss = discuss && ConversationPanel != nullptr && ConversationPanel->parentWidget() != DiscussPage;
		// [[BPR-1781]]. Overview shows when nothing above covers it; then its
		// drawer borrows the conversation tabs.
		const auto overviewShown = overview && !discuss && !(threads && ThreadsPage != nullptr);
		if (ConversationPanel != nullptr) {
			if (discuss && ConversationPanel->parentWidget() != DiscussPage) {
				// Leaving Overview's conversation for Discuss's feed.
				ConversationPanel->set_pull_request_conversation(false);
				static_cast<QBoxLayout*>(DiscussPage->layout())->insertWidget(0, ConversationPanel, 1);
				ConversationPanel->show();
			} else if (!discuss && ConversationPanel->parentWidget() == DiscussPage) {
				if (ConversationPanel->isAncestorOf(QApplication::focusWidget())) {
					DiscussReturnFocus = QApplication::focusWidget();
				}
				ConversationLayout->insertWidget(0, ConversationPanel, 1);
				ConversationPanel->show();
			}
			ConversationPanel->set_discuss(discuss);
		}
		lend_conversation_tabs(overviewShown);
		auto* page = ComparisonPage;
		if (overview) {
			page = OverviewPage;
		}
		if (discuss) {
			page = DiscussPage;
		}
		if (threads && ThreadsPage != nullptr) {
			page = ThreadsPage;
		}
		if (CompositionPage != nullptr) {
			page = DiscussPage;
		}
		// [[BPR-1593]]. The tour page covers Review while it is open, with no
		// drawers beside it.
		if (TourPageOpen && TourPage != nullptr) {
			if (Modality != nullptr && Modality->current() == review_modality::review && CompositionPage == nullptr) {
				page = TourPage;
			}
		}
		// The composer column shows the one page being written, and the
		// conversations narrow to a list beside it.
		if (DiscussComposer != nullptr) {
			for (auto* const composition : { static_cast<QWidget*>(ComposeInlinePage), static_cast<QWidget*>(ComposeReviewPage) }) {
				if (composition != nullptr) {
					composition->setVisible(composition == CompositionPage);
				}
			}
			DiscussComposer->setVisible(CompositionPage != nullptr);
			// [[BPR-1590]]. The expanded review is one centred column, as the
			// chosen layout A draws it (RFC-BPR-0085 D6), with no thread list; a
			// maximized draft keeps its list beside it ([[BPR-1212]]).
			const auto focused = CompositionPage != nullptr && CompositionPage == ComposeReviewPage;
			const auto column  = core::reading_width_limit(Behaviour.ReadingWidth);
			static_cast<void>(column);
			if (focused) {
				centre_review_page();
			}
			if (ConversationPanel != nullptr && ConversationPanel->parentWidget() == DiscussPage) {
				ConversationPanel->setVisible(!focused);
			}
			if (ConversationPanel != nullptr) {
				const auto listed = CompositionPage != nullptr && ConversationPanel->parentWidget() == DiscussPage;
				// [[BPR-1584]]. Discuss's notes stop at the reading width,
				// at the page's left, as Threads' rows do; beside the code
				// the drawer keeps its own bounds.
				const auto limit = core::reading_width_limit(Behaviour.ReadingWidth);
				const auto read  = ConversationPanel->parentWidget() == DiscussPage && limit.has_value()
					? static_cast<int>(std::min<std::uint32_t>(*limit, QWIDGETSIZE_MAX))
					: QWIDGETSIZE_MAX;
				ConversationPanel->setMaximumWidth(listed ? discuss_composer_list_width : read);
				if (ConversationPanel->parentWidget() == DiscussPage) {
					static_cast<QBoxLayout*>(DiscussPage->layout())->setStretchFactor(ConversationPanel, listed ? 0 : 1);
				}
			}
		}
		if (ToggleStatusDrawerAction != nullptr) {
			ToggleStatusDrawerAction->setEnabled(page == OverviewPage);
		}
		const auto enteringOverview = page == OverviewPage && CentreStack->currentWidget() != OverviewPage;
		const auto enteringThreads  = page == ThreadsPage && CentreStack->currentWidget() != ThreadsPage;
		CentreStack->setCurrentWidget(page);
		apply_drawer_layout();
		update_review_readiness();
		// [[BPR-1089]]. Back on the Overview after a count opened Discuss, the
		// same cell has focus.
		if (enteringOverview && OverviewReturnFocus) {
			OverviewReturnFocus->setFocus(Qt::OtherFocusReason);
			OverviewReturnFocus.clear();
		}
		if (enteringDiscuss && CompositionPage == nullptr && DiscussReturnFocus) {
			DiscussReturnFocus->setFocus(Qt::OtherFocusReason);
		}
		// [[BPR-1293]]. Entering Threads: the search has focus and the
		// cursor starts on the conversation open in Review.
		if (enteringThreads && Finder != nullptr) {
			present_finder_reading();
			Finder->enter();
		}
		if (ShowThreadsAction != nullptr) {
			ShowThreadsAction->setEnabled(HasGithubPullRequest);
		}
		update_show_thread_actions();
	}

	void workbench_window::open_review_composition(const bool aInline)
	{
		// [[BPR-1590]]. Compose review and Finish review open the sheet; only
		// its Expand opens the review as a page.
		if (!aInline && !ExpandingReviewSheet) {
			open_review_sheet();
			return;
		}
		if (aInline && ReviewSheetOpen) {
			close_review_sheet();
		}
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		// [[BPR-1212]]. The draft on the selected line is written in the
		// drawer at its line while the code shows; the full composer, and
		// the whole-review summary (owner's Q3: "A: move to Discuss"), are
		// Discuss's.
		if (aInline && CompositionPage == nullptr && Modality != nullptr && Modality->current() == review_modality::review && !showing_home()
			&& ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && !ReviewEditor->shown_draft()->ReplyToThread.has_value()
			&& !ReviewEditor->body_editor()->isReadOnly()) {
			if (begin_new_comment(false)) {
				return;
			}
		}
		if (CompositionPage == nullptr) {
			CompositionReturnFocus     = QApplication::focusWidget();
			CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
			CompositionReturnsToDrawer = false;
		}
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr && ConversationPanel->cards()->writing_draft()) {
			ConversationPanel->cards()->end_draft();
		}
		if (Modality != nullptr && Modality->current() != review_modality::discuss) {
			const auto returnFocus     = CompositionReturnFocus;
			const auto returnModality  = CompositionReturnModality;
			const auto returnsToDrawer = CompositionReturnsToDrawer;
			choose_modality(review_modality::discuss);
			CompositionReturnFocus     = returnFocus;
			CompositionReturnModality  = returnModality;
			CompositionReturnsToDrawer = returnsToDrawer;
		}
		CompositionPage = aInline ? ComposeInlinePage : ComposeReviewPage;
		apply_modality_surface();
		present_inline_draft();
		auto* body = aInline ? ReviewEditor->body_editor() : ReviewSubmissionEditor->body_editor();
		if (body->isVisible() && body->isEnabled()) {
			body->setFocus(Qt::OtherFocusReason);
			if (aInline && CompositionCaret >= 0) {
				auto cursor = ReviewEditor->body_editor()->textCursor();
				cursor.setPosition(std::min(CompositionCaret, static_cast<int>(ReviewEditor->body_editor()->document()->characterCount() - 1)));
				ReviewEditor->body_editor()->setTextCursor(cursor);
			}
		} else if (auto* back = CompositionPage->findChild<QPushButton*>(aInline ? QStringLiteral("composeInlineDraftPageBack") : QStringLiteral("composeReviewPageBack"))) {
			back->setFocus(Qt::OtherFocusReason);
		}
		CompositionCaret = -1;
	}

	void workbench_window::open_draft_in_discuss()
	{
		if (CompositionPage == nullptr) {
			CompositionReturnFocus     = QApplication::focusWidget();
			CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
			CompositionReturnsToDrawer = false;
		}
		CompositionPage = ComposeInlinePage;
		open_review_composition(true);
	}

	void workbench_window::maximize_drawer_draft()
	{
		// [[BPR-1212]]. Maximize: the same draft, with room, in Discuss; the
		// same key brings it back to the drawer with its text and caret.
		auto* const cards = ConversationPanel != nullptr ? ConversationPanel->cards() : nullptr;
		if (cards != nullptr && cards->draft_editor() != nullptr) {
			CompositionCaret = cards->draft_editor()->textCursor().position();
		}
		CompositionReturnFocus     = ComparisonSurface != nullptr ? ComparisonSurface->right_pane() : nullptr;
		CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
		CompositionReturnsToDrawer = true;
		CompositionPage            = ComposeInlinePage;
		open_review_composition(true);
	}

	void workbench_window::close_review_composition(const bool aRestoreSheet)
	{
		const auto reviewPage      = CompositionPage != nullptr && CompositionPage == ComposeReviewPage;
		const auto inlineDraft     = CompositionPage == ComposeInlinePage;
		const auto returnsToDrawer = inlineDraft && CompositionReturnsToDrawer;
		const auto returnModality  = std::exchange(CompositionReturnModality, std::nullopt);
		CompositionReturnsToDrawer = false;
		if (returnsToDrawer && ReviewEditor != nullptr) {
			CompositionCaret = ReviewEditor->body_editor()->textCursor().position();
		}
		CompositionPage = nullptr;
		if (returnModality.has_value() && Modality != nullptr && Modality->current() != *returnModality) {
			const auto focus = CompositionReturnFocus;
			choose_modality(*returnModality);
			CompositionReturnFocus = focus;
		} else {
			apply_modality_surface();
		}
		if (returnsToDrawer && Modality != nullptr && Modality->current() == review_modality::review && begin_new_comment(false)) {
			auto* const editor = ConversationPanel->cards()->draft_editor();
			if (editor != nullptr && CompositionCaret >= 0) {
				auto cursor = editor->textCursor();
				cursor.setPosition(std::min(CompositionCaret, static_cast<int>(editor->document()->characterCount() - 1)));
				editor->setTextCursor(cursor);
			}
			CompositionCaret = -1;
			return;
		}
		CompositionCaret = -1;
		// [[BPR-1590]]. The expanded review restores to its sheet, beside the
		// surface it came from.
		if (reviewPage && aRestoreSheet) {
			const auto back = CompositionReturnFocus;
			open_review_sheet();
			SheetReturnFocus = back;
			return;
		}
		if (CompositionReturnFocus && CompositionReturnFocus->isVisible()) {
			CompositionReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	auto workbench_window::review_sheet() const noexcept -> compose_review_sheet*
	{
		return ReviewSheet;
	}

	auto workbench_window::review_sheet_open() const noexcept -> bool
	{
		return ReviewSheetOpen;
	}

	void workbench_window::centre_review_page()
	{
		if (ComposeReviewPage == nullptr || ComposeReviewPage->widget() == nullptr || ComposeReviewPage->widget()->layout() == nullptr) {
			return;
		}
		// The chosen layout A: a 680-pixel column, centred, at least 24 pixels in.
		constexpr auto column = 680;
		const auto     side   = std::max(24, (ComposeReviewPage->viewport()->width() - column) / 2);
		ComposeReviewPage->widget()->layout()->setContentsMargins(side, 12, side, 24);
	}

	void workbench_window::present_review_context(const bool aInSheet)
	{
		if (ReviewSubmissionEditor == nullptr) {
			return;
		}
		// The pull request in one quiet line under the page's title; the
		// sheet's header says enough, as layout C draws it.
		auto context = QString{ };
		if (!aInSheet && OverviewPage != nullptr && OverviewPage->projection().has_value()) {
			const auto& shown = *OverviewPage->projection();
			context           = tr("%1/%2 #%3 · %4").arg(QString::fromStdString(shown.Owner), QString::fromStdString(shown.RepositoryName), QString::number(shown.Number.value()), QString::fromStdString(shown.Title));
		}
		ReviewSubmissionEditor->set_context(context);
		// [[BPR-1590]] v003. How a row opens, on the sheet and the page alike.
		ReviewSubmissionEditor->set_reveal_hint(true);
	}

	void workbench_window::place_review_sheet()
	{
		if (ReviewSheet == nullptr || CentralSplitter == nullptr) {
			return;
		}
		ReviewSheet->place(CentralSplitter->geometry());
	}

	void workbench_window::move_review_composer(const bool aToSheet)
	{
		if (ReviewSheet == nullptr || ComposeReviewPage == nullptr || ReviewSubmissionEditor == nullptr) {
			return;
		}
		auto* const pageColumn = qobject_cast<QVBoxLayout*>(ComposeReviewPage->widget()->layout());
		auto* const from       = aToSheet ? static_cast<QScrollArea*>(ComposeReviewPage) : ReviewSheet->scroll();
		auto* const to         = aToSheet ? ReviewSheet->scroll() : static_cast<QScrollArea*>(ComposeReviewPage);
		const auto  inSheet    = ReviewSubmissionEditor->parentWidget() == ReviewSheet->scroll()->widget();
		if (inSheet == aToSheet) {
			return;
		}
		const auto scroll = from->verticalScrollBar()->value();
		if (aToSheet) {
			pageColumn->removeWidget(ReviewSubmissionEditor);
			ReviewSheet->content_layout()->addWidget(ReviewSubmissionEditor, 1);
		} else {
			ReviewSheet->content_layout()->removeWidget(ReviewSubmissionEditor);
			pageColumn->addWidget(ReviewSubmissionEditor, 1);
		}
		ReviewSubmissionEditor->show();
		// The sheet's header names the review; the page shows the editor's own title.
		ReviewSubmissionEditor->title_label()->setVisible(!aToSheet);
		present_review_context(aToSheet);
		ReviewSheet->title()->setText(ReviewSubmissionEditor->title_label()->text());
		to->widget()->adjustSize();
		to->verticalScrollBar()->setValue(scroll);
		// The geometry settles on the next pass; keep the place the reader had.
		QTimer::singleShot(0, to, [to, scroll] { to->verticalScrollBar()->setValue(scroll); });
	}

	void workbench_window::open_review_sheet(const bool aFocus)
	{
		if (ReviewSheet == nullptr) {
			return;
		}
		if (CompositionPage != nullptr) {
			// An inline draft's page gives way; the expanded review becomes the sheet.
			close_review_composition(false);
		}
		if (!ReviewSheetOpen) {
			SheetReturnFocus = QApplication::focusWidget();
		}
		move_review_composer(true);
		ReviewSheetOpen = true;
		present_compose_toggle();
		ReviewSubmissionEditor->ensurePolished();
		ReviewSheet->content_layout()->activate();
		place_review_sheet();
		ReviewSheet->show();
		ReviewSheet->raise();
		// Its content's minimum settles once shown; the sheet follows it.
		QTimer::singleShot(0, ReviewSheet, [this] {
			if (ReviewSheetOpen) {
				place_review_sheet();
			}
		});
		if (!aFocus) {
			return;
		}
		auto* const body = ReviewSubmissionEditor->body_editor();
		if (body->isVisible() && body->isEnabled()) {
			body->setFocus(Qt::OtherFocusReason);
		} else {
			ReviewSheet->close_button()->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::close_review_sheet()
	{
		if (ReviewSheet == nullptr || !ReviewSheetOpen) {
			return;
		}
		ReviewSheetOpen = false;
		ReviewSheet->hide();
		present_compose_toggle();
		if (SheetReturnFocus && SheetReturnFocus->isVisible()) {
			SheetReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::toggle_review_composition()
	{
		// Closing takes the composer's own way out: the sheet's Close, or the
		// expanded page's, which here closes it outright rather than restoring
		// the sheet. Either only hides the editor, so its words are kept.
		if (ReviewSheetOpen) {
			close_review_sheet();
		} else if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
			close_review_composition(false);
		} else {
			open_review_composition(false);
		}
		present_compose_toggle();
	}

	void workbench_window::present_compose_toggle()
	{
		if (ComposeReviewAction == nullptr) {
			return;
		}
		const auto shown = ReviewSheetOpen || (CompositionPage != nullptr && CompositionPage == ComposeReviewPage);
		if (ComposeReviewAction->isChecked() != shown) {
			ComposeReviewAction->setChecked(shown);
		}
	}

	void workbench_window::expand_review_sheet()
	{
		if (ReviewSheet == nullptr) {
			return;
		}
		const auto caret = ReviewSubmissionEditor->body_editor()->textCursor();
		const auto focus = SheetReturnFocus;
		ReviewSheetOpen  = false;
		ReviewSheet->hide();
		move_review_composer(false);
		ExpandingReviewSheet = true;
		CompositionReturnFocus.clear();
		open_review_composition(false);
		ExpandingReviewSheet   = false;
		CompositionReturnFocus = focus;
		ReviewSubmissionEditor->body_editor()->setTextCursor(caret);
	}

	auto workbench_window::begin_new_comment(const bool aSuggestion) -> bool
	{
		update_comment_target();
		if (CommentAction == nullptr || ReviewEditor == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return false;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!CommentAction->isEnabled() || !draft.has_value()) {
			ConversationPanel->present_notice(CommentAction->toolTip());
			return false;
		}
		// A new comment is never a reply: R answers a thread (BPR-1212's
		// flagged point 3). A draft that answers one is finished first.
		if (draft->ReplyToThread.has_value()) {
			ConversationPanel->present_notice(tr("Finish or cancel the reply on this line first; a new comment starts once it is gone."));
			return false;
		}
		const auto inReview = Modality != nullptr && Modality->current() == review_modality::review && !showing_home() && CompositionPage == nullptr;
		// [[RFC-BPR-0090]]. In Review, Suggest change writes the change in the
		// code and the drawer never prefills a block (owner, Q5). Elsewhere no
		// code shows, so the full composer starts as a block holding the
		// selected lines exactly; a draft with words keeps them and gains it.
		if (aSuggestion && !inReview && !ReviewEditor->body_editor()->isReadOnly()
			&& core::classify_review_body(ReviewEditor->body_editor()->toPlainText().toStdString()) != core::review_body_compatibility::requires_line_subject) {
			if (const auto lines = draft_source_lines(); lines.has_value()) {
				const auto fence = QString::fromStdString(core::suggestion_fence_body(*lines));
				const auto words = ReviewEditor->body_editor()->toPlainText();
				ReviewEditor->body_editor()->setPlainText(words.trimmed().isEmpty() ? fence : words.trimmed() + QStringLiteral("\n\n") + fence);
				CompositionCaret = static_cast<int>(ReviewEditor->body_editor()->toPlainText().lastIndexOf(QLatin1Char{ '\n' }));
			}
		}
		if (!inReview || ReviewEditor->body_editor()->isReadOnly()) {
			// Discuss, or a draft whose kept text is in conflict: the full
			// composer, where Reload and Reapply are.
			CompositionPage = nullptr;
			open_draft_in_discuss();
			return true;
		}
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded || (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab)) {
			if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
				TrayTabs->setCurrentIndex(tray_comments_tab);
			}
			if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
				if (!Dispatch) {
					open_draft_in_discuss();
					return true;
				}
				PendingDraft = aSuggestion;
				Dispatch(core::set_workbench_drawer_intent{
					.Drawer = core::workbench_drawer::conversations,
					.State  = core::workbench_drawer_state::expanded,
				});
				if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
					return true;
				}
				PendingDraft.reset();
			}
		}
		WritingAnchor = draft->Anchor;
		present_inline_draft();
		ConversationPanel->cards()->begin_draft();
		if (auto* const editor = ConversationPanel->cards()->draft_editor(); editor != nullptr && CompositionCaret >= 0) {
			auto cursor = editor->textCursor();
			cursor.setPosition(std::min(CompositionCaret, static_cast<int>(editor->document()->characterCount() - 1)));
			editor->setTextCursor(cursor);
		}
		CompositionCaret = -1;
		apply_thread_marks();
		// [[RFC-BPR-0090]]. The draft's lines open for writing in the head pane;
		// a kept suggestion opens there again however it is opened, and the
		// keyboard stays with its words.
		if (aSuggestion) {
			static_cast<void>(begin_pane_suggestion());
		} else if (!PaneSuggestion.has_value() || PaneSuggestion->Anchor != draft->Anchor) {
			if (core::split_suggestion_body(ReviewEditor->body_editor()->toPlainText().toStdString()).has_value()) {
				static_cast<void>(begin_pane_suggestion(false));
			}
		}
		return true;
	}

	auto workbench_window::suggestion_range_reason(const core::comment_range_decision& aDecision, QString* const aWords) const -> QString
	{
		const auto refuse = [aWords](QString aFull, QString aShort) {
			if (aWords != nullptr) {
				*aWords = std::move(aShort);
			}
			return aFull;
		};
		const auto* const result = ComparisonSurface != nullptr ? ComparisonSurface->current_result().get() : nullptr;
		if (result == nullptr) {
			return refuse(tr("Wait for the comparison to load, then select a line."), tr("The comparison is loading"));
		}
		const auto first    = core::source_line_ref{ .Comparison = aDecision.Anchor.Comparison, .Side = aDecision.Anchor.Side, .Line = aDecision.StartLine.value_or(aDecision.Anchor.Line) };
		const auto decision = core::decide_suggestion_range(result->Alignment, first, aDecision.Anchor, !RightConversationEndpointCurrent);
		switch (decision.Refusal) {
		case core::suggestion_range_refusal::none:
			return { };
		case core::suggestion_range_refusal::base_side:
			return refuse(tr("Suggest change replaces lines on the right, head side. A base line takes a comment."), tr("Suggest change: head side only"));
		case core::suggestion_range_refusal::outside_changed_region:
			return refuse(tr("GitHub takes a suggestion only on lines inside a changed region. Select a line within a hunk, or add a comment here instead."),
						  tr("Suggestions need a changed region"));
		case core::suggestion_range_refusal::filtered_range:
			return refuse(tr("This comparison's right side is an earlier commit than the head, where GitHub takes a comment on one line only. Select one line to suggest a change."),
						  tr("One line on an earlier commit"));
		case core::suggestion_range_refusal::comment_range:
			break;
		}
		return refuse(tr("GitHub takes a suggestion on several lines only inside one changed region. Select lines within one hunk, or a single line."),
					  tr("Lines must be in one changed region"));
	}

	auto workbench_window::begin_pane_suggestion(const bool aFocusCode) -> bool
	{
		if (ReviewEditor == nullptr || ComparisonSurface == nullptr) {
			return false;
		}
		const auto& draft  = ReviewEditor->shown_draft();
		const auto& result = ComparisonSurface->current_result();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || ReviewEditor->body_editor()->isReadOnly() || !result
			|| draft->Anchor.Comparison != result->Alignment.Digest || draft->Anchor.Side != core::diff_side::right || !result->RightDocument.has_value()) {
			return false;
		}
		const auto first = draft->StartLine.value_or(draft->Anchor.Line);
		const auto last  = draft->Anchor.Line;
		const auto bytes = std::string{ result->RightDocument->Bytes.begin(), result->RightDocument->Bytes.end() };
		auto       all   = core::split_text_lines(bytes);
		if (first > last || last >= all.size()) {
			return false;
		}
		auto original = std::vector<core::text_line>{ all.begin() + first, all.begin() + last + 1U };
		// A kept suggestion opens with its block; any other words stay words.
		const auto body    = ReviewEditor->body_editor()->toPlainText().toStdString();
		auto       rebuilt = core::rebuild_suggestion_span(body, original, true);
		auto       state   = pane_suggestion{ .Anchor = draft->Anchor, .StartLine = draft->StartLine, .Original = original };
		auto       span    = std::optional<std::vector<core::text_line>>{ };
		if (rebuilt.Refusal == core::suggestion_rebuild_refusal::none) {
			state.Prose = QString::fromStdString(rebuilt.Prose);
			span        = std::move(rebuilt.Lines);
		} else {
			state.Prose    = QString::fromStdString(body);
			state.OwnBlock = core::classify_review_body(body) == core::review_body_compatibility::requires_line_subject;
		}
		const auto words  = first == last ? tr("line %1").arg(last + 1U) : tr("lines %1–%2").arg(first + 1U).arg(last + 1U);
		const auto only   = first == last ? tr("Only line %1 is in this suggestion. Esc ends the edit.").arg(last + 1U)
										  : tr("Only lines %1–%2 are in this suggestion. Esc ends the edit.").arg(first + 1U).arg(last + 1U);
		auto       accent = ConversationPanel != nullptr && ConversationPanel->cards() != nullptr ? ConversationPanel->cards()->accent() : palette().color(QPalette::Highlight);
		auto       wash   = accent;
		wash.setAlphaF(0.14F);
		// The link is the drawer's from now on; an edit opened earlier ends first.
		PaneSuggestion.reset();
		auto* const editor = ComparisonSurface->begin_head_edit(diff_workbench::head_edit_request{
			.First     = first,
			.Last      = last,
			.Span      = std::move(span),
			.EmptyNote = tr("Deletes %1").arg(words),
			.Wash      = wash,
			.Edge      = accent,
			.Flash     = accent.darker(135),
			.Changed   = [this] { compose_pane_suggestion(); },
			.Ended =
				[this] {
					if (PaneSuggestion.has_value()) {
						PaneSuggestion->Open = false;
					}
					// The drawer shows the kept change once the edit has gone.
					QTimer::singleShot(0, this, [this] { present_inline_draft(); });
				},
		});
		if (editor == nullptr) {
			statusBar()->showMessage(tr("This file is too large to edit in place. Write the change in the comment as a suggestion block instead."), 10000);
			return false;
		}
		state.Open      = true;
		PaneSuggestion  = std::move(state);
		editor->Refused = [this, only] { statusBar()->showMessage(only, 6000); };
		editor->Escaped = [this] {
			end_pane_suggestion();
			return true;
		};
		statusBar()->showMessage(tr("Suggesting a change to %1. Type it in the code; Esc ends the edit.").arg(words), 6000);
		present_inline_draft();
		if (aFocusCode) {
			editor->setFocus(Qt::OtherFocusReason);
		} else if (auto* const field = ConversationPanel != nullptr && ConversationPanel->cards() != nullptr ? ConversationPanel->cards()->draft_editor() : nullptr; field != nullptr) {
			field->setFocus(Qt::OtherFocusReason);
		}
		// [[BPR-1733]]. Opened on a carried suggestion's lines: it takes its body.
		if (Carrying.has_value() && !Carrying->Written && PaneSuggestion->Anchor == Carrying->Anchor) {
			write_carried_suggestion();
		}
		return true;
	}

	void workbench_window::end_pane_suggestion()
	{
		if (PaneSuggestion.has_value() && PaneSuggestion->Open && ComparisonSurface != nullptr) {
			ComparisonSurface->end_head_edit();
		}
		if (PaneSuggestion.has_value()) {
			PaneSuggestion->Open = false;
		}
	}

	void workbench_window::release_pane_suggestion()
	{
		end_pane_suggestion();
		PaneSuggestion.reset();
	}

	auto workbench_window::pane_suggestion_plan() const -> std::optional<core::pull_request_suggestion_plan>
	{
		if (!PaneSuggestion.has_value() || ReviewEditor == nullptr) {
			return std::nullopt;
		}
		const auto prose = PaneSuggestion->Prose.toStdString();
		const auto first = PaneSuggestion->StartLine.value_or(PaneSuggestion->Anchor.Line);
		auto       input = core::pull_request_suggestion_input{ .First = first, .Last = PaneSuggestion->Anchor.Line, .Original = PaneSuggestion->Original, .Prose = prose };
		// On a commit-filtered diff, where the host's core puts a comment on
		// the anchor line (RFC-BPR-0089 Q1); not yet known is not placed.
		if (FilteredComments) {
			input.Filtered = FilteredPlacement.has_value() && FilteredPlacement->Selection == PaneSuggestion->Anchor
				? FilteredPlacement->Target
				: core::filtered_comment_target{ .Placement = core::filtered_comment_placement::refused, .Refusal = core::filtered_comment_refusal::lines_pending };
		} else if (WholeDiffPlacement.has_value() && WholeDiffPlacement->Selection == PaneSuggestion->Anchor) {
			// [[BPR-1735]]. On the Whole PR diff a line GitHub resolves nowhere
			// cannot take a suggestion either.
			input.Filtered = WholeDiffPlacement->Target;
		}
		auto* const editor = PaneSuggestion->Open && ComparisonSurface != nullptr ? ComparisonSurface->head_editor() : nullptr;
		if (editor != nullptr) {
			const auto range = editor->model().editable_range();
			if (!range.has_value()) {
				return std::nullopt;
			}
			const auto& lines = editor->model().state().Lines;
			input.Edited      = std::span{ lines }.subspan(static_cast<std::size_t>(range->first), static_cast<std::size_t>(range->second - range->first + 1U));
			return core::plan_pull_request_suggestion(input);
		}
		// Closed: the change the draft keeps, from its body's last block, so
		// words holding a block of their own never lose the code's.
		const auto split = core::split_suggestion_body(ReviewEditor->body_editor()->toPlainText().toStdString());
		const auto kept  = split.has_value() ? core::rebuild_suggestion_span(core::compose_suggestion_body({ }, split->Block), PaneSuggestion->Original, true) : core::suggestion_rebuild{ };
		input.Edited     = split.has_value() && kept.Refusal == core::suggestion_rebuild_refusal::none ? std::span<const core::text_line>{ kept.Lines }
																									   : std::span<const core::text_line>{ PaneSuggestion->Original };
		return core::plan_pull_request_suggestion(input);
	}

	auto workbench_window::suggestion_hold(const core::pull_request_suggestion_plan& aPlan) const -> std::pair<QString, QString>
	{
		if (!aPlan.Changed || aPlan.placeable() || !PaneSuggestion.has_value()) {
			return { };
		}
		const auto line   = PaneSuggestion->Anchor.Line + 1U;
		const auto commit = FilteredPlacement.has_value() ? FilteredPlacement->Commit : QString{ };
		switch (aPlan.Reason) {
		case core::suggestion_placement_reason::changed_after_commit:
			return { tr("Line %1 changed after commit %2. GitHub shows a comment here as outdated, so it cannot apply a suggestion.").arg(line).arg(commit),
					 tr("Convert to a comment on %1").arg(commit) };
		case core::suggestion_placement_reason::unresolvable_line:
			return { tr("GitHub can't attach a comment to line %1 on any commit, so it cannot apply a suggestion here.").arg(line), tr("Convert to file comment") };
		case core::suggestion_placement_reason::not_placed: {
			const auto [notice, blocks] = filtered_comment_notice();
			static_cast<void>(blocks);
			return { notice.isEmpty() ? tr("Working out where GitHub puts a comment on this line…") : notice, QString{ } };
		}
		case core::suggestion_placement_reason::none:
			break;
		}
		return { };
	}

	void workbench_window::convert_pane_suggestion()
	{
		const auto plan = pane_suggestion_plan();
		if (!plan.has_value() || !plan->Changed || ReviewEditor == nullptr || ReviewEditor->body_editor()->isReadOnly()) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->Anchor != PaneSuggestion->Anchor) {
			return;
		}
		auto original = std::vector<std::string>{ };
		for (const auto& line : PaneSuggestion->Original) {
			original.push_back(line.Content);
		}
		const auto first = PaneSuggestion->StartLine.value_or(PaneSuggestion->Anchor.Line);
		const auto last  = PaneSuggestion->Anchor.Line;
		const auto text  = core::convert_suggestion_to_text(PaneSuggestion->Prose.toStdString(), first, last, original, plan->Block);
		const auto id    = draft->Id;
		// The draft is text from now on: the link to the code ends with it.
		release_pane_suggestion();
		HeldSuggestions.erase(id);
		ReviewEditor->body_editor()->setPlainText(QString::fromStdString(text));
		present_held_suggestions();
		statusBar()->showMessage(first == last ? tr("Your words and the change are now a comment on line %1.").arg(last + 1U)
											   : tr("Your words and the change are now a comment on lines %1–%2.").arg(first + 1U).arg(last + 1U),
								 6000);
		present_inline_draft();
	}

	void workbench_window::present_held_suggestions()
	{
		if (ReviewSubmissionEditor == nullptr) {
			return;
		}
		// The reading amber, deep enough to read as text on either variant.
		auto ink = ConversationPanel != nullptr && ConversationPanel->cards() != nullptr ? ConversationPanel->cards()->marker() : QColor{ };
		if (ink.isValid()) {
			ink = palette().color(QPalette::Window).lightness() < 128 ? ink.lighter(125) : ink.darker(155);
		}
		auto held = std::map<core::review_draft_id, review_submission_editor::held_comment>{ };
		for (const auto& [id, suggestion] : HeldSuggestions) {
			held.emplace(id, review_submission_editor::held_comment{ .Reason = suggestion.Reason, .Convert = suggestion.Convert, .Ink = ink });
		}
		// [[BPR-1735]]. A comment GitHub cannot place in a review is held the
		// same way, its action Publish as file comment.
		for (const auto& [id, comment] : UnplaceableComments) {
			const auto [reason, action] = unplaceable_hold(comment);
			held.insert_or_assign(id, review_submission_editor::held_comment{ .Reason = reason, .Convert = action, .Ink = ink, .Kind = review_submission_editor::held_kind::unplaceable_line });
		}
		ReviewSubmissionEditor->set_held_comments(std::move(held));
	}

	void workbench_window::compose_pane_suggestion()
	{
		const auto plan = pane_suggestion_plan();
		if (!plan.has_value() || ReviewEditor == nullptr || ReviewEditor->body_editor()->isReadOnly()) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->Anchor != PaneSuggestion->Anchor) {
			return;
		}
		const auto body = QString::fromStdString(plan->Body);
		if (ReviewEditor->body_editor()->toPlainText() != body) {
			ReviewEditor->body_editor()->setPlainText(body);
		}
		present_inline_draft();
	}

	auto workbench_window::decide_selected_lines(QString& aReason, QString* const aShort) const -> std::optional<core::comment_range_decision>
	{
		const auto refuse = [&aReason, aShort](QString aFull, QString aWords) {
			aReason = std::move(aFull);
			if (aShort != nullptr) {
				*aShort = std::move(aWords);
			}
			return std::optional<core::comment_range_decision>{ };
		};
		// [[BPR-1212]] slice 1. The selection's two ends, as the mouse and the
		// keyboard line cursor share them, decided by the core.
		const auto range = ComparisonSurface != nullptr ? ComparisonSurface->line_draft_range() : std::nullopt;
		if (!range) {
			return refuse(tr("Select a line first: click it, or move to it with Up and Down while the code has focus."), tr("Select a line to comment on"));
		}
		if (comparing_off_current_head()) {
			return refuse(tr("Select the pull request's current head; this tab compares an earlier update."), tr("Comments need the current head"));
		}
		const auto& result = ComparisonSurface->current_result();
		if (!result) {
			return refuse(tr("Wait for the comparison to load, then select a line."), tr("The comparison is loading"));
		}
		// A base line takes a comment once its numbering is known to be
		// GitHub's (`LEFT` lines count the merge base's version of the file).
		const auto decision = core::decide_comment_range(result->Alignment, range->first, range->second, LeftConversationEndpointCurrent);
		switch (decision.Refusal) {
		case core::comment_range_refusal::none:
			return decision;
		case core::comment_range_refusal::base_side:
			return refuse(tr("GitHub counts base lines in the merge base's version of this file, and this comparison's base is not known to match it. Comment on a right-side line, or reply to an existing thread."),
						  tr("This file's base is not GitHub's"));
		case core::comment_range_refusal::mixed_side:
			return refuse(tr("Select lines on one side to comment on them together."), tr("Select lines on one side"));
		case core::comment_range_refusal::outside_hunk:
			if (range->first == range->second) {
				// One base line: GitHub takes a `LEFT` comment only in the diff.
				return refuse(tr("GitHub takes a comment on a base line only inside a changed region. Select a base line within a hunk, or a right-side line."),
							  tr("Base lines need a changed region"));
			}
			[[fallthrough]];
		case core::comment_range_refusal::crosses_hunk:
			return refuse(tr("GitHub takes a comment on several lines only inside one changed region. Select lines within one hunk, or a single line."),
						  tr("Lines must be in one changed region"));
		case core::comment_range_refusal::mixed_comparison:
		case core::comment_range_refusal::outside_document:
			break;
		}
		return refuse(tr("Select a line of this comparison."), tr("Select a line of this comparison"));
	}

	auto workbench_window::comment_on_selected_lines(const bool aSuggestion) -> bool
	{
		// [[BPR-1212]]. One path for Return, Shift+Return, the right-click
		// items and the gutter's "+": the core's decision, then the drawer
		// draft `begin_new_comment` opens.
		PendingLineComment.reset();
		auto       reason   = QString{ };
		const auto decision = decide_selected_lines(reason);
		if (!decision) {
			statusBar()->showMessage(reason, 10000);
			return false;
		}
		// A suggestion replaces head lines; a base line takes a comment only.
		if (aSuggestion && decision->Anchor.Side != core::diff_side::right) {
			statusBar()->showMessage(tr("Suggest change replaces lines on the right, head side. A base line takes a comment: Add comment, or Return."), 10000);
			return false;
		}
		// [[RFC-BPR-0090]]. And the lines must be ones GitHub can suggest on.
		if (aSuggestion) {
			if (const auto why = suggestion_range_reason(*decision); !why.isEmpty()) {
				statusBar()->showMessage(why, 10000);
				return false;
			}
		}
		// The draft composes on the range's last line, which GitHub anchors
		// it to.
		const auto selected = ComparisonSurface->selection().has_value() && ComparisonSurface->selection()->PreferredLine == decision->Anchor;
		if (!selected && !ComparisonSurface->select_conversation_line(decision->Anchor.Side, decision->Anchor.Line)) {
			statusBar()->showMessage(tr("Select a line of this comparison."), 10000);
			return false;
		}
		statusBar()->clearMessage();
		PendingLineComment = pending_line_comment{ .Anchor = decision->Anchor, .StartLine = decision->StartLine, .Suggestion = aSuggestion };
		if (resume_line_comment()) {
			return true;
		}
		if (!PendingLineComment.has_value()) {
			return false;
		}
		// No draft is on the line yet. The host answers a line selected just
		// now, which a right-click on a new line is ([[BPR-1212]] v002: the
		// owner had to left-click first). It opens when it arrives while the
		// line stays selected; if none comes within the wait, the status line
		// says why.
		const auto base  = decision->Anchor.Side == core::diff_side::left ? tr("base ") : QString{ };
		const auto lines = decision->StartLine.has_value() ? tr("%1lines %2–%3").arg(base).arg(*decision->StartLine + 1U).arg(decision->Anchor.Line + 1U)
														   : tr("%1line %2").arg(base).arg(decision->Anchor.Line + 1U);
		statusBar()->showMessage(tr("Opening a draft on %1…").arg(lines), line_comment_wait_ms);
		QTimer::singleShot(line_comment_wait_ms, this, [this, anchor = decision->Anchor] { report_line_comment_wait(anchor); });
		return true;
	}

	void workbench_window::report_line_comment_wait(const core::source_line_ref aAnchor)
	{
		if (!PendingLineComment.has_value() || PendingLineComment->Anchor != aAnchor) {
			return;
		}
		update_comment_target();
		statusBar()->showMessage(CommentAction != nullptr && !CommentAction->isEnabled()
									 ? CommentAction->toolTip()
									 : tr("No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment."),
								 10000);
	}

	auto workbench_window::resume_line_comment() -> bool
	{
		if (!PendingLineComment.has_value()) {
			return false;
		}
		// Another line selected since: the waiting comment is not wanted.
		const auto& selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (!selection || selection->PreferredLine != PendingLineComment->Anchor) {
			PendingLineComment.reset();
			return false;
		}
		const auto& draft = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt;
		if (!draft || draft->Anchor != PendingLineComment->Anchor) {
			return false;
		}
		const auto pending = *std::exchange(PendingLineComment, std::nullopt);
		// The range goes on the draft in one edit through the editor's edit
		// path, before any suggestion block is written from it. A reply is
		// left to begin_new_comment, which says to finish it first.
		if (!draft->ReplyToThread.has_value() && draft->StartLine != pending.StartLine && !ReviewEditor->change_range(pending.StartLine) && pending.StartLine.has_value()) {
			statusBar()->showMessage(tr("This draft cannot take several lines now. Select one line, or settle the draft's retained text first."), 10000);
			return false;
		}
		// The wait's message goes first, so what opening says stays.
		statusBar()->clearMessage();
		return begin_new_comment(pending.Suggestion);
	}

	auto workbench_window::add_comment_action() const noexcept -> QAction*
	{
		return AddCommentAction;
	}

	auto workbench_window::suggest_change_action() const noexcept -> QAction*
	{
		return SuggestChangeAction;
	}

	auto workbench_window::code_context_menu(QPlainTextEdit* const aPane, const QPoint aPosition) -> QMenu*
	{
		// [[BPR-1212]]. A right-click outside the selection selects its line
		// first, as a click would; the menu then names it. Inside the selected
		// lines it keeps them all. [[BPR-1724]]. The workbench reads the
		// line from the pane, so one pane's rows give the line they show.
		if (ComparisonSurface != nullptr && aPane != nullptr) {
			static_cast<void>(ComparisonSurface->select_line_for_menu(aPane, aPosition));
		}
		update_comment_target();
		auto* const menu = new QMenu{ aPane != nullptr ? static_cast<QWidget*>(aPane) : this };
		menu->setObjectName(QStringLiteral("codeContextMenu"));
		menu->setToolTipsVisible(true);
		const auto& selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (selection.has_value()) {
			// [[BPR-1212]] slice 1. "Lines a–b" when the selection covers several.
			const auto range = ComparisonSurface->line_draft_range();
			const auto first = range.has_value() ? std::min(range->first.Line, range->second.Line) : selection->PreferredLine.Line;
			const auto last  = range.has_value() ? std::max(range->first.Line, range->second.Line) : selection->PreferredLine.Line;
			const auto base  = selection->PreferredLine.Side == core::diff_side::left;
			// [[BPR-1724]]. One pane's rows can start on one side and end
			// on the other; the caption names each end's own side.
			const auto  mixed   = range.has_value() && range->first.Side != range->second.Side;
			const auto  heading = mixed ? (range->first.Side == core::diff_side::left ? tr("Base line %1 to line %2") : tr("Line %1 to base line %2"))
											  .arg(range->first.Line + 1U)
											  .arg(range->second.Line + 1U)
				: first == last         ? (base ? tr("Base line %1") : tr("Line %1")).arg(first + 1U)
										: (base ? tr("Base lines %1–%2") : tr("Lines %1–%2")).arg(first + 1U).arg(last + 1U);
			auto* const caption = menu->addAction(heading);
			caption->setObjectName(QStringLiteral("codeContextMenuCaption"));
			caption->setEnabled(false);
			// [[BPR-1335]]. The line's thread, reachable without its marker.
			const auto side = selection->PreferredLine.Side;
			const auto line = selection->PreferredLine.Line;
			if (thread_on_line(side, line).has_value()) {
				auto* const open = menu->addAction(tr("Open thread"));
				open->setObjectName(QStringLiteral("codeContextMenuOpenThread"));
				open->setToolTip(tr("Open this line's thread in the drawer"));
				connect(open, &QAction::triggered, this, [this, side, line] { go_to_thread_at(side, line); });
				// [[BPR-1581]]. The same thread with the rest of the conversation.
				auto* const discuss = menu->addAction(tr("Show in Discuss"));
				discuss->setObjectName(QStringLiteral("codeContextMenuShowInDiscuss"));
				discuss->setToolTip(tr("Show this line's thread in Discuss"));
				if (ShowInDiscussAction != nullptr) {
					discuss->setShortcut(ShowInDiscussAction->shortcut());
					discuss->setShortcutContext(Qt::WidgetShortcut);
					discuss->setShortcutVisibleInContextMenu(true);
				}
				connect(discuss, &QAction::triggered, this, [this, side, line] {
					if (const auto identity = thread_card_on_line(side, line); identity.has_value()) {
						static_cast<void>(move_thread(*identity, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
					}
				});
				// [[BPR-1586]]. And its row in Threads.
				auto* const threads = menu->addAction(tr("Show in Threads"));
				threads->setObjectName(QStringLiteral("codeContextMenuShowInThreads"));
				threads->setToolTip(tr("Show this line's thread in Threads"));
				if (ShowInThreadsAction != nullptr) {
					threads->setShortcut(ShowInThreadsAction->shortcut());
					threads->setShortcutContext(Qt::WidgetShortcut);
					threads->setShortcutVisibleInContextMenu(true);
				}
				connect(threads, &QAction::triggered, this, [this, side, line] {
					if (const auto identity = thread_card_on_line(side, line); identity.has_value()) {
						static_cast<void>(move_thread(*identity, core::workbench_thread_origin::review, core::workbench_thread_destination::threads));
					}
				});
			}
		}
		// Add comment and Suggest change start a new comment, never a reply.
		// [[BPR-1212]] v002. A line selected by this right-click has its draft
		// on the way, so its absence greys nothing: the item waits for it. A
		// reason the line cannot take a comment shows under the items.
		const auto reply = ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && ReviewEditor->shown_draft()->ReplyToThread.has_value()
			&& selection.has_value() && ReviewEditor->shown_draft()->Anchor == selection->PreferredLine;
		auto words  = QString{ };
		auto reason = comment_target_reason(false, &words);
		if (reason.isEmpty() && reply) {
			reason = tr("Finish or cancel the reply on this line first.");
			words  = tr("Finish the reply on this line first");
		}
		// [[BPR-1212]] slice 1. Lines the core refuses together say why.
		auto decision = std::optional<core::comment_range_decision>{ };
		if (reason.isEmpty()) {
			decision = decide_selected_lines(reason, &words);
		}
		// A suggestion replaces head lines; a base line takes a comment only.
		auto suggestReason = reason;
		auto suggestWords  = words;
		if (suggestReason.isEmpty() && decision.has_value()) {
			suggestReason = suggestion_range_reason(*decision, &suggestWords);
		}
		const auto several = decision.has_value() && decision->StartLine.has_value();
		for (auto* const action : { AddCommentAction, SuggestChangeAction }) {
			const auto& why = action == AddCommentAction ? reason : suggestReason;
			action->setEnabled(why.isEmpty());
			action->setToolTip(!why.isEmpty()                   ? why
								   : action == AddCommentAction ? (several ? tr("Write a comment on these lines, in the drawer beside the code") : tr("Write a comment on this line, in the drawer beside the code"))
								   : several                    ? tr("Write new text for these lines in the code: a suggestion GitHub can apply")
																: tr("Write new text for this line in the code: a suggestion GitHub can apply"));
			action->setStatusTip(action->toolTip());
			// [[BPR-1721]]. An item that takes several lines names them,
			// as the core decided them, so a range is seen before it is chosen.
			auto text = action->text();
			if (several && why.isEmpty()) {
				const auto first = *decision->StartLine + 1U;
				const auto last  = decision->Anchor.Line + 1U;
				text             = decision->Anchor.Side == core::diff_side::left ? tr("%1 on base lines %2–%3").arg(text).arg(first).arg(last)
																				  : tr("%1 on lines %2–%3").arg(text).arg(first).arg(last);
			}
			// The menu's item shows the code's key; it is the menu's own, so
			// no window shortcut ever claims Return.
			auto* const item = new QAction{ action->icon(), text, menu };
			item->setObjectName(action->objectName());
			item->setShortcuts(shortcut_keys(action == AddCommentAction ? shortcut_command::add_comment : shortcut_command::suggest_change));
			item->setShortcutContext(Qt::WidgetShortcut);
			item->setEnabled(action->isEnabled());
			item->setToolTip(action->toolTip());
			item->setStatusTip(action->statusTip());
			connect(item, &QAction::triggered, action, &QAction::trigger);
			menu->addAction(item);
		}
		// [[BPR-1594]]. Ask the agent about these lines, in the Agent tab.
		if (AskAgentAction != nullptr) {
			auto* const ask = new QAction{ AskAgentAction->text(), menu };
			ask->setObjectName(AskAgentAction->objectName());
			ask->setShortcuts(AskAgentAction->shortcuts());
			ask->setShortcutContext(Qt::WidgetShortcut);
			ask->setToolTip(AskAgentAction->toolTip());
			connect(ask, &QAction::triggered, AskAgentAction, &QAction::trigger);
			menu->addAction(ask);
		}
		// The reason in words under the items, not only in a tooltip.
		if (const auto& shown = !words.isEmpty() ? words : suggestWords; !shown.isEmpty()) {
			auto* const why = menu->addAction(shown);
			why->setObjectName(QStringLiteral("codeContextMenuReason"));
			why->setEnabled(false);
			why->setToolTip(!reason.isEmpty() ? reason : suggestReason);
		}
		if (aPane != nullptr) {
			auto* const standard = aPane->createStandardContextMenu();
			standard->setParent(menu, standard->windowFlags());
			standard->hide();
			menu->addSeparator();
			for (auto* const action : standard->actions()) {
				menu->addAction(action);
			}
		}
		menu->addSeparator();
		menu->addAction(AddTourStopAction);
		return menu;
	}

	void workbench_window::resume_pending_draft()
	{
		if (!PendingDraft.has_value() || Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			return;
		}
		const auto suggestion = *std::exchange(PendingDraft, std::nullopt);
		// The block was already written when the drawer was asked for.
		static_cast<void>(suggestion);
		static_cast<void>(begin_new_comment(false));
	}

	auto workbench_window::reopen_draft_in_drawer() -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr || Modality == nullptr) {
			return false;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || Modality->current() != review_modality::review || showing_home()
			|| CompositionPage != nullptr || Drawers[1].Panel->state() != core::workbench_drawer_state::expanded
			|| (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab)) {
			return false;
		}
		WritingAnchor = draft->Anchor;
		present_inline_draft();
		ConversationPanel->cards()->begin_draft();
		apply_thread_marks();
		return ConversationPanel->cards()->writing_draft();
	}

	void workbench_window::leave_drawer_draft(const bool aDiscard)
	{
		// [[BPR-1212]]. Esc keeps a written draft at its line and drops an
		// empty one; Discard drops it through the editor's own Discard.
		// [[RFC-BPR-0090]]. The head pane's edit ends with the drawer draft.
		release_pane_suggestion();
		if (ReviewEditor != nullptr && ReviewEditor->discard_action()->isEnabled()
			&& (aDiscard || ReviewEditor->body_editor()->toPlainText().trimmed().isEmpty())
			&& !ReviewEditor->body_editor()->toPlainText().isEmpty()) {
			ReviewEditor->discard_action()->trigger();
		}
		const auto side = WritingAnchor.has_value() ? WritingAnchor->Side : core::diff_side::right;
		WritingAnchor.reset();
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->end_draft();
		}
		refresh_cards();
		// Focus goes back to the code, on the same line, in its own pane.
		if (auto* const code = ComparisonSurface == nullptr ? nullptr
				: side == core::diff_side::left             ? ComparisonSurface->left_pane()
															: ComparisonSurface->right_pane();
			code != nullptr) {
			code->setFocus(Qt::OtherFocusReason);
		}
	}

	auto workbench_window::draft_source_lines() const -> std::optional<std::vector<std::string>>
	{
		if (ReviewEditor == nullptr || !ReviewEditor->shown_draft().has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto& anchor = ReviewEditor->shown_draft()->Anchor;
		const auto& result = *ComparisonSurface->current_result();
		if (anchor.Comparison != result.Alignment.Digest) {
			return std::nullopt;
		}
		const auto& document = anchor.Side == core::diff_side::right ? result.RightDocument : result.LeftDocument;
		if (!document.has_value()) {
			return std::nullopt;
		}
		// [[BPR-1212]] slice 1. A draft on several lines suggests a change to
		// all of them, from its first line to its anchor.
		return core::suggestion_source_lines(*document, ReviewEditor->shown_draft()->StartLine.value_or(anchor.Line), anchor.Line);
	}

	void workbench_window::write_inline_draft(const QString& aText)
	{
		if (ReviewEditor == nullptr) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || ReviewEditor->body_editor()->isReadOnly()
			|| (WritingAnchor.has_value() && draft->Anchor != *WritingAnchor)) {
			return;
		}
		// [[RFC-BPR-0090]]. While the head pane holds the block, the field holds
		// the words, and the body is the core's composition of the two.
		if (PaneSuggestion.has_value() && PaneSuggestion->Anchor == draft->Anchor) {
			if (PaneSuggestion->Prose != aText) {
				PaneSuggestion->Prose = aText;
				compose_pane_suggestion();
			}
			return;
		}
		if (ReviewEditor->body_editor()->toPlainText() != aText) {
			ReviewEditor->body_editor()->setPlainText(aText);
		}
	}
}
