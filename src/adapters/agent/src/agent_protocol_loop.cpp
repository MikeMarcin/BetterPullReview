#include "agent_protocol_loop.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
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
				const application::agent_launch_request&           aRequest,
				const application::agent_output_sink&              aSink,
				agent_protocol_transport&                          aTransport,
				std::optional<application::agent_protocol_decoder> aDecoder = std::nullopt)
				: Request{ aRequest }
				, Sink{ aSink }
				, Transport{ aTransport }
				, Result{ .Session = aRequest.Session, .Run = aRequest.Run }
				, Decoder{ aDecoder ? std::move(*aDecoder) : application::agent_protocol_decoder{ aRequest.Protocol, aRequest.WritingOnly, aRequest.Purpose, aRequest.ReadOnlyTools, aRequest.RunChecks } }
				, IsAppServer{
					aRequest.Protocol == core::agent_protocol::codex_app_server
				}
			{
				Decoder.expect_native_handle(Request.NativeHandle);
				Decoder.expect_response(Request.NativeTurn);
				if (Request.NativeConnected) {
					Decoder.continue_native(Request.NativeHandle, Request.NativeTurn);
					AppServerPhase = IsAppServer ? 2U : 0U;
				}
			}

			[[nodiscard]] auto run() -> application::agent_run_result
			{
				if (const auto admitted = application::admit_agent_input(Request); !admitted) {
					record_protocol_failure(admitted.error() == application::agent_launch_failure::input_too_large ? application::agent_protocol_failure::input_too_large : application::agent_protocol_failure::invalid_runtime_metadata);
				} else {
					Result.Metadata = Request.Metadata;
					write_input(Request.Input, !IsAppServer || Request.NativeConnected);
				}
				if (!IsAppServer && !has_cause() && !native()) {
					Transport.close_input();
				}

				while (!has_cause() && !ProcessExit) {
					drain_available();
					if (IsAppServer && !native() && !has_cause() && Decoder.turn_complete() && !InputClosed) {
						// Modern App Server treats EOF as cancellation. Retain stdin
						// until this explicit one-shot turn has actually completed.
						Transport.close_input();
						InputClosed = true;
					}
					if (native() && !has_cause() && Decoder.turn_complete()) {
						finish_protocol();
						ClassifiedOutcome = application::agent_run_outcome::success;
						break;
					}
					if (!has_cause()) {
						record_wait(Transport.wait());
					}
				}

				if (ProcessExit && !has_cause()) {
					drain_available();
					finish_protocol();
					// [[BPR-1827]]. A child that exits with an error before
					// finishing its transcript is reported by its exit, which
					// says more than the unfinished frame it left.
					const auto exitedWithError = !ProcessExit->ExitedNormally || ProcessExit->ExitCode != 0;
					if (exitedWithError && ProtocolFailure == application::agent_protocol_failure::incomplete_frame && !ForcedOutcome) {
						ProtocolFailure.reset();
					}
					if (!has_cause()) {
						Result.ExitCode = ProcessExit->ExitCode;
						ClassifiedOutcome
							= ProcessExit->ExitedNormally
								&& ProcessExit->ExitCode == 0
							? application::agent_run_outcome::success
							: application::agent_run_outcome::nonzero_exit;
					}
				}

				const auto terminated = native() && !has_cause() && ClassifiedOutcome == application::agent_run_outcome::success ? true : Transport.terminate();
				if (
					!terminated && !has_cause()
					&& ClassifiedOutcome
						== application::agent_run_outcome::success) {
					record_protocol_failure(
						application::agent_protocol_failure::malformed_frame);
					Result.ExitCode.reset();
				}

				if (Request.PrivateInputClaim) {
					Request.PrivateInputClaim->release();
				}
				complete_result();
				return std::move(Result);
			}

			/// \brief Drains bounded idle notifications through the completed turn's decoder.
			[[nodiscard]] auto idle() -> bool
			{
				for (const auto stream : { application::agent_protocol_event_kind::output, application::agent_protocol_event_kind::usage_report }) {
					while (true) {
						const auto read = stream == application::agent_protocol_event_kind::output ? Transport.read_standard_output(Buffer) : Transport.read_standard_error(Buffer);
						if (!valid_read(read) || read.Status == agent_protocol_pipe_read_status::failed || read.Status == agent_protocol_pipe_read_status::closed) {
							return false;
						}
						if (read.Status == agent_protocol_pipe_read_status::no_data) {
							break;
						}
						if (read.Count > 64U * 1024U - IdleBytes) {
							return false;
						}
						IdleBytes += read.Count;
						if (stream == application::agent_protocol_event_kind::output) {
							const auto events = Decoder.push({ Buffer.data(), read.Count });
							if (!events || !events->empty()) {
								return false;
							}
						}
					}
				}
				return true;
			}

			[[nodiscard]] auto accepts_next_turn() const noexcept -> bool { return Decoder.turn_complete(); }

			[[nodiscard]] auto release_decoder() -> application::agent_protocol_decoder { return std::move(Decoder); }

		private:
			[[nodiscard]] auto native() const noexcept -> bool { return !Request.NativeConversation.empty() || Request.NativeConnection != nullptr; }
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

			void write_input(std::string_view aBytes, bool aPrivate = false)
			{
				if (aBytes.size() > application::maximum_agent_input_bytes - InputBytes) {
					record_protocol_failure(application::agent_protocol_failure::input_too_large);
					return;
				}
				if (aPrivate && Request.PrivateInputClaim && !Request.PrivateInputClaim->permitted()) {
					record_forced_outcome(application::agent_run_outcome::cancelled);
					Request.PrivateInputClaim->release();
					return;
				}
				InputBytes += aBytes.size();
				record_write(Transport.write(aBytes));
				if (aPrivate && Request.PrivateInputClaim) {
					Request.PrivateInputClaim->release();
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
					.Run      = Request.Run,
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
				if (native() && aMessage.Kind == application::agent_protocol_event_kind::context_channel_ready && Request.SaveNativeHandle && !(*Request.SaveNativeHandle)(aMessage.Text)) {
					record_forced_outcome(application::agent_run_outcome::private_storage_failure);
					return;
				}
				write_input(*followup, aMessage.Kind == application::agent_protocol_event_kind::context_channel_ready);
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
			}

			/// \brief Turns one lifted payload into this run's findings, once.
			///
			/// \details This is where a completed turn's bytes become
			/// `core::agent_finding` values, and it lives in the shared loop so
			/// that the POSIX and Windows transports get it once and neither
			/// reimplements it. The loop holds no core store, no snapshot, and
			/// no verdict: whether an anchor holds is decided later, by the
			/// host, against the session's frozen snapshot.
			void process_finding_payload(
				const application::agent_protocol_event& aMessage)
			{
				if (Result.Findings.has_value()) {
					// One completed turn returns one payload. A second is not
					// merged with the first, because merging would let a later
					// frame append findings to an answer already given.
					record_protocol_failure(
						application::agent_protocol_failure::
							malformed_finding_payload);
					return;
				}
				auto decoded = application::decode_agent_finding_payload(
					Request.Protocol, aMessage.Text, Request.Session);
				if (!decoded) {
					record_protocol_failure(decoded.error());
					return;
				}
				Result.Findings = std::move(*decoded);
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
					} else if (
						message.Kind
						== application::agent_protocol_event_kind::finding_payload) {
						process_finding_payload(message);
						if (has_cause()) {
							return;
						}
					} else if (message.Kind == application::agent_protocol_event_kind::usage_report) {
						Result.ReportedCostUsd = message.Text;
					} else if (message.Kind == application::agent_protocol_event_kind::plugins_reported) {
						// [[BPR-1891]]. Named in diagnostics, never refused.
						Result.ReportedPlugins = message.Text;
					} else if (message.Kind == application::agent_protocol_event_kind::auto_memory_reported) {
						// [[BPR-1896]]. Named in diagnostics, never refused.
						Result.ReportedAutoMemory = true;
					} else if (message.Kind == application::agent_protocol_event_kind::thinking_reported) {
						// [[BPR-1899]]. Named in diagnostics; the content was never kept.
						Result.ReportedThinking = true;
					} else if (message.Kind == application::agent_protocol_event_kind::tool_activity) {
						// [[BPR-1900]]. For the tour session's transcript, bounded.
						if (Result.ToolActivity.size() < application::maximum_agent_tool_activity) {
							Result.ToolActivity.emplace_back(message.Text, message.Item);
						}
					} else if (message.Kind == application::agent_protocol_event_kind::outside_list_ran) {
						// [[BPR-1912]]. Reported, never refused.
						++Result.OutsideListCommands;
					} else if (message.Kind == application::agent_protocol_event_kind::check_relaxed) {
						// [[BPR-1918]]. Counted by check; its activity names it.
						for (std::size_t check = 0U; check < application::relaxed_check_count; ++check) {
							if (message.Text == application::relaxed_check_name(static_cast<application::relaxed_check>(check))) {
								++Result.RelaxedCounts[check];
							}
						}
					} else if (message.Kind == application::agent_protocol_event_kind::model_catalog) {
						// Owner look v006: a catalog run ends once its list is in.
						Result.ModelCatalog = message.Text;
						Transport.close_input();
						InputClosed = true;
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
					// Claude announces its session before its first answer. Save
					// that identity immediately, including when the turn is later
					// interrupted; uncertainty must not lose the only resume ID.
					if (native() && Request.Protocol == core::agent_protocol::claude_stream_json
						&& Request.SaveNativeHandle && !Decoder.native_handle().empty()
						&& SavedNativeHandle != Decoder.native_handle()) {
						if (!(*Request.SaveNativeHandle)(Decoder.native_handle())) {
							record_forced_outcome(application::agent_run_outcome::private_storage_failure);
							return;
						}
						SavedNativeHandle = Decoder.native_handle();
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
				// A catalog run writes one follow-up, a conversation two.
				if (IsAppServer && AppServerPhase != (Request.Purpose == application::agent_launch_purpose::model_catalog ? 1U : 2U)) {
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
				Result.NativeHandle = Decoder.native_handle();
				if (native()) {
					Result.NativeProcess = Transport.process_identity();
				}
				Result.AssistantItems      = Decoder.assistant_items();
				Result.StandardOutputBytes = Decoder.pushed_bytes();
				if (ProtocolFailure) {
					Result.RejectedOutputLine = Decoder.rejected_line();
				}
				for (const auto& report : Decoder.metadata().ProtocolReported) {
					auto appended = core::append_runtime_report(Result.Metadata, report);
					if (!appended) {
						record_protocol_failure(application::agent_protocol_failure::invalid_runtime_metadata);
						break;
					}
					Result.Metadata = std::move(*appended);
				}
				if (const auto conflicts = core::inspect_runtime_metadata(Result.Metadata)) {
					Result.ProvenanceConflicts = *conflicts;
				}

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
					// A run that failed did not complete a turn, so whatever it
					// wrote is not a turn's answer. Staleness is the one reason
					// a payload survives an unsuccessful result, and staleness
					// is decided by the host rather than here.
					Result.Findings.reset();
				}
			}

			const application::agent_launch_request&           Request;
			const application::agent_output_sink&              Sink;
			agent_protocol_transport&                          Transport;
			application::agent_run_result                      Result;
			application::agent_protocol_decoder                Decoder;
			std::string                                        SavedNativeHandle;
			std::array<char, protocol_buffer_bytes>            Buffer{ };
			bool                                               InputClosed    = false;
			std::size_t                                        IdleBytes      = 0U;
			std::size_t                                        InputBytes     = 0;
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
		if (aRequest.NativeConnection == nullptr) {
			return protocol_loop{ aRequest, aSink, aTransport }.run();
		}
		auto* const connection = aRequest.NativeConnection;
		auto        request    = aRequest;
		auto        last       = application::agent_run_result{ };
		auto        decoder    = std::optional<application::agent_protocol_decoder>{ };
		while (true) {
			auto loop          = protocol_loop{ request, aSink, aTransport, std::move(decoder) };
			last               = loop.run();
			const auto success = last.Outcome == application::agent_run_outcome::success;
			const auto handle  = last.NativeHandle;
			connection->complete(last);
			if (!success) {
				return last;
			}
			aTransport.begin_turn(std::numeric_limits<int>::max());
			const auto idleEnd = std::chrono::steady_clock::now() + std::chrono::minutes{ 5 };
			while (true) {
				if (connection->cancelled() || !loop.idle() || std::chrono::steady_clock::now() >= idleEnd) {
					static_cast<void>(aTransport.terminate());
					return last;
				}
				if (auto next = loop.accepts_next_turn() ? connection->next() : std::nullopt) {
					next->NativeHandle     = handle;
					next->NativeConnection = connection;
					auto prepared          = application::prepare_native_agent_turn(std::move(*next), true);
					if (!prepared) {
						connection->complete({ .Session = request.Session, .Outcome = application::agent_run_outcome::protocol_failure });
						static_cast<void>(aTransport.terminate());
						return last;
					}
					decoder = loop.release_decoder();
					request = std::move(*prepared);
					aTransport.begin_turn(connection->timeout_milliseconds());
					break;
				}
				if (aTransport.wait().Status != agent_protocol_wait_status::running) {
					static_cast<void>(aTransport.terminate());
					return last;
				}
			}
		}
	}

	auto agent_protocol_read_ahead_limit(const agent_protocol_stream aStream) noexcept
		-> std::size_t
	{
		// Standard output's ceiling is the one the decoder the loop drives
		// applies to the whole stream; standard error's is drain_standard_error's.
		switch (aStream) {
		case agent_protocol_stream::standard_output:
			return application::maximum_agent_stdout_bytes + 1U;
		case agent_protocol_stream::standard_error:
			return application::maximum_agent_stderr_bytes + 1U;
		}
		return 0U;
	}
} // namespace bpr::adapters::agent::detail
