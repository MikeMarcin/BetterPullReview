#ifndef BPR_SQLITE_AGENT_SETUP_STORE_HPP
#define BPR_SQLITE_AGENT_SETUP_STORE_HPP

#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/review_skill_host.hpp"

#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite {
	/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0092. A profile's agent setup
	/// preferences in `agent-setup.sqlite3` under its state root: the chosen
	/// agent, the Codex and plain-text harness programs and Codex's protocol.
	///
	/// \details One owner-private file, opened for each read and write like the
	/// diagnostic preferences store. It holds identities and paths only; no
	/// column could hold a credential, token or environment value. A row that
	/// does not read back as valid is reported as defaults rather than
	/// trusted. Settings resets clear it with the other settings stores.
	[[nodiscard]] auto make_agent_setup_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_setup_store_port>;

	/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0092 D2. The same file's clone
	/// folders (at most `core::maximum_clone_folders`, in order) and repository
	/// links (at most 1,000), each a folder path, a remote name and a private
	/// copy flag; no URL or credential is stored.
	[[nodiscard]] auto make_clone_link_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::clone_link_store_port>;

	/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0092 D1. The same file's edited copy
	/// of the review skill: its text (at most `core::maximum_review_skill_bytes`),
	/// the built-in version and SHA-256 it came from, and the version Keep mine
	/// was chosen for. A row that does not read back whole is reported as no
	/// copy. Settings resets remove it with the rest of the file.
	[[nodiscard]] auto make_review_skill_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::review_skill_store_port>;
} // namespace bpr::adapters::sqlite

#endif
