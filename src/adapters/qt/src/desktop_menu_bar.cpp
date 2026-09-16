#include "bpr/qt/desktop_menu_bar.hpp"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QVariant>

#include <algorithm>
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
}
