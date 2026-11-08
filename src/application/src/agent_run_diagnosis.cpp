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
		/// The longest line `describe` writes: the counts (with a relaxed run's), and every plugin
		/// token with its separator.
		inline constexpr std::size_t maximum_agent_run_diagnosis_bytes = 512U + (maximum_reported_plugins + 1U) * (maximum_reported_plugin_bytes + 1U);

		/// [[BPR-1891]]. Exactly the list a `plugins_reported` event carries.
		[[nodiscard]] auto plugin_list(std::string_view aText) -> bool
		{
			auto count = std::size_t{ 0U };
			while (true) {
				const auto comma = aText.find(',');
				const auto token = aText.substr(0U, comma);
				if (!is_reported_plugin_token(token) || ++count > maximum_reported_plugins + 1U) {
					return false;
				}
				if (comma == std::string_view::npos) {
					return true;
				}
				aText.remove_prefix(comma + 1U);
			}
		}

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

		/// [[BPR-1918]]. A relaxed run's counts, for its line.
		[[nodiscard]] auto relaxed_counts(const agent_launch_request& aRequest, const agent_run_result& aResult) -> std::optional<std::array<std::uint64_t, relaxed_check_count>>
		{
			if (aRequest.RunChecks != agent_run_checks::relaxed) {
				return std::nullopt;
			}
			auto counts = std::array<std::uint64_t, relaxed_check_count>{ };
			std::ranges::copy(aResult.RelaxedCounts, counts.begin());
			return counts;
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
		if (!aDiagnosis.Plugins.empty()) {
			text += " plugins=" + aDiagnosis.Plugins;
		}
		if (aDiagnosis.AutoMemory) {
			text += " memory=on";
		}
		if (aDiagnosis.Thinking) {
			text += " thinking=on";
		}
		if (aDiagnosis.OutsideList) {
			text += " outside_list=" + std::to_string(*aDiagnosis.OutsideList);
		}
		if (aDiagnosis.Relaxed) {
			text += " relaxed=on";
			for (std::size_t check = 0U; check < relaxed_check_count; ++check) {
				text += " relaxed_" + std::string{ relaxed_check_name(static_cast<relaxed_check>(check)) } + "=" + std::to_string((*aDiagnosis.Relaxed)[check]);
			}
		}
		return text;
	}

	auto parse_agent_run_diagnosis(const std::string_view aText) -> std::optional<agent_run_diagnosis>
	{
		if (aText.size() > maximum_agent_run_diagnosis_bytes) {
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
			} else if (key == "plugins") {
				if (!plugin_list(value)) {
					return std::nullopt;
				}
				result.Plugins = std::string{ value };
			} else if (key == "memory") {
				if (value != "on") {
					return std::nullopt;
				}
				result.AutoMemory = true;
			} else if (key == "outside_list") {
				result.OutsideList = read_number<std::uint64_t>(value);
				if (!result.OutsideList) {
					return std::nullopt;
				}
			} else if (key == "relaxed") {
				if (value != "on") {
					return std::nullopt;
				}
				result.Relaxed.emplace();
			} else if (key.starts_with("relaxed_") && result.Relaxed) {
				auto known = false;
				for (std::size_t check = 0U; check < relaxed_check_count; ++check) {
					if (key.substr(8U) == relaxed_check_name(static_cast<relaxed_check>(check))) {
						const auto count = read_number<std::uint64_t>(value);
						if (!count) {
							return std::nullopt;
						}
						(*result.Relaxed)[check] = *count;
						known                    = true;
					}
				}
				if (!known) {
					return std::nullopt;
				}
			} else if (key == "thinking") {
				if (value != "on") {
					return std::nullopt;
				}
				result.Thinking = true;
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
				   .Plugins             = result.ReportedPlugins,
				   .AutoMemory          = result.ReportedAutoMemory,
				   .Thinking            = result.ReportedThinking,
				   .OutsideList         = aRequest.ReadOnlyTools && aRequest.Protocol == core::agent_protocol::claude_stream_json ? std::optional<std::uint64_t>{ result.OutsideListCommands } : std::nullopt,
				   .Relaxed             = relaxed_counts(aRequest, result),
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
