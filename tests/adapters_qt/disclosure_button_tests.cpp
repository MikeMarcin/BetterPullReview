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
		double Text = 0.0;
		QColor Chevron;
		QImage Image;
	};

	/// \brief Reads a disclosure as it paints: once with its title and once
	/// without, so the title is exactly the pixels that differ.
	[[nodiscard]] auto read_bar(QToolButton& aBar, const bool aHovered = false) -> bar_reading
	{
		// Whether the pointer is over the bar, as the hover tint asks.
		aBar.setAttribute(Qt::WA_UnderMouse, aHovered);
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
		std::string                               Name;
		std::function<std::unique_ptr<QWidget>()> Make;
		std::function<QToolButton*(QWidget&)>     Find;
		QString                                   Title;
	};

	[[nodiscard]] auto named(const char* aName) -> std::function<QToolButton*(QWidget&)>
	{
		return [name = QString::fromLatin1(aName)](QWidget& aHost) { return aHost.findChild<QToolButton*>(name); };
	}

	[[nodiscard]] auto sites() -> std::vector<site>
	{
		auto all = std::vector<site>{ };
		all.push_back({ "Overview Requirement details", [] { return std::make_unique<qt::pull_request_overview_page>(); }, named("overviewRequirementDisclosure"), QString{ } });
		all.push_back({ "Compose review Pull request and account", [] { return std::make_unique<qt::review_submission_editor>([](const auto&) { }); }, named("wholeReviewDetails"), QString{ } });
		all.push_back({ "Source search skipped sides", [] { return std::make_unique<qt::source_search_panel>(nullptr); }, named("sourceSearchSkippedToggle"), QStringLiteral("2 sides not searched") });
		all.push_back({ "Diagnostics About local diagnostics", [] { return std::make_unique<qt::diagnostic_settings_panel>(); },
						[](QWidget& aHost) -> QToolButton* {
							for (auto* const button : aHost.findChildren<QToolButton*>()) {
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
				const auto hovered = read_bar(*bar, true);
				bar->setAttribute(Qt::WA_UnderMouse, false);
				expect(hovered.Text >= 4.5, label + " hovered: the title stays readable (" + std::to_string(hovered.Text) + ")");
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

	/// \brief A palette whose button family is a native bezel's light grey,
	/// as macOS draws a checked tool button in the light system appearance,
	/// under the theme's own text.
	[[nodiscard]] auto under_native_bezel(QPalette aColours) -> QPalette
	{
		const auto bezel = QColor{ 0xE3, 0xE3, 0xE3 };
		for (const auto group : { QPalette::Active, QPalette::Inactive }) {
			aColours.setColor(group, QPalette::Button, bezel);
			aColours.setColor(group, QPalette::Light, bezel.lighter(110));
			aColours.setColor(group, QPalette::Midlight, bezel.lighter(105));
			aColours.setColor(group, QPalette::Mid, bezel.darker(120));
			aColours.setColor(group, QPalette::Dark, bezel.darker(150));
		}
		return aColours;
	}

	void test_the_old_platform_bar_fails_the_reading(const QString& aRoot)
	{
		// The negative controls: the reading must refuse the old bar. The
		// offscreen platform cannot draw the macOS style's native bezel (it
		// paints nothing there), so the owner's bezel is reproduced by giving
		// the old bar's button family the bezel's light grey under the theme's
		// text, and the style draws it from that.
		for (const auto& colours : built_in_themes()) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(colours.Mode));
			const auto title = QStringLiteral("Check details: 3 failed");

			// As the application's style draws the old bar from the theme.
			// The high-contrast themes are flat, their raised surface their
			// surface, so only Light and Dark can tell the fills apart.
			{
				auto host = QWidget{ };
				profile.apply_to(host);
				host.resize(300, 60);
				auto* const bar = old_disclosure(host, title);
				host.show();
				settle();
				const auto reading = read_bar(*bar);
				const auto label   = "the old bar as " + QApplication::style()->name().toStdString() + " draws it in " + colours.Name;
				std::cout << label << ": fill " << hex(reading.Fill) << ", title " << reading.Text << ":1\n";
				if (colours.Raised != colours.Surface) {
					expect(!passes(reading, colours), label + " fails the disclosure reading");
				}
			}

			// As the owner saw it: the native bezel's grey under the theme's text.
			auto host = QWidget{ };
			profile.apply_to(host);
			host.setPalette(under_native_bezel(host.palette()));
			host.resize(276, 84);
			auto* const bar = old_disclosure(host, title);
			// A checked Windows-style tool button takes its fill from Window,
			// while Cocoa supplies its native bezel. Keep the fixed-grey input
			// on this old-bar control without changing the host's theme surface.
			auto bezel = bar->palette();
			for (const auto group : { QPalette::Active, QPalette::Inactive }) {
				bezel.setColor(group, QPalette::Window, bezel.color(group, QPalette::Button));
			}
			bar->setPalette(bezel);
			host.show();
			settle();
			const auto before = read_bar(*bar);
			const auto label  = "the old bar on the native bezel in " + colours.Name;
			std::cout << label << ": fill " << hex(before.Fill) << ", title " << before.Text << ":1\n";
			expect(colour_near(before.Fill, bezel.color(QPalette::Active, QPalette::Button)), "the old-bar control actually paints the fixed native bezel");
			expect(host.palette().color(QPalette::Active, QPalette::Window) == colours.Surface, "the bezel control leaves the host's theme surface unchanged");
			expect(!passes(before, colours), label + " fails the disclosure reading");
			if (colours.Mode == core::appearance_mode::dark) {
				expect(before.Text < 4.5, "the owner's report: Dark's title on the bezel is below 4.5:1 (" + std::to_string(before.Text) + ")");
			}
			// The shared bar under the same palette falls back to the surface.
			auto* const after = new qt::disclosure_button{ &host };
			after->setText(title);
			after->setChecked(true);
			after->setGeometry(QRect{ QPoint{ 8, bar->geometry().bottom() + 12 }, QSize{ 260, after->sizeHint().height() } });
			after->show();
			settle();
			const auto fixed = read_bar(*after);
			expect(fixed.Text >= 4.5, "the shared bar under the bezel palette in " + colours.Name + " still reads (" + std::to_string(fixed.Text) + ")");
			expect(colour_near(fixed.Fill, qt::disclosure_fill(host.palette(), QPalette::Active)), "the shared bar under the bezel palette in " + colours.Name + " is that palette's readable fill, not " + hex(fixed.Fill));
			if (colours.Mode == core::appearance_mode::dark || colours.Mode == core::appearance_mode::high_contrast_dark) {
				expect(colour_near(fixed.Fill, colours.Surface), "under light text the bezel grey is refused for the surface " + hex(colours.Surface));
			}
			if (!aRoot.isEmpty()) {
				// The card's evidence: the old bar above the shared one, on the theme's surface.
				auto themed = QWidget{ };
				profile.apply_to(themed);
				after->setPalette(themed.palette());
				settle();
				static_cast<void>(host.grab().save(aRoot + QStringLiteral("/before-after-") + QString::fromStdString(colours.Name) + QStringLiteral(".png")));
			}
		}
	}

	void test_a_bar_at_its_size_shows_its_whole_title()
	{
		// A bar laid out at its own size hint draws the same title as a wide
		// one: the hint must not round the title a pixel short, which elided
		// "Pull request and account" at 200%.
		for (const auto& candidate : sites()) {
			auto        host  = candidate.Make();
			auto* const found = candidate.Find(*host);
			if (found == nullptr) {
				continue;
			}
			const auto title  = candidate.Title.isEmpty() ? found->text() : candidate.Title;
			auto       canvas = QWidget{ };
			canvas.setFont(found->font());
			auto* const fitted = new qt::disclosure_button{ &canvas };
			auto* const wide   = new qt::disclosure_button{ &canvas };
			fitted->setText(title);
			wide->setText(title);
			// Neither takes focus when the canvas shows, so neither draws a ring.
			fitted->setFocusPolicy(Qt::NoFocus);
			wide->setFocusPolicy(Qt::NoFocus);
			fitted->setGeometry(QRect{ QPoint{ 0, 0 }, fitted->sizeHint() });
			wide->setGeometry(QRect{ QPoint{ 0, 40 }, fitted->sizeHint() + QSize{ 80, 0 } });
			canvas.resize(fitted->sizeHint().width() + 100, 80);
			canvas.show();
			settle();
			// Nor is either under the pointer, which sits at the canvas's corner.
			fitted->setAttribute(Qt::WA_UnderMouse, false);
			wide->setAttribute(Qt::WA_UnderMouse, false);
			const auto small = fitted->grab().toImage().convertToFormat(QImage::Format_ARGB32);
			const auto large = wide->grab().toImage().convertToFormat(QImage::Format_ARGB32);
			// Compare the title's run, short of the bar's rounded right end.
			const auto scale = small.devicePixelRatio();
			const auto right = static_cast<int>((fitted->width() - 6) * scale);
			auto       same  = true;
			for (auto y = 0; y < small.height() && same; ++y) {
				for (auto x = 0; x < right && same; ++x) {
					same = small.pixelColor(x, y) == large.pixelColor(x, y);
				}
			}
			expect(same, candidate.Name + ": the bar at its size hint shows the whole title \"" + title.toStdString() + "\"");
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
	test_a_bar_at_its_size_shows_its_whole_title();
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
