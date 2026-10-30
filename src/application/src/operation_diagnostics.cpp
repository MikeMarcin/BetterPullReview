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
		// [[BPR-1829]].
		case core::operation_diagnosis_family::sign_in:          return diagnostic_operation::sign_in;
		case core::operation_diagnosis_family::credential_vault: return diagnostic_operation::credential_vault;
		case core::operation_diagnosis_family::updater:          return diagnostic_operation::updater;
		// [[BPR-1831]].
		case core::operation_diagnosis_family::storage:       return diagnostic_operation::storage;
		case core::operation_diagnosis_family::clone_scan:    return diagnostic_operation::clone_scan;
		case core::operation_diagnosis_family::source_search: return diagnostic_operation::source_search;
		case core::operation_diagnosis_family::image_fetch:   return diagnostic_operation::image_fetch;
		case core::operation_diagnosis_family::notification:  return diagnostic_operation::notification;
		case core::operation_diagnosis_family::mcp_helper:    return diagnostic_operation::mcp_helper;
		case core::operation_diagnosis_family::tour:          return diagnostic_operation::tour_operation;
		case core::operation_diagnosis_family::cycle_round:   return diagnostic_operation::cycle_round;
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

	auto operation_failure_of(const core::device_authorization_stage aStage, const core::device_authorization_reason_code aReason) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		using stage   = core::device_authorization_stage;
		switch (aStage) {
		case stage::authorized:             return failure::none;
		case stage::cancelled:              return failure::cancelled;
		case stage::denied:                 return failure::authorization_denied;
		case stage::expired:                return failure::authorization_expired;
		case stage::disabled:               return failure::device_flow_disabled;
		case stage::unavailable:
		case stage::failed:
		case stage::idle:
		case stage::requested:
		case stage::awaiting_authorization:
		case stage::cancelling:             break;
		}
		using reason = core::device_authorization_reason_code;
		switch (aReason) {
		case reason::authorization_denied:     return failure::authorization_denied;
		case reason::authorization_expired:    return failure::authorization_expired;
		case reason::device_flow_disabled:     return failure::device_flow_disabled;
		case reason::cancelled:                return failure::cancelled;
		case reason::network_unavailable:      return failure::network_unavailable;
		case reason::certificate_rejected:     return failure::certificate_rejected;
		case reason::redirect_rejected:        return failure::redirect_rejected;
		case reason::timed_out:                return failure::timed_out;
		case reason::malformed_response:       return failure::malformed_response;
		case reason::response_too_large:       return failure::response_too_large;
		case reason::vault_unavailable:        return failure::vault_unavailable;
		case reason::device_flow_unavailable:
		case reason::none:
		case reason::flow_in_progress:
		case reason::no_active_flow:
		case reason::cancellation_in_progress:
		case reason::stale_completion:         break;
		}
		return aStage == stage::failed ? failure::transport_failed : failure::device_flow_unavailable;
	}

	auto operation_failure_of(const device_authorization_refresh_failure aFailure) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aFailure) {
		case device_authorization_refresh_failure::none:              return failure::none;
		case device_authorization_refresh_failure::vault_unavailable: return failure::vault_unavailable;
		case device_authorization_refresh_failure::refresh_absent:    return failure::refresh_absent;
		case device_authorization_refresh_failure::refresh_rejected:  return failure::refresh_rejected;
		case device_authorization_refresh_failure::transport_failed:  return failure::transport_failed;
		case device_authorization_refresh_failure::storage_failed:    return failure::storage_failed;
		}
		return failure::transport_failed;
	}

	auto operation_failure_of(const github_credential_vault_failure aFailure) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aFailure) {
		case github_credential_vault_failure::none:               return failure::none;
		case github_credential_vault_failure::invalid_request:    return failure::invalid_request;
		case github_credential_vault_failure::unavailable:        return failure::unavailable;
		case github_credential_vault_failure::locked:             return failure::locked;
		case github_credential_vault_failure::access_denied:      return failure::access_denied;
		case github_credential_vault_failure::corrupt:            return failure::corrupt;
		case github_credential_vault_failure::operation_failed:   return failure::operation_failed;
		case github_credential_vault_failure::allocation_failure: return failure::allocation_failure;
		}
		return failure::operation_failed;
	}

	auto operation_failure_of(const core::release_update_reason aReason) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		using reason  = core::release_update_reason;
		switch (aReason) {
		case reason::none:                   return failure::none;
		case reason::current:
		case reason::skipped:
		case reason::disabled:
		case reason::managed:
		case reason::unsupported_platform:
		case reason::untrusted_installation:
		case reason::read_only:
		case reason::translocated:
		case reason::mounted_image:
		case reason::not_due:
		case reason::no_candidate:           return failure::policy_refused;
		case reason::access_required:        return failure::access_required;
		case reason::unauthenticated:        return failure::unauthenticated;
		case reason::forbidden:              return failure::forbidden;
		case reason::inaccessible:           return failure::inaccessible;
		case reason::offline:                return failure::offline;
		case reason::rate_limited:           return failure::rate_limited;
		case reason::malformed_release:      return failure::malformed_release;
		case reason::invalid_version:        return failure::invalid_version;
		case reason::unstable_release:       return failure::unstable_release;
		case reason::no_compatible_asset:    return failure::no_compatible_asset;
		case reason::ambiguous_asset:        return failure::ambiguous_asset;
		case reason::invalid_digest:         return failure::invalid_digest;
		case reason::size_limit:             return failure::size_limit;
		case reason::certificate:            return failure::certificate_rejected;
		case reason::redirect:               return failure::redirect_rejected;
		case reason::timed_out:              return failure::timed_out;
		case reason::cancelled:              return failure::cancelled;
		case reason::unavailable:            return failure::unavailable;
		case reason::unsafe_archive:         return failure::unsafe_archive;
		case reason::invalid_bundle:         return failure::invalid_bundle;
		case reason::wrong_architecture:     return failure::wrong_architecture;
		case reason::wrong_signer:           return failure::wrong_signer;
		case reason::digest_mismatch:        return failure::digest_mismatch;
		case reason::changed_candidate:      return failure::changed_candidate;
		case reason::busy:                   return failure::busy;
		case reason::stale_completion:       return failure::stale_completion;
		case reason::invalid_progress:       return failure::invalid_progress;
		case reason::identity_exhausted:     return failure::identity_exhausted;
		case reason::installer_unavailable:  return failure::installer_unavailable;
		case reason::flush_failed:           return failure::flush_failed;
		case reason::quit_refused:           return failure::quit_refused;
		case reason::recovery_required:      return failure::recovery_required;
		case reason::shutdown:               return failure::shutdown;
		}
		return failure::unavailable;
	}

	auto operation_action_of(const core::release_update_operation aOperation) noexcept -> core::operation_action
	{
		using action = core::operation_action;
		switch (aOperation) {
		case core::release_update_operation::check:           return action::update_check;
		case core::release_update_operation::download:        return action::update_download;
		case core::release_update_operation::verify:          return action::update_verify;
		case core::release_update_operation::refresh_install: return action::update_refresh;
		case core::release_update_operation::install:         return action::update_apply;
		}
		return action::update_check;
	}

	auto operation_failure_of(const cycle_store_failure aFailure) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aFailure) {
		case cycle_store_failure::unavailable:        return failure::unavailable;
		case cycle_store_failure::corrupt:            return failure::corrupt;
		case cycle_store_failure::schema_mismatch:    return failure::schema_mismatch;
		case cycle_store_failure::wrong_profile:      return failure::wrong_profile;
		case cycle_store_failure::not_found:          return failure::not_found;
		case cycle_store_failure::conflict:           return failure::conflict;
		case cycle_store_failure::refused:            return failure::refused;
		case cycle_store_failure::operation_reused:   return failure::operation_reused;
		case cycle_store_failure::bounds:             return failure::bounds;
		case cycle_store_failure::object_unavailable: return failure::object_unavailable;
		case cycle_store_failure::quota:              return failure::quota;
		case cycle_store_failure::preview_stale:      return failure::preview_stale;
		case cycle_store_failure::cancelled:          return failure::cancelled;
		case cycle_store_failure::store_damaged:      return failure::store_damaged;
		}
		return failure::unavailable;
	}

	auto operation_failure_of(const core::cycle_authorization aDecision) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		switch (aDecision) {
		case core::cycle_authorization::allowed:      return failure::none;
		case core::cycle_authorization::not_granted:  return failure::not_granted;
		case core::cycle_authorization::revoked:      return failure::revoked;
		case core::cycle_authorization::wrong_target: return failure::wrong_target;
		default:                                      return failure::not_authorized;
		}
	}

	auto tour_cli_failure(const std::string_view aCode, const int aExit) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		if (aCode == "cancelled" || aExit == 130) {
			return failure::cancelled;
		}
		if (aCode == "authentication_unavailable" || aCode == "authentication_required" || aCode == "vault_unavailable") {
			return failure::authentication_unavailable;
		}
		if (aCode == "transport_unavailable") {
			return failure::transport_unavailable;
		}
		if (aCode == "remote_incomplete") {
			return failure::remote_incomplete;
		}
		if (aCode == "storage_unavailable") {
			return failure::storage_unavailable;
		}
		switch (aExit) {
		case 2:  return failure::invalid_input;
		case 3:  return failure::interaction_required;
		case 4:  return failure::conflict;
		case 5:  return failure::unavailable;
		case 6:  return failure::outcome_unknown;
		default: return failure::local_failure;
		}
	}
} // namespace bpr::application
