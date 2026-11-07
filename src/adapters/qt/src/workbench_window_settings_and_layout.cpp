#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/overview_tour.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/keyboard_shortcuts_window.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/resize_handle_cursor.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "conversation_glyphs.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QAnyStringView>
#include <QApplication>
#include <QBoxLayout>
#include <QByteArrayView>
#include <QChar>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFlags>
#include <QFrame>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QListSpecialMethods>
#include <QListSpecialMethodsBase>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QString>
#include <QStyle>
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
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-1331]]. The mark the private progress button shows: the
		/// core's decision for the selected file's receipt, or none while no
		/// file's progress is known.
		[[nodiscard]] auto shown_progress_mark(const std::optional<application::file_receipt_action_projection>& aReceipt,
											   const bool                                                        aLedgerUnavailable) -> std::optional<core::file_progress_mark>
		{
			if (aLedgerUnavailable || !aReceipt.has_value()) {
				return std::nullopt;
			}
			return core::progress_mark(aReceipt->Receipt);
		}

		/// \brief The width the conversations drawer opens at, in logical pixels.
		///
		/// \details Stated here because the alternative is what the drawer had
		/// before: whichever row was widest set the column's size hint, the
		/// splitter handed the drawer exactly that, and then the vertical scroll
		/// bar took 18 px back out of it. So the widest row was always the row
		/// that lost its last control, at whatever width the drawer opened -
		/// 540 px while the defect rows were horizontal, 366 px once BPR-0520
		/// stacked them and the agent launcher's run controls became the widest
		/// row instead. A number nobody chose, that moved whenever a label did,
		/// and that was wrong by the scroll bar's width every time.
		///
		/// Measured at 100 % with every row of the drawer stacked (BPR-0506,
		/// BPR-0520, BPR-0527): the column asks for 357 px to draw every control
		/// at its natural width, needs 251 px for its widest single control -
		/// *Classify selected conversation as defect* - below which it starts
		/// losing controls again, and the vertical scroll bar takes 18 px that
		/// the column never gets. 357 plus 18 is this. A style or system font
		/// whose metrics run wider than this machine's - the headless Linux gate
		/// uses a different style from macOS - squeezes the column toward the
		/// 251 it needs rather than clipping it, which is why the number that is
		/// stated is the one the column asks for and not the one it needs.
		///
		/// It is the width the product opens at, not a floor on the drawer.
		/// [[BPR-0728]]: the drawer's content stops at
		/// `minimum_presented_drawer_width` like the other drawers, so a
		/// reviewer can drag it narrower and the host persists what they chose.
		/// What holds at those narrower widths is that every row is a column, so
		/// a row cannot put its last control out of reach.
		constexpr int conversation_drawer_default_width = 375;

		/// \brief Narrowest width a drawer is ever presented at when expanded.
		///
		/// \details [[BPR-0662]]. The host's floor is 64 pixels, the least it
		/// will retain; a persisted width beneath this one was not chosen by a
		/// reviewer but written by a squeeze (a hidden splitter laid out at
		/// its default size wrote 64 for every drawer), so the window grants
		/// the drawer's design width instead and the next drag persists a
		/// real one.
		constexpr std::uint32_t minimum_presented_drawer_width = 160U;

		[[nodiscard]] auto receipt_failure_text(
			const application::receipt_failure_publication& aFailure) -> QString
		{
			if (aFailure.Store.has_value()) {
				switch (*aFailure.Store) {
				case application::ledger_store_failure::unavailable:
					return QStringLiteral("Private review progress is unavailable.");
				case application::ledger_store_failure::conflict:
					return QStringLiteral("Private review progress changed elsewhere.");
				case application::ledger_store_failure::integrity_failure:
					return QStringLiteral("Private review progress failed its integrity check.");
				case application::ledger_store_failure::quota_exceeded:
					return QStringLiteral("Private review progress reached its storage limit.");
				case application::ledger_store_failure::cancelled:
					return QStringLiteral("The private progress change was cancelled.");
				case application::ledger_store_failure::invalid_record:
					return QStringLiteral("The private progress change was invalid.");
				}
			}
			switch (aFailure.Rejection) {
			case core::file_receipt_rejection::none:
				break;
			case core::file_receipt_rejection::unknown_file:
				return QStringLiteral("The selected file is no longer current.");
			case core::file_receipt_rejection::material_mismatch:
				return QStringLiteral("The selected file material changed.");
			case core::file_receipt_rejection::excluded_file_cannot_be_accepted:
				return QStringLiteral("Include this file before accepting it.");
			case core::file_receipt_rejection::accepted_file_cannot_be_excluded:
				return QStringLiteral("Revoke acceptance before excluding this file.");
			}
			return QStringLiteral("Private review progress could not be updated.");
		}

		void swap_settings_pages(settings_window& aFirst, settings_window& aSecond)
		{
			auto*      first      = aFirst.pages();
			auto*      second     = aSecond.pages();
			const auto firstPage  = first->currentIndex();
			const auto secondPage = second->currentIndex();
			for (int index = 0; index < static_cast<int>(settings_page_count); ++index) {
				auto* firstPageWidget  = first->widget(index);
				auto* secondPageWidget = second->widget(index);
				first->removeWidget(firstPageWidget);
				second->removeWidget(secondPageWidget);
				first->insertWidget(index, secondPageWidget);
				second->insertWidget(index, firstPageWidget);
			}
			first->setCurrentIndex(firstPage);
			second->setCurrentIndex(secondPage);
		}
	}

	void workbench_window::set_profile_management_dispatch(
		application::profile_management_dispatch aDispatch,
		std::function<void()> aManage, std::function<void()> aQuit)
	{
		ProfileManagementDispatch = std::move(aDispatch);
		ManageProfiles            = std::move(aManage);
		QuitProfiles              = std::move(aQuit);
		ManageProfilesAction->setEnabled(static_cast<bool>(ManageProfiles));
		QuitProfilesAction->setEnabled(static_cast<bool>(QuitProfiles));
	}

	void workbench_window::adopt_profile_management(
		const application::profile_management_projection& aProjection,
		const core::bpr_profile_id                        aCurrentProfile)
	{
		// [[BPR-0777]]. Adopted before the bounded-list guard: a refused projection is
		// exactly when this window has to say so.
		ProfileFailure = aProjection.Failure
			? std::optional{ aProjection.Failure->Reason }
			: std::nullopt;
		ProfileStatusAction->setText(ProfileFailure
										 ? profile_management_failure_text(*ProfileFailure)
										 : QString{ });
		ProfileStatusAction->setVisible(ProfileFailure.has_value());
		ProfileStatusSeparator->setVisible(ProfileFailure.has_value());
		update_status_text();
		// [[BPR-1864]]. With more than one profile, a pull request's title
		// names the profile too.
		ProfileCount = aProjection.Profiles.size();
		update_window_title();
		if (aProjection.Profiles.size() > core::maximum_bpr_profiles) {
			return;
		}
		CurrentBprProfile = aCurrentProfile;
		NewBprWindowAction->setEnabled(aProjection.Ready && !aProjection.Closing
									   && std::ranges::any_of(aProjection.Profiles, [aCurrentProfile](const auto& aProfile) {
											  return aProfile.Profile == aCurrentProfile && aProfile.Runtime == application::profile_management_runtime::open;
										  }));
		update_home_review_actions();
		for (auto iterator = ProfileActions.begin(); iterator != ProfileActions.end();) {
			if (std::ranges::none_of(aProjection.Profiles, [iterator](const auto& aProfile) { return aProfile.Profile == iterator->first; })) {
				delete iterator->second;
				iterator = ProfileActions.erase(iterator);
			} else {
				++iterator;
			}
		}
		auto* before = ManageProfilesAction;
		for (auto iterator = aProjection.Profiles.rbegin(); iterator != aProjection.Profiles.rend(); ++iterator) {
			const auto& profile    = *iterator;
			auto [entry, inserted] = ProfileActions.try_emplace(profile.Profile, nullptr);
			if (inserted) {
				entry->second = new QAction{ ProfilesMenu };
				entry->second->setCheckable(true);
				entry->second->setData(QVariant::fromValue<qulonglong>(profile.Profile.value()));
				entry->second->setObjectName(QStringLiteral("openBprProfile_%1").arg(profile.Profile.value()));
				connect(entry->second, &QAction::triggered, this, [this, profileId = profile.Profile] {
					ProfileActions.at(profileId)->setChecked(profileId == CurrentBprProfile);
					if (ProfileManagementDispatch) {
						ProfileManagementDispatch(application::open_bpr_profile_intent{ profileId });
					}
				});
			}
			auto* action = entry->second;
			auto  label  = QString::fromStdString(profile.Label);
			label.replace(QStringLiteral("&"), QStringLiteral("&&"));
			action->setText(label);
			action->setChecked(profile.Profile == aCurrentProfile);
			action->setEnabled(aProjection.Ready && !aProjection.Closing
							   && profile.Runtime != application::profile_management_runtime::closing);
			const auto actions = ProfilesMenu->actions();
			if (actions.indexOf(action) + 1 != actions.indexOf(before) || !actions.contains(action)) {
				ProfilesMenu->insertAction(before, action);
			}
			before = action;
		}
	}

	auto workbench_window::profiles_menu() const noexcept -> QMenu*
	{
		return ProfilesMenu;
	}

	void workbench_window::set_diagnostic_exporter(std::function<void(core::diagnostic_export_policy)> aExporter)
	{
		DiagnosticExporter = std::move(aExporter);
	}

	void workbench_window::set_insights_presenter(std::function<void()> aPresenter) { InsightsPresenter = std::move(aPresenter); }

	void workbench_window::set_settings_presenter(std::function<void(settings_page)> aPresenter)
	{
		SettingsPresenter = std::move(aPresenter);
	}

	void workbench_window::mount_settings(settings_window& aSettings)
	{
		if (&aSettings == SettingsWindow || MountedSettings == &aSettings) {
			return;
		}
		unmount_settings();
		SettingsWindow->hide();
		swap_settings_pages(*SettingsWindow, aSettings);
		MountedSettings = &aSettings;
		aSettings.setPalette(palette());
		aSettings.set_close_handler([panel = QPointer<appearance_settings_panel>{ AppearanceSettings }] {
			if (panel) {
				panel->cancel_preview();
			}
		});
	}

	void workbench_window::unmount_settings()
	{
		if (!MountedSettings) {
			return;
		}
		auto* shell     = MountedSettings.data();
		MountedSettings = nullptr;
		shell->set_close_handler({ });
		swap_settings_pages(*SettingsWindow, *shell);
		AppearanceSettings->cancel_preview();
	}

	void workbench_window::set_sign_in_cancel_dispatch(std::function<void()> aDispatch)
	{
		SignInCancelDispatch = std::move(aDispatch);
	}

	auto workbench_window::sign_in_dialog() const noexcept -> QDialog* { return SignInDialog; }

	void workbench_window::connect_github_profile(const core::host_profile_id aProfile)
	{
		select_github_profile(aProfile);
		GithubConnectAction->trigger();
	}

	void workbench_window::present_cli_account_choices(std::vector<core::host_profile_id> aProfiles)
	{
		SignInCliProfiles = std::move(aProfiles);
		update_sign_in_surface();
	}

	void workbench_window::update_sign_in_surface()
	{
		if (!SignInDialog || !SignInDialog->isVisible()) {
			return;
		}
		for (auto* card : ServerCards) {
			const auto matches = SignInHostname.empty() || card->hostname() == SignInHostname;
			const auto cli     = !SignInCliOnly || std::ranges::any_of(SignInCliProfiles, [card](auto aProfile) { return card->holds(aProfile); });
			// [[BPR-0803]]. Choosing GitHub CLI while a device flow runs keeps
			// the card holding the flow in view: its code and address stay
			// beside the GitHub CLI choice until one of them ends. Without a
			// discovered account on this host the card shows only the flow.
			const auto flowHere = DevicePanel != nullptr && card->device_sign_in() == DevicePanel && DevicePanel->signing_in();
			card->setVisible(matches && (cli || flowHere));
			card->set_setup_mode(true, SignInCliOnly && cli);
		}
		if (auto* lede = AccountsPanel->findChild<QLabel*>(QStringLiteral("accountsLede"))) {
			lede->hide();
		}
		EnterpriseServerCancel->hide();
		AddEnterpriseServerButton->setVisible(QueueReady && static_cast<bool>(SavedServerDispatch) && SignInHostname.empty() && !SignInCliOnly && !EnterpriseDraftOpen);
		UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch)
									   && (!SignInCliOnly || GithubCliDiscoveryState == github_cli_discovery_state::empty
										   || GithubCliDiscoveryState == github_cli_discovery_state::process_failed
										   || GithubCliDiscoveryState == github_cli_discovery_state::malformed_response
										   || GithubCliDiscoveryState == github_cli_discovery_state::unsupported_version
										   || GithubCliDiscoveryState == github_cli_discovery_state::ineligible_accounts
										   || GithubCliDiscoveryState == github_cli_discovery_state::missing_executable
										   || GithubCliDiscoveryState == github_cli_discovery_state::launch_failed
										   || GithubCliDiscoveryState == github_cli_discovery_state::timed_out
										   || GithubCliDiscoveryState == github_cli_discovery_state::exited_unsuccessfully
										   || GithubCliDiscoveryState == github_cli_discovery_state::output_rejected));
		if (DevicePanel && DevicePanel->signing_in()) {
			DevicePanel->cancel_button()->hide();
		}
	}

	void workbench_window::open_sign_in(std::string aHostname, const bool aCliOnly)
	{
		if (!AccountsPanel || (SessionPanel != nullptr && !SessionPanel->connect_enabled())) {
			return;
		}
		if (!SignInDialog) {
			SignInDialog = new QDialog{ this };
			SignInDialog->setObjectName(QStringLiteral("sharedSignInDialog"));
			SignInDialog->setWindowModality(Qt::NonModal);
			auto* layout = new QVBoxLayout{ SignInDialog };
			SignInScroll = new QScrollArea{ SignInDialog };
			SignInScroll->setWidgetResizable(true);
			SignInScroll->setFrameShape(QFrame::NoFrame);
			layout->addWidget(SignInScroll);
			auto* buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, SignInDialog };
			layout->addWidget(buttons);
			connect(buttons, &QDialogButtonBox::rejected, SignInDialog, &QDialog::reject);
			connect(SignInDialog, &QDialog::finished, this, [this](int aResult) {
				SignInScroll->takeWidget();
				AccountsPanel->layout()->setAlignment(Qt::Alignment{ });
				if (AccountsReturnParent) {
					AccountsPanel->setParent(AccountsReturnParent);
					if (auto* returnLayout = qobject_cast<QVBoxLayout*>(AccountsReturnParent->layout())) {
						returnLayout->insertWidget(1, AccountsPanel);
					}
					AccountsPanel->show();
				}
				for (auto* card : ServerCards) {
					card->show();
					card->set_setup_mode(false);
				}
				if (auto* lede = AccountsPanel->findChild<QLabel*>(QStringLiteral("accountsLede"))) {
					lede->show();
				}
				EnterpriseServerCancel->show();
				SignInHostname.clear();
				SignInCliOnly = false;
				SignInCliProfiles.clear();
				UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch));
				close_enterprise_server_draft();
				update_server_cards();
				if (aResult == QDialog::Rejected && SignInCancelDispatch) {
					SignInCancelDispatch();
				}
			});
		}
		if (!SignInDialog->isVisible()) {
			AccountsReturnParent = AccountsPanel->parentWidget();
			AccountsPanel->layout()->setAlignment(Qt::AlignTop);
			SignInScroll->setWidget(AccountsPanel);
			SignInDialog->resize(620, 440);
		}
		SignInHostname = std::move(aHostname);
		SignInCliOnly  = aCliOnly;
		if (aCliOnly) {
			close_enterprise_server_draft();
		}
		SignInDialog->setWindowTitle(SignInHostname.empty() ? tr("Sign in") : tr("Sign in to %1").arg(QString::fromStdString(SignInHostname)));
		settings()->hide();
		SignInDialog->show();
		update_sign_in_surface();
		SignInDialog->raise();
		SignInDialog->activateWindow();
	}

	void workbench_window::open_settings(const settings_page aPage)
	{
		if (SignInDialog && SignInDialog->isVisible()) {
			SignInDialog->reject();
		}
		if (SettingsPresenter) {
			SettingsPresenter(aPage);
		} else if (SettingsWindow != nullptr) {
			SettingsWindow->present(aPage);
		}
	}

	auto workbench_window::settings() const noexcept -> settings_window*
	{
		return MountedSettings ? MountedSettings.data() : SettingsWindow;
	}

	auto workbench_window::keyboard_shortcuts_action() const noexcept -> QAction* { return KeyboardShortcutsAction; }

	void workbench_window::show_keyboard_shortcuts()
	{
		// Created on first use and kept: it reads a static table, and a
		// second press raises the same window rather than opening another.
		if (KeyboardShortcuts == nullptr) {
			KeyboardShortcuts = new keyboard_shortcuts_window{ this };
		}
		KeyboardShortcuts->show();
		KeyboardShortcuts->raise();
		KeyboardShortcuts->activateWindow();
	}

	auto workbench_window::keyboard_shortcuts() const noexcept -> keyboard_shortcuts_window* { return KeyboardShortcuts; }

	auto workbench_window::settings_action() const noexcept -> QAction*
	{
		return SettingsAction;
	}

	void workbench_window::dispatch_line_numbers_toggle()
	{
		// The widget never decides. Whatever Qt just did to the check mark is
		// undone by re-applying the projection, and the intent is what
		// eventually changes it.
		apply_line_numbers();
		if (!Dispatch) {
			return;
		}
		Dispatch(core::toggle_workbench_line_numbers_intent{ });
	}

	void workbench_window::apply_line_numbers()
	{
		const auto shown
			= LineNumberState.State == core::workbench_line_numbers::shown;
		LineNumbersAction->setChecked(shown);
		const auto projection
			= command(core::workbench_command_id::toggle_line_numbers);
		describe_action(
			LineNumbersAction,
			projection && projection->Enabled,
			shown
				? QStringLiteral(
					  "Hide the source line numbers and give the gutter's width "
					  "back to the code.")
				: QStringLiteral("Number each pane's own source lines."),
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_line_numbers_visible(shown);
		}
	}

	void workbench_window::dispatch_one_pane_toggle()
	{
		apply_one_pane();
		if (Dispatch) {
			Dispatch(core::toggle_workbench_one_pane_intent{ });
		}
	}

	void workbench_window::apply_one_pane()
	{
		const auto enabled = OnePaneState.State == core::workbench_one_pane::enabled;
		OnePaneAction->setChecked(enabled);
		const auto projection = command(core::workbench_command_id::toggle_one_pane);
		const auto offered    = projection && projection->Enabled;
		// [[BPR-1815]]. The tooltip's first line is what a press does and its
		// key, as the file and change arrows' are; the second says what that
		// layout is, or why the toggle is unavailable.
		const auto purpose = enabled
			? QStringLiteral("Show the comparison in two source panes.")
			: QStringLiteral("Show both sides in one document.");
		describe_navigation_action(
			OnePaneAction,
			enabled ? QStringLiteral("Two panes") : QStringLiteral("One pane"),
			offered,
			purpose,
			offered ? purpose
					: drawer_reason_text(projection
											 ? projection->DisabledReason
											 : core::workbench_reason_code::no_active_workbench),
			presented_key(OnePaneAction));
		// The glyph says the layout showing, as Hide drawers' says the drawers.
		if (OnePaneAction->icon().isNull() || OnePaneAction->property("bprOnePane").toBool() != enabled) {
			OnePaneAction->setProperty("bprOnePane", enabled);
			OnePaneAction->setIcon(strip_icon(enabled ? strip_glyph::one_pane : strip_glyph::two_panes, palette()));
		}
		if (auto* const button = ReviewBar != nullptr ? qobject_cast<QToolButton*>(ReviewBar->widgetForAction(OnePaneAction)) : nullptr;
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("One pane"));
			button->setAccessibleDescription(OnePaneAction->toolTip());
		}
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_one_pane_enabled(enabled);
		}
	}

	void workbench_window::dispatch_drawers_toggle()
	{
		// The action flips its own check when triggered; the core decides,
		// so what it last decided is shown again and only the gesture leaves.
		apply_drawers_toggle();
		if (Dispatch && ToggleDrawersAction->isEnabled()) {
			Dispatch(core::toggle_workbench_drawers_intent{ });
		}
	}

	void workbench_window::apply_drawers_toggle()
	{
		update_header_source_action();
		update_source_find_actions();
		if (ToggleDrawersAction == nullptr) {
			return;
		}
		const auto projection = command(core::workbench_command_id::toggle_drawers);
		// A new window's core is in Review before it has published a command,
		// and the drawers are the reviewer's then too ([[BPR-0675]]). Home has
		// no review, so nothing there to give the room to.
		const auto  offered   = !StripHome && (projection ? projection->Enabled : true);
		auto* const focused   = QApplication::focusWidget();
		const auto  composing = focused != nullptr && editable_multi_line(focused) && isAncestorOf(focused);
		const auto  enabled   = offered && !composing;
		const auto  open      = [](const core::workbench_drawer_state aState) {
			return aState == core::workbench_drawer_state::expanded;
		};
		const auto hidden = offered && !open(DrawerStates.Files) && !open(DrawerStates.Conversations)
			&& !open(DrawerStates.Repositories);
		ToggleDrawersAction->setChecked(hidden);
		// The tooltip names what Show brings back, left to right.
		auto names = QStringList{ };
		if (open(DrawerRestore.Drawers.Repositories)) {
			names.push_back(QStringLiteral("Repositories"));
		}
		if (open(DrawerRestore.Drawers.Files)) {
			names.push_back(QStringLiteral("Files"));
		}
		if (open(DrawerRestore.Drawers.Conversations)) {
			names.push_back(QStringLiteral("Comments"));
		}
		const auto last   = names.isEmpty() ? QStringLiteral("the drawers") : names.takeLast();
		const auto listed = names.isEmpty() ? last : QStringLiteral("%1 and %2").arg(names.join(QStringLiteral(", ")), last);
		const auto key    = ToggleDrawersAction->shortcut().toString(QKeySequence::NativeText);
		auto       name   = hidden ? QStringLiteral("Show %1").arg(listed) : QStringLiteral("Hide drawers");
		auto       reason = QStringLiteral("Open a pull request to hide and show its drawers.");
		if (offered && composing) {
			reason = QStringLiteral("The text you are writing keeps %1. Click the code first.").arg(key);
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::overview_keeps_drawers_collapsed) {
			reason = QStringLiteral("Overview keeps every drawer closed");
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::discuss_shows_the_conversation) {
			reason = QStringLiteral("In Discuss the thread is the page");
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::threads_finds_a_thread) {
			reason = QStringLiteral("In Threads the finder is the page");
		}
		describe_action(ToggleDrawersAction, enabled, QStringLiteral("%1 (%2)").arg(name, key), reason);
		if (auto* const button = PrimaryToolbar != nullptr ? qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(ToggleDrawersAction)) : nullptr;
			button != nullptr) {
			button->setAccessibleName(offered ? name : QStringLiteral("Hide drawers"));
			button->setAccessibleDescription(ToggleDrawersAction->toolTip());
		}
		// The glyph says the state: filled columns while the drawers show.
		if (ToggleDrawersAction->icon().isNull() || ToggleDrawersAction->property("bprDrawersHidden").toBool() != hidden) {
			ToggleDrawersAction->setProperty("bprDrawersHidden", hidden);
			ToggleDrawersAction->setIcon(strip_icon(hidden ? strip_glyph::drawers_hidden : strip_glyph::drawers_shown, palette()));
		}
	}

	auto workbench_window::drawer_holding_focus() const -> QWidget*
	{
		auto* const focused = QApplication::focusWidget();
		if (focused == nullptr) {
			return nullptr;
		}
		for (const auto& entry : Drawers) {
			if (entry.Panel != nullptr && entry.Panel->isAncestorOf(focused)) {
				return entry.Panel;
			}
		}
		return nullptr;
	}

	void workbench_window::keep_focus_out_of_hidden_drawers(QWidget* const aHeld)
	{
		// [[BPR-1295]]. Hiding the drawer that held focus would otherwise hand
		// it to whatever Qt finds next; the code is where the room went.
		if (aHeld == nullptr || aHeld->isVisible() || ComparisonSurface == nullptr) {
			return;
		}
		for (auto* const pane : { static_cast<QWidget*>(ComparisonSurface->right_pane()), static_cast<QWidget*>(ComparisonSurface->left_pane()) }) {
			if (pane != nullptr && pane->isVisible()) {
				pane->setFocus(Qt::OtherFocusReason);
				return;
			}
		}
		ComparisonSurface->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::dispatch_updates_line_toggle()
	{
		// Whatever Qt just did to the check mark or the arrow is undone by
		// showing the fold the core holds; the intent is what changes it.
		apply_updates_line();
		if (Dispatch) {
			Dispatch(core::toggle_workbench_updates_line_intent{ });
		}
	}

	void workbench_window::apply_updates_line()
	{
		const auto open    = updates_line_open();
		auto*      line    = EpochSelectors != nullptr ? EpochSelectors->line() : nullptr;
		const auto marked  = line != nullptr && line->history_marked();
		const auto history = line != nullptr ? line->statement() : QString{ };
		const auto purpose = open
			? QStringLiteral("Fold the Updates line; the review bar still names the comparison and its dates.")
			: QStringLiteral("Open the Updates line: the pull request's updates, with the compared two marked.");
		if (UpdatesLineAction != nullptr) {
			UpdatesLineAction->setChecked(open);
			UpdatesLineAction->setToolTip(purpose);
			UpdatesLineAction->setStatusTip(purpose);
		}
		if (UpdatesDisclosure != nullptr) {
			UpdatesDisclosure->set_open(open);
			UpdatesDisclosure->set_history_marked(marked);
			// The line's statement travels with the disclosure, so a folded
			// line still says whether its history is complete.
			const auto tip = history.isEmpty() ? purpose : QStringLiteral("%1\n%2").arg(purpose, history);
			if (UpdatesDisclosure->toolTip() != tip) {
				UpdatesDisclosure->setToolTip(tip);
				UpdatesDisclosure->setStatusTip(purpose);
			}
			UpdatesDisclosure->setAccessibleName(marked ? QStringLiteral("Updates, history incomplete") : QStringLiteral("Updates"));
		}
		if (EpochSelectors != nullptr && line != nullptr) {
			// Folded, the timeline leaves no row; the control stays only for
			// the review details it also holds.
			EpochSelectors->set_readiness(EpochSelectors->readiness_text(), open);
			const auto shown = open || EpochSelectors->details_visible();
			if (EpochSelectors->isHidden() == shown) {
				EpochSelectors->setVisible(shown);
			}
		}
	}

	void workbench_window::dispatch_drawer_toggle(const std::size_t aIndex)
	{
		// A checkable action flips itself the moment it is triggered. The core owns
		// drawer state, so the last decided state is re-asserted immediately and
		// only the gesture leaves; the window never shows a state it decided.
		update_action_state();
		if (!Dispatch) {
			return;
		}
		const auto drawer = [aIndex] {
			switch (aIndex) {
			case 0U:
				return core::workbench_drawer::files;
			case 1U:
				return core::workbench_drawer::conversations;
			default:
				return core::workbench_drawer::repositories;
			}
		}();
		Dispatch(core::toggle_workbench_drawer_intent{ .Drawer = drawer });
	}

	void workbench_window::dispatch_receipt(const std::size_t aIndex)
	{
		update_receipt_actions();
		if (!ReceiptDispatch || !Receipt.has_value()
			|| aIndex >= Receipt->Actions.size()
			|| !Receipt->Actions[aIndex].Enabled) {
			return;
		}
		++Statistics.ReceiptGestures;
		ReceiptDispatch(Receipt->Actions[aIndex].Command);
	}

	void workbench_window::observe_drawer_width(
		const core::workbench_drawer aDrawer,
		const std::uint32_t          aWidth)
	{
		// Geometry this window just applied is not a user resize. Reporting it
		// would let a width the window could merely grant overwrite the width the
		// host persisted.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		// [[BPR-1925]]. A drag while the Tour tab holds its chat is the chat's
		// width; the drawer's own narrow width stays as the host keeps it.
		if (aDrawer == core::workbench_drawer::conversations && tour_chat_wide()) {
			TourChatWidth = static_cast<int>(aWidth);
			return;
		}
		auto& retained = [this, aDrawer]() -> std::uint32_t& {
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return LayoutWidths->FilesWidth;
			case core::workbench_drawer::conversations:
				return LayoutWidths->ConversationsWidth;
			case core::workbench_drawer::repositories:
				return LayoutWidths->RepositoriesWidth;
			}
			return LayoutWidths->FilesWidth;
		}();
		if (retained == aWidth) {
			return;
		}
		// The host owns and clamps the durable width. This local echo only keeps
		// one observed width from being reported twice before it is published back.
		retained = aWidth;
		if (WidthDispatch) {
			WidthDispatch(aDrawer, aWidth);
		}
	}

	void workbench_window::apply_drawer_layout()
	{
		ApplyingLayout = true;
		// [[BPR-0721]]. Whether a drawer shows its own controls is the
		// published preference's, applied before the state so a collapsed
		// drawer is presented the way the preference says.
		const auto controls = Behaviour.DrawerControls == application::review_drawer_controls::shown;
		auto       reshown  = false;
		for (auto& entry : Drawers) {
			reshown = reshown || (controls && !entry.Panel->controls_shown());
			entry.Panel->set_controls_shown(controls);
		}
		if (reshown) {
			// A panel that left the splitter comes back with the splitter's
			// record of the room it last had, which the rail cannot fill;
			// the sizes are set again so the centre takes the difference,
			// with the design's widths when the host has published none.
			CentralSplitter->refresh();
			if (!LayoutWidths) {
				CentralSplitter->setSizes(central_sizes());
			}
		}
		Drawers[0].Panel->adopt(presented_files_state());
		Drawers[1].Panel->adopt(DrawerStates.Conversations);
		Drawers[2].Panel->adopt(presented_repositories_state());
		// These full-content pages borrow the center width without changing the
		// retained drawer choices. Returning restores the same code arrangement.
		const auto fullPage = CompositionPage != nullptr || TourPageOpen
			|| (Modality != nullptr
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads));
		for (auto& entry : Drawers) {
			entry.Panel->setVisible(
				!fullPage
				&& (controls
					|| entry.Panel->state()
						== core::workbench_drawer_state::expanded));
		}
		ApplyingLayout = false;
		apply_drawer_widths();
		update_dif_presentation();
		// [[BPR-1212]]. A new comment that asked for the drawer opens in it.
		if (PendingDraft.has_value()) {
			QTimer::singleShot(0, this, [this] { resume_pending_draft(); });
		}
	}

	void workbench_window::apply_drawer_widths()
	{
		if (!LayoutWidths || ApplyingLayout) {
			return;
		}
		ApplyingLayout = true;
		// A layout from before the repositories drawer carries no width
		// for it. The design's 300 is what the window grants, echoed here
		// so the grant is not reported back as a resize.
		if (LayoutWidths->RepositoriesWidth == 0U) {
			LayoutWidths->RepositoriesWidth = 300U;
		}
		// [[BPR-1925]]. The tour and its chat need the wider range.
		if (tour_chat_wide()) {
			Drawers[1].Panel->set_expanded_width_bounds(tour_chat_drawer_minimum_width, tour_chat_drawer_maximum_width);
		} else {
			Drawers[1].Panel->set_expanded_width_bounds(conversation_drawer_minimum_width, conversation_drawer_maximum_width);
		}
		CentralSplitter->setSizes(central_sizes());
		ApplyingLayout = false;
	}

	void workbench_window::observe_handle_drag()
	{
		// [[BPR-0662]]. The handle moved under the reviewer's hold: each
		// expanded drawer's width is now theirs, and the host persists it.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		const auto sizes = CentralSplitter->sizes();
		if (sizes.size() != 4) {
			return;
		}
		const auto report = [this, &sizes](const int aIndex, const core::workbench_drawer aDrawer,
										   const core::workbench_drawer_state aState) {
			if (aState == core::workbench_drawer_state::expanded && sizes[aIndex] > 0) {
				observe_drawer_width(aDrawer, static_cast<std::uint32_t>(sizes[aIndex]));
			}
		};
		report(0, core::workbench_drawer::repositories, presented_repositories_state());
		report(1, core::workbench_drawer::files, presented_files_state());
		report(3, core::workbench_drawer::conversations, DrawerStates.Conversations);
		update_dif_presentation();
	}

	workbench_central_splitter::workbench_central_splitter(QWidget* const aParent)
		: QSplitter{ Qt::Horizontal, aParent }
	{
	}

	void workbench_central_splitter::move_handle(const int aIndex, const int aPosition)
	{
		moveSplitter(aPosition, aIndex);
	}

	auto workbench_central_splitter::createHandle() -> QSplitterHandle*
	{
		return new workbench_splitter_handle{ orientation(), this };
	}

	workbench_splitter_handle::workbench_splitter_handle(
		const Qt::Orientation aOrientation,
		QSplitter* const      aParent)
		: QSplitterHandle{ aOrientation, aParent }
	{
		// [[BPR-1330]]. The same cursor hold as the Overview summary seam.
		hold_resize_cursor(*this);
	}

	auto workbench_splitter_handle::rule_edge() const -> int
	{
		// Keep aimable space on both sides of the one-pixel rule. For an even
		// style width, choose the left middle column; odd widths are symmetric.
		return (width() - 1) / 2;
	}

	void workbench_splitter_handle::paintEvent(QPaintEvent* const aEvent)
	{
		QSplitterHandle::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		// One constant weight, as the accepted asset draws it: the owner's
		// answer in the v009 attended session. See the class doc comment.
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		const auto edge = rule_edge();
		painter.drawLine(edge, 0, edge, height());
	}

	auto workbench_window::presented_files_state() const
		-> core::workbench_drawer_state
	{
		// The core decides the drawer while it offers the drawer command. When
		// it offers none -- no workbench is active -- the files drawer holds
		// the sign-in, the host chrome and the queue that would make one, so it
		// presents expanded: a rail the reviewer could not reopen would shut
		// the way in. The core's state is untouched and returns with the
		// command.
		const auto toggle = command(core::workbench_command_id::toggle_files_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Files;
	}

	auto workbench_window::central_sizes() const -> QList<int>
	{
		if (CompositionPage != nullptr
			|| (Modality != nullptr
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads))) {
			return { 0, 0, CentralSplitter->width(), 0 };
		}
		const auto granted = [](const std::uint32_t aWidth, const std::uint32_t aDefault) {
			// An arithmetic guard on a host-owned value, not a second clamp: the
			// host already refuses to retain a width outside its own bound. A
			// width beneath the presented floor is a squeeze that was written
			// back, not a choice; the drawer opens at its design width.
			if (aWidth < minimum_presented_drawer_width) {
				return static_cast<int>(aDefault);
			}
			return static_cast<int>(
				std::min(aWidth, application::maximum_workbench_drawer_width));
		};
		const auto defaults = application::workbench_layout_settings{ };
		// Before the host publishes a layout the widths are the design's.
		const auto widths = LayoutWidths.value_or(application::workbench_layout_delta{ });
		// A collapsed drawer is its rail, so the rail's width is what it keeps;
		// [[BPR-0721]] with the drawer controls hidden it has left the
		// splitter, and neither it nor its handle takes any width.
		const auto collapsedWidth = Behaviour.DrawerControls == application::review_drawer_controls::shown
			? workbench_drawer_rail::width_pixels
			: 0;
		const auto files          = presented_files_state() == core::workbench_drawer_state::expanded
			? granted(widths.FilesWidth, defaults.FilesWidth)
			: collapsedWidth;
		// [[BPR-1925]]. While the Tour tab shows the tour and its chat the
		// drawer holds both, at the width the reviewer last gave it so.
		const auto conversations
			= DrawerStates.Conversations != core::workbench_drawer_state::expanded ? collapsedWidth
			: tour_chat_wide()                                                     ? std::clamp(TourChatWidth, tour_chat_drawer_minimum_width, tour_chat_drawer_maximum_width)
																				   : std::clamp(granted(widths.ConversationsWidth, conversation_drawer_default_width), conversation_drawer_minimum_width, conversation_drawer_maximum_width);
		const auto repositories
			= presented_repositories_state() == core::workbench_drawer_state::expanded
			? granted(widths.RepositoriesWidth, defaults.RepositoriesWidth)
			: collapsedWidth;
		const auto handles = ((repositories > 0 ? 1 : 0) + (files > 0 ? 1 : 0) + (conversations > 0 ? 1 : 0))
			* CentralSplitter->handleWidth();
		const auto centre = std::max(
			1, CentralSplitter->width() - repositories - files - conversations - handles);
		return { repositories, files, centre, conversations };
	}

	void workbench_window::configure_tool_buttons()
	{
		for (const auto& entry : Navigation) {
			auto* const button = qobject_cast<QToolButton*>(
				ReviewBar->widgetForAction(entry.Action));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Purpose);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		for (const auto& entry : Drawers) {
			auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Toggle));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Collapse);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		update_compose_mark();
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(GithubRefreshAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Refresh"));
			button->setAccessibleDescription(QStringLiteral(
				"Reload the open pull request from its host."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(HomeAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Home"));
			button->setAccessibleDescription(HomeAction->toolTip());
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(SettingsAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Settings"));
			button->setAccessibleDescription(QStringLiteral(
				"Open the Settings window."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		update_account_action();
		update_home_landing();
	}

	void workbench_window::refresh_toolbar_icons()
	{
		auto* const nativeStyle = style();
		if (nativeStyle == nullptr) {
			return;
		}
		// [[BPR-0646]]. The strip's glyphs are painted in the live palette's
		// ink, so a theme change repaints them here exactly as the arrows were.
		const auto& ink = palette();
		if (HomeAction != nullptr) {
			HomeAction->setIcon(strip_icon(strip_glyph::home, ink));
		}
#if defined(_WIN32)
		// [[BPR-1596]]. The menu control stands first in the Windows row; left
		// in the previous theme's ink it vanishes into the new ground and reads
		// as an empty space before Home.
		if (StripMenuButton != nullptr) {
			StripMenuButton->setIcon(windows_menu_icon(ink));
		}
#endif
		// [[BPR-1290]]. Next new reply's icon on the Comments tab row.
		if (NextNewCommentAction != nullptr) {
			NextNewCommentAction->setIcon(conversation_icon(conversation_glyph::next_new, ink));
		}
		Drawers[0].Toggle->setIcon(strip_icon(strip_glyph::files_drawer, ink));
		Drawers[2].Toggle->setIcon(strip_icon(strip_glyph::repositories_drawer, ink));
		Navigation[0].Action->setIcon(strip_icon(strip_glyph::previous_file, ink));
		Navigation[1].Action->setIcon(strip_icon(strip_glyph::next_file, ink));
		Navigation[2].Action->setIcon(strip_icon(strip_glyph::previous_change, ink));
		Navigation[3].Action->setIcon(strip_icon(strip_glyph::next_change, ink));
		ReviewDetailsAction->setIcon(strip_icon(strip_glyph::review_details, ink));
		// [[BPR-1331]]. The selected file's mark and the menu's marks, in the
		// new palette.
		refresh_progress_icons();
		if (ComposeReviewAction != nullptr && CommentAction != nullptr) {
			ComposeReviewAction->setIcon(strip_icon(strip_glyph::compose_review, ink));
			// The composition pages' Back and Restore follow the theme too.
			for (auto* const page : { ComposeReviewPage, ComposeInlinePage }) {
				if (page != nullptr) {
					for (auto* const back : page->findChildren<QPushButton*>(QString{ }, Qt::FindChildrenRecursively)) {
						if (back->objectName().endsWith(QStringLiteral("PageBack"))) {
							back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
						}
					}
				}
			}
			if (ReviewSheet != nullptr) {
				ReviewSheet->expand_button()->setIcon(conversation_icon(conversation_glyph::maximize, palette()));
				ReviewSheet->close_button()->setIcon(drawer_icon(drawer_glyph::close, palette()));
			}
			CommentAction->setIcon(strip_icon(strip_glyph::comment, ink));
		}
		// [[BPR-1338]]. One glyph for Open on GitHub wherever it is offered.
		if (OpenOnGithubAction != nullptr) {
			OpenOnGithubAction->setIcon(strip_icon(strip_glyph::open_on_github, ink));
			if (HomeList != nullptr) {
				HomeList->set_page_action(OpenOnGithubAction->icon(), OpenOnGithubAction->toolTip());
			}
		}
		Drawers[1].Toggle->setIcon(strip_icon(strip_glyph::comments_drawer, ink));
		// [[BPR-1815]]. The review bar's view controls.
		if (HeaderSourceAction != nullptr) {
			HeaderSourceAction->setIcon(strip_icon(strip_glyph::header_source, ink));
		}
		if (OnePaneAction != nullptr) {
			// Cleared so the layout pass paints it again in the new ink.
			OnePaneAction->setIcon(QIcon{ });
			apply_one_pane();
		}
		if (ToggleDrawersAction != nullptr) {
			// Painted again in the new ink by the next projection's pass.
			ToggleDrawersAction->setIcon(QIcon{ });
			apply_drawers_toggle();
		}
		if (GithubRefreshAction != nullptr) {
			GithubRefreshAction->setIcon(strip_icon(strip_glyph::refresh, ink));
		}
		if (QueueRefreshAction != nullptr) {
			QueueRefreshAction->setIcon(strip_icon(strip_glyph::refresh, ink));
		}
		// [[BPR-1575]]. A refresh under way keeps its own look in the new ink.
		present_refresh();
		if (AccountAction != nullptr) {
			AccountAction->setIcon(strip_icon(strip_glyph::account, ink));
		}
		if (SettingsAction != nullptr) {
			SettingsAction->setIcon(strip_icon(strip_glyph::settings, ink));
		}
	}

	void workbench_window::set_progress_mark_colours(const progress_mark_colours& aColours)
	{
		ProgressMarkColours = aColours;
		refresh_progress_icons();
	}

	auto workbench_window::shown_progress_mark_colours() const -> progress_mark_colours
	{
		if (ProgressMarkColours.has_value()) {
			return *ProgressMarkColours;
		}
		const auto dark = palette().color(QPalette::Active, QPalette::Window).lightness() < 128;
		return builtin_progress_mark_colours(dark ? core::appearance_variant::dark : core::appearance_variant::light);
	}

	void workbench_window::refresh_progress_icons()
	{
		// [[BPR-1331]] v003. The owner rejected two pictures of one state
		// ("Pick one. I think the circles here is probably better overall but
		// it should get some color. All green aint the best either."), so the
		// button and its menu paint the same marks Overview and the Files
		// drawer do. The menu is the family's legend: each action shows the
		// mark it is about, Seen, Read, Accepted and Excluded, whatever its
		// label says now.
		const auto colours  = shown_progress_mark_colours();
		const auto disabled = palette().color(QPalette::Disabled, QPalette::ButtonText);
		if (ReviewControlsButton != nullptr) {
			ReviewControlsButton->setIcon(progress_mark_icon(shown_progress_mark(Receipt, LedgerUnavailable), colours, disabled));
		}
		// [[BPR-1578]]. The track's marks and fills follow the theme's colours.
		update_progress_track();
		if (ReviewControlsMenu == nullptr) {
			return;
		}
		constexpr auto marks = std::array{
			core::file_progress_mark::seen,
			core::file_progress_mark::read,
			core::file_progress_mark::accepted,
			core::file_progress_mark::excluded,
		};
		const auto& menu = ReviewControlsMenu->palette();
		for (std::size_t index = 0U; index < ReceiptActions.size() && index < marks.size(); ++index) {
			if (ReceiptActions[index] != nullptr) {
				ReceiptActions[index]->setIcon(progress_mark_icon(marks[index], colours, menu.color(QPalette::Disabled, QPalette::Text), menu.color(QPalette::Active, QPalette::Window)));
			}
		}
	}

	void workbench_window::apply_focus_order()
	{
		const auto order = focus_order();
		if (order.isEmpty()) {
			return;
		}
		for (auto index = qsizetype{ 1 }; index < order.size(); ++index) {
			auto* const previous = order[index - 1];
			auto* const next     = order[index];
			// setTabOrder moves its second argument. A stop inside a composed
			// region is left exactly where RFC-BPR-0003 and RFC-BPR-0004 put it,
			// so the shell orders its own chrome around those regions rather than
			// replacing their internal order with one it decided.
			if (EpochSelectors->isAncestorOf(next)
				|| ComparisonSurface->isAncestorOf(next)) {
				continue;
			}
			QWidget::setTabOrder(previous, next);
		}
		++Statistics.FocusOrderUpdates;
	}

	void workbench_window::update_action_state()
	{
		update_header_source_action();
		update_source_find_actions();
		// One projection for all four primary actions. Entry `i` of the core's
		// delta decides action `i`; the window adds no legality of its own and
		// keeps no second carrier for the change actions.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			const auto projection
				= command(static_cast<core::workbench_command_id>(index));
			const auto reason = projection
				? projection->DisabledReason
				: core::workbench_reason_code::no_active_workbench;
			// [[BPR-1328]]. An arrow at its bound stays enabled and names the
			// bound on the tooltip's second line.
			const auto enabled = projection && projection->Enabled;
			const auto shown   = enabled ? projection->Bound : reason;
			const auto detail  = index < file_navigation_entries ? reason_text(shown) : change_reason_text(shown);
			describe_navigation_action(Navigation[index].Action,
									   Navigation[index].Name,
									   enabled,
									   Navigation[index].Purpose,
									   detail);
		}
		describe_change_ends();

		update_drawer_action(0U,
							 core::workbench_command_id::toggle_files_drawer,
							 DrawerStates.Files);
		// The presented files state depends on whether the command is offered.
		if (Drawers[0].Panel->state() != presented_files_state()) {
			apply_drawer_layout();
		}
		update_drawer_action(
			1U,
			core::workbench_command_id::toggle_conversations_drawer,
			DrawerStates.Conversations);
		update_drawer_action(
			2U,
			core::workbench_command_id::toggle_repositories_drawer,
			DrawerStates.Repositories);
		if (Drawers[2].Panel->state() != presented_repositories_state()) {
			apply_drawer_layout();
		}
		apply_line_numbers();
		apply_one_pane();
		apply_drawers_toggle();
		apply_updates_line();
		update_receipt_actions();
		update_new_comment_group();
		// Last, and deliberately. The mode is not a second opinion about a
		// command's legality; it is the answer that stands once the core's
		// projections have had theirs.
		apply_public_preview_availability();
	}

	void workbench_window::update_accept_file_shortcut()
	{
		// [[BPR-1515]]. Accept's retained action becomes Revoke after
		// persistence, without this binding. Removing it while editing also
		// yields native menu equivalents without changing receipt eligibility.
		// [[BPR-1531]]. The key is Option-A (Alt+A), the owner's replacement
		// for Command-Shift-A. Option-A types "å", so the binding steps aside
		// for every widget that takes typed text, not only line and text
		// edits: the Cocoa plugin offers the key to the shortcut map before
		// the text input system sees it.
		auto* const       action  = ReceiptActions[2];
		const auto* const focused = QApplication::focusWidget();
		const auto        editing = focused != nullptr && isAncestorOf(focused) && takes_typed_text(focused);
		const auto        accepts = !Receipt || Receipt->Actions[2].Command.Kind == core::file_receipt_command_kind::accept;
		action->setShortcut(accepts && !editing ? accept_file_key() : QKeySequence{ });
		const auto key = action->shortcut().toString(QKeySequence::NativeText);
		action->setToolTip(key.isEmpty() ? action->statusTip()
										 : QStringLiteral("Accept file (%1)\n%2").arg(key, action->statusTip()));
		if (AcceptFileButton != nullptr) {
			AcceptFileButton->setAccessibleDescription(action->toolTip());
		}
	}

	void workbench_window::update_typing_keys()
	{
		const auto* const focused = QApplication::focusWidget();
		const auto        typing  = focused != nullptr && isAncestorOf(focused) && takes_typed_text(focused);
		if (typing == TypingFocus) {
			return;
		}
		TypingFocus = typing;
		update_accept_file_shortcut();
		update_show_thread_actions();
		if (!option_types_text || PrimaryMenuBar == nullptr) {
			return;
		}
		// [[BPR-1536]]. AppKit fires a native menu item's key equivalent
		// before Qt sees the key, with no ShortcutOverride ([[BPR-1295]]), so
		// the menu bar's Option+character keys are lifted while the keyboard
		// is in a widget that takes typed text, and restored after.
		if (!TypingFocus) {
			for (const auto& [action, keys] : LiftedOptionKeys) {
				if (action != nullptr) {
					action->setShortcuts(keys);
				}
			}
			LiftedOptionKeys.clear();
			return;
		}
		auto pending = QList<QAction*>{ PrimaryMenuBar->actions() };
		while (!pending.isEmpty()) {
			auto* const action = pending.takeLast();
			if (auto* const menu = action->menu(); menu != nullptr) {
				pending.append(menu->actions());
				continue;
			}
			const auto keys = action->shortcuts();
			auto       kept = QList<QKeySequence>{ };
			std::ranges::copy_if(keys, std::back_inserter(kept), [](const QKeySequence& aKey) {
				return aKey.count() != 1 || !option_character_key(aKey[0]);
			});
			if (kept.size() != keys.size()) {
				LiftedOptionKeys.emplace_back(action, keys);
				action->setShortcuts(kept);
			}
		}
	}

	auto workbench_window::presented_key(const QAction* const aAction) const -> QString
	{
		const auto lifted = std::ranges::find_if(LiftedOptionKeys, [aAction](const auto& aLifted) { return aLifted.first == aAction; });
		const auto key    = lifted != LiftedOptionKeys.end() && !lifted->second.isEmpty() ? lifted->second.front() : aAction->shortcut();
		return key.toString(QKeySequence::NativeText);
	}

	void workbench_window::update_receipt_actions()
	{
		const auto disabled = LedgerUnavailable
			? QStringLiteral("Private review progress is unavailable.")
			: QStringLiteral("Select an available text comparison first.");
		for (std::size_t index = 0U; index < ReceiptActions.size(); ++index) {
			auto* const action = ReceiptActions[index];
			if (action == nullptr) {
				continue;
			}
			const auto projected = Receipt.has_value()
					&& index < Receipt->Actions.size()
				? &Receipt->Actions[index]
				: nullptr;
			const auto kind      = projected != nullptr
				? projected->Command.Kind
				: index == 0U
				? core::file_receipt_command_kind::mark_seen
				: index == 1U
				? core::file_receipt_command_kind::mark_read
				: index == 2U
				? core::file_receipt_command_kind::accept
				: core::file_receipt_command_kind::exclude;
			const auto label     = [&] {
				switch (kind) {
				case core::file_receipt_command_kind::mark_seen:
					return QStringLiteral("&Seen");
				case core::file_receipt_command_kind::mark_read:
					return QStringLiteral("&Read");
				// [[BPR-1701]]. Seen's inverse, which takes Read off too,
				// and Read's, which leaves the file seen.
				case core::file_receipt_command_kind::mark_unread:
					return QStringLiteral("Mark u&nseen");
				case core::file_receipt_command_kind::unmark_read:
					return QStringLiteral("Mark &unread");
				case core::file_receipt_command_kind::accept:
					return QStringLiteral("&Accept");
				case core::file_receipt_command_kind::revoke_acceptance:
					return QStringLiteral("Revoke &acceptance");
				case core::file_receipt_command_kind::exclude:
					return QStringLiteral("E&xclude");
				case core::file_receipt_command_kind::include:
					return QStringLiteral("&Include");
				}
				return QStringLiteral("Private progress");
			}();
			action->setText(label);
			action->setIconText(QString{ label }.remove(QLatin1Char{ '&' }));
			const auto checked = Receipt.has_value()
				&& (index == 0U
						? Receipt->Receipt.Read != core::file_read_state::unread
						: index == 1U
						? Receipt->Receipt.Read == core::file_read_state::read
						: index == 2U ? Receipt->Receipt.Accepted
									  : Receipt->Receipt.Excluded);
			action->setChecked(checked);
			const auto purpose = [&] {
				switch (kind) {
				case core::file_receipt_command_kind::mark_seen:
					return QStringLiteral("Mark this file seen.");
				case core::file_receipt_command_kind::mark_unread:
					return QStringLiteral("Mark this file unseen. Read comes off with it.");
				case core::file_receipt_command_kind::mark_read:
					return QStringLiteral("Mark this file read.");
				case core::file_receipt_command_kind::unmark_read:
					return QStringLiteral("Mark this file unread. It stays seen.");
				case core::file_receipt_command_kind::accept:
					return QStringLiteral("Accept this file privately.");
				case core::file_receipt_command_kind::revoke_acceptance:
					return QStringLiteral("Revoke private acceptance for this file.");
				case core::file_receipt_command_kind::exclude:
				case core::file_receipt_command_kind::include:
					break;
				}
				return QStringLiteral("Update private progress for the selected exact file material.");
			}();
			describe_action(
				action,
				projected != nullptr && projected->Enabled && !LedgerUnavailable,
				purpose,
				index == 2U && Receipt.has_value() && Receipt->Receipt.Excluded && !LedgerUnavailable
					? QStringLiteral("Include this file from Private progress before accepting it.")
					: disabled);
		}
		update_accept_file_shortcut();
		if (AcceptFileButton != nullptr) {
			AcceptFileButton->setAccessibleName(ReceiptActions[2]->isChecked()
													? QStringLiteral("Revoke file acceptance")
													: QStringLiteral("Accept file"));
			AcceptFileButton->setAccessibleDescription(ReceiptActions[2]->toolTip());
		}
		// [[BPR-1129]] [[BPR-1331]]. The review bar's progress button shows the
		// selected file's mark, the one the core decided, as its icon, and says
		// it in its name and tooltip with the key and what a click offers. The
		// owner, 2026-10-08: "I think the toolbar for seen read accept needs to
		// update its icon to the current state".
		if (ReviewControlsButton != nullptr) {
			const auto mark  = shown_progress_mark(Receipt, LedgerUnavailable);
			auto       state = QString{ };
			auto       word  = QString{ };
			if (LedgerUnavailable) {
				state = QStringLiteral("Private review progress is unavailable.");
				word  = QStringLiteral("unavailable");
			} else if (!mark.has_value()) {
				state = QStringLiteral("No file comparison is selected.");
				word  = QStringLiteral("no file selected");
			} else {
				const auto& receipt = Receipt->Receipt;
				word                = QString::fromUtf8(core::name(*mark));
				auto facts          = QStringList{ QString::fromUtf8(core::name(receipt.Read)) };
				if (receipt.Accepted) {
					facts.push_back(QStringLiteral("accepted"));
				}
				if (receipt.Excluded) {
					facts.push_back(QStringLiteral("excluded"));
				}
				state = facts.size() == 1 ? QStringLiteral("This file is %1.").arg(facts.front())
										  : QStringLiteral("This file is %1 and %2.").arg(facts.mid(0, facts.size() - 1).join(QStringLiteral(", ")), facts.back());
			}
			// What a click offers: the actions the core enabled, by their current
			// names; with none enabled, the four the menu holds.
			auto offered = QStringList{ };
			auto every   = QStringList{ };
			for (auto* const action : ReceiptActions) {
				if (action == nullptr) {
					continue;
				}
				const auto label = QString{ action->text() }.remove(QLatin1Char{ '&' });
				every.push_back(label);
				if (action->isEnabled()) {
					offered.push_back(label);
				}
			}
			const auto listed = [](const QStringList& aLabels) {
				return aLabels.size() <= 1 ? aLabels.join(QString{ })
										   : QStringLiteral("%1 or %2").arg(aLabels.mid(0, aLabels.size() - 1).join(QStringLiteral(", ")), aLabels.back());
			};
			const auto click = offered.isEmpty() ? QStringLiteral("Click to open %1, none available now.").arg(listed(every))
												 : QStringLiteral("Click to choose %1.").arg(listed(offered));
			const auto key   = ReviewControlsButton->shortcut().toString(QKeySequence::NativeText);
			ReviewControlsButton->setIcon(progress_mark_icon(mark, shown_progress_mark_colours(), palette().color(QPalette::Disabled, QPalette::ButtonText)));
			ReviewControlsButton->setAccessibleName(QStringLiteral("Private progress: %1").arg(word));
			ReviewControlsButton->setToolTip(QStringLiteral("Private progress: %1 (%2)\n%3\n%4").arg(word, key, state, click));
			ReviewControlsButton->setStatusTip(state);
			ReviewControlsButton->setAccessibleDescription(QStringLiteral("%1 %2").arg(state, click));
		}
		update_progress_track();
	}

	auto workbench_window::progress_step_offered(const std::size_t aIndex) const -> bool
	{
		// [[BPR-1578]]. The command is the one the core projected for that
		// action, never one the track makes up. [[BPR-1701]]. The owner,
		// 2026-10-21: "In the toolbar I can uncheck accepted by clicking the
		// accepted button. I should be able to do the same for read and
		// seen." A step not reached checks it (Mark seen, Mark read); a step
		// reached unchecks it (Mark unseen, Mark unread), as Accept toggles to
		// Revoke. An excluded file still rests the track.
		if (LedgerUnavailable || !Receipt.has_value() || Receipt->Receipt.Excluded || aIndex >= Receipt->Actions.size()) {
			return false;
		}
		const auto& action  = Receipt->Actions[aIndex];
		const auto  kind    = action.Command.Kind;
		const auto  stepped = aIndex == progress_track::seen_step
			? kind == core::file_receipt_command_kind::mark_seen || kind == core::file_receipt_command_kind::mark_unread
			: kind == core::file_receipt_command_kind::mark_read || kind == core::file_receipt_command_kind::unmark_read;
		return action.Enabled && stepped;
	}

	void workbench_window::take_progress_step(const std::size_t aIndex)
	{
		if ((aIndex == progress_track::seen_step || aIndex == progress_track::read_step) && progress_step_offered(aIndex)) {
			dispatch_receipt(aIndex);
			return;
		}
		// A checkable step toggles itself on a click. The receipt decides its
		// checked state, so a click that dispatched nothing puts it back.
		update_progress_track();
	}

	void workbench_window::update_progress_track()
	{
		if (ProgressTrack == nullptr) {
			return;
		}
		auto shown = progress_track::state{ };
		if (Receipt.has_value() && !LedgerUnavailable) {
			const auto& receipt = Receipt->Receipt;
			shown.Excluded      = receipt.Excluded;
			shown.Live          = !receipt.Excluded;
			shown.Seen          = receipt.Read != core::file_read_state::unread;
			shown.Read          = receipt.Read == core::file_read_state::read;
			shown.Accepted      = receipt.Accepted;
		}
		ProgressTrack->set_state(shown, shown_progress_mark_colours());
		const auto unavailable = LedgerUnavailable ? QStringLiteral("Private review progress is unavailable.")
			: !Receipt.has_value()                 ? QStringLiteral("Select an available text comparison first.")
												   : QStringLiteral("Include this file from Private progress first.");
		const auto describe    = [&](QToolButton* const aStep, const std::size_t aIndex, const QString& aDone, const QString& aOffer) {
			if (aStep == nullptr) {
				return;
			}
			const auto offered = progress_step_offered(aIndex);
			const auto reached = aIndex == progress_track::seen_step ? shown.Seen : shown.Read;
			aStep->setEnabled(offered);
			// [[BPR-1701]]. Checked is the saved receipt's fact, so a
			// screen reader hears it too, and a click waits for persistence.
			aStep->setChecked(reached);
			const auto tip = !shown.Live ? unavailable : reached ? aDone
																 : aOffer;
			aStep->setToolTip(tip);
			aStep->setStatusTip(tip);
			aStep->setAccessibleDescription(tip);
		};
		describe(SeenStepButton,
				 progress_track::seen_step,
				 shown.Read ? QStringLiteral("This file is seen. Click to mark it unseen, which clears Read too.")
							: QStringLiteral("This file is seen. Click to mark it unseen."),
				 QStringLiteral("Mark this file seen."));
		describe(ReadStepButton, progress_track::read_step, QStringLiteral("This file is read. Click to mark it unread; it stays seen."), QStringLiteral("Mark this file read."));
	}

	void workbench_window::update_drawer_action(
		const std::size_t                  aIndex,
		const core::workbench_command_id   aId,
		const core::workbench_drawer_state aState)
	{
		const auto& entry      = Drawers[aIndex];
		const auto  projection = command(aId);
		const auto  expanded   = aState == core::workbench_drawer_state::expanded;
		entry.Toggle->setChecked(expanded);
		// [[BPR-0727]]. In Overview the toggle stays enabled, as the strip
		// requires, and says what the gesture will meet: the core keeps every
		// drawer collapsed until another modality is chosen.
		const auto enabledText = expanded ? entry.Collapse
			: current_modality() == review_modality::overview
			? reason_text(core::workbench_reason_code::overview_keeps_drawers_collapsed)
			: entry.Expand;
		// [[BPR-0675]]. A drawer's room is the reviewer's before the core has
		// published any command at all: a pull request whose comparison never
		// arrived publishes none, and the toggles were dead for exactly the
		// reviewer who most wanted the room back.
		describe_action(
			entry.Toggle,
			projection ? projection->Enabled : true,
			enabledText,
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Toggle));
			button != nullptr) {
			button->setAccessibleDescription(entry.Toggle->toolTip());
		}
	}

	void workbench_window::update_status_text()
	{
		update_review_readiness();
		StatusLabel->setText(status_line());
		update_home_status();
		update_strip_positions();
		update_rail_summaries();
		update_home_landing();
	}

	void workbench_window::update_review_readiness()
	{
		if (EpochSelectors == nullptr || SessionPanel == nullptr) {
			return;
		}
		auto wording = QString{ };
		auto busy    = false;
		if (SessionPanel->status() == core::github_status::loading_pull_request) {
			busy       = true;
			auto stage = QStringLiteral("pull request details");
			if (ReadProgress) {
				switch (ReadProgress->Phase) {
				case application::github_read_phase::metadata:              break;
				case application::github_read_phase::files:                 stage = QStringLiteral("changed files"); break;
				case application::github_read_phase::commits:               stage = QStringLiteral("commits"); break;
				case application::github_read_phase::reviews:               stage = QStringLiteral("reviews"); break;
				case application::github_read_phase::review_threads:        stage = QStringLiteral("review threads"); break;
				case application::github_read_phase::comments:              stage = QStringLiteral("comments"); break;
				case application::github_read_phase::checks:                stage = QStringLiteral("checks"); break;
				case application::github_read_phase::related_pull_requests: stage = QStringLiteral("related pull requests"); break;
				case application::github_read_phase::update_history:        stage = QStringLiteral("update history"); break;
				}
			}
			wording = QStringLiteral("Loading %1…").arg(stage);
			if (ReadProgress && ReadProgress->PagesReceived != 0U) {
				wording += QStringLiteral(" %1 pages received.").arg(ReadProgress->PagesReceived);
			}
		} else if (HasGithubPullRequest) {
			const auto epochStatus = EpochSelectors->status();
			busy                   = HasEpochState && (epochStatus == core::epoch_status::loading_catalog || epochStatus == core::epoch_status::preparing || epochStatus == core::epoch_status::committing || epochStatus == core::epoch_status::resolving || epochStatus == core::epoch_status::maintaining);
			if (busy) {
				wording = QStringLiteral("Pull request loaded. Preparing file comparisons… %1").arg(EpochSelectors->status_text());
			} else if (EpochSelectors->comparison_available()) {
				wording = QStringLiteral("Ready to review. Select a file to load its comparison.");
			} else {
				wording = QStringLiteral("File comparisons unavailable. %1 Refresh to try again.").arg(EpochSelectors->status_text());
			}
		}
		EpochSelectors->set_readiness(busy || !EpochSelectors->comparison_available() ? wording : QString{ }, updates_line_open());
		// [[BPR-1576]]. A wait on the Updates line turns before its words.
		EpochSelectors->set_readiness_busy(busy, pending_full_motion());
		if (OverviewPage != nullptr) {
			OverviewPage->set_read_waiting(read_stage_words());
		}
	}

	auto workbench_window::status_line() const -> QString
	{
		// [[BPR-1157]]. A local cycle is not waiting for a pull request; the
		// cycle page says what it is doing itself.
		if (showing_local_cycle()) {
			return { };
		}
		auto status = QString{ };
		// [[BPR-0775]]. On Home the line is the session's, and never that no
		// pull request is open: Home is what the window shows when none is. A
		// host's own failure is still said after it.
		const auto home = showing_home();
		// [[BPR-0668]]. A cursor is a position in a comparison, and commands
		// are a workbench; neither is a pull request. Saying nothing was open
		// while the strip, the drawer and the conversations all named one told
		// the owner the product was broken and gave them nothing to do.
		if (!home && SessionPanel->status() == core::github_status::loading_pull_request) {
			status = EpochSelectors->readiness_text();
		} else if (!home && HasCachedOverview) {
			status = QStringLiteral("Cached review data; freshness unknown. Current review state is unavailable offline.");
		} else if (!home && HasGithubPullRequest && SessionPanel->status() == core::github_status::pull_request_failed) {
			// A failed refresh retains the accepted observation. State its
			// freshness beside the content, including when its diff is ready.
			status = QStringLiteral("Refresh failed. Showing previously loaded review data; freshness is unknown.");
		} else if (home) {
			status = home_session_line();
		} else if (HasGithubPullRequest && (!HasCommands || !Cursor)) {
			// [[BPR-0677]]. Work in progress is not failure. The owner'''s
			// comparison spent ninety-five seconds in revision_prepare while
			// the line told them to refresh, which would have thrown that
			// work away and started it again.
			const auto working = HasEpochState
				&& (EpochSelectors->status() == core::epoch_status::loading_catalog
					|| EpochSelectors->status() == core::epoch_status::preparing
					|| EpochSelectors->status() == core::epoch_status::committing
					|| EpochSelectors->status() == core::epoch_status::resolving
					|| EpochSelectors->status() == core::epoch_status::maintaining);
			status = working
				? QStringLiteral(
					  "This pull request is open. Its comparison is being prepared.")
				: QStringLiteral(
					  "This pull request is open. Its comparison is not ready; "
					  "Refresh to try again.");
		} else if (!HasCommands) {
			status = QStringLiteral("Open a pull request to begin reviewing.");
		} else if (!Cursor) {
			status = QStringLiteral("No pull request is open in the workbench.");
		} else if (Cursor->ActionableCount == 0U) {
			status = QStringLiteral("This pull request has no actionable files.");
		} else if (Cursor->Ordinal == 0U && !Cursor->File.has_value()) {
			// [[BPR-1287]]. Nothing is chosen, which is not a file outside the list.
			status = QStringLiteral("No file is open. Choose one of the %1 actionable file(s).")
						 .arg(Cursor->ActionableCount);
		} else if (Cursor->Ordinal == 0U) {
			status = QStringLiteral(
						 "The selected file is outside the %1 actionable file(s).")
						 .arg(Cursor->ActionableCount);
		} else {
			status = QStringLiteral("Actionable file %1 of %2.")
						 .arg(Cursor->Ordinal)
						 .arg(Cursor->ActionableCount);
		}
		if (Failure) {
			status.append(QStringLiteral(" %1").arg(failure_text(*Failure, EpochSelectors)));
		}
		// [[BPR-0742]]. Retained, so the projection that resets `Failure` does not
		// silence it; said once when a host failure already says the same.
		if (SavedLayoutUnavailable
			&& Failure != application::workbench_host_failure::settings_unavailable) {
			status.append(QStringLiteral(" %1").arg(failure_text(
				application::workbench_host_failure::settings_unavailable)));
		}
		// [[BPR-0777]]. A profile refusal is this window's to say on whatever it is
		// showing, because RFC-BPR-0033 routes it here instead of presenting the chooser.
		if (ProfileFailure) {
			status.append(QStringLiteral(" %1").arg(profile_management_failure_text(*ProfileFailure)));
		}
		// [[BPR-0817]]. Said on whatever the window shows, Home included, because
		// the agent panel, which also says it, is not showing at Home.
		if (AgentWorkspacesUnavailable) {
			status.append(QStringLiteral(" %1").arg(agent_workspaces_problem_text()));
		}
		if (home) {
			return status;
		}
		if (LedgerUnavailable) {
			status.append(QStringLiteral(" Private review progress is unavailable."));
		} else if (ReceiptFailure.has_value()) {
			status.append(QStringLiteral(" %1").arg(receipt_failure_text(*ReceiptFailure)));
		} else if (Receipt.has_value()) {
			status.append(QStringLiteral(" Private progress: %1, %2%3.")
							  .arg(Receipt->Receipt.Read == core::file_read_state::read
									   ? QStringLiteral("read")
									   : Receipt->Receipt.Read == core::file_read_state::seen
									   ? QStringLiteral("seen")
									   : QStringLiteral("unread"))
							  .arg(Receipt->Receipt.Accepted
									   ? QStringLiteral("accepted")
									   : QStringLiteral("not accepted"))
							  .arg(Receipt->Receipt.Excluded
									   ? QStringLiteral(", excluded")
									   : QString{ }));
		}
		if (HasEpochState) {
			// Quoted, never restated. Review-history loading, failure,
			// cancellation, and empty history are decided and worded by the
			// composed control; the shell repeats that exact sentence so its one
			// status line states the whole workbench state in text rather than in
			// enabled state or colour alone.
			const auto epochs = EpochSelectors->status_text();
			if (!epochs.isEmpty()) {
				status.append(QStringLiteral(" %1").arg(epochs));
			}
		}
		return status;
	}

	void workbench_window::announce_update()
	{
		const auto announcement = StatusLabel->text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		++Statistics.Announcements;
		QAccessibleAnnouncementEvent event{ this, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto workbench_window::command(const core::workbench_command_id aId) const
		-> std::optional<core::workbench_command_projection>
	{
		if (!HasCommands) {
			return std::nullopt;
		}
		const auto index = static_cast<std::size_t>(aId);
		if (index >= Commands.Commands.size()
			|| Commands.Commands[index].Id != aId) {
			return std::nullopt;
		}
		return Commands.Commands[index];
	}
}
