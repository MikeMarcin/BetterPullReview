#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_DESIGNATED_REVIEW_PUBLIC_OBSERVER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_DESIGNATED_REVIEW_PUBLIC_OBSERVER_HPP

#include <memory>

namespace bpr::application::detail {
	class designated_review_public_observation_port;
}

namespace bpr::adapters::github::detail {
	/// \brief Builds the unauthenticated direct designated-target observer.
	[[nodiscard]] auto make_designated_review_public_observer()
		-> std::unique_ptr<application::detail::designated_review_public_observation_port>;
}

#endif
