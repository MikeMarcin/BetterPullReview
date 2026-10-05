#include "diff_publication_fixture.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QPainterPathStroker>
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
#include <ranges>
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
	constexpr auto readable_contrast      = 4.5;
	constexpr auto high_contrast_minimum  = 7.0;
	constexpr auto maximum_contrast       = 21.0;
	constexpr auto disabled_icon_contrast = 3.0;

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

	/// \brief Counts the strongly opaque pixels carrying one exact palette tint.
	[[nodiscard]] auto palette_foreground_pixels(
		const QIcon&      aIcon,
		const QIcon::Mode aMode,
		const QColor&     aExpected) -> std::size_t
	{
		const auto image = aIcon.pixmap(
									QSize{ 24, 24 }, 2.0, aMode, QIcon::Off)
							   .toImage()
							   .convertToFormat(QImage::Format_ARGB32);
		if (image.isNull() || !aExpected.isValid()) {
			return 0U;
		}
		constexpr auto rgbComponentMask = QRgb{ 0x00FF'FFFFU };
		const auto     expected         = aExpected.rgb() & rgbComponentMask;
		auto           count            = std::size_t{ 0U };
		for (auto y = 0; y < image.height(); ++y) {
			for (auto x = 0; x < image.width(); ++x) {
				const auto pixel = image.pixel(x, y);
				if (qAlpha(pixel) >= 224
					&& (pixel & rgbComponentMask) == expected) {
					++count;
				}
			}
		}
		return count;
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

		// BPR-0216. The check above runs under an empty observation, which is
		// the one case where the composed default's palette and the built-in
		// palette of the resolved variant happen to be the same table -- so it
		// passed while the product painted light on a dark desktop.
		//
		// A fresh record is the absence of a decision, and the desktop it is
		// read on cannot turn it into forty-six of them.
		const auto onDark = qt::appearance_profile{
			qt::default_appearance_settings(),
			core::appearance_observation{ .Scheme = core::os_color_scheme::dark },
		};
		expect(
			onDark.variant() == core::appearance_variant::dark,
			"System mode on a dark desktop resolves the dark variant");
		expect(
			onDark.customised_roles() == 0U,
			"a fresh installation customises no semantic colour on a dark "
			"desktop either");
		expect(
			onDark.palette()
				== qt::builtin_appearance_palette(core::appearance_variant::dark),
			"and therefore paints the dark palette, not the default record's "
			"light one");
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

	/// \brief [[BPR-0720]]: the dark syntax roles follow the Visual Studio dark
	/// theme with Visual Assist colouring; the light palette is untouched.
	auto test_the_dark_palette_follows_the_visual_assist_scheme() -> void
	{
		const auto colour = [](const std::uint8_t aRed,
							   const std::uint8_t aGreen,
							   const std::uint8_t aBlue) {
			return core::appearance_color{ aRed, aGreen, aBlue, 255U };
		};
		const auto& dark  = qt::builtin_appearance_palette(core::appearance_variant::dark);
		const auto& light = qt::builtin_appearance_palette(core::appearance_variant::light);
		const auto  pins  = std::array{
			std::pair{ role::syntax_comment, colour(0x66U, 0xB5U, 0x59U) },
			std::pair{ role::syntax_documentation, colour(0x66U, 0xB5U, 0x59U) },
			std::pair{ role::syntax_preprocessor, colour(0xA3U, 0xA3U, 0xA3U) },
			std::pair{ role::syntax_import, colour(0xA3U, 0xA3U, 0xA3U) },
			std::pair{ role::syntax_keyword, colour(0x6DU, 0xA9U, 0xDBU) },
			std::pair{ role::syntax_type, colour(0xFFU, 0xD7U, 0x00U) },
			std::pair{ role::syntax_function, colour(0xFFU, 0x80U, 0x00U) },
			std::pair{ role::syntax_macro, colour(0xCEU, 0x8AU, 0xD4U) },
			std::pair{ role::syntax_variable, colour(0xBDU, 0xB7U, 0x6BU) },
			std::pair{ role::syntax_attribute, colour(0x4EU, 0xC9U, 0xB0U) },
			std::pair{ role::syntax_string, colour(0xD6U, 0x9DU, 0x85U) },
			std::pair{ role::syntax_number, colour(0xB5U, 0xCEU, 0xA8U) },
		};
		for (const auto& [pinned, expected] : pins) {
			expect(dark[core::index_of(pinned)] == expected,
				   "the dark palette gives the pinned Visual Assist colour to "
					   + std::string{ core::stable_name(pinned) });
		}
		expect(dark[core::index_of(role::syntax_macro)] != dark[core::index_of(role::syntax_preprocessor)]
				   && dark[core::index_of(role::syntax_macro)] != colour(0xA3U, 0xA3U, 0xA3U)
				   && dark[core::index_of(role::syntax_function)] != colour(0xDCU, 0xDCU, 0xAAU),
			   "macros are not painted the directive grey and functions are not Visual Studio's pale yellow");
		// [[BPR-0744]]: a C or C++ `#include` path is classified as a string
		// rather than an import, so the colour the reference paints an include
		// path is the string salmon pinned above. No dark value moved for that
		// change; the import grey stays because it still names `import`, `from`
		// and `@import` in the other packaged grammars.
		expect(dark[core::index_of(role::syntax_string)]
					   != dark[core::index_of(role::syntax_preprocessor)]
				   && dark[core::index_of(role::syntax_string)]
					   != dark[core::index_of(role::syntax_import)],
			   "an include path, carried by the string role, is not the directive grey");
		expect(light[core::index_of(role::syntax_comment)] == colour(0x55U, 0x5EU, 0x69U)
				   && light[core::index_of(role::syntax_preprocessor)] == colour(0x7AU, 0x3EU, 0x9DU),
			   "the light palette keeps its own comment and preprocessor colours");
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

	auto test_navigation_icons_follow_every_profile_palette() -> void
	{
		auto profile = qt::appearance_profile{ };
		auto window  = qt::workbench_window{ };

		for (const auto variant : all_variants) {
			static_cast<void>(profile.select_mode(explicit_mode_for(variant)));
			profile.apply_to(window);
			process_events();

			const auto background = window.palette().color(
				QPalette::Active, QPalette::Button);
			const auto normal = window.palette().color(
				QPalette::Active, QPalette::ButtonText);
			const auto disabled = window.palette().color(
				QPalette::Disabled, QPalette::ButtonText);
			const auto normalMinimum
				= variant == core::appearance_variant::high_contrast_light
					|| variant == core::appearance_variant::high_contrast_dark
				? high_contrast_minimum
				: readable_contrast;
			expect(
				qt::contrast_ratio(
					qt::to_appearance_color(background),
					qt::to_appearance_color(normal))
					>= normalMinimum,
				"normal navigation foreground meets the profile's text contrast floor");
			expect(
				qt::contrast_ratio(
					qt::to_appearance_color(background),
					qt::to_appearance_color(disabled))
					>= disabled_icon_contrast,
				"disabled navigation foreground remains a readable non-text indicator");
			expect(
				normal != disabled,
				"normal and disabled navigation foregrounds remain distinguishable");

			const auto actions = std::array{
				window.previous_file_action(),
				window.next_file_action(),
				window.previous_change_action(),
				window.next_change_action(),
			};
			auto normalImages = std::array<QImage, 4U>{ };
			for (std::size_t index = 0U; index < actions.size(); ++index) {
				const auto& icon = actions[index]->icon();
				expect(
					palette_foreground_pixels(icon, QIcon::Normal, normal) > 0U,
					"each normal navigation icon paints exact live-palette foreground pixels");
				expect(
					palette_foreground_pixels(icon, QIcon::Disabled, disabled) > 0U,
					"each disabled navigation icon paints exact live-palette foreground pixels");
				const auto normalImage   = icon.pixmap(
												   QSize{ 24, 24 }, 2.0, QIcon::Normal, QIcon::Off)
											   .toImage();
				const auto disabledImage = icon.pixmap(
												   QSize{ 24, 24 }, 2.0, QIcon::Disabled, QIcon::Off)
											   .toImage();
				expect(
					!normalImage.isNull() && normalImage != disabledImage,
					"each navigation icon carries visibly distinct normal and disabled modes");
				normalImages[index] = normalImage;
			}
			for (std::size_t first = 0U; first < normalImages.size(); ++first) {
				for (std::size_t second = first + 1U;
					 second < normalImages.size(); ++second) {
					expect(
						normalImages[first] != normalImages[second],
						"previous and next file/change silhouettes remain visually distinct");
				}
			}
		}
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
			application::customisation_of(
				profile.settings(), core::appearance_variant::dark)
					.Palette[core::index_of(role::code_background)]
				== customBackground,
			"the durable record carries the customised colour under the theme it "
			"was chosen for");

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

	auto test_a_stored_palette_customises_exactly_one_variant() -> void
	{
		constexpr auto onDark = core::appearance_observation{
			.Scheme = core::os_color_scheme::dark
		};
		constexpr auto onLight = core::appearance_observation{
			.Scheme = core::os_color_scheme::light
		};

		// BPR-0645. The record that produced the defect: System mode and a
		// stored palette that is byte-for-byte the built-in light profile. Read
		// against the variant the desktop resolves it is forty-six deliberate
		// overrides pinning light colours over the dark profile; read against
		// the variant it actually customises it is no customisation at all.
		auto record = qt::default_appearance_settings();
		record.Mode = core::appearance_mode::system;
		application::customisation_of(record, core::appearance_variant::light)
			= application::appearance_palette_customisation{
				  .Palette = qt::builtin_appearance_palette(
					  core::appearance_variant::light),
				  .Stored = true,
			  };

		const auto builtin = qt::appearance_profile{ record, onDark };
		expect(builtin.variant() == core::appearance_variant::dark,
			   "a dark desktop still resolves the dark variant");
		expect(builtin.customised_roles() == 0U,
			   "a stored palette equal to a built-in profile is no "
			   "customisation of it");
		expect(
			builtin.palette()
				== qt::builtin_appearance_palette(core::appearance_variant::dark),
			"and the dark desktop renders the built-in dark profile untouched");

		// A real customisation of the dark profile, which must apply on a dark
		// desktop and colour nothing on a light one.
		constexpr auto chosen = core::appearance_color{ 3U, 5U, 7U, 255U };
		auto           darkPalette
			= qt::builtin_appearance_palette(core::appearance_variant::dark);
		darkPalette[core::index_of(role::state_defect)] = chosen;
		auto dark                                       = qt::default_appearance_settings();
		dark.Mode                                       = core::appearance_mode::system;
		application::customisation_of(dark, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ .Palette = darkPalette,
															 .Stored  = true };

		const auto applied = qt::appearance_profile{ dark, onDark };
		expect(applied.customised_roles() == 1U
				   && applied.palette()[core::index_of(role::state_defect)]
					   == chosen,
			   "a dark customisation applies while the dark variant resolves");

		const auto elsewhere = qt::appearance_profile{ dark, onLight };
		expect(elsewhere.variant() == core::appearance_variant::light,
			   "the same record on a light desktop resolves light");
		expect(
			elsewhere.palette()
				== qt::builtin_appearance_palette(core::appearance_variant::light),
			"and renders the built-in light profile untouched, because a "
			"colour chosen against the dark profile says nothing about this "
			"one");

		// The same profile carried across a live desktop change, not two
		// separately constructed ones: the overrides must leave and return
		// with their own variant rather than survive into the other.
		auto moving = qt::appearance_profile{ dark, onDark };
		expect(moving.observe(onLight)
				   && moving.palette()
					   == qt::builtin_appearance_palette(
						   core::appearance_variant::light),
			   "following the desktop into light drops the dark customisation "
			   "from what is rendered");
		expect(moving.observe(onDark)
				   && moving.palette()[core::index_of(role::state_defect)]
					   == chosen,
			   "and following it back restores the customisation to the "
			   "variant it belongs to");

		// The record written back names the variant its palette customises, so
		// the next launch reads it against the same built-in profile.
		expect(application::customisation_of(
				   applied.settings(), core::appearance_variant::dark)
				   == application::appearance_palette_customisation{
					   .Palette = darkPalette, .Stored = true },
			   "the durable record carries the customisation under the theme it "
			   "belongs to");
		expect(elsewhere.settings() == applied.settings(),
			   "and reports the same record while another theme is on screen, so an "
			   "observation flip cannot rewrite it");
		const auto reread = qt::appearance_profile{ applied.settings(), onDark };
		expect(reread.palette() == applied.palette()
				   && reread.customised_roles() == 1U,
			   "and reproduces exactly the same customisation on reload");
	}

	/// \brief [[BPR-1083]]. A stored palette written before the twelve label
	/// tints existed reads them as the built-in tints of its own theme, and no
	/// other role changes (owner, BPR-1079 `tints-v002`).
	auto test_a_palette_without_label_tints_reads_the_builtin_tints() -> void
	{
		constexpr auto onDark                      = core::appearance_observation{ .Scheme = core::os_color_scheme::dark };
		constexpr auto chosen                      = core::appearance_color{ 3U, 5U, 7U, 255U };
		auto           stored                      = qt::builtin_appearance_palette(core::appearance_variant::dark);
		stored[core::index_of(role::state_defect)] = chosen;
		for (auto index = core::index_of(core::first_feedback_label_role); index < core::appearance_role_count; ++index) {
			stored[index] = core::unassigned_appearance_color;
		}
		auto record = qt::default_appearance_settings();
		record.Mode = core::appearance_mode::dark;
		application::customisation_of(record, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ .Palette = stored, .Stored = true };
		const auto profile                           = qt::appearance_profile{ record, onDark };
		auto       expected                          = qt::builtin_appearance_palette(core::appearance_variant::dark);
		expected[core::index_of(role::state_defect)] = chosen;
		expect(profile.palette() == expected && profile.customised_roles() == 1U,
			   "the missing label tints are the dark built-in tints and only the chosen role is customised");
		expect(profile.color(role::feedback_label_issue) == QColor{ 0xFF, 0x7B, 0x72 },
			   "dark issue is #FF7B72, the dark built-in tint");

		auto previewed                                        = qt::appearance_profile{ qt::default_appearance_settings(), onDark };
		auto imported                                         = qt::builtin_appearance_palette(core::appearance_variant::dark);
		imported[core::index_of(role::feedback_label_praise)] = core::unassigned_appearance_color;
		static_cast<void>(previewed.preview_palette(imported));
		expect(previewed.palette() == qt::builtin_appearance_palette(core::appearance_variant::dark),
			   "an imported theme without a label tint previews that tint as the built-in one, never transparent");

		const auto& light  = qt::builtin_appearance_palette(core::appearance_variant::light);
		const auto& dark   = qt::builtin_appearance_palette(core::appearance_variant::dark);
		const auto& hcl    = qt::builtin_appearance_palette(core::appearance_variant::high_contrast_light);
		const auto& hcd    = qt::builtin_appearance_palette(core::appearance_variant::high_contrast_dark);
		const auto  issue  = core::index_of(role::feedback_label_issue);
		const auto  praise = core::index_of(role::feedback_label_praise);
		expect(light[issue] == core::appearance_color{ 0xD1U, 0x24U, 0x2FU, 255U } && dark[issue] == core::appearance_color{ 0xFFU, 0x7BU, 0x72U, 255U }
				   && hcl[issue] == core::appearance_color{ 0xA5U, 0x00U, 0x00U, 255U } && hcd[issue] == core::appearance_color{ 0xFFU, 0x70U, 0x70U, 255U }
				   && light[praise] == core::appearance_color{ 0xBFU, 0x39U, 0x89U, 255U } && hcd[praise] == core::appearance_color{ 0xFFU, 0x8FU, 0xD0U, 255U },
			   "each theme variant has its own built-in tints, the BPR-1079 table");
	}

	auto test_a_record_adopted_with_its_variant_does_not_colour_another() -> void
	{
		auto record  = qt::default_appearance_settings();
		record.Mode  = core::appearance_mode::system;
		auto profile = qt::appearance_profile{
			record, core::appearance_observation{ .Scheme = core::os_color_scheme::dark }
		};
		const auto dark = qt::builtin_appearance_palette(core::appearance_variant::dark);
		expect(profile.variant() == core::appearance_variant::dark
				   && profile.palette() == dark,
			   "a dark desktop resolves the dark built-in before any record arrives");
		// The durable record carries the light built-in palette and names light.
		static_cast<void>(profile.adopt_palette(
			qt::builtin_appearance_palette(core::appearance_variant::light),
			qt::palette_preview_adoption::commit,
			core::appearance_variant::light));
		expect(profile.palette() == dark && profile.customised_roles() == 0U,
			   "a light record adopted with its own variant leaves the dark profile untouched");
		// The adopted palette *is* the built-in light profile, so it overrides
		// nothing: a stored palette equal to a built-in profile is no customisation
		// of it (BPR-0645), which is the same rule the neighbouring fixture proves
		// from the other direction.
		expect(profile.customised_roles(core::appearance_variant::light) == 0U
				   && profile.settings() == application::appearance_settings{ .Mode = profile.mode(), .Typography = profile.typography() },
			   "and the record reports no customisation for light either, because "
			   "matching the built-in profile is not customising it");
	}

	/// \brief RFC-BPR-0034's behaviour change: each theme keeps its own colours.
	///
	/// \details This fixture is the one that used to assert the opposite. A single
	/// override table had to belong to one variant at a time, so editing a colour
	/// while Dark was on screen *discarded* whatever had been chosen for Light —
	/// "I'm a bit confused at how colors and theme interact". The edit is still a
	/// statement about the theme on screen and nothing else, but the other theme's
	/// statement now survives it.
	auto test_editing_a_colour_keeps_every_other_theme() -> void
	{
		constexpr auto onDark = core::appearance_observation{
			.Scheme = core::os_color_scheme::dark
		};
		constexpr auto onLight = core::appearance_observation{
			.Scheme = core::os_color_scheme::light
		};
		constexpr auto chosen = core::appearance_color{ 11U, 22U, 33U, 255U };
		constexpr auto stale  = core::appearance_color{ 200U, 100U, 50U, 255U };

		// Starts customised for light, then edits while dark is on screen.
		auto lightPalette
			= qt::builtin_appearance_palette(core::appearance_variant::light);
		lightPalette[core::index_of(role::state_stale)] = stale;
		auto record                                     = qt::default_appearance_settings();
		record.Mode                                     = core::appearance_mode::system;
		application::customisation_of(record, core::appearance_variant::light)
			= application::appearance_palette_customisation{ .Palette = lightPalette,
															 .Stored  = true };

		auto profile = qt::appearance_profile{ record, onDark };
		expect(profile.customise(role::state_defect, chosen),
			   "customising while the dark variant resolves changes what is "
			   "rendered");
		expect(profile.customised_roles() == 1U
				   && profile.customised_roles(core::appearance_variant::dark) == 1U,
			   "the dark edit is one override of the dark profile");
		expect(profile.customised_roles(core::appearance_variant::light) == 1U,
			   "and the light customisation survives it rather than being "
			   "discarded, which is the behaviour RFC-BPR-0034 changed");

		const auto written = profile.settings();
		expect(application::customisation_of(written, core::appearance_variant::dark)
					   .Stored
				   && application::customisation_of(
						  written, core::appearance_variant::dark)
						   .Palette[core::index_of(role::state_defect)]
					   == chosen,
			   "the record written back carries the dark edit under Dark");
		expect(application::customisation_of(written, core::appearance_variant::dark)
					   .Palette[core::index_of(role::state_stale)]
				   == qt::builtin_appearance_palette(
					   core::appearance_variant::dark)[core::index_of(role::state_stale)],
			   "every role the dark edit did not name keeps its built-in dark colour");
		expect(application::customisation_of(written, core::appearance_variant::light)
				   == application::appearance_palette_customisation{
					   .Palette = lightPalette, .Stored = true },
			   "and Light's own colours are written back exactly as they were");

		// Both themes render their own colours as the desktop moves between them.
		expect(profile.palette()[core::index_of(role::state_defect)] == chosen,
			   "the dark edit applies while dark resolves");
		expect(profile.observe(onLight)
				   && profile.palette()[core::index_of(role::state_stale)] == stale
				   && profile.palette()[core::index_of(role::state_defect)]
					   == qt::builtin_appearance_palette(
						   core::appearance_variant::light)[core::index_of(role::state_defect)],
			   "and following the desktop to light renders light's own "
			   "customisation, not dark's");

		// Reset all is scoped to one theme too.
		expect(profile.restore_all(core::appearance_variant::light)
				   && profile.customised_roles(core::appearance_variant::light) == 0U
				   && profile.customised_roles(core::appearance_variant::dark) == 1U,
			   "resetting the theme on screen spares every other theme's colours");
	}

	/// \brief Per-variant counts and each theme's own storable palette.
	auto test_customised_counts_and_palettes_are_per_theme() -> void
	{
		auto profile = qt::appearance_profile{ };
		expect(profile.variant() == core::appearance_variant::light,
			   "a fresh profile with no observation resolves light");
		static_cast<void>(profile.customise(role::state_defect,
											core::appearance_color{ 1U, 2U, 3U, 255U }));
		static_cast<void>(profile.customise(role::state_stale,
											core::appearance_color{ 4U, 5U, 6U, 255U }));
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		static_cast<void>(profile.customise(role::state_fixed,
											core::appearance_color{ 7U, 8U, 9U, 255U }));

		expect(profile.customised_roles(core::appearance_variant::light) == 2U
				   && profile.customised_roles(core::appearance_variant::dark) == 1U
				   && profile.customised_roles(core::appearance_variant::high_contrast_light) == 0U
				   && profile.customised_roles(core::appearance_variant::high_contrast_dark) == 0U,
			   "each theme reports the number of roles it differs from its own "
			   "built-in palette by");
		expect(profile.customised_roles()
				   == profile.customised_roles(core::appearance_variant::dark),
			   "the unqualified count is the theme on screen");
		expect(profile.customised_roles(static_cast<core::appearance_variant>(200U)) == 0U,
			   "a variant outside the closed vocabulary customises nothing");

		// Each theme's storable palette is its own built-in profile plus its own
		// overrides, whichever theme happens to be on screen.
		const auto light = profile.palette_for(core::appearance_variant::light);
		expect(light[core::index_of(role::state_defect)]
					   == core::appearance_color{ 1U, 2U, 3U, 255U }
				   && light[core::index_of(role::state_fixed)]
					   == qt::builtin_appearance_palette(
						   core::appearance_variant::light)[core::index_of(role::state_fixed)],
			   "light's palette carries light's overrides over light's built-in");
		expect(profile.palette_for(core::appearance_variant::high_contrast_dark)
				   == qt::builtin_appearance_palette(
					   core::appearance_variant::high_contrast_dark),
			   "an uncustomised theme's palette is exactly its built-in profile");
	}

	auto test_total_palette_preview_cancel_and_adoption_are_exact() -> void
	{
		auto profile       = qt::appearance_profile{ };
		auto notifications = std::size_t{ 0U };
		profile.set_sink([&notifications] { ++notifications; });
		const auto committed                                          = profile.palette();
		auto       preview                                            = committed;
		preview[core::index_of(role::diff_group_boundary_rule)].Alpha = 173U;

		const auto lightSlot = [&profile] {
			return application::customisation_of(
				profile.settings(), core::appearance_variant::light);
		};

		expect(profile.preview_palette(preview)
				   && profile.previewing_palette()
				   && profile.palette() == preview,
			   "a total core palette is previewed immediately");
		expect(lightSlot() == application::appearance_palette_customisation{ },
			   "preview never enters the settings record that persistence reads");
		expect(profile.cancel_palette_preview()
				   && !profile.previewing_palette()
				   && profile.palette() == committed,
			   "cancel restores the exact last host-adopted palette");

		expect(profile.preview_palette(preview),
			   "the candidate can be previewed again before commit");
		const auto beforeAdoption = notifications;
		expect(!profile.adopt_palette(
				   preview,
				   qt::palette_preview_adoption::commit,
				   core::appearance_variant::light),
			   "adopting pixels already on screen produces no duplicate repaint");
		expect(notifications == beforeAdoption
				   && !profile.previewing_palette()
				   && lightSlot().Stored && lightSlot().Palette == preview,
			   "host adoption clears preview state and makes the exact palette durable");

		auto laterPreview                                        = preview;
		laterPreview[core::index_of(role::syntax_keyword)].Green = 91U;
		expect(profile.preview_palette(laterPreview),
			   "a later preview can begin after a palette is already durable");
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		// Re-adopting light's palette while dark is on screen. It belongs to light
		// and is filed there whatever is resolved, which is why the variant is the
		// caller's to name rather than the profile's to derive.
		expect(!profile.adopt_palette(
				   preview,
				   qt::palette_preview_adoption::retain,
				   core::appearance_variant::light)
				   && profile.previewing_palette()
				   && profile.palette() == laterPreview
				   && lightSlot().Palette == preview,
			   "an unrelated host adoption refreshes the committed baseline beneath the exact preview");
		expect(profile.cancel_palette_preview()
				   && profile.palette()
					   == qt::builtin_appearance_palette(core::appearance_variant::dark)
				   && lightSlot().Palette == preview,
			   "cancel after unrelated adoption renders the theme on screen while "
			   "light keeps its own durable palette");
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

	[[nodiscard]] auto computed_connector_comparison()
		-> core::computed_file_diff
	{
		constexpr auto leftText
			= std::string_view{ "same-head\nold-one\nsame-middle\nold-two\nsame-tail\n" };
		constexpr auto rightText
			= std::string_view{ "same-head\nnew-one\nsame-middle\nnew-two\nsame-tail\n" };
		const auto options = core::diff_options{ };
		auto       left    = std::vector<std::uint8_t>{ leftText.begin(), leftText.end() };
		auto       right   = std::vector<std::uint8_t>{ rightText.begin(), rightText.end() };
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(
			outcome.Reason == core::diff_reason_code::none
				&& outcome.Result.has_value(),
			"the connector fixture computes and publishes");
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

	struct connector_pixel_observation
	{
		QColor       Background;
		QColor       Dominant;
		std::size_t  SampledPixels            = 0U;
		std::size_t  ContrastPixels           = 0U;
		std::size_t  DominantPixels           = 0U;
		std::int64_t MedianContrastHundredths = 0;
	};

	[[nodiscard]] auto dominant_pixel(std::vector<QRgb> aPixels)
		-> std::pair<QColor, std::size_t>
	{
		if (aPixels.empty()) {
			return { QColor{ }, 0U };
		}
		std::ranges::sort(aPixels);
		auto bestPixel = aPixels.front();
		auto bestCount = std::size_t{ 0U };
		for (auto first = aPixels.begin(); first != aPixels.end();) {
			const auto last = std::ranges::find_if(
				first, aPixels.end(), [first](const QRgb aPixel) {
					return aPixel != *first;
				});
			const auto count = static_cast<std::size_t>(
				std::distance(first, last));
			if (count > bestCount) {
				bestPixel = *first;
				bestCount = count;
			}
			first = last;
		}
		return { QColor::fromRgba(bestPixel), bestCount };
	}

	[[nodiscard]] auto observe_connector_pixels(
		qt::diff_link_map&           aMap,
		const qt::visible_diff_link& aTarget) -> connector_pixel_observation
	{
		const auto image = aMap.grab().toImage().convertToFormat(
			QImage::Format_ARGB32);
		const auto scale = image.devicePixelRatio();
		if (image.isNull() || scale <= 0.0) {
			return { };
		}

		const auto links   = aMap.visible_links();
		auto       stroker = QPainterPathStroker{ };
		stroker.setWidth(3.0);
		const auto targetMask
			= aTarget.Path.united(stroker.createStroke(aTarget.Path));
		auto backgroundPixels = std::vector<QRgb>{ };
		auto targetPixels     = std::vector<QRgb>{ };
		backgroundPixels.reserve(static_cast<std::size_t>(image.sizeInBytes() / 4));
		targetPixels.reserve(backgroundPixels.capacity() / 4U);
		for (auto y = 0; y < image.height(); ++y) {
			for (auto x = 0; x < image.width(); ++x) {
				const auto point = QPointF{
					(static_cast<qreal>(x) + 0.5) / scale,
					(static_cast<qreal>(y) + 0.5) / scale,
				};
				const auto inAny = std::ranges::any_of(
					links,
					[point](const qt::visible_diff_link& aLink) {
						return aLink.Path.contains(point);
					});
				if (targetMask.contains(point)) {
					const auto overlapsAnother = std::ranges::any_of(
						links,
						[&aTarget, point](const qt::visible_diff_link& aLink) {
							return &aLink != &aTarget && aLink.Path.contains(point);
						});
					if (!overlapsAnother) {
						targetPixels.push_back(image.pixel(x, y));
					}
				} else if (!inAny) {
					backgroundPixels.push_back(image.pixel(x, y));
				}
			}
		}

		const auto [background, backgroundCount]
			= dominant_pixel(std::move(backgroundPixels));
		const auto [dominant, dominantCount] = dominant_pixel(targetPixels);
		if (!background.isValid() || backgroundCount == 0U
			|| !dominant.isValid() || targetPixels.empty()) {
			return { };
		}

		auto contrasts = std::vector<std::int64_t>{ };
		contrasts.reserve(targetPixels.size());
		for (const auto pixel : targetPixels) {
			contrasts.push_back(static_cast<std::int64_t>(std::lround(
				100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(QColor::fromRgba(pixel))))));
		}
		std::ranges::sort(contrasts);
		return {
			.Background     = background,
			.Dominant       = dominant,
			.SampledPixels  = targetPixels.size(),
			.ContrastPixels = static_cast<std::size_t>(
				std::ranges::count_if(contrasts, [](const auto aContrast) {
					return aContrast >= 300;
				})),
			.DominantPixels           = dominantCount,
			.MedianContrastHundredths = contrasts[contrasts.size() / 2U],
		};
	}

	void test_rendered_connectors_clear_the_non_text_floor_in_every_profile()
	{
		const auto diff      = computed_connector_comparison();
		auto       profile   = qt::appearance_profile{ };
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(900, 480);
		workbench.show();
		process_events();

		const auto pending = core::diff_request_context{
			.Request            = core::request_id{ 91U },
			.ApplicationSession = core::application_session_id{ 92U },
			.Key                = diff.Alignment.Key,
			.Generation         = core::request_generation{ 93U },
		};
		workbench.set_loading(pending);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = pending,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		expect(
			workbench.navigate_change(
				core::change_navigation_direction::next,
				core::change_navigation_policy::stop),
			"the connector fixture selects one semantic change");
		process_events();

		for (const auto variant : all_variants) {
			static_cast<void>(profile.select_mode(explicit_mode_for(variant)));
			profile.apply_to(workbench);
			process_events();

			auto* const map       = workbench.link_map();
			const auto  selection = map->selected();
			const auto  links     = map->visible_links();
			const auto  selected  = std::ranges::find_if(
				links,
				[&selection](const qt::visible_diff_link& aLink) {
					return selection.has_value()
						&& (selection->Group.has_value()
									&& aLink.Identity.Group.has_value()
								? selection->Group == aLink.Identity.Group
								: selection->Segment == aLink.Identity.Segment);
				});
			const auto unselected = std::ranges::find_if(
				links,
				[&selection](const qt::visible_diff_link& aLink) {
					return !selection.has_value()
						|| (selection->Group.has_value()
									&& aLink.Identity.Group.has_value()
								? selection->Group != aLink.Identity.Group
								: selection->Segment != aLink.Identity.Segment);
				});
			expect(
				selected != links.end() && unselected != links.end(),
				"the rendered map exposes known selected and unselected paths");

			const auto selectedPixels   = observe_connector_pixels(*map, *selected);
			const auto unselectedPixels = observe_connector_pixels(*map, *unselected);
			const auto sampled          = [](const connector_pixel_observation& aValue) {
				return aValue.SampledPixels >= 24U
					&& aValue.ContrastPixels >= 24U
					&& aValue.DominantPixels >= 24U;
			};
			const auto unselectedReadable = sampled(unselectedPixels)
				&& unselectedPixels.ContrastPixels * 2U
					>= unselectedPixels.SampledPixels
				&& unselectedPixels.MedianContrastHundredths >= 300;
			if (!sampled(selectedPixels) || !unselectedReadable) {
				std::cerr << "  connector variant " << static_cast<int>(variant)
						  << " selected pixels/contrast/dominant/median "
						  << selectedPixels.SampledPixels << '/'
						  << selectedPixels.ContrastPixels << '/'
						  << selectedPixels.DominantPixels << '/'
						  << selectedPixels.MedianContrastHundredths
						  << "; unselected " << unselectedPixels.SampledPixels << '/'
						  << unselectedPixels.ContrastPixels << '/'
						  << unselectedPixels.DominantPixels << '/'
						  << unselectedPixels.MedianContrastHundredths << '\n';
			}
			expect(
				sampled(selectedPixels) && unselectedReadable,
				"actual selected and unselected connector pixels clear the non-text floor");
			expect(
				selectedPixels.Background == unselectedPixels.Background
					&& selectedPixels.Dominant != unselectedPixels.Dominant,
				"selected and unselected connector rasters stay distinct on one observed gutter");
		}
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
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = pending,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();

		profile.apply_code_font_to(workbench);
		process_events();

		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "the semantic navigation bridge selects a retained change");
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
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = pending,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		expect(
			workbench.current_result() != nullptr,
			"the accepted immutable comparison reaches the composed workbench");

		// Presentation starts on the profile's own code font, so what follows is
		// a font change rather than the first font a surface ever received.
		profile.apply_code_font_to(workbench);
		process_events();

		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "the semantic navigation bridge selects a retained change");
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

		// Naming the tightest pair in each profile, and ratcheting it.
		//
		// BPR-0204 recorded the gap this closes. The selection colour the owner
		// chose landed at 4.51:1 against a 4.50 minimum -- a hundredth of a
		// point -- and nothing said so. A palette edit that ate that hundredth
		// would have failed only when it finally crossed, with no warning that
		// the margin had been spent, and whoever made it would have had no
		// reason to connect their change to a role they never touched.
		//
		// The figures below are what the shipped palettes have, not what they
		// ought to have. Buying more would mean moving colours the product
		// owner accepted at attended review, so this is a ratchet: an edit that
		// spends the remaining margin fails here and has to lower a number
		// deliberately, which is the decision that used to be invisible.
		//
		// light still has *none*: its comment-on-removed-emphasis pair sits
		// exactly on 7.00:1, so any erosion there is a failure rather than a
		// warning. BPR-0210 asked what margin high_contrast_light could
		// achieve; the answer arrived sideways. The emphasis rework moved
		// mark readability onto derived fills with guaranteed foregrounds,
		// which retired the pair that card measured at zero, and the pairs
		// that bind both high-contrast profiles today - syntax text over the
		// moved band - hold 18 and 4 hundredths. Those figures were regained,
		// not granted, so they are recorded here at their measured values
		// rather than left at zero for anyone to spend silently.
		{
			struct tightest_pair
			{
				core::appearance_variant Variant;
				std::uint32_t            HeadroomHundredths;
			};
			// These figures are near zero on purpose. Every emphasis fill in
			// every profile is now the exact colour a product owner selected
			// or approved from rendered captures, and each one was pushed as
			// far as its floor allows -- that is what "strong enough to see"
			// turned out to mean. The tightest pair therefore sits on its
			// floor by construction rather than by neglect.
			//
			// Dark drops from 5 to 1 with BPR-0323's stronger green, which is
			// the last of those pushes. Any later change to any emphasis fill
			// trips this guard immediately, which is the intended effect: the
			// numbers came from verdicts, so moving them needs another one.
			constexpr auto recorded = std::array{
				tightest_pair{ core::appearance_variant::light, 0U },
				tightest_pair{ core::appearance_variant::dark, 1U },
				tightest_pair{ core::appearance_variant::high_contrast_light, 18U },
				tightest_pair{ core::appearance_variant::high_contrast_dark, 4U },
			};
			auto eroded = std::size_t{ 0U };
			for (const auto& expectation : recorded) {
				const auto& palette = qt::builtin_appearance_palette(expectation.Variant);
				const auto  findings
					= core::measure_required_contrast(palette, expectation.Variant);
				const auto tightest = std::ranges::min_element(
					findings,
					{ },
					[](const core::contrast_finding& aFinding) {
						return aFinding.RatioHundredths - aFinding.MinimumHundredths;
					});
				if (tightest == findings.end()) {
					continue;
				}
				const auto headroom
					= tightest->RatioHundredths - tightest->MinimumHundredths;
				std::cerr << "  variant " << static_cast<int>(expectation.Variant)
						  << " tightest pair: foreground role "
						  << static_cast<int>(tightest->Pair.Foreground)
						  << " on background role "
						  << static_cast<int>(tightest->Pair.Background)
						  << (tightest->Pair.Wash ? " through the selection wash" : "")
						  << ", headroom " << headroom << " hundredths (recorded "
						  << expectation.HeadroomHundredths << ")\n";
				if (headroom < expectation.HeadroomHundredths) {
					++eroded;
				}
			}
			expect(
				eroded == 0U,
				"no profile's tightest checked pair has less headroom than the "
				"figure recorded for it, so spending the remaining margin is a "
				"deliberate edit rather than a surprise");
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
		// Inequality is not the requirement and never was. Before BPR-0320 the
		// light profile's removed emphasis sat 4.2 units off its row and this
		// check passed, because #FFEBE9 and #FFE2DF are different numbers. The
		// core publishes the distance a mark has to keep from the fill it
		// marks; this asks the palettes for that distance instead.
		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);

			for (const auto& finding : core::failing_emphasis_separation(palette)) {
				std::cerr << "FAILING EMPHASIS: variant "
						  << static_cast<int>(variant) << " emphasis role "
						  << static_cast<int>(core::index_of(finding.Pair.Emphasis))
						  << " measured " << finding.SeparationTenths << " needs "
						  << finding.MinimumTenths << '\n';
			}
			expect(
				core::failing_emphasis_separation(palette).empty(),
				"every intraline emphasis stays visible on the row it marks");
		}
	}

	// BPR-0323: a selected row's mark is a deeper shade of the band that row
	// is wearing, because a palette colour there kept its own hue and read as
	// foreign. The mark still has to be visible and the code on it still has
	// to be readable, and neither follows from the unselected measurements.
	void test_a_selected_rows_mark_stays_visible_and_readable()
	{
		constexpr auto bands = std::array{
			role::diff_added_background,
			role::diff_removed_background,
		};

		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			const auto  wash    = palette[core::index_of(role::code_selection_background)];
			const auto  floor   = core::minimum_contrast_hundredths(
				core::contrast_kind::emphasis_text, variant);

			for (const auto bandRole : bands) {
				const auto washedBand = core::composited(
					wash,
					palette[core::index_of(bandRole)],
					core::selection_wash_alpha);
				const auto mark = core::derived_emphasis_fill(washedBand, variant);

				expect(
					core::color_difference_tenths(
						washedBand, mark, core::color_vision::typical)
						>= core::derived_emphasis_separation_minimum_tenths,
					"a selected row's mark stands clear of the band it sits in");

				// Whatever is painted on the mark has to clear the floor:
				// either the profile's own syntax colours, or the foreground
				// the mark carries when they cannot.
				// Whatever is painted on the mark has to clear the floor:
				// either the profile's own syntax colours, or the foreground
				// the mark carries when they cannot.
				const auto readable = std::ranges::all_of(
					std::views::iota(
						core::index_of(role::syntax_normal),
						core::appearance_role_count),
					[&palette, mark, floor](const std::size_t aIndex) {
						return core::contrast_ratio_hundredths(
								   palette[aIndex], mark)
							>= floor;
					});
				expect(
					readable
						|| core::contrast_ratio_hundredths(
							   core::emphasis_foreground(mark), mark)
							>= floor,
					"a selected row's mark is readable, by its syntax colours "
					"or by the foreground it carries");
			}
		}
	}

	// A palette can clear the floor on both sides and still tell the reader
	// that one pane matters more, which is the defect the product owner
	// actually reported: the added span was obvious and the removed span was
	// not there at all.
	void test_intraline_emphasis_is_balanced_across_both_sides()
	{
		for (const auto variant : all_variants) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			const auto  balance = core::measure_emphasis_balance(palette);

			if (!balance.Meets) {
				std::cerr << "UNBALANCED EMPHASIS: variant "
						  << static_cast<int>(variant) << " strongest "
						  << balance.StrongestTenths << " weakest "
						  << balance.WeakestTenths << " ratio "
						  << balance.RatioHundredths << " maximum "
						  << balance.MaximumHundredths << '\n';
			}
			expect(
				balance.Meets,
				"neither intraline side is disproportionately stronger");
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
		// BPR-0935's owner-requested quieter dark fills preserve syntax ink.
		// The exact edit span, unified +/- prefix and earlier/later pane labels
		// must now carry this distinction under red/green colour deficiency.
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
		// The high-contrast dark intraline pair was recorded here between
		// BPR-0320 and BPR-0322. The accepted bright pairing discharged it:
		// #55FF55 and #FF9999 stay 36.4 apart under the worst colour-vision
		// model against a floor of 10.0. The obligation is gone because a
		// product owner picked colours that removed it, not because the check
		// was relaxed.
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

	// [[BPR-0776]]. Home's state colours are the reason word's ink, so each reads
	// as small text on the base and the surface of every built-in variant.
	void test_attention_colours_read_as_text_in_every_variant()
	{
		const auto tones = std::array{
			qt::attention_tone::needs_me,
			qt::attention_tone::waiting_on_others,
			qt::attention_tone::following,
		};
		for (const auto variant : { core::appearance_variant::light,
									core::appearance_variant::dark,
									core::appearance_variant::high_contrast_light,
									core::appearance_variant::high_contrast_dark }) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			const auto  base    = palette[core::index_of(role::code_background)];
			const auto  surface = palette[core::index_of(role::application_surface)];
			auto        seen    = std::vector<QColor>{ };
			for (const auto tone : tones) {
				const auto colour    = qt::builtin_attention_color(variant, tone);
				const auto value     = qt::to_appearance_color(colour);
				const auto onBase    = qt::contrast_ratio(value, base);
				const auto onSurface = qt::contrast_ratio(value, surface);
				std::cout << "attention-contrast variant=" << static_cast<int>(variant)
						  << " tone=" << static_cast<int>(tone) << " colour=" << colour.name().toStdString()
						  << " base=" << onBase << " surface=" << onSurface << '\n';
				expect(colour.isValid() && colour.alpha() == 255, "every Home state colour is opaque");
				expect(onBase >= 4.5 && onSurface >= 4.5,
					   "every Home state colour reads as small text on its variant's base and surface");
				expect(std::ranges::find(seen, colour) == seen.end(), "the three states never share a colour");
				seen.push_back(colour);
			}
		}
		const auto profile = qt::appearance_profile{ };
		for (const auto tone : tones) {
			expect(profile.attention_color(tone) == qt::builtin_attention_color(profile.variant(), tone),
				   "a profile's Home state colour is its resolved variant's");
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_fresh_installation_defaults_to_system();
	test_builtin_profiles_are_total();
	test_the_dark_palette_follows_the_visual_assist_scheme();
	test_high_contrast_profiles_are_not_copies();
	test_derived_text_stays_readable_on_every_fill();
	test_the_core_decides_which_variant_applies();
	test_applies_semantic_colors_without_forcing_style_or_stylesheet();
	test_navigation_icons_follow_every_profile_palette();
	test_immediate_preview_updates_without_restart();
	test_customisation_round_trips_through_the_durable_record();
	test_a_stored_palette_customises_exactly_one_variant();
	test_a_palette_without_label_tints_reads_the_builtin_tints();
	test_a_record_adopted_with_its_variant_does_not_colour_another();
	test_editing_a_colour_keeps_every_other_theme();
	test_customised_counts_and_palettes_are_per_theme();
	test_total_palette_preview_cancel_and_adoption_are_exact();
	test_typography_defaults_are_legal_and_native();
	test_fonts_are_built_from_the_core_record();
	test_every_typography_field_previews_immediately();
	test_a_value_outside_the_published_bounds_is_never_applied();
	test_every_role_converts_through_presentation_colour();
	test_rendered_connectors_clear_the_non_text_floor_in_every_profile();
	test_a_font_change_moves_geometry_and_not_semantics();
	test_a_tab_width_change_alone_invalidates_measured_geometry();
	test_intraline_emphasis_never_collapses_onto_its_row_fill();
	test_intraline_emphasis_is_balanced_across_both_sides();
	test_a_selected_rows_mark_stays_visible_and_readable();
	test_hue_critical_pairs_are_measured_for_every_builtin_profile();
	test_every_builtin_profile_passes_the_published_contrast_audit();
	test_attention_colours_read_as_text_in_every_variant();

	std::cout << "appearance-profile-tests:pass\n";
	return EXIT_SUCCESS;
}
