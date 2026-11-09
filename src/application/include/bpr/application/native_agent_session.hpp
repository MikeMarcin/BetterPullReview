#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_NATIVE_AGENT_SESSION_HPP

#include "bpr/application/agent_workspace.hpp"

#include <expected>
#include <optional>
#include <string>

namespace bpr::application {
	/// \brief Durable host state, separate from window and invocation identities.
	/// Handles and workspace ownership never enter ordinary diagnostic metadata.
	struct native_agent_session
	{
		std::string                          Id;
		std::string                          Scope;
		std::string                          Configuration;
		core::agent_revision_binding         Binding;
		std::string                          Handle;
		std::optional<owned_agent_workspace> Workspace;
		std::uint64_t                        Turn     = 2U;
		std::uint64_t                        Revision = 0U;
		/// Written before submission; a crash here is uncertain, never a retry.
		bool Pending = false;
	};

	enum class native_agent_storage_failure { unavailable,
											  capacity,
											  conflict };

	/// \brief Effect-lane catalog. compare-before-write serializes BPR windows.
	class native_agent_session_store_port
	{
	public:
		virtual ~native_agent_session_store_port() = default;
		[[nodiscard]] virtual auto select(const std::string& aScope, const std::string& aConfiguration,
										  const core::agent_revision_binding& aBinding, bool aStartAtCurrentRevision = false)
			-> std::expected<native_agent_session, native_agent_storage_failure> = 0;
		/// \brief Observes selected metadata without creating a generation or inference.
		[[nodiscard]] virtual auto find(const std::string&, const std::string&, const core::agent_revision_binding&)
			-> std::expected<std::optional<native_agent_session>, native_agent_storage_failure> = 0;
		/// Updates the caller's revision on success; failure preserves its prior identity.
		[[nodiscard]] virtual auto store(native_agent_session& aSession) -> bool = 0;
	};
}
#endif
