#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_JSON_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_JSON_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief The Review Cycles wire encoding (RFC-BPR-0056): a strict JSON
/// reader and the `bpr.cycle.canonical-json.v1` writer, with strict base64.
/// \details The canonical form is this product's own profile, not RFC 8785:
/// UTF-8, object keys sorted by their bytes (which is code point order),
/// array order preserved, no insignificant whitespace, no floating-point
/// values and no trailing newline. `"`, `\` and the control characters are
/// escaped (`\b \t \n \f \r` by name, the rest as lowercase `\u00xx`);
/// every other scalar is written as its UTF-8 bytes. Nothing is Unicode
/// normalized. The reader refuses duplicate keys, invalid UTF-8, lone
/// surrogates, fractions, exponents, integers beyond 2^53-1 and nesting
/// deeper than 32 before any value is admitted.
namespace bpr::core {
	inline constexpr std::int64_t maximum_cycle_json_integer = 9'007'199'254'740'991;
	inline constexpr std::size_t  maximum_cycle_json_depth   = 32U;

	struct cycle_json_member;
	/// \brief One JSON value. Objects keep their members in the order given;
	/// the canonical writer sorts them, and the reader returns them sorted.
	struct cycle_json_value
	{
		using array  = std::vector<cycle_json_value>;
		using object = std::vector<cycle_json_member>;
		std::variant<std::monostate, bool, std::int64_t, std::string, array, object> Value;
	};
	struct cycle_json_member
	{
		std::string      Key;
		cycle_json_value Value;
	};

	enum class cycle_json_error : std::uint8_t {
		syntax,
		/// Invalid UTF-8, or an escaped lone surrogate.
		encoding,
		duplicate_key,
		/// Nesting deeper than `maximum_cycle_json_depth`.
		depth,
		/// A fraction, an exponent or an integer beyond the exact range.
		number,
		too_large,
	};
	[[nodiscard]] auto stable_name(cycle_json_error aValue) noexcept -> std::string_view;

	/// \brief Reads one JSON document of at most `aMaximumBytes`. Whitespace
	/// may surround it; a byte-order mark or trailing content is refused.
	[[nodiscard]] auto parse_cycle_json(std::string_view aText, std::size_t aMaximumBytes) -> std::expected<cycle_json_value, cycle_json_error>;
	/// \brief Writes the canonical form. Nothing when a string is not valid
	/// UTF-8, an integer is out of range, an object repeats a key or nesting
	/// is too deep: such a value has no canonical form.
	[[nodiscard]] auto canonical_cycle_json(const cycle_json_value& aValue) -> std::optional<std::string>;

	/// \brief The member named `aKey`, or null.
	[[nodiscard]] auto find_member(const cycle_json_value::object& aObject, std::string_view aKey) noexcept -> const cycle_json_value*;
	/// \brief True when the object's keys are exactly `aKeys`, in any order.
	[[nodiscard]] auto has_exactly(const cycle_json_value::object& aObject, std::span<const std::string_view> aKeys) noexcept -> bool;

	/// \brief True for well-formed UTF-8 scalar values (NUL included).
	[[nodiscard]] auto valid_cycle_utf8(std::string_view aText) noexcept -> bool;

	/// \brief Standard padded base64.
	[[nodiscard]] auto encode_cycle_base64(std::string_view aBytes) -> std::string;
	/// \brief Strict base64: padded, canonical (unused bits zero), alphabet
	/// only, no whitespace. Nothing for anything else.
	[[nodiscard]] auto decode_cycle_base64(std::string_view aText) -> std::optional<std::string>;
	/// \brief Lowercase hexadecimal.
	[[nodiscard]] auto cycle_hex(std::span<const std::uint8_t> aBytes) -> std::string;
	/// \brief Reads exactly 64 lowercase hexadecimal digits.
	[[nodiscard]] auto parse_cycle_sha256(std::string_view aText) -> std::optional<std::array<std::uint8_t, 32>>;
}

#endif
