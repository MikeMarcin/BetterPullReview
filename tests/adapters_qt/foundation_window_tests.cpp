#include "bpr/qt/foundation_window.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto test_event_translation_and_accessibility() -> void
	{
		std::vector<core::application_event> events;
		qt::foundation_window                window{
			qt::foundation_view_config{
				.Version       = "test-version",
				.Platform      = "test-platform",
				.Configuration = "test-config",
			},
			[&events](core::application_event aEvent) {
				events.push_back(std::move(aEvent));
			},
		};

		auto* const action = window.refresh_action();
		expect(action != nullptr, "refresh QAction exists");
		expect(
			!action->shortcut().isEmpty(),
			"refresh QAction has keyboard access");
		action->trigger();
		expect(events.size() == 1U, "QAction emits one semantic event");
		expect(
			std::holds_alternative<core::refresh_requested_intent>(events.front()),
			"QAction translates to refresh_requested_intent");

		auto* const toolbar = window.findChild<QToolBar*>(QStringLiteral("applicationToolbar"));
		expect(toolbar != nullptr, "application toolbar exists");
		expect(
			!toolbar->accessibleName().isEmpty(),
			"application toolbar has an accessible name");
		auto* const button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
		expect(button != nullptr, "QAction is presented as a tool button");
		expect(
			button->accessibleName() == QStringLiteral("Refresh"),
			"refresh tool button has an accessible name");

		auto* const facts = window.findChild<QLabel*>(QStringLiteral("buildFacts"));
		expect(facts != nullptr, "build facts are rendered");
		expect(
			facts->text().contains(QStringLiteral("test-platform")),
			"selected platform is visible");

		window.resize(640, 420);
		window.show();
		QApplication::processEvents();
		expect(window.centralWidget()->isVisible(), "empty state is visible");
		expect(toolbar->isVisible(), "toolbar is visible at minimum size");
		expect(button->isVisible(), "primary action is visible at minimum size");
		expect(facts->isVisible(), "build facts are visible at minimum size");
		expect(
			window.centralWidget()->width() > 0
				&& window.centralWidget()->height() > 0,
			"minimum-size central viewport has positive geometry");

		window.activateWindow();
		button->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == button,
			"primary action participates in keyboard focus");

		window.resize(960, 640);
		QApplication::processEvents();
		expect(
			window.centralWidget()->width() > 0
				&& window.centralWidget()->height() > 0,
			"default-size central viewport has positive geometry");
	}

	auto test_incremental_projection_application() -> void
	{
		qt::foundation_window window{
			qt::foundation_view_config{
				.Version       = "test-version",
				.Platform      = "test-platform",
				.Configuration = "test-config",
			},
			[](core::application_event) { },
		};

		core::transition_output pending;
		pending.Commands.push_back(core::command_projection{
			.Id             = core::command_id::refresh,
			.Enabled        = false,
			.DisabledReason = core::reason_code::refresh_in_progress,
		});
		pending.Status = core::status_delta{ .Message = "Refreshing foundation state" };
		window.publish(pending);

		expect(
			!window.refresh_action()->isEnabled(),
			"command projection disables QAction");
		expect(
			window.status_text() == "Refreshing foundation state",
			"status delta updates status presentation");
		const auto pendingStats = window.statistics();
		expect(
			pendingStats.CommandUpdates == 1U
				&& pendingStats.StatusUpdates == 1U,
			"only supplied command and status projections are counted");
		expect(pendingStats.FullResets == 0U, "bounded update performs no reset");

		core::transition_output diagnostic;
		diagnostic.Diagnostics.push_back(core::diagnostic{
			.Code   = core::reason_code::stale_completion,
			.Detail = "internal detail is not presentation text",
		});
		window.publish(diagnostic);

		expect(
			window.diagnostic_text() == "An outdated refresh result was ignored.",
			"diagnostic uses presentation-owned wording");
		const auto diagnosticStats = window.statistics();
		expect(
			diagnosticStats.CommandUpdates == pendingStats.CommandUpdates
				&& diagnosticStats.StatusUpdates == pendingStats.StatusUpdates,
			"diagnostic update leaves unrelated projections untouched");
		expect(
			diagnosticStats.DiagnosticUpdates == 1U,
			"diagnostic update is tracked independently");

		core::transition_output failure;
		failure.Diagnostics.push_back(core::diagnostic{
			.Code   = core::reason_code::foundation_unavailable,
			.Detail = "provider detail stays outside presentation",
		});
		window.publish(failure);
		expect(
			window.diagnostic_text()
				== "Foundation state is temporarily unavailable.",
			"operational failure has presentation-owned wording");

		core::transition_output complete;
		complete.Commands.push_back(core::command_projection{
			.Id      = core::command_id::refresh,
			.Enabled = true,
		});
		complete.Status = core::status_delta{ .Message = "Foundation state is current" };
		window.publish(complete);
		expect(
			window.refresh_action()->isEnabled(),
			"completion projection re-enables QAction");
		expect(
			window.status_text() == "Foundation state is current",
			"completion status is visible");
		expect(
			window.statistics().FullResets == 0U,
			"all foundation projections remain incremental");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewQtTests"));

	test_event_translation_and_accessibility();
	test_incremental_projection_application();

	std::cout << "All Qt foundation adapter tests passed\n";
	return EXIT_SUCCESS;
}
