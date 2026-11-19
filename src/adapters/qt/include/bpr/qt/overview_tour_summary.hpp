#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_TOUR_SUMMARY_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_TOUR_SUMMARY_HPP

#include "bpr/core/overview_tour.hpp"
#include "bpr/core/tour_publication_head.hpp"
#include "bpr/qt/agent_reference_links.hpp"

#include <QScrollArea>

#include <functional>
#include <vector>

class QLabel;
namespace bpr::qt {
	class markdown_body_view;
	class tour_head_bar;
}
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

		/// \brief [[BPR-1993]]. With the Tour tab head's decision, which a
		/// writer sees here as its state line and actions.
		void               present(const core::overview_tour& aTour, const core::tour_head& aHead = { });
		[[nodiscard]] auto shown() const noexcept -> const core::overview_tour& { return Shown; }
		[[nodiscard]] auto head() const noexcept -> const core::tour_head& { return ShownHead; }
		void               set_dispatch(gesture_dispatch aDispatch);
		/// \brief [[BPR-1993]]. A stop row opens that stop in Review.
		void set_stop_dispatch(std::function<void(core::tour_stop_id)> aDispatch);
		/// \brief [[BPR-1993]]. The head's actions, as the Tour tab's do.
		void               set_head_dispatch(std::function<void(core::tour_head_action)> aDispatch);
		[[nodiscard]] auto head_bar() const noexcept -> tour_head_bar* { return Head; }
		/// \brief [[BPR-1996]], RFC-BPR-0100. Where a code reference in the
		/// tour's text opens its lines in Review.
		void set_reference_links(agent_reference_files_source aFiles, agent_reference_navigation aNavigate);
		/// \brief [[BPR-1993]]. Each row's place line and first sentence.
		[[nodiscard]] auto stop_places() const -> QStringList;
		[[nodiscard]] auto stop_leads() const -> QStringList;
		/// \brief [[BPR-1993]]. Opens the row's stop, as a click on it does.
		void open_row(std::size_t aIndex);
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
			QLabel*  Place     = nullptr;
			QLabel*  Lead      = nullptr;
			QLabel*  Readiness = nullptr;
		};

		void rebuild_stops();
		void refresh_stops();
		void refresh_colours();

		core::overview_tour                         Shown;
		agent_reference_files_source                ReferenceFiles;
		agent_reference_navigation                  ReferenceNavigate;
		core::tour_head                             ShownHead;
		gesture_dispatch                            Dispatch;
		std::function<void(core::tour_stop_id)>     StopDispatch;
		std::function<void(core::tour_head_action)> HeadDispatch;
		tour_head_bar*                              Head         = nullptr;
		QLabel*                                     State        = nullptr;
		QLabel*                                     Invitation   = nullptr;
		QLabel*                                     StopsHeading = nullptr;
		QWidget*                                    Route        = nullptr;
		QVBoxLayout*                                RouteLayout  = nullptr;
		std::vector<stop_row>                       Rows;
		QPushButton*                                Action = nullptr;
	};
} // namespace bpr::qt

#endif
