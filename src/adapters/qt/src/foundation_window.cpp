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

namespace bpr::qt {
	namespace {
		auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue);
		}

		auto diagnostic_message(const core::reason_code aCode) -> QString
		{
			switch (aCode) {
			case core::reason_code::refresh_in_progress:
				return QStringLiteral("A refresh is already in progress.");
			case core::reason_code::stale_completion:
				return QStringLiteral("An outdated refresh result was ignored.");
			case core::reason_code::foundation_unavailable:
				return QStringLiteral("Foundation state is temporarily unavailable.");
			case core::reason_code::none:
				return QString{ };
			}

			return QStringLiteral("An unknown diagnostic was reported.");
		}
	}

	foundation_window::foundation_window(
		foundation_view_config aConfig,
		semantic_dispatch      aDispatch,
		QWidget* const         aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
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
			Dispatch(core::application_event{ core::refresh_requested_intent{ } });
		});

		if (auto* const button = qobject_cast<QToolButton*>(toolbar->widgetForAction(RefreshAction))) {
			button->setAccessibleName(QStringLiteral("Refresh"));
			button->setAccessibleDescription(
				QStringLiteral("Refresh BetterPullReview state"));
		}

		auto* const central = new QWidget{ this };
		central->setObjectName(QStringLiteral("emptyState"));
		central->setAccessibleName(QStringLiteral("BetterPullReview empty state"));
		auto* const layout = new QVBoxLayout{ central };
		layout->setContentsMargins(48, 48, 48, 48);
		layout->setSpacing(14);

		auto* const title = new QLabel{ QStringLiteral("BetterPullReview"), central };
		title->setObjectName(QStringLiteral("productTitle"));
		title->setAccessibleName(QStringLiteral("BetterPullReview"));
		auto titleFont = title->font();
		titleFont.setPointSize(titleFont.pointSize() + 8);
		titleFont.setBold(true);
		title->setFont(titleFont);

		auto* const emptyMessage = new QLabel{ QStringLiteral("No pull request is open."), central };
		emptyMessage->setObjectName(QStringLiteral("emptyMessage"));
		emptyMessage->setAccessibleName(QStringLiteral("No pull request is open"));

		const auto  facts      = QStringLiteral(
									 "Version: %1\nPlatform: %2\nConfiguration: %3")
									 .arg(
										 text(aConfig.Version),
										 text(aConfig.Platform),
										 text(aConfig.Configuration));
		auto* const buildFacts = new QLabel{ facts, central };
		buildFacts->setObjectName(QStringLiteral("buildFacts"));
		buildFacts->setAccessibleName(
			QStringLiteral("Build and platform information"));
		buildFacts->setTextInteractionFlags(Qt::TextSelectableByKeyboard);

		DiagnosticLabel = new QLabel{ central };
		DiagnosticLabel->setObjectName(QStringLiteral("diagnosticMessage"));
		DiagnosticLabel->setAccessibleName(QStringLiteral("Application diagnostic"));
		DiagnosticLabel->setWordWrap(true);
		DiagnosticLabel->hide();

		layout->addStretch();
		layout->addWidget(title);
		layout->addWidget(emptyMessage);
		layout->addWidget(buildFacts);
		layout->addWidget(DiagnosticLabel);
		layout->addStretch();
		setCentralWidget(central);

		StatusLabel = new QLabel{ QStringLiteral("Ready"), this };
		StatusLabel->setObjectName(QStringLiteral("foundationStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Application status"));
		statusBar()->addWidget(StatusLabel, 1);
	}

	void foundation_window::publish(const core::transition_output& aOutput)
	{
		for (const auto& command : aOutput.Commands) {
			apply_command(command);
		}

		if (aOutput.Status.has_value()) {
			StatusLabel->setText(text(aOutput.Status->Message));
			++Statistics.StatusUpdates;
		}

		for (const auto& diagnostic : aOutput.Diagnostics) {
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
		const core::command_projection& aCommand)
	{
		if (aCommand.Id != core::command_id::refresh) {
			return;
		}

		RefreshAction->setEnabled(aCommand.Enabled);
		RefreshAction->setChecked(aCommand.Checked);
		++Statistics.CommandUpdates;
	}

	void foundation_window::apply_diagnostic(const core::diagnostic& aDiagnostic)
	{
		const auto message = diagnostic_message(aDiagnostic.Code);
		DiagnosticLabel->setText(message);
		DiagnosticLabel->setVisible(!message.isEmpty());
		++Statistics.DiagnosticUpdates;
	}
}
