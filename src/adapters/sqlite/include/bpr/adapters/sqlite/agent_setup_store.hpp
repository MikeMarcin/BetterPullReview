#ifndef BPR_SQLITE_AGENT_SETUP_STORE_HPP
#define BPR_SQLITE_AGENT_SETUP_STORE_HPP

#include "bpr/application/agent_setup_preferences.hpp"

#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite {
	/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0091. A profile's agent setup
	/// preferences in `agent-setup.sqlite3` under its state root: the chosen
	/// agent, the Codex and plain-text harness programs and Codex's protocol.
	///
	/// \details One owner-private file, opened for each read and write like the
	/// diagnostic preferences store. It holds identities and paths only; no
	/// column could hold a credential, token or environment value. A row that
	/// does not read back as valid is reported as defaults rather than
	/// trusted. Settings resets clear it with the other settings stores.
	[[nodiscard]] auto make_agent_setup_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_setup_store_port>;
} // namespace bpr::adapters::sqlite

#endif
