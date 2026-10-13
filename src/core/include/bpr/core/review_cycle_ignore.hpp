#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IGNORE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IGNORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Which untracked paths a capture leaves out by default
/// (RFC-BPR-0054, RFC-BPR-0083).
/// \details Git roots follow the checkout's own `.gitignore` files and
/// `info/exclude`; the reviewer's global excludes file is not read, so a path
/// only it ignores stays visible as an untracked candidate rather than being
/// hidden. Folder roots use a small disclosed exclusion set. Tracked files
/// are never excluded by either.
namespace bpr::core {
	inline constexpr std::size_t maximum_ignore_rules = 100'000U;

	/// \brief Git's `wildmatch` with `WM_PATHNAME`: `*` and `?` stop at `/`,
	/// a `**` segment matches any number of directories, `\` escapes, and
	/// `[...]` matches one byte with `!` or `^` negation, ranges and the
	/// twelve POSIX classes. An unterminated bracket, an unknown class or a
	/// trailing `\` makes the pattern match nothing.
	/// \details `aCaseFold` is `WM_CASEFOLD`, which `core.ignoreCase` sets.
	/// It folds ASCII case exactly as Git does, including Git's quirks: an
	/// escaped character and a single character inside brackets keep their
	/// case, while ranges, `[:upper:]` and `[:lower:]` fold.
	[[nodiscard]] auto wildcard_match(std::string_view aPattern, std::string_view aPath, bool aCaseFold = false) -> bool;

	/// \brief The ignore rules seen so far, lowest precedence first.
	class workspace_ignore_rules
	{
	public:
		/// \param aIgnoreCase The repository's `core.ignoreCase`.
		explicit workspace_ignore_rules(bool aIgnoreCase = false) noexcept
			: IgnoreCase{ aIgnoreCase }
		{
		}

		/// \brief Adds one exclude file's rules, read as Git reads them. `aBase`
		/// is the root-relative directory holding it (empty for the root and
		/// for `info/exclude`). Files must be added parents before children.
		/// \returns False when the rule bound would be exceeded; nothing is
		/// added then.
		auto add(std::string_view aBase, std::string_view aContents) -> bool;
		/// \brief True when the last rule matching the path excludes it. Rules
		/// that excluded a parent directory are not consulted; the caller
		/// walks parents first, as Git does.
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
			/// The pattern has a `/` before its end: it matches the path below
			/// its base, not just the last segment.
			bool Anchored = false;
			/// Bytes any matching subject must contain (see `required_bytes`).
			std::array<std::uint64_t, 4> Required{ };
		};
		std::vector<rule> Rules;
		bool              IgnoreCase = false;
	};

	/// \brief An attribute's state for one path, as `git check-attr` reports it.
	struct git_attribute_state
	{
		enum class kind : std::uint8_t {
			unspecified,
			/// Set without a value (`attr`).
			set,
			/// Unset (`-attr`).
			unset,
			/// Set to `Value` (`attr=value`).
			value,
		};
		kind        Kind = kind::unspecified;
		std::string Value;
		auto        operator<=>(const git_attribute_state&) const = default;
	};

	/// \brief The `filter` attribute from `.gitattributes` files and
	/// `info/attributes`, with Git's precedence and macros. Other attributes
	/// are interpreted only as far as a macro needs them.
	/// \details A filtered path's Git object is a transformation of its
	/// working bytes (LFS, for example); capture records that fact and never
	/// runs the filter.
	class workspace_attribute_rules
	{
	public:
		/// \param aIgnoreCase The repository's `core.ignoreCase`.
		explicit workspace_attribute_rules(bool aIgnoreCase = false) noexcept
			: IgnoreCase{ aIgnoreCase }
		{
		}

		/// \brief Adds one `.gitattributes` file. `aBase` is the root-relative
		/// directory holding it; only the root file (empty base) may define
		/// macros. Files must be added parents before children.
		/// \returns False when the rule bound would be exceeded.
		auto add(std::string_view aBase, std::string_view aContents) -> bool;
		/// \brief Adds `info/attributes`, which outranks every `.gitattributes`
		/// file and may define macros.
		auto add_info(std::string_view aContents) -> bool;
		/// \brief The path's `filter` attribute.
		[[nodiscard]] auto filter(std::string_view aPath) const -> git_attribute_state;

	private:
		struct assignment
		{
			std::string         Name;
			git_attribute_state State;
		};
		struct rule
		{
			std::string                  Base;
			std::string                  Pattern;
			bool                         Anchored      = false;
			bool                         DirectoryOnly = false;
			std::vector<assignment>      Assignments;
			std::array<std::uint64_t, 4> Required{ };
		};
		/// Attribute names to their states, for one path's evaluation.
		using assigned_states = std::map<std::string_view, const git_attribute_state*, std::less<>>;
		/// Macro names to their definitions; a later definition replaces one.
		using macro_table = std::map<std::string, std::vector<assignment>, std::less<>>;
		auto parse(std::string_view aBase, std::string_view aContents, bool aMacros, std::vector<rule>& aRules, std::vector<std::pair<std::string, std::vector<assignment>>>& aDefinitions) const
			-> bool;
		void               fill(const std::vector<assignment>& aAssignments, assigned_states& aAssigned) const;
		[[nodiscard]] auto definition(std::string_view aName) const -> const std::vector<assignment>*;
		[[nodiscard]] auto matches(const rule& aRule, std::string_view aPath) const -> bool;

		std::vector<rule> Rules;
		std::vector<rule> InfoRules;
		macro_table       RootMacros;
		macro_table       InfoMacros;
		std::size_t       Assignments = 0;
		bool              IgnoreCase  = false;
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
