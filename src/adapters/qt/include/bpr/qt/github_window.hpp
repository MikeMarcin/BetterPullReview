#pragma once

#include "bpr/application/github_projection.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include <QMainWindow>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class QAction;
class QComboBox;
class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QResizeEvent;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeView;
class QWidget;

namespace bpr::qt
{
struct github_view_config
{
    std::vector<application::github_profile_option> Profiles;
};

struct github_window_statistics
{
    std::size_t CommandUpdates = 0;
    std::size_t StatusUpdates = 0;
    std::size_t IdentityUpdates = 0;
    std::size_t CapabilityUpdates = 0;
    std::size_t SessionUpdates = 0;
    std::size_t PullRequestUpdates = 0;
    std::size_t DiagnosticUpdates = 0;
    std::size_t FullResets = 0;
};

class github_window final
    : public QMainWindow
    , public application::github_projection_port
{
public:
    github_window(
        github_view_config config,
        application::github_intent_dispatch dispatch,
        QWidget* parent = nullptr);

    void publish(
        const application::github_presentation_update& update) override;

    [[nodiscard]] auto connect_action() const noexcept -> QAction*;
    [[nodiscard]] auto open_action() const noexcept -> QAction*;
    [[nodiscard]] auto refresh_action() const noexcept -> QAction*;
    [[nodiscard]] auto show_review_threads_action() const noexcept -> QAction*;
    [[nodiscard]] auto profile_selector() const noexcept -> QComboBox*;
    [[nodiscard]] auto owner_editor() const noexcept -> QLineEdit*;
    [[nodiscard]] auto repository_editor() const noexcept -> QLineEdit*;
    [[nodiscard]] auto pull_request_editor() const noexcept -> QSpinBox*;
    [[nodiscard]] auto conversation_view() const noexcept -> QTreeView*;
    [[nodiscard]] auto conversation_model() const noexcept
        -> github_conversation_model*;
    [[nodiscard]] auto status_text() const -> std::string;
    [[nodiscard]] auto diagnostic_text() const -> std::string;
    [[nodiscard]] auto active_host_text() const -> std::string;
    [[nodiscard]] auto active_account_text() const -> std::string;
    [[nodiscard]] auto statistics() const noexcept -> github_window_statistics;

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void apply_transition(
        const application::github_transition_projection& transition);
    void apply_session(
        const application::github_session_projection& session);
    void apply_pull_request(
        const application::github_pull_request_projection& pull_request);
    void apply_host_diagnostic(
        const application::github_host_diagnostic_projection& diagnostic);
    void apply_command(const core::github_command_projection& command);
    void apply_identity(const core::github_identity_delta& identity);
    void apply_capabilities(const core::github_capabilities& capabilities);
    void apply_status(core::github_status status);
    void apply_diagnostic(
        core::github_reason_code reason,
        std::optional<std::uint32_t> retry_after = std::nullopt,
        const std::optional<
            application::github_rate_limit_projection>& rate_limit =
            std::nullopt,
        bool host_detail = false);
    void apply_rate_limits(
        const application::github_rate_limits_projection& limits);
    void clear_pull_request();
    void update_selected_profile();
    void update_connect_enabled();
    void update_presentation_state();
    void update_splitter_orientation();
    void refresh_native_appearance();
    void announce_presentation_update();
    void focus_review_threads();

    [[nodiscard]] auto presentation_summary_text() const -> QString;
    [[nodiscard]] auto selected_profile() const
        -> std::optional<core::host_profile_id>;
    [[nodiscard]] auto profile_option(core::host_profile_id profile) const
        -> const application::github_profile_option*;
    [[nodiscard]] auto profile_index(core::host_profile_id profile) const
        -> int;

    application::github_intent_dispatch Dispatch;
    std::vector<application::github_profile_option> Profiles;

    QAction* ConnectAction = nullptr;
    QAction* OpenAction = nullptr;
    QAction* RefreshAction = nullptr;
    QAction* ShowReviewThreadsAction = nullptr;
    QToolButton* ConnectButton = nullptr;
    QToolButton* OpenButton = nullptr;
    QToolButton* RefreshButton = nullptr;
    QToolButton* ShowReviewThreadsButton = nullptr;
    QToolButton* ConnectionDetailsButton = nullptr;
    QToolButton* TechnicalDetailsButton = nullptr;
    QComboBox* ProfileSelector = nullptr;
    QLineEdit* OwnerEditor = nullptr;
    QLineEdit* RepositoryEditor = nullptr;
    QSpinBox* PullRequestEditor = nullptr;
    QLabel* HostLabel = nullptr;
    QLabel* TransportLabel = nullptr;
    QLabel* AccountLabel = nullptr;
    QLabel* CapabilityLabel = nullptr;
    QLabel* RateLimitLabel = nullptr;
    QLabel* PullRequestLabel = nullptr;
    QLabel* TitleLabel = nullptr;
    QLabel* StateLabel = nullptr;
    QLabel* AuthorLabel = nullptr;
    QLabel* BranchLabel = nullptr;
    QLabel* OidLabel = nullptr;
    QLabel* CountLabel = nullptr;
    QPlainTextEdit* BodyEditor = nullptr;
    QLabel* ConversationAvailabilityLabel = nullptr;
    QLabel* DiagnosticLabel = nullptr;
    QLabel* StatusLabel = nullptr;
    QLabel* EmptyStateTitleLabel = nullptr;
    QLabel* EmptyStateBodyLabel = nullptr;
    QFrame* StatusBanner = nullptr;
    QWidget* ConnectionDetails = nullptr;
    QWidget* TechnicalDetails = nullptr;
    QWidget* LocatorActions = nullptr;
    QGridLayout* LocatorLayout = nullptr;
    QStackedWidget* PullRequestStack = nullptr;
    QStackedWidget* ConversationStack = nullptr;
    QSplitter* ContentSplitter = nullptr;
    QTreeView* ConversationView = nullptr;
    github_conversation_model* ConversationModel = nullptr;

    std::optional<core::host_profile_id> ActiveProfile;
    std::optional<core::github_account_id> SessionAccount;
    std::optional<core::github_reason_code> LastDiagnosticReason;
    bool LastDiagnosticHasHostDetail = false;
    bool ConnectCommandEnabled = true;
    core::github_reason_code ConnectDisabledReason =
        core::github_reason_code::none;
    core::github_status CurrentStatus = core::github_status::no_profile;
    QString LastAnnouncement;
    bool HasPullRequest = false;
    bool ExpandConversationsOnNextProjection = true;
    github_window_statistics Statistics;
};
}
