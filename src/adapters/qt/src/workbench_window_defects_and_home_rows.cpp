#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QApplication>
#include <QChar>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFlags>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethods>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMainWindow>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QTabBar>
#include <QToolButton>
#include <QTreeView>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		// [[BPR-0733]]. One recently viewed row. The pull request is named by
		// its locator and, where a snapshot was seen, its title; a kept
		// reading is said in words, so nothing here depends on colour. The
		// host and the instant it was last viewed are the row's tooltip and
		// its accessible description, which leaves the row itself one
		// readable line at every scale. The instant is the host's own
		// reading, not a clock this window consulted.
		[[nodiscard]] auto recent_review_item(
			const application::recent_review_entry& aEntry) -> QListWidgetItem*
		{
			const auto identity = QStringLiteral("%1/%2#%3")
									  .arg(QString::fromStdString(aEntry.Destination.Repository.Owner),
										   QString::fromStdString(aEntry.Destination.Repository.Name))
									  .arg(aEntry.Destination.Number.value());
			auto       text     = identity;
			if (!aEntry.Title.empty()) {
				text += QStringLiteral(" — ") + QString::fromStdString(aEntry.Title);
			}
			if (aEntry.Resumable) {
				text += QStringLiteral(" · saved reading");
			}
			auto       detail = QString::fromStdString(aEntry.Destination.Hostname);
			const auto viewed = aEntry.ViewedAt != 0
				? QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(aEntry.ViewedAt).toLocalTime(), QLocale::ShortFormat)
				: QString{ };
			if (!viewed.isEmpty()) {
				detail += QStringLiteral(" · viewed ") + viewed;
			}
			auto* const item = new QListWidgetItem{ text };
			item->setData(home_row_role::draft, aEntry.Draft.value_or(false));
			item->setData(Qt::AccessibleTextRole, aEntry.Draft.value_or(false) ? QStringLiteral("Draft pull request; %1").arg(text) : text);
			item->setToolTip(detail);
			// [[BPR-1843]]. The review this window has open is Home's Active
			// review card; its Recently viewed row carries no Reviewing tag
			// (the owner found it confusing), so it is an ordinary row.
			item->setData(Qt::AccessibleDescriptionRole, detail);
			// [[BPR-0776]]. The queue row's anatomy. A recent row has no state, so
			// no reason word and no colour; its time is the host's reading of
			// when it was viewed. [[BPR-1843]]: its one chip is the queue row's
			// Open on GitHub; Resume stays in the row's menu and on Shift and
			// Return, since the row itself opens the pull request.
			item->setData(home_row_role::identity, identity);
			item->setData(home_row_role::title, QString::fromStdString(aEntry.Title));
			item->setData(home_row_role::why, aEntry.Resumable ? QStringLiteral("My reading is saved.") : QStringLiteral("Viewed."));
			item->setData(home_row_role::state, -1);
			item->setData(home_row_role::time, viewed);
			item->setData(home_row_role::icon_action, QStringLiteral("Open on GitHub"));
			return item;
		}

		/// [[BPR-1575]]. "2 new comments and 1 new push", or empty for nothing.
		[[nodiscard]] auto refresh_change_words(const core::refresh_changes& aChanges) -> QString
		{
			auto parts = QStringList{ };
			if (aChanges.NewPullRequests != 0U) {
				parts.push_back(counted(aChanges.NewPullRequests, QStringLiteral("1 new pull request"), QStringLiteral("%1 new pull requests")));
			}
			if (aChanges.NewComments != 0U) {
				parts.push_back(counted(aChanges.NewComments, QStringLiteral("1 new comment"), QStringLiteral("%1 new comments")));
			}
			if (aChanges.NewReviews != 0U) {
				parts.push_back(counted(aChanges.NewReviews, QStringLiteral("1 new review"), QStringLiteral("%1 new reviews")));
			}
			if (aChanges.NewPush) {
				parts.push_back(aChanges.NewCommits == 0U ? QStringLiteral("1 new push")
														  : QStringLiteral("1 new push (%1)").arg(counted(aChanges.NewCommits, QStringLiteral("1 commit"), QStringLiteral("%1 commits"))));
			}
			if (aChanges.Status.has_value()) {
				switch (*aChanges.Status) {
				case core::refresh_status::merged: parts.push_back(QStringLiteral("now merged")); break;
				case core::refresh_status::closed: parts.push_back(QStringLiteral("now closed")); break;
				case core::refresh_status::draft:  parts.push_back(QStringLiteral("now a draft")); break;
				case core::refresh_status::open:   parts.push_back(QStringLiteral("now open for review")); break;
				}
			}
			if (parts.size() < 2) {
				return parts.join(QString{ });
			}
			const auto last = parts.takeLast();
			return parts.join(QStringLiteral(", ")) + QStringLiteral(" and ") + last;
		}

		[[nodiscard]] auto refresh_failure_words(const core::refresh_failure aFailure) -> QString
		{
			switch (aFailure) {
			case core::refresh_failure::offline:      return QStringLiteral("GitHub could not be reached");
			case core::refresh_failure::rate_limited: return QStringLiteral("GitHub's rate limit was reached");
			case core::refresh_failure::signed_out:   return QStringLiteral("the account needs to sign in again");
			case core::refresh_failure::other:        return QStringLiteral("GitHub's answer could not be used");
			}
			return { };
		}
	}

	auto workbench_window::defect_card() const noexcept -> defect_card_view*
	{
		return DefectCard;
	}

	auto workbench_window::classify_defect_action() const noexcept -> QAction*
	{
		return ClassifyDefect;
	}

	auto workbench_window::previous_defect_action() const noexcept -> QAction*
	{
		return PreviousDefect;
	}

	auto workbench_window::next_defect_action() const noexcept -> QAction*
	{
		return NextDefect;
	}

	auto workbench_window::defect_count_label() const noexcept -> QLabel*
	{
		return DefectCountLabel;
	}

	auto workbench_window::shown_defect_index() const noexcept
		-> std::optional<std::size_t>
	{
		return ShownDefectIndex;
	}

	auto workbench_window::defect_count() const noexcept -> std::size_t
	{
		return Defects.size();
	}

	auto workbench_window::queue_landing() const noexcept -> review_landing_model*
	{
		return QueueLanding;
	}

	auto workbench_window::queue_filter_model() const noexcept
		-> review_landing_filter_model*
	{
		return QueueFilterModel;
	}

	auto workbench_window::home_list() const noexcept -> qt::home_list*
	{
		return HomeList;
	}

	auto workbench_window::selected_queue_item() const -> std::optional<core::queue_item>
	{
		if (HomeList == nullptr) {
			return std::nullopt;
		}
		return HomeList->current_queue_item();
	}

	auto workbench_window::home_open_bar() const noexcept -> QLineEdit*
	{
		return HomeOpenBar;
	}

	void workbench_window::open_pull_request_item(const core::queue_item& aItem)
	{
		if (!GithubDispatch) {
			return;
		}
		const auto repository = QString::fromStdString(aItem.Ref.Repository);
		const auto slash      = repository.indexOf(QLatin1Char{ '/' });
		if (slash <= 0 || aItem.Ref.Number <= 0) {
			return;
		}
		auto intent = core::open_pull_request_intent{
			.Repository = core::repository_locator{
				.Owner = repository.left(slash).toStdString(),
				.Name  = repository.mid(slash + 1).toStdString(),
			},
			.Number = core::pull_request_number{ static_cast<std::uint32_t>(aItem.Ref.Number) },
		};
		// [[BPR-1357]]. The pull request already being opened asks for nothing
		// again, whichever row or key asked: the core says it is in flight.
		if (HomeOpen.has_value() && core::home_open_in_flight(HomeOpen->State, intent.Repository, intent.Number)) {
			return;
		}
		// [[BPR-0651]] [[BPR-0650]]. A pull request chosen from a list
		// arrives on Overview.
		OpenOnOverview = true;
		GithubDispatch(std::move(intent));
	}

	auto workbench_window::home_open_action() const noexcept -> QAction*
	{
		return HomeOpenAction;
	}

	auto workbench_window::home_open_notice() const noexcept -> QLabel*
	{
		return HomeOpenNotice;
	}

	auto workbench_window::home_open_details_button() const noexcept -> QPushButton*
	{
		return HomeOpenDetails;
	}

	auto workbench_window::home_refusal_card() const noexcept -> home_refusal*
	{
		return HomeRefusal;
	}

	void workbench_window::present_home_restoration_notice(const QString& aText)
	{
		HomeRestorationNotice = aText;
		update_home_open_notice();
	}

	auto workbench_window::home_recent_panel() const noexcept -> home_panel*
	{
		return HomeList != nullptr ? HomeList->recent_panel() : nullptr;
	}

	auto workbench_window::home_recent_row_action() const noexcept -> QToolButton*
	{
		return HomeList != nullptr ? HomeList->recent_page_action() : nullptr;
	}

	auto workbench_window::home_recent_list() const noexcept -> QListWidget*
	{
		return HomeRecentList;
	}

	auto workbench_window::home_recent_resume_action() const noexcept -> QAction*
	{
		return HomeRecentResumeAction;
	}

	auto workbench_window::home_recent_remove_action() const noexcept -> QAction*
	{
		return HomeRecentRemoveAction;
	}

	void workbench_window::set_recent_review_dispatch(
		std::function<void(application::recent_review_intent)> aDispatch)
	{
		RecentReviewDispatch = std::move(aDispatch);
	}

	void workbench_window::set_recent_removal_dispatch(std::function<void(application::recent_review_row)> aDispatch)
	{
		RecentRemovalDispatch = std::move(aDispatch);
		update_recent_resume_action();
	}

	void workbench_window::publish(const application::recent_reviews_projection& aProjection)
	{
		RecentReviews = aProjection;
		apply_recent_reviews();
		// The section appears and disappears with its entries, so the declared
		// chain is rebuilt around it. Focus itself is never moved: a
		// publication must not take the keyboard from wherever the reviewer
		// left it, which is the same rule [[BPR-0732]] holds Home's bar to.
		apply_focus_order();
	}

	void workbench_window::apply_recent_reviews()
	{
		if (HomeRecentList == nullptr) {
			return;
		}
		const auto previousRow = HomeRecentList->currentRow();
		{
			// The rows are replaced in one step, so nothing reads a half-filled
			// list; the row the reviewer had is kept where the new list still
			// has one. Making a row current is a selection, not a focus move.
			const QSignalBlocker blocked{ HomeRecentList };
			HomeRecentList->clear();
			for (const auto& entry : RecentReviews.Entries) {
				HomeRecentList->addItem(recent_review_item(entry));
			}
			if (!RecentReviews.Entries.empty()) {
				const auto rows = static_cast<int>(RecentReviews.Entries.size());
				// [[BPR-1365]]. Current, but selected only while Recently viewed is
				// the list with Home's keyboard: a publication never paints a
				// second selection beside the queue's.
				HomeRecentList->setCurrentRow(
					previousRow >= 0 && previousRow < rows ? previousRow : 0,
					HomeSelection == home_selection_list::recent ? QItemSelectionModel::ClearAndSelect : QItemSelectionModel::NoUpdate);
			}
		}
		// [[BPR-0776]]. The rows are the last panel of Home's list, which appears
		// with them. Home scrolls as one page, so the list is as tall as its
		// rows rather than five of them.
		if (HomeList != nullptr) {
			HomeList->set_recent(static_cast<int>(RecentReviews.Entries.size()),
								 home_recent_summary(RecentReviews.Entries));
		}
		update_home_surface();
		update_recent_resume_action();
		update_pending_panes();
		// [[BPR-1357]]. A row that appeared or went may carry the opening.
		if (HomeOpen.has_value()) {
			refresh_home_open();
		}
	}

	void workbench_window::update_home_surface()
	{
		// [[BPR-0776]]. The queue's surface holds its status, Home's list and
		// the queue's controls. An account shows all of them; without one only
		// Recently viewed can have anything to list.
		if (QueueSurface != nullptr) {
			QueueSurface->setVisible(HomeAccount || !RecentReviews.Entries.empty());
		}
		// Without an account the surface is shown only for Recently viewed, and
		// then the queue's status is hidden itself; otherwise it follows the
		// surface, as it always has. [[BPR-0783]]: the six typed controls are
		// Settings' now, so Home has none of its own to hide here.
		const auto controls = HomeAccount || RecentReviews.Entries.empty();
		if (QueueStatus != nullptr) {
			QueueStatus->setVisible(controls && QueueStatusNotable);
		}
		if (HomeList != nullptr) {
			HomeList->set_queue_status(QueueReady, HomeAccount);
		}
		// [[BPR-1865]]. The filter stands while Home has rows to filter.
		if (HomeFilter != nullptr) {
			const auto rows = HomeAccount || !RecentReviews.Entries.empty() || !CycleRecents.empty();
			if (!rows && !HomeFilter->query().isEmpty()) {
				HomeFilter->clear_query();
			}
			HomeFilter->setVisible(rows);
		}
	}

	void workbench_window::update_recent_resume_action()
	{
		if (HomeRecentResumeAction == nullptr || HomeRecentList == nullptr) {
			return;
		}
		const auto row    = HomeRecentList->currentRow();
		const auto listed = row >= 0 && static_cast<std::size_t>(row) < RecentReviews.Entries.size();
		HomeRecentResumeAction->setEnabled(listed && RecentReviews.Entries[static_cast<std::size_t>(row)].Resumable);
		if (HomeRecentRemoveAction != nullptr) {
			HomeRecentRemoveAction->setEnabled(listed && static_cast<bool>(RecentRemovalDispatch));
		}
	}

	void workbench_window::remove_recent_review(const int aRow)
	{
		if (!RecentRemovalDispatch || aRow < 0 || static_cast<std::size_t>(aRow) >= RecentReviews.Entries.size()) {
			return;
		}
		RecentRemovalDispatch(application::recent_review_row{ static_cast<std::size_t>(aRow) });
	}

	void workbench_window::claim_home_selection(const home_selection_list aList)
	{
		HomeSelection = aList;
		if (HomeList != nullptr) {
			HomeList->set_selection_shown(aList == home_selection_list::queue);
		}
		for (const auto& [list, view] : { std::pair{ home_selection_list::recent, static_cast<QAbstractItemView*>(HomeRecentList) },
										  std::pair{ home_selection_list::cycles, static_cast<QAbstractItemView*>(HomeCyclesList) } }) {
			if (view == nullptr) {
				continue;
			}
			if (list == aList) {
				home_show_selection(*view);
			} else {
				home_hide_selection(*view);
			}
		}
		update_open_on_github_action();
		update_home_review_actions();
	}

	void workbench_window::resume_home_cycle(const int aRow)
	{
		if (aRow >= 0 && static_cast<std::size_t>(aRow) < CycleRecents.size() && ResumeCycleDispatch) {
			ResumeCycleDispatch(CycleRecents[static_cast<std::size_t>(aRow)].Cycle.Subject);
		}
	}

	void workbench_window::dispatch_recent_review(const int aRow, const bool aResume)
	{
		if (!RecentReviewDispatch || aRow < 0
			|| static_cast<std::size_t>(aRow) >= RecentReviews.Entries.size()) {
			return;
		}
		const auto index = static_cast<std::size_t>(aRow);
		if (aResume && !RecentReviews.Entries[index].Resumable) {
			return;
		}
		// [[BPR-1357]]. A row already being opened asks for nothing again.
		if (const auto* const item = HomeRecentList->item(aRow);
			HomeOpen.has_value() && item != nullptr && HomeList != nullptr && HomeList->row_delegate() != nullptr) {
			const auto& open = HomeList->row_delegate()->open();
			if ((open.Phase == home_row_open_phase::opening || open.Phase == home_row_open_phase::waiting)
				&& item->data(home_row_role::identity).toString() == open.Identity) {
				return;
			}
		}
		// [[BPR-0733]] [[BPR-0651]]. A pull request chosen from a list arrives
		// on Overview, exactly as a queue row's does.
		OpenOnOverview = true;
		RecentReviewDispatch(application::recent_review_intent{
			.Index  = index,
			.Resume = aResume,
		});
	}

	auto workbench_window::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
#if defined(_WIN32)
		// [[BPR-1022]]. The window's own input, before any of its widgets.
		if (auto* const handle = windowHandle();
			aEvent != nullptr && handle != nullptr && aWatched == handle && read_strip_menu_key(*aEvent)) {
			return true;
		}
#endif
		if (aEvent != nullptr && aEvent->type() == QEvent::Paint && aWatched == statusBar() && home_status_echoes_bar()) {
			return true;
		}
		if (TrayHeader != nullptr && aWatched == TrayHeader && aEvent != nullptr && aEvent->type() == QEvent::Resize) {
			fit_tray_tabs();
		}
		// [[BPR-1590]] v002. The expanded review stays centred as the page resizes.
		if (ComposeReviewPage != nullptr && aWatched == ComposeReviewPage->viewport() && aEvent != nullptr && aEvent->type() == QEvent::Resize) {
			centre_review_page();
		}
		// [[BPR-1590]]. The sheet keeps to the review area's right edge.
		if (aWatched == CentralSplitter && aEvent != nullptr && (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::Move) && ReviewSheetOpen) {
			place_review_sheet();
		}
		if (aWatched == DifCornerSlot && aEvent != nullptr
			&& (aEvent->type() == QEvent::Show || aEvent->type() == QEvent::Hide
				|| aEvent->type() == QEvent::Move || aEvent->type() == QEvent::Resize)) {
			place_dif_corner();
		}
		// The new repository's field: Escape discards the draft, as Cancel does,
		// before Settings would take it to close the window.
		if (aWatched == SavedRepositoryName && aEvent != nullptr && aEvent->type() == QEvent::KeyPress
			&& static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Escape) {
			close_repository_draft();
			return true;
		}
		// Settings › Repositories: Delete (or Backspace, as macOS lists take it)
		// removes the selected repository, exactly as Remove does.
		if (aWatched == SavedRepositoryList && aEvent != nullptr && aEvent->type() == QEvent::KeyPress) {
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && RemoveSavedRepository->isEnabled()) {
				RemoveSavedRepository->trigger();
				return true;
			}
		}
		if (aWatched == ReviewBar && aEvent != nullptr
			&& (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange)) {
			apply_strip_width();
		}
		if (aWatched == HomeCyclesList && aEvent != nullptr && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			// [[BPR-1865]]. As in every Home list, "/" or a character typed
			// goes to the filter.
			if (const auto typed = key->text(); HomeFilter != nullptr && HomeFilter->isVisible() && !typed.isEmpty() && typed.front().isPrint() && !typed.front().isSpace()
				&& (key->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier) {
				HomeFilter->take_typing(typed == QStringLiteral("/") ? QString{ } : typed);
				return true;
			}
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
				&& (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier)) {
				resume_home_cycle(HomeCyclesList->currentRow());
				return true;
			}
		}
		if (aWatched == HomeRecentList && aEvent != nullptr
			&& aEvent->type() == QEvent::KeyPress) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			// Delete, or Backspace as macOS lists take it, removes the row, as
			// Settings › Repositories' list does.
			if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)
				&& key->modifiers() == Qt::NoModifier) {
				remove_recent_review(HomeRecentList->currentRow());
				return true;
			}
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				// Both Return gestures are read here rather than left to the
				// view. On macOS Return is an item view's editing key and
				// never activates a row, so a keyboard reviewer would have no
				// way into the list at all; elsewhere the view would activate
				// on the shifted key too and resume would also open. Taking
				// the key means one behaviour on every platform, and the
				// pointer's own activation still arrives through
				// `itemDoubleClicked` ([[BPR-1364]]).
				// [[BPR-1342]]. With ⌘ (Ctrl off macOS) the row opens in a
				// new window, on Overview.
				if (key->modifiers().testFlag(Qt::ControlModifier)) {
					const auto row = HomeRecentList->currentRow();
					if (row >= 0 && static_cast<std::size_t>(row) < RecentReviews.Entries.size()) {
						request_review_window(application::recent_review_row{ static_cast<std::size_t>(row) });
					}
					return true;
				}
				dispatch_recent_review(
					HomeRecentList->currentRow(),
					key->modifiers().testFlag(Qt::ShiftModifier));
				return true;
			}
		}
		return QMainWindow::eventFilter(aWatched, aEvent);
	}

	void workbench_window::add_setup_profile(application::github_profile_option aProfile)
	{
		if (std::ranges::any_of(GithubProfiles, [&](const auto& aExisting) { return aExisting.Profile == aProfile.Profile; })) {
			return;
		}
		GithubProfiles.push_back(aProfile);
		add_server_card(aProfile);
		SessionPanel->add_profile(std::move(aProfile));
		update_server_cards();
	}

	void workbench_window::set_setup_destination_dispatch(application::setup_destination_dispatch aDispatch)
	{
		SetupDestinationDispatch = std::move(aDispatch);
	}

	void workbench_window::present_setup_destination(const application::setup_destination_projection& aProjection)
	{
		if (RepositoryPreparing) {
			SetupDestinationRequest = aProjection.Pending ? std::optional{ aProjection.Pending->Request } : std::nullopt;
			if (aProjection.Pending && (aProjection.Pending->Stage == core::setup_destination_stage::add_server || aProjection.Pending->Stage == core::setup_destination_stage::sign_in)) {
				open_sign_in(aProjection.Pending->Destination.Hostname);
				if (aProjection.Pending->Stage == core::setup_destination_stage::add_server) {
					open_enterprise_server_draft();
					EnterpriseServerHostname->setText(QString::fromStdString(aProjection.Pending->Destination.Hostname));
					update_enterprise_server_actions();
				}
			} else if (!aProjection.Pending) {
				open_repository_addition();
				repository_addition_notice(aProjection.RepositoryPrepared
											   ? tr("Check the destination, then choose Add to save.")
											   : tr("Setup cancelled or unavailable. This draft has not been saved."));
			}
			return;
		}
		ApplyingSetupProjection   = true;
		SetupDestinationRequest   = aProjection.Pending ? std::optional{ aProjection.Pending->Request } : std::nullopt;
		SetupDestinationNeedsHome = aProjection.Pending
			&& aProjection.Pending->Stage != core::setup_destination_stage::opening
			&& aProjection.Pending->Stage != core::setup_destination_stage::public_preview;
		if (aProjection.Opened) {
			// [[BPR-0987]]. A link to the pull request already shown opens
			// nothing new, so the window keeps its view rather than returning
			// to Overview when that review next refreshes.
			OpenOnOverview = !aProjection.Navigated;
			HomeOpenBar->clear();
		}
		HomeSetupNotice = QString::fromStdString(aProjection.Notice);
		update_home_open_notice();
		if (aProjection.Opened && !aProjection.Notice.empty()) {
			statusBar()->showMessage(QString::fromStdString(aProjection.Notice));
		} else {
			statusBar()->clearMessage();
		}
		// [[BPR-0732]]. A public-preview stage needs no press: the host is
		// already opening it, so only Cancel is offered.
		const auto actionable = aProjection.Pending && (aProjection.Pending->Stage == core::setup_destination_stage::add_server || aProjection.Pending->Stage == core::setup_destination_stage::sign_in);
		SetupContinueButton->hide();
		if (actionable) {
			open_sign_in(aProjection.Pending->Destination.Hostname);
			if (aProjection.Pending->Stage == core::setup_destination_stage::add_server) {
				open_enterprise_server_draft();
				EnterpriseServerHostname->setText(QString::fromStdString(aProjection.Pending->Destination.Hostname));
				update_enterprise_server_actions();
			}
		}
		SetupContinueButton->setText(aProjection.Pending && aProjection.Pending->Stage == core::setup_destination_stage::add_server ? tr("Add server and continue") : tr("Sign in and continue"));
		SetupCancelButton->setVisible(aProjection.Pending && aProjection.Pending->Stage != core::setup_destination_stage::opening && (!SignInDialog || !SignInDialog->isVisible()));
		update_home_landing();
		ApplyingSetupProjection = false;
	}

	void workbench_window::present_link_location_notice(const QString& aText)
	{
		HomeSetupNotice = aText;
		update_home_open_notice();
		if (aText.isEmpty()) {
			statusBar()->clearMessage();
		} else {
			statusBar()->showMessage(aText);
		}
	}

	auto workbench_window::reveal_conversation_in_review(const std::string& aIdentity) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		const auto location = Finder->locate(aIdentity);
		return location.has_value() && open_conversation_in_review(location->Thread, location->Post);
	}

	auto workbench_window::open_conversation_in_review(const std::string& aThread, const std::string& aPost, const bool aFocus) -> bool
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return false;
		}
		auto location = Finder->locate(aPost.empty() ? aThread : aPost);
		if (!location.has_value() || location->Thread != aThread) {
			location = Finder->locate(aThread);
		}
		if (!location.has_value()) {
			return false;
		}
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{ .OnFile = !location->Path.isEmpty() });
		if (!route.Available) {
			return false;
		}
		// [[BPR-1293]]. The pick is Review's: the file through the files
		// drawer's intent, Review in the arrangement it last had, Comments
		// opened if it is closed (a manual open, as a gutter mark's press
		// is), and the thread in place once the file's comparison shows it.
		PendingPick = conversation_pick{
			.Thread = location->Thread,
			.Post   = location->Post,
			.File   = core::file_identity_of(location->Path.toStdString()),
			.Before = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt,
			.Focus  = aFocus,
		};
		choose_home(false);
		// Review first, then the file: a file gesture that follows the
		// modality wins over the active tab's restoration.
		choose_modality(review_modality::review);
		if (const auto file = file_for_path(location->Path); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
		if (route.OpenComments && Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{
				.Drawer = core::workbench_drawer::conversations,
				.State  = core::workbench_drawer_state::expanded,
			});
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		resume_conversation_pick();
		resume_pending_comment_pick();
		return true;
	}

	auto workbench_window::open_thread_from_threads(const std::string& aThread, const std::string& aPost, const core::workbench_thread_destination aDestination) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		const auto& conversations = Finder->conversations();
		if (std::ranges::find(conversations, aThread, &finder_conversation::Id) == conversations.end()) {
			return false;
		}
		return move_thread(aPost.empty() ? aThread : aPost, core::workbench_thread_origin::threads, aDestination);
	}

	auto workbench_window::move_thread(const std::string& aIdentity, const core::workbench_thread_origin aOrigin, const core::workbench_thread_destination aDestination) -> bool
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || aIdentity.empty()) {
			return false;
		}
		if (ThreadsRevealNotice) {
			ThreadsRevealNotice = false;
			ConversationPanel->present_notice(QString{ });
		}
		const auto say = [this](const QString& aText) {
			statusBar()->showMessage(aText);
			QAccessibleAnnouncementEvent event{ this, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		};
		// [[BPR-1581]]. My draft, written or pending, by its note.
		const auto draft = aIdentity.starts_with("draft:unpublished");
		// The window observes; the core decides. A thread on a file is the
		// finder's; one that is not is the conversation model's alone.
		auto thread   = aIdentity;
		auto post     = std::string{ };
		auto onFile   = draft;
		auto resolved = false;
		auto unread   = false;
		auto path     = QString{ };
		if (!draft) {
			if (const auto location = Finder->locate(aIdentity); location.has_value()) {
				thread                    = location->Thread;
				post                      = location->Post;
				onFile                    = !location->Path.isEmpty();
				path                      = location->Path;
				const auto& conversations = Finder->conversations();
				if (const auto found = std::ranges::find(conversations, thread, &finder_conversation::Id); found != conversations.end()) {
					resolved = found->Resolved;
					unread   = found->Unread > 0U;
				}
			} else {
				auto* const model = ConversationPanel->model();
				auto        index = model->index_for_id(aIdentity);
				if (!index.isValid()) {
					return false;
				}
				while (index.parent().isValid()) {
					index = index.parent();
				}
				thread   = model->id_for_index(index);
				post     = aIdentity;
				resolved = index.data(github_conversation_model::resolved_role).toBool();
			}
		}
		auto placed = true;
		if (const auto root = ConversationPanel->model()->index_for_id(thread); root.isValid() && !draft) {
			using availability = application::github_conversation_anchor_availability;
			const auto anchor  = static_cast<availability>(root.data(github_conversation_model::anchor_availability_role).toInt());
			placed             = !root.data(github_conversation_model::outdated_role).toBool() && (anchor == availability::current || anchor == availability::file_level);
		}
		const auto view  = ConversationPanel->discuss_view();
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{
			.Destination = aDestination,
			.Origin      = aOrigin,
			.Draft       = draft || thread == "draft:unpublished",
			.OnFile      = onFile,
			.Placed      = placed,
			.Hiding      = core::workbench_discuss_filters{
				.OtherFile    = view.Scope == cards_scope::this_file && (!onFile || (view.File.has_value() && *view.File != core::file_identity_of(path.toStdString()))),
				.OnlyNew      = view.OnlyNew && !unread,
				.HideResolved = view.HideResolved && resolved,
				.Listing      = view.Listing.has_value() && !view.Listing->contains(thread),
			},
		});
		if (!route.Available) {
			say(aDestination == core::workbench_thread_destination::threads ? tr("This comment is not on a file, so Threads has no row for it. Discuss shows it.")
																			: tr("This comment is not on a file, so Review has no place for it. Discuss shows it."));
			return false;
		}
		if (route.Modality == core::workbench_modality::threads) {
			return show_thread_in_threads(draft ? std::string{ "draft:unpublished" } : thread, draft ? std::string{ } : post);
		}
		if (route.Composer) {
			const auto shown = show_draft_in_discuss(aIdentity);
			if (shown) {
				say(tr("Discuss lists no drafts, so your draft opens in its composer."));
			}
			return shown;
		}
		if (route.Modality == core::workbench_modality::discuss) {
			return open_conversation_in_discuss(thread, post, route.Clear);
		}
		const auto opened = open_conversation_in_review(thread, post, aOrigin != core::workbench_thread_origin::threads);
		if (opened && route.Reason == core::workbench_thread_route_reason::draft_opens_in_review) {
			say(tr("Your draft is written beside its line, so it opens in Review."));
		} else if (opened && route.Reason == core::workbench_thread_route_reason::opens_without_a_line) {
			say(tr("This thread has no current line, so it opens among %1's threads without one.").arg(path.section(QLatin1Char{ '/' }, -1)));
		}
		return opened;
	}

	auto workbench_window::show_draft_in_discuss(const std::string& aCard) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return false;
		}
		auto* const cards = ConversationPanel->cards();
		if (cards->writing_draft()) {
			maximize_drawer_draft();
			return true;
		}
		// Another line's draft is brought to its line first, as opening its
		// note does, so the composer holds it.
		if (aCard != "draft:unpublished") {
			const auto& all  = cards->cards();
			const auto  card = std::ranges::find(all, aCard, &thread_card::Id);
			if (card == all.end() || !card->Side.has_value() || card->Line.value_or(0U) == 0U || ComparisonSurface == nullptr) {
				return false;
			}
			static_cast<void>(ComparisonSurface->select_conversation_line(*card->Side, *card->Line - 1U));
		}
		if (!ReviewEditor->shown_draft().has_value()) {
			return false;
		}
		if (begin_new_comment(false) && cards->writing_draft()) {
			maximize_drawer_draft();
		}
		return CompositionPage == ComposeInlinePage;
	}

	auto workbench_window::thread_card_on_line(const core::diff_side aSide, const std::uint32_t aLine) const -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return std::nullopt;
		}
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find_if(cards, [aSide, aLine](const thread_card& aCard) {
			return aCard.Side == aSide && aCard.Line == aLine + 1U && !aCard.Outdated;
		});
		return card == cards.end() ? std::nullopt : std::optional{ card->Id };
	}

	auto workbench_window::show_selected_thread(const core::workbench_thread_destination aDestination) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || Modality == nullptr) {
			return false;
		}
		// [[BPR-1586]]. Threads, like Discuss, is shown from Review.
		const auto toDiscuss = aDestination != core::workbench_thread_destination::review;
		const auto here      = Modality->current();
		if ((toDiscuss && here != review_modality::review) || (!toDiscuss && here != review_modality::discuss) || CompositionPage != nullptr) {
			return false;
		}
		auto* const cards     = ConversationPanel->cards();
		auto        selection = core::workbench_thread_selection{
			.OpenThread = cards->open_thread_id(),
			.PickedPost = cards->picked_post(),
		};
		if (const auto current = ConversationPanel->model()->id_for_index(ConversationPanel->view()->currentIndex()); !current.empty()) {
			auto index = ConversationPanel->model()->index_for_id(current);
			while (index.parent().isValid()) {
				index = index.parent();
			}
			selection.CurrentThread = ConversationPanel->model()->id_for_index(index);
			selection.CurrentPost   = current == selection.CurrentThread ? std::string{ } : current;
		}
		if (toDiscuss && ComparisonSurface != nullptr && ComparisonSurface->selection().has_value()) {
			const auto& line = ComparisonSurface->selection()->PreferredLine;
			if (const auto onLine = thread_card_on_line(line.Side, line.Line); onLine.has_value()) {
				selection.LineThread = *onLine;
			}
			const auto* const focus = QApplication::focusWidget();
			selection.CodeFocused   = focus != nullptr && (focus == ComparisonSurface->left_pane() || focus == ComparisonSurface->right_pane());
		}
		const auto target = core::choose_workbench_thread_target(selection);
		if (!target.has_value()) {
			statusBar()->showMessage(tr("Select a thread first."));
			return false;
		}
		const auto origin = toDiscuss ? core::workbench_thread_origin::review : core::workbench_thread_origin::discuss;
		return move_thread(target->Post.empty() ? target->Thread : target->Post, origin, aDestination);
	}

	auto workbench_window::show_thread_in_discuss_action() const noexcept -> QAction*
	{
		return ShowInDiscussAction;
	}

	auto workbench_window::show_thread_in_review_action() const noexcept -> QAction*
	{
		return ShowInReviewAction;
	}

	auto workbench_window::show_thread_in_threads_action() const noexcept -> QAction*
	{
		return ShowInThreadsAction;
	}

	auto workbench_window::show_thread_in_threads(const std::string& aThread, const std::string& aPost) -> bool
	{
		if (Finder == nullptr || Modality == nullptr) {
			return false;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		auto cleared = false;
		if (!Finder->reveal(aThread, aPost, &cleared)) {
			statusBar()->showMessage(tr("Threads has no row for this thread."));
			return false;
		}
		if (cleared) {
			statusBar()->showMessage(tr("To show this thread, Threads cleared its search and filters."));
		}
		return true;
	}

	void workbench_window::update_show_thread_actions()
	{
		const auto here = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
		// Ctrl+Alt is AltGr on Windows, where AltGr with 2 or 3 types a
		// character: the keys step aside while the keyboard is typing.
		const auto reading = HasGithubPullRequest && CompositionPage == nullptr && !showing_home() && !TypingFocus;
		if (ShowInDiscussAction != nullptr) {
			ShowInDiscussAction->setEnabled(reading && here == review_modality::review);
		}
		if (ShowInReviewAction != nullptr) {
			ShowInReviewAction->setEnabled(reading && here == review_modality::discuss);
		}
		if (ShowInThreadsAction != nullptr) {
			ShowInThreadsAction->setEnabled(reading && here == review_modality::review);
		}
	}

	auto workbench_window::open_conversation_in_discuss(const std::string& aThread, const std::string& aPost, const core::workbench_discuss_filters& aClear) -> bool
	{
		choose_home(false);
		choose_modality(review_modality::discuss);
		ConversationPanel->clear_discuss_filters(aClear.OtherFile, aClear.OnlyNew, aClear.HideResolved, aClear.Listing);
		auto* const cards = ConversationPanel->cards();
		if (std::ranges::none_of(cards->cards(), [&aThread](const thread_card& aCard) { return aCard.Id == aThread; })) {
			return false;
		}
		// As a Review pick: the thread opens in place with its post washed
		// amber and brought into view, and the selection names the post.
		// [[BPR-1581]]. A thread named by its root opens at its first post,
		// and the post's arrival is marked.
		auto post = aPost;
		if (post.empty()) {
			const auto& all  = cards->cards();
			const auto  card = std::ranges::find(all, aThread, &thread_card::Id);
			post             = card != all.end() && !card->Posts.empty() ? card->Posts.front().Id : aThread;
		}
		cards->open_thread_at(aThread, post);
		if (const auto index = ConversationPanel->model()->index_for_id(post); index.isValid()) {
			ConversationPanel->choose(index);
		}
		cards->mark_arrival();
		cards->setFocus(Qt::OtherFocusReason);
		if (aClear.any()) {
			auto cleared = QStringList{ };
			if (aClear.OtherFile) {
				cleared.push_back(tr("File scope"));
			}
			if (aClear.OnlyNew) {
				cleared.push_back(tr("Only new"));
			}
			if (aClear.HideResolved) {
				cleared.push_back(tr("Hide resolved"));
			}
			if (aClear.Listing) {
				cleared.push_back(tr("the Overview's list"));
			}
			const auto last  = cleared.takeLast();
			const auto names = cleared.isEmpty() ? last : tr("%1 and %2").arg(cleared.join(QStringLiteral(", ")), last);
			const auto text  = tr("To show this thread, Discuss turned off %1.").arg(names);
			ConversationPanel->present_notice(text);
			ThreadsRevealNotice = true;
			QAccessibleAnnouncementEvent event{ ConversationPanel, text };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		return true;
	}

	void workbench_window::resume_conversation_pick()
	{
		if (!PendingPick.has_value() || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr
			|| (Modality != nullptr && Modality->current() != review_modality::review)) {
			return;
		}
		auto* const cards   = ConversationPanel->cards();
		const auto  context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		const auto  digest  = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
			? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
			: std::nullopt;
		// The thread opens once its file's comparison is the one shown, so
		// the drawer's notes are that file's for good.
		const auto shown = context.has_value() && context->Key.File == PendingPick->File && digest.has_value();
		if (shown && std::ranges::any_of(cards->cards(), [this](const thread_card& aCard) { return aCard.Id == PendingPick->Thread; })) {
			const auto pick = *PendingPick;
			PendingPick.reset();
			// [[BPR-1740]]. The thread is open beside its code now.
			clear_reply_refusal();
			cards->open_thread_at(pick.Thread, pick.Post);
			// The selection authority names the picked post itself, as a
			// revealed comment always has; its thread stays the one open.
			if (const auto post = ConversationPanel->model()->index_for_id(pick.Post); post.isValid()) {
				// [[BPR-1698]]. The pick reveals its line itself, once.
				// Choosing moves the code only through the view's current
				// change, and a long file's comparison is adopted turns after
				// it is published, from the surface's selection observer,
				// whose guard drops that change: the file opened where its
				// own selection put it, not at the thread's line. An
				// unchanged current row sends no change either.
				const auto applying = std::exchange(ApplyingConversationAnchor, true);
				ConversationPanel->choose(post);
				ApplyingConversationAnchor = applying;
				synchronize_selected_conversation(post);
			}
			// [[BPR-1581]]. The post's arrival is marked, as in Discuss, and a
			// move from Discuss leaves the keyboard on the notes.
			cards->mark_arrival();
			if (pick.Focus) {
				cards->setFocus(Qt::OtherFocusReason);
			}
			return;
		}
		// Another file's comparison arrived first: the reviewer went on.
		if (context.has_value() && context->Key.File != PendingPick->File && digest.has_value() && digest != PendingPick->Before) {
			PendingPick.reset();
		}
	}

	void workbench_window::toggle_threads()
	{
		if (Modality == nullptr || !HasGithubPullRequest) {
			return;
		}
		if (Modality->current() == review_modality::threads && CompositionPage == nullptr && !showing_home()) {
			leave_threads();
			return;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
	}

	void workbench_window::leave_threads()
	{
		if (Modality == nullptr || Modality->current() != review_modality::threads) {
			return;
		}
		const auto back  = ThreadsReturn.value_or(review_modality::review);
		const auto focus = ThreadsReturnFocus;
		choose_modality(back == review_modality::threads ? review_modality::review : back);
		if (focus && focus->isVisible()) {
			focus->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::present_finder_reading()
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		Finder->set_colours(cards->accent(), cards->marker());
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		Finder->set_reading(finder_reading{
			.File   = context.has_value() ? std::optional{ context->Key.File } : std::nullopt,
			.Thread = cards->open_thread_id(),
			.Post   = cards->picked_post(),
		});
	}

	auto workbench_window::finder() const noexcept -> conversation_finder*
	{
		return Finder;
	}

	auto workbench_window::show_threads_action() const noexcept -> QAction*
	{
		return ShowThreadsAction;
	}

	void workbench_window::set_conversation_labels(std::optional<application::conversation_labels_projection> aLabels)
	{
		if (Finder != nullptr) {
			Finder->set_labels(std::move(aLabels));
		}
	}

	auto workbench_window::reveal_conversation(const std::string& aIdentity) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->model() == nullptr || Modality == nullptr) {
			return false;
		}
		const auto previous = Modality->current();
		choose_home(false);
		choose_modality(review_modality::discuss);
		auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (!index.isValid()) {
			// Discuss's file scope or filters may hide it; the link asked for
			// this conversation, so they widen as Clear filters would.
			ConversationPanel->scope_whole_pull_request_button()->setChecked(true);
			ConversationPanel->filter_unread_button()->setChecked(false);
			ConversationPanel->filter_unresolved_button()->setChecked(false);
			index = ConversationPanel->model()->index_for_id(aIdentity);
		}
		if (!index.isValid()) {
			choose_modality(previous);
			return false;
		}
		// The view's current row is what reveals a conversation, as a press on
		// it does; the cards follow it.
		ConversationPanel->choose(index);
		ConversationPanel->view()->scrollTo(index);
		return true;
	}

	void workbench_window::open_from_home_bar()
	{
		if (SessionPanel != nullptr && SessionPanel->status() == core::github_status::establishing_session) {
			return;
		}
		if (HomeOpenBar && SetupDestinationDispatch) {
			if (SignInDialog && SignInDialog->isVisible()) {
				SignInDialog->reject();
			}
			SetupDestinationDispatch(application::submit_setup_destination{ HomeOpenBar->text().toStdString() });
		}
	}

	auto workbench_window::home_refusal_wording() const -> std::optional<core::pull_request_refusal_wording>
	{
		if (SessionPanel == nullptr || HasGithubPullRequest || !PullRequestRefusal
			|| SessionPanel->status() != core::github_status::pull_request_failed) {
			return std::nullopt;
		}
		// The core words it from the typed refusal and this window's connection
		// facts; the window only gathers those facts.
		const auto  session = SessionPanel->session();
		const auto* option  = session ? SessionPanel->profile_option(session->Profile) : nullptr;
		return core::word_pull_request_refusal(
			*PullRequestRefusal,
			core::pull_request_refusal_context{
				.Login            = session && !PublicPreviewSignedOut ? session->Login : std::string{ },
				.GithubCliOffered = static_cast<bool>(GithubCliDiscoveryDispatch),
				.ThroughGithubCli = option != nullptr
					&& option->Transport == application::github_profile_transport::github_cli,
			});
	}

	auto workbench_window::home_refusal_text() const -> QString
	{
		if (SessionPanel == nullptr || HasGithubPullRequest) {
			return { };
		}
		if (SessionPanel->status() == core::github_status::pull_request_failed) {
			if (const auto wording = home_refusal_wording(); wording.has_value()) {
				return QString::fromStdString(wording->Heading) + QLatin1Char{ '\n' }
				+ QString::fromStdString(wording->Explanation);
			}
			return SessionPanel->showing_diagnostic()
				? QString::fromStdString(SessionPanel->diagnostic_text())
				: QStringLiteral("The pull request could not be loaded.");
		}
		// A signed-out mode refusal is not a pull request the bar named; it is
		// said on the reading host's card instead.
		return { };
	}

	void workbench_window::take_refusal_action(const core::pull_request_refusal_action aAction)
	{
		// [[BPR-1200]]. Each step is one Home already offers elsewhere; the
		// refusal leads to it rather than performing anything of its own.
		switch (aAction) {
		case core::pull_request_refusal_action::sign_in:
			begin_selected_sign_in();
			return;
		case core::pull_request_refusal_action::use_github_cli:
			begin_github_cli_from_home();
			return;
		case core::pull_request_refusal_action::use_another_account:
			open_sign_in(SessionPanel != nullptr ? SessionPanel->active_host_text() : std::string{ });
			return;
		}
	}

	void workbench_window::update_home_open_notice()
	{
		if (HomeOpenNotice == nullptr || HomeOpenDetails == nullptr || SessionPanel == nullptr) {
			return;
		}
		if (SessionPanel->status() == core::github_status::loading_pull_request) {
			DismissedHomeRefusal.clear();
		}
		// [[BPR-1357]]. The opening the core and the application decided, when
		// no shown row says it: what the bar asked for, or a row Home no longer
		// shows. A row that says it is where the reviewer is looking.
		const auto& state       = HomeOpen.has_value() ? HomeOpen->State : std::optional<core::home_open_state>{ };
		const auto  inFlight    = state.has_value() && state->Phase != core::home_open_phase::failed;
		const auto  failed      = state.has_value() && state->Phase == core::home_open_phase::failed;
		const auto  bandOpening = inFlight && !HomeOpenOnRow;
		// A setup stage's own notice speaks first, then a review restoration's,
		// then the refusal of the last load, unless the reviewer has already
		// begun rewriting what was said.
		const auto refusal = home_refusal_text();
		const auto passive = !HomeRestorationNotice.isEmpty() ? HomeRestorationNotice : refusal;
		// [[BPR-1357]]. A failure a row already says is said there, not twice.
		const auto rowSaysFailure = failed && HomeOpenOnRow && HomeRestorationNotice.isEmpty();
		const auto spoken         = HomeSetupNotice.isEmpty() && !passive.isEmpty()
			&& passive != DismissedHomeRefusal && !rowSaysFailure;
		const auto refused = spoken && HomeRestorationNotice.isEmpty();
		// [[BPR-0775]]. A first load in progress is said here as well, where the
		// pull request was named, since the stack's page that said it is not
		// Home's. It is not a refusal, so it has no Copy details. [[BPR-1357]]:
		// with the application's opening it names the pull request and the step;
		// a row that says it leaves this line to the rest of Home's status.
		const auto loading = SessionPanel->status() == core::github_status::loading_pull_request && !HasGithubPullRequest
			? (bandOpening ? home_open_text(false)
						   : (HomeOpen.has_value() ? QString{ } : QString::fromStdString(SessionPanel->status_text())))
			: QString{ };
		const auto notice  = !HomeSetupNotice.isEmpty() ? HomeSetupNotice
			: spoken                                    ? passive
														: loading;
		// [[BPR-1200]]. A refusal the core has its own words for is its card;
		// every other notice keeps the line.
		const auto wording = refused ? home_refusal_wording() : std::nullopt;
		const auto carded  = wording.has_value() && HomeRefusal != nullptr;
		HomeOpenNotice->setText(carded ? QString{ } : notice);
		HomeRefusalDetails.clear();
		if (refused) {
			HomeRefusalDetails = QString::fromStdString(SessionPanel->status_text());
			if (HomeRefusalDetails != refusal) {
				HomeRefusalDetails += QLatin1Char('\n') + refusal;
			}
		}
		const auto said = !carded && !notice.isEmpty();
		HomeOpenDetails->setVisible(refused && !carded);
		if (HomeOpenCancel != nullptr && HomeOpenRetry != nullptr) {
			// Offered only where the core offers them, and only beside the line
			// that says the opening.
			const auto sayingOpening = said && notice == loading && bandOpening;
			HomeOpenCancel->setVisible(sayingOpening && HomeOpen->Cancellable);
			if (HomeOpenActivity != nullptr && HomeOpenShortcut != nullptr) {
				HomeOpenActivity->setVisible(sayingOpening);
				HomeOpenShortcut->setVisible(!sayingOpening);
			}
			HomeOpenRetry->setVisible(refused && failed && !HomeOpenOnRow && HomeOpen->Retryable);
		}
		if (HomeStatus != nullptr && said) {
			HomeStatus->set_tone(refused                                         ? home_status_tone::problem
									 : (!loading.isEmpty() && notice == loading) ? home_status_tone::busy
																				 : home_status_tone::notice);
		}
		HomeOpenNotice->setVisible(said);
		if (HomeRefusal != nullptr) {
			if (carded) {
				HomeRefusal->present(*wording);
			}
			HomeRefusal->setVisible(carded);
		}
		update_home_status();
	}

	auto workbench_window::home_status_echoes_bar() const -> bool
	{
		const auto message = statusBar()->currentMessage();
		return showing_home() && !message.isEmpty() && HomeStatus != nullptr && HomeStatus->isVisible()
			&& HomeStatusText != nullptr && HomeStatusText->isVisible() && HomeStatusText->text() == message;
	}

	auto workbench_window::home_status_line() const noexcept -> qt::home_status_line*
	{
		return HomeStatus;
	}

	auto workbench_window::home_status_text() const noexcept -> QLabel*
	{
		return HomeStatusText;
	}

	auto workbench_window::home_open_cancel_button() const noexcept -> QPushButton*
	{
		return HomeOpenCancel;
	}

	auto workbench_window::home_open_retry_button() const noexcept -> QPushButton*
	{
		return HomeOpenRetry;
	}

	auto workbench_window::home_open() const noexcept -> const std::optional<application::home_open_projection>&
	{
		return HomeOpen;
	}

	void workbench_window::set_home_open_dispatch(application::home_open_dispatch aDispatch)
	{
		HomeOpenDispatch = std::move(aDispatch);
	}

	void workbench_window::set_close_review_dispatch(std::function<void()> aDispatch)
	{
		CloseReviewDispatch = std::move(aDispatch);
	}

	auto workbench_window::release_review() -> std::optional<QString>
	{
		if (HomeLanding == nullptr || HomeLanding->state() != landing_state::back_to_review || !CloseReviewDispatch) {
			return std::nullopt;
		}
		// The pull request's short name, as a link names it, not the strip's
		// identity with its title.
		auto closing    = IdentityOwner.isEmpty() || IdentityRepository.isEmpty() || IdentityNumber.isEmpty()
			? QString{ }
			: QStringLiteral("%1/%2#%3").arg(IdentityOwner, IdentityRepository, IdentityNumber);
		ReleasingReview = true;
		CloseReviewDispatch();
		ReleasingReview = false;
		if (HomeLanding->state() == landing_state::back_to_review) {
			return std::nullopt;
		}
		return closing;
	}

	void workbench_window::close_review()
	{
		const auto closed = release_review();
		if (!closed) {
			return;
		}
		statusBar()->showMessage(
			closed->isEmpty() ? QStringLiteral("Closed the review.") : QStringLiteral("Closed the review of %1.").arg(*closed), 8000);
		if (showing_home() && HomeOpenBar != nullptr) {
			HomeOpenBar->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::finish_review_at_home(QString aSaid)
	{
		// [[BPR-1816]]. Only the review that was confirmed: another opened
		// meanwhile, or none left open, is not closed.
		const auto expected = std::exchange(FinishingAtHome, std::nullopt);
		auto       said     = std::move(aSaid);
		if (expected && *expected == IdentityText) {
			choose_home(true);
			if (const auto closed = release_review(); closed) {
				said = closed->isEmpty() ? tr("%1. Closed the review.").arg(said) : tr("%1. Closed the review of %2.").arg(said, *closed);
				if (showing_home() && HomeOpenBar != nullptr) {
					HomeOpenBar->setFocus(Qt::OtherFocusReason);
				}
			} else {
				// Nothing was closed, so Home does not cover the review.
				choose_home(false);
			}
		}
		statusBar()->showMessage(said, 8000);
		QAccessibleAnnouncementEvent event{ this, said };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto workbench_window::finishing_at_home() const noexcept -> bool { return FinishingAtHome.has_value(); }

	void workbench_window::set_home_failure_ink(const QColor& aInk)
	{
		if (HomeStatus != nullptr) {
			HomeStatus->set_failure_ink(aInk);
		}
		PendingFailureInk = aInk;
		apply_pending_appearance();
	}

	void workbench_window::set_pending_high_contrast(const bool aHigh)
	{
		PendingHighContrast = aHigh;
		apply_pending_appearance();
	}

	auto workbench_window::pending_veils() const noexcept -> const pending_panes&
	{
		return PendingPanes;
	}

	auto workbench_window::pending_full_motion() const noexcept -> bool
	{
		// The system's Reduce Motion is honoured; the Dif motion choice is the
		// mascot's own and does not still the application's waits. A platform
		// with no such setting turns; one not yet observed, or whose
		// observation failed, stays still.
		return DifResolutionReady
			&& (DifResolution.Observation == core::motion_observation::full || DifResolution.Observation == core::motion_observation::unsupported);
	}

	void workbench_window::apply_pending_appearance()
	{
		// Every waiting pane in the window, its own and its surfaces', draws
		// alike: found rather than listed, so a surface added later follows.
		const auto full = pending_full_motion();
		for (auto* const widget : findChildren<QWidget*>()) {
			auto* const veil = dynamic_cast<pending_veil*>(widget);
			if (veil == nullptr) {
				continue;
			}
			veil->set_full_motion(full);
			veil->set_high_contrast(PendingHighContrast);
			if (PendingFailureInk.isValid()) {
				veil->set_failure_ink(PendingFailureInk);
			}
		}
		if (EpochSelectors != nullptr && EpochSelectors->readiness_mark() != nullptr) {
			EpochSelectors->readiness_mark()->set_full_motion(full);
		}
	}

	void workbench_window::build_pending_veils()
	{
		// [[BPR-1576]]. The review's content, while a refresh replaces it: one
		// veil and one rule across the drawers and the centre, under the bar.
		PendingPanes.Content = new pending_veil{ *ReviewSurface, pending_shape::block, QStringLiteral("Review"), CentralSplitter };
		PendingPanes.Content->setObjectName(QStringLiteral("reviewPendingVeil"));
		PendingPanes.Content->set_announces(false);
		const auto pane = [](QWidget* const aTarget, const QString& aName, const QString& aObject) -> pending_veil* {
			if (aTarget == nullptr) {
				return nullptr;
			}
			auto* const veil = new pending_veil{ *aTarget, pending_shape::rows, aName };
			veil->setObjectName(aObject);
			// How many rows are coming is not known yet: a short list says
			// "a list" without promising a long one.
			veil->set_placeholder_count(8);
			// The Overview says when the pull request is ready; the drawers
			// beside it do not say it again.
			veil->set_announces(false);
			return veil;
		};
		PendingPanes.Files         = pane(FilesTree, QStringLiteral("Files"), QStringLiteral("filesPendingVeil"));
		PendingPanes.Conversations = pane(ConversationPanel, QStringLiteral("Comments"), QStringLiteral("commentsPendingVeil"));
		PendingPanes.Threads       = pane(Finder, QStringLiteral("Threads"), QStringLiteral("threadsPendingVeil"));
		PendingPanes.Repositories  = pane(RepositoriesTree, QStringLiteral("Repositories"), QStringLiteral("repositoriesPendingVeil"));
		PendingPanes.HomeList      = pane(HomeList, QStringLiteral("My pull requests"), QStringLiteral("homeListPendingVeil"));
		if (PendingPanes.HomeList != nullptr) {
			// Home's list is the page: its own wait and failure are said.
			PendingPanes.HomeList->set_announces(true);
			PendingPanes.HomeList->set_retry([this] {
				if (QueueDiscoveryDispatch) {
					QueueDiscoveryDispatch(application::queue_discovery_intent::refresh);
				}
			});
		}
		apply_pending_appearance();
	}

	auto workbench_window::read_stage_words() const -> QString
	{
		if (SessionPanel == nullptr || SessionPanel->status() != core::github_status::loading_pull_request) {
			return { };
		}
		auto stage = QStringLiteral("the pull request's details");
		if (ReadProgress.has_value()) {
			switch (ReadProgress->Phase) {
			case application::github_read_phase::metadata:              break;
			case application::github_read_phase::files:                 stage = QStringLiteral("the changed files"); break;
			case application::github_read_phase::commits:               stage = QStringLiteral("the commits"); break;
			case application::github_read_phase::reviews:               stage = QStringLiteral("the reviews"); break;
			case application::github_read_phase::review_threads:        stage = QStringLiteral("the review threads"); break;
			case application::github_read_phase::comments:              stage = QStringLiteral("the comments"); break;
			case application::github_read_phase::checks:                stage = QStringLiteral("the checks"); break;
			case application::github_read_phase::related_pull_requests: stage = QStringLiteral("related pull requests"); break;
			case application::github_read_phase::update_history:        stage = QStringLiteral("the update history"); break;
			}
			if (ReadProgress->PagesReceived > 1U) {
				stage += QStringLiteral(", %1 pages so far").arg(ReadProgress->PagesReceived);
			}
		}
		return stage;
	}

	void workbench_window::update_pending_panes()
	{
		if (SessionPanel == nullptr) {
			return;
		}
		const auto reading    = SessionPanel->status() == core::github_status::loading_pull_request;
		const auto first      = reading && !HasGithubPullRequest && !HasCachedOverview;
		const auto refreshing = reading && HasGithubPullRequest;
		if (!reading) {
			RefreshConversationCurrent = false;
		}
		const auto stage   = read_stage_words();
		const auto waiting = stage.isEmpty() ? QString{ } : QStringLiteral("Still waiting for GitHub: %1.").arg(stage);
		const auto apply   = [](pending_veil* const aVeil, const bool aPending, const bool aContent, const pending_words& aWords) {
			if (aVeil == nullptr) {
				return;
			}
			if (!aPending) {
				aVeil->present_ready();
				return;
			}
			aVeil->present(core::pending_activity::loading, aContent, false, aWords);
		};
		// The review's own words are its Refresh control's; the veil only
		// speaks once the read is slow.
		if (refreshing && RefreshConversationCurrent && PendingPanes.Content != nullptr) {
			// [[BPR-1851]]. The file on screen and its threads are this
			// refresh's already: they are not washed over while the rest comes,
			// which the partial plate says at the review's foot.
			PendingPanes.Content->present(core::pending_activity::loading, true, true,
										  { .Waiting = waiting, .Coming = QStringLiteral("Refreshing remaining details…") });
		} else {
			apply(PendingPanes.Content, refreshing, true, { .Waiting = waiting });
		}
		apply(PendingPanes.Files, first, false, { .Loading = QStringLiteral("Loading the file list…"), .Waiting = waiting });
		apply(PendingPanes.Conversations, first, false, { .Loading = QStringLiteral("Loading comments…"), .Waiting = waiting });
		apply(PendingPanes.Threads, first, false, { .Loading = QStringLiteral("Loading threads…"), .Waiting = waiting });
		// Only a composed queue restores: without one nothing is on its way.
		const auto queued = static_cast<bool>(QueueDiscoveryDispatch);
		apply(PendingPanes.Repositories, queued && HomeAccount && QueueProjection.Status == application::queue_landing_status::restoring, false,
			  { .Loading = QStringLiteral("Loading saved repositories…") });
		if (PendingPanes.HomeList == nullptr) {
			return;
		}
		// Home's list: restoring, then personal discovery. Rows already listed
		// (the queue's or Recently viewed) stay readable while it runs.
		const auto& discovery    = QueueProjection.Discovery;
		const auto  rows         = (QueueLanding != nullptr && QueueLanding->rowCount() > 0) || !RecentReviews.Entries.empty();
		const auto  hasSaved     = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
			return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
		});
		const auto  restoring    = queued && HomeAccount && QueueProjection.Status == application::queue_landing_status::restoring;
		const auto  finding      = HomeAccount && hasSaved && discovery.State == application::queue_discovery_state::refreshing;
		const auto  repositories = discovery.Repositories == 1U ? QStringLiteral("1 saved repository") : QStringLiteral("%1 saved repositories").arg(discovery.Repositories);
		if (restoring) {
			PendingPanes.HomeList->present(core::pending_activity::loading, rows, false, { .Loading = QStringLiteral("Loading my pull requests…") });
		} else if (finding) {
			// [[BPR-1848]]. Rows already listed are the newest there are until
			// the refresh replaces them row by row, so they are never washed
			// over: the refresh is said on the partial plate at the list's foot,
			// and every row stays as readable and usable as before it began.
			PendingPanes.HomeList->present(core::pending_activity::loading, rows, rows,
										   { .Loading = QStringLiteral("Finding my pull requests in %1…").arg(repositories),
											 .Waiting = QStringLiteral("Still waiting for GitHub to search %1.").arg(repositories),
											 .Coming  = QStringLiteral("Refreshing my pull requests in %1…").arg(repositories),
											 .Done    = QStringLiteral("My pull requests found.") });
		} else if (HomeAccount && hasSaved && discovery.State == application::queue_discovery_state::stale && !rows) {
			PendingPanes.HomeList->present(core::pending_activity::failed, false, false,
										   { .Failure = QStringLiteral("Couldn't find my pull requests. %1").arg(home_discovery_sentence(discovery)) });
		} else {
			PendingPanes.HomeList->present_ready();
		}
	}

	void workbench_window::run_automatic_refresh(const std::function<void()>& aDispatch)
	{
		AutomaticRefreshNoted = true;
		if (aDispatch) {
			aDispatch();
		}
		AutomaticRefreshNoted = false;
	}

	auto workbench_window::pull_request_refresh() const noexcept -> const core::refresh_state&
	{
		return PullRefresh;
	}

	auto workbench_window::home_refresh() const noexcept -> const core::refresh_state&
	{
		return HomeRefresh;
	}

	auto workbench_window::refresh_note_widget() const noexcept -> refresh_note*
	{
		return RefreshNote;
	}

	auto workbench_window::refresh_freshness_label() const noexcept -> QLabel*
	{
		return RefreshFreshness;
	}

	auto workbench_window::refresh_clock() const -> std::int64_t
	{
		return HomeClock ? HomeClock().value_or(0) : 0;
	}

	auto workbench_window::refresh_freshness_text() const -> QString
	{
		const auto home = showing_home();
		if (!home && !HasGithubPullRequest) {
			return { };
		}
		if (home && !HomeAccount) {
			return { };
		}
		const auto& state = home ? HomeRefresh : PullRefresh;
		const auto  age   = state.UpdatedAt.has_value() ? refresh_age_words(core::refresh_age_at(*state.UpdatedAt, refresh_clock())) : QString{ };
		switch (state.Phase) {
		case core::refresh_phase::refreshing:
			return state.Trigger == core::refresh_trigger::reviewer ? QStringLiteral("Refreshing…") : QStringLiteral("Updating…");
		case core::refresh_phase::failed:
			return age.isEmpty() ? QStringLiteral("Not refreshed") : QStringLiteral("Not refreshed · updated %1").arg(age);
		case core::refresh_phase::updated:
		case core::refresh_phase::never:
			return age.isEmpty() ? QString{ } : QStringLiteral("Updated %1").arg(age);
		}
		return { };
	}

	auto workbench_window::refresh_result_text(const bool aHome, const core::refresh_state& aState) const -> QString
	{
		if (aState.Phase == core::refresh_phase::failed) {
			auto said = QStringLiteral("Couldn't refresh: %1.").arg(refresh_failure_words(aState.Failure));
			if (aState.Failure == core::refresh_failure::rate_limited && aState.RetryAfterSeconds.has_value()) {
				const auto minutes = (*aState.RetryAfterSeconds + 59U) / 60U;
				said += *aState.RetryAfterSeconds < 60U ? QStringLiteral(" Try again in %1 s.").arg(*aState.RetryAfterSeconds)
														: QStringLiteral(" Try again in %1 min.").arg(minutes);
			}
			if (aState.UpdatedAt.has_value()) {
				said += QStringLiteral(" Showing what was loaded %1.").arg(refresh_age_words(core::refresh_age_at(*aState.UpdatedAt, refresh_clock())));
			}
			return said;
		}
		const auto words = refresh_change_words(aState.Changes);
		const auto lead  = aState.Trigger == core::refresh_trigger::automatic ? QStringLiteral("Updated automatically")
			: aState.Trigger == core::refresh_trigger::elsewhere              ? QStringLiteral("Updated from another window")
																			  : QStringLiteral("Updated");
		if (words.isEmpty()) {
			return aHome ? QStringLiteral("Up to date: no new pull requests.") : QStringLiteral("Up to date: nothing new.");
		}
		return QStringLiteral("%1: %2.").arg(lead, words);
	}

	void workbench_window::observe_refresh_read(const core::pull_request_read_delta& aRead)
	{
		if (!aRead.Refresh) {
			return;
		}
		RefreshReadSeen = true;
		if (aRead.Stage == core::pull_request_read_stage::pending) {
			RefreshEnded.reset();
			RefreshConversationCurrent = false;
			RefreshBaseline            = RefreshLatest;
			apply_refresh_event(false, core::refresh_started{ .Trigger = AutomaticRefreshNoted ? core::refresh_trigger::automatic : core::refresh_trigger::reviewer });
			return;
		}
		RefreshEnded = aRead;
	}

	void workbench_window::settle_pull_request_refresh(const application::github_presentation_update& aUpdate)
	{
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			if (diagnostic.RetryAfterSeconds.has_value()) {
				RefreshRetryAfter = diagnostic.RetryAfterSeconds;
			}
		}
		if (RefreshEnded.has_value()) {
			const auto ended = *RefreshEnded;
			RefreshEnded.reset();
			switch (ended.Stage) {
			case core::pull_request_read_stage::loaded:
				apply_refresh_event(false, core::refresh_succeeded{ .Changes = core::compare_refresh(RefreshBaseline, RefreshLatest), .At = refresh_clock() });
				break;
			case core::pull_request_read_stage::failed:
				apply_refresh_event(false, core::refresh_failed{ .Failure = core::refresh_failure_of(ended.Reason), .RetryAfterSeconds = ended.Reason == core::github_reason_code::rate_limited ? RefreshRetryAfter : std::nullopt, .At = refresh_clock() });
				break;
			case core::pull_request_read_stage::cancelled:
			case core::pull_request_read_stage::pending:
				apply_refresh_event(false, core::refresh_cancelled{ });
				break;
			}
			RefreshRetryAfter.reset();
		} else if (ElsewhereChanges.has_value()) {
			// Another window of the profile read this pull request; its answer
			// was handed to this one.
			apply_refresh_event(false, core::refresh_started{ .Trigger = core::refresh_trigger::elsewhere });
			apply_refresh_event(false, core::refresh_succeeded{ .Changes = *ElsewhereChanges, .At = refresh_clock() });
		}
		ElsewhereChanges.reset();
		present_refresh();
	}

	void workbench_window::observe_home_refresh(const application::queue_discovery_status& aBefore, const application::queue_landing_projection& aAfter)
	{
		if (!aAfter.UpdatedItem) {
			HomeRefs.clear();
			HomeRefs.reserve(aAfter.Items.size());
			for (const auto& item : aAfter.Items) {
				HomeRefs.push_back(item.Ref);
			}
		}
		const auto& after = aAfter.Discovery;
		if (aBefore.Host != after.Host || aBefore.Account != after.Account) {
			HomeRefresh = core::refresh_state{ };
		}
		if (after.LastSuccess.has_value()) {
			HomeRefresh.UpdatedAt = after.LastSuccess;
		}
		using state           = application::queue_discovery_state;
		const auto refreshing = [](const state aState) { return aState == state::refreshing; };
		if (!refreshing(aBefore.State) && refreshing(after.State)) {
			HomeBaseline = HomeRefs;
			apply_refresh_event(true, core::refresh_started{ .Trigger = HomeRefreshAsked ? core::refresh_trigger::reviewer : core::refresh_trigger::automatic });
			HomeRefreshAsked = false;
		} else if (refreshing(aBefore.State) && !refreshing(after.State)) {
			switch (after.State) {
			case state::current:
			case state::partial:
				apply_refresh_event(true, core::refresh_succeeded{ .Changes = core::compare_refresh(HomeBaseline, HomeRefs), .At = after.LastSuccess.value_or(refresh_clock()) });
				break;
			case state::stale: {
				using problem = application::queue_discovery_problem;
				auto failure  = core::refresh_failure::other;
				switch (after.Problem) {
				case problem::rate_limited:
				case problem::secondary_rate_limited: failure = core::refresh_failure::rate_limited; break;
				case problem::offline:                failure = core::refresh_failure::offline; break;
				case problem::authentication:         failure = core::refresh_failure::signed_out; break;
				default:                              break;
				}
				apply_refresh_event(true, core::refresh_failed{ .Failure = failure, .RetryAfterSeconds = after.RetryAfterSeconds, .At = refresh_clock() });
				break;
			}
			case state::idle:
			case state::refreshing:
			case state::unavailable:
				apply_refresh_event(true, core::refresh_cancelled{ });
				break;
			}
		}
		present_refresh();
	}
}
