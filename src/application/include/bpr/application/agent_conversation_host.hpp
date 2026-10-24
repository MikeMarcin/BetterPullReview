#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HOST_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/port.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace bpr::application {
	/// \brief Keeps the newest `maximum_conversation_turns` turns and cuts
	/// each text, at a character boundary, to `maximum_conversation_text_bytes`.
	[[nodiscard]] auto bound_conversation(std::vector<agent_conversation_turn> aTurns) -> std::vector<agent_conversation_turn>;

	/// \brief What the Agent tab's thread shows for the open pull request.
	struct agent_conversation_projection
	{
		/// The pull request the turns are for; none when none is open.
		std::optional<agent_conversation_key> Key;
		std::vector<agent_conversation_turn>  Turns;
		/// The kept turns have been read for `Key`.
		bool          Loaded     = false;
		std::uint64_t Generation = 0U;

		auto operator<=>(const agent_conversation_projection&) const = default;
	};

	struct agent_conversation_dependencies
	{
		agent_conversation_store_port*                            Store    = nullptr;
		effect_executor*                                          Effects  = nullptr;
		decision_executor*                                        Decision = nullptr;
		std::function<void(const agent_conversation_projection&)> Publish;
	};

	/// \brief [[BPR-1753]], owner decision (b) 2026-10-23 ("Continuous
	/// session per PR with an option to clear session"). One conversation per
	/// pull request: the turns the thread shows, kept so a follow-up continues
	/// them (they ride the next request's brief) and so they come back when the
	/// pull request is opened again, in this run or a later one.
	///
	/// \details Lives on the decision lane; the store runs on the effect lane,
	/// one write at a time, and only the newest thread waiting is written. A
	/// read that ends after another pull request was opened is dropped. The
	/// conversation is carried by the application rather than by any harness's
	/// own resumable session, so it continues in whichever agent answers.
	class agent_conversation_host final
	{
	public:
		explicit agent_conversation_host(agent_conversation_dependencies aDependencies);
		~agent_conversation_host();
		agent_conversation_host(const agent_conversation_host&)                    = delete;
		auto operator=(const agent_conversation_host&) -> agent_conversation_host& = delete;

		/// \brief A pull request was opened: the agent it was last asked with
		/// (`aKey.Agent` when none was kept) and that agent's turns are read
		/// and published. Opening the one already open changes nothing.
		void open(const agent_conversation_key& aKey);
		/// \brief Owner decision 2026-10-23: another agent's own conversation
		/// about the open pull request, kept as the one it is asked with.
		void switch_agent(std::uint32_t aAgent);
		/// \brief No pull request is open.
		void close();
		/// \brief The thread now, after a turn ended; bounded and kept.
		void record(std::vector<agent_conversation_turn> aTurns);
		/// \brief Clear session: no turns, kept as none.
		void clear();
		/// \brief Reset agents: no pull request opens on an agent it was
		/// last asked with; the conversations themselves stay.
		void forget_agent_choices();

		[[nodiscard]] auto projection() const -> agent_conversation_projection;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONVERSATION_HOST_HPP
