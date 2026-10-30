#include "bpr/core/operation_diagnosis.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <system_error>

namespace bpr::core {
	namespace {
		constexpr std::array<std::string_view, 63>  action_names{ "agent_status_check", "agent_program_check", "agent_custom_check", "workspace_materialize", "workspace_cleanup", "observe_target", "publish_comment", "reconcile_publication", "repair_publications", "resolve_thread", "unresolve_thread", "add_reaction", "remove_reaction", "edit_description", "edit_comment", "search_reviewers", "request_reviewer", "device_sign_in", "token_renewal", "sign_out", "account_change", "vault_store", "vault_load", "vault_delete", "refresh_store", "refresh_load", "refresh_delete", "update_check", "update_download", "update_verify", "update_refresh", "update_apply",
																  // [[BPR-1831]].
																  "storage_open", "storage_migrate", "storage_read", "storage_write", "storage_statement", "clone_scan", "clone_verify", "clone_load", "source_search", "image_fetch", "avatar_fetch", "notification_deliver", "notification_permission", "notification_activate",
																  // [[BPR-1830]].
																  "mcp_helper_request", "tour_observe_imports", "tour_import", "tour_open", "tour_source", "tour_counts", "tour_preview", "tour_publish", "cycle_round_freeze", "cycle_round_grant", "cycle_round_revoke", "cycle_round_read", "cycle_round_respond", "cycle_response_import", "cycle_round_export", "cycle_bundle_import", "cycle_round_send" };
		constexpr std::array<std::string_view, 137> failure_names{ "none", "cancelled", "unavailable", "agent_not_found", "program_refused", "no_program_chosen", "not_signed_in", "failed_to_start", "status_unreadable", "timed_out", "workspace_unavailable", "invalid_request", "stale", "cleanup_failed", "copy_failed", "download_failed", "connection_failed", "sign_in_refused", "too_large", "too_large_not_in_clone", "refused_by_server", "undesignated_target", "target_moved", "stale_head", "unsupported", "target_unavailable", "malformed_response", "incompatible_body", "credential_unavailable", "credential_tool_failed", "unplaceable_line", "refused_not_published", "settled_not_published", "off_hunk_refused", "not_confirmed", "conflict", "authorization_denied", "authorization_expired", "device_flow_disabled", "device_flow_unavailable", "network_unavailable", "certificate_rejected", "redirect_rejected", "response_too_large", "vault_unavailable", "refresh_absent", "refresh_rejected", "transport_failed", "storage_failed", "no_session", "locked", "access_denied", "corrupt", "operation_failed", "allocation_failure", "policy_refused", "access_required", "unauthenticated", "forbidden", "inaccessible", "offline", "rate_limited", "malformed_release", "invalid_version", "unstable_release", "no_compatible_asset", "ambiguous_asset", "invalid_digest", "size_limit", "unsafe_archive", "invalid_bundle", "wrong_architecture", "wrong_signer", "digest_mismatch", "changed_candidate", "busy", "stale_completion", "invalid_progress", "identity_exhausted", "installer_unavailable", "flush_failed", "quit_refused", "recovery_required", "shutdown",
																   // [[BPR-1831]].
																   "invalid_path", "invalid_permissions", "schema_mismatch", "integrity_failure", "quota_exceeded", "platform_unsupported", "io_error", "read_only", "constraint", "not_a_database", "cannot_open", "sql_error", "not_a_clone", "no_matching_remote", "scan_failed", "store_unreadable", "search_failed", "resource_limit", "fetch_failed", "blocked", "over_limit", "permission_refused", "unattributable_activation",
																   // [[BPR-1830]].
																   "no_helper", "not_pinned", "network_location", "already_running", "helper_failed", "invalid_input", "interaction_required", "authentication_unavailable", "transport_unavailable", "remote_incomplete", "outcome_unknown", "storage_unavailable", "local_failure", "not_offered", "draft_exists", "target_mismatch", "refused", "wrong_profile", "not_found", "operation_reused", "bounds", "object_unavailable", "quota", "preview_stale", "store_damaged", "not_granted", "revoked", "wrong_target", "not_authorized", "read_failed" };
		constexpr std::array<std::string_view, 10>  process_names{ "none", "success", "private_storage_failure", "failed_to_start", "workspace_unavailable", "nonzero_exit", "protocol_failure", "cancelled", "timeout", "stale_result" };

		static_assert(action_names.size() == static_cast<std::size_t>(operation_action::cycle_round_send) + 1U);
		static_assert(failure_names.size() == static_cast<std::size_t>(operation_failure::read_failed) + 1U);

		// [[BPR-1831]]. The store names, by each store file's fixed name.
		constexpr std::array<std::string_view, 22> store_names{ "unknown", "other", "agent_setup", "appearance_settings", "diagnostic_preferences", "workbench_settings", "review_queue", "review_ledger", "review_tours", "review_cycles", "review_overviews", "window_restoration", "remembered_connections", "device_bindings", "review_mutation_recovery", "epoch_catalog", "markdown_images", "insights", "profile_registry", "release_update", "update_draft_recovery", "clone_ledger" };
		static_assert(store_names.size() == static_cast<std::size_t>(operation_store::clone_ledger) + 1U);
		static_assert(process_names.size() == static_cast<std::size_t>(operation_process::stale_result) + 1U);

		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto named(const std::array<std::string_view, N>& aNames, const TEnum aValue) noexcept -> std::string_view
		{
			const auto index = static_cast<std::size_t>(aValue);
			return index < aNames.size() ? aNames[index] : std::string_view{ };
		}

		/// One `key=value` word, ending the text or followed by one space and more.
		[[nodiscard]] auto word(std::string_view& aText, const std::string_view aKey) -> std::optional<std::string_view>
		{
			if (!aText.starts_with(aKey) || aText.size() == aKey.size() || aText[aKey.size()] != '=') {
				return std::nullopt;
			}
			aText.remove_prefix(aKey.size() + 1U);
			const auto end   = aText.find(' ');
			const auto value = aText.substr(0U, end);
			aText.remove_prefix(end == std::string_view::npos ? aText.size() : end + 1U);
			if (value.empty() || (end != std::string_view::npos && aText.empty())) {
				return std::nullopt;
			}
			return value;
		}

		/// A decimal with no leading zero, and a minus sign only when signed.
		template <typename TNumber>
		[[nodiscard]] auto decimal(const std::string_view aDigits) -> std::optional<TNumber>
		{
			const auto unsigned_digits = aDigits.starts_with('-') ? aDigits.substr(1U) : aDigits;
			if (unsigned_digits.empty() || (unsigned_digits.size() > 1U && unsigned_digits.front() == '0') || (aDigits.starts_with('-') && (!std::numeric_limits<TNumber>::is_signed || unsigned_digits == "0"))) {
				return std::nullopt;
			}
			auto       value  = TNumber{ };
			const auto parsed = std::from_chars(aDigits.data(), aDigits.data() + aDigits.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != aDigits.data() + aDigits.size()) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto number(std::string_view& aText, const std::string_view aKey) -> std::optional<std::uint32_t>
		{
			const auto digits = word(aText, aKey);
			return digits ? decimal<std::uint32_t>(*digits) : std::nullopt;
		}

		[[nodiscard]] auto flag(std::string_view& aText, const std::string_view aKey) -> std::optional<bool>
		{
			const auto value = word(aText, aKey);
			if (!value || (*value != "0" && *value != "1")) {
				return std::nullopt;
			}
			return *value == "1";
		}

		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto enumerated(std::string_view& aText, const std::string_view aKey, const std::array<std::string_view, N>& aNames) -> std::optional<TEnum>
		{
			const auto value = word(aText, aKey);
			if (!value) {
				return std::nullopt;
			}
			const auto found = std::ranges::find(aNames, *value);
			return found == aNames.end() ? std::nullopt : std::optional{ static_cast<TEnum>(found - aNames.begin()) };
		}

		[[nodiscard]] auto within(const operation_failure aFailure, const operation_failure aFirst, const operation_failure aLast) noexcept -> bool
		{
			return aFailure >= aFirst && aFailure <= aLast;
		}

		/// Whether a failure is one the action's family can report.
		[[nodiscard]] auto belongs(const operation_diagnosis_family aFamily, const operation_failure aFailure) noexcept -> bool
		{
			if (aFailure == operation_failure::none || aFailure == operation_failure::cancelled) {
				return true;
			}
			switch (aFamily) {
			case operation_diagnosis_family::agent_setup:     return within(aFailure, operation_failure::agent_not_found, operation_failure::workspace_unavailable);
			case operation_diagnosis_family::agent_workspace: return within(aFailure, operation_failure::invalid_request, operation_failure::too_large_not_in_clone);
			case operation_diagnosis_family::review_mutation: return aFailure == operation_failure::unavailable || within(aFailure, operation_failure::refused_by_server, operation_failure::conflict);
			// [[BPR-1829]].
			case operation_diagnosis_family::sign_in: return aFailure == operation_failure::timed_out || aFailure == operation_failure::malformed_response || within(aFailure, operation_failure::authorization_denied, operation_failure::no_session);
			case operation_diagnosis_family::credential_vault:
				return aFailure == operation_failure::invalid_request || aFailure == operation_failure::unavailable || within(aFailure, operation_failure::locked, operation_failure::allocation_failure);
			case operation_diagnosis_family::updater:
				return aFailure == operation_failure::timed_out || aFailure == operation_failure::unavailable || aFailure == operation_failure::certificate_rejected || aFailure == operation_failure::redirect_rejected
					|| within(aFailure, operation_failure::policy_refused, operation_failure::shutdown);
			// [[BPR-1831]].
			case operation_diagnosis_family::storage:
				return aFailure == operation_failure::unavailable || aFailure == operation_failure::busy || aFailure == operation_failure::locked || within(aFailure, operation_failure::invalid_path, operation_failure::sql_error);
			case operation_diagnosis_family::clone_scan: return within(aFailure, operation_failure::not_a_clone, operation_failure::store_unreadable);
			case operation_diagnosis_family::source_search:
				return aFailure == operation_failure::unavailable || aFailure == operation_failure::identity_exhausted || within(aFailure, operation_failure::search_failed, operation_failure::resource_limit);
			case operation_diagnosis_family::image_fetch:
				return aFailure == operation_failure::too_large || aFailure == operation_failure::unsupported || within(aFailure, operation_failure::fetch_failed, operation_failure::over_limit);
			case operation_diagnosis_family::notification: return within(aFailure, operation_failure::permission_refused, operation_failure::unattributable_activation);
			// [[BPR-1830]].
			case operation_diagnosis_family::mcp_helper: return within(aFailure, operation_failure::no_helper, operation_failure::helper_failed) || aFailure == operation_failure::failed_to_start || aFailure == operation_failure::timed_out;
			case operation_diagnosis_family::tour:
				return within(aFailure, operation_failure::invalid_input, operation_failure::refused) || aFailure == operation_failure::busy || aFailure == operation_failure::unavailable || aFailure == operation_failure::conflict;
			case operation_diagnosis_family::cycle_round:
				return within(aFailure, operation_failure::wrong_profile, operation_failure::read_failed) || aFailure == operation_failure::corrupt || aFailure == operation_failure::schema_mismatch || aFailure == operation_failure::invalid_bundle
					|| aFailure == operation_failure::unavailable || aFailure == operation_failure::conflict || aFailure == operation_failure::refused || aFailure == operation_failure::malformed_response;
			}
			return false;
		}
	} // namespace

	auto family(const operation_action aAction) noexcept -> operation_diagnosis_family
	{
		if (aAction <= operation_action::agent_custom_check) {
			return operation_diagnosis_family::agent_setup;
		}
		if (aAction <= operation_action::workspace_cleanup) {
			return operation_diagnosis_family::agent_workspace;
		}
		if (aAction <= operation_action::request_reviewer) {
			return operation_diagnosis_family::review_mutation;
		}
		// [[BPR-1829]].
		if (aAction >= operation_action::update_check && aAction <= operation_action::update_apply) {
			return operation_diagnosis_family::updater;
		}
		if (aAction >= operation_action::vault_store && aAction <= operation_action::refresh_delete) {
			return operation_diagnosis_family::credential_vault;
		}
		if (aAction >= operation_action::device_sign_in && aAction <= operation_action::account_change) {
			return operation_diagnosis_family::sign_in;
		}
		// [[BPR-1831]].
		if (aAction <= operation_action::storage_statement) {
			return operation_diagnosis_family::storage;
		}
		if (aAction <= operation_action::clone_load) {
			return operation_diagnosis_family::clone_scan;
		}
		if (aAction == operation_action::source_search) {
			return operation_diagnosis_family::source_search;
		}
		if (aAction <= operation_action::avatar_fetch) {
			return operation_diagnosis_family::image_fetch;
		}
		if (aAction <= operation_action::notification_activate) {
			return operation_diagnosis_family::notification;
		}
		// [[BPR-1830]].
		if (aAction == operation_action::mcp_helper_request) {
			return operation_diagnosis_family::mcp_helper;
		}
		if (aAction <= operation_action::tour_publish) {
			return operation_diagnosis_family::tour;
		}
		return operation_diagnosis_family::cycle_round;
	}

	auto name(const operation_action aAction) noexcept -> std::string_view
	{
		return named(action_names, aAction);
	}

	auto name(const operation_failure aFailure) noexcept -> std::string_view
	{
		return named(failure_names, aFailure);
	}

	auto name(const operation_process aProcess) noexcept -> std::string_view
	{
		return named(process_names, aProcess);
	}

	auto name(const operation_store aStore) noexcept -> std::string_view
	{
		return named(store_names, aStore);
	}

	auto valid(const operation_diagnosis& aDiagnosis) noexcept -> bool
	{
		if (name(aDiagnosis.Action).empty() || name(aDiagnosis.Failure).empty() || name(aDiagnosis.Process).empty()) {
			return false;
		}
		const auto kind = family(aDiagnosis.Action);
		if (!belongs(kind, aDiagnosis.Failure) || (aDiagnosis.ExitCode && aDiagnosis.Process == operation_process::none)) {
			return false;
		}
		if (kind != operation_diagnosis_family::agent_setup && kind != operation_diagnosis_family::mcp_helper && aDiagnosis.Process != operation_process::none) {
			return false;
		}
		// [[BPR-1831]]. A store exactly on a storage line, a schema version
		// only there.
		const auto storage = kind == operation_diagnosis_family::storage;
		if (storage != aDiagnosis.Store.has_value() || (aDiagnosis.Store && name(*aDiagnosis.Store).empty()) || (!storage && aDiagnosis.Schema != 0U)) {
			return false;
		}
		return kind == operation_diagnosis_family::review_mutation || (!aDiagnosis.Written && !aDiagnosis.ReadBack);
	}

	auto describe(const operation_diagnosis& aDiagnosis) -> std::string
	{
		if (!valid(aDiagnosis)) {
			return { };
		}
		auto text = std::string{ "action=" };
		text += name(aDiagnosis.Action);
		text += " failure=";
		text += name(aDiagnosis.Failure);
		text += " process=";
		text += name(aDiagnosis.Process);
		text += " exit=";
		text += aDiagnosis.ExitCode ? std::to_string(*aDiagnosis.ExitCode) : std::string{ "none" };
		text += " duration_ms=" + std::to_string(aDiagnosis.DurationMilliseconds);
		text += " count=" + std::to_string(aDiagnosis.Count);
		text += aDiagnosis.Written ? " written=1" : " written=0";
		text += aDiagnosis.ReadBack ? " read_back=1" : " read_back=0";
		if (aDiagnosis.Store) {
			text += " store=";
			text += name(*aDiagnosis.Store);
			text += " schema=" + std::to_string(aDiagnosis.Schema);
		}
		return text;
	}

	auto parse_operation_diagnosis(std::string_view aText) -> std::optional<operation_diagnosis>
	{
		const auto action  = enumerated<operation_action>(aText, "action", action_names);
		const auto failure = action ? enumerated<operation_failure>(aText, "failure", failure_names) : std::nullopt;
		const auto process = failure ? enumerated<operation_process>(aText, "process", process_names) : std::nullopt;
		const auto exit    = process ? word(aText, "exit") : std::nullopt;
		if (!exit) {
			return std::nullopt;
		}
		auto result = operation_diagnosis{ .Action = *action, .Failure = *failure, .Process = *process };
		if (*exit != "none") {
			result.ExitCode = decimal<int>(*exit);
			if (!result.ExitCode) {
				return std::nullopt;
			}
		}
		const auto duration = number(aText, "duration_ms");
		const auto count    = duration ? number(aText, "count") : std::nullopt;
		const auto written  = count ? flag(aText, "written") : std::nullopt;
		const auto readBack = written ? flag(aText, "read_back") : std::nullopt;
		if (!readBack) {
			return std::nullopt;
		}
		// [[BPR-1831]]. A storage line's store and schema version.
		if (!aText.empty()) {
			result.Store      = enumerated<operation_store>(aText, "store", store_names);
			const auto schema = result.Store ? number(aText, "schema") : std::nullopt;
			if (!schema || !aText.empty()) {
				return std::nullopt;
			}
			result.Schema = *schema;
		}
		result.DurationMilliseconds = *duration;
		result.Count                = *count;
		result.Written              = *written;
		result.ReadBack             = *readBack;
		if (!valid(result)) {
			return std::nullopt;
		}
		return result;
	}
} // namespace bpr::core
