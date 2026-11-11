/// \file
/// \brief [[BPR-1924]], owner look on BPR-1914 v004 ("this box around the
/// model picker looks more like a text entry field than a dropdown/picker.
/// It also makes me think that maybe the picker needs to wrap to a 2nd row on
/// the agent tab if the drawer is narrow").
///
/// \details The Agent tab's question field in a Comments drawer swept from
/// 380 to 900 pixels, Light and Dark. At every width:
/// - the agent · model · effort picker keeps its whole words;
/// - it overlaps neither the question field nor any other control;
/// - the question field keeps the field's whole width;
/// - it shares the tool row exactly when the row holds every control whole
///   with the gap to spare (the wrap threshold), and has a row of its own
///   under the tools otherwise;
/// - once it fits beside Send as the drawer widens, it stays there.
/// The picker draws a chevron, and deepens under the pointer. The Tour tab's
/// picker stands on its bar with no field drawn round it.
///
/// `BPR_AGENT_PICKER_RENDER_ROOT`, when set, receives the Agent and Tour tabs
/// at 380, 520 and 900 pixels in each theme (AC1).

#include "../support/review_tour_fixture.hpp"

#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEnterEvent>
#include <QImage>
#include <QLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabBar>
#include <QTextEdit>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	void save(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_AGENT_PICKER_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(aWidget.grab().save(QStringLiteral("%1/%2.png").arg(root, aName), "PNG"), "the render is saved: " + aName.toStdString());
		}
	}

	/// \brief A window in Review with the Comments drawer \p aDrawerWidth wide.
	void open_review(qt::workbench_window& aWindow, qt::appearance_profile& aProfile, const std::uint32_t aDrawerWidth)
	{
		aWindow.resize(1'920, 900);
		aProfile.apply_to(aWindow);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = aDrawerWidth };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
	}

	/// \brief Drags the Comments drawer's seam until the drawer is \p aWidth.
	void drag_drawer(qt::workbench_window& aWindow, const int aWidth)
	{
		auto* const splitter = aWindow.central_splitter();
		splitter->move_handle(3, splitter->width() - aWidth - splitter->handleWidth());
		settle();
	}

	/// \brief An agent chosen, with its model and effort, so the picker shows.
	void choose_agent(qt::workbench_window& aWindow)
	{
		auto profile        = core::agent_profile{ };
		profile.Id          = core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U);
		profile.Protocol    = core::agent_protocol::claude_stream_json;
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto settings            = qt::agent_run_settings{ .Profile = aWindow.agent_panel()->selected_profile()->value(), .Agent = QStringLiteral("Claude Code"), .Program = QStringLiteral("/usr/bin/true") };
		settings.ModelChoosable  = true;
		settings.EffortChoosable = true;
		settings.Effort          = "high";
		settings.Efforts         = { "low", "medium", "high" };
		aWindow.agent_tab()->present_run_settings({ settings });
		aWindow.agent_tab()->refresh();
		settle();
	}

	[[nodiscard]] auto in(const QWidget& aWidget, const QWidget& aAncestor) -> QRect { return QRect{ aWidget.mapTo(&aAncestor, QPoint{ 0, 0 }), aWidget.size() }; }

	/// \brief The picker draws something besides its words on its right: the
	/// chevron. Its fill differs from the field it sits on.
	void expect_reads_as_picker(QToolButton& aPicker, const QWidget& aField, const std::string& aAt)
	{
		const auto image = aPicker.grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto fill  = image.pixelColor(static_cast<int>(6 * ratio), image.height() / 2);
		auto       marks = 0;
		for (auto x = static_cast<int>((aPicker.width() - 18) * ratio); x < static_cast<int>((aPicker.width() - 6) * ratio); ++x) {
			for (auto y = 0; y < image.height(); ++y) {
				const auto pixel = image.pixelColor(x, y);
				if (std::abs(pixel.lightness() - fill.lightness()) > 40) {
					++marks;
				}
			}
		}
		expect(marks > 4, "the picker draws its chevron" + aAt);
		const auto ground = aField.palette().color(QPalette::Active, QPalette::Base);
		expect(fill != ground, "the picker is a filled pill, not the field's own ground" + aAt);
	}

	// Predict from the widget's independent public constraints, rather than
	// calling the production fit function or its layout-item calculation.
	[[nodiscard]] auto effective_width(const QWidget& aWidget) -> int
	{
		const auto preferred = std::max(aWidget.sizeHint().width(), aWidget.minimumSizeHint().width());
		return std::max(aWidget.minimumWidth(), std::min(preferred, aWidget.maximumWidth()));
	}

	void test_an_explicit_minimum_is_not_lost()
	{
		auto                 profile = qt::appearance_profile{ qt::default_appearance_settings() };
		qt::workbench_window window;
		open_review(window, profile, 900U);
		window.tray_tabs()->setCurrentIndex(3);
		settle();
		choose_agent(window);
		auto* const agent  = window.agent_tab();
		auto* const picker = agent->run_settings_button();
		auto* const field  = picker->parentWidget();
		auto* const scope  = window.side_chat()->scope_selector();
		auto* const ask    = agent->ask_button();
		expect(field != nullptr, "the explicit-minimum control has the question field");
		scope->setMinimumWidth(effective_width(*scope) + 96);
		expect(scope->minimumWidth() > scope->sizeHint().width(), "the control's explicit minimum is greater than its raw size hint");
		auto distinguished = false;
		for (auto width = 380; width <= 900; width += 4) {
			drag_drawer(window, width);
			const auto left          = scope->mapTo(field, QPoint{ 0, 0 }).x();
			const auto boundary      = field->width() - field->layout()->contentsMargins().right();
			const auto oldPrediction = left + scope->sizeHint().width() + 12 + picker->sizeHint().width() + 2 + ask->sizeHint().width();
			const auto required      = left + effective_width(*scope) + 12 + effective_width(*picker) + 2 + effective_width(*ask);
			if (oldPrediction <= boundary && required > boundary) {
				expect(agent->run_settings_wrapped(), "an explicit minimum beyond the raw hint requires the picker to wrap");
				expect(in(*picker, *field).top() >= in(*ask, *field).bottom(), "the minimum-width control preserves the picker's separate row");
				distinguished = true;
				break;
			}
		}
		expect(distinguished, "the bounded width sweep reaches the raw-hint versus explicit-minimum boundary");
		window.hide();
	}

	void test_the_picker_wraps_and_stays_whole()
	{
		struct theme
		{
			core::appearance_mode Mode;
			const char*           Name;
		};
		for (const auto& [mode, name] : { theme{ core::appearance_mode::light, "light" }, theme{ core::appearance_mode::dark, "dark" } }) {
			auto settings                = qt::default_appearance_settings();
			settings.Mode                = mode;
			auto                 profile = qt::appearance_profile{ settings };
			qt::workbench_window window;
			open_review(window, profile, 380U);
			window.tray_tabs()->setCurrentIndex(3);
			settle();
			choose_agent(window);
			auto* const agent    = window.agent_tab();
			auto* const picker   = agent->run_settings_button();
			auto* const field    = picker->parentWidget();
			auto* const composer = window.side_chat()->composer();
			auto* const ask      = agent->ask_button();
			auto* const scope    = window.side_chat()->scope_selector();
			expect(field != nullptr && field->isAncestorOf(composer) && field->isVisible(), std::string{ "the question field shows, " } + name);
			expect(picker->isVisible() && agent->run_settings_text() == QStringLiteral("Claude Code · Default model · high effort"), std::string{ "the picker names the agent, model and effort, " } + name);

			auto fitted = false;
			for (auto width = 380; width <= 900; width += 4) {
				drag_drawer(window, width);
				const auto at          = std::string{ " (" } + name + " at " + std::to_string(window.conversations_drawer()->width()) + ")";
				const auto pickerBox   = in(*picker, *field);
				const auto composerBox = in(*composer, *field);
				const auto margins     = field->layout()->contentsMargins();
				expect(picker->width() >= picker->sizeHint().width(), "the picker keeps its whole words" + at);
				expect(!pickerBox.intersects(composerBox), "the picker never lies over the question field" + at);
				expect(composerBox.width() == field->width() - margins.left() - margins.right(), "the question field keeps the field's whole width" + at);
				for (auto* const other : { static_cast<QWidget*>(ask), static_cast<QWidget*>(scope) }) {
					expect(!pickerBox.intersects(in(*other, *field)), "the picker overlaps no other control" + at);
				}
				expect(pickerBox.right() < field->width() - margins.right() + 1, "the picker stays inside the field" + at);
				// The threshold: one row while every control fits it whole with
				// the gap to spare.
				const auto row  = scope->mapTo(field, QPoint{ 0, 0 }).x() + effective_width(*scope) + 12 + effective_width(*picker) + 2 + effective_width(*ask);
				const auto fits = row <= field->width() - margins.right();
				expect(agent->run_settings_wrapped() == !fits, "the picker wraps exactly when the row cannot hold it whole" + at);
				if (agent->run_settings_wrapped()) {
					expect(pickerBox.top() >= in(*ask, *field).bottom(), "a wrapped picker is on its own row, under the tools" + at);
					expect(pickerBox.left() == margins.left(), "a wrapped picker starts at the field's left" + at);
					expect(!fitted, "once the picker fits beside Send as the drawer widens, it stays" + at);
				} else {
					expect(std::abs(pickerBox.center().y() - in(*ask, *field).center().y()) <= 2, "an unwrapped picker sits on Send's row" + at);
					expect(pickerBox.left() >= in(*scope, *field).right() + 12, "an unwrapped picker keeps its gap from the scope menu" + at);
					fitted = true;
				}
				expect_reads_as_picker(*picker, *field, at);
				if (width == 380 || width == 520 || width == 900) {
					save(window, QStringLiteral("agent-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				}
			}
			expect(fitted, std::string{ "the picker shares the row in a wide drawer, " } + name);
			drag_drawer(window, 380);
			expect(agent->run_settings_wrapped(), std::string{ "it wraps again when the drawer narrows, " } + name);

			// Under the pointer the pill deepens.
			const auto rest  = picker->grab().toImage().pixelColor(8, picker->height() / 2);
			auto       enter = QEnterEvent{ QPointF{ 4.0, 4.0 }, QPointF{ 4.0, 4.0 }, QPointF{ 4.0, 4.0 } };
			QApplication::sendEvent(picker, &enter);
			picker->setAttribute(Qt::WA_UnderMouse, true);
			const auto hover = picker->grab().toImage().pixelColor(8, picker->height() / 2);
			picker->setAttribute(Qt::WA_UnderMouse, false);
			expect(hover != rest, std::string{ "the picker answers the pointer, " } + name);

			// The Tour tab: the picker on its bar, no field drawn round it.
			auto* const editor = window.tour_editor();
			editor->set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
			auto presentation  = application::tour_presentation{ };
			presentation.Draft = std::make_shared<const core::review_tour>(tests::review_tour_fixture());
			editor->present(presentation);
			window.open_review_tour();
			settle();
			auto* const tourPicker = editor->agent_picker();
			auto* const tourField  = editor->findChild<QWidget*>(QStringLiteral("tourAgentField"));
			for (const auto width : { 380, 520, 900 }) {
				drag_drawer(window, width);
				const auto at = std::string{ " (Tour, " } + name + " at " + std::to_string(width) + ")";
				expect(tourPicker != nullptr && tourPicker->isVisible() && tourPicker->width() >= tourPicker->sizeHint().width(), "the Tour tab's picker shows whole" + at);
				// A field drawn round it would put its outline at the edge and
				// its own fill just inside; bare, the two are the same ground.
				const auto image = tourField->grab().toImage();
				const auto ratio = image.devicePixelRatio();
				const auto y     = image.height() / 2;
				expect(image.pixelColor(static_cast<int>(1 * ratio), y) == image.pixelColor(static_cast<int>(5 * ratio), y), "no field is drawn round the Tour tab's picker" + at);
				expect_reads_as_picker(*tourPicker, *tourField, at);
				save(window, QStringLiteral("tour-%1-%2").arg(QString::fromLatin1(name)).arg(width));
			}
			window.hide();
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_an_explicit_minimum_is_not_lost();
	test_the_picker_wraps_and_stays_whole();
	std::cout << "agent-picker-wrap:pass\n";
	return EXIT_SUCCESS;
}
