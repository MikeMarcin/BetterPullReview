#include "bpr/application/operation_diagnostics.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace bpr::application {
	auto diagnostic_operation_of(const core::operation_action aAction) noexcept -> diagnostic_operation
	{
		switch (core::family(aAction)) {
		case core::operation_diagnosis_family::agent_setup:     return diagnostic_operation::agent_setup_check;
		case core::operation_diagnosis_family::agent_workspace: return diagnostic_operation::agent_workspace;
		case core::operation_diagnosis_family::review_mutation: return diagnostic_operation::review_mutation;
		}
		return diagnostic_operation::review_mutation;
	}

	void record_operation(diagnostic_log_port* const aLog, const core::operation_diagnosis& aDiagnosis, const bool aStarted) noexcept
	{
		if (aLog == nullptr) {
			return;
		}
		try {
			const auto text  = core::describe(aDiagnosis);
			auto       event = diagnostic_event{ };
			if (text.empty() || text.size() > event.Summary.Text.size()) {
				return;
			}
			event.Operation = diagnostic_operation_of(aDiagnosis.Action);
			event.Outcome   = aStarted                                     ? diagnostic_outcome::started
				: aDiagnosis.Failure == core::operation_failure::none      ? diagnostic_outcome::succeeded
				: aDiagnosis.Failure == core::operation_failure::cancelled ? diagnostic_outcome::cancelled
																		   : diagnostic_outcome::failed;
			std::ranges::copy(text, event.Summary.Text.begin());
			event.Summary.Size  = static_cast<std::uint16_t>(text.size());
			event.Summary.State = diagnostic_summary_state::sanitized;
			static_cast<void>(aLog->record(event));
		} catch (...) {
			// A diagnostic that cannot be formed is dropped.
		}
	}

	auto operation_stopwatch::milliseconds() const noexcept -> std::uint32_t
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Started).count();
		return elapsed <= 0 ? 0U : static_cast<std::uint32_t>(std::min<long long>(elapsed, std::numeric_limits<std::uint32_t>::max()));
	}

	auto operation_failure_of(const core::agent_setup_failure aFailure) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aFailure) {
		case core::agent_setup_failure::agent_not_found:       return failure::agent_not_found;
		case core::agent_setup_failure::program_refused:       return failure::program_refused;
		case core::agent_setup_failure::no_program_chosen:     return failure::no_program_chosen;
		case core::agent_setup_failure::not_signed_in:         return failure::not_signed_in;
		case core::agent_setup_failure::failed_to_start:       return failure::failed_to_start;
		case core::agent_setup_failure::status_unreadable:     return failure::status_unreadable;
		case core::agent_setup_failure::timed_out:             return failure::timed_out;
		case core::agent_setup_failure::workspace_unavailable: return failure::workspace_unavailable;
		}
		return failure::failed_to_start;
	}

	auto operation_failure_of(const agent_workspace_error& aError) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aError.Code) {
		case agent_workspace_failure::invalid_request: return failure::invalid_request;
		case agent_workspace_failure::cancelled:       return failure::cancelled;
		case agent_workspace_failure::stale:           return failure::stale;
		case agent_workspace_failure::cleanup_failed:  return failure::cleanup_failed;
		case agent_workspace_failure::unavailable:     break;
		}
		switch (aError.Reason) {
		case agent_workspace_unavailable_reason::copy_failed:            return failure::copy_failed;
		case agent_workspace_unavailable_reason::download_failed:        return failure::download_failed;
		case agent_workspace_unavailable_reason::connection_failed:      return failure::connection_failed;
		case agent_workspace_unavailable_reason::sign_in_refused:        return failure::sign_in_refused;
		case agent_workspace_unavailable_reason::too_large:              return failure::too_large;
		case agent_workspace_unavailable_reason::too_large_not_in_clone: return failure::too_large_not_in_clone;
		}
		return failure::copy_failed;
	}

	auto operation_failure_of(const review_mutation_failure aFailure) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aFailure) {
		case review_mutation_failure::unavailable:            return failure::unavailable;
		case review_mutation_failure::refused_by_server:      return failure::refused_by_server;
		case review_mutation_failure::undesignated_target:    return failure::undesignated_target;
		case review_mutation_failure::target_moved:           return failure::target_moved;
		case review_mutation_failure::stale_head:             return failure::stale_head;
		case review_mutation_failure::unsupported:            return failure::unsupported;
		case review_mutation_failure::cancelled:              return failure::cancelled;
		case review_mutation_failure::target_unavailable:     return failure::target_unavailable;
		case review_mutation_failure::malformed_response:     return failure::malformed_response;
		case review_mutation_failure::incompatible_body:      return failure::incompatible_body;
		case review_mutation_failure::credential_unavailable: return failure::credential_unavailable;
		case review_mutation_failure::credential_tool_failed: return failure::credential_tool_failed;
		case review_mutation_failure::unplaceable_line:       return failure::unplaceable_line;
		case review_mutation_failure::refused_not_published:  return failure::refused_not_published;
		case review_mutation_failure::settled_not_published:  return failure::settled_not_published;
		case review_mutation_failure::off_hunk_refused:       return failure::off_hunk_refused;
		case review_mutation_failure::not_confirmed:          return failure::not_confirmed;
		}
		return failure::unavailable;
	}

	auto operation_process_of(const agent_run_outcome aOutcome) noexcept -> core::operation_process
	{
		using process = core::operation_process;
		switch (aOutcome) {
		case agent_run_outcome::success:                 return process::success;
		case agent_run_outcome::private_storage_failure: return process::private_storage_failure;
		case agent_run_outcome::failed_to_start:         return process::failed_to_start;
		case agent_run_outcome::workspace_unavailable:   return process::workspace_unavailable;
		case agent_run_outcome::nonzero_exit:            return process::nonzero_exit;
		case agent_run_outcome::protocol_failure:        return process::protocol_failure;
		case agent_run_outcome::cancelled:               return process::cancelled;
		case agent_run_outcome::timeout:                 return process::timeout;
		case agent_run_outcome::stale_result:            return process::stale_result;
		}
		return process::failed_to_start;
	}
} // namespace bpr::application
