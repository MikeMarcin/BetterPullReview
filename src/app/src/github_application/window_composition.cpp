#include "github_application/window_composition.hpp"

namespace bpr::app {
	github_application::implementation::document_tab_ports::document_tab_ports(implementation& aOwner)
		: Owner{ aOwner }
	{
	}

	void github_application::implementation::document_tab_ports::load(const core::document_tab_view_request& aRequest)
	{
		Owner.present_tab(aRequest);
	}

	void github_application::implementation::document_tab_ports::publish(const application::document_tabs_projection& aProjection)
	{
		if (!Owner.Closing && Owner.Window != nullptr) {
			if (Owner.PaneFind) {
				Owner.PaneFind->observe_scope();
			}
			if (Owner.PrFind) {
				Owner.PrFind->observe_scope();
			}
			Owner.Window->publish(aProjection);
			// A retained navigation press reads the active tab's view,
			// which an observed reading position has just changed.
			if (Owner.PendingReviewNavigation) {
				Owner.schedule_tab_presentation();
			}
		}
	}

	auto github_application::implementation::draft_state_reading::facts() const noexcept -> core::draft_state_facts
	{
		return { .Author = Author, .Viewer = Viewer, .Open = Open, .Draft = Draft };
	}
}
