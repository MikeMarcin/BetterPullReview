#include "bpr/qt/github_window.hpp"
#include "bpr/qt/github_conversation_delegate.hpp"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(
				aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto valid(
			const application::github_profile_product aProduct) noexcept -> bool
		{
			switch (aProduct) {
			case application::github_profile_product::github_com:
			case application::github_profile_product::github_enterprise_server:
				return true;
			}
			return false;
		}

		[[nodiscard]] auto valid(
			const application::github_profile_transport aTransport) noexcept -> bool
		{
			switch (aTransport) {
			case application::github_profile_transport::github_cli:
			case application::github_profile_transport::native_https:
				return true;
			}
			return false;
		}

		[[nodiscard]] auto valid(
			const application::github_profile_option& aOption) noexcept -> bool
		{
			return aOption.Profile.value() != 0U && !aOption.Label.empty()
				&& !aOption.Hostname.empty() && valid(aOption.Product)
				&& valid(aOption.Transport);
		}

		[[nodiscard]] auto product_text(
			const application::github_profile_product aProduct) -> QString
		{
			switch (aProduct) {
			case application::github_profile_product::github_com:
				return QStringLiteral("GitHub.com");
			case application::github_profile_product::github_enterprise_server:
				return QStringLiteral("GitHub Enterprise Server");
			}
			return QStringLiteral("Unknown GitHub product");
		}

		[[nodiscard]] auto transport_text(
			const application::github_profile_transport aTransport) -> QString
		{
			switch (aTransport) {
			case application::github_profile_transport::github_cli:
				return QStringLiteral("GitHub CLI");
			case application::github_profile_transport::native_https:
				return QStringLiteral("Native HTTPS");
			}
			return QStringLiteral("Unknown transport");
		}

		[[nodiscard]] auto github_status_text(
			const core::github_status aStatus) -> QString
		{
			switch (aStatus) {
			case core::github_status::no_profile:
				return QStringLiteral("No GitHub host is connected.");
			case core::github_status::establishing_session:
				return QStringLiteral("Connecting to the selected GitHub host.");
			case core::github_status::session_ready:
				return QStringLiteral("GitHub account connected.");
			case core::github_status::session_failed:
				return QStringLiteral("The GitHub connection failed.");
			case core::github_status::session_cancelled:
				return QStringLiteral("The GitHub connection was canceled.");
			case core::github_status::loading_pull_request:
				return QStringLiteral("Loading the pull request.");
			case core::github_status::pull_request_ready:
				return QStringLiteral("Pull request ready.");
			case core::github_status::pull_request_failed:
				return QStringLiteral("The pull request could not be loaded.");
			case core::github_status::pull_request_cancelled:
				return QStringLiteral("Pull request loading was canceled.");
			}
			return QStringLiteral("GitHub status is unavailable.");
		}

		[[nodiscard]] auto resource_text(
			const application::github_rate_limit_resource_projection aResource)
			-> QString
		{
			switch (aResource) {
			case application::github_rate_limit_resource_projection::core:
				return QStringLiteral("GitHub API");
			case application::github_rate_limit_resource_projection::search:
				return QStringLiteral("GitHub search");
			case application::github_rate_limit_resource_projection::graphql:
				return QStringLiteral("GitHub GraphQL API");
			}
			return QStringLiteral("GitHub API");
		}

		[[nodiscard]] auto reason_text(
			const core::github_reason_code     aReason,
			const std::optional<std::uint32_t> aRetryAfter,
			const std::optional<application::github_rate_limit_projection>&
				aRateLimit) -> QString
		{
			QString result;
			switch (aReason) {
			case core::github_reason_code::none:
				return { };
			case core::github_reason_code::invalid_profile:
				result = QStringLiteral("The selected GitHub host profile is invalid.");
				break;
			case core::github_reason_code::session_in_progress:
				result = QStringLiteral(
					"Wait for the GitHub connection attempt to finish.");
				break;
			case core::github_reason_code::session_required:
				result = QStringLiteral(
					"Connect a GitHub host profile before opening a pull request.");
				break;
			case core::github_reason_code::invalid_repository:
				result = QStringLiteral(
					"Enter a valid repository owner and repository name.");
				break;
			case core::github_reason_code::invalid_pull_request_number:
				result = QStringLiteral("Enter a valid pull request number.");
				break;
			case core::github_reason_code::pull_request_load_in_progress:
				result = QStringLiteral(
					"Wait for the current pull request load to finish.");
				break;
			case core::github_reason_code::no_active_pull_request:
				result = QStringLiteral("Open a pull request before refreshing it.");
				break;
			case core::github_reason_code::capability_unknown:
				result = QStringLiteral(
					"GitHub review-thread support has not been confirmed.");
				break;
			case core::github_reason_code::capability_unsupported:
				result = QStringLiteral(
					"The selected GitHub host does not support review threads.");
				break;
			case core::github_reason_code::stale_completion:
				result = QStringLiteral("An outdated GitHub result was safely ignored.");
				break;
			case core::github_reason_code::completion_identity_mismatch:
				result = QStringLiteral(
					"GitHub returned a result for a different identity.");
				break;
			case core::github_reason_code::authentication_failed:
				result = QStringLiteral(
					"Authentication failed for the selected GitHub host profile.");
				break;
			case core::github_reason_code::permission_denied:
				result = QStringLiteral(
					"The authenticated account does not have permission to read this "
					"pull request.");
				break;
			case core::github_reason_code::network_unavailable:
				result = QStringLiteral(
					"The GitHub host could not be reached. Check the network "
					"connection and try again.");
				break;
			case core::github_reason_code::api_version_unsupported:
				result = QStringLiteral(
					"The selected host does not support the required GitHub API "
					"version.");
				break;
			case core::github_reason_code::capability_unavailable:
				result = QStringLiteral(
					"Required GitHub capability information is unavailable.");
				break;
			case core::github_reason_code::rate_limited:
				result = aRateLimit.has_value()
					? QStringLiteral("%1 rate limit reached.")
						  .arg(resource_text(aRateLimit->Resource))
					: QStringLiteral("GitHub rate limit reached.");
				if (aRateLimit.has_value() && aRateLimit->SecondaryLimited) {
					result = QStringLiteral(
								 "%1 temporarily limited this request.")
								 .arg(resource_text(aRateLimit->Resource));
				}
				break;
			case core::github_reason_code::malformed_response:
				result = QStringLiteral(
					"GitHub returned data that BetterPullReview could not safely use.");
				break;
			case core::github_reason_code::response_too_large:
				result = QStringLiteral(
					"The GitHub response exceeded BetterPullReview's safe limits.");
				break;
			case core::github_reason_code::vault_unavailable:
				result = QStringLiteral(
					"The operating system credential store is unavailable.");
				break;
			case core::github_reason_code::certificate_rejected:
				result = QStringLiteral(
					"The GitHub host certificate was rejected.");
				break;
			case core::github_reason_code::redirect_rejected:
				result = QStringLiteral(
					"A redirect to an untrusted destination was blocked.");
				break;
			case core::github_reason_code::timed_out:
				result = QStringLiteral("The GitHub request timed out.");
				break;
			case core::github_reason_code::cli_unavailable:
				result = QStringLiteral("The selected GitHub CLI connection is unavailable.");
				break;
			case core::github_reason_code::cancelled:
				result = QStringLiteral("The GitHub request was canceled.");
				break;
			}

			if (aRetryAfter.has_value()) {
				result.append(
					QStringLiteral(" Try again in %1 seconds.")
						.arg(QString::number(*aRetryAfter)));
			}
			return result;
		}

		[[nodiscard]] auto capability_text(
			const core::github_capability_state aState) -> QString
		{
			switch (aState) {
			case core::github_capability_state::supported:
				return QStringLiteral("Supported");
			case core::github_capability_state::unsupported:
				return QStringLiteral("Unsupported");
			case core::github_capability_state::unknown:
				return QStringLiteral("Not yet known");
			}
			return QStringLiteral("Not yet known");
		}

		[[nodiscard]] auto capabilities_text(
			const core::github_capabilities& aCapabilities) -> QString
		{
			return QStringLiteral(
					   "Review threads: %1\n"
					   "Viewed-file changes: %2\n"
					   "Resolve review threads: %3\n"
					   "Reopen review threads: %4")
				.arg(
					capability_text(aCapabilities.ReviewThreads),
					capability_text(aCapabilities.ViewedFileMutations),
					capability_text(aCapabilities.ResolveThread),
					capability_text(aCapabilities.UnresolveThread));
		}

		[[nodiscard]] auto rate_limit_line(
			const QString&                                   aLabel,
			const application::github_rate_limit_projection& aLimit) -> QString
		{
			if (aLimit.SecondaryLimited) {
				return QStringLiteral("%1: Temporarily limited").arg(aLabel);
			}
			return QStringLiteral("%1: %2 of %3 requests remaining")
				.arg(
					aLabel,
					QString::number(aLimit.Remaining),
					QString::number(aLimit.Limit));
		}

		[[nodiscard]] auto rate_limits_text(
			const application::github_rate_limits_projection& aLimits) -> QString
		{
			QStringList lines;
			if (aLimits.Core.has_value()) {
				lines.push_back(rate_limit_line(QStringLiteral("API"), *aLimits.Core));
			}
			if (aLimits.Search.has_value()) {
				lines.push_back(
					rate_limit_line(QStringLiteral("Search"), *aLimits.Search));
			}
			if (aLimits.Graphql.has_value()) {
				lines.push_back(
					rate_limit_line(QStringLiteral("GraphQL"), *aLimits.Graphql));
			}
			return lines.isEmpty()
				? QStringLiteral("Rate limits: Not reported")
				: lines.join(QLatin1Char('\n'));
		}

		[[nodiscard]] auto pull_request_state_text(
			const application::github_pull_request_state_projection aState,
			const bool                                              aDraft) -> QString
		{
			QString result;
			switch (aState) {
			case application::github_pull_request_state_projection::open:
				result = QStringLiteral("Open");
				break;
			case application::github_pull_request_state_projection::closed:
				result = QStringLiteral("Closed");
				break;
			case application::github_pull_request_state_projection::merged:
				result = QStringLiteral("Merged");
				break;
			}
			if (aDraft) {
				result.append(QStringLiteral(" draft"));
			}
			return result;
		}

		[[nodiscard]] auto review_thread_availability_text(
			const bool                          aAvailable,
			const std::size_t                   aCount,
			const core::github_capability_state aCapability) -> QString
		{
			if (aAvailable) {
				if (aCount == 0U) {
					return QStringLiteral(
						"Review threads are available; this pull request has none.");
				}
				if (aCount == 1U) {
					return QStringLiteral("1 review thread available.");
				}
				return QStringLiteral("%1 review threads available.")
					.arg(QString::number(aCount));
			}

			switch (aCapability) {
			case core::github_capability_state::unsupported:
				return QStringLiteral(
					"Review threads are unavailable because this host does not "
					"support them.");
			case core::github_capability_state::unknown:
				return QStringLiteral(
					"Review threads are unavailable because support has not been "
					"confirmed.");
			case core::github_capability_state::supported:
				return QStringLiteral(
					"Review threads are unavailable for this pull request.");
			}
			return QStringLiteral(
				"Review threads are unavailable for this pull request.");
		}

		void set_plain_text_label(QLabel* const aLabel)
		{
			aLabel->setTextFormat(Qt::PlainText);
			aLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		}

		void apply_action_state(
			QAction* const                 aAction,
			QToolButton* const             aButton,
			const bool                     aEnabled,
			const core::github_reason_code aReason,
			const QString&                 aEnabledDescription)
		{
			aAction->setEnabled(aEnabled);
			const auto description = aEnabled
				? aEnabledDescription
				: reason_text(aReason, std::nullopt, std::nullopt);
			aAction->setToolTip(description);
			aAction->setStatusTip(description);
			aAction->setProperty(
				"githubDisabledReason", static_cast<int>(aEnabled ? core::github_reason_code::none : aReason));
			if (aButton != nullptr) {
				aButton->setAccessibleDescription(description);
			}
		}
	}

	github_window::github_window(
		github_view_config                  aConfig,
		application::github_intent_dispatch aDispatch,
		QWidget* const                      aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		std::unordered_set<std::uint64_t> profileIds;
		profileIds.reserve(aConfig.Profiles.size());
		for (auto& profile : aConfig.Profiles) {
			if (
				valid(profile)
				&& profileIds.insert(profile.Profile.value()).second) {
				Profiles.push_back(std::move(profile));
			}
		}

		setObjectName(QStringLiteral("githubWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview — GitHub"));
		setMinimumSize(640, 420);
		resize(1100, 720);

		ConnectAction = new QAction{ QStringLiteral("&Connect"), this };
		ConnectAction->setObjectName(QStringLiteral("connectGithubAction"));
		ConnectAction->setShortcut(
			QKeySequence{ QStringLiteral("Ctrl+Shift+C") });
		ConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(ConnectAction);
		OpenAction = new QAction{ QStringLiteral("&Open pull request"), this };
		OpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		OpenAction->setShortcut(QKeySequence::Open);
		OpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(OpenAction);
		RefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		RefreshAction->setObjectName(QStringLiteral("refreshPullRequestAction"));
		RefreshAction->setShortcut(QKeySequence::Refresh);
		RefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(RefreshAction);
		ShowReviewThreadsAction = new QAction{
			QStringLiteral("Show review &threads"),
			this,
		};
		ShowReviewThreadsAction->setObjectName(
			QStringLiteral("showReviewThreadsAction"));
		addAction(ShowReviewThreadsAction);

		auto* const presentationScroll = new QScrollArea{ this };
		presentationScroll->setObjectName(
			QStringLiteral("githubPresentationScroll"));
		presentationScroll->setAccessibleName(
			QStringLiteral("GitHub pull request presentation"));
		presentationScroll->setWidgetResizable(true);
		presentationScroll->setHorizontalScrollBarPolicy(
			Qt::ScrollBarAlwaysOff);
		presentationScroll->setFrameShape(QFrame::NoFrame);

		auto* const central = new QWidget{ presentationScroll };
		central->setObjectName(QStringLiteral("githubPresentation"));
		central->setMinimumHeight(500);
		auto* const centralLayout    = new QVBoxLayout{ central };
		const auto  horizontalMargin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, central);
		const auto  verticalMargin   = style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, central);
		centralLayout->setContentsMargins(
			horizontalMargin,
			verticalMargin,
			horizontalMargin,
			verticalMargin);
		centralLayout->setSpacing(
			style()->pixelMetric(
				QStyle::PM_LayoutVerticalSpacing,
				nullptr,
				central));

		auto* const identityGroup = new QFrame{ central };
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
		centralLayout->addWidget(identityGroup);

		auto* const locatorGroup = new QFrame{ central };
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
		centralLayout->addWidget(locatorGroup);

		StatusBanner = new QFrame{ central };
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
		centralLayout->addWidget(StatusBanner);

		PullRequestStack = new QStackedWidget{ central };
		PullRequestStack->setObjectName(QStringLiteral("githubViewState"));
		PullRequestStack->setAccessibleName(
			QStringLiteral("GitHub pull request state"));

		auto* const emptyState = new QWidget{ PullRequestStack };
		emptyState->setObjectName(QStringLiteral("githubEmptyState"));
		auto* const emptyLayout = new QVBoxLayout{ emptyState };
		emptyLayout->addStretch(1);
		EmptyStateTitleLabel = new QLabel{ emptyState };
		EmptyStateTitleLabel->setObjectName(
			QStringLiteral("githubEmptyStateTitle"));
		EmptyStateTitleLabel->setAccessibleName(
			QStringLiteral("GitHub view state"));
		EmptyStateTitleLabel->setAlignment(Qt::AlignCenter);
		EmptyStateTitleLabel->setWordWrap(true);
		set_plain_text_label(EmptyStateTitleLabel);
		emptyLayout->addWidget(EmptyStateTitleLabel);
		EmptyStateBodyLabel = new QLabel{ emptyState };
		EmptyStateBodyLabel->setObjectName(
			QStringLiteral("githubEmptyStateBody"));
		EmptyStateBodyLabel->setAccessibleName(
			QStringLiteral("GitHub view guidance"));
		EmptyStateBodyLabel->setAlignment(Qt::AlignCenter);
		EmptyStateBodyLabel->setWordWrap(true);
		set_plain_text_label(EmptyStateBodyLabel);
		emptyLayout->addWidget(EmptyStateBodyLabel);
		emptyLayout->addStretch(2);
		PullRequestStack->addWidget(emptyState);

		auto* const pullRequestContent = new QWidget{ PullRequestStack };
		pullRequestContent->setObjectName(
			QStringLiteral("githubPullRequestContent"));
		auto* const pullRequestLayout = new QVBoxLayout{ pullRequestContent };
		pullRequestLayout->setContentsMargins(0, 0, 0, 0);

		auto* const metadataGroup = new QFrame{ pullRequestContent };
		metadataGroup->setFrameShape(QFrame::NoFrame);
		metadataGroup->setObjectName(QStringLiteral("pullRequestMetadataGroup"));
		metadataGroup->setAccessibleName(QStringLiteral("Pull request metadata"));
		auto* const metadataLayout = new QGridLayout{ metadataGroup };
		metadataLayout->setContentsMargins(0, 0, 0, 0);
		metadataLayout->setColumnStretch(0, 1);

		PullRequestLabel = new QLabel{ metadataGroup };
		PullRequestLabel->setObjectName(QStringLiteral("pullRequestIdentity"));
		PullRequestLabel->setAccessibleName(
			QStringLiteral("Pull request identity"));
		set_plain_text_label(PullRequestLabel);
		TitleLabel = new QLabel{ metadataGroup };
		TitleLabel->setObjectName(QStringLiteral("pullRequestTitle"));
		TitleLabel->setAccessibleName(QStringLiteral("Pull request title"));
		TitleLabel->setWordWrap(true);
		set_plain_text_label(TitleLabel);
		StateLabel = new QLabel{ metadataGroup };
		StateLabel->setObjectName(QStringLiteral("pullRequestState"));
		StateLabel->setAccessibleName(QStringLiteral("Pull request state"));
		set_plain_text_label(StateLabel);
		AuthorLabel = new QLabel{ metadataGroup };
		AuthorLabel->setObjectName(QStringLiteral("pullRequestAuthor"));
		AuthorLabel->setAccessibleName(QStringLiteral("Pull request author"));
		set_plain_text_label(AuthorLabel);
		BranchLabel = new QLabel{ metadataGroup };
		BranchLabel->setObjectName(QStringLiteral("pullRequestBranches"));
		BranchLabel->setAccessibleName(QStringLiteral("Pull request branches"));
		BranchLabel->setWordWrap(true);
		set_plain_text_label(BranchLabel);
		OidLabel = new QLabel{ metadataGroup };
		OidLabel->setObjectName(QStringLiteral("pullRequestCommitOids"));
		OidLabel->setAccessibleName(
			QStringLiteral("Pull request commit identities"));
		OidLabel->setWordWrap(true);
		set_plain_text_label(OidLabel);
		CountLabel = new QLabel{ metadataGroup };
		CountLabel->setObjectName(QStringLiteral("pullRequestCounts"));
		CountLabel->setAccessibleName(QStringLiteral("Pull request record counts"));
		CountLabel->setWordWrap(true);
		set_plain_text_label(CountLabel);
		BodyEditor = new QPlainTextEdit{ metadataGroup };
		BodyEditor->setObjectName(QStringLiteral("pullRequestBody"));
		BodyEditor->setAccessibleName(QStringLiteral("Pull request description"));
		BodyEditor->setReadOnly(true);
		BodyEditor->setTabChangesFocus(true);
		BodyEditor->setMinimumHeight(120);

		TechnicalDetailsButton = new QToolButton{ metadataGroup };
		TechnicalDetailsButton->setObjectName(
			QStringLiteral("githubTechnicalDetailsButton"));
		TechnicalDetailsButton->setText(QStringLiteral("Technical details"));
		TechnicalDetailsButton->setAccessibleName(
			QStringLiteral("Show pull request technical details"));
		TechnicalDetailsButton->setCheckable(true);
		TechnicalDetailsButton->setArrowType(Qt::RightArrow);
		TechnicalDetailsButton->setToolButtonStyle(
			Qt::ToolButtonTextBesideIcon);
		TechnicalDetails = new QWidget{ metadataGroup };
		TechnicalDetails->setObjectName(
			QStringLiteral("githubTechnicalDetails"));
		auto* const technicalLayout = new QHBoxLayout{ TechnicalDetails };
		technicalLayout->setContentsMargins(0, 0, 0, 0);
		auto* const commitsLabel = new QLabel{ QStringLiteral("Commit identities:"), TechnicalDetails };
		technicalLayout->addWidget(commitsLabel);
		technicalLayout->addWidget(OidLabel, 1);
		TechnicalDetails->hide();

		metadataLayout->addWidget(PullRequestLabel, 0, 0);
		metadataLayout->addWidget(
			StateLabel,
			0,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TitleLabel, 1, 0, 1, 2);
		metadataLayout->addWidget(AuthorLabel, 2, 0);
		metadataLayout->addWidget(BranchLabel, 3, 0, 1, 2);
		metadataLayout->addWidget(CountLabel, 4, 0);
		metadataLayout->addWidget(
			TechnicalDetailsButton,
			4,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TechnicalDetails, 5, 0, 1, 2);
		pullRequestLayout->addWidget(metadataGroup);

		auto* const comparisonGroup = new QGroupBox{ QStringLiteral("Review comparison"), pullRequestContent };
		comparisonGroup->setObjectName(
			QStringLiteral("epochComparisonGroup"));
		comparisonGroup->setAccessibleName(
			QStringLiteral("Review epoch comparison controls"));
		auto* const comparisonLayout = new QVBoxLayout{ comparisonGroup };
		EpochControl                 = new epoch_comparison_control{ { }, comparisonGroup };
		comparisonLayout->addWidget(EpochControl);
		pullRequestLayout->addWidget(comparisonGroup);

		auto* const diffGroup = new QGroupBox{
			QStringLiteral("Selected file comparison"), pullRequestContent
		};
		diffGroup->setObjectName(QStringLiteral("diffWorkbenchGroup"));
		diffGroup->setAccessibleName(QStringLiteral("Selected file comparison"));
		auto* const diffLayout = new QVBoxLayout{ diffGroup };
		DiffWorkbench          = new diff_workbench{ diffGroup };
		DiffWorkbench->setMinimumHeight(430);
		diffLayout->addWidget(DiffWorkbench);
		pullRequestLayout->addWidget(diffGroup, 1);

		ContentSplitter = new QSplitter{
			Qt::Horizontal,
			pullRequestContent,
		};
		ContentSplitter->setObjectName(QStringLiteral("githubContentSplitter"));
		ContentSplitter->setAccessibleName(
			QStringLiteral("Pull request content"));
		ContentSplitter->setChildrenCollapsible(false);

		auto* const descriptionGroup = new QGroupBox{ QStringLiteral("Description"), ContentSplitter };
		descriptionGroup->setObjectName(
			QStringLiteral("pullRequestDescriptionGroup"));
		descriptionGroup->setAccessibleName(
			QStringLiteral("Pull request description"));
		auto* const descriptionLayout = new QVBoxLayout{ descriptionGroup };
		BodyEditor->setParent(descriptionGroup);
		descriptionLayout->addWidget(BodyEditor);

		auto* const conversationGroup = new QGroupBox{ QStringLiteral("Conversations"), ContentSplitter };
		conversationGroup->setObjectName(
			QStringLiteral("pullRequestConversationGroup"));
		conversationGroup->setAccessibleName(
			QStringLiteral("Read-only GitHub conversations"));
		auto* const conversationLayout = new QVBoxLayout{ conversationGroup };
		ConversationAvailabilityLabel  = new QLabel{ conversationGroup };
		ConversationAvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		ConversationAvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		ConversationAvailabilityLabel->setWordWrap(true);
		set_plain_text_label(ConversationAvailabilityLabel);
		auto* const conversationHeader = new QHBoxLayout;
		conversationHeader->addWidget(ConversationAvailabilityLabel, 1);
		ShowReviewThreadsButton = new QToolButton{ conversationGroup };
		ShowReviewThreadsButton->setDefaultAction(ShowReviewThreadsAction);
		ShowReviewThreadsButton->setObjectName(
			QStringLiteral("showReviewThreadsButton"));
		ShowReviewThreadsButton->setAccessibleName(
			QStringLiteral("Show review threads"));
		ShowReviewThreadsButton->setFocusPolicy(Qt::StrongFocus);
		ShowReviewThreadsButton->setToolButtonStyle(
			Qt::ToolButtonTextBesideIcon);
		conversationHeader->addWidget(ShowReviewThreadsButton);
		conversationLayout->addLayout(conversationHeader);

		ConversationStack = new QStackedWidget{ conversationGroup };
		ConversationStack->setObjectName(
			QStringLiteral("githubConversationState"));
		auto* const conversationEmpty = new QWidget{ ConversationStack };
		conversationEmpty->setObjectName(
			QStringLiteral("githubConversationEmptyState"));
		ConversationStack->addWidget(conversationEmpty);
		ConversationModel = new github_conversation_model{ this };
		ConversationView  = new QTreeView{ ConversationStack };
		ConversationView->setObjectName(QStringLiteral("githubConversationView"));
		ConversationView->setAccessibleName(
			QStringLiteral("Read-only GitHub conversations"));
		ConversationView->setAccessibleDescription(
			QStringLiteral(
				"Reviews, review threads, and pull request comments"));
		ConversationView->setModel(ConversationModel);
		ConversationView->setSelectionBehavior(QAbstractItemView::SelectRows);
		ConversationView->setSelectionMode(QAbstractItemView::SingleSelection);
		ConversationView->setEditTriggers(QAbstractItemView::NoEditTriggers);
		ConversationView->setRootIsDecorated(true);
		ConversationView->setItemsExpandable(true);
		ConversationView->setWordWrap(true);
		ConversationView->setUniformRowHeights(false);
		ConversationView->setTextElideMode(Qt::ElideNone);
		ConversationView->header()->hide();
		ConversationView->setItemDelegate(
			new github_conversation_delegate{ ConversationView });
		ConversationStack->addWidget(ConversationView);
		conversationLayout->addWidget(ConversationStack, 1);

		ContentSplitter->addWidget(descriptionGroup);
		ContentSplitter->addWidget(conversationGroup);
		ContentSplitter->setStretchFactor(0, 2);
		ContentSplitter->setStretchFactor(1, 3);
		ContentSplitter->setMinimumHeight(240);
		pullRequestLayout->addWidget(ContentSplitter, 1);
		PullRequestStack->addWidget(pullRequestContent);
		centralLayout->addWidget(PullRequestStack, 1);

		// The settings surface lives with the presentation it governs rather
		// than behind a modal dialog, so a reviewer sees a font or mode change
		// take effect on the very comparison they are reading. It translates
		// gestures only: the composition wires its dispatch to the appearance
		// host, and until it does, every gesture goes nowhere visible - which
		// is the honest state for an unwired surface.
		auto* const appearanceGroup = new QGroupBox{
			QStringLiteral("Appearance"),
			central,
		};
		appearanceGroup->setObjectName(QStringLiteral("appearanceSettingsGroup"));
		auto* const appearanceLayout = new QVBoxLayout{ appearanceGroup };
		AppearanceSettings           = new appearance_settings_panel{ { }, appearanceGroup };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		appearanceLayout->addWidget(AppearanceSettings);
		centralLayout->addWidget(appearanceGroup);

		presentationScroll->setWidget(central);
		setCentralWidget(presentationScroll);
		statusBar()->hide();

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
		connect(
			TechnicalDetailsButton,
			&QToolButton::toggled,
			this,
			[this](const bool aVisible) {
				TechnicalDetails->setVisible(aVisible);
				TechnicalDetailsButton->setArrowType(
					aVisible ? Qt::DownArrow : Qt::RightArrow);
				TechnicalDetailsButton->setAccessibleName(
					aVisible
						? QStringLiteral("Hide pull request technical details")
						: QStringLiteral("Show pull request technical details"));
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
		connect(ShowReviewThreadsAction,
				&QAction::triggered,
				this,
				[this] { focus_review_threads(); });

		setTabOrder(ProfileSelector, ConnectButton);
		setTabOrder(ConnectButton, ConnectionDetailsButton);
		setTabOrder(ConnectionDetailsButton, OwnerEditor);
		setTabOrder(OwnerEditor, RepositoryEditor);
		setTabOrder(RepositoryEditor, PullRequestEditor);
		setTabOrder(PullRequestEditor, OpenButton);
		setTabOrder(OpenButton, RefreshButton);
		setTabOrder(RefreshButton, TechnicalDetailsButton);
		setTabOrder(TechnicalDetailsButton, BodyEditor);
		setTabOrder(BodyEditor, ShowReviewThreadsButton);
		setTabOrder(ShowReviewThreadsButton, ConversationView);

		CapabilityLabel->setText(capabilities_text({ }));
		apply_rate_limits({ });
		clear_pull_request();
		update_selected_profile();
		update_connect_enabled();
		apply_action_state(
			OpenAction,
			OpenButton,
			false,
			core::github_reason_code::session_required,
			QStringLiteral("Open the specified pull request"));
		apply_action_state(
			RefreshAction,
			RefreshButton,
			false,
			core::github_reason_code::no_active_pull_request,
			QStringLiteral("Refresh the current pull request"));
		apply_action_state(
			ShowReviewThreadsAction,
			ShowReviewThreadsButton,
			false,
			core::github_reason_code::no_active_pull_request,
			QStringLiteral("Focus the review-thread conversations"));
		refresh_native_appearance();
		update_presentation_state();
		update_splitter_orientation();
	}

	void github_window::publish(
		const application::github_presentation_update& aUpdate)
	{
		if (aUpdate.Transition.has_value()) {
			apply_transition(*aUpdate.Transition);
		}
		if (aUpdate.Session.has_value()) {
			apply_session(*aUpdate.Session);
		}
		if (aUpdate.PullRequest.has_value()) {
			apply_pull_request(*aUpdate.PullRequest);
		}
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			apply_host_diagnostic(diagnostic);
		}
		announce_presentation_update();
	}

	auto github_window::connect_action() const noexcept -> QAction*
	{
		return ConnectAction;
	}

	auto github_window::open_action() const noexcept -> QAction*
	{
		return OpenAction;
	}

	auto github_window::refresh_action() const noexcept -> QAction*
	{
		return RefreshAction;
	}

	auto github_window::show_review_threads_action() const noexcept -> QAction*
	{
		return ShowReviewThreadsAction;
	}

	auto github_window::profile_selector() const noexcept -> QComboBox*
	{
		return ProfileSelector;
	}

	auto github_window::owner_editor() const noexcept -> QLineEdit*
	{
		return OwnerEditor;
	}

	auto github_window::repository_editor() const noexcept -> QLineEdit*
	{
		return RepositoryEditor;
	}

	auto github_window::pull_request_editor() const noexcept -> QSpinBox*
	{
		return PullRequestEditor;
	}

	auto github_window::conversation_view() const noexcept -> QTreeView*
	{
		return ConversationView;
	}

	auto github_window::conversation_model() const noexcept
		-> github_conversation_model*
	{
		return ConversationModel;
	}

	auto github_window::epoch_control() const noexcept
		-> epoch_comparison_control*
	{
		return EpochControl;
	}

	auto github_window::diff_comparison_workbench() const noexcept
		-> diff_workbench*
	{
		return DiffWorkbench;
	}

	auto github_window::appearance_settings() const noexcept
		-> appearance_settings_panel*
	{
		return AppearanceSettings;
	}

	auto github_window::status_text() const -> std::string
	{
		return StatusLabel->text().toUtf8().toStdString();
	}

	auto github_window::diagnostic_text() const -> std::string
	{
		return DiagnosticLabel->text().toUtf8().toStdString();
	}

	auto github_window::active_host_text() const -> std::string
	{
		return HostLabel->text().toUtf8().toStdString();
	}

	auto github_window::active_account_text() const -> std::string
	{
		return AccountLabel->text().toUtf8().toStdString();
	}

	auto github_window::statistics() const noexcept -> github_window_statistics
	{
		return Statistics;
	}

	void github_window::changeEvent(QEvent* const aEvent)
	{
		QMainWindow::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::ApplicationPaletteChange:
		case QEvent::PaletteChange:
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
			refresh_native_appearance();
			break;
		default:
			break;
		}
	}

	void github_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		update_splitter_orientation();
	}

	void github_window::apply_transition(
		const application::github_transition_projection& aTransition)
	{
		for (const auto& command : aTransition.Commands) {
			apply_command(command);
		}
		if (aTransition.Identity.has_value()) {
			apply_identity(*aTransition.Identity);
		}
		if (aTransition.Capabilities.has_value()) {
			apply_capabilities(aTransition.Capabilities->Capabilities);
		}
		if (
			aTransition.PullRequest.has_value()
			&& !aTransition.PullRequest->Summary.has_value()) {
			clear_pull_request();
		}
		if (aTransition.Status.has_value()) {
			apply_status(aTransition.Status->Status);
		}
		for (const auto& diagnostic : aTransition.Diagnostics) {
			apply_diagnostic(diagnostic.Code);
		}
	}

	void github_window::apply_session(
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

	void github_window::apply_pull_request(
		const application::github_pull_request_projection& aPullRequest)
	{
		const auto selectedIdentity = ConversationModel->id_for_index(ConversationView->currentIndex());

		PullRequestLabel->setText(
			QStringLiteral("%1/%2 #%3")
				.arg(
					text(aPullRequest.Owner),
					text(aPullRequest.RepositoryName),
					QString::number(aPullRequest.Number.value())));
		TitleLabel->setText(text(aPullRequest.Title));
		StateLabel->setText(
			pull_request_state_text(aPullRequest.State, aPullRequest.Draft));
		AuthorLabel->setText(
			aPullRequest.AuthorLogin.empty()
				? QStringLiteral("Unknown author")
				: QStringLiteral("@%1").arg(text(aPullRequest.AuthorLogin)));
		BranchLabel->setText(
			QStringLiteral("%1 → %2")
				.arg(text(aPullRequest.BaseRef), text(aPullRequest.HeadRef)));
		OidLabel->setText(
			QStringLiteral("Base %1\nHead %2")
				.arg(text(aPullRequest.BaseOid), text(aPullRequest.HeadOid)));
		CountLabel->setText(
			QStringLiteral(
				"%1 files · %2 commits · %3 reviews · %4 review threads · "
				"%5 pull request comments")
				.arg(
					QString::number(aPullRequest.FileCount),
					QString::number(aPullRequest.CommitCount),
					QString::number(aPullRequest.ReviewCount),
					QString::number(aPullRequest.ReviewThreadCount),
					QString::number(aPullRequest.IssueCommentCount)));
		BodyEditor->setPlainText(text(aPullRequest.Body));
		ConversationAvailabilityLabel->setText(
			review_thread_availability_text(
				aPullRequest.ReviewThreadsAvailable,
				aPullRequest.ReviewThreadCount,
				aPullRequest.Capabilities.ReviewThreads));
		ConversationModel->apply(aPullRequest.Conversations);
		HasPullRequest = true;
		ConversationStack->setCurrentIndex(
			aPullRequest.Conversations.empty() ? 0 : 1);
		ConversationView->doItemsLayout();
		if (
			ExpandConversationsOnNextProjection
			&& !aPullRequest.Conversations.empty()) {
			ConversationView->expandAll();
			ExpandConversationsOnNextProjection = false;
		}
		if (!selectedIdentity.empty()) {
			const auto restored = ConversationModel->index_for_id(selectedIdentity);
			if (restored.isValid()) {
				const QSignalBlocker blocker{ ConversationView->selectionModel() };
				ConversationView->setCurrentIndex(restored);
			}
		}
		apply_capabilities(aPullRequest.Capabilities);
		apply_rate_limits(aPullRequest.RateLimits);
		update_presentation_state();
		++Statistics.PullRequestUpdates;
	}

	void github_window::apply_host_diagnostic(
		const application::github_host_diagnostic_projection& aDiagnostic)
	{
		apply_rate_limits(aDiagnostic.RateLimits);
		apply_diagnostic(
			aDiagnostic.Reason,
			aDiagnostic.RetryAfterSeconds,
			aDiagnostic.RateLimitCause,
			true);
	}

	void github_window::apply_command(
		const core::github_command_projection& aCommand)
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
			apply_action_state(
				ShowReviewThreadsAction,
				ShowReviewThreadsButton,
				aCommand.Enabled,
				aCommand.DisabledReason,
				QStringLiteral("Focus the review-thread conversations"));
			break;
		}
		++Statistics.CommandUpdates;
	}

	void github_window::apply_identity(
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

	void github_window::apply_capabilities(
		const core::github_capabilities& aCapabilities)
	{
		CapabilityLabel->setText(capabilities_text(aCapabilities));
		++Statistics.CapabilityUpdates;
	}

	void github_window::apply_status(const core::github_status aStatus)
	{
		CurrentStatus = aStatus;
		if (aStatus == core::github_status::loading_pull_request && HasPullRequest) {
			StatusLabel->setText(
				QStringLiteral("Refreshing the pull request."));
		} else if (
			aStatus == core::github_status::pull_request_failed
			&& HasPullRequest) {
			StatusLabel->setText(
				QStringLiteral(
					"Refresh failed. Showing the last accepted pull request."));
		} else if (
			aStatus == core::github_status::pull_request_cancelled
			&& HasPullRequest) {
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
		update_presentation_state();
		++Statistics.StatusUpdates;
	}

	void github_window::apply_diagnostic(
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
		update_presentation_state();
		++Statistics.DiagnosticUpdates;
	}

	void github_window::apply_rate_limits(
		const application::github_rate_limits_projection& aLimits)
	{
		RateLimitLabel->setText(rate_limits_text(aLimits));
	}

	void github_window::clear_pull_request()
	{
		HasPullRequest                      = false;
		ExpandConversationsOnNextProjection = true;
		PullRequestLabel->setText(QStringLiteral("No pull request is open"));
		TitleLabel->setText(QStringLiteral("Not available"));
		StateLabel->setText(QStringLiteral("Not available"));
		AuthorLabel->setText(QStringLiteral("Not available"));
		BranchLabel->setText(QStringLiteral("Not available"));
		OidLabel->setText(QStringLiteral("Not available"));
		CountLabel->setText(QStringLiteral("No records"));
		BodyEditor->clear();
		ConversationAvailabilityLabel->setText(
			QStringLiteral(
				"Review threads are unavailable until a pull request is open."));
		ConversationModel->clear();
		ConversationStack->setCurrentIndex(0);
		TechnicalDetailsButton->setChecked(false);
		update_presentation_state();
	}

	void github_window::update_selected_profile()
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

	void github_window::update_connect_enabled()
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

	void github_window::update_presentation_state()
	{
		PullRequestStack->setCurrentIndex(HasPullRequest ? 1 : 0);

		QString state;
		if (HasPullRequest) {
			switch (CurrentStatus) {
			case core::github_status::loading_pull_request:
				state = QStringLiteral("refreshing");
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("loaded-error");
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("loaded-cancelled");
				break;
			default:
				state = QStringLiteral("loaded");
				break;
			}
		} else {
			switch (CurrentStatus) {
			case core::github_status::no_profile:
				state = QStringLiteral("disconnected");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connect to GitHub"));
				EmptyStateBodyLabel->setText(
					Profiles.empty()
						? QStringLiteral(
							  "No GitHub host profiles are configured.")
						: QStringLiteral(
							  "Choose a host profile above, then connect."));
				break;
			case core::github_status::establishing_session:
				state = QStringLiteral("connecting");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connecting to GitHub…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The selected host is establishing an authenticated "
						"session."));
				break;
			case core::github_status::session_ready:
			case core::github_status::pull_request_ready:
				state = QStringLiteral("empty");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Open a pull request"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Enter an owner, repository, and pull request number "
						"above."));
				break;
			case core::github_status::loading_pull_request:
				state = QStringLiteral("loading");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Loading pull request…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"BetterPullReview is reading the selected GitHub "
						"snapshot."));
				break;
			case core::github_status::session_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection unavailable"));
				EmptyStateBodyLabel->setText(
					DiagnosticLabel->isVisible()
						? DiagnosticLabel->text()
						: QStringLiteral(
							  "Review the connection details and try again."));
				break;
			case core::github_status::session_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Choose Connect when you are ready to try again."));
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request unavailable"));
				EmptyStateBodyLabel->setText(
					DiagnosticLabel->isVisible()
						? DiagnosticLabel->text()
						: QStringLiteral(
							  "Check the locator and try opening it again."));
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request loading canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The locator is unchanged and can be opened again."));
				break;
			}
		}
		PullRequestStack->setProperty("githubPresentationState", state);

		StatusBanner->setAccessibleDescription(presentation_summary_text());
	}

	void github_window::update_splitter_orientation()
	{
		const auto narrow = width() < 820;
		if (
			LocatorLayout != nullptr && LocatorActions != nullptr
			&& LocatorActions->property("githubNarrowLayout").toBool() != narrow) {
			LocatorLayout->removeWidget(LocatorActions);
			if (narrow) {
				LocatorLayout->addWidget(LocatorActions, 1, 0, 1, 6);
			} else {
				LocatorLayout->addWidget(LocatorActions, 0, 6);
			}
			LocatorActions->setProperty("githubNarrowLayout", narrow);
		}

		if (ContentSplitter == nullptr) {
			return;
		}
		const auto desired = narrow ? Qt::Vertical : Qt::Horizontal;
		if (ContentSplitter->orientation() == desired) {
			return;
		}
		ContentSplitter->setOrientation(desired);
		const auto extent = desired == Qt::Horizontal
			? std::max(ContentSplitter->width(), 500)
			: std::max(ContentSplitter->height(), 400);
		ContentSplitter->setSizes({ extent * 2 / 5, extent * 3 / 5 });
	}

	void github_window::refresh_native_appearance()
	{
		auto titleFont = font();
		titleFont.setWeight(QFont::DemiBold);
		if (titleFont.pointSizeF() > 0.0) {
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.35);
		}
		TitleLabel->setFont(titleFont);

		auto emptyTitleFont = font();
		emptyTitleFont.setWeight(QFont::DemiBold);
		if (emptyTitleFont.pointSizeF() > 0.0) {
			emptyTitleFont.setPointSizeF(
				emptyTitleFont.pointSizeF() * 1.2);
		}
		EmptyStateTitleLabel->setFont(emptyTitleFont);

		auto emphasisFont = font();
		emphasisFont.setWeight(QFont::DemiBold);
		StatusLabel->setFont(emphasisFont);
		PullRequestLabel->setFont(emphasisFont);
		StateLabel->setFont(emphasisFont);

		ConnectAction->setIcon(
			style()->standardIcon(QStyle::SP_DialogApplyButton));
		OpenAction->setIcon(
			style()->standardIcon(QStyle::SP_DialogOpenButton));
		RefreshAction->setIcon(
			style()->standardIcon(QStyle::SP_BrowserReload));
		ShowReviewThreadsAction->setIcon(
			style()->standardIcon(QStyle::SP_FileDialogListView));

		ConversationView->doItemsLayout();
		ConversationView->viewport()->update();
		StatusBanner->update();
		PullRequestStack->update();
	}

	void github_window::announce_presentation_update()
	{
		const auto announcement = presentation_summary_text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		QAccessibleAnnouncementEvent event{ StatusBanner, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto github_window::presentation_summary_text() const -> QString
	{
		auto summary = StatusLabel->text();
		if (DiagnosticLabel->isVisible() && !DiagnosticLabel->text().isEmpty()) {
			summary.append(
				QStringLiteral(" %1").arg(DiagnosticLabel->text()));
		}
		return summary;
	}

	void github_window::focus_review_threads()
	{
		for (auto row = 0; row < ConversationModel->rowCount(); ++row) {
			const auto modelIndex = ConversationModel->index(row, 0);
			if (
				modelIndex.data(github_conversation_model::kind_role).toInt()
				== static_cast<int>(
					application::github_conversation_kind::review_thread)) {
				ConversationView->expand(modelIndex);
				ConversationView->setCurrentIndex(modelIndex);
				ConversationView->scrollTo(
					modelIndex, QAbstractItemView::PositionAtCenter);
				break;
			}
		}
		ConversationView->setFocus(Qt::ShortcutFocusReason);
	}

	auto github_window::selected_profile() const
		-> std::optional<core::host_profile_id>
	{
		if (ProfileSelector->currentIndex() < 0) {
			return std::nullopt;
		}
		const auto value = ProfileSelector->currentData().toULongLong();
		return value == 0U
			? std::nullopt
			: std::optional{ core::host_profile_id{
				  static_cast<std::uint64_t>(value) } };
	}

	auto github_window::profile_option(const core::host_profile_id aProfile) const
		-> const application::github_profile_option*
	{
		const auto found = std::find_if(
			Profiles.begin(),
			Profiles.end(),
			[aProfile](const application::github_profile_option& aOption) {
				return aOption.Profile == aProfile;
			});
		return found == Profiles.end() ? nullptr : &*found;
	}

	auto github_window::profile_index(const core::host_profile_id aProfile) const
		-> int
	{
		for (auto index = 0; index < ProfileSelector->count(); ++index) {
			if (ProfileSelector->itemData(index).toULongLong() == aProfile.value()) {
				return index;
			}
		}
		return -1;
	}
}
