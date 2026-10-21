#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_CONTRAST_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_APPEARANCE_CONTRAST_HPP

#include "bpr/core/appearance_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
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
		emphasis_text,
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

	/// \brief The minimum ratio text drawn on an emphasis fill is held to.
	///
	/// \details 3.50:1 is below WCAG 2.2 AA, and that is a deliberate,
	/// attended, and narrowly scoped decision rather than an oversight.
	///
	/// The reason it exists: an intraline emphasis fill has to be visibly
	/// different from the row fill it sits inside, and on the removed side
	/// that means darker, because red carries 0.2126 of relative luminance
	/// against green's 0.7152. Holding the emphasis fill to the full 4.50:1
	/// text floor caps its separation from the row at about 8.4, which the
	/// product owner reported as no mark at all. The floor and the mark cannot
	/// both be satisfied on the removed side; one of them had to give.
	///
	/// Its scope is exactly the two intraline emphasis fills. Row fills, the
	/// code canvas, the current line, the moved band, and every other
	/// background keep the full floor, so ordinary code is unaffected and only
	/// the changed characters inside an already-highlighted row are read at
	/// the lower ratio.
	///
	/// Its provenance is a rendered attended selection, not a preference:
	/// batch `bpr:/attended-review/betterpullreview/ebe2ee306b89f453d1de6704`
	/// presented five alternatives with each one's exact breach values, and
	/// the accepted candidate's worst standard-variant pair measures 3.50:1.
	/// Raising the separation further would lower this again, which is why the
	/// number is pinned to that decision instead of rounded.
	inline constexpr std::uint32_t emphasis_text_contrast_minimum_hundredths = 350U;

	/// \brief The minimum ratio a high-contrast variant holds emphasis text to.
	///
	/// \details 4.65:1 for the same reason and from the same accepted batch,
	/// whose worst high-contrast pair measures 4.65:1. It stays above the
	/// standard variants' emphasis floor, preserving the ordering that makes a
	/// high-contrast profile worth selecting, while acknowledging that the
	/// 7.00:1 enhanced floor and a visible mark are not simultaneously
	/// reachable on these fills.
	inline constexpr std::uint32_t high_contrast_emphasis_text_minimum_hundredths = 465U;

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
		/// \brief A wash laid over `Background` before the ratio is taken.
		///
		/// \details Set for the pairs where the reader sees the foreground
		/// through a translucent overlay rather than directly on the named
		/// background - the selected line being the only one today. Measuring
		/// such a pair against the wash's own colour asks whether text would be
		/// legible on a fill that is never painted, which is how a selection
		/// dark enough to see could read as failing while the pixels it
		/// produces pass.
		std::optional<appearance_role> Wash;

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

	/// \brief The difference an emphasis fill needs from the fill beneath it.
	///
	/// \details 20.0 is not derived from a standard, because no accessibility
	/// standard publishes a threshold for a mark painted inside the fill it
	/// marks. It is the weakest separation in the palette a product owner
	/// selected while looking at rendered captures of five alternatives, and it
	/// is published here so a later change has to argue with the observation
	/// rather than with a preference.
	///
	/// The observations it rests on, all from the real selected-diff course:
	/// 4.2 was reported as no mark at all; 7.5 and 8.7 were invisible in the
	/// dark captures; 19.2 was marginal in high-contrast dark; 38.3 was the
	/// separation the same owner described as highlighted. The accepted palette
	/// puts its weakest pair at 20.1.
	///
	/// A guessed value sat here first. It was 7.0, reasoned from
	/// just-noticeable differences and adjacency, and the dark profile passed
	/// it at 7.5 while showing nothing. That is why this constant now cites
	/// captures instead of arithmetic.
	inline constexpr std::uint32_t emphasis_separation_minimum_tenths = 200U;

	/// \brief How far apart the two sides' separations are allowed to drift.
	///
	/// \details Expressed in hundredths of a ratio, so 200 is 2.00:1. Meeting
	/// the minimum on both sides is not sufficient: a palette whose added
	/// emphasis clears the floor nine times over while its removed emphasis
	/// sits under it reads as though the tool considers one pane more important
	/// than the other, which is the defect the product owner actually reported.
	///
	/// 2.00:1 rather than 1.00:1 because the two hues cannot be made equally
	/// strong at equal luminance — green carries 0.7152 of relative luminance
	/// and red 0.2126, so a red emphasis matching a green one exactly would
	/// have to leave the text-contrast band it is painted under. The accepted
	/// palette's widest profile measures 1.87:1, which is that forced asymmetry
	/// and not a palette that drifted. The bound admits the first and refuses
	/// the second.
	inline constexpr std::uint32_t emphasis_balance_maximum_hundredths = 200U;

	/// \brief Names one fill painted over another, which it must stay visible on.
	///
	/// \details The order is load-bearing in a way `hue_critical_pair`'s is
	/// not: `Emphasis` is drawn inside `Fill`, so a finding reads as "this mark
	/// vanished into that row" rather than as an unordered distance.
	struct emphasis_pair
	{
		appearance_role Fill     = appearance_role::diff_added_background;
		appearance_role Emphasis = appearance_role::diff_intraline_added_background;

		auto operator<=>(const emphasis_pair&) const = default;
	};

	/// \brief Returns every fill that is painted over another fill.
	///
	/// \details Today this is the two intraline emphases over the two diff row
	/// fills. The set is enumerated rather than derived because "is painted
	/// over" is a presentation fact the role vocabulary does not encode, and a
	/// rule that guessed would either miss a pair or constrain a combination
	/// nobody paints.
	///
	/// The span refers to storage with static lifetime and outlives any caller.
	[[nodiscard]] auto emphasis_pairs() noexcept -> std::span<const emphasis_pair>;

	/// \brief Reports how far one emphasis fill stays from the fill beneath it.
	///
	/// \details The separation is the `typical` CIE76 difference. The dichromat
	/// models are deliberately not applied: this pair is not separated by hue
	/// carrying a meaning, it is one shade of a row marking part of that row,
	/// and both members move together under a deficiency.
	struct emphasis_finding
	{
		emphasis_pair Pair;
		std::uint32_t SeparationTenths = 0U;
		std::uint32_t MinimumTenths    = 0U;
		bool          Meets            = false;

		auto operator<=>(const emphasis_finding&) const = default;
	};

	/// \brief Measures one emphasis pair against a palette.
	[[nodiscard]] auto measure_emphasis_separation(
		const appearance_palette& aPalette,
		emphasis_pair             aPair) noexcept -> emphasis_finding;

	/// \brief Measures every emphasis pair, in `emphasis_pairs` order.
	[[nodiscard]] auto measure_emphasis_separation(const appearance_palette& aPalette)
		-> std::vector<emphasis_finding>;

	/// \brief Returns only the emphasis pairs that miss the published minimum.
	///
	/// \details Empty exactly when every emphasis fill is visible on the fill
	/// it marks.
	[[nodiscard]] auto failing_emphasis_separation(const appearance_palette& aPalette)
		-> std::vector<emphasis_finding>;

	/// \brief Reports whether the emphasis pairs are comparably strong.
	///
	/// \details `RatioHundredths` is the strongest separation over the weakest,
	/// so an exactly balanced palette reports 100. A palette with any pair at
	/// zero separation reports the maximum representable ratio rather than
	/// dividing by zero; such a palette has already failed
	/// `failing_emphasis_separation`, and the balance answer for it is "as
	/// unbalanced as it is possible to be".
	struct emphasis_balance_finding
	{
		std::uint32_t StrongestTenths = 0U;
		std::uint32_t WeakestTenths   = 0U;
		std::uint32_t RatioHundredths = 0U;
		std::uint32_t MaximumHundredths
			= emphasis_balance_maximum_hundredths;
		bool Meets = false;

		auto operator<=>(const emphasis_balance_finding&) const = default;
	};

	/// \brief Measures how far the emphasis pairs' separations drift apart.
	[[nodiscard]] auto measure_emphasis_balance(const appearance_palette& aPalette)
		-> emphasis_balance_finding;

	/// \brief Returns the foreground an emphasis fill would carry itself.
	///
	/// \details Black or white, whichever the fill contrasts with more. A fill
	/// bright enough to be unmistakable in a high-contrast profile cannot also
	/// let the profile's own syntax colours read on it — a dark profile's text
	/// is light, and light text on bright amber is unreadable at any size. Such
	/// a fill carries its own foreground instead, and this is it.
	///
	/// The choice is between the two extremes rather than a palette role
	/// because the point is maximum legibility on one specific fill, and no
	/// role can be guaranteed to beat both extremes on an arbitrary colour.
	[[nodiscard]] auto emphasis_foreground(appearance_color aFill) noexcept
		-> appearance_color;

	/// \brief Keeps readable syntax ink, otherwise moves it toward the nearest
	/// readable tint or shade instead of erasing its semantic hue.
	///
	/// \details Ink that misses the emphasis floor moves until it reads at the
	/// text floor. On a fill that wants light ink it first brightens along its
	/// own hue, every channel scaled by one factor until the largest is full,
	/// and only then tints toward white, so a green comment stays a brighter
	/// green rather than turning white ([[BPR-1709]]). On a fill that
	/// wants dark ink it shades toward black, which keeps hue and saturation.
	/// High-contrast variants retain their maximum-contrast black/white ink.
	[[nodiscard]] auto readable_emphasis_foreground(
		appearance_color aSyntax, appearance_color aFill, appearance_variant aVariant) noexcept
		-> appearance_color;

	/// \brief The ink a syntax-coloured glyph is drawn in on an opaque text
	/// selection.
	///
	/// \details [[BPR-1156]]. The code view selects text as every text box in
	/// the window does, with `code_selection_background` filled solid, rather
	/// than with the translucent wash the diff panes lay over a selected line,
	/// which the owner read as darker than a usual selection (composer-layout
	/// v005, 2026-10-02). Unlike Qt's own selection it keeps the glyphs' role
	/// colours, which the owner asked for in v004. A role colour that already
	/// reaches the variant's text floor on the fill is kept; one that does not
	/// moves toward black or white, whichever the fill contrasts with more,
	/// by the least amount that reaches the floor, so it stays recognisably
	/// its role. When even black or white cannot reach the floor, it is that
	/// extreme.
	///
	/// The required-pair audit is unchanged: its wash pairs measure the diff
	/// panes' line wash, which they still paint, and this ink meets the text
	/// floor by construction wherever any ink can.
	[[nodiscard]] auto readable_selection_ink(
		appearance_color aSyntax, appearance_color aFill, appearance_variant aVariant) noexcept
		-> appearance_color;

	/// \brief [[BPR-1585]]. The step, in hundredths of a contrast ratio, that the
	/// Overview Files table's quiet row band stands from the table's ground.
	inline constexpr std::uint32_t quiet_band_contrast_hundredths = 108U;

	/// \brief [[BPR-1604]]. The floor an ink holds on a table's ground so that it
	/// still reaches `aMinimum` on the quiet band: the floor raised by the band's
	/// step, rounded up, with one hundredth for the band's own rounding.
	[[nodiscard]] constexpr auto banded_floor_hundredths(const std::uint32_t aMinimum) noexcept -> std::uint32_t
	{
		return ((aMinimum * quiet_band_contrast_hundredths) + 99U) / 100U + 1U;
	}

	/// \brief [[BPR-1604]]. One ground an ink is drawn on, and the ratio it
	/// must reach there.
	struct contrast_ground
	{
		appearance_color Colour;
		std::uint32_t    MinimumHundredths = text_contrast_minimum_hundredths;

		auto operator<=>(const contrast_ground&) const = default;
	};

	/// \brief [[BPR-1604]]. The quiet secondary ink: notes, folder paths,
	/// hints, unknown marks.
	///
	/// \details The midpoint of `aText` and `aBase` (each channel's integer
	/// mean, `aText`'s alpha), which is what the derived palette always gave
	/// `PlaceholderText`, wherever it reaches every ground's floor. Where it
	/// does not, the least move from it toward `aText` that does, so it stays a
	/// grey between the text and the ground: quieter than the text and never a
	/// new hue. When even `aText` cannot reach a floor, `aText`.
	[[nodiscard]] auto readable_muted_ink(appearance_color aText, appearance_color aBase, std::span<const contrast_ground> aGrounds) noexcept -> appearance_color;

	/// \brief [[BPR-1604]]. A state colour (the attention amber) drawn as
	/// digits, a badge with a letter cut from the ground, or a small mark.
	///
	/// \details `aSignal` wherever it reaches every ground's floor. Otherwise
	/// it moves away from the first ground, by the least amount that reaches
	/// every floor, in two stages that keep its character. On a light ground it
	/// first darkens only its weaker channels, holding the strongest, so it
	/// keeps its saturation and leans toward its dominant primary: an amber
	/// turns a burnt orange instead of the olive brown that mixing it with black
	/// gives. On a dark ground it first raises its strongest channel. Only if
	/// that stage cannot reach the floors does it then move toward black or
	/// white, and it is that extreme when nothing between does. A grey has no
	/// weaker channel and goes straight to the second stage.
	[[nodiscard]] auto readable_signal_ink(appearance_color aSignal, std::span<const contrast_ground> aGrounds) noexcept -> appearance_color;

	/// \brief The colour a keyboard focus ring is drawn in over `aSurface`.
	///
	/// \details [[BPR-1157]]. The window's focus rings are drawn in the
	/// selection colour (`code_selection_background`, which the profile
	/// gives Qt as `Highlight`), as Home's bar and panels already draw
	/// theirs. A selection colour is chosen to sit quietly under text, so
	/// on the standard dark surface it measured about 2:1 and the owner
	/// could not tell what had focus (`BPR-1157-native-keyboard-v001`,
	/// 2026-10-02). A ring is an indicator, so it is held to the indicator
	/// floor: a colour that already reaches it on the surface is kept, and
	/// one that does not moves toward black or white, whichever the surface
	/// contrasts with more, by the least amount that reaches it. The hue
	/// stays the theme's. When even black or white cannot reach the floor,
	/// it is that extreme.
	[[nodiscard]] auto visible_focus_indicator(appearance_color aAccent, appearance_color aSurface) noexcept -> appearance_color;

	/// \brief Reports whether an emphasis fill must carry its own foreground.
	///
	/// \details True when any syntax role fails the emphasis floor over the
	/// fill, which is the measurable form of "the profile's own text cannot be
	/// read on this". A profile whose emphasis fills are gentle tints — the
	/// standard light and dark ones — answers false and keeps its syntax
	/// colouring showing through the mark, which is what makes an ordinary
	/// diff readable.
	[[nodiscard]] auto emphasis_needs_own_foreground(
		const appearance_palette& aPalette,
		appearance_role           aFill,
		appearance_variant        aVariant) noexcept -> bool;

	/// \brief The separation a mark derived from its own fill is held to.
	///
	/// \details 23.0, taken from a reference diff the product owner supplied as
	/// the standard to meet. Its dark rows sit 23.3 and 24.4 from their
	/// intraline marks, so this is a measured preference rather than a chosen
	/// number. An earlier version of this constant was 9.0, reasoned from the
	/// idea that a separate signal was carrying the mark, and the result was
	/// rejected as terrible.
	inline constexpr std::uint32_t derived_emphasis_separation_minimum_tenths = 230U;

	/// \brief Returns a stronger version of one fill, in the fill's own hue.
	///
	/// \details For a fill the palette has no emphasis role for — a row wearing
	/// the selection wash above all — the mark cannot be a palette colour,
	/// because the fill it sits inside is a composite the palette never names.
	/// The product owner's requirement is exact: "if the line fill is blue the
	/// intraline emphasis fill should also be a blue, just a darker or lighter
	/// version". Hue is therefore preserved and only saturation and lightness
	/// move.
	///
	/// The direction is away from the ground the fill sits on: a dark fill
	/// brightens, a light one darkens, and both gain saturation. That is the
	/// same move the supplied reference makes — its dark marks go from value
	/// 0.37 to 0.58 and saturation 0.33 to 0.51 — rather than the deeper,
	/// muddier direction an earlier attempt took.
	///
	/// Returns the fill unchanged when the minimum cannot be reached, which is
	/// honest rather than convenient.
	[[nodiscard]] auto derived_emphasis_fill(
		appearance_color   aFill,
		appearance_variant aVariant) noexcept -> appearance_color;

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
