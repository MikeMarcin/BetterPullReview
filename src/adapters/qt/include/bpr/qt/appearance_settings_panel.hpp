#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/colour_role_list.hpp"
#include "bpr/qt/theme_preview_strip.hpp"

#include <QFont>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace bpr::qt {
	class appearance_profile;
	class type_preview;

	/// \brief Records directly observable settings-surface activity.
	struct appearance_settings_panel_statistics
	{
		std::size_t Adoptions        = 0U;
		std::size_t Rejections       = 0U;
		std::size_t ModeGestures     = 0U;
		std::size_t FontGestures     = 0U;
		std::size_t PalettePreviews  = 0U;
		std::size_t PaletteCommits   = 0U;
		std::size_t PaletteCancels   = 0U;
		std::size_t ThemeImports     = 0U;
		std::size_t SuppressedEchoes = 0U;
		/// [[BPR-1714]]. Known-namespace lists offered to the host, and
		/// lists refused before or after offering, each naming why.
		std::size_t NamespaceGestures = 0U;
		std::size_t NamespaceRefusals = 0U;

		auto operator<=>(const appearance_settings_panel_statistics&) const
			= default;
	};

	/// \brief RFC-BPR-0034's Appearance page: Theme, Type, Colours, Syntax and Dif.
	///
	/// \details [[BPR-0790]]. The lede, then cards. Each card's header carries its
	/// name and, at the right, its Reset; a card body is rows in the shared row
	/// language of RFC-BPR-0032 as this RFC amends it — one line each, of label, an
	/// info glyph whose popover carries the consequence, and the control. The
	/// consequence is also the glyph's and the control's accessible description, so
	/// it is never reachable by pointer only.
	///
	/// Theme is the six-tile strip in its inset well: the tiles are the control, and
	/// the six-way mode combo box that repeated them is gone. Type is eleven rows
	/// beside the labelled preview, with sizes in points rather than in the
	/// hundredths the core stores. Colours is the whole 60-role list for the theme on
	/// screen, edited through the platform picker, with one bar at its foot while
	/// anything is previewed. Dif keeps the mascot's visibility, its motion and the
	/// explanation of what the system asked for, which RFC-BPR-0031's B3 requires to
	/// remain applied, migrated, persisted and resettable from real controls.
	/// Syntax holds the known C++ namespaces ([[BPR-1714]]): one field of
	/// names separated by commas or spaces, `std` until the reviewer changes it,
	/// applied when they press Return or leave the field. The core splits and
	/// judges the text; an entry it refuses is named under the field with the
	/// reason, the text stays for the reviewer to correct, and nothing is offered
	/// until every entry is a namespace name.
	///
	/// Every committed control shows what the host last published through `adopt`,
	/// and every gesture leaves as an existing `application::appearance_intent`;
	/// nothing here applies a font, a colour, or a mode. The sole transient state is
	/// the palette preview, which Keep changes offers to the host and Cancel discards
	/// in favour of the last adopted palette.
	///
	/// Rejections are shown, never repaired. `core::validate_typography` refuses a
	/// record whole and names each violation; `report` displays those reasons and
	/// puts every control back to the last adopted record. Clamping an out-of-range
	/// value into range would tell the reviewer that the value they typed was the
	/// value they got.
	///
	/// Numeric ranges come from `core::bounds_of`, which is the same function the
	/// validator enforces with, so an in-range gesture cannot be one the core would
	/// then reject. Only the family, which the core bounds by byte length rather than
	/// by value, can be refused from this surface — and that path is exercised rather
	/// than assumed.
	class appearance_settings_panel final : public QWidget
	{
	public:
		explicit appearance_settings_panel(
			application::appearance_intent_dispatch aDispatch = { },
			QWidget*                                aParent   = nullptr);

		void set_dispatch(application::appearance_intent_dispatch aDispatch);
		/// \brief Names the theme on screen, as the composition resolved it.
		///
		/// \details Supplied rather than derived. The core decides the variant from
		/// the selected mode and a bounded observation; this surface observes
		/// nothing and must not appear to. RFC-BPR-0034: the Colours card is titled
		/// for the theme on screen and export, import and reset act on it and say
		/// so, and this is what they name.
		void               set_resolved_variant(core::appearance_variant aVariant);
		[[nodiscard]] auto resolved_variant() const noexcept
			-> const std::optional<core::appearance_variant>&;
		/// \brief Publishes how many colours each theme customises, by variant.
		///
		/// \details Computed by the appearance profile, which owns the built-in
		/// palettes a customisation is measured against, and forwarded to the theme
		/// strip that captions each tile with it.
		void set_customised_counts(
			std::array<std::size_t, core::appearance_variant_count> aCounts);
		/// \brief Publishes each theme's palette: its built-in plus its overrides.
		///
		/// \details Forwarded to the theme strip, which paints each tile in that
		/// theme's own colours. The profile owns the built-in palettes and the
		/// per-variant overrides, so the composition is the only place that can say
		/// what a theme looks like; this surface passes the answer on and keeps
		/// none of it.
		void set_theme_palettes(
			std::array<core::appearance_palette, core::appearance_variant_count>
				aPalettes);
		/// \brief Adopts the code font the accepted typography asks for.
		///
		/// \details The tiles paint their snippet in it and the Colours list sets
		/// every value in it, which is what the accepted design asks for. Supplied,
		/// because the profile builds the font from the record the host accepted.
		void set_code_font(QFont aFont);
		/// \brief Adopts everything the applied appearance decides for this page.
		///
		/// \details One call for the facts only the profile can answer: which theme
		/// is on screen, what each theme's palette is, how many colours each one
		/// customises, the code font, and the preview's own repaint. The composition
		/// still publishes the host's accepted record through `adopt`; this is the
		/// presentation half, and it is separate because a settings record cannot say
		/// what a built-in palette looks like.
		void adopt_appearance(appearance_profile& aProfile);
		/// \brief Installs the picker a swatch opens.
		///
		/// \details Production leaves the platform picker in place. A course installs
		/// a recording stub, which is what makes the live-preview path verifiable
		/// without a modal dialog no test could drive.
		void set_colour_picker(colour_picker aPicker);
		/// \brief Chooses one appearance mode, as choosing a tile does.
		///
		/// \details The gesture the tiles perform, offered by name because the tiles
		/// are now the control: there is no combo box index to set. It marks the
		/// chosen tile and dispatches the same intent.
		void choose_mode(core::appearance_mode aMode);
		/// Cancels visible preview and pending import, even before a preview arrives.
		void cancel_preview();
		/// \brief Reports that an accepted session change could not be persisted.
		void report_persistence_unavailable();

		/// \brief Presents the host's attachment-local motion resolution without an effect.
		void               adopt_motion(core::dif_resolution aResolution);
		[[nodiscard]] auto dif_visibility_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto motion_selector() const noexcept -> QComboBox*;
		/// \brief Shows the settings the host has put into effect.
		void adopt(const application::appearance_settings& aSettings);
		/// \brief Shows settings plus the palette currently resolved by presentation.
		///
		/// \details A fresh System record intentionally carries no stored palette.
		/// Supplying the resolved palette separately makes a one-role edit start
		/// from the colours actually on screen, including on a dark desktop.
		void adopt(
			const application::appearance_settings& aSettings,
			const core::appearance_palette&         aResolvedPalette);
		/// \brief Mirrors a host adoption while retaining its active palette preview.
		///
		/// \details The composed appearance profile owns whether a preview is still
		/// active after an unrelated mode, typography, or native observation. Passing
		/// that exact palette keeps the panel's preview bar consistent with the
		/// pixels; `std::nullopt` makes an accepted palette commit terminal.
		void adopt(
			const application::appearance_settings& aSettings,
			const core::appearance_palette&         aResolvedPalette,
			std::optional<core::appearance_palette> aActivePreview);

		/// \brief Shows why an offered record was refused, and restores the
		/// controls to the last adopted one.
		void report(std::span<const core::typography_rejection> aRejections);
		/// \brief Mirrors a host-validated imported palette and marks it preview-only.
		void preview_import(const core::appearance_palette& aPalette);
		/// \brief Shows the core parser's exact structural refusal.
		void report(std::span<const core::theme_import_rejection> aRejections);
		/// \brief Shows the core contrast findings without substituting colours.
		void report(
			std::span<const application::appearance_contrast_rejection> aRejections);
		/// \brief [[BPR-1714]]. Names each entry the host refused, under the field.
		///
		/// \details The indices are the offered list's; the field keeps the text it
		/// was offered from so the reviewer can correct it.
		void report(std::span<const core::known_namespace_rejection> aRejections);
		void report_theme_document_unavailable();
		void report_theme_export(bool aSucceeded);
		/// \brief Dispatches a path selected by Qt; this widget never opens it.
		void request_theme_import(const QString& aPath);
		void request_theme_export(const QString& aPath);

		/// \brief The six theme tiles, which are the Theme card's control.
		[[nodiscard]] auto theme_previews() const noexcept -> theme_preview_strip*;
		/// \brief The Type card's labelled preview.
		[[nodiscard]] auto specimen() const noexcept -> type_preview*;
		/// \brief The Colours card's 60-role list.
		[[nodiscard]] auto colours() const noexcept -> colour_role_list*;
		[[nodiscard]] auto colours_title() const noexcept -> QLabel*;
		[[nodiscard]] auto colours_subtitle() const noexcept -> QLabel*;
		/// \brief The interface font chooser, which shows the family in effect.
		[[nodiscard]] auto ui_font_chooser() const noexcept -> QComboBox*;
		[[nodiscard]] auto code_font_chooser() const noexcept -> QComboBox*;
		/// \brief The size steppers, which read in points and carry centipoints.
		[[nodiscard]] auto ui_size_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto ui_weight_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto ui_line_height_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto code_size_editor() const noexcept -> QSpinBox*;
		/// \brief [[BPR-1336]]. The comparison's zoom, one item per core step.
		[[nodiscard]] auto code_zoom_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto code_weight_editor() const noexcept -> QSpinBox*;
		/// \brief One segment of the 2, 4, 8 tab-width control, or none.
		///
		/// \details Absent for any other width: the segment offers the three the
		/// design names, and a durable record carrying another legal width leaves
		/// every segment unchosen rather than showing a width that is not in effect.
		[[nodiscard]] auto tab_width_option(std::uint32_t aWidth) const
			-> QAbstractButton*;
		/// \brief The tab width the segment currently expresses.
		[[nodiscard]] auto tab_width() const -> std::uint32_t;
		/// \brief [[BPR-1107]]. When the code panes draw white space.
		[[nodiscard]] auto white_space_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto ligatures_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto syntax_bold_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto syntax_italic_switch() const noexcept -> QCheckBox*;
		/// \brief [[BPR-1714]]. The Syntax card's known C++ namespaces field.
		[[nodiscard]] auto known_namespaces_editor() const noexcept -> QLineEdit*;
		/// \brief The line under the field naming refused entries; hidden when none.
		[[nodiscard]] auto known_namespaces_problem() const noexcept -> QLabel*;
		[[nodiscard]] auto import_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto export_button() const noexcept -> QPushButton*;
		/// \brief The bar at the Colours card's foot, or nothing.
		///
		/// \details Present exactly while something is previewed. It is created when
		/// a preview begins and removed when it ends, rather than hidden, so it is
		/// absent from the widget tree and from the accessibility tree at every
		/// moment the reviewer is not previewing anything.
		[[nodiscard]] auto preview_bar() const noexcept -> QWidget*;
		[[nodiscard]] auto preview_bar_text() const -> QString;
		[[nodiscard]] auto keep_changes_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_changes_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto preview_palette() const noexcept
			-> const std::optional<core::appearance_palette>&;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto adopted() const noexcept
			-> const application::appearance_settings&;
		[[nodiscard]] auto statistics() const noexcept
			-> appearance_settings_panel_statistics;

	private:
		void dispatch_dif();
		void dispatch_mode(core::appearance_mode aMode);
		void dispatch_typography();
		/// \brief Chooses one segment of the tab-width control and offers the record.
		///
		/// \details The three segments are not an auto-exclusive group, because Qt
		/// refuses to uncheck the checked member of one and a durable record carrying
		/// a legal width the design does not offer must leave every segment unchosen.
		/// Exclusivity is therefore kept here, where that case can be expressed.
		void choose_tab_width(std::uint32_t aWidth);
		void dispatch_reset(application::appearance_reset_scope aScope);
		/// \brief Judges the field's text and offers it when every entry is legal.
		void commit_known_namespaces();
		/// \brief Shows, or with no rejections hides, the refused entries of `aOffered`.
		void show_namespace_problems(const core::known_namespaces&                    aOffered,
									 std::span<const core::known_namespace_rejection> aRejections);
		void dispatch_palette_commit();
		void dispatch_palette_cancel();
		void select_theme_file();
		void select_theme_destination();
		/// \brief Folds one edited colour into the preview and offers it.
		void edit_colour(core::appearance_role aRole, core::appearance_color aColour);
		/// \brief Queues one coalesced preview delivery of the latest candidate.
		void offer_preview();
		/// \brief Returns the record the controls currently express.
		[[nodiscard]] auto code_zoom() const -> std::uint32_t;
		void               show_code_zoom(const core::code_font_settings& aCode);
		[[nodiscard]] auto composed_typography() const
			-> core::typography_settings;
		/// \brief Writes the adopted record into the controls.
		///
		/// \details Every write is echo-suppressed. A control set
		/// programmatically emits the same change signal a reviewer's gesture
		/// does, so without this an adoption would dispatch the intent that
		/// produced it, and a rejection would re-offer the record that was just
		/// refused.
		void show_adopted();
		/// \brief Shows the adopted and previewed colours, and the card's title.
		void refresh_colours();
		/// \brief Creates, updates or removes the one preview bar.
		void refresh_preview_bar();
		/// \brief Adds the session-only diagnostic when persistence is unavailable.
		[[nodiscard]] auto with_persistence_status(QString aStatus) const
			-> QString;
		/// \brief Names the theme on screen, or says so when nobody has supplied one.
		[[nodiscard]] auto theme_text() const -> QString;

		application::appearance_intent_dispatch Dispatch;
		/// The last record the host published. This is a mirror rather than an
		/// opinion: it is only ever written by `adopt`, and it exists so a
		/// refusal has something exact to restore.
		application::appearance_settings AdoptedSettings;
		core::appearance_palette         CommittedPalette{ };
		/// \brief The theme on screen, once the composition has named it.
		std::optional<core::appearance_variant> ResolvedVariant;

		theme_preview_strip* ThemePreviews   = nullptr;
		type_preview*        Specimen        = nullptr;
		colour_role_list*    Colours         = nullptr;
		QLabel*              ColoursTitle    = nullptr;
		QLabel*              ColoursSubtitle = nullptr;
		QComboBox*           UiFamily        = nullptr;
		QSpinBox*            UiSize          = nullptr;
		QSpinBox*            UiWeight        = nullptr;
		QSpinBox*            UiLineHeight    = nullptr;
		QComboBox*           CodeFamily      = nullptr;
		QSpinBox*            CodeSize        = nullptr;
		QSpinBox*            CodeWeight      = nullptr;
		/// The 2, 4, 8 segment, in that order; an entry is absent when the core's
		/// own bounds do not admit that width.
		std::array<QAbstractButton*, 3U> TabWidths{ };
		QComboBox*                       WhiteSpace       = nullptr;
		QComboBox*                       CodeZoom         = nullptr;
		QCheckBox*                       Ligatures        = nullptr;
		QCheckBox*                       SyntaxBold       = nullptr;
		QCheckBox*                       SyntaxItalic     = nullptr;
		QLineEdit*                       KnownNamespaces  = nullptr;
		QLabel*                          NamespaceProblem = nullptr;
		/// \brief The consequence the field always carries, before any problem.
		QString NamespaceConsequence;
		/// \brief The text under the field names refused entries the reviewer has
		/// not corrected yet, so an unrelated adoption leaves their text alone.
		bool NamespacesHeld = false;
		/// \brief The list the field last showed from an adoption.
		core::known_namespaces ShownNamespaces;
		QPushButton*           ImportButton   = nullptr;
		QPushButton*           ExportButton   = nullptr;
		QCheckBox*             DifVisibility  = nullptr;
		QComboBox*             MotionSelector = nullptr;
		QLabel*                MotionStatus   = nullptr;
		core::dif_resolution   MotionResolution;
		QWidget*               ColoursCard     = nullptr;
		QWidget*               PreviewBar      = nullptr;
		QLabel*                PreviewBarLabel = nullptr;
		QPushButton*           KeepChanges     = nullptr;
		QPushButton*           CancelChanges   = nullptr;
		QLabel*                Status          = nullptr;

		std::optional<core::appearance_palette> PreviewPalette;
		std::optional<core::appearance_palette> PendingPreview;
		bool                                    PreviewDeliveryQueued = false;
		bool                                    Echoing               = false;
		bool                                    PersistenceAvailable  = true;

		appearance_settings_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
