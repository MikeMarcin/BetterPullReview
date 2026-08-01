#include "bpr/qt/epoch_comparison_control.hpp"

#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <variant>

namespace bpr::qt
{
namespace
{
[[nodiscard]] auto oid_text(const core::git_commit_oid& oid) -> QString
{
    const auto canonical = core::canonical_hex(oid);
    const auto view = canonical.view();
    return QString::fromLatin1(
        view.data(), static_cast<qsizetype>(view.size()));
}

[[nodiscard]] auto reason_text(const core::epoch_reason_code reason) -> QString
{
    switch(reason)
    {
    case core::epoch_reason_code::none:
        return {};
    case core::epoch_reason_code::invalid_identity:
        return QStringLiteral("The review-history identity is invalid.");
    case core::epoch_reason_code::invalid_oid:
        return QStringLiteral("A commit identity is invalid.");
    case core::epoch_reason_code::invalid_revision:
        return QStringLiteral("The pull-request revision is invalid.");
    case core::epoch_reason_code::invalid_catalog:
        return QStringLiteral("The review-history catalog is invalid.");
    case core::epoch_reason_code::invalid_history_cursor:
        return QStringLiteral("The history-page position is invalid.");
    case core::epoch_reason_code::invalid_maintenance_target:
        return QStringLiteral("The maintenance target is invalid.");
    case core::epoch_reason_code::invalid_observation:
        return QStringLiteral("The review-history observation is invalid.");
    case core::epoch_reason_code::invalid_selection:
        return QStringLiteral("That comparison cannot be selected.");
    case core::epoch_reason_code::no_active_authority:
        return QStringLiteral("Open an authenticated pull request first.");
    case core::epoch_reason_code::no_catalog:
        return QStringLiteral("Review history is not available yet.");
    case core::epoch_reason_code::no_resolved_comparison:
        return QStringLiteral("No exact comparison is resolved.");
    case core::epoch_reason_code::catalog_revision_mismatch:
        return QStringLiteral("Review history changed; retry the selection.");
    case core::epoch_reason_code::stale_completion:
        return QStringLiteral("An obsolete review-history result was ignored.");
    case core::epoch_reason_code::duplicate_history_page:
        return QStringLiteral("That history page is already loading.");
    case core::epoch_reason_code::duplicate_maintenance:
        return QStringLiteral("Review-history maintenance is already running.");
    case core::epoch_reason_code::completion_identity_mismatch:
        return QStringLiteral("A mismatched review-history result was ignored.");
    case core::epoch_reason_code::request_counter_exhausted:
    case core::epoch_reason_code::epoch_generation_exhausted:
    case core::epoch_reason_code::comparison_generation_exhausted:
    case core::epoch_reason_code::epoch_ordinal_exhausted:
    case core::epoch_reason_code::catalog_revision_exhausted:
    case core::epoch_reason_code::comparison_revision_exhausted:
        return QStringLiteral("A review-history identity limit was reached.");
    case core::epoch_reason_code::unavailable:
        return QStringLiteral("The exact comparison is unavailable.");
    case core::epoch_reason_code::catalog_conflict:
        return QStringLiteral("Review history changed concurrently.");
    case core::epoch_reason_code::quota_exceeded:
        return QStringLiteral("The local review-history limit was reached.");
    case core::epoch_reason_code::repair_required:
        return QStringLiteral("The local review history requires repair.");
    case core::epoch_reason_code::cancelled:
        return QStringLiteral("Review-history work was canceled.");
    case core::epoch_reason_code::cancellation_after_point_of_no_return:
        return QStringLiteral(
            "Maintenance is finishing safely and cannot be canceled.");
    }
    return QStringLiteral("Review history is unavailable.");
}

[[nodiscard]] auto epoch_status_text(const core::epoch_status status) -> QString
{
    switch(status)
    {
    case core::epoch_status::inactive:
        return QStringLiteral("Review history is inactive.");
    case core::epoch_status::loading_catalog:
        return QStringLiteral("Loading review history…");
    case core::epoch_status::preparing:
        return QStringLiteral("Refreshing retained revisions…");
    case core::epoch_status::committing:
        return QStringLiteral("Saving the observed revision…");
    case core::epoch_status::resolving:
        return QStringLiteral("Resolving the exact comparison…");
    case core::epoch_status::maintaining:
        return QStringLiteral("Updating retained review history…");
    case core::epoch_status::ready:
        return QStringLiteral("Review history is ready.");
    case core::epoch_status::unavailable:
        return QStringLiteral("Review history is unavailable.");
    case core::epoch_status::failed:
        return QStringLiteral("Review history could not be updated.");
    case core::epoch_status::cancelled:
        return QStringLiteral("Review-history refresh was canceled.");
    }
    return QStringLiteral("Review history is inactive.");
}

[[nodiscard]] auto alias_text(const core::named_comparison alias) -> QString
{
    switch(alias)
    {
    case core::named_comparison::whole_pr:
        return QStringLiteral("Whole PR");
    case core::named_comparison::previous_upload_to_latest:
        return QStringLiteral("Previous upload → latest");
    case core::named_comparison::my_last_reviewed_to_latest:
        return QStringLiteral("My last reviewed → latest");
    }
    return QStringLiteral("Comparison");
}

[[nodiscard]] auto availability_text(
    const core::anchor_availability availability) -> QString
{
    switch(availability)
    {
    case core::anchor_availability::available:
        return QStringLiteral("available");
    case core::anchor_availability::unavailable:
        return QStringLiteral("unavailable");
    case core::anchor_availability::pruned:
        return QStringLiteral("pruned");
    case core::anchor_availability::cache_deleted:
        return QStringLiteral("cache deleted");
    case core::anchor_availability::corrupt:
        return QStringLiteral("corrupt");
    case core::anchor_availability::never_observed:
        return QStringLiteral("never observed");
    }
    return QStringLiteral("unavailable");
}

void configure_action(
    QAction* const action,
    const QString& object_name,
    const QKeySequence& shortcut)
{
    action->setObjectName(object_name);
    action->setShortcut(shortcut);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

void configure_oid_editor(
    QLineEdit* const editor,
    const QString& object_name,
    const QString& accessible_name)
{
    editor->setObjectName(object_name);
    editor->setAccessibleName(accessible_name);
    editor->setAccessibleDescription(
        QStringLiteral("Complete lowercase commit OID; read-only and copyable"));
    editor->setReadOnly(true);
    editor->setClearButtonEnabled(false);
    editor->setPlaceholderText(QStringLiteral("No exact commit selected"));
}
}

epoch_comparison_control::epoch_comparison_control(
    application::epoch_intent_dispatch dispatch,
    QWidget* const parent)
    : QWidget{parent}
    , Dispatch{std::move(dispatch)}
{
    setObjectName(QStringLiteral("epochComparisonControl"));
    setAccessibleName(QStringLiteral("Review epoch comparison"));
    setAccessibleDescription(QStringLiteral(
        "Choose independent exact left and right retained revision anchors"));

    HistoryModel = new epoch_history_model{this};

    WholePrAction = new QAction{QStringLiteral("&Whole PR"), this};
    configure_action(WholePrAction,
        QStringLiteral("wholePrComparisonAction"),
        QKeySequence{QStringLiteral("Alt+1")});
    WholePrAction->setCheckable(true);
    PreviousUploadAction =
        new QAction{QStringLiteral("Previous &upload → latest"), this};
    configure_action(PreviousUploadAction,
        QStringLiteral("previousUploadComparisonAction"),
        QKeySequence{QStringLiteral("Alt+2")});
    PreviousUploadAction->setCheckable(true);
    LastReviewedAction =
        new QAction{QStringLiteral("My last &reviewed → latest"), this};
    configure_action(LastReviewedAction,
        QStringLiteral("lastReviewedComparisonAction"),
        QKeySequence{QStringLiteral("Alt+3")});
    LastReviewedAction->setCheckable(true);
    auto* const aliases = new QActionGroup{this};
    aliases->setExclusive(true);
    aliases->addAction(WholePrAction);
    aliases->addAction(PreviousUploadAction);
    aliases->addAction(LastReviewedAction);
    addActions(aliases->actions());

    LoadHistoryAction = new QAction{QStringLiteral("Load &earlier epochs"), this};
    configure_action(LoadHistoryAction,
        QStringLiteral("loadEpochHistoryAction"),
        QKeySequence{QStringLiteral("Ctrl+Alt+Down")});
    ReturnNewestAction =
        new QAction{QStringLiteral("Return to &newest epochs"), this};
    configure_action(ReturnNewestAction,
        QStringLiteral("returnToNewestEpochsAction"),
        QKeySequence{QStringLiteral("Ctrl+Alt+Up")});
    RefreshAction = new QAction{QStringLiteral("Refresh review &history"), this};
    configure_action(RefreshAction,
        QStringLiteral("refreshEpochsAction"),
        QKeySequence{QStringLiteral("Ctrl+Alt+R")});
    CancelAction = new QAction{QStringLiteral("&Cancel history refresh"), this};
    configure_action(CancelAction,
        QStringLiteral("cancelEpochRefreshAction"),
        QKeySequence{QStringLiteral("Escape")});
    addAction(LoadHistoryAction);
    addAction(ReturnNewestAction);
    addAction(RefreshAction);
    addAction(CancelAction);

    auto* const layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));

    auto* const alias_row = new QHBoxLayout;
    alias_row->setContentsMargins(0, 0, 0, 0);
    const auto make_button = [this, alias_row](QAction* const action)
    {
        auto* const button = new QToolButton{this};
        button->setDefaultAction(action);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        alias_row->addWidget(button);
    };
    make_button(WholePrAction);
    make_button(PreviousUploadAction);
    make_button(LastReviewedAction);
    alias_row->addStretch(1);
    layout->addLayout(alias_row);

    auto* const selector_grid = new QGridLayout;
    selector_grid->setContentsMargins(0, 0, 0, 0);
    selector_grid->setColumnStretch(1, 1);
    selector_grid->setColumnStretch(3, 1);
    LeftSelector = new QComboBox{this};
    LeftSelector->setObjectName(QStringLiteral("leftEpochSelector"));
    LeftSelector->setAccessibleName(QStringLiteral("Left review epoch"));
    LeftSelector->setAccessibleDescription(QStringLiteral(
        "Select the exact base or head anchor for the left side"));
    LeftSelector->setModel(HistoryModel);
    LeftSelector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    LeftSelector->setMinimumContentsLength(24);
    RightSelector = new QComboBox{this};
    RightSelector->setObjectName(QStringLiteral("rightEpochSelector"));
    RightSelector->setAccessibleName(QStringLiteral("Right review epoch"));
    RightSelector->setAccessibleDescription(QStringLiteral(
        "Select the exact base or head anchor for the right side"));
    RightSelector->setModel(HistoryModel);
    RightSelector->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    RightSelector->setMinimumContentsLength(24);
    auto* const left_label = new QLabel{QStringLiteral("&Left:"), this};
    left_label->setObjectName(QStringLiteral("leftEpochSelectorLabel"));
    left_label->setBuddy(LeftSelector);
    auto* const right_label = new QLabel{QStringLiteral("&Right:"), this};
    right_label->setObjectName(QStringLiteral("rightEpochSelectorLabel"));
    right_label->setBuddy(RightSelector);
    selector_grid->addWidget(left_label, 0, 0);
    selector_grid->addWidget(LeftSelector, 0, 1);
    selector_grid->addWidget(right_label, 0, 2);
    selector_grid->addWidget(RightSelector, 0, 3);

    LeftOid = new QLineEdit{this};
    configure_oid_editor(LeftOid,
        QStringLiteral("leftEpochOid"),
        QStringLiteral("Exact left commit OID"));
    RightOid = new QLineEdit{this};
    configure_oid_editor(RightOid,
        QStringLiteral("rightEpochOid"),
        QStringLiteral("Exact right commit OID"));
    auto* const left_oid_label = new QLabel{QStringLiteral("Left OID:"), this};
    left_oid_label->setBuddy(LeftOid);
    auto* const right_oid_label =
        new QLabel{QStringLiteral("Right OID:"), this};
    right_oid_label->setBuddy(RightOid);
    selector_grid->addWidget(left_oid_label, 1, 0);
    selector_grid->addWidget(LeftOid, 1, 1);
    selector_grid->addWidget(right_oid_label, 1, 2);
    selector_grid->addWidget(RightOid, 1, 3);
    layout->addLayout(selector_grid);

    auto* const footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    auto* const status_column = new QVBoxLayout;
    StatusLabel = new QLabel{this};
    StatusLabel->setObjectName(QStringLiteral("epochStatus"));
    StatusLabel->setAccessibleName(QStringLiteral("Review-history status"));
    StatusLabel->setWordWrap(true);
    StatusLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
        | Qt::TextSelectableByMouse);
    ComparisonLabel = new QLabel{this};
    ComparisonLabel->setObjectName(QStringLiteral("epochComparisonStatus"));
    ComparisonLabel->setAccessibleName(QStringLiteral("Comparison availability"));
    ComparisonLabel->setWordWrap(true);
    ComparisonLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
        | Qt::TextSelectableByMouse);
    status_column->addWidget(StatusLabel);
    status_column->addWidget(ComparisonLabel);
    footer->addLayout(status_column, 1);
    LoadHistoryButton = new QToolButton{this};
    LoadHistoryButton->setObjectName(QStringLiteral("loadEpochHistoryButton"));
    LoadHistoryButton->setDefaultAction(LoadHistoryAction);
    LoadHistoryButton->setFocusPolicy(Qt::StrongFocus);
    ReturnNewestButton = new QToolButton{this};
    ReturnNewestButton->setObjectName(
        QStringLiteral("returnToNewestEpochsButton"));
    ReturnNewestButton->setDefaultAction(ReturnNewestAction);
    ReturnNewestButton->setFocusPolicy(Qt::StrongFocus);
    RefreshButton = new QToolButton{this};
    RefreshButton->setObjectName(QStringLiteral("refreshEpochsButton"));
    RefreshButton->setDefaultAction(RefreshAction);
    RefreshButton->setFocusPolicy(Qt::StrongFocus);
    CancelButton = new QToolButton{this};
    CancelButton->setObjectName(QStringLiteral("cancelEpochRefreshButton"));
    CancelButton->setDefaultAction(CancelAction);
    CancelButton->setFocusPolicy(Qt::StrongFocus);
    footer->addWidget(LoadHistoryButton);
    footer->addWidget(ReturnNewestButton);
    footer->addWidget(RefreshButton);
    footer->addWidget(CancelButton);
    layout->addLayout(footer);

    connect(WholePrAction, &QAction::triggered, this, [this]
    {
        dispatch_alias(core::named_comparison::whole_pr);
    });
    connect(PreviousUploadAction, &QAction::triggered, this, [this]
    {
        dispatch_alias(core::named_comparison::previous_upload_to_latest);
    });
    connect(LastReviewedAction, &QAction::triggered, this, [this]
    {
        dispatch_alias(core::named_comparison::my_last_reviewed_to_latest);
    });
    connect(LeftSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](const int row)
        {
            dispatch_anchor(core::comparison_side::left, row);
        });
    connect(RightSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](const int row)
        {
            dispatch_anchor(core::comparison_side::right, row);
        });
    connect(LoadHistoryAction, &QAction::triggered, this, [this]
    {
        dispatch_older_history_page();
    });
    connect(ReturnNewestAction, &QAction::triggered, this, [this]
    {
        dispatch_newest_history_page();
    });
    connect(RefreshAction, &QAction::triggered, this, [this]
    {
        if(Dispatch)
        {
            Dispatch(core::refresh_epochs_intent{});
        }
    });
    connect(CancelAction, &QAction::triggered, this, [this]
    {
        if(Dispatch)
        {
            Dispatch(core::cancel_epoch_refresh_intent{});
        }
    });

    WholePrAction->setChecked(true);
    update_controls();
    refresh_native_appearance();
}

void epoch_comparison_control::publish(
    const application::epoch_presentation_update& update)
{
    ApplyingProjection = true;
    if(update.Catalog)
    {
        apply_catalog(*update.Catalog);
    }
    if(update.Transition)
    {
        apply_transition(*update.Transition);
    }
    update_controls();
    ApplyingProjection = false;
    ++Statistics.PublishedUpdates;
    announce_update();
}

void epoch_comparison_control::set_dispatch(
    application::epoch_intent_dispatch dispatch)
{
    Dispatch = std::move(dispatch);
}

auto epoch_comparison_control::history_model() const noexcept
    -> epoch_history_model*
{
    return HistoryModel;
}

auto epoch_comparison_control::left_selector() const noexcept -> QComboBox*
{
    return LeftSelector;
}

auto epoch_comparison_control::right_selector() const noexcept -> QComboBox*
{
    return RightSelector;
}

auto epoch_comparison_control::left_oid_editor() const noexcept -> QLineEdit*
{
    return LeftOid;
}

auto epoch_comparison_control::right_oid_editor() const noexcept -> QLineEdit*
{
    return RightOid;
}

auto epoch_comparison_control::whole_pr_action() const noexcept -> QAction*
{
    return WholePrAction;
}

auto epoch_comparison_control::previous_upload_action() const noexcept
    -> QAction*
{
    return PreviousUploadAction;
}

auto epoch_comparison_control::last_reviewed_action() const noexcept -> QAction*
{
    return LastReviewedAction;
}

auto epoch_comparison_control::load_history_action() const noexcept -> QAction*
{
    return LoadHistoryAction;
}

auto epoch_comparison_control::return_to_newest_action() const noexcept
    -> QAction*
{
    return ReturnNewestAction;
}

auto epoch_comparison_control::refresh_action() const noexcept -> QAction*
{
    return RefreshAction;
}

auto epoch_comparison_control::cancel_action() const noexcept -> QAction*
{
    return CancelAction;
}

auto epoch_comparison_control::status_text() const -> QString
{
    return StatusLabel->text();
}

auto epoch_comparison_control::comparison_text() const -> QString
{
    return ComparisonLabel->text();
}

auto epoch_comparison_control::statistics() const noexcept
    -> epoch_comparison_control_statistics
{
    return Statistics;
}

void epoch_comparison_control::changeEvent(QEvent* const event)
{
    QWidget::changeEvent(event);
    switch(event->type())
    {
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

void epoch_comparison_control::dispatch_alias(
    const core::named_comparison alias)
{
    if(ApplyingProjection || !Dispatch)
    {
        return;
    }
    const auto catalog = HistoryModel->catalog();
    if(!catalog)
    {
        return;
    }
    Dispatch(core::select_named_comparison_intent{
        .Alias = alias,
        .ExpectedCatalogRevision = catalog->Revision,
    });
    update_selection_controls();
}

void epoch_comparison_control::dispatch_anchor(
    const core::comparison_side side,
    const int row)
{
    if(ApplyingProjection || !Dispatch || row < 0)
    {
        return;
    }
    const auto catalog = HistoryModel->catalog();
    const auto anchor = HistoryModel->anchor_at(HistoryModel->index(row, 0));
    if(!catalog || !anchor)
    {
        return;
    }
    Dispatch(core::select_comparison_anchor_intent{
        .Side = side,
        .Anchor = anchor->Identity,
        .ExpectedCatalogRevision = catalog->Revision,
    });
    update_selection_controls();
}

void epoch_comparison_control::dispatch_older_history_page()
{
    if(!Dispatch || !Continuation)
    {
        return;
    }
    const auto catalog = HistoryModel->catalog();
    if(!catalog)
    {
        return;
    }
    Dispatch(core::request_epoch_history_page_intent{
        .Catalog = *catalog,
        .Navigation = core::epoch_history_navigation::older,
        .Cursor = Continuation,
        .Limit = core::maximum_epoch_history_page_size,
    });
}

void epoch_comparison_control::dispatch_newest_history_page()
{
    if(!Dispatch)
    {
        return;
    }
    const auto catalog = HistoryModel->catalog();
    if(!catalog)
    {
        return;
    }
    Dispatch(core::request_epoch_history_page_intent{
        .Catalog = *catalog,
        .Navigation = core::epoch_history_navigation::newest,
        .Limit = core::maximum_epoch_history_page_size,
    });
}

void epoch_comparison_control::apply_catalog(
    const application::epoch_catalog_projection& projection)
{
    const auto previous_catalog = HistoryModel->catalog();
    HistoryModel->apply(projection);
    if(projection.Replace && !projection.Catalog)
    {
        Continuation.reset();
        HistoryNavigation.reset();
        HistoryPhase.reset();
        HistoryReason = core::epoch_reason_code::none;
        HistoryWindowStale = false;
    }
    else if(projection.Catalog && previous_catalog
        && *projection.Catalog != *previous_catalog
        && !projection.HistoryPage)
    {
        Continuation.reset();
    }
    if(projection.HistoryPage)
    {
        HistoryPhase = projection.HistoryPage->Phase;
        HistoryReason = projection.HistoryPage->Reason;
        if(projection.HistoryPage->Phase
            == core::epoch_history_page_phase::loaded)
        {
            HistoryNavigation = projection.HistoryPage->Navigation;
            Continuation = projection.HistoryPage->Continuation;
            HistoryWindowStale = false;
        }
        else if(projection.HistoryPage->Phase
            == core::epoch_history_page_phase::invalidated)
        {
            Continuation.reset();
            HistoryWindowStale = true;
        }
    }
}

void epoch_comparison_control::apply_transition(
    const application::epoch_transition_projection& projection)
{
    if(projection.Authority)
    {
        if(!projection.Authority->Authority)
        {
            HistoryModel->clear();
            Selection.reset();
            Comparison.reset();
            Continuation.reset();
            HistoryNavigation.reset();
            HistoryPhase.reset();
            HistoryWindowStale = false;
        }
    }
    if(projection.Selection)
    {
        Selection = projection.Selection->Selection;
        if(Comparison && Comparison->Selection != *Selection)
        {
            Comparison.reset();
        }
    }
    if(projection.Comparison)
    {
        Comparison = projection.Comparison;
        Selection = projection.Comparison->Selection;
    }
    if(projection.Status)
    {
        Status = projection.Status->Status;
    }
    if(projection.Commands)
    {
        Commands = projection.Commands->Commands;
        HasCommands = true;
    }
    if(!projection.Diagnostics.empty())
    {
        DiagnosticReason = projection.Diagnostics.back().Code;
    }
    else if(projection.Comparison
        || (projection.Status
            && projection.Status->Status == core::epoch_status::ready))
    {
        DiagnosticReason = core::epoch_reason_code::none;
    }
}

void epoch_comparison_control::update_controls()
{
    update_selection_controls();
    update_status_text();
    update_action_state();
}

void epoch_comparison_control::update_selection_controls()
{
    const QSignalBlocker left_blocker{LeftSelector};
    const QSignalBlocker right_blocker{RightSelector};
    auto left_index = -1;
    auto right_index = -1;
    auto left_oid = QString{};
    auto right_oid = QString{};
    auto left_description = QStringLiteral(
        "Complete lowercase commit OID; read-only and copyable");
    auto right_description = left_description;
    auto left_selector_description = QStringLiteral(
        "Select the exact base or head anchor for the left side");
    auto right_selector_description = QStringLiteral(
        "Select the exact base or head anchor for the right side");

    if(Comparison && Comparison->Pair)
    {
        const auto& pair = *Comparison->Pair;
        const auto left = HistoryModel->index_for_anchor(pair.Left.Identity);
        const auto right = HistoryModel->index_for_anchor(pair.Right.Identity);
        if(left.isValid())
        {
            left_index = left.row();
        }
        else
        {
            left_selector_description = QStringLiteral(
                "The exact selected left anchor is outside the loaded "
                "history window; its complete OID remains available below");
        }
        if(right.isValid())
        {
            right_index = right.row();
        }
        else
        {
            right_selector_description = QStringLiteral(
                "The exact selected right anchor is outside the loaded "
                "history window; its complete OID remains available below");
        }
        left_oid = oid_text(pair.Left.Commit);
        right_oid = oid_text(pair.Right.Commit);
        left_description =
            QStringLiteral("Complete lowercase left commit OID; %1")
                .arg(availability_text(pair.Left.Availability));
        right_description =
            QStringLiteral("Complete lowercase right commit OID; %1")
                .arg(availability_text(pair.Right.Availability));
    }
    if(LeftSelector->currentIndex() != left_index)
    {
        LeftSelector->setCurrentIndex(left_index);
    }
    if(RightSelector->currentIndex() != right_index)
    {
        RightSelector->setCurrentIndex(right_index);
    }
    if(LeftOid->text() != left_oid)
    {
        LeftOid->setText(left_oid);
    }
    if(RightOid->text() != right_oid)
    {
        RightOid->setText(right_oid);
    }
    LeftOid->setAccessibleDescription(left_description);
    RightOid->setAccessibleDescription(right_description);
    LeftSelector->setAccessibleDescription(left_selector_description);
    RightSelector->setAccessibleDescription(right_selector_description);

    WholePrAction->setChecked(false);
    PreviousUploadAction->setChecked(false);
    LastReviewedAction->setChecked(false);
    if(Selection)
    {
        if(const auto* named =
               std::get_if<core::named_comparison_selection>(&*Selection))
        {
            switch(named->Alias)
            {
            case core::named_comparison::whole_pr:
                WholePrAction->setChecked(true);
                break;
            case core::named_comparison::previous_upload_to_latest:
                PreviousUploadAction->setChecked(true);
                break;
            case core::named_comparison::my_last_reviewed_to_latest:
                LastReviewedAction->setChecked(true);
                break;
            }
        }
    }
}

void epoch_comparison_control::update_status_text()
{
    auto status = epoch_status_text(Status);
    const auto retained = HistoryModel->epoch_count() != 0U;
    if(retained && (Status == core::epoch_status::loading_catalog
                       || Status == core::epoch_status::preparing
                       || Status == core::epoch_status::committing
                       || Status == core::epoch_status::resolving))
    {
        status.append(QStringLiteral(" The retained comparison remains visible."));
    }
    if(HistoryPhase == core::epoch_history_page_phase::loading)
    {
        status = retained
            ? QStringLiteral(
                  "Loading earlier epochs; retained history remains visible.")
            : QStringLiteral("Loading review history…");
    }
    else if(HistoryPhase == core::epoch_history_page_phase::failed)
    {
        status = retained
            ? QStringLiteral(
                  "Earlier history could not be loaded; retained epochs "
                  "remain visible.")
            : QStringLiteral("Review history could not be loaded.");
    }
    else if(HistoryPhase == core::epoch_history_page_phase::cancelled)
    {
        status = retained
            ? QStringLiteral(
                  "History loading was canceled; retained epochs remain "
                  "visible.")
            : QStringLiteral("Review-history loading was canceled.");
    }
    else if(HistoryPhase == core::epoch_history_page_phase::invalidated)
    {
        status = retained
            ? QStringLiteral(
                  "Retained history changed; the visible page is stale. "
                  "Return to newest epochs to refresh it.")
            : QStringLiteral(
                  "Retained history changed; load the newest epoch page.");
    }
    else if(HistoryPhase == core::epoch_history_page_phase::loaded
        && !retained)
    {
        status = QStringLiteral("No review epochs have been recorded.");
    }
    const auto history_reason = reason_text(HistoryReason);
    if(!history_reason.isEmpty())
    {
        status.append(QStringLiteral(" %1").arg(history_reason));
    }
    const auto diagnostic = reason_text(DiagnosticReason);
    if(!diagnostic.isEmpty() && DiagnosticReason != HistoryReason)
    {
        status.append(QStringLiteral(" %1").arg(diagnostic));
    }
    StatusLabel->setText(status);

    QString comparison;
    if(Selection)
    {
        if(const auto* named =
               std::get_if<core::named_comparison_selection>(&*Selection))
        {
            comparison = QStringLiteral("%1. ").arg(alias_text(named->Alias));
        }
        else
        {
            comparison = QStringLiteral("Exact epoch pair. ");
        }
    }
    if(Comparison && Comparison->Pair)
    {
        comparison.append(QStringLiteral("Left %1; right %2. ")
                              .arg(
                                  availability_text(
                                      Comparison->Pair->Left.Availability),
                                  availability_text(
                                      Comparison->Pair->Right.Availability)));
        comparison.append(Comparison->AvailableForDiff
                ? QStringLiteral("Ready for diff.")
                : QStringLiteral(
                      "The exact pair is retained but unavailable for diff."));
    }
    else
    {
        comparison.append(QStringLiteral("No exact pair is resolved."));
    }
    if(Comparison)
    {
        const auto reason = reason_text(Comparison->Reason);
        if(!reason.isEmpty())
        {
            comparison.append(QStringLiteral(" %1").arg(reason));
        }
    }
    ComparisonLabel->setText(comparison);
}

void epoch_comparison_control::update_action_state()
{
    const auto select = command(core::epoch_command_id::select_comparison);
    const auto refresh = command(core::epoch_command_id::refresh_epochs);
    const auto cancel = command(core::epoch_command_id::cancel_epoch_refresh);
    const bool select_enabled = select && select->Enabled;
    WholePrAction->setEnabled(select_enabled);
    PreviousUploadAction->setEnabled(select_enabled);
    LastReviewedAction->setEnabled(select_enabled);
    LeftSelector->setEnabled(select_enabled && !HistoryWindowStale
        && HistoryModel->rowCount() != 0);
    RightSelector->setEnabled(select_enabled && !HistoryWindowStale
        && HistoryModel->rowCount() != 0);
    RefreshAction->setEnabled(refresh && refresh->Enabled);
    CancelAction->setEnabled(cancel && cancel->Enabled);
    const bool page_loading = HistoryPhase
        == core::epoch_history_page_phase::loading;
    LoadHistoryAction->setEnabled(
        HistoryModel->catalog().has_value() && !page_loading
        && !HistoryWindowStale && Continuation.has_value());
    ReturnNewestAction->setEnabled(
        HistoryModel->catalog().has_value() && !page_loading
        && (HistoryWindowStale || !HistoryNavigation.has_value()
            || *HistoryNavigation == core::epoch_history_navigation::older));

    const auto describe =
        [](QAction* const action,
            const std::optional<core::epoch_command_projection>& value,
            const QString& enabled_text)
        {
            const auto description = value && value->Enabled
                ? enabled_text
                : reason_text(
                      value ? value->DisabledReason
                            : core::epoch_reason_code::no_active_authority);
            action->setToolTip(description);
            action->setStatusTip(description);
        };
    describe(WholePrAction,
        select,
        QStringLiteral("Compare branch base with latest."));
    describe(PreviousUploadAction,
        select,
        QStringLiteral("Compare the exact previous upload with latest."));
    describe(LastReviewedAction,
        select,
        QStringLiteral("Compare the exact last-reviewed commit with latest."));
    describe(RefreshAction,
        refresh,
        QStringLiteral("Refresh retained review revisions."));
    describe(CancelAction,
        cancel,
        QStringLiteral("Cancel the active review-history refresh."));
    LoadHistoryAction->setToolTip(LoadHistoryAction->isEnabled()
            ? QStringLiteral("Load the next bounded page of retained epochs.")
            : QStringLiteral("No earlier retained epoch page is available."));
    LoadHistoryAction->setStatusTip(LoadHistoryAction->toolTip());
    ReturnNewestAction->setToolTip(ReturnNewestAction->isEnabled()
            ? QStringLiteral("Load the newest bounded page of retained epochs.")
            : QStringLiteral("The newest retained epoch page is visible."));
    ReturnNewestAction->setStatusTip(ReturnNewestAction->toolTip());
}

void epoch_comparison_control::refresh_native_appearance()
{
    if(HistoryModel == nullptr)
    {
        return;
    }
    HistoryModel->refresh_presentation();
    LeftSelector->updateGeometry();
    RightSelector->updateGeometry();
    updateGeometry();
    ++Statistics.AppearanceUpdates;
}

void epoch_comparison_control::announce_update()
{
    const auto announcement = summary_text();
    if(announcement.isEmpty() || announcement == LastAnnouncement)
    {
        return;
    }
    LastAnnouncement = announcement;
    ++Statistics.Announcements;
    QAccessibleAnnouncementEvent event{this, announcement};
    event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&event);
}

auto epoch_comparison_control::summary_text() const -> QString
{
    auto summary = StatusLabel->text();
    if(!ComparisonLabel->text().isEmpty())
    {
        summary.append(QStringLiteral(" %1").arg(ComparisonLabel->text()));
    }
    if(summary.size() > 1'024)
    {
        summary.truncate(1'024);
    }
    return summary;
}

auto epoch_comparison_control::command(const core::epoch_command_id id) const
    -> std::optional<core::epoch_command_projection>
{
    if(!HasCommands)
    {
        return std::nullopt;
    }
    const auto found = std::ranges::find(Commands, id,
        &core::epoch_command_projection::Id);
    return found == Commands.end() ? std::nullopt : std::optional{*found};
}
}
