#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_CONTRAST_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_CONTRAST_HPP

#include "bpr/core/appearance_model.hpp"

#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace bpr::core {
	/// \brief Returns the relative-luminance contrast ratio in hundredths.
	///
	/// \details This is the standard sRGB contrast ratio. Each channel is
	/// normalised to 0..1 and linearised with the sRGB electro-optical transfer
	/// function
	///
	///     c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)
	///
	/// the relative luminance is `0.2126 R + 0.7152 G + 0.0722 B` over those
	/// linear channels, and the ratio of two colours is
	///
	///     (Lbrighter + 0.05) / (Ldarker + 0.05)
	///
	/// so the result lies in 1.0 (identical luminance) to 21.0 (black against
	/// white) inclusive.
	///
	/// The result is reported in hundredths of a ratio, so 2100 is 21.00:1 and
	/// 450 is 4.50:1. A ratio needs fractional precision and the published
	/// thresholds are stated to two decimals, so hundredths is the coarsest unit
	/// that can express a threshold exactly; reporting a `double` instead would
	/// make every threshold comparison a floating-point comparison at the point
	/// of decision. The conversion truncates rather than rounds, so a reported
	/// ratio never overstates the measured one, and the truncation also absorbs
	/// the sub-unit-in-last-place variation between platform `pow`
	/// implementations that would otherwise make a decision machine dependent.
	///
	/// Alpha is ignored: both colours are treated as opaque. Compositing a
	/// translucent role against whatever happens to be behind it is presentation
	/// work with a result the core cannot know, so a caller that wants the
	/// contrast of a translucent role must composite it first and pass the
	/// resulting opaque colour.
	[[nodiscard]] auto contrast_ratio_hundredths(
		appearance_color aFirst,
		appearance_color aSecond) noexcept -> std::uint32_t;

	/// \brief Names what one checked colour pair is for.
	///
	/// \details The kind selects the threshold. Text has to be read character by
	/// character and carries the stricter minimum; an indicator is a shape or a
	/// mark that only has to be found and told apart from its surface.
	enum class contrast_kind : std::uint8_t {
		text,
		indicator,
	};

	/// \brief The minimum ratio the project holds text-on-background pairs to.
	///
	/// \details 4.50:1 is WCAG 2.2 success criterion 1.4.3 at level AA for text
	/// below the large-text size. Review text is dense small monospaced source,
	/// which is never large text, so the large-text allowance of 3.00:1 is not
	/// available to this project anywhere.
	inline constexpr std::uint32_t text_contrast_minimum_hundredths = 450U;

	/// \brief The minimum ratio a high-contrast variant holds text pairs to.
	///
	/// \details 7.00:1 is WCAG 2.2 success criterion 1.4.6 at level AAA. A
	/// high-contrast variant exists precisely because the standard minimum was
	/// not enough for the person who selected it, so it is held to the enhanced
	/// criterion rather than to the same one under a different name.
	inline constexpr std::uint32_t high_contrast_text_minimum_hundredths = 700U;

	/// \brief The minimum ratio the project holds indicator pairs to.
	///
	/// \details 3.00:1 is WCAG 2.2 success criterion 1.4.11, which covers the
	/// parts of a graphic that are needed to understand the content. It is not
	/// raised for high-contrast variants because 1.4.6 raises the text criterion
	/// only; inventing a stricter non-text rule here would be this project's
	/// opinion wearing a standard's name.
	inline constexpr std::uint32_t indicator_contrast_minimum_hundredths = 300U;

	/// \brief Returns the minimum ratio one kind must reach in one variant.
	///
	/// \details This is the single published source of the thresholds, so a
	/// theme editor that reports legality with it cannot disagree with the check
	/// that enforces it.
	[[nodiscard]] auto minimum_contrast_hundredths(
		contrast_kind      aKind,
		appearance_variant aVariant) noexcept -> std::uint32_t;

	/// \brief Names one foreground/background pair that must stay legible.
	///
	/// \details The pair is ordered: the foreground is drawn over the background,
	/// which the ratio itself cannot tell apart because it is symmetric. Keeping
	/// the direction makes a finding readable without a second lookup.
	struct contrast_pair
	{
		appearance_role Foreground = appearance_role::application_text;
		appearance_role Background = appearance_role::application_surface;
		contrast_kind   Kind       = contrast_kind::text;

		auto operator<=>(const contrast_pair&) const = default;
	};

	/// \brief Returns every colour pair the project checks.
	///
	/// \details A contrast check that never names its pairs cannot be audited,
	/// and a check that asserted every pair of the 45-role vocabulary would be
	/// asserting that decorative combinations nobody draws are legible. This is
	/// the enumerated middle: the pairs the presentation layer can actually
	/// produce.
	///
	/// The set is generated by three stated rules rather than hand-listed, so it
	/// cannot fall behind the vocabulary:
	///
	/// - The three fixed text pairs: application text on the application
	///   surface, line-number text on the gutter, and hunk-header text on the
	///   hunk-header background.
	/// - Every syntax role on every code-canvas background. The code canvas has
	///   no general foreground role — syntax colouring supplies it — and the
	///   diff row roles, the current-line role, and the selection role are all
	///   fills that syntax colouring shows through, so any token colour can land
	///   on any of them.
	/// - Every review-state role on the application surface, as an indicator
	///   rather than as text.
	///
	/// The span refers to storage with static lifetime and outlives any caller.
	[[nodiscard]] auto required_contrast_pairs() noexcept
		-> std::span<const contrast_pair>;

	/// \brief Reports one pair's measured ratio against the minimum it must meet.
	///
	/// \details `Meets` is redundant with the two ratios and is present anyway,
	/// because the comparison is the check's answer and a caller that recomputed
	/// it could recompute it differently. The measured ratio travels with the
	/// verdict so that a failure can be reported as a quantity rather than as
	/// the word "no".
	struct contrast_finding
	{
		contrast_pair Pair;
		std::uint32_t RatioHundredths   = 0U;
		std::uint32_t MinimumHundredths = 0U;
		bool          Meets             = false;

		auto operator<=>(const contrast_finding&) const = default;
	};

	/// \brief Measures one pair against the palette of one variant.
	[[nodiscard]] auto measure_contrast(
		const appearance_palette& aPalette,
		contrast_pair             aPair,
		appearance_variant        aVariant) noexcept -> contrast_finding;

	/// \brief Measures every required pair, in `required_contrast_pairs` order.
	[[nodiscard]] auto measure_required_contrast(
		const appearance_palette& aPalette,
		appearance_variant        aVariant) -> std::vector<contrast_finding>;

	/// \brief Returns only the required pairs that miss their minimum.
	///
	/// \details The result is empty exactly when the palette satisfies the
	/// published minimum for every pair it is checked against.
	[[nodiscard]] auto failing_contrast(
		const appearance_palette& aPalette,
		appearance_variant        aVariant) -> std::vector<contrast_finding>;

	/// \brief Names one colour-vision model the checks simulate.
	///
	/// \details Only the two dichromacies that lose the red-green axis are
	/// modelled, because they are both the common deficiencies and the ones a
	/// review tool's added-versus-removed vocabulary walks straight into.
	/// Tritanopia is deliberately absent rather than forgotten: it is rare, and
	/// the blue-yellow axis is not what this palette uses to separate states.
	enum class color_vision : std::uint8_t {
		typical,
		protanopia,
		deuteranopia,
	};

	/// \brief Returns the colour a viewer with the supplied vision perceives.
	///
	/// \details The two dichromat projections are the Viénot, Brettel and Mollon
	/// (1999) matrices, applied to linear sRGB and re-encoded to sRGB:
	///
	///     protanopia               deuteranopia
	///     [ 0.11238  0.88762  0 ]  [ 0.29275  0.70725  0 ]
	///     [ 0.07276  0.92724  0 ]  [ 0.34597  0.65403  0 ]
	///     [-0.00399  0.00399  1 ]  [-0.02795  0.02795  1 ]
	///
	/// `typical` is the identity and is a member rather than a special case so
	/// that a caller can measure all three models through one code path.
	///
	/// This is a simulation of a deficiency, not a correction of one, and it is
	/// approximate: it models the two dichromacies, not the far more common
	/// anomalous trichromacies, whose perception lies somewhere between typical
	/// vision and these results. Treating a dichromat result as the worst case
	/// is therefore the conservative reading.
	[[nodiscard]] auto simulate_color_vision(
		appearance_color aColor,
		color_vision     aVision) noexcept -> appearance_color;

	/// \brief Returns the perceived difference of two colours, in tenths.
	///
	/// \details Both colours are put through `simulate_color_vision` for the
	/// supplied model and the difference is the CIE 1976 \f$\Delta E^*_{ab}\f$
	/// between them: the Euclidean distance in CIELAB under the D65 white point.
	/// Measuring between the colours the public simulation returns keeps the
	/// number auditable — a reader can print both simulated colours and check
	/// the distance by hand.
	///
	/// A contrast ratio cannot answer this question. Two fills chosen to differ
	/// in hue alone have almost equal luminance by construction, so their ratio
	/// is near 1.00:1 both before and after simulation and says nothing about
	/// whether they collapsed. \f$\Delta E^*_{ab}\f$ measures the chromatic
	/// distance the deficiency actually destroys.
	///
	/// CIE76 is the simple metric and is known to overstate differences among
	/// saturated blues. It is used here because it is the one difference formula
	/// that can be read straight out of its own definition; a later CIEDE2000
	/// move would change the numbers, not the vocabulary.
	///
	/// The result is in tenths, so 100 is 10.0. Black against white is 1000.
	[[nodiscard]] auto color_difference_tenths(
		appearance_color aFirst,
		appearance_color aSecond,
		color_vision     aVision) noexcept -> std::uint32_t;

	/// \brief The difference two colours need before colour alone may carry them.
	///
	/// \details 10.0 is a project decision, not a standard: no accessibility
	/// standard publishes a dichromat separation threshold, so the project
	/// publishes its own here where it can be argued with. One CIE76 unit is
	/// roughly one just-noticeable difference for large adjacent patches under
	/// laboratory conditions, and about 2.3 is the figure usually quoted for a
	/// noticeable difference. A review surface meets none of those conditions:
	/// the marks are small, they are usually far apart on screen rather than
	/// adjacent, and they are read on uncalibrated displays in uncontrolled
	/// light. Requiring roughly four just-noticeable differences is the margin
	/// that buys.
	inline constexpr std::uint32_t distinguishable_difference_minimum_tenths = 100U;

	/// \brief Names two roles whose meanings differ, and whose colours may not.
	///
	/// \details These are the pairs where hue is doing the semantic work. The
	/// members are peers rather than a foreground and a background, so the
	/// order carries no meaning and the difference is symmetric.
	struct hue_critical_pair
	{
		appearance_role First  = appearance_role::diff_added_background;
		appearance_role Second = appearance_role::diff_removed_background;

		auto operator<=>(const hue_critical_pair&) const = default;
	};

	/// \brief Returns every pair whose distinction would otherwise be hue alone.
	///
	/// \details Added against removed is the obvious one and it is not the only
	/// one: the diff row fills against the moved fill, accepted and verified
	/// against defect, fixed against unread, and warning against error are all
	/// pairs that a palette separates by hue and a dichromat may not.
	///
	/// The span refers to storage with static lifetime and outlives any caller.
	[[nodiscard]] auto hue_critical_pairs() noexcept
		-> std::span<const hue_critical_pair>;

	/// \brief Reports how far apart one hue-critical pair stays under each model.
	///
	/// \details All three measured differences are reported rather than only the
	/// smallest, because which deficiency collapses a pair is the fact that
	/// tells a palette author what to change.
	///
	/// `DistinguishableByColorAlone` is false when any model falls under the
	/// published minimum, including `typical`: a pair two colours too close for
	/// anyone to separate is not rescued by the viewer having full colour
	/// vision. False is a statement about the palette and not a defect: it means
	/// the presentation layer must carry that distinction with an icon, a line
	/// prefix, a gutter mark, an outline, or an accessible name, which is what
	/// RFC-BPR-0006 requires of every state anyway. What the core can decide is
	/// exactly which pairs have no colour-only fallback.
	struct color_vision_finding
	{
		hue_critical_pair Pair;
		std::uint32_t     TypicalTenths               = 0U;
		std::uint32_t     ProtanopiaTenths            = 0U;
		std::uint32_t     DeuteranopiaTenths          = 0U;
		bool              DistinguishableByColorAlone = false;

		auto operator<=>(const color_vision_finding&) const = default;
	};

	/// \brief Measures one hue-critical pair under all three vision models.
	[[nodiscard]] auto measure_color_vision(
		const appearance_palette& aPalette,
		hue_critical_pair         aPair) noexcept -> color_vision_finding;

	/// \brief Measures every hue-critical pair, in `hue_critical_pairs` order.
	[[nodiscard]] auto measure_hue_critical_pairs(const appearance_palette& aPalette)
		-> std::vector<color_vision_finding>;

	/// \brief Returns the pairs that colour alone cannot separate.
	///
	/// \details This is the list a presentation adapter owes a redundant
	/// non-colour signal for. It is a requirement it must satisfy, not a palette
	/// failure it must repaint away.
	[[nodiscard]] auto pairs_requiring_non_color_indicator(
		const appearance_palette& aPalette) -> std::vector<color_vision_finding>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_CONTRAST_HPP
