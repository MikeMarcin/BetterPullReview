#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKSPACE_GIT_ISOLATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKSPACE_GIT_ISOLATION_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The configuration BetterPullReview's own Git runs with in a retained
/// agent workspace (RFC-BPR-0096), whose `.git/config`, `.git/info/attributes`
/// and `.gitattributes` the agent can write.
/// \details Git has no switch that skips the repository's configuration, so
/// the isolation is built from what that configuration names. Every value
/// here is set at command scope, which outranks the repository's own:
///
/// - The fixed overrides disable every single-valued key that can start a
///   program, or change what the move reads, in the verbs the move runs.
/// - A driver runs only when the configuration defines a command for the
///   name an attribute gives. Each filter, diff and merge driver the
///   repository's configuration names (through includes too) has its
///   command keys set empty: Git then runs nothing for it, and an attribute
///   naming it is inert. The set is the driver names actually defined, so it
///   does not grow with the names an agent might choose.
/// - A `hook.<name>` key is refused, not overridden: a configured hook is
///   not covered by the private hooks path, and an empty value is not known
///   to disable one.
namespace bpr::core {
	/// \brief More drivers than any workspace defines; more are refused.
	inline constexpr std::size_t maximum_workspace_git_drivers = 64U;

	/// \brief One `key=value` Git reads at command scope.
	struct git_config_override
	{
		std::string Key;
		std::string Value;
		auto        operator<=>(const git_config_override&) const = default;
	};

	/// \brief Why BetterPullReview's Git does not run in a workspace.
	enum class workspace_git_refusal : std::uint8_t {
		none,
		/// The listing is not Git's NUL-terminated key names.
		malformed_listing,
		/// A `hook.<name>` key: a configured hook.
		configured_hook,
		/// More than `maximum_workspace_git_drivers` drivers.
		too_many_drivers,
	};

	/// \brief The command-scope configuration for one Git command in an
	/// agent workspace, or why it must not run.
	struct workspace_git_isolation
	{
		std::vector<git_config_override> Overrides;
		workspace_git_refusal            Refusal                                           = workspace_git_refusal::none;
		auto                             operator<=>(const workspace_git_isolation&) const = default;
	};

	/// \brief The overrides every Git command in an agent workspace carries,
	/// whatever its configuration lists. The private hooks path is the
	/// host's and is not among them.
	[[nodiscard]] auto fixed_workspace_git_overrides() -> std::vector<git_config_override>;

	/// \brief The isolation for \p aListing, the output of
	/// `git config --list --name-only --includes -z` run in the workspace.
	/// \details The fixed overrides come first, then each driver's in the
	/// order the listing first names it. Equal listings give equal results.
	[[nodiscard]] auto plan_workspace_git_isolation(std::string_view aListing) -> workspace_git_isolation;
}

#endif
