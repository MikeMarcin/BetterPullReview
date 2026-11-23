#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FOLLOW_UP_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FOLLOW_UP_HOST_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/core/agent_run_policy.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-2286]]. Ask follow-up and Ask about this: the host of the
/// follow-ups a window holds, one a review item, each a `core::follow_up_state`.
///
/// \details Each round is one fresh, tool-free run of the agent the window's
/// picker names, on the agent lane; its answer comes back on the decision lane
/// and the core settles it. Stop ends a run; an answer for a run that is not
/// the one going is dropped by the core. The host applies nothing: Use returns
/// the core's decision and the window puts the text in place, and only the
/// reviewer publishes.
namespace bpr::application {
	/// \brief The agent a round runs with, as the window's picker names it.
	struct follow_up_agent
	{
		std::string   Agent;
		std::string   AgentName;
		std::uint64_t Profile = 0U;
		/// What the run is told to be, as its system prompt.
		std::string                      Instructions;
		core::agent_instruction_delivery Delivery = core::agent_instruction_delivery::system_prompt;
		/// The Writing style Feature: a person reads the answers.
		std::string WritingStyle;
		/// The bounded inline guidance named in this turn's question.
		std::vector<core::supplement_skill> Skills;
	};

	/// \brief Runs one round; blocking, on the agent lane, stoppable. The
	/// agent's answer, or why there is none in the person's words.
	using follow_up_runner = std::function<std::expected<std::string, std::string>(const follow_up_agent&, const std::string& aMessage, std::stop_token)>;

	/// \brief What the window shows: the follow-up open, if any.
	struct follow_up_projection
	{
		std::optional<core::follow_up_state> Open;
		/// Why the last run failed, in the person's words; empty otherwise.
		std::string   RunFailure;
		std::uint64_t Revision = 0U;

		auto operator==(const follow_up_projection&) const -> bool = default;
	};

	class follow_up_host
	{
	public:
		follow_up_host(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const follow_up_projection&)> aPublish);
		~follow_up_host();
		follow_up_host(const follow_up_host&)                    = delete;
		auto operator=(const follow_up_host&) -> follow_up_host& = delete;

		void set_runner(follow_up_runner aRunner);
		/// \brief [[BPR-2322]]. Where each question, proposal and Use is
		/// logged (`agent.side_chat`, Standard detail): the item's kind, the
		/// round, the apply action, the outcome or refusal, the duration and
		/// the texts' lengths, never their words.
		void               set_log(std::shared_ptr<diagnostic_log_port> aLog);
		[[nodiscard]] auto available() const noexcept -> bool;
		/// \brief Opens the follow-up about `aSubject`: the one already held
		/// for its key, its item refreshed, or a new one. False for an invalid
		/// subject.
		auto open(core::follow_up_subject aSubject) -> bool;
		/// \brief The window closed the follow-up; it is kept for its key.
		void close();
		/// \brief Asks one question in the open follow-up.
		auto ask(follow_up_agent aAgent, std::string_view aQuestion) -> core::follow_up_refusal;
		/// \brief Asks for the text `aApply` would put in place; `aBase` is
		/// that place's text now.
		auto propose(follow_up_agent aAgent, core::follow_up_apply aApply, std::string aBase) -> core::follow_up_refusal;
		/// \brief Use: the core's decision over the text the proposal replaces
		/// as it is now. Nothing is put anywhere here.
		auto apply(std::uint64_t aProposal, std::string_view aCurrent) -> core::follow_up_application;
		auto discard(std::uint64_t aProposal) -> bool;
		/// \brief Stops the open follow-up's run, if any.
		void               stop();
		[[nodiscard]] auto projection() const noexcept -> const follow_up_projection&;
		/// \brief The follow-up held for `aKey`, open or not.
		[[nodiscard]] auto held(const std::string& aKey) const -> const core::follow_up_state*;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif
