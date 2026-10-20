#include "bpr/qt/release_update_panel.hpp"

#include "bpr/qt/markdown_body_view.hpp"

#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#if defined(Q_OS_MACOS)
	#include "release_update_menu_mac.hpp"
#endif

namespace bpr::qt {
	namespace {
		using reason  = core::release_update_reason;
		using command = core::release_update_command;
		using phase   = core::release_update_phase;
		auto version(const core::release_update_version& aVersion) -> QString
		{
			return QStringLiteral("%1.%2.%3").arg(aVersion.Major).arg(aVersion.Minor).arg(aVersion.Patch);
		}
		auto offered(const application::release_update_projection& aProjection) -> QString
		{
			return aProjection.Candidate ? version(aProjection.Candidate->Version) : QString{ };
		}
		auto enabled(const application::release_update_projection& aProjection, command aCommand) -> bool
		{
			return std::ranges::any_of(aProjection.Commands, [aCommand](const auto& aProjected) { return aProjected.Command == aCommand && aProjected.enabled(); });
		}
		// Plain, short and actionable: releases are public, so nothing here
		// asks for an account or a sign-in ([[BPR-1669]], [[BPR-1671]]).
		auto reason_text(reason aReason) -> QString
		{
			switch (aReason) {
			case reason::none:                   return { };
			case reason::current:                return QStringLiteral("You’re up to date.");
			case reason::skipped:                return QStringLiteral("You skipped the latest version. Check now to see it again.");
			case reason::disabled:               return QStringLiteral("Automatic checks are off.");
			case reason::managed:                return QStringLiteral("Your organization manages updates for this app.");
			case reason::unsupported_platform:   return QStringLiteral("This platform can’t update in place. Open the release page to get the latest version.");
			case reason::untrusted_installation: return QStringLiteral("This copy can’t update itself. Open the release page to get the latest version.");
			case reason::read_only:              return QStringLiteral("This copy is read-only, so it can’t update itself. Open the release page to get the latest version.");
			case reason::translocated:           return QStringLiteral("Move BetterPullReview to your Applications folder and reopen it to update.");
			case reason::mounted_image:          return QStringLiteral("Copy BetterPullReview from the disk image to your Applications folder and reopen it to update.");
			case reason::invalid_version:        return QStringLiteral("This build has no release version, so it can’t update itself.");
			case reason::access_required:        return QStringLiteral("Update checks aren’t ready yet. Try again in a moment.");
			case reason::unauthenticated:        return QStringLiteral("GitHub didn’t accept the request. Try again later.");
			case reason::forbidden:              return QStringLiteral("GitHub refused the request. Try again later.");
			case reason::inaccessible:           return QStringLiteral("The release couldn’t be found on GitHub. Open the release page to update manually.");
			case reason::offline:                return QStringLiteral("GitHub couldn’t be reached. Check your connection and try again.");
			case reason::rate_limited:           return QStringLiteral("GitHub is limiting requests right now. Try again later.");
			case reason::timed_out:              return QStringLiteral("GitHub took too long to respond. Try again.");
			case reason::no_compatible_asset:    return QStringLiteral("The latest release has no download for this platform. Open the release page to update manually.");
			case reason::cancelled:              return QStringLiteral("Download cancelled. Nothing was changed.");
			case reason::installer_unavailable:  return QStringLiteral("This build can’t install updates. Open the release page to install it yourself.");
			case reason::flush_failed:           return QStringLiteral("Your work couldn’t be saved, so the update stopped. Nothing was changed.");
			case reason::quit_refused:           return QStringLiteral("BetterPullReview couldn’t quit safely, so the update stopped. Nothing was changed.");
			case reason::recovery_required:      return QStringLiteral("Your previous version was kept. Try updating again later.");
			case reason::shutdown:               return QStringLiteral("Updates are stopping.");
			case reason::busy:                   return QStringLiteral("An update is already in progress.");
			case reason::not_due:                return QStringLiteral("The next automatic check isn’t due yet.");
			case reason::certificate:            return QStringLiteral("GitHub’s secure connection couldn’t be verified. Check your network and try again.");
			case reason::redirect:               return QStringLiteral("The download came from an unexpected address, so it was stopped.");
			case reason::wrong_signer:           return QStringLiteral("The update’s signature couldn’t be verified, so it wasn’t installed.");
			case reason::digest_mismatch:        return QStringLiteral("The download was damaged. Try again.");
			case reason::unsafe_archive:         return QStringLiteral("The update file contains unsafe content, so it wasn’t installed.");
			case reason::invalid_bundle:         return QStringLiteral("The downloaded app isn’t valid, so it wasn’t installed.");
			case reason::wrong_architecture:     return QStringLiteral("The update doesn’t support this computer’s processor.");
			case reason::changed_candidate:      return QStringLiteral("The release changed during the update. Check again.");
			case reason::size_limit:             return QStringLiteral("The update is larger than allowed, so it was stopped.");
			default:                             return QStringLiteral("No update could be found. Try again, or open the release page.");
			}
		}
		/// Reasons the quiet line states instead of the version facts.
		auto explains(reason aReason) -> bool
		{
			return aReason != reason::none && aReason != reason::current && aReason != reason::not_due && aReason != reason::disabled && aReason != reason::busy;
		}
		auto failure_headline(reason aReason) -> QString
		{
			switch (aReason) {
			case reason::redirect:
			case reason::wrong_signer:
			case reason::digest_mismatch:
			case reason::unsafe_archive:
			case reason::invalid_bundle:
			case reason::wrong_architecture:
			case reason::changed_candidate:
			case reason::size_limit:            return QStringLiteral("Couldn’t download the update");
			case reason::installer_unavailable:
			case reason::flush_failed:
			case reason::quit_refused:
			case reason::recovery_required:     return QStringLiteral("Couldn’t install the update");
			default:                            return QStringLiteral("Couldn’t check for updates");
			}
		}
		auto headline(const application::release_update_projection& aProjection) -> QString
		{
			switch (aProjection.Phase) {
			case phase::checking:    return QStringLiteral("Checking for updates…");
			case phase::available:   return QStringLiteral("Version %1 is available").arg(offered(aProjection));
			case phase::downloading: return QStringLiteral("Downloading version %1…").arg(offered(aProjection));
			case phase::verifying:   return QStringLiteral("Verifying version %1…").arg(offered(aProjection));
			case phase::ready:
				return enabled(aProjection, command::install) ? QStringLiteral("Ready to install version %1").arg(offered(aProjection))
															  : QStringLiteral("Version %1 is downloaded").arg(offered(aProjection));
			case phase::quiescing:  return QStringLiteral("Getting ready to restart…");
			case phase::handed_off: return QStringLiteral("Restarting to finish the update…");
			case phase::recovered:  return QStringLiteral("The update didn’t finish");
			case phase::failed:     return failure_headline(aProjection.Reason);
			default:                break;
			}
			if (aProjection.Reason == reason::current) {
				return QStringLiteral("You’re up to date");
			}
			return aProjection.Installed ? QStringLiteral("BetterPullReview %1").arg(version(*aProjection.Installed)) : QStringLiteral("BetterPullReview");
		}
		auto last_checked(const application::release_update_projection& aProjection) -> QString
		{
			if (!aProjection.Preferences.LastSuccess) {
				return QStringLiteral("Not checked yet.");
			}
			const auto when   = QDateTime::fromSecsSinceEpoch(*aProjection.Preferences.LastSuccess).toLocalTime();
			const auto locale = QLocale{ };
			const auto time   = locale.toString(when.time(), QLocale::ShortFormat);
			const auto today  = QDate::currentDate();
			const auto date   = when.date();
			if (date == today) {
				return QStringLiteral("Last checked today at %1.").arg(time);
			}
			if (date == today.addDays(-1)) {
				return QStringLiteral("Last checked yesterday at %1.").arg(time);
			}
			const auto day = locale.toString(date, date.year() == today.year() ? QStringLiteral("MMMM d") : QStringLiteral("MMMM d, yyyy"));
			return QStringLiteral("Last checked %1 at %2.").arg(day, time);
		}
		auto installed(const application::release_update_projection& aProjection) -> QString
		{
			return aProjection.Installed ? QStringLiteral("You have %1.").arg(version(*aProjection.Installed)) : QString{ };
		}
		auto size(std::uint64_t aBytes) -> QString
		{
			return QLocale{ }.formattedDataSize(static_cast<qint64>(aBytes), 1, QLocale::DataSizeSIFormat);
		}
		auto joined(const QString& aFirst, const QString& aSecond) -> QString
		{
			return aFirst.isEmpty() ? aSecond : aSecond.isEmpty() ? aFirst
																  : aFirst + QLatin1Char(' ') + aSecond;
		}
		auto detail_line(const application::release_update_projection& aProjection) -> QString
		{
			switch (aProjection.Phase) {
			case phase::checking: return installed(aProjection);
			case phase::available:
				return aProjection.Candidate && aProjection.Candidate->Size != 0U
					? joined(installed(aProjection), QStringLiteral("The download is %1.").arg(size(aProjection.Candidate->Size)))
					: installed(aProjection);
			case phase::downloading:
				return aProjection.Candidate ? QStringLiteral("%1 of %2").arg(size(aProjection.Bytes), size(aProjection.Candidate->Size))
											 : QString{ };
			case phase::verifying: return QStringLiteral("Checking the publisher’s signature.");
			case phase::ready:
				return enabled(aProjection, command::install) ? QStringLiteral("BetterPullReview restarts to finish. Unsent review text is saved first.")
															  : reason_text(reason::installer_unavailable);
			case phase::quiescing:  return QStringLiteral("Saving your work before restarting.");
			case phase::handed_off: return { };
			case phase::recovered:
			case phase::failed:     return reason_text(aProjection.Reason);
			default:                break;
			}
			if (aProjection.Reason == reason::skipped && aProjection.Preferences.SkippedVersion) {
				return QStringLiteral("You skipped version %1. Check now to see it again.").arg(version(*aProjection.Preferences.SkippedVersion));
			}
			if (explains(aProjection.Reason)) {
				return reason_text(aProjection.Reason);
			}
			return aProjection.Reason == reason::current ? joined(installed(aProjection), last_checked(aProjection)) : last_checked(aProjection);
		}
		/// The one command the panel leads with, most consequential first.
		auto primary(const application::release_update_projection& aProjection) -> std::optional<command>
		{
			for (const auto candidate : { command::install, command::download, command::cancel, command::check }) {
				if (enabled(aProjection, candidate)) {
					return candidate;
				}
			}
			return std::nullopt;
		}
		/// Secondary gestures belong to the offer: Later until it is installed,
		/// Skip only before it is downloaded.
		auto secondary(const application::release_update_projection& aProjection, command aCommand) -> bool
		{
			return aCommand == command::later ? aProjection.Phase == phase::available || aProjection.Phase == phase::ready
											  : aCommand == command::skip && aProjection.Phase == phase::available;
		}
		/// Manual fallback: the release page is offered beside an offer and
		/// wherever this copy cannot update itself.
		auto offers_release_page(const application::release_update_projection& aProjection) -> bool
		{
			return (aProjection.OfferVisible && aProjection.Phase == phase::available) || !aProjection.AutomaticAvailable || aProjection.Reason == reason::installer_unavailable || aProjection.Reason == reason::inaccessible
				|| aProjection.Reason == reason::no_compatible_asset || (aProjection.Phase == phase::ready && !enabled(aProjection, command::install));
		}
	}
	release_update_panel::release_update_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("releaseUpdatePanel"));
		setAccessibleName(QStringLiteral("Application updates"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("releaseUpdateStatus"));
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		emphasize();
		Detail = new QLabel{ this };
		Detail->setObjectName(QStringLiteral("releaseUpdateDetail"));
		Detail->setWordWrap(true);
		Detail->setTextFormat(Qt::PlainText);
		Detail->setForegroundRole(QPalette::PlaceholderText);
		Progress = new QProgressBar{ this };
		Progress->setObjectName(QStringLiteral("releaseUpdateProgress"));
		Progress->setAccessibleName(QStringLiteral("Download progress"));
		Progress->setRange(0, 1000);
		Progress->setTextVisible(false);
		// The pull request overview's reader, as the overview composes it:
		// rendered Markdown with its source behind the context menu. No link
		// or image dispatch is bound, so a link opens nothing and an image
		// stays its accessible placeholder ([[BPR-1671]]).
		Notes = new markdown_body_view{ this };
		Notes->setObjectName(QStringLiteral("releaseUpdateNotes"));
		Notes->setAccessibleName(QStringLiteral("Release notes"));
		Notes->set_context_controls(true);
		Notes->setMinimumHeight(120);
		Notes->setMaximumHeight(180);
		Notice = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("releaseUpdateNotice"));
		Notice->setWordWrap(true);
		Notice->setTextFormat(Qt::PlainText);
		column->addWidget(Status);
		column->addWidget(Detail);
		column->addSpacing(4);
		column->addWidget(Progress);
		column->addWidget(Notes);
		column->addWidget(Notice);
		// Secondary gestures sit left of the default command, which closes
		// the row on the right.
		Row = new QHBoxLayout;
		Row->setSpacing(8);
		View = new QPushButton{ QStringLiteral("Open Release Page"), this };
		View->setObjectName(QStringLiteral("releaseUpdateViewRelease"));
		View->setAutoDefault(false);
		connect(View, &QPushButton::clicked, this, [this] { if (Dispatch.ViewRelease) { Dispatch.ViewRelease(); } });
		Row->addWidget(View);
		const std::array labels{
			std::pair{ command::skip, "Skip This Version" }, std::pair{ command::later, "Later" },
			std::pair{ command::check, "Check Now" }, std::pair{ command::download, "Download" },
			std::pair{ command::cancel, "Cancel" }, std::pair{ command::install, "Install and Restart" }
		};
		for (const auto& [value, label] : labels) {
			auto* action = new QAction{ QString::fromUtf8(label), this };
			action->setEnabled(false);
			Actions.emplace(value, action);
			auto* button = new QPushButton{ action->text(), this };
			button->setObjectName(QStringLiteral("releaseUpdateCommand%1").arg(static_cast<int>(value)));
			button->setAutoDefault(false);
			button->hide();
			Buttons.emplace(value, button);
			connect(button, &QPushButton::clicked, action, &QAction::trigger);
			connect(action, &QAction::triggered, this, [this, value] { if (Dispatch.Intent) { Dispatch.Intent({value, value == command::check ? std::nullopt : Projection.Candidate}); } });
			if (value == command::later) {
				Row->addStretch(1);
			}
			Row->addWidget(button);
		}
		column->addSpacing(6);
		column->addLayout(Row);
		Automatic = new QCheckBox{ QStringLiteral("Check for updates automatically"), this };
		Automatic->setObjectName(QStringLiteral("releaseUpdateAutomatic"));
		column->addSpacing(10);
		column->addWidget(Automatic);
		connect(Automatic, &QCheckBox::toggled, this, [this](bool aEnabled) { if (Dispatch.Automatic) { Dispatch.Automatic(aEnabled); } });
	}
	void release_update_panel::emphasize()
	{
		// Derived from the inherited font, so a theme or font change follows.
		auto heading = font();
		heading.setPointSizeF(heading.pointSizeF() * 1.3);
		heading.setWeight(QFont::DemiBold);
		Status->setFont(heading);
	}
	void release_update_panel::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			emphasize();
		}
	}
	void release_update_panel::set_dispatch(application::release_update_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	auto release_update_panel::action(command aCommand) const -> QAction* { return Actions.at(aCommand); }
	void release_update_panel::adopt(const application::release_update_projection& aProjection, bool aDurable)
	{
		Projection = aProjection;
		{
			const QSignalBlocker blocker{ Automatic };
			Automatic->setChecked(aProjection.Preferences.AutomaticChecks);
		}
		Automatic->setVisible(aProjection.AutomaticAvailable);
		const auto text = headline(aProjection);
		const auto line = detail_line(aProjection);
		Detail->setText(line);
		Detail->setVisible(!line.isEmpty());
		// One polite announcement per new headline, carrying its detail;
		// download progress alone does not speak.
		if (Status->text() != text) {
			Status->setText(text);
			QAccessibleAnnouncementEvent event{ Status, line.isEmpty() ? text : text + QStringLiteral(". ") + line };
			QAccessible::updateAccessibility(&event);
		}
		// An identical body under the same release is a no-op for the reader.
		const bool notes = aProjection.OfferVisible && aProjection.Candidate && !aProjection.Notes.empty();
		if (notes) {
			Notes->set_body(aProjection.Notes, "release-notes:" + aProjection.Candidate->Tag);
		} else if (!Notes->original_body().empty()) {
			Notes->clear();
		}
		Notes->setVisible(notes);
		Progress->setVisible(aProjection.Phase == phase::downloading);
		Progress->setValue(aProjection.Candidate && aProjection.Candidate->Size != 0U ? static_cast<int>(aProjection.Bytes * 1000U / aProjection.Candidate->Size) : 0);
		// Only what needs attention: storage that will not keep preferences,
		// and a previous installation that did not complete.
		QString notice;
		if (!aDurable) {
			notice = QStringLiteral("Update settings can’t be saved right now, so they reset when BetterPullReview quits.");
		}
		if (aProjection.InstallationOutcome.has_value() && !*aProjection.InstallationOutcome && aProjection.Phase != phase::recovered) {
			notice = joined(notice, QStringLiteral("The last update didn’t finish. Your previous version was kept."));
		}
		Notice->setText(notice);
		Notice->setVisible(!notice.isEmpty());
		const auto lead = primary(aProjection);
		for (const auto& [key, action] : Actions) {
			const auto& projected = aProjection.Commands[static_cast<std::size_t>(key)];
			action->setEnabled(projected.Command == key && projected.enabled());
			action->setToolTip(action->isEnabled() ? QString{ } : reason_text(projected.DisabledReason));
			auto* button = Buttons.at(key);
			// Check Now only leads; beside an offer it would be noise.
			const bool shown = action->isEnabled() && (key == lead || secondary(aProjection, key));
			button->setVisible(shown);
			button->setEnabled(shown);
			button->setDefault(shown && key == lead);
		}
		// With nothing else to do here, the release page is the lead.
		const bool fallback = !lead && offers_release_page(aProjection);
		if (Row->indexOf(View) != (fallback ? Row->count() - 1 : 0)) {
			Row->removeWidget(View);
			Row->insertWidget(fallback ? Row->count() : 0, View);
		}
		View->setDefault(fallback);
		View->setVisible(offers_release_page(aProjection));
	}
	release_update_menu::release_update_menu(std::function<void()> aCheck)
		: Check{ std::move(aCheck) }
	{
#if defined(Q_OS_MACOS)
		Native = make_release_update_native_menu(Check);
#endif
	}
	release_update_menu::~release_update_menu() = default;
	void release_update_menu::attach(QMenuBar& aMenu)
	{
		if (std::ranges::any_of(Menus, [&aMenu](const auto& aKnown) { return aKnown == &aMenu; })) {
			return;
		}
		Menus.emplace_back(&aMenu);
		connect(&aMenu, &QObject::destroyed, this, [this] {
			std::erase_if(Menus, [](const auto& aKnown) { return aKnown.isNull(); });
		});
		auto* menu = aMenu.findChild<QMenu*>(QStringLiteral("releaseUpdateMenu"), Qt::FindDirectChildrenOnly);
		if (!menu) {
			menu = aMenu.addMenu(QStringLiteral("&Help"));
		}
		menu->setObjectName(QStringLiteral("releaseUpdateMenu"));
		auto* action = new QAction{ QStringLiteral("Check for Updates…"), menu };
		action->setObjectName(QStringLiteral("checkForUpdatesAction"));
		action->setMenuRole(QAction::NoRole);
		action->setVisible(!Native);
		menu->addAction(action);
		connect(action, &QAction::triggered, this, Check);
	}
}
