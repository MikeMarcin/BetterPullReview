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
} // namespace

int main()
{
	test_line();
	test_refusals();
	test_account_and_update_lines();
	std::cout << "operation-diagnosis-tests:pass\n";
	return EXIT_SUCCESS;
}
