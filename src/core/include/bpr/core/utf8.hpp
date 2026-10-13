#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_UTF8_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_UTF8_HPP

#include <cstddef>
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
}

#endif
