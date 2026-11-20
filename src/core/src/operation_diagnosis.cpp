#include "bpr/core/operation_diagnosis.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <system_error>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::array<std::string_view, 84>  action_names{ "agent_status_check", "agent_program_check", "agent_custom_check", "workspace_materialize", "workspace_cleanup", "observe_target", "publish_comment", "reconcile_publication", "repair_publications", "resolve_thread", "unresolve_thread", "add_reaction", "remove_reaction", "edit_description", "edit_comment", "search_reviewers", "request_reviewer", "device_sign_in", "token_renewal", "sign_out", "account_change", "vault_store", "vault_load", "vault_delete", "refresh_store", "refresh_load", "refresh_delete", "update_check", "update_download", "update_verify", "update_refresh", "update_apply",
																  // [[BPR-1831]].
																  "storage_open", "storage_migrate", "storage_read", "storage_write", "storage_statement", "clone_scan", "clone_verify", "clone_load", "source_search", "image_fetch", "avatar_fetch", "notification_deliver", "notification_permission", "notification_activate",
																  // [[BPR-1830]].
																  "mcp_helper_request", "tour_observe_imports", "tour_import", "tour_open", "tour_source", "tour_counts", "tour_preview", "tour_publish", "cycle_round_freeze", "cycle_round_grant", "cycle_round_revoke", "cycle_round_read", "cycle_round_respond", "cycle_response_import", "cycle_round_export", "cycle_bundle_import", "cycle_round_send",
																  // [[BPR-1850]].
																  "activity_poll_changed", "activity_poll_unchanged", "activity_poll", "activity_post", "activity_suppress", "activity_activate",
																  // [[BPR-1867]].
																  "activity_refresh",
																  // [[BPR-1879]].
																  "clone_keep",
																  // [[BPR-1884]].
																  "clone_refresh", "native_continuation",
																  // [[RFC-BPR-0105]].
																  "review_submit_request", "review_submit_wait", "review_submit_send", "review_submit_drop", "review_submit_end",
																  // [[BPR-2261]].
																  "workspace_verify",
																  // [[BPR-2263]].
																  "review_submit_repair", "side_chat_ask", "side_chat_propose", "side_chat_apply", "finding_draft_cleared" };
		constexpr std::array<std::string_view, 140> failure_names{ "none", "cancelled", "unavailable", "agent_not_found", "program_refused", "no_program_chosen", "not_signed_in", "failed_to_start", "status_unreadable", "timed_out", "workspace_unavailable", "invalid_request", "stale", "cleanup_failed", "copy_failed", "download_failed", "connection_failed", "sign_in_refused", "too_large", "clone_needed", "refused_by_server", "undesignated_target", "target_moved", "stale_head", "unsupported", "target_unavailable", "malformed_response", "incompatible_body", "credential_unavailable", "credential_tool_failed", "unplaceable_line", "refused_not_published", "settled_not_published", "off_hunk_refused", "not_confirmed", "conflict", "authorization_denied", "authorization_expired", "device_flow_disabled", "device_flow_unavailable", "network_unavailable", "certificate_rejected", "redirect_rejected", "response_too_large", "vault_unavailable", "refresh_absent", "refresh_rejected", "transport_failed", "storage_failed", "no_session", "locked", "access_denied", "corrupt", "operation_failed", "allocation_failure", "policy_refused", "access_required", "unauthenticated", "forbidden", "inaccessible", "offline", "rate_limited", "malformed_release", "invalid_version", "unstable_release", "no_compatible_asset", "ambiguous_asset", "invalid_digest", "size_limit", "unsafe_archive", "invalid_bundle", "wrong_architecture", "wrong_signer", "digest_mismatch", "changed_candidate", "busy", "stale_completion", "invalid_progress", "identity_exhausted", "installer_unavailable", "flush_failed", "quit_refused", "recovery_required", "shutdown",
																   // [[BPR-1831]].
																   "invalid_path", "invalid_permissions", "schema_mismatch", "integrity_failure", "quota_exceeded", "platform_unsupported", "io_error", "read_only", "constraint", "not_a_database", "cannot_open", "sql_error", "not_a_clone", "no_matching_remote", "scan_failed", "store_unreadable", "search_failed", "resource_limit", "fetch_failed", "blocked", "over_limit", "permission_refused", "unattributable_activation",
																   // [[BPR-1830]].
																   "no_helper", "not_pinned", "network_location", "already_running", "helper_failed", "invalid_input", "interaction_required", "authentication_unavailable", "transport_unavailable", "remote_incomplete", "outcome_unknown", "storage_unavailable", "local_failure", "not_offered", "draft_exists", "target_mismatch", "refused", "wrong_profile", "not_found", "operation_reused", "bounds", "object_unavailable", "quota", "preview_stale", "store_damaged", "not_granted", "revoked", "wrong_target", "not_authorized", "read_failed",
																   // [[BPR-1879]].
																   "keep_failed",
																   // [[BPR-1884]].
																   "refresh_failed",
																   // [[BPR-2295]].
																   "history_missing" };
		constexpr std::array<std::string_view, 10>  process_names{ "none", "success", "private_storage_failure", "failed_to_start", "workspace_unavailable", "nonzero_exit", "protocol_failure", "cancelled", "timeout", "stale_result" };

		constexpr auto tour_stage_names = std::array<std::string_view, 36>{ "preview", "admission", "discovery", "recovery", "publish", "factory", "identity_check", "request_preparation", "baseline_read", "journal", "verification", "terminal", "stop", "list_reviews", "read_review", "list_comments", "create_pending", "update_comment", "update_summary", "submit_comment", "discard_pending", "update_description", "list_issue_comments", "create_issue_comment", "update_issue_comment", "update_pending_comment", "plan_validation", "recovery_open", "lease", "configuration", "binding", "journal_read", "credential_scope", "journal_save", "create_file_comment", "list_file_comments" };
		constexpr auto tour_state_names = std::array<std::string_view, 9>{ "none", "unavailable", "ready", "staged", "outcome_unknown", "conflict", "published", "description_attention", "partial" };
		static_assert(action_names.size() == static_cast<std::size_t>(operation_action::finding_draft_cleared) + 1U);
		constexpr auto submission_note_names = std::array<std::string_view, 12>{ "none", "pending_comments", "review_lane", "superseded", "retired", "published", "not_published", "outcome_unknown", "wait_expired", "repairing", "repaired", "repair_failed" };
		static_assert(submission_note_names.size() == static_cast<std::size_t>(review_submission_note::repair_failed) + 1U);
		static_assert(failure_names.size() == static_cast<std::size_t>(operation_failure::history_missing) + 1U);
		// [[BPR-2295]].
		constexpr auto workspace_source_names = std::array<std::string_view, 7>{ "none", "pull_request_files", "borrowed", "head_from_clone", "delta", "cache_copy", "cache_copy_shallow" };
		static_assert(workspace_source_names.size() == static_cast<std::size_t>(workspace_source::cache_copy_shallow) + 1U);
		constexpr auto workspace_command_names = std::array<std::string_view, 13>{ "none", "init", "rev_parse", "ls_tree", "pack_objects", "rev_list", "hash_object", "index_pack", "cat_file", "read_tree", "checkout_index", "config", "other" };
		static_assert(workspace_command_names.size() == static_cast<std::size_t>(workspace_git_command::other) + 1U);
		constexpr auto workspace_error_names = std::array<std::string_view, 5>{ "none", "could_not_read", "failed_to_traverse_parents", "missing_object", "other" };
		static_assert(workspace_error_names.size() == static_cast<std::size_t>(workspace_git_error::other) + 1U);
		// [[BPR-2321]].
		constexpr auto workspace_seed_names = std::array<std::string_view, 5>{ "none", "seeded", "too_spread", "refused", "failed" };
		static_assert(workspace_seed_names.size() == static_cast<std::size_t>(workspace_seed_outcome::failed) + 1U);

		// [[BPR-1831]]. The store names, by each store file's fixed name.
		constexpr std::array<std::string_view, 24> store_names{ "unknown", "other", "agent_setup", "appearance_settings", "diagnostic_preferences", "workbench_settings", "review_queue", "review_ledger", "review_tours", "review_cycles", "review_overviews", "window_restoration", "remembered_connections", "device_bindings", "review_mutation_recovery", "epoch_catalog", "markdown_images", "insights", "profile_registry", "release_update", "update_draft_recovery", "clone_ledger", "view_filters", "pull_request_activity" };
		static_assert(store_names.size() == static_cast<std::size_t>(operation_store::pull_request_activity) + 1U);
		static_assert(process_names.size() == static_cast<std::size_t>(operation_process::stale_result) + 1U);
		// [[BPR-2322]]. A side chat line's item, apply action and refusal, in
		// the line's own vocabulary.
		constexpr auto side_chat_item_names = std::array<std::string_view, 3>{ "suggested_comment", "draft", "thread" };
		static_assert(side_chat_item_names.size() == follow_up_subject_kind_count);
		constexpr auto side_chat_apply_names = std::array<std::string_view, 3>{ "update_suggestion", "update_draft", "draft_reply" };
		static_assert(side_chat_apply_names.size() == follow_up_apply_count);
		constexpr auto side_chat_refusal_names = std::array<std::string_view, 12>{ "none", "busy", "empty_question", "not_offered", "too_many_rounds", "stale", "no_proposal", "empty_answer", "unchanged", "too_long", "target_changed", "failed" };
		static_assert(side_chat_refusal_names.size() == follow_up_refusal_count);

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
			const auto unsignedDigits = aDigits.starts_with('-') ? aDigits.substr(1U) : aDigits;
			if (unsignedDigits.empty() || (unsignedDigits.size() > 1U && unsignedDigits.front() == '0') || (aDigits.starts_with('-') && (!std::numeric_limits<TNumber>::is_signed || unsignedDigits == "0"))) {
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
			// [[BPR-2254]]. And a check refused because the saved agents could
			// not be read.
			case operation_diagnosis_family::agent_setup:     return within(aFailure, operation_failure::agent_not_found, operation_failure::workspace_unavailable) || aFailure == operation_failure::store_unreadable;
			case operation_diagnosis_family::agent_workspace: return within(aFailure, operation_failure::invalid_request, operation_failure::clone_needed)
				|| aFailure == operation_failure::history_missing || aFailure == operation_failure::object_unavailable || aFailure == operation_failure::size_limit || aFailure == operation_failure::timed_out
				|| aFailure == operation_failure::storage_failed || aFailure == operation_failure::unavailable || aFailure == operation_failure::unsupported
				|| aFailure == operation_failure::outcome_unknown || aFailure == operation_failure::changed_candidate;
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
			case operation_diagnosis_family::clone_scan: return within(aFailure, operation_failure::not_a_clone, operation_failure::store_unreadable) || aFailure == operation_failure::keep_failed || aFailure == operation_failure::refresh_failed;
			case operation_diagnosis_family::source_search:
				return aFailure == operation_failure::unavailable || aFailure == operation_failure::identity_exhausted || within(aFailure, operation_failure::search_failed, operation_failure::resource_limit);
			case operation_diagnosis_family::image_fetch:
				return aFailure == operation_failure::too_large || aFailure == operation_failure::unsupported || within(aFailure, operation_failure::fetch_failed, operation_failure::over_limit)
					// [[BPR-1846]]. The GitHub CLI avatar helper: missing or not
					// launched, gh unusable or signed out, or an unreadable answer.
					|| aFailure == operation_failure::no_helper || aFailure == operation_failure::credential_unavailable
					|| aFailure == operation_failure::credential_tool_failed || aFailure == operation_failure::malformed_response;
			// [[BPR-1850]]. And an activity poll's unsupported sign-in, rate
			// limit or failure, or a platform without a surface.
			case operation_diagnosis_family::notification:
				return within(aFailure, operation_failure::permission_refused, operation_failure::unattributable_activation) || aFailure == operation_failure::unsupported
					|| aFailure == operation_failure::rate_limited || aFailure == operation_failure::unavailable;
			// [[BPR-1830]].
			case operation_diagnosis_family::mcp_helper: return within(aFailure, operation_failure::no_helper, operation_failure::helper_failed) || aFailure == operation_failure::failed_to_start || aFailure == operation_failure::timed_out;
			case operation_diagnosis_family::tour:
				return within(aFailure, operation_failure::invalid_input, operation_failure::refused) || aFailure == operation_failure::busy || aFailure == operation_failure::unavailable || aFailure == operation_failure::conflict || within(aFailure, operation_failure::refused_by_server, operation_failure::not_confirmed) || aFailure == operation_failure::preview_stale;
			// [[BPR-2322]]. A core refusal, a run that failed or timed out, or
			// no agent to ask.
			case operation_diagnosis_family::side_chat:
				return aFailure == operation_failure::refused || aFailure == operation_failure::operation_failed || aFailure == operation_failure::timed_out || aFailure == operation_failure::unavailable;
			// [[BPR-2325]]. Nothing it does can fail.
			case operation_diagnosis_family::finding_card: return false;
			case operation_diagnosis_family::cycle_round:
				return within(aFailure, operation_failure::wrong_profile, operation_failure::read_failed) || aFailure == operation_failure::corrupt || aFailure == operation_failure::schema_mismatch || aFailure == operation_failure::invalid_bundle
					|| aFailure == operation_failure::unavailable || aFailure == operation_failure::conflict || aFailure == operation_failure::refused || aFailure == operation_failure::malformed_response;
			}
			return false;
		}
	} // namespace

	auto character_count(const std::string_view aText) noexcept -> std::uint32_t
	{
		auto count = std::uint64_t{ 0U };
		for (const auto byte : aText) {
			// Every byte but a continuation byte starts a code point.
			count += (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U ? 1U : 0U;
		}
		return static_cast<std::uint32_t>(std::min<std::uint64_t>(count, std::numeric_limits<std::uint32_t>::max()));
	}

	auto family(const operation_action aAction) noexcept -> operation_diagnosis_family
	{
		// [[BPR-2325]]. Appended last.
		if (aAction == operation_action::finding_draft_cleared) {
			return operation_diagnosis_family::finding_card;
		}
		// [[BPR-2322]].
		if (aAction >= operation_action::side_chat_ask) {
			return operation_diagnosis_family::side_chat;
		}
		// [[BPR-1879]]. Appended after the activity lines, but a clone line.
		if (aAction == operation_action::clone_keep || aAction == operation_action::clone_refresh) {
			return operation_diagnosis_family::clone_scan;
		}
		if (aAction == operation_action::native_continuation) {
			return operation_diagnosis_family::agent_workspace;
		}
		// [[BPR-2261]]. Appended, but an agent workspace line.
		if (aAction == operation_action::workspace_verify) {
			return operation_diagnosis_family::agent_workspace;
		}
		// [[RFC-BPR-0105]]. Appended, but a review mutation line.
		if (aAction >= operation_action::review_submit_request) {
			return operation_diagnosis_family::review_mutation;
		}
		// [[BPR-1850]]. Activity notifications are notification lines.
		if (aAction >= operation_action::activity_poll_changed) {
			return operation_diagnosis_family::notification;
		}
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

	auto name(const review_submission_note aNote) noexcept -> std::string_view
	{
		return named(submission_note_names, aNote);
	}

	auto name(const workspace_source aSource) noexcept -> std::string_view
	{
		return named(workspace_source_names, aSource);
	}

	auto name(const workspace_git_command aCommand) noexcept -> std::string_view
	{
		return named(workspace_command_names, aCommand);
	}

	auto name(const workspace_git_error aError) noexcept -> std::string_view
	{
		return named(workspace_error_names, aError);
	}

	auto name(const workspace_seed_outcome aOutcome) noexcept -> std::string_view
	{
		return named(workspace_seed_names, aOutcome);
	}

	auto classify_workspace_git_error(const std::string_view aStandardError) noexcept -> workspace_git_error
	{
		if (aStandardError.find_first_not_of(" \t\r\n") == std::string_view::npos) {
			return workspace_git_error::none;
		}
		// The fatal line names the walk that failed; the error line before it
		// names the object it could not read.
		if (aStandardError.find("Failed to traverse parents") != std::string_view::npos) {
			return workspace_git_error::failed_to_traverse_parents;
		}
		if (aStandardError.find("Could not read") != std::string_view::npos) {
			return workspace_git_error::could_not_read;
		}
		if (aStandardError.find("missing") != std::string_view::npos || aStandardError.find("unable to read") != std::string_view::npos
			|| aStandardError.find("bad object") != std::string_view::npos) {
			return workspace_git_error::missing_object;
		}
		return workspace_git_error::other;
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
		if (aDiagnosis.Tour) {
			const auto& trace  = *aDiagnosis.Tour;
			const auto  digest = trace.Attempt.size() == 64U && std::ranges::all_of(trace.Attempt, [](char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
			if ((aDiagnosis.Action != operation_action::tour_preview && aDiagnosis.Action != operation_action::tour_publish) || named(tour_stage_names, trace.Stage).empty() || named(tour_state_names, trace.State).empty()
				|| (!digest && trace.Attempt != "none") || (trace.HttpStatus != 0U && (trace.HttpStatus < 100U || trace.HttpStatus > 599U))) {
				return false;
			}
		}
		// [[RFC-BPR-0105]]. A subject exactly on a review submission line.
		const auto submission = (aDiagnosis.Action >= operation_action::review_submit_request && aDiagnosis.Action <= operation_action::review_submit_end) || aDiagnosis.Action == operation_action::review_submit_repair;
		if (submission != aDiagnosis.Submission.has_value() || (aDiagnosis.Submission && name(*aDiagnosis.Submission).empty())) {
			return false;
		}
		// [[BPR-2263]]. A refusal only on an end or repair line, and valid.
		if (aDiagnosis.Refusal && ((aDiagnosis.Action != operation_action::review_submit_end && aDiagnosis.Action != operation_action::review_submit_repair) || !valid(*aDiagnosis.Refusal))) {
			return false;
		}
		// [[BPR-2295]]. A workspace trace only on a materialization line.
		if (aDiagnosis.Workspace
			&& (aDiagnosis.Action != operation_action::workspace_materialize || name(aDiagnosis.Workspace->Source).empty() || name(aDiagnosis.Workspace->Command).empty()
				|| name(aDiagnosis.Workspace->GitError).empty() || name(aDiagnosis.Workspace->Seed.Outcome).empty()
				|| (aDiagnosis.Workspace->Seed.Outcome == workspace_seed_outcome::none && aDiagnosis.Workspace->Seed != workspace_seed_trace{ }))) {
			return false;
		}
		// [[BPR-2322]]. A side chat trace exactly on a side chat line: an
		// apply action exactly on a propose or apply line, and a refusal
		// exactly when the line failed.
		const auto sideChat = kind == operation_diagnosis_family::side_chat;
		if (sideChat != aDiagnosis.SideChat.has_value()) {
			return false;
		}
		if (aDiagnosis.SideChat) {
			const auto& trace = *aDiagnosis.SideChat;
			if (named(side_chat_item_names, trace.Item).empty() || named(side_chat_refusal_names, trace.Refusal).empty() || (trace.Apply && named(side_chat_apply_names, *trace.Apply).empty())
				|| trace.Apply.has_value() == (aDiagnosis.Action == operation_action::side_chat_ask)
				|| (trace.Refusal == follow_up_refusal::none) != (aDiagnosis.Failure == operation_failure::none) || aDiagnosis.Count != 0U) {
				return false;
			}
		}
		return kind == operation_diagnosis_family::review_mutation || aDiagnosis.Tour || (!aDiagnosis.Written && !aDiagnosis.ReadBack);
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
		if (aDiagnosis.Tour) {
			text += " tour_stage=";
			text += named(tour_stage_names, aDiagnosis.Tour->Stage);
			text += " tour_state=";
			text += named(tour_state_names, aDiagnosis.Tour->State);
			text += " tour_attempt=" + aDiagnosis.Tour->Attempt;
			text += " tour_http=" + std::to_string(aDiagnosis.Tour->HttpStatus);
		}
		if (aDiagnosis.Submission) {
			text += " submit_subject=";
			text += name(*aDiagnosis.Submission);
		}
		if (aDiagnosis.Refusal) {
			text += " submit_check=";
			text += name(aDiagnosis.Refusal->Check);
			text += " submit_reason=";
			text += name(aDiagnosis.Refusal->Reason);
			text += " submit_part=";
			text += name(aDiagnosis.Refusal->Part);
			text += " submit_comment=" + std::to_string(aDiagnosis.Refusal->Comment);
		}
		if (aDiagnosis.Workspace) {
			const auto& trace = *aDiagnosis.Workspace;
			text += " source=";
			text += name(trace.Source);
			text += trace.Sparse ? " sparse=1" : " sparse=0";
			text += trace.CloneGiven ? " clone=1" : " clone=0";
			text += trace.CloneHeldHead ? " clone_head=1" : " clone_head=0";
			text += " git=";
			text += name(trace.Command);
			text += " git_error=";
			text += name(trace.GitError);
			// [[BPR-2321]]. The seed's steps, only when one was tried.
			if (const auto& seed = trace.Seed; seed.Outcome != workspace_seed_outcome::none) {
				text += " seed=";
				text += name(seed.Outcome);
				text += " seed_walk_ms=" + std::to_string(seed.WalkMilliseconds);
				text += " seed_clone_ms=" + std::to_string(seed.CloneMilliseconds);
				text += " seed_refresh_ms=" + std::to_string(seed.RefreshMilliseconds);
				text += " seed_checkout_ms=" + std::to_string(seed.CheckoutMilliseconds);
				text += " seed_clean_ms=" + std::to_string(seed.CleanMilliseconds);
				text += " seed_folders=" + std::to_string(seed.FoldersCloned);
				text += " seed_files=" + std::to_string(seed.FilesCloned);
				text += " seed_written=" + std::to_string(seed.FilesWritten);
				text += " seed_cleaned=" + std::to_string(seed.Cleaned);
			}
		}
		if (aDiagnosis.SideChat) {
			const auto& trace = *aDiagnosis.SideChat;
			text += " item=";
			text += named(side_chat_item_names, trace.Item);
			text += " round=" + std::to_string(trace.Round);
			text += " apply=";
			text += trace.Apply ? named(side_chat_apply_names, *trace.Apply) : std::string_view{ "none" };
			text += " refusal=";
			text += named(side_chat_refusal_names, trace.Refusal);
			text += " question_chars=" + std::to_string(trace.QuestionChars);
			text += " answer_chars=" + std::to_string(trace.AnswerChars);
			text += " before_chars=" + std::to_string(trace.BeforeChars);
			text += " after_chars=" + std::to_string(trace.AfterChars);
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
		if (aText.starts_with("submit_subject=")) {
			result.Submission = enumerated<review_submission_note>(aText, "submit_subject", submission_note_names);
			if (!result.Submission) {
				return std::nullopt;
			}
			// [[BPR-2263]]. The refusal, when the line carries one.
			if (!aText.empty()) {
				const auto check   = word(aText, "submit_check");
				const auto reason  = check ? word(aText, "submit_reason") : std::nullopt;
				const auto part    = reason ? word(aText, "submit_part") : std::nullopt;
				const auto comment = part ? number(aText, "submit_comment") : std::nullopt;
				const auto checked = check ? review_submission_refusal_check_named(*check) : std::nullopt;
				const auto why     = reason ? review_submission_refusal_reason_named(*reason) : std::nullopt;
				const auto which   = part ? review_submission_refusal_part_named(*part) : std::nullopt;
				if (!comment || !checked || !why || !which || *comment > maximum_review_submission_comments || !aText.empty()) {
					return std::nullopt;
				}
				result.Refusal = review_submission_refusal{ .Check = *checked, .Reason = *why, .Part = *which, .Comment = static_cast<std::uint16_t>(*comment) };
			}
		} else if (aText.starts_with("tour_stage=")) {
			const auto stage   = enumerated<tour_publication_stage>(aText, "tour_stage", tour_stage_names);
			const auto state   = stage ? enumerated<tour_publication_trace_state>(aText, "tour_state", tour_state_names) : std::nullopt;
			const auto attempt = state ? word(aText, "tour_attempt") : std::nullopt;
			const auto http    = attempt ? number(aText, "tour_http") : std::nullopt;
			if (!http || *http > 599U || !aText.empty()) {
				return std::nullopt;
			}
			result.Tour = tour_publication_trace{ *stage, *state, std::string{ *attempt }, static_cast<std::uint16_t>(*http) };
		} else if (aText.starts_with("source=")) {
			// [[BPR-2295]]. A workspace materialization's trace.
			const auto source    = enumerated<workspace_source>(aText, "source", workspace_source_names);
			const auto sparse    = source ? flag(aText, "sparse") : std::nullopt;
			const auto clone     = sparse ? flag(aText, "clone") : std::nullopt;
			const auto cloneHead = clone ? flag(aText, "clone_head") : std::nullopt;
			const auto command   = cloneHead ? enumerated<workspace_git_command>(aText, "git", workspace_command_names) : std::nullopt;
			const auto error     = command ? enumerated<workspace_git_error>(aText, "git_error", workspace_error_names) : std::nullopt;
			if (!error) {
				return std::nullopt;
			}
			result.Workspace = workspace_trace{ .Source = *source, .Sparse = *sparse, .CloneGiven = *clone, .CloneHeldHead = *cloneHead, .Command = *command, .GitError = *error };
			// [[BPR-2321]]. The seed's steps, when the line carries them.
			if (!aText.empty()) {
				const auto outcome = enumerated<workspace_seed_outcome>(aText, "seed", workspace_seed_names);
				auto&      seed    = result.Workspace->Seed;
				auto       read    = outcome.has_value() && *outcome != workspace_seed_outcome::none;
				for (const auto& [key, field] : std::array<std::pair<std::string_view, std::uint32_t*>, 9>{ { { "seed_walk_ms", &seed.WalkMilliseconds },
																											  { "seed_clone_ms", &seed.CloneMilliseconds },
																											  { "seed_refresh_ms", &seed.RefreshMilliseconds },
																											  { "seed_checkout_ms", &seed.CheckoutMilliseconds },
																											  { "seed_clean_ms", &seed.CleanMilliseconds },
																											  { "seed_folders", &seed.FoldersCloned },
																											  { "seed_files", &seed.FilesCloned },
																											  { "seed_written", &seed.FilesWritten },
																											  { "seed_cleaned", &seed.Cleaned } } }) {
					const auto value = read ? number(aText, key) : std::nullopt;
					read             = value.has_value();
					if (read) {
						*field = *value;
					}
				}
				if (!read || !aText.empty()) {
					return std::nullopt;
				}
				seed.Outcome = *outcome;
			}
		} else if (aText.starts_with("item=")) {
			// [[BPR-2322]]. A side chat line's trace.
			const auto item  = enumerated<follow_up_subject_kind>(aText, "item", side_chat_item_names);
			const auto round = item ? number(aText, "round") : std::nullopt;
			auto       apply = std::optional<follow_up_apply>{ };
			auto       read  = round.has_value();
			if (read && aText.starts_with("apply=none")) {
				read = word(aText, "apply").has_value();
			} else if (read) {
				apply = enumerated<follow_up_apply>(aText, "apply", side_chat_apply_names);
				read  = apply.has_value();
			}
			const auto refusal  = read ? enumerated<follow_up_refusal>(aText, "refusal", side_chat_refusal_names) : std::nullopt;
			const auto question = refusal ? number(aText, "question_chars") : std::nullopt;
			const auto answer   = question ? number(aText, "answer_chars") : std::nullopt;
			const auto before   = answer ? number(aText, "before_chars") : std::nullopt;
			const auto after    = before ? number(aText, "after_chars") : std::nullopt;
			if (!after || !aText.empty()) {
				return std::nullopt;
			}
			result.SideChat = side_chat_trace{ .Item = *item, .Round = *round, .Apply = apply, .Refusal = *refusal, .QuestionChars = *question, .AnswerChars = *answer, .BeforeChars = *before, .AfterChars = *after };
		} else if (!aText.empty()) {
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
