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

		const auto workspace = operation_diagnosis{ .Action = operation_action::workspace_materialize, .Failure = operation_failure::too_large_not_in_clone, .DurationMilliseconds = 2400U };
		expect(parse_operation_diagnosis(describe(workspace)) == workspace, "a workspace line reads back");
		expect(family(operation_action::workspace_cleanup) == operation_diagnosis_family::agent_workspace && family(operation_action::agent_program_check) == operation_diagnosis_family::agent_setup
				   && family(operation_action::resolve_thread) == operation_diagnosis_family::review_mutation,
			   "each action belongs to its family");
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
} // namespace

int main()
{
	test_line();
	test_refusals();
	test_account_and_update_lines();
	test_storage_and_host_lines();
	test_helper_tour_and_round_lines();
	std::cout << "operation-diagnosis-tests:pass\n";
	return EXIT_SUCCESS;
}
