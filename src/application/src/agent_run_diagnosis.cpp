#include "bpr/application/agent_run_diagnosis.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace bpr::application {
	namespace {
		inline constexpr std::size_t agent_run_outcome_count = static_cast<std::size_t>(agent_run_outcome::stale_result) + 1U;

		template <typename Value>
		[[nodiscard]] auto read_number(const std::string_view aText) -> std::optional<Value>
		{
			auto value = Value{ };
			if (aText.empty() || aText.size() > 20U) {
				return std::nullopt;
			}
			const auto [end, error] = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (error != std::errc{ } || end != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}
	} // namespace

	auto describe(const agent_run_diagnosis& aDiagnosis) -> std::string
	{
		auto text = std::string{ "protocol=" } + std::string{ core::stable_name(aDiagnosis.Protocol) };
		if (aDiagnosis.Outcome) {
			text += " outcome=" + std::string{ name(*aDiagnosis.Outcome) };
		}
		if (aDiagnosis.Failure) {
			text += " failure=" + std::string{ name(*aDiagnosis.Failure) };
		}
		if (aDiagnosis.ExitCode) {
			text += " exit=" + std::to_string(*aDiagnosis.ExitCode);
		}
		text += " stdout_bytes=" + std::to_string(aDiagnosis.StandardOutputBytes);
		text += " stderr_bytes=" + std::to_string(aDiagnosis.StandardErrorBytes);
		return text;
	}

	auto parse_agent_run_diagnosis(const std::string_view aText) -> std::optional<agent_run_diagnosis>
	{
		if (aText.size() > 256U) {
			return std::nullopt;
		}
		auto result      = agent_run_diagnosis{ };
		auto sawProtocol = false;
		auto rest        = aText;
		while (!rest.empty()) {
			const auto space = rest.find(' ');
			const auto token = rest.substr(0U, space);
			rest             = space == std::string_view::npos ? std::string_view{ } : rest.substr(space + 1U);
			const auto equal = token.find('=');
			if (equal == std::string_view::npos) {
				return std::nullopt;
			}
			const auto key   = token.substr(0U, equal);
			const auto value = token.substr(equal + 1U);
			if (key == "protocol") {
				for (std::size_t index = 0U; index < core::agent_protocol_count; ++index) {
					const auto protocol = static_cast<core::agent_protocol>(index);
					if (core::stable_name(protocol) == value) {
						result.Protocol = protocol;
						sawProtocol     = true;
					}
				}
			} else if (key == "outcome") {
				for (std::size_t index = 0U; index < agent_run_outcome_count; ++index) {
					const auto outcome = static_cast<agent_run_outcome>(index);
					if (name(outcome) == value) {
						result.Outcome = outcome;
					}
				}
			} else if (key == "failure") {
				for (std::size_t index = 0U; index < agent_protocol_failure_count; ++index) {
					const auto failure = static_cast<agent_protocol_failure>(index);
					if (name(failure) == value) {
						result.Failure = failure;
					}
				}
			} else if (key == "exit") {
				result.ExitCode = read_number<int>(value);
			} else if (key == "stdout_bytes") {
				result.StandardOutputBytes = read_number<std::uint64_t>(value).value_or(~std::uint64_t{ });
			} else if (key == "stderr_bytes") {
				result.StandardErrorBytes = read_number<std::uint64_t>(value).value_or(~std::uint64_t{ });
			} else {
				return std::nullopt;
			}
		}
		// Exactly what describe writes: any unread, repeated or reordered
		// token makes the round trip differ.
		if (!sawProtocol || describe(result) != aText) {
			return std::nullopt;
		}
		return result;
	}

	auto diagnostic_outcome_of(const agent_run_outcome aOutcome) noexcept -> diagnostic_outcome
	{
		switch (aOutcome) {
		case agent_run_outcome::success:   return diagnostic_outcome::succeeded;
		case agent_run_outcome::cancelled: return diagnostic_outcome::cancelled;
		default:                           return diagnostic_outcome::failed;
		}
	}

	diagnosed_agent_process::diagnosed_agent_process(agent_process_port& aProcess, std::shared_ptr<diagnostic_log_port> aLog)
		: Process{ aProcess }
		, Log{ std::move(aLog) }
	{
	}

	auto diagnosed_agent_process::run(
		const agent_launch_request& aRequest,
		const int                   aTimeoutMilliseconds,
		const agent_output_sink&    aSink,
		std::stop_token             aStop) -> agent_run_result
	{
		record(diagnostic_outcome::started, { .Protocol = aRequest.Protocol });
		auto result = Process.run(aRequest, aTimeoutMilliseconds, aSink, std::move(aStop));
		record(diagnostic_outcome_of(result.Outcome),
			   {
				   .Protocol            = aRequest.Protocol,
				   .Outcome             = result.Outcome,
				   .Failure             = result.Outcome == agent_run_outcome::protocol_failure ? result.ProtocolFailure : std::nullopt,
				   .ExitCode            = result.ExitCode,
				   .StandardOutputBytes = result.StandardOutputBytes,
				   .StandardErrorBytes  = result.StandardError.size(),
			   });
		return result;
	}

	void diagnosed_agent_process::cancel() noexcept { Process.cancel(); }

	void diagnosed_agent_process::record(const diagnostic_outcome aOutcome, const agent_run_diagnosis& aDiagnosis) noexcept
	{
		if (!Log) {
			return;
		}
		try {
			auto       event = diagnostic_event{ .Operation = diagnostic_operation::agent_run, .Outcome = aOutcome };
			const auto text  = describe(aDiagnosis);
			if (text.size() <= event.Summary.Text.size()) {
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = diagnostic_summary_state::sanitized;
			}
			static_cast<void>(Log->record(event));
		} catch (...) {
			// A diagnostic that cannot be built changes nothing about the run.
			static_cast<void>(std::current_exception());
		}
	}
} // namespace bpr::application
