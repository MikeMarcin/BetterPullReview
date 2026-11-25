#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_MANAGEMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_MANAGEMENT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-2377]]. The rules for naming and deleting one of
/// BetterPullReview's agent sessions (owner, 2026-11-24: "Our agent session
/// management is weak. There's as far as I can tell no way to set a session
/// name or delete a session.").
/// \details A session's name is BetterPullReview's alone: it is stored with
/// the session's record and shown in History and the Agent tab's title; no
/// agent sees it. Deleting a session removes BetterPullReview's record of it
/// (its transcript, name, cost and selection) and its retained workspace when
/// no other session of the pull request uses that workspace. The agent's own
/// native session files (Claude Code's or Codex's transcripts under their
/// home folders) are not BetterPullReview's and are never touched.
namespace bpr::core {
	/// \brief The longest session name, in UTF-8 bytes, after it is tidied.
	inline constexpr std::size_t maximum_agent_session_name_bytes = 120U;

	/// \brief Why a name was refused.
	enum class agent_session_name_refusal : std::uint8_t {
		/// Longer than `maximum_agent_session_name_bytes` once tidied.
		too_long,
		/// Not well-formed UTF-8, or holds a NUL.
		not_text,
	};

	/// \brief The name to store for what the person typed: its words with
	/// every run of white space or control characters made one space, and none
	/// at either end. Empty means "no name": the default title returns.
	[[nodiscard]] auto tidy_agent_session_name(std::string_view aTyped) -> std::expected<std::string, agent_session_name_refusal>;

	/// \brief The words History and the Agent tab show for a session: its
	/// name, else its first question, else "New session".
	[[nodiscard]] auto agent_session_title(std::string_view aName, std::string_view aFirstQuestion) -> std::string;

	/// \brief What is observed about one session before it is deleted.
	struct agent_session_deletion_facts
	{
		/// The session is in the catalog, in the scope asked about.
		bool Found = false;
		/// A turn holds the session's workspace lease now: it is running.
		bool Running = false;
		/// The session holds a retained workspace.
		bool HoldsWorkspace = false;
		/// Other sessions, or the pull request's kept workspace, that use the
		/// same retained workspace.
		std::size_t OtherHolders = 0U;
	};

	/// \brief Why a session was not deleted.
	enum class agent_session_deletion_refusal : std::uint8_t {
		none,
		/// No such session; nothing was deleted.
		not_found,
		/// A turn is running on it or on its workspace: stop it first.
		running,
	};

	/// \brief What deleting a session does.
	struct agent_session_deletion_plan
	{
		agent_session_deletion_refusal Refusal = agent_session_deletion_refusal::none;
		/// Remove its retained workspace before forgetting it: no other
		/// session uses it.
		bool RemoveWorkspace = false;
		/// Its workspace stays for the other sessions that use it.
		bool WorkspaceShared                                              = false;
		auto operator==(const agent_session_deletion_plan&) const -> bool = default;
	};

	/// \brief Decides one deletion from what was observed.
	[[nodiscard]] auto plan_agent_session_deletion(const agent_session_deletion_facts& aFacts) noexcept -> agent_session_deletion_plan;
}

#endif
