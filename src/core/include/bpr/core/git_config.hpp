#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GIT_CONFIG_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GIT_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief A Git config file read with Git's own syntax (RFC-BPR-0083).
/// \details Sections and keys fold to lower case; a quoted subsection keeps
/// its case, the deprecated `[section.sub]` form folds it. Values follow Git:
/// unquoted runs of white space become single spaces and are dropped at
/// either end, `"` toggles quoting, `#` and `;` start a comment outside
/// quotes, a trailing `\` continues the line, and only `\\`, `\"`, `\n`,
/// `\t` and `\b` escape. A file Git would refuse to read is refused whole.
/// `include` and `includeIf` are entries like any other: nothing here opens
/// another file.
namespace bpr::core {
	/// \brief More entries than any repository config needs; a larger file is
	/// refused rather than read partly.
	inline constexpr std::size_t maximum_git_config_entries = 65'536U;

	/// \brief One `key = value` line.
	struct git_config_entry
	{
		/// Lower case, without the subsection.
		std::string                Section;
		std::optional<std::string> Subsection;
		/// Lower case.
		std::string Key;
		/// Absent for a bare `key` line, which Git reads as boolean true.
		std::optional<std::string> Value;
		auto                       operator<=>(const git_config_entry&) const = default;
	};

	/// \brief The entries in file order, or nothing when Git would report a
	/// bad config line (or the entry bound is exceeded).
	[[nodiscard]] auto parse_git_config(std::string_view aText) -> std::optional<std::vector<git_config_entry>>;

	/// \brief Git's boolean: `true`, `yes`, `on`, `false`, `no`, `off`, the
	/// empty value, or an integer (with an optional `k`, `m` or `g` unit),
	/// nonzero being true. A bare key is true. Nothing for anything else,
	/// which Git refuses as a bad boolean.
	[[nodiscard]] auto git_config_bool(const std::optional<std::string>& aValue) -> std::optional<bool>;

	/// \brief Git's integer: optional sign, decimal digits and an optional
	/// `k`, `m` or `g` unit. Nothing for a bare key, an empty value, other
	/// text or a value outside 32 bits, all of which Git refuses.
	[[nodiscard]] auto git_config_int(const std::optional<std::string>& aValue) -> std::optional<std::int32_t>;
}

#endif
