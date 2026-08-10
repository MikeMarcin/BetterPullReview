#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEFECT_CARD_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEFECT_CARD_VIEW_HPP

#include "bpr/core/defect_workflow.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QComboBox;
class QLabel;

namespace bpr::qt {
	/// \brief Records directly observable defect-card activity.
	struct defect_card_statistics
	{
		std::uint64_t Intents       = 0U;
		std::uint64_t Presentations = 0U;

		auto operator<=>(const defect_card_statistics&) const = default;
	};

	/// \brief Returns the non-colour badge for one defect state.
	///
	/// \details Colour is never the only signal. A reviewer deciding whether
	/// something still needs their attention must be able to read that from
	/// text, in greyscale, or through a screen reader.
	[[nodiscard]] auto badge_for(core::defect_state aState) -> QString;

	/// \brief Returns the sentence explaining one defect state.
	[[nodiscard]] auto explanation_for(core::defect_state aState) -> QString;

	/// \brief Presents one defect and offers the reviewer's explicit actions.
	///
	/// \details The card is given one dispatch callback and nothing else. It
	/// holds no defect store and no provider, so there is no path by which it
	/// could decide a defect is verified — the only way out is a command the
	/// core will judge.
	///
	/// Every action emits an intent and changes nothing. The card moves only
	/// when `present` is called with an accepted projection. A card that
	/// advanced its own badge would show a defect as verified before anyone had
	/// decided it was, which is the exact failure this RFC exists to prevent.
	class defect_card_view final : public QWidget
	{
	public:
		using dispatch = std::function<void(core::defect_command)>;

		explicit defect_card_view(dispatch aDispatch, QWidget* aParent = nullptr);

		/// \brief Adopts one accepted defect projection.
		void present(const core::defect_record& aDefect);
		/// \brief Adopts one defect together with retained anchor availability.
		void present(const core::defect_history& aHistory);

		[[nodiscard]] auto state_label() const noexcept -> QLabel*;
		[[nodiscard]] auto classification_label() const noexcept -> QLabel*;
		[[nodiscard]] auto context_label() const noexcept -> QLabel*;
		[[nodiscard]] auto severity_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto category_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto reclassify_action() const noexcept -> QAction*;
		[[nodiscard]] auto verify_action() const noexcept -> QAction*;
		[[nodiscard]] auto reopen_action() const noexcept -> QAction*;
		[[nodiscard]] auto withdraw_action() const noexcept -> QAction*;
		[[nodiscard]] auto accept_as_is_action() const noexcept -> QAction*;

		[[nodiscard]] auto shown_defect() const noexcept
			-> const std::optional<core::defect_record>&;
		[[nodiscard]] auto statistics() const noexcept -> defect_card_statistics;

	private:
		void emit_command(core::defect_command_kind aKind);
		void emit_reclassification();
		void refresh();

		dispatch Dispatch;

		QLabel*    StateLabel          = nullptr;
		QLabel*    ClassificationLabel = nullptr;
		QLabel*    ContextLabel        = nullptr;
		QComboBox* Severity            = nullptr;
		QComboBox* Category            = nullptr;
		QAction*   Reclassify          = nullptr;
		QAction*   Verify              = nullptr;
		QAction*   Reopen              = nullptr;
		QAction*   Withdraw            = nullptr;
		QAction*   AcceptAsIs          = nullptr;

		std::optional<core::defect_record>  ShownDefect;
		std::optional<core::defect_history> ShownHistory;
		defect_card_statistics              Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEFECT_CARD_VIEW_HPP
