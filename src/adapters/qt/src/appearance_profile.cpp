#include "bpr/qt/appearance_profile.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/bundled_code_font.hpp"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QPlainTextEdit>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
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
			{ role::code_selection_background, srgb(0x7FB0E8) },
			{ role::code_current_line_background, srgb(0xF2F6FC) },
			{ role::diff_added_background, srgb(0xE6FFEC) },
			{ role::diff_removed_background, srgb(0xFFEBE9) },
			{ role::diff_intraline_added_background, srgb(0x9EFFB5) },
			{ role::diff_intraline_removed_background, srgb(0xF1B9B4) },
			{ role::diff_moved_background, srgb(0xFFF1D6) },
			{ role::diff_hunk_header_background, srgb(0xE7EEF9) },
			{ role::diff_hunk_header_text, srgb(0x3B5070) },
			{ role::diff_group_boundary_rule, srgb(0x8C8C8C) },
			{ role::state_unread, srgb(0x0969DA) },
			{ role::state_accepted, srgb(0x1A7F37) },
			{ role::state_defect, srgb(0xCF222E) },
			{ role::state_fixed, srgb(0x8250DF) },
			{ role::state_verified, srgb(0x0F7B6C) },
			{ role::state_stale, srgb(0x5B6570) },
			{ role::syntax_normal, srgb(0x1F2328) },
			{ role::syntax_keyword, srgb(0xB51D27) },
			{ role::syntax_control_flow, srgb(0xA40E26) },
			{ role::syntax_operator, srgb(0x24292F) },
			{ role::syntax_builtin, srgb(0x953800) },
			{ role::syntax_function, srgb(0x6639BA) },
			{ role::syntax_variable, srgb(0x24509B) },
			{ role::syntax_type, srgb(0x6E3EC8) },
			{ role::syntax_constant, srgb(0x0A3069) },
			{ role::syntax_number, srgb(0x0550AE) },
			{ role::syntax_string, srgb(0x086A49) },
			{ role::syntax_character, srgb(0x116329) },
			{ role::syntax_escape_sequence, srgb(0x944800) },
			{ role::syntax_comment, srgb(0x555E69) },
			{ role::syntax_documentation, srgb(0x456351) },
			{ role::syntax_annotation, srgb(0x7D4E00) },
			{ role::syntax_macro, srgb(0x7A3E9D) },
			{ role::syntax_preprocessor, srgb(0x7A3E9D) },
			{ role::syntax_import, srgb(0x156B2E) },
			{ role::syntax_attribute, srgb(0x7E5500) },
			{ role::syntax_region_marker, srgb(0x555E68) },
			{ role::syntax_information, srgb(0x0759BC) },
			{ role::syntax_warning, srgb(0x9F3F00) },
			{ role::syntax_alert, srgb(0xB60A5B) },
			{ role::syntax_error, srgb(0xA40E11) },
			{ role::syntax_other, srgb(0x444D56) },
			// [[BPR-1083]]. The label tints, BPR-1079 `tints-v002` (owner, v2).
			{ role::feedback_label_praise, srgb(0xBF3989) },
			{ role::feedback_label_nitpick, srgb(0x6E7781) },
			{ role::feedback_label_suggestion, srgb(0x1F6FD0) },
			{ role::feedback_label_issue, srgb(0xD1242F) },
			{ role::feedback_label_todo, srgb(0xC24E00) },
			{ role::feedback_label_question, srgb(0x8250DF) },
			{ role::feedback_label_thought, srgb(0x4F5BD5) },
			{ role::feedback_label_chore, srgb(0x8A5A2B) },
			{ role::feedback_label_note, srgb(0x0A7EA4) },
			{ role::feedback_label_typo, srgb(0x6F7A00) },
			{ role::feedback_label_polish, srgb(0x0F8577) },
			{ role::feedback_label_quibble, srgb(0x9A5B7A) },
			// [[BPR-1085]]. The attention amber: at least 3:1 against the
			// application surface (a mark), GitHub's amber where that holds.
			{ role::state_attention, srgb(0xAD7A00) },
		} });

		constexpr auto dark_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0x1C2026) },
			{ role::application_text, srgb(0xE6EDF3) },
			{ role::code_background, srgb(0x0D1117) },
			{ role::code_gutter_background, srgb(0x141921) },
			{ role::code_line_number_text, srgb(0x8B949E) },
			{ role::code_selection_background, srgb(0x28518C) },
			{ role::code_current_line_background, srgb(0x161C24) },
			{ role::diff_added_background, srgb(0x153622) },
			{ role::diff_removed_background, srgb(0x3E1F1F) },
			{ role::diff_intraline_added_background, srgb(0x104A17) },
			{ role::diff_intraline_removed_background, srgb(0x672323) },
			{ role::diff_moved_background, srgb(0x2D1900) },
			{ role::diff_hunk_header_background, srgb(0x18212F) },
			{ role::diff_hunk_header_text, srgb(0x9FB6D4) },
			{ role::diff_group_boundary_rule, srgb(0x808080) },
			{ role::state_unread, srgb(0x58A6FF) },
			{ role::state_accepted, srgb(0x3FB950) },
			{ role::state_defect, srgb(0xFF7B72) },
			{ role::state_fixed, srgb(0xBC8CFF) },
			{ role::state_verified, srgb(0x39C5B0) },
			{ role::state_stale, srgb(0x8B949E) },
			// The syntax roles follow Visual Studio's dark theme with the
			// Visual Assist colouring the owner asked for ([[BPR-0720]],
			// owner direction 2026-09-11, corrected on [[BPR-0738]] from a
			// per-token sample of the owner's reference): green comments,
			// grey preprocessor directives, blue keywords, gold class and
			// struct types (#FFD700), orange functions (#FF8000), purple
			// macros, khaki variables (#BDB76B), teal attributes
			// (#4EC9B0), salmon strings. Four of the reference values
			// (keyword #569CD6, comment #57A64A, preprocessor #9B9B9B,
			// macro #BD63C5) fall under 4.5:1 on this product's added and
			// removed row fills, so they are lightened along their own hue
			// to the first value that clears the floor.
			// [[BPR-0744]] narrowed the directive grey to the directive
			// itself: an ALL_CAPS argument of a directive is classified as
			// a macro and takes the purple below, and a C or C++ include
			// path is classified as a string and takes the salmon below.
			// No value moved for that; the import grey keeps naming
			// `import`, `from` and `@import` in the other grammars.
			// The four note roles below the region marker are not part of
			// that scheme and keep their colours.
			{ role::syntax_normal, srgb(0xDCDCDC) },
			{ role::syntax_keyword, srgb(0x6DA9DB) },
			{ role::syntax_control_flow, srgb(0xD8A0DF) },
			{ role::syntax_operator, srgb(0xB4B4B4) },
			{ role::syntax_builtin, srgb(0x6DA9DB) },
			{ role::syntax_function, srgb(0xFF8000) },
			{ role::syntax_variable, srgb(0xBDB76B) },
			{ role::syntax_type, srgb(0xFFD700) },
			{ role::syntax_constant, srgb(0xB8D7A3) },
			{ role::syntax_number, srgb(0xB5CEA8) },
			{ role::syntax_string, srgb(0xD69D85) },
			{ role::syntax_character, srgb(0xD69D85) },
			{ role::syntax_escape_sequence, srgb(0xFFD68F) },
			{ role::syntax_comment, srgb(0x66B559) },
			{ role::syntax_documentation, srgb(0x66B559) },
			{ role::syntax_annotation, srgb(0x8FBF72) },
			{ role::syntax_macro, srgb(0xCE8AD4) },
			{ role::syntax_preprocessor, srgb(0xA3A3A3) },
			{ role::syntax_import, srgb(0xA3A3A3) },
			{ role::syntax_attribute, srgb(0x4EC9B0) },
			{ role::syntax_region_marker, srgb(0xA3A3A3) },
			{ role::syntax_information, srgb(0x60B5FF) },
			{ role::syntax_warning, srgb(0xF0A03C) },
			{ role::syntax_alert, srgb(0xFF8AC7) },
			{ role::syntax_error, srgb(0xF39B73) },
			{ role::syntax_other, srgb(0xB1BAC4) },
			{ role::feedback_label_praise, srgb(0xF778BA) },
			{ role::feedback_label_nitpick, srgb(0x9BA4AE) },
			{ role::feedback_label_suggestion, srgb(0x6CB0FF) },
			{ role::feedback_label_issue, srgb(0xFF7B72) },
			{ role::feedback_label_todo, srgb(0xFFA05A) },
			{ role::feedback_label_question, srgb(0xC29BFF) },
			{ role::feedback_label_thought, srgb(0x9AA5FF) },
			{ role::feedback_label_chore, srgb(0xD2A26E) },
			{ role::feedback_label_note, srgb(0x4DC3E8) },
			{ role::feedback_label_typo, srgb(0xC3CE4A) },
			{ role::feedback_label_polish, srgb(0x3FCDB8) },
			{ role::feedback_label_quibble, srgb(0xD99BBB) },
			{ role::state_attention, srgb(0xD29922) },
		} });

		constexpr auto high_contrast_light_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0xFFFFFF) },
			{ role::application_text, srgb(0x000000) },
			{ role::code_background, srgb(0xFFFFFF) },
			{ role::code_gutter_background, srgb(0xEBEBEB) },
			{ role::code_line_number_text, srgb(0x2B2B2B) },
			{ role::code_selection_background, srgb(0xFFE877) },
			{ role::code_current_line_background, srgb(0xF2F2F2) },
			{ role::diff_added_background, srgb(0xD6FFD6) },
			{ role::diff_removed_background, srgb(0xFFEAEA) },
			{ role::diff_intraline_added_background, srgb(0x55FF55) },
			{ role::diff_intraline_removed_background, srgb(0xFF9999) },
			{ role::diff_moved_background, srgb(0xFFE9A8) },
			{ role::diff_hunk_header_background, srgb(0x000000) },
			{ role::diff_hunk_header_text, srgb(0xFFFFFF) },
			{ role::diff_group_boundary_rule, srgb(0x000000) },
			{ role::state_unread, srgb(0x0033CC) },
			{ role::state_accepted, srgb(0x005A1E) },
			{ role::state_defect, srgb(0xA50000) },
			{ role::state_fixed, srgb(0x5A009E) },
			{ role::state_verified, srgb(0x005050) },
			{ role::state_stale, srgb(0x3A3A3A) },
			{ role::syntax_normal, srgb(0x000000) },
			{ role::syntax_keyword, srgb(0x9C0000) },
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
			{ role::syntax_escape_sequence, srgb(0x793A00) },
			{ role::syntax_comment, srgb(0x3A3A3A) },
			{ role::syntax_documentation, srgb(0x1F402A) },
			{ role::syntax_annotation, srgb(0x5A3800) },
			{ role::syntax_macro, srgb(0x440066) },
			{ role::syntax_preprocessor, srgb(0x440066) },
			{ role::syntax_import, srgb(0x00581D) },
			{ role::syntax_attribute, srgb(0x604000) },
			{ role::syntax_region_marker, srgb(0x2B2B2B) },
			{ role::syntax_information, srgb(0x0033CC) },
			{ role::syntax_warning, srgb(0x7A3000) },
			{ role::syntax_alert, srgb(0x8B0045) },
			{ role::syntax_error, srgb(0x8B0000) },
			{ role::syntax_other, srgb(0x1A1A1A) },
			{ role::feedback_label_praise, srgb(0x8A0F5C) },
			{ role::feedback_label_nitpick, srgb(0x3A3F45) },
			{ role::feedback_label_suggestion, srgb(0x0033AA) },
			{ role::feedback_label_issue, srgb(0xA50000) },
			{ role::feedback_label_todo, srgb(0x7A2E00) },
			{ role::feedback_label_question, srgb(0x5A009E) },
			{ role::feedback_label_thought, srgb(0x2A2F9E) },
			{ role::feedback_label_chore, srgb(0x5A3510) },
			{ role::feedback_label_note, srgb(0x004F6B) },
			{ role::feedback_label_typo, srgb(0x434A00) },
			{ role::feedback_label_polish, srgb(0x005048) },
			{ role::feedback_label_quibble, srgb(0x5E2A44) },
			{ role::state_attention, srgb(0x8A5300) },
		} });

		constexpr auto high_contrast_dark_profile = make_profile(profile_entries{ {
			{ role::application_surface, srgb(0x000000) },
			{ role::application_text, srgb(0xFFFFFF) },
			{ role::code_background, srgb(0x000000) },
			{ role::code_gutter_background, srgb(0x141414) },
			{ role::code_line_number_text, srgb(0xD6D6D6) },
			{ role::code_selection_background, srgb(0x002D6C) },
			{ role::code_current_line_background, srgb(0x1A1A1A) },
			{ role::diff_added_background, srgb(0x003300) },
			{ role::diff_removed_background, srgb(0x440000) },
			{ role::diff_intraline_added_background, srgb(0x55FF55) },
			{ role::diff_intraline_removed_background, srgb(0xFF9999) },
			{ role::diff_moved_background, srgb(0x3D2E00) },
			{ role::diff_hunk_header_background, srgb(0xFFFFFF) },
			{ role::diff_hunk_header_text, srgb(0x000000) },
			{ role::diff_group_boundary_rule, srgb(0xFFFFFF) },
			{ role::state_unread, srgb(0x66B2FF) },
			{ role::state_accepted, srgb(0x4AE56B) },
			{ role::state_defect, srgb(0xFF6B6B) },
			{ role::state_fixed, srgb(0xD49BFF) },
			{ role::state_verified, srgb(0x3FE0C8) },
			{ role::state_stale, srgb(0xC8C8C8) },
			{ role::syntax_normal, srgb(0xFFFFFF) },
			{ role::syntax_keyword, srgb(0xFFA69E) },
			{ role::syntax_control_flow, srgb(0xFFB3AB) },
			{ role::syntax_operator, srgb(0xFFFFFF) },
			{ role::syntax_builtin, srgb(0xFFC166) },
			{ role::syntax_function, srgb(0xE0B8FF) },
			{ role::syntax_variable, srgb(0x8CCBFF) },
			{ role::syntax_type, srgb(0xDBAAFF) },
			{ role::syntax_constant, srgb(0x66E6EE) },
			{ role::syntax_number, srgb(0xB8E0FF) },
			{ role::syntax_string, srgb(0x8CF595) },
			{ role::syntax_character, srgb(0x66E075) },
			{ role::syntax_escape_sequence, srgb(0xFFD199) },
			{ role::syntax_comment, srgb(0xC8C8C8) },
			{ role::syntax_documentation, srgb(0xBEE0C6) },
			{ role::syntax_annotation, srgb(0xF2C75C) },
			{ role::syntax_macro, srgb(0xE6BCFF) },
			{ role::syntax_preprocessor, srgb(0xE6BCFF) },
			{ role::syntax_import, srgb(0x4AE56B) },
			{ role::syntax_attribute, srgb(0xF2D98C) },
			{ role::syntax_region_marker, srgb(0xD6D6D6) },
			{ role::syntax_information, srgb(0x87C3FF) },
			{ role::syntax_warning, srgb(0xFFBC66) },
			{ role::syntax_alert, srgb(0xFFA6D9) },
			{ role::syntax_error, srgb(0xFFA6A0) },
			{ role::syntax_other, srgb(0xE0E0E0) },
			{ role::feedback_label_praise, srgb(0xFF8FD0) },
			{ role::feedback_label_nitpick, srgb(0xD0D5DA) },
			{ role::feedback_label_suggestion, srgb(0x7FC0FF) },
			{ role::feedback_label_issue, srgb(0xFF7070) },
			{ role::feedback_label_todo, srgb(0xFFB074) },
			{ role::feedback_label_question, srgb(0xD9B0FF) },
			{ role::feedback_label_thought, srgb(0xB4BCFF) },
			{ role::feedback_label_chore, srgb(0xE6BC8A) },
			{ role::feedback_label_note, srgb(0x6FD8F5) },
			{ role::feedback_label_typo, srgb(0xD6E070) },
			{ role::feedback_label_polish, srgb(0x5EE6D2) },
			{ role::feedback_label_quibble, srgb(0xF0B8D4) },
			{ role::state_attention, srgb(0xFFBC66) },
		} });

		/// \brief [[BPR-0776]]. Home's state colours per built-in variant, in
		/// `attention_tone` order: amber, blue and green.
		///
		/// \details The light amber is darker than the design board's so that the
		/// reason word reads as small text on white; the high-contrast tones are
		/// the accepted high-contrast warning, unread and accepted colours.
		using attention_entries = std::array<std::uint32_t, 3U>;

		constexpr auto light_attention               = attention_entries{ 0x9A6100U, 0x3468D6U, 0x1A7A50U };
		constexpr auto dark_attention                = attention_entries{ 0xE0A23CU, 0x6D9BFFU, 0x4CC08AU };
		constexpr auto high_contrast_light_attention = attention_entries{ 0x7A3000U, 0x0033CCU, 0x005A1EU };
		constexpr auto high_contrast_dark_attention  = attention_entries{ 0xFFBC66U, 0x66B2FFU, 0x4AE56BU };

		constexpr auto contrast_hundredths_per_unit = 100.0;

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

		constexpr auto centipoints_per_point = 100.0;
		constexpr auto percent_scale         = 100.0;

		// Nominal starting values, none of which is a bound. Each one reaches a
		// record only through `core::bounds_of`, so this file never restates a
		// legality rule the core owns.
		constexpr auto nominal_point_size_centipoints = std::uint64_t{ 1'000U };
		constexpr auto nominal_line_height_percent    = std::uint64_t{ 100U };
		constexpr auto nominal_tab_width              = std::uint64_t{ 4U };

		// Qt's own generic family aliases, used only when the platform reports a
		// family the core would reject.
		constexpr auto general_fallback_family = std::string_view{ "Sans Serif" };
		constexpr auto fixed_fallback_family   = std::string_view{ "Monospace" };

		// The standard and contextual OpenType ligature features. One user
		// switch moves both, because a family that ships ligatures may deliver
		// them through either.
		constexpr auto standard_ligature_feature   = QFont::Tag{ "liga" };
		constexpr auto contextual_ligature_feature = QFont::Tag{ "clig" };

		/// \brief Places one value inside the core's published bounds for a field.
		[[nodiscard]] auto within_bounds(
			const core::typography_field aField,
			const std::uint64_t          aValue) noexcept -> std::uint32_t
		{
			const auto bounds = core::bounds_of(aField);
			return static_cast<std::uint32_t>(std::clamp(
				aValue,
				static_cast<std::uint64_t>(bounds.Minimum),
				static_cast<std::uint64_t>(bounds.Maximum)));
		}

		/// \brief Converts one Qt point size to a legal centipoint value.
		[[nodiscard]] auto centipoints_of(const qreal aPointSize) noexcept
			-> std::uint32_t
		{
			// A pixel-sized Qt font reports no point size at all, which is a
			// missing observation rather than a small one.
			const auto centipoints = aPointSize > 0.0
				? std::llround(aPointSize * centipoints_per_point)
				: 0;
			return within_bounds(
				core::typography_field::point_size,
				centipoints > 0 ? static_cast<std::uint64_t>(centipoints)
								: nominal_point_size_centipoints);
		}

		[[nodiscard]] auto usable_family(const std::string& aFamily) noexcept
			-> bool
		{
			const auto bounds = core::bounds_of(core::typography_field::family);
			return aFamily.size() >= bounds.Minimum
				&& aFamily.size() <= bounds.Maximum
				&& std::ranges::none_of(aFamily, [](const char aCharacter) {
					   const auto value = static_cast<unsigned int>(
						   static_cast<unsigned char>(aCharacter));
					   return value < 0x20U || value == 0x7FU;
				   });
		}

		/// \brief Reads one platform system font into a legal core record.
		[[nodiscard]] auto native_font(
			const QFontDatabase::SystemFont aRole,
			const std::string_view          aFallbackFamily) -> core::font_settings
		{
			const auto system = QFontDatabase::systemFont(aRole);
			auto       family = system.family().toStdString();
			return core::font_settings{
				.Family               = usable_family(family) ? std::move(family)
															  : std::string{ aFallbackFamily },
				.PointSizeCentipoints = centipoints_of(system.pointSizeF()),
				.Weight               = within_bounds(
					core::typography_field::weight,
					static_cast<std::uint64_t>(
						std::max(0, static_cast<int>(system.weight())))),
			};
		}

		/// \brief Reads one platform system font into a legal core UI record.
		[[nodiscard]] auto native_ui_font(
			const QFontDatabase::SystemFont aRole,
			const std::string_view          aFallbackFamily) -> core::ui_font_settings
		{
			return core::ui_font_settings{
				.Font              = native_font(aRole, aFallbackFamily),
				.LineHeightPercent = within_bounds(
					core::typography_field::line_height,
					nominal_line_height_percent),
			};
		}

		/// \brief Returns the durable typography with each illegal font replaced.
		///
		/// \details A durable record can carry a font this build's core rejects,
		/// whether because it was written by a different build or because it was
		/// tampered with. Only the rejected font is replaced, so an illegal code
		/// font cannot discard a legal UI font the user chose.
		[[nodiscard]] auto adopted_typography(core::typography_settings aSettings)
			-> core::typography_settings
		{
			const auto rejections = core::validate_typography(aSettings);
			if (rejections.empty()) {
				return aSettings;
			}
			const auto rejected = [&rejections](
									  const core::typography_target aTarget) {
				return std::ranges::any_of(
					rejections,
					[aTarget](const core::typography_rejection& aRejection) {
						return aRejection.Target == aTarget;
					});
			};
			auto replacement = default_typography_settings();
			if (rejected(core::typography_target::ui_font)) {
				aSettings.Ui = std::move(replacement.Ui);
			}
			if (rejected(core::typography_target::code_font)) {
				aSettings.Code = std::move(replacement.Code);
			}
			return aSettings;
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

	auto contrast_ratio(
		const core::appearance_color aFirst,
		const core::appearance_color aSecond) noexcept -> double
	{
		// The core owns the sRGB linearisation, the luminance coefficients, and
		// the offset. Keeping a second copy here would let a corrected formula
		// or a changed threshold leave this readability heuristic silently
		// disagreeing with the audited contrast checks.
		return static_cast<double>(
				   core::contrast_ratio_hundredths(aFirst, aSecond))
			/ contrast_hundredths_per_unit;
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

	auto builtin_attention_color(
		const core::appearance_variant aVariant,
		const attention_tone           aTone) noexcept -> QColor
	{
		const auto& entries = [aVariant]() -> const attention_entries& {
			switch (aVariant) {
			case core::appearance_variant::light:
				break;
			case core::appearance_variant::dark:
				return dark_attention;
			case core::appearance_variant::high_contrast_light:
				return high_contrast_light_attention;
			case core::appearance_variant::high_contrast_dark:
				return high_contrast_dark_attention;
			}
			return light_attention;
		}();
		const auto index = static_cast<std::size_t>(aTone);
		return to_presentation_color(srgb(entries[index < entries.size() ? index : 0U]));
	}

	auto to_presentation_font(const core::font_settings& aFont) -> QFont
	{
		auto font = QFont{ };
		font.setFamily(QString::fromStdString(aFont.Family));
		font.setPointSizeF(
			static_cast<qreal>(aFont.PointSizeCentipoints) / centipoints_per_point);
		font.setWeight(static_cast<QFont::Weight>(aFont.Weight));
		return font;
	}

	auto to_presentation_font(const core::code_font_settings& aCode) -> QFont
	{
		auto       font    = to_presentation_font(aCode.Font);
		const auto enabled = aCode.Ligatures ? 1U : 0U;
		font.setFeature(standard_ligature_feature, enabled);
		font.setFeature(contextual_ligature_feature, enabled);
		return font;
	}

	auto line_height_of(const core::ui_font_settings& aFont) -> qreal
	{
		return QFontMetricsF{ to_presentation_font(aFont.Font) }.lineSpacing()
			* static_cast<qreal>(aFont.LineHeightPercent) / percent_scale;
	}

	auto tab_stop_distance_of(const core::code_font_settings& aCode) -> qreal
	{
		return QFontMetricsF{ to_presentation_font(aCode) }.horizontalAdvance(
				   QLatin1Char{ ' ' })
			* static_cast<qreal>(aCode.TabWidth);
	}

	auto default_typography_settings() -> core::typography_settings
	{
		return default_typography_settings(bundled_code_font_available());
	}

	auto default_typography_settings(const bool aBundledCodeFont)
		-> core::typography_settings
	{
		auto code = native_font(QFontDatabase::FixedFont, fixed_fallback_family);
		// [[BPR-1270]]. The bundled family at the platform's own code size and
		// at the regular weight, the faces it bundles being regular and bold.
		if (aBundledCodeFont) {
			code.Family = bundled_code_font_family().toStdString();
			code.Weight = within_bounds(
				core::typography_field::weight,
				static_cast<std::uint64_t>(QFont::Normal));
		}
		return core::typography_settings{
			.Ui = native_ui_font(
				QFontDatabase::GeneralFont,
				general_fallback_family),
			.Code = core::code_font_settings{
				.Font     = std::move(code),
				.TabWidth = within_bounds(
					core::typography_field::tab_width,
					nominal_tab_width),
			},
		};
	}

	auto with_builtin_label_tints(
		core::appearance_palette       aPalette,
		const core::appearance_variant aVariant) noexcept -> core::appearance_palette
	{
		const auto& builtin = builtin_appearance_palette(aVariant);
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count; ++index) {
			if (core::defaults_on_import(static_cast<core::appearance_role>(static_cast<std::uint8_t>(index)))
				&& aPalette[index] == core::unassigned_appearance_color) {
				aPalette[index] = builtin[index];
			}
		}
		return aPalette;
	}

	auto default_appearance_settings() -> application::appearance_settings
	{
		// No customization, for any variant. A fresh installation is the absence
		// of a decision, and the record says so by leaving every stored flag
		// clear rather than by carrying a palette that a reader would have to
		// decide the meaning of. The earlier record had one palette and no way to
		// be empty, so the composed default had to be a real palette — the light
		// one, whatever the desktop was doing — and on a dark desktop that read
		// as forty-six deliberate overrides (BPR-0216).
		auto settings       = application::appearance_settings{ };
		settings.Typography = default_typography_settings();
		return settings;
	}

	appearance_profile::appearance_profile(
		application::appearance_settings   aSettings,
		const core::appearance_observation aObservation)
		: Mode{ application::known(aSettings.Mode) ? aSettings.Mode
												   : core::appearance_mode::system }
		, Observation{ aObservation }
		, Typography{ adopted_typography(std::move(aSettings.Typography)) }
	{
		rebuild_typography();
		// The durable record carries one palette per variant. Each is read as
		// customisation relative to the built-in profile of *its own* variant, so
		// a slot whose palette is a built-in profile carries no customisation at
		// all, and the overrides a slot does carry apply only while that variant
		// resolves.
		//
		// Only a *stored* slot is read this way. An unstored slot is the absence
		// of a decision, and comparing it against a built-in palette would turn
		// that absence into forty-six of them: that was BPR-0216, where the
		// composed default (the light palette, whatever the desktop was doing)
		// diffed against the resolved dark built-in. BPR-0645 was the same defect
		// for a record that is present, which is why each palette is read against
		// the profile of the variant that owns it and no other.
		adopt_record(aSettings);
		rebuild();
	}

	/// \brief Rebuilds every variant's overrides from one durable record.
	void appearance_profile::adopt_record(
		const application::appearance_settings& aSettings)
	{
		for (auto variant = std::size_t{ 0U };
			 variant < core::appearance_variant_count;
			 ++variant) {
			const auto& customisation = aSettings.Palettes[variant];
			auto&       overrides     = Overrides[variant];
			overrides.fill(std::nullopt);
			if (!customisation.Stored) {
				continue;
			}
			const auto& builtin = builtin_appearance_palette(
				static_cast<core::appearance_variant>(
					static_cast<std::uint8_t>(variant)));
			for (auto index = std::size_t{ 0U };
				 index < core::appearance_role_count;
				 ++index) {
				// [[BPR-1083]]. A label tint the stored palette predates is the
				// built-in tint, not a colour the reviewer chose.
				if (core::defaults_on_import(static_cast<core::appearance_role>(static_cast<std::uint8_t>(index)))
					&& customisation.Palette[index] == core::unassigned_appearance_color) {
					continue;
				}
				if (customisation.Palette[index] != builtin[index]) {
					overrides[index] = customisation.Palette[index];
				}
			}
		}
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

	auto appearance_profile::attention_color(const attention_tone aTone) const noexcept
		-> QColor
	{
		return builtin_attention_color(Resolution.Variant, aTone);
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
			&& Overrides[application::variant_index(Resolution.Variant)]
						[core::index_of(aRole)]
							.has_value();
	}

	auto appearance_profile::customised_roles() const noexcept -> std::size_t
	{
		return customised_roles(Resolution.Variant);
	}

	auto appearance_profile::customised_roles(
		const core::appearance_variant aVariant) const noexcept -> std::size_t
	{
		if (!application::known(aVariant)) {
			return 0U;
		}
		return static_cast<std::size_t>(std::ranges::count_if(
			Overrides[application::variant_index(aVariant)],
			[](const std::optional<core::appearance_color>& aOverride) {
				return aOverride.has_value();
			}));
	}

	auto appearance_profile::palette_for(const core::appearance_variant aVariant) const
		-> core::appearance_palette
	{
		const auto& builtin   = builtin_appearance_palette(aVariant);
		const auto& overrides = Overrides[application::known(aVariant)
											  ? application::variant_index(aVariant)
											  : 0U];
		auto        palette   = core::appearance_palette{ };
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			palette[index] = application::known(aVariant)
				? overrides[index].value_or(builtin[index])
				: builtin[index];
		}
		return palette;
	}

	auto appearance_profile::settings() const -> application::appearance_settings
	{
		auto settings       = application::appearance_settings{ };
		settings.Mode       = Mode;
		settings.Typography = Typography;
		// One slot per variant, each holding that theme's own colours. A slot with
		// overrides is by definition a stored one: without saying so, a round trip
		// through the durable record would read a reviewer's chosen colours back as
		// the absence of any choice, which is BPR-0216 in the other direction. A
		// slot with no override is the absence of a decision and says that instead
		// of carrying a built-in palette a later reader would have to interpret.
		//
		// Every slot is reported, not just the resolved one. Reporting only what is
		// on screen would let an observation flip write one theme's colours over
		// another's — and losing the other themes is exactly what the single-palette
		// record did.
		for (auto variant = std::size_t{ 0U };
			 variant < core::appearance_variant_count;
			 ++variant) {
			const auto named = static_cast<core::appearance_variant>(
				static_cast<std::uint8_t>(variant));
			if (customised_roles(named) == 0U) {
				continue;
			}
			settings.Palettes[variant] = application::appearance_palette_customisation{
				.Palette = palette_for(named),
				.Stored  = true,
			};
		}
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

	auto appearance_profile::typography() const noexcept
		-> const core::typography_settings&
	{
		return Typography;
	}

	auto appearance_profile::ui_font() const -> QFont
	{
		return UiFont;
	}

	auto appearance_profile::code_font() const -> QFont
	{
		return CodeFont;
	}

	auto appearance_profile::code_font_for(const bool aBold, const bool aItalic)
		const -> QFont
	{
		auto font = CodeFont;
		font.setBold(aBold && Typography.Code.SyntaxBold);
		font.setItalic(aItalic && Typography.Code.SyntaxItalic);
		return font;
	}

	auto appearance_profile::ui_line_height() const -> qreal
	{
		return line_height_of(Typography.Ui);
	}

	auto appearance_profile::code_tab_stop_distance() const -> qreal
	{
		return tab_stop_distance_of(Typography.Code);
	}

	auto appearance_profile::select_typography(
		core::typography_settings aTypography) -> typography_selection
	{
		auto selection = typography_selection{
			.Rejections = core::validate_typography(aTypography),
		};
		if (!selection.accepted()) {
			// Nothing is applied and nothing is clamped. A clamped value would
			// tell a settings surface that its rejected value was accepted.
			++Statistics.RejectedTypography;
			return selection;
		}
		if (aTypography == Typography) {
			return selection;
		}
		Typography = std::move(aTypography);
		rebuild_typography();
		++Statistics.TypographyChanges;
		selection.Changed = publish();
		return selection;
	}

	auto appearance_profile::select_ui_font(core::ui_font_settings aFont)
		-> typography_selection
	{
		auto candidate = Typography;
		candidate.Ui   = std::move(aFont);
		return select_typography(std::move(candidate));
	}

	auto appearance_profile::select_code_font(core::code_font_settings aFont)
		-> typography_selection
	{
		auto candidate = Typography;
		candidate.Code = std::move(aFont);
		return select_typography(std::move(candidate));
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
		// Editing a colour is a statement about the theme on screen, and only that
		// one. It used to also *discard* whatever had been banked for another
		// variant, because there was one override table and it could belong to one
		// theme at a time — tuning Dark silently forgot Light. Each theme now has
		// its own table, so the edit lands in the resolved variant's and no other
		// theme is touched.
		auto& slot = Overrides[application::variant_index(Resolution.Variant)]
							  [core::index_of(aRole)];
		if (slot.has_value() && *slot == aColor) {
			return false;
		}
		slot = aColor;
		return adopt();
	}

	auto appearance_profile::preview_palette(core::appearance_palette aPalette)
		-> bool
	{
		aPalette = with_builtin_label_tints(std::move(aPalette), Resolution.Variant);
		if (PreviewPalette.has_value() && *PreviewPalette == aPalette) {
			return false;
		}
		PreviewPalette = std::move(aPalette);
		return adopt();
	}

	auto appearance_profile::cancel_palette_preview() -> bool
	{
		if (!PreviewPalette.has_value()) {
			return false;
		}
		PreviewPalette.reset();
		return adopt();
	}

	auto appearance_profile::adopt_palette(
		core::appearance_palette       aPalette,
		palette_preview_adoption       aPreview,
		const core::appearance_variant aVariant) -> bool
	{
		set_committed_palette(aPalette, aVariant);
		if (aPreview == palette_preview_adoption::commit) {
			PreviewPalette.reset();
		}
		return adopt();
	}

	auto appearance_profile::adopt_palettes(
		const application::appearance_palette_customisations& aPalettes,
		const palette_preview_adoption                        aPreview) -> bool
	{
		auto adopted     = application::appearance_settings{ };
		adopted.Palettes = aPalettes;
		adopt_record(adopted);
		if (aPreview == palette_preview_adoption::commit) {
			PreviewPalette.reset();
		}
		return adopt();
	}

	auto appearance_profile::previewing_palette() const noexcept -> bool
	{
		return PreviewPalette.has_value();
	}

	auto appearance_profile::restore(const core::appearance_role aRole) -> bool
	{
		if (!customised(aRole)) {
			return false;
		}
		Overrides[application::variant_index(Resolution.Variant)]
				 [core::index_of(aRole)]
					 .reset();
		return adopt();
	}

	auto appearance_profile::restore_all(const core::appearance_variant aVariant)
		-> bool
	{
		if (customised_roles(aVariant) == 0U) {
			return false;
		}
		Overrides[application::variant_index(aVariant)].fill(std::nullopt);
		// Reports whether the *applied* presentation changed, so restoring a theme
		// that is not on screen answers false: the record moved and no pixel did.
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

		// The chrome family. Qt's own styles draw scrollbars, buttons, frames
		// and separators from `Button` and the shades derived from it, and
		// `setColor` does not recompute the shades - so assigning only the
		// seven roles above left every one of them at whatever the inherited
		// palette happened to hold. Under the dark profile that was a light
		// default, which is why the comparison scrollbars rendered as bright
		// white troughs against a near-black pane (BPR-0207).
		//
		// Derived from the surface rather than named individually, so a
		// profile cannot ship a chrome colour that belongs to no role. The
		// factors are Qt's own conventions for the shade family; keeping them
		// means a style that expects `Light` to be lighter than `Button` still
		// gets that in a dark scheme, where a hand-picked set would have to
		// re-establish the ordering by hand.
		assign(QPalette::Button, surface);
		assign(QPalette::ButtonText, surfaceText);
		assign(QPalette::Light, surface.lighter(150));
		assign(QPalette::Midlight, surface.lighter(115));
		assign(QPalette::Mid, surface.darker(150));
		assign(QPalette::Dark, surface.darker(200));
		assign(QPalette::Shadow, surface.darker(300));
		assign(QPalette::ToolTipBase, surface);
		assign(QPalette::ToolTipText, surfaceText);
		assign(QPalette::PlaceholderText, blend_midpoint(baseText, base));
		derived.setColor(
			QPalette::Disabled, QPalette::Button, surface);
		derived.setColor(
			QPalette::Disabled,
			QPalette::ButtonText,
			blend_midpoint(surfaceText, surface));

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

	void appearance_profile::apply_ui_font_to(QWidget& aSurface)
	{
		aSurface.setFont(UiFont);
		++Statistics.AppliedFontSurfaces;
	}

	auto code_typography_event::registered_type() noexcept -> QEvent::Type
	{
		static const auto type
			= static_cast<QEvent::Type>(QEvent::registerEventType());
		return type;
	}

	code_typography_event::code_typography_event(const qreal aTabStopDistance)
		: QEvent{ registered_type() }
		, TabStopDistance{ aTabStopDistance }
	{
	}

	auto code_typography_event::tab_stop_distance() const noexcept -> qreal
	{
		return TabStopDistance;
	}

	void appearance_profile::apply_code_font_to(QWidget& aSurface)
	{
		const auto panes = aSurface.findChildren<QPlainTextEdit*>();
		apply_code_metrics(aSurface);
		for (auto* const pane : panes) {
			apply_code_metrics(*pane);
		}
		aSurface.setFont(CodeFont);
		for (auto* const pane : panes) {
			pane->setFont(CodeFont);
		}

		// Published after the fonts so a subscriber that rebuilds on this event
		// measures the typography that is already applied rather than the one
		// being replaced.
		auto published = code_typography_event{ code_tab_stop_distance() };
		QCoreApplication::sendEvent(&aSurface, &published);

		++Statistics.AppliedFontSurfaces;
	}

	void appearance_profile::apply_code_metrics(QWidget& aSurface) const
	{
		if (auto* const pane = qobject_cast<QPlainTextEdit*>(&aSurface);
			pane != nullptr) {
			pane->setTabStopDistance(code_tab_stop_distance());
		}
	}

	void appearance_profile::rebuild_typography()
	{
		UiFont   = to_presentation_font(Typography.Ui.Font);
		CodeFont = to_presentation_font(Typography.Code);
	}

	void appearance_profile::set_committed_palette(
		const core::appearance_palette& aPalette,
		const core::appearance_variant  aVariant)
	{
		// An adopted palette is a total set of colours for exactly one theme, and
		// the caller names which. It becomes that theme's customisation and leaves
		// every other theme's alone — it used to replace whatever was banked for
		// another one, because there was only one table to bank in.
		//
		// The variant is no longer derived from the mode and the observation when a
		// caller omits it. That derivation was a guess the presentation adapter is
		// not entitled to make, and by the time a host adoption arrives the desktop
		// may already have flipped, which would file the colours under a theme the
		// reviewer never edited.
		if (!application::known(aVariant)) {
			return;
		}
		const auto& builtin   = builtin_appearance_palette(aVariant);
		auto&       overrides = Overrides[application::variant_index(aVariant)];
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			const auto unassigned = core::defaults_on_import(static_cast<core::appearance_role>(static_cast<std::uint8_t>(index)))
				&& aPalette[index] == core::unassigned_appearance_color;
			overrides[index] = unassigned || aPalette[index] == builtin[index]
				? std::optional<core::appearance_color>{ }
				: std::optional<core::appearance_color>{ aPalette[index] };
		}
	}

	void appearance_profile::rebuild()
	{
		// The core decides which variant applies; this only reads its answer.
		Resolution          = core::resolve_appearance(Mode, Observation);
		const auto& builtin = builtin_appearance_palette(Resolution.Variant);
		// An override is a colour chosen against one built-in profile. Against any
		// other profile it is not a preference, it is a leftover — so only the
		// resolved variant's own table is applied, and a theme with no
		// customisation of its own renders its built-in palette untouched.
		const auto& overrides
			= Overrides[application::variant_index(Resolution.Variant)];
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			CommittedPalette[index] = overrides[index].value_or(builtin[index]);
		}
		Palette = PreviewPalette.value_or(CommittedPalette);
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			Colors[index] = to_presentation_color(Palette[index]);
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
		return publish();
	}

	auto appearance_profile::publish() -> bool
	{
		++Statistics.PresentationChanges;
		++Generation;
		if (Sink) {
			Sink();
		}
		return true;
	}
} // namespace bpr::qt
