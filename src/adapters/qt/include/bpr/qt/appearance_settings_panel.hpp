#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <span>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace bpr::qt {
	/// \brief Records directly observable settings-surface activity.
	struct appearance_settings_panel_statistics
	{
		std::size_t Adoptions        = 0U;
		std::size_t Rejections       = 0U;
		std::size_t ModeGestures     = 0U;
		std::size_t FontGestures     = 0U;
		std::size_t SuppressedEchoes = 0U;

		auto operator<=>(const appearance_settings_panel_statistics&) const
			= default;
	};

	/// \brief Lets a reviewer choose appearance mode, fonts, and code layout.
	///
	/// \details The surface holds no presentation state of its own. Every
	/// control shows what the host last published through `adopt`, and every
	/// gesture leaves as an intent; nothing here applies a font, a colour, or a
	/// mode. A control that remembered its own value would disagree with the
	/// host the first time a change was refused, and would go on showing the
	/// reviewer a setting that is not in effect.
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

		/// \brief Shows the settings the host has put into effect.
		void adopt(const application::appearance_settings& aSettings);

		/// \brief Shows why an offered record was refused, and restores the
		/// controls to the last adopted one.
		void report(std::span<const core::typography_rejection> aRejections);

		[[nodiscard]] auto mode_selector() const noexcept -> QComboBox*;
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
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto adopted() const noexcept
			-> const application::appearance_settings&;
		[[nodiscard]] auto statistics() const noexcept
			-> appearance_settings_panel_statistics;

	private:
		void dispatch_mode();
		void dispatch_typography();
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

		application::appearance_intent_dispatch Dispatch;
		/// The last record the host published. This is a mirror rather than an
		/// opinion: it is only ever written by `adopt`, and it exists so a
		/// refusal has something exact to restore.
		application::appearance_settings AdoptedSettings;

		QComboBox* ModeSelector = nullptr;
		QLineEdit* UiFamily     = nullptr;
		QSpinBox*  UiSize       = nullptr;
		QSpinBox*  UiWeight     = nullptr;
		QSpinBox*  UiLineHeight = nullptr;
		QLineEdit* CodeFamily   = nullptr;
		QSpinBox*  CodeSize     = nullptr;
		QSpinBox*  CodeWeight   = nullptr;
		QSpinBox*  TabWidth     = nullptr;
		QCheckBox* Ligatures    = nullptr;
		QCheckBox* SyntaxBold   = nullptr;
		QCheckBox* SyntaxItalic = nullptr;
		QLabel*    Status       = nullptr;
		bool       Echoing      = false;

		appearance_settings_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_SETTINGS_PANEL_HPP
