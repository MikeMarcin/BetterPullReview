/// \file
/// \brief [[BPR-2196]]. Revise, the author's mode, without Qt or a provider:
/// whether it is offered (owned and not owned), the mode a window opens in
/// for each case, the handoff, and the checklist's marks.

#include "bpr/core/author_revise.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/workflow_action.hpp"
#include "bpr/core/workflow_configuration.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::author_mode;
	using core::checklist_mark;
	using core::revise_case;

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

	auto make_draft() -> core::local_draft
	{
		return ok(core::create_local_draft({ }, { std::string(32U, '1') },
										   { .Location = "/work/search", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } }, "feature/search",
										   { .Target = { .Remote = "origin", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" } }, { },
										   candidate('a'), 100),
				  "a draft is created");
	}

	auto organization() -> core::workflow_configuration
	{
		const auto command = [](std::string aId, const core::workflow_slot aSlot, std::string aTask, const bool aRequired = false) {
			return core::workflow_action{ .Id          = std::move(aId),
										  .Slot        = aSlot,
										  .Task        = std::move(aTask),
										  .Binding     = core::workflow_binding::command,
										  .ProgramName = "org-companion",
										  .Arguments   = { "--request", "{request}", "--response", "{response}" },
										  .Effects     = { core::workflow_effect::read_context },
										  .Required    = aRequired };
		};
		return { .PortableId = "synthetic-org",
				 .Name       = "Synthetic Org pull request",
				 .Origin     = "Synthetic Org",
				 .Actions    = { command("context", core::workflow_slot::gather_context, "Gather ticket context"),
								 core::workflow_action{ .Id      = "describe",
														.Slot    = core::workflow_slot::propose_description,
														.Task    = "Draft the description",
														.Binding = core::workflow_binding::agent,
														.Agent   = "writer",
														.Prompt  = "Draft a description.",
														.Effects = { core::workflow_effect::write_proposal } },
								 command("template", core::workflow_slot::check_preparation, "Check the template", true),
								 command("publish", core::workflow_slot::publish_draft, "Create the pull request"),
								 command("follow", core::workflow_slot::finish_publication, "Link the ticket") } };
	}

	auto answer(const core::local_draft& aDraft, const std::string& aAction, const core::workflow_outcome aOutcome) -> core::workflow_response
	{
		return { .Invocation = core::find_record(*aDraft.Workflow, aAction)->Invocation, .Draft = aDraft.Id.Value, .Action = aAction, .Outcome = aOutcome };
	}

	auto row(const core::author_checklist& aList, const std::string_view aId) -> const core::checklist_row&
	{
		const auto found = std::ranges::find(aList.Rows, aId, &core::checklist_row::Id);
		expect(found != aList.Rows.end(), "the row is listed");
		return *found;
	}

	void test_revise_is_offered_to_the_owner_alone()
	{
		const auto pull = [](const std::string_view aViewer, const bool aOpen, const bool aDraft) {
			return core::revise_case_of({ .PullRequest = true, .Author = "octo-author", .Viewer = aViewer, .Open = aOpen, .Draft = aDraft });
		};
		expect(core::revise_case_of({ .LocalDraft = true }) == std::optional{ revise_case::local_draft }, "a local draft is always its author's");
		expect(core::revise_case_of({ .LocalDraft = true, .Linked = true }) == std::optional{ revise_case::draft_pull_request }, "a linked draft is a draft pull request");
		expect(pull("octo-author", true, true) == std::optional{ revise_case::draft_pull_request }, "my draft pull request");
		expect(pull("OCTO-AUTHOR", true, false) == std::optional{ revise_case::open_pull_request }, "my open pull request, logins without case");
		expect(!pull("octo-reviewer", true, false).has_value(), "anyone else's pull request has no Revise");
		expect(!pull("", true, false).has_value(), "an anonymous read has no Revise");
		expect(!pull("octo-author", false, false).has_value(), "a closed pull request has no Revise");
		expect(!core::revise_case_of({ }).has_value(), "nothing shown, nothing offered");
	}

	void test_the_opening_mode_per_case()
	{
		expect(core::opening_mode(revise_case::local_draft, std::nullopt, author_mode::overview) == author_mode::revise, "a local draft opens in Revise");
		expect(core::opening_mode(revise_case::draft_pull_request, std::nullopt, author_mode::overview) == author_mode::revise, "my draft pull request opens in Revise");
		expect(core::opening_mode(revise_case::open_pull_request, std::nullopt, author_mode::overview) == author_mode::overview, "my open pull request opens on Overview");
		expect(core::opening_mode(std::nullopt, std::nullopt, author_mode::review) == author_mode::review, "anyone else's opens as the setting says");
		expect(core::opening_mode(revise_case::open_pull_request, author_mode::revise, author_mode::overview) == author_mode::revise, "a window reopens in the mode it was left in");
		expect(core::opening_mode(revise_case::local_draft, author_mode::review, author_mode::overview) == author_mode::review, "a draft left in Review reopens there");
		expect(core::opening_mode(std::nullopt, author_mode::revise, author_mode::overview) == author_mode::overview, "Revise left in, and no longer offered, is not reopened");
		expect(core::opening_mode(revise_case::local_draft, author_mode::discuss, author_mode::overview) == author_mode::revise, "a draft has no Discuss");
	}

	void test_the_handoff()
	{
		expect(core::revise_handoff_of(revise_case::local_draft, core::workflow_publisher::built_in) == core::revise_handoff::create_draft_pull_request, "Create Draft PR…");
		expect(core::revise_handoff_of(revise_case::local_draft, core::workflow_publisher::external) == core::revise_handoff::hand_off_plan, "Hand Off Plan…");
		expect(core::revise_handoff_of(revise_case::draft_pull_request, core::workflow_publisher::external) == core::revise_handoff::ready_for_review, "Ready for review");
		expect(core::revise_handoff_of(revise_case::open_pull_request, core::workflow_publisher::built_in) == core::revise_handoff::publish_update, "Publish update…");
	}

	void test_the_checklist_marks()
	{
		auto plain = make_draft();
		auto list  = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &plain });
		expect(list.Rows.size() == 2U && row(list, "branch").Mark == checklist_mark::not_yet && row(list, "create").Mark == checklist_mark::not_yet,
			   "without a workflow the list is the built-in steps");
		list = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &plain, .Branch = core::hosted_branch_state::matches });
		expect(row(list, "branch").Mark == checklist_mark::observed && row(list, "branch").Runner == core::checklist_runner::betterpullreview, "BetterPullReview saw the branch: filled");

		auto draft = ok(core::select_workflow(plain, plain.Version, organization(), 101), "the workflow is selected");
		list       = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &draft });
		expect(list.RequiredUnmet == 1U && !list.Ready, "the required check is unmet before it runs");
		expect(row(list, "describe").Runner == core::checklist_runner::agent && row(list, "context").Runner == core::checklist_runner::companion, "who runs each row");
		expect(std::ranges::none_of(list.Rows, [](const core::checklist_row& aRow) { return aRow.Id == "publish"; }) && row(list, "create").Section == core::checklist_section::publish,
			   "the built-in publisher is a row; the workflow's is not while it is not chosen");
		expect(row(list, "follow").Mark == checklist_mark::waiting && row(list, "follow").Wait == core::checklist_wait::pull_request, "follow-up waits for the pull request");

		draft = ok(core::begin_action(draft, draft.Version, "context", { }, 102), "context handed off");
		list  = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &draft });
		expect(row(list, "context").Mark == checklist_mark::in_hand, "handed off, no answer yet");
		draft = ok(core::admit_workflow_response(draft, draft.Version, answer(draft, "context", core::workflow_outcome::completed), 103), "it answers");
		list  = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &draft });
		expect(row(list, "context").Mark == checklist_mark::reported, "a companion's answer is reported: outlined, never observed");

		draft          = ok(core::begin_action(draft, draft.Version, "template", { }, 104), "the check handed off");
		auto unmet     = answer(draft, "template", core::workflow_outcome::findings);
		unmet.Findings = { { .Check = "template", .State = core::workflow_finding_state::unmet, .Detail = "Testing is missing" } };
		draft          = ok(core::admit_workflow_response(draft, draft.Version, unmet, 105), "it reports not met");
		list           = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &draft });
		expect(row(list, "template").Mark == checklist_mark::reported_unmet && row(list, "template").Required && list.RequiredUnmet == 1U, "reported not met");

		draft.Workflow->Publisher = core::workflow_publisher::external;
		list                      = core::decide_author_checklist({ .Case = revise_case::local_draft, .Draft = &draft });
		expect(row(list, "publish").Mark == checklist_mark::waiting && row(list, "publish").Wait == core::checklist_wait::required_check, "the org's publisher waits for the required check");

		draft.Association = core::local_draft_association{ .Host = "github.com", .Owner = "acme", .Name = "engine", .Number = 7U, .HeadRef = "feature/search" };
		list              = core::decide_author_checklist({ .Case = revise_case::draft_pull_request, .Draft = &draft });
		expect(row(list, "pull-request").Mark == checklist_mark::observed && row(list, "follow").Mark == checklist_mark::not_yet, "the pull request exists: follow-up is ready to run");

		list = core::decide_author_checklist({ .Case = revise_case::open_pull_request });
		expect(list.Rows.size() == 1U && list.Rows.front().Wait == core::checklist_wait::worktree, "without a worktree the list asks for one");
	}
} // namespace

auto main() -> int
{
	test_revise_is_offered_to_the_owner_alone();
	test_the_opening_mode_per_case();
	test_the_handoff();
	test_the_checklist_marks();
	std::cout << "author-revise-tests:pass\n";
	return EXIT_SUCCESS;
}
