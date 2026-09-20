#include "bpr/qt/release_update_panel.hpp"

#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace bpr::qt {
	namespace {
		using reason  = core::release_update_reason;
		using command = core::release_update_command;
		auto version(const std::optional<core::release_update_version>& aVersion) -> QString
		{
			return aVersion ? QStringLiteral("%1.%2.%3").arg(aVersion->Major).arg(aVersion->Minor).arg(aVersion->Patch)
							: QStringLiteral("Unavailable");
		}
		auto reason_text(reason aReason) -> QString
		{
			switch (aReason) {
			case reason::none:                   return QStringLiteral("Ready to check for updates.");
			case reason::current:                return QStringLiteral("You’re up to date.");
			case reason::skipped:                return QStringLiteral("This version is skipped for automatic checks.");
			case reason::disabled:               return QStringLiteral("Automatic checks are off. You can check manually.");
			case reason::managed:                return QStringLiteral("Updates are managed by your administrator.");
			case reason::unsupported_platform:   return QStringLiteral("Use the release page to update manually on this platform.");
			case reason::untrusted_installation: return QStringLiteral("Self-update requires a trusted production installation. Use the release page to update manually.");
			case reason::read_only:              return QStringLiteral("This installation is read-only. Update it manually.");
			case reason::translocated:           return QStringLiteral("Move the application to its installed location before updating.");
			case reason::mounted_image:          return QStringLiteral("Install the application outside its disk image before updating.");
			case reason::access_required:        return QStringLiteral("Select a signed-in native GitHub.com account with access to the release repository. GitHub CLI and enterprise accounts cannot be used.");
			case reason::unauthenticated:        return QStringLiteral("Release access has expired. Sign in again and select the account.");
			case reason::forbidden:              return QStringLiteral("The selected account cannot access this release.");
			case reason::inaccessible:           return QStringLiteral("The release is unavailable or inaccessible to the selected account.");
			case reason::offline:                return QStringLiteral("The release service could not be reached.");
			case reason::rate_limited:           return QStringLiteral("The release service has limited requests. Try again after its retry interval.");
			case reason::cancelled:              return QStringLiteral("Update cancelled. The installed application is unchanged.");
			case reason::timed_out:              return QStringLiteral("The update operation timed out.");
			case reason::installer_unavailable:  return QStringLiteral("The update is verified. Installation is not enabled in this build.");
			case reason::flush_failed:           return QStringLiteral("Local changes could not be saved. Installation was stopped.");
			case reason::quit_refused:           return QStringLiteral("The application could not quit safely. Installation was stopped.");
			case reason::recovery_required:      return QStringLiteral("The update needs recovery. The previous application has been retained.");
			case reason::shutdown:               return QStringLiteral("Update service is shutting down.");
			case reason::busy:                   return QStringLiteral("An update operation is already running.");
			case reason::not_due:                return QStringLiteral("The next automatic check is not due yet.");
			case reason::certificate:            return QStringLiteral("The release service’s secure connection could not be verified.");
			case reason::redirect:               return QStringLiteral("The release service used an unapproved download destination.");
			case reason::wrong_signer:           return QStringLiteral("The update’s publisher signature or notarization could not be verified.");
			case reason::digest_mismatch:        return QStringLiteral("The downloaded update does not match its digest.");
			case reason::unsafe_archive:         return QStringLiteral("The update archive contains unsafe entries.");
			case reason::invalid_bundle:         return QStringLiteral("The update has an invalid application identity or layout.");
			case reason::wrong_architecture:     return QStringLiteral("The update is missing a required processor architecture.");
			case reason::changed_candidate:      return QStringLiteral("The staged update changed. Download it again.");
			case reason::size_limit:             return QStringLiteral("The update exceeds a download or staging limit.");
			default:                             return QStringLiteral("No verified update is available. Try again or use the release page.");
			}
		}
		auto status_text(const application::release_update_projection& aProjection) -> QString
		{
			switch (aProjection.Phase) {
			case core::release_update_phase::checking:    return QStringLiteral("Checking for updates…");
			case core::release_update_phase::downloading: return QStringLiteral("Downloading update…");
			case core::release_update_phase::verifying:   return QStringLiteral("Verifying archive and publisher…");
			case core::release_update_phase::ready:
				return std::ranges::any_of(aProjection.Commands, [](const auto& aCommand) { return aCommand.Command == command::install && aCommand.enabled(); })
					? QStringLiteral("The update is verified and ready to install.")
					: reason_text(reason::installer_unavailable);
			case core::release_update_phase::quiescing:  return QStringLiteral("Preparing to install and restart…");
			case core::release_update_phase::handed_off: return QStringLiteral("Restarting to finish the update…");
			case core::release_update_phase::recovered:  return QStringLiteral("The previous application was recovered.");
			case core::release_update_phase::available:
				if (aProjection.OfferVisible) {
					return QStringLiteral("An update is available.");
				}
				break;
			default: break;
			}
			return reason_text(aProjection.Reason);
		}
	}
	release_update_panel::release_update_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("releaseUpdatePanel"));
		setAccessibleName(QStringLiteral("Application updates"));
		auto* column = new QVBoxLayout{ this };
		Versions     = new QLabel{ this };
		Status       = new QLabel{ this };
		Status->setObjectName(QStringLiteral("releaseUpdateStatus"));
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		Checked   = new QLabel{ this };
		Automatic = new QCheckBox{ QStringLiteral("Automatically check for updates"), this };
		Automatic->setObjectName(QStringLiteral("releaseUpdateAutomatic"));
		Accounts = new QComboBox{ this };
		Accounts->setObjectName(QStringLiteral("releaseUpdateAccount"));
		Accounts->setAccessibleName(QStringLiteral("Release access account"));
		Accounts->addItem(QStringLiteral("No release access account selected"), QString{ });
		auto* accountHelp = new QLabel{ QStringLiteral("Choose release access separately from the repository you review. Sign in with native GitHub.com authentication in Accounts & hosts to add an account here."), this };
		accountHelp->setWordWrap(true);
		Notes = new QPlainTextEdit{ this };
		Notes->setObjectName(QStringLiteral("releaseUpdateNotes"));
		Notes->setAccessibleName(QStringLiteral("Release notes"));
		Notes->setReadOnly(true);
		Notes->setMaximumHeight(180);
		Progress = new QProgressBar{ this };
		Progress->setRange(0, 1000);
		Persistence = new QLabel{ this };
		Persistence->setWordWrap(true);
		for (auto* item : std::array<QWidget*, 8>{ Versions, Status, Checked, Automatic, Accounts, accountHelp, Notes, Progress }) {
			column->addWidget(item);
		}
		const std::array labels{
			std::pair{ command::check, "Check for Updates…" }, std::pair{ command::download, "Download Update" },
			std::pair{ command::later, "Later" }, std::pair{ command::skip, "Skip This Version" },
			std::pair{ command::cancel, "Cancel Download" }, std::pair{ command::install, "Install and Restart" }
		};
		for (const auto& [value, label] : labels) {
			auto* action = new QAction{ QString::fromUtf8(label), this };
			action->setEnabled(false);
			Actions.emplace(value, action);
			auto* button = new QPushButton{ action->text(), this };
			button->setObjectName(QStringLiteral("releaseUpdateCommand%1").arg(static_cast<int>(value)));
			button->setEnabled(false);
			connect(action, &QAction::changed, button, [action, button] { button->setEnabled(action->isEnabled()); button->setToolTip(action->toolTip()); });
			connect(button, &QPushButton::clicked, action, &QAction::trigger);
			connect(action, &QAction::triggered, this, [this, value] { if (Dispatch.Intent) { Dispatch.Intent({value, value == command::check ? std::nullopt : Projection.Candidate}); } });
			column->addWidget(button);
		}
		auto* view = new QPushButton{ QStringLiteral("View Release"), this };
		view->setObjectName(QStringLiteral("releaseUpdateViewRelease"));
		connect(view, &QPushButton::clicked, this, [this] { if (Dispatch.ViewRelease) { Dispatch.ViewRelease(); } });
		column->addWidget(view);
		column->addWidget(Persistence);
		Installation = new QLabel{ this };
		Installation->setObjectName(QStringLiteral("releaseUpdateInstallationResult"));
		Installation->setWordWrap(true);
		column->addWidget(Installation);
		connect(Automatic, &QCheckBox::toggled, this, [this](bool aEnabled) { if (Dispatch.Automatic) { Dispatch.Automatic(aEnabled); } });
		connect(Accounts, &QComboBox::activated, this, [this](int aIndex) { if (Dispatch.SelectAccount) { Dispatch.SelectAccount(Accounts->itemData(aIndex).toString().toStdString()); } });
	}
	void release_update_panel::set_dispatch(application::release_update_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	auto release_update_panel::action(command aCommand) const -> QAction* { return Actions.at(aCommand); }
	void release_update_panel::adopt(const application::release_update_projection& aProjection, bool aDurable)
	{
		Projection = aProjection;
		const QSignalBlocker blocker{ Automatic };
		Automatic->setChecked(aProjection.Preferences.AutomaticChecks);
		Automatic->setEnabled(aProjection.AutomaticAvailable);
		Accounts->setEnabled(aProjection.AutomaticAvailable);
		Versions->setText(QStringLiteral("Installed: %1   Available: %2").arg(version(aProjection.Installed), version(aProjection.Candidate ? std::optional{ aProjection.Candidate->Version } : std::nullopt)));
		const auto text = status_text(aProjection);
		if (Status->text() != text) {
			Status->setText(text);
			QAccessibleAnnouncementEvent event{ Status, text };
			QAccessible::updateAccessibility(&event);
		}
		Checked->setText(aProjection.Preferences.LastSuccess ? QStringLiteral("Last successful check: %1").arg(QDateTime::fromSecsSinceEpoch(*aProjection.Preferences.LastSuccess).toLocalTime().toString(Qt::ISODate)) : QStringLiteral("No successful check yet"));
		const auto notes = aProjection.OfferVisible ? QString::fromStdString(aProjection.Notes) : QString{ };
		if (Notes->toPlainText() != notes) {
			Notes->setPlainText(notes);
		}
		Notes->setVisible(!notes.isEmpty());
		Progress->setVisible(aProjection.Phase == core::release_update_phase::downloading);
		Progress->setValue(aProjection.Candidate && aProjection.Candidate->Size != 0U ? static_cast<int>(aProjection.Bytes * 1000U / aProjection.Candidate->Size) : 0);
		Persistence->setText(aDurable ? QStringLiteral("Update preferences are saved for this application.") : QStringLiteral("Update preferences are session-only because storage is unavailable."));
		Installation->setText(!aProjection.InstallationOutcome ? QString{ } : *aProjection.InstallationOutcome ? QStringLiteral("The last update completed with a matching healthy-start receipt.")
																											   : QStringLiteral("The last installation did not complete. The previous application was preserved or restored; no publication was retried."));
		for (auto& [key, action] : Actions) {
			static_cast<void>(key);
			action->setEnabled(false);
		}
		for (const auto& projected : aProjection.Commands) {
			auto* action = Actions.at(projected.Command);
			action->setEnabled(projected.enabled());
			action->setToolTip(projected.enabled() ? QString{ } : reason_text(projected.DisabledReason));
		}
	}
	void release_update_panel::set_accounts(const std::vector<std::pair<std::string, std::string>>& aAccounts, const std::string& aSelected)
	{
		const QSignalBlocker blocker{ Accounts };
		bool                 same = Accounts->count() == static_cast<int>(aAccounts.size() + 1U);
		for (std::size_t index = 0; same && index < aAccounts.size(); ++index) {
			same = Accounts->itemData(static_cast<int>(index + 1U)).toString().toStdString() == aAccounts[index].first
				&& Accounts->itemText(static_cast<int>(index + 1U)).toStdString() == aAccounts[index].second;
		}
		if (!same) {
			Accounts->clear();
			Accounts->addItem(QStringLiteral("No release access account selected"), QString{ });
			for (const auto& [key, label] : aAccounts) {
				Accounts->addItem(QString::fromStdString(label), QString::fromStdString(key));
			}
		}
		const auto index = Accounts->findData(QString::fromStdString(aSelected));
		Accounts->setCurrentIndex(index >= 0 ? index : 0);
	}
	auto add_release_update_action(QMenuBar& aMenu, std::function<void()> aCheck) -> QAction*
	{
		auto* menu = aMenu.findChild<QMenu*>(QStringLiteral("releaseUpdateMenu"));
		if (!menu) {
			menu = aMenu.addMenu(QStringLiteral("&Help"));
			menu->setObjectName(QStringLiteral("releaseUpdateMenu"));
		}
		auto* action = menu->addAction(QStringLiteral("Check for Updates…"));
		action->setObjectName(QStringLiteral("checkForUpdatesAction"));
		action->setMenuRole(QAction::ApplicationSpecificRole);
		QObject::connect(action, &QAction::triggered, &aMenu, std::move(aCheck));
		return action;
	}
}
