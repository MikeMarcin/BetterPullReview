#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CACHED_REVIEW_SOURCE_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CACHED_REVIEW_SOURCE_PORT_HPP
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/github_projection.hpp"
#include <functional>

namespace bpr::application {
	/// The authenticated window host issues this historical read after core admission.
	/// No accepted snapshot, selected comparison, epoch or mutation lease is granted.
	struct cached_review_source_request
	{
		epoch_catalog_open_request Catalog;
		core::git_commit_oid       Base, Head;
		core::cached_review_file   File;
	};
	inline constexpr std::uint64_t maximum_cached_source_bytes = 256 * 1024;
	using cached_review_source_completion                      = std::function<void(cached_review_source_projection)>;
	class cached_review_source_port
	{
	public:
		virtual ~cached_review_source_port() = default;
		/// Decision-lane ingress; admitted jobs complete on that lane. Shares the Git actor.
		[[nodiscard]] virtual auto read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool = 0;
	};
}
#endif
