#pragma once

#include "bpr/application/epoch_projection.hpp"
#include "bpr/qt/epoch_history_model.hpp"

#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>

class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QToolButton;

namespace bpr::qt
{
struct epoch_comparison_control_statistics
{
    std::size_t PublishedUpdates = 0U;
    std::size_t Announcements = 0U;
    std::size_t AppearanceUpdates = 0U;
};

class epoch_comparison_control final
    : public QWidget
    , public application::epoch_projection_port
{
public:
    explicit epoch_comparison_control(
        application::epoch_intent_dispatch dispatch = {},
        QWidget* parent = nullptr);

    void publish(
        const application::epoch_presentation_update& update) override;
    void set_dispatch(application::epoch_intent_dispatch dispatch);

    [[nodiscard]] auto history_model() const noexcept -> epoch_history_model*;
    [[nodiscard]] auto left_selector() const noexcept -> QComboBox*;
    [[nodiscard]] auto right_selector() const noexcept -> QComboBox*;
    [[nodiscard]] auto left_oid_editor() const noexcept -> QLineEdit*;
    [[nodiscard]] auto right_oid_editor() const noexcept -> QLineEdit*;
    [[nodiscard]] auto whole_pr_action() const noexcept -> QAction*;
    [[nodiscard]] auto previous_upload_action() const noexcept -> QAction*;
    [[nodiscard]] auto last_reviewed_action() const noexcept -> QAction*;
    [[nodiscard]] auto load_history_action() const noexcept -> QAction*;
    [[nodiscard]] auto return_to_newest_action() const noexcept -> QAction*;
    [[nodiscard]] auto refresh_action() const noexcept -> QAction*;
    [[nodiscard]] auto cancel_action() const noexcept -> QAction*;
    [[nodiscard]] auto status_text() const -> QString;
    [[nodiscard]] auto comparison_text() const -> QString;
    [[nodiscard]] auto statistics() const noexcept
        -> epoch_comparison_control_statistics;

protected:
    void changeEvent(QEvent* event) override;

private:
    void dispatch_alias(core::named_comparison alias);
    void dispatch_anchor(core::comparison_side side, int row);
    void dispatch_older_history_page();
    void dispatch_newest_history_page();
    void apply_catalog(
        const application::epoch_catalog_projection& projection);
    void apply_transition(
        const application::epoch_transition_projection& projection);
    void update_controls();
    void update_selection_controls();
    void update_status_text();
    void update_action_state();
    void refresh_native_appearance();
    void announce_update();

    [[nodiscard]] auto summary_text() const -> QString;
    [[nodiscard]] auto command(core::epoch_command_id id) const
        -> std::optional<core::epoch_command_projection>;

    application::epoch_intent_dispatch Dispatch;
    epoch_history_model* HistoryModel = nullptr;
    QComboBox* LeftSelector = nullptr;
    QComboBox* RightSelector = nullptr;
    QLineEdit* LeftOid = nullptr;
    QLineEdit* RightOid = nullptr;
    QLabel* StatusLabel = nullptr;
    QLabel* ComparisonLabel = nullptr;
    QAction* WholePrAction = nullptr;
    QAction* PreviousUploadAction = nullptr;
    QAction* LastReviewedAction = nullptr;
    QAction* LoadHistoryAction = nullptr;
    QAction* ReturnNewestAction = nullptr;
    QAction* RefreshAction = nullptr;
    QAction* CancelAction = nullptr;
    QToolButton* LoadHistoryButton = nullptr;
    QToolButton* ReturnNewestButton = nullptr;
    QToolButton* RefreshButton = nullptr;
    QToolButton* CancelButton = nullptr;

    std::optional<core::comparison_selection> Selection;
    std::optional<application::epoch_comparison_projection> Comparison;
    std::optional<core::epoch_history_cursor> Continuation;
    std::optional<core::epoch_history_navigation> HistoryNavigation;
    std::optional<core::epoch_history_page_phase> HistoryPhase;
    core::epoch_reason_code HistoryReason = core::epoch_reason_code::none;
    core::epoch_reason_code DiagnosticReason = core::epoch_reason_code::none;
    core::epoch_status Status = core::epoch_status::inactive;
    std::array<core::epoch_command_projection, 3> Commands{};
    bool HasCommands = false;
    bool ApplyingProjection = false;
    bool HistoryWindowStale = false;
    QString LastAnnouncement;
    epoch_comparison_control_statistics Statistics;
};
}
