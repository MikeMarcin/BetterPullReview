#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TABS_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TABS_PROJECTION_HPP

#include "bpr/core/document_tabs.hpp"

#include <optional>
#include <vector>

namespace bpr::application {
	// [[BPR-1132]]. What presentation receives of a window's document tabs,
	// apart from the host that owns them ([[BPR-1131]]).

	/// \brief One tab as the tab strip shows it.
	struct document_tab_entry
	{
		core::document_tab_id         Id;
		core::window_review_position  View;
		core::document_tab_view_state State         = core::document_tab_view_state::idle;
		bool                          Active        = false;
		bool                          OwnComparison = false;

		auto operator==(const document_tab_entry&) const -> bool = default;
	};

	/// \brief A window's tabs, whole, in order.
	struct document_tabs_projection
	{
		std::vector<document_tab_entry>      Tabs;
		std::optional<core::comparison_pair> WindowComparison;
		core::document_tab_reason            Reason = core::document_tab_reason::none;

		auto operator==(const document_tabs_projection&) const -> bool = default;
	};

	/// \brief Receives the window's tab projection.
	class document_tabs_projection_port
	{
	public:
		virtual ~document_tabs_projection_port() = default;

		virtual void publish(const document_tabs_projection& aProjection) = 0;
	};

} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TABS_PROJECTION_HPP
