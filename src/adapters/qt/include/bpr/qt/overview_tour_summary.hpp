#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_TOUR_SUMMARY_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_TOUR_SUMMARY_HPP

#include "bpr/core/overview_tour.hpp"

#include <QScrollArea>

#include <functional>
#include <vector>

class QLabel;
class QPushButton;
class QVBoxLayout;

/// \file
/// \brief [[BPR-1781]]. Overview's Tour tab: the pull request's tour as a
/// summary that routes into Review.
namespace bpr::qt {
	/// \brief What a reviewer may ask of Overview's Tour tab; the window
	/// decides what each does.
	enum class overview_tour_gesture : std::uint8_t {
		/// Open Review's Tour tab to write a new tour.
		prepare,
		/// Open Review's Tour tab at the current stop.
		continue_tour,
		/// Open Review at the published tour's first stop.
		start_tour,
	};

	/// \brief The tour, read: the brief, the route of stops and one action.
	///
	/// \details A writer sees each stop's readiness as the route in Review
	/// shows it, and whether the tour is a private draft or published; a
	/// reader sees the brief and the stop titles. Nothing here edits or
	/// publishes: the one button reports its gesture.
	class overview_tour_summary final : public QScrollArea
	{
	public:
		using gesture_dispatch = std::function<void(overview_tour_gesture)>;

		explicit overview_tour_summary(QWidget* aParent = nullptr);

		void               present(const core::overview_tour& aTour);
		[[nodiscard]] auto shown() const noexcept -> const core::overview_tour& { return Shown; }
		void               set_dispatch(gesture_dispatch aDispatch);
		/// \brief The one action: Prepare review tour, Continue in Review or Start tour.
		[[nodiscard]] auto action() const noexcept -> QPushButton* { return Action; }
		[[nodiscard]] auto state_label() const noexcept -> QLabel* { return State; }
		/// \brief The stop rows' titles, in route order.
		[[nodiscard]] auto stop_titles() const -> QStringList;
		/// \brief The stop rows' readiness words, in route order; empty for a reader.
		[[nodiscard]] auto stop_readiness() const -> QStringList;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		struct stop_row
		{
			QWidget* Row       = nullptr;
			QWidget* Ring      = nullptr;
			QLabel*  Title     = nullptr;
			QLabel*  Readiness = nullptr;
		};

		void rebuild_stops();
		void refresh_stops();
		void refresh_colours();

		core::overview_tour   Shown;
		gesture_dispatch      Dispatch;
		QLabel*               State           = nullptr;
		QLabel*               Invitation      = nullptr;
		QLabel*               ProblemHeading  = nullptr;
		QLabel*               Problem         = nullptr;
		QLabel*               SolutionHeading = nullptr;
		QLabel*               Solution        = nullptr;
		QLabel*               StopsHeading    = nullptr;
		QWidget*              Route           = nullptr;
		QVBoxLayout*          RouteLayout     = nullptr;
		std::vector<stop_row> Rows;
		QPushButton*          Action = nullptr;
	};
} // namespace bpr::qt

#endif
