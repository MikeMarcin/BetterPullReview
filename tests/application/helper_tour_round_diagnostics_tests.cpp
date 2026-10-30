/// \file
/// \brief [[BPR-1830]]. The MCP context helper's supervisor, tour import and
/// the typed tour and Review Cycle failures leave operation lines: a start
/// where something is written or run, an end with its typed failure, duration
/// and count, and never a path, binding, request, answer, tour text or label.

#include "../support/review_tour_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "bpr/application/review_tour_host.hpp"
#if defined(BPR_TEST_MCP_SUPERVISOR)
	#include "bpr/application/mcp_context_helper_supervisor.hpp"
#endif

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using application::diagnostic_operation;
	using application::diagnostic_outcome;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::string_view canary = "PRIVATE_CANARY";

	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }

		[[nodiscard]] auto line(const std::size_t aIndex) const -> std::string { return std::string{ Events.at(aIndex).Summary.text() }; }
		[[nodiscard]] auto last() const -> std::string { return line(Events.size() - 1U); }

		std::vector<application::diagnostic_event> Events;
	};

	/// Every record is exactly an operation line on `aOperation`, with no identity and no private text.
	void expect_private(const recording_log& aLog, const diagnostic_operation aOperation)
	{
		for (const auto& event : aLog.Events) {
			const auto text = event.Summary.text();
			expect(text.find(canary) == std::string_view::npos && text.find('/') == std::string_view::npos, "no path, binding, request, answer or tour text reaches a record");
			expect(core::parse_operation_diagnosis(text).has_value(), "every record is exactly an operation line");
			expect(event.Operation == aOperation && event.Request == 0U && event.WindowSession == 0U, "on its family's operation, with no identity");
		}
	}

#if defined(BPR_TEST_MCP_SUPERVISOR)
	/// Answers each run from a script; keeps what it was asked.
	class scripted_processes final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, int, const application::agent_output_sink& = { }, std::stop_token = { }) -> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			auto result     = application::agent_run_result{ };
			result.Outcome  = Outcome;
			result.ExitCode = ExitCode;
			for (auto index = 0; index < Lines; ++index) {
				result.Events.push_back({ .Text = std::string{ canary } });
			}
			result.StandardError = std::string{ canary };
			return result;
		}
		void cancel() noexcept override { ++Cancels; }

		application::agent_run_outcome                 Outcome  = application::agent_run_outcome::success;
		std::optional<int>                             ExitCode = 0;
		int                                            Lines    = 2;
		int                                            Cancels  = 0;
		std::vector<application::agent_launch_request> Requests;
	};

	void test_mcp_helper()
	{
		auto log       = recording_log{ };
		auto processes = scripted_processes{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto helper    = application::mcp_context_helper_supervisor{ "/opt/" + std::string{ canary } + "/helper", processes, effects, decisions, &log };

		expect(helper.start(std::string{ canary }) == application::mcp_helper_refusal::not_pinned, "an unpinned helper is refused");
		expect(log.Events.size() == 1U && log.Events[0].Outcome == diagnostic_outcome::failed
				   && log.line(0U) == "action=mcp_helper_request failure=not_pinned process=none exit=none duration_ms=0 count=0 written=0 read_back=0",
			   "a refusal records its typed failure and nothing it was asked");

		helper.pin("/snapshots/" + std::string{ canary }, "binding " + std::string{ canary });
		expect(helper.start("{\"request\":\"" + std::string{ canary } + "\"}") == application::mcp_helper_refusal::none, "a pinned helper starts");
		expect(log.Events.size() == 2U && log.Events[1].Outcome == diagnostic_outcome::started && log.line(1U).starts_with("action=mcp_helper_request failure=none process=none "), "a start is recorded before the run");
		expect(helper.start("again") == application::mcp_helper_refusal::already_running && log.last().starts_with("action=mcp_helper_request failure=already_running "), "a second start is refused and recorded");
		effects.drain();
		decisions.drain();
		expect(processes.Requests.size() == 1U && helper.answer().find(canary) != std::string::npos, "the run and its answer are unchanged");
		const auto ended = core::parse_operation_diagnosis(log.last());
		expect(log.Events.back().Outcome == diagnostic_outcome::succeeded && ended && ended->Process == core::operation_process::success && ended->ExitCode == 0 && ended->Count == 2U,
			   "a finished run records its process outcome, exit code and how many lines it answered");

		processes.Outcome  = application::agent_run_outcome::nonzero_exit;
		processes.ExitCode = 3;
		processes.Lines    = 0;
		expect(helper.start("{}") == application::mcp_helper_refusal::none, "a second request starts");
		effects.drain();
		decisions.drain();
		expect(log.Events.back().Outcome == diagnostic_outcome::failed && log.last().starts_with("action=mcp_helper_request failure=helper_failed process=nonzero_exit exit=3 "), "a failed run records its exit code");

		processes.Outcome  = application::agent_run_outcome::cancelled;
		processes.ExitCode = std::nullopt;
		expect(helper.start("{}") == application::mcp_helper_refusal::none, "a third request starts");
		effects.drain();
		decisions.drain();
		expect(log.Events.back().Outcome == diagnostic_outcome::cancelled && log.last().starts_with("action=mcp_helper_request failure=cancelled process=cancelled exit=none "), "a stopped run is a cancellation");

		processes.Outcome = application::agent_run_outcome::timeout;
		expect(helper.start("{}") == application::mcp_helper_refusal::none, "a fourth request starts");
		effects.drain();
		decisions.drain();
		expect(log.last().starts_with("action=mcp_helper_request failure=timed_out process=timeout "), "a run past its bound timed out");
		expect_private(log, diagnostic_operation::mcp_helper);

		const auto recorded = log.Events.size();
		auto       unlogged = application::mcp_context_helper_supervisor{ "/opt/helper", processes, effects, decisions };
		expect(unlogged.start("{}") == application::mcp_helper_refusal::not_pinned && log.Events.size() == recorded, "a supervisor without a log records nothing and behaves the same");
	}
#endif

	class empty_store final : public application::review_tour_store
	{
	public:
		auto load(const core::tour_owner&, core::tour_draft_id, std::stop_token) -> application::tour_store_result<std::optional<core::review_tour>> override { return Saved; }
		auto save(const core::review_tour& aDraft, std::uint64_t, std::stop_token) -> application::tour_store_result<core::review_tour> override
		{
			Saved = aDraft;
			return aDraft;
		}
		std::optional<core::review_tour> Saved;
	};

	void test_tour_import()
	{
		auto log       = recording_log{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto published = tests::review_tour_fixture();
		auto seed      = core::review_tour{ .Id = published.Id, .Owner = published.Owner, .Base = published.Base, .Head = published.Head };
		auto host      = application::review_tour_host{ seed, std::make_unique<empty_store>(), effects, decisions, { } };
		host.set_diagnostics(&log);
		host.start();
		effects.drain();
		decisions.drain();

		published.Problem = std::string{ canary };
		auto other        = published;
		other.Head        = *core::parse_git_commit_oid("2222222222222222222222222222222222222222");
		host.observe_imports({ { .Label = std::string{ canary }, .Draft = published }, { .Label = "/" + std::string{ canary }, .Draft = other } });
		expect(log.Events.size() == 1U && log.Events[0].Outcome == diagnostic_outcome::succeeded && log.line(0U).starts_with("action=tour_observe_imports failure=none ")
				   && core::parse_operation_diagnosis(log.line(0U))->Count == 2U,
			   "the offered tours are counted, never labelled");

		host.import_selected(7U);
		expect(log.Events.back().Outcome == diagnostic_outcome::failed && log.last().starts_with("action=tour_import failure=not_offered "), "an import nobody offered fails as not offered");
		host.import_selected(1U);
		expect(log.last().starts_with("action=tour_import failure=target_mismatch "), "a tour of another head is refused");
		host.import_selected(0U);
		const auto imported = core::parse_operation_diagnosis(log.last());
		expect(log.Events.back().Outcome == diagnostic_outcome::succeeded && imported && imported->Action == core::operation_action::tour_import && imported->Count == 1U
				   && host.presentation().Draft->Problem == canary,
			   "an import records its success and the offered stop count");
		host.import_selected(0U);
		expect(log.last().starts_with("action=tour_import failure=draft_exists "), "a second import cannot replace the private draft");

		auto request        = application::tour_service_request{ };
		request.Operation   = application::tour_service_operation::import_selected;
		request.ImportIndex = 9U;
		request.Command     = { .Draft = host.presentation().Draft->Id, .Owner = host.presentation().Draft->Owner, .ExpectedRevision = host.presentation().Draft->Revision };
		expect(host.execute(request).Status == application::tour_service_status::refused && log.last().starts_with("action=tour_import failure=not_offered "), "a terminal's import of nothing offered fails too");
		expect_private(log, diagnostic_operation::tour_operation);
		host.shutdown();
		effects.drain();
		decisions.drain();
	}

	void test_failure_names()
	{
		using failure = core::operation_failure;
		expect(application::tour_cli_failure("cancelled", 130) == failure::cancelled && application::tour_cli_failure("authentication_required", 5) == failure::authentication_unavailable
				   && application::tour_cli_failure("transport_unavailable", 5) == failure::transport_unavailable && application::tour_cli_failure("remote_incomplete", 5) == failure::remote_incomplete
				   && application::tour_cli_failure("storage_unavailable", 7) == failure::storage_unavailable,
			   "a tour error's code names its class");
		expect(application::tour_cli_failure(canary, 2) == failure::invalid_input && application::tour_cli_failure(canary, 3) == failure::interaction_required && application::tour_cli_failure(canary, 4) == failure::conflict
				   && application::tour_cli_failure(canary, 5) == failure::unavailable && application::tour_cli_failure(canary, 6) == failure::outcome_unknown && application::tour_cli_failure(canary, 7) == failure::local_failure
				   && application::tour_cli_failure(canary, 99) == failure::local_failure,
			   "otherwise its exit status does, and an unknown code is never copied");
		expect(application::operation_failure_of(application::cycle_store_failure::quota) == failure::quota && application::operation_failure_of(application::cycle_store_failure::cancelled) == failure::cancelled
				   && application::operation_failure_of(application::cycle_store_failure::store_damaged) == failure::store_damaged,
			   "a cycle store failure is named");
		expect(application::operation_failure_of(core::cycle_authorization::allowed) == failure::none && application::operation_failure_of(core::cycle_authorization::revoked) == failure::revoked
				   && application::operation_failure_of(core::cycle_authorization::channel_denied) == failure::not_authorized,
			   "a grant's authorization is named");
		expect(application::diagnostic_operation_of(core::operation_action::mcp_helper_request) == diagnostic_operation::mcp_helper
				   && application::diagnostic_operation_of(core::operation_action::tour_publish) == diagnostic_operation::tour_operation
				   && application::diagnostic_operation_of(core::operation_action::cycle_round_grant) == diagnostic_operation::cycle_round,
			   "each new action is logged on its family's operation");
		const auto category = [](const diagnostic_operation aOperation) { return application::diagnostic_category(application::diagnostic_operation_names[static_cast<std::size_t>(aOperation)]); };
		expect(category(diagnostic_operation::mcp_helper) == "agent" && category(diagnostic_operation::tour_operation) == "tour" && category(diagnostic_operation::cycle_round) == "cycle", "and filed under its own category");
	}
} // namespace

int main()
{
#if defined(BPR_TEST_MCP_SUPERVISOR)
	test_mcp_helper();
#endif
	test_tour_import();
	test_failure_names();
	std::cout << "helper-tour-round-diagnostics-tests:pass\n";
	return EXIT_SUCCESS;
}
