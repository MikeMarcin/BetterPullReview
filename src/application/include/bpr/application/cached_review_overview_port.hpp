#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CACHED_REVIEW_OVERVIEW_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CACHED_REVIEW_OVERVIEW_PORT_HPP
#include "bpr/core/cached_review_overview.hpp"
#include <stop_token>
namespace bpr::application {
	/// \brief A profile-private, bounded overview store. Call only on its serial effect lane.
	/// Failed loads expose no content. Saves atomically replace the exact destination.
	class cached_review_overview_port
	{
	public:
		virtual ~cached_review_overview_port() = default;
		[[nodiscard]] virtual auto load(const core::cached_review_key& aKey, std::stop_token aStop)
			-> std::optional<core::cached_review_overview>                                                            = 0;
		[[nodiscard]] virtual auto save(const core::cached_review_overview& aOverview, std::stop_token aStop) -> bool = 0;
	};
}
#endif
