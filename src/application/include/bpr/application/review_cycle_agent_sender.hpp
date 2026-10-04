#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_AGENT_SENDER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_AGENT_SENDER_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/review_cycle_send.hpp"

#include <stop_token>

/// \file
/// \brief [[BPR-1180]]. The installed Send to Agent adapter: built only with
/// the agent integrations (`BPR_BUILD_AGENT_INTEGRATIONS`).
namespace bpr::application {
	/// \brief Sends one round through the agent launch every other use of a
	/// profile goes through: `build_round_launch`, then the process port's
	/// supervised run with its byte, frame and time bounds and its protocol
	/// decoding. It passes no credential and inherits only the profile's
	/// allowed environment names. It reports what it saw and decides nothing:
	/// it never sends again, chooses another profile or starts a session it
	/// was not asked to.
	class agent_round_sender final : public review_cycle_sender
	{
	public:
		agent_round_sender(agent_process_port& aProcesses, environment_reader aEnvironment);
		auto send(const cycle_send_assignment& aAssignment, std::stop_token aStop) -> core::cycle_send_result override;
		void cancel() noexcept override;

	private:
		agent_process_port& Processes;
		environment_reader  Environment;
	};
}

#endif
