#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DESCRIPTION_DRAFT_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DESCRIPTION_DRAFT_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/agent_run_policy.hpp"
#include "bpr/core/description_draft.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

/// \file
/// \brief [[BPR-2281]]. Draft with agent beside the pull request's
/// description in Author: one fresh, tool-free run of the agent the window's
/// picker names, with the Description writing system prompt as its
/// instructions and `core::compose_description_draft` as its message.
///
/// \details The run goes on the agent lane and its answer comes back on the
/// decision lane as a projection: running, the description it drafted, or
/// why there is none. Stop ends a run; a run's answer after Stop, or after
/// another run began, is dropped. Nothing is written anywhere: the window
/// puts the draft in the description editor, and only Save sends.
namespace bpr::application {
	/// \brief The agent a draft runs with, as the window's picker names it.
	struct description_draft_agent
	{
		std::string   Agent;
		std::string   AgentName;
		std::uint64_t Profile = 0U;
		/// The Description writing system prompt's effective text.
		std::string                      Instructions;
		core::agent_instruction_delivery Delivery = core::agent_instruction_delivery::system_prompt;
		/// [[BPR-2264]]. The Writing style Feature: a person reads the draft.
		std::string WritingStyle;
	};

	/// \brief Runs one draft; blocking, on the agent lane, stoppable. The
	/// agent's answer, or why there is none in the person's words.
	using description_draft_runner = std::function<std::expected<std::string, std::string>(const description_draft_agent&, const std::string& aMessage, std::stop_token)>;

	/// \brief What the window shows of the draft.
	struct description_draft_projection
	{
		bool Running = false;
		/// The last draft, and a count that moves with each new one.
		std::optional<std::string> Draft;
		std::uint64_t              DraftRevision = 0U;
		/// Why the last run drafted nothing; empty when it did, or ran none.
		std::string Failure;
		/// The last run was stopped.
		bool          Stopped  = false;
		std::uint64_t Revision = 0U;

		auto operator==(const description_draft_projection&) const -> bool = default;
	};

	class description_draft_host
	{
	public:
		description_draft_host(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const description_draft_projection&)> aPublish);
		~description_draft_host();
		description_draft_host(const description_draft_host&)                    = delete;
		auto operator=(const description_draft_host&) -> description_draft_host& = delete;

		void               set_runner(description_draft_runner aRunner);
		[[nodiscard]] auto available() const noexcept -> bool;
		/// \brief Starts a draft; false while one runs or with no runner.
		auto draft(description_draft_agent aAgent, const core::description_draft_material& aMaterial) -> bool;
		/// \brief Says why a draft could not start, as a run's failure would.
		void refuse(std::string aWhy);
		/// \brief Stops the run going, if any.
		void               stop();
		[[nodiscard]] auto projection() const noexcept -> const description_draft_projection&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif
