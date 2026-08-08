#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QPlainTextEdit>
#include <QString>
#include <QStyle>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using role = core::appearance_role;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto close(
		const qreal aLeft,
		const qreal aRight,
		const qreal aTolerance = 0.000'001) noexcept -> bool
	{
		return std::abs(aLeft - aRight) <= aTolerance;
	}

	void process_events()
	{
		for (auto count = 0; count < 4; ++count) {
			QApplication::processEvents();
		}
	}

	// WCAG contrast floors. AA is the ordinary-profile obligation and AAA the
	// obligation a high-contrast profile exists to meet.
	constexpr auto readable_contrast     = 4.5;
	constexpr auto high_contrast_minimum = 7.0;
	constexpr auto maximum_contrast      = 21.0;

	constexpr auto all_variants = std::array{
		core::appearance_variant::light,
		core::appearance_variant::dark,
		core::appearance_variant::high_contrast_light,
		core::appearance_variant::high_contrast_dark,
	};

	[[nodiscard]] auto explicit_mode_for(
		const core::appearance_variant aVariant) noexcept -> core::appearance_mode
	{
		switch (aVariant) {
		case core::appearance_variant::light:
			break;
		case core::appearance_variant::dark:
			return core::appearance_mode::dark;
		case core::appearance_variant::high_contrast_light:
			return core::appearance_mode::high_contrast_light;
		case core::appearance_variant::high_contrast_dark:
			return core::appearance_mode::high_contrast_dark;
		}
		return core::appearance_mode::light;
	}

	/// \brief Holds the readability facts one profile palette reports.
	struct profile_contrast
	{
		double MinimumSyntax = maximum_contrast;
		double NormalText    = 0.0;
		double ChromeText    = 0.0;
	};

	[[nodiscard]] auto measure(const core::appearance_palette& aPalette)
		-> profile_contrast
	{
		auto       measurement = profile_contrast{ };
		const auto background  = aPalette[core::index_of(role::code_background)];
		for (auto index = core::index_of(role::syntax_normal);
			 index <= core::index_of(role::syntax_other);
			 ++index) {
			measurement.MinimumSyntax = std::min(
				measurement.MinimumSyntax,
				qt::contrast_ratio(background, aPalette[index]));
		}
		measurement.NormalText = qt::contrast_ratio(
			background,
			aPalette[core::index_of(role::syntax_normal)]);
		measurement.ChromeText = qt::contrast_ratio(
			aPalette[core::index_of(role::application_surface)],
			aPalette[core::index_of(role::application_text)]);
		return measurement;
	}

	[[nodiscard]] auto opaque_roles(const core::appearance_palette& aPalette)
		-> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
			aPalette,
			[](const core::appearance_color& aColor) {
				return aColor.Alpha == 255U;
			}));
	}

	[[nodiscard]] auto differing_roles(
		const core::appearance_palette& aFirst,
		const core::appearance_palette& aSecond) -> std::size_t
	{
		auto differing = std::size_t{ 0U };
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			if (aFirst[index] != aSecond[index]) {
				++differing;
			}
		}
		return differing;
	}

	auto test_fresh_installation_defaults_to_system() -> void
	{
		const auto composed = qt::default_appearance_settings();
		expect(
			composed.Mode == core::appearance_mode::system,
			"a fresh installation with no persisted value selects System");
		expect(
			application::appearance_settings{ }.Mode
				== core::appearance_mode::system,
			"the durable appearance record itself defaults to System");

		const auto profile = qt::appearance_profile{ };
		expect(
			profile.mode() == core::appearance_mode::system,
			"a profile composed with no persisted value selects System");
		expect(
			profile.settings().Mode == core::appearance_mode::system,
			"the profile publishes System back into the durable record");

		const auto resolution = profile.resolution();
		expect(
			resolution.Variant == core::appearance_variant::light,
			"System with no observed scheme resolves to the light variant");
		expect(
			resolution.Scheme == core::appearance_scheme_source::unobserved_default,
			"the light family came from the absent observation, not an explicit mode");
		expect(
			resolution.Contrast == core::appearance_contrast_source::none,
			"no contrast source applies without an observation");
		expect(
			!resolution.HighContrastRecommended,
			"nothing recommends high contrast without an observation");

		expect(
			profile.customised_roles() == 0U,
			"a fresh installation customises no semantic colour");
		expect(
			profile.palette()
				== qt::builtin_appearance_palette(core::appearance_variant::light),
			"a fresh installation starts from the built-in profile of its variant");
		expect(
			profile.profile_name() == QStringLiteral("BetterPullReview Light"),
			"the fresh installation names its built-in profile");

		auto hostile       = qt::default_appearance_settings();
		hostile.Mode       = static_cast<core::appearance_mode>(200U);
		const auto guarded = qt::appearance_profile{ hostile };
		expect(
			guarded.mode() == core::appearance_mode::system,
			"a persisted mode outside the closed vocabulary falls back to System");
	}

	auto test_builtin_profiles_are_total() -> void
	{
		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			expect(
				opaque_roles(palette) == core::appearance_role_count,
				"every role of the closed vocabulary carries an opaque colour");

			expect(
				palette[core::index_of(role::diff_added_background)]
					!= palette[core::index_of(role::diff_removed_background)],
				"an added row is not painted like a removed one");
			expect(
				palette[core::index_of(role::code_background)]
					!= palette[core::index_of(role::syntax_normal)],
				"code text is not painted like the canvas behind it");

			constexpr auto states = std::array{
				role::state_unread,
				role::state_accepted,
				role::state_defect,
				role::state_fixed,
				role::state_verified,
				role::state_stale,
			};
			for (auto first = std::size_t{ 0U }; first < states.size(); ++first) {
				for (auto second = first + 1U; second < states.size(); ++second) {
					expect(
						palette[core::index_of(states[first])]
							!= palette[core::index_of(states[second])],
						"every review state carries its own colour");
				}
			}
		}
	}

	auto test_high_contrast_profiles_are_not_copies() -> void
	{
		constexpr auto pairs = std::array{
			std::pair{
				core::appearance_variant::light,
				core::appearance_variant::high_contrast_light },
			std::pair{
				core::appearance_variant::dark,
				core::appearance_variant::high_contrast_dark },
		};

		for (const auto& [ordinary, elevated] : pairs) {
			const auto& base = qt::builtin_appearance_palette(ordinary);
			const auto& high = qt::builtin_appearance_palette(elevated);
			expect(
				base != high,
				"a high-contrast profile is not a copy of its counterpart");
			expect(
				differing_roles(base, high) > (core::appearance_role_count / 2U),
				"a high-contrast profile differs in most roles, not a token few");

			const auto ordinaryContrast = measure(base);
			const auto elevatedContrast = measure(high);
			std::cout << "profile contrast: "
					  << qt::builtin_appearance_profile_name(ordinary).toStdString()
					  << " syntax>=" << ordinaryContrast.MinimumSyntax
					  << " text=" << ordinaryContrast.NormalText
					  << " chrome=" << ordinaryContrast.ChromeText << " | "
					  << qt::builtin_appearance_profile_name(elevated).toStdString()
					  << " syntax>=" << elevatedContrast.MinimumSyntax
					  << " text=" << elevatedContrast.NormalText
					  << " chrome=" << elevatedContrast.ChromeText << '\n';

			expect(
				ordinaryContrast.MinimumSyntax >= readable_contrast,
				"every ordinary syntax colour is readable on its code canvas");
			expect(
				elevatedContrast.MinimumSyntax >= high_contrast_minimum,
				"every high-contrast syntax colour clears the elevated floor");
			expect(
				elevatedContrast.MinimumSyntax > ordinaryContrast.MinimumSyntax,
				"a high-contrast profile raises the weakest syntax contrast");
			expect(
				ordinaryContrast.ChromeText >= readable_contrast,
				"ordinary chrome text is readable on its own surface");
			expect(
				elevatedContrast.ChromeText >= high_contrast_minimum,
				"high-contrast chrome text clears the elevated floor");
			expect(
				elevatedContrast.ChromeText > ordinaryContrast.ChromeText,
				"a high-contrast profile raises the chrome text contrast");
		}
	}

	auto test_derived_text_stays_readable_on_every_fill() -> void
	{
		constexpr auto fills = std::array{
			role::diff_added_background,
			role::diff_removed_background,
			role::diff_intraline_added_background,
			role::diff_intraline_removed_background,
			role::diff_moved_background,
			role::code_selection_background,
			role::code_current_line_background,
		};

		for (const auto variant : all_variants) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(explicit_mode_for(variant));
			expect(
				profile.variant() == variant,
				"an explicit mode reaches its own profile variant");
			for (const auto fill : fills) {
				const auto derived
					= qt::to_appearance_color(profile.readable_text(fill));
				expect(
					qt::contrast_ratio(
						profile.palette()[core::index_of(fill)],
						derived)
						>= readable_contrast,
					"derived text stays readable on every semantic fill");
			}
			expect(
				qt::contrast_ratio(
					profile.palette()[core::index_of(
						role::diff_hunk_header_background)],
					profile.palette()[core::index_of(role::diff_hunk_header_text)])
					>= readable_contrast,
				"the hunk header pair the vocabulary names is readable");
		}
	}

	auto test_the_core_decides_which_variant_applies() -> void
	{
		constexpr auto modes = std::array{
			core::appearance_mode::system,
			core::appearance_mode::light,
			core::appearance_mode::dark,
			core::appearance_mode::high_contrast_system,
			core::appearance_mode::high_contrast_light,
			core::appearance_mode::high_contrast_dark,
		};
		constexpr auto observations = std::array{
			core::appearance_observation{ },
			core::appearance_observation{
				core::os_color_scheme::light,
				core::os_contrast_preference::standard },
			core::appearance_observation{
				core::os_color_scheme::dark,
				core::os_contrast_preference::standard },
			core::appearance_observation{
				core::os_color_scheme::dark,
				core::os_contrast_preference::high },
			core::appearance_observation{
				core::os_color_scheme::unknown,
				core::os_contrast_preference::high },
		};

		for (const auto mode : modes) {
			for (const auto observation : observations) {
				auto profile = qt::appearance_profile{ };
				profile.select_mode(mode);
				profile.observe(observation);
				expect(
					profile.resolution()
						== core::resolve_appearance(mode, observation),
					"presentation applies exactly the resolution the core decided");
				expect(
					profile.palette()
						== qt::builtin_appearance_palette(profile.variant()),
					"an uncustomised profile is the built-in palette of its variant");
				expect(
					!profile.profile_name().isEmpty(),
					"every resolved variant names a built-in profile");
			}
		}

		auto chosen = qt::appearance_profile{ };
		chosen.select_mode(core::appearance_mode::light);
		const auto chosenPalette = chosen.palette();
		expect(
			!chosen.observe(core::appearance_observation{
				core::os_color_scheme::dark,
				core::os_contrast_preference::standard }),
			"an explicit mode is unaffected by a supplied scheme observation");
		expect(
			chosen.palette() == chosenPalette,
			"an explicit mode keeps its palette across a supplied observation");

		auto following = qt::appearance_profile{ };
		expect(
			following.observe(core::appearance_observation{
				core::os_color_scheme::dark,
				core::os_contrast_preference::standard }),
			"System mode consumes the supplied scheme observation");
		expect(
			following.variant() == core::appearance_variant::dark,
			"a supplied dark observation reaches the dark profile under System");
		expect(
			following.resolution().Scheme
				== core::appearance_scheme_source::observed_scheme,
			"the resolved family reports the observation as its source");
	}

	auto test_applies_semantic_colors_without_forcing_style_or_stylesheet() -> void
	{
		const auto  inheritedApplication = QApplication::palette();
		auto* const inheritedStyle       = QApplication::style();
		expect(inheritedStyle != nullptr, "the application inherits a style");

		QWidget canvas;
		canvas.setObjectName(QStringLiteral("bpr-appearance-canvas"));
		// A role the profile has no opinion about. Button used to serve here,
		// and it stopped being a fair example when BPR-0207 showed that
		// leaving Qt's chrome family inherited renders a white scrollbar on a
		// dark theme. Link is still genuinely unowned: nothing in the diff
		// surface draws one.
		const auto inheritedLink
			= canvas.palette().color(QPalette::Active, QPalette::Link);

		auto profile = qt::appearance_profile{ };
		expect(
			profile.select_mode(core::appearance_mode::dark),
			"selecting Dark changes the applied presentation");
		profile.apply_to(canvas);

		const auto applied = canvas.palette();
		expect(
			applied.color(QPalette::Active, QPalette::Base)
				== profile.color(role::code_background),
			"the code canvas fill carries the profile's code background");
		expect(
			applied.color(QPalette::Active, QPalette::Text)
				== profile.color(role::syntax_normal),
			"canvas text carries the profile's normal syntax colour");
		expect(
			applied.color(QPalette::Active, QPalette::Window)
				== profile.color(role::application_surface),
			"chrome carries the profile's application surface");
		expect(
			applied.color(QPalette::Active, QPalette::WindowText)
				== profile.color(role::application_text),
			"chrome text carries the profile's application text");
		expect(
			applied.color(QPalette::Active, QPalette::AlternateBase)
				== profile.color(role::code_current_line_background),
			"the alternate fill carries the profile's current line background");
		expect(
			applied.color(QPalette::Active, QPalette::Highlight)
				== profile.color(role::code_selection_background),
			"selection carries the profile's selection background");
		expect(
			applied.color(QPalette::Active, QPalette::HighlightedText)
				== profile.readable_text(role::code_selection_background),
			"selected text carries the derived readable colour");
		expect(
			applied.color(QPalette::Inactive, QPalette::Base)
				== profile.color(role::code_background),
			"an inactive surface presents the same semantic fill");
		expect(
			applied.color(QPalette::Disabled, QPalette::Text)
				!= profile.color(role::syntax_normal),
			"disabled text stays distinguishable from enabled text");

		expect(
			applied.color(QPalette::Active, QPalette::Link) == inheritedLink,
			"a role the profile does not own stays exactly as inherited");

		// And the chrome family, which it now does own. Qt's styles draw
		// scrollbars, frames and separators from these and do not recompute
		// the shades when Button alone is set, so leaving any of them
		// inherited puts a light control on a dark surface.
		const auto surface = profile.color(role::application_surface);
		expect(
			applied.color(QPalette::Active, QPalette::Button) == surface,
			"the chrome fill carries the profile's application surface");
		expect(
			applied.color(QPalette::Active, QPalette::ToolTipBase) == surface,
			"tooltips carry the profile's application surface");
		const auto light    = applied.color(QPalette::Active, QPalette::Light);
		const auto midlight = applied.color(QPalette::Active, QPalette::Midlight);
		const auto mid      = applied.color(QPalette::Active, QPalette::Mid);
		const auto dark     = applied.color(QPalette::Active, QPalette::Dark);
		expect(
			light.lightness() > midlight.lightness()
				&& midlight.lightness() > mid.lightness()
				&& mid.lightness() > dark.lightness(),
			"the shade family stays ordered light to dark, which is what a "
			"style assumes when it picks one of them for an edge");
		expect(
			mid.lightness() < QColor{ Qt::white }.lightness() / 2,
			"the shades follow a dark surface rather than an inherited light "
			"one, which is the defect that put a white scrollbar on the dark "
			"comparison panes");
		expect(
			QApplication::palette() == inheritedApplication,
			"applying a profile writes no application palette");
		expect(
			QApplication::style() == inheritedStyle,
			"applying a profile installs no style");
		expect(
			QApplication::instance() != nullptr
				&& qApp->styleSheet().isEmpty(),
			"the application carries no style sheet");
		expect(
			canvas.styleSheet().isEmpty(),
			"the presented surface carries no style sheet");
		expect(
			profile.statistics().AppliedSurfaces == 1U,
			"exactly one surface adopted the derived palette");
	}

	auto test_immediate_preview_updates_without_restart() -> void
	{
		QWidget canvas;
		auto    profile       = qt::appearance_profile{ };
		auto    notifications = std::size_t{ 0U };
		profile.set_sink([&notifications] { ++notifications; });

		profile.apply_to(canvas);
		const auto lightBase
			= canvas.palette().color(QPalette::Active, QPalette::Base);
		expect(
			lightBase == profile.color(role::code_background),
			"the live surface starts on the resolved profile");

		const auto before = profile.generation();
		expect(
			profile.select_mode(core::appearance_mode::dark),
			"selecting a different mode changes the applied presentation");
		expect(
			notifications == 1U,
			"the mode change publishes exactly one presentation change");
		expect(
			profile.generation() == before + 1U,
			"the published change advances the presentation generation");

		profile.apply_to(canvas);
		const auto darkBase
			= canvas.palette().color(QPalette::Active, QPalette::Base);
		expect(
			darkBase != lightBase,
			"the same live surface presents the newly selected profile");
		expect(
			darkBase == profile.color(role::code_background),
			"the live surface shows the new profile's code background");

		expect(
			!profile.select_mode(core::appearance_mode::dark),
			"re-selecting the current mode changes nothing");
		expect(notifications == 1U, "a repeated selection publishes nothing");

		constexpr auto customBackground
			= core::appearance_color{ 12U, 34U, 56U, 255U };
		expect(
			profile.customise(role::code_background, customBackground),
			"customising a semantic colour changes the applied presentation");
		expect(notifications == 2U, "the customisation publishes exactly once");
		expect(
			profile.customised(role::code_background)
				&& profile.customised_roles() == 1U,
			"exactly the customised role is marked customised");
		profile.apply_to(canvas);
		expect(
			canvas.palette().color(QPalette::Active, QPalette::Base)
				== QColor{ 12, 34, 56 },
			"the live surface shows the customised colour immediately");
		expect(
			profile.settings().Palette[core::index_of(role::code_background)]
				== customBackground,
			"the durable record carries the customised colour");

		expect(
			!profile.customise(role::code_background, customBackground),
			"repeating the same customisation changes nothing");
		expect(
			profile.restore(role::code_background),
			"restoring one colour changes the applied presentation");
		expect(
			profile.customised_roles() == 0U,
			"restoring one colour clears its customisation");
		profile.apply_to(canvas);
		expect(
			canvas.palette().color(QPalette::Active, QPalette::Base) == darkBase,
			"the live surface returns to the built-in profile immediately");

		const auto settled          = profile.palette();
		const auto beforeNative     = profile.generation();
		const auto beforeStatistics = profile.statistics();
		profile.refresh_native_appearance();
		expect(
			profile.palette() == settled,
			"a native palette, font, style, or theme change alters no semantic colour");
		expect(
			profile.generation() == beforeNative + 1U,
			"a native change republishes so every surface derives again");
		expect(
			profile.statistics().NativeRefreshes
					== beforeStatistics.NativeRefreshes + 1U
				&& profile.statistics().PresentationChanges
					== beforeStatistics.PresentationChanges,
			"a native change is counted apart from an appearance change");
	}

	auto test_customisation_round_trips_through_the_durable_record() -> void
	{
		auto profile = qt::appearance_profile{ };
		profile.select_mode(core::appearance_mode::dark);
		constexpr auto custom = core::appearance_color{ 7U, 8U, 9U, 255U };
		expect(
			profile.customise(role::state_defect, custom),
			"customising a review state colour changes presentation");

		const auto stored   = profile.settings();
		const auto restored = qt::appearance_profile{ stored };
		expect(
			restored.mode() == core::appearance_mode::dark,
			"the durable record carries the selected mode");
		expect(
			restored.palette() == profile.palette(),
			"the durable record reproduces the applied palette exactly");
		expect(
			restored.customised(role::state_defect)
				&& restored.customised_roles() == 1U,
			"the durable record reproduces exactly the customised role");
	}

	// Typography fixtures. Every offered value is placed against the core's own
	// published bounds, so a test can never accidentally offer a legal value it
	// believed was illegal or the reverse.
	[[nodiscard]] auto legal_font(const std::string_view aFamily)
		-> core::font_settings
	{
		return core::font_settings{
			.Family               = std::string{ aFamily },
			.PointSizeCentipoints = core::bounds_of(
										core::typography_field::point_size)
										.Minimum
				+ 600U,
			.Weight = 400U,
		};
	}

	// The UI font is the only one with a line height. BPR-0118 retired the code
	// font's, so there is no code-font counterpart to this and cannot be one.
	[[nodiscard]] auto legal_ui_font(const std::string_view aFamily)
		-> core::ui_font_settings
	{
		return core::ui_font_settings{
			.Font              = legal_font(aFamily),
			.LineHeightPercent = 100U,
		};
	}

	auto test_typography_defaults_are_legal_and_native() -> void
	{
		const auto defaults = qt::default_typography_settings();
		expect(
			core::validate_typography(defaults).empty(),
			"the composed fresh-install typography is legal by the core's own validator");
		expect(
			!defaults.Ui.Font.Family.empty() && !defaults.Code.Font.Family.empty(),
			"the composition supplies both families the core cannot know");
		expect(
			qt::default_appearance_settings().Typography == defaults,
			"a fresh installation starts from exactly that typography");

		const auto profile = qt::appearance_profile{ };
		expect(
			profile.typography() == defaults,
			"a profile composed with no persisted value applies the fresh typography");
		expect(
			profile.settings().Typography == defaults,
			"the profile publishes its typography back into the durable record");

		// A durable record can carry a font this build rejects. Only the
		// rejected font is replaced, and what replaces it must itself be legal.
		auto hostile                        = qt::default_appearance_settings();
		hostile.Typography.Ui               = legal_ui_font("Courier");
		hostile.Typography.Code.Font.Family = std::string{ };
		hostile.Typography.Code.TabWidth    = core::bounds_of(
												  core::typography_field::tab_width)
												  .Maximum
			+ 1U;
		const auto guarded = qt::appearance_profile{ hostile };
		expect(
			core::validate_typography(guarded.typography()).empty(),
			"a rejected durable typography is replaced by a legal record");
		expect(
			guarded.typography().Ui == legal_ui_font("Courier"),
			"an illegal code font does not discard the legal UI font beside it");
		expect(
			guarded.typography().Code == defaults.Code,
			"exactly the rejected code font falls back to the composed default");
	}

	auto test_fonts_are_built_from_the_core_record() -> void
	{
		const auto bounds           = core::bounds_of(core::typography_field::point_size);
		auto       record           = legal_font("Courier");
		record.PointSizeCentipoints = 1'250U;
		record.Weight               = 700U;

		const auto font = qt::to_presentation_font(record);
		expect(
			font.family() == QStringLiteral("Courier"),
			"the built font carries exactly the record's family");
		expect(
			close(font.pointSizeF(), 12.5),
			"hundredths of a point reach Qt as an exact fractional point size");
		expect(
			font.weight() == QFont::Bold,
			"the record's weight axis reaches Qt unchanged");

		auto smallest                 = record;
		smallest.PointSizeCentipoints = bounds.Minimum;
		auto largest                  = record;
		largest.PointSizeCentipoints  = bounds.Maximum;
		expect(
			close(
				qt::to_presentation_font(smallest).pointSizeF(),
				static_cast<qreal>(bounds.Minimum) / 100.0)
				&& close(
					qt::to_presentation_font(largest).pointSizeF(),
					static_cast<qreal>(bounds.Maximum) / 100.0),
			"both published bounds are buildable point sizes, so presentation offers no narrower range");

		auto code        = core::code_font_settings{ .Font = record };
		code.Ligatures   = false;
		const auto plain = qt::to_presentation_font(code);
		expect(
			plain.isFeatureSet(QFont::Tag{ "liga" })
				&& plain.featureValue(QFont::Tag{ "liga" }) == 0U
				&& plain.featureValue(QFont::Tag{ "clig" }) == 0U,
			"turning ligatures off disables both standard and contextual ligatures");
		code.Ligatures     = true;
		const auto ligated = qt::to_presentation_font(code);
		expect(
			ligated.featureValue(QFont::Tag{ "liga" }) == 1U
				&& ligated.featureValue(QFont::Tag{ "clig" }) == 1U,
			"turning ligatures on enables both standard and contextual ligatures");

		const auto single         = core::ui_font_settings{ .Font              = record,
															.LineHeightPercent = 100U };
		auto       doubled        = single;
		doubled.LineHeightPercent = 200U;
		expect(
			close(qt::line_height_of(doubled), 2.0 * qt::line_height_of(single))
				&& qt::line_height_of(single) > 0.0,
			"line height scales the font's own line spacing by the record's percentage");
		expect(
			qt::to_presentation_font(doubled.Font).pointSizeF()
				== qt::to_presentation_font(single.Font).pointSizeF(),
			"line height is a layout fact and does not resize the glyphs");

		auto wide       = code;
		wide.TabWidth   = 8U;
		auto narrow     = code;
		narrow.TabWidth = 2U;
		expect(
			qt::tab_stop_distance_of(narrow) > 0.0
				&& close(
					qt::tab_stop_distance_of(wide),
					4.0 * qt::tab_stop_distance_of(narrow)),
			"the tab stop is the code font's space advance times the record's tab width");
	}

	auto test_every_typography_field_previews_immediately() -> void
	{
		auto profile       = qt::appearance_profile{ };
		auto notifications = std::size_t{ 0U };
		profile.set_sink([&notifications] { ++notifications; });

		QWidget        chrome;
		QPlainTextEdit canvas;
		profile.apply_ui_font_to(chrome);
		profile.apply_code_font_to(canvas);
		expect(
			chrome.font().family() == profile.ui_font().family()
				&& canvas.font().family() == profile.code_font().family(),
			"the live surfaces start on the profile's own UI and code families");

		const auto uiBefore  = profile.ui_font();
		auto       chosenUi  = profile.typography().Ui;
		chosenUi.Font.Family = "Courier";
		chosenUi.Font.PointSizeCentipoints += 250U;
		chosenUi.Font.Weight       = 700U;
		chosenUi.LineHeightPercent = 140U;
		const auto uiSelection     = profile.select_ui_font(chosenUi);
		expect(
			uiSelection.accepted() && uiSelection.Changed && notifications == 1U,
			"a legal UI font is accepted and published exactly once");
		expect(
			profile.ui_font().family() == QStringLiteral("Courier")
				&& profile.ui_font().pointSizeF() > uiBefore.pointSizeF()
				&& profile.ui_font().weight() == QFont::Bold
				&& close(
					profile.ui_line_height(),
					qt::line_height_of(chosenUi)),
			"family, size, weight, and line height all reach the applied UI typography");

		profile.apply_ui_font_to(chrome);
		expect(
			chrome.font().family() == QStringLiteral("Courier")
				&& chrome.font().weight() == QFont::Bold,
			"the same live surface shows the new UI font without being replaced");
		expect(
			!profile.select_ui_font(chosenUi).Changed && notifications == 1U,
			"re-offering the applied UI font publishes nothing");

		auto chosenCode        = profile.typography().Code;
		chosenCode.Font.Family = "Courier";
		chosenCode.Font.PointSizeCentipoints += 300U;
		chosenCode.TabWidth      = 8U;
		chosenCode.Ligatures     = true;
		chosenCode.SyntaxBold    = false;
		chosenCode.SyntaxItalic  = true;
		const auto codeSelection = profile.select_code_font(chosenCode);
		expect(
			codeSelection.accepted() && codeSelection.Changed
				&& notifications == 2U,
			"a legal code font is accepted and published exactly once");
		expect(
			profile.code_font().featureValue(QFont::Tag{ "liga" }) == 1U
				&& close(
					profile.code_tab_stop_distance(),
					qt::tab_stop_distance_of(chosenCode)),
			"ligatures and tab width reach the applied code typography");
		expect(
			!profile.code_font_for(true, true).bold()
				&& profile.code_font_for(true, true).italic()
				&& !profile.code_font_for(false, false).italic(),
			"a disabled syntax emphasis switch suppresses it and an enabled one does not");

		profile.apply_code_font_to(canvas);
		const auto wideTabStop = canvas.tabStopDistance();
		expect(
			canvas.font().family() == QStringLiteral("Courier")
				&& wideTabStop > 0.0
				&& close(wideTabStop, profile.code_tab_stop_distance()),
			"the same live code surface shows the new font and tab stop without being replaced");

		auto narrower     = chosenCode;
		narrower.TabWidth = 2U;
		expect(
			profile.select_code_font(narrower).Changed,
			"tab width alone is a published typography change");
		profile.apply_code_font_to(canvas);
		expect(
			close(canvas.tabStopDistance(), wideTabStop / 4.0),
			"a tab width change previews on the live surface immediately");

		expect(
			profile.statistics().TypographyChanges == 3U
				&& profile.statistics().RejectedTypography == 0U
				&& profile.generation() == 3U,
			"exactly the three accepted typography changes were published");

		const auto restored = qt::appearance_profile{ profile.settings() };
		expect(
			restored.typography() == profile.typography(),
			"the durable record reproduces every applied typography value exactly");
		expect(
			restored.code_font().featureValue(QFont::Tag{ "liga" })
					== profile.code_font().featureValue(QFont::Tag{ "liga" })
				&& close(
					restored.code_tab_stop_distance(),
					profile.code_tab_stop_distance()),
			"a record loaded from persistence rebuilds the same applied fonts");
	}

	auto test_a_value_outside_the_published_bounds_is_never_applied() -> void
	{
		const auto sizeBounds = core::bounds_of(core::typography_field::point_size);
		auto       profile    = qt::appearance_profile{ };
		auto       published  = std::size_t{ 0U };
		profile.set_sink([&published] { ++published; });

		auto atMinimum                      = profile.typography().Ui;
		atMinimum.Font.PointSizeCentipoints = sizeBounds.Minimum;
		expect(
			profile.select_ui_font(atMinimum).accepted(),
			"the published minimum point size is applied, so the bound is inclusive");
		auto atMaximum                      = profile.typography().Ui;
		atMaximum.Font.PointSizeCentipoints = sizeBounds.Maximum;
		expect(
			profile.select_ui_font(atMaximum).accepted(),
			"the published maximum point size is applied, so the bound is inclusive");

		const auto applied     = profile.typography();
		const auto appliedUi   = profile.ui_font();
		const auto generation  = profile.generation();
		const auto beforeCount = published;

		auto tooLarge                      = applied.Ui;
		tooLarge.Font.PointSizeCentipoints = sizeBounds.Maximum + 1U;
		const auto oversized               = profile.select_ui_font(tooLarge);
		expect(
			!oversized.accepted() && !oversized.Changed
				&& oversized.Rejections.size() == 1U
				&& oversized.Rejections.front()
					== core::typography_rejection{
						.Target = core::typography_target::ui_font,
						.Field  = core::typography_field::point_size,
						.Reason = core::typography_reason::above_maximum,
					},
			"a point size one centipoint above the published maximum is rejected by field and reason");
		expect(
			profile.typography() == applied
				&& profile.ui_font().pointSizeF() == appliedUi.pointSizeF(),
			"a rejected point size never reaches the applied typography or its font");

		auto tooSmall                      = applied.Ui;
		tooSmall.Font.PointSizeCentipoints = sizeBounds.Minimum - 1U;
		const auto undersized              = profile.select_ui_font(tooSmall);
		expect(
			!undersized.accepted()
				&& undersized.Rejections.front().Reason
					== core::typography_reason::below_minimum,
			"a point size one centipoint below the published minimum is rejected");
		expect(
			profile.typography() == applied
				&& profile.ui_font().pointSizeF() == appliedUi.pointSizeF(),
			"a rejected small point size is not clamped into range and applied");

		auto illegalCode                      = applied.Code;
		illegalCode.Font.PointSizeCentipoints = sizeBounds.Maximum + 1U;
		illegalCode.TabWidth
			= core::bounds_of(core::typography_field::tab_width).Maximum + 1U;
		const auto codeRejection = profile.select_code_font(illegalCode);
		expect(
			!codeRejection.accepted() && codeRejection.Rejections.size() == 2U,
			"every illegal code value is reported, not only the first");
		expect(
			profile.typography() == applied
				&& close(
					profile.code_tab_stop_distance(),
					qt::tab_stop_distance_of(applied.Code)),
			"one illegal field rejects the whole offer rather than applying the legal half");

		auto longFamily        = applied.Ui;
		longFamily.Font.Family = std::string(
			core::bounds_of(core::typography_field::family).Maximum + 1U,
			'F');
		expect(
			!profile.select_ui_font(longFamily).accepted()
				&& profile.typography() == applied,
			"a family longer than the published bound is rejected and never applied");

		expect(
			published == beforeCount && profile.generation() == generation,
			"a rejected offer publishes no presentation change at all");
		expect(
			profile.statistics().RejectedTypography == 4U,
			"every rejected offer is counted apart from an applied one");
	}

	auto test_every_role_converts_through_presentation_colour() -> void
	{
		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			for (auto index = std::size_t{ 0U };
				 index < core::appearance_role_count;
				 ++index) {
				const auto original = palette[index];
				const auto shown    = qt::to_presentation_color(original);
				expect(shown.isValid(), "every role converts to a valid colour");
				expect(
					qt::to_appearance_color(shown) == original,
					"a role's colour survives the Qt conversion unchanged");
			}
		}
	}

	// A composed comparison fixture. The appearance profile owns no semantics,
	// so the only way to prove a font change moves geometry without moving
	// semantics is to drive a real comparison through its public surfaces and
	// read both back from outside.
	template <typename TIdentity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return TIdentity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "the fixture options normalise");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<core::file_identity>(31U),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto comparison_documents() -> std::pair<std::string, std::string>
	{
		auto       left   = std::string{ };
		auto       right  = std::string{ };
		const auto common = [&left, &right](
								const std::string_view aTag,
								const std::size_t      aCount) {
			for (auto line = std::size_t{ 0U }; line < aCount; ++line) {
				const auto text = std::string{ aTag } + std::to_string(line) + "\n";
				left += text;
				right += text;
			}
		};
		common("head-", 20U);
		left += "old-a\nold-b\nold-c\n";
		right += "new-a\nnew-b\nnew-c\nnew-d\n";
		common("middle-", 16U);
		left += "removed-a\nremoved-b\n";
		common("later-", 16U);
		right += "added-a\nadded-b\n";
		common("tail-", 20U);
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto computed_comparison() -> core::computed_file_diff
	{
		const auto [leftText, rightText] = comparison_documents();
		const auto options               = core::diff_options{ };
		auto       left                  = std::vector<std::uint8_t>{ leftText.begin(), leftText.end() };
		auto       right                 = std::vector<std::uint8_t>{ rightText.begin(), rightText.end() };
		const auto outcome               = core::compute_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(
			outcome.Reason == core::diff_reason_code::none
				&& outcome.Result.has_value(),
			"the fixture comparison computes and publishes");
		return *outcome.Result;
	}

	void append_code(std::vector<std::uint8_t>& aBytes, const std::uint8_t aCode)
	{
		aBytes.push_back(aCode);
	}

	void append_number(std::vector<std::uint8_t>& aBytes, const std::uint32_t aValue)
	{
		for (auto shift = 0U; shift < 32U; shift += 8U) {
			aBytes.push_back(static_cast<std::uint8_t>((aValue >> shift) & 0xFFU));
		}
	}

	template <typename TIdentity>
	void append_identity(
		std::vector<std::uint8_t>& aBytes,
		const TIdentity&           aIdentity)
	{
		aBytes.insert(
			aBytes.end(),
			aIdentity.bytes().begin(),
			aIdentity.bytes().end());
	}

	template <typename TIdentity>
	void append_optional_identity(
		std::vector<std::uint8_t>&      aBytes,
		const std::optional<TIdentity>& aIdentity)
	{
		append_code(aBytes, aIdentity.has_value() ? 1U : 0U);
		if (aIdentity.has_value()) {
			append_identity(aBytes, *aIdentity);
		}
	}

	void append_line(
		std::vector<std::uint8_t>&   aBytes,
		const core::source_line_ref& aLine)
	{
		append_identity(aBytes, aLine.Comparison);
		append_code(aBytes, static_cast<std::uint8_t>(aLine.Side));
		append_number(aBytes, aLine.Line);
	}

	/// \brief Serialises the exact semantic content of one line mapping.
	///
	/// \details Every identity contributes its own 32 bytes and every category
	/// its code, so comparing two of these compares the mapping and the pairing
	/// confidence byte for byte rather than trusting a field-by-field equality
	/// operator to cover the record.
	[[nodiscard]] auto mapping_bytes(const core::line_mapping& aMapping)
		-> std::vector<std::uint8_t>
	{
		auto bytes = std::vector<std::uint8_t>{ };
		append_code(bytes, static_cast<std::uint8_t>(aMapping.Kind));
		append_line(bytes, aMapping.Source);
		append_code(bytes, aMapping.Counterpart.has_value() ? 1U : 0U);
		if (aMapping.Counterpart.has_value()) {
			append_line(bytes, *aMapping.Counterpart);
		}
		append_code(bytes, aMapping.OpposingSpan.has_value() ? 1U : 0U);
		if (aMapping.OpposingSpan.has_value()) {
			append_identity(bytes, aMapping.OpposingSpan->Comparison);
			append_code(
				bytes,
				static_cast<std::uint8_t>(aMapping.OpposingSpan->Side));
			append_number(bytes, aMapping.OpposingSpan->Lines.Begin);
			append_number(bytes, aMapping.OpposingSpan->Lines.End);
		}
		append_code(bytes, aMapping.OpposingBoundary.has_value() ? 1U : 0U);
		if (aMapping.OpposingBoundary.has_value()) {
			append_identity(bytes, aMapping.OpposingBoundary->Comparison);
			append_code(
				bytes,
				static_cast<std::uint8_t>(aMapping.OpposingBoundary->Side));
			append_number(bytes, aMapping.OpposingBoundary->Boundary);
		}
		append_optional_identity(bytes, aMapping.Segment);
		append_optional_identity(bytes, aMapping.Group);
		append_optional_identity(bytes, aMapping.Pair);
		append_code(bytes, static_cast<std::uint8_t>(aMapping.Reason));
		append_code(bytes, static_cast<std::uint8_t>(aMapping.Diagnostic));
		return bytes;
	}

	/// \brief Returns the painted extent of one ribbon, or nothing when hidden.
	[[nodiscard]] auto ribbon_extent(
		const qt::diff_link_map&          aMap,
		const qt::diff_link_map_identity& aIdentity) -> std::optional<qreal>
	{
		const auto links = aMap.visible_links();
		const auto found = std::ranges::find(
			links,
			aIdentity,
			[](const qt::visible_diff_link& aLink) { return aLink.Identity; });
		return found == links.end()
			? std::nullopt
			: std::optional{ (found->LeftBottom - found->LeftTop)
							 + (found->RightBottom - found->RightTop) };
	}

	// Tab width is not a font property, and Qt sends no event when a widget is
	// given a font it already has, so a tab-width-only change reached the
	// workbench through nothing at all: ribbons and the measured axis stayed
	// stale against text that had visibly moved. The typography publication is
	// what closes that, and this is the case that proves it — every font
	// property is held constant so a rebuild cannot be coming from the font.
	auto test_a_tab_width_change_alone_invalidates_measured_geometry() -> void
	{
		const auto diff = computed_comparison();

		auto profile   = qt::appearance_profile{ };
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'100, 760);
		workbench.show();
		process_events();

		const auto pending = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = diff.Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(pending);
		workbench.publish({
			.Context = pending,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		profile.apply_code_font_to(workbench);
		process_events();

		workbench.next_change_button()->click();
		process_events();
		auto* const controller = workbench.scroll_controller();
		expect(
			workbench.selection().has_value(),
			"a semantic selection is held before the tab width changes");
		process_events();

		const auto beforeResult    = workbench.current_result();
		const auto beforeSelection = workbench.selection();
		const auto beforeMapping   = core::map_line(
			*beforeResult,
			beforeSelection->PreferredLine);
		const auto beforeMappingBytes = mapping_bytes(beforeMapping);
		const auto beforeDetail       = core::detail_for_selection(
			*beforeResult,
			*beforeSelection);
		const auto beforeRibbon     = workbench.link_map()->selected();
		const auto beforeStatistics = workbench.statistics();
		const auto beforeRibbonBuilds
			= workbench.link_map()->statistics().GeometryRebuilds;
		const auto beforePaneFont = workbench.left_pane()->font();
		const auto beforeTabStop  = workbench.left_pane()->tabStopDistance();

		// Only the tab width moves. Family, size, weight, italic, and the
		// ligature and hinting strategy all stay exactly as they were.
		auto widened         = profile.typography().Code;
		widened.TabWidth     = widened.TabWidth == 8U ? 4U : 8U;
		const auto selection = profile.select_code_font(widened);
		expect(
			selection.Changed && selection.accepted(),
			"the wider tab width is legal and changes the applied presentation");

		profile.apply_code_font_to(workbench);
		process_events();
		process_events();

		expect(
			workbench.left_pane()->font() == beforePaneFont,
			"no font property moved, so a font change cannot explain a rebuild");
		expect(
			!close(workbench.left_pane()->tabStopDistance(), beforeTabStop),
			"the panes received the new tab stop distance");
		expect(
			workbench.statistics().GeometryRebuilds
					> beforeStatistics.GeometryRebuilds
				&& workbench.statistics().GeometryFailures
					== beforeStatistics.GeometryFailures,
			"the tab width alone invalidates the measured axis and rebuilds it");
		expect(
			workbench.link_map()->statistics().GeometryRebuilds
				> beforeRibbonBuilds,
			"the tab width alone invalidates the ribbon geometry and rebuilds it");

		// Semantics are no more tab-dependent than they are font-dependent.
		expect(
			workbench.current_result() == beforeResult,
			"the accepted immutable comparison is not recomputed or replaced");
		expect(
			workbench.selection() == beforeSelection,
			"the tab width change preserves the exact semantic selection");
		const auto afterMapping = core::map_line(
			*workbench.current_result(),
			workbench.selection()->PreferredLine);
		expect(
			mapping_bytes(afterMapping) == beforeMappingBytes,
			"every byte of the line mapping is identical across the change");
		expect(
			afterMapping.Kind == beforeMapping.Kind
				&& afterMapping.Pair == beforeMapping.Pair
				&& afterMapping.Counterpart == beforeMapping.Counterpart,
			"pairing confidence, the paired line, and its counterpart are unchanged");
		expect(
			core::detail_for_selection(
				*workbench.current_result(),
				*workbench.selection())
				== beforeDetail,
			"the line-over-line detail the selection resolves to is unchanged");
		expect(
			workbench.link_map()->selected() == beforeRibbon,
			"the painted correspondence still names the same stable identity");
		expect(
			controller->maximum_position() > 0.0,
			"the comparison space remains measured after the rebuild");

		// Re-applying the same typography is not a change and must not rebuild.
		const auto settledRebuilds = workbench.statistics().GeometryRebuilds;
		profile.apply_code_font_to(workbench);
		process_events();
		process_events();
		expect(
			workbench.statistics().GeometryRebuilds == settledRebuilds,
			"re-publishing an unchanged tab width invalidates nothing");
	}

	auto test_a_font_change_moves_geometry_and_not_semantics() -> void
	{
		const auto diff = computed_comparison();
		expect(
			diff.Alignment.Groups.size() >= 3U,
			"the fixture carries several semantic changes");

		auto profile   = qt::appearance_profile{ };
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'100, 760);
		workbench.show();
		process_events();

		const auto pending = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = diff.Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(pending);
		workbench.publish({
			.Context = pending,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();
		expect(
			workbench.current_result() != nullptr,
			"the accepted immutable comparison reaches the composed workbench");

		// Presentation starts on the profile's own code font, so what follows is
		// a font change rather than the first font a surface ever received.
		profile.apply_code_font_to(workbench);
		process_events();

		workbench.next_change_button()->click();
		process_events();
		auto* const controller = workbench.scroll_controller();
		expect(
			workbench.selection().has_value()
				&& workbench.selection()->Segment.has_value()
				&& controller->reveal_segment(*workbench.selection()->Segment, 0.5),
			"a semantic selection holds the one shared comparison position");
		process_events();

		const auto beforeResult    = workbench.current_result();
		const auto beforeSelection = workbench.selection();
		const auto beforeMapping   = core::map_line(
			*beforeResult,
			beforeSelection->PreferredLine);
		const auto beforeMappingBytes = mapping_bytes(beforeMapping);
		const auto beforeDetail       = core::detail_for_selection(
			*beforeResult,
			*beforeSelection);
		const auto beforeRibbon = workbench.link_map()->selected();
		expect(
			beforeRibbon.has_value(),
			"the selected correspondence is painted before the font changes");
		const auto beforeExtent = ribbon_extent(*workbench.link_map(), *beforeRibbon);
		expect(
			beforeExtent.has_value() && *beforeExtent > 0.0,
			"the selected ribbon has a measured extent before the font changes");
		const auto beforeFrame      = *controller->current_frame();
		const auto beforeComparison = controller->maximum_position();
		const auto beforeStatistics = workbench.statistics();
		const auto beforeRibbonBuilds
			= workbench.link_map()->statistics().GeometryRebuilds;
		const auto beforePaneFont = workbench.left_pane()->font();
		expect(
			beforeFrame.Position > 0.0 && beforeComparison > 0.0,
			"the shared position sits inside a measured comparison space");

		const auto originalCode          = profile.typography().Code;
		auto       larger                = originalCode;
		larger.Font.PointSizeCentipoints = std::min(
			core::bounds_of(core::typography_field::point_size).Maximum,
			larger.Font.PointSizeCentipoints + 400U);
		expect(
			larger.Font.PointSizeCentipoints
				> originalCode.Font.PointSizeCentipoints,
			"the offered code font is genuinely larger than the applied one");
		const auto selection = profile.select_code_font(larger);
		expect(
			selection.accepted() && selection.Changed,
			"a legal larger code font is applied and published");
		profile.apply_code_font_to(workbench);
		process_events();
		process_events();

		// Geometry is font-dependent and must move.
		expect(
			workbench.left_pane()->font().pointSizeF()
				> beforePaneFont.pointSizeF(),
			"the applied code font reaches the live source pane without a restart");
		expect(
			workbench.statistics().GeometryRebuilds
					> beforeStatistics.GeometryRebuilds
				&& workbench.statistics().GeometryFailures
					== beforeStatistics.GeometryFailures,
			"the font change invalidates the Qt-local measured axis and rebuilds it");
		expect(
			workbench.link_map()->statistics().GeometryRebuilds
				> beforeRibbonBuilds,
			"the font change invalidates the ribbon geometry and rebuilds it");
		expect(
			controller->maximum_position() > beforeComparison,
			"a larger code font lengthens the measured comparison space");
		const auto afterExtent = ribbon_extent(*workbench.link_map(), *beforeRibbon);
		expect(
			afterExtent.has_value() && *afterExtent > *beforeExtent,
			"the same ribbon is painted at a new measured extent");

		// Semantics are not font-dependent and must not move.
		expect(
			workbench.current_result() == beforeResult,
			"the accepted immutable comparison is not recomputed or replaced");
		expect(
			workbench.selection() == beforeSelection,
			"the font change preserves the exact semantic selection");
		const auto afterMapping = core::map_line(
			*workbench.current_result(),
			workbench.selection()->PreferredLine);
		expect(
			mapping_bytes(afterMapping) == beforeMappingBytes,
			"every byte of the line mapping is identical across the font change");
		expect(
			afterMapping.Kind == beforeMapping.Kind
				&& afterMapping.Pair == beforeMapping.Pair
				&& afterMapping.Counterpart == beforeMapping.Counterpart,
			"pairing confidence, the paired line, and its counterpart are unchanged");
		expect(
			core::detail_for_selection(
				*workbench.current_result(),
				*workbench.selection())
				== beforeDetail,
			"the line-over-line detail the selection resolves to is unchanged");
		expect(
			workbench.link_map()->selected() == beforeRibbon,
			"the painted correspondence still names the same stable identity");
		expect(
			workbench.statistics().SelectionChanges
				== beforeStatistics.SelectionChanges,
			"a font change is not a selection change");

		// The one shared comparison position is reprojected, not discarded.
		const auto& afterFrame = *controller->current_frame();
		expect(
			afterFrame.Comparison == beforeFrame.Comparison
				&& afterFrame.Segment == beforeFrame.Segment,
			"the reprojected position keeps its comparison and semantic anchor");
		expect(
			!close(afterFrame.Position, beforeFrame.Position)
				&& afterFrame.Position > 0.0,
			"the shared position is reprojected into the new measured coordinates");
		expect(
			afterFrame.Left.ProjectedDocumentY
					> beforeFrame.Left.ProjectedDocumentY
				&& afterFrame.Right.ProjectedDocumentY
					> beforeFrame.Right.ProjectedDocumentY,
			"both panes project the same semantic anchor at a new document position");

		// Reverting the font is the same claim in the other direction.
		expect(
			profile.select_code_font(originalCode).Changed,
			"the original code font is offered back and applied");
		profile.apply_code_font_to(workbench);
		process_events();
		process_events();
		expect(
			close(controller->maximum_position(), beforeComparison, 0.5),
			"reverting the font returns the measured comparison space");
		expect(
			workbench.selection() == beforeSelection
				&& mapping_bytes(core::map_line(
					   *workbench.current_result(),
					   workbench.selection()->PreferredLine))
					== beforeMappingBytes,
			"the round trip through two font changes moves no semantic byte");
	}

	// The core enumerates the pairs and publishes the thresholds; the Qt adapter
	// owns the palettes that ship. Until this test, nothing joined them, so the
	// audit had never run against the colours a user actually sees.
	//
	// The pair set and the minimums are taken from the core as-is. Narrowing
	// either to make a built-in palette pass would leave a check that measures
	// only the cases already known to be fine.
	void test_every_builtin_profile_passes_the_published_contrast_audit()
	{
		// Every variant is measured before anything fails, because stopping at
		// the first shortfall would hide how many palettes are affected and
		// turn a palette repair into a sequence of blind single steps.
		auto failures = std::size_t{ 0 };

		for (const auto variant : all_variants) {
			const auto& palette  = qt::builtin_appearance_palette(variant);
			const auto  findings = core::measure_required_contrast(palette, variant);

			expect(
				findings.size() == core::required_contrast_pairs().size(),
				"the audit measures every published pair");

			for (const auto& finding : findings) {
				if (finding.Meets) {
					continue;
				}

				++failures;
				std::cerr << "  variant " << static_cast<int>(variant)
						  << " foreground role "
						  << static_cast<int>(finding.Pair.Foreground)
						  << " on background role "
						  << static_cast<int>(finding.Pair.Background) << ": "
						  << finding.RatioHundredths << " hundredths, minimum "
						  << finding.MinimumHundredths << '\n';
			}

			expect(
				core::failing_contrast(palette, variant).size()
					== static_cast<std::size_t>(std::ranges::count_if(
						findings,
						[](const auto& aFinding) { return !aFinding.Meets; })),
				"the failing-pair list agrees with the per-pair verdicts");
		}

		if (failures != 0U) {
			std::cerr << "  " << failures
					  << " checked pairs miss their published minimum\n";
		}

		expect(
			failures == 0U,
			"every built-in profile meets the published minimum for every "
			"checked pair");
	}

	// Meeting the contrast band pins every fill's luminance, and on a white
	// canvas a fully saturated red at that luminance is very nearly the row
	// fill's own colour. Satisfying the audit by collapsing the intraline
	// emphasis onto the row it marks would trade one accessibility failure for
	// another, so the two are required to stay distinct.
	void test_intraline_emphasis_never_collapses_onto_its_row_fill()
	{
		constexpr auto pairs = std::array{
			std::pair{ role::diff_added_background,
					   role::diff_intraline_added_background },
			std::pair{ role::diff_removed_background,
					   role::diff_intraline_removed_background },
		};

		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);

			for (const auto& [rowRole, emphasisRole] : pairs) {
				expect(
					palette[core::index_of(rowRole)]
						!= palette[core::index_of(emphasisRole)],
					"an intraline emphasis fill differs from its row fill");
			}
		}
	}

	struct color_only_obligation
	{
		core::appearance_variant Variant;
		role                     First;
		role                     Second;
	};

	// The pairs below are the ones colour alone cannot carry in a built-in
	// profile, so the presentation layer owes each of them a line prefix,
	// gutter mark, icon, outline, or accessible name. RFC-BPR-0006 already
	// requires that of every state; this list is what makes the requirement
	// specific and reviewable rather than a general intention.
	//
	// Red against green is the axis both modelled dichromacies lose, and the
	// RFC specifies red/green diff fills, so these pairs are inherent to the
	// stated design rather than a palette that can be repainted out of them.
	// The list is pinned so a later palette edit that creates a *new*
	// colour-only-indistinguishable pair fails instead of passing quietly.
	constexpr auto reviewed_color_only_obligations = std::array{
		color_only_obligation{ core::appearance_variant::light,
							   role::diff_added_background,
							   role::diff_removed_background },
		color_only_obligation{ core::appearance_variant::light,
							   role::diff_added_background,
							   role::diff_moved_background },
		color_only_obligation{ core::appearance_variant::light,
							   role::state_fixed,
							   role::state_unread },
		// Warning and error both have to be dark enough to stay legible on a
		// selected line, and deuteranopia collapses the axis that separates a
		// dark amber from a dark red. The high-contrast light profile has owed
		// this same pair a non-colour signal since it was written; refitting
		// the light syntax colours for the selection wash brings the default
		// light profile to the same place. RFC-BPR-0006 already requires a
		// non-colour indicator, so the obligation is dischargeable - it is
		// recorded here rather than dodged by keeping the tokens too light to
		// read when selected.
		color_only_obligation{ core::appearance_variant::light,
							   role::syntax_warning,
							   role::syntax_error },
		color_only_obligation{ core::appearance_variant::dark,
							   role::diff_added_background,
							   role::diff_removed_background },
		// The intraline pair, one level down from the row pair above and for
		// the same reason: softening the dark bands toward the reference the
		// owner asked for brought the green and the red close enough that
		// deuteranopia collapses them. Accepting it here rather than keeping
		// the harsher tones the owner rejected -- the row bands have owed the
		// same signal in this profile since it was written, so the intraline
		// spans owing it too is consistent rather than new in kind.
		color_only_obligation{ core::appearance_variant::dark,
							   role::diff_intraline_added_background,
							   role::diff_intraline_removed_background },
		color_only_obligation{ core::appearance_variant::high_contrast_light,
							   role::diff_added_background,
							   role::diff_removed_background },
		color_only_obligation{ core::appearance_variant::high_contrast_light,
							   role::syntax_warning,
							   role::syntax_error },
		color_only_obligation{ core::appearance_variant::high_contrast_dark,
							   role::diff_added_background,
							   role::diff_removed_background },
		color_only_obligation{ core::appearance_variant::high_contrast_dark,
							   role::diff_added_background,
							   role::diff_moved_background },
		color_only_obligation{ core::appearance_variant::high_contrast_dark,
							   role::state_accepted,
							   role::state_defect },
		color_only_obligation{ core::appearance_variant::high_contrast_dark,
							   role::state_fixed,
							   role::state_unread },
	};

	// A pair the core reports as not separable by colour alone is not a palette
	// defect — it is an obligation on the presentation layer to carry the
	// distinction some other way. The audit records which pairs carry that
	// obligation rather than repainting them away.
	void test_hue_critical_pairs_are_measured_for_every_builtin_profile()
	{
		auto unreviewed = std::size_t{ 0 };
		auto mismatched = std::size_t{ 0 };

		for (const auto variant : all_variants) {
			const auto& palette  = qt::builtin_appearance_palette(variant);
			const auto  findings = core::measure_hue_critical_pairs(palette);

			expect(
				findings.size() == core::hue_critical_pairs().size(),
				"every hue-critical pair is measured");

			const auto owed = core::pairs_requiring_non_color_indicator(palette);

			expect(
				owed.size()
					== static_cast<std::size_t>(std::ranges::count_if(
						findings,
						[](const auto& aFinding) {
							return !aFinding.DistinguishableByColorAlone;
						})),
				"the owed-indicator list agrees with the per-pair verdicts");

			for (const auto& finding : findings) {
				if (finding.DistinguishableByColorAlone) {
					continue;
				}

				std::cerr << "  variant " << static_cast<int>(variant)
						  << " roles " << static_cast<int>(finding.Pair.First)
						  << "/" << static_cast<int>(finding.Pair.Second)
						  << " need a non-colour signal: typical "
						  << finding.TypicalTenths << ", protanopia "
						  << finding.ProtanopiaTenths << ", deuteranopia "
						  << finding.DeuteranopiaTenths << " tenths\n";
			}

			for (const auto& finding : owed) {
				const auto reviewed = std::ranges::any_of(
					reviewed_color_only_obligations,
					[&](const auto& aExpected) {
						return aExpected.Variant == variant
							&& aExpected.First == finding.Pair.First
							&& aExpected.Second == finding.Pair.Second;
					});

				if (!reviewed) {
					++unreviewed;
					std::cerr << "  UNREVIEWED variant "
							  << static_cast<int>(variant) << " roles "
							  << static_cast<int>(finding.Pair.First) << "/"
							  << static_cast<int>(finding.Pair.Second) << '\n';
				}
			}

			const auto expected = static_cast<std::size_t>(std::ranges::count(
				reviewed_color_only_obligations,
				variant,
				&color_only_obligation::Variant));

			if (owed.size() != expected) {
				++mismatched;
				std::cerr << "  variant " << static_cast<int>(variant)
						  << " owes " << owed.size() << " non-colour signals, "
						  << expected << " recorded\n";
			}
		}

		expect(
			mismatched == 0U,
			"no reviewed non-colour obligation has silently disappeared");
		expect(
			unreviewed == 0U,
			"every pair colour alone cannot separate is one this palette was "
			"reviewed and accepted as owing a non-colour signal");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_fresh_installation_defaults_to_system();
	test_builtin_profiles_are_total();
	test_high_contrast_profiles_are_not_copies();
	test_derived_text_stays_readable_on_every_fill();
	test_the_core_decides_which_variant_applies();
	test_applies_semantic_colors_without_forcing_style_or_stylesheet();
	test_immediate_preview_updates_without_restart();
	test_customisation_round_trips_through_the_durable_record();
	test_typography_defaults_are_legal_and_native();
	test_fonts_are_built_from_the_core_record();
	test_every_typography_field_previews_immediately();
	test_a_value_outside_the_published_bounds_is_never_applied();
	test_every_role_converts_through_presentation_colour();
	test_a_font_change_moves_geometry_and_not_semantics();
	test_a_tab_width_change_alone_invalidates_measured_geometry();
	test_intraline_emphasis_never_collapses_onto_its_row_fill();
	test_hue_critical_pairs_are_measured_for_every_builtin_profile();
	test_every_builtin_profile_passes_the_published_contrast_audit();

	std::cout << "appearance-profile-tests:pass\n";
	return EXIT_SUCCESS;
}
