#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_workspace.hpp"

#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>

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
	};

	enum class native_agent_storage_failure { unavailable,
											  capacity,
											  conflict };

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
		[[nodiscard]] virtual auto history(const std::string&, const std::string&, const std::string& = { }, const std::optional<agent_conversation_key>& = std::nullopt)
			-> std::expected<native_agent_history_projection, native_agent_storage_failure> { return std::unexpected(native_agent_storage_failure::unavailable); }
		/// Explicit prose-only generation. Source identity and original state are preserved.
		[[nodiscard]] virtual auto replay(const std::string&, const std::string&, const core::agent_revision_binding&, const std::string&, const std::optional<agent_conversation_key>& = std::nullopt)
			-> std::expected<native_agent_session, native_agent_storage_failure> { return std::unexpected(native_agent_storage_failure::unavailable); }
		/// Appends only to the originating generation, independently of the selected pointer.
		[[nodiscard]] virtual auto append(const std::string&, const agent_conversation_turn&) -> bool { return false; }
		/// Updates the caller's revision on success; failure preserves its prior identity.
		[[nodiscard]] virtual auto store(native_agent_session& aSession) -> bool = 0;

	private:
		std::mutex                                                  WorkspaceLocksMutex;
		std::map<std::string, std::weak_ptr<std::binary_semaphore>> WorkspaceLocks;
	};
}
#endif
