#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_OPERATION_DIAGNOSIS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_OPERATION_DIAGNOSIS_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1828]]. Which family of host operations a diagnostic
	/// line belongs to. Each family is one diagnostic operation in the log.
	enum class operation_diagnosis_family : std::uint8_t {
		/// Agent setup's Check it works (`agent.setup_check`).
		agent_setup,
		/// A disposable agent workspace made or removed (`agent.workspace`).
		agent_workspace,
		/// A write to, or read for a write to, the pull request on GitHub
		/// (`github.review_mutation`).
		review_mutation,
		/// [[BPR-1829]]. Device sign-in, token renewal, Sign out and the
		/// selected account changing (`github.sign_in`).
		sign_in,
		/// [[BPR-1829]]. A credential vault slot stored, loaded or deleted
		/// (`credential.vault`).
		credential_vault,
		/// [[BPR-1829]]. An updater check, download, verification or
		/// installation handoff (`updater.release`).
		updater,
		/// [[BPR-1831]]. A SQLite store opened, migrated, read or written
		/// (`storage.database`).
		storage,
		/// [[BPR-1831]]. The linked clone scan and its checks
		/// (`workspace.clone_scan`).
		clone_scan,
		/// [[BPR-1831]]. One source search execution (`search.source`).
		source_search,
		/// [[BPR-1831]]. A body image or avatar fetch (`image.fetch`).
		image_fetch,
		/// [[BPR-1831]]. A queue notification handed to the platform
		/// (`notification.delivery`).
		notification,
		/// [[BPR-1830]]. The read-only MCP context helper run for one request
		/// (`agent.mcp_helper`).
		mcp_helper,
		/// [[BPR-1830]]. A review tour imported, or read from or published to
		/// the pull request by `bpr tour` (`tour.operation`).
		tour,
		/// [[BPR-1830]]. A Review Cycle round frozen, granted, read, answered,
		/// exported, imported or sent (`cycle.round`).
		cycle_round,
	};

	/// \brief [[BPR-1828]]. What one operation line is about.
	enum class operation_action : std::uint8_t {
		/// A preset agent's sign-in status run, and its `--version` run.
		agent_status_check,
		/// The plain-text harness's check that its program can run.
		agent_program_check,
		/// A custom agent's own check command, or its launchability.
		agent_custom_check,
		/// A disposable workspace materialized for one agent invocation.
		workspace_materialize,
		/// That workspace removed again.
		workspace_cleanup,
		/// The pull request read again before a write.
		observe_target,
		/// One comment publication and its reconciliation.
		publish_comment,
		/// A read-only repair of an earlier publication attempt.
		reconcile_publication,
		/// One read-only pass over retained publication joins.
		repair_publications,
		resolve_thread,
		unresolve_thread,
		add_reaction,
		remove_reaction,
		edit_description,
		edit_comment,
		search_reviewers,
		request_reviewer,
		/// [[BPR-1829]]. One device sign-in flow, from its device-code request
		/// to its terminal stage.
		device_sign_in,
		/// [[BPR-1829]]. One renewal of a device grant's access token.
		token_renewal,
		/// [[BPR-1829]]. Sign out of the selected account.
		sign_out,
		/// [[BPR-1829]]. The selected account replaced, gained or lost.
		account_change,
		/// [[BPR-1829]]. The account credential's vault slot.
		vault_store,
		vault_load,
		vault_delete,
		/// [[BPR-1829]]. The refresh material's vault slot beside it.
		refresh_store,
		refresh_load,
		refresh_delete,
		/// [[BPR-1829]]. The updater's effect jobs.
		update_check,
		update_download,
		update_verify,
		/// The release read again immediately before installing.
		update_refresh,
		/// Installation handed to the platform installer.
		update_apply,
		/// [[BPR-1831]]. A store's connection opened and its policy applied;
		/// `Schema` is the version found.
		storage_open,
		/// [[BPR-1831]]. A store's schema version written (a fresh store's
		/// creation is a migration from 0); `Schema` is the new version.
		storage_migrate,
		/// [[BPR-1831]]. A read-only statement failed.
		storage_read,
		/// [[BPR-1831]]. A writing statement failed.
		storage_write,
		/// [[BPR-1831]]. A statement failed before it could be classified
		/// as a read or a write (it did not prepare).
		storage_statement,
		/// [[BPR-1831]]. The listed clone folders scanned for a repository's
		/// clones; `Count` is the clones found.
		clone_scan,
		/// [[BPR-1831]]. A remembered clone link checked again.
		clone_verify,
		/// [[BPR-1831]]. The clone folders and links read from their store.
		clone_load,
		/// [[BPR-1831]]. One source search execution.
		source_search,
		/// [[BPR-1831]]. One body image fetch.
		image_fetch,
		/// [[BPR-1831]]. One avatar fetch.
		avatar_fetch,
		/// [[BPR-1831]]. One queue notification handed to the platform.
		notification_deliver,
		/// [[BPR-1831]]. The reviewer asked once for notification permission.
		notification_permission,
		/// [[BPR-1831]]. The reviewer chose a delivered notification.
		notification_activate,
		/// [[BPR-1830]]. The MCP context helper started for one request and
		/// its answer.
		mcp_helper_request,
		/// [[BPR-1830]]. Tours offered for import, as read from the pull request.
		tour_observe_imports,
		/// [[BPR-1830]]. One offered tour imported into the private draft.
		tour_import,
		/// [[BPR-1830]]. `bpr tour open`: the pull request read for a draft.
		tour_open,
		/// [[BPR-1830]]. `bpr tour source`: one pinned source read.
		tour_source,
		/// [[BPR-1830]]. `bpr tour counts`: the conversations read and counted.
		tour_counts,
		/// [[BPR-1830]]. `bpr tour preview`/`publish`: the plan prepared.
		tour_preview,
		/// [[BPR-1830]]. `bpr tour publish`: the confirmed plan published.
		tour_publish,
		/// [[BPR-1830]]. A round draft frozen into a round.
		cycle_round_freeze,
		/// [[BPR-1830]]. A frozen round granted to a harness.
		cycle_round_grant,
		/// [[BPR-1830]]. A round's grant withdrawn.
		cycle_round_revoke,
		/// [[BPR-1830]]. A granted round read by its harness.
		cycle_round_read,
		/// [[BPR-1830]]. A granted harness's response appended (acting on it).
		cycle_round_respond,
		/// [[BPR-1830]]. A response file imported into a round by the reviewer.
		cycle_response_import,
		/// [[BPR-1830]]. A frozen round exported (rendered, or to a file).
		cycle_round_export,
		/// [[BPR-1830]]. A portable round bundle opened into quarantine.
		cycle_bundle_import,
		/// [[BPR-1830]]. What one Send to Agent of a round observed, recorded.
		cycle_round_send,
	};

	/// \brief [[BPR-1828]]. Why an operation did not succeed: a closed set
	/// drawn from each family's own typed failure. `none` on a start and a
	/// success.
	enum class operation_failure : std::uint8_t {
		none,
		// Shared.
		cancelled,
		unavailable,
		// Agent setup (core::agent_setup_failure).
		agent_not_found,
		program_refused,
		no_program_chosen,
		not_signed_in,
		failed_to_start,
		status_unreadable,
		timed_out,
		workspace_unavailable,
		// Agent workspace (application::agent_workspace_failure and, for an
		// unavailable one, its agent_workspace_unavailable_reason).
		invalid_request,
		stale,
		cleanup_failed,
		copy_failed,
		download_failed,
		connection_failed,
		sign_in_refused,
		too_large,
		too_large_not_in_clone,
		// Review mutation (application::review_mutation_failure).
		refused_by_server,
		undesignated_target,
		target_moved,
		stale_head,
		unsupported,
		target_unavailable,
		malformed_response,
		incompatible_body,
		credential_unavailable,
		credential_tool_failed,
		unplaceable_line,
		refused_not_published,
		settled_not_published,
		off_hunk_refused,
		not_confirmed,
		/// An edit found GitHub's text changed since editing began; nothing
		/// was written.
		conflict,
		// [[BPR-1829]]. Sign-in (core::device_authorization_reason_code and
		// application::device_authorization_refresh_failure); `timed_out`,
		// `malformed_response` and `cancelled` are shared.
		authorization_denied,
		authorization_expired,
		device_flow_disabled,
		device_flow_unavailable,
		network_unavailable,
		certificate_rejected,
		redirect_rejected,
		response_too_large,
		vault_unavailable,
		refresh_absent,
		refresh_rejected,
		transport_failed,
		storage_failed,
		/// Sign out found no account or flow to end.
		no_session,
		// [[BPR-1829]]. Credential vault (the application vault port's typed failure);
		// `invalid_request`, `unavailable` and `cancelled` are shared.
		locked,
		access_denied,
		corrupt,
		operation_failed,
		allocation_failure,
		// [[BPR-1829]]. Updater (core::release_update_reason); `timed_out`,
		// `unavailable`, `cancelled`, `certificate_rejected` and
		// `redirect_rejected` are shared.
		/// A policy reason (current, skipped, disabled, an ineligible
		/// installation, not due, no candidate) ended an effect job.
		policy_refused,
		access_required,
		unauthenticated,
		forbidden,
		inaccessible,
		offline,
		rate_limited,
		malformed_release,
		invalid_version,
		unstable_release,
		no_compatible_asset,
		ambiguous_asset,
		invalid_digest,
		size_limit,
		unsafe_archive,
		invalid_bundle,
		wrong_architecture,
		wrong_signer,
		digest_mismatch,
		changed_candidate,
		busy,
		stale_completion,
		invalid_progress,
		identity_exhausted,
		installer_unavailable,
		flush_failed,
		quit_refused,
		recovery_required,
		shutdown,
		// [[BPR-1831]]. Storage: a refused open (native_storage::failure) or
		// a statement's SQLite result class (and the shared `busy` and
		// `locked`).
		invalid_path,
		invalid_permissions,
		schema_mismatch,
		integrity_failure,
		quota_exceeded,
		platform_unsupported,
		io_error,
		read_only,
		constraint,
		not_a_database,
		cannot_open,
		sql_error,
		// [[BPR-1831]]. Clone scan.
		not_a_clone,
		no_matching_remote,
		scan_failed,
		store_unreadable,
		// [[BPR-1831]]. Source search (and the shared `unavailable` and
		// `identity_exhausted`).
		search_failed,
		resource_limit,
		// [[BPR-1831]]. Image and avatar fetch (and the shared `too_large`
		// and `unsupported`).
		fetch_failed,
		blocked,
		over_limit,
		// [[BPR-1831]]. Notification.
		permission_refused,
		unattributable_activation,
		// [[BPR-1830]]. MCP helper (application::mcp_helper_refusal, and how
		// its run ended).
		no_helper,
		not_pinned,
		network_location,
		already_running,
		helper_failed,
		// [[BPR-1830]]. Tour: the CLI's error classes, and an import's refusals.
		invalid_input,
		interaction_required,
		authentication_unavailable,
		transport_unavailable,
		remote_incomplete,
		outcome_unknown,
		storage_unavailable,
		local_failure,
		not_offered,
		draft_exists,
		target_mismatch,
		refused,
		// [[BPR-1830]]. Review Cycle round (application::cycle_store_failure,
		// a grant's authorization and a response's or bundle's refusal);
		// `corrupt`, `busy`, `invalid_bundle` ([[BPR-1829]]) and
		// `schema_mismatch` ([[BPR-1831]]) are shared.
		wrong_profile,
		not_found,
		operation_reused,
		bounds,
		object_unavailable,
		quota,
		preview_stale,
		store_damaged,
		not_granted,
		revoked,
		wrong_target,
		not_authorized,
		read_failed,
	};

	/// \brief [[BPR-1831]]. Which SQLite store a storage line is about, by its
	/// file's fixed name; `unknown` when a statement's store could not be told.
	enum class operation_store : std::uint8_t {
		unknown,
		other,
		agent_setup,
		appearance_settings,
		diagnostic_preferences,
		workbench_settings,
		review_queue,
		review_ledger,
		review_tours,
		review_cycles,
		review_overviews,
		window_restoration,
		remembered_connections,
		device_bindings,
		review_mutation_recovery,
		epoch_catalog,
		markdown_images,
		insights,
		profile_registry,
		release_update,
		update_draft_recovery,
		clone_ledger,
	};

	/// \brief [[BPR-1828]]. How an agent setup check's process run ended,
	/// as the host's run outcome names it; `none` when no process ran.
	/// [[BPR-1830]]. Also how an MCP helper run ended.
	enum class operation_process : std::uint8_t {
		none,
		success,
		private_storage_failure,
		failed_to_start,
		workspace_unavailable,
		nonzero_exit,
		protocol_failure,
		cancelled,
		timeout,
		stale_result,
	};

	/// \brief [[BPR-1828]]. One host operation's diagnostic line: enums,
	/// counts, a duration and an exit code only.
	///
	/// \details Diagnostics are exported, so nothing here names a program
	/// path, workspace path, repository, account, thread, comment or body.
	/// `Count` is the family's one useful count: the process runs a check
	/// made, the reviewers a reviewer answer lists, or the comments a
	/// reconciliation read back. [[BPR-1829]]: a device sign-in's token
	/// polls, 1 when a vault load found or a delete removed its slot (0
	/// when absent), 1 when an account change leaves an account selected,
	/// and a download's received bytes. `Written` and `ReadBack` say, for a
	/// mutation, whether a write was sent and whether its outcome was
	/// learned by reading GitHub back. `Process` and `ExitCode` are an agent
	/// setup check's last process run.
	struct operation_diagnosis
	{
		operation_action   Action  = operation_action::agent_status_check;
		operation_failure  Failure = operation_failure::none;
		operation_process  Process = operation_process::none;
		std::optional<int> ExitCode;
		std::uint32_t      DurationMilliseconds = 0U;
		std::uint32_t      Count                = 0U;
		bool               Written              = false;
		bool               ReadBack             = false;
		/// [[BPR-1831]]. A storage line's store and schema version, written
		/// after the fixed keys as `store=STORE schema=N`; absent on every
		/// other family's line.
		std::optional<operation_store> Store;
		std::uint32_t                  Schema = 0U;

		auto operator<=>(const operation_diagnosis&) const = default;
	};

	[[nodiscard]] auto family(operation_action aAction) noexcept -> operation_diagnosis_family;

	/// \brief The stable lower-case names, empty outside each set.
	[[nodiscard]] auto name(operation_action aAction) noexcept -> std::string_view;
	[[nodiscard]] auto name(operation_failure aFailure) noexcept -> std::string_view;
	[[nodiscard]] auto name(operation_process aProcess) noexcept -> std::string_view;
	[[nodiscard]] auto name(operation_store aStore) noexcept -> std::string_view;

	/// \brief Whether a line is one the host writes: known enums, a failure
	/// of the action's own family, a process and exit code only on an agent
	/// setup check or ([[BPR-1830]]) an MCP helper run (an exit code only
	/// with a process), and the write flags
	/// only on a review mutation; a store exactly on a storage line, and a
	/// schema version only there ([[BPR-1831]]).
	[[nodiscard]] auto valid(const operation_diagnosis& aDiagnosis) noexcept -> bool;

	/// \brief The line for the diagnostics log, in fixed key order:
	/// `action=ACTION failure=FAILURE process=PROCESS exit=none|N
	/// duration_ms=N count=N written=0|1 read_back=0|1`, and on a storage
	/// line ` store=STORE schema=N` ([[BPR-1831]]). Empty when the line is
	/// not valid.
	[[nodiscard]] auto describe(const operation_diagnosis& aDiagnosis) -> std::string;

	/// \brief Reads exactly what `describe` writes; anything else is none.
	[[nodiscard]] auto parse_operation_diagnosis(std::string_view aText) -> std::optional<operation_diagnosis>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_OPERATION_DIAGNOSIS_HPP
