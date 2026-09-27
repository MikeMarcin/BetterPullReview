#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#include "bpr/application/review_tour_projection.hpp"
#include <expected>
#include <stop_token>
namespace bpr::application {
	/// \brief Local lookup metadata captured from an accepted provider read, never a write grant.
	struct tour_saved_target
	{
		core::tour_owner          Owner;
		core::tour_draft_id       Draft;
		core::host_profile_id     HostProfile;
		std::string               Hostname;
		core::repository_locator  Repository;
		core::pull_request_number Number;
		core::git_commit_oid      Base, Head;
		auto                      operator==(const tour_saved_target&) const -> bool = default;
	};
	struct tour_store_error
	{
		tour_store_failure               Failure = tour_store_failure::unavailable;
		std::optional<core::review_tour> Current;
	};
	template <typename T>
	using tour_store_result = std::expected<T, tour_store_error>;
	/// \brief Admitted profile-private storage. All calls run on a serialized effect lane.
	class review_tour_store
	{
	public:
		virtual ~review_tour_store()                                                                                                                                = default;
		virtual auto load(const core::tour_owner& aOwner, core::tour_draft_id aDraft, std::stop_token aStop) -> tour_store_result<std::optional<core::review_tour>> = 0;
		/// Expected zero creates an absent draft; otherwise exact CAS is mandatory.
		virtual auto save(const core::review_tour& aDraft, std::uint64_t aExpectedRevision, std::stop_token aStop) -> tour_store_result<core::review_tour> = 0;
		/// \brief Bounded exact target discovery under the same admitted profile owner.
		virtual auto targets(std::stop_token) -> tour_store_result<std::vector<tour_saved_target>> { return std::unexpected(tour_store_error{ }); }
		virtual auto remember_target(const tour_saved_target&, std::stop_token) -> tour_store_result<void> { return std::unexpected(tour_store_error{ }); }
	};
}
#endif
