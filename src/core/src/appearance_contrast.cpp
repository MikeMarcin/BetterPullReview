#include "bpr/core/appearance_contrast.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace bpr::core {
	namespace {
		constexpr double channel_maximum = 255.0;

		// The sRGB electro-optical transfer function and its inverse.
		constexpr double srgb_decode_threshold = 0.040'45;
		constexpr double srgb_encode_threshold = 0.003'130'8;
		constexpr double srgb_linear_slope     = 12.92;
		constexpr double srgb_offset           = 0.055;
		constexpr double srgb_scale            = 1.055;
		constexpr double srgb_gamma            = 2.4;

		// The Rec. 709 luminance weights the contrast ratio is defined over.
		constexpr double luminance_red   = 0.2126;
		constexpr double luminance_green = 0.7152;
		constexpr double luminance_blue  = 0.0722;

		// The constant that keeps the ratio finite for two black colours and
		// caps it at 21.00:1 for black against white.
		constexpr double contrast_offset = 0.05;

		constexpr double hundredths_scale = 100.0;
		constexpr double tenths_scale     = 10.0;

		/// \brief Holds one colour as linear sRGB, before any encoding.
		struct linear_rgb
		{
			double Red   = 0.0;
			double Green = 0.0;
			double Blue  = 0.0;
		};

		/// \brief Holds one colour as CIELAB coordinates.
		struct lab_color
		{
			double Lightness = 0.0;
			double A         = 0.0;
			double B         = 0.0;
		};

		[[nodiscard]] auto decode_channel(const std::uint8_t aValue) noexcept -> double
		{
			const auto normalised = static_cast<double>(aValue) / channel_maximum;
			return normalised <= srgb_decode_threshold
				? normalised / srgb_linear_slope
				: std::pow((normalised + srgb_offset) / srgb_scale, srgb_gamma);
		}

		[[nodiscard]] auto encode_channel(const double aValue) noexcept -> std::uint8_t
		{
			const auto bounded = std::clamp(aValue, 0.0, 1.0);
			const auto encoded = bounded <= srgb_encode_threshold
				? bounded * srgb_linear_slope
				: (srgb_scale * std::pow(bounded, 1.0 / srgb_gamma)) - srgb_offset;
			const auto quantum = std::lround(std::clamp(encoded, 0.0, 1.0) * channel_maximum);
			return static_cast<std::uint8_t>(quantum);
		}

		[[nodiscard]] auto linear_of(const appearance_color aColor) noexcept -> linear_rgb
		{
			return linear_rgb{
				.Red   = decode_channel(aColor.Red),
				.Green = decode_channel(aColor.Green),
				.Blue  = decode_channel(aColor.Blue),
			};
		}

		[[nodiscard]] auto relative_luminance(const linear_rgb aColor) noexcept -> double
		{
			return (luminance_red * aColor.Red)
				+ (luminance_green * aColor.Green)
				+ (luminance_blue * aColor.Blue);
		}

		/// \brief Truncates a scaled non-negative measurement to a whole unit.
		[[nodiscard]] auto truncated(const double aValue, const double aScale) noexcept
			-> std::uint32_t
		{
			const auto scaled = std::floor(std::max(aValue, 0.0) * aScale);
			return static_cast<std::uint32_t>(scaled);
		}

		// The Viénot, Brettel and Mollon (1999) dichromat projections, in linear
		// sRGB. Each row is one output channel. Both matrices leave a neutral
		// colour untouched, which is the property that makes them recognisable:
		// a dichromacy loses a chromatic axis, it does not shift greys.
		constexpr std::array<std::array<double, 3U>, 3U> protanopia_matrix = { {
			{ 0.112'38, 0.887'62, 0.0 },
			{ 0.072'76, 0.927'24, 0.0 },
			{ -0.003'99, 0.003'99, 1.0 },
		} };

		constexpr std::array<std::array<double, 3U>, 3U> deuteranopia_matrix = { {
			{ 0.292'75, 0.707'25, 0.0 },
			{ 0.345'97, 0.654'03, 0.0 },
			{ -0.027'95, 0.027'95, 1.0 },
		} };

		[[nodiscard]] auto projected(
			const linear_rgb                              aColor,
			const std::array<std::array<double, 3U>, 3U>& aMatrix) noexcept -> linear_rgb
		{
			const auto row = [&aColor, &aMatrix](const std::size_t aIndex) {
				return (aMatrix[aIndex][0U] * aColor.Red)
					+ (aMatrix[aIndex][1U] * aColor.Green)
					+ (aMatrix[aIndex][2U] * aColor.Blue);
			};

			return linear_rgb{
				.Red   = row(0U),
				.Green = row(1U),
				.Blue  = row(2U),
			};
		}

		// Linear sRGB to CIE XYZ under D65, and the white point that matrix maps
		// pure white onto. Taking the white point from the matrix's own column
		// sums rather than from a separately quoted illuminant keeps white at
		// exactly L* 100 with no chroma.
		constexpr std::array<std::array<double, 3U>, 3U> xyz_matrix = { {
			{ 0.412'456'4, 0.357'576'1, 0.180'437'5 },
			{ 0.212'672'9, 0.715'152'2, 0.072'175'0 },
			{ 0.019'333'9, 0.119'192'0, 0.950'304'1 },
		} };

		constexpr double white_x
			= xyz_matrix[0U][0U] + xyz_matrix[0U][1U] + xyz_matrix[0U][2U];
		constexpr double white_y
			= xyz_matrix[1U][0U] + xyz_matrix[1U][1U] + xyz_matrix[1U][2U];
		constexpr double white_z
			= xyz_matrix[2U][0U] + xyz_matrix[2U][1U] + xyz_matrix[2U][2U];

		// The CIELAB companding function, with its linear segment below the
		// intersection at (6/29)^3.
		constexpr double lab_epsilon   = 216.0 / 24'389.0;
		constexpr double lab_slope     = 841.0 / 108.0;
		constexpr double lab_intercept = 4.0 / 29.0;
		constexpr double lab_lightness = 116.0;
		constexpr double lab_offset    = 16.0;
		constexpr double lab_a_scale   = 500.0;
		constexpr double lab_b_scale   = 200.0;

		[[nodiscard]] auto companded(const double aRatio) noexcept -> double
		{
			return aRatio > lab_epsilon
				? std::cbrt(aRatio)
				: (lab_slope * aRatio) + lab_intercept;
		}

		[[nodiscard]] auto lab_of(const linear_rgb aColor) noexcept -> lab_color
		{
			const auto bounded = linear_rgb{
				.Red   = std::clamp(aColor.Red, 0.0, 1.0),
				.Green = std::clamp(aColor.Green, 0.0, 1.0),
				.Blue  = std::clamp(aColor.Blue, 0.0, 1.0),
			};
			const auto component = [&bounded](const std::size_t aIndex) {
				return (xyz_matrix[aIndex][0U] * bounded.Red)
					+ (xyz_matrix[aIndex][1U] * bounded.Green)
					+ (xyz_matrix[aIndex][2U] * bounded.Blue);
			};

			const auto fx = companded(component(0U) / white_x);
			const auto fy = companded(component(1U) / white_y);
			const auto fz = companded(component(2U) / white_z);

			return lab_color{
				.Lightness = (lab_lightness * fy) - lab_offset,
				.A         = lab_a_scale * (fx - fy),
				.B         = lab_b_scale * (fy - fz),
			};
		}

		/// \brief Holds one colour as hue, saturation and value.
		struct hsv_color
		{
			double Hue        = 0.0;
			double Saturation = 0.0;
			double Value      = 0.0;
		};

		[[nodiscard]] auto hsv_of(const appearance_color aColor) noexcept -> hsv_color
		{
			const auto red   = static_cast<double>(aColor.Red) / channel_maximum;
			const auto green = static_cast<double>(aColor.Green) / channel_maximum;
			const auto blue  = static_cast<double>(aColor.Blue) / channel_maximum;
			const auto high  = std::max({ red, green, blue });
			const auto low   = std::min({ red, green, blue });
			const auto span  = high - low;

			auto hue = 0.0;
			if (span > 0.0) {
				if (high == red) {
					hue = std::fmod(((green - blue) / span) + 6.0, 6.0);
				} else if (high == green) {
					hue = ((blue - red) / span) + 2.0;
				} else {
					hue = ((red - green) / span) + 4.0;
				}
				hue /= 6.0;
			}

			return hsv_color{
				.Hue        = hue,
				.Saturation = high > 0.0 ? span / high : 0.0,
				.Value      = high,
			};
		}

		[[nodiscard]] auto rgb_of(const hsv_color aColor) noexcept -> appearance_color
		{
			const auto sector    = aColor.Hue * 6.0;
			const auto index     = static_cast<int>(std::floor(sector)) % 6;
			const auto remainder = sector - std::floor(sector);
			const auto p         = aColor.Value * (1.0 - aColor.Saturation);
			const auto q         = aColor.Value * (1.0 - (aColor.Saturation * remainder));
			const auto t         = aColor.Value * (1.0 - (aColor.Saturation * (1.0 - remainder)));

			auto red   = 0.0;
			auto green = 0.0;
			auto blue  = 0.0;
			switch (index) {
			case 0:
				red   = aColor.Value;
				green = t;
				blue  = p;
				break;
			case 1:
				red   = q;
				green = aColor.Value;
				blue  = p;
				break;
			case 2:
				red   = p;
				green = aColor.Value;
				blue  = t;
				break;
			case 3:
				red   = p;
				green = q;
				blue  = aColor.Value;
				break;
			case 4:
				red   = t;
				green = p;
				blue  = aColor.Value;
				break;
			default:
				red   = aColor.Value;
				green = p;
				blue  = q;
				break;
			}

			const auto quantise = [](const double aChannel) {
				return static_cast<std::uint8_t>(
					std::lround(std::clamp(aChannel, 0.0, 1.0) * channel_maximum));
			};
			return appearance_color{ quantise(red), quantise(green), quantise(blue), 255U };
		}

		// --- the checked pair tables -------------------------------------

		constexpr auto first_syntax_role = appearance_role::syntax_normal;
		constexpr auto last_syntax_role  = appearance_role::syntax_other;
		constexpr auto original_syntax_role_count
			= (index_of(last_syntax_role) - index_of(first_syntax_role)) + 1U;
		// New roles append to the durable vocabulary, rather than renumbering
		// palettes. They have the same text obligations as the original run.
		constexpr auto syntax_roles = [] {
			auto roles = std::array<appearance_role, original_syntax_role_count + 2U>{ };
			for (auto index = std::size_t{ 0U }; index < original_syntax_role_count; ++index) {
				roles[index] = static_cast<appearance_role>(index_of(first_syntax_role) + index);
			}
			roles[original_syntax_role_count]      = appearance_role::syntax_namespace;
			roles[original_syntax_role_count + 1U] = appearance_role::syntax_enum_member;
			return roles;
		}();
		constexpr auto syntax_role_count = syntax_roles.size();

		static_assert(
			original_syntax_role_count == 26U,
			"the original syntax roles retain their contiguous ordinal identities");
		static_assert(
			index_of(last_syntax_role) + 1U == index_of(first_feedback_label_role),
			"the original syntax roles retain their position before the label tints");

		/// \brief Selects the floor one background holds its text to.
		///
		/// \details Only the two intraline emphasis fills carry the reduced
		/// emphasis floor. Every other background, including the diff row
		/// fills those two sit inside, keeps the full text floor.
		[[nodiscard]] consteval auto kind_for_background(
			const appearance_role aBackground) -> contrast_kind
		{
			return aBackground == appearance_role::diff_intraline_added_background
					|| aBackground == appearance_role::diff_intraline_removed_background
				? contrast_kind::emphasis_text
				: contrast_kind::text;
		}

		/// \brief The backgrounds that syntax-coloured code text is drawn over.
		constexpr auto code_canvas_backgrounds = std::array{
			appearance_role::code_background,
			appearance_role::code_current_line_background,
			appearance_role::diff_added_background,
			appearance_role::diff_removed_background,
			appearance_role::diff_intraline_added_background,
			appearance_role::diff_intraline_removed_background,
			appearance_role::diff_moved_background,
		};

		/// \brief The text pairs that are not part of the code canvas.
		constexpr auto fixed_text_pairs = std::array{
			contrast_pair{
				.Foreground = appearance_role::application_text,
				.Background = appearance_role::application_surface,
				.Kind       = contrast_kind::text,
			},
			contrast_pair{
				.Foreground = appearance_role::code_line_number_text,
				.Background = appearance_role::code_gutter_background,
				.Kind       = contrast_kind::text,
			},
			contrast_pair{
				.Foreground = appearance_role::diff_hunk_header_text,
				.Background = appearance_role::diff_hunk_header_background,
				.Kind       = contrast_kind::text,
			},
		};

		/// \brief Marks drawn on the code canvas rather than filled behind it.
		///
		/// \details The boundary rule is a drawn line, so the indicator ratio is
		/// the right obligation for it - unlike the row fills above, where a
		/// luminance ratio measures the wrong property of a tint.
		constexpr auto canvas_indicator_pairs = std::array{
			contrast_pair{
				.Foreground = appearance_role::diff_group_boundary_rule,
				.Background = appearance_role::code_background,
				.Kind       = contrast_kind::indicator,
			},
		};

		/// \brief The review-state roles, which are marks rather than text.
		constexpr auto state_roles = std::array{
			appearance_role::state_unread,
			appearance_role::state_accepted,
			appearance_role::state_defect,
			appearance_role::state_fixed,
			appearance_role::state_verified,
			appearance_role::state_stale,
		};

		/// \brief Overlays a selected line is read through.
		///
		/// \details The selected line and Find's matches are the places the
		/// product draws code through a translucent layer, and a background it
		/// can land on has to stay legible with it on top as well as bare -
		/// selecting or finding on an added line must not make it unreadable.
		constexpr auto canvas_washes = std::array{
			appearance_role::code_selection_background,
			// [[BPR-1503]]. Find lays its match and current-match washes over
			// the same backgrounds at the same opacity as the selection.
			appearance_role::find_match_background,
			appearance_role::find_current_match_background,
		};

		/// \brief The backgrounds a wash can actually be composited over.
		///
		/// \details The row fills and the pane itself. Deliberately not every
		/// canvas background: `code_current_line_background` is the item-view
		/// alternate base, which no pane paints and which views draw a solid
		/// selection over rather than a wash, so pairing it with the wash would
		/// constrain a composite that is never produced.
		///
		/// That exclusion is load-bearing rather than tidy, so it is stated
		/// plainly. Including it makes a legible selection unreachable in the
		/// light profile at any value: with the accepted selection colour the
		/// tightest syntax role reaches 4.22:1 over the present tint and only
		/// 4.43:1 over a tint of #FDFDFE, which is already invisible at 1.017:1
		/// against the pane. The constraint is unsatisfiable, not merely tight.
		/// If a pane ever paints a current-line highlight, this list has to
		/// grow and that trade-off has to be reopened.
		constexpr auto wash_backgrounds = std::array{
			appearance_role::code_background,
			appearance_role::diff_added_background,
			appearance_role::diff_removed_background,
			appearance_role::diff_intraline_added_background,
			appearance_role::diff_intraline_removed_background,
			appearance_role::diff_moved_background,
		};

		constexpr auto required_pair_count = fixed_text_pairs.size()
			+ canvas_indicator_pairs.size()
			+ (syntax_role_count * code_canvas_backgrounds.size())
			+ (syntax_role_count * wash_backgrounds.size() * canvas_washes.size())
			+ state_roles.size();

		[[nodiscard]] consteval auto build_required_pairs()
			-> std::array<contrast_pair, required_pair_count>
		{
			auto pairs = std::array<contrast_pair, required_pair_count>{ };
			auto next  = std::size_t{ 0U };

			for (const auto pair : fixed_text_pairs) {
				pairs[next] = pair;
				++next;
			}
			for (const auto background : code_canvas_backgrounds) {
				for (auto offset = std::size_t{ 0U }; offset < syntax_role_count; ++offset) {
					pairs[next] = contrast_pair{
						.Foreground = syntax_roles[offset],
						.Background = background,
						.Kind       = kind_for_background(background),
					};
					++next;
				}
			}
			for (const auto wash : canvas_washes) {
				for (const auto background : wash_backgrounds) {
					for (auto offset = std::size_t{ 0U }; offset < syntax_role_count;
						 ++offset) {
						pairs[next] = contrast_pair{
							.Foreground = syntax_roles[offset],
							.Background = background,
							.Kind       = kind_for_background(background),
							.Wash       = wash,
						};
						++next;
					}
				}
			}

			for (const auto pair : canvas_indicator_pairs) {
				pairs[next] = pair;
				++next;
			}

			for (const auto state : state_roles) {
				pairs[next] = contrast_pair{
					.Foreground = state,
					.Background = appearance_role::application_surface,
					.Kind       = contrast_kind::indicator,
				};
				++next;
			}

			if (next != required_pair_count) {
				throw std::logic_error{ "every required contrast pair must be assigned" };
			}

			return pairs;
		}

		constexpr auto required_pairs = build_required_pairs();

		/// \brief The pairs a palette separates by hue and a dichromat may not.
		constexpr auto hue_critical_table = std::array{
			hue_critical_pair{
				.First  = appearance_role::diff_added_background,
				.Second = appearance_role::diff_removed_background,
			},
			hue_critical_pair{
				.First  = appearance_role::diff_intraline_added_background,
				.Second = appearance_role::diff_intraline_removed_background,
			},
			hue_critical_pair{
				.First  = appearance_role::diff_added_background,
				.Second = appearance_role::diff_moved_background,
			},
			hue_critical_pair{
				.First  = appearance_role::diff_removed_background,
				.Second = appearance_role::diff_moved_background,
			},
			hue_critical_pair{
				.First  = appearance_role::state_accepted,
				.Second = appearance_role::state_defect,
			},
			hue_critical_pair{
				.First  = appearance_role::state_verified,
				.Second = appearance_role::state_defect,
			},
			hue_critical_pair{
				.First  = appearance_role::state_fixed,
				.Second = appearance_role::state_unread,
			},
			hue_critical_pair{
				.First  = appearance_role::syntax_warning,
				.Second = appearance_role::syntax_error,
			},
		};

		/// \brief The fills that are painted inside another fill.
		constexpr auto emphasis_table = std::array{
			emphasis_pair{
				.Fill     = appearance_role::diff_added_background,
				.Emphasis = appearance_role::diff_intraline_added_background,
			},
			emphasis_pair{
				.Fill     = appearance_role::diff_removed_background,
				.Emphasis = appearance_role::diff_intraline_removed_background,
			},
		};
	} // namespace

	auto contrast_ratio_hundredths(
		const appearance_color aFirst,
		const appearance_color aSecond) noexcept -> std::uint32_t
	{
		const auto first    = relative_luminance(linear_of(aFirst));
		const auto second   = relative_luminance(linear_of(aSecond));
		const auto brighter = std::max(first, second);
		const auto darker   = std::min(first, second);
		const auto ratio    = (brighter + contrast_offset) / (darker + contrast_offset);

		return truncated(ratio, hundredths_scale);
	}

	auto minimum_contrast_hundredths(
		const contrast_kind      aKind,
		const appearance_variant aVariant) noexcept -> std::uint32_t
	{
		if (aKind == contrast_kind::indicator) {
			return indicator_contrast_minimum_hundredths;
		}
		if (aKind == contrast_kind::emphasis_text) {
			return high_contrast(aVariant)
				? high_contrast_emphasis_text_minimum_hundredths
				: emphasis_text_contrast_minimum_hundredths;
		}

		return high_contrast(aVariant)
			? high_contrast_text_minimum_hundredths
			: text_contrast_minimum_hundredths;
	}

	auto required_contrast_pairs() noexcept -> std::span<const contrast_pair>
	{
		return std::span<const contrast_pair>{ required_pairs };
	}

	auto measure_contrast(
		const appearance_palette& aPalette,
		const contrast_pair       aPair,
		const appearance_variant  aVariant) noexcept -> contrast_finding
	{
		const auto background = aPair.Wash.has_value()
			? composited(
				  aPalette[index_of(*aPair.Wash)],
				  aPalette[index_of(aPair.Background)],
				  selection_wash_alpha)
			: aPalette[index_of(aPair.Background)];
		// Emphasis paints the token's readable tint or shade, not a global
		// black/white override. Audit the same foreground the pane paints.
		const auto foreground
			= aPair.Kind == contrast_kind::emphasis_text
			? readable_emphasis_foreground(aPalette[index_of(aPair.Foreground)], background, aVariant)
			: aPalette[index_of(aPair.Foreground)];
		const auto ratio   = contrast_ratio_hundredths(foreground, background);
		const auto minimum = minimum_contrast_hundredths(aPair.Kind, aVariant);

		return contrast_finding{
			.Pair              = aPair,
			.RatioHundredths   = ratio,
			.MinimumHundredths = minimum,
			.Meets             = ratio >= minimum,
		};
	}

	auto measure_required_contrast(
		const appearance_palette& aPalette,
		const appearance_variant  aVariant) -> std::vector<contrast_finding>
	{
		auto findings = std::vector<contrast_finding>{ };
		findings.reserve(required_pairs.size());
		for (const auto pair : required_pairs) {
			findings.push_back(measure_contrast(aPalette, pair, aVariant));
		}

		return findings;
	}

	auto failing_contrast(
		const appearance_palette& aPalette,
		const appearance_variant  aVariant) -> std::vector<contrast_finding>
	{
		auto failures = std::vector<contrast_finding>{ };
		for (const auto pair : required_pairs) {
			const auto finding = measure_contrast(aPalette, pair, aVariant);
			if (!finding.Meets) {
				failures.push_back(finding);
			}
		}

		return failures;
	}

	auto simulate_color_vision(
		const appearance_color aColor,
		const color_vision     aVision) noexcept -> appearance_color
	{
		if (aVision == color_vision::typical) {
			return aColor;
		}

		const auto simulated = projected(
			linear_of(aColor),
			aVision == color_vision::protanopia ? protanopia_matrix : deuteranopia_matrix);

		return appearance_color{
			encode_channel(simulated.Red),
			encode_channel(simulated.Green),
			encode_channel(simulated.Blue),
			aColor.Alpha,
		};
	}

	auto color_difference_tenths(
		const appearance_color aFirst,
		const appearance_color aSecond,
		const color_vision     aVision) noexcept -> std::uint32_t
	{
		const auto first  = lab_of(linear_of(simulate_color_vision(aFirst, aVision)));
		const auto second = lab_of(linear_of(simulate_color_vision(aSecond, aVision)));

		const auto lightness = first.Lightness - second.Lightness;
		const auto a         = first.A - second.A;
		const auto b         = first.B - second.B;
		const auto difference
			= std::sqrt((lightness * lightness) + (a * a) + (b * b));

		return truncated(difference, tenths_scale);
	}

	auto emphasis_pairs() noexcept -> std::span<const emphasis_pair>
	{
		return std::span<const emphasis_pair>{ emphasis_table };
	}

	auto measure_emphasis_separation(
		const appearance_palette& aPalette,
		const emphasis_pair       aPair) noexcept -> emphasis_finding
	{
		const auto fill     = aPalette[index_of(aPair.Fill)];
		const auto emphasis = aPalette[index_of(aPair.Emphasis)];
		const auto separation
			= color_difference_tenths(fill, emphasis, color_vision::typical);

		return emphasis_finding{
			.Pair             = aPair,
			.SeparationTenths = separation,
			.MinimumTenths    = emphasis_separation_minimum_tenths,
			.Meets            = separation >= emphasis_separation_minimum_tenths,
		};
	}

	auto measure_emphasis_separation(const appearance_palette& aPalette)
		-> std::vector<emphasis_finding>
	{
		auto findings = std::vector<emphasis_finding>{ };
		findings.reserve(emphasis_table.size());
		for (const auto pair : emphasis_table) {
			findings.push_back(measure_emphasis_separation(aPalette, pair));
		}

		return findings;
	}

	auto failing_emphasis_separation(const appearance_palette& aPalette)
		-> std::vector<emphasis_finding>
	{
		auto failing = std::vector<emphasis_finding>{ };
		for (const auto pair : emphasis_table) {
			const auto finding = measure_emphasis_separation(aPalette, pair);
			if (!finding.Meets) {
				failing.push_back(finding);
			}
		}

		return failing;
	}

	auto measure_emphasis_balance(const appearance_palette& aPalette)
		-> emphasis_balance_finding
	{
		auto strongest = std::uint32_t{ 0U };
		auto weakest   = std::numeric_limits<std::uint32_t>::max();
		for (const auto pair : emphasis_table) {
			const auto separation
				= measure_emphasis_separation(aPalette, pair).SeparationTenths;
			strongest = std::max(strongest, separation);
			weakest   = std::min(weakest, separation);
		}

		// A pair with no separation at all has already failed the minimum, and
		// dividing by it would be undefined rather than merely uninformative.
		// Reporting the largest representable ratio keeps the finding readable
		// as the extreme it is.
		constexpr auto hundredthsPerUnit = std::uint64_t{ 100U };
		const auto     ratio             = weakest == 0U
			? std::numeric_limits<std::uint32_t>::max()
			: static_cast<std::uint32_t>(
				  (static_cast<std::uint64_t>(strongest) * hundredthsPerUnit)
				  / weakest);

		return emphasis_balance_finding{
			.StrongestTenths   = strongest,
			.WeakestTenths     = weakest,
			.RatioHundredths   = ratio,
			.MaximumHundredths = emphasis_balance_maximum_hundredths,
			.Meets             = ratio <= emphasis_balance_maximum_hundredths,
		};
	}

	auto emphasis_foreground(const appearance_color aFill) noexcept
		-> appearance_color
	{
		constexpr auto black = appearance_color{ 0U, 0U, 0U, 255U };
		constexpr auto white = appearance_color{ 255U, 255U, 255U, 255U };
		return contrast_ratio_hundredths(black, aFill)
				>= contrast_ratio_hundredths(white, aFill)
			? black
			: white;
	}

	namespace {
		/// `aColour` with every channel scaled by one factor so its largest
		/// becomes `aPeak`: the same hue and saturation at another brightness.
		[[nodiscard]] auto scaled_to_peak(const appearance_color aColour, const std::uint32_t aPeak) noexcept -> appearance_color
		{
			const auto largest = std::uint32_t{ std::max({ aColour.Red, aColour.Green, aColour.Blue }) };
			const auto scale   = [largest, aPeak](const std::uint8_t aChannel) {
				return static_cast<std::uint8_t>(((std::uint32_t{ aChannel } * aPeak) + (largest / 2U)) / largest);
			};
			return appearance_color{ scale(aColour.Red), scale(aColour.Green), scale(aColour.Blue), aColour.Alpha };
		}
	} // namespace

	auto readable_emphasis_foreground(
		const appearance_color aSyntax, const appearance_color aFill,
		const appearance_variant aVariant) noexcept -> appearance_color
	{
		if (high_contrast(aVariant)) {
			return emphasis_foreground(aFill);
		}
		const auto minimum = minimum_contrast_hundredths(contrast_kind::emphasis_text, aVariant);
		if (contrast_ratio_hundredths(aSyntax, aFill) >= minimum) {
			return aSyntax;
		}
		const auto     target             = emphasis_foreground(aFill);
		constexpr auto adjustmentHeadroom = 25U;
		const auto     adjustedMinimum    = std::min(
			contrast_ratio_hundredths(target, aFill),
			std::max(text_contrast_minimum_hundredths, minimum + adjustmentHeadroom));
		// [[BPR-1709]]. Toward white the ink first brightens along its own
		// hue, as far as its largest channel can go, and only then tints:
		// blending straight toward white drains the hue, and a green comment
		// on a selected row's mark read as white. Toward black, compositing
		// only lowers the value, which already keeps hue and saturation.
		constexpr auto white    = appearance_color{ 255U, 255U, 255U, 255U };
		constexpr auto fullPeak = std::uint32_t{ 255U };
		auto           from     = aSyntax;
		const auto     largest  = std::uint32_t{ std::max({ aSyntax.Red, aSyntax.Green, aSyntax.Blue }) };
		if (target == white && largest > 0U) {
			for (auto peak = largest + 1U; peak <= fullPeak; ++peak) {
				const auto candidate = scaled_to_peak(aSyntax, peak);
				if (contrast_ratio_hundredths(candidate, aFill) >= adjustedMinimum) {
					return candidate;
				}
			}
			from = scaled_to_peak(aSyntax, fullPeak);
		}
		for (auto alpha = std::uint16_t{ 1U }; alpha <= 255U; ++alpha) {
			const auto candidate = composited(target, from, static_cast<std::uint8_t>(alpha));
			if (contrast_ratio_hundredths(candidate, aFill) >= adjustedMinimum) {
				return candidate;
			}
		}
		return target;
	}

	namespace {
		/// `aColour`, or the least move of it toward black or white, whichever
		/// `aGround` contrasts with more, that reaches `aMinimum` on `aGround`.
		auto reaching_floor(const appearance_color aColour, const appearance_color aGround, const std::uint32_t aMinimum) noexcept -> appearance_color
		{
			if (contrast_ratio_hundredths(aColour, aGround) >= aMinimum) {
				return aColour;
			}
			const auto target = emphasis_foreground(aGround);
			for (auto alpha = std::uint16_t{ 1U }; alpha <= 255U; ++alpha) {
				const auto candidate = composited(target, aColour, static_cast<std::uint8_t>(alpha));
				if (contrast_ratio_hundredths(candidate, aGround) >= aMinimum) {
					return candidate;
				}
			}
			return target;
		}
	}

	auto readable_selection_ink(
		const appearance_color aSyntax, const appearance_color aFill,
		const appearance_variant aVariant) noexcept -> appearance_color
	{
		return reaching_floor(aSyntax, aFill, minimum_contrast_hundredths(contrast_kind::text, aVariant));
	}

	namespace {
		[[nodiscard]] auto reads_on(const appearance_color aInk, const std::span<const contrast_ground> aGrounds) noexcept -> bool
		{
			return std::ranges::all_of(aGrounds, [aInk](const contrast_ground& aGround) { return contrast_ratio_hundredths(aInk, aGround.Colour) >= aGround.MinimumHundredths; });
		}

		/// The least move of `aFrom` toward `aTarget` that reads on every
		/// ground, if any does.
		[[nodiscard]] auto least_toward(const appearance_color aTarget, const appearance_color aFrom, const std::span<const contrast_ground> aGrounds) noexcept
			-> std::optional<appearance_color>
		{
			for (auto alpha = std::uint16_t{ 0U }; alpha <= 255U; ++alpha) {
				const auto candidate = composited(aTarget, aFrom, static_cast<std::uint8_t>(alpha));
				if (reads_on(candidate, aGrounds)) {
					return candidate;
				}
			}
			return std::nullopt;
		}
	}

	auto readable_muted_ink(const appearance_color aText, const appearance_color aBase, const std::span<const contrast_ground> aGrounds) noexcept -> appearance_color
	{
		const auto mean     = [](const std::uint8_t aOne, const std::uint8_t aOther) { return static_cast<std::uint8_t>((static_cast<std::uint32_t>(aOne) + aOther) / 2U); };
		const auto midpoint = appearance_color{ mean(aText.Red, aBase.Red), mean(aText.Green, aBase.Green), mean(aText.Blue, aBase.Blue), aText.Alpha };
		return least_toward(aText, midpoint, aGrounds).value_or(aText);
	}

	auto readable_signal_ink(const appearance_color aSignal, const std::span<const contrast_ground> aGrounds) noexcept -> appearance_color
	{
		if (aGrounds.empty() || reads_on(aSignal, aGrounds)) {
			return aSignal;
		}
		const auto target    = emphasis_foreground(aGrounds.front().Colour);
		const auto darkening = target.Red == 0U;
		const auto strongest = std::max({ aSignal.Red, aSignal.Green, aSignal.Blue });
		// First stage: darken the weaker channels, or raise the strongest.
		const auto staged = [&](const std::uint32_t aStep) {
			const auto channel = [&](const std::uint8_t aValue) -> std::uint8_t {
				const auto value = static_cast<std::uint32_t>(aValue);
				if (darkening) {
					return aValue == strongest ? aValue : static_cast<std::uint8_t>(((value * (255U - aStep)) + 127U) / 255U);
				}
				return aValue != strongest ? aValue : static_cast<std::uint8_t>(value + ((((255U - value) * aStep) + 127U) / 255U));
			};
			return appearance_color{ channel(aSignal.Red), channel(aSignal.Green), channel(aSignal.Blue), aSignal.Alpha };
		};
		for (auto step = std::uint32_t{ 1U }; step <= 255U; ++step) {
			if (const auto candidate = staged(step); reads_on(candidate, aGrounds)) {
				return candidate;
			}
		}
		// Second stage: toward the extreme.
		return least_toward(target, staged(255U), aGrounds).value_or(target);
	}

	auto visible_focus_indicator(const appearance_color aAccent, const appearance_color aSurface) noexcept -> appearance_color
	{
		return reaching_floor(aAccent, aSurface, indicator_contrast_minimum_hundredths);
	}

	auto emphasis_needs_own_foreground(
		const appearance_palette& aPalette,
		const appearance_role     aFill,
		const appearance_variant  aVariant) noexcept -> bool
	{
		const auto fill    = aPalette[index_of(aFill)];
		const auto minimum = minimum_contrast_hundredths(
			contrast_kind::emphasis_text, aVariant);
		for (auto offset = std::size_t{ 0U }; offset < syntax_role_count; ++offset) {
			const auto role = syntax_roles[offset];
			if (contrast_ratio_hundredths(aPalette[index_of(role)], fill) < minimum) {
				return true;
			}
		}

		return false;
	}

	auto derived_emphasis_fill(
		const appearance_color   aFill,
		const appearance_variant aVariant) noexcept -> appearance_color
	{
		static_cast<void>(aVariant);
		constexpr auto darkGroundLuminance = 0.18;
		constexpr auto steps               = std::uint32_t{ 100U };
		constexpr auto saturationReach     = 0.55;
		constexpr auto lightnessReach      = 0.45;

		const auto source = hsv_of(aFill);
		const auto dark   = relative_luminance(linear_of(aFill)) <= darkGroundLuminance;

		auto strongest = aFill;
		for (auto step = std::uint32_t{ 1U }; step <= steps; ++step) {
			const auto reach   = static_cast<double>(step) / static_cast<double>(steps);
			const auto shifted = hsv_color{
				.Hue        = source.Hue,
				.Saturation = std::min(1.0, source.Saturation + (reach * saturationReach)),
				.Value      = dark
					? std::min(1.0, source.Value + (reach * lightnessReach))
					: std::max(0.0, source.Value - (reach * lightnessReach)),
			};
			const auto candidate = rgb_of(shifted);
			strongest            = candidate;
			if (color_difference_tenths(aFill, candidate, color_vision::typical)
				>= derived_emphasis_separation_minimum_tenths) {
				return candidate;
			}
		}

		return strongest;
	}

	auto hue_critical_pairs() noexcept -> std::span<const hue_critical_pair>
	{
		return std::span<const hue_critical_pair>{ hue_critical_table };
	}

	auto measure_color_vision(
		const appearance_palette& aPalette,
		const hue_critical_pair   aPair) noexcept -> color_vision_finding
	{
		const auto first  = aPalette[index_of(aPair.First)];
		const auto second = aPalette[index_of(aPair.Second)];

		const auto typical
			= color_difference_tenths(first, second, color_vision::typical);
		const auto protanopia
			= color_difference_tenths(first, second, color_vision::protanopia);
		const auto deuteranopia
			= color_difference_tenths(first, second, color_vision::deuteranopia);
		const auto weakest = std::min({ typical, protanopia, deuteranopia });

		return color_vision_finding{
			.Pair                        = aPair,
			.TypicalTenths               = typical,
			.ProtanopiaTenths            = protanopia,
			.DeuteranopiaTenths          = deuteranopia,
			.DistinguishableByColorAlone = weakest
				>= distinguishable_difference_minimum_tenths,
		};
	}

	auto measure_hue_critical_pairs(const appearance_palette& aPalette)
		-> std::vector<color_vision_finding>
	{
		auto findings = std::vector<color_vision_finding>{ };
		findings.reserve(hue_critical_table.size());
		for (const auto pair : hue_critical_table) {
			findings.push_back(measure_color_vision(aPalette, pair));
		}

		return findings;
	}

	auto pairs_requiring_non_color_indicator(const appearance_palette& aPalette)
		-> std::vector<color_vision_finding>
	{
		auto required = std::vector<color_vision_finding>{ };
		for (const auto pair : hue_critical_table) {
			const auto finding = measure_color_vision(aPalette, pair);
			if (!finding.DistinguishableByColorAlone) {
				required.push_back(finding);
			}
		}

		return required;
	}
} // namespace bpr::core
