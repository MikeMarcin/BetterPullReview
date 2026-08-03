#include "bpr/qt/appearance_profile.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

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
		const auto inheritedButton
			= canvas.palette().color(QPalette::Active, QPalette::Button);

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
			applied.color(QPalette::Active, QPalette::Button) == inheritedButton,
			"a role the profile does not own stays exactly as inherited");
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
	test_every_role_converts_through_presentation_colour();

	std::cout << "appearance-profile-tests:pass\n";
	return EXIT_SUCCESS;
}
