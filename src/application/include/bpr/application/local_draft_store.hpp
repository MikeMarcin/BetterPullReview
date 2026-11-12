#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_STORE_HPP

#include "bpr/core/local_draft.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

/// \file
/// \brief The profile's private store of local PR drafts (RFC-BPR-0108).
/// \details Each draft is one record with a version; every write names the
/// version it read, so a competing write is refused, never merged. A
/// candidate's material (both sides of every changed file it keeps) is
/// installed in the same transaction as the record that names it, so a
/// restart recovers the prior or the new candidate, never part of one. The
/// store is bounded: a write past its byte or draft bound is refused with
/// the amounts, and nothing is evicted. Discarding a draft removes its record
/// and every object only it named. Nothing here reads or writes a worktree.
namespace bpr::application {
	/// \brief The most bytes of material the store keeps across all drafts.
	inline constexpr std::uint64_t maximum_local_draft_store_bytes = 1024ULL * 1024ULL * 1024ULL;

	/// \brief One exact object a candidate keeps.
	struct local_draft_object
	{
		core::git_entry_oid       Object;
		std::vector<std::uint8_t> Bytes;
	};

	enum class local_draft_store_failure : std::uint8_t {
		/// The store could not be opened, read or written.
		unavailable,
		/// The draft changed since it was read.
		stale,
		/// No such draft.
		missing,
		/// The write would pass the store's byte bound, or its draft bound.
		full,
		/// A stored record is not one this build reads.
		corrupt,
	};

	struct local_draft_store_error
	{
		local_draft_store_failure Failure = local_draft_store_failure::unavailable;
		/// For `full`: what is stored and the bound.
		std::uint64_t Used  = 0;
		std::uint64_t Limit = 0;
		auto          operator<=>(const local_draft_store_error&) const = default;
	};

	template <typename T>
	using local_draft_store_result = std::expected<T, local_draft_store_error>;

	class local_draft_store
	{
	public:
		virtual ~local_draft_store() = default;

		/// \brief Every draft, in no particular order.
		virtual auto list() -> local_draft_store_result<std::vector<core::local_draft>> = 0;
		/// \brief One draft by identity.
		virtual auto find(const core::local_draft_id& aId) -> local_draft_store_result<core::local_draft> = 0;
		/// \brief Writes a new draft (`aPrevious` absent) or the next version
		/// of one stored at `aPrevious`, with any new objects, atomically.
		virtual auto put(const core::local_draft& aDraft, std::optional<std::uint64_t> aPrevious, std::span<const local_draft_object> aObjects)
			-> local_draft_store_result<void> = 0;
		/// \brief Removes a draft and the objects only it kept.
		virtual auto remove(const core::local_draft_id& aId) -> local_draft_store_result<void> = 0;
		/// \brief One kept object's bytes; absent when the store has none.
		virtual auto object(const core::git_entry_oid& aObject) -> local_draft_store_result<std::optional<std::vector<std::uint8_t>>> = 0;
	};
}

#endif
