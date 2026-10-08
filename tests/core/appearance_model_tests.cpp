#include "bpr/core/appearance_model.hpp"
#include "bpr/core/white_space.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

	[[nodiscard]] auto observation(
		const os_color_scheme        aScheme,
		const os_contrast_preference aContrast = os_contrast_preference::standard)
		-> appearance_observation
	{
		return appearance_observation{ .Scheme = aScheme, .Contrast = aContrast };
	}

	constexpr auto every_scheme = std::array{
		os_color_scheme::unknown,
		os_color_scheme::light,
		os_color_scheme::dark,
	};

	constexpr auto every_contrast = std::array{
		os_contrast_preference::standard,
		os_contrast_preference::high,
	};

	[[nodiscard]] auto mode_at(const std::size_t aIndex) -> appearance_mode
	{
		return static_cast<appearance_mode>(static_cast<std::uint8_t>(aIndex));
	}

	[[nodiscard]] auto role_at(const std::size_t aIndex) -> appearance_role
	{
		return static_cast<appearance_role>(static_cast<std::uint8_t>(aIndex));
	}

	auto test_system_mode_follows_the_observed_scheme() -> void
	{
		const auto observedDark  = observation(os_color_scheme::dark);
		const auto observedLight = observation(os_color_scheme::light);

		const auto followedDark = resolve_appearance(appearance_mode::system, observedDark);
		expect(followedDark.Variant == appearance_variant::dark
				   && followedDark.Scheme == appearance_scheme_source::observed_scheme,
			   "system mode resolves the observed dark scheme");
		const auto followedLight = resolve_appearance(appearance_mode::system, observedLight);
		expect(followedLight.Variant == appearance_variant::light
				   && followedLight.Scheme == appearance_scheme_source::observed_scheme,
			   "system mode resolves the observed light scheme");

		const auto explicitLight = resolve_appearance(appearance_mode::light, observedDark);
		const auto explicitDark  = resolve_appearance(appearance_mode::dark, observedLight);
		expect(explicitLight.Variant == appearance_variant::light
				   && explicitLight.Scheme == appearance_scheme_source::selected_mode
				   && explicitDark.Variant == appearance_variant::dark
				   && explicitDark.Scheme == appearance_scheme_source::selected_mode,
			   "an explicit mode ignores the observed scheme that system mode follows");

		expect(follows_observed_scheme(appearance_mode::system)
				   && follows_observed_scheme(appearance_mode::high_contrast_system)
				   && !follows_observed_scheme(appearance_mode::light)
				   && !follows_observed_scheme(appearance_mode::dark)
				   && !follows_observed_scheme(appearance_mode::high_contrast_light)
				   && !follows_observed_scheme(appearance_mode::high_contrast_dark),
			   "exactly the two system modes consume the observed scheme");

		for (const auto scheme : every_scheme) {
			for (const auto contrast : every_contrast) {
				const auto resolved = resolve_appearance(
					appearance_mode::high_contrast_dark, observation(scheme, contrast));
				expect(resolved.Variant == appearance_variant::high_contrast_dark,
					   "no observation moves an explicit high-contrast dark mode");
			}
		}

		const auto unobserved = resolve_appearance(
			appearance_mode::system, observation(os_color_scheme::unknown));
		expect(unobserved.Variant == appearance_variant::light
				   && unobserved.Scheme == appearance_scheme_source::unobserved_default,
			   "an unobservable scheme defaults to light and reports the default");

		const auto systemHighContrast = resolve_appearance(
			appearance_mode::high_contrast_system, observedDark);
		expect(systemHighContrast.Variant == appearance_variant::high_contrast_dark
				   && systemHighContrast.Scheme == appearance_scheme_source::observed_scheme
				   && systemHighContrast.Contrast == appearance_contrast_source::selected_mode,
			   "a high-contrast system mode follows the scheme and keeps its own contrast");
	}

	auto test_high_contrast_preference_selects_or_recommends() -> void
	{
		const auto highDark = observation(
			os_color_scheme::dark, os_contrast_preference::high);
		const auto selected = resolve_appearance(appearance_mode::system, highDark);
		expect(selected.Variant == appearance_variant::high_contrast_dark
				   && selected.Contrast == appearance_contrast_source::observed_preference
				   && !selected.HighContrastRecommended,
			   "system mode selects the high-contrast variant of the observed scheme");

		const auto highLight = observation(
			os_color_scheme::light, os_contrast_preference::high);
		expect(resolve_appearance(appearance_mode::system, highLight).Variant
				   == appearance_variant::high_contrast_light,
			   "the selected high-contrast variant keeps the observed light family");

		const auto recommended = resolve_appearance(appearance_mode::light, highDark);
		expect(recommended.Variant == appearance_variant::light
				   && recommended.Contrast == appearance_contrast_source::none
				   && recommended.HighContrastRecommended,
			   "an explicit standard-contrast mode keeps its variant and carries the recommendation");

		const auto alreadyHigh = resolve_appearance(
			appearance_mode::high_contrast_light, highDark);
		expect(alreadyHigh.Variant == appearance_variant::high_contrast_light
				   && alreadyHigh.Contrast == appearance_contrast_source::selected_mode
				   && !alreadyHigh.HighContrastRecommended,
			   "a mode that is already high contrast needs no recommendation");

		const auto standard = resolve_appearance(
			appearance_mode::system, observation(os_color_scheme::dark));
		expect(standard.Variant == appearance_variant::dark
				   && standard.Contrast == appearance_contrast_source::none
				   && !standard.HighContrastRecommended,
			   "a standard preference selects no high-contrast variant");

		const auto unobservedHigh = resolve_appearance(
			appearance_mode::system,
			observation(os_color_scheme::unknown, os_contrast_preference::high));
		expect(unobservedHigh.Variant == appearance_variant::high_contrast_light
				   && unobservedHigh.Scheme == appearance_scheme_source::unobserved_default
				   && unobservedHigh.Contrast == appearance_contrast_source::observed_preference,
			   "an unobservable scheme still adopts the observed contrast preference");

		expect(high_contrast(appearance_variant::high_contrast_light)
				   && high_contrast(appearance_variant::high_contrast_dark)
				   && !high_contrast(appearance_variant::light)
				   && !high_contrast(appearance_variant::dark),
			   "exactly the two high-contrast variants report high contrast");
	}

	auto test_resolution_is_total_and_self_consistent() -> void
	{
		for (std::size_t index = 0U; index < appearance_mode_count; ++index) {
			const auto mode = mode_at(index);
			for (const auto scheme : every_scheme) {
				for (const auto contrast : every_contrast) {
					const auto resolved = resolve_appearance(
						mode, observation(scheme, contrast));
					expect(high_contrast(resolved.Variant)
							   == (resolved.Contrast != appearance_contrast_source::none),
						   "a high-contrast variant is exactly a variant with a contrast source");
					expect(resolved.HighContrastRecommended
							   == (contrast == os_contrast_preference::high
								   && !high_contrast(resolved.Variant)),
						   "a recommendation appears exactly when an observed preference is unmet");
					expect((resolved.Scheme == appearance_scheme_source::selected_mode)
							   == !follows_observed_scheme(mode),
						   "only a scheme-following mode reports an observed scheme source");
				}
			}
		}
	}

	auto test_mode_identifiers_round_trip() -> void
	{
		auto names = std::vector<std::string_view>{ };
		for (std::size_t index = 0U; index < appearance_mode_count; ++index) {
			const auto mode = mode_at(index);
			const auto name = stable_name(mode);
			expect(!name.empty(), "every mode carries a stable identifier");
			const auto found = find_appearance_mode(name);
			expect(found.has_value() && *found == mode,
				   "every mode identifier resolves back to its own mode");
			expect(std::ranges::find(names, name) == names.end(),
				   "mode identifiers are distinct");
			names.push_back(name);
		}

		expect(stable_name(appearance_mode::system) == "system",
			   "the fresh-installation mode keeps its stable identifier");
		expect(!find_appearance_mode("automatic").has_value()
				   && !find_appearance_mode("").has_value()
				   && !find_appearance_mode("System").has_value(),
			   "an identifier outside the mode vocabulary is refused");
		expect(!find_appearance_mode("syntax_keyword").has_value()
				   && !find_appearance_role("system").has_value(),
			   "the mode and role vocabularies do not overlap");
	}

	auto test_role_vocabulary_is_closed() -> void
	{
		auto names = std::vector<std::string_view>{ };
		for (std::size_t index = 0U; index < appearance_role_count; ++index) {
			const auto role = role_at(index);
			const auto name = stable_name(role);
			expect(!name.empty(), "every role carries a stable identifier");
			expect(index_of(role) == index, "every role indexes its own palette slot");
			const auto found = find_appearance_role(name);
			expect(found.has_value() && *found == role,
				   "every role identifier resolves back to its own role");
			expect(std::ranges::find(names, name) == names.end(),
				   "role identifiers are distinct");
			names.push_back(name);
		}
		expect(names.size() == appearance_role_count,
			   "the vocabulary covers every declared role exactly once");

		expect(!find_appearance_role("").has_value()
				   && !find_appearance_role("syntax_lambda").has_value()
				   && !find_appearance_role("Syntax_Keyword").has_value()
				   && !find_appearance_role("code_background ").has_value(),
			   "an identifier outside the closed vocabulary is refused");

		auto       palette  = appearance_palette{ };
		const auto emphasis = appearance_color{
			.Red = 12U, .Green = 34U, .Blue = 56U, .Alpha = 200U
		};
		palette[index_of(appearance_role::diff_intraline_added_background)] = emphasis;
		expect(palette.size() == appearance_role_count,
			   "a palette is a total mapping over the closed vocabulary");
		expect(palette[index_of(appearance_role::diff_intraline_added_background)] == emphasis,
			   "a role reads back the colour written through its own slot");
		expect(palette[index_of(appearance_role::diff_added_background)]
				   == appearance_color{ .Red = 0U, .Green = 0U, .Blue = 0U, .Alpha = 255U },
			   "distinct roles do not share a palette slot");
	}

	[[nodiscard]] auto legal_settings() -> typography_settings
	{
		auto settings             = typography_settings{ };
		settings.Ui.Font.Family   = "Sample Interface Family";
		settings.Code.Font.Family = "Sample Code Family";
		return settings;
	}

	[[nodiscard]] auto only_rejection(const std::vector<typography_rejection>& aRejections)
		-> typography_rejection
	{
		expect(aRejections.size() == 1U, "exactly one value is rejected");
		return aRejections.front();
	}

	[[nodiscard]] auto font_of(
		typography_settings&    aSettings,
		const typography_target aTarget) -> font_settings&
	{
		return aTarget == typography_target::ui_font ? aSettings.Ui.Font
													 : aSettings.Code.Font;
	}

	auto assign(
		font_settings&         aFont,
		const typography_field aField,
		const std::uint32_t    aValue) -> void
	{
		switch (aField) {
		case typography_field::point_size:
			aFont.PointSizeCentipoints = aValue;
			break;
		case typography_field::weight:
			aFont.Weight = aValue;
			break;
		case typography_field::line_height:
		case typography_field::family:
		case typography_field::tab_width:
		case typography_field::white_space:
		case typography_field::code_zoom:
			expect(false, "only the shared numeric font fields are assignable here");
			break;
		}
	}

	template <typename Record>
	concept carries_a_line_height = requires(Record aRecord) {
		aRecord.LineHeightPercent;
	};

	// BPR-0118 retired the code line height, and this is where that decision is
	// held. It reads the types rather than a construction site, so re-adding
	// the field anywhere fails here even if nothing ever assigns to it.
	//
	// The third assertion is the one that matters. `code_font_settings` embeds
	// `font_settings`, so putting a line height back on the shared record would
	// silently give the code font one again, and the first two would still pass.
	static_assert(
		carries_a_line_height<ui_font_settings>,
		"the UI font carries a line height");
	static_assert(
		!carries_a_line_height<code_font_settings>,
		"the code font has no field a line height could occupy");
	static_assert(
		!carries_a_line_height<font_settings>,
		"the shared font record has no line height, so no font gains one by embedding it");

	auto test_the_core_invents_no_font_family() -> void
	{
		const auto defaulted = validate_typography(typography_settings{ });
		expect(defaulted
				   == std::vector<typography_rejection>{
					   { .Target = typography_target::ui_font,
						 .Field  = typography_field::family,
						 .Reason = typography_reason::below_minimum },
					   { .Target = typography_target::code_font,
						 .Field  = typography_field::family,
						 .Reason = typography_reason::below_minimum },
				   },
			   "the core invents no family, so a defaulted record is illegal on both families alone");
		expect(validate_typography(legal_settings()).empty(),
			   "the numeric defaults are legal once the host supplies both families");

		const auto bounds        = bounds_of(typography_field::family);
		auto       longest       = legal_settings();
		longest.Code.Font.Family = std::string(bounds.Maximum, 'a');
		expect(validate_typography(longest).empty(),
			   "a family name of exactly the maximum length is legal");

		auto tooLong             = legal_settings();
		tooLong.Code.Font.Family = std::string(bounds.Maximum + 1U, 'a');
		expect(only_rejection(validate_typography(tooLong))
				   == typography_rejection{
					   .Target = typography_target::code_font,
					   .Field  = typography_field::family,
					   .Reason = typography_reason::above_maximum },
			   "one byte over the published family maximum is rejected as too long");

		auto malformed           = legal_settings();
		malformed.Ui.Font.Family = "Sample\nFamily";
		expect(only_rejection(validate_typography(malformed))
				   == typography_rejection{
					   .Target = typography_target::ui_font,
					   .Field  = typography_field::family,
					   .Reason = typography_reason::malformed_value },
			   "a control byte in a family name is refused under its own typed reason");

		auto nonAscii           = legal_settings();
		nonAscii.Ui.Font.Family = "\xe3\x83\x92\xe3\x83\xa9\xe3\x82\xae\xe3\x83\x8e";
		expect(validate_typography(nonAscii).empty(),
			   "a non-ASCII family name is opaque to the core, not malformed");
	}

	auto test_typography_bounds_are_enforced_exactly() -> void
	{
		// Line height is absent from this loop because it is absent from the
		// shared record. It is exercised on its own below, against the UI font,
		// which is now the only font that has one.
		constexpr auto numericFields = std::array{
			typography_field::point_size,
			typography_field::weight,
		};
		constexpr auto targets = std::array{
			typography_target::ui_font,
			typography_target::code_font,
		};

		for (const auto field : numericFields) {
			const auto bounds = bounds_of(field);
			expect(bounds.Minimum > 0U && bounds.Maximum > bounds.Minimum,
				   "every bounded field publishes a usable inclusive range");

			for (const auto target : targets) {
				auto below = legal_settings();
				assign(font_of(below, target), field, bounds.Minimum - 1U);
				expect(only_rejection(validate_typography(below))
						   == typography_rejection{
							   .Target = target,
							   .Field  = field,
							   .Reason = typography_reason::below_minimum },
					   "one step under the published minimum is rejected with its exact typed reason");

				auto above = legal_settings();
				assign(font_of(above, target), field, bounds.Maximum + 1U);
				expect(only_rejection(validate_typography(above))
						   == typography_rejection{
							   .Target = target,
							   .Field  = field,
							   .Reason = typography_reason::above_maximum },
					   "one step over the published maximum is rejected with its exact typed reason");

				auto atMinimum = legal_settings();
				assign(font_of(atMinimum, target), field, bounds.Minimum);
				auto atMaximum = legal_settings();
				assign(font_of(atMaximum, target), field, bounds.Maximum);
				expect(validate_typography(atMinimum).empty()
						   && validate_typography(atMaximum).empty(),
					   "both published edges are legal");
			}
		}

		const auto heightBounds = bounds_of(typography_field::line_height);
		expect(heightBounds.Minimum > 0U && heightBounds.Maximum > heightBounds.Minimum,
			   "the line height publishes a usable inclusive range");

		auto tooShort                 = legal_settings();
		tooShort.Ui.LineHeightPercent = heightBounds.Minimum - 1U;
		expect(only_rejection(validate_typography(tooShort))
				   == typography_rejection{
					   .Target = typography_target::ui_font,
					   .Field  = typography_field::line_height,
					   .Reason = typography_reason::below_minimum },
			   "a UI line height under the published minimum is rejected with its exact typed reason");

		auto tooTall                 = legal_settings();
		tooTall.Ui.LineHeightPercent = heightBounds.Maximum + 1U;
		expect(only_rejection(validate_typography(tooTall))
				   == typography_rejection{
					   .Target = typography_target::ui_font,
					   .Field  = typography_field::line_height,
					   .Reason = typography_reason::above_maximum },
			   "a UI line height over the published maximum is rejected with its exact typed reason");

		auto shortest                 = legal_settings();
		shortest.Ui.LineHeightPercent = heightBounds.Minimum;
		auto tallest                  = legal_settings();
		tallest.Ui.LineHeightPercent  = heightBounds.Maximum;
		expect(validate_typography(shortest).empty()
				   && validate_typography(tallest).empty(),
			   "both published line-height edges are legal");

		const auto tabBounds    = bounds_of(typography_field::tab_width);
		auto       tooNarrow    = legal_settings();
		tooNarrow.Code.TabWidth = tabBounds.Minimum - 1U;
		expect(only_rejection(validate_typography(tooNarrow))
				   == typography_rejection{
					   .Target = typography_target::code_font,
					   .Field  = typography_field::tab_width,
					   .Reason = typography_reason::below_minimum },
			   "a tab width under the published minimum is rejected with its exact typed reason");

		auto tooWide          = legal_settings();
		tooWide.Code.TabWidth = tabBounds.Maximum + 1U;
		expect(only_rejection(validate_typography(tooWide))
				   == typography_rejection{
					   .Target = typography_target::code_font,
					   .Field  = typography_field::tab_width,
					   .Reason = typography_reason::above_maximum },
			   "a tab width over the published maximum is rejected with its exact typed reason");

		auto narrowest          = legal_settings();
		narrowest.Code.TabWidth = tabBounds.Minimum;
		auto widest             = legal_settings();
		widest.Code.TabWidth    = tabBounds.Maximum;
		expect(validate_typography(narrowest).empty() && validate_typography(widest).empty(),
			   "both published tab-width edges are legal");

		for (std::uint32_t mask = 0U; mask < 8U; ++mask) {
			auto settings              = legal_settings();
			settings.Code.Ligatures    = (mask & 1U) != 0U;
			settings.Code.SyntaxBold   = (mask & 2U) != 0U;
			settings.Code.SyntaxItalic = (mask & 4U) != 0U;
			expect(validate_typography(settings).empty(),
				   "no combination of the boolean code switches is illegal");
		}
	}

	auto test_every_violation_is_reported_in_order() -> void
	{
		auto settings = legal_settings();
		settings.Ui.Font.Family.clear();
		settings.Ui.Font.PointSizeCentipoints   = 1U;
		settings.Ui.Font.Weight                 = 5000U;
		settings.Ui.LineHeightPercent           = 1U;
		settings.Code.Font.PointSizeCentipoints = 999999U;
		settings.Code.TabWidth                  = 0U;

		const auto expected = std::vector<typography_rejection>{
			{ .Target = typography_target::ui_font,
			  .Field  = typography_field::family,
			  .Reason = typography_reason::below_minimum },
			{ .Target = typography_target::ui_font,
			  .Field  = typography_field::point_size,
			  .Reason = typography_reason::below_minimum },
			{ .Target = typography_target::ui_font,
			  .Field  = typography_field::weight,
			  .Reason = typography_reason::above_maximum },
			{ .Target = typography_target::ui_font,
			  .Field  = typography_field::line_height,
			  .Reason = typography_reason::below_minimum },
			{ .Target = typography_target::code_font,
			  .Field  = typography_field::point_size,
			  .Reason = typography_reason::above_maximum },
			{ .Target = typography_target::code_font,
			  .Field  = typography_field::tab_width,
			  .Reason = typography_reason::below_minimum },
		};
		expect(validate_typography(settings) == expected,
			   "every violation is reported once in the documented order");
	}
	void test_dif_preferences_resolve_conservatively()
	{
		expect(dif_preferences{ } == dif_preferences{ .Visibility = dif_visibility::shown, .Motion = motion_mode::system }, "Dif defaults to shown/System");
		for (const auto visibility : { dif_visibility::shown, dif_visibility::hidden }) {
			for (const auto observation : { motion_observation::full, motion_observation::reduced, motion_observation::unknown, motion_observation::unsupported, motion_observation::failed }) {
				for (const auto mode : { motion_mode::system, motion_mode::full, motion_mode::reduced }) {
					const auto preferences = dif_preferences{ .Visibility = visibility, .Motion = mode };
					const auto resolved    = resolve_dif(preferences, observation);
					expect(resolved.Preferences == preferences && resolved.Observation == observation, "resolution retains choices and bounded evidence");
					const auto full = mode == motion_mode::full || (mode == motion_mode::system && observation == motion_observation::full);
					expect(resolved.EffectiveMotion == (full ? motion_mode::full : motion_mode::reduced), "only explicit Full or reliable System full allows full motion");
				}
			}
		}
		expect(!valid(dif_preferences{ .Visibility = static_cast<dif_visibility>(255) }) && !valid(dif_preferences{ .Motion = static_cast<motion_mode>(255) }), "unknown preferences are illegal");
		expect(resolve_dif({ }, static_cast<motion_observation>(255)).EffectiveMotion == motion_mode::reduced, "out-of-vocabulary observation cannot grant motion");
	}

} // namespace

namespace {
	/// \brief [[BPR-1107]]. One line written with `.` for a space, `>` for a tab
	/// and anything else for another character.
	[[nodiscard]] auto line_of(const std::string_view aText) -> std::vector<white_space_character>
	{
		auto line = std::vector<white_space_character>{ };
		for (const auto character : aText) {
			line.push_back(character == '.'       ? white_space_character::space
							   : character == '>' ? white_space_character::tab
												  : white_space_character::other);
		}
		return line;
	}

	/// \brief The drawn characters of one line, written back with `^` under each.
	[[nodiscard]] auto marked(const std::string_view aText, const white_space_drawing aDrawing, const bool aSelected) -> std::string
	{
		const auto drawn  = white_space_drawn(line_of(aText), aDrawing, aSelected);
		auto       result = std::string{ };
		for (const auto flag : drawn) {
			result += flag ? '^' : ' ';
		}
		return result;
	}

	void test_white_space_drawing_follows_the_choice()
	{
		// The owner's reference, line 1: four spaces, a word, a space, a word,
		// spaces, and two trailing spaces.
		constexpr auto line = std::string_view{ "....a.bc...d.." };
		expect(marked(line, white_space_drawing::none, true) == "              ", "none draws nothing, even selected");
		expect(marked(line, white_space_drawing::selection, false) == "              ", "selection draws nothing on a line that is not selected");
		expect(marked(line, white_space_drawing::selection, true) == "^^^^ ^  ^^^ ^^", "and every white space character of a selected line");
		expect(marked(line, white_space_drawing::leading, false) == "^^^^          ", "leading draws the indentation only");
		expect(marked(line, white_space_drawing::trailing, false) == "            ^^", "trailing draws what follows the last other character");
		expect(marked(line, white_space_drawing::boundary, false) == "^^^^        ^^", "boundary draws both");
		expect(marked(line, white_space_drawing::all, false) == "^^^^ ^  ^^^ ^^", "all draws every white space character, enclosed ones too");
		// Tabs are white space like spaces, and a line of white space alone is
		// both leading and trailing.
		expect(marked(">>.a>b", white_space_drawing::leading, false) == "^^^   ", "leading takes tabs and spaces alike");
		expect(marked(">>.a>b", white_space_drawing::all, false) == "^^^ ^ ", "an enclosed tab is drawn only by all or a selection");
		expect(marked(">.>", white_space_drawing::trailing, false) == "^^^" && marked(">.>", white_space_drawing::leading, false) == "^^^",
			   "a line of white space alone is both leading and trailing");
		expect(white_space_drawn(line_of(""), white_space_drawing::all, true).empty(), "an empty line draws nothing");
		expect(marked("abc", white_space_drawing::all, true) == "   ", "a line with no white space draws nothing");
		expect(white_space_drawn(line_of("..a"), static_cast<white_space_drawing>(white_space_drawing_count), true) == std::vector<bool>(3U, false),
			   "a value outside the vocabulary draws nothing");
	}

	void test_white_space_vocabulary_is_closed()
	{
		auto names = std::vector<std::string_view>{ };
		for (auto index = std::size_t{ 0U }; index < white_space_drawing_count; ++index) {
			const auto drawing = static_cast<white_space_drawing>(static_cast<std::uint8_t>(index));
			expect(valid(drawing) && !stable_name(drawing).empty(), "every declared choice is valid and named");
			names.push_back(stable_name(drawing));
		}
		std::ranges::sort(names);
		expect(std::ranges::adjacent_find(names) == names.end(), "no two choices share a durable name");
		expect(code_font_settings{ }.WhiteSpace == white_space_drawing::selection, "the default is Sublime Text's own: selection");
		expect(bounds_of(typography_field::white_space).Minimum == 0U
				   && bounds_of(typography_field::white_space).Maximum == white_space_drawing_count - 1U,
			   "the bounds are the vocabulary's ordinals");
		auto settings             = typography_settings{ };
		settings.Ui.Font.Family   = "UI";
		settings.Code.Font.Family = "Code";
		expect(std::ranges::none_of(validate_typography(settings), [](const typography_rejection& aRejection) { return aRejection.Field == typography_field::white_space; }),
			   "every declared choice is legal");
		settings.Code.WhiteSpace = static_cast<white_space_drawing>(white_space_drawing_count);
		const auto rejections    = validate_typography(settings);
		expect(std::ranges::count(rejections, typography_rejection{ .Target = typography_target::code_font, .Field = typography_field::white_space, .Reason = typography_reason::malformed_value }) == 1,
			   "a value outside the vocabulary is refused as malformed");
	}

	/// [[BPR-1336]]. The comparison's zoom: a multiplier of the code size, its
	/// ladder, its clamp and its bounds.
	void test_code_zoom_steps_scale_and_clamp()
	{
		auto code                      = code_font_settings{ };
		code.Font.Family               = "Code";
		code.Font.PointSizeCentipoints = 1300U;
		expect(code.ZoomPercent == actual_code_zoom_percent && zoomed_point_size(code) == 1300U,
			   "a fresh record shows the code at its own size");
		expect(std::ranges::is_sorted(code_zoom_steps) && std::ranges::adjacent_find(code_zoom_steps) == code_zoom_steps.end()
				   && std::ranges::contains(code_zoom_steps, actual_code_zoom_percent),
			   "the ladder ascends without repeats and passes through Actual Size");
		expect(bounds_of(typography_field::code_zoom).Minimum == code_zoom_steps.front()
				   && bounds_of(typography_field::code_zoom).Maximum == code_zoom_steps.back(),
			   "the bounds are the ends of the ladder");

		// Up the ladder from 100% to the top, one step a gesture, every size larger.
		auto ladder = std::vector<std::uint32_t>{ code.ZoomPercent };
		while (code_zoom_steps_from(code, code_zoom_step::in)) {
			const auto before = zoomed_point_size(code);
			code.ZoomPercent  = stepped_code_zoom(code, code_zoom_step::in);
			expect(zoomed_point_size(code) > before, "each Zoom In shows a larger size");
			ladder.push_back(code.ZoomPercent);
		}
		expect(ladder == std::vector<std::uint32_t>{ 100U, 110U, 125U, 150U, 175U, 200U, 250U, 300U },
			   "Zoom In walks the ladder from Actual Size to its top");
		expect(zoomed_point_size(code) == 3900U, "300% of 13 pt is 39 pt");
		expect(stepped_code_zoom(code, code_zoom_step::in) == 300U, "Zoom In stops at the top");
		code.ZoomPercent = 50U;
		expect(!code_zoom_steps_from(code, code_zoom_step::out) && stepped_code_zoom(code, code_zoom_step::out) == 50U,
			   "Zoom Out stops at the bottom");
		expect(zoomed_point_size(code) == 650U, "50% of 13 pt is 6.5 pt");
		expect(stepped_code_zoom(code, code_zoom_step::actual_size) == 100U && code_zoom_steps_from(code, code_zoom_step::actual_size),
			   "Actual Size returns to 100%");
		code.ZoomPercent = 100U;
		expect(!code_zoom_steps_from(code, code_zoom_step::actual_size), "Actual Size at 100% changes nothing");
		expect(stepped_code_zoom(code, code_zoom_step::out) == 90U, "Zoom Out from 100% is 90%");

		// Off the ladder, the nearer step in the gesture's direction.
		code.ZoomPercent = 115U;
		expect(stepped_code_zoom(code, code_zoom_step::in) == 125U && stepped_code_zoom(code, code_zoom_step::out) == 110U,
			   "a zoom between steps moves to the neighbouring step");

		// The clamp: a 40 pt code size reaches 72 pt at 200% and no further.
		code.Font.PointSizeCentipoints = 4000U;
		code.ZoomPercent               = 175U;
		expect(zoomed_point_size(code) == 7000U && stepped_code_zoom(code, code_zoom_step::in) == 200U,
			   "Zoom In steps while the size can still grow");
		code.ZoomPercent = 200U;
		expect(zoomed_point_size(code) == bounds_of(typography_field::point_size).Maximum
				   && !code_zoom_steps_from(code, code_zoom_step::in),
			   "Zoom In is spent once the size is at the point-size maximum");
		code.ZoomPercent = 300U;
		expect(stepped_code_zoom(code, code_zoom_step::out) == 175U,
			   "Zoom Out from a clamped zoom skips the steps that show the same 72 pt");
		code.Font.PointSizeCentipoints = 600U;
		code.ZoomPercent               = 90U;
		expect(zoomed_point_size(code) == 600U && !code_zoom_steps_from(code, code_zoom_step::out),
			   "a 6 pt code size cannot be zoomed below 6 pt");

		// Validation.
		auto settings             = typography_settings{ };
		settings.Ui.Font.Family   = "UI";
		settings.Code.Font.Family = "Code";
		for (const auto zoom : code_zoom_steps) {
			settings.Code.ZoomPercent = zoom;
			expect(validate_typography(settings).empty(), "every step is legal");
		}
		for (const auto& [zoom, reason] : std::array{ std::pair{ 49U, typography_reason::below_minimum }, std::pair{ 301U, typography_reason::above_maximum } }) {
			settings.Code.ZoomPercent = zoom;
			expect(validate_typography(settings) == std::vector{ typography_rejection{ .Target = typography_target::code_font, .Field = typography_field::code_zoom, .Reason = reason } },
				   "a zoom outside the ladder's ends is refused, naming the field");
		}
	}
} // namespace

int main()
{
	test_dif_preferences_resolve_conservatively();
	test_system_mode_follows_the_observed_scheme();
	test_high_contrast_preference_selects_or_recommends();
	test_resolution_is_total_and_self_consistent();
	test_mode_identifiers_round_trip();
	test_role_vocabulary_is_closed();
	test_the_core_invents_no_font_family();
	test_typography_bounds_are_enforced_exactly();
	test_every_violation_is_reported_in_order();
	test_white_space_drawing_follows_the_choice();
	test_white_space_vocabulary_is_closed();
	test_code_zoom_steps_scale_and_clamp();
	return EXIT_SUCCESS;
}
