#include "bpr/qt/foundation_window.hpp"

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace bpr::qt
{
namespace
{
auto text(const std::string& value) -> QString
{
    return QString::fromUtf8(value);
}

auto diagnostic_message(const core::reason_code code) -> QString
{
    switch(code)
    {
    case core::reason_code::refresh_in_progress:
        return QStringLiteral("A refresh is already in progress.");
    case core::reason_code::stale_completion:
        return QStringLiteral("An outdated refresh result was ignored.");
    case core::reason_code::foundation_unavailable:
        return QStringLiteral("Foundation state is temporarily unavailable.");
    case core::reason_code::none:
        return QString{};
    }

    return QStringLiteral("An unknown diagnostic was reported.");
}
}

foundation_window::foundation_window(
    foundation_view_config config,
    semantic_dispatch dispatch,
    QWidget* const parent)
    : QMainWindow{parent}
    , Dispatch{std::move(dispatch)}
{
    setObjectName(QStringLiteral("foundationWindow"));
    setWindowTitle(QStringLiteral("BetterPullReview"));
    setMinimumSize(640, 420);
    resize(960, 640);

    auto* const toolbar = addToolBar(QStringLiteral("Application"));
    toolbar->setObjectName(QStringLiteral("applicationToolbar"));
    toolbar->setAccessibleName(QStringLiteral("Application actions"));
    toolbar->setMovable(false);

    RefreshAction = toolbar->addAction(QStringLiteral("&Refresh"));
    RefreshAction->setObjectName(QStringLiteral("refreshAction"));
    RefreshAction->setToolTip(QStringLiteral("Refresh BetterPullReview state"));
    RefreshAction->setStatusTip(
        QStringLiteral("Refresh the current BetterPullReview state"));
    RefreshAction->setShortcut(QKeySequence::Refresh);
    RefreshAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(RefreshAction, &QAction::triggered, this, [this] {
        Dispatch(core::application_event{core::refresh_requested_intent{}});
    });

    if(auto* const button =
           qobject_cast<QToolButton*>(toolbar->widgetForAction(RefreshAction)))
    {
        button->setAccessibleName(QStringLiteral("Refresh"));
        button->setAccessibleDescription(
            QStringLiteral("Refresh BetterPullReview state"));
    }

    auto* const central = new QWidget{this};
    central->setObjectName(QStringLiteral("emptyState"));
    central->setAccessibleName(QStringLiteral("BetterPullReview empty state"));
    auto* const layout = new QVBoxLayout{central};
    layout->setContentsMargins(48, 48, 48, 48);
    layout->setSpacing(14);

    auto* const title = new QLabel{QStringLiteral("BetterPullReview"), central};
    title->setObjectName(QStringLiteral("productTitle"));
    title->setAccessibleName(QStringLiteral("BetterPullReview"));
    auto title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 8);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* const empty_message =
        new QLabel{QStringLiteral("No pull request is open."), central};
    empty_message->setObjectName(QStringLiteral("emptyMessage"));
    empty_message->setAccessibleName(QStringLiteral("No pull request is open"));

    const auto facts = QStringLiteral(
                           "Version: %1\nPlatform: %2\nConfiguration: %3")
                           .arg(
                               text(config.Version),
                               text(config.Platform),
                               text(config.Configuration));
    auto* const build_facts = new QLabel{facts, central};
    build_facts->setObjectName(QStringLiteral("buildFacts"));
    build_facts->setAccessibleName(
        QStringLiteral("Build and platform information"));
    build_facts->setTextInteractionFlags(Qt::TextSelectableByKeyboard);

    DiagnosticLabel = new QLabel{central};
    DiagnosticLabel->setObjectName(QStringLiteral("diagnosticMessage"));
    DiagnosticLabel->setAccessibleName(QStringLiteral("Application diagnostic"));
    DiagnosticLabel->setWordWrap(true);
    DiagnosticLabel->hide();

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(empty_message);
    layout->addWidget(build_facts);
    layout->addWidget(DiagnosticLabel);
    layout->addStretch();
    setCentralWidget(central);

    StatusLabel = new QLabel{QStringLiteral("Ready"), this};
    StatusLabel->setObjectName(QStringLiteral("foundationStatus"));
    StatusLabel->setAccessibleName(QStringLiteral("Application status"));
    statusBar()->addWidget(StatusLabel, 1);
}

void foundation_window::publish(const core::transition_output& output)
{
    for(const auto& command : output.Commands)
    {
        apply_command(command);
    }

    if(output.Status.has_value())
    {
        StatusLabel->setText(text(output.Status->Message));
        ++Statistics.StatusUpdates;
    }

    for(const auto& diagnostic : output.Diagnostics)
    {
        apply_diagnostic(diagnostic);
    }
}

auto foundation_window::refresh_action() const noexcept -> QAction*
{
    return RefreshAction;
}

auto foundation_window::status_text() const -> std::string
{
    return StatusLabel->text().toStdString();
}

auto foundation_window::diagnostic_text() const -> std::string
{
    return DiagnosticLabel->text().toStdString();
}

auto foundation_window::statistics() const noexcept -> projection_stats
{
    return Statistics;
}

void foundation_window::apply_command(
    const core::command_projection& command)
{
    if(command.Id != core::command_id::refresh)
    {
        return;
    }

    RefreshAction->setEnabled(command.Enabled);
    RefreshAction->setChecked(command.Checked);
    ++Statistics.CommandUpdates;
}

void foundation_window::apply_diagnostic(const core::diagnostic& diagnostic)
{
    const auto message = diagnostic_message(diagnostic.Code);
    DiagnosticLabel->setText(message);
    DiagnosticLabel->setVisible(!message.isEmpty());
    ++Statistics.DiagnosticUpdates;
}
}
