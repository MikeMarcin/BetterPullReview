#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QMenu>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

// [[BPR-2356]]. With Dark chosen on a light desktop, the Review workbench
// stayed dark while its comparison menu and its Ask menu opened light. A
// popup is a window, and Qt gives a window the application palette, which is
// the desktop's, not its parent's. These courses set the desktop's palette as
// the application palette (which is what the platform does), apply the app
// appearance to a window the way the workbench does, open the real menus and
// read what they wear and what they draw.

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

	enum class desktop : std::uint8_t {
		light,
		dark,
	};

	/// \brief A desktop palette as the platform would publish it: Fusion's
	/// light one, or a dark one shaped like KDE's Breeze Dark (the
	/// reproduction's).
	[[nodiscard]] auto desktop_palette(const desktop aDesktop) -> QPalette
	{
		auto palette = QApplication::style()->standardPalette();
		if (aDesktop == desktop::light) {
			return palette;
		}
		const auto set = [&palette](const QPalette::ColorRole aRole, const QColor aActive, const QColor aDisabled) {
			palette.setColor(QPalette::Active, aRole, aActive);
			palette.setColor(QPalette::Inactive, aRole, aActive);
			palette.setColor(QPalette::Disabled, aRole, aDisabled);
		};
		set(QPalette::Window, QColor{ 0x20, 0x23, 0x26 }, QColor{ 0x20, 0x23, 0x26 });
		set(QPalette::WindowText, QColor{ 0xFC, 0xFC, 0xFC }, QColor{ 0x6E, 0x71, 0x73 });
		set(QPalette::Base, QColor{ 0x14, 0x16, 0x18 }, QColor{ 0x14, 0x16, 0x18 });
		set(QPalette::AlternateBase, QColor{ 0x1D, 0x1F, 0x22 }, QColor{ 0x1D, 0x1F, 0x22 });
		set(QPalette::Text, QColor{ 0xFC, 0xFC, 0xFC }, QColor{ 0x6E, 0x71, 0x73 });
		set(QPalette::Button, QColor{ 0x29, 0x2C, 0x30 }, QColor{ 0x29, 0x2C, 0x30 });
		set(QPalette::ButtonText, QColor{ 0xFC, 0xFC, 0xFC }, QColor{ 0x6E, 0x71, 0x73 });
		set(QPalette::Highlight, QColor{ 0x3D, 0xAE, 0xE9 }, QColor{ 0x3D, 0xAE, 0xE9 });
		set(QPalette::HighlightedText, QColor{ 0xFC, 0xFC, 0xFC }, QColor{ 0xFC, 0xFC, 0xFC });
		set(QPalette::Light, QColor{ 0x40, 0x45, 0x4A }, QColor{ 0x40, 0x45, 0x4A });
		set(QPalette::Midlight, QColor{ 0x33, 0x37, 0x3B }, QColor{ 0x33, 0x37, 0x3B });
		set(QPalette::Mid, QColor{ 0x1F, 0x22, 0x24 }, QColor{ 0x1F, 0x22, 0x24 });
		set(QPalette::Dark, QColor{ 0x14, 0x16, 0x18 }, QColor{ 0x14, 0x16, 0x18 });
		return palette;
	}

	void set_desktop(const desktop aDesktop)
	{
		QApplication::setPalette(desktop_palette(aDesktop));
		settle();
	}

	[[nodiscard]] auto observation_of(const desktop aDesktop) -> core::appearance_observation
	{
		return { .Scheme = aDesktop == desktop::dark ? core::os_color_scheme::dark : core::os_color_scheme::light };
	}

	[[nodiscard]] auto settings_for(const core::appearance_mode aMode) -> application::appearance_settings
	{
		auto settings = qt::default_appearance_settings();
		settings.Mode = aMode;
		return settings;
	}

	[[nodiscard]] auto is_dark(const QColor& aColour) -> bool { return aColour.lightness() < 128; }

	/// \brief The share of a rendered image's pixels on the dark side.
	[[nodiscard]] auto dark_share(const QImage& aImage, const QRect& aArea) -> double
	{
		auto dark  = 0;
		auto total = 0;
		for (auto y = aArea.top(); y <= aArea.bottom(); ++y) {
			for (auto x = aArea.left(); x <= aArea.right(); ++x) {
				++total;
				dark += is_dark(aImage.pixelColor(x, y)) ? 1 : 0;
			}
		}
		return total == 0 ? 0.0 : static_cast<double>(dark) / total;
	}

	/// \brief Whether a rendered area has a pixel within `aTolerance` of
	/// `aColour`.
	[[nodiscard]] auto shows(const QImage& aImage, const QRect& aArea, const QColor& aColour, const int aTolerance = 12) -> bool
	{
		for (auto y = aArea.top(); y <= aArea.bottom(); ++y) {
			for (auto x = aArea.left(); x <= aArea.right(); ++x) {
				const auto pixel = aImage.pixelColor(x, y);
				if (std::abs(pixel.red() - aColour.red()) <= aTolerance && std::abs(pixel.green() - aColour.green()) <= aTolerance
					&& std::abs(pixel.blue() - aColour.blue()) <= aTolerance) {
					return true;
				}
			}
		}
		return false;
	}

	/// \brief The window the workbench's menus open from: an Ask button
	/// with an ordinary, a disabled and a selected row, and the epoch
	/// comparison control whose "Comparing" menu is the revision dropdown.
	struct workbench_stand_in
	{
		QWidget                       Window;
		qt::ask_menu_button*          Ask        = nullptr;
		qt::epoch_comparison_control* Comparison = nullptr;

		workbench_stand_in()
		{
			Window.setObjectName(QStringLiteral("popupPaletteWindow"));
			auto* const layout = new QVBoxLayout{ &Window };
			Ask                = new qt::ask_menu_button{ &Window };
			Ask->set_filler([](QMenu& aMenu) {
				aMenu.addAction(QStringLiteral("Explain this pull request"));
				aMenu.addAction(QStringLiteral("Summarise the latest upload"))->setEnabled(false);
				aMenu.addAction(QStringLiteral("What changed since my review?"));
			});
			Comparison = new qt::epoch_comparison_control{ { }, &Window };
			layout->addWidget(Ask);
			layout->addWidget(Comparison);
			Window.resize(640, 240);
			Window.show();
			settle();
		}
	};

	/// \brief Opens one real menu as its button would, reads it, closes it.
	void read_open_menu(QToolButton& aOpener, QMenu& aMenu, const bool aDark, const std::string& aWhat)
	{
		expect(aMenu.isWindow() && aMenu.parentWidget() != nullptr, aWhat + ": the menu is a popup window with a parent");
		// What QToolButton's InstantPopup does, without its nested event loop.
		emit aMenu.aboutToShow();
		aMenu.popup(aOpener.mapToGlobal(QPoint{ 0, aOpener.height() }));
		settle();
		expect(aMenu.isVisible(), aWhat + ": the menu opened");
		const auto& opener  = aOpener.palette();
		const auto& wearing = aMenu.palette();
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			for (const auto role : { QPalette::Window, QPalette::WindowText, QPalette::Base, QPalette::Text, QPalette::Button, QPalette::ButtonText, QPalette::Highlight,
									 QPalette::HighlightedText, QPalette::Light, QPalette::Mid, QPalette::Dark }) {
				expect(wearing.color(group, role) == opener.color(group, role),
					   aWhat + ": the menu wears its opener's colour for role " + std::to_string(static_cast<int>(role)) + " in group "
						   + std::to_string(static_cast<int>(group)));
			}
		}
		expect(is_dark(wearing.color(QPalette::Window)) == aDark, aWhat + (aDark ? ": the menu's ground is dark" : ": the menu's ground is light"));
		{
			// The profile's disabled ink is its text blended toward its ground.
			const auto ground   = wearing.color(QPalette::Window).lightness();
			const auto ink      = wearing.color(QPalette::Active, QPalette::WindowText).lightness();
			const auto disabled = wearing.color(QPalette::Disabled, QPalette::WindowText).lightness();
			expect(std::min(ground, ink) < disabled && disabled < std::max(ground, ink), aWhat + ": a disabled row's ink lies between the theme's text and ground");
		}

		// The rendered menu, with its first enabled row selected.
		QAction* selected = nullptr;
		for (auto* const action : aMenu.actions()) {
			if (action->isEnabled() && !action->isSeparator()) {
				selected = action;
				break;
			}
		}
		if (selected != nullptr) {
			aMenu.setActiveAction(selected);
			settle();
		}
		const auto image = aMenu.grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto scale = [ratio](const QRect& aRect) {
			return QRect{ static_cast<int>(aRect.x() * ratio), static_cast<int>(aRect.y() * ratio), static_cast<int>(aRect.width() * ratio),
						  static_cast<int>(aRect.height() * ratio) };
		};
		const auto whole = QRect{ QPoint{ 0, 0 }, image.size() };
		const auto share = dark_share(image, whole);
		expect(aDark ? share > 0.6 : share < 0.4, aWhat + ": the rendered menu is " + (aDark ? "dark" : "light") + " (dark share " + std::to_string(share) + ")");
		if (selected != nullptr) {
			expect(shows(image, scale(aMenu.actionGeometry(selected)).intersected(whole), wearing.color(QPalette::Active, QPalette::Highlight)),
				   aWhat + ": the selected row is drawn in the theme's selection colour");
		}
		aMenu.hide();
		settle();
	}

	void read_both_menus(workbench_stand_in& aBench, const bool aDark, const std::string& aWhat)
	{
		read_open_menu(*aBench.Ask, *aBench.Ask->menu(), aDark, aWhat + ", Ask");
		auto* const comparing = aBench.Comparison->comparing_button();
		expect(comparing != nullptr && comparing->menu() != nullptr, aWhat + ": the comparison control has its Comparing menu");
		if (comparing != nullptr && comparing->menu() != nullptr) {
			read_open_menu(*comparing, *comparing->menu(), aDark, aWhat + ", Comparing");
		}
	}

	/// \brief The reported failure, before any appearance is applied: a
	/// window given a dark palette of its own opens its menu in the
	/// application's (offscreen, the light desktop's; on Cocoa, the theme's
	/// menu palette). Proves the readings below can see the defect.
	void test_a_menu_takes_the_desktop_palette_without_the_profile()
	{
		set_desktop(desktop::light);
		auto window = QWidget{ };
		window.setPalette(desktop_palette(desktop::dark));
		auto* const opener = new QToolButton{ &window };
		auto* const menu   = new QMenu{ opener };
		menu->addAction(QStringLiteral("Explain this pull request"));
		window.show();
		settle();
		menu->popup(opener->mapToGlobal(QPoint{ 0, opener->height() }));
		settle();
		const auto wears = menu->palette().color(QPalette::Window);
		expect(wears == QApplication::palette(menu).color(QPalette::Window) && wears != window.palette().color(QPalette::Window),
			   "control: an unfollowed menu wears the application's palette (" + wears.name().toStdString() + "), not its window's ("
				   + window.palette().color(QPalette::Window).name().toStdString() + ")");
		menu->hide();
		settle();
	}

	void test_explicit_modes_win_over_the_opposite_desktop()
	{
		{
			set_desktop(desktop::light);
			auto bench   = workbench_stand_in{ };
			auto profile = qt::appearance_profile{ settings_for(core::appearance_mode::dark), observation_of(desktop::light) };
			profile.apply_to(bench.Window);
			settle();
			expect(is_dark(bench.Window.palette().color(QPalette::Window)), "Dark on a light desktop: the workbench is dark");
			read_both_menus(bench, true, "Dark on a light desktop");
			// The desktop turns dark and light again during the session; the
			// app's Dark stays, and the menus keep it each time they reopen.
			set_desktop(desktop::dark);
			profile.observe(observation_of(desktop::dark));
			profile.apply_to(bench.Window);
			settle();
			read_both_menus(bench, true, "Dark after the desktop turned dark");
			set_desktop(desktop::light);
			profile.observe(observation_of(desktop::light));
			profile.apply_to(bench.Window);
			settle();
			read_both_menus(bench, true, "Dark after the desktop turned light again");
		}
		{
			set_desktop(desktop::dark);
			auto bench   = workbench_stand_in{ };
			auto profile = qt::appearance_profile{ settings_for(core::appearance_mode::light), observation_of(desktop::dark) };
			profile.apply_to(bench.Window);
			settle();
			expect(!is_dark(bench.Window.palette().color(QPalette::Window)), "Light on a dark desktop: the workbench is light");
			read_both_menus(bench, false, "Light on a dark desktop");
			set_desktop(desktop::light);
			profile.observe(observation_of(desktop::light));
			profile.apply_to(bench.Window);
			settle();
			set_desktop(desktop::dark);
			profile.observe(observation_of(desktop::dark));
			profile.apply_to(bench.Window);
			settle();
			read_both_menus(bench, false, "Light after the desktop turned light and dark again");
		}
	}

	void test_system_follows_the_desktop_and_its_switch()
	{
		for (const auto first : { desktop::light, desktop::dark }) {
			const auto second = first == desktop::light ? desktop::dark : desktop::light;
			const auto name   = std::string{ first == desktop::light ? "light" : "dark" };
			set_desktop(first);
			auto bench   = workbench_stand_in{ };
			auto profile = qt::appearance_profile{ settings_for(core::appearance_mode::system), observation_of(first) };
			profile.apply_to(bench.Window);
			settle();
			expect(is_dark(bench.Window.palette().color(QPalette::Window)) == (first == desktop::dark), "System on a " + name + " desktop: the workbench follows it");
			read_both_menus(bench, first == desktop::dark, "System on a " + name + " desktop");
			// What the application does on QStyleHints::colorSchemeChanged:
			// observe, then apply again.
			set_desktop(second);
			profile.observe(observation_of(second));
			profile.apply_to(bench.Window);
			settle();
			expect(is_dark(bench.Window.palette().color(QPalette::Window)) == (second == desktop::dark), "System after the " + name + " desktop switched: the workbench follows it");
			read_both_menus(bench, second == desktop::dark, "System after the " + name + " desktop switched");
		}
	}

	/// \brief A submenu opens from a menu that already wears the theme, and a
	/// popup whose owner chose a palette keeps it.
	void test_submenus_follow_and_owner_palettes_stay()
	{
		set_desktop(desktop::light);
		auto bench   = workbench_stand_in{ };
		auto profile = qt::appearance_profile{ settings_for(core::appearance_mode::dark), observation_of(desktop::light) };
		profile.apply_to(bench.Window);
		settle();
		auto* const outer = new QMenu{ bench.Ask };
		auto* const inner = outer->addMenu(QStringLiteral("More"));
		inner->addAction(QStringLiteral("Explain this file"));
		outer->popup(bench.Ask->mapToGlobal(QPoint{ 0, bench.Ask->height() }));
		settle();
		inner->popup(outer->mapToGlobal(QPoint{ outer->width(), 0 }));
		settle();
		expect(is_dark(outer->palette().color(QPalette::Window)) && is_dark(inner->palette().color(QPalette::Window)), "a menu and its submenu wear the app's Dark");
		inner->hide();
		outer->hide();
		settle();

		auto* const owned = new QMenu{ bench.Ask };
		owned->addAction(QStringLiteral("Accent"));
		auto chosen = desktop_palette(desktop::light);
		chosen.setColor(QPalette::Window, QColor{ 0xFA, 0xF0, 0xD0 });
		owned->setPalette(chosen);
		owned->popup(bench.Ask->mapToGlobal(QPoint{ 0, 0 }));
		settle();
		expect(owned->palette().color(QPalette::Window) == QColor{ 0xFA, 0xF0, 0xD0 }, "a popup whose owner chose its palette keeps it");
		owned->hide();
		settle();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_a_menu_takes_the_desktop_palette_without_the_profile();
	test_explicit_modes_win_over_the_opposite_desktop();
	test_system_follows_the_desktop_and_its_switch();
	test_submenus_follow_and_owner_palettes_stay();
	if (Failures != 0) {
		std::cerr << Failures << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "popup-palette-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " style=" << QApplication::style()->name().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
