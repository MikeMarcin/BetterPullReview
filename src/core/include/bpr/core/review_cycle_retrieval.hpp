#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_RETRIEVAL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_RETRIEVAL_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_round.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Reading one frozen round from outside the review (RFC-BPR-0058,
/// [[BPR-1169]]): the round's measure, its items a page at a time and its
/// source attachments, all bound to the round's immutable digest.
/// \details A consumer must hold every instruction before it implements any,
/// so nothing here shortens the instruction set: an item is delivered whole or
/// not at all, a page names exactly where it starts and where the next one
/// does, and a cursor from another round or digest is refused. A consumer
/// that states how much it can hold is told plainly when the round is larger,
/// so it asks for a partition instead of acting on the first page. Source
/// attachments are only those the round includes; nothing here can name
/// other material. Who may read is decided by `authorize` in
/// `review_cycle_policy.hpp`, not here.
namespace bpr::core {
	/// The page a read returns unless the consumer asks for a smaller one.
	inline constexpr std::uint64_t default_round_page_bytes = 1024U * 1024U;
	inline constexpr std::uint64_t minimum_round_page_bytes = 4U * 1024U;
	/// No page is larger: an item over this is never split, only refused.
	inline constexpr std::uint64_t maximum_round_page_bytes = 2U * 1024U * 1024U;
	/// The most attachments one listing names.
	inline constexpr std::size_t round_source_list_entries = 256U;
	/// The most attachment bytes one read returns.
	inline constexpr std::uint64_t maximum_round_source_chunk_bytes = 1024U * 1024U;

	/// \brief What a round holds, as a consumer weighs it.
	struct cycle_round_extent
	{
		std::uint64_t Items       = 0;
		std::uint64_t Suggestions = 0;
		std::uint64_t Sources     = 0;
		/// The canonical bytes of every item plus the assignment's texts:
		/// what a consumer must hold before implementing.
		std::uint64_t InstructionBytes = 0;
		/// Attachment bytes, read on demand.
		std::uint64_t SourceBytes                                  = 0;
		auto          operator<=>(const cycle_round_extent&) const = default;
	};
	/// \brief Measures a round. Nothing when an item has no canonical form.
	[[nodiscard]] auto measure_round(const cycle_round_payload& aPayload) -> std::optional<cycle_round_extent>;

	/// \brief Where the next page of one round starts, and how large pages are.
	struct cycle_round_cursor
	{
		cycle_round_id     Round;
		cycle_round_digest Digest;
		/// Index of the first item of the next page.
		std::uint64_t Next                                         = 0;
		std::uint64_t PageBytes                                    = default_round_page_bytes;
		auto          operator<=>(const cycle_round_cursor&) const = default;
	};
	/// \brief `F<n>.<payload sha256>.<next item>.<page bytes>`: a reference,
	/// not a grant; it opens nothing the grant does not.
	[[nodiscard]] auto encode_round_cursor(const cycle_round_cursor& aCursor) -> std::string;
	[[nodiscard]] auto parse_round_cursor(std::string_view aText) -> std::optional<cycle_round_cursor>;

	enum class cycle_retrieval_problem : std::uint8_t {
		/// Page bytes outside `minimum_round_page_bytes` to `maximum_round_page_bytes`.
		page_size,
		/// A cursor of another round or digest, or past the last item.
		cursor,
		/// The instructions exceed what the consumer said it can hold: it
		/// needs a partition, not the first page.
		over_budget,
		/// One item is larger than any page: it is never split.
		item_over_page,
		/// The round has no attachment by that identity.
		unknown_source,
		/// A range outside the attachment, empty or reversed.
		source_range,
		/// A listing position past the last attachment.
		source_cursor,
	};
	[[nodiscard]] auto stable_name(cycle_retrieval_problem aValue) noexcept -> std::string_view;

	/// \brief Consecutive whole items of one round.
	struct cycle_round_items_page
	{
		/// Index of the first item on this page.
		std::uint64_t First = 0;
		/// Each item's canonical JSON, exactly as the payload holds it.
		std::vector<std::string> Items;
		std::uint64_t            Bytes = 0;
		/// Where the next page starts; nothing on the last page.
		std::optional<cycle_round_cursor> Next;
	};
	/// \brief The items of `aRound` from index `aFrom`, as many whole items as
	/// fit `aPageBytes`, and at least one. `aId` and the round's digest bind
	/// the next cursor.
	[[nodiscard]] auto page_round_items(const cycle_decoded_round& aRound, cycle_round_id aId, std::uint64_t aFrom, std::uint64_t aPageBytes)
		-> std::expected<cycle_round_items_page, cycle_retrieval_problem>;
	/// \brief Checks a cursor against the round it is presented for.
	[[nodiscard]] auto admit_round_cursor(const cycle_round_cursor& aCursor, cycle_round_id aRound, const cycle_round_digest& aDigest, const cycle_round_extent& aExtent) noexcept
		-> std::optional<cycle_retrieval_problem>;
	/// \brief `over_budget` when the instructions exceed `aBudget` bytes.
	[[nodiscard]] auto admit_round_budget(const cycle_round_extent& aExtent, std::uint64_t aBudget) noexcept -> std::optional<cycle_retrieval_problem>;

	/// \brief One listing of a round's attachments.
	struct cycle_round_source_listing
	{
		std::uint64_t First = 0;
		std::uint64_t Count = 0;
		/// Where the next listing starts; nothing on the last.
		std::optional<std::uint64_t> Next;
	};
	[[nodiscard]] auto list_round_sources(const cycle_round_payload& aPayload, std::uint64_t aFrom) -> std::expected<cycle_round_source_listing, cycle_retrieval_problem>;

	/// \brief A byte range within one attachment's bytes, half-open.
	struct cycle_source_range
	{
		std::uint64_t Start                                        = 0;
		std::uint64_t End                                          = 0;
		auto          operator<=>(const cycle_source_range&) const = default;
	};
	/// \brief `START:END`, decimal, half-open. Nothing for any other spelling.
	[[nodiscard]] auto parse_source_range(std::string_view aText) -> std::optional<cycle_source_range>;
	/// \brief The bytes of one attachment a read returns: the range asked for,
	/// or the whole attachment, up to `maximum_round_source_chunk_bytes`.
	struct cycle_round_source_slice
	{
		/// Index into the payload's sources.
		std::size_t        Source = 0;
		cycle_source_range Range;
		/// The rest of the range asked for, when it was more than one read returns.
		std::optional<cycle_source_range> Next;
	};
	[[nodiscard]] auto slice_round_source(const cycle_round_payload& aPayload, std::string_view aSource, const std::optional<cycle_source_range>& aRange)
		-> std::expected<cycle_round_source_slice, cycle_retrieval_problem>;
}

#endif
