#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_EXPIRY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_EXPIRY_HPP

#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/native_agent_session.hpp"
#include "bpr/core/agent_workspace_expiry.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <stop_token>

namespace bpr::application {
	/// \brief [[BPR-2293]]. What one retained workspace holds on disk.
	struct retained_workspace_measure
	{
		/// The workspace's folder is there at all.
		bool          Present = true;
		std::uint64_t Bytes   = 0U;
		/// The newest change to anything in it, in seconds since the epoch.
		std::int64_t NewestSeconds = 0;

		auto operator<=>(const retained_workspace_measure&) const = default;
	};

	enum class retained_workspace_removal : std::uint8_t {
		removed,
		/// Nothing of it was left to remove.
		gone,
		/// Another process, or this one's own turn, holds its ownership record.
		in_use,
		/// Its exact ownership could not be verified, or removal failed; what is
		/// there is left as it was.
		refused,
	};

	/// \brief [[BPR-2293]]. Measures and removes retained workspaces by their
	/// exact ownership; the Git adapter implements it.
	class retained_workspace_files_port
	{
	public:
		virtual ~retained_workspace_files_port() = default;
		/// \brief A bounded walk of the workspace that never follows a link.
		[[nodiscard]] virtual auto measure(const owned_agent_workspace& aWorkspace, std::stop_token aStop) -> retained_workspace_measure = 0;
		/// \brief The exact, journaled cleanup that refuses a substituted path.
		[[nodiscard]] virtual auto remove(const owned_agent_workspace& aWorkspace, std::stop_token aStop) -> retained_workspace_removal = 0;
	};

	enum class retained_workspace_sweep : std::uint8_t {
		/// Only measure and count.
		survey,
		/// The policy: ended, unused past the days, then the oldest over the cap.
		policy,
		/// Remove in Settings: every one not in use.
		remove_all,
	};

	/// \brief What a sweep did, and what is left.
	struct retained_workspace_summary
	{
		/// The catalog could not be read; nothing was measured or removed.
		bool          Unavailable  = false;
		std::size_t   Count        = 0U;
		std::uint64_t Bytes        = 0U;
		std::size_t   InUse        = 0U;
		std::size_t   Removed      = 0U;
		std::uint64_t RemovedBytes = 0U;
		std::size_t   Failed       = 0U;

		auto operator<=>(const retained_workspace_summary&) const = default;
	};

	/// \brief [[BPR-2293]]. Lists the retained workspaces the catalog records,
	/// measures them, and removes those \p aSweep chooses.
	///
	/// \details Blocking: runs on an effect worker. A workspace whose turn
	/// lease (by any of its pull request keys, or its path) cannot be taken is
	/// in use and is never removed; the leases are taken again around each
	/// removal and released after it. A removed workspace is then cleared from
	/// its conversations (`forget_workspace`), so their next turns make a fresh
	/// one; one whose folder is already gone is only cleared. `aStop` ends the
	/// sweep between workspaces.
	[[nodiscard]] auto sweep_retained_workspaces(native_agent_session_store_port& aStore, retained_workspace_files_port& aFiles, retained_workspace_sweep aSweep,
												 const core::agent_workspace_expiry_policy& aPolicy, std::int64_t aNowSeconds, std::stop_token aStop)
		-> retained_workspace_summary;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_EXPIRY_HPP
