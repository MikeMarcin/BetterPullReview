#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto expect_equal(
		const std::uint32_t    aMeasured,
		const std::uint32_t    aExpected,
		const std::string_view aMessage) -> void
	{
		if (aMeasured != aExpected) {
			std::cerr << "FAILED: " << aMessage << " (measured " << aMeasured
					  << ", expected " << aExpected << ")\n";
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief Builds one opaque colour from a packed 0xRRGGBB value.
	[[nodiscard]] constexpr auto srgb(const std::uint32_t aValue) noexcept
		-> appearance_color
	{
		constexpr auto channelMask = std::uint32_t{ 0xFFU };
		return appearance_color{
			static_cast<std::uint8_t>((aValue >> 16U) & channelMask),
			static_cast<std::uint8_t>((aValue >> 8U) & channelMask),
			static_cast<std::uint8_t>(aValue & channelMask),
			255U,
		};
	}

	constexpr auto black = srgb(0x000000);
	constexpr auto white = srgb(0xFFFFFF);

	/// \brief Reports whether a role is read as a background of some pair.
	///
	/// \details A wash counts. It is composited over the named background
	/// before the ratio is taken, so a palette that made it a foreground colour
	/// would put a dark layer over every canvas background and no pair could
	/// reach the maximum through it.
	[[nodiscard]] auto is_checked_background(const appearance_role aRole) -> bool
	{
		return std::ranges::any_of(
			required_contrast_pairs(),
			[aRole](const contrast_pair& aPair) {
				return aPair.Background == aRole || aPair.Wash == std::optional{ aRole };
			});
	}

	/// \brief Builds a palette in which every required pair reaches 21.00:1.
	///
	/// \details Every role that any required pair names as a background becomes
	/// white and every other role becomes black. No role is both, so the palette
	/// is well defined and no pair can fail for an incidental reason while a test
	/// is looking at the one pair it changed.
	[[nodiscard]] auto maximal_contrast_palette() -> appearance_palette
	{
		auto palette = appearance_palette{ };
		for (auto index = std::size_t{ 0U }; index < appearance_role_count; ++index) {
			const auto role = static_cast<appearance_role>(static_cast<std::uint8_t>(index));
			palette[index]  = is_checked_background(role) ? white : black;
		}

		return palette;
	}

	[[nodiscard]] auto with_role(
		appearance_palette     aPalette,
		const appearance_role  aRole,
		const appearance_color aColor) -> appearance_palette
	{
		aPalette[index_of(aRole)] = aColor;
		return aPalette;
	}

	auto test_the_ratio_matches_known_values() -> void
	{
		expect_equal(contrast_ratio_hundredths(black, white), 2100U,
					 "black against white is the maximum 21.00:1");
		expect_equal(contrast_ratio_hundredths(white, black), 2100U,
					 "the ratio is symmetric in its arguments");
		expect_equal(contrast_ratio_hundredths(black, black), 100U,
					 "black against itself is 1.00:1");
		expect_equal(contrast_ratio_hundredths(white, white), 100U,
					 "white against itself is 1.00:1");
		expect_equal(contrast_ratio_hundredths(srgb(0x808080), srgb(0x808080)), 100U,
					 "any colour against itself is 1.00:1");

		// The smallest neutral grey that still clears 4.50:1 against white,
		// which is the value the criterion is usually quoted with.
		expect_equal(contrast_ratio_hundredths(srgb(0x767676), white), 454U,
					 "the canonical 4.54:1 grey measures 4.54:1");

		// These three pin the luminance weights individually. An implementation
		// that averaged the channels, or that transposed two weights, would
		// report the same number for at least two of them.
		expect_equal(contrast_ratio_hundredths(srgb(0xFF0000), white), 399U,
					 "pure red against white measures 3.99:1");
		expect_equal(contrast_ratio_hundredths(srgb(0x00FF00), white), 137U,
					 "pure green against white measures 1.37:1");
		expect_equal(contrast_ratio_hundredths(srgb(0x0000FF), white), 859U,
					 "pure blue against white measures 8.59:1");
		expect(contrast_ratio_hundredths(srgb(0x00FF00), white)
					   < contrast_ratio_hundredths(srgb(0xFF0000), white)
				   && contrast_ratio_hundredths(srgb(0xFF0000), white)
					   < contrast_ratio_hundredths(srgb(0x0000FF), white),
			   "the three primaries are ordered green, red, blue against white");

		// Linearisation is not a linear ramp: half of the encoded range is far
		// below half of the light.
		expect(contrast_ratio_hundredths(srgb(0x808080), white) > 350U
				   && contrast_ratio_hundredths(srgb(0x808080), white) < 450U,
			   "mid grey against white is near 4:1, not near 2:1");

		expect_equal(contrast_ratio_hundredths(appearance_color{ 0x0AU, 0x7BU, 0x54U, 0U },
											   appearance_color{ 0xB6U, 0xD7U, 0xFFU, 32U }),
					 contrast_ratio_hundredths(srgb(0x0A7B54), srgb(0xB6D7FF)),
					 "alpha does not change the measured ratio");
	}

	auto test_the_minimums_are_published_once() -> void
	{
		expect_equal(text_contrast_minimum_hundredths, 450U,
					 "the published text minimum is 4.50:1");
		expect_equal(high_contrast_text_minimum_hundredths, 700U,
					 "the published high-contrast text minimum is 7.00:1");
		expect_equal(indicator_contrast_minimum_hundredths, 300U,
					 "the published indicator minimum is 3.00:1");

		constexpr auto everyVariant = std::array{
			appearance_variant::light,
			appearance_variant::dark,
			appearance_variant::high_contrast_light,
			appearance_variant::high_contrast_dark,
		};
		for (const auto variant : everyVariant) {
			const auto expectedText = high_contrast(variant)
				? high_contrast_text_minimum_hundredths
				: text_contrast_minimum_hundredths;
			expect_equal(minimum_contrast_hundredths(contrast_kind::text, variant),
						 expectedText,
						 "a high-contrast variant is held to the enhanced text minimum");
			expect_equal(minimum_contrast_hundredths(contrast_kind::indicator, variant),
						 indicator_contrast_minimum_hundredths,
						 "the indicator minimum is the same in every variant");
		}
	}

	auto test_every_checked_pair_is_named() -> void
	{
		const auto pairs = required_contrast_pairs();
		expect(!pairs.empty(), "the checked pairs are enumerated");

		auto sorted = std::vector<contrast_pair>(pairs.begin(), pairs.end());
		std::ranges::sort(sorted);
		expect(std::ranges::adjacent_find(sorted) == sorted.end(),
			   "no pair is checked twice");

		for (const auto& pair : pairs) {
			expect(pair.Foreground != pair.Background,
				   "no pair checks a role against itself");
			expect(index_of(pair.Foreground) < appearance_role_count
					   && index_of(pair.Background) < appearance_role_count,
				   "every checked role is inside the closed vocabulary");
		}

		const auto contains = [pairs](const appearance_role aForeground,
									  const appearance_role aBackground,
									  const contrast_kind   aKind) {
			return std::ranges::find(
					   pairs,
					   contrast_pair{ .Foreground = aForeground,
									  .Background = aBackground,
									  .Kind       = aKind })
				!= pairs.end();
		};

		expect(contains(appearance_role::application_text,
						appearance_role::application_surface,
						contrast_kind::text),
			   "application text on the application surface is checked");
		expect(contains(appearance_role::code_line_number_text,
						appearance_role::code_gutter_background,
						contrast_kind::text),
			   "line-number text on the gutter is checked");
		expect(contains(appearance_role::diff_hunk_header_text,
						appearance_role::diff_hunk_header_background,
						contrast_kind::text),
			   "hunk-header text on its own background is checked");
		expect(contains(appearance_role::syntax_normal,
						appearance_role::code_background,
						contrast_kind::text),
			   "code text on the code background is checked");
		expect(contains(appearance_role::syntax_comment,
						appearance_role::diff_added_background,
						contrast_kind::text)
				   && contains(appearance_role::syntax_comment,
							   appearance_role::diff_removed_background,
							   contrast_kind::text)
				   && contains(appearance_role::syntax_comment,
							   appearance_role::diff_moved_background,
							   contrast_kind::text),
			   "syntax colours are checked over each diff row fill");
		expect(contains(appearance_role::state_defect,
						appearance_role::application_surface,
						contrast_kind::indicator),
			   "a review-state mark is checked against the surface it sits on");

		// Every syntax role reaches every code-canvas background, and the diff
		// row fills are among those backgrounds.
		constexpr auto canvasBackgrounds = std::array{
			appearance_role::code_background,
			appearance_role::code_current_line_background,
			appearance_role::diff_added_background,
			appearance_role::diff_removed_background,
			appearance_role::diff_intraline_added_background,
			appearance_role::diff_intraline_removed_background,
			appearance_role::diff_moved_background,
		};
		for (const auto background : canvasBackgrounds) {
			// Counted by kind. A canvas background can also carry marks drawn
			// over it - the change-group boundary rule is one - and those are
			// held to the indicator ratio, not the text one. Counting every
			// pair regardless of kind would make this assertion about how many
			// obligations a background happens to have rather than about
			// syntax coverage, which is what it is for.
			// The two intraline emphasis fills carry `emphasis_text` rather
			// than `text`, because BPR-0320 gave them their own published
			// floor. They are still syntax-text pairs and must still be
			// covered, so both kinds count here and the kind itself is
			// asserted separately below.
			const auto expectedKind
				= background == appearance_role::diff_intraline_added_background
					|| background == appearance_role::diff_intraline_removed_background
				? contrast_kind::emphasis_text
				: contrast_kind::text;
			const auto count = std::ranges::count_if(
				pairs,
				[background, expectedKind](const contrast_pair& aPair) {
					return aPair.Background == background
						&& aPair.Kind == expectedKind
						&& !aPair.Wash.has_value();
				});
			expect_equal(static_cast<std::uint32_t>(count), 28U,
						 "every syntax role is checked over every canvas background");
			expect(std::ranges::none_of(
					   pairs,
					   [background, expectedKind](const contrast_pair& aPair) {
						   return aPair.Background == background
							   && (aPair.Kind == contrast_kind::text
								   || aPair.Kind == contrast_kind::emphasis_text)
							   && aPair.Kind != expectedKind;
					   }),
				   "a background holds all of its text pairs to one floor");

			// And again through the selection wash, for the backgrounds a wash
			// can land on. Selecting a line must not be what makes it
			// unreadable, which checking the bare background cannot see.
			// code_current_line_background is excluded: it is the item-view
			// alternate base, which no pane paints and which views draw a
			// solid selection over rather than a wash.
			const auto washable
				= background != appearance_role::code_current_line_background;
			const auto washed = std::ranges::count_if(
				pairs,
				[background, expectedKind](const contrast_pair& aPair) {
					return aPair.Background == background
						&& aPair.Kind == expectedKind
						&& aPair.Wash
						== std::optional{
							   appearance_role::code_selection_background
						   };
				});
			expect_equal(
				static_cast<std::uint32_t>(washed), washable ? 28U : 0U,
				"every syntax role is checked over every washable canvas "
				"background with the selection wash on top");
		}

		// Against the application surface, which is where the state marks are
		// drawn. The change-group boundary rule is also an indicator but is
		// drawn on the code canvas, so it is counted separately below rather
		// than inflating the state total.
		const auto indicators = std::ranges::count_if(
			pairs,
			[](const contrast_pair& aPair) {
				return aPair.Kind == contrast_kind::indicator
					&& aPair.Background == appearance_role::application_surface;
			});
		expect_equal(static_cast<std::uint32_t>(indicators), 6U,
					 "all six review-state roles are checked as indicators");

		const auto canvasIndicators = std::ranges::count_if(
			pairs,
			[](const contrast_pair& aPair) {
				return aPair.Kind == contrast_kind::indicator
					&& aPair.Background == appearance_role::code_background;
			});
		expect_equal(
			static_cast<std::uint32_t>(canvasIndicators), 1U,
			"the change-group boundary rule is held to the indicator ratio "
			"against the code surface it is drawn on");

		expect_equal(static_cast<std::uint32_t>(pairs.size()), 374U,
					 "the checked set is the three fixed pairs, the boundary rule, "
					 "28 syntax roles over "
					 "bare and washed canvas backgrounds, and six state indicators");
	}

	auto test_a_conforming_palette_reports_nothing() -> void
	{
		const auto palette = maximal_contrast_palette();
		for (const auto variant : std::array{ appearance_variant::light,
											  appearance_variant::high_contrast_dark }) {
			expect(failing_contrast(palette, variant).empty(),
				   "a palette at 21.00:1 fails no pair in any variant");

			const auto findings = measure_required_contrast(palette, variant);
			expect_equal(static_cast<std::uint32_t>(findings.size()),
						 static_cast<std::uint32_t>(required_contrast_pairs().size()),
						 "every required pair produces one finding");
			expect(std::ranges::all_of(
					   findings,
					   [](const contrast_finding& aFinding) {
						   return aFinding.Meets && aFinding.RatioHundredths == 2100U;
					   }),
				   "each finding carries the measured 21.00:1 and passes");
		}
	}

	auto test_a_failing_pair_is_reported_with_its_ratio() -> void
	{
		// Two colours the built-in light profile actually assigns: its string
		// colour over the blue it used to fill a selected line with. That fill
		// is now an added-line band, because the selection is no longer a
		// background in its own right - it is a wash laid over one.
		constexpr auto stringColor = srgb(0x0A7B54);
		constexpr auto bandColor   = srgb(0xB6D7FF);
		expect_equal(contrast_ratio_hundredths(stringColor, bandColor), 356U,
					 "the sample pair measures 3.56:1");

		const auto palette = with_role(
			with_role(maximal_contrast_palette(),
					  appearance_role::syntax_string,
					  stringColor),
			appearance_role::diff_added_background,
			bandColor);

		// Two, not one: the band is checked bare and again through the
		// selection wash, because selecting a line must not be what makes it
		// unreadable. Both fail here, and the pair is what distinguishes them.
		const auto failures = failing_contrast(palette, appearance_variant::light);
		expect_equal(static_cast<std::uint32_t>(failures.size()), 2U,
					 "exactly the pairs below the minimum are reported, bare and "
					 "washed");

		const auto bare = std::ranges::find_if(
			failures, [](const contrast_finding& aFinding) {
				return !aFinding.Pair.Wash.has_value();
			});
		const auto washed = std::ranges::find_if(
			failures, [](const contrast_finding& aFinding) {
				return aFinding.Pair.Wash.has_value();
			});
		expect(bare != failures.end() && washed != failures.end(),
			   "one failure is the bare band and the other is the washed one");

		for (const auto* finding : { &*bare, &*washed }) {
			expect(finding->Pair.Foreground == appearance_role::syntax_string
					   && finding->Pair.Background
						   == appearance_role::diff_added_background,
				   "the failure names the pair that failed");
			expect_equal(finding->MinimumHundredths, 450U,
						 "the failure carries the minimum it was measured against");
			expect(!finding->Meets, "the failure is reported as failing");
		}

		expect_equal(bare->RatioHundredths, 356U,
					 "the bare failure carries the measured ratio, not just a "
					 "verdict");
		expect(
			washed->Pair.Wash
				== std::optional{ appearance_role::code_selection_background },
			"the washed failure names the wash it was measured through");

		// The wash in this palette is white, so compositing it over the band
		// can only move the background towards white and away from the dark
		// string colour. A washed ratio that did not exceed the bare one would
		// mean the wash was not applied at all.
		expect(washed->RatioHundredths > bare->RatioHundredths,
			   "measuring through the wash uses the composited background");

		const auto measured = measure_contrast(
			palette,
			contrast_pair{ .Foreground = appearance_role::syntax_string,
						   .Background = appearance_role::diff_added_background,
						   .Kind       = contrast_kind::text },
			appearance_variant::light);
		expect(measured == *bare,
			   "measuring one pair agrees with the sweep over every pair");
	}

	auto test_the_high_contrast_minimum_is_the_stricter_one() -> void
	{
		// 5.00:1 clears the standard minimum and misses the enhanced one, so the
		// same palette must pass in a standard variant and fail in a
		// high-contrast variant.
		constexpr auto standardText = srgb(0x6A6A6A);
		const auto     palette      = with_role(maximal_contrast_palette(),
												appearance_role::application_text,
												standardText);
		const auto     ratio        = contrast_ratio_hundredths(standardText, white);
		expect(ratio >= text_contrast_minimum_hundredths
				   && ratio < high_contrast_text_minimum_hundredths,
			   "the sample text colour sits between the two published minimums");

		expect(failing_contrast(palette, appearance_variant::light).empty(),
			   "the sample palette meets the standard minimum");

		const auto strict = failing_contrast(palette, appearance_variant::high_contrast_light);
		expect_equal(static_cast<std::uint32_t>(strict.size()), 1U,
					 "the same palette misses the high-contrast minimum");
		expect(strict.front().Pair.Foreground == appearance_role::application_text
				   && strict.front().RatioHundredths == ratio
				   && strict.front().MinimumHundredths
					   == high_contrast_text_minimum_hundredths,
			   "the high-contrast failure reports the same ratio against 7.00:1");
	}

	auto test_an_indicator_is_held_to_the_indicator_minimum() -> void
	{
		// 3.10:1 clears the indicator minimum and misses the text minimum.
		constexpr auto markColor = srgb(0x949494);
		const auto     ratio     = contrast_ratio_hundredths(markColor, white);
		expect(ratio >= indicator_contrast_minimum_hundredths
				   && ratio < text_contrast_minimum_hundredths,
			   "the sample mark colour sits between the indicator and text minimums");

		const auto palette = with_role(maximal_contrast_palette(),
									   appearance_role::state_defect,
									   markColor);
		expect(failing_contrast(palette, appearance_variant::light).empty(),
			   "a mark that clears 3.00:1 is not judged as text");

		const auto asText = with_role(maximal_contrast_palette(),
									  appearance_role::application_text,
									  markColor);
		expect_equal(
			static_cast<std::uint32_t>(
				failing_contrast(asText, appearance_variant::light).size()),
			1U,
			"the same colour used as text does fail the text minimum");
	}

	auto test_simulation_keeps_what_a_dichromat_keeps() -> void
	{
		constexpr auto everyVision = std::array{
			color_vision::typical,
			color_vision::protanopia,
			color_vision::deuteranopia,
		};

		for (const auto vision : everyVision) {
			for (const auto neutral : std::array{ black, srgb(0x808080), white }) {
				expect(simulate_color_vision(neutral, vision) == neutral,
					   "a dichromat projection leaves neutral colours untouched");
			}
			expect(simulate_color_vision(srgb(0x0000FF), vision).Blue == 255U,
				   "the blue axis survives both red-green deficiencies");
		}

		expect(simulate_color_vision(srgb(0x1F2328), color_vision::typical)
				   == srgb(0x1F2328),
			   "typical vision is the identity");
		expect(simulate_color_vision(srgb(0xCF222E), color_vision::deuteranopia)
				   != srgb(0xCF222E),
			   "a saturated red is not left alone by deuteranopia");
		expect(simulate_color_vision(appearance_color{ 0xCFU, 0x22U, 0x2EU, 64U },
									 color_vision::protanopia)
					   .Alpha
				   == 64U,
			   "simulation preserves alpha");

		// Red and green are the textbook case: far apart for typical vision,
		// nearly the same colour for a deuteranope.
		expect(color_difference_tenths(srgb(0xFF0000), srgb(0x00FF00), color_vision::typical)
				   > 1000U,
			   "pure red and pure green are far apart for typical vision");
		expect(color_difference_tenths(srgb(0xFF0000), srgb(0x00FF00),
									   color_vision::deuteranopia)
				   < 300U,
			   "deuteranopia collapses most of that distance");
		expect(color_difference_tenths(srgb(0xFF0000), srgb(0x00FF00),
									   color_vision::deuteranopia)
				   < color_difference_tenths(srgb(0xFF0000), srgb(0x00FF00),
											 color_vision::protanopia),
			   "the two deficiencies are modelled differently");
	}

	auto test_the_difference_matches_known_values() -> void
	{
		expect_equal(color_difference_tenths(black, white, color_vision::typical), 1000U,
					 "black against white is 100.0 units apart");
		expect_equal(color_difference_tenths(white, black, color_vision::typical), 1000U,
					 "the difference is symmetric in its arguments");
		expect_equal(color_difference_tenths(srgb(0x123456), srgb(0x123456),
											 color_vision::typical),
					 0U,
					 "a colour is zero units from itself");
		for (const auto vision : std::array{ color_vision::protanopia,
											 color_vision::deuteranopia }) {
			expect_equal(color_difference_tenths(black, white, vision), 1000U,
						 "the lightness axis survives both deficiencies intact");
		}
	}

	auto test_every_hue_critical_pair_is_named() -> void
	{
		const auto pairs = hue_critical_pairs();
		expect_equal(static_cast<std::uint32_t>(pairs.size()), 8U,
					 "the hue-critical set is enumerated");

		auto sorted = std::vector<hue_critical_pair>(pairs.begin(), pairs.end());
		std::ranges::sort(sorted);
		expect(std::ranges::adjacent_find(sorted) == sorted.end(),
			   "no hue-critical pair is checked twice");
		expect(std::ranges::all_of(
				   pairs,
				   [](const hue_critical_pair& aPair) { return aPair.First != aPair.Second; }),
			   "no pair compares a role with itself");

		expect(std::ranges::find(
				   pairs,
				   hue_critical_pair{ .First  = appearance_role::diff_added_background,
									  .Second = appearance_role::diff_removed_background })
				   != pairs.end(),
			   "added against removed is checked");
		expect(std::ranges::find(
				   pairs,
				   hue_critical_pair{ .First  = appearance_role::state_accepted,
									  .Second = appearance_role::state_defect })
				   != pairs.end(),
			   "accepted against defect is checked");
	}

	auto test_a_collapsing_pair_is_reported_with_its_differences() -> void
	{
		// The built-in light profile's own added and removed row fills.
		constexpr auto addedFill   = srgb(0xE6FFEC);
		constexpr auto removedFill = srgb(0xFFEBE9);

		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background, addedFill);
		palette      = with_role(palette, appearance_role::diff_removed_background, removedFill);

		const auto finding = measure_color_vision(
			palette,
			hue_critical_pair{ .First  = appearance_role::diff_added_background,
							   .Second = appearance_role::diff_removed_background });

		expect_equal(finding.TypicalTenths, 185U,
					 "the two fills are 18.5 units apart for typical vision");
		expect_equal(finding.ProtanopiaTenths, 83U,
					 "protanopia leaves 8.3 units");
		expect_equal(finding.DeuteranopiaTenths, 20U,
					 "deuteranopia leaves 2.0 units");
		expect(!finding.DistinguishableByColorAlone,
			   "a pair that collapses under a deficiency cannot rely on colour");

		const auto required = pairs_requiring_non_color_indicator(palette);
		expect(std::ranges::find(required, finding) != required.end(),
			   "the collapsed pair is listed as needing a non-colour indicator");
	}

	auto test_a_separated_pair_survives_both_deficiencies() -> void
	{
		// The built-in light profile's accepted and defect colours, which stay
		// apart because they differ in lightness as well as in hue.
		constexpr auto accepted = srgb(0x1A7F37);
		constexpr auto defect   = srgb(0xCF222E);

		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::state_accepted, accepted);
		palette      = with_role(palette, appearance_role::state_defect, defect);

		const auto finding = measure_color_vision(
			palette,
			hue_critical_pair{ .First  = appearance_role::state_accepted,
							   .Second = appearance_role::state_defect });

		expect_equal(finding.TypicalTenths, 1090U,
					 "the two marks are 109.0 units apart for typical vision");
		expect_equal(finding.ProtanopiaTenths, 301U, "protanopia leaves 30.1 units");
		expect_equal(finding.DeuteranopiaTenths, 282U, "deuteranopia leaves 28.2 units");
		expect(finding.DistinguishableByColorAlone,
			   "a pair that stays above the published minimum survives");

		const auto required = pairs_requiring_non_color_indicator(palette);
		expect(std::ranges::none_of(
				   required,
				   [](const color_vision_finding& aFinding) {
					   return aFinding.Pair.First == appearance_role::state_accepted
						   && aFinding.Pair.Second == appearance_role::state_defect;
				   }),
			   "a surviving pair is not listed as needing an indicator");
	}

	auto test_identical_colors_never_pass_the_colour_check() -> void
	{
		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background, srgb(0x123456));
		palette      = with_role(palette, appearance_role::diff_removed_background, srgb(0x123456));

		const auto finding = measure_color_vision(
			palette,
			hue_critical_pair{ .First  = appearance_role::diff_added_background,
							   .Second = appearance_role::diff_removed_background });
		expect(finding.TypicalTenths == 0U && !finding.DistinguishableByColorAlone,
			   "two roles painted the same colour are never distinguishable");

		const auto findings = measure_hue_critical_pairs(palette);
		expect_equal(static_cast<std::uint32_t>(findings.size()),
					 static_cast<std::uint32_t>(hue_critical_pairs().size()),
					 "every hue-critical pair produces one finding");
		expect(std::ranges::find(findings, finding) != findings.end(),
			   "the sweep agrees with the single measurement");
	}

	// The emphasis fills are the ones painted inside another fill, so the
	// question they answer is not "are these two states different" but "can
	// the reader see the mark at all". Both are enumerated, in a fixed order,
	// so a palette author can be told which one collapsed.
	auto test_every_emphasis_pair_is_named() -> void
	{
		const auto pairs = emphasis_pairs();
		expect_equal(static_cast<std::uint32_t>(pairs.size()), 2U,
					 "both intraline emphases are checked");
		expect(pairs[0]
				   == emphasis_pair{
					   .Fill     = appearance_role::diff_added_background,
					   .Emphasis = appearance_role::diff_intraline_added_background },
			   "the added emphasis is checked against the added row");
		expect(pairs[1]
				   == emphasis_pair{
					   .Fill     = appearance_role::diff_removed_background,
					   .Emphasis = appearance_role::diff_intraline_removed_background },
			   "the removed emphasis is checked against the removed row");
	}

	// The exact values the built-in light profile carried before BPR-0320.
	// They are the reason the check exists, so they are the case it is pinned
	// to: the added mark was plainly visible and the removed mark was not,
	// which is what the product owner reported seeing.
	auto test_a_collapsed_emphasis_is_reported_with_its_separation() -> void
	{
		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background,
								 srgb(0xE6FFEC));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_added_background,
								 srgb(0x9EFFB5));
		palette      = with_role(palette, appearance_role::diff_removed_background,
								 srgb(0xFFEBE9));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_removed_background,
								 srgb(0xFFE2DF));

		const auto added = measure_emphasis_separation(
			palette,
			emphasis_pair{
				.Fill     = appearance_role::diff_added_background,
				.Emphasis = appearance_role::diff_intraline_added_background });
		expect_equal(added.SeparationTenths, 383U,
					 "the added emphasis stands 38.3 units off its row");
		expect(added.Meets, "the added emphasis clears the published minimum");

		const auto removed = measure_emphasis_separation(
			palette,
			emphasis_pair{
				.Fill     = appearance_role::diff_removed_background,
				.Emphasis = appearance_role::diff_intraline_removed_background });
		expect_equal(removed.SeparationTenths, 42U,
					 "the removed emphasis stands 4.2 units off its row");
		expect(!removed.Meets, "the removed emphasis misses the published minimum");
		expect_equal(removed.MinimumTenths, emphasis_separation_minimum_tenths,
					 "the finding carries the minimum it was judged against");

		const auto failing = failing_emphasis_separation(palette);
		expect_equal(static_cast<std::uint32_t>(failing.size()), 1U,
					 "only the collapsed side is reported");
		expect(failing.front() == removed, "the report is the measurement");
	}

	// Clearing the floor on both sides is not the whole requirement. This
	// palette does exactly that and still reads as one-sided, because one mark
	// is nine times stronger than the other.
	auto test_a_lopsided_palette_fails_the_balance_bound() -> void
	{
		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background,
								 srgb(0xE6FFEC));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_added_background,
								 srgb(0x2AFF6E));
		palette      = with_role(palette, appearance_role::diff_removed_background,
								 srgb(0xFFEBE9));
		// 20.7 units off its row: past the floor, nowhere near the added side.
		palette = with_role(palette,
							appearance_role::diff_intraline_removed_background,
							srgb(0xF1B9B4));

		const auto failing = failing_emphasis_separation(palette);
		expect(failing.empty(), "both sides clear the separation minimum");

		const auto balance = measure_emphasis_balance(palette);
		expect_equal(balance.StrongestTenths, 822U, "the added side measures 82.2");
		expect_equal(balance.WeakestTenths, 207U, "the removed side measures 20.7");
		expect_equal(balance.RatioHundredths, 397U, "the sides differ by 3.97:1");
		expect(!balance.Meets, "a four-to-one split is not a balanced palette");
	}

	auto test_a_balanced_palette_passes_both_bounds() -> void
	{
		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background,
								 srgb(0xE6FFEC));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_added_background,
								 srgb(0x9EFFB5));
		palette      = with_role(palette, appearance_role::diff_removed_background,
								 srgb(0xFFEBE9));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_removed_background,
								 srgb(0xF1B9B4));

		expect(failing_emphasis_separation(palette).empty(),
			   "both emphases are visible on their rows");

		const auto balance = measure_emphasis_balance(palette);
		expect(balance.Meets, "neither side dominates the other");
		expect(balance.RatioHundredths <= emphasis_balance_maximum_hundredths,
			   "the reported ratio agrees with the verdict");

		const auto findings = measure_emphasis_separation(palette);
		expect_equal(static_cast<std::uint32_t>(findings.size()),
					 static_cast<std::uint32_t>(emphasis_pairs().size()),
					 "the sweep reports one finding per pair");
	}

	// Dividing by a zero separation would be undefined rather than merely
	// uninformative, and the palette that produces it has already failed.
	auto test_an_invisible_emphasis_reports_the_extreme_ratio() -> void
	{
		auto palette = maximal_contrast_palette();
		palette      = with_role(palette, appearance_role::diff_added_background,
								 srgb(0xE6FFEC));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_added_background,
								 srgb(0x9EFFB5));
		palette      = with_role(palette, appearance_role::diff_removed_background,
								 srgb(0xFFEBE9));
		palette      = with_role(palette,
								 appearance_role::diff_intraline_removed_background,
								 srgb(0xFFEBE9));

		const auto failing = failing_emphasis_separation(palette);
		expect_equal(static_cast<std::uint32_t>(failing.size()), 1U,
					 "an emphasis painted its row's own colour has failed");
		expect_equal(failing.front().SeparationTenths, 0U, "it is zero units away");

		const auto balance = measure_emphasis_balance(palette);
		expect_equal(balance.WeakestTenths, 0U, "the weakest side is invisible");
		expect_equal(balance.RatioHundredths,
					 std::numeric_limits<std::uint32_t>::max(),
					 "the ratio saturates rather than dividing by zero");
		expect(!balance.Meets, "an invisible emphasis is never balanced");
	}
	// A selected row's mark keeps the hue of the fill it sits in. The product
	// owner's requirement is exact: a blue line gets a blue mark, just darker
	// or lighter. Hue is the property under test, and three earlier attempts
	// failed it or failed to be visible at all.
	auto test_a_derived_mark_keeps_the_hue_of_its_fill() -> void
	{
		// A washed blue band, the case that was reported three times.
		constexpr auto band = srgb(0xC9E9EB);
		const auto     mark
			= derived_emphasis_fill(band, appearance_variant::light);

		expect(
			color_difference_tenths(band, mark, color_vision::typical)
				>= derived_emphasis_separation_minimum_tenths,
			"a derived mark reaches the published separation");
		expect(
			mark.Blue >= mark.Green && mark.Green >= mark.Red,
			"a derived mark keeps the channel order of the fill it came from");
		expect(
			mark.Red < band.Red && mark.Green < band.Green,
			"a mark on a light fill darkens rather than brightening");
	}

	// A dark fill has nowhere to darken to, so the same rule brightens it.
	// This is the direction the supplied reference takes, and the opposite of
	// an earlier attempt that deepened everything and read as mud.
	auto test_a_derived_mark_brightens_a_dark_fill() -> void
	{
		constexpr auto band = srgb(0x1A3E40);
		const auto     mark = derived_emphasis_fill(band, appearance_variant::dark);

		expect(
			color_difference_tenths(band, mark, color_vision::typical)
				>= derived_emphasis_separation_minimum_tenths,
			"a dark fill still reaches the published separation");
		expect(
			mark.Green > band.Green || mark.Blue > band.Blue,
			"a mark on a dark fill brightens rather than darkening");
		expect(
			mark.Blue >= mark.Green && mark.Green >= mark.Red,
			"brightening does not reorder the channels");
	}

	// A palette whose emphasis fills are gentle tints keeps its syntax
	// colouring showing through the mark; one whose fills are bold does not,
	// and the audit has to measure what is actually painted.
	auto test_only_a_bold_fill_carries_its_own_foreground() -> void
	{
		auto gentle = maximal_contrast_palette();
		gentle      = with_role(gentle,
								appearance_role::diff_intraline_added_background,
								srgb(0xFFFFFF));
		expect(
			!emphasis_needs_own_foreground(
				gentle,
				appearance_role::diff_intraline_added_background,
				appearance_variant::light),
			"black syntax text on a white fill needs no override");

		auto bold = with_role(gentle,
							  appearance_role::diff_intraline_added_background,
							  srgb(0x000000));
		expect(
			emphasis_needs_own_foreground(
				bold,
				appearance_role::diff_intraline_added_background,
				appearance_variant::light),
			"black syntax text on a black fill needs one");
		expect(
			emphasis_foreground(srgb(0x000000)) == srgb(0xFFFFFF),
			"the override picks the extreme that contrasts");
	}

	auto test_emphasis_preserves_readable_ink_and_adjusts_only_contrast() -> void
	{
		constexpr auto macro  = srgb(0xCE8AD4);
		constexpr auto gentle = srgb(0x672323);
		expect(readable_emphasis_foreground(macro, gentle, appearance_variant::dark) == macro,
			   "a readable token keeps its exact accepted syntax colour");
		for (const auto variant : { appearance_variant::dark, appearance_variant::high_contrast_dark }) {
			for (const auto fill : { srgb(0x933030), srgb(0x147B80), srgb(0xFFB400) }) {
				const auto ink = readable_emphasis_foreground(macro, fill, variant);
				expect(variant == appearance_variant::high_contrast_dark || (ink.Red > ink.Green && ink.Blue > ink.Green),
					   "a low-contrast macro remains a purple tint or shade instead of becoming monochrome");
				expect(contrast_ratio_hundredths(ink, fill) >= minimum_contrast_hundredths(contrast_kind::emphasis_text, variant),
					   "adjusted syntax clears the variant's emphasis floor");
			}
		}
	}

	/// [[BPR-1156]]. Glyphs on the code view's solid text selection keep a
	/// readable role colour exactly and move a faint one only as far as the
	/// variant's text floor, keeping its hue (owner note on composer-layout v005).
	auto test_selection_ink_keeps_readable_roles_and_lifts_faint_ones() -> void
	{
		constexpr auto fill    = srgb(0x28518C);
		constexpr auto text    = srgb(0xE6EDF3);
		constexpr auto keyword = srgb(0x6DA9DB);
		expect(readable_selection_ink(text, fill, appearance_variant::dark) == text, "a readable role keeps its exact colour on the selection");
		const auto lifted = readable_selection_ink(keyword, fill, appearance_variant::dark);
		expect(contrast_ratio_hundredths(keyword, fill) < text_contrast_minimum_hundredths && contrast_ratio_hundredths(lifted, fill) >= text_contrast_minimum_hundredths,
			   "a faint role reaches the text floor, not the lower emphasis floor");
		expect(lifted.Blue > lifted.Red && lifted.Blue > lifted.Green, "and stays blue");
		auto step = std::uint16_t{ 1U };
		while (step < 255U && composited(srgb(0xFFFFFF), keyword, static_cast<std::uint8_t>(step)) != lifted) {
			++step;
		}
		expect(step < 255U && contrast_ratio_hundredths(composited(srgb(0xFFFFFF), keyword, static_cast<std::uint8_t>(step - 1U)), fill) < text_contrast_minimum_hundredths,
			   "moved toward white by the least amount that reads");
		expect(readable_selection_ink(srgb(0x7A3E9D), srgb(0xFFE877), appearance_variant::high_contrast_light) == srgb(0x7A3E9D)
				   || contrast_ratio_hundredths(readable_selection_ink(srgb(0x7A3E9D), srgb(0xFFE877), appearance_variant::high_contrast_light), srgb(0xFFE877))
					   >= high_contrast_text_minimum_hundredths,
			   "a high-contrast variant is held to its own floor");
		expect(readable_selection_ink(srgb(0x777777), srgb(0x767676), appearance_variant::high_contrast_dark) == srgb(0x000000)
				   || readable_selection_ink(srgb(0x777777), srgb(0x767676), appearance_variant::high_contrast_dark) == srgb(0xFFFFFF),
			   "where no ink reaches the floor, the extreme that contrasts most");
	}

	/// [[BPR-1157]]. A focus ring keeps a selection colour that already stands
	/// out on its surface and lifts one that does not to the indicator floor,
	/// keeping its hue (owner rejection of native-keyboard v001).
	auto test_focus_ring_reaches_the_indicator_floor() -> void
	{
		constexpr auto selection = srgb(0x28518C);
		constexpr auto dark      = srgb(0x1C2026);
		constexpr auto light     = srgb(0xFFFFFF);
		expect(contrast_ratio_hundredths(selection, dark) < indicator_contrast_minimum_hundredths, "the dark selection colour alone is too faint a ring");
		const auto ring = visible_focus_indicator(selection, dark);
		expect(contrast_ratio_hundredths(ring, dark) >= indicator_contrast_minimum_hundredths, "lifted to the indicator floor");
		expect(contrast_ratio_hundredths(ring, dark) < text_contrast_minimum_hundredths, "and not to the stricter text floor");
		expect(ring.Blue > ring.Red && ring.Blue > ring.Green, "still the theme's blue");
		auto step = std::uint16_t{ 1U };
		while (step < 255U && composited(srgb(0xFFFFFF), selection, static_cast<std::uint8_t>(step)) != ring) {
			++step;
		}
		expect(step < 255U && contrast_ratio_hundredths(composited(srgb(0xFFFFFF), selection, static_cast<std::uint8_t>(step - 1U)), dark) < indicator_contrast_minimum_hundredths,
			   "moved toward white by the least amount that stands out");
		expect(visible_focus_indicator(srgb(0x0A64C8), light) == srgb(0x0A64C8), "a colour that already stands out is kept exactly");
		const auto pale = visible_focus_indicator(srgb(0xADD6FF), light);
		expect(contrast_ratio_hundredths(pale, light) >= indicator_contrast_minimum_hundredths && pale.Red < 0xADU, "a pale selection on white moves toward black");
		const auto grey = visible_focus_indicator(srgb(0x777777), srgb(0x767676));
		expect(contrast_ratio_hundredths(grey, srgb(0x767676)) >= indicator_contrast_minimum_hundredths || grey == srgb(0x000000) || grey == srgb(0xFFFFFF), "where nothing between reaches it, the extreme");
	}

} // namespace

int main()
{
	test_the_ratio_matches_known_values();
	test_the_minimums_are_published_once();
	test_every_checked_pair_is_named();
	test_a_conforming_palette_reports_nothing();
	test_a_failing_pair_is_reported_with_its_ratio();
	test_the_high_contrast_minimum_is_the_stricter_one();
	test_an_indicator_is_held_to_the_indicator_minimum();
	test_simulation_keeps_what_a_dichromat_keeps();
	test_the_difference_matches_known_values();
	test_every_hue_critical_pair_is_named();
	test_a_collapsing_pair_is_reported_with_its_differences();
	test_a_separated_pair_survives_both_deficiencies();
	test_identical_colors_never_pass_the_colour_check();
	test_every_emphasis_pair_is_named();
	test_a_collapsed_emphasis_is_reported_with_its_separation();
	test_a_lopsided_palette_fails_the_balance_bound();
	test_a_balanced_palette_passes_both_bounds();
	test_an_invisible_emphasis_reports_the_extreme_ratio();
	test_a_derived_mark_keeps_the_hue_of_its_fill();
	test_a_derived_mark_brightens_a_dark_fill();
	test_only_a_bold_fill_carries_its_own_foreground();
	test_emphasis_preserves_readable_ink_and_adjusts_only_contrast();
	test_selection_ink_keeps_readable_roles_and_lifts_faint_ones();
	test_focus_ring_reaches_the_indicator_floor();
	return EXIT_SUCCESS;
}
