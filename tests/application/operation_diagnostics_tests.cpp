/// \file
/// \brief [[BPR-1828]]. Review mutations and agent workspaces leave
/// diagnostic records: each wrapped provider call records its start and end
/// with its typed failure, duration and write flags, its answer passes through
/// unchanged, and no thread, body, login or query reaches a record.

#include "bpr/application/operation_diagnostics.hpp"
#include "bpr/application/review_mutation_diagnostics.hpp"

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
	using application::review_mutation_failure;

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

		std::vector<application::diagnostic_event> Events;
	};

	/// Answers each call from a script; counts what reached it.
	class scripted_provider final : public application::review_mutation_port
	{
	public:
		[[nodiscard]] auto observe_target() -> std::expected<core::review_mutation_target, review_mutation_failure> override
		{
			++Calls;
			return std::unexpected(review_mutation_failure::target_moved);
		}
		[[nodiscard]] auto submit(const application::review_mutation_request&) -> std::expected<application::review_mutation_receipt, review_mutation_failure> override
		{
			++Calls;
			return std::unexpected(review_mutation_failure::unplaceable_line);
		}
		[[nodiscard]] auto submit_and_reconcile(const application::review_mutation_request&, std::stop_token) -> application::review_mutation_submission override
		{
			++Calls;
			auto submission              = application::review_mutation_submission{ };
			submission.Result            = application::review_mutation_receipt{ .ServerCommentId = std::string{ canary } };
			submission.Reconciliation    = std::vector<application::review_comment_projection>(3U);
			submission.MutationAttempted = true;
			return submission;
		}
		[[nodiscard]] auto resolve_thread(const application::review_thread_resolution_request& aRequest, std::stop_token)
			-> std::expected<application::review_thread_resolution_receipt, review_mutation_failure> override
		{
			++Calls;
			if (!aRequest.Resolve) {
				return std::unexpected(review_mutation_failure::refused_by_server);
			}
			return application::review_thread_resolution_receipt{ .Thread = aRequest.Thread, .Resolved = true };
		}
		[[nodiscard]] auto set_reaction(const application::review_reaction_request&, std::stop_token) -> std::expected<application::review_reaction_receipt, review_mutation_failure> override
		{
			++Calls;
			return std::unexpected(review_mutation_failure::cancelled);
		}
		[[nodiscard]] auto edit_description(const application::review_description_request& aRequest, std::stop_token)
			-> std::expected<application::review_description_receipt, review_mutation_failure> override
		{
			++Calls;
			auto receipt     = application::review_description_receipt{ };
			receipt.Body     = aRequest.Body;
			receipt.Conflict = true;
			return receipt;
		}
		[[nodiscard]] auto edit_comment(const application::review_comment_edit_request& aRequest, std::stop_token)
			-> std::expected<application::review_comment_edit_receipt, review_mutation_failure> override
		{
			++Calls;
			auto receipt                = application::review_comment_edit_receipt{ };
			receipt.Body                = aRequest.Body;
			receipt.Written             = true;
			receipt.ConfirmedByReadBack = true;
			return receipt;
		}
		[[nodiscard]] auto reviewers(const application::review_reviewer_request& aRequest, std::stop_token) -> std::expected<application::review_reviewer_receipt, review_mutation_failure> override
		{
			++Calls;
			auto receipt = application::review_reviewer_receipt{ };
			receipt.Answer.Requested.push_back(aRequest.Login);
			receipt.Answer.Requested.emplace_back("second");
			receipt.Written = aRequest.Operation == application::review_reviewer_operation::request;
			return receipt;
		}
		void               repair_pending_publications(std::stop_token) override { ++Calls; }
		[[nodiscard]] auto capabilities() const -> application::review_provider_capabilities override { return { .Reacts = true, .RequestsReviewers = true }; }

		int Calls = 0;
	};

	void expect_private(const recording_log& aLog)
	{
		for (const auto& event : aLog.Events) {
			const auto text = event.Summary.text();
			expect(text.find(canary) == std::string_view::npos && text.find("second") == std::string_view::npos, "no thread, body, login or query reaches a record");
			expect(core::parse_operation_diagnosis(text).has_value(), "every record is exactly an operation line");
			expect(event.Operation == diagnostic_operation::review_mutation && event.Request == 0U && event.WindowSession == 0U, "on the review mutation operation, with no identity");
		}
	}

	void test_review_mutations()
	{
		auto        log      = std::make_shared<recording_log>();
		auto        owned    = std::make_unique<scripted_provider>();
		auto* const provider = owned.get();
		auto        port     = application::with_review_mutation_diagnostics(std::move(owned), log);
		expect(port != nullptr && port.get() != provider, "a log wraps the provider");
		expect(port->capabilities().Reacts && port->capabilities().RequestsReviewers, "capabilities pass through");

		auto resolve        = application::review_thread_resolution_request{ };
		resolve.Thread      = std::string{ canary };
		const auto resolved = port->resolve_thread(resolve, { });
		expect(resolved && resolved->Thread == canary, "a resolution's answer passes through unchanged");
		expect(log->Events.size() == 2U && log->Events[0].Outcome == diagnostic_outcome::started && log->Events[1].Outcome == diagnostic_outcome::succeeded, "a resolve records its start and its success");
		expect(log->line(0U) == "action=resolve_thread failure=none process=none exit=none duration_ms=0 count=0 written=0 read_back=0", "the start names its action only");
		const auto ended = core::parse_operation_diagnosis(log->line(1U));
		expect(ended && ended->Action == core::operation_action::resolve_thread && ended->Written, "the end says the resolution was taken");

		resolve.Resolve = false;
		expect(!port->resolve_thread(resolve, { }), "an unresolve's refusal passes through");
		expect(log->Events.size() == 4U && log->Events[3].Outcome == diagnostic_outcome::failed, "an unresolve's refusal is a failure");
		expect(log->line(3U).starts_with("action=unresolve_thread failure=refused_by_server "), "with its typed failure");

		auto reaction    = application::review_reaction_request{ };
		reaction.Subject = std::string{ canary };
		expect(!port->set_reaction(reaction, { }), "a cancelled reaction passes through");
		expect(log->Events.back().Outcome == diagnostic_outcome::cancelled && log->line(log->Events.size() - 1U).starts_with("action=add_reaction failure=cancelled "), "a cancelled reaction records a cancellation");

		auto description  = application::review_description_request{ };
		description.Body  = std::string{ canary };
		const auto edited = port->edit_description(description, { });
		expect(edited && edited->Conflict && edited->Body == canary, "a description conflict passes through");
		expect(log->Events.back().Outcome == diagnostic_outcome::failed && log->line(log->Events.size() - 1U).starts_with("action=edit_description failure=conflict "), "a conflict is a failure that wrote nothing");

		auto comment = application::review_comment_edit_request{ };
		comment.Body = std::string{ canary };
		expect(port->edit_comment(comment, { }).has_value(), "a comment edit passes through");
		const auto commentLine = core::parse_operation_diagnosis(log->line(log->Events.size() - 1U));
		expect(commentLine && commentLine->Written && commentLine->ReadBack && log->Events.back().Outcome == diagnostic_outcome::succeeded, "an edit learned by read back says so");

		auto reviewer      = application::review_reviewer_request{ };
		reviewer.Operation = application::review_reviewer_operation::request;
		reviewer.Login     = std::string{ canary };
		reviewer.Query     = std::string{ canary };
		const auto before  = log->Events.size();
		expect(port->reviewers(reviewer, { }).has_value(), "a reviewer request passes through");
		expect(log->Events.size() == before + 2U && log->line(before + 1U).starts_with("action=request_reviewer failure=none ") && core::parse_operation_diagnosis(log->line(before + 1U))->Count == 2U,
			   "a reviewer request records its start and its end, counting the requested reviewers");
		reviewer.Operation = application::review_reviewer_operation::search;
		const auto search  = log->Events.size();
		expect(port->reviewers(reviewer, { }).has_value(), "a reviewer search passes through");
		expect(log->Events.size() == search + 1U && log->line(search).starts_with("action=search_reviewers "), "a search, a read, records only its end");

		expect(!port->observe_target(), "a target read passes through");
		expect(log->line(log->Events.size() - 1U).starts_with("action=observe_target failure=target_moved "), "a target read records its end and typed failure");

		const auto publication = port->submit_and_reconcile({ }, { });
		expect(publication.Result && publication.Result->ServerCommentId == canary, "a publication's answer passes through");
		const auto published = core::parse_operation_diagnosis(log->line(log->Events.size() - 1U));
		expect(published && published->Action == core::operation_action::publish_comment && published->Count == 3U && published->Written, "a publication counts what it read back");

		expect(!port->submit({ }), "a refused submission passes through");
		expect(log->line(log->Events.size() - 1U).starts_with("action=publish_comment failure=unplaceable_line "), "with its typed failure");

		port->repair_pending_publications({ });
		expect(log->line(log->Events.size() - 1U).starts_with("action=repair_publications failure=none "), "a repair pass records its end");
		expect(provider->Calls == 11, "every call reached the provider exactly once");
		expect_private(*log);

		auto        unlogged = std::make_unique<scripted_provider>();
		auto* const bare     = unlogged.get();
		expect(application::with_review_mutation_diagnostics(std::move(unlogged), nullptr).get() == bare, "no log leaves the provider as it is");
		expect(application::with_review_mutation_diagnostics(nullptr, log) == nullptr, "no provider stays none");
	}

	void test_failure_names()
	{
		expect(application::operation_failure_of(application::agent_workspace_error{ application::agent_workspace_failure::unavailable, std::string{ canary }, application::agent_workspace_unavailable_reason::sign_in_refused })
				   == core::operation_failure::sign_in_refused,
			   "an unavailable workspace names its reason, not its detail");
		expect(application::operation_failure_of(application::agent_workspace_error{ application::agent_workspace_failure::cleanup_failed }) == core::operation_failure::cleanup_failed, "a cleanup failure is named");
		expect(application::operation_failure_of(core::agent_setup_failure::timed_out) == core::operation_failure::timed_out, "a setup failure is named");
		expect(application::operation_process_of(application::agent_run_outcome::protocol_failure) == core::operation_process::protocol_failure, "a run outcome is named");
		expect(application::diagnostic_operation_of(core::operation_action::workspace_materialize) == diagnostic_operation::agent_workspace
				   && application::diagnostic_operation_of(core::operation_action::agent_custom_check) == diagnostic_operation::agent_setup_check,
			   "each action is logged on its family's operation");
		expect(application::diagnostic_category(application::diagnostic_operation_names[static_cast<std::size_t>(diagnostic_operation::agent_workspace)]) == "agent"
				   && application::diagnostic_category(application::diagnostic_operation_names[static_cast<std::size_t>(diagnostic_operation::review_mutation)]) == "github"
				   && application::diagnostic_category("application") == "application" && application::diagnostic_category("epoch") == "epoch",
			   "categories are the operation's first word");

		auto log = recording_log{ };
		application::record_operation(&log, { .Action = core::operation_action::workspace_materialize, .Failure = core::operation_failure::too_large, .DurationMilliseconds = 7U });
		application::record_operation(&log, { .Action = core::operation_action::workspace_cleanup, .Failure = core::operation_failure::refused_by_server });
		application::record_operation(nullptr, { .Action = core::operation_action::workspace_cleanup });
		expect(log.Events.size() == 1U && log.Events[0].Operation == diagnostic_operation::agent_workspace && log.Events[0].Outcome == diagnostic_outcome::failed
				   && log.line(0U) == "action=workspace_materialize failure=too_large process=none exit=none duration_ms=7 count=0 written=0 read_back=0",
			   "a workspace failure records its line; an invalid line or a missing log records nothing");
	}
} // namespace

int main()
{
	test_review_mutations();
	test_failure_names();
	std::cout << "operation-diagnostics-tests:pass\n";
	return EXIT_SUCCESS;
}
