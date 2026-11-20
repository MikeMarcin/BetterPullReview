// [[BPR-1828]]. The diagnostic line agent setup checks, agent workspaces and
// review mutations record: exactly its enums, counts, duration and exit code.
#include "bpr/core/operation_diagnosis.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_line()
	{
		const auto check = operation_diagnosis{ .Action = operation_action::agent_status_check, .Failure = operation_failure::not_signed_in, .Process = operation_process::nonzero_exit, .ExitCode = 3, .DurationMilliseconds = 812U, .Count = 2U };
		const auto text  = describe(check);
		expect(text == "action=agent_status_check failure=not_signed_in process=nonzero_exit exit=3 duration_ms=812 count=2 written=0 read_back=0", "a check's line holds its fields in order");
		expect(parse_operation_diagnosis(text) == check, "and reads back as itself");

		const auto negative = operation_diagnosis{ .Action = operation_action::agent_custom_check, .Failure = operation_failure::timed_out, .Process = operation_process::timeout, .ExitCode = -9 };
		expect(parse_operation_diagnosis(describe(negative)) == negative, "a negative exit code reads back");

		const auto mutation = operation_diagnosis{ .Action = operation_action::request_reviewer, .DurationMilliseconds = 900U, .Count = 2U, .Written = true, .ReadBack = true };
		expect(describe(mutation) == "action=request_reviewer failure=none process=none exit=none duration_ms=900 count=2 written=1 read_back=1", "a mutation's line carries its write flags");
		expect(parse_operation_diagnosis(describe(mutation)) == mutation, "and reads back as itself");

		const auto workspace = operation_diagnosis{ .Action = operation_action::workspace_materialize, .Failure = operation_failure::clone_needed, .DurationMilliseconds = 2400U };
		expect(parse_operation_diagnosis(describe(workspace)) == workspace, "a workspace line reads back");
		expect(family(operation_action::workspace_cleanup) == operation_diagnosis_family::agent_workspace && family(operation_action::agent_program_check) == operation_diagnosis_family::agent_setup
				   && family(operation_action::resolve_thread) == operation_diagnosis_family::review_mutation,
			   "each action belongs to its family");
		// [[BPR-2261]]. A retained workspace checked, not made: its own line,
		// with its duration, among the workspace lines.
		const auto verified     = operation_diagnosis{ .Action = operation_action::workspace_verify, .DurationMilliseconds = 117000U };
		const auto verifiedLine = describe(verified);
		expect(verifiedLine.starts_with("action=workspace_verify failure=none") && verifiedLine.find("duration_ms=117000") != std::string::npos && parse_operation_diagnosis(verifiedLine) == verified,
			   "a workspace check reads back as itself: " + verifiedLine);
		expect(family(operation_action::workspace_verify) == operation_diagnosis_family::agent_workspace, "and is an agent workspace line");
	}

	void test_tour_publication_trace()
	{
		auto       line = operation_diagnosis{ .Action = operation_action::tour_publish, .Failure = operation_failure::refused_by_server, .Tour = tour_publication_trace{ .Stage = tour_publication_stage::baseline_read, .State = tour_publication_trace_state::unavailable, .Attempt = std::string(64U, 'a'), .HttpStatus = 403 } };
		const auto text = describe(line);
		expect(!text.empty() && parse_operation_diagnosis(text) == line, "publication stage failure status and full attempt digest round-trip through export classifier grammar");
		for (const auto& unsafe : { std::string{ "private prose" }, std::string(64U, 'g'), std::string(63U, 'a') }) {
			line.Tour->Attempt = unsafe;
			expect(describe(line).empty(), "unclassified strings cannot enter publication trace");
		}
		line.Tour->Attempt    = "none";
		line.Tour->HttpStatus = 999;
		expect(describe(line).empty(), "invalid HTTP status is not a classified trace");
		line.Tour->HttpStatus = 0;
		line.Action           = operation_action::publish_comment;
		expect(describe(line).empty(), "tour metadata belongs only to tour lifecycle actions");
		expect(!parse_operation_diagnosis(text + " body=private"), "unknown trace suffix is rejected");
	}
	// [[BPR-1829]]. Sign-in, credential vault and updater lines.
	void test_account_and_update_lines()
	{
		const auto signIn = operation_diagnosis{ .Action = operation_action::device_sign_in, .Failure = operation_failure::authorization_expired, .DurationMilliseconds = 900000U, .Count = 12U };
		expect(describe(signIn) == "action=device_sign_in failure=authorization_expired process=none exit=none duration_ms=900000 count=12 written=0 read_back=0", "a device sign-in's line holds its fields in order");
		expect(parse_operation_diagnosis(describe(signIn)) == signIn, "and reads back as itself");
		const auto vault = operation_diagnosis{ .Action = operation_action::refresh_load, .Failure = operation_failure::locked, .DurationMilliseconds = 3U };
		expect(parse_operation_diagnosis(describe(vault)) == vault, "a vault line reads back");
		const auto update = operation_diagnosis{ .Action = operation_action::update_apply, .Failure = operation_failure::quit_refused, .DurationMilliseconds = 40U };
		expect(parse_operation_diagnosis(describe(update)) == update, "an updater line reads back");
		expect(family(operation_action::account_change) == operation_diagnosis_family::sign_in && family(operation_action::vault_delete) == operation_diagnosis_family::credential_vault
				   && family(operation_action::update_check) == operation_diagnosis_family::updater && family(operation_action::request_reviewer) == operation_diagnosis_family::review_mutation,
			   "each new action belongs to its family, and the last mutation action still to its own");
		expect(!describe({ .Action = operation_action::sign_out, .Failure = operation_failure::no_session }).empty() && !describe({ .Action = operation_action::update_download, .Failure = operation_failure::certificate_rejected }).empty()
				   && !describe({ .Action = operation_action::device_sign_in, .Failure = operation_failure::certificate_rejected }).empty()
				   && !describe({ .Action = operation_action::vault_store, .Failure = operation_failure::invalid_request }).empty(),
			   "shared failure words are admitted by each family that can report them");
		expect(describe({ .Action = operation_action::vault_store, .Failure = operation_failure::authorization_denied }).empty(), "a sign-in failure on a vault line is no line");
		expect(describe({ .Action = operation_action::update_check, .Failure = operation_failure::refresh_absent }).empty(), "a sign-in failure on an updater line is no line");
		expect(describe({ .Action = operation_action::device_sign_in, .Failure = operation_failure::wrong_signer }).empty(), "an updater failure on a sign-in line is no line");
		expect(describe({ .Action = operation_action::resolve_thread, .Failure = operation_failure::locked }).empty(), "a vault failure on a mutation is no line");
		expect(describe({ .Action = operation_action::update_check, .Process = operation_process::success }).empty(), "an updater line names no process");
		expect(describe({ .Action = operation_action::vault_delete, .Written = true }).empty(), "a vault line has no write flag");
	}

	void test_refusals()
	{
		for (const auto why : { operation_failure::object_unavailable, operation_failure::size_limit, operation_failure::timed_out,
								operation_failure::storage_failed, operation_failure::unavailable, operation_failure::unsupported,
								operation_failure::outcome_unknown, operation_failure::changed_candidate, operation_failure::stale }) {
			const auto diagnosis = operation_diagnosis{ .Action = operation_action::native_continuation, .Failure = why };
			expect(family(diagnosis.Action) == operation_diagnosis_family::agent_workspace && parse_operation_diagnosis(describe(diagnosis)) == diagnosis,
				   "native pre-dispatch refusals round-trip as exportable workspace diagnostics with stable failure names");
		}

		expect(describe({ .Action = operation_action::resolve_thread, .Failure = operation_failure::not_signed_in }).empty(), "a failure from another family is no line");
		expect(describe({ .Action = operation_action::workspace_cleanup, .Failure = operation_failure::refused_by_server }).empty(), "a mutation failure on a workspace is no line");
		expect(describe({ .Action = operation_action::resolve_thread, .Process = operation_process::success }).empty(), "only a setup check names a process");
		expect(describe({ .Action = operation_action::agent_status_check, .ExitCode = 0 }).empty(), "an exit code needs a process");
		expect(describe({ .Action = operation_action::workspace_materialize, .Written = true }).empty(), "only a mutation says it wrote");
		expect(describe({ .Action = static_cast<operation_action>(200) }).empty(), "an unknown action is no line");
		expect(describe({ .Action = operation_action::resolve_thread, .Failure = static_cast<operation_failure>(200) }).empty(), "an unknown failure is no line");

		const std::string valid = "action=resolve_thread failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0";
		expect(parse_operation_diagnosis(valid).has_value(), "the control line reads");
		for (const std::string_view forged : {
				 "action=resolve_thread failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 thread=PRIVATE",
				 "action=resolve_thread failure=none process=none exit=none duration_ms=1 count=0 written=0",
				 "action=PRIVATE failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0",
				 "action=resolve_thread failure=PRIVATE process=none exit=none duration_ms=1 count=0 written=0 read_back=0",
				 "action=resolve_thread failure=none process=none exit=none duration_ms=01 count=0 written=0 read_back=0",
				 "action=resolve_thread failure=none process=none exit=none duration_ms=-1 count=0 written=0 read_back=0",
				 "action=resolve_thread failure=none process=none exit=none duration_ms=1 count=0 written=2 read_back=0",
				 "action=resolve_thread  failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0",
				 "action=agent_status_check failure=none process=success exit=-0 duration_ms=1 count=1 written=0 read_back=0",
				 "action=agent_status_check failure=none process=success exit=x duration_ms=1 count=1 written=0 read_back=0",
				 "action=resolve_thread failure=none process=none exit=none duration_ms=4294967296 count=0 written=0 read_back=0",
				 "failure=none action=resolve_thread process=none exit=none duration_ms=1 count=0 written=0 read_back=0",
				 "" }) {
			expect(!parse_operation_diagnosis(forged).has_value(), std::string{ "a line that is not exactly what describe writes is refused: " } + std::string{ forged });
		}
	}
	/// [[BPR-1830]]. The MCP helper, tour and Review Cycle round families.
	void test_helper_tour_and_round_lines()
	{
		const auto helper = operation_diagnosis{ .Action = operation_action::mcp_helper_request, .Failure = operation_failure::helper_failed, .Process = operation_process::nonzero_exit, .ExitCode = 2, .DurationMilliseconds = 40U, .Count = 1U };
		expect(describe(helper) == "action=mcp_helper_request failure=helper_failed process=nonzero_exit exit=2 duration_ms=40 count=1 written=0 read_back=0", "a helper run names its process and exit code");
		expect(parse_operation_diagnosis(describe(helper)) == helper, "and reads back as itself");
		expect(!describe({ .Action = operation_action::mcp_helper_request, .Failure = operation_failure::not_pinned }).empty(), "a helper refusal is a line");

		const auto tour = operation_diagnosis{ .Action = operation_action::tour_publish, .Failure = operation_failure::outcome_unknown, .DurationMilliseconds = 1200U, .Count = 3U };
		expect(parse_operation_diagnosis(describe(tour)) == tour, "a tour line reads back");
		const auto import = operation_diagnosis{ .Action = operation_action::tour_import, .Failure = operation_failure::draft_exists, .Count = 2U };
		expect(parse_operation_diagnosis(describe(import)) == import, "an import line reads back");

		const auto round = operation_diagnosis{ .Action = operation_action::cycle_round_read, .Failure = operation_failure::revoked, .DurationMilliseconds = 3U };
		expect(describe(round) == "action=cycle_round_read failure=revoked process=none exit=none duration_ms=3 count=0 written=0 read_back=0", "a round line is enums and a duration");
		expect(parse_operation_diagnosis(describe(round)) == round, "and reads back as itself");
		const auto respond = operation_diagnosis{ .Action = operation_action::cycle_round_respond, .Failure = operation_failure::malformed_response };
		expect(parse_operation_diagnosis(describe(respond)) == respond, "a malformed response is a round failure");

		expect(family(operation_action::mcp_helper_request) == operation_diagnosis_family::mcp_helper && family(operation_action::tour_observe_imports) == operation_diagnosis_family::tour
				   && family(operation_action::tour_publish) == operation_diagnosis_family::tour && family(operation_action::cycle_round_freeze) == operation_diagnosis_family::cycle_round
				   && family(operation_action::cycle_round_send) == operation_diagnosis_family::cycle_round && family(operation_action::request_reviewer) == operation_diagnosis_family::review_mutation,
			   "each new action belongs to its family, and the earlier ones keep theirs");

		// Each family keeps to its own failures, process and flags.
		expect(describe({ .Action = operation_action::cycle_round_grant, .Failure = operation_failure::not_signed_in }).empty(), "an agent setup failure on a round is no line");
		expect(describe({ .Action = operation_action::tour_open, .Failure = operation_failure::revoked }).empty(), "a round failure on a tour is no line");
		expect(describe({ .Action = operation_action::mcp_helper_request, .Failure = operation_failure::draft_exists }).empty(), "a tour failure on the helper is no line");
		expect(describe({ .Action = operation_action::resolve_thread, .Failure = operation_failure::no_helper }).empty(), "a helper failure on a mutation is no line");
		expect(describe({ .Action = operation_action::tour_open, .Process = operation_process::success }).empty(), "a tour names no process");
		expect(describe({ .Action = operation_action::cycle_round_read, .Written = true }).empty(), "a round says nothing of writes");
		for (const std::string_view forged : {
				 "action=cycle_round_read failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 cycle=PRIVATE",
				 "action=tour_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 repository=PRIVATE",
				 "action=mcp_helper_request failure=none process=success exit=0 duration_ms=1 count=0 written=0 read_back=0 root=/PRIVATE",
				 "action=tour_open failure=none process=success exit=0 duration_ms=1 count=0 written=0 read_back=0" }) {
			expect(!parse_operation_diagnosis(forged).has_value(), std::string{ "a forged helper, tour or round line is refused: " } + std::string{ forged });
		}
	}
} // namespace

namespace {
	/// [[BPR-1831]]. Storage lines carry their store and schema version;
	/// every other family's line stays exactly as it was.
	void test_storage_and_host_lines()
	{
		const auto open = operation_diagnosis{ .Action = operation_action::storage_open, .DurationMilliseconds = 3U, .Count = 4U, .Store = operation_store::review_ledger, .Schema = 7U };
		expect(describe(open) == "action=storage_open failure=none process=none exit=none duration_ms=3 count=4 written=0 read_back=0 store=review_ledger schema=7", "a storage line ends with its store and schema");
		expect(parse_operation_diagnosis(describe(open)) == open, "and reads back as itself");
		const auto refused = operation_diagnosis{ .Action = operation_action::storage_write, .Failure = operation_failure::busy, .Count = 1U, .Store = operation_store::unknown };
		expect(parse_operation_diagnosis(describe(refused)) == refused, "a statement failure of an unknown store reads back");
		expect(describe({ .Action = operation_action::storage_open }).empty(), "a storage line names its store");
		expect(describe({ .Action = operation_action::image_fetch, .Store = operation_store::insights }).empty(), "only a storage line names a store");
		expect(describe({ .Action = operation_action::image_fetch, .Schema = 2U }).empty(), "only a storage line names a schema");
		expect(describe({ .Action = operation_action::image_fetch, .Failure = operation_failure::busy }).empty(), "a storage failure is no image failure");
		expect(describe({ .Action = operation_action::notification_deliver, .Failure = operation_failure::too_large }).empty(), "an image failure is no notification failure");
		expect(describe({ .Action = operation_action::storage_open, .Store = static_cast<operation_store>(200) }).empty(), "an unknown store is no line");

		for (const auto& line : { operation_diagnosis{ .Action = operation_action::clone_scan, .DurationMilliseconds = 90U, .Count = 2U },
								  operation_diagnosis{ .Action = operation_action::clone_verify, .Failure = operation_failure::no_matching_remote },
								  operation_diagnosis{ .Action = operation_action::clone_load, .Failure = operation_failure::store_unreadable },
								  operation_diagnosis{ .Action = operation_action::source_search, .Failure = operation_failure::resource_limit, .Count = 12U },
								  operation_diagnosis{ .Action = operation_action::source_search, .Failure = operation_failure::unavailable },
								  operation_diagnosis{ .Action = operation_action::avatar_fetch, .Failure = operation_failure::blocked },
								  operation_diagnosis{ .Action = operation_action::image_fetch, .Failure = operation_failure::too_large },
								  // [[BPR-1846]]. The GitHub CLI avatar helper's own failures.
								  operation_diagnosis{ .Action = operation_action::avatar_fetch, .Failure = operation_failure::no_helper },
								  operation_diagnosis{ .Action = operation_action::avatar_fetch, .Failure = operation_failure::credential_unavailable },
								  operation_diagnosis{ .Action = operation_action::avatar_fetch, .Failure = operation_failure::credential_tool_failed },
								  operation_diagnosis{ .Action = operation_action::avatar_fetch, .Failure = operation_failure::malformed_response },
								  operation_diagnosis{ .Action = operation_action::notification_permission, .Failure = operation_failure::permission_refused },
								  operation_diagnosis{ .Action = operation_action::notification_activate },
								  operation_diagnosis{ .Action = operation_action::storage_migrate, .Failure = operation_failure::cancelled, .Store = operation_store::clone_ledger, .Schema = 3U } }) {
			expect(!describe(line).empty() && parse_operation_diagnosis(describe(line)) == line, "each new family's line reads back as itself");
		}
		expect(family(operation_action::storage_statement) == operation_diagnosis_family::storage && family(operation_action::clone_load) == operation_diagnosis_family::clone_scan
				   && family(operation_action::source_search) == operation_diagnosis_family::source_search && family(operation_action::avatar_fetch) == operation_diagnosis_family::image_fetch
				   && family(operation_action::notification_activate) == operation_diagnosis_family::notification && family(operation_action::request_reviewer) == operation_diagnosis_family::review_mutation,
			   "each new action belongs to its family, and the old ones keep theirs");

		for (const std::string_view forged : {
				 "action=resolve_thread failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 store=insights schema=1",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 store=insights",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 store=PRIVATE schema=1",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 store=insights schema=1 path=PRIVATE",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 schema=1 store=insights",
				 "action=storage_open failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 store=insights schema=01",
				 "action=storage_read failure=none process=none exit=none duration_ms=1 count=0 written=1 read_back=0 store=insights schema=1",
				 "action=image_fetch failure=none process=success exit=0 duration_ms=1 count=1 written=0 read_back=0",
			 }) {
			expect(!parse_operation_diagnosis(forged).has_value(), std::string{ "a host line that is not exactly what describe writes is refused: " } + std::string{ forged });
		}
	}

	/// [[RFC-BPR-0105]]. A review submission's request, waits, drops and
	/// ending, each naming its subject.
	void test_review_submission_lines()
	{
		const auto wait = operation_diagnosis{ .Action = operation_action::review_submit_wait, .DurationMilliseconds = 20000U, .Submission = review_submission_note::review_lane };
		expect(describe(wait) == "action=review_submit_wait failure=none process=none exit=none duration_ms=20000 count=0 written=0 read_back=0 submit_subject=review_lane", "a wait names what it waited for and how long");
		expect(parse_operation_diagnosis(describe(wait)) == wait && family(wait.Action) == operation_diagnosis_family::review_mutation, "and reads back as a review mutation line");
		const auto ended = operation_diagnosis{ .Action = operation_action::review_submit_end, .Failure = operation_failure::cancelled, .DurationMilliseconds = 20001U, .Submission = review_submission_note::wait_expired };
		expect(parse_operation_diagnosis(describe(ended)) == ended, "an ending reads back");
		const auto drop = operation_diagnosis{ .Action = operation_action::review_submit_drop, .Submission = review_submission_note::retired };
		expect(parse_operation_diagnosis(describe(drop)) == drop, "a drop reads back");
		expect(describe({ .Action = operation_action::review_submit_request }).empty(), "a submission line names its subject");
		expect(describe({ .Action = operation_action::resolve_thread, .Submission = review_submission_note::published }).empty(), "only a submission line names one");
		expect(describe({ .Action = operation_action::review_submit_end, .Submission = static_cast<review_submission_note>(200) }).empty(), "an unknown subject is no line");
		// [[BPR-2263]]. Not folded into refused_not_published: the failure
		// it came with, and the check and reason that refused it.
		const auto refused = operation_diagnosis{ .Action = operation_action::review_submit_end, .Failure = operation_failure::unavailable, .DurationMilliseconds = 35U, .Submission = review_submission_note::not_published, .Refusal = review_submission_refusal{ .Check = review_submission_refusal_check::host, .Reason = review_submission_refusal_reason::provider_unavailable } };
		expect(describe(refused) == "action=review_submit_end failure=unavailable process=none exit=none duration_ms=35 count=0 written=0 read_back=0 submit_subject=not_published submit_check=host submit_reason=provider_unavailable submit_part=none submit_comment=0",
			   "an ending names the check, reason and part that refused it");
		expect(parse_operation_diagnosis(describe(refused)) == refused, "and reads back");
		const auto repaired = operation_diagnosis{ .Action = operation_action::review_submit_repair, .Submission = review_submission_note::repaired };
		expect(parse_operation_diagnosis(describe(repaired)) == repaired, "a repair reads back");
		const auto invalid = operation_diagnosis{ .Action = operation_action::review_submit_end, .Failure = operation_failure::unavailable, .Submission = review_submission_note::not_published, .Refusal = review_submission_refusal{ .Reason = review_submission_refusal_reason::invalid_request, .Part = review_submission_refusal_part::comment_body_missing, .Comment = 3U } };
		expect(parse_operation_diagnosis(describe(invalid)) == invalid, "an invalid part and its comment read back");
		expect(describe({ .Action = operation_action::review_submit_wait, .Submission = review_submission_note::review_lane, .Refusal = review_submission_refusal{ } }).empty(), "a refusal only on an end or repair line");
	}
	/// [[BPR-2295]]. A workspace line names its source, the review's
	/// loading, the clone and, on a failure, the Git command and Git's words
	/// as a class; never an object or a path.
	void test_workspace_trace()
	{
		const auto failed = operation_diagnosis{ .Action = operation_action::workspace_materialize, .Failure = operation_failure::history_missing, .DurationMilliseconds = 106U, .Workspace = workspace_trace{ .Source = workspace_source::cache_copy, .Sparse = false, .CloneGiven = false, .CloneHeldHead = false, .Command = workspace_git_command::pack_objects, .GitError = workspace_git_error::failed_to_traverse_parents } };
		expect(describe(failed) == "action=workspace_materialize failure=history_missing process=none exit=none duration_ms=106 count=0 written=0 read_back=0 source=cache_copy sparse=0 clone=0 clone_head=0 git=pack_objects git_error=failed_to_traverse_parents",
			   "a history_missing failure names the cache copy, the review's loading, the clone and Git's words");
		expect(parse_operation_diagnosis(describe(failed)) == failed, "and reads back");
		const auto borrowed = operation_diagnosis{ .Action = operation_action::workspace_materialize, .Workspace = workspace_trace{ .Source = workspace_source::borrowed, .Sparse = true, .CloneGiven = true, .CloneHeldHead = true } };
		expect(parse_operation_diagnosis(describe(borrowed)) == borrowed, "a borrowed workspace reads back");
		expect(describe({ .Action = operation_action::workspace_cleanup, .Workspace = workspace_trace{ } }).empty(), "a trace only on a materialization line");
		expect(describe({ .Action = operation_action::workspace_materialize, .Workspace = workspace_trace{ .Source = static_cast<workspace_source>(90) } }).empty(), "an unknown source is no line");
		expect(name(operation_failure::history_missing) == "history_missing", "history_missing has its stable name");
		expect(classify_workspace_git_error("error: Could not read 0123abcd\nfatal: Failed to traverse parents of commit 4567ef\n") == workspace_git_error::failed_to_traverse_parents,
			   "the owner's two lines are the traverse failure");
		expect(classify_workspace_git_error("error: Could not read 0123abcd\n") == workspace_git_error::could_not_read, "a lone unreadable object");
		expect(classify_workspace_git_error("fatal: unable to read 0123abcd\n") == workspace_git_error::missing_object, "a missing object");
		expect(classify_workspace_git_error("fatal: something else\n") == workspace_git_error::other, "anything else");
		expect(classify_workspace_git_error(" \n") == workspace_git_error::none, "nothing written");
		expect(!parse_operation_diagnosis("action=workspace_materialize failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 source=borrowed sparse=1 clone=1 clone_head=1 git=none git_error=none path=/x").has_value(),
			   "nothing follows the trace");
	}

	/// [[BPR-2321]]. A seed from the linked clone names its outcome, each
	/// step's time and its counts; a line with no seed is unchanged.
	void test_workspace_seed_trace()
	{
		const auto seeded = operation_diagnosis{ .Action = operation_action::workspace_materialize, .DurationMilliseconds = 900U, .Workspace = workspace_trace{ .Source = workspace_source::borrowed, .Sparse = true, .CloneGiven = true, .CloneHeldHead = true, .Seed = workspace_seed_trace{ .Outcome = workspace_seed_outcome::seeded, .WalkMilliseconds = 40U, .CloneMilliseconds = 230U, .RefreshMilliseconds = 200U, .CheckoutMilliseconds = 5U, .CleanMilliseconds = 30U, .FoldersCloned = 98U, .FilesCloned = 402U, .FilesWritten = 3U, .Cleaned = 0U } } };
		expect(describe(seeded).ends_with("git=none git_error=none seed=seeded seed_walk_ms=40 seed_clone_ms=230 seed_refresh_ms=200 seed_checkout_ms=5 seed_clean_ms=30 seed_folders=98 seed_files=402 seed_written=3 seed_cleaned=0"),
			   "the seed's steps and counts follow the trace");
		expect(parse_operation_diagnosis(describe(seeded)) == seeded, "and read back");
		const auto spread = operation_diagnosis{ .Action = operation_action::workspace_materialize, .Workspace = workspace_trace{ .Source = workspace_source::borrowed, .CloneGiven = true, .Seed = workspace_seed_trace{ .Outcome = workspace_seed_outcome::too_spread, .WalkMilliseconds = 12U, .CheckoutMilliseconds = 1800U, .FilesWritten = 20003U } } };
		expect(describe(spread).find(" seed=too_spread ") != std::string::npos && parse_operation_diagnosis(describe(spread)) == spread, "a fallback names which");
		expect(describe({ .Action = operation_action::workspace_materialize, .Workspace = workspace_trace{ .Seed = workspace_seed_trace{ .Cleaned = 4U } } }).empty(), "counts with no seed are no line");
		expect(describe({ .Action = operation_action::workspace_materialize, .Workspace = workspace_trace{ .Seed = workspace_seed_trace{ .Outcome = static_cast<workspace_seed_outcome>(40) } } }).empty(), "an unknown outcome is no line");
		const auto prefix = std::string{ "action=workspace_materialize failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 source=borrowed sparse=1 clone=1 clone_head=1 git=none git_error=none" };
		expect(!parse_operation_diagnosis(prefix + " seed=none seed_walk_ms=0 seed_clone_ms=0 seed_refresh_ms=0 seed_checkout_ms=0 seed_clean_ms=0 seed_folders=0 seed_files=0 seed_written=0 seed_cleaned=0").has_value(), "no seed is written as none");
		expect(!parse_operation_diagnosis(prefix + " seed=seeded seed_walk_ms=1").has_value(), "a seed names every step");
		expect(!parse_operation_diagnosis(prefix + " seed=seeded seed_walk_ms=0 seed_clone_ms=0 seed_refresh_ms=0 seed_checkout_ms=0 seed_clean_ms=0 seed_folders=0 seed_files=0 seed_written=0 seed_cleaned=0 path=/x").has_value(),
			   "nothing follows the seed");
	}
} // namespace

namespace {
	/// [[BPR-2322]]. A side chat line: the item's kind, the round, the apply
	/// action, the refusal and the texts' lengths, and nothing else.
	void test_side_chat_lines()
	{
		auto asked = operation_diagnosis{ .Action = operation_action::side_chat_ask, .DurationMilliseconds = 1200U, .SideChat = side_chat_trace{ .Item = follow_up_subject_kind::suggested_comment, .Round = 2U, .QuestionChars = 31U, .AnswerChars = 410U } };
		expect(describe(asked) == "action=side_chat_ask failure=none process=none exit=none duration_ms=1200 count=0 written=0 read_back=0 item=suggested_comment round=2 apply=none refusal=none question_chars=31 answer_chars=410 before_chars=0 after_chars=0",
			   "an asked line holds its trace in order");
		expect(parse_operation_diagnosis(describe(asked)) == asked && family(operation_action::side_chat_ask) == operation_diagnosis_family::side_chat, "and reads back as a side chat line");
		auto used = operation_diagnosis{ .Action = operation_action::side_chat_apply, .Failure = operation_failure::refused, .SideChat = side_chat_trace{ .Item = follow_up_subject_kind::draft, .Round = 1U, .Apply = follow_up_apply::update_draft, .Refusal = follow_up_refusal::target_changed, .BeforeChars = 17U, .AfterChars = 64U } };
		expect(parse_operation_diagnosis(describe(used)) == used && describe(used).find("apply=update_draft refusal=target_changed") != std::string::npos, "a refused Use reads back with its action and why");
		auto stopped              = asked;
		stopped.Failure           = operation_failure::cancelled;
		stopped.SideChat->Refusal = follow_up_refusal::failed;
		expect(parse_operation_diagnosis(describe(stopped)) == stopped, "a stopped question reads back");
		// What is not a side chat line is refused.
		auto noTrace     = asked;
		noTrace.SideChat = std::nullopt;
		expect(describe(noTrace).empty(), "a side chat line carries its trace");
		auto applyOnAsk            = asked;
		applyOnAsk.SideChat->Apply = follow_up_apply::draft_reply;
		expect(describe(applyOnAsk).empty(), "a question names no apply action");
		auto noApply            = used;
		noApply.SideChat->Apply = std::nullopt;
		expect(describe(noApply).empty(), "a Use names its apply action");
		auto silent    = used;
		silent.Failure = operation_failure::none;
		expect(describe(silent).empty(), "a refusal is a failed line");
		auto elsewhere = operation_diagnosis{ .Action = operation_action::tour_import, .SideChat = asked.SideChat };
		expect(describe(elsewhere).empty(), "a side chat trace only on a side chat line");
		expect(!parse_operation_diagnosis(describe(asked) + " question=Why"), "no words after the trace");
		expect(!parse_operation_diagnosis("action=side_chat_ask failure=none process=none exit=none duration_ms=1 count=0 written=0 read_back=0 item=finding round=1 apply=none refusal=none question_chars=1 answer_chars=1 before_chars=0 after_chars=0"),
			   "an unknown item kind is refused");
		expect(character_count("héllo") == 5U && character_count("") == 0U, "lengths count characters, not bytes");
	}
} // namespace

int main()
{
	test_side_chat_lines();
	test_line();
	test_workspace_trace();
	test_workspace_seed_trace();
	test_review_submission_lines();
	test_tour_publication_trace();
	test_refusals();
	test_account_and_update_lines();
	test_storage_and_host_lines();
	test_helper_tour_and_round_lines();
	std::cout << "operation-diagnosis-tests:pass\n";
	return EXIT_SUCCESS;
}
