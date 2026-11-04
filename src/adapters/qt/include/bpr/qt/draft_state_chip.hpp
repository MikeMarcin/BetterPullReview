#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DRAFT_STATE_CHIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DRAFT_STATE_CHIP_HPP

#include <QLabel>

namespace bpr::qt {
	/// \brief [[BPR-1852]]. "Draft", the way GitHub marks a draft pull
	/// request: a solid grey pill with a dashed ring before the word.
	///
	/// \details The owner, 2026-11-03: "I can only barely tell that my PR is
	/// a draft from the overview tab". The chip stands beside the pull
	/// request's name wherever it is shown (the title row and Overview's
	/// title) in the theme's own quiet ink, filled so it reads at a glance.
	/// The word is the label's text, so it is its accessible name; the shape
	/// is never the only sign. It takes no focus and performs nothing.
	class draft_state_chip final : public QLabel
	{
	public:
		explicit draft_state_chip(QWidget* aParent = nullptr);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};
} // namespace bpr::qt

#endif
