#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FILE_PROGRESS_MARK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FILE_PROGRESS_MARK_HPP

#include "bpr/core/review_ledger.hpp"

#include <cstdint>
#include <string_view>

/// \file
/// \brief [[BPR-1331]]. The one mark that sums up a file's private progress.
namespace bpr::core {
	/// \brief The single state a file's private progress shows at a glance.
	///
	/// \details A receipt holds three independent facts: how far the file was
	/// read, whether it was accepted and whether it was excluded. A control
	/// that shows one glyph needs one answer. Exclusion wins, because an
	/// excluded file has left the review; then acceptance, the strongest
	/// claim a reader makes about included material; then the read
	/// progression. The core cannot hold a receipt that is both accepted and
	/// excluded (`apply` refuses either order), so the precedence between
	/// those two only matters for a receipt built by hand.
	enum class file_progress_mark : std::uint8_t {
		unread,
		seen,
		read,
		accepted,
		excluded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(file_progress_mark aMark) noexcept -> std::string_view;

	/// \brief Decides the mark a receipt shows.
	[[nodiscard]] auto progress_mark(const file_receipt& aReceipt) noexcept -> file_progress_mark;

	/// \brief [[BPR-1339]]. Whether a file counts as done in my review progress.
	///
	/// \details A file is done once it is read or accepted. The owner,
	/// 2026-10-08: "Rule gap, marking it read updates it to read. Accepted is
	/// stronger than read or unread". Acceptance does not set the read fact,
	/// so revoking it leaves the file as far read as it was; it only counts
	/// the file as done while it holds. Exclusion is not considered: an
	/// excluded file counts by how far it was read, as it always has.
	[[nodiscard]] auto progress_done(const file_receipt& aReceipt) noexcept -> bool;
} // namespace bpr::core

#endif
