#ifndef BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP
#define BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP

#include "bpr/application/agent_launch.hpp"

#include <QObject>

#include <cstdint>
#include <memory>
#include <string>

namespace bpr::adapters::agent {
	/// \brief How one agent process ended.
	enum class agent_run_outcome : std::uint8_t {
		completed,
		cancelled,
		failed_to_start,
	};

	/// \brief What one finished agent run produced.
	struct agent_run_result
	{
		agent_run_outcome Outcome = agent_run_outcome::failed_to_start;
		std::string       Output;
	};

	/// \brief Starts one agent process with exactly what the request names.
	///
	/// \details The child's environment is **set**, never inherited. The
	/// toolkit's default is to hand the child a copy of this process's
	/// environment, and that default is precisely what would carry the
	/// reviewer's own GitHub token, AWS key, and model provider key into a
	/// tool this product does not audit. The child environment is constructed
	/// empty in the implementation and filled only from the request.
	///
	/// The supplement is written to the child's standard input. Nothing from
	/// it reaches the argument list, and the argument list is passed as a
	/// string list rather than a command string, so no shell parses it.
	///
	/// This lives outside the Qt adapter on purpose. That whole directory is
	/// an effect-free region — presentation starts no processes — and the
	/// architecture scan enforces it. Putting a launcher there was the first
	/// attempt and the scan refused it, correctly: the mistake was the
	/// placement rather than the wording. `src/adapters/git` is the precedent
	/// this follows.
	class agent_process_launcher final : public QObject
	{
	public:
		explicit agent_process_launcher(QObject* aParent = nullptr);
		~agent_process_launcher() override;

		agent_process_launcher(const agent_process_launcher&)                    = delete;
		auto operator=(const agent_process_launcher&) -> agent_process_launcher& = delete;
		agent_process_launcher(agent_process_launcher&&)                         = delete;
		auto operator=(agent_process_launcher&&) -> agent_process_launcher&      = delete;

		/// \brief Runs one agent to completion, or until cancelled.
		///
		/// \param aTimeoutMilliseconds A bound, not the mechanism. A child that
		/// outlives it is terminated and reported cancelled.
		[[nodiscard]] auto run(
			const application::agent_launch_request& aRequest,
			int                                      aTimeoutMilliseconds) -> agent_run_result;

		/// \brief Terminates the running child, if there is one.
		///
		/// \details A cancelled run publishes nothing: whatever the child had
		/// written is discarded rather than delivered as a partial answer,
		/// because a partial answer from an agent is indistinguishable from a
		/// complete one that happened to be short.
		void cancel();

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::adapters::agent

#endif // BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP
