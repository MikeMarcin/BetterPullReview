#ifndef BPR_APP_TOUR_OWNER_SERVICE_HPP
#define BPR_APP_TOUR_OWNER_SERVICE_HPP
#include "bpr/application/tour_owner_channel.hpp"
#include "tour_assist.hpp"
#include "tour_runtime.hpp"
#include <filesystem>
#include <functional>
#include <span>
namespace bpr::app {
	/// \brief Runs its callable to completion on the lane that may block on private storage.
	using tour_store_lane = std::function<void(std::function<void()>)>;
	/// \brief Opaque exact-draft selector shared by the CLI and the owner channel.
	[[nodiscard]] auto tour_draft_token(const application::tour_saved_target& aTarget) -> std::string;
	/// \brief Reads one exact stored draft by token; never selects an implicit last-opened target.
	[[nodiscard]] auto load_local_tour(application::review_tour_store& aStore, std::string_view aDraft, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>;
	/// \brief One durable local transaction with request deduplication; no provider or editor authority.
	[[nodiscard]] auto mutate_local_tour(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, application::tour_service_request aRequest, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>;
	/// \brief Issues the draft's one live assist ticket on a short-lived host; the journal is durable on return.
	[[nodiscard]] auto issue_local_assist(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>;
	/// \brief Retires every pending ticket and proposal for the draft.
	[[nodiscard]] auto cancel_local_assist(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>;
	/// \brief [[BPR-1227]]. Reads one pinned destination for an exact stored draft. The desktop answers a
	/// source request from the window that holds the draft's pull request and never reaches this.
	using tour_owner_source = std::function<tour_cli_result<tour_source_material>(const tour_local_snapshot&, const core::tour_code_destination&)>;
	/// \brief Serves finite local tour commands for the profile owner from its own store.
	/// Every request is one bounded transaction on the caller's store lane. Only tour commands are
	/// admitted; the peer cannot select a process, URL, SQL statement or provider operation.
	class tour_owner_service final
	{
	public:
		/// \param aSource How a `source` request is read; without one, it needs an open pull request.
		tour_owner_service(std::filesystem::path aRoot, core::bpr_profile_id aProfile, tour_owner_source aSource = { });
		/// \brief Returns the encoded response. Malformed input yields a refusal, never a mutation.
		[[nodiscard]] auto serve(std::span<const std::uint8_t> aPayload) const -> application::tour_channel_bytes;

	private:
		std::filesystem::path Root;
		core::bpr_profile_id  Profile;
		tour_owner_source     Source;
	};
}
#endif
