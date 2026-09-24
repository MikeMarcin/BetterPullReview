#ifndef BPR_SRC_ADAPTERS_AGENT_SRC_AGENT_PROTOCOL_LOOP_HPP
#define BPR_SRC_ADAPTERS_AGENT_SRC_AGENT_PROTOCOL_LOOP_HPP

#include "bpr/application/agent_launch.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace bpr::adapters::agent::detail {
	enum class agent_protocol_write_status : std::uint8_t {
		success,
		cancelled,
		timeout,
		process_exited,
		failed,
	};

	enum class agent_protocol_pipe_read_status : std::uint8_t {
		no_data,
		data,
		closed,
		failed,
	};

	struct agent_protocol_pipe_read_result
	{
		agent_protocol_pipe_read_status Status
			= agent_protocol_pipe_read_status::no_data;
		std::size_t Count = 0U;
		/// False exactly while the same observed availability snapshot still
		/// has unread bytes. Non-data results must always be batch-complete.
		bool BatchComplete = true;
	};

	enum class agent_protocol_wait_status : std::uint8_t {
		running,
		exited,
		cancelled,
		timeout,
		failed,
	};

	struct agent_protocol_wait_result
	{
		agent_protocol_wait_status Status         = agent_protocol_wait_status::running;
		int                        ExitCode       = 0;
		bool                       ExitedNormally = true;
	};

	/// \brief One of the child's output streams, as the loop reads it.
	enum class agent_protocol_stream : std::uint8_t {
		standard_output,
		standard_error,
	};

	/// \brief How many bytes of \p aStream a transport may read ahead of the loop.
	///
	/// \details [[BPR-0886]]. A transport that reads the child's output while
	/// it writes the request keeps those bytes for the loop to read later. It
	/// keeps at most one byte past the ceiling the loop applies to that
	/// stream: enough for the loop to meet its own refusal, and no more. The
	/// ceilings belong to the loop, so a transport asks for them here rather
	/// than naming them.
	[[nodiscard]] auto agent_protocol_read_ahead_limit(agent_protocol_stream aStream) noexcept
		-> std::size_t;

	/// \brief Narrow platform transport used by the shared agent protocol loop.
	///
	/// \details Implementations retain ownership of native process, pipe,
	/// cancellation, deadline, and containment primitives. The loop supplies
	/// caller-owned read buffers and closes input only at protocol boundaries.
	class agent_protocol_transport
	{
	public:
		virtual ~agent_protocol_transport() = default;

		[[nodiscard]] virtual auto write(std::string_view aBytes)
			-> agent_protocol_write_status
			= 0;
		virtual void               close_input() noexcept = 0;
		[[nodiscard]] virtual auto read_standard_output(std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result
			= 0;
		[[nodiscard]] virtual auto read_standard_error(std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result
			= 0;
		[[nodiscard]] virtual auto wait() -> agent_protocol_wait_result = 0;
		[[nodiscard]] virtual auto terminate() noexcept -> bool         = 0;
	};

	/// \brief Drives one complete, already-started agent protocol exchange.
	///
	/// \details Output is drained and decoded before exit classification. The
	/// platform transport is terminated exactly once after the first outcome is
	/// established; a later teardown failure cannot replace an earlier cause.
	[[nodiscard]] auto run_agent_protocol_loop(
		const application::agent_launch_request& aRequest,
		const application::agent_output_sink&    aSink,
		agent_protocol_transport&                aTransport)
		-> application::agent_run_result;
} // namespace bpr::adapters::agent::detail

#endif // BPR_SRC_ADAPTERS_AGENT_SRC_AGENT_PROTOCOL_LOOP_HPP
