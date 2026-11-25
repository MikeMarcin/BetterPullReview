#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/core/agent_cost.hpp"

#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>
#include <vector>

namespace bpr::application {
	[[nodiscard]] auto native_scope_key(const core::pull_request_authority&, const std::string&, const std::string&) -> std::string;

	/// \brief Durable host state, separate from window and invocation identities.
	/// Handles and workspace ownership never enter ordinary diagnostic metadata.
	struct native_agent_session
	{
		std::string                  Id;
		std::string                  Scope;
		std::string                  Configuration;
		core::agent_revision_binding Binding;
		std::string                  Handle;
		/// Exact PR/source workspace key, attached only while an explicit turn owns its lease.
		std::string                          WorkspaceKey;
		std::optional<owned_agent_workspace> Workspace;
		std::uint64_t                        Turn     = 2U;
		std::uint64_t                        Revision = 0U;
		/// Written before submission; a crash here is uncertain, never a retry.
		bool        Pending = false;
		bool        Replay  = false;
		std::string ReplaySource;
		/// [[BPR-1999]]. What the conversation has cost, and the runtime's
		/// last figure for the session it resumes.
		core::agent_cost_ledger Cost;
	};

	enum class native_agent_storage_failure { unavailable,
											  capacity,
											  conflict };

	/// \brief [[BPR-2293]]. One retained workspace the catalog records, once
	/// however many conversations hold it.
	struct retained_workspace_record
	{
		owned_agent_workspace Workspace;
		/// The pull request workspace keys (`agent_workspace_key`) that lead to
		/// it; a turn on it holds one of their leases.
		std::vector<std::string> Keys;
		/// When a turn last stored a conversation holding it, in seconds since
		/// the epoch; none for a workspace kept before this was recorded.
		std::optional<std::int64_t> LastUse;
		/// No selected conversation reaches it: each conversation of its pull
		/// request that is selected now is at another base or head, or none is.
		bool Ended = false;
	};

	/// \brief [[BPR-2377]]. What the catalog holds about one conversation
	/// before it is deleted.
	struct native_agent_session_removal
	{
		/// It is in the catalog under the scope asked about.
		bool Found = false;
		/// Its retained workspace, if it holds one.
		std::optional<owned_agent_workspace> Workspace;
		/// The pull request workspace keys (`agent_workspace_key`) that lead
		/// to it; a turn on it holds one of their leases.
		std::vector<std::string> Keys;
		/// Other conversations, and the pull request's kept workspace, that
		/// hold the same retained workspace.
		std::size_t OtherHolders = 0U;
	};

	/// \brief [[BPR-2293]]. The turn lease that guards one retained workspace by
	/// its path, beside its pull request key: a workspace kept before its key
	/// was recorded is still guarded.
	[[nodiscard]] inline auto agent_workspace_path_lease(const std::string& aPath) -> std::string
	{
		return "workspace-path:" + aPath;
	}

	/// \brief Nonblocking turn ownership for one shared source tree, across profile windows.
	class agent_workspace_turn_lease
	{
	public:
		explicit agent_workspace_turn_lease(std::shared_ptr<std::binary_semaphore> aMutex)
			: Mutex{ std::move(aMutex) }
			, Locked{ Mutex->try_acquire() }
		{ }
		[[nodiscard]] auto owns_lock() const noexcept -> bool { return Locked; }
		~agent_workspace_turn_lease()
		{
			if (Locked) {
				Mutex->release();
			}
		}
		agent_workspace_turn_lease(const agent_workspace_turn_lease&)                    = delete;
		auto operator=(const agent_workspace_turn_lease&) -> agent_workspace_turn_lease& = delete;

	private:
		std::shared_ptr<std::binary_semaphore> Mutex;
		bool                                   Locked;
	};

	/// \brief Effect-lane catalog. compare-before-write serializes BPR windows.
	class native_agent_session_store_port
	{
	public:
		virtual ~native_agent_session_store_port() = default;
		/// \brief The profile owns one catalog; a busy tree refuses before any send or mutation.
		[[nodiscard]] auto try_lock_workspace(const std::string& aKey) -> std::unique_ptr<agent_workspace_turn_lease>
		{
			std::scoped_lock lock{ WorkspaceLocksMutex };
			std::erase_if(WorkspaceLocks, [](const auto& aEntry) { return aEntry.second.expired(); });
			auto mutex = WorkspaceLocks[aKey].lock();
			if (!mutex) {
				mutex                = std::make_shared<std::binary_semaphore>(1);
				WorkspaceLocks[aKey] = mutex;
			}
			auto lease = std::make_unique<agent_workspace_turn_lease>(std::move(mutex));
			return lease->owns_lock() ? std::move(lease) : nullptr;
		}
		/// \brief Looks up shared files without importing another conversation's handle or prose.
		/// A different legacy tree is a conflict, never a silent native-session workdir change.
		[[nodiscard]] virtual auto shared_workspace(const std::string&, const native_agent_session&)
			-> std::expected<std::optional<owned_agent_workspace>, native_agent_storage_failure> = 0;
		[[nodiscard]] virtual auto select(const std::string& aScope, const std::string& aConfiguration,
										  const core::agent_revision_binding& aBinding, bool aStartAtCurrentRevision = false)
			-> std::expected<native_agent_session, native_agent_storage_failure> = 0;
		/// \brief Observes selected metadata without creating a generation or inference.
		[[nodiscard]] virtual auto find(const std::string&, const std::string&, const core::agent_revision_binding&)
			-> std::expected<std::optional<native_agent_session>, native_agent_storage_failure> = 0;
		/// Explicit selection/admission. Empty ID starts fresh; unused generations are reused.
		[[nodiscard]] virtual auto choose(const std::string&, const std::string&, const core::agent_revision_binding&, const std::string&)
			-> std::expected<native_agent_session, native_agent_storage_failure> { return std::unexpected(native_agent_storage_failure::unavailable); }
		[[nodiscard]] virtual auto history(const std::string&, const std::string&, const std::string& = { })
			-> std::expected<native_agent_history_projection, native_agent_storage_failure> { return std::unexpected(native_agent_storage_failure::unavailable); }
		/// Explicit prose-only generation. Source identity and original state are preserved.
		[[nodiscard]] virtual auto replay(const std::string&, const std::string&, const core::agent_revision_binding&, const std::string&)
			-> std::expected<native_agent_session, native_agent_storage_failure> { return std::unexpected(native_agent_storage_failure::unavailable); }
		/// Appends only to the originating generation, independently of the selected pointer.
		[[nodiscard]] virtual auto append(const std::string&, const agent_conversation_turn&) -> bool { return false; }
		/// Updates the caller's revision on success; failure preserves its prior identity.
		[[nodiscard]] virtual auto store(native_agent_session& aSession) -> bool = 0;
		/// \brief [[BPR-2293]]. Every retained workspace a conversation holds,
		/// each once.
		[[nodiscard]] virtual auto retained_workspaces() -> std::expected<std::vector<retained_workspace_record>, native_agent_storage_failure>
		{
			return std::unexpected(native_agent_storage_failure::unavailable);
		}
		/// \brief [[BPR-2293]]. Clears \p aWorkspace from every conversation
		/// that holds it, and its pull request keys, in one revision: its
		/// conversations' next turns make a fresh one. Answers whether it was
		/// written.
		[[nodiscard]] virtual auto forget_workspace(const owned_agent_workspace& aWorkspace) -> bool
		{
			static_cast<void>(aWorkspace);
			return false;
		}
		/// \brief [[BPR-2345]]. Keeps \p aWorkspace, which Agent setup's check
		/// made for \p aBinding, as the pull request's retained workspace under
		/// \p aKey (`agent_workspace_key`) before any conversation holds it:
		/// `shared_workspace` answers it to the first turn, which reattaches
		/// it, and `retained_workspaces` lists it for expiry. Answers false,
		/// and keeps nothing, when a conversation already holds a workspace
		/// under the key, another is kept there, or it could not be written;
		/// the caller then removes its own.
		[[nodiscard]] virtual auto keep_workspace(const std::string& aKey, const core::agent_revision_binding& aBinding, const owned_agent_workspace& aWorkspace) -> bool
		{
			static_cast<void>(aKey);
			static_cast<void>(aBinding);
			static_cast<void>(aWorkspace);
			return false;
		}
		/// \brief [[BPR-2377]]. Gives the conversation \p aId in \p aScope the
		/// name \p aName (already tidied by `core::tidy_agent_session_name`);
		/// an empty name removes it. Answers whether it was written.
		[[nodiscard]] virtual auto rename(const std::string& aScope, const std::string& aId, const std::string& aName) -> bool
		{
			static_cast<void>(aScope);
			static_cast<void>(aId);
			static_cast<void>(aName);
			return false;
		}
		/// \brief [[BPR-2377]]. What deleting \p aId in \p aScope would touch.
		[[nodiscard]] virtual auto removal(const std::string& aScope, const std::string& aId) -> std::expected<native_agent_session_removal, native_agent_storage_failure>
		{
			static_cast<void>(aScope);
			static_cast<void>(aId);
			return std::unexpected(native_agent_storage_failure::unavailable);
		}
		/// \brief [[BPR-2377]]. Forgets the conversation \p aId in \p aScope:
		/// its turns, name, cost, replay source and selection, in one
		/// revision. A workspace key that led to it leads to another
		/// conversation holding the same workspace, or to none. The
		/// workspace's files are the caller's. Answers whether it was written.
		[[nodiscard]] virtual auto erase(const std::string& aScope, const std::string& aId) -> bool
		{
			static_cast<void>(aScope);
			static_cast<void>(aId);
			return false;
		}
		/// \brief [[BPR-2377]]. Forgets every conversation, native and
		/// earlier, with its transcript, name, cost, selection, finding card
		/// states and kept workspace records; agent setup stays. Answers
		/// whether it was written.
		[[nodiscard]] virtual auto erase_all() -> bool { return false; }

	private:
		std::mutex                                                  WorkspaceLocksMutex;
		std::map<std::string, std::weak_ptr<std::binary_semaphore>> WorkspaceLocks;
	};
}
#endif
