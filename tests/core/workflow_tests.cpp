/// \file
/// \brief RFC-BPR-0112 ([[BPR-2032]], [[BPR-2033]], [[BPR-2034]]). Optional
/// workflows without any network or program: the shared file and its
/// version, import choices and local bindings, a draft's explicit
/// selection, the companion's bounded request and answer, honest check
/// status, and the interactive publication handoff with its recovery.

#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_publication.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/workflow_action.hpp"
#include "bpr/core/workflow_configuration.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename T>
	auto ok(T aResult, const std::string_view aMessage)
	{
		expect(aResult.has_value(), aMessage);
		return std::move(*aResult);
	}

	auto commit(const char aDigit) -> core::git_commit_oid { return *core::parse_git_commit_oid(std::string(40U, aDigit)); }
	auto entry_oid(const char aDigit) -> core::git_entry_oid { return *core::parse_git_entry_oid(std::string(40U, aDigit)); }

	auto candidate(const char aHead) -> core::local_draft_candidate
	{
		return { .Head      = commit(aHead),
				 .TargetTip = commit('b'),
				 .Base      = commit('c'),
				 .Changes   = { { .Path   = "src/search.cpp",
								  .Change = core::draft_change::modified,
								  .Left   = core::draft_side{ .Mode = 0100644, .Object = entry_oid('2') },
								  .Right  = core::draft_side{ .Mode = 0100644, .Object = entry_oid('3') } } } };
	}

	auto make_draft(const char aId = '1') -> core::local_draft
	{
		auto draft = ok(core::create_local_draft({ }, { std::string(32U, aId) },
												 { .Location = "/work/search", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } }, "feature/search",
												 { .Target = { .Remote = "origin", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" } }, { },
												 candidate('a'), 100),
						"a draft is created");
		return ok(core::edit_description(draft, draft.Version, "Cancel searches promptly", "Searches kept running.\n", 101), "the description saves");
	}

	/// A synthetic organization's workflow: context, a description proposal
	/// by an agent, a required template check, its publisher and a follow-up.
	auto organization() -> core::workflow_configuration
	{
		const auto command = [](std::string aId, const core::workflow_slot aSlot, std::string aTask, std::vector<core::workflow_effect> aEffects, const bool aRequired = false) {
			return core::workflow_action{ .Id          = std::move(aId),
										  .Slot        = aSlot,
										  .Task        = std::move(aTask),
										  .Binding     = core::workflow_binding::command,
										  .ProgramName = "org-companion",
										  .Arguments   = { "--request", "{request}", "--response", "{response}" },
										  .Effects     = std::move(aEffects),
										  .Required    = aRequired };
		};
		return { .PortableId = "synthetic-org",
				 .Name       = "Synthetic Org pull request",
				 .Origin     = "Synthetic Org",
				 .Guidance   = "Say what changed and why.",
				 .Actions    = { command("context", core::workflow_slot::gather_context, "Gather ticket context", { core::workflow_effect::read_context }),
								 core::workflow_action{ .Id      = "describe",
														.Slot    = core::workflow_slot::propose_description,
														.Task    = "Draft the description",
														.Binding = core::workflow_binding::agent,
														.Agent   = "writer",
														.Prompt  = "Draft a description in the house style.",
														.Effects = { core::workflow_effect::read_context, core::workflow_effect::write_proposal } },
								 command("template", core::workflow_slot::check_preparation, "Check the template", { core::workflow_effect::read_context, core::workflow_effect::run_check }, true),
								 command("publish", core::workflow_slot::publish_draft, "Create the pull request", { core::workflow_effect::publish_pull_request, core::workflow_effect::change_ticket }),
								 command("follow", core::workflow_slot::finish_publication, "Link the ticket", { core::workflow_effect::change_ticket }) } };
	}

	auto file_with_workflow() -> core::agent_configuration
	{
		return { .Label      = "Org",
				 .ExportedBy = "lead",
				 .Agents     = { core::portable_agent{ .PortableId = "writer", .Name = "Writer", .Protocol = core::agent_protocol::claude_stream_json, .Preset = core::setup_agent::claude_code } },
				 .Prompts    = { },
				 .Workflows  = { organization() } };
	}

	void test_file_version_and_problems()
	{
		const auto bytes = ok(core::encode_agent_configuration(file_with_workflow()), "a file with a workflow encodes");
		expect(bytes.find("\"version\": 2") != std::string::npos, "a file with a workflow is version 2, refused as newer by an importer from before workflows");
		const auto plain = ok(core::encode_agent_configuration({ .Label = "x", .ExportedBy = "y", .Agents = file_with_workflow().Agents }), "a file without one");
		expect(plain.find("\"version\": 1") != std::string::npos && plain.find("workflows") == std::string::npos, "a file with no workflow stays version 1");
		const auto decoded = ok(core::decode_agent_configuration(bytes, { }), "it reads back");
		expect(decoded.Workflows.size() == 1U && !decoded.Workflows[0].Problem && decoded.Workflows[0].Workflow == organization(), "the workflow reads back exactly");
		expect(bytes.find("/") == std::string::npos || bytes.find("\"program\"") == std::string::npos, "no program path travels");

		auto unknown = bytes;
		unknown.replace(unknown.find("\"check_preparation\""), 19U, "\"sign_off_by_robot\"");
		const auto withUnknown = ok(core::decode_agent_configuration(unknown, { }), "an unknown slot does not refuse the file");
		expect(withUnknown.Workflows[0].Problem == core::workflow_problem::unsupported && withUnknown.Workflows[0].Unsupported == "sign_off_by_robot",
			   "an unknown action is named, never accepted as inert");
		expect(withUnknown.Agents.size() == 1U && !withUnknown.Agents[0].Problem, "the rest of the file still imports");
		const auto refused = core::apply_workflow_choices({ }, withUnknown.Workflows, std::vector{ core::workflow_import_choice{ .Item = 0U, .Action = core::workflow_import_action::add } });
		expect(!refused && refused.error().Refusal == core::workflow_apply_refusal::unusable_item, "the unsupported selection is refused");

		auto agentPublishes                         = file_with_workflow();
		agentPublishes.Workflows[0].Actions[1].Slot = core::workflow_slot::publish_draft;
		expect(!core::encode_agent_configuration(agentPublishes), "an agent cannot be bound to publication");
		auto noAgent = file_with_workflow();
		noAgent.Agents.clear();
		expect(!core::encode_agent_configuration(noAgent), "an agent action needs the agent in the file");
		auto version1 = bytes;
		version1.replace(version1.find("\"version\": 2"), 12U, "\"version\": 1");
		expect(core::decode_agent_configuration(version1, { }).error() == core::configuration_file_refusal::malformed, "workflows at version 1 are malformed");
	}

	void test_apply_choices_and_bindings()
	{
		const auto decoded        = ok(core::decode_agent_configuration(*core::encode_agent_configuration(file_with_workflow()), { }), "decoded").Workflows;
		auto       other          = core::local_workflow{ .Workflow = organization() };
		other.Workflow.PortableId = "unrelated";
		const auto bound          = core::workflow_import_choice{ .Item     = 0U,
																  .Action   = core::workflow_import_action::add,
																  .Programs = { { .Action = "template", .Program = "/opt/org/bin/org-companion" } },
																  .Agents   = { { .Action = "describe", .AgentName = "Writer" } } };
		const auto added          = ok(core::apply_workflow_choices(std::vector{ other }, decoded, std::vector{ bound }), "add applies");
		expect(added.size() == 2U && added[0] == other, "the unrelated workflow is untouched");
		expect(core::bound_program(added[1], "template")->Program == "/opt/org/bin/org-companion" && core::bound_program(added[1], "context") == nullptr, "only what was bound is bound");
		const auto& action = added[1].Workflow.Actions[0];
		expect(core::action_prerequisite(added[1], action, false, false) == core::workflow_prerequisite::program_unbound, "an unbound command waits for its program");
		expect(core::action_prerequisite(added[1], added[1].Workflow.Actions[1], false, true) == core::workflow_prerequisite::ready, "a bound agent is ready");
		expect(core::action_prerequisite(added[1], added[1].Workflow.Actions[2], false, false) == core::workflow_prerequisite::program_missing, "a vanished program is missing");

		expect(core::apply_workflow_choices(added, decoded, std::vector{ bound }).error().Refusal == core::workflow_apply_refusal::conflict, "add over an existing ID is a conflict");
		auto relative                = bound;
		relative.Programs[0].Program = "bin/org-companion";
		expect(core::apply_workflow_choices({ }, decoded, std::vector{ relative }).error().Refusal == core::workflow_apply_refusal::invalid_binding, "a relative program is refused");

		auto changed = decoded;
		changed[0].Workflow.Actions[0].Effects.push_back(core::workflow_effect::change_ticket);
		const auto differences = core::workflow_differences(added[1].Workflow, changed[0].Workflow);
		expect(std::ranges::find(differences, core::workflow_difference{ .Change = core::workflow_change::effects_widened, .Action = "context" }) != differences.end(),
			   "the preview names a widened effect");
		const auto replaced = ok(core::apply_workflow_choices(added, changed, std::vector{ core::workflow_import_choice{ .Item = 0U, .Action = core::workflow_import_action::replace } }),
								 "replace applies");
		expect(core::bound_program(replaced[1], "template") != nullptr, "an unchanged program name keeps its binding on this computer");
		expect(core::apply_workflow_choices(added, decoded, std::vector{ core::workflow_import_choice{ .Item = 0U, .Action = core::workflow_import_action::leave } }).value() == added,
			   "leave changes nothing");

		const auto command = core::workflow_command("/opt/org/bin/org companion", added[1].Workflow.Actions[0].Arguments, "/tmp/req.json", "/tmp/resp.json");
		expect((command == std::vector<std::string>{ "/opt/org/bin/org companion", "--request", "/tmp/req.json", "--response", "/tmp/resp.json" }), "whole arguments are substituted");
		expect(core::shell_line(std::vector<std::string>{ "a b", "it's" }) == "'a b' 'it'\\''s'", "the pasted line quotes every element");
		const auto text = core::encode_local_workflow(replaced[1]);
		expect(core::decode_local_workflow(text) == replaced[1], "a local workflow stores and reads back");
	}

	auto selected(core::local_draft aDraft) -> core::local_draft
	{
		return ok(core::select_workflow(aDraft, aDraft.Version, organization(), 102), "the author selects the workflow");
	}

	auto answer(const core::local_draft& aDraft, const std::string& aAction, const core::workflow_outcome aOutcome) -> core::workflow_response
	{
		return { .Invocation = core::find_record(*aDraft.Workflow, aAction)->Invocation, .Draft = aDraft.Id.Value, .Action = aAction, .Outcome = aOutcome };
	}

	void test_selection_and_companion_contract()
	{
		const auto unrelated = make_draft('2');
		auto       draft     = selected(make_draft());
		expect(!unrelated.Workflow && draft.Workflow->Revision == core::workflow_revision(organization()), "only the selected draft pins the revision");
		expect(draft.Workflow->Actions.empty(), "selecting runs nothing");

		draft              = ok(core::begin_action(draft, draft.Version, "context", { }, 103), "the context action is handed off");
		const auto request = ok(core::encode_workflow_request(draft, "context"), "its request");
		expect(request.find("\"format\": \"bpr.workflow-request\"") != std::string::npos && request.find("\"invocation\": 1") != std::string::npos
				   && request.find("Searches kept running.") != std::string::npos && request.find("src/search.cpp") != std::string::npos
				   && request.find("\"plan\"") == std::string::npos,
			   "the request carries the draft, candidate and selected context, and no plan");
		auto asked     = answer(draft, "context", core::workflow_outcome::input_request);
		asked.Question = "Which ticket?";
		draft          = ok(core::admit_workflow_response(draft, draft.Version, asked, 104), "an input request is admitted");
		expect(core::find_record(*draft.Workflow, "context")->State == core::workflow_action_state::awaiting_input, "the action waits for the author");

		// Unrelated preparation continues while it waits.
		draft                      = ok(core::begin_action(draft, draft.Version, "template", { }, 105), "a check is handed off meanwhile");
		const auto checkInvocation = core::find_record(*draft.Workflow, "template")->Invocation;
		draft                      = ok(core::begin_action(draft, draft.Version, "context", "ENG-42", 106), "Continue sends the answer");
		expect(core::find_record(*draft.Workflow, "template")->Invocation == checkInvocation, "continuing reruns no other action");
		const auto continued = *core::encode_workflow_request(draft, "context");
		expect(continued.find("\"answer\": \"ENG-42\"") != std::string::npos && continued.find("Which ticket?") != std::string::npos, "the answer and its question travel");
		expect(core::begin_action(draft, draft.Version, "template", "x", 107).error() == core::workflow_refusal::state, "an answer only continues a question");

		auto proposed      = answer(draft, "context", core::workflow_outcome::proposal);
		proposed.Proposals = { { .Target = core::local_proposal_target::description, .Text = "Fixes ENG-42: searches stop promptly.\n" } };
		const auto late    = draft;
		// The author edits the description while the companion works.
		draft                = ok(core::edit_description(draft, draft.Version, "Cancel searches promptly", "My own words.\n", 108), "the author edits");
		draft                = ok(core::admit_workflow_response(draft, draft.Version, proposed, 109), "the proposal is admitted");
		const auto& proposal = draft.Preparation.Proposals.back();
		expect(proposal.Producer == "Synthetic Org pull request: Gather ticket context" && core::proposal_stale(draft, proposal), "a proposal on an edited field is stale, never applied");
		expect(core::accept_proposal(draft, draft.Version, proposal.Id, 110).error() == core::preparation_refusal::proposal_stale, "accept refuses it");
		expect(draft.Preparation.Description.Body == "My own words.\n", "the author's text stands");
		expect(core::admit_workflow_response(draft, draft.Version, proposed, 111).error() == core::workflow_refusal::state, "an answer is admitted once");
		auto old       = answer(late, "context", core::workflow_outcome::completed);
		old.Invocation = 1U;
		expect(core::admit_workflow_response(draft, draft.Version, old, 111).error() == core::workflow_refusal::mismatched, "an earlier invocation's answer is refused");

		const auto encoded = core::encode_local_draft(draft);
		expect(core::decode_local_draft(encoded) == draft, "the draft with its workflow stores and reads back");
		expect(core::decode_workflow_response(std::string(core::maximum_workflow_response_bytes + 1U, ' ')).error() == core::workflow_response_refusal::too_large, "16 MiB bounds an answer");
		expect(core::decode_workflow_response(R"({"format":"bpr.workflow-response","version":1,"invocation":1,"draft":"x","action":"a","outcome":"proposal","signature":"ok"})").error()
				   == core::workflow_response_refusal::malformed,
			   "an unknown field is not ignored");
	}

	auto met_check(core::local_draft aDraft, const std::string& aFingerprint = "rules-1") -> core::local_draft
	{
		aDraft           = ok(core::begin_action(aDraft, aDraft.Version, "template", { }, 120), "the check is handed off");
		auto checked     = answer(aDraft, "template", core::workflow_outcome::findings);
		checked.Findings = { { .Check = "template", .State = core::workflow_finding_state::met } };
		checked.Upstream = core::workflow_upstream{ .Name = "org-create-pr", .Fingerprint = aFingerprint };
		return ok(core::admit_workflow_response(aDraft, aDraft.Version, checked, 121), "its findings are admitted");
	}

	void test_honest_checks()
	{
		auto draft = selected(make_draft());
		expect(core::required_checks(draft).front().Gap == core::check_gap::never_run, "a check never run is unmet");
		draft = met_check(draft);
		expect(!core::required_checks(draft).front().Gap, "met on the current candidate, revision and upstream");

		auto marker         = ok(core::begin_action(draft, draft.Version, "template", { }, 122), "again");
		auto limited        = answer(marker, "template", core::workflow_outcome::findings);
		limited.Findings    = { { .Check = "template", .State = core::workflow_finding_state::met } };
		limited.Limitations = { { .Kind = core::workflow_limitation_kind::review_marker, .Detail = "the upstream self-review marker" } };
		marker              = ok(core::admit_workflow_response(marker, marker.Version, limited, 123), "a limitation is admitted");
		expect(core::required_checks(marker).front().Gap == core::check_gap::limitation, "a review marker only upstream writes stays unmet");

		auto upstream  = ok(core::begin_action(draft, draft.Version, "context", { }, 124), "context");
		auto newer     = answer(upstream, "context", core::workflow_outcome::completed);
		newer.Upstream = core::workflow_upstream{ .Name = "org-create-pr", .Fingerprint = "rules-2" };
		upstream       = ok(core::admit_workflow_response(upstream, upstream.Version, newer, 125), "a later answer reports changed rules");
		expect(core::required_checks(upstream).front().Gap == core::check_gap::upstream_changed, "changed upstream rules need the check again");

		auto refreshed = ok(core::add_candidate(draft, draft.Version, candidate('d'), 126), "a refreshed candidate");
		expect(core::required_checks(refreshed).front().Gap == core::check_gap::earlier_candidate, "a check on an earlier candidate is unmet");
		auto updatedWorkflow = organization();
		updatedWorkflow.Guidance += " Keep it short.";
		auto updated = ok(core::accept_workflow_update(draft, draft.Version, updatedWorkflow, 127), "the author accepts the update");
		expect(core::required_checks(updated).front().Gap == core::check_gap::earlier_configuration, "a check under the earlier revision is unmet");
	}

	auto observed() -> core::local_publication_observation
	{
		return { .Host = "github.com", .Account = core::github_account_id{ 7U }, .Login = "author", .BranchObserved = true, .BranchHead = commit('a'), .TargetTip = commit('b') };
	}

	auto pull() -> core::local_publication_link
	{
		return { .Number     = 42U,
				 .Url        = "https://github.com/acme/engine/pull/42",
				 .NodeId     = "PR_x",
				 .Owner      = "acme",
				 .Repository = "engine",
				 .HeadBranch = "feature/search",
				 .Head       = commit('a'),
				 .BaseBranch = "main",
				 .Author     = "author",
				 .Draft      = true };
	}

	void test_publication_handoff()
	{
		auto draft        = met_check(selected(make_draft()));
		auto plan         = ok(core::make_local_publication(draft, observed(), false), "a plan");
		draft             = ok(core::record_local_publication(draft, draft.Version, std::move(plan), 130), "recorded");
		const auto digest = draft.Publication->Journal.Digest;
		expect(core::hand_off_publication(draft, draft.Version, "publish", digest, 131).error() == core::workflow_refusal::other_publisher, "the built-in publisher is the default");
		draft = ok(core::choose_publisher(draft, draft.Version, core::workflow_publisher::external, 131), "the author chooses the workflow's publisher");
		expect(core::authorize_local_publication(draft, draft.Version, digest, core::github_account_id{ 7U }, 132).error() == core::local_publication_refusal::external_publisher,
			   "the built-in route stands aside: one publisher");
		expect(core::hand_off_publication(draft, draft.Version, "publish", "0000", 132).error() == core::workflow_refusal::no_plan, "only the accepted plan's digest");

		draft              = ok(core::hand_off_publication(draft, draft.Version, "publish", digest, 133), "the plan is handed off");
		const auto request = *core::encode_workflow_request(draft, "publish");
		expect(request.find("\"digest\": \"" + digest + "\"") != std::string::npos && request.find("\"title\": \"Cancel searches promptly\"") != std::string::npos,
			   "the publisher receives the exact accepted plan");
		expect(core::external_publication_open(draft), "the handoff is open");
		expect(core::select_workflow(draft, draft.Version, std::nullopt, 134).error() == core::workflow_refusal::external_open, "the selection cannot change under it");
		expect(core::choose_publisher(draft, draft.Version, core::workflow_publisher::built_in, 134).error() == core::workflow_refusal::external_open, "nor the publisher");

		// The answer was lost: uncertain, and only a read settles it.
		draft = ok(core::mark_action_lost(draft, draft.Version, "publish", 135), "the author records a lost answer");
		expect(core::hand_off_publication(draft, draft.Version, "publish", digest, 136).error() == core::workflow_refusal::external_open, "no replay before reconciliation");
		auto none = ok(core::reconcile_external_publication(draft, draft.Version, "publish", std::nullopt, 137), "GitHub shows none");
		expect(!core::external_publication_open(none), "reconciled with nothing found");
		none                = ok(core::hand_off_publication(none, none.Version, "publish", digest, 138), "a new confirmed handoff");
		auto reported       = answer(none, "publish", core::workflow_outcome::completed);
		reported.References = { { .Kind = core::workflow_reference_kind::pull_request, .State = core::workflow_reference_state::created, .Id = "42" } };
		none                = ok(core::admit_workflow_response(none, none.Version, reported, 139), "the publisher reports a pull request");
		expect(core::external_publication_open(none) && !none.Publication->Journal.Link, "a report alone does not link the draft");
		auto drifted       = pull();
		drifted.BaseBranch = "release";
		expect(core::reconcile_external_publication(none, none.Version, "publish", drifted, 140).error() == core::workflow_refusal::mismatched, "a pull request into another base is not ours");
		auto linked = ok(core::reconcile_external_publication(none, none.Version, "publish", pull(), 140), "the read finds it");
		expect(linked.Publication->Journal.Phase == core::local_publication_phase::linked && linked.Publication->Journal.Link->Adopted, "linked as one BPR did not create");

		// A follow-up fails partway; its results stay and a retry waits for reconciliation.
		linked             = ok(core::hand_off_follow_up(linked, linked.Version, "follow", 141), "the follow-up is handed off");
		auto partial       = answer(linked, "follow", core::workflow_outcome::completed);
		partial.References = { { .Kind = core::workflow_reference_kind::ticket, .State = core::workflow_reference_state::updated, .Id = "ENG-42" },
							   { .Kind = core::workflow_reference_kind::visual, .State = core::workflow_reference_state::failed, .Id = "screenshot", .Detail = "upload refused" } };
		linked             = ok(core::admit_workflow_response(linked, linked.Version, partial, 142), "partial results are admitted");
		expect(core::hand_off_follow_up(linked, linked.Version, "follow", 143).error() == core::workflow_refusal::state, "a failed follow-up is reconciled before a retry");
		expect(linked.Publication->Journal.Phase == core::local_publication_phase::linked, "a follow-up failure leaves the pull request as it is");
		linked           = ok(core::reconcile_follow_up(linked, linked.Version, "follow", 144), "the author reconciles");
		linked           = ok(core::hand_off_follow_up(linked, linked.Version, "follow", 145), "then it may be handed off again");
		const auto retry = *core::encode_workflow_request(linked, "follow");
		expect(retry.find("\"previous\"") != std::string::npos && retry.find("ENG-42") != std::string::npos && retry.find("\"number\": 42") != std::string::npos,
			   "the retry carries what was already made so nothing is made twice");
		expect(core::decode_local_draft(core::encode_local_draft(linked)) == linked, "the publication handoff stores and reads back");
	}
} // namespace

auto main() -> int
{
	test_file_version_and_problems();
	test_apply_choices_and_bindings();
	test_selection_and_companion_contract();
	test_honest_checks();
	test_publication_handoff();
	std::cout << "workflow-tests:pass\n";
	return EXIT_SUCCESS;
}
