#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_KNOWN_NAMESPACES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_KNOWN_NAMESPACES_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1714]]. The C++ namespaces a reviewer names as known.
/// \details The owner asked on 2026-10-21 for "a setting for a list of well
/// known namespaces", pre-populated with `std`, to which users add their own.
/// A known namespace is predeclared before the C++ refinement reads a blob, as
/// if the blob declared it, so it qualifies a name as a namespace wherever the
/// blob gives no other evidence. A class or enum of that name the blob itself
/// declares still wins in its scope ([[BPR-1710]]).
///
/// An entry is one identifier, or a nested path of identifiers joined by `::`
/// (`boost::asio`), which declares every segment. `std` brings the namespaces
/// ISO C++ nests in it (`std::chrono`, `std::ranges`, `std::views` and the
/// rest) with it, so the list stays a short list of top-level names. The list
/// is an explicit input to classification and part of the classification
/// cache key: equal lists classify equal blobs equally.
namespace bpr::core {
	/// \brief The most entries a list may hold.
	inline constexpr std::size_t maximum_known_namespaces = 64U;
	/// \brief The longest entry, in bytes, `::` separators included.
	inline constexpr std::size_t maximum_known_namespace_bytes = 128U;

	/// \brief Why one entry is not a namespace name.
	enum class known_namespace_problem : std::uint8_t {
		/// Nothing on one side of a `::`: `::boost`, `boost::`, `a::::b`, or an
		/// empty entry.
		empty_part,
		/// A byte other than an ASCII letter, digit, underscore or `::`.
		invalid_character,
		/// A part that starts with a digit.
		leading_digit,
		/// A part that is a C++ keyword or alternative token.
		keyword,
		/// Longer than `maximum_known_namespace_bytes`.
		too_long,
		/// The same entry already appears earlier in the list.
		duplicate,
		/// Past `maximum_known_namespaces` entries.
		too_many,
	};

	/// \brief One refused entry, by its zero-based position in the list.
	struct known_namespace_rejection
	{
		std::size_t             Index   = 0U;
		known_namespace_problem Problem = known_namespace_problem::invalid_character;

		auto operator<=>(const known_namespace_rejection&) const = default;
	};

	/// \brief The reviewer's list, in the order they wrote it.
	/// \details A default-constructed list is the default setting: `std` alone.
	/// An empty list is a legal choice that predeclares nothing, which is how
	/// C++ classified before [[BPR-1710]].
	struct known_namespaces
	{
		std::vector<std::string> Names{ std::string{ "std" } };

		auto operator<=>(const known_namespaces&) const = default;
	};

	/// \brief Checks one entry on its own; duplicates and the count are list facts.
	[[nodiscard]] auto check_known_namespace(std::string_view aName)
		-> std::optional<known_namespace_problem>;

	/// \brief Every refused entry of a list, in list order; empty when it is legal.
	[[nodiscard]] auto validate_known_namespaces(const known_namespaces& aNamespaces)
		-> std::vector<known_namespace_rejection>;

	/// \brief Splits what a reviewer typed into entries, without judging them.
	/// \details Commas and ASCII white space separate entries; empty ones are
	/// dropped. `boost :: asio` is three entries, so its `::` is refused rather
	/// than silently joined.
	[[nodiscard]] auto split_known_namespaces(std::string_view aText) -> known_namespaces;

	/// \brief Writes a list the way the settings field shows it: `std, Qt`.
	[[nodiscard]] auto join_known_namespaces(const known_namespaces& aNamespaces) -> std::string;

	/// \brief The legal entries of a list, sorted and unique.
	/// \details What classification consumes and the cache key carries, so two
	/// lists that predeclare the same namespaces share one identity whatever
	/// order they were written in. Refused entries are dropped, and at most
	/// `maximum_known_namespaces` legal entries, taken in list order, are kept.
	[[nodiscard]] auto canonical_known_namespaces(const known_namespaces& aNamespaces)
		-> known_namespaces;
}

#endif
