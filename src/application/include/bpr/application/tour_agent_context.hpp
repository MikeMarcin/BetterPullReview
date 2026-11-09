#ifndef BPR_APPLICATION_TOUR_AGENT_CONTEXT_HPP
#define BPR_APPLICATION_TOUR_AGENT_CONTEXT_HPP

#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/tour_agent_session.hpp"

namespace bpr::application {
	/// \brief Coherently captured host observations; absent facts remain unknown.
	struct tour_agent_context
	{
		std::optional<core::git_commit_oid>      ObservedBase, ObservedHead, PinnedBase, PinnedHead;
		core::tour_attention                     Attention;
		std::optional<core::tour_source_range>   SelectedSource;
		std::vector<core::tour_destination_plan> Destinations;
		bool                                     Compatibility = false;
		core::tour_operation_support             BodyEdits     = core::tour_operation_support::unknown;
		std::optional<bool>                      PublicationAllowed;
		std::string                              PublicationStatus;
		bool                                     Saving = false, SaveFailed = false, StoreConflict = false;
		const core::tour_agent_session*          Session = nullptr;
	};
	/// \brief Trusted, versioned host guide, independent of repository text.
	[[nodiscard]] auto tour_agent_guide() noexcept -> std::string_view;
	/// \brief Complete current facts and a content digest, never a partial snapshot.
	[[nodiscard]] auto tour_agent_snapshot(const core::review_tour&, const std::optional<core::tour_proposal>&, std::string_view, const core::cycle_json_value&, const tour_agent_context&) -> std::optional<core::cycle_json_value>;
}
#endif
