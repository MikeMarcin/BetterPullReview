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

		// [[BPR-1083]]. The Conventional Comments label tints (RFC-BPR-0050),
		// one per catalog label in `feedback_label` order. A mark's border and
		// soft fill take its label's tint, and a blocking mark is filled with
		// it. These roles may be absent from a stored palette or a theme
		// document written before they existed; see `defaults_on_import`.
		feedback_label_praise,
		feedback_label_nitpick,
		feedback_label_suggestion,
		feedback_label_issue,
		feedback_label_todo,
		feedback_label_question,
		feedback_label_thought,
		feedback_label_chore,
		feedback_label_note,
		feedback_label_typo,
		feedback_label_polish,
		feedback_label_quibble,

		// [[BPR-1084]] and [[BPR-1085]]. The amber of something that needs the
		// reviewer's attention before acting, such as the dot on Approve while
		// the reviewer's own blocking feedback is open. A mark, held to the
		// indicator ratio by its built-in values. It may be absent from a stored
		// palette or a theme document written before it existed; see
		// `defaults_on_import`.
		state_attention,

		// Appended for durable ordinal compatibility (RFC-BPR-0072).
		syntax_namespace,
		syntax_enum_member,
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

	inline constexpr std::size_t appearance_role_count = 62U;

	static_assert(
		static_cast<std::size_t>(appearance_role::syntax_enum_member) + 1U == appearance_role_count,
		"appearance_role_count must cover every declared role");

	/// \brief The first of the twelve label tint roles, in catalog order.
	inline constexpr auto        first_feedback_label_role = appearance_role::feedback_label_praise;
	inline constexpr std::size_t feedback_label_role_count = 12U;
	static_assert(
		static_cast<std::size_t>(first_feedback_label_role) + feedback_label_role_count == static_cast<std::size_t>(appearance_role::state_attention),
		"the label tint roles must stay one contiguous run, directly before the attention amber");

	/// \brief Returns the palette index of the supplied role.
	[[nodiscard]] constexpr auto index_of(const appearance_role aRole) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(aRole);
	}

	/// \brief Whether a stored palette or a theme document may leave the role out.
	///
	/// \details [[BPR-1083]]. Only the roles added after palettes and theme
	/// documents had been written without them: the twelve label tints and,
	/// after them, `state_attention` ([[BPR-1085]]) and the namespace/enum
	/// categories (RFC-BPR-0072). The owner's rule (BPR-1079
	/// `tints-v002`) is that such a palette reads them as the built-in values and
	/// no other role changes. The core holds no built-in colour, so the parser
	/// leaves such a role `unassigned_appearance_color`. A stored palette's slot
	/// is its variant, and presentation reads the sentinel as that variant's
	/// built-in value. A theme import fills it from the document's base before
	/// anything is checked or previewed (`with_base_defaults`, [[BPR-1479]]).
	[[nodiscard]] constexpr auto defaults_on_import(const appearance_role aRole) noexcept -> bool
	{
		return index_of(aRole) >= index_of(first_feedback_label_role) && index_of(aRole) < appearance_role_count;
	}

	/// \brief The value an import gives a role the document left out.
	///
	/// \details Fully transparent black, which no built-in palette uses and which
	/// a presentation never paints: it reads it as "use the built-in value".
	/// Only a role for which `defaults_on_import` holds can carry it, and only a
	/// stored palette keeps it; an imported theme never does.
	inline constexpr appearance_color unassigned_appearance_color{ .Red = 0U, .Green = 0U, .Blue = 0U, .Alpha = 0U };

	/// \brief Maps every role of the closed vocabulary to one colour.
	///
	/// \details A palette is total by construction: it cannot omit a role and it
	/// cannot carry a key outside the vocabulary. Index it with `index_of`.
	using appearance_palette = std::array<appearance_color, appearance_role_count>;

	/// \brief Gives every role a palette left unassigned the base palette's value.
	///
	/// \details [[BPR-1479]]. A theme document that predates a role leaves it
	/// `unassigned_appearance_color`, and the owner's rule (2026-10-13) is that
	/// the import fills it from the built-in palette of the variant the theme
	/// was made against, not from whichever theme is on screen. The core holds
	/// no built-in colour, so the caller supplies that palette. No other role
	/// changes, and a palette with nothing unassigned is returned unchanged.
	[[nodiscard]] constexpr auto with_base_defaults(
		appearance_palette        aPalette,
		const appearance_palette& aBase) noexcept -> appearance_palette
	{
		for (auto index = std::size_t{ 0U }; index < appearance_role_count; ++index) {
			if (defaults_on_import(static_cast<appearance_role>(static_cast<std::uint8_t>(index)))
				&& aPalette[index] == unassigned_appearance_color) {
				aPalette[index] = aBase[index];
			}
		}
		return aPalette;
	}

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
		/// [[BPR-1107]]. Bounded as the ordinals of `white_space_drawing`.
		white_space,
		/// [[BPR-1336]]. The comparison's zoom, a percentage of the code size.
		code_zoom,
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

	/// \brief When the code panes draw white space ([[BPR-1107]]).
	///
	/// \details Sublime Text's model, which the owner asked for on 2026-09-24:
	/// a drawn space is a dot and a drawn tab a line to its stop. The choices are
	/// the easy and useful subset of Sublime's `draw_white_space` list, one at a
	/// time. Leading white space runs from the start of a line to its first
	/// other character, trailing white space from its last other character to
	/// the end, and a line of white space alone is both. `boundary` is leading
	/// and trailing together. `selection` draws every white space character of
	/// a selected line and none elsewhere, which is Sublime's own default.
	///
	/// Ordinal values are the appearance schema contract.
	enum class white_space_drawing : std::uint8_t {
		none      = 0,
		selection = 1,
		leading   = 2,
		trailing  = 3,
		boundary  = 4,
		all       = 5,
	};

	inline constexpr std::size_t white_space_drawing_count = 6U;

	/// \brief Rejects a value outside the closed vocabulary.
	[[nodiscard]] constexpr auto valid(const white_space_drawing aDrawing) noexcept -> bool
	{
		return static_cast<std::size_t>(aDrawing) < white_space_drawing_count;
	}

	/// \brief Returns the durable name of one white-space choice.
	[[nodiscard]] auto stable_name(white_space_drawing aDrawing) noexcept -> std::string_view;

	/// \brief Describes the code font's settings.
	///
	/// \details Tab width, ligatures, the two syntax emphasis switches and white
	/// space drawing are code-only: a tab width for application chrome has no
	/// meaning. The three boolean members have no legality rule because every
	/// boolean value is legal, and that omission is deliberate rather than
	/// missing.
	///
	/// There is deliberately no line height. See `ui_font_settings`.
	struct code_font_settings
	{
		font_settings       Font;
		std::uint32_t       TabWidth     = 4U;
		bool                Ligatures    = false;
		bool                SyntaxBold   = true;
		bool                SyntaxItalic = true;
		white_space_drawing WhiteSpace   = white_space_drawing::selection;
		/// [[BPR-1336]]. How large the comparison shows the code, as a
		/// percentage of `Font.PointSizeCentipoints`. A multiplier rather than a
		/// second size so that Actual Size returns to the size the reviewer
		/// chose, and so that zooming the comparison never rewrites that size.
		std::uint32_t ZoomPercent = 100U;

		auto operator<=>(const code_font_settings&) const = default;
	};

	/// \brief [[BPR-1336]]. One zoom gesture on the comparison.
	enum class code_zoom_step : std::uint8_t {
		in,
		out,
		actual_size,
	};

	/// \brief The zoom at which the comparison shows the code at its own size.
	inline constexpr std::uint32_t actual_code_zoom_percent = 100U;

	/// \brief The zooms Zoom In and Zoom Out step through, ascending.
	///
	/// \details The browser ladder: fine steps near the code size, where a
	/// reader adjusts, and wide ones far from it, where a reader who needs large
	/// text gets there in few presses. The first and last are the bounds of
	/// `typography_field::code_zoom`.
	inline constexpr std::array<std::uint32_t, 13> code_zoom_steps{
		50U, 67U, 75U, 80U, 90U, 100U, 110U, 125U, 150U, 175U, 200U, 250U, 300U
	};

	/// \brief The size the comparison shows the code at, in hundredths of a point.
	///
	/// \details The code size scaled by the zoom, rounded to the nearest
	/// hundredth and held to the point-size bounds, so a zoom can never show a
	/// size the code size row itself could not hold.
	[[nodiscard]] auto zoomed_point_size(const code_font_settings& aCode) noexcept -> std::uint32_t;

	/// \brief The zoom one gesture leads to from the current one.
	///
	/// \details Actual Size is always `actual_code_zoom_percent`. Zoom In is
	/// the next step up whose shown size is larger than the current one, and
	/// Zoom Out the next step down whose shown size is smaller, so a step never
	/// lands on a size the clamp makes equal to the current one. A zoom between
	/// two steps moves to the nearer step in the gesture's direction. With
	/// nowhere to go the current zoom is returned unchanged.
	[[nodiscard]] auto stepped_code_zoom(const code_font_settings& aCode, code_zoom_step aStep) noexcept -> std::uint32_t;

	/// \brief Whether a gesture would change what the comparison shows.
	[[nodiscard]] auto code_zoom_steps_from(const code_font_settings& aCode, code_zoom_step aStep) noexcept -> bool;

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
