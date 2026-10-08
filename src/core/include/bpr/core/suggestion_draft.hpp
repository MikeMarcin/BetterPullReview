#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_DRAFT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_DRAFT_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1212]]. Suggest change on a pull request line: the draft's
/// first text, and what the suggestion in a draft changes.
/// \details Suggest change starts a draft whose body is a ```` ```suggestion ````
/// block holding the selected lines exactly as the head document holds them,
/// so an unedited suggestion changes nothing. While the reviewer edits the
/// block, the draft shows what it changes: the lines it removes and the lines
/// it adds, with the lines both keep at the start and the end left out. Both
/// are text decisions on values the caller already holds; nothing here reads
/// a file, asks a provider or decides whether GitHub will apply it
/// (`decide_suggestion_applicability` does that).
namespace bpr::core {
	/// \brief Lines `aFirst` to `aLast` (zero-based, inclusive) of a decoded
	/// document, without their line endings; none when the range is not in it
	/// or a line is not UTF-8, so a suggestion never carries bytes other than
	/// the file's.
	/// \details One line, or the range a multi-line draft covers
	/// (`review_comment_draft::StartLine` to its anchor).
	[[nodiscard]] auto suggestion_source_lines(const decoded_text_document& aDocument, std::uint32_t aFirst, std::uint32_t aLast)
		-> std::optional<std::vector<std::string>>;

	/// \brief A draft body that is only a suggestion block over these lines.
	/// \details The fence is three backticks, or one more than the longest run
	/// of backticks anywhere in the lines, so the block cannot be closed early
	/// by the code it quotes. The body ends with the closing
	/// fence and no newline: the caret belongs at the end of the last line
	/// inside the block, which is just before the body's last line break.
	[[nodiscard]] auto suggestion_fence_body(std::span<const std::string> aLines) -> std::string;

	/// \brief What the suggestion in a draft changes against the lines it replaces.
	struct suggested_change
	{
		/// The body holds a ```` ```suggestion ```` block.
		bool Fenced = false;
		/// The replaced lines the block no longer holds, in order.
		std::vector<std::string> Removed;
		/// The block's lines the replaced lines did not hold, in order.
		std::vector<std::string> Added;

		[[nodiscard]] auto unchanged() const noexcept -> bool
		{
			return Removed.empty() && Added.empty();
		}

		auto operator<=>(const suggested_change&) const = default;
	};

	/// \brief Reads the first suggestion block of `aBody` and compares its
	/// lines with `aOriginal`, leaving out the lines both start and end with.
	/// \details A block that is never closed runs to the end of the body, as
	/// it does while it is being typed. `\r` before a line break is not part
	/// of a line.
	[[nodiscard]] auto suggested_change_of(std::string_view aBody, std::span<const std::string> aOriginal) -> suggested_change;
} // namespace bpr::core

#endif
