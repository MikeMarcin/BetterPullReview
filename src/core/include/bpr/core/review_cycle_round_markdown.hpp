#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ROUND_MARKDOWN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ROUND_MARKDOWN_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_round.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief The readable form of a frozen round (RFC-BPR-0056, BPR-1159): one
/// complete Markdown document that a person or any tool can act on without
/// BPR, a model or network access. Copy to Clipboard and Save to File emit
/// exactly these bytes.
/// \details The document is a projection of the canonical payload: the
/// assignment, every item in order with its raw body, locations and
/// relations, every suggestion's exact bytes, and every selected source. For
/// the pack's example round it is byte-for-byte the pack's `review.md`.
/// Fences are longer than any backtick run they enclose. A body whose own
/// Markdown would change the document's structure (an unclosed fence, a
/// top-level heading, an unclosed HTML comment or raw block) is shown
/// verbatim in a fence instead. Bytes a fence cannot carry exactly (CR, NUL
/// and other controls, invalid UTF-8, trailing spaces, no final line break)
/// are labelled and also given as base64. Nothing is truncated: a document
/// over its budget is refused with its size.
namespace bpr::core {
	/// Clipboard text is kept within this size; a larger round is saved instead.
	inline constexpr std::uint64_t maximum_round_clipboard_bytes = 16U * 1024U * 1024U;
	/// A saved document is kept within this size.
	inline constexpr std::uint64_t maximum_round_document_file_bytes = 256U * 1024U * 1024U;

	/// \brief Why a rendering was not produced: its full size and what it holds.
	struct cycle_round_output_limit
	{
		std::uint64_t RequiredBytes                                      = 0;
		std::uint64_t BudgetBytes                                        = 0;
		std::uint64_t Items                                              = 0;
		std::uint64_t Sources                                            = 0;
		auto          operator<=>(const cycle_round_output_limit&) const = default;
	};

	/// \brief The complete readable document for a decoded round.
	[[nodiscard]] auto render_round_markdown(const cycle_decoded_round& aRound) -> std::string;
	/// \brief The complete document, or its size when over `aBudgetBytes`.
	[[nodiscard]] auto render_round_markdown(const cycle_decoded_round& aRound, std::uint64_t aBudgetBytes) -> std::expected<std::string, cycle_round_output_limit>;

	enum class cycle_round_file_kind : std::uint8_t {
		markdown,
		bundle,
	};
	/// \brief A default file name made only from wire identities, such as
	/// `RC-3-F2.md` or `RC-3-F2.bpr-review.zip`: never a label or a path.
	[[nodiscard]] auto round_file_name(const cycle_round_payload& aPayload, cycle_round_file_kind aKind) -> std::string;

	/// \brief What saving to a chosen file does, given what is there now.
	enum class cycle_round_save_action : std::uint8_t {
		/// Nothing is there: create the file.
		create,
		/// The same bytes are already there: write nothing.
		already_saved,
		/// Other bytes are there and replacing them was not confirmed.
		conflict,
		/// Other bytes are there and the reviewer confirmed replacing exactly them.
		replace,
	};
	[[nodiscard]] auto stable_name(cycle_round_save_action aValue) noexcept -> std::string_view;
	/// \brief Decides a save. `aConfirmed` is the existing file's digest the
	/// reviewer agreed to replace; a file that changed since is a conflict.
	[[nodiscard]] auto decide_round_save(const std::optional<cycle_material_digest>& aExisting, const cycle_material_digest& aNew,
										 const std::optional<cycle_material_digest>& aConfirmed) noexcept -> cycle_round_save_action;
}

#endif
