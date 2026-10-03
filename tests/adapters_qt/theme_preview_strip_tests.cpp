/// \file
/// \brief Proves RFC-BPR-0034's theme strip: six tiles, a well, a diagonal split.
///
/// \details Criterion C2. The inventory and the order, the sideways-scrolling well
/// and the keyboard selection that scrolls into view, pixel assertions on both
/// halves of both System tiles against the built-in palettes, and the captions per
/// resolution and per customisation count.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/theme_preview_strip.hpp"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using role = core::appearance_role;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	void show(qt::theme_preview_strip& aStrip)
	{
		aStrip.resize(aStrip.sizeHint());
		aStrip.show();
		settle();
	}

	/// \brief One point inside a tile's frame, in the grabbed image's pixels.
	///
	/// \details The frame is the top `theme_preview_tile_height` points of the
	/// tile, inset by the ring's own pixel. Offsets are given in points and scaled
	/// by the raster's device pixel ratio, so the probe follows the scale the test
	/// was run at rather than assuming one.
	[[nodiscard]] auto probe(
		const QImage& aImage,
		const QRect&  aTile,
		const int     aX,
		const int     aY) -> QColor
	{
		const auto ratio = aImage.devicePixelRatio();
		const auto x     = static_cast<int>(
			static_cast<double>(aTile.left() + 1 + aX) * ratio);
		const auto y = static_cast<int>(
			static_cast<double>(aTile.top() + 1 + aY) * ratio);
		expect(
			x >= 0 && y >= 0 && x < aImage.width() && y < aImage.height(),
			"the probe lands inside the raster that was grabbed");
		return aImage.pixelColor(x, y);
	}

	/// The two probes sit beneath the snippet's rows, where the tile is nothing but
	/// its own code background: one well inside the top-left triangle and one well
	/// inside the bottom-right one.
	constexpr int TopLeftProbeX     = 4;
	constexpr int BottomRightProbeX = 224;
	constexpr int ProbeY            = 116;

	[[nodiscard]] auto builtin_code_background(const core::appearance_variant aVariant)
		-> QColor
	{
		return qt::to_presentation_color(
			qt::builtin_appearance_palette(aVariant)[core::index_of(
				role::code_background)]);
	}

	void press(qt::theme_preview_strip& aStrip, const Qt::Key aKey)
	{
		auto event = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aStrip, &event);
		settle();
	}

	void test_the_inventory_is_one_tile_per_mode_in_the_accepted_order()
	{
		auto strip = qt::theme_preview_strip{ };
		expect(
			qt::theme_preview_count == core::appearance_mode_count,
			"there is one tile per mode the core declares, so a seventh mode "
			"cannot appear without a tile");
		const auto& cards = strip.cards();
		expect(cards.size() == 6U, "six tiles");
		const auto expected = std::array{
			core::appearance_mode::system,
			core::appearance_mode::light,
			core::appearance_mode::dark,
			core::appearance_mode::high_contrast_system,
			core::appearance_mode::high_contrast_light,
			core::appearance_mode::high_contrast_dark,
		};
		for (auto index = std::size_t{ 0U }; index < expected.size(); ++index) {
			expect(
				cards[index].Mode == expected[index] && !cards[index].Name.isEmpty(),
				"the tiles are System, Light, Dark, High contrast system, High "
				"contrast light and High contrast dark, in that order");
		}
		expect(
			cards[0].Rect.width() == qt::theme_preview_tile_width
				&& cards[0].Rect.height() > qt::theme_preview_tile_height,
			"a tile is the accepted 232 point frame plus room for its name and "
			"caption");
		for (auto index = std::size_t{ 1U }; index < cards.size(); ++index) {
			expect(
				cards[index].Rect.left() > cards[index - 1U].Rect.right()
					&& cards[index].Rect.top() == cards[0].Rect.top(),
				"the tiles lie in one sideways row with a gap, never wrapped");
		}
	}

	void test_the_well_scrolls_sideways_without_widening_the_page()
	{
		auto strip = qt::theme_preview_strip{ };
		show(strip);
		auto* const well = strip.well();
		expect(well != nullptr, "the tiles sit in a well");
		expect(
			well->horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded
				&& well->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
			"the well scrolls sideways and never downwards");
		expect(
			well->horizontalScrollBar() != nullptr
				&& well->horizontalScrollBar()->maximum() > 0,
			"at the strip's own preferred width the six tiles do not all fit, so "
			"the well really does scroll");
		const auto shown = strip.width() - 2 * well->frameWidth();
		const auto tile  = qt::theme_preview_tile_width;
		expect(
			shown >= 3 * tile && shown < 6 * tile,
			"three tiles and part of a fourth are what the preferred width shows");
		expect(
			strip.minimumSizeHint().width() < 2 * tile
				&& strip.minimumSizeHint().width()
					< strip.cards().back().Rect.right(),
			"the strip's minimum is about one tile, so a bounded Settings page "
			"never has to widen or scroll sideways for it");
	}

	void test_keyboard_selection_moves_and_scrolls_into_view()
	{
		auto strip      = qt::theme_preview_strip{ };
		auto dispatched = std::vector<core::appearance_mode>{ };
		strip.set_dispatch([&dispatched](const core::appearance_mode aMode) {
			dispatched.push_back(aMode);
		});
		show(strip);
		strip.setFocus();
		expect(
			strip.current() == core::appearance_mode::system,
			"the strip starts on the mode it was told about, which is System until "
			"an adoption says otherwise");
		const auto before = strip.statistics();
		press(strip, Qt::Key_Right);
		press(strip, Qt::Key_Right);
		press(strip, Qt::Key_Right);
		expect(
			strip.current() == core::appearance_mode::high_contrast_system
				&& dispatched.size() == 3U
				&& dispatched.back() == core::appearance_mode::high_contrast_system,
			"Right moves the selection one tile at a time and reports each one");
		expect(
			strip.statistics().ScrollsIntoView > before.ScrollsIntoView
				&& strip.well()->horizontalScrollBar()->value() > 0,
			"choosing a tile that was out of sight scrolled the well");
		expect(
			strip.tile_rect_in_strip(3U).size() == strip.cards()[3].Rect.size(),
			"the chosen tile is wholly in view afterwards, which is what scrolling "
			"it into view has to mean");

		press(strip, Qt::Key_Left);
		expect(
			strip.current() == core::appearance_mode::dark,
			"Left moves back");
		for (auto step = 0; step < 3; ++step) {
			press(strip, Qt::Key_Left);
		}
		expect(
			strip.current() == core::appearance_mode::high_contrast_dark
				&& strip.tile_rect_in_strip(5U).size()
					== strip.cards()[5].Rect.size(),
			"Left wraps to the last tile and scrolls it into view too");

		const auto chosen = dispatched.size();
		press(strip, Qt::Key_Down);
		expect(
			dispatched.size() == chosen,
			"a key the strip does not own dispatches nothing");
	}

	void test_a_press_chooses_the_tile_under_it()
	{
		auto strip      = qt::theme_preview_strip{ };
		auto dispatched = std::optional<core::appearance_mode>{ };
		strip.set_dispatch([&dispatched](const core::appearance_mode aMode) {
			dispatched = aMode;
		});
		show(strip);
		const auto dark  = strip.cards()[2].Rect.center();
		auto       event = QMouseEvent{ QEvent::MouseButtonPress,
										QPointF{ dark },
										QPointF{ dark },
										Qt::LeftButton,
										Qt::LeftButton,
										Qt::NoModifier };
		QApplication::sendEvent(&strip, &event);
		settle();
		expect(
			dispatched == std::optional{ core::appearance_mode::dark }
				&& strip.current() == core::appearance_mode::dark,
			"a press on the Dark tile chooses Dark");
		expect(
			strip.hit_test(strip.cards()[4].Rect.center()) == std::optional{ 4U }
				&& !strip.hit_test(QPoint{ -20, -20 }).has_value(),
			"the strip knows which tile covers a point of itself and which points "
			"cover none");
	}

	void test_captions_name_the_resolution_and_the_customised_counts()
	{
		auto strip = qt::theme_preview_strip{ };
		expect(
			!strip.system_variant().has_value()
				&& strip.cards()[0].Note.isEmpty()
				&& strip.cards()[3].Note.isEmpty(),
			"before the composition says what the desktop resolves, the System "
			"tiles say nothing rather than guessing a variant");

		strip.set_system_variant(core::appearance_variant::dark);
		expect(
			strip.cards()[0].Note == QStringLiteral("dark now")
				&& strip.cards()[3].Note == QStringLiteral("dark now"),
			"both desktop-following tiles caption the half in effect");
		strip.set_system_variant(core::appearance_variant::light);
		expect(
			strip.cards()[0].Note == QStringLiteral("light now"),
			"and follow it when it changes");

		auto counts                                                       = std::array<std::size_t, core::appearance_variant_count>{ };
		counts[static_cast<std::size_t>(core::appearance_variant::dark)]  = 2U;
		counts[static_cast<std::size_t>(core::appearance_variant::light)] = 1U;
		strip.set_customised_counts(counts);
		expect(
			strip.cards()[2].Note == QStringLiteral("2 colours customised"),
			"a theme with customised colours captions how many");
		expect(
			strip.cards()[1].Note == QStringLiteral("1 colour customised"),
			"one colour reads as one colour");
		expect(
			strip.cards()[4].Note.isEmpty() && strip.cards()[5].Note.isEmpty(),
			"a theme that customises nothing captions nothing");
		expect(
			strip.cards()[0].Note.contains(QStringLiteral("light now"))
				&& strip.cards()[0].Note.contains(
					QStringLiteral("1 colour customised")),
			"the System tile reports the resolved half and that half's own count, "
			"which is the count of the theme it is actually showing");
		expect(
			strip.accessibleDescription().contains(QStringLiteral("Left"))
				&& strip.accessibleDescription().contains(
					strip.cards()[0].Name),
			"the strip announces how to move and which tile is selected");
	}

	/// \brief [[BPR-1107]]. The tiles draw white space as the typography's
	/// choice says, through the review panes' own painter.
	void test_tiles_draw_white_space_as_the_typography_chooses()
	{
		auto strip = qt::theme_preview_strip{ };
		show(strip);
		const auto drawn = [&strip](const core::white_space_drawing aDrawing) {
			strip.set_white_space(aDrawing);
			const auto before = strip.statistics().WhiteSpacePainted;
			static_cast<void>(strip.grab());
			return strip.statistics().WhiteSpacePainted - before;
		};
		expect(drawn(core::white_space_drawing::none) == 0U, "none draws no white space on any tile");
		expect(drawn(core::white_space_drawing::all) > 0U, "all draws the snippet's spaces on the tiles");
		expect(drawn(core::white_space_drawing::selection) == 0U, "the tile snippet selects nothing, so Selection draws nothing");
	}

	/// \brief C2's pixel comparison: both halves of both System tiles.
	void test_both_system_tiles_split_diagonally_by_palette()
	{
		auto strip = qt::theme_preview_strip{ };
		show(strip);
		const auto before = strip.statistics();
		const auto image  = strip.grab().toImage();
		const auto after  = strip.statistics();
		expect(
			after.TilesPainted - before.TilesPainted == 6U,
			"one repaint paints six tiles");
		expect(
			after.DiagonalTilesPainted - before.DiagonalTilesPainted == 2U,
			"exactly two of them are split, which is the two desktop-following "
			"modes and no other");

		const auto system = strip.tile_rect_in_strip(0U);
		expect(
			system.size() == strip.cards()[0].Rect.size(),
			"the System tile is wholly in view at the preferred width");
		expect(
			probe(image, system, TopLeftProbeX, ProbeY)
				== builtin_code_background(core::appearance_variant::light),
			"the System tile's top-left triangle is painted in the built-in Light "
			"palette");
		expect(
			probe(image, system, BottomRightProbeX, ProbeY)
				== builtin_code_background(core::appearance_variant::dark),
			"and its bottom-right triangle in the built-in Dark palette");

		const auto light = strip.tile_rect_in_strip(1U);
		expect(
			probe(image, light, TopLeftProbeX, ProbeY)
					== builtin_code_background(core::appearance_variant::light)
				&& probe(image, light, BottomRightProbeX, ProbeY)
					== builtin_code_background(core::appearance_variant::light),
			"a Light tile is Light in both corners, so the clip is applied to the "
			"System tiles alone");

		// The high-contrast System tile is off screen at the preferred width, so
		// reaching it is the same gesture a reviewer makes.
		strip.setFocus();
		for (auto step = 0; step < 3; ++step) {
			press(strip, Qt::Key_Right);
		}
		const auto scrolled     = strip.grab().toImage();
		const auto highContrast = strip.tile_rect_in_strip(3U);
		expect(
			highContrast.size() == strip.cards()[3].Rect.size(),
			"the high-contrast System tile is wholly in view once chosen");
		expect(
			probe(scrolled, highContrast, TopLeftProbeX, ProbeY)
				== builtin_code_background(
					core::appearance_variant::high_contrast_light),
			"the high-contrast System tile's top-left triangle is the built-in "
			"High Contrast Light palette");
		expect(
			probe(scrolled, highContrast, BottomRightProbeX, ProbeY)
				== builtin_code_background(
					core::appearance_variant::high_contrast_dark),
			"and its bottom-right triangle the built-in High Contrast Dark one");
	}

	void test_tiles_follow_the_supplied_theme_palettes_and_font()
	{
		auto strip = qt::theme_preview_strip{ };
		show(strip);
		const auto dark = strip.tile_rect_in_strip(2U);
		expect(
			dark.size() == strip.cards()[2].Rect.size(),
			"the Dark tile is in view");
		expect(
			probe(strip.grab().toImage(), dark, BottomRightProbeX, ProbeY)
				== builtin_code_background(core::appearance_variant::dark),
			"until the composition supplies palettes a tile shows its built-in "
			"profile, which is the truth about an installation that customised "
			"nothing");

		auto palettes
			= std::array<core::appearance_palette, core::appearance_variant_count>{ };
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_variant_count;
			 ++index) {
			palettes[index] = qt::builtin_appearance_palette(
				static_cast<core::appearance_variant>(index));
		}
		palettes[static_cast<std::size_t>(core::appearance_variant::dark)]
				[core::index_of(role::code_background)]
			= core::appearance_color{ 3U, 5U, 7U, 255U };
		strip.set_theme_palettes(palettes);
		settle();
		expect(
			probe(strip.grab().toImage(), dark, BottomRightProbeX, ProbeY)
				== QColor{ 3, 5, 7 },
			"a theme's own overrides reach its tile, so the tile is that theme "
			"rather than that theme's built-in profile");

		// The family is supplied too. Setting it to the default typography's code
		// font must change nothing, because that is already what an unsupplied
		// strip draws with: the family is never named in the widget.
		const auto beforeFont = strip.grab().toImage();
		auto       fresh      = qt::to_presentation_font(qt::default_typography_settings().Code);
		fresh.setStyleHint(QFont::Monospace);
		strip.set_code_font(fresh);
		settle();
		expect(
			strip.grab().toImage() == beforeFont,
			"the default code font is the fresh typography's code font, so no "
			"family is hard-coded in the strip");
	}

	void test_adoption_marks_the_tile_without_dispatching_or_scrolling()
	{
		auto strip      = qt::theme_preview_strip{ };
		auto dispatched = 0U;
		strip.set_dispatch([&dispatched](core::appearance_mode) { ++dispatched; });
		show(strip);
		const auto position = strip.well()->horizontalScrollBar()->value();
		strip.set_current(core::appearance_mode::high_contrast_dark);
		settle();
		expect(
			strip.current() == core::appearance_mode::high_contrast_dark
				&& dispatched == 0U,
			"showing what the host published dispatches nothing");
		expect(
			strip.well()->horizontalScrollBar()->value() == position,
			"and does not move the page under the reviewer, which only a gesture "
			"does");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewThemePreviewStripTests"));
	test_the_inventory_is_one_tile_per_mode_in_the_accepted_order();
	test_the_well_scrolls_sideways_without_widening_the_page();
	test_keyboard_selection_moves_and_scrolls_into_view();
	test_a_press_chooses_the_tile_under_it();
	test_captions_name_the_resolution_and_the_customised_counts();
	test_tiles_draw_white_space_as_the_typography_chooses();
	test_both_system_tiles_split_diagonally_by_palette();
	test_tiles_follow_the_supplied_theme_palettes_and_font();
	test_adoption_marks_the_tile_without_dispatching_or_scrolling();
	std::cout << "theme-preview-strip-tests:pass\n";
	return EXIT_SUCCESS;
}
