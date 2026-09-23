#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PULL_REQUEST_LINK_LOCATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PULL_REQUEST_LINK_LOCATION_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/pull_request_location.hpp"

#include <string>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-0987]]. What one accepted snapshot tells a link location.
	///
	/// \details The changed paths, and each review and review comment the read
	/// identified by the provider's numeric identity, paired with the
	/// conversation identity the window's projection gives it. An
	/// authenticated read names review comments by node identity alone, so it
	/// contributes no review comment here; a signed-out read names them by
	/// their REST identity and contributes them all.
	struct pull_request_link_location_facts
	{
		std::vector<std::string>                              ChangedPaths;
		std::vector<core::pull_request_location_conversation> Reviews;
		std::vector<core::pull_request_location_conversation> ReviewComments;

		/// \brief The core's view of these facts for one comparison.
		[[nodiscard]] auto facts(const bool aRightIsHead) const noexcept -> core::pull_request_location_facts
		{
			return { .ChangedPaths = ChangedPaths, .Reviews = Reviews, .ReviewComments = ReviewComments, .RightIsHead = aRightIsHead };
		}
	};

	/// \brief Reads the link-location facts of one accepted snapshot.
	[[nodiscard]] auto pull_request_link_location_facts_of(const github_pull_request_snapshot& aSnapshot)
		-> pull_request_link_location_facts;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PULL_REQUEST_LINK_LOCATION_HPP
