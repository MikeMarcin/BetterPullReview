#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/home_filter.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/type_preview.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAbstractListModel>
#include <QAbstractSlider>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1String>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethodsBase>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMargins>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto review_position_text_width(const QFontMetrics& aMetrics, const QString& aName, const bool aShown, const bool aCompact) -> int
		{
			// Native glyph advances can be fractional. Rounding down makes
			// elidedText shorten even the group name that the slot must retain.
			const auto precise = QFontMetricsF{ aMetrics };
			const auto width   = [&precise](const QString& aText) {
				return static_cast<int>(std::ceil(precise.horizontalAdvance(aText)));
			};
			auto result = width(aName);
			if (!aShown) {
				return result;
			}
			for (auto digit = '0'; digit <= '9'; ++digit) {
				const auto number = QString(aCompact ? 3 : 4, QLatin1Char{ digit });
				const auto sample = aCompact ? QStringLiteral("%1 %2/%2").arg(aName, number)
											 : QStringLiteral("%1 %2 of %2").arg(aName, number);
				result            = std::max(result, width(sample));
			}
			return result;
		}

		/// \brief [[BPR-1200]]. A read's first two answered pages are its
		/// repository and its pull request: GitHub has shown both to this
		/// connection, so the read may leave Home for Overview.
		constexpr std::uint32_t minimum_answered_pages = 2U;

		/// \brief [[BPR-1129]]. The least of the title worth showing after the
		/// number; with less room the name drops the title rather than show a
		/// sliver of it.
		constexpr int identity_title_minimum = 80;

		/// \brief Least width the repository keeps beside the number when elided.
		constexpr int identity_fragment_width = 24;

		/// \brief [[BPR-1532]]. Whether the focused widget, or a widget between
		/// it and the window, binds `aKey` with a widget-scoped shortcut of its
		/// own, as the tour editor's stop outline binds Option-Shift-Up to
		/// reorder. A window shortcut on the same key there would make Qt's
		/// match ambiguous, and neither would run.
		[[nodiscard]] auto binds_key_nearer(const QWidget* const aFocus, const QWidget* const aWindow, const QKeySequence& aKey) -> bool
		{
			for (const auto* widget = aFocus; widget != nullptr && widget != aWindow; widget = widget->parentWidget()) {
				const auto actions = widget->actions();
				if (std::ranges::any_of(actions, [&](const QAction* const aAction) {
						const auto context = aAction->shortcutContext();
						return (context == Qt::WidgetWithChildrenShortcut || (context == Qt::WidgetShortcut && widget == aFocus))
							&& aAction->shortcuts().contains(aKey);
					})) {
					return true;
				}
			}
			return false;
		}
	}

	auto workbench_window::behaviour_from_controls() const -> application::review_behaviour_settings
	{
		const auto finishChoice = [](const QCheckBox* const aBox) {
			return aBox->isChecked() ? core::review_finish_return::home : core::review_finish_return::pull_request;
		};
		const auto chosen = [this](const std::size_t aRow) -> std::size_t {
			const auto& buttons = BehaviourOptions[aRow];
			for (std::size_t index = 0U; index < buttons.size(); ++index) {
				if (buttons[index]->isChecked()) {
					return index;
				}
			}
			return 0U;
		};
		return application::review_behaviour_settings{
			.OpenModality      = static_cast<application::review_open_modality>(chosen(0U)),
			.MarkRead          = static_cast<application::review_mark_read_behaviour>(chosen(2U)),
			.ReturnComparison  = static_cast<application::review_return_comparison>(chosen(3U)),
			.CommentsFilter    = static_cast<application::review_comments_filter>(chosen(4U)),
			.DrawerControls    = static_cast<application::review_drawer_controls>(chosen(5U)),
			.FeedbackMarks     = static_cast<application::review_feedback_marks>(chosen(6U)),
			.LabelHelper       = LabelHelperSwitch != nullptr && LabelHelperSwitch->isChecked() ? application::review_label_helper::on : application::review_label_helper::off,
			.OverviewCounts    = static_cast<application::review_overview_counts>(chosen(7U)),
			.LabelColumns      = LabelColumnsEditor != nullptr ? LabelColumnsEditor->columns() : Behaviour.LabelColumns,
			.LineDetails       = BehaviourOptions[8].empty() ? Behaviour.LineDetails : static_cast<application::review_line_details>(chosen(8U)),
			.CenterRibbonWidth = CenterRibbonWidth != nullptr ? static_cast<std::uint32_t>(CenterRibbonWidth->value()) : Behaviour.CenterRibbonWidth,
			.MarkdownImages    = BehaviourOptions[9].empty() ? Behaviour.MarkdownImages : static_cast<application::review_markdown_images>(chosen(9U)),
			.ReadingWidth      = BehaviourOptions[10].empty() ? Behaviour.ReadingWidth : static_cast<core::reading_width>(chosen(10U)),
			.FinishReview      = FinishReviewSwitches[0] == nullptr ? Behaviour.FinishReview : core::review_finish_settings{
																								   .Comment        = finishChoice(FinishReviewSwitches[0]),
																								   .Approve        = finishChoice(FinishReviewSwitches[1]),
																								   .RequestChanges = finishChoice(FinishReviewSwitches[2]),
																							   },
		};
	}

	void workbench_window::dispatch_behaviour_from_controls()
	{
		if (!BehaviourDispatch) {
			return;
		}
		const auto next = behaviour_from_controls();
		if (next != AskedBehaviour.value_or(Behaviour)) {
			// [[BPR-1844]]. Choosing the first filter in Settings is a choice of
			// Review's Comments' filters: the next pull request opens with it.
			if (next.CommentsFilter != AskedBehaviour.value_or(Behaviour).CommentsFilter && ViewFilterRemember) {
				ViewFilterRemember({ .View = core::filter_view::review_comments, .Choice = application::first_filter_choice(next.CommentsFilter) });
			}
			AskedBehaviour = next;
			BehaviourDispatch(next);
		}
	}

	auto workbench_window::opening_modality() const -> review_modality
	{
		switch (Behaviour.OpenModality) {
		case application::review_open_modality::review:
			return review_modality::review;
		case application::review_open_modality::last_used:
			return LastUsedModality.value_or(review_modality::overview);
		case application::review_open_modality::overview:
			break;
		}
		return review_modality::overview;
	}

	void workbench_window::apply_first_comments_filter()
	{
		if (ConversationPanel == nullptr || FirstFilterApplied || !BehaviourPublished) {
			return;
		}
		FirstFilterApplied = true;
		// [[BPR-1844]]. Each view opens with the profile's last choice; Review's
		// Comments, until one is made, with the first-filter preference.
		ViewFilters = core::open_view_filters(ViewFilterDefaults ? ViewFilterDefaults() : core::view_filter_defaults{ },
											  application::first_filter_choice(Behaviour.CommentsFilter));
		ConversationPanel->open_with_filters(ViewFilters.Discuss, ViewFilters.ReviewComments);
		if (Finder != nullptr) {
			auto filters         = Finder->filters();
			filters.OnlyNew      = ViewFilters.Threads.OnlyNew;
			filters.HideResolved = ViewFilters.Threads.HideResolved;
			Finder->set_filters(std::move(filters));
		}
	}

	void workbench_window::choose_view_filter(const core::filter_view aView, const core::view_filter_choice aChoice)
	{
		auto step   = core::choose_view_filter(ViewFilters, aView, aChoice);
		ViewFilters = step.State;
		if (step.Remember && ViewFilterRemember) {
			ViewFilterRemember(*step.Remember);
		}
	}

	void workbench_window::set_view_filter_memory(std::function<core::view_filter_defaults()> aDefaults, std::function<void(const core::view_filter_remember&)> aRemember)
	{
		ViewFilterDefaults = std::move(aDefaults);
		ViewFilterRemember = std::move(aRemember);
	}

	auto workbench_window::view_filters() const noexcept -> const core::view_filter_state&
	{
		return ViewFilters;
	}

	// The named comparison is dispatched through the control's own action,
	// which is what carries the catalog revision the core expects; until the
	// history offers it the preference waits, and a later epoch publish asks
	// again.
	void workbench_window::honour_return_comparison()
	{
		if (!ReturnComparisonPending || EpochSelectors == nullptr) {
			return;
		}
		auto* const action = Behaviour.ReturnComparison
				== application::review_return_comparison::since_last_review
			? EpochSelectors->last_reviewed_action()
			: Behaviour.ReturnComparison == application::review_return_comparison::since_last_upload
			? EpochSelectors->previous_upload_action()
			: nullptr;
		if (action == nullptr) {
			ReturnComparisonPending = false;
			return;
		}
		if (!action->isEnabled()) {
			return;
		}
		ReturnComparisonPending = false;
		ReturnComparisonAsked   = true;
		if (!action->isChecked()) {
			action->trigger();
		}
	}

	// "Fully seen" is the shared scrollbar at its end (or nothing to scroll)
	// with a comparison shown; the file's own mark-read command is dispatched
	// once per file and material, only while the ledger offers it.
	void workbench_window::consider_mark_read_when_fully_seen()
	{
		if (Behaviour.MarkRead != application::review_mark_read_behaviour::when_fully_seen
			|| (TourEditor != nullptr && TourEditor->isVisible())
			|| !ReceiptDispatch || !Receipt.has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return;
		}
		const auto* const scrollbar = ComparisonSurface->shared_scrollbar();
		if (scrollbar == nullptr || scrollbar->value() < scrollbar->maximum()) {
			return;
		}
		for (const auto& action : Receipt->Actions) {
			if (action.Command.Kind != core::file_receipt_command_kind::mark_read || !action.Enabled) {
				continue;
			}
			if (MarkedReadWhenSeen == std::optional{ action.Command }) {
				return;
			}
			MarkedReadWhenSeen = action.Command;
			ReceiptDispatch(action.Command);
			return;
		}
	}

	auto workbench_window::file_for_path(const QString& aPath) const
		-> std::optional<core::workbench_file_id>
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr) {
			return std::nullopt;
		}
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (model->data(index, Qt::DisplayRole).toString() == aPath) {
				return model->file_at(index);
			}
		}
		return std::nullopt;
	}

	void workbench_window::update_home_landing()
	{
		if (HomeLanding == nullptr || SessionPanel == nullptr) {
			return;
		}
		// Home hosts the empty state. Which way in it
		// offers follows the session: an account first, then a pull request.
		const auto status   = SessionPanel->status();
		const auto signedIn = session_signed_in();
		// [[BPR-0732]]. A public-preview session is signed in for the
		// window's chrome but has no account: no queue, no picker, and the
		// same card and bar as before the session, since the bar is what
		// opened it.
		const auto account   = signedIn && !PublicPreviewSignedOut;
		const auto comparing = ComparisonSurface != nullptr
			&& ComparisonSurface->current_result() != nullptr;
		const auto loading = status == core::github_status::loading_pull_request;
		// [[BPR-1843]]. The Active review card names the pull request as a
		// link does, with its literal title beside it.
		HomeLanding->set_review_identity(
			IdentityOwner.isEmpty() || IdentityRepository.isEmpty() || IdentityNumber.isEmpty()
				? IdentityText
				: QStringLiteral("%1/%2#%3").arg(IdentityOwner, IdentityRepository, IdentityNumber));
		HomeLanding->set_review_title(
			IdentityOwner.isEmpty() ? QString{ } : QString::fromStdString(OpenQueueTitle));
		HomeLanding->set_review_position(
			Cursor && Cursor->Ordinal > 0U
				? QStringLiteral("file %1 of %2").arg(Cursor->Ordinal).arg(Cursor->ActionableCount)
				: QString{ });
		const auto landing = status == core::github_status::establishing_session ? landing_state::connecting
			: (HasGithubPullRequest || HasCachedOverview)                        ? landing_state::back_to_review
			: account                                                            ? landing_state::choose_pull_request
																				 : landing_state::sign_in;
		// [[BPR-1843]]. Opening another pull request from Home lets the open
		// review go before the new one arrives. Its card's place is held,
		// empty, until that open is over, so nothing under the pointer moves.
		// The open's projection may reach the window just after the review is
		// let go, in the same turn, so the hold is checked once that turn is
		// over. Close review holds nothing.
		const auto opening = HomeOpen.has_value() && HomeOpen->State.has_value();
		if (HomeLanding->state() == landing_state::back_to_review && landing == landing_state::choose_pull_request && !ReleasingReview
			&& showing_home() && HomeLanding->isVisible()) {
			HomeLandingHold = HomeLanding->height();
			if (!opening) {
				QTimer::singleShot(0, this, [this] {
					if (HomeLandingHold > 0 && !(HomeOpen.has_value() && HomeOpen->State.has_value())) {
						HomeLandingHold = 0;
						update_home_landing();
					}
				});
			}
		} else if (landing != landing_state::choose_pull_request) {
			HomeLandingHold = 0;
		}
		HomeLanding->set_held_height(HomeLandingHold);
		HomeLanding->set_state(landing);
		place_home_landing();
		if (HomeOpenAction != nullptr && HomeOpenBar != nullptr) {
			HomeOpenAction->setEnabled(status != core::github_status::establishing_session && !HomeOpenBar->text().trimmed().isEmpty());
		}
		// [[BPR-0775]]. Signed out, the bar reads a public pull request without
		// an account and says so; with an account it names any pull request.
		if (HomeOpenBar != nullptr) {
			HomeOpenBar->setPlaceholderText(
				account ? QStringLiteral("Paste a link, or write owner/name#123")
						: QStringLiteral("Paste a public pull request link to read it without an account"));
		}
		// The queue is a signed-in surface (RFC-BPR-0026 writes it for a
		// viewer); without an account there is nothing it could list.
		// [[BPR-0776]]. Recently viewed stays, for a signed-out window's public
		// reading, as [[BPR-0733]] and [[BPR-0775]] keep it.
		// The decision applies once the queue's surface exists; a window whose
		// session never spoke after it was built shows its queue, as it did.
		if (QueueSurface != nullptr) {
			HomeAccount = account;
			update_home_surface();
		}
		// Host-projected commands also keep standalone workbench compositions
		// visible before a GitHub snapshot or comparison has been presented.
		auto active = false;
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			const auto projection = command(static_cast<core::workbench_command_id>(index));
			active                = active || (projection && projection->Enabled);
		}
		// Drawer commands remain enabled without accepted review content. An
		// initial read failure or cancellation belongs on Home, where the Open
		// bar and diagnostic are available; a retained review stays in place.
		const auto readUnavailable = status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		// [[BPR-1200]]. A first read stays on Home, said under the bar, until
		// something shows that this connection can read it: GitHub has answered
		// for both the repository and the pull request, the read has shown its
		// overview, or the review or a cached overview is here. A read the
		// connection is refused therefore never passes through an empty
		// Overview on its way back; a long read still shows its progress on
		// Overview from its first answers ([[BPR-0795]]).
		const auto answered    = ReadProgress.has_value() && ReadProgress->PagesReceived >= minimum_answered_pages;
		const auto unconfirmed = loading && !LoadingOverviewShown && !answered;
		const auto nothing     = !HasGithubPullRequest && !HasCachedOverview && !comparing
			&& (!loading || unconfirmed) && (!active || readUnavailable || unconfirmed);
		const auto home = (nothing && !LocalCycleShown) || HomeRequested || SetupDestinationNeedsHome;
		// [[BPR-1157]]. A local cycle takes the review surface's place, and the
		// title row keeps only what Home keeps plus the cycle's name.
		const auto cycle = !home && LocalCycleShown && LocalCyclePage != nullptr;
		if (HomeAction != nullptr && HomeAction->isChecked() != home) {
			HomeAction->setChecked(home);
		}
		auto leavingHome = false;
		auto arrivedHome = false;
		if (CentralStack != nullptr) {
			const auto arriving = home && CentralStack->currentWidget() != HomePage;
			arrivedHome         = arriving;
			leavingHome         = !home && CentralStack->currentWidget() == HomePage;
			if (arriving) {
				// [[BPR-0775]]. What had focus in the review has it again when
				// the review returns: Home covering it moves nothing there.
				// The window's own focus child, which it keeps whether or not it
				// is the active window.
				auto* const focused = focusWidget();
				FocusBeforeHome     = focused != nullptr && isAncestorOf(focused) && !HomePage->isAncestorOf(focused)
					? focused
					: nullptr;
			}
			CentralStack->setCurrentWidget(home ? HomePage : cycle ? static_cast<QWidget*>(LocalCyclePage)
																   : ReviewSurface);
			// [[BPR-0732]], [[BPR-0775]]. The bar has focus when Home appears,
			// signed in or out. Only on arrival, so a projection cannot steal
			// focus from wherever the reviewer has moved it since.
			if ((arriving || !HomeFocused) && home && HomeOpenBar != nullptr && isVisible()) {
				HomeOpenBar->setFocus(Qt::OtherFocusReason);
				HomeFocused = true;
			}
		}
		update_dif_presentation();
		// On Home the strip keeps only what Home can use: the account and
		// settings at its right, the inset and the spacers that place them.
		// Drawers, navigation, the modalities, review details and refresh
		// are a pull request's, and return with one.
		const auto stripHome = home || cycle;
		if (PrimaryToolbar != nullptr && (StripHome != stripHome || stripHome)) {
			StripHome = stripHome;
			for (auto* const action : PrimaryToolbar->actions()) {
				const auto name = action->objectName();
				const auto kept = action == AccountAction || action == SettingsAction
					|| action == HomeAction
					|| name == QLatin1String{ "workbenchStripInsetAction" }
					|| name == QLatin1String{ "workbenchNavigationSpacerAction" }
					// [[BPR-0653]]. On Windows the strip is the title row on
					// Home too: its menu control and caption buttons stay.
					|| name == QLatin1String{ "workbenchStripMenuAction" }
					|| name == QLatin1String{ "workbenchCaptionButtonsAction" }
					// With a review open behind Home, the modalities are the
					// way back to it.
					|| (HasGithubPullRequest && !cycle
						&& name == QLatin1String{ "workbenchModalityAction" })
					|| (cycle && action == IdentityAction);
				action->setVisible(!stripHome || kept);
			}
			// [[BPR-0783]], [[BPR-1580]]. View is Home's own, and
			// only while an account can fill the list: absent from the review
			// strip, and absent rather than disabled while signed out, which is
			// how RFC-BPR-0028's rule still holds there. [[BPR-0990]]: on Home
			// the refresh glyph standing with them is the profile's discovery;
			// the pull request's refresh is the review strip's.
			if (HomeViewItem != nullptr) {
				HomeViewItem->setVisible(home && HomeAccount);
			}
			if (GithubRefreshAction != nullptr) {
				GithubRefreshAction->setVisible(!stripHome);
			}
			// [[BPR-1575]]. The freshness follows the surface shown.
			present_refresh();
			if (QueueRefreshAction != nullptr) {
				QueueRefreshAction->setVisible(home && HomeAccount);
			}
			apply_refresh_key(home);
			apply_strip_width();
			// [[BPR-1295]]. Home has no review, so Hide Drawers is disabled.
			apply_drawers_toggle();
			// The toolbar shows and hides a widget action's widget on its
			// next layout; take it now so what the strip shows is settled
			// when this returns.
			if (auto* const layout = PrimaryToolbar->layout(); layout != nullptr) {
				layout->invalidate();
				layout->activate();
			}
		}
		// [[BPR-0775]]. Focus goes back once the strip is settled, so a strip
		// control that had it before Home is shown again to take it back.
		if (leavingHome && HomeFilter != nullptr) {
			// [[BPR-1865]]. A filter is for finding one pull request; coming
			// back finds Home whole.
			HomeFilter->clear_query();
		}
		if (leavingHome) {
			if (FocusBeforeHome != nullptr && FocusBeforeHome->isVisible()) {
				FocusBeforeHome->setFocus(Qt::OtherFocusReason);
			}
			FocusBeforeHome = nullptr;
		}
		// [[BPR-0671]], [[BPR-0775]]. The pull request stack's empty page is not
		// Home's. It stays parented in the hidden pull request column, so what
		// writes to it keeps its shape. A first load in progress and a load
		// refusal are said under the bar, and a session's problem on its host's
		// card.
		if (StatusLabel != nullptr) {
			// update_status_text calls here, so the line is recomputed rather
			// than refreshed through it.
			if (const auto line = status_line(); StatusLabel->text() != line) {
				StatusLabel->setText(line);
			}
		}
		update_discovery_controls();
		// [[BPR-0990]]. Arriving at Home asks discovery to catch up if its list
		// is older than the freshness window; the host decides whether it is.
		// [[BPR-1848]]. Coming back from a pull request names it, so a fresh
		// list reads only that row and an aged one reads it first.
		if (!home && !cycle) {
			if (auto shown = open_queue_ref()) {
				ReviewShownRef = std::move(shown);
			}
		}
		if (arrivedHome) {
			const auto left = std::exchange(ReviewShownRef, std::nullopt);
			if (HomeAccount && left && HomeReturnDispatch) {
				HomeReturnDispatch(*left);
			} else if (HomeAccount && QueueDiscoveryDispatch) {
				QueueDiscoveryDispatch(application::queue_discovery_intent::activate);
			}
		}
		update_open_on_github_action();
		update_window_title();
	}

	auto workbench_window::showing_ai_tools() const -> bool
	{
		return AiToolsSelected && !showing_home()
			&& DrawerStates.Conversations == core::workbench_drawer_state::expanded
			&& Drawers[1].Panel != nullptr && Drawers[1].Panel->isVisible()
			&& Drawers[1].Panel->width() >= 240;
	}

	auto workbench_window::showing_dif_workshop() const -> bool
	{
		// Reuse the editor's exact draft/proposal and conflict gate rather than
		// retaining a second workshop identity just for decoration.
		return !showing_home() && CompositionPage == ComposeInlinePage
			&& ReviewEditor != nullptr && ReviewEditor->isVisible()
			&& ReviewEditor->accept_revision_action()->isEnabled();
	}

	void workbench_window::update_dif_presentation()
	{
		if (WorkshopHeader != nullptr) {
			WorkshopHeader->setVisible(!showing_home() && CompositionPage == ComposeInlinePage
									   && ReviewEditor->shown_proposal().has_value());
		}
		if (HomeLanding == nullptr || !DifResolutionReady) {
			return;
		}
		const auto shown = DifResolution.Preferences.Visibility == core::dif_visibility::shown;
		const auto home  = showing_home();
		// [[BPR-1209]]. Home and the exact offered workshop are prime spots;
		// every other surface uses the 28-pixel status corner.
		const auto card         = home && HomeLanding->state() == landing_state::sign_in;
		const auto cardSlot     = card && HomeLanding->illustration_space_available();
		const auto workshopSlot = showing_dif_workshop();
		const auto cornerSlot   = !card && !workshopSlot;
		const auto cornerShown  = cornerSlot && statusBar()->currentMessage().isEmpty();
		if (Dif == nullptr && shown && isVisible() && (cardSlot || cornerSlot || workshopSlot) && DifCatalog) {
			if (auto cache = DifCatalog->acquire()) {
				auto* const parent = cardSlot ? static_cast<QWidget*>(HomeLanding)
					: cornerSlot              ? static_cast<QWidget*>(this)
											  : DifWorkshopSlot;
				Dif                = new dif_widget{ std::move(cache), parent };
				Dif->setObjectName(QStringLiteral("workbenchDif"));
				Dif->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}
		}
		HomeLanding->set_illustration_enabled(shown && card);
		DifWorkshopSlot->setVisible(shown && workshopSlot && Dif != nullptr);
		DifCornerSlot->setVisible(shown && cornerShown && Dif != nullptr);
		DifCues->set_visible(
			shown && (workshopSlot || cornerShown) && DifResolution.EffectiveMotion == core::motion_mode::full
			&& isActiveWindow() && QApplication::activeModalWidget() == nullptr);
		if (Dif == nullptr) {
			return;
		}
		if (card) {
			if (HomeLanding->illustration() != Dif) {
				HomeLanding->set_illustration(Dif);
			}
		} else if (cornerSlot) {
			if (HomeLanding->illustration() == Dif) {
				static_cast<void>(HomeLanding->take_illustration());
			}
			if (Dif->parentWidget() != this) {
				Dif->setParent(this);
			}
			Dif->setFixedSize(dif_corner_size, dif_corner_size);
			place_dif_corner();
		} else if (workshopSlot) {
			if (HomeLanding->illustration() == Dif) {
				static_cast<void>(HomeLanding->take_illustration());
			}
			if (Dif->parentWidget() != DifWorkshopSlot) {
				Dif->setParent(DifWorkshopSlot);
				DifWorkshopSlot->layout()->addWidget(Dif);
				DifWorkshopSlot->layout()->setAlignment(Dif, Qt::AlignCenter);
			}
			Dif->setFixedSize(dif_workshop_size, dif_workshop_size);
			Dif->show();
		} else {
			Dif->hide();
		}
		Dif->set_eligibility({
			.Shown      = shown && (cardSlot || cornerShown || workshopSlot),
			.FullMotion = DifResolution.EffectiveMotion == core::motion_mode::full,
			.Active     = isActiveWindow(),
			.Modal      = QApplication::activeModalWidget() != nullptr,
			.Suspended  = false,
		});
		Dif->set_home(home);
	}

	auto workbench_window::home_action() const noexcept -> QAction*
	{
		return HomeAction;
	}

	void workbench_window::choose_home(const bool aHome)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		// Asked for, Home shows over an open review until a modality or a
		// pull request brings the review back; with nothing open Home is
		// simply where the window is, and cannot be left.
		HomeRequested = aHome;
		update_home_landing();
	}

	auto workbench_window::home_page() const noexcept -> QWidget*
	{
		return HomePage;
	}

	auto workbench_window::open_project_action() const noexcept -> QAction*
	{
		return OpenProjectAction;
	}

	void workbench_window::adopt_local_cycle(review_cycle_view* const aView)
	{
		if (LocalCyclePage != nullptr || aView == nullptr || CentralStack == nullptr) {
			return;
		}
		LocalCyclePage = aView;
		CentralStack->addWidget(static_cast<QWidget*>(aView));
	}

	auto workbench_window::local_cycle() const noexcept -> review_cycle_view*
	{
		return LocalCyclePage;
	}

	void workbench_window::show_local_cycle(const QString& aTitle)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		LocalCycleTitle = aTitle;
		LocalCycleShown = true;
		HomeRequested   = false;
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::retitle_local_cycle(const QString& aTitle)
	{
		if (!LocalCycleShown || LocalCycleTitle == aTitle) {
			return;
		}
		LocalCycleTitle = aTitle;
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::leave_local_cycle()
	{
		if (!LocalCycleShown) {
			return;
		}
		LocalCycleShown = false;
		LocalCycleTitle.clear();
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
	}

	auto workbench_window::showing_local_cycle() const noexcept -> bool
	{
		return LocalCycleShown && CentralStack != nullptr && LocalCyclePage != nullptr && CentralStack->currentWidget() == static_cast<QWidget*>(LocalCyclePage);
	}

	void workbench_window::present_local_cycle_recents(const std::vector<application::review_cycle_recent>& aRecents)
	{
		if (CycleRecents == aRecents) {
			return;
		}
		CycleRecents = aRecents;
		if (HomeCyclesList == nullptr) {
			return;
		}
		HomeCyclesList->clear();
		for (const auto& recent : CycleRecents) {
			const auto  title    = QString::fromUtf8(recent.Cycle.Title.data(), static_cast<qsizetype>(recent.Cycle.Title.size()));
			const auto  location = QString::fromUtf8(recent.Location.data(), static_cast<qsizetype>(recent.Location.size()));
			auto* const item     = new QListWidgetItem{ location.isEmpty() ? title : QStringLiteral("%1  —  %2").arg(title, location), HomeCyclesList };
			item->setToolTip(location);
		}
		HomeCyclesList->setVisible(!CycleRecents.empty());
		update_home_surface();
		if (HomeFilter != nullptr && !HomeFilter->query().isEmpty()) {
			apply_home_filter();
		}
	}

	void workbench_window::set_local_cycle_entry_dispatch(std::function<void()> aOpenProject, std::function<void(const core::local_cycle_subject&)> aResume)
	{
		OpenProjectDispatch = std::move(aOpenProject);
		ResumeCycleDispatch = std::move(aResume);
		if (OpenProjectAction != nullptr) {
			OpenProjectAction->setEnabled(static_cast<bool>(OpenProjectDispatch));
		}
	}

	auto workbench_window::home_cycles_list() const noexcept -> QListWidget*
	{
		return HomeCyclesList;
	}

	void workbench_window::apply_refresh_key(const bool aHome)
	{
		// [[BPR-1600]]. One window shows Home or a pull request, and the Refresh
		// key does the refresh that is showing. The key is on one action at a
		// time, taken off before it is put on, so no press is ever ambiguous.
		if (GithubRefreshAction == nullptr || QueueRefreshAction == nullptr) {
			return;
		}
		auto* const from = aHome ? GithubRefreshAction : QueueRefreshAction;
		auto* const to   = aHome ? QueueRefreshAction : GithubRefreshAction;
		from->setShortcuts({ });
		to->setShortcuts(shortcut_keys(aHome ? shortcut_command::home_refresh : shortcut_command::refresh));
		// [[BPR-1865]]. Find's key, the same way: Home's filter while Home
		// shows, the file's Find otherwise.
		if (HomeFilterAction != nullptr && SourceFindActions[0] != nullptr) {
			auto* const findFrom = aHome ? SourceFindActions[0] : HomeFilterAction;
			auto* const findTo   = aHome ? HomeFilterAction : SourceFindActions[0];
			findFrom->setShortcuts({ });
			findTo->setShortcuts(shortcut_keys(aHome ? shortcut_command::home_filter : shortcut_command::find));
		}
	}

	auto workbench_window::showing_home() const -> bool
	{
		return CentralStack != nullptr && CentralStack->currentWidget() == HomePage;
	}

	void workbench_window::place_home_landing()
	{
		if (HomeLanding == nullptr || HomeColumn == nullptr || HomeSetupRow == nullptr) {
			return;
		}
		const auto state = HomeLanding->state();
		// [[BPR-1865]]. Over a review the card is filtered with the lists; the
		// sign-in and first-launch cards are not rows and always stand.
		HomeLanding->setVisible((state != landing_state::choose_pull_request || HomeLandingHold > 0)
								&& !(HomeLandingFiltered && state == landing_state::back_to_review));
		// [[BPR-1843]]. Over a review the Active review card follows the bar
		// and stands above every list, as the first-launch card does signed
		// out. The bar keeps its whole width and never moves ([[BPR-1381]]).
		// [[BPR-1865]]. Under the filter, which stands above all it filters.
		const auto anchor = [this] {
			return HomeFilter != nullptr ? HomeColumn->indexOf(HomeFilter) : HomeColumn->indexOf(HomeSetupRow);
		};
		const auto wanted = anchor() + 1;
		if (HomeColumn->indexOf(HomeLanding) == wanted) {
			return;
		}
		if (HomeOpenRow != nullptr) {
			HomeOpenRow->removeWidget(HomeLanding);
		}
		HomeColumn->removeWidget(HomeLanding);
		HomeColumn->insertWidget(anchor() + 1, HomeLanding);
	}

	void workbench_window::focus_home_bar()
	{
		if (HomeOpenBar == nullptr) {
			return;
		}
		if (!showing_home()) {
			choose_home(true);
		}
		HomeOpenBar->setFocus(Qt::ShortcutFocusReason);
		HomeOpenBar->selectAll();
	}

	void workbench_window::place_dif_corner()
	{
		if (Dif == nullptr || DifCornerSlot == nullptr || Dif->parentWidget() != this) {
			return;
		}
		// The slot is hidden while a prime spot holds Dif, and while the
		// status bar shows a transient message over its ordinary widgets; the
		// sprite follows it rather than covering that message.
		if (!DifCornerSlot->isVisible()) {
			Dif->hide();
			return;
		}
		const auto bottomLeft = DifCornerSlot->mapTo(this, QPoint{ 0, DifCornerSlot->height() });
		Dif->move(bottomLeft.x(), bottomLeft.y() - dif_corner_size);
		// Home's rows end above the part of the sprite that rises over the
		// status bar, whatever height the platform gives that bar (review of
		// BPR-0775, finding 6).
		if (HomeColumn != nullptr) {
			const auto inset = std::max(0, dif_corner_size - statusBar()->height()) + 4;
			if (HomeColumn->contentsMargins().bottom() != inset) {
				HomeColumn->setContentsMargins(0, 0, 0, inset);
			}
		}
		Dif->raise();
		Dif->show();
	}

	void workbench_window::begin_enterprise_server_from_home()
	{
		open_sign_in();
		// The same draft the page's own button opens, under the same readiness.
		open_enterprise_server_draft();
	}

	void workbench_window::begin_github_cli_from_home()
	{
		if (SessionPanel != nullptr && !SessionPanel->connect_enabled()) {
			return;
		}
		open_sign_in({ }, true);
		if (GithubCliDiscoveryDispatch && GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
			GithubCliDiscoveryDispatch();
		}
	}

	auto workbench_window::session_signed_in() const -> bool
	{
		if (SessionPanel == nullptr) {
			return false;
		}
		const auto status = SessionPanel->status();
		return status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
	}

	auto workbench_window::home_session_line() const -> QString
	{
		// [[BPR-0775]]. RFC-BPR-0032's session line. The session projection
		// carries one account on one host and no refresh time, so the line says
		// that much and no more.
		const auto host = SessionPanel != nullptr
			? QString::fromStdString(SessionPanel->active_host_text())
			: QString{ };
		if (session_signed_in() && !PublicPreviewSignedOut) {
			// The accepted session's login. The strip's account text is a label
			// ("Not authenticated" until the account row learns its login), not
			// the login itself.
			const auto& session = SessionPanel->session();
			const auto  login   = session.has_value() ? QString::fromStdString(session->Login) : QString{ };
			if (!login.isEmpty()) {
				return host.isEmpty() ? QStringLiteral("Signed in as %1.").arg(login)
									  : QStringLiteral("Signed in as %1 on %2.").arg(login, host);
			}
			return host.isEmpty() ? QStringLiteral("Signed in.")
								  : QStringLiteral("Signed in on %1.").arg(host);
		}
		if (SessionPanel != nullptr && SessionPanel->status() == core::github_status::establishing_session) {
			return host.isEmpty() ? QStringLiteral("Connecting.")
								  : QStringLiteral("Connecting to %1.").arg(host);
		}
		return QStringLiteral("Not signed in. Public pull requests open read-only.");
	}

	auto workbench_window::dif_corner_slot() const noexcept -> QWidget*
	{
		return DifCornerSlot;
	}

	auto workbench_window::home_open_label() const noexcept -> QLabel*
	{
		return HomeOpenLabel;
	}

	auto workbench_window::home_open_frame() const noexcept -> QWidget*
	{
		return HomeOpenFrame;
	}

	auto workbench_window::home_filter_box() const noexcept -> home_filter_field*
	{
		return HomeFilter;
	}

	auto workbench_window::home_filter_action() const noexcept -> QAction*
	{
		return HomeFilterAction;
	}

	auto workbench_window::home_landing_matches() const -> bool
	{
		if (HomeFilter == nullptr || IdentityOwner.isEmpty() || IdentityRepository.isEmpty()) {
			return false;
		}
		const auto candidate = core::home_filter_candidate{
			.Number     = IdentityNumber.toLongLong(),
			.Title      = OpenQueueTitle,
			.Repository = QStringLiteral("%1/%2").arg(IdentityOwner, IdentityRepository).toStdString(),
		};
		return core::match_home_filter(HomeFilter->query().toStdString(), candidate).has_value();
	}

	void workbench_window::apply_home_filter()
	{
		if (HomeFilter == nullptr || HomeList == nullptr) {
			return;
		}
		const auto query  = HomeFilter->query();
		const auto active = core::home_filter_active(query.toStdString());
		HomeList->set_filter(query);
		// The Active review card and Review Cycles are filtered with the list:
		// each is shown while it matches, and counted as a match.
		auto elsewhere      = 0;
		HomeLandingFiltered = active && !home_landing_matches();
		if (active && !HomeLandingFiltered && HomeLanding != nullptr && HomeLanding->state() == landing_state::back_to_review) {
			++elsewhere;
		}
		if (HomeCyclesList != nullptr && HomeCycles != nullptr) {
			auto cycles = 0;
			for (auto row = 0; row < HomeCyclesList->count() && static_cast<std::size_t>(row) < CycleRecents.size(); ++row) {
				const auto& recent  = CycleRecents[static_cast<std::size_t>(row)];
				const auto  matched = !active || core::match_home_filter(query.toStdString(), core::home_filter_candidate{ .Title = std::string{ recent.Cycle.Title }, .Repository = recent.Location }).has_value();
				HomeCyclesList->setRowHidden(row, !matched);
				cycles += active && matched ? 1 : 0;
			}
			HomeCycles->setVisible(!active || cycles > 0);
			elsewhere += cycles;
		}
		HomeList->set_filter_elsewhere(elsewhere);
		HomeFilter->set_count(HomeList->filter_matches() + elsewhere);
		update_home_landing();
	}

	auto workbench_window::home_focus_bar_action() const noexcept -> QAction*
	{
		return HomeFocusBarAction;
	}

	auto workbench_window::status_label() const noexcept -> QLabel*
	{
		return StatusLabel;
	}

	auto workbench_window::account_action() const noexcept -> QAction*
	{
		return AccountAction;
	}

	auto workbench_window::identity_label() const noexcept -> QLabel*
	{
		return IdentityLabel;
	}

	auto workbench_window::change_position_label() const noexcept -> QLabel*
	{
		return ChangePositionLabel;
	}

	auto workbench_window::file_position_label() const noexcept -> QLabel*
	{
		return FilePositionLabel;
	}

	void workbench_window::update_strip_positions()
	{
		if (FilePositionLabel == nullptr || ChangePositionLabel == nullptr) {
			return;
		}
		// [[BPR-1385]]. Native glyph widths and digit boundaries must not move
		// the arrow hit targets. Both positions reserve four digits per number
		// (three in compact mode), even without a selection. Larger positions elide with
		// their complete words retained in the tooltip and accessible description.
		const auto present = [this](QLabel& aLabel, const QString& aName, const QString& aText, const QString& aWords) {
			const auto metrics   = aLabel.fontMetrics();
			const auto textWidth = review_position_text_width(metrics, aName, ReviewBarPositionsShown, ReviewBarCompact);
			const auto margins   = aLabel.contentsMargins();
			aLabel.setFixedWidth(textWidth + margins.left() + margins.right());
			aLabel.setText(metrics.elidedText(ReviewBarPositionsShown ? aText : aName, Qt::ElideRight, textWidth));
			aLabel.setToolTip(aWords);
			aLabel.setAccessibleDescription(aWords);
		};
		// [[BPR-1202]]. The group names remain visible even with no selection.
		// Narrow bars shorten the position, keeping its label before the arrows.
		auto fileText  = QStringLiteral("File");
		auto fileWords = QStringLiteral("No actionable file selected");
		if (Cursor && Cursor->ActionableCount > 0U) {
			fileWords = Cursor->Ordinal > 0U
				? QStringLiteral("File %1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QStringLiteral("Files %1").arg(Cursor->ActionableCount);
			fileText  = ReviewBarCompact && Cursor->Ordinal > 0U
				? QStringLiteral("File %1/%2").arg(Cursor->Ordinal).arg(Cursor->ActionableCount)
				: fileWords;
		}
		present(*FilePositionLabel, QStringLiteral("File"), fileText, fileWords);

		// Derived from the comparison the surface holds, exactly as the host
		// derives the change cursor it observes for the core: the group the
		// selection names, counted among the alignment's groups. No second
		// authority, and nothing when the selection is for another comparison.
		auto changeText  = QStringLiteral("Changes");
		auto changeWords = QStringLiteral("No comparison selected");
		if (ComparisonSurface != nullptr) {
			const auto& result    = ComparisonSurface->current_result();
			const auto& selection = ComparisonSurface->selection();
			if (result != nullptr) {
				const auto& groups  = result->Alignment.Groups;
				auto        ordinal = std::size_t{ 0U };
				if (selection.has_value() && selection->Group.has_value()
					&& selection->Comparison == result->Alignment.Digest) {
					const auto found = std::ranges::find(
						groups, *selection->Group, &core::change_group::Id);
					if (found != groups.end()) {
						ordinal = static_cast<std::size_t>(
							std::ranges::distance(groups.begin(), found) + 1);
					}
				}
				changeWords = ordinal > 0U
					? QStringLiteral("Changes %1 of %2").arg(ordinal).arg(groups.size())
					: QStringLiteral("Changes %1").arg(groups.size());
				changeText  = ReviewBarCompact && ordinal > 0U
					? QStringLiteral("Changes %1/%2").arg(ordinal).arg(groups.size())
					: changeWords;
			}
		}
		present(*ChangePositionLabel, QStringLiteral("Changes"), changeText, changeWords);
		update_idle_direction();
	}

	// [[BPR-1287]]. The owner, 2026-10-06: "this is what happens when you go
	// to Read without a file open. We need to not show a blank diff here and
	// instead have some clear direction to open a file." The comparison area
	// says so and offers the window's own gestures: Next file, which from no
	// file opens the first, and the Files drawer.
	void workbench_window::update_idle_direction()
	{
		if (ComparisonSurface == nullptr || Navigation[1].Action == nullptr) {
			return;
		}
		const auto actionable = Cursor.has_value() ? Cursor->ActionableCount : std::size_t{ 0U };
		const auto next       = Navigation[1].Action->shortcut().toString(QKeySequence::NativeText);
		auto       direction  = diff_workbench::idle_direction{ };
		if (Cursor.has_value() && actionable == 0U) {
			direction.Title  = QStringLiteral("No files to read");
			direction.Detail = QStringLiteral("This pull request has no file this comparison can show.");
		} else {
			direction.Title     = QStringLiteral("No file open");
			direction.Detail    = actionable > 0U
				? QStringLiteral("Choose one of the %1 files in Files, or open the first one. %2 opens the next file.").arg(actionable).arg(next)
				: QStringLiteral("Choose a file in Files, or open the first one. %1 opens the next file.").arg(next);
			direction.OpenFirst = [this] {
				if (Navigation[1].Action->isEnabled()) {
					Navigation[1].Action->trigger();
				}
			};
		}
		direction.ShowFiles = [this] {
			if (Drawers[0].Panel != nullptr && Drawers[0].Panel->state() != core::workbench_drawer_state::expanded && Drawers[0].Toggle != nullptr) {
				Drawers[0].Toggle->trigger();
			}
			if (FilesTree != nullptr) {
				FilesTree->setFocus(Qt::OtherFocusReason);
			}
		};
		ComparisonSurface->set_idle_direction(std::move(direction));
		ComparisonSurface->idle_open_first_button()->setEnabled(Navigation[1].Action->isEnabled());
	}

	void workbench_window::update_strip_identity()
	{
		// [[BPR-1864]]. The window's title names what the strip names.
		update_window_title();
		if (IdentityLabel == nullptr) {
			return;
		}
		update_open_on_github_action();
		// [[BPR-0722]]. The identity is the only in-window statement of which
		// pull request is open (the macOS title bar is hidden), so it yields
		// last. By width it sheds the title, then the owner; the number stays
		// as far down as it has room (apply_strip_width decides that). Nothing
		// here folds into an overflow menu.
		const auto& metrics = IdentityLabel->fontMetrics();
		// What the other title row items leave: the toolbar folds anything
		// past its width into the extension button, so the name never asks
		// for more than that remainder.
		const auto margins = IdentityLabel->contentsMargins();
		const auto room    = std::max(strip_identity_budget() - margins.left() - margins.right(), 0);
		auto       text    = QString{ };
		if (LocalCycleShown && !showing_home()) {
			// [[BPR-1157]]. A local cycle is named by its title and badged as
			// local: no repository, number or account.
			const auto badge = QStringLiteral("  ·  Local cycle");
			text             = metrics.elidedText(LocalCycleTitle, Qt::ElideRight, std::max(room - metrics.horizontalAdvance(badge), identity_fragment_width)) + badge;
			IdentityLabel->setText(text);
			IdentityLabel->setToolTip(LocalCycleTitle + badge);
			return;
		}
		// [[BPR-1842]]. While nothing is accepted, the name GitHub's first
		// answer gave stands in, in the same forms.
		const auto  loading     = IdentityText.isEmpty() && LoadingIdentity.has_value();
		const auto& shownText   = loading ? LoadingIdentity->Text : IdentityText;
		const auto& shownOwner  = loading ? LoadingIdentity->Owner : IdentityOwner;
		const auto& shownRepo   = loading ? LoadingIdentity->Repository : IdentityRepository;
		const auto& shownNumber = loading ? LoadingIdentity->Number : IdentityNumber;
		if (!shownText.isEmpty()) {
			// [[BPR-1129]]. The name has the room the navigation took, so the
			// forms follow the room rather than fixed widths: the whole name
			// with its title elided at the right, then owner/repository with
			// the number, then the repository, then a fragment of it; the
			// number stays in every form.
			const auto number = QStringLiteral("  #%1").arg(shownNumber);
			const auto owned  = QStringLiteral("%1/%2").arg(shownOwner, shownRepo) + number;
			const auto titled = metrics.horizontalAdvance(owned) + identity_title_minimum;
			const auto bare   = shownRepo + number;
			// [[BPR-1251]]. The title also needs the room it would have beside
			// the whole modality (apply_strip_width), or it could yield and
			// then return when the shortcuts go. [[BPR-1324]]. So does the
			// owner: with four modalities it yielded on Windows above the width
			// where the shortcuts go, and came back below it.
			const auto wholeRoom  = IdentityTitleBudget - margins.left() - margins.right();
			const auto ownedWidth = metrics.horizontalAdvance(owned);
			if (room >= titled && wholeRoom >= titled) {
				text = metrics.elidedText(shownText, Qt::ElideRight, room);
			} else if (room >= ownedWidth && wholeRoom >= ownedWidth) {
				text = owned;
			} else if (room >= metrics.horizontalAdvance(bare)) {
				text = bare;
			} else {
				const auto budget = room - metrics.horizontalAdvance(number);
				text              = metrics.elidedText(shownRepo, Qt::ElideMiddle, std::max(budget, identity_fragment_width)) + number;
			}
		}
		IdentityLabel->setText(text);
		IdentityLabel->setToolTip(shownText);
	}

	auto workbench_window::strip_identity_budget() const -> int
	{
		if (PrimaryToolbar == nullptr || IdentityLabel == nullptr || IdentityAction == nullptr) {
			return 0;
		}
		// [[BPR-0722]]. The toolbar folds into its extension button exactly
		// when its size hint exceeds its width, so the identity's room is
		// the width less what every other visible item asks for: the hint
		// with the identity's own current hint taken back out. A small
		// slack covers the toolbar's rounding; the floor leaves no other.
		constexpr int slack    = 12;
		const auto    identity = IdentityAction->isVisible() ? IdentityLabel->sizeHint().width() : 0;
		return width() - (PrimaryToolbar->sizeHint().width() - identity) - slack;
	}

	auto workbench_window::strip_identity_minimum() const -> int
	{
		if (IdentityLabel == nullptr) {
			return 0;
		}
		// The number, a short repository fragment and the label's margins.
		const auto  margins = IdentityLabel->contentsMargins();
		const auto& number  = IdentityText.isEmpty() && LoadingIdentity.has_value() ? LoadingIdentity->Number : IdentityNumber;
		return IdentityLabel->fontMetrics().horizontalAdvance(QStringLiteral("  #%1").arg(number))
			+ identity_fragment_width + margins.left() + margins.right();
	}

	void workbench_window::update_account_action()
	{
		if (AccountAction == nullptr || SessionPanel == nullptr) {
			return;
		}
		const auto status   = SessionPanel->status();
		const auto signedIn = status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		const auto account     = QString::fromStdString(SessionPanel->active_account_text());
		const auto description = signedIn
			? QStringLiteral("Signed in as %1. Open the accounts and hosts settings.")
				  .arg(account)
			: QStringLiteral("Sign in with a device code.");
		AccountAction->setText(signedIn ? account : QStringLiteral("Sign in"));
		AccountAction->setToolTip(description);
		AccountAction->setStatusTip(description);
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(AccountAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Account"));
			button->setAccessibleDescription(description);
			button->setFocusPolicy(Qt::StrongFocus);
		}
	}

	auto workbench_window::previous_file_action() const noexcept -> QAction*
	{
		return Navigation[0].Action;
	}

	auto workbench_window::next_file_action() const noexcept -> QAction*
	{
		return Navigation[1].Action;
	}

	auto workbench_window::header_source_action() const noexcept -> QAction*
	{
		return HeaderSourceAction;
	}

	void workbench_window::set_header_source_dispatch(std::function<void()> aDispatch)
	{
		HeaderSourceDispatch = std::move(aDispatch);
		update_header_source_action();
		update_source_find_actions();
	}

	void workbench_window::present_header_source_result(const core::header_source_resolution aResult)
	{
		auto message = QString{ };
		switch (aResult.Status) {
		case core::header_source_status::matched:
			ComparisonSurface->focus_source();
			return;
		case core::header_source_status::no_match:
			message = aResult.SelectedFamily == core::header_source_family::source ? tr("No matching header in this review") : tr("No matching source file in this review");
			break;
		case core::header_source_status::ambiguous:
			message = tr("More than one equally close header or source file matches in this review");
			break;
		case core::header_source_status::unavailable_catalogue:
			message = reason_text(core::workbench_reason_code::header_source_unavailable_catalogue);
			break;
		case core::header_source_status::unsupported_file:
			message = reason_text(core::workbench_reason_code::header_source_unsupported_file);
			break;
		case core::header_source_status::no_selection:
			message = reason_text(core::workbench_reason_code::header_source_no_selection);
			break;
		}
		statusBar()->showMessage(message, bounded_navigation_message_ms);
		QAccessibleAnnouncementEvent announcement{ this, message };
		announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&announcement);
	}

	void workbench_window::set_pr_search_dispatch(std::function<void(source_find_gesture)> aDispatch)
	{
		PrSearchDispatch = std::move(aDispatch);
		connect(FileSearchTabs, &QTabWidget::currentChanged, this, [this](int aIndex) {if(aIndex==1 && PrSearchDispatch){PrSearchDispatch(source_find_gesture::document_changed);} });
		SearchPanel->Dispatch = [this](auto aGesture) {if(PrSearchDispatch){PrSearchDispatch(aGesture);}if(aGesture==source_find_gesture::close){FileSearchTabs->setCurrentIndex(SearchPreviousTab);if(SearchWasCollapsed && Dispatch){Dispatch(core::set_workbench_drawer_intent{core::workbench_drawer::files,core::workbench_drawer_state::collapsed});}ComparisonSurface->focus_source();}update_source_find_actions(); };
		update_source_find_actions();
	}

	auto workbench_window::pr_search() const -> source_search_panel* { return SearchPanel; }

	void workbench_window::update_source_find_actions()
	{
		update_go_to_line_action();
		if (!SourceFindActions[0] || !ComparisonSurface) {
			return;
		}
		auto*      focused     = QApplication::focusWidget();
		const bool sourceFocus = focused == ComparisonSurface->left_pane() || focused == ComparisonSurface->right_pane()
			|| (focused && focused->objectName() == QStringLiteral("diffUnifiedSource"));
		const bool queryFocus = focused && ComparisonSurface->source_find()->isAncestorOf(focused);
		const bool textFocus  = qobject_cast<QLineEdit*>(focused) || qobject_cast<QPlainTextEdit*>(focused) || qobject_cast<QTextEdit*>(focused);

		const auto filesCommands = core::project_source_files_commands({ .PullRequest = HasGithubPullRequest && !showing_home() && !LocalCycleShown && static_cast<bool>(PrSearchDispatch), .Modal = QApplication::activeModalWidget() != nullptr, .Editor = textFocus && !sourceFocus && !queryFocus && !(focused && SearchPanel && SearchPanel->isAncestorOf(focused)), .Query = SearchPanel && !SearchPanel->projection().Query.empty(), .Hits = SearchPanel && SearchPanel->projection().CurrentMatch.has_value() });
		for (std::size_t index = 0; index < SourceFilesActions.size(); ++index) {
			if (SourceFilesActions[index]) {
				describe_action(SourceFilesActions[index], filesCommands[index], index == 0 ? tr("Search full changed files in the active comparison") : tr("Open the next exact search occurrence"), tr("Open a pull request and leave modal dialogs"));
			}
		}
		const auto commands = core::project_source_find_commands({ .Reader         = !showing_home() && !LocalCycleShown && !CompositionPage && current_modality() == review_modality::review,
																   .Material       = ComparisonSurface->source_find_available(),
																   .EditorOwnsFind = textFocus && !sourceFocus && !queryFocus,
																   .Modal          = QApplication::activeModalWidget() != nullptr,
																   .Query          = !ComparisonSurface->source_find()->query().empty(),
																   .Hits           = ComparisonSurface->source_find_navigable() });
		for (std::size_t index = 0; index < SourceFindActions.size(); ++index) {
			describe_action(SourceFindActions[index], commands[index], index == 0U ? tr("Find in the focused source pane") : tr("Traverse source matches"), tr("Focus an available source reader in Review"));
		}
	}

	void workbench_window::update_header_source_action()
	{
		if (HeaderSourceAction == nullptr) {
			return;
		}
		const auto  projection = command(core::workbench_command_id::go_to_header_source);
		const auto* focused    = QApplication::focusWidget();
		const auto* line       = qobject_cast<const QLineEdit*>(focused);
		const auto  editing    = focused != nullptr && isAncestorOf(focused) && (editable_multi_line(focused) || (line != nullptr && !line->isReadOnly()));
		const auto  inReview   = !showing_home() && !LocalCycleShown && CompositionPage == nullptr && current_modality() == review_modality::review;
		const auto  reason     = !inReview ? reason_text(core::workbench_reason_code::header_source_requires_review)
			: editing                      ? tr("Keep Option/Alt text input in the editor. Focus the source comparison to navigate.")
										   : reason_text(projection ? projection->DisabledReason : core::workbench_reason_code::header_source_no_selection);
		// [[BPR-1815]]. The review bar presents it, so its tooltip names its
		// key first, as the arrows beside it do.
		const auto enabled = inReview && !editing && projection && projection->Enabled && static_cast<bool>(HeaderSourceDispatch);
		const auto purpose = tr("Go to the closest matching header or source file in this review");
		describe_navigation_action(HeaderSourceAction, tr("Go to header/source"), enabled, purpose, enabled ? purpose : reason, presented_key(HeaderSourceAction));
		if (auto* const button = ReviewBar != nullptr ? qobject_cast<QToolButton*>(ReviewBar->widgetForAction(HeaderSourceAction)) : nullptr;
			button != nullptr && button->accessibleDescription() != HeaderSourceAction->toolTip()) {
			button->setAccessibleDescription(HeaderSourceAction->toolTip());
		}
	}

	auto workbench_window::previous_change_action() const noexcept -> QAction*
	{
		return Navigation[2].Action;
	}

	auto workbench_window::next_change_action() const noexcept -> QAction*
	{
		return Navigation[3].Action;
	}

	auto workbench_window::first_change_action() const noexcept -> QAction*
	{
		return ChangeEnds[0].Action;
	}

	auto workbench_window::last_change_action() const noexcept -> QAction*
	{
		return ChangeEnds[1].Action;
	}

	void workbench_window::update_change_end_keys()
	{
		const auto* const focused = QApplication::focusWidget();
		const auto        inside  = focused != nullptr && isAncestorOf(focused);
		const auto        keys    = change_end_keys();
		for (std::size_t index = 0U; index < ChangeEnds.size(); ++index) {
			if (ChangeEnds[index].Action == nullptr) {
				continue;
			}
			const auto yields = inside && (takes_typed_text(focused) || binds_key_nearer(focused, this, keys[index]));
			ChangeEnds[index].Action->setShortcut(yields ? QKeySequence{ } : keys[index]);
		}
		describe_change_ends();
	}

	void workbench_window::describe_change_ends()
	{
		for (const auto& [entry, id] : { std::pair{ &ChangeEnds[0], core::workbench_command_id::first_change }, std::pair{ &ChangeEnds[1], core::workbench_command_id::last_change } }) {
			if (entry->Action == nullptr) {
				continue;
			}
			const auto projection = command(id);
			const auto enabled    = projection && projection->Enabled;
			const auto shown      = !projection ? core::workbench_reason_code::no_active_workbench
				: enabled                       ? projection->Bound
												: projection->DisabledReason;
			describe_navigation_action(entry->Action, entry->Name, enabled, entry->Purpose, change_reason_text(shown));
		}
	}

	auto workbench_window::seen_action() const noexcept -> QAction*
	{
		return ReceiptActions[0];
	}

	auto workbench_window::read_action() const noexcept -> QAction*
	{
		return ReceiptActions[1];
	}

	auto workbench_window::accept_action() const noexcept -> QAction*
	{
		return ReceiptActions[2];
	}

	auto workbench_window::exclude_action() const noexcept -> QAction*
	{
		return ReceiptActions[3];
	}

	auto workbench_window::review_controls_button() const noexcept
		-> QToolButton*
	{
		return ReviewControlsButton;
	}

	auto workbench_window::review_controls_menu() const noexcept -> QMenu*
	{
		return ReviewControlsMenu;
	}

	auto workbench_window::menu_bar() const noexcept -> QMenuBar*
	{
		return PrimaryMenuBar;
	}

	auto workbench_window::toggle_status_drawer_action() const noexcept -> QAction*
	{
		return ToggleStatusDrawerAction;
	}

	auto workbench_window::view_menu() const noexcept -> QMenu*
	{
		return ViewMenu;
	}

	auto workbench_window::line_numbers_action() const noexcept -> QAction*
	{
		return LineNumbersAction;
	}

	auto workbench_window::line_details_action() const noexcept -> QAction*
	{
		return LineDetailsAction;
	}

	auto workbench_window::one_pane_action() const noexcept -> QAction*
	{
		return OnePaneAction;
	}

	auto workbench_window::zoom_in_action() const noexcept -> QAction*
	{
		return ZoomInAction;
	}

	auto workbench_window::zoom_out_action() const noexcept -> QAction*
	{
		return ZoomOutAction;
	}

	auto workbench_window::actual_size_action() const noexcept -> QAction*
	{
		return ActualSizeAction;
	}

	void workbench_window::set_code_zoom_dispatch(std::function<void(core::code_zoom_step)> aDispatch)
	{
		CodeZoomDispatch = std::move(aDispatch);
		apply_code_zoom();
	}

	void workbench_window::dispatch_code_zoom(const core::code_zoom_step aStep)
	{
		if (CodeZoomDispatch && CodeZoom && core::code_zoom_steps_from(*CodeZoom, aStep)) {
			CodeZoomDispatch(aStep);
		}
	}

	void workbench_window::adopt_code_zoom(const core::code_font_settings& aCode)
	{
		const auto moved = CodeZoom && CodeZoom->ZoomPercent != aCode.ZoomPercent;
		CodeZoom         = aCode;
		apply_code_zoom();
		// [[BPR-1805]] repair. A tour stop sets its code in the profile's
		// code font, as comments do.
		if (TourEditor != nullptr) {
			TourEditor->set_code_font(to_presentation_font(aCode));
		}
		if (moved && ComparisonSurface != nullptr) {
			ComparisonSurface->show_code_zoom(aCode.ZoomPercent, core::zoomed_point_size(aCode));
		}
	}

	/// \brief [[BPR-1336]]. Each zoom command's state and the reason for it.
	///
	/// \details The core says whether a step changes what is shown and what it
	/// leads to; the words here only report it. Before the profile's code
	/// record arrives there is nothing to step from.
	void workbench_window::apply_code_zoom()
	{
		if (ZoomInAction == nullptr) {
			return;
		}
		const auto size = [](const std::uint32_t aCentipoints) {
			return point_size_text(aCentipoints) + QStringLiteral(" pt");
		};
		if (!CodeZoom || !CodeZoomDispatch) {
			const auto reason = QStringLiteral("The code size is not loaded yet.");
			for (auto* const action : { ZoomInAction, ZoomOutAction, ActualSizeAction }) {
				describe_action(action, false, { }, reason);
			}
			return;
		}
		const auto& code    = *CodeZoom;
		const auto  shown   = size(core::zoomed_point_size(code));
		const auto  towards = [&](const core::code_zoom_step aStep) {
			auto next        = code;
			next.ZoomPercent = core::stepped_code_zoom(code, aStep);
			return QStringLiteral("%1% (%2)").arg(next.ZoomPercent).arg(size(core::zoomed_point_size(next)));
		};
		describe_action(ZoomInAction, core::code_zoom_steps_from(code, core::code_zoom_step::in),
						QStringLiteral("Show the code in the comparison larger: %1.").arg(towards(core::code_zoom_step::in)),
						QStringLiteral("The code in the comparison is as large as it goes: %1% (%2).").arg(code.ZoomPercent).arg(shown));
		describe_action(ZoomOutAction, core::code_zoom_steps_from(code, core::code_zoom_step::out),
						QStringLiteral("Show the code in the comparison smaller: %1.").arg(towards(core::code_zoom_step::out)),
						QStringLiteral("The code in the comparison is as small as it goes: %1% (%2).").arg(code.ZoomPercent).arg(shown));
		const auto own = size(code.Font.PointSizeCentipoints);
		describe_action(ActualSizeAction, core::code_zoom_steps_from(code, core::code_zoom_step::actual_size),
						QStringLiteral("Show the code in the comparison at your code size, %1.").arg(own),
						QStringLiteral("The code in the comparison is at your code size, %1.").arg(own));
	}

	auto workbench_window::files_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[0].Panel;
	}

	auto workbench_window::files_tree() const noexcept -> files_tree_view*
	{
		return FilesTree;
	}

	auto workbench_window::files_order_selector() const noexcept -> QComboBox*
	{
		return FilesOrderSelector;
	}

	auto workbench_window::files_filter() const noexcept -> QLineEdit*
	{
		return FilesFilter;
	}

	auto workbench_window::files_filter_notice() const noexcept -> filter_empty_notice*
	{
		return FilesFilterNotice;
	}

	auto workbench_window::repositories_filter() const noexcept -> QLineEdit*
	{
		return RepositoriesFilter;
	}

	auto workbench_window::repositories_filter_notice() const noexcept -> filter_empty_notice*
	{
		return RepositoriesFilterNotice;
	}

	void workbench_window::refresh_filter_notices()
	{
		// An empty state is a filter that found nothing among rows that exist;
		// an empty drawer with no filter says nothing new.
		if (FilesFilterNotice != nullptr && FilesTree != nullptr && FilesFilter != nullptr) {
			const auto filter = FilesFilter->text().trimmed();
			FilesFilterNotice->set_filter(filter);
			FilesFilterNotice->setVisible(!filter.isEmpty() && FilesTree->tree_model()->file_count() == 0U);
		}
		if (RepositoriesFilterNotice != nullptr && RepositoriesTree != nullptr && RepositoriesFilter != nullptr) {
			const auto filter = RepositoriesFilter->text().trimmed();
			RepositoriesFilterNotice->set_filter(filter);
			RepositoriesFilterNotice->setVisible(!filter.isEmpty() && RepositoriesTree->tree_model()->rowCount() == 0);
		}
	}

	auto workbench_window::central_splitter() const noexcept -> workbench_central_splitter*
	{
		return CentralSplitter;
	}

	auto workbench_window::repositories_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[2].Panel;
	}

	auto workbench_window::repositories_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[2].Toggle;
	}

	auto workbench_window::repositories_tree() const noexcept -> repositories_tree_view*
	{
		return RepositoriesTree;
	}

	auto workbench_window::rules_editor() const noexcept -> QWidget*
	{
		return RulesEditor;
	}

	auto workbench_window::presented_repositories_state() const
		-> core::workbench_drawer_state
	{
		// As the files drawer: shown while the core offers no drawer command,
		// so a reviewer with nothing open can still reach a pull request.
		const auto toggle = command(core::workbench_command_id::toggle_repositories_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Repositories;
	}

	auto workbench_window::conversations_drawer() const noexcept
		-> workbench_drawer_panel*
	{
		return Drawers[1].Panel;
	}

	void workbench_window::publish(
		const application::github_presentation_update& aUpdate)
	{
		if (aUpdate.ReadProgress) {
			const auto wasAnswered = ReadProgress.has_value() && ReadProgress->PagesReceived >= minimum_answered_pages;
			ReadProgress           = aUpdate.ReadProgress;
			// [[BPR-1200]]. The first answers that show the connection can read
			// what was named decide the page again.
			if (!wasAnswered && ReadProgress->PagesReceived >= minimum_answered_pages) {
				update_home_landing();
			}
			// [[BPR-1357]]. The step a waiting opening names.
			if (HomeOpen.has_value() && HomeOpen->State.has_value() && HomeOpen->State->Phase == core::home_open_phase::waiting) {
				refresh_home_open();
			}
		}
		if (aUpdate.Transition.has_value()) {
			apply_github_transition(*aUpdate.Transition);
		}
		if (aUpdate.LoadingOverview && !HasGithubPullRequest
			&& SessionPanel->status() == core::github_status::loading_pull_request) {
			if (aUpdate.LoadingOverview->People) {
				if (LoadingOverviewShown) {
					OverviewPage->present_observed_people(*aUpdate.LoadingOverview->People);
				}
			} else if (aUpdate.LoadingOverview->Merge) {
				if (LoadingOverviewShown) {
					OverviewPage->present_observed_merge(*aUpdate.LoadingOverview->Merge);
				}
			} else if (aUpdate.LoadingOverview->Checks) {
				if (LoadingOverviewShown) {
					OverviewPage->present_observed_checks(*aUpdate.LoadingOverview->Checks);
				}
			} else if (aUpdate.LoadingOverview->Files) {
				// [[BPR-1842]]. The file list, once its pages have arrived, to
				// read while the rest of the review still loads.
				if (LoadingOverviewShown) {
					OverviewPage->present_observed_files(*aUpdate.LoadingOverview->Files);
				}
			} else {
				OverviewPage->present_observation(*aUpdate.LoadingOverview);
				// [[BPR-1200]]. The read has shown what it names, so the connection
				// can read it: Overview now has something to be about.
				LoadingOverviewShown = true;
				// [[BPR-1842]]. The title row names the pull request as soon as
				// GitHub does, not when the whole review has arrived.
				const auto& overview = *aUpdate.LoadingOverview;
				if (!overview.Owner.empty() && !overview.RepositoryName.empty() && overview.Number != 0U) {
					auto name       = loading_identity{ };
					name.Owner      = QString::fromStdString(overview.Owner);
					name.Repository = QString::fromStdString(overview.RepositoryName);
					name.Number     = QString::number(overview.Number);
					name.Title      = QString::fromStdString(overview.Title);
					name.Text       = QStringLiteral("%1/%2  #%3 %4").arg(name.Owner, name.Repository, name.Number, QString::fromStdString(overview.Title));
					name.Draft      = overview.Draft && overview.State == application::github_pull_request_state_projection::open;
					LoadingIdentity = std::move(name);
					apply_strip_width();
				}
				update_home_landing();
			}
		}
		if (aUpdate.CachedOverview && !HasGithubPullRequest) {
			HasCachedOverview = true;
			HomeRequested     = false;
			OverviewPage->present_cached(*aUpdate.CachedOverview);
			update_status_text();
			enter_opening_modality(review_modality::overview);
			apply_modality_surface();
		}
		if (aUpdate.CachedSource && HasCachedOverview && !HasGithubPullRequest) {
			OverviewPage->present_cached_source(*aUpdate.CachedSource);
		}
		// RFC-BPR-0084. Each answer reaches the body it names; a reader ignores
		// one that is not for its current image.
		for (const auto& image : aUpdate.Images) {
			if (image.Body.empty()) {
				OverviewPage->body_view()->apply_image(image);
			} else {
				ConversationPanel->apply_markdown_image(image);
			}
		}
		if (aUpdate.Session.has_value()) {
			// [[BPR-0803]]. A device flow still asking for its code when some
			// other way in -- GitHub CLI above all -- has signed in is ended
			// here, through its own Cancel, so it never keeps running where the
			// reviewer cannot see it. The flow's own session arrives after the
			// flow has authorized, so it is never cancelled by this.
			if (aUpdate.Session->Account.value() != 0U && DevicePanel != nullptr && DevicePanel->signing_in()
				&& DevicePanel->cancel_button()->isEnabled()) {
				DevicePanel->cancel_button()->click();
			}
			SessionPanel->apply_session(*aUpdate.Session);
			update_server_cards();
			if (SignInDialog && SignInDialog->isVisible() && aUpdate.Session->Account.value() != 0U
				&& (SignInHostname.empty() || (server_card_for(aUpdate.Session->Profile) && server_card_for(aUpdate.Session->Profile)->hostname() == SignInHostname))) {
				SignInDialog->accept();
			}
			// [[BPR-0775]]. Home's line names the accepted session's login, which
			// arrives after the transition that already decided the line.
			if (StatusLabel != nullptr) {
				StatusLabel->setText(status_line());
			}
			// [[BPR-1017]]. So does the strip's account button. A window told the
			// session in one presentation has no later transition to correct it.
			update_account_action();
			if (ConversationPanel != nullptr) {
				ConversationPanel->cards()->set_viewer_login(
					QString::fromStdString(aUpdate.Session->Login));
				if (Finder != nullptr) {
					Finder->set_viewer_login(QString::fromStdString(aUpdate.Session->Login));
				}
			}
		}
		// [[BPR-1807]]. A picture belongs to one host and account: another one, or
		// a sign-in that changed the account, forgets them all first.
		if (aUpdate.Session.has_value() || aUpdate.PullRequest.has_value()) {
			const auto scope = aUpdate.PullRequest.has_value()
				? std::to_string(aUpdate.PullRequest->Profile.value()) + ":" + std::to_string(aUpdate.PullRequest->Host.value()) + ":" + std::to_string(aUpdate.PullRequest->Account.value())
				: std::to_string(aUpdate.Session->Profile.value()) + ":" + std::to_string(aUpdate.Session->Host.value()) + ":" + std::to_string(aUpdate.Session->Account.value());
			if (scope != AvatarScope) {
				AvatarPictures->reset();
			}
			AvatarScope = scope;
		}
		for (const auto& avatar : aUpdate.Avatars) {
			AvatarPictures->apply(avatar);
		}
		if (aUpdate.PullRequest.has_value()) {
			apply_github_pull_request(*aUpdate.PullRequest, aUpdate.PreserveWindowNavigation);
			if (aUpdate.ConversationRefreshed && SessionPanel != nullptr && SessionPanel->status() == core::github_status::loading_pull_request) {
				RefreshConversationCurrent = true;
			}
		}
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			apply_github_host_diagnostic(diagnostic);
		}
		update_review_readiness();
		update_pending_panes();
		settle_pull_request_refresh(aUpdate);
		announce_github_presentation_update();
	}
}
