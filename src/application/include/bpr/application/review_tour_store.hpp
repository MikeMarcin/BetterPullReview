#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#include "bpr/core/review_tour.hpp"
#include <expected>
#include <stop_token>
namespace bpr::application {
	enum class tour_store_failure : std::uint8_t { unavailable,
												   cancelled,
												   corrupt,
												   schema_mismatch,
												   conflict,
												   bounds,
												   wrong_profile };
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
	};
}
#endif
