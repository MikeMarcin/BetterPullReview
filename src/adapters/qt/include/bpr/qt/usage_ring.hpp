#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_USAGE_RING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_USAGE_RING_HPP

#include "bpr/application/public_preview_projection.hpp"

#include <QSize>
#include <QString>
#include <QWidget>

#include <cstdint>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace bpr::qt {
	/// \brief Names the mode wherever the mode is named.
	///
	/// \details One string, one place. The window title, the host picker entry
	/// and the review header all say this, and a second copy of the wording is
	/// how two of the three would come to disagree.
	[[nodiscard]] auto public_preview_mode_text() -> QString;

	/// \brief Composes the consumed-fraction and reset sentence.
	///
	/// \details Exactly `<N>% of the hourly limit · Resets in <H> hr <M> min`,
	/// with the hours dropped under an hour and `Resets now` once the reset
	/// instant has passed. `aNowUnixSeconds` is supplied by the caller; nothing
	/// in this adapter reads a clock, so the same budget renders the same
	/// sentence in every test and on every platform.
	[[nodiscard]] auto public_preview_budget_text(
		const application::public_preview_budget& aBudget,
		std::uint64_t                             aNowUnixSeconds) -> QString;

	/// \brief Composes what the preview cannot do, one sentence per line.
	[[nodiscard]] auto public_preview_limitation_text(
		const application::public_preview_presentation& aPresentation)
		-> QString;

	/// \brief Composes the switch-to-sign-in action line.
	///
	/// \details Offered from the ring's tooltip and from every refusal, which
	/// is why it is one function rather than a literal repeated at each site.
	[[nodiscard]] auto public_preview_sign_in_text() -> QString;

	/// \brief Composes a refusal: its typed reason, its budget, and sign-in.
	[[nodiscard]] auto public_preview_refusal_text(
		const application::public_preview_refusal_presentation& aRefusal,
		std::uint64_t                                           aNowUnixSeconds) -> QString;

	/// \brief Composes the wording a control disabled by the mode carries.
	[[nodiscard]] auto public_preview_unavailable_text(const QString& aControl)
		-> QString;

	/// \brief Presents the shared hourly budget as a small filled ring.
	///
	/// \details Not a persistent bar. An arc that fills with the consumed
	/// fraction of the shared hourly core budget, with the numbers, the reset,
	/// what the preview cannot do, and the way out of it in one tooltip.
	///
	/// The widget holds no clock and no budget policy. It is handed a
	/// presentation and an instant and paints what they say; a ring that aged
	/// its own reset would drift from the refusal the host would actually
	/// issue. It performs no effect either: activation leaves through the
	/// dispatch closure, so a ring nobody wired stays visible and inert.
	///
	/// Colours come from the current palette and nothing here sets a style
	/// sheet, so the ring follows the application and operating-system theme.
	class usage_ring final : public QWidget
	{
	public:
		explicit usage_ring(QWidget* aParent = nullptr);

		/// \brief Adopts one presentation and the instant to measure it from.
		void apply(
			const application::public_preview_presentation& aPresentation,
			std::uint64_t                                   aNowUnixSeconds);
		/// \brief Adopts the sink a switch-to-sign-in gesture leaves through.
		void set_dispatch(
			application::public_preview_gesture_dispatch aDispatch);

		/// \brief The consumed fraction of the budget, in [0, 1].
		///
		/// \details Zero when no budget has been observed, which is also when
		/// `has_budget()` is false: a ring drawn empty and a ring drawn for an
		/// untouched budget are the same picture and different facts.
		[[nodiscard]] auto consumed_fraction() const noexcept -> double;
		[[nodiscard]] auto has_budget() const noexcept -> bool;
		[[nodiscard]] auto tooltip_text() const -> QString;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;

	private:
		void emit_sign_in();
		void refresh_text();

		application::public_preview_gesture_dispatch Dispatch;
		application::public_preview_presentation     Presentation;
		std::uint64_t                                NowUnixSeconds = 0;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_USAGE_RING_HPP
