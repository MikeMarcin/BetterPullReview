#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-1753]], owner decision (b) 2026-10-23 ("Continuous
	/// session per PR with an option to clear session"). Which pull request a
	/// conversation is about: its host name, owner, repository name and
	/// number, the names a reopening reads again, not this run's identities.
	struct agent_conversation_key
	{
		std::string   Host;
		std::string   Owner;
		std::string   Name;
		std::uint64_t Number = 0U;
		/// Owner decision 2026-10-23 ("separate session for each agent"):
		/// whose conversation it is, the agent entry's identity; 0 for an
		/// agent that is not in the list.
		std::uint32_t Agent = 0U;

		auto operator<=>(const agent_conversation_key&) const = default;
	};

	/// \brief One turn of a pull request's conversation, as the Agent tab
	/// shows it: what was asked, what the agent answered, and how the turn
	/// ended (who answered, in how long, at what cost).
	struct agent_conversation_turn
	{
		std::string Asked;
		std::string Answered;
		std::string Ending;

		auto operator<=>(const agent_conversation_turn&) const = default;
	};

	/// The most turns one pull request keeps; older ones go first.
	inline constexpr std::size_t maximum_conversation_turns = 50U;
	/// The longest question, answer or ending one turn keeps.
	inline constexpr std::size_t maximum_conversation_text_bytes = 64U * 1024U;
	/// The most pull requests whose conversations a profile keeps.
	inline constexpr std::size_t maximum_conversations = 500U;

	[[nodiscard]] inline auto valid(const agent_conversation_key& aKey) noexcept -> bool
	{
		constexpr auto bound = std::size_t{ 1024U };
		return !aKey.Host.empty() && !aKey.Owner.empty() && !aKey.Name.empty() && aKey.Number > 0U && aKey.Host.size() <= bound && aKey.Owner.size() <= bound
			&& aKey.Name.size() <= bound && aKey.Agent <= 64U;
	}

	[[nodiscard]] inline auto valid(const std::vector<agent_conversation_turn>& aTurns) noexcept -> bool
	{
		if (aTurns.size() > maximum_conversation_turns) {
			return false;
		}
		for (const auto& turn : aTurns) {
			if (turn.Asked.size() > maximum_conversation_text_bytes || turn.Answered.size() > maximum_conversation_text_bytes
				|| turn.Ending.size() > maximum_conversation_text_bytes) {
				return false;
			}
		}
		return true;
	}

	/// \brief Where a profile keeps each pull request's conversation, so it
	/// survives closing the pull request and the application.
	///
	/// \details Prose the reviewer typed and the agent wrote, keyed by pull
	/// request; no credential, token or environment value. Storing no turns
	/// clears the pull request's conversation (Clear session).
	class agent_conversation_store_port
	{
	public:
		virtual ~agent_conversation_store_port() = default;
		/// \brief The pull request's turns, oldest first; none when it has
		/// none, and nothing when the store cannot be read.
		[[nodiscard]] virtual auto load(const agent_conversation_key& aKey) -> std::optional<std::vector<agent_conversation_turn>>       = 0;
		[[nodiscard]] virtual auto store(const agent_conversation_key& aKey, const std::vector<agent_conversation_turn>& aTurns) -> bool = 0;
		/// \brief The agent the pull request was last asked with (the key's
		/// `Agent` is not read), and keeping the key's `Agent` as that.
		[[nodiscard]] virtual auto load_agent(const agent_conversation_key& aKey) -> std::optional<std::uint32_t> = 0;
		[[nodiscard]] virtual auto store_agent(const agent_conversation_key& aKey) -> bool                        = 0;
		/// \brief [[BPR-1755]]. Forgets which agent every pull request
		/// was last asked with, so a reset's new agent, whose id may be an old
		/// one's, does not open on that agent's thread. Every turn stays.
		[[nodiscard]] virtual auto forget_agents() -> bool = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HPP
