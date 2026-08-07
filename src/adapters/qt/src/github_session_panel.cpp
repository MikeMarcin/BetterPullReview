#include "bpr/qt/github_session_panel.hpp"

#include "github_presentation_text.hpp"

#include <QAction>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <ranges>
#include <utility>

namespace bpr::qt {
	using namespace detail;

	github_session_panel::github_session_panel(
		std::vector<application::github_profile_option> aProfiles,
		application::github_intent_dispatch             aDispatch,
		QAction* const                                  aConnectAction,
		QAction* const                                  aOpenAction,
		QAction* const                                  aRefreshAction,
		github_session_environment                      aEnvironment,
		QWidget* const                                  aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, Profiles{ std::move(aProfiles) }
		, Environment{ std::move(aEnvironment) }
		, ConnectAction{ aConnectAction }
		, OpenAction{ aOpenAction }
		, RefreshAction{ aRefreshAction }
	{
		setObjectName(QStringLiteral("githubSessionPanel"));
		auto* const panelLayout = new QVBoxLayout{ this };
		panelLayout->setContentsMargins(0, 0, 0, 0);
		panelLayout->setSpacing(
			style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this));

		auto* const identityGroup = new QFrame{ this };
		identityGroup->setFrameShape(QFrame::StyledPanel);
		identityGroup->setObjectName(QStringLiteral("githubIdentityGroup"));
		identityGroup->setAccessibleName(QStringLiteral("GitHub connection"));
		auto* const identityLayout = new QGridLayout{ identityGroup };
		identityLayout->setColumnStretch(1, 1);

		ProfileSelector = new QComboBox{ identityGroup };
		ProfileSelector->setObjectName(QStringLiteral("githubProfileSelector"));
		ProfileSelector->setAccessibleName(QStringLiteral("GitHub host profile"));
		ProfileSelector->setAccessibleDescription(
			QStringLiteral("Select a non-secret GitHub host profile"));
		for (const auto& profile : Profiles) {
			ProfileSelector->addItem(
				text(profile.Label),
				QVariant::fromValue<qulonglong>(profile.Profile.value()));
		}
		auto* const profileLabel = new QLabel{ QStringLiteral("&Host profile:"), identityGroup };
		profileLabel->setObjectName(QStringLiteral("githubProfileLabel"));
		profileLabel->setBuddy(ProfileSelector);

		ConnectButton = new QToolButton{ identityGroup };
		ConnectButton->setDefaultAction(ConnectAction);
		ConnectButton->setObjectName(QStringLiteral("connectGithubButton"));
		ConnectButton->setAccessibleName(QStringLiteral("Connect GitHub host"));
		ConnectButton->setFocusPolicy(Qt::StrongFocus);
		ConnectButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

		ConnectionDetailsButton = new QToolButton{ identityGroup };
		ConnectionDetailsButton->setObjectName(
			QStringLiteral("githubConnectionDetailsButton"));
		ConnectionDetailsButton->setText(QStringLiteral("Details"));
		ConnectionDetailsButton->setAccessibleName(
			QStringLiteral("Show GitHub connection details"));
		ConnectionDetailsButton->setCheckable(true);
		ConnectionDetailsButton->setArrowType(Qt::RightArrow);
		ConnectionDetailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

		HostLabel = new QLabel{ identityGroup };
		HostLabel->setObjectName(QStringLiteral("activeGithubHost"));
		HostLabel->setAccessibleName(QStringLiteral("Active GitHub host"));
		set_plain_text_label(HostLabel);
		TransportLabel = new QLabel{ identityGroup };
		TransportLabel->setObjectName(QStringLiteral("activeGithubTransport"));
		TransportLabel->setAccessibleName(
			QStringLiteral("Active GitHub transport"));
		set_plain_text_label(TransportLabel);
		AccountLabel = new QLabel{ QStringLiteral("Not authenticated"), identityGroup };
		AccountLabel->setObjectName(QStringLiteral("activeGithubAccount"));
		AccountLabel->setAccessibleName(
			QStringLiteral("Authenticated GitHub account"));
		set_plain_text_label(AccountLabel);
		CapabilityLabel = new QLabel{ identityGroup };
		CapabilityLabel->setObjectName(QStringLiteral("githubCapabilities"));
		CapabilityLabel->setAccessibleName(
			QStringLiteral("GitHub host capabilities"));
		CapabilityLabel->setWordWrap(true);
		set_plain_text_label(CapabilityLabel);
		RateLimitLabel = new QLabel{ identityGroup };
		RateLimitLabel->setObjectName(QStringLiteral("githubRateLimits"));
		RateLimitLabel->setAccessibleName(QStringLiteral("GitHub rate limits"));
		RateLimitLabel->setWordWrap(true);
		set_plain_text_label(RateLimitLabel);

		ConnectionDetails = new QWidget{ identityGroup };
		ConnectionDetails->setObjectName(
			QStringLiteral("githubConnectionDetails"));
		auto* const detailsLayout = new QGridLayout{ ConnectionDetails };
		detailsLayout->setContentsMargins(0, 0, 0, 0);
		auto* const capabilitiesLabel = new QLabel{ QStringLiteral("Capabilities:"), ConnectionDetails };
		auto* const availabilityLabel = new QLabel{ QStringLiteral("Availability:"), ConnectionDetails };
		detailsLayout->addWidget(capabilitiesLabel, 0, 0);
		detailsLayout->addWidget(CapabilityLabel, 0, 1);
		detailsLayout->addWidget(availabilityLabel, 0, 2);
		detailsLayout->addWidget(RateLimitLabel, 0, 3);
		detailsLayout->setColumnStretch(1, 1);
		detailsLayout->setColumnStretch(3, 1);
		ConnectionDetails->hide();

		identityLayout->addWidget(profileLabel, 0, 0);
		identityLayout->addWidget(ProfileSelector, 0, 1);
		identityLayout->addWidget(ConnectButton, 0, 2);
		identityLayout->addWidget(ConnectionDetailsButton, 0, 3);
		identityLayout->addWidget(HostLabel, 1, 0, 1, 3);
		identityLayout->addWidget(TransportLabel, 1, 3);
		identityLayout->addWidget(AccountLabel, 2, 0, 1, 4);
		identityLayout->addWidget(ConnectionDetails, 3, 0, 1, 4);
		panelLayout->addWidget(identityGroup);

		auto* const locatorGroup = new QFrame{ this };
		locatorGroup->setFrameShape(QFrame::StyledPanel);
		locatorGroup->setObjectName(QStringLiteral("pullRequestLocatorGroup"));
		locatorGroup->setAccessibleName(QStringLiteral("Pull request locator"));
		LocatorLayout = new QGridLayout{ locatorGroup };
		LocatorLayout->setColumnStretch(1, 1);
		LocatorLayout->setColumnStretch(3, 1);

		OwnerEditor = new QLineEdit{ locatorGroup };
		OwnerEditor->setObjectName(QStringLiteral("repositoryOwnerEditor"));
		OwnerEditor->setAccessibleName(QStringLiteral("Repository owner"));
		OwnerEditor->setMaxLength(
			static_cast<int>(core::maximum_repository_owner_length));
		auto* const ownerLabel = new QLabel{ QStringLiteral("&Owner:"), locatorGroup };
		ownerLabel->setBuddy(OwnerEditor);

		RepositoryEditor = new QLineEdit{ locatorGroup };
		RepositoryEditor->setObjectName(QStringLiteral("repositoryNameEditor"));
		RepositoryEditor->setAccessibleName(QStringLiteral("Repository name"));
		RepositoryEditor->setMaxLength(
			static_cast<int>(core::maximum_repository_name_length));
		auto* const repositoryLabel = new QLabel{ QStringLiteral("&Repository:"), locatorGroup };
		repositoryLabel->setBuddy(RepositoryEditor);

		PullRequestEditor = new QSpinBox{ locatorGroup };
		PullRequestEditor->setObjectName(QStringLiteral("pullRequestNumberEditor"));
		PullRequestEditor->setAccessibleName(QStringLiteral("Pull request number"));
		PullRequestEditor->setRange(
			1, static_cast<int>(core::maximum_pull_request_number));
		auto* const numberLabel = new QLabel{ QStringLiteral("&PR:"), locatorGroup };
		numberLabel->setBuddy(PullRequestEditor);

		OpenButton = new QToolButton{ locatorGroup };
		OpenButton->setDefaultAction(OpenAction);
		OpenButton->setObjectName(QStringLiteral("openPullRequestButton"));
		OpenButton->setAccessibleName(QStringLiteral("Open pull request"));
		OpenButton->setFocusPolicy(Qt::StrongFocus);
		OpenButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

		RefreshButton = new QToolButton{ locatorGroup };
		RefreshButton->setDefaultAction(RefreshAction);
		RefreshButton->setObjectName(
			QStringLiteral("refreshPullRequestButton"));
		RefreshButton->setAccessibleName(QStringLiteral("Refresh pull request"));
		RefreshButton->setFocusPolicy(Qt::StrongFocus);
		RefreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

		LocatorLayout->addWidget(ownerLabel, 0, 0);
		LocatorLayout->addWidget(OwnerEditor, 0, 1);
		LocatorLayout->addWidget(repositoryLabel, 0, 2);
		LocatorLayout->addWidget(RepositoryEditor, 0, 3);
		LocatorLayout->addWidget(numberLabel, 0, 4);
		LocatorLayout->addWidget(PullRequestEditor, 0, 5);
		LocatorActions = new QWidget{ locatorGroup };
		LocatorActions->setObjectName(
			QStringLiteral("pullRequestLocatorActions"));
		auto* const locatorActionLayout = new QHBoxLayout{ LocatorActions };
		locatorActionLayout->setContentsMargins(0, 0, 0, 0);
		locatorActionLayout->addStretch(1);
		locatorActionLayout->addWidget(OpenButton);
		locatorActionLayout->addWidget(RefreshButton);
		LocatorLayout->addWidget(LocatorActions, 0, 6);
		panelLayout->addWidget(locatorGroup);

		StatusBanner = new QFrame{ this };
		StatusBanner->setFrameShape(QFrame::StyledPanel);
		StatusBanner->setObjectName(QStringLiteral("githubStatusBanner"));
		StatusBanner->setAccessibleName(QStringLiteral("GitHub activity"));
		auto* const statusLayout = new QVBoxLayout{ StatusBanner };
		StatusLabel              = new QLabel{
			QStringLiteral("No GitHub host is connected."),
			StatusBanner,
		};
		StatusLabel->setObjectName(QStringLiteral("githubStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("GitHub status"));
		StatusLabel->setWordWrap(true);
		set_plain_text_label(StatusLabel);
		statusLayout->addWidget(StatusLabel);
		DiagnosticLabel = new QLabel{ StatusBanner };
		DiagnosticLabel->setObjectName(QStringLiteral("githubDiagnostic"));
		DiagnosticLabel->setAccessibleName(QStringLiteral("GitHub diagnostic"));
		DiagnosticLabel->setWordWrap(true);
		set_plain_text_label(DiagnosticLabel);
		DiagnosticLabel->hide();
		statusLayout->addWidget(DiagnosticLabel);
		panelLayout->addWidget(StatusBanner);
		connect(ProfileSelector,
				&QComboBox::currentIndexChanged,
				this,
				[this] {
					update_selected_profile();
					update_connect_enabled();
				});
		connect(
			ConnectionDetailsButton,
			&QToolButton::toggled,
			this,
			[this](const bool aVisible) {
				ConnectionDetails->setVisible(aVisible);
				ConnectionDetailsButton->setArrowType(
					aVisible ? Qt::DownArrow : Qt::RightArrow);
				ConnectionDetailsButton->setAccessibleName(
					aVisible
						? QStringLiteral("Hide GitHub connection details")
						: QStringLiteral("Show GitHub connection details"));
			});
		connect(ConnectAction,
				&QAction::triggered,
				this,
				[this] {
					if (const auto profile = selected_profile();
						profile.has_value() && Dispatch) {
						Dispatch(application::github_intent{
							core::activate_host_profile_intent{
								.Profile = *profile,
							},
						});
					}
				});
		connect(OpenAction,
				&QAction::triggered,
				this,
				[this] {
					if (!Dispatch) {
						return;
					}
					Dispatch(application::github_intent{
						core::open_pull_request_intent{
							.Repository = {
								.Owner = OwnerEditor->text().toUtf8().toStdString(),
								.Name  = RepositoryEditor->text()
											 .toUtf8()
											 .toStdString(),
							},
							.Number = core::pull_request_number{ static_cast<std::uint32_t>(PullRequestEditor->value()) },
						},
					});
				});
		connect(RefreshAction,
				&QAction::triggered,
				this,
				[this] {
					if (Dispatch) {
						Dispatch(application::github_intent{
							core::refresh_pull_request_intent{ },
						});
					}
				});

		update_selected_profile();
		update_connect_enabled();
	}

	auto github_session_panel::apply_command(
		const core::github_command_projection& aCommand) -> bool
	{
		switch (aCommand.Id) {
		case core::github_command_id::activate_host_profile:
			ConnectCommandEnabled = aCommand.Enabled;
			ConnectDisabledReason = aCommand.DisabledReason;
			update_connect_enabled();
			break;
		case core::github_command_id::open_pull_request:
			apply_action_state(
				OpenAction,
				OpenButton,
				aCommand.Enabled,
				aCommand.DisabledReason,
				QStringLiteral("Open the specified pull request"));
			break;
		case core::github_command_id::refresh_pull_request:
			apply_action_state(
				RefreshAction,
				RefreshButton,
				aCommand.Enabled,
				aCommand.DisabledReason,
				QStringLiteral("Refresh the current pull request"));
			break;
		case core::github_command_id::show_review_threads:
			// The conversation view is the composing window's, so is its
			// command. Reported unhandled rather than ignored.
			return false;
		}
		++Statistics.CommandUpdates;
		return true;
	}

	void github_session_panel::apply_identity(
		const core::github_identity_delta& aIdentity)
	{
		ActiveProfile = aIdentity.Profile;
		if (aIdentity.Profile.has_value()) {
			const auto index = profile_index(*aIdentity.Profile);
			if (index >= 0) {
				const QSignalBlocker blocker{ ProfileSelector };
				ProfileSelector->setCurrentIndex(index);
			}
		}
		SessionAccount.reset();
		AccountLabel->setText(QStringLiteral("Not authenticated"));
		apply_rate_limits({ });
		update_selected_profile();
		update_connect_enabled();
		++Statistics.IdentityUpdates;
	}

	void github_session_panel::apply_capabilities(
		const core::github_capabilities& aCapabilities)
	{
		CapabilityLabel->setText(capabilities_text(aCapabilities));
		++Statistics.CapabilityUpdates;
	}

	void github_session_panel::apply_status(const core::github_status aStatus)
	{
		CurrentStatus = aStatus;
		const auto hasPullRequest
			= Environment.HasPullRequest && Environment.HasPullRequest();
		if (aStatus == core::github_status::loading_pull_request && hasPullRequest) {
			StatusLabel->setText(
				QStringLiteral("Refreshing the pull request."));
		} else if (
			aStatus == core::github_status::pull_request_failed
			&& hasPullRequest) {
			StatusLabel->setText(
				QStringLiteral(
					"Refresh failed. Showing the last accepted pull request."));
		} else if (
			aStatus == core::github_status::pull_request_cancelled
			&& hasPullRequest) {
			StatusLabel->setText(
				QStringLiteral(
					"Refresh canceled. Showing the last accepted pull request."));
		} else {
			StatusLabel->setText(github_status_text(aStatus));
		}
		switch (aStatus) {
		case core::github_status::no_profile:
			SessionAccount.reset();
			AccountLabel->setText(QStringLiteral("Not authenticated"));
			apply_rate_limits({ });
			[[fallthrough]];
		case core::github_status::session_ready:
		case core::github_status::loading_pull_request:
		case core::github_status::pull_request_ready:
			DiagnosticLabel->clear();
			DiagnosticLabel->hide();
			LastDiagnosticReason.reset();
			LastDiagnosticHasHostDetail = false;
			break;
		case core::github_status::establishing_session:
			SessionAccount.reset();
			AccountLabel->setText(QStringLiteral("Connecting…"));
			apply_rate_limits({ });
			DiagnosticLabel->clear();
			DiagnosticLabel->hide();
			LastDiagnosticReason.reset();
			LastDiagnosticHasHostDetail = false;
			break;
		case core::github_status::session_failed:
		case core::github_status::session_cancelled:
			SessionAccount.reset();
			AccountLabel->setText(QStringLiteral("Not authenticated"));
			apply_rate_limits({ });
			break;
		case core::github_status::pull_request_failed:
		case core::github_status::pull_request_cancelled:
			break;
		}
		if (Environment.PresentationChanged) {
			Environment.PresentationChanged();
		}
		++Statistics.StatusUpdates;
	}

	void github_session_panel::apply_diagnostic(
		const core::github_reason_code                                  aReason,
		const std::optional<std::uint32_t>                              aRetryAfter,
		const std::optional<application::github_rate_limit_projection>& aRateLimit,
		const bool                                                      aHostDetail)
	{
		if (
			!aHostDetail && LastDiagnosticHasHostDetail
			&& LastDiagnosticReason == aReason) {
			return;
		}
		const auto message = reason_text(aReason, aRetryAfter, aRateLimit);
		DiagnosticLabel->setText(message);
		DiagnosticLabel->setVisible(!message.isEmpty());
		LastDiagnosticReason        = message.isEmpty() ? std::nullopt : std::optional{ aReason };
		LastDiagnosticHasHostDetail = !message.isEmpty() && aHostDetail;
		if (Environment.PresentationChanged) {
			Environment.PresentationChanged();
		}
		++Statistics.DiagnosticUpdates;
	}

	void github_session_panel::apply_session(
		const application::github_session_projection& aSession)
	{
		SessionAccount = aSession.Account;
		if (aSession.DisplayName.empty()) {
			AccountLabel->setText(
				aSession.Login.empty()
					? QStringLiteral("Authenticated account")
					: QStringLiteral("@%1").arg(text(aSession.Login)));
		} else if (aSession.Login.empty()) {
			AccountLabel->setText(text(aSession.DisplayName));
		} else {
			AccountLabel->setText(
				QStringLiteral("%1 (@%2)")
					.arg(text(aSession.DisplayName), text(aSession.Login)));
		}
		apply_capabilities(aSession.Capabilities);
		apply_rate_limits(aSession.RateLimits);
		++Statistics.SessionUpdates;
	}

	void github_session_panel::apply_rate_limits(
		const application::github_rate_limits_projection& aLimits)
	{
		RateLimitLabel->setText(rate_limits_text(aLimits));
	}

	void github_session_panel::update_selected_profile()
	{
		const application::github_profile_option* selected = nullptr;
		QString                                   prefix;
		if (ActiveProfile.has_value()) {
			selected = profile_option(*ActiveProfile);
			prefix   = QStringLiteral("Active host");
		} else if (const auto profile = selected_profile(); profile.has_value()) {
			selected = profile_option(*profile);
			prefix   = QStringLiteral("Selected host");
		}

		if (selected == nullptr) {
			HostLabel->setText(QStringLiteral("No host profile selected"));
			TransportLabel->setText(QStringLiteral("No transport selected"));
			return;
		}

		HostLabel->setText(
			QStringLiteral("%1: %2 (%3)")
				.arg(
					prefix,
					text(selected->Hostname),
					product_text(selected->Product)));
		TransportLabel->setText(transport_text(selected->Transport));
	}

	void github_session_panel::update_connect_enabled()
	{
		const auto hasProfile = selected_profile().has_value();
		const auto enabled    = ConnectCommandEnabled && hasProfile;
		if (!hasProfile) {
			ConnectAction->setEnabled(false);
			ConnectAction->setProperty(
				"githubDisabledReason",
				static_cast<int>(core::github_reason_code::invalid_profile));
			const auto description = QStringLiteral("No GitHub host profiles are configured.");
			ConnectAction->setToolTip(description);
			ConnectAction->setStatusTip(description);
			if (ConnectButton != nullptr) {
				ConnectButton->setAccessibleDescription(description);
			}
			return;
		}
		apply_action_state(
			ConnectAction,
			ConnectButton,
			enabled,
			ConnectDisabledReason,
			QStringLiteral("Connect the selected GitHub host profile"));
	}

	void github_session_panel::refresh_native_appearance()
	{
		auto emphasisFont = font();
		emphasisFont.setWeight(QFont::DemiBold);
		StatusLabel->setFont(emphasisFont);

		ConnectAction->setIcon(
			style()->standardIcon(QStyle::SP_DialogApplyButton));
		OpenAction->setIcon(
			style()->standardIcon(QStyle::SP_DialogOpenButton));
		RefreshAction->setIcon(
			style()->standardIcon(QStyle::SP_BrowserReload));
	}

	void github_session_panel::apply_compact_layout(const bool aCompact)
	{
		if (
			LocatorLayout == nullptr || LocatorActions == nullptr
			|| LocatorActions->property("githubNarrowLayout").toBool() == aCompact) {
			return;
		}
		LocatorLayout->removeWidget(LocatorActions);
		if (aCompact) {
			LocatorLayout->addWidget(LocatorActions, 1, 0, 1, 6);
		} else {
			LocatorLayout->addWidget(LocatorActions, 0, 6);
		}
		LocatorActions->setProperty("githubNarrowLayout", aCompact);
	}

	auto github_session_panel::profile_selector() const noexcept -> QComboBox*
	{
		return ProfileSelector;
	}

	auto github_session_panel::owner_editor() const noexcept -> QLineEdit*
	{
		return OwnerEditor;
	}

	auto github_session_panel::repository_editor() const noexcept -> QLineEdit*
	{
		return RepositoryEditor;
	}

	auto github_session_panel::pull_request_editor() const noexcept -> QSpinBox*
	{
		return PullRequestEditor;
	}

	auto github_session_panel::connect_button() const noexcept -> QToolButton*
	{
		return ConnectButton;
	}

	auto github_session_panel::open_button() const noexcept -> QToolButton*
	{
		return OpenButton;
	}

	auto github_session_panel::refresh_button() const noexcept -> QToolButton*
	{
		return RefreshButton;
	}

	auto github_session_panel::connection_details_button() const noexcept
		-> QToolButton*
	{
		return ConnectionDetailsButton;
	}

	auto github_session_panel::showing_diagnostic() const -> bool
	{
		return DiagnosticLabel->isVisible() && !DiagnosticLabel->text().isEmpty();
	}

	auto github_session_panel::status_banner() const noexcept -> QFrame*
	{
		return StatusBanner;
	}

	auto github_session_panel::summary_text() const -> QString
	{
		auto summary = StatusLabel->text();
		if (DiagnosticLabel->isVisible() && !DiagnosticLabel->text().isEmpty()) {
			summary.append(QStringLiteral(" %1").arg(DiagnosticLabel->text()));
		}
		return summary;
	}

	auto github_session_panel::status_text() const -> std::string
	{
		return StatusLabel->text().toUtf8().toStdString();
	}

	auto github_session_panel::diagnostic_text() const -> std::string
	{
		// The text, whether or not the banner is currently showing it. This is
		// the window accessor's own contract, kept exactly: an earlier draft of
		// this panel added an `isVisible()` guard, which reads as more correct
		// and is not what the window did. Every assertion about diagnostic
		// wording runs against a window that was never shown, so the guard
		// silently returned nothing and thirteen reason codes appeared to have
		// lost their presentation text.
		return DiagnosticLabel->text().toUtf8().toStdString();
	}

	auto github_session_panel::active_host_text() const -> std::string
	{
		return HostLabel->text().toUtf8().toStdString();
	}

	auto github_session_panel::active_account_text() const -> std::string
	{
		return AccountLabel->text().toUtf8().toStdString();
	}

	auto github_session_panel::status() const noexcept -> core::github_status
	{
		return CurrentStatus;
	}

	auto github_session_panel::selected_profile() const
		-> std::optional<core::host_profile_id>
	{
		const auto index = ProfileSelector->currentIndex();
		if (index < 0) {
			return std::nullopt;
		}
		const auto value = ProfileSelector->itemData(index);
		if (!value.isValid()) {
			return std::nullopt;
		}
		return core::host_profile_id{ value.toULongLong() };
	}

	auto github_session_panel::statistics() const noexcept
		-> github_session_panel_statistics
	{
		return Statistics;
	}

	auto github_session_panel::profile_option(
		const core::host_profile_id aProfile) const
		-> const application::github_profile_option*
	{
		const auto found = std::ranges::find_if(
			Profiles,
			[aProfile](const application::github_profile_option& aOption) {
				return aOption.Profile == aProfile;
			});
		return found == Profiles.end() ? nullptr : &*found;
	}

	auto github_session_panel::profile_index(
		const core::host_profile_id aProfile) const -> int
	{
		for (auto index = 0; index < ProfileSelector->count(); ++index) {
			const auto value = ProfileSelector->itemData(index);
			if (
				value.isValid()
				&& core::host_profile_id{ value.toULongLong() } == aProfile) {
				return index;
			}
		}
		return -1;
	}
} // namespace bpr::qt
