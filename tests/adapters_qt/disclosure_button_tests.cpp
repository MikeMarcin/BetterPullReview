#include "bpr/application/github_projection.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diagnostic_settings_panel.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/source_search_panel.hpp"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QStyle>
#include <QStyleFactory>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// [[BPR-1579]]. Every disclosure bar in the application, in the four built-in
// themes: its title reads at 4.5:1 or more on the bar, the bar is the theme's
// raised surface (BPR-1513's accepted treatment) and the chevron is the text
// colour. The owner's report was "Check details: 3 failed" in Dark, white text
// on a light grey native bezel.

namespace {
	using namespace bpr;

	auto Failures = 0;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			++Failures;
			std::cerr << "FAIL: " << aMessage << '\n';
		}
	}

	void settle()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto ratio(const QColor& aOne, const QColor& aOther) -> double
	{
		return qt::contrast_ratio(qt::to_appearance_color(aOne), qt::to_appearance_color(aOther));
	}

	[[nodiscard]] auto colour_near(const QColor& aOne, const QColor& aOther, const int aTolerance = 2) -> bool
	{
		return std::abs(aOne.red() - aOther.red()) <= aTolerance && std::abs(aOne.green() - aOther.green()) <= aTolerance
			&& std::abs(aOne.blue() - aOther.blue()) <= aTolerance;
	}

	[[nodiscard]] auto hex(const QColor& aColour) -> std::string
	{
		return aColour.name(QColor::HexRgb).toUpper().toStdString();
	}

	/// \brief What a bar shows: its fill beside the title, its title's
	/// strongest pixel against the bar under it, and its chevron's colour.
	struct bar_reading
	{
		QColor Fill;
		double Text    = 0.0;
		QColor Chevron;
		QImage Image;
	};

	/// \brief Reads a disclosure as it paints: once with its title and once
	/// without, so the title is exactly the pixels that differ.
	[[nodiscard]] auto read_bar(QToolButton& aBar) -> bar_reading
	{
		const auto title   = aBar.text();
		const auto with    = aBar.grab().toImage().convertToFormat(QImage::Format_ARGB32);
		const auto blocked = aBar.blockSignals(true);
		aBar.setText(QString{ });
		const auto without = aBar.grab().toImage().convertToFormat(QImage::Format_ARGB32);
		aBar.setText(title);
		aBar.blockSignals(blocked);
		auto       reading = bar_reading{ };
		const auto scale   = with.devicePixelRatio();
		const auto middle  = static_cast<int>((aBar.height() / 2.0) * scale);
		// Beside the title's end, inside the bar's right inset.
		reading.Fill    = without.pixelColor(static_cast<int>((aBar.width() - 3) * scale), middle);
		reading.Chevron = without.pixelColor(static_cast<int>(9.0 * scale), middle);
		for (auto y = 0; y < with.height(); ++y) {
			for (auto x = 0; x < with.width(); ++x) {
				const auto drawn = with.pixelColor(x, y);
				const auto under = without.pixelColor(x, y);
				if (drawn != under) {
					reading.Text = std::max(reading.Text, ratio(drawn, under));
				}
			}
		}
		reading.Image = with;
		return reading;
	}

	/// \brief One theme as the application hands it to a surface.
	struct theme
	{
		core::appearance_mode Mode;
		std::string           Name;
		QColor                Surface;
		QColor                Raised;
		QColor                Text;
	};

	[[nodiscard]] auto built_in_themes() -> std::vector<theme>
	{
		auto themes = std::vector<theme>{ };
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			auto widget = QWidget{ };
			profile.apply_to(widget);
			const auto& colours = widget.palette();
			themes.push_back({ .Mode    = mode,
							   .Name    = std::string{ core::stable_name(profile.variant()) },
							   .Surface = colours.color(QPalette::Active, QPalette::Window),
							   .Raised  = colours.color(QPalette::Active, QPalette::Light),
							   .Text    = colours.color(QPalette::Active, QPalette::WindowText) });
		}
		return themes;
	}

	/// \brief Whether a bar reading is BPR-1513's raised surface with a
	/// readable title, the property every disclosure must have.
	[[nodiscard]] auto passes(const bar_reading& aReading, const theme& aTheme) -> bool
	{
		return aReading.Text >= 4.5 && colour_near(aReading.Fill, aTheme.Raised);
	}

	/// \brief One disclosure in its real host.
	struct site
	{
		std::string                                     Name;
		std::function<std::unique_ptr<QWidget>()>       Make;
		std::function<QToolButton*(QWidget&)>           Find;
		QString                                         Title;
	};

	[[nodiscard]] auto named(const char* aName) -> std::function<QToolButton*(QWidget&)>
	{
		return [name = QString::fromLatin1(aName)](QWidget& aHost) { return aHost.findChild<QToolButton*>(name); };
	}

	[[nodiscard]] auto sites() -> std::vector<site>
	{
		auto all = std::vector<site>{ };
		all.push_back({ "Overview Check details", [] { return std::make_unique<qt::pull_request_overview_page>(); }, named("overviewChecksDisclosure"), QStringLiteral("Check details: 3 failed") });
		all.push_back({ "Overview Requirement details", [] { return std::make_unique<qt::pull_request_overview_page>(); }, named("overviewRequirementDisclosure"), QString{ } });
		all.push_back({ "Compose review Pull request and account", [] { return std::make_unique<qt::review_submission_editor>([](const auto&) {}); }, named("wholeReviewDetails"), QString{ } });
		all.push_back({ "Source search skipped sides", [] { return std::make_unique<qt::source_search_panel>(nullptr); }, named("sourceSearchSkippedToggle"), QStringLiteral("2 sides not searched") });
		all.push_back({ "Diagnostics About local diagnostics", [] { return std::make_unique<qt::diagnostic_settings_panel>(); },
						[](QWidget& aHost) -> QToolButton* {
							for (auto* const button : aHost.findChildren<qt::disclosure_button*>()) {
								if (button->text() == QStringLiteral("About local diagnostics")) {
									return button;
								}
							}
							return nullptr;
						},
						QString{ } });
		all.push_back({ "Pull request Technical details", [] { return std::make_unique<qt::github_pull_request_panel>(); }, named("githubTechnicalDetailsButton"), QString{ } });
		all.push_back({ "Server card Details",
						[] {
							auto option     = application::github_profile_option{ };
							option.Profile  = core::host_profile_id{ 7U };
							option.Label    = "GitHub";
							option.Hostname = "github.com";
							return std::make_unique<qt::server_card>(option, qt::server_card_gestures{ });
						},
						named("serverCardDetailsButton"), QString{ } });
		all.push_back({ "Profile maintenance", [] { return std::make_unique<qt::release_maintenance_panel>(); }, named("profileMaintenanceOptions"), QString{ } });
		return all;
	}

	/// \brief Places a disclosure where it can be read: shown, laid out, and
	/// at least as large as it asks.
	void lay_out(QWidget& aHost, QToolButton& aBar, const QString& aTitle)
	{
		if (!aTitle.isEmpty()) {
			aBar.setText(aTitle);
		}
		aHost.resize(720, 900);
		aHost.show();
		aBar.show();
		settle();
		if (aBar.width() < aBar.sizeHint().width() || aBar.height() < aBar.sizeHint().height() || !aBar.isVisible()) {
			// A section its host keeps closed until it has content: read the
			// bar at its own size.
			aBar.setParent(&aHost);
			aBar.setGeometry(QRect{ QPoint{ 8, 8 }, aBar.sizeHint() + QSize{ 40, 0 } });
			aBar.show();
			settle();
		}
	}

	void test_every_disclosure_reads_in_every_theme(const QString& aRoot)
	{
		const auto themes = built_in_themes();
		for (const auto& candidate : sites()) {
			for (const auto& colours : themes) {
				auto profile = qt::appearance_profile{ };
				static_cast<void>(profile.select_mode(colours.Mode));
				auto host = candidate.Make();
				profile.apply_to(*host);
				if (auto* const page = dynamic_cast<qt::pull_request_overview_page*>(host.get())) {
					page->adopt_appearance(profile);
				}
				auto* const bar   = candidate.Find(*host);
				const auto  label = candidate.Name + " in " + colours.Name;
				expect(bar != nullptr && dynamic_cast<qt::disclosure_button*>(bar) != nullptr, label + ": the disclosure is the shared disclosure bar");
				if (bar == nullptr) {
					continue;
				}
				lay_out(*host, *bar, candidate.Title);
				expect(colour_near(bar->palette().color(QPalette::Active, QPalette::Window), colours.Surface), label + ": the bar has the theme's palette");
				for (const auto open : { false, true }) {
					bar->setChecked(open);
					settle();
					const auto state = label + (open ? " open" : " closed");
					expect(bar->arrowType() == (open ? Qt::DownArrow : Qt::RightArrow), state + ": the arrow follows the state");
					const auto reading = read_bar(*bar);
					expect(colour_near(reading.Fill, colours.Raised),
						   state + ": the bar is the theme's raised surface " + hex(colours.Raised) + ", not " + hex(reading.Fill));
					expect(reading.Text >= 4.5, state + ": the title reads at 4.5:1 or more on the bar (" + std::to_string(reading.Text) + ")");
					expect(colour_near(reading.Chevron, colours.Text, 24), state + ": the chevron is the text colour " + hex(colours.Text) + ", not " + hex(reading.Chevron));
					if (!aRoot.isEmpty()) {
						auto file = QString::fromStdString(candidate.Name + "-" + colours.Name + (open ? "-open" : "-closed") + ".png");
						file.replace(QLatin1Char{ ' ' }, QLatin1Char{ '-' });
						static_cast<void>(reading.Image.save(aRoot + QLatin1Char{ '/' } + file));
					}
				}
				// Pressed: the tint keeps the title readable.
				bar->setDown(true);
				const auto pressed = read_bar(*bar);
				bar->setDown(false);
				expect(pressed.Text >= 4.5, label + " pressed: the title stays readable (" + std::to_string(pressed.Text) + ")");
				bar->setChecked(false);
			}
		}
	}

	/// \brief The disclosure as it was before [[BPR-1579]]: a plain tool button
	/// with an arrow, drawn by the platform style.
	[[nodiscard]] auto old_disclosure(QWidget& aHost, const QString& aTitle) -> QToolButton*
	{
		auto* const bar = new QToolButton{ &aHost };
		bar->setText(aTitle);
		bar->setCheckable(true);
		bar->setChecked(true);
		bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		bar->setArrowType(Qt::DownArrow);
		bar->setGeometry(QRect{ QPoint{ 8, 8 }, QSize{ 260, bar->sizeHint().height() } });
		return bar;
	}

	void test_the_old_platform_bar_fails_the_reading(const QString& aRoot)
	{
		// The negative control: the reading must refuse what the owner saw.
		// On macOS the native style draws the old bar as a bezel in the system
		// appearance's grey; other platforms draw the style's own button
		// fill, which is not the raised surface either.
		const auto themes = built_in_themes();
		const auto dark   = std::ranges::find_if(themes, [](const theme& aTheme) { return aTheme.Mode == core::appearance_mode::dark; });
		std::unique_ptr<QStyle> native{ QStyleFactory::create(QStringLiteral("macos")) };
		for (const auto& colours : themes) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(colours.Mode));
			auto host = QWidget{ };
			profile.apply_to(host);
			host.resize(300, 60);
			auto* const bar = old_disclosure(host, QStringLiteral("Check details: 3 failed"));
			if (native) {
				bar->setStyle(native.get());
			}
			host.show();
			settle();
			const auto reading = read_bar(*bar);
			const auto label   = "the old " + std::string{ native ? "macOS" : "platform" } + " bar in " + colours.Name;
			std::cout << label << ": fill " << hex(reading.Fill) << ", title " << reading.Text << ":1\n";
			expect(!passes(reading, colours), label + " fails the disclosure reading");
			if (!aRoot.isEmpty()) {
				static_cast<void>(reading.Image.save(aRoot + QStringLiteral("/before-") + QString::fromStdString(colours.Name) + QStringLiteral(".png")));
			}
			bar->setStyle(nullptr);
		}
		if (native && dark != themes.end()) {
			// What the owner reported: the Dark title on the native bezel.
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(core::appearance_mode::dark));
			auto host = QWidget{ };
			profile.apply_to(host);
			auto* const bar = old_disclosure(host, QStringLiteral("Check details: 3 failed"));
			bar->setStyle(native.get());
			host.show();
			settle();
			const auto reading = read_bar(*bar);
			expect(reading.Text < 4.5, "the old macOS bar's Dark title is below 4.5:1 on its bezel (" + std::to_string(reading.Text) + ")");
			bar->setStyle(nullptr);
		}
	}

	void test_a_palette_without_a_readable_raised_surface_falls_back()
	{
		// A top-level dialog keeps the platform palette, whose `Light` need not
		// pair with its text: the bar then sits on the plain surface.
		auto colours = QPalette{ };
		colours.setColor(QPalette::Window, QColor{ 0x1C, 0x20, 0x26 });
		colours.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		colours.setColor(QPalette::Light, QColor{ 0xE3, 0xE3, 0xE3 });
		expect(qt::disclosure_fill(colours, QPalette::Active) == QColor{ 0x1C, 0x20, 0x26 }, "a light raised surface under light text falls back to the surface");
		colours.setColor(QPalette::Light, QColor{ 0x2A, 0x30, 0x39 });
		expect(qt::disclosure_fill(colours, QPalette::Active) == QColor{ 0x2A, 0x30, 0x39 }, "a readable raised surface is the fill");
	}

	void test_the_treatment_is_bpr_1513s()
	{
		// The accepted v003 values: Light #FFFFFF on #F6F7F9, Dark #2A3039 on
		// #1C2026, the high-contrast themes flat.
		for (const auto& colours : built_in_themes()) {
			if (colours.Mode == core::appearance_mode::light) {
				expect(hex(colours.Raised) == "#FFFFFF" && hex(colours.Surface) == "#F6F7F9", "Light's bar is #FFFFFF on #F6F7F9");
			}
			if (colours.Mode == core::appearance_mode::dark) {
				expect(hex(colours.Raised) == "#2A3039" && hex(colours.Surface) == "#1C2026", "Dark's bar is #2A3039 on #1C2026");
			}
			expect(ratio(colours.Raised, colours.Surface) < 1.5, colours.Name + ": the bar is a subtle lift, not a block");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   root = qEnvironmentVariable("BPR_DISCLOSURE_RENDER_ROOT");
	test_the_treatment_is_bpr_1513s();
	test_a_palette_without_a_readable_raised_surface_falls_back();
	test_every_disclosure_reads_in_every_theme(root);
	test_the_old_platform_bar_fails_the_reading(root);
	if (Failures != 0) {
		std::cerr << Failures << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "disclosure-button-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " style=" << QApplication::style()->name().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
