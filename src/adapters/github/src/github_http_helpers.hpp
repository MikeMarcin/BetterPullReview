#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_HTTP_HELPERS_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_HTTP_HELPERS_HPP

// [[BPR-1439]]. The GitHub adapter's one `Link` header tokenizer, JSON
// media-type predicate and percent-encoder. Each reader keeps its own URL and
// strictness policy on top of the tokens; none of these decides what a page
// or a target means. The header is standard C++ so the gh response reader and
// its fuzzer can include it.

#include "github_read_contract.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::github::detail {
	/// \brief The longest `Link` header value the tokenizer reads.
	inline constexpr std::size_t maximum_link_header_bytes = 16U * 1024U;
	/// \brief The most link values one `Link` header may carry.
	inline constexpr std::size_t maximum_link_values = 64U;

	/// \brief One `; name=value` parameter of a link value, as written.
	struct link_parameter
	{
		std::string_view Name;
		/// The value without its quotes.
		std::string_view Value;
		bool             Quoted = false;
	};

	/// \brief One `<target>` and its parameters. Views into the parsed text.
	struct link_value
	{
		std::string_view            Target;
		std::vector<link_parameter> Parameters;
	};

	/// \brief Tokenizes an RFC 8288 `Link` value: comma-separated
	/// `<target>` entries, each with `;`-separated `name=value` parameters
	/// (token or quoted, no escapes), optional space or tab around every
	/// delimiter. The target is visible ASCII without `<` or `"`. Nothing when
	/// the value is empty, over a bound, or not of that shape.
	[[nodiscard]] auto parse_link_header(std::string_view aValue)
		-> std::optional<std::vector<link_value>>;

	/// \brief The relations of a link's one `rel` parameter, split on single
	/// spaces. Nothing when the link has no `rel`, more than one, or an empty
	/// relation (leading, trailing or doubled space).
	[[nodiscard]] auto link_relations(const link_value& aLink)
		-> std::optional<std::vector<std::string_view>>;

	/// \brief Whether `aRelation` is one of GitHub's pagination relations:
	/// `next`, `prev`, `first` or `last`.
	[[nodiscard]] auto pagination_relation(std::string_view aRelation) noexcept -> bool;

	/// \brief The completeness a REST response's `Link` header states, for the
	/// gh response reader and the native transport: complete without a header;
	/// otherwise every link needs exactly one `rel` whose relations are all
	/// pagination relations, and a `next` among them means another page.
	/// Nothing for an empty or malformed header.
	[[nodiscard]] auto link_completeness(const std::optional<std::string>& aLink)
		-> std::optional<page_completeness>;

	/// \brief Whether a `Content-Type` value is JSON: `application/json`,
	/// optionally with exactly the parameter `charset=utf-8`, ASCII
	/// case-insensitive, space or tab around the parts, at most 256 bytes.
	[[nodiscard]] auto json_media_type(std::string_view aValue) noexcept -> bool;

	/// \brief Percent-encodes `aBytes` byte for byte: RFC 3986 unreserved
	/// characters and the bytes in `aKeep` stay, every other byte becomes
	/// `%XX` with uppercase digits. Invalid UTF-8 is encoded as it is; no
	/// locale is consulted.
	[[nodiscard]] auto percent_encode(std::string_view aBytes, std::string_view aKeep = { })
		-> std::string;
}

#endif
