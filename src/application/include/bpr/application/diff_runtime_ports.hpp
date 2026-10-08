#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_RUNTIME_PORTS_HPP

#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/diff_transition.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t   maximum_diff_outstanding_jobs = 64U;
	inline constexpr std::size_t   maximum_diff_cache_entries    = 512U;
	inline constexpr std::uint64_t maximum_diff_cache_bytes      = 256U * 1024U * 1024U;

	/// \brief Describes the diff host limits record.
	struct diff_host_limits
	{
		std::size_t   OutstandingJobs = 4U;
		std::size_t   CacheEntries    = 64U;
		std::uint64_t CacheBytes      = 64U * 1024U * 1024U;
		/// [[BPR-1327]]. Comparisons prepared before the reviewer opens them
		/// live inside the cache above, never beside it. They may hold at most
		/// this many of its entries and bytes, and never more than a quarter of
		/// its entries or an eighth of its bytes, whichever is smaller. They use
		/// only room the reviewer's own comparisons leave free: a comparison the
		/// reviewer opened is never evicted to make room for a prepared one,
		/// and a prepared one is evicted first when the reviewer needs room.
		/// Zero disables preparation.
		std::size_t   PreparedEntries = 16U;
		std::uint64_t PreparedBytes   = 8U * 1024U * 1024U;

		auto operator<=>(const diff_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const diff_host_limits& aLimits) noexcept
		-> bool
	{
		return aLimits.OutstandingJobs > 0U
			&& aLimits.OutstandingJobs <= maximum_diff_outstanding_jobs
			&& aLimits.CacheEntries > 0U
			&& aLimits.CacheEntries <= maximum_diff_cache_entries
			&& aLimits.CacheBytes > 0U
			&& aLimits.CacheBytes <= maximum_diff_cache_bytes
			&& aLimits.PreparedEntries <= maximum_diff_cache_entries
			&& aLimits.PreparedBytes <= maximum_diff_cache_bytes;
	}

	/// \brief The prepared share of the cache these limits allow.
	[[nodiscard]] constexpr auto prepared_entry_capacity(const diff_host_limits& aLimits) noexcept
		-> std::size_t
	{
		return aLimits.PreparedEntries < aLimits.CacheEntries / 4U ? aLimits.PreparedEntries : aLimits.CacheEntries / 4U;
	}

	/// \brief The prepared share of the cache's bytes these limits allow.
	[[nodiscard]] constexpr auto prepared_byte_capacity(const diff_host_limits& aLimits) noexcept
		-> std::uint64_t
	{
		return aLimits.PreparedBytes < aLimits.CacheBytes / 8U ? aLimits.PreparedBytes : aLimits.CacheBytes / 8U;
	}

	/// \brief One comparison to prepare before the reviewer opens it.
	///
	/// \details [[BPR-1327]]. The key is exactly the one the reviewer's own
	/// request for the file would carry, so opening the file finds it.
	struct prepared_diff_request
	{
		core::file_comparison_key Key;
		core::diff_options        Options;

		auto operator<=>(const prepared_diff_request&) const = default;
	};

	enum class diff_blob_status : std::uint8_t {
		available,
		missing,
		binary,
		decode_failure,
		materialization_limit,
		generated_file_unsupported,
		cancelled,
		failure,
	};

	/// \brief Describes the diff blob request record.
	struct diff_blob_request
	{
		core::diff_request_context Context;
		core::diff_side            Side = core::diff_side::left;
		core::git_blob_oid         Blob;
		std::uint64_t              MaximumBytes = 0U;

		auto operator<=>(const diff_blob_request&) const = default;
	};

	/// \brief Describes the diff blob observation record.
	struct diff_blob_observation
	{
		core::diff_request_context Context;
		core::diff_side            Side = core::diff_side::left;
		core::git_blob_oid         Blob;
		diff_blob_status           Status = diff_blob_status::failure;
		std::vector<std::uint8_t>  Bytes;

		auto operator<=>(const diff_blob_observation&) const = default;
	};

	/// \brief Provides the diff blob port contract.
	class diff_blob_port
	{
	public:
		virtual ~diff_blob_port() = default;

		[[nodiscard]] virtual auto materialize(
			const diff_blob_request& aRequest,
			std::stop_token          aStop) -> diff_blob_observation = 0;
	};

	/// \brief Describes the diff publication record.
	struct diff_publication
	{
		core::diff_request_context                      Context;
		std::shared_ptr<const core::computed_file_diff> Result;
		/// Prepared on the effect lane and bound to Result by exact ownership.
		std::shared_ptr<const core::diff_semantic_index> Index;
		bool                                             CacheHit = false;
	};

	/// \brief Describes the diff failure publication record.
	struct diff_failure_publication
	{
		core::diff_request_context Context;
		core::diff_reason_code     Reason = core::diff_reason_code::host_failure;

		auto operator<=>(const diff_failure_publication&) const = default;
	};

	/// \brief Provides the diff projection port contract.
	class diff_projection_port
	{
	public:
		virtual ~diff_projection_port() = default;

		// Result is immutable. Presentation may retain the shared value for its
		// projection lifetime but cannot mutate or become semantic authority.
		virtual void publish(const diff_publication& aPublication)              = 0;
		virtual void fail(const diff_failure_publication& aFailure)             = 0;
		virtual void invalidate(const core::pull_request_authority& aAuthority) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_RUNTIME_PORTS_HPP
