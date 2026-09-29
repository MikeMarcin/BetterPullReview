#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKSPACE_SOURCE_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKSPACE_SOURCE_PORT_HPP

#include "bpr/core/review_cycle_workspace.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Read-only access to one deliberately chosen local root
/// (RFC-BPR-0054).
/// \details The port never writes the source, its index or its refs, never
/// runs Git, hooks, filters or repository scripts, and has no network or
/// credential capability. Descendants resolve without following symbolic
/// links, junctions or other reparse points and without leaving the root's
/// volume.
namespace bpr::application {
	/// Entries in one directory listing; a larger directory is refused.
	inline constexpr std::size_t maximum_workspace_directory_entries = 250'000U;
	/// Repository metadata files (index, HEAD, config, excludes) read whole.
	inline constexpr std::uint64_t maximum_workspace_metadata_bytes = 256U * 1024U * 1024U;

	enum class workspace_failure : std::uint8_t {
		/// Nothing answers at the location or path.
		missing,
		/// The object is a device, socket, pipe or other unsupported kind.
		unsupported,
		permission_denied,
		/// A component is a symbolic link, junction or other reparse point, or
		/// lies on another volume; nothing beneath it is read.
		escape,
		/// The object at the location is no longer the one that was opened.
		replaced,
		/// The path is not an admitted root-relative path.
		unsafe_path,
		too_large,
		/// The file changed identity, size or change stamps while it was read.
		changed,
		cancelled,
		io,
	};

	enum class workspace_entry_kind : std::uint8_t {
		file,
		directory,
		symlink,
		/// A device, socket, pipe or a reparse point that is not a symbolic link.
		other,
	};

	/// \brief One entry as observed without following it.
	/// \details `Modified` and `Changed` are opaque nanosecond stamps compared
	/// only for equality; a matching stamp is necessary for stability, never
	/// sufficient on its own. `Executable` is the POSIX owner-execute bit of a
	/// regular file; it is absent where the platform has none, so no mode is
	/// ever invented.
	struct workspace_entry_observation
	{
		std::string                     Name;
		workspace_entry_kind            Kind = workspace_entry_kind::other;
		core::workspace_object_identity Identity;
		std::uint64_t                   Size = 0;
		std::optional<bool>             Executable;
		std::int64_t                    Modified                                              = 0;
		std::int64_t                    Changed                                               = 0;
		auto                            operator<=>(const workspace_entry_observation&) const = default;
	};

	/// \brief What reading one regular file observed around the read.
	struct workspace_read_observation
	{
		workspace_entry_observation Before;
		workspace_entry_observation After;
		std::uint64_t               BytesRead = 0;
	};

	/// \brief Receives one read's bytes in order. Returning false stops the read.
	using workspace_chunk_sink = std::function<bool(std::span<const std::uint8_t>)>;

	/// \brief Which Git directory a metadata read addresses.
	enum class workspace_git_area : std::uint8_t {
		/// The per-worktree directory: `index`, `HEAD`.
		worktree,
		/// The shared directory: `config`, `info/exclude`, `packed-refs`, `refs/`.
		common,
	};

	/// \brief One opened root. The root object stays open for the session's
	/// lifetime, so every descendant resolves beneath the object that was
	/// chosen even if something is later renamed into its place.
	/// \details Paths are `/`-separated and root-relative; the empty path is
	/// the root. For a single-file root the only path is the file's name.
	class workspace_root_session
	{
	public:
		virtual ~workspace_root_session() = default;

		[[nodiscard]] virtual auto observation() const -> const core::workspace_root_observation& = 0;
		/// \brief Opens the location afresh and compares it with the held root.
		virtual auto verify(std::stop_token aStop) -> std::expected<core::workspace_verification, workspace_failure> = 0;
		/// \brief Lists one directory, sorted by name bytes, without following
		/// any entry. `.git` at the root is omitted: it is never material.
		virtual auto list(std::string_view aDirectory, std::stop_token aStop) -> std::expected<std::vector<workspace_entry_observation>, workspace_failure> = 0;
		/// \brief Observes one path without following it.
		virtual auto stat(std::string_view aPath, std::stop_token aStop) -> std::expected<workspace_entry_observation, workspace_failure> = 0;
		/// \brief Reads a regular file in bounded chunks. A file larger than
		/// `aMaximumBytes` is refused before any byte is delivered.
		virtual auto read(std::string_view aPath, std::uint64_t aMaximumBytes, const workspace_chunk_sink& aSink, std::stop_token aStop)
			-> std::expected<workspace_read_observation, workspace_failure> = 0;
		/// \brief The target bytes of a symbolic link, as data. Never followed.
		virtual auto read_link(std::string_view aPath, std::stop_token aStop) -> std::expected<std::string, workspace_failure> = 0;
		/// \brief Reads one Git metadata file whole. Refused for plain roots.
		virtual auto read_git(workspace_git_area aArea, std::string_view aPath, std::uint64_t aMaximumBytes, std::stop_token aStop)
			-> std::expected<std::vector<std::uint8_t>, workspace_failure> = 0;
	};

	/// \brief Opens deliberately chosen roots.
	class workspace_source_port
	{
	public:
		virtual ~workspace_source_port() = default;

		/// \brief Opens a folder, file, Git checkout or linked worktree. The
		/// location itself may be reached through links the user chose; nothing
		/// beneath it is.
		virtual auto open(std::string_view aLocation, std::stop_token aStop) -> std::expected<std::unique_ptr<workspace_root_session>, workspace_failure> = 0;
	};
}

#endif
