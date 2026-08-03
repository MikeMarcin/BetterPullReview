#include "bpr/qt/appearance_profile.hpp"

#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace bpr::qt {
	namespace {
		using role = core::appearance_role;

		/// \brief Pairs one role with the colour a built-in profile gives it.
		struct role_color
		{
			role                   Role;
			core::appearance_color Color;
		};

		using profile_entries
			= std::array<role_color, core::appearance_role_count>;

		/// \brief Builds one opaque colour from a packed 0xRRGGBB value.
		[[nodiscard]] constexpr auto srgb(const std::uint32_t aValue) noexcept
			-> core::appearance_color
		{
			constexpr auto channelMask = std::uint32_t{ 0xFFU };
			return core::appearance_color{
				static_cast<std::uint8_t>((aValue >> 16U) & channelMask),
				static_cast<std::uint8_t>((aValue >> 8U) & channelMask),
				static_cast<std::uint8_t>(aValue & channelMask),
				255U
			};
		}

		/// \brief Builds one total profile palette from an ordered role table.
		///
		/// \details The palette starts fully transparent, which no built-in
		/// profile assigns, so an unassigned role stays detectable rather than
		/// silently becoming opaque black. The table must list every role of the
		/// closed vocabulary exactly once in declaration order; a table that does
		/// not is rejected while the profile is compiled, because this function
		/// can only be evaluated at compile time and a reached throw is not a
		/// constant expression.
		consteval auto make_profile(const profile_entries& aEntries)
			-> core::appearance_palette
		{
			auto palette = core::appearance_palette{ };
			palette.fill(core::appearance_color{ 0U, 0U, 0U, 0U });
			for (auto index = std::size_t{ 0U };
				 index < core::appearance_role_count;
				 ++index) {
				if (core::index_of(aEntries[index].Role) != index) {
					throw std::logic_error{
						"a built-in appearance profile must assign every role once in order"
					};
				}
				palette[index] = aEntries[index].Color;
			}
			return palette;
		}

		constexpr auto light_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0xF6F7F9) },
			{ role::application_text, srgb(0x1F2328) },
			{ role::code_background, srgb(0xFFFFFF) },
			{ role::code_gutter_background, srgb(0xF0F2F5) },
			{ role::code_line_number_text, srgb(0x5B6570) },
			{ role::code_selection_background, srgb(0xB6D7FF) },
			{ role::code_current_line_background, srgb(0xF2F6FC) },
			{ role::diff_added_background, srgb(0xE6FFEC) },
			{ role::diff_removed_background, srgb(0xFFEBE9) },
			{ role::diff_intraline_added_background, srgb(0xABF2BC) },
			{ role::diff_intraline_removed_background, srgb(0xFFC1BC) },
			{ role::diff_moved_background, srgb(0xFFF1D6) },
			{ role::diff_hunk_header_background, srgb(0xE7EEF9) },
			{ role::diff_hunk_header_text, srgb(0x3B5070) },
			{ role::state_unread, srgb(0x0969DA) },
			{ role::state_accepted, srgb(0x1A7F37) },
			{ role::state_defect, srgb(0xCF222E) },
			{ role::state_fixed, srgb(0x8250DF) },
			{ role::state_verified, srgb(0x0F7B6C) },
			{ role::state_stale, srgb(0x5B6570) },
			{ role::syntax_normal, srgb(0x1F2328) },
			{ role::syntax_keyword, srgb(0xCF222E) },
			{ role::syntax_control_flow, srgb(0xA40E26) },
			{ role::syntax_operator, srgb(0x24292F) },
			{ role::syntax_builtin, srgb(0x953800) },
			{ role::syntax_function, srgb(0x6639BA) },
			{ role::syntax_variable, srgb(0x24509B) },
			{ role::syntax_type, srgb(0x8250DF) },
			{ role::syntax_constant, srgb(0x0A3069) },
			{ role::syntax_number, srgb(0x0550AE) },
			{ role::syntax_string, srgb(0x0A7B54) },
			{ role::syntax_character, srgb(0x116329) },
			{ role::syntax_escape_sequence, srgb(0xB35900) },
			{ role::syntax_comment, srgb(0x5B6570) },
			{ role::syntax_documentation, srgb(0x4A6B57) },
			{ role::syntax_annotation, srgb(0x7D4E00) },
			{ role::syntax_preprocessor, srgb(0x7A3E9D) },
			{ role::syntax_import, srgb(0x1A7F37) },
			{ role::syntax_attribute, srgb(0x9A6700) },
			{ role::syntax_region_marker, srgb(0x57606A) },
			{ role::syntax_information, srgb(0x0969DA) },
			{ role::syntax_warning, srgb(0xBC4C00) },
			{ role::syntax_alert, srgb(0xB60A5B) },
			{ role::syntax_error, srgb(0xA40E11) },
			{ role::syntax_other, srgb(0x444D56) },
		} });

		constexpr auto dark_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0x1C2026) },
			{ role::application_text, srgb(0xE6EDF3) },
			{ role::code_background, srgb(0x0D1117) },
			{ role::code_gutter_background, srgb(0x141921) },
			{ role::code_line_number_text, srgb(0x8B949E) },
			{ role::code_selection_background, srgb(0x1F3B63) },
			{ role::code_current_line_background, srgb(0x161C24) },
			{ role::diff_added_background, srgb(0x0F2F1C) },
			{ role::diff_removed_background, srgb(0x3A1416) },
			{ role::diff_intraline_added_background, srgb(0x1B5C33) },
			{ role::diff_intraline_removed_background, srgb(0x6B1F24) },
			{ role::diff_moved_background, srgb(0x3A2E0C) },
			{ role::diff_hunk_header_background, srgb(0x18212F) },
			{ role::diff_hunk_header_text, srgb(0x9FB6D4) },
			{ role::state_unread, srgb(0x58A6FF) },
			{ role::state_accepted, srgb(0x3FB950) },
			{ role::state_defect, srgb(0xFF7B72) },
			{ role::state_fixed, srgb(0xBC8CFF) },
			{ role::state_verified, srgb(0x39C5B0) },
			{ role::state_stale, srgb(0x8B949E) },
			{ role::syntax_normal, srgb(0xE6EDF3) },
			{ role::syntax_keyword, srgb(0xFF7B72) },
			{ role::syntax_control_flow, srgb(0xFFA198) },
			{ role::syntax_operator, srgb(0xC9D1D9) },
			{ role::syntax_builtin, srgb(0xFFA657) },
			{ role::syntax_function, srgb(0xD2A8FF) },
			{ role::syntax_variable, srgb(0x79C0FF) },
			{ role::syntax_type, srgb(0xBC8CFF) },
			{ role::syntax_constant, srgb(0x56D4DD) },
			{ role::syntax_number, srgb(0xA5D6FF) },
			{ role::syntax_string, srgb(0x7EE787) },
			{ role::syntax_character, srgb(0x56D364) },
			{ role::syntax_escape_sequence, srgb(0xFFC180) },
			{ role::syntax_comment, srgb(0x8B949E) },
			{ role::syntax_documentation, srgb(0xA3C9AE) },
			{ role::syntax_annotation, srgb(0xE3B341) },
			{ role::syntax_preprocessor, srgb(0xD0A0E8) },
			{ role::syntax_import, srgb(0x3FB950) },
			{ role::syntax_attribute, srgb(0xE3C078) },
			{ role::syntax_region_marker, srgb(0x9EA7B3) },
			{ role::syntax_information, srgb(0x58A6FF) },
			{ role::syntax_warning, srgb(0xF0A03C) },
			{ role::syntax_alert, srgb(0xFF8AC7) },
			{ role::syntax_error, srgb(0xFF6A62) },
			{ role::syntax_other, srgb(0xB1BAC4) },
		} });

		constexpr auto high_contrast_light_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0xFFFFFF) },
			{ role::application_text, srgb(0x000000) },
			{ role::code_background, srgb(0xFFFFFF) },
			{ role::code_gutter_background, srgb(0xEBEBEB) },
			{ role::code_line_number_text, srgb(0x2B2B2B) },
			{ role::code_selection_background, srgb(0xFFD300) },
			{ role::code_current_line_background, srgb(0xF2F2F2) },
			{ role::diff_added_background, srgb(0xD6FFD6) },
			{ role::diff_removed_background, srgb(0xFFD9D9) },
			{ role::diff_intraline_added_background, srgb(0x8BF28B) },
			{ role::diff_intraline_removed_background, srgb(0xFF9E9E) },
			{ role::diff_moved_background, srgb(0xFFE9A8) },
			{ role::diff_hunk_header_background, srgb(0x000000) },
			{ role::diff_hunk_header_text, srgb(0xFFFFFF) },
			{ role::state_unread, srgb(0x0033CC) },
			{ role::state_accepted, srgb(0x005A1E) },
			{ role::state_defect, srgb(0xA50000) },
			{ role::state_fixed, srgb(0x5A009E) },
			{ role::state_verified, srgb(0x005050) },
			{ role::state_stale, srgb(0x3A3A3A) },
			{ role::syntax_normal, srgb(0x000000) },
			{ role::syntax_keyword, srgb(0xA50000) },
			{ role::syntax_control_flow, srgb(0x7A0000) },
			{ role::syntax_operator, srgb(0x000000) },
			{ role::syntax_builtin, srgb(0x7A3300) },
			{ role::syntax_function, srgb(0x4B0082) },
			{ role::syntax_variable, srgb(0x002A7A) },
			{ role::syntax_type, srgb(0x5A009E) },
			{ role::syntax_constant, srgb(0x001F5A) },
			{ role::syntax_number, srgb(0x002F99) },
			{ role::syntax_string, srgb(0x00452E) },
			{ role::syntax_character, srgb(0x003A14) },
			{ role::syntax_escape_sequence, srgb(0x7A3A00) },
			{ role::syntax_comment, srgb(0x3A3A3A) },
			{ role::syntax_documentation, srgb(0x1F402A) },
			{ role::syntax_annotation, srgb(0x5A3800) },
			{ role::syntax_preprocessor, srgb(0x440066) },
			{ role::syntax_import, srgb(0x005A1E) },
			{ role::syntax_attribute, srgb(0x604000) },
			{ role::syntax_region_marker, srgb(0x2B2B2B) },
			{ role::syntax_information, srgb(0x0033CC) },
			{ role::syntax_warning, srgb(0x7A3000) },
			{ role::syntax_alert, srgb(0x8B0045) },
			{ role::syntax_error, srgb(0x8B0000) },
			{ role::syntax_other, srgb(0x1A1A1A) },
		} });

		constexpr auto high_contrast_dark_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0x000000) },
			{ role::application_text, srgb(0xFFFFFF) },
			{ role::code_background, srgb(0x000000) },
			{ role::code_gutter_background, srgb(0x141414) },
			{ role::code_line_number_text, srgb(0xD6D6D6) },
			{ role::code_selection_background, srgb(0x004CB4) },
			{ role::code_current_line_background, srgb(0x1A1A1A) },
			{ role::diff_added_background, srgb(0x003300) },
			{ role::diff_removed_background, srgb(0x440000) },
			{ role::diff_intraline_added_background, srgb(0x006600) },
			{ role::diff_intraline_removed_background, srgb(0x8B0000) },
			{ role::diff_moved_background, srgb(0x4A3800) },
			{ role::diff_hunk_header_background, srgb(0xFFFFFF) },
			{ role::diff_hunk_header_text, srgb(0x000000) },
			{ role::state_unread, srgb(0x66B2FF) },
			{ role::state_accepted, srgb(0x4AE56B) },
			{ role::state_defect, srgb(0xFF6B6B) },
			{ role::state_fixed, srgb(0xD49BFF) },
			{ role::state_verified, srgb(0x3FE0C8) },
			{ role::state_stale, srgb(0xC8C8C8) },
			{ role::syntax_normal, srgb(0xFFFFFF) },
			{ role::syntax_keyword, srgb(0xFF8A80) },
			{ role::syntax_control_flow, srgb(0xFFB3AB) },
			{ role::syntax_operator, srgb(0xFFFFFF) },
			{ role::syntax_builtin, srgb(0xFFC166) },
			{ role::syntax_function, srgb(0xE0B8FF) },
			{ role::syntax_variable, srgb(0x8CCBFF) },
			{ role::syntax_type, srgb(0xD49BFF) },
			{ role::syntax_constant, srgb(0x66E6EE) },
			{ role::syntax_number, srgb(0xB8E0FF) },
			{ role::syntax_string, srgb(0x8CF595) },
			{ role::syntax_character, srgb(0x66E075) },
			{ role::syntax_escape_sequence, srgb(0xFFD199) },
			{ role::syntax_comment, srgb(0xC8C8C8) },
			{ role::syntax_documentation, srgb(0xBEE0C6) },
			{ role::syntax_annotation, srgb(0xF2C75C) },
			{ role::syntax_preprocessor, srgb(0xE6BCFF) },
			{ role::syntax_import, srgb(0x4AE56B) },
			{ role::syntax_attribute, srgb(0xF2D98C) },
			{ role::syntax_region_marker, srgb(0xD6D6D6) },
			{ role::syntax_information, srgb(0x66B2FF) },
			{ role::syntax_warning, srgb(0xFFBC66) },
			{ role::syntax_alert, srgb(0xFFA6D9) },
			{ role::syntax_error, srgb(0xFF8078) },
			{ role::syntax_other, srgb(0xE0E0E0) },
		} });

		constexpr auto srgb_linear_threshold = 0.040'45;
		constexpr auto srgb_linear_divisor   = 12.92;
		constexpr auto srgb_offset           = 0.055;
		constexpr auto srgb_scale            = 1.055;
		constexpr auto srgb_gamma            = 2.4;
		constexpr auto luminance_red         = 0.2126;
		constexpr auto luminance_green       = 0.7152;
		constexpr auto luminance_blue        = 0.0722;
		constexpr auto contrast_offset       = 0.05;
		constexpr auto channel_maximum       = 255.0;

		[[nodiscard]] auto linear_channel(const std::uint8_t aValue) noexcept
			-> double
		{
			const auto normalised = static_cast<double>(aValue) / channel_maximum;
			return normalised <= srgb_linear_threshold
				? normalised / srgb_linear_divisor
				: std::pow((normalised + srgb_offset) / srgb_scale, srgb_gamma);
		}

		[[nodiscard]] auto blend_midpoint(
			const QColor& aFirst,
			const QColor& aSecond) -> QColor
		{
			return QColor{
				(aFirst.red() + aSecond.red()) / 2,
				(aFirst.green() + aSecond.green()) / 2,
				(aFirst.blue() + aSecond.blue()) / 2,
				aFirst.alpha()
			};
		}

		[[nodiscard]] auto in_vocabulary(const role aRole) noexcept -> bool
		{
			return core::index_of(aRole) < core::appearance_role_count;
		}
	} // namespace

	auto to_presentation_color(const core::appearance_color aColor) noexcept
		-> QColor
	{
		return QColor{
			static_cast<int>(aColor.Red),
			static_cast<int>(aColor.Green),
			static_cast<int>(aColor.Blue),
			static_cast<int>(aColor.Alpha)
		};
	}

	auto to_appearance_color(const QColor& aColor) noexcept
		-> core::appearance_color
	{
		const auto opaque = aColor.toRgb();
		return core::appearance_color{
			static_cast<std::uint8_t>(opaque.red()),
			static_cast<std::uint8_t>(opaque.green()),
			static_cast<std::uint8_t>(opaque.blue()),
			static_cast<std::uint8_t>(opaque.alpha())
		};
	}

	auto relative_luminance(const core::appearance_color aColor) noexcept -> double
	{
		return (luminance_red * linear_channel(aColor.Red))
			+ (luminance_green * linear_channel(aColor.Green))
			+ (luminance_blue * linear_channel(aColor.Blue));
	}

	auto contrast_ratio(
		const core::appearance_color aFirst,
		const core::appearance_color aSecond) noexcept -> double
	{
		const auto first  = relative_luminance(aFirst) + contrast_offset;
		const auto second = relative_luminance(aSecond) + contrast_offset;
		return first >= second ? first / second : second / first;
	}

	auto builtin_appearance_palette(
		const core::appearance_variant aVariant) noexcept
		-> const core::appearance_palette&
	{
		switch (aVariant) {
		case core::appearance_variant::light:
			break;
		case core::appearance_variant::dark:
			return dark_profile;
		case core::appearance_variant::high_contrast_light:
			return high_contrast_light_profile;
		case core::appearance_variant::high_contrast_dark:
			return high_contrast_dark_profile;
		}
		return light_profile;
	}

	auto builtin_appearance_profile_name(const core::appearance_variant aVariant)
		-> QString
	{
		switch (aVariant) {
		case core::appearance_variant::light:
			break;
		case core::appearance_variant::dark:
			return QStringLiteral("BetterPullReview Dark");
		case core::appearance_variant::high_contrast_light:
			return QStringLiteral("BetterPullReview High Contrast Light");
		case core::appearance_variant::high_contrast_dark:
			return QStringLiteral("BetterPullReview High Contrast Dark");
		}
		return QStringLiteral("BetterPullReview Light");
	}

	auto default_appearance_settings() -> application::appearance_settings
	{
		auto settings    = application::appearance_settings{ };
		settings.Palette = builtin_appearance_palette(
			core::resolve_appearance(settings.Mode, core::appearance_observation{ })
				.Variant);
		return settings;
	}

	appearance_profile::appearance_profile(
		application::appearance_settings   aSettings,
		const core::appearance_observation aObservation)
		: Mode{ application::known(aSettings.Mode) ? aSettings.Mode
												   : core::appearance_mode::system }
		, Observation{ aObservation }
		, Typography{ std::move(aSettings.Typography) }
	{
		// The durable record carries exactly one palette, so it is read as
		// customisation relative to the built-in profile of the variant that
		// record's own mode resolves to. A fresh record, whose palette is that
		// profile, therefore carries no customisation at all.
		const auto& builtin = builtin_appearance_palette(
			core::resolve_appearance(Mode, Observation).Variant);
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			if (aSettings.Palette[index] != builtin[index]) {
				Overrides[index] = aSettings.Palette[index];
			}
		}
		rebuild();
	}

	void appearance_profile::set_sink(appearance_presentation_sink aSink)
	{
		Sink = std::move(aSink);
	}

	auto appearance_profile::mode() const noexcept -> core::appearance_mode
	{
		return Mode;
	}

	auto appearance_profile::observation() const noexcept
		-> core::appearance_observation
	{
		return Observation;
	}

	auto appearance_profile::resolution() const noexcept
		-> core::appearance_resolution
	{
		return Resolution;
	}

	auto appearance_profile::variant() const noexcept -> core::appearance_variant
	{
		return Resolution.Variant;
	}

	auto appearance_profile::profile_name() const -> QString
	{
		return builtin_appearance_profile_name(Resolution.Variant);
	}

	auto appearance_profile::palette() const noexcept
		-> const core::appearance_palette&
	{
		return Palette;
	}

	auto appearance_profile::color(const core::appearance_role aRole) const noexcept
		-> QColor
	{
		return in_vocabulary(aRole) ? Colors[core::index_of(aRole)] : QColor{ };
	}

	auto appearance_profile::customised(
		const core::appearance_role aRole) const noexcept -> bool
	{
		return in_vocabulary(aRole)
			&& Overrides[core::index_of(aRole)].has_value();
	}

	auto appearance_profile::customised_roles() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
			Overrides,
			[](const std::optional<core::appearance_color>& aOverride) {
				return aOverride.has_value();
			}));
	}

	auto appearance_profile::settings() const -> application::appearance_settings
	{
		auto settings       = application::appearance_settings{ };
		settings.Mode       = Mode;
		settings.Palette    = Palette;
		settings.Typography = Typography;
		return settings;
	}

	auto appearance_profile::generation() const noexcept -> std::uint64_t
	{
		return Generation;
	}

	auto appearance_profile::statistics() const noexcept
		-> appearance_profile_statistics
	{
		return Statistics;
	}

	auto appearance_profile::select_mode(const core::appearance_mode aMode) -> bool
	{
		if (!application::known(aMode) || aMode == Mode) {
			return false;
		}
		Mode = aMode;
		return adopt();
	}

	auto appearance_profile::observe(
		const core::appearance_observation aObservation) -> bool
	{
		if (aObservation == Observation) {
			return false;
		}
		Observation = aObservation;
		return adopt();
	}

	auto appearance_profile::customise(
		const core::appearance_role  aRole,
		const core::appearance_color aColor) -> bool
	{
		if (!in_vocabulary(aRole)) {
			return false;
		}
		auto& slot = Overrides[core::index_of(aRole)];
		if (slot.has_value() && *slot == aColor) {
			return false;
		}
		slot = aColor;
		return adopt();
	}

	auto appearance_profile::restore(const core::appearance_role aRole) -> bool
	{
		if (!customised(aRole)) {
			return false;
		}
		Overrides[core::index_of(aRole)].reset();
		return adopt();
	}

	auto appearance_profile::restore_all() -> bool
	{
		if (customised_roles() == 0U) {
			return false;
		}
		Overrides.fill(std::nullopt);
		return adopt();
	}

	void appearance_profile::refresh_native_appearance()
	{
		++Statistics.NativeRefreshes;
		++Generation;
		if (Sink) {
			Sink();
		}
	}

	auto appearance_profile::readable_text(
		const core::appearance_role aBackground) const noexcept -> QColor
	{
		if (!in_vocabulary(aBackground)) {
			return QColor{ };
		}
		const auto background = Palette[core::index_of(aBackground)];
		const auto text       = Palette[core::index_of(role::syntax_normal)];
		const auto inverse    = Palette[core::index_of(role::code_background)];
		return contrast_ratio(background, text) >= contrast_ratio(background, inverse)
			? color(role::syntax_normal)
			: color(role::code_background);
	}

	auto appearance_profile::derive(const QPalette& aInherited) const -> QPalette
	{
		auto derived = aInherited;

		const auto surface       = color(role::application_surface);
		const auto surfaceText   = color(role::application_text);
		const auto base          = color(role::code_background);
		const auto baseText      = color(role::syntax_normal);
		const auto alternateBase = color(role::code_current_line_background);
		const auto highlight     = color(role::code_selection_background);
		const auto highlightText = readable_text(role::code_selection_background);

		const auto assign
			= [&derived](const QPalette::ColorRole aRole, const QColor& aColor) {
				  derived.setColor(QPalette::Active, aRole, aColor);
				  derived.setColor(QPalette::Inactive, aRole, aColor);
			  };
		assign(QPalette::Window, surface);
		assign(QPalette::WindowText, surfaceText);
		assign(QPalette::Base, base);
		assign(QPalette::AlternateBase, alternateBase);
		assign(QPalette::Text, baseText);
		assign(QPalette::Highlight, highlight);
		assign(QPalette::HighlightedText, highlightText);

		derived.setColor(QPalette::Disabled, QPalette::Window, surface);
		derived.setColor(QPalette::Disabled, QPalette::Base, base);
		derived.setColor(QPalette::Disabled, QPalette::AlternateBase, alternateBase);
		derived.setColor(QPalette::Disabled, QPalette::Highlight, highlight);
		derived.setColor(
			QPalette::Disabled,
			QPalette::WindowText,
			blend_midpoint(surfaceText, surface));
		derived.setColor(
			QPalette::Disabled,
			QPalette::Text,
			blend_midpoint(baseText, base));
		derived.setColor(
			QPalette::Disabled,
			QPalette::HighlightedText,
			blend_midpoint(highlightText, highlight));
		return derived;
	}

	void appearance_profile::apply_to(QWidget& aSurface)
	{
		aSurface.setPalette(derive(aSurface.palette()));
		++Statistics.AppliedSurfaces;
	}

	void appearance_profile::rebuild()
	{
		Resolution          = core::resolve_appearance(Mode, Observation);
		const auto& builtin = builtin_appearance_palette(Resolution.Variant);
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			Palette[index] = Overrides[index].value_or(builtin[index]);
			Colors[index]  = to_presentation_color(Palette[index]);
		}
	}

	auto appearance_profile::adopt() -> bool
	{
		const auto previousResolution = Resolution;
		const auto previousPalette    = Palette;
		rebuild();
		if (Resolution == previousResolution && Palette == previousPalette) {
			return false;
		}
		++Statistics.PresentationChanges;
		++Generation;
		if (Sink) {
			Sink();
		}
		return true;
	}
} // namespace bpr::qt
