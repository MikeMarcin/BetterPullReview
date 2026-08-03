#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

	/// \brief Reports whether a role is a background of some required pair.
	[[nodiscard]] auto is_checked_background(const appearance_role aRole) -> bool
	{
		return std::ranges::any_of(
			required_contrast_pairs(),
			[aRole](const contrast_pair& aPair) { return aPair.Background == aRole; });
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
			appearance_role::code_selection_background,
			appearance_role::diff_added_background,
			appearance_role::diff_removed_background,
			appearance_role::diff_intraline_added_background,
			appearance_role::diff_intraline_removed_background,
			appearance_role::diff_moved_background,
		};
		for (const auto background : canvasBackgrounds) {
			const auto count = std::ranges::count_if(
				pairs,
				[background](const contrast_pair& aPair) {
					return aPair.Background == background;
				});
			expect_equal(static_cast<std::uint32_t>(count), 25U,
						 "every syntax role is checked over every canvas background");
		}

		const auto indicators = std::ranges::count_if(
			pairs,
			[](const contrast_pair& aPair) {
				return aPair.Kind == contrast_kind::indicator;
			});
		expect_equal(static_cast<std::uint32_t>(indicators), 6U,
					 "all six review-state roles are checked as indicators");

		expect_equal(static_cast<std::uint32_t>(pairs.size()), 209U,
					 "the checked set is the three fixed pairs, 25 syntax roles over "
					 "eight canvas backgrounds, and six state indicators");
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
		// colour drawn over its selection fill.
		constexpr auto stringColor    = srgb(0x0A7B54);
		constexpr auto selectionColor = srgb(0xB6D7FF);
		expect_equal(contrast_ratio_hundredths(stringColor, selectionColor), 356U,
					 "the sample pair measures 3.56:1");

		const auto palette = with_role(
			with_role(maximal_contrast_palette(),
					  appearance_role::syntax_string,
					  stringColor),
			appearance_role::code_selection_background,
			selectionColor);

		const auto failures = failing_contrast(palette, appearance_variant::light);
		expect_equal(static_cast<std::uint32_t>(failures.size()), 1U,
					 "exactly the one pair below the minimum is reported");

		const auto& failure = failures.front();
		expect(failure.Pair.Foreground == appearance_role::syntax_string
				   && failure.Pair.Background == appearance_role::code_selection_background,
			   "the failure names the pair that failed");
		expect_equal(failure.RatioHundredths, 356U,
					 "the failure carries the measured ratio, not just a verdict");
		expect_equal(failure.MinimumHundredths, 450U,
					 "the failure carries the minimum it was measured against");
		expect(!failure.Meets, "the failure is reported as failing");

		const auto measured = measure_contrast(
			palette,
			contrast_pair{ .Foreground = appearance_role::syntax_string,
						   .Background = appearance_role::code_selection_background,
						   .Kind       = contrast_kind::text },
			appearance_variant::light);
		expect(measured == failure,
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
	return EXIT_SUCCESS;
}
