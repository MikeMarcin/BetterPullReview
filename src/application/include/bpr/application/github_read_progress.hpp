#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_PROGRESS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_PROGRESS_HPP

#include <compare>
#include <cstdint>

namespace bpr::application {
	// The bounded snapshot read permits 1,024 pages and update history adds
	// at most ten. Each page reports one start and one received observation.
	inline constexpr std::uint32_t maximum_github_read_progress_pages    = 1'034U;
	inline constexpr std::uint32_t maximum_github_read_progress_sequence = 2U * maximum_github_read_progress_pages;

	enum class github_read_phase : std::uint8_t {
		metadata,
		files,
		commits,
		reviews,
		review_threads,
		comments,
		checks,
		related_pull_requests,
		update_history,
	};

	/// \brief Bounded loading observations, never acceptance of partial content.
	struct github_read_progress
	{
		github_read_phase Phase         = github_read_phase::metadata;
		std::uint32_t     PagesReceived = 0;
		std::uint32_t     Sequence      = 0;

		auto operator<=>(const github_read_progress&) const = default;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_PROGRESS_HPP
