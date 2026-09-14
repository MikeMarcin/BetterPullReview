#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_MODEL_HPP

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief Profile-owned decorative visibility and motion choices.
	enum class dif_visibility : std::uint8_t { shown  = 0,
											   hidden = 1 };
	enum class motion_mode : std::uint8_t { system  = 0,
											full    = 1,
											reduced = 2 };
	/// \brief Bounded native observation; unknown, unsupported and failed remain distinct.
	enum class motion_observation : std::uint8_t { full,
												   reduced,
												   unknown,
												   unsupported,
												   failed };

	/// \brief Durable choices; ordinal values are the appearance schema contract.
	struct dif_preferences
	{
		dif_visibility Visibility                                = dif_visibility::shown;
		motion_mode    Motion                                    = motion_mode::system;
		auto           operator<=>(const dif_preferences&) const = default;
	};

	/// \brief Accepted choices plus attachment-local effective motion and its evidence.
	struct dif_resolution
	{
		dif_preferences    Preferences;
		motion_observation Observation                              = motion_observation::unknown;
		motion_mode        EffectiveMotion                          = motion_mode::reduced;
		auto               operator<=>(const dif_resolution&) const = default;
	};

	/// \brief Rejects preferences outside the closed vocabulary.
	[[nodiscard]] constexpr auto valid(const dif_preferences aPreferences) noexcept -> bool
	{
		return (aPreferences.Visibility == dif_visibility::shown || aPreferences.Visibility == dif_visibility::hidden)
			&& (aPreferences.Motion == motion_mode::system || aPreferences.Motion == motion_mode::full || aPreferences.Motion == motion_mode::reduced);
	}

	/// \brief Resolves System conservatively; explicit Full overrides unavailable observation.
	[[nodiscard]] constexpr auto resolve_dif(const dif_preferences aPreferences, const motion_observation aObservation) noexcept -> dif_resolution
	{
		return { .Preferences = aPreferences, .Observation = aObservation, .EffectiveMotion = valid(aPreferences) && (aPreferences.Motion == motion_mode::full || (aPreferences.Motion == motion_mode::system && aObservation == motion_observation::full)) ? motion_mode::full : motion_mode::reduced };
	}

	/// \brief Describes one colour as four straight 8-bit sRGB channels.
	///
	/// \details The core owns the appearance vocabulary and decides nothing about
	/// how a colour is drawn, so this is a plain value with no presentation
	/// behaviour: no blending, no colour-space conversion, no toolkit colour
	/// class. Channels are unpremultiplied with an explicit alpha, which is the
	/// one shape a presentation adapter can accept without guessing a channel
	/// order or an alpha convention.
	struct appearance_color
	{
		std::uint8_t Red   = 0U;
		std::uint8_t Green = 0U;
		std::uint8_t Blue  = 0U;
		std::uint8_t Alpha = 255U;

		auto operator<=>(const appearance_color&) const = default;
	};

	/// \brief Names one semantic colour slot of the appearance vocabulary.
	///
	/// \details This enumeration is closed and core-owned so that a theme is a
	/// total mapping over a fixed vocabulary rather than a bag of arbitrary keys.
	/// Every member names exactly one colour slot; an element that needs both a
	/// fill and a text colour therefore has two members rather than one member
	/// with an implied second meaning. The core assigns no colour to any role and
	/// holds no default theme: choosing values is presentation work.
	enum class appearance_role : std::uint8_t {
		// Application chrome outside the code canvas.
		application_surface,
		application_text,

		// The code canvas itself. Code text colour comes from the syntax roles
		// below, so this group carries no general code foreground.
		code_background,
		code_gutter_background,
		code_line_number_text,
		code_selection_background,
		code_current_line_background,

		// Diff presentation. The row roles are fills so that syntax colouring
		// still shows through added, removed, and moved lines.
		diff_added_background,
		diff_removed_background,
		diff_intraline_added_background,
		diff_intraline_removed_background,
		diff_moved_background,
		diff_hunk_header_background,
		diff_hunk_header_text,
		/// The rule drawn on the edge where one change group begins. A mark on
		/// the code canvas rather than a fill of it, so it is held to the
		/// indicator ratio and not treated as a background wash.
		diff_group_boundary_rule,

		// Review state indicators. RFC-BPR-0006 also requires a non-colour
		// indicator for each state; that redundancy is presentation work and is
		// deliberately absent from this colour vocabulary.
		state_unread,
		state_accepted,
		state_defect,
		state_fixed,
		state_verified,
		state_stale,

		// Syntax token categories. These are the categories a highlighter's
		// token stream maps onto; the core performs no lexical analysis and
		// names no grammar, definition, or highlighting library.
		syntax_normal,
		syntax_keyword,
		syntax_control_flow,
		syntax_operator,
		syntax_builtin,
		syntax_function,
		syntax_variable,
		syntax_type,
		syntax_constant,
		syntax_number,
		syntax_string,
		syntax_character,
		syntax_escape_sequence,
		syntax_comment,
		syntax_documentation,
		syntax_annotation,
		syntax_macro,
		syntax_preprocessor,
		syntax_import,
		syntax_attribute,
		syntax_region_marker,
		syntax_information,
		syntax_warning,
		syntax_alert,
		syntax_error,
		syntax_other,
	};

	/// \brief The opacity the code panes composite the selection wash at.
	///
	/// \details The selection is not painted as a solid fill over the code
	/// canvas; it is laid over whatever band the line already carries, so the
	/// background syntax text is actually drawn on is this colour composited
	/// over that band. The value lives here rather than in the painter because
	/// it decides whether the result is legible, and a number that decides
	/// legibility belongs in the contract that checks legibility. A painter
	/// holding it privately is how the contrast obligation came to be measured
	/// against a colour the panes never paint.
	inline constexpr std::uint8_t selection_wash_alpha = 72U;

	/// \brief Lays one colour over another at the given opacity.
	[[nodiscard]] constexpr auto composited(
		const appearance_color aOver,
		const appearance_color aUnder,
		const std::uint8_t     aAlpha) noexcept -> appearance_color
	{
		const auto blend = [aAlpha](
							   const std::uint8_t aTop,
							   const std::uint8_t aBottom) -> std::uint8_t {
			const auto over  = static_cast<std::uint32_t>(aTop) * aAlpha;
			const auto under = static_cast<std::uint32_t>(aBottom)
				* (255U - static_cast<std::uint32_t>(aAlpha));
			return static_cast<std::uint8_t>((over + under + 127U) / 255U);
		};
		return appearance_color{
			.Red   = blend(aOver.Red, aUnder.Red),
			.Green = blend(aOver.Green, aUnder.Green),
			.Blue  = blend(aOver.Blue, aUnder.Blue),
			.Alpha = 255U,
		};
	}

	inline constexpr std::size_t appearance_role_count = 47U;

	static_assert(
		static_cast<std::size_t>(appearance_role::syntax_other) + 1U == appearance_role_count,
		"appearance_role_count must cover every declared role");

	/// \brief Returns the palette index of the supplied role.
	[[nodiscard]] constexpr auto index_of(const appearance_role aRole) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(aRole);
	}

	/// \brief Maps every role of the closed vocabulary to one colour.
	///
	/// \details A palette is total by construction: it cannot omit a role and it
	/// cannot carry a key outside the vocabulary. Index it with `index_of`.
	using appearance_palette = std::array<appearance_color, appearance_role_count>;

	/// \brief Returns the stable exchange identifier of the supplied role.
	///
	/// \details This is the identifier a persisted or exchanged theme uses as its
	/// key, so it is part of the compatibility contract and does not change with
	/// presentation wording. It is deliberately distinct from a display name,
	/// which is translated presentation text the core never owns.
	[[nodiscard]] auto stable_name(appearance_role aRole) noexcept -> std::string_view;

	/// \brief Resolves a stable identifier back to its role.
	///
	/// \returns The matching role, or no value when the identifier is outside the
	/// closed vocabulary.
	[[nodiscard]] auto find_appearance_role(std::string_view aName) noexcept
		-> std::optional<appearance_role>;

	/// \brief Names the appearance mode a user selected.
	///
	/// \details The three high-contrast members are the high-contrast variants of
	/// the three base modes. `high_contrast_system` exists because following the
	/// operating-system light/dark scheme and requiring high contrast are
	/// independent needs, and a platform that reports a scheme but no contrast
	/// preference would otherwise leave that combination unreachable.
	enum class appearance_mode : std::uint8_t {
		system,
		light,
		dark,
		high_contrast_system,
		high_contrast_light,
		high_contrast_dark,
	};

	inline constexpr std::size_t appearance_mode_count = 6U;

	static_assert(
		static_cast<std::size_t>(appearance_mode::high_contrast_dark) + 1U == appearance_mode_count,
		"appearance_mode_count must cover every declared mode");

	/// \brief Returns the stable exchange identifier of the supplied mode.
	[[nodiscard]] auto stable_name(appearance_mode aMode) noexcept -> std::string_view;

	/// \brief Resolves a stable identifier back to its mode.
	///
	/// \returns The matching mode, or no value when the identifier is unknown.
	[[nodiscard]] auto find_appearance_mode(std::string_view aName) noexcept
		-> std::optional<appearance_mode>;

	/// \brief Names the profile variant a resolved appearance applies.
	///
	/// \details A variant is a decision rather than a selection: it is derived
	/// from the selected mode and the current observation every time, and no
	/// user ever picks one. It does carry a stable exchange identifier, because
	/// a stored palette customises exactly one variant and a durable record
	/// that could not say which one would read a light customisation as
	/// overrides over the dark profile. Naming the variant is not the same as
	/// storing the decision: the decision is still made by
	/// `resolve_appearance` on every read.
	enum class appearance_variant : std::uint8_t {
		light,
		dark,
		high_contrast_light,
		high_contrast_dark,
	};

	inline constexpr std::size_t appearance_variant_count = 4U;

	static_assert(
		static_cast<std::size_t>(appearance_variant::high_contrast_dark) + 1U
			== appearance_variant_count,
		"appearance_variant_count must cover every declared variant");

	/// \brief Returns the stable exchange identifier of the supplied variant.
	[[nodiscard]] auto stable_name(appearance_variant aVariant) noexcept
		-> std::string_view;

	/// \brief Resolves a stable identifier back to its variant.
	///
	/// \returns The matching variant, or no value when the identifier is unknown.
	[[nodiscard]] auto find_appearance_variant(std::string_view aName) noexcept
		-> std::optional<appearance_variant>;

	/// \brief Reports whether the supplied variant is a high-contrast variant.
	[[nodiscard]] auto high_contrast(appearance_variant aVariant) noexcept -> bool;

	enum class os_color_scheme : std::uint8_t {
		unknown,
		light,
		dark,
	};

	enum class os_contrast_preference : std::uint8_t {
		standard,
		high,
	};

	/// \brief Describes the bounded operating-system appearance observation.
	///
	/// \details The core reads no operating-system state. The host observes the
	/// scheme and the contrast preference and reports them here; `unknown` is the
	/// honest answer on a platform that exposes no scheme, and it is distinct
	/// from `light` so that a defaulted resolution can be told apart from an
	/// observed one.
	struct appearance_observation
	{
		os_color_scheme        Scheme   = os_color_scheme::unknown;
		os_contrast_preference Contrast = os_contrast_preference::standard;

		auto operator<=>(const appearance_observation&) const = default;
	};

	/// \brief Names why a resolution chose its light-or-dark family.
	enum class appearance_scheme_source : std::uint8_t {
		selected_mode,
		observed_scheme,
		unobserved_default,
	};

	/// \brief Names why a resolution chose its contrast level.
	enum class appearance_contrast_source : std::uint8_t {
		none,
		selected_mode,
		observed_preference,
	};

	/// \brief Describes the resolved appearance decision.
	///
	/// \details `Variant` is the profile variant that applies. The two source
	/// members report why, which the variant alone cannot say: a dark variant
	/// looks the same whether an explicit mode chose it or an observation did.
	/// `HighContrastRecommended` is true exactly when the operating system asks
	/// for high contrast and the resolved variant is not high contrast, which is
	/// the one case where an explicit user choice outranks the preference and the
	/// core can only recommend.
	struct appearance_resolution
	{
		appearance_variant         Variant                 = appearance_variant::light;
		appearance_scheme_source   Scheme                  = appearance_scheme_source::selected_mode;
		appearance_contrast_source Contrast                = appearance_contrast_source::none;
		bool                       HighContrastRecommended = false;

		auto operator<=>(const appearance_resolution&) const = default;
	};

	/// \brief Reports whether the supplied mode follows the observed scheme.
	///
	/// \details A host observes the operating-system scheme only for the modes
	/// that consume it; an explicit mode ignores every scheme change.
	[[nodiscard]] auto follows_observed_scheme(appearance_mode aMode) noexcept -> bool;

	/// \brief Resolves which appearance profile variant applies.
	///
	/// \details A system mode takes its light-or-dark family from the observation
	/// and falls back to light when no scheme was observed. An explicit mode
	/// takes its family from the mode alone and is unaffected by any observation.
	/// An observed high-contrast preference selects the high-contrast variant of
	/// whichever family a system mode resolved, and is a recommendation only when
	/// the selected mode is an explicit standard-contrast mode.
	[[nodiscard]] auto resolve_appearance(
		appearance_mode               aMode,
		const appearance_observation& aObservation) noexcept -> appearance_resolution;

	/// \brief Names which font a typography value belongs to.
	enum class typography_target : std::uint8_t {
		ui_font,
		code_font,
	};

	/// \brief Names one bounded typography field.
	enum class typography_field : std::uint8_t {
		family,
		point_size,
		weight,
		line_height,
		tab_width,
	};

	/// \brief Names why one typography value is illegal.
	///
	/// \details A rejection is the triple of target, field, and reason, so the
	/// reason carries the kind of violation rather than repeating which value was
	/// violated. There is no catch-all member: every rejection names an exact
	/// bound or an exact structural rule.
	enum class typography_reason : std::uint8_t {
		none,
		below_minimum,
		above_maximum,
		malformed_value,
	};

	/// \brief Describes the inclusive legality bounds of one typography field.
	///
	/// \details Bounds are inclusive on both ends. For `family` they bound the
	/// length of the family name in bytes rather than a numeric value.
	struct typography_bounds
	{
		std::uint32_t Minimum = 0U;
		std::uint32_t Maximum = 0U;

		auto operator<=>(const typography_bounds&) const = default;
	};

	/// \brief Returns the inclusive bounds the core enforces for one field.
	///
	/// \details This is the single published source of the bounds and the same
	/// function the validator enforces with, so a settings widget built from it
	/// cannot offer a value the core would then reject.
	[[nodiscard]] auto bounds_of(typography_field aField) noexcept -> typography_bounds;

	/// \brief Describes one font's configurable settings.
	///
	/// \details `Family` is an opaque platform identifier the core never
	/// interprets, matches, or invents; it has no default because the core cannot
	/// know one, so a default-constructed record is deliberately illegal until
	/// the host supplies a family. The numeric members are fixed point so that
	/// every legal value is exactly representable and comparable without floating
	/// point: `PointSizeCentipoints` is hundredths of a point, so 1000 is
	/// 10.00pt, and `Weight` is the usual 100-to-900 axis, where 400 is regular.
	/// Their defaults are legal placeholders the host may replace.
	///
	/// There is no line height here. It belongs to `ui_font_settings` alone, so
	/// that `code_font_settings` has no field one could occupy — see BPR-0118.
	struct font_settings
	{
		std::string   Family;
		std::uint32_t PointSizeCentipoints = 1000U;
		std::uint32_t Weight               = 400U;

		auto operator<=>(const font_settings&) const = default;
	};

	/// \brief Describes the UI font's settings.
	///
	/// \details `LineHeightPercent` is a percentage of the natural line height,
	/// so 100 is 1.00x.
	///
	/// It lives on the UI font and not on `font_settings` because the code font
	/// must not be able to carry one. The product owner retired the code line
	/// height on 2026-08-06, after the presentation adapter measured that its
	/// plain-text document layout ignores line height in every mode the toolkit
	/// offers, and block margins with it. Leaving the field on the shared record
	/// would have left a code setting that validates, persists, and can never
	/// apply — which is worse than not offering it, because it defeats the next
	/// reader's attempt to find the bug.
	///
	/// Which toolkit and which layout class is deliberately not named here. The
	/// core states the rule; the measurement that forced it belongs to the
	/// adapter that took it, and naming a presentation type in this header is a
	/// boundary violation the dependency scan refuses — as it refused an earlier
	/// draft of this very comment.
	struct ui_font_settings
	{
		font_settings Font;
		std::uint32_t LineHeightPercent = 100U;

		auto operator<=>(const ui_font_settings&) const = default;
	};

	/// \brief Describes the code font's settings.
	///
	/// \details Tab width, ligatures, and the two syntax emphasis switches are
	/// code-only: a tab width for application chrome has no meaning. The three
	/// boolean members have no legality rule because every boolean value is
	/// legal, and that omission is deliberate rather than missing.
	///
	/// There is deliberately no line height. See `ui_font_settings`.
	struct code_font_settings
	{
		font_settings Font;
		std::uint32_t TabWidth     = 4U;
		bool          Ligatures    = false;
		bool          SyntaxBold   = true;
		bool          SyntaxItalic = true;

		auto operator<=>(const code_font_settings&) const = default;
	};

	/// \brief Describes the complete typography settings record.
	struct typography_settings
	{
		ui_font_settings   Ui;
		code_font_settings Code;

		auto operator<=>(const typography_settings&) const = default;
	};

	/// \brief Describes one rejected typography value.
	struct typography_rejection
	{
		typography_target Target = typography_target::ui_font;
		typography_field  Field  = typography_field::family;
		typography_reason Reason = typography_reason::none;

		auto operator<=>(const typography_rejection&) const = default;
	};

	/// \brief Reports every illegal typography value.
	///
	/// \details The result is empty exactly when the settings are legal. Every
	/// violation is reported rather than only the first, so a settings surface can
	/// mark each offending field at once. The order is fixed: the UI font before
	/// the code font, and within one font family, point size, weight, line
	/// height, then tab width. The core decides legality alone; it decides
	/// nothing about how any legal value looks.
	[[nodiscard]] auto validate_typography(const typography_settings& aSettings)
		-> std::vector<typography_rejection>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_MODEL_HPP
