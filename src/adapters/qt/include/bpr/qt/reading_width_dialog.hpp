#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_READING_WIDTH_DIALOG_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_READING_WIDTH_DIALOG_HPP

#include "bpr/core/workbench_interaction.hpp"

#include <QDialog>

#include <array>
#include <cstdint>
#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace bpr::qt {
	/// \brief [[BPR-2299]] (owner, 2026-11-19: "an advanced button and
	/// advanced can set the width per feature including to arbitrary custom
	/// values"). Settings › Review › Reading width › Advanced: for Threads,
	/// Discuss and reports, Use Reading width (the default), a preset, the
	/// whole page, or a width of its own in logical pixels, never narrower
	/// than Narrow's.
	///
	/// \details Presentation only: each change is handed to the dispatch at
	/// once, as the Reading width's own buttons are, and the core holds the
	/// bounds (`core::reading_override_of`) and resolves each surface's
	/// width (`core::reading_surface_limit`).
	class reading_width_dialog final : public QDialog
	{
	public:
		/// \brief The choices each surface's list offers, in order.
		enum class choice : std::uint8_t {
			reading_width = 0,
			narrow        = 1,
			medium        = 2,
			wide          = 3,
			full          = 4,
			custom        = 5,
		};

		explicit reading_width_dialog(QWidget* aParent = nullptr);

		/// \brief Shows the profile's Reading width and each surface's width,
		/// without dispatching.
		void set_state(core::reading_width aWidth, const core::reading_overrides& aOverrides);
		/// \brief Where each change goes: every surface's width as chosen now.
		void set_dispatch(std::function<void(const core::reading_overrides&)> aDispatch);
		/// \brief Each surface's width as the dialog shows it.
		[[nodiscard]] auto overrides() const -> core::reading_overrides;

		[[nodiscard]] auto choices(core::reading_surface aSurface) const -> QComboBox*;
		[[nodiscard]] auto custom_width(core::reading_surface aSurface) const -> QSpinBox*;
		[[nodiscard]] auto reset_button() const noexcept -> QPushButton* { return Reset; }
		[[nodiscard]] auto bounds_note() const noexcept -> QLabel* { return Bounds; }

	private:
		void settle(core::reading_surface aSurface);
		void dispatch();

		core::reading_width                                 Width = core::default_reading_width;
		std::array<QComboBox*, core::reading_surface_count> Choices{ };
		std::array<QSpinBox*, core::reading_surface_count>  Custom{ };
		QLabel*                                             Bounds = nullptr;
		QPushButton*                                        Reset  = nullptr;
		std::function<void(const core::reading_overrides&)> Dispatch;
		bool                                                Applying = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_READING_WIDTH_DIALOG_HPP
