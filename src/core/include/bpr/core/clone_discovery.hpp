#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CLONE_DISCOVERY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CLONE_DISCOVERY_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0091 owner decision D2. The most of
	/// a `.git/config` the discovery reads.
	inline constexpr std::size_t maximum_clone_config_bytes = 64U * 1024U;
	/// \brief The most remotes one clone's configuration is read for.
	inline constexpr std::size_t maximum_clone_remotes = 64U;
	/// \brief The longest remote URL or name kept.
	inline constexpr std::size_t maximum_remote_text_bytes = 1024U;
	/// \brief The most clone folders a profile lists.
	inline constexpr std::size_t maximum_clone_folders = 32U;
	/// \brief The deepest a listed folder is searched below itself.
	inline constexpr std::size_t clone_search_depth = 2U;
	/// \brief The most directories looked at per listed folder.
	inline constexpr std::size_t maximum_clone_search_directories = 2'000U;

	/// \brief One `[remote "NAME"]` section's URL, as written.
	struct git_remote
	{
		std::string Name;
		std::string Url;

		auto operator<=>(const git_remote&) const = default;
	};

	/// \brief Reads the remotes out of one `.git/config`'s text.
	///
	/// \details The narrow subset of Git's configuration syntax that names a
	/// remote's URL: `[remote "NAME"]` headers (the section name in any case,
	/// the quoted subsection with `\"` and `\\` escapes), and `url` keys in
	/// any case with a value that is bare or quoted, a `#` or `;` outside
	/// quotes beginning a comment. Every other section, key and line is
	/// skipped, `include` and `includeIf` included: nothing is followed. Text
	/// past `maximum_clone_config_bytes`, remotes past `maximum_clone_remotes`
	/// and names or URLs past `maximum_remote_text_bytes` are not read. It is
	/// not a general configuration reader and claims none of Git's rules for
	/// line continuations or other keys.
	[[nodiscard]] auto parse_git_config_remotes(std::string_view aConfig) -> std::vector<git_remote>;

	/// \brief Where a remote URL points, without any user information.
	struct remote_location
	{
		/// Lower case, without a port.
		std::string Host;
		std::string Owner;
		std::string Name;

		auto operator<=>(const remote_location&) const = default;
	};

	/// \brief Normalizes one remote URL to its host and repository.
	///
	/// \details `https://`, `http://`, `ssh://` and `git://` URLs with an
	/// optional `USER[:PASSWORD]@` and port, and scp-like `[USER@]HOST:PATH`.
	/// The path must be exactly `OWNER/NAME`, with an optional `.git` and
	/// trailing `/`. User information is dropped before anything else: a token
	/// in `https://TOKEN@github.com/...` is never part of the answer. A local
	/// path, a `file://` URL or anything else is no location.
	[[nodiscard]] auto normalize_remote_url(std::string_view aUrl) -> std::optional<remote_location>;

	/// \brief The branch `.git/HEAD` names, or empty when detached or unread.
	[[nodiscard]] auto head_branch(std::string_view aHead) -> std::string;

	/// \brief One clone the host found under a listed folder, by metadata.
	struct clone_candidate
	{
		/// Absolute path of the working tree.
		std::string Path;
		/// The listed folder it was found under, or empty when the reviewer
		/// chose its folder directly.
		std::string             Folder;
		std::vector<git_remote> Remotes;
		std::string             Branch;

		auto operator<=>(const clone_candidate&) const = default;
	};

	/// \brief The repository a pull request belongs to, as a clone must name it.
	struct clone_target
	{
		/// The Git host's name, `github.com` or the GHES host, any case.
		std::string Host;
		std::string Owner;
		std::string Name;

		auto operator<=>(const clone_target&) const = default;
	};

	/// \brief One clone that names the target, with the remote that does.
	struct clone_match
	{
		std::string Path;
		std::string Folder;
		std::string Remote;
		/// `HOST/OWNER/NAME` as the remote names it, user information dropped.
		std::string Location;
		std::string Branch;

		auto operator<=>(const clone_match&) const = default;
	};

	/// \brief The candidates whose remotes name the target, in the Repository
	/// step's order.
	///
	/// \details A clone matches when one remote's normalized host equals the
	/// target's host and its owner and name equal the target's, compared
	/// without regard to ASCII case; the first such remote is the one shown.
	/// Order: the previously linked clone first, then by the listed folder's
	/// position in `aFolders`, then by path. A path is listed once.
	[[nodiscard]] auto match_clones(std::span<const clone_candidate> aCandidates, const clone_target& aTarget, std::span<const std::string> aFolders,
									std::string_view aLinked) -> std::vector<clone_match>;

	/// \brief Whether a folder may be listed: absolute, bounded, no control
	/// character. POSIX or Windows rules by the caller's platform flag.
	[[nodiscard]] auto valid_clone_folder(std::string_view aFolder, bool aWindows) noexcept -> bool;

	/// \brief The usual places clones live, offered as one-click suggestions
	/// once the host has seen they exist: `HOME/Documents/GitHub`,
	/// `HOME/Developer`, `HOME/src`, `HOME/code`, `HOME/Projects`.
	[[nodiscard]] auto usual_clone_folders(std::string_view aHome, bool aWindows) -> std::vector<std::string>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_CLONE_DISCOVERY_HPP
