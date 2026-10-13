#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_UTF8_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_UTF8_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief The functional core's one UTF-8 and Unicode toolkit, on utf8proc.
/// \details [[BPR-1434]]. Replaces hand-written decoders, encoders and Unicode
/// tables. Well-formed means shortest form, no surrogate and at most U+10FFFF;
/// U+0000 is well formed, so callers refuse NUL separately where they must.
namespace bpr::core::utf8 {
	/// \brief Decodes the scalar at `aOffset` and advances past it; nothing,
	/// with `aOffset` unchanged, when the sequence there is ill formed.
	[[nodiscard]] auto decode(std::string_view aText, std::size_t& aOffset) noexcept -> std::optional<char32_t>;
	/// \brief Whether every byte of `aText` belongs to a well-formed scalar.
	[[nodiscard]] auto valid(std::string_view aText) noexcept -> bool;
	/// \brief Appends the UTF-8 encoding of a scalar value; false, appending
	/// nothing, for a surrogate or a value above U+10FFFF.
	auto append(std::string& aOut, char32_t aValue) -> bool;
	/// \brief The canonical decomposition (NFD) of well-formed UTF-8, with
	/// combining marks in canonical order; nothing when `aText` is ill formed.
	[[nodiscard]] auto decompose(std::string_view aText) -> std::optional<std::u32string>;
	/// \brief The one-to-one case fold of a scalar: its simple uppercase mapping,
	/// lowered again, as case-insensitive file systems compare names (so σ and
	/// ς meet, and ß stays ß).
	[[nodiscard]] auto simple_fold(char32_t aValue) noexcept -> char32_t;
	/// \brief The end of the extended grapheme cluster (UAX #29) that starts at
	/// `aOffset`, which must be a cluster boundary. A byte that begins no
	/// well-formed scalar is a cluster of its own, so ill-formed text still
	/// steps byte by byte. At or past the end, the size of `aText`.
	[[nodiscard]] auto grapheme_end(std::string_view aText, std::size_t aOffset) noexcept -> std::size_t;
	/// \brief The display columns one cluster takes in a monospaced grid.
	/// \details [[BPR-1438]]. The widest scalar in it: 2 for East Asian wide
	/// and fullwidth characters and emoji presentation, 0 for marks, joiners
	/// and other continuations. A cluster with U+FE0F or a regional-indicator
	/// pair (a flag) is drawn as an emoji, so it takes 2. Every cluster takes at
	/// least 1, so a lone mark, a control or an ill-formed byte still has a
	/// cell. A tab's stop is the caller's rule.
	[[nodiscard]] auto grapheme_width(std::string_view aCluster) noexcept -> std::uint32_t;
	/// \brief How word movement treats a scalar.
	enum class word_class : std::uint8_t {
		/// A space or tab, or another space separator (Zs).
		space,
		/// A letter, mark, number or connector punctuation such as `_`.
		word,
		/// Any other punctuation, symbol, control or unassigned scalar.
		other,
	};
	/// \brief The word class of a scalar, from its Unicode general category.
	[[nodiscard]] auto classify_word(char32_t aValue) noexcept -> word_class;
}

#endif
