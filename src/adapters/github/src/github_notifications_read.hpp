#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_NOTIFICATIONS_READ_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_NOTIFICATIONS_READ_HPP

#include "github_read_contract.hpp"

#include "bpr/core/pull_request_activity.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1850]]. Reads one `GET /notifications` answer into what the
/// core decides from.
/// \details Only pull request threads are kept, and of each only its
/// identity, dates, unread flag, reason, repository, number and the kind and
/// identity of its latest comment. The subject's title and every other field
/// are skipped unread into anything: nothing a notification could show comes
/// from here.
namespace bpr::adapters::github::detail {
	/// \brief The page size the request asks for.
	inline constexpr std::size_t notifications_page_size = 50U;
	/// \brief The largest answer read.
	inline constexpr std::size_t maximum_notifications_body_bytes = 1024U * 1024U;

	struct notifications_read
	{
		core::activity_poll_answer Poll;
		/// The answer's `Last-Modified`, when it is an HTTP date.
		std::string                        LastModified;
		std::vector<core::activity_thread> Threads;
		/// The page held every thread GitHub would list.
		bool Complete = false;

		auto operator==(const notifications_read&) const -> bool = default;
	};

	/// \brief Interprets one answer: 200 lists threads, 304 is unchanged, a 403
	/// or 404 that is not a rate limit says this sign-in cannot read
	/// notifications, 403 and 429 rate limits say how long to wait, and
	/// anything else, or a 200 that does not parse, is a failure.
	[[nodiscard]] auto read_notifications_response(const wire_response& aResponse) -> notifications_read;

	/// \brief `2026-10-30T04:02:16Z` in seconds since the epoch.
	[[nodiscard]] auto notification_timestamp(std::string_view aValue) -> std::optional<std::int64_t>;
} // namespace bpr::adapters::github::detail

#endif
