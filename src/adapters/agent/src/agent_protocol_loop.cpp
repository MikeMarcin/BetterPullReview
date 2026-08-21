#include "agent_protocol_loop.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::agent::detail {
	namespace {
		constexpr auto protocol_buffer_bytes = std::size_t{ 16U * 1024U };

		class protocol_loop final
		{
		public:
			protocol_loop(
				const application::agent_launch_request& aRequest,
				const application::agent_output_sink&    aSink,
				agent_protocol_transport&                aTransport)
				: Request{ aRequest }
				, Sink{ aSink }
				, Transport{ aTransport }
				, Result{ .Session = aRequest.Session }
				, Decoder{ aRequest.Protocol }
				, IsAppServer{
					aRequest.Protocol == core::agent_protocol::codex_app_server
				}
			{
			}

			[[nodiscard]] auto run() -> application::agent_run_result
			{
				record_write(Transport.write(Request.Input));
				if (!IsAppServer && !has_cause()) {
					Transport.close_input();
				}

				while (!has_cause() && !ProcessExit) {
					drain_available();
					if (!has_cause()) {
						record_wait(Transport.wait());
					}
				}

				if (ProcessExit && !has_cause()) {
					drain_available();
					finish_protocol();
					if (!has_cause()) {
						Result.ExitCode = ProcessExit->ExitCode;
						ClassifiedOutcome
							= ProcessExit->ExitedNormally
								&& ProcessExit->ExitCode == 0
							? application::agent_run_outcome::success
							: application::agent_run_outcome::nonzero_exit;
					}
				}

				const auto terminated = Transport.terminate();
				if (
					!terminated && !has_cause()
					&& ClassifiedOutcome
						== application::agent_run_outcome::success) {
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					Result.ExitCode.reset();
				}

				complete_result();
				return std::move(Result);
			}

		private:
			[[nodiscard]] auto has_cause() const noexcept -> bool
			{
				return ForcedOutcome.has_value() || ProtocolFailure.has_value();
			}

			void record_protocol_failure(
				const application::agent_protocol_failure aFailure) noexcept
			{
				if (!has_cause()) {
					ProtocolFailure = aFailure;
				}
			}

			void record_forced_outcome(
				const application::agent_run_outcome aOutcome) noexcept
			{
				if (!has_cause()) {
					ForcedOutcome = aOutcome;
				}
			}

			void record_write(const agent_protocol_write_status aStatus) noexcept
			{
				switch (aStatus) {
				case agent_protocol_write_status::success:
					return;
				case agent_protocol_write_status::cancelled:
					record_forced_outcome(application::agent_run_outcome::cancelled);
					return;
				case agent_protocol_write_status::timeout:
					record_forced_outcome(application::agent_run_outcome::timeout);
					return;
				case agent_protocol_write_status::process_exited:
				case agent_protocol_write_status::failed:
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					return;
				}
			}

			void record_wait(const agent_protocol_wait_result& aWait) noexcept
			{
				switch (aWait.Status) {
				case agent_protocol_wait_status::running:
					return;
				case agent_protocol_wait_status::exited:
					ProcessExit = aWait;
					return;
				case agent_protocol_wait_status::cancelled:
					record_forced_outcome(application::agent_run_outcome::cancelled);
					return;
				case agent_protocol_wait_status::timeout:
					record_forced_outcome(application::agent_run_outcome::timeout);
					return;
				case agent_protocol_wait_status::failed:
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					return;
				}
			}

			[[nodiscard]] auto valid_read(
				const agent_protocol_pipe_read_result& aRead) noexcept -> bool
			{
				const auto validData
					= aRead.Status == agent_protocol_pipe_read_status::data
					&& aRead.Count > 0U && aRead.Count <= Buffer.size();
				const auto validEmpty
					= aRead.Status != agent_protocol_pipe_read_status::data
					&& aRead.Count == 0U && aRead.BatchComplete;
				if (!validData && !validEmpty) {
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					return false;
				}
				return true;
			}

			void drain_standard_error()
			{
				while (!has_cause()) {
					const auto read = Transport.read_standard_error(Buffer);
					if (!valid_read(read)) {
						return;
					}
					switch (read.Status) {
					case agent_protocol_pipe_read_status::no_data:
					case agent_protocol_pipe_read_status::closed:
						return;
					case agent_protocol_pipe_read_status::failed:
						record_protocol_failure(
							application::agent_protocol_failure::malformed_frame);
						return;
					case agent_protocol_pipe_read_status::data:
						break;
					}
					const auto remaining = application::maximum_agent_stderr_bytes
						- Result.StandardError.size();
					if (read.Count > remaining) {
						record_protocol_failure(
							application::agent_protocol_failure::output_too_large);
						return;
					}
					Result.StandardError.append(Buffer.data(), read.Count);
				}
			}

			[[nodiscard]] auto publish(
				const application::agent_protocol_event& aMessage) -> bool
			{
				auto event = application::agent_stream_event{
					.Session  = Request.Session,
					.Sequence = Sequence++,
					.Text     = aMessage.Text,
				};
				Result.Events.push_back(event);
				if (Sink) {
					try {
						Sink(event);
					} catch (...) {
						return false;
					}
				}
				return true;
			}

			void process_control(
				const application::agent_protocol_event& aMessage)
			{
				const auto expectedPhase
					= aMessage.Kind
						== application::agent_protocol_event_kind::initialization_complete
					? 0U
					: 1U;
				if (!IsAppServer || AppServerPhase != expectedPhase) {
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					return;
				}
				const auto followup
					= application::build_agent_protocol_followup(Request, aMessage);
				if (!followup) {
					record_protocol_failure(followup.error());
					return;
				}
				record_write(Transport.write(*followup));
				if (has_cause()) {
					return;
				}
				const auto acknowledged
					= Decoder.acknowledge_followup(aMessage.Kind);
				if (!acknowledged) {
					record_protocol_failure(acknowledged.error());
					return;
				}
				++AppServerPhase;
				if (AppServerPhase == 2U) {
					Transport.close_input();
				}
			}

			void process_messages(
				const std::span<const application::agent_protocol_event> aMessages)
			{
				for (const auto& message : aMessages) {
					if (message.Kind == application::agent_protocol_event_kind::output) {
						if (!publish(message)) {
							record_protocol_failure(
								application::agent_protocol_failure::malformed_frame);
							return;
						}
					} else {
						process_control(message);
						if (has_cause()) {
							return;
						}
					}
				}
			}

			void drain_standard_output()
			{
				auto batchMessages
					= std::vector<application::agent_protocol_event>{ };
				auto batchPending = false;
				while (!has_cause()) {
					const auto read = Transport.read_standard_output(Buffer);
					if (!valid_read(read)) {
						return;
					}
					switch (read.Status) {
					case agent_protocol_pipe_read_status::no_data:
					case agent_protocol_pipe_read_status::closed:
						if (batchPending) {
							record_protocol_failure(
								application::agent_protocol_failure::malformed_frame);
						}
						return;
					case agent_protocol_pipe_read_status::failed:
						record_protocol_failure(
							application::agent_protocol_failure::malformed_frame);
						return;
					case agent_protocol_pipe_read_status::data:
						break;
					}
					const auto decoded = Decoder.push(
						std::string_view{ Buffer.data(), read.Count });
					if (!decoded) {
						record_protocol_failure(decoded.error());
						return;
					}
					batchMessages.insert(
						batchMessages.end(),
						std::make_move_iterator(decoded->begin()),
						std::make_move_iterator(decoded->end()));
					batchPending = !read.BatchComplete;
					if (read.BatchComplete) {
						process_messages(batchMessages);
						batchMessages.clear();
					}
				}
			}

			void drain_available()
			{
				drain_standard_error();
				if (!has_cause()) {
					drain_standard_output();
				}
			}

			void finish_protocol()
			{
				const auto trailing = Decoder.finish();
				if (!trailing) {
					record_protocol_failure(trailing.error());
					return;
				}
				if (IsAppServer && AppServerPhase != 2U) {
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					return;
				}
				for (const auto& message : *trailing) {
					if (
						message.Kind != application::agent_protocol_event_kind::output
						|| !publish(message)) {
						record_protocol_failure(
							application::agent_protocol_failure::malformed_frame);
						return;
					}
				}
			}

			void complete_result()
			{
				if (ForcedOutcome) {
					Result.Outcome = *ForcedOutcome;
				} else if (ProtocolFailure) {
					Result.Outcome         = application::agent_run_outcome::protocol_failure;
					Result.ProtocolFailure = ProtocolFailure;
				} else {
					Result.Outcome = ClassifiedOutcome.value_or(
						application::agent_run_outcome::protocol_failure);
					if (!ClassifiedOutcome) {
						Result.ProtocolFailure
							= application::agent_protocol_failure::malformed_frame;
					}
				}
				if (Result.Outcome != application::agent_run_outcome::success) {
					Result.Events.clear();
				}
			}

			const application::agent_launch_request&           Request;
			const application::agent_output_sink&              Sink;
			agent_protocol_transport&                          Transport;
			application::agent_run_result                      Result;
			application::agent_protocol_decoder                Decoder;
			std::array<char, protocol_buffer_bytes>            Buffer{ };
			std::uint64_t                                      Sequence       = 0U;
			std::uint8_t                                       AppServerPhase = 0U;
			bool                                               IsAppServer    = false;
			std::optional<application::agent_protocol_failure> ProtocolFailure;
			std::optional<application::agent_run_outcome>      ForcedOutcome;
			std::optional<application::agent_run_outcome>      ClassifiedOutcome;
			std::optional<agent_protocol_wait_result>          ProcessExit;
		};
	} // namespace

	auto run_agent_protocol_loop(
		const application::agent_launch_request& aRequest,
		const application::agent_output_sink&    aSink,
		agent_protocol_transport&                aTransport)
		-> application::agent_run_result
	{
		return protocol_loop{ aRequest, aSink, aTransport }.run();
	}
} // namespace bpr::adapters::agent::detail
