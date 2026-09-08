#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/theme_preview_strip.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <span>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace bpr::qt {
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

		auto operator<=>(const appearance_settings_panel_statistics&) const
			= default;
	};

	/// \brief Lets a reviewer choose appearance mode, fonts, and code layout.
	///
	/// \details Every committed control shows what the host last published
	/// through `adopt`, and every gesture leaves as an intent; nothing here
	/// applies a font, a colour, or a mode. The sole transient state is an
	/// explicitly labelled total-palette preview, which commit offers to the host
	/// and cancel discards in favour of the last adopted palette.
	///
	/// Rejections are shown, never repaired. `core::validate_typography`
	/// refuses a record whole and names each violation; `report` displays those
	/// reasons and puts every control back to the last adopted record. Clamping
	/// an out-of-range value into range would tell the reviewer that the value
	/// they typed was the value they got.
	///
	/// Numeric ranges come from `core::bounds_of`, which is the same function
	/// the validator enforces with, so an in-range gesture cannot be one the
	/// core would then reject. Only the family, which the core bounds by byte
	/// length rather than by value, can be refused from this surface — and that
	/// path is exercised rather than assumed.
	class appearance_settings_panel final : public QWidget
	{
	public:
		explicit appearance_settings_panel(
			application::appearance_intent_dispatch aDispatch = { },
			QWidget*                                aParent   = nullptr);

		void set_dispatch(application::appearance_intent_dispatch aDispatch);
		/// \brief Reports that an accepted session change could not be persisted.
		void report_persistence_unavailable();

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
		/// that exact palette keeps the panel's commit/cancel controls consistent with
		/// the pixels; `std::nullopt` makes an accepted palette commit terminal.
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
		void report_theme_document_unavailable();
		void report_theme_export(bool aSucceeded);
		/// \brief Dispatches a path selected by Qt; this widget never opens it.
		void request_theme_import(const QString& aPath);
		void request_theme_export(const QString& aPath);

		[[nodiscard]] auto mode_selector() const noexcept -> QComboBox*;
		/// \brief [[BPR-0652]]. The four theme previews above the mode selector.
		[[nodiscard]] auto theme_previews() const noexcept -> theme_preview_strip*;
		[[nodiscard]] auto ui_family_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto ui_size_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto ui_weight_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto ui_line_height_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto code_family_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto code_size_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto code_weight_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto tab_width_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto ligatures_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto syntax_bold_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto syntax_italic_switch() const noexcept -> QCheckBox*;
		[[nodiscard]] auto role_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto color_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto preview_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto commit_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto import_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto preview_palette() const noexcept
			-> const std::optional<core::appearance_palette>&;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto adopted() const noexcept
			-> const application::appearance_settings&;
		[[nodiscard]] auto statistics() const noexcept
			-> appearance_settings_panel_statistics;

	private:
		void dispatch_mode();
		void dispatch_typography();
		void dispatch_palette_preview();
		void dispatch_palette_commit();
		void dispatch_palette_cancel();
		void select_theme_file();
		void show_selected_color();
		/// \brief Returns the record the controls currently express.
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
		/// \brief Adds the session-only diagnostic when persistence is unavailable.
		[[nodiscard]] auto with_persistence_status(QString aStatus) const
			-> QString;

		application::appearance_intent_dispatch Dispatch;
		/// The last record the host published. This is a mirror rather than an
		/// opinion: it is only ever written by `adopt`, and it exists so a
		/// refusal has something exact to restore.
		application::appearance_settings AdoptedSettings;
		core::appearance_palette         CommittedPalette{ };

		QComboBox*                              ModeSelector  = nullptr;
		QLineEdit*                              UiFamily      = nullptr;
		QSpinBox*                               UiSize        = nullptr;
		QSpinBox*                               UiWeight      = nullptr;
		QSpinBox*                               UiLineHeight  = nullptr;
		QLineEdit*                              CodeFamily    = nullptr;
		QSpinBox*                               CodeSize      = nullptr;
		QSpinBox*                               CodeWeight    = nullptr;
		QSpinBox*                               TabWidth      = nullptr;
		QCheckBox*                              Ligatures     = nullptr;
		QCheckBox*                              SyntaxBold    = nullptr;
		QCheckBox*                              SyntaxItalic  = nullptr;
		QComboBox*                              RoleSelector  = nullptr;
		QLineEdit*                              ColorEditor   = nullptr;
		QPushButton*                            PreviewButton = nullptr;
		QPushButton*                            CommitButton  = nullptr;
		QPushButton*                            CancelButton  = nullptr;
		QPushButton*                            ImportButton  = nullptr;
		QLabel*                                 Status        = nullptr;
		theme_preview_strip*                    ThemePreviews = nullptr;
		std::optional<core::appearance_palette> PreviewPalette;
		bool                                    Echoing              = false;
		bool                                    PersistenceAvailable = true;

		appearance_settings_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
