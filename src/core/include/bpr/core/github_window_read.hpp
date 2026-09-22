#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_WINDOW_READ_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_WINDOW_READ_HPP

#include "bpr/core/github_transition.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstdint>

namespace bpr::core {
	/// Profile-owned identity observed by one independent window read lane.
	struct github_window_session
	{
		bpr_profile_id             Profile;
		github_session_ready_event Identity;
		github_access_mode         Mode{ };
		auto                       operator<=>(const github_window_session&) const = default;
	};

	using github_window_read_event = std::variant<open_pull_request_intent,
												  refresh_pull_request_intent, pull_request_loaded_event,
												  pull_request_load_failed_event, pull_request_load_cancelled_event>;

	/// A read lane cannot request authentication or retire a profile session.
	struct github_window_read_output
	{
		github_transition_output                  Projection;
		std::optional<fetch_pull_request_request> Fetch;
	};

	class github_window_read
	{
	public:
		explicit github_window_read(bpr_window_context     aWindow,
									application_session_id aSession);
		[[nodiscard]] auto adopt(const github_window_session& aSession) -> bool;
		[[nodiscard]] auto retire(const github_window_session& aSession) -> bool;
		[[nodiscard]] auto apply(bpr_window_context              aWindow,
								 const github_window_read_event& aEvent) -> github_window_read_output;
		void               advance_request_floor(request_id aNext) noexcept;
		/// Accept an already-admitted profile sibling's server observation.
		/// Only snapshot identity changes; requests and window navigation do not.
		/// The profile owner orders observations, including reentrant delivery.
		[[nodiscard]] auto share_snapshot(const github_window_read& aSource, std::uint64_t aSequence) noexcept -> bool;
		/// Accept at most one provisional metadata observation for this exact
		/// pending request, without changing command or snapshot authority.
		[[nodiscard]] auto accept_overview_observation(bpr_window_context                  aWindow,
													   const pull_request_request_context& aRequest) noexcept -> bool;
		[[nodiscard]] auto state() const noexcept -> const github_state&;
		[[nodiscard]] auto window() const noexcept -> bpr_window_context;

	private:
		bpr_window_context                   Window;
		github_state                         State;
		std::optional<github_window_session> Session;
		github_identity_generation           RetiredGeneration{ };
		bool                                 OverviewObserved = false;
		std::uint64_t                        SharedSnapshotSequence = 0U;
	};
}

#endif
