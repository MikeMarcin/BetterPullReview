#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"

#include <QColor>
#include <QPalette>
#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QWidget;

namespace bpr::qt {
	/// \brief Converts one core appearance colour to its Qt colour.
	[[nodiscard]] auto to_presentation_color(core::appearance_color aColor) noexcept
		-> QColor;

	/// \brief Converts one Qt colour back to a core appearance colour.
	[[nodiscard]] auto to_appearance_color(const QColor& aColor) noexcept
		-> core::appearance_color;

	/// \brief Returns the WCAG relative luminance of one appearance colour.
	///
	/// \details The result is in the closed range 0.0 to 1.0. Alpha takes no
	/// part: a semantic role is composited by whatever draws it, and guessing a
	/// backdrop here would report a contrast nothing on screen has.
	[[nodiscard]] auto relative_luminance(core::appearance_color aColor) noexcept
		-> double;

	/// \brief Returns the WCAG contrast ratio between two appearance colours.
	///
	/// \returns A ratio in the closed range 1.0 to 21.0, independent of argument
	/// order.
	[[nodiscard]] auto contrast_ratio(
		core::appearance_color aFirst,
		core::appearance_color aSecond) noexcept -> double;

	/// \brief Returns the built-in profile palette of one resolved variant.
	///
	/// \details The four built-in profiles are BetterPullReview Light, Dark,
	/// High Contrast Light, and High Contrast Dark. Each is total over the core's
	/// closed role vocabulary and fully opaque; a role that carried no colour
	/// would be transparent and is rejected when the profile is built.
	[[nodiscard]] auto builtin_appearance_palette(
		core::appearance_variant aVariant) noexcept
		-> const core::appearance_palette&;

	/// \brief Returns the presentation name of one built-in profile.
	[[nodiscard]] auto builtin_appearance_profile_name(
		core::appearance_variant aVariant) -> QString;

	/// \brief Returns the appearance settings a fresh installation starts from.
	///
	/// \details A fresh installation has no durable record, so the mode is the
	/// application default and is deliberately not restated here: restating it
	/// would move the fresh-install default out of the durable contract and into
	/// presentation. Only the palette is supplied, because the host chooses no
	/// colour and its composition must hand it one.
	[[nodiscard]] auto default_appearance_settings()
		-> application::appearance_settings;

	/// \brief Describes the appearance profile statistics record.
	///
	/// \details `PresentationChanges` counts published changes of the applied
	/// appearance, and `NativeRefreshes` counts republications caused by a
	/// native palette, font, style, or theme change, which alter no semantic
	/// colour. `AppliedSurfaces` counts how many times a surface adopted the
	/// derived palette.
	struct appearance_profile_statistics
	{
		std::uint64_t PresentationChanges = 0U;
		std::uint64_t NativeRefreshes     = 0U;
		std::uint64_t AppliedSurfaces     = 0U;
	};

	/// \brief Receives one published appearance presentation change.
	using appearance_presentation_sink = std::function<void()>;

	/// \brief Applies one resolved appearance profile to Qt presentation.
	///
	/// \details The core decides which variant applies: every resolution here
	/// comes from `core::resolve_appearance` with the selected mode and the
	/// supplied observation, and this class contains no rule of its own about
	/// what a mode means. What it owns is presentation: the colour each semantic
	/// role carries, the derived roles the closed vocabulary deliberately omits,
	/// and the widget palette a surface adopts.
	///
	/// It supplies semantic colours only. It installs no style, adds no style
	/// sheet, and never writes an application-wide palette or font, so the
	/// application keeps inheriting the native style and font. `derive` starts
	/// from the palette a surface already inherited and replaces only the roles
	/// this profile owns, which leaves button, tool tip, and every other native
	/// role inherited.
	///
	/// Every mutator answers whether the applied presentation changed, so a
	/// surface re-derives on a real change and not on a repeated selection.
	class appearance_profile final
	{
	public:
		explicit appearance_profile(
			application::appearance_settings aSettings    = default_appearance_settings(),
			core::appearance_observation     aObservation = { });

		/// \brief Installs the sink notified of each published change.
		void set_sink(appearance_presentation_sink aSink);

		[[nodiscard]] auto mode() const noexcept -> core::appearance_mode;
		[[nodiscard]] auto observation() const noexcept
			-> core::appearance_observation;
		[[nodiscard]] auto resolution() const noexcept
			-> core::appearance_resolution;
		[[nodiscard]] auto variant() const noexcept -> core::appearance_variant;
		[[nodiscard]] auto profile_name() const -> QString;
		[[nodiscard]] auto palette() const noexcept
			-> const core::appearance_palette&;
		[[nodiscard]] auto color(core::appearance_role aRole) const noexcept
			-> QColor;
		[[nodiscard]] auto customised(core::appearance_role aRole) const noexcept
			-> bool;
		[[nodiscard]] auto customised_roles() const noexcept -> std::size_t;
		[[nodiscard]] auto settings() const -> application::appearance_settings;
		[[nodiscard]] auto generation() const noexcept -> std::uint64_t;
		[[nodiscard]] auto statistics() const noexcept
			-> appearance_profile_statistics;

		/// \brief Selects one appearance mode.
		/// \returns Whether the applied presentation changed.
		auto select_mode(core::appearance_mode aMode) -> bool;

		/// \brief Adopts one bounded operating-system observation.
		///
		/// \details The observation is a value the host supplies. This class
		/// neither reads operating-system state nor decides what an observation
		/// means; it hands the observation and the selected mode to the core.
		///
		/// \returns Whether the applied presentation changed.
		auto observe(core::appearance_observation aObservation) -> bool;

		/// \brief Customises one semantic colour.
		/// \returns Whether the applied presentation changed.
		auto customise(core::appearance_role aRole, core::appearance_color aColor)
			-> bool;

		/// \brief Restores one semantic colour to the built-in profile.
		/// \returns Whether the applied presentation changed.
		auto restore(core::appearance_role aRole) -> bool;

		/// \brief Restores every semantic colour to the built-in profile.
		/// \returns Whether the applied presentation changed.
		auto restore_all() -> bool;

		/// \brief Republishes after a native palette, font, style, or theme change.
		///
		/// \details A native change cannot alter a semantic colour, so nothing is
		/// re-decided. It does change the roles a surface inherits, so the
		/// publication exists to make every surface derive its palette again.
		void refresh_native_appearance();

		/// \brief Returns the derived text colour for one background role.
		///
		/// \details The closed vocabulary names fills for diff rows, review
		/// states, and the selection without naming a text colour for each,
		/// because the readable choice follows from the profile. This picks
		/// whichever of the profile's normal code text and code background
		/// contrasts more against the named background.
		[[nodiscard]] auto readable_text(core::appearance_role aBackground) const
			noexcept -> QColor;

		/// \brief Derives one surface palette from the palette it inherited.
		///
		/// \details Only the roles this profile owns are replaced. Every other
		/// role, including the button and tool tip roles native styling paints,
		/// stays exactly as inherited. Disabled text derives from the profile's
		/// own text and fill rather than from the inherited palette, so a dark
		/// profile cannot inherit a light disabled colour.
		[[nodiscard]] auto derive(const QPalette& aInherited) const -> QPalette;

		/// \brief Applies the derived palette to one surface and its children.
		///
		/// \details This sets a widget-local palette. It writes no application
		/// palette, installs no style, and adds no style sheet.
		void apply_to(QWidget& aSurface);

	private:
		void rebuild();
		auto adopt() -> bool;

		core::appearance_mode        Mode = core::appearance_mode::system;
		core::appearance_observation Observation;
		core::typography_settings    Typography;
		std::array<std::optional<core::appearance_color>, core::appearance_role_count>
														Overrides;
		core::appearance_resolution                     Resolution;
		core::appearance_palette                        Palette{ };
		std::array<QColor, core::appearance_role_count> Colors;
		appearance_presentation_sink                    Sink;
		appearance_profile_statistics                   Statistics;
		std::uint64_t                                   Generation = 0U;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP
