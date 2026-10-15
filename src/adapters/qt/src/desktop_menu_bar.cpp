#include "bpr/qt/desktop_menu_bar.hpp"

#include <array>
#include <utility>

#include <QAction>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QVariant>

#include <algorithm>
#include <tuple>
#include <utility>

namespace bpr::qt {
	desktop_menu_bar::desktop_menu_bar()
		: QMenuBar{ nullptr }
	{
		setObjectName(QStringLiteral("desktopMenuBar"));
		setAccessibleName(QStringLiteral("BetterPullReview menu bar"));
		setAccessibleDescription(QStringLiteral("Profiles and application menus while no window is open"));
		auto* fileMenu = addMenu(QStringLiteral("&File"));
		fileMenu->setAccessibleName(QStringLiteral("File"));
		QuitProfilesAction = fileMenu->addAction(QStringLiteral("Quit"));
		QuitProfilesAction->setObjectName(QStringLiteral("desktopQuitProfilesAction"));
		QuitProfilesAction->setMenuRole(QAction::QuitRole);
		QuitProfilesAction->setShortcut(QKeySequence::Quit);
		QuitProfilesAction->setEnabled(false);
		connect(QuitProfilesAction, &QAction::triggered, this, [this] {
			if (QuitProfiles) {
				QuitProfiles();
			}
		});
		// [[BPR-1250]]. The window's local Review Cycles entry ([[BPR-1157]]),
		// offered and disabled with the rest of the window's commands.
		auto* const openProject = fileMenu->addAction(QStringLiteral("Open Project…"));
		openProject->setObjectName(QStringLiteral("desktopOpenProjectAction"));
		openProject->setEnabled(false);
		// [[BPR-1195]]. Keep the workbench's tab command after Quit moves to
		// the macOS application menu; otherwise the now-empty File menu vanishes.
		auto* const closeTab = fileMenu->addAction(tr("Close Tab"));
		closeTab->setObjectName(QStringLiteral("desktopCloseDocumentTabAction"));
		closeTab->setShortcut(QKeySequence{ QStringLiteral("Ctrl+W") });
		closeTab->setEnabled(false);
		ProfilesMenu = addMenu(QStringLiteral("&Profiles"));
		ProfilesMenu->setObjectName(QStringLiteral("desktopProfilesMenu"));
		ProfilesMenu->setAccessibleName(QStringLiteral("Profiles"));
		NewBprWindowAction = ProfilesMenu->addAction(QStringLiteral("New Window"));
		NewBprWindowAction->setObjectName(QStringLiteral("desktopNewBprWindowAction"));
		NewBprWindowAction->setShortcut(QKeySequence::New);
		NewBprWindowAction->setEnabled(false);
		connect(NewBprWindowAction, &QAction::triggered, this, [this] {
			if (ProfileManagementDispatch && CurrentBprProfile.value() != 0U) {
				ProfileManagementDispatch(application::new_bpr_window_intent{ CurrentBprProfile });
			}
		});
		ManageProfilesAction = ProfilesMenu->addAction(QStringLiteral("Manage Profiles…"));
		ManageProfilesAction->setObjectName(QStringLiteral("desktopManageProfilesAction"));
		ManageProfilesAction->setEnabled(false);
		connect(ManageProfilesAction, &QAction::triggered, this, [this] {
			if (ManageProfiles) {
				ManageProfiles();
			}
		});
		// [[BPR-1035]]. The workbench window's Pull Request menu ([[BPR-0990]]),
		// offered here and disabled for the same reason as View below: without it
		// the last close took Pull Request off the bar. Follow and Save Repository
		// act on an open pull request, and with no window none is open.
		PullRequestMenu = addMenu(QStringLiteral("P&ull Request"));
		PullRequestMenu->setObjectName(QStringLiteral("desktopPullRequestMenu"));
		PullRequestMenu->setAccessibleName(QStringLiteral("Pull Request"));
		PullRequestMenu->setAccessibleDescription(QStringLiteral(
			"Open the pull request on GitHub, or keep it or its repository in my queue, unavailable while no window is open"));
		// [[BPR-1338]]. Open on GitHub, first as in the window's menu, on the
		// window's key. It opens the window's pull request, and with no window
		// there is none to open.
		auto* const openOnGithub = PullRequestMenu->addAction(tr("Open on &GitHub"));
		openOnGithub->setObjectName(QStringLiteral("desktopOpenOnGithubAction"));
		openOnGithub->setShortcut(QKeySequence{ QStringLiteral("Ctrl+Shift+O") });
		openOnGithub->setStatusTip(QStringLiteral("Open the pull request's page on GitHub, unavailable while no window is open"));
		openOnGithub->setEnabled(false);
		PullRequestMenu->addSeparator();
		auto* const follow = PullRequestMenu->addAction(QStringLiteral("&Follow"));
		follow->setObjectName(QStringLiteral("desktopFollowPullRequestAction"));
		follow->setCheckable(true);
		follow->setEnabled(false);
		auto* const saveRepository = PullRequestMenu->addAction(QStringLiteral("&Save Repository"));
		saveRepository->setObjectName(QStringLiteral("desktopSavePullRequestRepositoryAction"));
		saveRepository->setEnabled(false);
		PullRequestMenu->addSeparator();
		auto* const composeReview = PullRequestMenu->addAction(tr("Compose &review"));
		composeReview->setObjectName(QStringLiteral("desktopComposeReviewAction"));
		composeReview->setShortcut(QKeySequence{ QStringLiteral("Alt+R") });
		composeReview->setEnabled(false);
		auto* const comment = PullRequestMenu->addAction(tr("Co&mment"));
		comment->setObjectName(QStringLiteral("desktopComposeInlineDraftAction"));
		comment->setShortcut(QKeySequence{ QStringLiteral("Alt+C") });
		comment->setEnabled(false);
		// [[BPR-1250]]. And the review tour's two commands (RFC-BPR-0045).
		auto* const prepareTour = PullRequestMenu->addAction(tr("Prepare review &tour"));
		prepareTour->setObjectName(QStringLiteral("desktopPrepareReviewTourAction"));
		prepareTour->setShortcut(QKeySequence{ QStringLiteral("Alt+T") });
		prepareTour->setEnabled(false);
		auto* const addTourStop = PullRequestMenu->addAction(tr("Add tour stop"));
		addTourStop->setObjectName(QStringLiteral("desktopAddTourStopAction"));
		addTourStop->setShortcut(QKeySequence{ QStringLiteral("Alt+Shift+T") });
		addTourStop->setEnabled(false);
		// [[BPR-0796]]. The workbench window's View menu, offered here and disabled.
		// Without it the last close took View off the bar and the merged Preferences
		// item out of the application menu, leaving one visible menu where
		// RFC-BPR-0033 B1 names four; the owner rejected exactly that on 2026-09-16.
		// Titles, order and roles mirror `workbench_window::build_menu_bar` so the
		// close subtracts nothing a reviewer can see. Nothing is connected: with no
		// window there is no presentation these could change, and this bar performs
		// no effect of its own.
		ViewMenu = addMenu(QStringLiteral("&View"));
		ViewMenu->setObjectName(QStringLiteral("desktopViewMenu"));
		ViewMenu->setAccessibleName(QStringLiteral("View"));
		// [[BPR-1493]]. The window's Find submenu in the window's order (the pane's
		// three commands, then the pull request's three), and the submenu itself
		// disabled like every other item here: no window, nothing to search.
		auto* const findMenu = ViewMenu->addMenu(tr("&Find"));
		findMenu->setObjectName(QStringLiteral("desktopSourceFindMenu"));
		findMenu->menuAction()->setEnabled(false);
		const auto titles   = std::array{ tr("Find"), tr("Next pane match"), tr("Previous pane match"), tr("Find in Files"), tr("Next PR result"), tr("Previous PR result") };
		const auto findKeys = std::array{ QKeySequence{ QKeySequence::Find }, QKeySequence{ QStringLiteral("F3") }, QKeySequence{ QStringLiteral("Shift+F3") }, QKeySequence{ QStringLiteral("Ctrl+Shift+F") }, QKeySequence{ QStringLiteral("F4") }, QKeySequence{ QStringLiteral("Shift+F4") } };
		for (std::size_t index = 0U; index < titles.size(); ++index) {
			auto* action = findMenu->addAction(titles[index]);
			action->setShortcuts({ findKeys[index] });
#if defined(Q_OS_MACOS)
			if (index == 1U) {
				action->setShortcuts({ findKeys[index], QKeySequence{ QStringLiteral("Ctrl+G") } });
			}
			if (index == 2U) {
				action->setShortcuts({ findKeys[index], QKeySequence{ QStringLiteral("Ctrl+Shift+G") } });
			}
#endif
			action->setEnabled(false);
		}
		ViewMenu->setAccessibleDescription(
			QStringLiteral("Presentation options for the comparison surface, unavailable while no window is open"));
		// [[BPR-1493]]. The window's navigation texts, mnemonics included, so the
		// two View menus read alike.
		const auto navigation = std::array{
			std::pair{ QStringLiteral("&Previous file"), QStringLiteral("Ctrl+Alt+Up") },
			std::pair{ QStringLiteral("&Next file"), QStringLiteral("Ctrl+Alt+Down") },
			std::pair{ QStringLiteral("Pre&vious change"), QStringLiteral("Alt+Up") },
			std::pair{ QStringLiteral("Ne&xt change"), QStringLiteral("Alt+Down") },
		};
		for (const auto& [name, key] : navigation) {
			auto* const action = ViewMenu->addAction(name);
			action->setShortcut(QKeySequence{ key });
			action->setEnabled(false);
		}
		ViewMenu->addSeparator();
		auto* const counterpart = ViewMenu->addAction(QStringLiteral("Go to header/source"));
		counterpart->setObjectName(QStringLiteral("desktopGoToHeaderSourceAction"));
		counterpart->setShortcut(QKeySequence{ QStringLiteral("Alt+O") });
		counterpart->setStatusTip(QStringLiteral("Open a review window to go to a matching header or source file."));
		counterpart->setEnabled(false);
		ViewMenu->addSeparator();
		auto* const lineNumbers = ViewMenu->addAction(QStringLiteral("&Line numbers"));
		lineNumbers->setObjectName(QStringLiteral("desktopToggleLineNumbersAction"));
		lineNumbers->setCheckable(true);
		lineNumbers->setShortcut(QKeySequence{ QStringLiteral("Alt+Shift+5") });
		lineNumbers->setEnabled(false);
		auto* const onePane = ViewMenu->addAction(QStringLiteral("&One pane"));
		onePane->setObjectName(QStringLiteral("desktopToggleOnePaneAction"));
		onePane->setCheckable(true);
		onePane->setEnabled(false);
		// [[BPR-1334]]. Line Details, after One pane as in the window's menu.
		auto* const lineDetails = ViewMenu->addAction(QStringLiteral("Line &Details"));
		lineDetails->setObjectName(QStringLiteral("desktopToggleLineDetailsAction"));
		lineDetails->setCheckable(true);
		lineDetails->setEnabled(false);
		// [[BPR-1295]]. Hide Drawers, after One pane as in the window's menu.
		auto* const toggleDrawers = ViewMenu->addAction(QStringLiteral("Hide &Drawers"));
		toggleDrawers->setObjectName(QStringLiteral("desktopToggleDrawersAction"));
		toggleDrawers->setCheckable(true);
		toggleDrawers->setShortcut(QKeySequence{ QStringLiteral("Ctrl+B") });
		toggleDrawers->setEnabled(false);
		// [[BPR-1293]] [[BPR-1304]] [[BPR-1341]]. Threads, after Hide Drawers as in the
		// window's menu. BPR-1293 gave the window's View this item and not this
		// bar's, so the last close took it off the bar.
		auto* const threads = ViewMenu->addAction(QStringLiteral("&Threads"));
		threads->setObjectName(QStringLiteral("desktopShowThreadsAction"));
		threads->setShortcut(QKeySequence{ QStringLiteral("Ctrl+J") });
		threads->setEnabled(false);
		auto* const updatesLine = ViewMenu->addAction(QStringLiteral("&Updates Line"));
		updatesLine->setObjectName(QStringLiteral("desktopToggleUpdatesLineAction"));
		updatesLine->setCheckable(true);
		updatesLine->setEnabled(false);
		// [[BPR-1336]]. The window's zoom commands, after Updates Line as in the
		// window's menu, with the window's keys.
		ViewMenu->addSeparator();
		for (const auto& [text, name, keys] : {
				 std::tuple{ tr("Zoom In"), QStringLiteral("desktopZoomInCodeAction"), QList<QKeySequence>{ QKeySequence{ QStringLiteral("Ctrl+=") }, QKeySequence{ QStringLiteral("Ctrl++") } } },
				 std::tuple{ tr("Zoom Out"), QStringLiteral("desktopZoomOutCodeAction"), QList<QKeySequence>{ QKeySequence{ QStringLiteral("Ctrl+-") } } },
				 std::tuple{ tr("Actual Size"), QStringLiteral("desktopActualSizeCodeAction"), QList<QKeySequence>{ QKeySequence{ QStringLiteral("Ctrl+0") } } },
			 }) {
			auto* const zoom = ViewMenu->addAction(text);
			zoom->setObjectName(name);
			zoom->setShortcuts(keys);
			zoom->setEnabled(false);
		}
		ViewMenu->addSeparator();
		auto* const nextTab = ViewMenu->addAction(tr("Next Tab"));
		nextTab->setObjectName(QStringLiteral("desktopNextDocumentTabAction"));
		nextTab->setEnabled(false);
		auto* const previousTab = ViewMenu->addAction(tr("Previous Tab"));
		previousTab->setObjectName(QStringLiteral("desktopPreviousDocumentTabAction"));
		previousTab->setEnabled(false);
#if defined(Q_OS_MACOS)
		nextTab->setShortcut(QKeySequence{ QStringLiteral("Meta+Tab") });
		previousTab->setShortcut(QKeySequence{ QStringLiteral("Meta+Shift+Tab") });
#else
		nextTab->setShortcut(QKeySequence{ QStringLiteral("Ctrl+Tab") });
		previousTab->setShortcut(QKeySequence{ QStringLiteral("Ctrl+Shift+Tab") });
#endif
		// [[BPR-0646]] gives Settings the preferences role, which macOS merges into
		// the application menu. Offered so that menu keeps every item it had while a
		// window was open; disabled because Settings is a window's, and no window is
		// open to carry one.
		ViewMenu->addSeparator();
		auto* const settings = ViewMenu->addAction(QStringLiteral("&Settings…"));
		settings->setObjectName(QStringLiteral("desktopOpenSettingsAction"));
		settings->setMenuRole(QAction::PreferencesRole);
		settings->setShortcut(QKeySequence::Preferences);
		settings->setEnabled(false);
		// [[BPR-1250]]. The workbench window's Help menu (RFC-BPR-0027's
		// diagnostics), offered here and disabled for the same reason as View.
		// Without it the last close took Help off the bar on macOS, because the
		// only item the resident bar gave it, Check for Updates…, is hidden where
		// the application menu carries it natively. Diagnostics opens a window's
		// Settings and an export is a profile's, so with no window neither has an
		// owner. The object name is the one `release_update_menu::attach` looks
		// for, so Check for Updates… joins this menu as it joins the window's.
		HelpMenu = addMenu(QStringLiteral("&Help"));
		HelpMenu->setObjectName(QStringLiteral("releaseUpdateMenu"));
		HelpMenu->setAccessibleName(QStringLiteral("Help"));
		HelpMenu->setAccessibleDescription(
			QStringLiteral("Diagnostics and their export, unavailable while no window is open"));
		auto* const diagnostics = HelpMenu->addAction(QStringLiteral("Diagnostics…"));
		diagnostics->setObjectName(QStringLiteral("desktopOpenDiagnosticsAction"));
		diagnostics->setEnabled(false);
		HelpMenu->addSeparator();
		auto* const anonymized = HelpMenu->addAction(QStringLiteral("Export anonymized diagnostics…"));
		anonymized->setObjectName(QStringLiteral("desktopExportAnonymizedDiagnosticsAction"));
		anonymized->setEnabled(false);
		auto* const identifying = HelpMenu->addAction(QStringLiteral("Export with identifying details…"));
		identifying->setObjectName(QStringLiteral("desktopExportIdentifyingDiagnosticsAction"));
		identifying->setEnabled(false);
	}

	void desktop_menu_bar::set_dispatch(application::profile_management_dispatch aDispatch,
										std::function<void()> aManage, std::function<void()> aQuit)
	{
		ProfileManagementDispatch = std::move(aDispatch);
		ManageProfiles            = std::move(aManage);
		QuitProfiles              = std::move(aQuit);
		ManageProfilesAction->setEnabled(static_cast<bool>(ManageProfiles));
		QuitProfilesAction->setEnabled(static_cast<bool>(QuitProfiles));
	}

	void desktop_menu_bar::adopt(const application::profile_management_projection& aProjection,
								 const core::bpr_profile_id                        aCurrentProfile)
	{
		if (aProjection.Profiles.size() > core::maximum_bpr_profiles) {
			return;
		}
		CurrentBprProfile = aCurrentProfile;
		const auto live   = aProjection.Ready && !aProjection.Closing;
		// Unlike the window's menu, New Window is offered while the named profile has
		// no runtime: reopening the most recently used profile is this menu's job.
		NewBprWindowAction->setEnabled(live && aCurrentProfile.value() != 0U);
		for (auto iterator = ProfileActions.begin(); iterator != ProfileActions.end();) {
			if (std::ranges::none_of(aProjection.Profiles, [iterator](const auto& aProfile) { return aProfile.Profile == iterator->first; })) {
				delete iterator->second;
				iterator = ProfileActions.erase(iterator);
			} else {
				++iterator;
			}
		}
		auto* before = NewBprWindowAction;
		for (auto iterator = aProjection.Profiles.rbegin(); iterator != aProjection.Profiles.rend(); ++iterator) {
			const auto& profile    = *iterator;
			auto [entry, inserted] = ProfileActions.try_emplace(profile.Profile, nullptr);
			if (inserted) {
				entry->second = new QAction{ ProfilesMenu };
				entry->second->setCheckable(true);
				entry->second->setData(QVariant::fromValue<qulonglong>(profile.Profile.value()));
				entry->second->setObjectName(QStringLiteral("desktopOpenBprProfile_%1").arg(profile.Profile.value()));
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
			action->setEnabled(live && profile.Runtime != application::profile_management_runtime::closing);
			const auto actions = ProfilesMenu->actions();
			if (actions.indexOf(action) + 1 != actions.indexOf(before) || !actions.contains(action)) {
				ProfilesMenu->insertAction(before, action);
			}
			before = action;
		}
	}

	auto desktop_menu_bar::profiles_menu() const noexcept -> QMenu* { return ProfilesMenu; }
	auto desktop_menu_bar::new_window_action() const noexcept -> QAction* { return NewBprWindowAction; }
	auto desktop_menu_bar::manage_profiles_action() const noexcept -> QAction* { return ManageProfilesAction; }
	auto desktop_menu_bar::quit_action() const noexcept -> QAction* { return QuitProfilesAction; }
	auto desktop_menu_bar::view_menu() const noexcept -> QMenu* { return ViewMenu; }
	auto desktop_menu_bar::pull_request_menu() const noexcept -> QMenu* { return PullRequestMenu; }
	auto desktop_menu_bar::help_menu() const noexcept -> QMenu* { return HelpMenu; }
}
