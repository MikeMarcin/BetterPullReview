#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IGNORE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IGNORE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Which untracked paths a capture leaves out by default
/// (RFC-BPR-0054).
/// \details Git roots follow the checkout's own `.gitignore` files and
/// `info/exclude`; the reviewer's global excludes file is not read, so a path
/// only it ignores stays visible as an untracked candidate rather than being
/// hidden. Folder roots use a small disclosed exclusion set. Tracked files
/// are never excluded by either.
namespace bpr::core {
	inline constexpr std::size_t maximum_ignore_rules = 100'000U;

	/// \brief Git-style wildcard match: `*` and `?` stop at `/`, `[...]`
	/// matches one byte (with `!` or `^` negation and ranges), a `**` segment
	/// matches any number of directories and `\` escapes. POSIX character
	/// classes are not supported and match nothing.
	[[nodiscard]] auto wildcard_match(std::string_view aPattern, std::string_view aPath) -> bool;

	/// \brief The ignore rules seen so far, lowest precedence first.
	class workspace_ignore_rules
	{
	public:
		/// \brief Adds one exclude file's rules. `aBase` is the root-relative
		/// directory holding it (empty for the root and for `info/exclude`).
		/// Files must be added parents before children.
		/// \returns False when the rule bound would be exceeded; nothing is
		/// added then.
		auto add(std::string_view aBase, std::string_view aContents) -> bool;
		/// \brief True when the last rule matching the path excludes it.
		[[nodiscard]] auto ignored(std::string_view aPath, bool aDirectory) const -> bool;
		[[nodiscard]] auto size() const noexcept -> std::size_t
		{
			return Rules.size();
		}

	private:
		struct rule
		{
			std::string Base;
			std::string Pattern;
			bool        Negated       = false;
			bool        DirectoryOnly = false;
			/// The pattern has a `/`: it matches the path below its base, not
			/// just the last segment.
			bool Anchored = false;
		};
		std::vector<rule> Rules;
	};

	/// \brief The `filter` attribute from `.gitattributes` files, lowest
	/// precedence first. Other attributes are not interpreted.
	/// \details A filtered path's Git object is a transformation of its
	/// working bytes (LFS, for example); capture records that fact and never
	/// runs the filter.
	class workspace_attribute_rules
	{
	public:
		/// \brief Adds one attributes file's `filter` settings. `aBase` is the
		/// root-relative directory holding it. Files must be added parents
		/// before children. \returns False when the rule bound would be exceeded.
		auto add(std::string_view aBase, std::string_view aContents) -> bool;
		/// \brief Whether the last rule naming `filter` for the path sets it
		/// (true) or unsets it (false); nothing when no rule names it.
		[[nodiscard]] auto filter(std::string_view aPath) const -> std::optional<bool>;

	private:
		struct rule
		{
			std::string Base;
			std::string Pattern;
			bool        Anchored = false;
			bool        Set      = false;
		};
		std::vector<rule> Rules;
	};

	enum class workspace_exclusion_reason : std::uint8_t {
		/// Matched a `.gitignore` or `info/exclude` rule.
		ignored,
		/// In the disclosed default set: version-control and tool state,
		/// caches and common credential stores.
		default_set,
		/// Named by the capture request.
		requested,
	};

	/// \brief The disclosed default exclusions, matched against one name.
	[[nodiscard]] auto default_exclusion_names() noexcept -> std::span<const std::string_view>;
	/// \brief True when a name is in the disclosed default set.
	[[nodiscard]] auto excluded_by_default(std::string_view aName) noexcept -> bool;
}

#endif
