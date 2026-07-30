#include "bpr/qt/github_window.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
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

namespace bpr::qt
{
namespace
{
[[nodiscard]] auto text(const std::string& value) -> QString
{
    return QString::fromUtf8(
        value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] auto valid(
    const application::github_profile_product product) noexcept -> bool
{
    switch(product)
    {
    case application::github_profile_product::github_com:
    case application::github_profile_product::github_enterprise_server:
        return true;
    }
    return false;
}

[[nodiscard]] auto valid(
    const application::github_profile_transport transport) noexcept -> bool
{
    switch(transport)
    {
    case application::github_profile_transport::github_cli:
    case application::github_profile_transport::native_https:
        return true;
    }
    return false;
}

[[nodiscard]] auto valid(
    const application::github_profile_option& option) noexcept -> bool
{
    return option.Profile.value() != 0U && !option.Label.empty()
        && !option.Hostname.empty() && valid(option.Product)
        && valid(option.Transport);
}

[[nodiscard]] auto product_text(
    const application::github_profile_product product) -> QString
{
    switch(product)
    {
    case application::github_profile_product::github_com:
        return QStringLiteral("GitHub.com");
    case application::github_profile_product::github_enterprise_server:
        return QStringLiteral("GitHub Enterprise Server");
    }
    return QStringLiteral("Unknown GitHub product");
}

[[nodiscard]] auto transport_text(
    const application::github_profile_transport transport) -> QString
{
    switch(transport)
    {
    case application::github_profile_transport::github_cli:
        return QStringLiteral("GitHub CLI");
    case application::github_profile_transport::native_https:
        return QStringLiteral("Native HTTPS");
    }
    return QStringLiteral("Unknown transport");
}

[[nodiscard]] auto github_status_text(
    const core::github_status status) -> QString
{
    switch(status)
    {
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
    const application::github_rate_limit_resource_projection resource)
    -> QString
{
    switch(resource)
    {
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
    const core::github_reason_code reason,
    const std::optional<std::uint32_t> retry_after,
    const std::optional<application::github_rate_limit_projection>&
        rate_limit) -> QString
{
    QString result;
    switch(reason)
    {
    case core::github_reason_code::none:
        return {};
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
        result =
            QStringLiteral("An outdated GitHub result was safely ignored.");
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
        result = rate_limit.has_value()
            ? QStringLiteral("%1 rate limit reached.")
                  .arg(resource_text(rate_limit->Resource))
            : QStringLiteral("GitHub rate limit reached.");
        if(rate_limit.has_value() && rate_limit->SecondaryLimited)
        {
            result = QStringLiteral(
                "%1 temporarily limited this request.")
                         .arg(resource_text(rate_limit->Resource));
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
        result =
            QStringLiteral("The selected GitHub CLI connection is unavailable.");
        break;
    case core::github_reason_code::cancelled:
        result = QStringLiteral("The GitHub request was canceled.");
        break;
    }

    if(retry_after.has_value())
    {
        result.append(
            QStringLiteral(" Try again in %1 seconds.")
                .arg(QString::number(*retry_after)));
    }
    return result;
}

[[nodiscard]] auto capability_text(
    const core::github_capability_state state) -> QString
{
    switch(state)
    {
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
    const core::github_capabilities& capabilities) -> QString
{
    return QStringLiteral(
               "Review threads: %1\n"
               "Viewed-file changes: %2\n"
               "Resolve review threads: %3\n"
               "Reopen review threads: %4")
        .arg(
            capability_text(capabilities.ReviewThreads),
            capability_text(capabilities.ViewedFileMutations),
            capability_text(capabilities.ResolveThread),
            capability_text(capabilities.UnresolveThread));
}

[[nodiscard]] auto rate_limit_line(
    const QString& label,
    const application::github_rate_limit_projection& limit) -> QString
{
    if(limit.SecondaryLimited)
    {
        return QStringLiteral("%1: Temporarily limited").arg(label);
    }
    return QStringLiteral("%1: %2 of %3 requests remaining")
        .arg(
            label,
            QString::number(limit.Remaining),
            QString::number(limit.Limit));
}

[[nodiscard]] auto rate_limits_text(
    const application::github_rate_limits_projection& limits) -> QString
{
    QStringList lines;
    if(limits.Core.has_value())
    {
        lines.push_back(rate_limit_line(QStringLiteral("API"), *limits.Core));
    }
    if(limits.Search.has_value())
    {
        lines.push_back(
            rate_limit_line(QStringLiteral("Search"), *limits.Search));
    }
    if(limits.Graphql.has_value())
    {
        lines.push_back(
            rate_limit_line(QStringLiteral("GraphQL"), *limits.Graphql));
    }
    return lines.isEmpty()
        ? QStringLiteral("Rate limits: Not reported")
        : lines.join(QLatin1Char('\n'));
}

[[nodiscard]] auto pull_request_state_text(
    const application::github_pull_request_state_projection state,
    const bool draft) -> QString
{
    QString result;
    switch(state)
    {
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
    if(draft)
    {
        result.append(QStringLiteral(" draft"));
    }
    return result;
}

[[nodiscard]] auto review_thread_availability_text(
    const bool available,
    const std::size_t count,
    const core::github_capability_state capability) -> QString
{
    if(available)
    {
        if(count == 0U)
        {
            return QStringLiteral(
                "Review threads are available; this pull request has none.");
        }
        if(count == 1U)
        {
            return QStringLiteral("1 review thread available.");
        }
        return QStringLiteral("%1 review threads available.")
            .arg(QString::number(count));
    }

    switch(capability)
    {
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

void set_plain_text_label(QLabel* const label)
{
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

void apply_action_state(
    QAction* const action,
    QToolButton* const button,
    const bool enabled,
    const core::github_reason_code reason,
    const QString& enabled_description)
{
    action->setEnabled(enabled);
    const auto description = enabled
        ? enabled_description
        : reason_text(reason, std::nullopt, std::nullopt);
    action->setToolTip(description);
    action->setStatusTip(description);
    action->setProperty(
        "githubDisabledReason", static_cast<int>(enabled
                                     ? core::github_reason_code::none
                                     : reason));
    if(button != nullptr)
    {
        button->setAccessibleDescription(description);
    }
}
}

github_window::github_window(
    github_view_config config,
    application::github_intent_dispatch dispatch,
    QWidget* const parent)
    : QMainWindow{parent}
    , Dispatch{std::move(dispatch)}
{
    std::unordered_set<std::uint64_t> profile_ids;
    profile_ids.reserve(config.Profiles.size());
    for(auto& profile : config.Profiles)
    {
        if(
            valid(profile)
            && profile_ids.insert(profile.Profile.value()).second)
        {
            Profiles.push_back(std::move(profile));
        }
    }

    setObjectName(QStringLiteral("githubWindow"));
    setWindowTitle(QStringLiteral("BetterPullReview — GitHub"));
    setMinimumSize(640, 420);
    resize(1100, 720);

    auto* const toolbar = addToolBar(QStringLiteral("GitHub"));
    toolbar->setObjectName(QStringLiteral("githubToolbar"));
    toolbar->setAccessibleName(QStringLiteral("GitHub actions"));
    toolbar->setMovable(false);

    ConnectAction = toolbar->addAction(QStringLiteral("&Connect"));
    ConnectAction->setObjectName(QStringLiteral("connectGithubAction"));
    ConnectAction->setShortcut(
        QKeySequence{QStringLiteral("Ctrl+Shift+C")});
    ConnectAction->setShortcutContext(Qt::ApplicationShortcut);
    OpenAction = toolbar->addAction(QStringLiteral("&Open pull request"));
    OpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
    OpenAction->setShortcut(QKeySequence::Open);
    OpenAction->setShortcutContext(Qt::ApplicationShortcut);
    RefreshAction = toolbar->addAction(QStringLiteral("&Refresh"));
    RefreshAction->setObjectName(QStringLiteral("refreshPullRequestAction"));
    RefreshAction->setShortcut(QKeySequence::Refresh);
    RefreshAction->setShortcutContext(Qt::ApplicationShortcut);
    ShowReviewThreadsAction =
        toolbar->addAction(QStringLiteral("Show review &threads"));
    ShowReviewThreadsAction->setObjectName(
        QStringLiteral("showReviewThreadsAction"));

    ConnectButton = qobject_cast<QToolButton*>(
        toolbar->widgetForAction(ConnectAction));
    OpenButton =
        qobject_cast<QToolButton*>(toolbar->widgetForAction(OpenAction));
    RefreshButton =
        qobject_cast<QToolButton*>(toolbar->widgetForAction(RefreshAction));
    ShowReviewThreadsButton = qobject_cast<QToolButton*>(
        toolbar->widgetForAction(ShowReviewThreadsAction));
    if(ConnectButton != nullptr)
    {
        ConnectButton->setObjectName(QStringLiteral("connectGithubButton"));
        ConnectButton->setAccessibleName(
            QStringLiteral("Connect GitHub host"));
        ConnectButton->setFocusPolicy(Qt::StrongFocus);
    }
    if(OpenButton != nullptr)
    {
        OpenButton->setObjectName(QStringLiteral("openPullRequestButton"));
        OpenButton->setAccessibleName(QStringLiteral("Open pull request"));
        OpenButton->setFocusPolicy(Qt::StrongFocus);
    }
    if(RefreshButton != nullptr)
    {
        RefreshButton->setObjectName(
            QStringLiteral("refreshPullRequestButton"));
        RefreshButton->setAccessibleName(QStringLiteral("Refresh pull request"));
        RefreshButton->setFocusPolicy(Qt::StrongFocus);
    }
    if(ShowReviewThreadsButton != nullptr)
    {
        ShowReviewThreadsButton->setObjectName(
            QStringLiteral("showReviewThreadsButton"));
        ShowReviewThreadsButton->setAccessibleName(
            QStringLiteral("Show review threads"));
        ShowReviewThreadsButton->setFocusPolicy(Qt::StrongFocus);
    }

    auto* const presentation_scroll = new QScrollArea{this};
    presentation_scroll->setObjectName(
        QStringLiteral("githubPresentationScroll"));
    presentation_scroll->setAccessibleName(
        QStringLiteral("GitHub pull request presentation"));
    presentation_scroll->setWidgetResizable(true);
    presentation_scroll->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    presentation_scroll->setFrameShape(QFrame::NoFrame);

    auto* const central = new QWidget{presentation_scroll};
    central->setObjectName(QStringLiteral("githubPresentation"));
    central->setMinimumHeight(560);
    auto* const central_layout = new QVBoxLayout{central};
    central_layout->setContentsMargins(12, 12, 12, 12);
    central_layout->setSpacing(10);

    auto* const identity_group =
        new QGroupBox{QStringLiteral("GitHub identity"), central};
    identity_group->setObjectName(QStringLiteral("githubIdentityGroup"));
    identity_group->setAccessibleName(QStringLiteral("GitHub identity"));
    auto* const identity_layout = new QFormLayout{identity_group};
    identity_layout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    ProfileSelector = new QComboBox{identity_group};
    ProfileSelector->setObjectName(QStringLiteral("githubProfileSelector"));
    ProfileSelector->setAccessibleName(QStringLiteral("GitHub host profile"));
    ProfileSelector->setAccessibleDescription(
        QStringLiteral("Select a non-secret GitHub host profile"));
    for(const auto& profile : Profiles)
    {
        ProfileSelector->addItem(
            text(profile.Label),
            QVariant::fromValue<qulonglong>(profile.Profile.value()));
    }
    auto* const profile_label =
        new QLabel{QStringLiteral("&Host profile:"), identity_group};
    profile_label->setObjectName(QStringLiteral("githubProfileLabel"));
    profile_label->setBuddy(ProfileSelector);

    HostLabel = new QLabel{identity_group};
    HostLabel->setObjectName(QStringLiteral("activeGithubHost"));
    HostLabel->setAccessibleName(QStringLiteral("Active GitHub host"));
    set_plain_text_label(HostLabel);
    TransportLabel = new QLabel{identity_group};
    TransportLabel->setObjectName(QStringLiteral("activeGithubTransport"));
    TransportLabel->setAccessibleName(
        QStringLiteral("Active GitHub transport"));
    set_plain_text_label(TransportLabel);
    AccountLabel = new QLabel{QStringLiteral("Not authenticated"), identity_group};
    AccountLabel->setObjectName(QStringLiteral("activeGithubAccount"));
    AccountLabel->setAccessibleName(
        QStringLiteral("Authenticated GitHub account"));
    set_plain_text_label(AccountLabel);
    CapabilityLabel = new QLabel{identity_group};
    CapabilityLabel->setObjectName(QStringLiteral("githubCapabilities"));
    CapabilityLabel->setAccessibleName(
        QStringLiteral("GitHub host capabilities"));
    CapabilityLabel->setWordWrap(true);
    set_plain_text_label(CapabilityLabel);
    RateLimitLabel = new QLabel{identity_group};
    RateLimitLabel->setObjectName(QStringLiteral("githubRateLimits"));
    RateLimitLabel->setAccessibleName(QStringLiteral("GitHub rate limits"));
    RateLimitLabel->setWordWrap(true);
    set_plain_text_label(RateLimitLabel);

    identity_layout->addRow(profile_label, ProfileSelector);
    identity_layout->addRow(QStringLiteral("Host:"), HostLabel);
    identity_layout->addRow(QStringLiteral("Transport:"), TransportLabel);
    identity_layout->addRow(QStringLiteral("Account:"), AccountLabel);
    identity_layout->addRow(QStringLiteral("Capabilities:"), CapabilityLabel);
    identity_layout->addRow(QStringLiteral("Availability:"), RateLimitLabel);
    central_layout->addWidget(identity_group);

    auto* const locator_group =
        new QGroupBox{QStringLiteral("Open pull request"), central};
    locator_group->setObjectName(QStringLiteral("pullRequestLocatorGroup"));
    locator_group->setAccessibleName(QStringLiteral("Pull request locator"));
    auto* const locator_layout = new QHBoxLayout{locator_group};

    OwnerEditor = new QLineEdit{locator_group};
    OwnerEditor->setObjectName(QStringLiteral("repositoryOwnerEditor"));
    OwnerEditor->setAccessibleName(QStringLiteral("Repository owner"));
    OwnerEditor->setMaxLength(
        static_cast<int>(core::maximum_repository_owner_length));
    auto* const owner_label =
        new QLabel{QStringLiteral("&Owner:"), locator_group};
    owner_label->setBuddy(OwnerEditor);

    RepositoryEditor = new QLineEdit{locator_group};
    RepositoryEditor->setObjectName(QStringLiteral("repositoryNameEditor"));
    RepositoryEditor->setAccessibleName(QStringLiteral("Repository name"));
    RepositoryEditor->setMaxLength(
        static_cast<int>(core::maximum_repository_name_length));
    auto* const repository_label =
        new QLabel{QStringLiteral("&Repository:"), locator_group};
    repository_label->setBuddy(RepositoryEditor);

    PullRequestEditor = new QSpinBox{locator_group};
    PullRequestEditor->setObjectName(QStringLiteral("pullRequestNumberEditor"));
    PullRequestEditor->setAccessibleName(QStringLiteral("Pull request number"));
    PullRequestEditor->setRange(
        1, static_cast<int>(core::maximum_pull_request_number));
    auto* const number_label =
        new QLabel{QStringLiteral("Pull request &number:"), locator_group};
    number_label->setBuddy(PullRequestEditor);

    locator_layout->addWidget(owner_label);
    locator_layout->addWidget(OwnerEditor, 1);
    locator_layout->addWidget(repository_label);
    locator_layout->addWidget(RepositoryEditor, 1);
    locator_layout->addWidget(number_label);
    locator_layout->addWidget(PullRequestEditor);
    central_layout->addWidget(locator_group);

    auto* const content_splitter = new QSplitter{Qt::Horizontal, central};
    content_splitter->setObjectName(QStringLiteral("githubContentSplitter"));
    content_splitter->setAccessibleName(
        QStringLiteral("Pull request content"));

    auto* const metadata_group =
        new QGroupBox{QStringLiteral("Pull request"), content_splitter};
    metadata_group->setObjectName(QStringLiteral("pullRequestMetadataGroup"));
    metadata_group->setAccessibleName(QStringLiteral("Pull request metadata"));
    auto* const metadata_layout = new QFormLayout{metadata_group};
    metadata_layout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    PullRequestLabel = new QLabel{metadata_group};
    PullRequestLabel->setObjectName(QStringLiteral("pullRequestIdentity"));
    PullRequestLabel->setAccessibleName(
        QStringLiteral("Pull request identity"));
    set_plain_text_label(PullRequestLabel);
    TitleLabel = new QLabel{metadata_group};
    TitleLabel->setObjectName(QStringLiteral("pullRequestTitle"));
    TitleLabel->setAccessibleName(QStringLiteral("Pull request title"));
    TitleLabel->setWordWrap(true);
    set_plain_text_label(TitleLabel);
    StateLabel = new QLabel{metadata_group};
    StateLabel->setObjectName(QStringLiteral("pullRequestState"));
    StateLabel->setAccessibleName(QStringLiteral("Pull request state"));
    set_plain_text_label(StateLabel);
    AuthorLabel = new QLabel{metadata_group};
    AuthorLabel->setObjectName(QStringLiteral("pullRequestAuthor"));
    AuthorLabel->setAccessibleName(QStringLiteral("Pull request author"));
    set_plain_text_label(AuthorLabel);
    BranchLabel = new QLabel{metadata_group};
    BranchLabel->setObjectName(QStringLiteral("pullRequestBranches"));
    BranchLabel->setAccessibleName(QStringLiteral("Pull request branches"));
    BranchLabel->setWordWrap(true);
    set_plain_text_label(BranchLabel);
    OidLabel = new QLabel{metadata_group};
    OidLabel->setObjectName(QStringLiteral("pullRequestCommitOids"));
    OidLabel->setAccessibleName(
        QStringLiteral("Pull request commit identities"));
    OidLabel->setWordWrap(true);
    set_plain_text_label(OidLabel);
    CountLabel = new QLabel{metadata_group};
    CountLabel->setObjectName(QStringLiteral("pullRequestCounts"));
    CountLabel->setAccessibleName(QStringLiteral("Pull request record counts"));
    CountLabel->setWordWrap(true);
    set_plain_text_label(CountLabel);
    BodyEditor = new QPlainTextEdit{metadata_group};
    BodyEditor->setObjectName(QStringLiteral("pullRequestBody"));
    BodyEditor->setAccessibleName(QStringLiteral("Pull request description"));
    BodyEditor->setReadOnly(true);
    BodyEditor->setTabChangesFocus(true);

    metadata_layout->addRow(QStringLiteral("Pull request:"), PullRequestLabel);
    metadata_layout->addRow(QStringLiteral("Title:"), TitleLabel);
    metadata_layout->addRow(QStringLiteral("State:"), StateLabel);
    metadata_layout->addRow(QStringLiteral("Author:"), AuthorLabel);
    metadata_layout->addRow(QStringLiteral("Branches:"), BranchLabel);
    metadata_layout->addRow(QStringLiteral("Commits:"), OidLabel);
    metadata_layout->addRow(QStringLiteral("Records:"), CountLabel);
    metadata_layout->addRow(QStringLiteral("Description:"), BodyEditor);

    auto* const conversation_group =
        new QGroupBox{QStringLiteral("Conversations"), content_splitter};
    conversation_group->setObjectName(
        QStringLiteral("pullRequestConversationGroup"));
    conversation_group->setAccessibleName(
        QStringLiteral("Read-only GitHub conversations"));
    auto* const conversation_layout = new QVBoxLayout{conversation_group};
    ConversationAvailabilityLabel = new QLabel{conversation_group};
    ConversationAvailabilityLabel->setObjectName(
        QStringLiteral("reviewThreadAvailability"));
    ConversationAvailabilityLabel->setAccessibleName(
        QStringLiteral("Review-thread availability"));
    ConversationAvailabilityLabel->setWordWrap(true);
    set_plain_text_label(ConversationAvailabilityLabel);
    conversation_layout->addWidget(ConversationAvailabilityLabel);
    ConversationModel = new github_conversation_model{this};
    ConversationView = new QTreeView{conversation_group};
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
    ConversationView->header()->setStretchLastSection(true);
    conversation_layout->addWidget(ConversationView);

    content_splitter->addWidget(metadata_group);
    content_splitter->addWidget(conversation_group);
    content_splitter->setStretchFactor(0, 1);
    content_splitter->setStretchFactor(1, 1);
    content_splitter->setMinimumHeight(220);
    central_layout->addWidget(content_splitter, 1);

    DiagnosticLabel = new QLabel{central};
    DiagnosticLabel->setObjectName(QStringLiteral("githubDiagnostic"));
    DiagnosticLabel->setAccessibleName(QStringLiteral("GitHub diagnostic"));
    DiagnosticLabel->setWordWrap(true);
    set_plain_text_label(DiagnosticLabel);
    DiagnosticLabel->hide();
    central_layout->addWidget(DiagnosticLabel);
    presentation_scroll->setWidget(central);
    setCentralWidget(presentation_scroll);

    StatusLabel = new QLabel{QStringLiteral("No GitHub host is connected."), this};
    StatusLabel->setObjectName(QStringLiteral("githubStatus"));
    StatusLabel->setAccessibleName(QStringLiteral("GitHub status"));
    set_plain_text_label(StatusLabel);
    statusBar()->addWidget(StatusLabel, 1);

    connect(ProfileSelector,
            &QComboBox::currentIndexChanged,
            this,
            [this]
            {
                update_selected_profile();
                update_connect_enabled();
            });
    connect(ConnectAction,
            &QAction::triggered,
            this,
            [this]
            {
                if(const auto profile = selected_profile();
                   profile.has_value() && Dispatch)
                {
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
            [this]
            {
                if(!Dispatch)
                {
                    return;
                }
                Dispatch(application::github_intent{
                    core::open_pull_request_intent{
                        .Repository =
                            {
                                .Owner =
                                    OwnerEditor->text().toUtf8().toStdString(),
                                .Name = RepositoryEditor->text()
                                            .toUtf8()
                                            .toStdString(),
                            },
                        .Number = core::pull_request_number{
                            static_cast<std::uint32_t>(
                                PullRequestEditor->value())},
                    },
                });
            });
    connect(RefreshAction,
            &QAction::triggered,
            this,
            [this]
            {
                if(Dispatch)
                {
                    Dispatch(application::github_intent{
                        core::refresh_pull_request_intent{},
                    });
                }
            });
    connect(ShowReviewThreadsAction,
            &QAction::triggered,
            this,
            [this] { focus_review_threads(); });

    if(ConnectButton != nullptr)
    {
        setTabOrder(ProfileSelector, ConnectButton);
        setTabOrder(ConnectButton, OwnerEditor);
    }
    else
    {
        setTabOrder(ProfileSelector, OwnerEditor);
    }
    setTabOrder(OwnerEditor, RepositoryEditor);
    setTabOrder(RepositoryEditor, PullRequestEditor);
    if(OpenButton != nullptr)
    {
        setTabOrder(PullRequestEditor, OpenButton);
        if(RefreshButton != nullptr)
        {
            setTabOrder(OpenButton, RefreshButton);
        }
    }
    if(RefreshButton != nullptr && ShowReviewThreadsButton != nullptr)
    {
        setTabOrder(RefreshButton, ShowReviewThreadsButton);
        setTabOrder(ShowReviewThreadsButton, BodyEditor);
    }
    else if(RefreshButton != nullptr)
    {
        setTabOrder(RefreshButton, BodyEditor);
    }
    setTabOrder(BodyEditor, ConversationView);

    CapabilityLabel->setText(capabilities_text({}));
    apply_rate_limits({});
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
}

void github_window::publish(
    const application::github_presentation_update& update)
{
    if(update.Transition.has_value())
    {
        apply_transition(*update.Transition);
    }
    if(update.Session.has_value())
    {
        apply_session(*update.Session);
    }
    if(update.PullRequest.has_value())
    {
        apply_pull_request(*update.PullRequest);
    }
    for(const auto& diagnostic : update.HostDiagnostics)
    {
        apply_host_diagnostic(diagnostic);
    }
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

void github_window::apply_transition(
    const application::github_transition_projection& transition)
{
    for(const auto& command : transition.Commands)
    {
        apply_command(command);
    }
    if(transition.Identity.has_value())
    {
        apply_identity(*transition.Identity);
    }
    if(transition.Capabilities.has_value())
    {
        apply_capabilities(transition.Capabilities->Capabilities);
    }
    if(
        transition.PullRequest.has_value()
        && !transition.PullRequest->Summary.has_value())
    {
        clear_pull_request();
    }
    if(transition.Status.has_value())
    {
        apply_status(transition.Status->Status);
    }
    for(const auto& diagnostic : transition.Diagnostics)
    {
        apply_diagnostic(diagnostic.Code);
    }
}

void github_window::apply_session(
    const application::github_session_projection& session)
{
    SessionAccount = session.Account;
    if(session.DisplayName.empty())
    {
        AccountLabel->setText(
            session.Login.empty()
                ? QStringLiteral("Authenticated account")
                : QStringLiteral("@%1").arg(text(session.Login)));
    }
    else if(session.Login.empty())
    {
        AccountLabel->setText(text(session.DisplayName));
    }
    else
    {
        AccountLabel->setText(
            QStringLiteral("%1 (@%2)")
                .arg(text(session.DisplayName), text(session.Login)));
    }
    apply_capabilities(session.Capabilities);
    apply_rate_limits(session.RateLimits);
    ++Statistics.SessionUpdates;
}

void github_window::apply_pull_request(
    const application::github_pull_request_projection& pull_request)
{
    const auto selected_identity =
        ConversationModel->id_for_index(ConversationView->currentIndex());

    PullRequestLabel->setText(
        QStringLiteral("%1/%2 #%3")
            .arg(
                text(pull_request.Owner),
                text(pull_request.RepositoryName),
                QString::number(pull_request.Number.value())));
    TitleLabel->setText(text(pull_request.Title));
    StateLabel->setText(
        pull_request_state_text(pull_request.State, pull_request.Draft));
    AuthorLabel->setText(
        pull_request.AuthorLogin.empty()
            ? QStringLiteral("Unknown author")
            : QStringLiteral("@%1").arg(text(pull_request.AuthorLogin)));
    BranchLabel->setText(
        QStringLiteral("%1 → %2")
            .arg(text(pull_request.BaseRef), text(pull_request.HeadRef)));
    OidLabel->setText(
        QStringLiteral("Base %1\nHead %2")
            .arg(text(pull_request.BaseOid), text(pull_request.HeadOid)));
    CountLabel->setText(
        QStringLiteral(
            "%1 files · %2 commits · %3 reviews · %4 review threads · "
            "%5 pull request comments")
            .arg(
                QString::number(pull_request.FileCount),
                QString::number(pull_request.CommitCount),
                QString::number(pull_request.ReviewCount),
                QString::number(pull_request.ReviewThreadCount),
                QString::number(pull_request.IssueCommentCount)));
    BodyEditor->setPlainText(text(pull_request.Body));
    ConversationAvailabilityLabel->setText(
        review_thread_availability_text(
            pull_request.ReviewThreadsAvailable,
            pull_request.ReviewThreadCount,
            pull_request.Capabilities.ReviewThreads));
    ConversationModel->apply(pull_request.Conversations);
    if(!selected_identity.empty())
    {
        const auto restored =
            ConversationModel->index_for_id(selected_identity);
        if(restored.isValid())
        {
            const QSignalBlocker blocker{ConversationView->selectionModel()};
            ConversationView->setCurrentIndex(restored);
        }
    }
    apply_capabilities(pull_request.Capabilities);
    apply_rate_limits(pull_request.RateLimits);
    ++Statistics.PullRequestUpdates;
}

void github_window::apply_host_diagnostic(
    const application::github_host_diagnostic_projection& diagnostic)
{
    apply_rate_limits(diagnostic.RateLimits);
    apply_diagnostic(
        diagnostic.Reason,
        diagnostic.RetryAfterSeconds,
        diagnostic.RateLimitCause,
        true);
}

void github_window::apply_command(
    const core::github_command_projection& command)
{
    switch(command.Id)
    {
    case core::github_command_id::activate_host_profile:
        ConnectCommandEnabled = command.Enabled;
        ConnectDisabledReason = command.DisabledReason;
        update_connect_enabled();
        break;
    case core::github_command_id::open_pull_request:
        apply_action_state(
            OpenAction,
            OpenButton,
            command.Enabled,
            command.DisabledReason,
            QStringLiteral("Open the specified pull request"));
        break;
    case core::github_command_id::refresh_pull_request:
        apply_action_state(
            RefreshAction,
            RefreshButton,
            command.Enabled,
            command.DisabledReason,
            QStringLiteral("Refresh the current pull request"));
        break;
    case core::github_command_id::show_review_threads:
        apply_action_state(
            ShowReviewThreadsAction,
            ShowReviewThreadsButton,
            command.Enabled,
            command.DisabledReason,
            QStringLiteral("Focus the review-thread conversations"));
        break;
    }
    ++Statistics.CommandUpdates;
}

void github_window::apply_identity(
    const core::github_identity_delta& identity)
{
    ActiveProfile = identity.Profile;
    if(identity.Profile.has_value())
    {
        const auto index = profile_index(*identity.Profile);
        if(index >= 0)
        {
            const QSignalBlocker blocker{ProfileSelector};
            ProfileSelector->setCurrentIndex(index);
        }
    }
    SessionAccount.reset();
    AccountLabel->setText(QStringLiteral("Not authenticated"));
    apply_rate_limits({});
    update_selected_profile();
    update_connect_enabled();
    ++Statistics.IdentityUpdates;
}

void github_window::apply_capabilities(
    const core::github_capabilities& capabilities)
{
    CapabilityLabel->setText(capabilities_text(capabilities));
    ++Statistics.CapabilityUpdates;
}

void github_window::apply_status(const core::github_status status)
{
    StatusLabel->setText(github_status_text(status));
    switch(status)
    {
    case core::github_status::no_profile:
        SessionAccount.reset();
        AccountLabel->setText(QStringLiteral("Not authenticated"));
        apply_rate_limits({});
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
        apply_rate_limits({});
        DiagnosticLabel->clear();
        DiagnosticLabel->hide();
        LastDiagnosticReason.reset();
        LastDiagnosticHasHostDetail = false;
        break;
    case core::github_status::session_failed:
    case core::github_status::session_cancelled:
        SessionAccount.reset();
        AccountLabel->setText(QStringLiteral("Not authenticated"));
        apply_rate_limits({});
        break;
    case core::github_status::pull_request_failed:
    case core::github_status::pull_request_cancelled:
        break;
    }
    ++Statistics.StatusUpdates;
}

void github_window::apply_diagnostic(
    const core::github_reason_code reason,
    const std::optional<std::uint32_t> retry_after,
    const std::optional<application::github_rate_limit_projection>& rate_limit,
    const bool host_detail)
{
    if(
        !host_detail && LastDiagnosticHasHostDetail
        && LastDiagnosticReason == reason)
    {
        return;
    }
    const auto message = reason_text(reason, retry_after, rate_limit);
    DiagnosticLabel->setText(message);
    DiagnosticLabel->setVisible(!message.isEmpty());
    LastDiagnosticReason =
        message.isEmpty() ? std::nullopt : std::optional{reason};
    LastDiagnosticHasHostDetail = !message.isEmpty() && host_detail;
    ++Statistics.DiagnosticUpdates;
}

void github_window::apply_rate_limits(
    const application::github_rate_limits_projection& limits)
{
    RateLimitLabel->setText(rate_limits_text(limits));
}

void github_window::clear_pull_request()
{
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
}

void github_window::update_selected_profile()
{
    const application::github_profile_option* selected = nullptr;
    QString prefix;
    if(ActiveProfile.has_value())
    {
        selected = profile_option(*ActiveProfile);
        prefix = QStringLiteral("Active host");
    }
    else if(const auto profile = selected_profile(); profile.has_value())
    {
        selected = profile_option(*profile);
        prefix = QStringLiteral("Selected host");
    }

    if(selected == nullptr)
    {
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
    const auto has_profile = selected_profile().has_value();
    const auto enabled = ConnectCommandEnabled && has_profile;
    if(!has_profile)
    {
        ConnectAction->setEnabled(false);
        ConnectAction->setProperty(
            "githubDisabledReason",
            static_cast<int>(core::github_reason_code::invalid_profile));
        const auto description =
            QStringLiteral("No GitHub host profiles are configured.");
        ConnectAction->setToolTip(description);
        ConnectAction->setStatusTip(description);
        if(ConnectButton != nullptr)
        {
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

void github_window::focus_review_threads()
{
    for(auto row = 0; row < ConversationModel->rowCount(); ++row)
    {
        const auto model_index = ConversationModel->index(row, 0);
        if(
            model_index.data(github_conversation_model::kind_role).toInt()
            == static_cast<int>(
                application::github_conversation_kind::review_thread))
        {
            ConversationView->expand(model_index);
            ConversationView->setCurrentIndex(model_index);
            ConversationView->scrollTo(
                model_index, QAbstractItemView::PositionAtCenter);
            break;
        }
    }
    ConversationView->setFocus(Qt::ShortcutFocusReason);
}

auto github_window::selected_profile() const
    -> std::optional<core::host_profile_id>
{
    if(ProfileSelector->currentIndex() < 0)
    {
        return std::nullopt;
    }
    const auto value = ProfileSelector->currentData().toULongLong();
    return value == 0U
        ? std::nullopt
        : std::optional{core::host_profile_id{
              static_cast<std::uint64_t>(value)}};
}

auto github_window::profile_option(const core::host_profile_id profile) const
    -> const application::github_profile_option*
{
    const auto found = std::find_if(
        Profiles.begin(),
        Profiles.end(),
        [profile](const application::github_profile_option& option)
        {
            return option.Profile == profile;
        });
    return found == Profiles.end() ? nullptr : &*found;
}

auto github_window::profile_index(const core::host_profile_id profile) const
    -> int
{
    for(auto index = 0; index < ProfileSelector->count(); ++index)
    {
        if(ProfileSelector->itemData(index).toULongLong() == profile.value())
        {
            return index;
        }
    }
    return -1;
}
}
