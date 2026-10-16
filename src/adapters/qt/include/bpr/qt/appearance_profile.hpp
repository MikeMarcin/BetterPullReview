#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QtTypes>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QWidget;

namespace bpr::qt {
	/// \brief Announces the code typography one surface has just had applied.
	///
	/// \details Tab width is not a font property, and Qt sends no event when a
	/// surface is given a font equal to the one it already has. A surface that
	/// derived geometry from the tab stop therefore had no way to learn that the
	/// tab stop moved: only a combined change reached it, by way of the font.
	///
	/// Widening what a font change means would have made the reason for the
	/// rebuild untrue, so the applied typography is published in its own right
	/// and a surface that measures text subscribes to it. The event carries the
	/// applied values rather than a bare notification, so a subscriber can tell
	/// which fact moved instead of rebuilding on every application.
	class code_typography_event final : public QEvent
	{
	public:
		[[nodiscard]] static auto registered_type() noexcept -> QEvent::Type;

		explicit code_typography_event(qreal aTabStopDistance);

		[[nodiscard]] auto tab_stop_distance() const noexcept -> qreal;

	private:
		qreal TabStopDistance = 0.0;
	};

	/// \brief Converts one core appearance colour to its Qt colour.
	[[nodiscard]] auto to_presentation_color(core::appearance_color aColor) noexcept
		-> QColor;

	/// \brief Converts one Qt colour back to a core appearance colour.
	[[nodiscard]] auto to_appearance_color(const QColor& aColor) noexcept
		-> core::appearance_color;

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

	/// \brief Gives every label tint a palette left unassigned its built-in value.
	///
	/// \details [[BPR-1083]]. A stored palette or theme document written before
	/// the twelve label tints existed imports them as
	/// `core::unassigned_appearance_color` (`core::defaults_on_import`). Shown
	/// or previewed, they read as the built-in tints of `aVariant`, and no other
	/// role changes. A palette with nothing unassigned is returned unchanged.
	/// The attention amber, `state_attention` ([[BPR-1085]]), and the namespace
	/// and enum-member roles (RFC-BPR-0072) are read the same way. An imported
	/// theme never needs it: the host fills those roles from the theme's own
	/// base before previewing it ([[BPR-1479]]).
	[[nodiscard]] auto with_builtin_label_tints(
		core::appearance_palette aPalette,
		core::appearance_variant aVariant) noexcept -> core::appearance_palette;

	/// \brief Returns the four built-in profile palettes in variant order.
	///
	/// \details [[BPR-1479]]. What the composition gives the appearance host,
	/// which owns no colour, to fill the roles an older theme omits from that
	/// theme's base.
	[[nodiscard]] auto builtin_appearance_palettes() noexcept
		-> application::appearance_builtin_palettes;

	/// \brief Returns the presentation name of one built-in profile.
	[[nodiscard]] auto builtin_appearance_profile_name(
		core::appearance_variant aVariant) -> QString;

	/// \brief [[BPR-0776]]. Home's three states: what needs me, what waits on
	/// others, and what I follow; and [[BPR-1528]] the section of my own pull
	/// requests.
	///
	/// \details My pull requests is a section, not a state: its rows keep their
	/// own state colours, and its tone is only the panel's header and rail
	/// (owner, 2026-10-15).
	enum class attention_tone : std::uint8_t {
		needs_me,
		waiting_on_others,
		following,
		my_pull_requests,
	};

	/// \brief Returns one Home section colour at text strength for one built-in variant.
	///
	/// \details A derived role the closed vocabulary deliberately omits. Home's
	/// sections present the core's attention grouping rather than a review-ledger
	/// state, so the colours are neither customisable nor stored. Each reads as
	/// small text at 4.5:1 or better against its variant's code background and
	/// application surface.
	[[nodiscard]] auto builtin_attention_color(
		core::appearance_variant aVariant,
		attention_tone           aTone) noexcept -> QColor;

	/// \brief Builds one Qt font from the core's UI font record.
	///
	/// \details The record is the only source of family, size, and weight. This
	/// invents no value and corrects none: `core::validate_typography` is the
	/// authority on legality, and `appearance_profile` applies no record that
	/// authority rejects, so an illegal record handed straight to this function
	/// produces whatever Qt makes of it rather than a silently repaired font.
	[[nodiscard]] auto to_presentation_font(const core::font_settings& aFont)
		-> QFont;

	/// \brief Builds one Qt code font from the core's code font record.
	///
	/// \details The code record adds the ligature switch, which is applied as
	/// the standard and contextual OpenType ligature features so that turning
	/// ligatures off reaches any family that ships them. Tab width and the two
	/// syntax emphasis switches are not font attributes and are applied where
	/// they belong: `tab_stop_distance_of` and `code_font_for`.
	[[nodiscard]] auto to_presentation_font(const core::code_font_settings& aCode)
		-> QFont;

	/// \brief Returns the line height one UI font record asks for.
	///
	/// \details The result is the font's own line spacing scaled by the record's
	/// percentage, in device-independent pixels.
	///
	/// This takes a `ui_font_settings` and not a `font_settings` because only
	/// the UI font has a line height. The code font's was retired by BPR-0118:
	/// `QPlainTextDocumentLayout` ignores every line-height mode Qt offers, and
	/// block margins with it, so the number could be computed and never applied.
	[[nodiscard]] auto line_height_of(const core::ui_font_settings& aFont) -> qreal;

	/// \brief Returns the tab stop distance one code font record asks for.
	///
	/// \details A tab advances by the record's tab width in space widths of the
	/// code font, so the distance moves whenever either the width or the font
	/// does.
	[[nodiscard]] auto tab_stop_distance_of(const core::code_font_settings& aCode)
		-> qreal;

	/// \brief [[BPR-1336]]. The code record as the comparison shows it.
	///
	/// \details The code size replaced by `core::zoomed_point_size` and the
	/// zoom spent, so building a font or a tab stop from the result gives the
	/// comparison's own. Every other field is the record's.
	[[nodiscard]] auto comparison_code_settings(const core::code_font_settings& aCode)
		-> core::code_font_settings;

	/// \brief Returns the typography a fresh installation starts from.
	///
	/// \details The core holds no default family because it cannot know one, so
	/// a default-constructed core record is deliberately illegal. The UI family
	/// is the platform's own general system font. The code family is the
	/// bundled JetBrains Mono NL where this process's font database
	/// resolves it ([[BPR-1270]]), and the platform's fixed-pitch system font
	/// where it does not. Every numeric value is placed inside
	/// `core::bounds_of` for its field rather than against a second copy of
	/// those bounds, so the composed record cannot be one the core would reject.
	///
	/// A stored record is never measured against this: the store cannot tell a
	/// chosen family from one written beside some other change, so whatever it
	/// holds stays the reviewer's.
	[[nodiscard]] auto default_typography_settings() -> core::typography_settings;

	/// \brief The same, with the bundled family's availability stated.
	///
	/// \details `false` is the fallback a failed registration leads to: the
	/// platform's fixed-pitch system font.
	[[nodiscard]] auto default_typography_settings(bool aBundledCodeFont)
		-> core::typography_settings;

	/// \brief Returns the appearance settings a fresh installation starts from.
	///
	/// \details A fresh installation has no durable record, so the mode is the
	/// application default and is deliberately not restated here: restating it
	/// would move the fresh-install default out of the durable contract and into
	/// presentation. The palette and the typography are supplied, because the
	/// host chooses neither a colour nor a font family and its composition must
	/// hand it both.
	[[nodiscard]] auto default_appearance_settings()
		-> application::appearance_settings;

	/// \brief Describes the appearance profile statistics record.
	///
	/// \details `PresentationChanges` counts published changes of the applied
	/// appearance, and `NativeRefreshes` counts republications caused by a
	/// native palette, font, style, or theme change, which alter no semantic
	/// colour. `TypographyChanges` names how many of the published changes were
	/// typography, and `RejectedTypography` counts offers the core refused,
	/// which publish nothing. `AppliedSurfaces` counts how many times a surface
	/// adopted the derived palette and `AppliedFontSurfaces` how many times one
	/// adopted a font.
	struct appearance_profile_statistics
	{
		std::uint64_t PresentationChanges = 0U;
		std::uint64_t NativeRefreshes     = 0U;
		std::uint64_t TypographyChanges   = 0U;
		std::uint64_t RejectedTypography  = 0U;
		std::uint64_t AppliedSurfaces     = 0U;
		std::uint64_t AppliedFontSurfaces = 0U;
	};

	/// \brief Reports what one offered typography selection did.
	///
	/// \details `Rejections` is empty exactly when the offer was legal, and
	/// repeats the core's own typed triples in the core's own order. An offer
	/// carrying even one rejection applies nothing at all: no field is clamped
	/// and no field is partially adopted, because a clamped value would tell a
	/// settings surface that its rejected value was accepted. `Changed` is true
	/// only when a legal offer differed from the applied typography.
	struct typography_selection
	{
		bool                                    Changed = false;
		std::vector<core::typography_rejection> Rejections;

		[[nodiscard]] auto accepted() const noexcept -> bool
		{
			return Rejections.empty();
		}
	};

	/// \brief Receives one published appearance presentation change.
	using appearance_presentation_sink = std::function<void()>;

	/// \brief Says what one host palette adoption means for an active preview.
	enum class palette_preview_adoption : std::uint8_t {
		retain,
		commit,
	};

	/// \brief Applies one resolved appearance profile to Qt presentation.
	///
	/// \details The core decides which variant applies: every resolution here
	/// comes from `core::resolve_appearance` with the selected mode and the
	/// supplied observation, and this class contains no rule of its own about
	/// what a mode means. What it owns is presentation: the colour each semantic
	/// role carries, the derived roles the closed vocabulary deliberately omits,
	/// the widget palette a surface adopts, and the two fonts it builds from the
	/// core's typography record. The core decides which typography values are
	/// legal, exactly as it decides which variant applies; this class applies
	/// only what that authority accepted.
	///
	/// It supplies semantic colours and widget-local fonts. It installs no
	/// style, adds no style sheet, and never writes an application-wide palette
	/// or font, so the application keeps inheriting the native style and font
	/// wherever a surface has not been handed one. `derive` starts from the
	/// palette a surface already inherited and replaces only the roles this
	/// profile owns, which leaves button, tool tip, and every other native role
	/// inherited.
	///
	/// A typography change is presentation-only by construction: it moves the
	/// font, the tab stop, and every measurement derived from them, and it
	/// carries no semantic identity, mapping, or selection that it could move.
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
		/// \brief [[BPR-0776]]. Home's state colour for the resolved variant.
		[[nodiscard]] auto attention_color(attention_tone aTone) const noexcept
			-> QColor;
		[[nodiscard]] auto customised(core::appearance_role aRole) const noexcept
			-> bool;
		[[nodiscard]] auto customised_roles() const noexcept -> std::size_t;
		/// \brief Returns how many roles one named theme customises.
		///
		/// \details RFC-BPR-0034 captions each theme tile with this number, so it
		/// is answered for every variant rather than only the resolved one. It is
		/// the same comparison the resolved-variant overload makes: a role counts
		/// when its override differs from *that* variant's built-in colour. The
		/// count lives here because the built-in profiles do: the host holds no
		/// built-in palette and the core holds no default theme, so neither can say
		/// what a colour differs from. A variant outside the closed vocabulary
		/// customises nothing.
		[[nodiscard]] auto customised_roles(
			core::appearance_variant aVariant) const noexcept -> std::size_t;

		/// \brief Returns one theme's built-in palette with its own overrides applied.
		///
		/// \details What the durable record holds for that variant, and what an
		/// export of that theme writes. It differs from `palette()` whenever the
		/// desktop resolves a different variant, and it has to: exporting or
		/// storing the rendered palette while another theme was on screen would
		/// write that theme's colours over this one's.
		[[nodiscard]] auto palette_for(core::appearance_variant aVariant) const
			-> core::appearance_palette;
		[[nodiscard]] auto settings() const -> application::appearance_settings;
		[[nodiscard]] auto generation() const noexcept -> std::uint64_t;
		[[nodiscard]] auto statistics() const noexcept
			-> appearance_profile_statistics;

		[[nodiscard]] auto typography() const noexcept
			-> const core::typography_settings&;
		[[nodiscard]] auto ui_font() const -> QFont;
		[[nodiscard]] auto code_font() const -> QFont;
		/// \brief [[BPR-1336]]. The code font at the comparison's zoom.
		[[nodiscard]] auto comparison_code_font() const -> QFont;
		/// \brief [[BPR-1336]]. The tab stop at the comparison's zoom.
		[[nodiscard]] auto comparison_tab_stop_distance() const -> qreal;

		/// \brief Returns the code font one syntax category asks to emphasise.
		///
		/// \details Bold and italic are user switches over the whole of syntax
		/// presentation, so a category that asks for either gets it only when
		/// the corresponding switch is on. A highlighter therefore states what a
		/// token means and never has to know whether emphasis is wanted.
		[[nodiscard]] auto code_font_for(bool aBold, bool aItalic) const -> QFont;

		[[nodiscard]] auto ui_line_height() const -> qreal;
		[[nodiscard]] auto code_tab_stop_distance() const -> qreal;

		/// \brief Offers one complete typography record.
		///
		/// \returns The core's verdict, and whether the applied presentation
		/// changed.
		auto select_typography(core::typography_settings aTypography)
			-> typography_selection;

		/// \brief Offers one UI font record, keeping the applied code font.
		///
		/// \details The retained typography is always legal, so every rejection
		/// the result carries names the offered UI font.
		///
		/// \returns The core's verdict, and whether the applied presentation
		/// changed.
		auto select_ui_font(core::ui_font_settings aFont) -> typography_selection;

		/// \brief Offers one code font record, keeping the applied UI font.
		///
		/// \details The retained typography is always legal, so every rejection
		/// the result carries names the offered code font.
		///
		/// \returns The core's verdict, and whether the applied presentation
		/// changed.
		auto select_code_font(core::code_font_settings aFont)
			-> typography_selection;

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

		/// \brief Applies one total palette to presentation without committing it.
		///
		/// \details The palette remains core-owned semantic data. Preview changes
		/// only the visible palette and never changes the settings returned for
		/// persistence.
		auto preview_palette(core::appearance_palette aPalette) -> bool;
		/// \brief Restores the exact palette last adopted from the host.
		auto cancel_palette_preview() -> bool;
		/// \brief Adopts one host-accepted total palette as the committed palette.
		///
		/// \details A committed preview clears the candidate before publishing.
		/// An unrelated mode, typography, or native-observation publication can
		/// instead re-express the committed palette against another built-in
		/// variant while retaining the exact visible, cancellable preview.
		///
		/// `aVariant` names the theme the palette customises and is required.
		/// It used to be optional, and an omitted variant meant "resolve one
		/// from the mode and the observation" — a guess this class is not
		/// entitled to make and which RFC-BPR-0034 prohibits: by the time an
		/// adoption arrives the desktop may have flipped, and the colours would
		/// be banked against a theme nobody was looking at. A durable record
		/// names its variant and a gesture is scoped to the resolved one by the
		/// composition, so there is no caller who does not know.
		auto adopt_palette(
			core::appearance_palette aPalette,
			palette_preview_adoption aPreview,
			core::appearance_variant aVariant) -> bool;

		/// \brief Adopts the whole per-variant customization table of one record.
		///
		/// \details The durable shape, adopted whole: each variant's overrides are
		/// rebuilt from its own slot against its own built-in profile, so a light
		/// customization cannot become the dark profile's overrides and a variant
		/// whose slot carries no customization renders its built-in palette
		/// untouched. This replaces the pair of calls a single-palette record
		/// needed, where "no stored palette" had to mean "restore everything".
		auto adopt_palettes(
			const application::appearance_palette_customisations& aPalettes,
			palette_preview_adoption                              aPreview) -> bool;
		[[nodiscard]] auto previewing_palette() const noexcept -> bool;

		/// \brief Restores one semantic colour to the built-in profile.
		/// \returns Whether the applied presentation changed.
		auto restore(core::appearance_role aRole) -> bool;

		/// \brief Restores one theme's every semantic colour to its built-in profile.
		///
		/// \details Scoped to a variant because RFC-BPR-0034's Reset all resets the
		/// theme on screen and no other. The other themes' colours are not part of
		/// that gesture and survive it.
		///
		/// \returns Whether the applied presentation changed.
		auto restore_all(core::appearance_variant aVariant) -> bool;

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

		/// \brief Applies the UI font to one surface and its children.
		///
		/// \details This sets a widget-local font and writes no application
		/// font, so a surface nobody hands a font to keeps inheriting the
		/// native one.
		void apply_ui_font_to(QWidget& aSurface);

		/// \brief Applies the code font and tab stop to one code surface.
		///
		/// \details The font reaches the surface and every plain-text pane
		/// beneath it, and each of those panes also receives the tab stop
		/// distance the code record asks for. The tab stop is applied before the
		/// font so that the font change Qt reports to the surface is the last
		/// geometry-affecting fact of the whole application.
		///
		/// Applying a font is a Qt-local geometry fact. It moves measured
		/// layout and every geometry a surface derives from it; it moves no
		/// semantic identity, mapping, pairing confidence, or selection,
		/// because none of those is reachable from here.
		void apply_code_font_to(QWidget& aSurface);

		/// \brief [[BPR-1336]]. Applies the code font at the comparison's zoom.
		///
		/// \details What `apply_code_font_to` does, with the zoomed font and
		/// tab stop: the comparison is the one surface the View menu's zoom
		/// scales. Drawers, Overview and composers keep the code size.
		void apply_comparison_code_font_to(QWidget& aSurface);

	private:
		void apply_code_font(QWidget& aSurface, const QFont& aFont, qreal aTabStopDistance);
		void adopt_record(const application::appearance_settings& aSettings);
		void rebuild();
		void rebuild_typography();
		void set_committed_palette(
			const core::appearance_palette& aPalette,
			core::appearance_variant        aVariant);
		auto adopt() -> bool;
		auto publish() -> bool;

		/// \brief One variant's overrides over its own built-in profile.
		using variant_overrides = std::array<
			std::optional<core::appearance_color>,
			core::appearance_role_count>;

		core::appearance_mode        Mode = core::appearance_mode::system;
		core::appearance_observation Observation;
		core::typography_settings    Typography;
		QFont                        UiFont;
		QFont                        CodeFont;
		QFont                        ComparisonFont;
		/// \brief One override table per variant, indexed by variant ordinal.
		///
		/// \details RFC-BPR-0034. An override is a colour chosen against one
		/// built-in profile: it is a statement about that profile and means nothing
		/// against another. Keying the table by variant is what lets Light and Dark
		/// each keep their own colours, and what makes it impossible for a light
		/// customisation to colour the dark profile — the single table this replaced
		/// had to be tagged with one variant and discarded whenever the reviewer
		/// edited another.
		std::array<variant_overrides, core::appearance_variant_count> Overrides;
		core::appearance_resolution                                   Resolution;
		core::appearance_palette                                      CommittedPalette{ };
		std::optional<core::appearance_palette>                       PreviewPalette;
		core::appearance_palette                                      Palette{ };
		std::array<QColor, core::appearance_role_count>               Colors;
		appearance_presentation_sink                                  Sink;
		appearance_profile_statistics                                 Statistics;
		std::uint64_t                                                 Generation = 0U;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_PROFILE_HPP
