#include "bpr/core/appearance_model.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>

namespace bpr::core {
	namespace {
		/// \brief Finds the vocabulary member carrying the supplied identifier.
		///
		/// \details The scan is linear over a closed vocabulary of fixed size, so
		/// the lookup and the identifier it matches cannot drift apart the way a
		/// second hand-written table would.
		template <typename TValue>
		[[nodiscard]] auto find_by_stable_name(
			const std::string_view aName,
			const std::size_t      aCount) noexcept -> std::optional<TValue>
		{
			for (std::size_t index = 0U; index < aCount; ++index) {
				const auto candidate = static_cast<TValue>(static_cast<std::uint8_t>(index));
				if (stable_name(candidate) == aName) {
					return candidate;
				}
			}

			return std::nullopt;
		}

		[[nodiscard]] auto high_contrast_mode(const appearance_mode aMode) noexcept -> bool
		{
			return aMode == appearance_mode::high_contrast_system
				|| aMode == appearance_mode::high_contrast_light
				|| aMode == appearance_mode::high_contrast_dark;
		}

		[[nodiscard]] auto dark_family(
			const appearance_mode         aMode,
			const appearance_observation& aObservation) noexcept -> bool
		{
			if (follows_observed_scheme(aMode)) {
				return aObservation.Scheme == os_color_scheme::dark;
			}

			return aMode == appearance_mode::dark
				|| aMode == appearance_mode::high_contrast_dark;
		}

		[[nodiscard]] auto variant_of(
			const bool aDark,
			const bool aHighContrast) noexcept -> appearance_variant
		{
			if (aHighContrast) {
				return aDark
					? appearance_variant::high_contrast_dark
					: appearance_variant::high_contrast_light;
			}

			return aDark ? appearance_variant::dark : appearance_variant::light;
		}

		[[nodiscard]] auto scheme_source_of(
			const appearance_mode         aMode,
			const appearance_observation& aObservation) noexcept -> appearance_scheme_source
		{
			if (!follows_observed_scheme(aMode)) {
				return appearance_scheme_source::selected_mode;
			}
			if (aObservation.Scheme == os_color_scheme::unknown) {
				return appearance_scheme_source::unobserved_default;
			}

			return appearance_scheme_source::observed_scheme;
		}

		[[nodiscard]] auto contrast_source_of(
			const bool aSelectedHighContrast,
			const bool aAdoptedPreference) noexcept -> appearance_contrast_source
		{
			if (aSelectedHighContrast) {
				return appearance_contrast_source::selected_mode;
			}
			if (aAdoptedPreference) {
				return appearance_contrast_source::observed_preference;
			}

			return appearance_contrast_source::none;
		}

		[[nodiscard]] auto malformed_family(const std::string_view aFamily) noexcept -> bool
		{
			// A family name is opaque to the core, but a control byte in one
			// cannot be a platform identifier and would corrupt every exchange
			// format the host later writes it into.
			return std::ranges::any_of(aFamily, [](const char aCharacter) noexcept {
				const auto value = static_cast<unsigned int>(
					static_cast<unsigned char>(aCharacter));
				return value < 0x20U || value == 0x7FU;
			});
		}

		[[nodiscard]] auto check_bounds(
			const typography_target aTarget,
			const typography_field  aField,
			const std::uint32_t     aValue) -> std::optional<typography_rejection>
		{
			const auto bounds = bounds_of(aField);
			if (aValue < bounds.Minimum) {
				return typography_rejection{
					.Target = aTarget,
					.Field  = aField,
					.Reason = typography_reason::below_minimum,
				};
			}
			if (aValue > bounds.Maximum) {
				return typography_rejection{
					.Target = aTarget,
					.Field  = aField,
					.Reason = typography_reason::above_maximum,
				};
			}

			return std::nullopt;
		}

		[[nodiscard]] auto check_family(
			const typography_target aTarget,
			const std::string&      aFamily) -> std::optional<typography_rejection>
		{
			const auto bounds = bounds_of(typography_field::family);
			const auto length = aFamily.size();
			if (length < bounds.Minimum) {
				return typography_rejection{
					.Target = aTarget,
					.Field  = typography_field::family,
					.Reason = typography_reason::below_minimum,
				};
			}
			if (length > bounds.Maximum) {
				return typography_rejection{
					.Target = aTarget,
					.Field  = typography_field::family,
					.Reason = typography_reason::above_maximum,
				};
			}
			if (malformed_family(aFamily)) {
				return typography_rejection{
					.Target = aTarget,
					.Field  = typography_field::family,
					.Reason = typography_reason::malformed_value,
				};
			}

			return std::nullopt;
		}

		auto append_rejection(
			std::vector<typography_rejection>&         aRejections,
			const std::optional<typography_rejection>& aRejection) -> void
		{
			if (aRejection.has_value()) {
				aRejections.push_back(*aRejection);
			}
		}

		auto append_font_rejections(
			std::vector<typography_rejection>& aRejections,
			const typography_target            aTarget,
			const font_settings&               aFont) -> void
		{
			append_rejection(aRejections, check_family(aTarget, aFont.Family));
			append_rejection(
				aRejections,
				check_bounds(aTarget, typography_field::point_size, aFont.PointSizeCentipoints));
			append_rejection(
				aRejections,
				check_bounds(aTarget, typography_field::weight, aFont.Weight));
		}
	} // namespace

	auto stable_name(const appearance_role aRole) noexcept -> std::string_view
	{
		switch (aRole) {
		case appearance_role::application_surface:
			return "application_surface";
		case appearance_role::application_text:
			return "application_text";
		case appearance_role::code_background:
			return "code_background";
		case appearance_role::code_gutter_background:
			return "code_gutter_background";
		case appearance_role::code_line_number_text:
			return "code_line_number_text";
		case appearance_role::code_selection_background:
			return "code_selection_background";
		case appearance_role::code_current_line_background:
			return "code_current_line_background";
		case appearance_role::diff_added_background:
			return "diff_added_background";
		case appearance_role::diff_removed_background:
			return "diff_removed_background";
		case appearance_role::diff_intraline_added_background:
			return "diff_intraline_added_background";
		case appearance_role::diff_intraline_removed_background:
			return "diff_intraline_removed_background";
		case appearance_role::diff_moved_background:
			return "diff_moved_background";
		case appearance_role::diff_hunk_header_background:
			return "diff_hunk_header_background";
		case appearance_role::diff_group_boundary_rule:
			return "diff_group_boundary_rule";
		case appearance_role::diff_hunk_header_text:
			return "diff_hunk_header_text";
		case appearance_role::state_unread:
			return "state_unread";
		case appearance_role::state_accepted:
			return "state_accepted";
		case appearance_role::state_defect:
			return "state_defect";
		case appearance_role::state_fixed:
			return "state_fixed";
		case appearance_role::state_verified:
			return "state_verified";
		case appearance_role::state_stale:
			return "state_stale";
		case appearance_role::syntax_normal:
			return "syntax_normal";
		case appearance_role::syntax_keyword:
			return "syntax_keyword";
		case appearance_role::syntax_control_flow:
			return "syntax_control_flow";
		case appearance_role::syntax_operator:
			return "syntax_operator";
		case appearance_role::syntax_builtin:
			return "syntax_builtin";
		case appearance_role::syntax_function:
			return "syntax_function";
		case appearance_role::syntax_variable:
			return "syntax_variable";
		case appearance_role::syntax_type:
			return "syntax_type";
		case appearance_role::syntax_constant:
			return "syntax_constant";
		case appearance_role::syntax_number:
			return "syntax_number";
		case appearance_role::syntax_string:
			return "syntax_string";
		case appearance_role::syntax_character:
			return "syntax_character";
		case appearance_role::syntax_escape_sequence:
			return "syntax_escape_sequence";
		case appearance_role::syntax_comment:
			return "syntax_comment";
		case appearance_role::syntax_documentation:
			return "syntax_documentation";
		case appearance_role::syntax_annotation:
			return "syntax_annotation";
		case appearance_role::syntax_macro:
			return "syntax_macro";
		case appearance_role::syntax_preprocessor:
			return "syntax_preprocessor";
		case appearance_role::syntax_import:
			return "syntax_import";
		case appearance_role::syntax_attribute:
			return "syntax_attribute";
		case appearance_role::syntax_region_marker:
			return "syntax_region_marker";
		case appearance_role::syntax_information:
			return "syntax_information";
		case appearance_role::syntax_warning:
			return "syntax_warning";
		case appearance_role::syntax_alert:
			return "syntax_alert";
		case appearance_role::syntax_error:
			return "syntax_error";
		case appearance_role::syntax_other:
			return "syntax_other";
		case appearance_role::feedback_label_praise:
			return "feedback_label_praise";
		case appearance_role::feedback_label_nitpick:
			return "feedback_label_nitpick";
		case appearance_role::feedback_label_suggestion:
			return "feedback_label_suggestion";
		case appearance_role::feedback_label_issue:
			return "feedback_label_issue";
		case appearance_role::feedback_label_todo:
			return "feedback_label_todo";
		case appearance_role::feedback_label_question:
			return "feedback_label_question";
		case appearance_role::feedback_label_thought:
			return "feedback_label_thought";
		case appearance_role::feedback_label_chore:
			return "feedback_label_chore";
		case appearance_role::feedback_label_note:
			return "feedback_label_note";
		case appearance_role::feedback_label_typo:
			return "feedback_label_typo";
		case appearance_role::feedback_label_polish:
			return "feedback_label_polish";
		case appearance_role::feedback_label_quibble:
			return "feedback_label_quibble";
		case appearance_role::syntax_namespace:
			return "syntax_namespace";
		case appearance_role::syntax_enum_member:
			return "syntax_enum_member";
		case appearance_role::find_match_background:
			return "find_match_background";
		case appearance_role::find_current_match_background:
			return "find_current_match_background";
		case appearance_role::state_attention:
			return "state_attention";
		}

		return { };
	}

	auto find_appearance_role(const std::string_view aName) noexcept
		-> std::optional<appearance_role>
	{
		return find_by_stable_name<appearance_role>(aName, appearance_role_count);
	}

	auto stable_name(const appearance_mode aMode) noexcept -> std::string_view
	{
		switch (aMode) {
		case appearance_mode::system:
			return "system";
		case appearance_mode::light:
			return "light";
		case appearance_mode::dark:
			return "dark";
		case appearance_mode::high_contrast_system:
			return "high_contrast_system";
		case appearance_mode::high_contrast_light:
			return "high_contrast_light";
		case appearance_mode::high_contrast_dark:
			return "high_contrast_dark";
		}

		return { };
	}

	auto find_appearance_mode(const std::string_view aName) noexcept
		-> std::optional<appearance_mode>
	{
		return find_by_stable_name<appearance_mode>(aName, appearance_mode_count);
	}

	auto stable_name(const appearance_variant aVariant) noexcept -> std::string_view
	{
		switch (aVariant) {
		case appearance_variant::light:
			return "light";
		case appearance_variant::dark:
			return "dark";
		case appearance_variant::high_contrast_light:
			return "high_contrast_light";
		case appearance_variant::high_contrast_dark:
			return "high_contrast_dark";
		}

		return { };
	}

	auto find_appearance_variant(const std::string_view aName) noexcept
		-> std::optional<appearance_variant>
	{
		return find_by_stable_name<appearance_variant>(
			aName, appearance_variant_count);
	}

	auto high_contrast(const appearance_variant aVariant) noexcept -> bool
	{
		return aVariant == appearance_variant::high_contrast_light
			|| aVariant == appearance_variant::high_contrast_dark;
	}

	auto follows_observed_scheme(const appearance_mode aMode) noexcept -> bool
	{
		return aMode == appearance_mode::system
			|| aMode == appearance_mode::high_contrast_system;
	}

	auto resolve_appearance(
		const appearance_mode         aMode,
		const appearance_observation& aObservation) noexcept -> appearance_resolution
	{
		const auto selectedHighContrast = high_contrast_mode(aMode);
		const auto observedHighContrast = aObservation.Contrast == os_contrast_preference::high;
		// Only a scheme-following mode adopts the operating-system preference.
		// An explicit standard-contrast mode is a decision the core will not
		// overrule, so the preference becomes a recommendation instead.
		const auto adoptedPreference = follows_observed_scheme(aMode)
			&& observedHighContrast
			&& !selectedHighContrast;
		const auto resolvedHighContrast = selectedHighContrast || adoptedPreference;

		return appearance_resolution{
			.Variant                 = variant_of(dark_family(aMode, aObservation), resolvedHighContrast),
			.Scheme                  = scheme_source_of(aMode, aObservation),
			.Contrast                = contrast_source_of(selectedHighContrast, adoptedPreference),
			.HighContrastRecommended = observedHighContrast && !resolvedHighContrast,
		};
	}

	auto bounds_of(const typography_field aField) noexcept -> typography_bounds
	{
		switch (aField) {
		case typography_field::family:
			// Bytes of the opaque platform identifier.
			return typography_bounds{ .Minimum = 1U, .Maximum = 128U };
		case typography_field::point_size:
			// 6.00pt to 72.00pt in hundredths of a point.
			return typography_bounds{ .Minimum = 600U, .Maximum = 7200U };
		case typography_field::weight:
			return typography_bounds{ .Minimum = 100U, .Maximum = 900U };
		case typography_field::line_height:
			// 0.80x to 3.00x as a percentage of the natural line height.
			return typography_bounds{ .Minimum = 80U, .Maximum = 300U };
		case typography_field::tab_width:
			return typography_bounds{ .Minimum = 1U, .Maximum = 16U };
		case typography_field::white_space:
			return typography_bounds{ .Minimum = 0U, .Maximum = static_cast<std::uint32_t>(white_space_drawing_count - 1U) };
		case typography_field::code_zoom:
			// [[BPR-1336]]. Percent of the code size: the ends of the step ladder.
			return typography_bounds{ .Minimum = code_zoom_steps.front(), .Maximum = code_zoom_steps.back() };
		}

		return typography_bounds{ };
	}

	auto stable_name(const white_space_drawing aDrawing) noexcept -> std::string_view
	{
		switch (aDrawing) {
		case white_space_drawing::none:
			return "none";
		case white_space_drawing::selection:
			return "selection";
		case white_space_drawing::leading:
			return "leading";
		case white_space_drawing::trailing:
			return "trailing";
		case white_space_drawing::boundary:
			return "boundary";
		case white_space_drawing::all:
			return "all";
		}
		return { };
	}

	auto validate_typography(const typography_settings& aSettings)
		-> std::vector<typography_rejection>
	{
		auto rejections = std::vector<typography_rejection>{ };
		append_font_rejections(
			rejections, typography_target::ui_font, aSettings.Ui.Font);
		// Line height is checked here rather than inside the shared helper
		// because only the UI font has one. The code font cannot carry a line
		// height at all, so there is no code-font value to check and no way to
		// write one that would be silently ignored.
		append_rejection(
			rejections,
			check_bounds(
				typography_target::ui_font,
				typography_field::line_height,
				aSettings.Ui.LineHeightPercent));
		append_font_rejections(rejections, typography_target::code_font, aSettings.Code.Font);
		append_rejection(
			rejections,
			check_bounds(
				typography_target::code_font,
				typography_field::tab_width,
				aSettings.Code.TabWidth));
		// [[BPR-1107]]. A closed choice, so the only illegal value is one outside
		// it, which only a cast can produce.
		if (!valid(aSettings.Code.WhiteSpace)) {
			rejections.push_back(typography_rejection{
				.Target = typography_target::code_font,
				.Field  = typography_field::white_space,
				.Reason = typography_reason::malformed_value,
			});
		}
		append_rejection(
			rejections,
			check_bounds(
				typography_target::code_font,
				typography_field::code_zoom,
				aSettings.Code.ZoomPercent));
		return rejections;
	}

	auto zoomed_point_size(const code_font_settings& aCode) noexcept -> std::uint32_t
	{
		const auto bounds = bounds_of(typography_field::point_size);
		const auto scaled = (static_cast<std::uint64_t>(aCode.Font.PointSizeCentipoints) * aCode.ZoomPercent + 50U) / 100U;
		return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(scaled, bounds.Minimum, bounds.Maximum));
	}

	auto stepped_code_zoom(const code_font_settings& aCode, const code_zoom_step aStep) noexcept -> std::uint32_t
	{
		if (aStep == code_zoom_step::actual_size) {
			return actual_code_zoom_percent;
		}
		const auto shown = zoomed_point_size(aCode);
		auto       probe = aCode;
		const auto moves = [&](const std::uint32_t aZoom) {
			probe.ZoomPercent = aZoom;
			const auto size   = zoomed_point_size(probe);
			return aStep == code_zoom_step::in ? size > shown : size < shown;
		};
		if (aStep == code_zoom_step::in) {
			const auto found = std::ranges::find_if(code_zoom_steps, [&](const std::uint32_t aZoom) {
				return aZoom > aCode.ZoomPercent && moves(aZoom);
			});
			return found == code_zoom_steps.end() ? aCode.ZoomPercent : *found;
		}
		const auto found = std::ranges::find_if(code_zoom_steps | std::views::reverse, [&](const std::uint32_t aZoom) {
			return aZoom < aCode.ZoomPercent && moves(aZoom);
		});
		return found == (code_zoom_steps | std::views::reverse).end() ? aCode.ZoomPercent : *found;
	}

	auto code_zoom_steps_from(const code_font_settings& aCode, const code_zoom_step aStep) noexcept -> bool
	{
		return stepped_code_zoom(aCode, aStep) != aCode.ZoomPercent;
	}
} // namespace bpr::core
