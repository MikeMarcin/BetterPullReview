#include "bpr/qt/github_window.hpp"

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Widget>
[[nodiscard]] auto required_child(
    qt::github_window& window,
    const char* const object_name) -> Widget*
{
    auto* const child =
        window.findChild<Widget*>(QString::fromLatin1(object_name));
    expect(child != nullptr, "required GitHub presentation object exists");
    return child;
}

[[nodiscard]] auto presentation_state(qt::github_window& window) -> QString
{
    return required_child<QStackedWidget>(window, "githubViewState")
        ->property("githubPresentationState")
        .toString();
}

auto capture_layout_state(
    qt::github_window& window,
    const QString& state_name) -> void
{
    QApplication::processEvents();
    const auto rendered = window.grab();
    const auto device_ratio = window.devicePixelRatioF();
    expect(
        !rendered.isNull() && rendered.width() > 0 && rendered.height() > 0
            && rendered.width()
                   == static_cast<int>(std::lround(
                       static_cast<double>(window.width()) * device_ratio))
            && rendered.height()
                   == static_cast<int>(std::lround(
                       static_cast<double>(window.height()) * device_ratio)),
        "state layout capture is nonempty and has exact device dimensions");

    const auto configured_path =
        qEnvironmentVariable("BPR_QT_RENDER_PATH");
    if(configured_path.isEmpty())
    {
        return;
    }
    const QFileInfo configured{configured_path};
    const auto state_path =
        QStringLiteral("%1/%2-%3.png")
            .arg(
                configured.path(),
                configured.completeBaseName(),
                state_name);
    expect(
        rendered.save(state_path, "PNG"),
        "state layout capture is saved in the ignored build tree");
}

[[nodiscard]] auto profiles()
    -> std::vector<application::github_profile_option>
{
    return {
        {
            .Profile = core::host_profile_id{11U},
            .Label = "GitHub.com through GitHub CLI",
            .Hostname = "github.com",
            .Product = application::github_profile_product::github_com,
            .Transport = application::github_profile_transport::github_cli,
        },
        {
            .Profile = core::host_profile_id{12U},
            .Label = "Engineering GHES",
            .Hostname = "ghe.example.invalid",
            .Product =
                application::github_profile_product::github_enterprise_server,
            .Transport =
                application::github_profile_transport::native_https,
        },
    };
}

[[nodiscard]] auto conversation(
    std::string id,
    const application::github_conversation_kind kind,
    std::string body = {},
    std::optional<std::string> parent = std::nullopt)
    -> application::github_conversation_projection
{
    return {
        .Id = std::move(id),
        .ParentId = std::move(parent),
        .Kind = kind,
        .AuthorLogin = "reviewer",
        .Body = std::move(body),
        .Path = "src/review.cpp",
        .Line = 42U,
        .ReviewState =
            application::github_review_state_projection::commented,
    };
}

[[nodiscard]] auto initial_conversations()
    -> std::vector<application::github_conversation_projection>
{
    using kind = application::github_conversation_kind;
    auto thread = conversation("review-thread:t1", kind::review_thread);
    thread.AuthorLogin.clear();
    thread.Resolved = false;
    return {
        conversation("review:r1", kind::review, "Review body"),
        std::move(thread),
        conversation(
            "review-comment:c1",
            kind::review_comment,
            "Thread reply",
            "review-thread:t1"),
        conversation(
            "issue-comment:i1",
            kind::issue_comment,
            "<b>& canary"),
    };
}

auto test_conversation_model_is_stable_and_incremental() -> void
{
    qt::github_conversation_model model;
    std::size_t inserted = 0U;
    std::size_t removed = 0U;
    std::size_t moved = 0U;
    std::size_t changed = 0U;
    std::size_t reset = 0U;

    QObject::connect(
        &model,
        &QAbstractItemModel::rowsInserted,
        [&inserted](const QModelIndex&, const int first, const int last) {
            inserted += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::rowsRemoved,
        [&removed](const QModelIndex&, const int first, const int last) {
            removed += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::rowsMoved,
        [&moved](
            const QModelIndex&,
            const int first,
            const int last,
            const QModelIndex&,
            const int) {
            moved += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::dataChanged,
        [&changed](
            const QModelIndex& top_left,
            const QModelIndex& bottom_right,
            const QList<int>&) {
            changed += static_cast<std::size_t>(
                bottom_right.row() - top_left.row() + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::modelReset,
        [&reset] { ++reset; });

    auto values = initial_conversations();
    model.apply(values);
    expect(model.rowCount() == 3, "three root conversations are inserted");
    const auto thread = model.index_for_id("review-thread:t1");
    expect(
        thread.isValid() && model.rowCount(thread) == 1,
        "review comment is a child of its stable thread");
    const auto child = model.index_for_id("review-comment:c1");
    expect(
        child.isValid()
            && model.id_for_index(model.parent(child)) == "review-thread:t1",
        "nested review comment exposes its stable parent");
    const auto hostile = model.index_for_id("issue-comment:i1");
    expect(
        model.data(hostile, qt::github_conversation_model::body_role)
            == QStringLiteral("<b>& canary"),
        "remote markup-like body remains literal model text");

    QPersistentModelIndex stable{hostile};
    inserted = 0U;
    removed = 0U;
    moved = 0U;
    changed = 0U;

    values.back().Body = "Changed body";
    model.apply(values);
    expect(
        changed == 1U && inserted == 0U && removed == 0U && moved == 0U,
        "one semantic edit emits only dataChanged");
    expect(
        stable.isValid()
            && model.id_for_index(QModelIndex{stable}) == "issue-comment:i1",
        "data change preserves the persistent identity index");

    values.insert(
        values.begin(),
        conversation(
            "issue-comment:i2",
            application::github_conversation_kind::issue_comment,
            "New first comment"));
    inserted = 0U;
    model.apply(values);
    expect(inserted == 1U, "one new conversation emits one row insertion");
    expect(
        stable.isValid()
            && model.id_for_index(QModelIndex{stable}) == "issue-comment:i1",
        "unrelated insertion preserves the persistent identity index");

    auto reordered = values;
    const auto issue = std::ranges::find_if(
        reordered,
        [](const auto& value) { return value.Id == "issue-comment:i1"; });
    expect(issue != reordered.end(), "reorder fixture contains stable issue");
    auto issue_value = std::move(*issue);
    reordered.erase(issue);
    reordered.insert(reordered.begin(), std::move(issue_value));
    moved = 0U;
    model.apply(reordered);
    expect(moved == 1U, "stable root reorder emits one row move");
    expect(
        stable.isValid()
            && stable.row() == 0
            && QModelIndex{stable} == model.index(0, 0)
            && model.id_for_index(model.index(0, 0)) == "issue-comment:i1"
            && model.id_for_index(model.index(1, 0)) == "issue-comment:i2"
            && model.id_for_index(model.index(2, 0)) == "review:r1"
            && model.id_for_index(model.index(3, 0))
                   == "review-thread:t1",
        "row move produces the complete requested order and preserves the "
        "persistent index");

    auto reverse_reordered = reordered;
    auto first = std::move(reverse_reordered.front());
    reverse_reordered.erase(reverse_reordered.begin());
    reverse_reordered.push_back(std::move(first));
    moved = 0U;
    model.apply(reverse_reordered);
    expect(
        moved == 1U,
        "first-to-last root reorder emits one proportional row move");
    expect(
        stable.isValid()
            && stable.row() == 3
            && QModelIndex{stable} == model.index(3, 0)
            && model.id_for_index(model.index(0, 0)) == "issue-comment:i2"
            && model.id_for_index(model.index(1, 0)) == "review:r1"
            && model.id_for_index(model.index(2, 0))
                   == "review-thread:t1"
            && model.id_for_index(model.index(3, 0)) == "issue-comment:i1",
        "reverse move produces the complete requested order and preserves "
        "the persistent index");
    reordered = std::move(reverse_reordered);

    const auto review = std::ranges::find_if(
        reordered,
        [](const auto& value) { return value.Id == "review:r1"; });
    expect(review != reordered.end(), "removal fixture contains review");
    reordered.erase(review);
    removed = 0U;
    model.apply(reordered);
    expect(removed == 1U, "one removed review emits one row removal");

    const auto first_reply = std::ranges::find_if(
        reordered,
        [](const auto& value) {
            return value.Id == "review-comment:c1";
        });
    expect(first_reply != reordered.end(), "nested insertion has a parent row");
    reordered.insert(
        first_reply + 1,
        conversation(
            "review-comment:c2",
            application::github_conversation_kind::review_comment,
            "Second reply",
            "review-thread:t1"));
    inserted = 0U;
    model.apply(reordered);
    const auto updated_thread =
        model.index_for_id("review-thread:t1");
    expect(
        inserted == 1U
            && model.rowCount(updated_thread) == 2
            && model.id_for_index(model.index(0, 0, updated_thread))
                   == "review-comment:c1"
            && model.id_for_index(model.index(1, 0, updated_thread))
                   == "review-comment:c2",
        "one nested reply emits one ordered child-row insertion");
    expect(reset == 0U, "incremental conversation updates never reset model");
    expect(
        model.statistics().ModelResets == 0U,
        "conversation statistics report zero model resets");
}

[[nodiscard]] auto enabled_commands()
    -> std::vector<core::github_command_projection>
{
    return {
        {.Id = core::github_command_id::activate_host_profile, .Enabled = true},
        {.Id = core::github_command_id::open_pull_request, .Enabled = true},
        {.Id = core::github_command_id::refresh_pull_request, .Enabled = true},
        {.Id = core::github_command_id::show_review_threads, .Enabled = true},
    };
}

[[nodiscard]] auto capabilities(
    const core::github_capability_state review_threads =
        core::github_capability_state::supported)
    -> core::github_capabilities
{
    return {
        .ReviewThreads = review_threads,
        .ViewedFileMutations = core::github_capability_state::supported,
        .ResolveThread = core::github_capability_state::unsupported,
        .UnresolveThread = core::github_capability_state::unknown,
    };
}

[[nodiscard]] auto rate_limits()
    -> application::github_rate_limits_projection
{
    return {
        .Core =
            application::github_rate_limit_projection{
                .Resource =
                    application::github_rate_limit_resource_projection::core,
                .Limit = 5'000U,
                .Remaining = 4'999U,
                .ResetUnixSeconds = 2'000'000'000U,
            },
        .Graphql =
            application::github_rate_limit_projection{
                .Resource = application::
                    github_rate_limit_resource_projection::graphql,
                .Limit = 5'000U,
                .Remaining = 4'998U,
                .ResetUnixSeconds = 2'000'000'000U,
            },
    };
}

[[nodiscard]] auto session_projection()
    -> application::github_session_projection
{
    return {
        .Profile = core::host_profile_id{12U},
        .Host = core::git_host_id{22U},
        .Account = core::github_account_id{52U},
        .Login = "octo-reviewer",
        .DisplayName = "Octo Reviewer",
        .Capabilities = capabilities(),
        .CapabilityRevision = core::capability_revision{7U},
        .RateLimits = rate_limits(),
    };
}

[[nodiscard]] auto pull_request_projection(
    const bool threads_available = true)
    -> application::github_pull_request_projection
{
    return {
        .Profile = core::host_profile_id{12U},
        .Host = core::git_host_id{22U},
        .Account = core::github_account_id{52U},
        .Snapshot = core::provider_snapshot_id{91U},
        .Repository = core::repository_id{71U},
        .PullRequest = core::pull_request_id{81U},
        .Owner = "openai",
        .RepositoryName = "better-pull-review",
        .Number = core::pull_request_number{27U},
        .Title = "<b>& literal pull request title",
        .Body = "<script>literal body & review</script>",
        .State = application::github_pull_request_state_projection::open,
        .Draft = true,
        .AuthorLogin = "octo-author",
        .BaseRef = "main",
        .HeadRef = "github-presentation",
        .BaseOid = "1111111111111111111111111111111111111111",
        .HeadOid = "2222222222222222222222222222222222222222",
        .FileCount = 2U,
        .CommitCount = 3U,
        .ReviewCount = threads_available ? 1U : 0U,
        .ReviewThreadCount = threads_available ? 1U : 0U,
        .IssueCommentCount = 1U,
        .ReviewThreadsAvailable = threads_available,
        .Capabilities =
            capabilities(
                threads_available
                    ? core::github_capability_state::supported
                    : core::github_capability_state::unknown),
        .CapabilityRevision = core::capability_revision{7U},
        .RateLimits = rate_limits(),
        .Conversations =
            threads_available
            ? initial_conversations()
            : std::vector<application::github_conversation_projection>{
                  conversation(
                      "issue-comment:i1",
                      application::github_conversation_kind::issue_comment,
                      "<b>& canary"),
              },
    };
}

auto test_intent_translation_is_narrow_and_account_is_output_only() -> void
{
    std::vector<application::github_intent> intents;
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [&intents](application::github_intent intent) {
            intents.push_back(std::move(intent));
        },
    };

    application::github_presentation_update command_update{
        .Transition =
            application::github_transition_projection{
                .Commands = enabled_commands(),
            },
    };
    window.publish(command_update);

    expect(window.profile_selector()->count() == 2, "two profiles are visible");
    window.profile_selector()->setCurrentIndex(1);
    expect(
        intents.empty(),
        "programmatic profile synchronization emits no authority intent");
    window.connect_action()->trigger();
    expect(intents.size() == 1U, "Connect emits exactly one semantic intent");
    const auto* const activate =
        std::get_if<core::activate_host_profile_intent>(&intents.back());
    expect(
        activate != nullptr
            && activate->Profile == core::host_profile_id{12U},
        "Connect emits only the selected opaque profile identity");

    window.owner_editor()->setText(QStringLiteral("better-pull-review"));
    window.repository_editor()->setText(QStringLiteral("desktop"));
    window.pull_request_editor()->setValue(73);
    window.open_action()->trigger();
    expect(intents.size() == 2U, "Open emits exactly one semantic intent");
    const auto* const open =
        std::get_if<core::open_pull_request_intent>(&intents.back());
    expect(
        open != nullptr
            && open->Repository
                   == core::repository_locator{
                       .Owner = "better-pull-review",
                       .Name = "desktop",
                   }
            && open->Number == core::pull_request_number{73U},
        "Open preserves owner, repository, and pull-request number");

    window.refresh_action()->trigger();
    expect(
        intents.size() == 3U
            && std::holds_alternative<core::refresh_pull_request_intent>(
                intents.back()),
        "Refresh emits only refresh_pull_request_intent");

    window.show_review_threads_action()->trigger();
    expect(
        intents.size() == 3U,
        "Show Review Threads remains a local presentation action");

    expect(
        !window.connect_action()->shortcut().isEmpty()
            && !window.open_action()->shortcut().isEmpty()
            && !window.refresh_action()->shortcut().isEmpty(),
        "primary actions expose keyboard shortcuts");
    expect(
        window.active_account_text() == "Not authenticated",
        "account identity is initially read-only authenticated output");

    auto* const accessible =
        QAccessible::queryAccessibleInterface(window.profile_selector());
    expect(
        accessible != nullptr
            && !accessible->text(QAccessible::Name).isEmpty()
            && accessible->state().focusable,
        "profile selector exposes an accessible name and focusable state");
}

auto publish_session_ready(qt::github_window& window) -> void
{
    const auto session = session_projection();
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands = enabled_commands(),
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::session_ready,
                    },
                .Identity =
                    core::github_identity_delta{
                        .Profile = session.Profile,
                        .Host = session.Host,
                        .Account = session.Account,
                        .Generation = core::github_identity_generation{2U},
                    },
                .Capabilities =
                    core::github_capability_delta{
                        .Capabilities = session.Capabilities,
                        .Revision = session.CapabilityRevision,
                    },
            },
        .Session = session,
    });
}

auto publish_pull_request(
    qt::github_window& window,
    const bool threads_available = true) -> void
{
    auto pull_request = pull_request_projection(threads_available);
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands = enabled_commands(),
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_ready,
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary =
                            core::github_pull_request_summary{
                                .Locator =
                                    {
                                        .Owner = pull_request.Owner,
                                        .Name =
                                            pull_request.RepositoryName,
                                    },
                                .Number = pull_request.Number,
                                .Repository = pull_request.Repository,
                                .PullRequest = pull_request.PullRequest,
                                .Snapshot = pull_request.Snapshot,
                            },
                    },
            },
        .PullRequest = std::move(pull_request),
    });
}

[[nodiscard]] auto required_label(
    qt::github_window& window,
    const char* const object_name) -> QLabel*
{
    return required_child<QLabel>(window, object_name);
}

auto test_presentation_states_disclosures_and_responsive_layout() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    window.resize(640, 720);
    window.show();
    QApplication::processEvents();

    auto* const banner =
        required_child<QFrame>(window, "githubStatusBanner");
    auto* const view_state =
        required_child<QStackedWidget>(window, "githubViewState");
    auto* const empty_state =
        required_child<QWidget>(window, "githubEmptyState");
    auto* const empty_title =
        required_label(window, "githubEmptyStateTitle");
    auto* const empty_body =
        required_label(window, "githubEmptyStateBody");
    auto* const connection_button =
        required_child<QToolButton>(
            window, "githubConnectionDetailsButton");
    auto* const connection_details =
        required_child<QWidget>(window, "githubConnectionDetails");
    auto* const technical_button =
        required_child<QToolButton>(
            window, "githubTechnicalDetailsButton");
    auto* const technical_details =
        required_child<QWidget>(window, "githubTechnicalDetails");
    auto* const conversation_state =
        required_child<QStackedWidget>(window, "githubConversationState");
    auto* const conversation_empty =
        required_child<QWidget>(window, "githubConversationEmptyState");
    auto* const splitter =
        required_child<QSplitter>(window, "githubContentSplitter");
    auto* const connect_button =
        required_child<QToolButton>(window, "connectGithubButton");
    auto* const refresh_button =
        required_child<QToolButton>(window, "refreshPullRequestButton");

    expect(
        banner->isVisible()
            && view_state->currentWidget() == empty_state
            && presentation_state(window) == QStringLiteral("disconnected")
            && empty_title->text() == QStringLiteral("Connect to GitHub")
            && !empty_body->text().isEmpty(),
        "initial presentation is a visible disconnected empty state");
    capture_layout_state(window, QStringLiteral("disconnected"));
    expect(
        splitter->orientation() == Qt::Vertical,
        "the content splitter is vertical at the 640-pixel minimum width");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::establishing_session,
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("connecting")
            && view_state->currentWidget() == empty_state
            && empty_title->text().contains(QStringLiteral("Connecting"))
            && empty_body->text().contains(
                QStringLiteral("authenticated session")),
        "connecting has explicit activity and guidance");

    publish_session_ready(window);
    expect(
        presentation_state(window) == QStringLiteral("empty")
            && view_state->currentWidget() == empty_state
            && empty_title->text() == QStringLiteral("Open a pull request")
            && empty_body->text().contains(QStringLiteral("owner")),
        "session-ready without a pull request has an actionable empty state");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("loading")
            && view_state->currentWidget() == empty_state
            && empty_title->text().contains(QStringLiteral("Loading"))
            && empty_body->text().contains(QStringLiteral("snapshot")),
        "a new target load clears content into an explicit loading state");
    capture_layout_state(window, QStringLiteral("loading"));

    publish_pull_request(window);
    QApplication::processEvents();
    expect(
        presentation_state(window) == QStringLiteral("loaded")
            && view_state->currentWidget() != empty_state
            && conversation_state->currentWidget()
                   == window.conversation_view(),
        "accepted pull-request content replaces the empty state");

    expect(
        connection_button->isVisible()
            && technical_button->isVisible()
            && !connection_details->isVisible()
            && !technical_details->isVisible()
            && (connection_button->focusPolicy() & Qt::TabFocus) != 0
            && (technical_button->focusPolicy() & Qt::TabFocus) != 0,
        "both disclosure controls are naturally focusable and initially "
        "collapsed");

    const auto next_visible_tab_stop = [](QWidget* const start) {
        auto* current = start->nextInFocusChain();
        while(current != start)
        {
            if(
                current->isVisible() && current->isEnabled()
                && (current->focusPolicy() & Qt::TabFocus) != 0)
            {
                return current;
            }
            current = current->nextInFocusChain();
        }
        return start;
    };
    expect(
        next_visible_tab_stop(window.profile_selector()) == connect_button
            && next_visible_tab_stop(connect_button) == connection_button
            && next_visible_tab_stop(refresh_button) == technical_button,
        "disclosures follow the natural primary-control tab order");

    connection_button->setFocus(Qt::TabFocusReason);
    connection_button->click();
    QApplication::processEvents();
    expect(
        connection_details->isVisible()
            && connection_button->arrowType() == Qt::DownArrow
            && connection_button->accessibleName().contains(
                QStringLiteral("Hide"))
            && QApplication::focusWidget() == connection_button,
        "connection disclosure opens in place without moving focus");
    connection_button->click();
    QApplication::processEvents();
    expect(
        !connection_details->isVisible()
            && connection_button->arrowType() == Qt::RightArrow
            && QApplication::focusWidget() == connection_button,
        "connection disclosure closes in place without moving focus");

    technical_button->setFocus(Qt::TabFocusReason);
    technical_button->click();
    QApplication::processEvents();
    expect(
        technical_details->isVisible()
            && technical_button->arrowType() == Qt::DownArrow
            && technical_button->accessibleName().contains(
                QStringLiteral("Hide"))
            && QApplication::focusWidget() == technical_button,
        "technical disclosure opens in place without moving focus");
    technical_button->click();
    QApplication::processEvents();
    expect(
        !technical_details->isVisible()
            && technical_button->arrowType() == Qt::RightArrow
            && QApplication::focusWidget() == technical_button,
        "technical disclosure closes in place without moving focus");

    window.resize(960, 720);
    QApplication::processEvents();
    expect(
        splitter->orientation() == Qt::Horizontal,
        "the content splitter is horizontal at 960 pixels");

    const auto retained_title =
        required_label(window, "pullRequestTitle")->text();
    const auto retained_identity =
        QPersistentModelIndex{
            window.conversation_model()->index_for_id("issue-comment:i1")};
    window.conversation_view()->setCurrentIndex(retained_identity);
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("refreshing")
            && required_label(window, "pullRequestTitle")->text()
                   == retained_title
            && retained_identity.isValid()
            && window.conversation_view()->currentIndex()
                   == retained_identity,
        "refresh retains accepted content in a distinct refreshing state");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_failed,
                    },
                .Diagnostics =
                    {
                        {
                            .Code =
                                core::github_reason_code::network_unavailable,
                        },
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("loaded-error")
            && required_label(window, "pullRequestTitle")->text()
                   == retained_title
            && retained_identity.isValid()
            && !window.diagnostic_text().empty(),
        "refresh failure retains accepted content and presents the error");
    capture_layout_state(window, QStringLiteral("retained-refresh-error"));

    auto available_empty = pull_request_projection();
    available_empty.ReviewCount = 0U;
    available_empty.ReviewThreadCount = 0U;
    available_empty.IssueCommentCount = 0U;
    available_empty.Conversations.clear();
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_ready,
                    },
            },
        .PullRequest = std::move(available_empty),
    });
    expect(
        presentation_state(window) == QStringLiteral("loaded")
            && conversation_state->currentWidget() == conversation_empty
            && required_label(window, "reviewThreadAvailability")
                   ->text()
                   .contains(QStringLiteral("available;"))
            && required_label(window, "reviewThreadAvailability")
                   ->text()
                   .contains(QStringLiteral("none")),
        "available-empty conversations are distinct from unavailable data");

    auto unknown_threads = pull_request_projection(false);
    unknown_threads.Conversations.clear();
    window.publish(application::github_presentation_update{
        .PullRequest = std::move(unknown_threads),
    });
    expect(
        presentation_state(window) == QStringLiteral("loaded")
            && conversation_state->currentWidget() == conversation_empty
            && required_label(window, "reviewThreadAvailability")
                   ->text()
                   .contains(QStringLiteral("not been confirmed")),
        "unknown review-thread support remains visibly unknown");

    auto unsupported_threads = pull_request_projection(false);
    unsupported_threads.Conversations.clear();
    unsupported_threads.Capabilities.ReviewThreads =
        core::github_capability_state::unsupported;
    window.publish(application::github_presentation_update{
        .PullRequest = std::move(unsupported_threads),
    });
    expect(
        required_label(window, "reviewThreadAvailability")
            ->text()
            .contains(QStringLiteral("does not support")),
        "unsupported review-thread capability is distinct from unknown");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::establishing_session,
                    },
                .Identity =
                    core::github_identity_delta{
                        .Profile = core::host_profile_id{11U},
                        .Host = std::nullopt,
                        .Account = std::nullopt,
                        .Generation = core::github_identity_generation{3U},
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("connecting")
            && view_state->currentWidget() == empty_state
            && window.conversation_model()->rowCount() == 0
            && window.active_account_text() == "Connecting…",
        "identity switching clears stale content into the connecting state");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::session_failed,
                    },
                .Diagnostics =
                    {
                        {
                            .Code =
                                core::github_reason_code::authentication_failed,
                        },
                    },
            },
    });
    expect(
        presentation_state(window) == QStringLiteral("error")
            && empty_title->text()
                   == QStringLiteral("Connection unavailable")
            && empty_body->text().contains(
                QStringLiteral("Authentication failed"))
            && banner->isVisible(),
        "identity connection failure presents a focused empty error state");
}

auto test_cancellation_states_and_failure_layouts() -> void
{
    qt::github_window session_window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    session_window.resize(960, 640);
    session_window.show();
    QApplication::processEvents();
    auto* const session_stack =
        required_child<QStackedWidget>(session_window, "githubViewState");
    auto* const session_empty =
        required_child<QWidget>(session_window, "githubEmptyState");
    auto* const session_diagnostic =
        required_label(session_window, "githubDiagnostic");
    session_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::establishing_session,
                    },
            },
    });
    session_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::session_cancelled,
                    },
            },
    });
    expect(
        presentation_state(session_window) == QStringLiteral("cancelled")
            && session_stack->currentWidget() == session_empty
            && required_label(session_window, "githubEmptyStateTitle")->text()
                   == QStringLiteral("Connection canceled")
            && required_label(session_window, "githubEmptyStateBody")
                   ->text()
                   .contains(QStringLiteral("Connect"))
            && session_window.status_text()
                   == "The GitHub connection was canceled."
            && session_window.diagnostic_text().empty()
            && session_diagnostic->isHidden()
            && required_label(session_window, "pullRequestIdentity")->text()
                   == QStringLiteral("No pull request is open")
            && !session_window.conversation_view()
                    ->currentIndex()
                    .isValid(),
        "unloaded session cancellation has an empty cancellation page "
        "without a diagnostic or retained pull request");
    capture_layout_state(
        session_window, QStringLiteral("session-cancelled"));

    qt::github_window first_load_window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    first_load_window.resize(960, 640);
    first_load_window.show();
    QApplication::processEvents();
    publish_session_ready(first_load_window);
    first_load_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    first_load_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_cancelled,
                    },
            },
    });
    auto* const first_load_stack =
        required_child<QStackedWidget>(first_load_window, "githubViewState");
    auto* const first_load_empty =
        required_child<QWidget>(first_load_window, "githubEmptyState");
    auto* const first_load_diagnostic =
        required_label(first_load_window, "githubDiagnostic");
    expect(
        presentation_state(first_load_window) == QStringLiteral("cancelled")
            && first_load_stack->currentWidget() == first_load_empty
            && required_label(first_load_window, "githubEmptyStateTitle")
                       ->text()
                   == QStringLiteral("Pull request loading canceled")
            && required_label(first_load_window, "githubEmptyStateBody")
                   ->text()
                   .contains(QStringLiteral("locator is unchanged"))
            && first_load_window.status_text()
                   == "Pull request loading was canceled."
            && first_load_window.diagnostic_text().empty()
            && first_load_diagnostic->isHidden()
            && required_label(first_load_window, "pullRequestIdentity")->text()
                   == QStringLiteral("No pull request is open")
            && required_child<QPlainTextEdit>(
                   first_load_window, "pullRequestBody")
                   ->toPlainText()
                   .isEmpty()
            && first_load_window.conversation_model()->rowCount() == 0
            && !first_load_window.conversation_view()
                    ->currentIndex()
                    .isValid(),
        "first-load pull-request cancellation has no retained projection or "
        "failure diagnostic");
    capture_layout_state(
        first_load_window, QStringLiteral("first-load-cancelled"));

    first_load_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    first_load_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_failed,
                    },
                .Diagnostics =
                    {
                        {
                            .Code =
                                core::github_reason_code::network_unavailable,
                        },
                    },
            },
    });
    expect(
        presentation_state(first_load_window) == QStringLiteral("error")
            && first_load_stack->currentWidget() == first_load_empty
            && required_label(first_load_window, "githubEmptyStateTitle")
                       ->text()
                   == QStringLiteral("Pull request unavailable")
            && required_label(first_load_window, "githubEmptyStateBody")->text()
                   == first_load_diagnostic->text()
            && first_load_window.status_text()
                   == "The pull request could not be loaded."
            && !first_load_window.diagnostic_text().empty()
            && first_load_diagnostic->isVisible()
            && first_load_window.conversation_model()->rowCount() == 0,
        "first-load failure has an empty error page with the visible "
        "diagnostic");
    capture_layout_state(
        first_load_window, QStringLiteral("first-load-error"));

    qt::github_window refresh_window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    publish_session_ready(refresh_window);
    publish_pull_request(refresh_window);
    refresh_window.resize(960, 640);
    refresh_window.show();
    QApplication::processEvents();
    auto* const refresh_stack =
        required_child<QStackedWidget>(refresh_window, "githubViewState");
    auto* const refresh_empty =
        required_child<QWidget>(refresh_window, "githubEmptyState");
    auto* const refresh_diagnostic =
        required_label(refresh_window, "githubDiagnostic");
    const auto retained_title =
        required_label(refresh_window, "pullRequestTitle")->text();
    const auto retained_identity =
        required_label(refresh_window, "pullRequestIdentity")->text();
    const auto retained_body = required_child<QPlainTextEdit>(
        refresh_window, "pullRequestBody")
                                   ->toPlainText();
    const QPersistentModelIndex retained_selection{
        refresh_window.conversation_model()->index_for_id(
            "issue-comment:i1")};
    refresh_window.conversation_view()->setCurrentIndex(retained_selection);
    refresh_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
            },
    });
    refresh_window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_cancelled,
                    },
            },
    });
    expect(
        presentation_state(refresh_window)
                == QStringLiteral("loaded-cancelled")
            && refresh_stack->currentWidget() != refresh_empty
            && required_label(refresh_window, "pullRequestTitle")->text()
                   == retained_title
            && required_label(refresh_window, "pullRequestIdentity")->text()
                   == retained_identity
            && required_child<QPlainTextEdit>(
                   refresh_window, "pullRequestBody")
                       ->toPlainText()
                   == retained_body
            && retained_selection.isValid()
            && refresh_window.conversation_view()->currentIndex()
                   == retained_selection
            && refresh_window.status_text()
                   == "Refresh canceled. Showing the last accepted pull "
                      "request."
            && refresh_window.diagnostic_text().empty()
            && refresh_diagnostic->isHidden(),
        "loaded refresh cancellation retains exact accepted content and "
        "selection without presenting a failure diagnostic");
    capture_layout_state(
        refresh_window, QStringLiteral("retained-refresh-cancelled"));
}

auto test_identity_metadata_commands_and_refresh_semantics() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands =
                    {
                        {
                            .Id = core::github_command_id::
                                activate_host_profile,
                            .Enabled = true,
                        },
                        {
                            .Id = core::github_command_id::open_pull_request,
                            .Enabled = false,
                            .DisabledReason =
                                core::github_reason_code::session_in_progress,
                        },
                        {
                            .Id =
                                core::github_command_id::refresh_pull_request,
                            .Enabled = false,
                            .DisabledReason = core::github_reason_code::
                                no_active_pull_request,
                        },
                        {
                            .Id =
                                core::github_command_id::show_review_threads,
                            .Enabled = false,
                            .DisabledReason = core::github_reason_code::
                                no_active_pull_request,
                        },
                    },
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::establishing_session,
                    },
                .Identity =
                    core::github_identity_delta{
                        .Profile = core::host_profile_id{12U},
                        .Host = std::nullopt,
                        .Account = std::nullopt,
                        .Generation = core::github_identity_generation{1U},
                    },
                .Capabilities =
                    core::github_capability_delta{
                        .Capabilities = {},
                        .Revision = {},
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    expect(
        window.active_host_text().find("ghe.example.invalid")
                != std::string::npos
            && required_label(window, "activeGithubTransport")->text()
                   == QStringLiteral("Native HTTPS"),
        "establishing identity shows exact selected host and transport");
    expect(
        window.active_account_text() == "Connecting…",
        "establishing identity visibly clears the old account");
    expect(
        !window.open_action()->isEnabled()
            && window.open_action()
                   ->property("githubDisabledReason")
                   .toInt()
                   == static_cast<int>(
                       core::github_reason_code::session_in_progress)
            && !window.open_action()->toolTip().isEmpty(),
        "Open command projection owns disabled state and explanation");

    publish_session_ready(window);
    expect(
        window.active_account_text()
            == "Octo Reviewer (@octo-reviewer)",
        "accepted /user output is visible as a read-only account");
    expect(
        window.active_host_text().find("Active host")
                != std::string::npos
            && window.open_action()->isEnabled(),
        "accepted identity marks the configured host active");

    publish_pull_request(window);
    expect(
        required_label(window, "pullRequestTitle")->text()
            == QStringLiteral("<b>& literal pull request title"),
        "remote title is rendered as literal plain text");
    auto* const body =
        window.findChild<QPlainTextEdit*>(QStringLiteral("pullRequestBody"));
    expect(
        body != nullptr && body->isReadOnly()
            && body->toPlainText()
                   == QStringLiteral(
                       "<script>literal body & review</script>"),
        "remote pull-request body is literal and read-only");
    expect(
        required_label(window, "pullRequestIdentity")->text()
                == QStringLiteral("openai/better-pull-review #27")
            && required_label(window, "pullRequestState")->text()
                   == QStringLiteral("Open draft")
            && required_label(window, "pullRequestBranches")
                   ->text()
                   .contains(QStringLiteral("main"))
            && required_label(window, "pullRequestCommitOids")
                   ->text()
                   .contains(
                       QStringLiteral(
                           "2222222222222222222222222222222222222222")),
        "normalized metadata includes identity, state, branches, and exact OIDs");
    expect(
        window.conversation_model()->rowCount() == 3
            && window.conversation_model()
                   ->index_for_id("review-comment:c1")
                   .isValid(),
        "accepted pull request renders review and comment identities");
    expect(
        required_label(window, "reviewThreadAvailability")->text()
            == QStringLiteral("1 review thread available."),
        "available review threads have an explicit visible state");
    expect(
        required_label(window, "githubCapabilities")
                ->text()
                .contains(QStringLiteral("Unsupported"))
            && required_label(window, "githubCapabilities")
                   ->text()
                   .contains(QStringLiteral("Not yet known"))
            && required_label(window, "githubRateLimits")
                   ->text()
                   .contains(QStringLiteral("4999")),
        "capability tri-state and numeric rate observations remain distinct");

    window.show();
    QApplication::processEvents();
    window.show_review_threads_action()->trigger();
    QApplication::processEvents();
    const auto focused_thread =
        window.conversation_model()->index_for_id("review-thread:t1");
    expect(
        focused_thread.isValid()
            && window.conversation_view()->currentIndex() == focused_thread
            && window.conversation_view()->isExpanded(focused_thread)
            && QApplication::focusWidget() == window.conversation_view(),
        "Show Review Threads focuses, selects, and expands a review thread");

    const auto selected_comment =
        window.conversation_model()->index_for_id("issue-comment:i1");
    window.conversation_view()->setCurrentIndex(selected_comment);
    auto reordered_projection = pull_request_projection();
    auto selected_projection = std::ranges::find_if(
        reordered_projection.Conversations,
        [](const auto& value) {
            return value.Id == "issue-comment:i1";
        });
    expect(
        selected_projection != reordered_projection.Conversations.end(),
        "selection fixture contains the issue comment");
    auto moved_projection = std::move(*selected_projection);
    reordered_projection.Conversations.erase(selected_projection);
    reordered_projection.Conversations.insert(
        reordered_projection.Conversations.begin(),
        std::move(moved_projection));
    window.publish(application::github_presentation_update{
        .PullRequest = std::move(reordered_projection),
    });
    expect(
        window.conversation_model()->id_for_index(
            window.conversation_view()->currentIndex())
            == "issue-comment:i1",
        "accepted refresh preserves selection by stable conversation identity");

    auto available_empty = pull_request_projection();
    available_empty.ReviewThreadCount = 0U;
    std::erase_if(
        available_empty.Conversations,
        [](const auto& value) {
            return value.Kind
                       == application::github_conversation_kind::review_thread
                || value.Kind
                       == application::github_conversation_kind::review_comment;
        });
    window.publish(application::github_presentation_update{
        .PullRequest = std::move(available_empty),
    });
    expect(
        required_label(window, "reviewThreadAvailability")
            ->text()
            .contains(QStringLiteral("available;"))
            && required_label(window, "reviewThreadAvailability")
                   ->text()
                   .contains(QStringLiteral("none")),
        "available-empty is distinct from unavailable review threads");

    window.publish(application::github_presentation_update{
        .PullRequest = pull_request_projection(false),
    });
    expect(
        required_label(window, "reviewThreadAvailability")
            ->text()
            .contains(QStringLiteral("not been confirmed"))
            && window.conversation_model()->rowCount() == 1,
        "unknown review-thread availability remains visibly unavailable");
    auto unsupported_threads = pull_request_projection(false);
    unsupported_threads.Capabilities.ReviewThreads =
        core::github_capability_state::unsupported;
    window.publish(application::github_presentation_update{
        .PullRequest = std::move(unsupported_threads),
    });
    expect(
        required_label(window, "reviewThreadAvailability")
            ->text()
            .contains(QStringLiteral("does not support")),
        "unsupported review threads remain distinct from unknown support");
    publish_pull_request(window);

    const auto prior_title =
        required_label(window, "pullRequestTitle")->text();
    const auto prior_rows = window.conversation_model()->rowCount();
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands =
                    {
                        {
                            .Id =
                                core::github_command_id::refresh_pull_request,
                            .Enabled = false,
                            .DisabledReason = core::github_reason_code::
                                pull_request_load_in_progress,
                        },
                    },
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
            },
    });
    expect(
        required_label(window, "pullRequestTitle")->text() == prior_title
            && window.conversation_model()->rowCount() == prior_rows,
        "refresh loading retains the last accepted projection");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_failed,
                    },
                .Diagnostics =
                    {
                        {
                            .Code =
                                core::github_reason_code::network_unavailable,
                        },
                    },
            },
    });
    expect(
        required_label(window, "pullRequestTitle")->text() == prior_title
            && window.conversation_model()->rowCount() == prior_rows
            && !window.diagnostic_text().empty(),
        "refresh failure retains accepted content and shows a fixed error");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::loading_pull_request,
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    expect(
        required_label(window, "pullRequestTitle")->text()
                == QStringLiteral("Not available")
            && window.conversation_model()->rowCount() == 0,
        "new pull-request open clears old metadata and conversations");

    publish_pull_request(window);
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands =
                    {
                        {
                            .Id =
                                core::github_command_id::show_review_threads,
                            .Enabled = false,
                            .DisabledReason =
                                core::github_reason_code::capability_unknown,
                        },
                    },
                .Capabilities =
                    core::github_capability_delta{
                        .Capabilities = capabilities(),
                        .Revision = core::capability_revision{7U},
                    },
            },
    });
    expect(
        !window.show_review_threads_action()->isEnabled(),
        "capability display cannot override command-disabled action state");
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Commands =
                    {
                        {
                            .Id =
                                core::github_command_id::show_review_threads,
                            .Enabled = true,
                        },
                    },
                .Capabilities =
                    core::github_capability_delta{
                        .Capabilities = capabilities(
                            core::github_capability_state::unsupported),
                        .Revision = core::capability_revision{7U},
                    },
            },
    });
    expect(
        window.show_review_threads_action()->isEnabled(),
        "command projection remains the sole action-legality authority");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::establishing_session,
                    },
                .Identity =
                    core::github_identity_delta{
                        .Profile = core::host_profile_id{11U},
                        .Host = std::nullopt,
                        .Account = std::nullopt,
                        .Generation = core::github_identity_generation{3U},
                    },
                .Capabilities =
                    core::github_capability_delta{
                        .Capabilities = {},
                        .Revision = {},
                    },
                .PullRequest =
                    core::pull_request_summary_delta{
                        .Summary = std::nullopt,
                    },
            },
    });
    expect(
        window.active_host_text().find("github.com") != std::string::npos
            && window.active_account_text() == "Connecting…"
            && window.conversation_model()->rowCount() == 0
            && required_label(window, "pullRequestTitle")->text()
                   == QStringLiteral("Not available")
            && required_label(window, "githubRateLimits")
                   ->text()
                   .contains(QStringLiteral("Not reported")),
        "identity switch removes old account, PR, and rate-limit content");

    window.show();
    QApplication::processEvents();
    window.owner_editor()->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    expect(
        QApplication::focusWidget() == window.owner_editor(),
        "owner editor receives focus before asynchronous projection");
    publish_session_ready(window);
    QApplication::processEvents();
    expect(
        QApplication::focusWidget() == window.owner_editor(),
        "asynchronous projections do not steal keyboard focus");
}

struct diagnostic_case
{
    core::github_reason_code Reason;
    std::string_view ExpectedText;
};

[[nodiscard]] auto diagnostic_cases() -> std::vector<diagnostic_case>
{
    using reason = core::github_reason_code;
    return {
        {reason::invalid_profile, "profile is invalid"},
        {reason::session_in_progress, "connection attempt"},
        {reason::session_required, "before opening"},
        {reason::invalid_repository, "repository owner"},
        {reason::invalid_pull_request_number, "pull request number"},
        {reason::pull_request_load_in_progress, "current pull request load"},
        {reason::no_active_pull_request, "before refreshing"},
        {reason::capability_unknown, "not been confirmed"},
        {reason::capability_unsupported, "does not support"},
        {reason::stale_completion, "outdated GitHub result"},
        {reason::completion_identity_mismatch, "different identity"},
        {reason::authentication_failed, "Authentication failed"},
        {reason::permission_denied, "does not have permission"},
        {reason::network_unavailable, "network connection"},
        {reason::api_version_unsupported, "GitHub API version"},
        {reason::capability_unavailable, "capability information"},
        {reason::rate_limited, "rate limit"},
        {reason::malformed_response, "could not safely use"},
        {reason::response_too_large, "safe limits"},
        {reason::vault_unavailable, "credential store"},
        {reason::certificate_rejected, "certificate"},
        {reason::redirect_rejected, "untrusted destination"},
        {reason::timed_out, "timed out"},
        {reason::cli_unavailable, "CLI connection"},
        {reason::cancelled, "was canceled"},
    };
}

auto test_status_diagnostics_and_host_detail_are_complete() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    using status = core::github_status;
    const std::vector statuses{
        status::no_profile,
        status::establishing_session,
        status::session_ready,
        status::session_failed,
        status::session_cancelled,
        status::loading_pull_request,
        status::pull_request_ready,
        status::pull_request_failed,
        status::pull_request_cancelled,
    };
    for(const auto value : statuses)
    {
        window.publish(application::github_presentation_update{
            .Transition =
                application::github_transition_projection{
                    .Status = core::github_status_delta{.Status = value},
                },
        });
        expect(
            !window.status_text().empty(),
            "every GitHub status has presentation-owned wording");
    }

    for(const auto& test_case : diagnostic_cases())
    {
        window.publish(application::github_presentation_update{
            .Transition =
                application::github_transition_projection{
                    .Diagnostics = {{.Code = test_case.Reason}},
                },
        });
        expect(
            window.diagnostic_text().find(test_case.ExpectedText)
                != std::string::npos,
            "every GitHub reason has category-specific presentation wording");
    }

    const application::github_rate_limit_projection cause{
        .Resource =
            application::github_rate_limit_resource_projection::graphql,
        .Limit = 5'000U,
        .Remaining = 100U,
        .ResetUnixSeconds = 2'000'000'000U,
        .SecondaryLimited = true,
    };
    window.publish(application::github_presentation_update{
        .HostDiagnostics =
            {
                {
                    .Reason = core::github_reason_code::rate_limited,
                    .RetryAfterSeconds = 30U,
                    .RateLimits = rate_limits(),
                    .RateLimitCause = cause,
                },
            },
    });
    const auto detailed = window.diagnostic_text();
    expect(
        detailed.find("30 seconds") != std::string::npos
            && detailed.find("temporarily") != std::string::npos,
        "bounded host advisory exposes retry and causal rate facts");
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Diagnostics =
                    {{.Code = core::github_reason_code::rate_limited}},
            },
    });
    expect(
        window.diagnostic_text() == detailed,
        "generic core reason cannot overwrite richer same-turn host detail");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Diagnostics = {{.Code = core::github_reason_code::none}},
            },
    });
    expect(
        window.diagnostic_text().empty(),
        "none diagnostic clears stale presentation wording");
}

auto test_native_inheritance_and_wrapped_conversation_layout() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    publish_session_ready(window);

    const std::string long_body =
        "<b>& literal markup remains text.\n"
        "A deliberately long second paragraph wraps across several visual "
        "lines when the conversation pane narrows, preserving every word, "
        "punctuation mark, and line break without clipping or elision.";
    auto pull_request = pull_request_projection();
    pull_request.Conversations.insert(
        pull_request.Conversations.begin(),
        conversation(
            "issue-comment:long",
            application::github_conversation_kind::issue_comment,
            long_body));
    auto accepted_refresh = pull_request;
    accepted_refresh.Title = "Accepted refreshed title";
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_ready,
                    },
            },
        .PullRequest = std::move(pull_request),
    });
    window.resize(960, 720);
    window.show();
    QApplication::processEvents();

    auto* const banner =
        required_child<QFrame>(window, "githubStatusBanner");
    auto* const view_state =
        required_child<QStackedWidget>(window, "githubViewState");
    auto* const title = required_label(window, "pullRequestTitle");
    auto* const view = window.conversation_view();
    auto* const model = window.conversation_model();
    const auto long_index = model->index_for_id("issue-comment:long");
    const auto short_index = model->index_for_id("review:r1");
    expect(
        long_index.isValid() && short_index.isValid(),
        "wrapped-row fixture exposes long and short stable rows");

    const auto literal = QString::fromUtf8(
        long_body.data(), static_cast<qsizetype>(long_body.size()));
    expect(
        long_index.data(qt::github_conversation_model::body_role).toString()
                == literal
            && long_index.data(Qt::DisplayRole).toString().contains(literal)
            && view->wordWrap()
            && !view->uniformRowHeights()
            && view->textElideMode() == Qt::ElideNone,
        "conversation content remains literal and is configured for "
        "nonuniform wrapping without elision");

    const auto measure_height =
        [view](const QModelIndex& index, const int width) {
            QStyleOptionViewItem option;
            option.initFrom(view);
            option.widget = view;
            option.rect = QRect{0, 0, width, 1};
            return view->itemDelegate()->sizeHint(option, index).height();
        };
    const auto original_wide_height = measure_height(long_index, 620);
    const auto original_short_height = measure_height(short_index, 620);
    expect(
        original_wide_height > original_short_height,
        "long multiline conversation rows receive a larger delegate height");

    const std::array<QWidget*, 4> inherited_widgets{
        banner,
        view_state,
        view,
        window.profile_selector(),
    };
    expect(
        std::ranges::all_of(
            inherited_widgets,
            [&window](const QWidget* const widget) {
                return !widget->testAttribute(Qt::WA_SetPalette)
                    && !widget->testAttribute(Qt::WA_SetStyle)
                    && !widget->testAttribute(Qt::WA_SetFont)
                    && widget->font() == window.font()
                    && widget->styleSheet().isEmpty();
            })
            && !window.testAttribute(Qt::WA_SetPalette)
            && !window.testAttribute(Qt::WA_SetStyle)
            && !window.testAttribute(Qt::WA_SetFont)
            && window.styleSheet().isEmpty(),
        "representative presentation widgets inherit application palette, "
        "style, and body font without overrides");

    std::size_t model_resets = 0U;
    QObject::connect(
        model,
        &QAbstractItemModel::modelReset,
        [&model_resets] { ++model_resets; });
    const QPersistentModelIndex stable_long{long_index};
    const QPersistentModelIndex collapsed_thread{
        model->index_for_id("review-thread:t1")};
    view->setCurrentIndex(long_index);
    view->selectionModel()->select(
        long_index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->setFocus(Qt::OtherFocusReason);
    view->collapse(collapsed_thread);
    QApplication::processEvents();
    expect(
        collapsed_thread.isValid() && !view->isExpanded(collapsed_thread)
            && QApplication::focusWidget() == view
            && view->selectionModel()->isSelected(long_index),
        "conversation fixture starts selected, focused, and explicitly "
        "collapsed");

    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status =
                    core::github_status_delta{
                        .Status = core::github_status::pull_request_ready,
                    },
            },
        .PullRequest = std::move(accepted_refresh),
    });
    QApplication::processEvents();
    expect(
        required_label(window, "pullRequestTitle")->text()
                == QStringLiteral("Accepted refreshed title")
            && stable_long.isValid() && collapsed_thread.isValid()
            && !view->isExpanded(collapsed_thread)
            && view->currentIndex() == stable_long
            && view->selectionModel()->isSelected(stable_long)
            && QApplication::focusWidget() == view
            && model_resets == 0U,
        "accepted refresh preserves explicit collapse, selection, focus, and "
        "incremental model state");

    const auto original_palette = QApplication::palette();
    const auto original_font = QApplication::font();
    const auto original_body_font_height =
        QFontMetrics{window.font()}.height();
    const auto original_title_font_height =
        QFontMetrics{title->font()}.height();

    auto controlled_palette = original_palette;
    const QColor controlled_window{37, 52, 68};
    const QColor controlled_base{25, 34, 43};
    const QColor controlled_text{226, 232, 240};
    const QColor controlled_highlight{103, 73, 184};
    controlled_palette.setColor(QPalette::Window, controlled_window);
    controlled_palette.setColor(QPalette::Base, controlled_base);
    controlled_palette.setColor(QPalette::Text, controlled_text);
    controlled_palette.setColor(
        QPalette::WindowText, controlled_text);
    controlled_palette.setColor(
        QPalette::Highlight, controlled_highlight);
    QApplication::setPalette(controlled_palette);
    QApplication::processEvents();
    expect(
        banner->palette().color(QPalette::Window) == controlled_window
            && view->palette().color(QPalette::Base) == controlled_base
            && title->palette().color(QPalette::WindowText)
                   == controlled_text
            && std::ranges::none_of(
                inherited_widgets,
                [](const QWidget* const widget) {
                    return widget->testAttribute(Qt::WA_SetPalette);
                })
            && !view->isExpanded(collapsed_thread)
            && view->currentIndex() == stable_long
            && view->selectionModel()->isSelected(stable_long)
            && QApplication::focusWidget() == view
            && model_resets == 0U,
        "an application palette change reaches representative widgets "
        "through inheritance without disturbing conversation state");

    auto larger_font = original_font;
    if(larger_font.pointSizeF() > 0.0)
    {
        larger_font.setPointSizeF(larger_font.pointSizeF() + 4.0);
    }
    else
    {
        larger_font.setPixelSize(
            std::max(1, larger_font.pixelSize()) + 6);
    }
    QApplication::setFont(larger_font);
    QApplication::processEvents();
    view->doItemsLayout();
    QApplication::processEvents();

    const auto larger_wide_height = measure_height(long_index, 620);
    const auto larger_narrow_height = measure_height(long_index, 190);
    expect(
        QFontMetrics{window.font()}.height() > original_body_font_height
            && QFontMetrics{title->font()}.height()
                   > original_title_font_height
            && larger_wide_height > original_wide_height
            && larger_narrow_height > larger_wide_height,
        "application font growth and narrower width increase wrapped row "
        "size hints");

    const auto next_index = view->indexBelow(long_index);
    const auto long_rect = view->visualRect(long_index);
    const auto next_rect = view->visualRect(next_index);
    expect(
        next_index.isValid() && long_rect.isValid() && next_rect.isValid()
            && long_rect.height() > next_rect.height()
            && long_rect.bottom() < next_rect.top(),
        "laid-out wrapped rows have nonuniform heights and never overlap");
    expect(
        stable_long.isValid()
            && view->currentIndex() == stable_long
            && view->selectionModel()->isSelected(stable_long)
            && !view->isExpanded(collapsed_thread)
            && QApplication::focusWidget() == view
            && model_resets == 0U,
        "appearance changes preserve row identity, selection, focus, and "
        "incremental model state");

    QApplication::setFont(original_font);
    QApplication::setPalette(original_palette);
    QApplication::processEvents();
    expect(
        QFontMetrics{window.font()}.height() == original_body_font_height
            && banner->palette().color(QPalette::Window)
                   == original_palette.color(QPalette::Window)
            && stable_long.isValid()
            && view->currentIndex() == stable_long
            && !view->isExpanded(collapsed_thread)
            && QApplication::focusWidget() == view
            && model_resets == 0U,
        "application palette and font restoration also preserves UI state");
}

struct accessible_announcement
{
    QString Message;
    QString ObjectName;
    QAccessible::AnnouncementPoliteness Politeness =
        QAccessible::AnnouncementPoliteness::Polite;
};

std::vector<accessible_announcement>* AnnouncementSink = nullptr;

void capture_accessibility_update(QAccessibleEvent* const event)
{
    if(
        AnnouncementSink == nullptr || event == nullptr
        || event->type() != QAccessible::Announcement)
    {
        return;
    }
    const auto* const announcement =
        static_cast<const QAccessibleAnnouncementEvent*>(event);
    AnnouncementSink->push_back(
        {
            .Message = announcement->message(),
            .ObjectName =
                event->object() == nullptr ? QString{}
                                           : event->object()->objectName(),
            .Politeness = announcement->politeness(),
        });
}

class scoped_accessibility_capture final
{
public:
    explicit scoped_accessibility_capture(
        std::vector<accessible_announcement>& announcements)
        : Previous{
              QAccessible::installUpdateHandler(capture_accessibility_update)}
    {
        AnnouncementSink = &announcements;
    }

    ~scoped_accessibility_capture()
    {
        AnnouncementSink = nullptr;
        static_cast<void>(QAccessible::installUpdateHandler(Previous));
    }

    scoped_accessibility_capture(const scoped_accessibility_capture&) = delete;
    auto operator=(const scoped_accessibility_capture&)
        -> scoped_accessibility_capture& = delete;
    scoped_accessibility_capture(scoped_accessibility_capture&&) = delete;
    auto operator=(scoped_accessibility_capture&&)
        -> scoped_accessibility_capture& = delete;

private:
    QAccessible::UpdateHandler Previous = nullptr;
};

auto test_accessible_announcements_are_combined_and_deduplicated() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    auto* const banner =
        required_child<QFrame>(window, "githubStatusBanner");
    window.show();
    QApplication::processEvents();
    std::vector<accessible_announcement> announcements;
    {
        const scoped_accessibility_capture capture{announcements};
        const application::github_presentation_update failure{
            .Transition =
                application::github_transition_projection{
                    .Status =
                        core::github_status_delta{
                            .Status = core::github_status::session_failed,
                        },
                    .Diagnostics =
                        {
                            {
                                .Code = core::github_reason_code::
                                    authentication_failed,
                            },
                        },
                },
        };
        window.publish(failure);
        expect(
            announcements.size() == 1U
                && announcements.front().ObjectName
                       == QStringLiteral("githubStatusBanner")
                && announcements.front().Message.contains(
                    QStringLiteral("connection failed"))
                && announcements.front().Message.contains(
                    QStringLiteral("Authentication failed"))
                && announcements.front().Message
                       == banner->accessibleDescription()
                && announcements.front().Politeness
                       == QAccessible::AnnouncementPoliteness::Polite,
            "one polite accessibility announcement combines status and "
            "diagnostic context");

        window.publish(failure);
        expect(
            announcements.size() == 1U,
            "an identical presentation update suppresses duplicate "
            "announcements");

        window.publish(application::github_presentation_update{
            .Transition =
                application::github_transition_projection{
                    .Status =
                        core::github_status_delta{
                            .Status = core::github_status::session_failed,
                        },
                    .Diagnostics =
                        {
                            {
                                .Code =
                                    core::github_reason_code::permission_denied,
                            },
                        },
                },
        });
        expect(
            announcements.size() == 2U
                && announcements.back().Message.contains(
                    QStringLiteral("does not have permission"))
                && announcements.back().Message
                       == banner->accessibleDescription(),
            "a changed diagnostic produces one newly combined announcement");
    }
}

auto test_accessibility_minimum_geometry_and_render() -> void
{
    qt::github_window window{
        qt::github_view_config{.Profiles = profiles()},
        [](application::github_intent) {},
    };
    publish_session_ready(window);
    publish_pull_request(window);
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* const profile_label =
        window.findChild<QLabel*>(QStringLiteral("githubProfileLabel"));
    auto* const connect_button =
        window.findChild<QToolButton*>(QStringLiteral("connectGithubButton"));
    auto* const splitter =
        window.findChild<QSplitter*>(QStringLiteral("githubContentSplitter"));
    auto* const presentation_scroll = window.findChild<QScrollArea*>(
        QStringLiteral("githubPresentationScroll"));
    expect(
        profile_label != nullptr
            && profile_label->buddy() == window.profile_selector(),
        "profile label exposes its accessible buddy relationship");
    const auto labels = window.findChildren<QLabel*>();
    const auto has_buddy = [&labels](const QWidget* const widget) {
        return std::ranges::any_of(
            labels,
            [widget](const QLabel* const label) {
                return label->buddy() == widget;
            });
    };
    expect(
        has_buddy(window.owner_editor())
            && has_buddy(window.repository_editor())
            && has_buddy(window.pull_request_editor()),
        "all pull-request locator editors expose label relationships");
    expect(
        connect_button != nullptr && connect_button->isVisible()
            && !connect_button->accessibleName().isEmpty()
            && !connect_button->accessibleDescription().isEmpty(),
        "Connect has visible named keyboard-accessible presentation");
    expect(
        window.owner_editor()->isVisible()
            && window.repository_editor()->isVisible()
            && window.pull_request_editor()->isVisible()
            && splitter != nullptr && splitter->isVisible()
            && presentation_scroll != nullptr
            && presentation_scroll->verticalScrollBar()->maximum() > 0
            && window.conversation_view()->isVisible()
            && window.conversation_view()->width() > 0
            && window.conversation_view()->height() > 0,
        "identity, locator, metadata, and conversations survive minimum size");

    auto* const conversation_accessible =
        QAccessible::queryAccessibleInterface(window.conversation_view());
    expect(
        conversation_accessible != nullptr
            && !conversation_accessible->text(QAccessible::Name).isEmpty()
            && conversation_accessible->state().focusable,
        "conversation tree exposes an accessible focusable interface");

    window.profile_selector()->setFocus(Qt::TabFocusReason);
    QApplication::processEvents();
    expect(
        QApplication::focusWidget() == window.profile_selector()
            && QAccessible::queryAccessibleInterface(
                   window.profile_selector())
                   ->state()
                   .focused,
        "profile selector reports focused accessible state");

    const auto expected_scale_text =
        qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
    std::optional<double> expected_scale;
    if(!expected_scale_text.isEmpty())
    {
        bool parsed = false;
        expected_scale = expected_scale_text.toDouble(&parsed);
        expect(parsed, "expected Qt scale is numeric");
        expect(
            std::abs(window.devicePixelRatioF() - *expected_scale) < 0.02,
            "window reports the requested fractional device-pixel ratio");
    }

    QCursor::setPos(
        window.mapToGlobal(
            QPoint{window.width() - 8, window.height() - 8}));
    window.statusBar()->clearMessage();
    QApplication::processEvents();
    const QPixmap minimum_render = window.grab();
    expect(
        !minimum_render.isNull(),
        "minimum-size GitHub state renders to a nonempty raster");
    if(expected_scale.has_value())
    {
        expect(
            minimum_render.width()
                    == static_cast<int>(std::lround(
                        static_cast<double>(window.width())
                        * *expected_scale))
                && minimum_render.height()
                       == static_cast<int>(std::lround(
                           static_cast<double>(window.height())
                           * *expected_scale)),
            "minimum raster dimensions match logical size and scale");
    }
    const auto minimum_render_path =
        qEnvironmentVariable("BPR_QT_MIN_RENDER_PATH");
    if(!minimum_render_path.isEmpty())
    {
        expect(
            minimum_render.save(minimum_render_path, "PNG"),
            "minimum-size GitHub render is saved under the build");
    }
    presentation_scroll->verticalScrollBar()->setValue(
        presentation_scroll->verticalScrollBar()->maximum());
    window.statusBar()->clearMessage();
    QApplication::processEvents();
    const auto minimum_lower_render_path =
        qEnvironmentVariable("BPR_QT_MIN_LOWER_RENDER_PATH");
    if(!minimum_lower_render_path.isEmpty())
    {
        expect(
            window.grab().save(minimum_lower_render_path, "PNG"),
            "minimum-size lower GitHub render is saved under the build");
    }

    presentation_scroll->verticalScrollBar()->setValue(0);
    window.resize(960, 640);
    QCursor::setPos(
        window.mapToGlobal(
            QPoint{window.width() - 8, window.height() - 8}));
    window.statusBar()->clearMessage();
    QApplication::processEvents();
    const QPixmap rendered = window.grab();
    expect(
        !rendered.isNull() && rendered.width() > 0 && rendered.height() > 0,
        "loaded GitHub state renders to a nonempty raster");
    if(expected_scale.has_value())
    {
        expect(
            rendered.width()
                    == static_cast<int>(std::lround(
                        static_cast<double>(window.width())
                        * *expected_scale))
                && rendered.height()
                       == static_cast<int>(std::lround(
                           static_cast<double>(window.height())
                           * *expected_scale)),
            "rendered raster dimensions match logical size and scale");
    }
    const auto render_path = qEnvironmentVariable("BPR_QT_RENDER_PATH");
    if(!render_path.isEmpty())
    {
        expect(
            rendered.save(render_path, "PNG"),
            "synthetic GitHub presentation render is saved under the build");
    }
    expect(
        window.statistics().FullResets == 0U
            && window.conversation_model()->statistics().ModelResets == 0U,
        "rendered projection path performs no full model or window reset");
}
}

auto main(int argc, char** argv) -> int
{
    QApplication application{argc, argv};
    application.setApplicationName(
        QStringLiteral("BetterPullReviewGitHubQtTests"));

    test_conversation_model_is_stable_and_incremental();
    test_intent_translation_is_narrow_and_account_is_output_only();
    test_presentation_states_disclosures_and_responsive_layout();
    test_cancellation_states_and_failure_layouts();
    test_identity_metadata_commands_and_refresh_semantics();
    test_status_diagnostics_and_host_detail_are_complete();
    test_native_inheritance_and_wrapped_conversation_layout();
    test_accessible_announcements_are_combined_and_deduplicated();
    test_accessibility_minimum_geometry_and_render();

    std::cout << "All GitHub Qt presentation tests passed\n";
    return EXIT_SUCCESS;
}
