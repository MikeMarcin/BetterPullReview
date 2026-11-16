#include "bpr/core/author_mode.hpp"

#include "bpr/core/review_role.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto linked(const local_draft& aDraft) -> bool
		{
			return aDraft.Association.has_value() || (aDraft.Publication && aDraft.Publication->Journal.Phase == local_publication_phase::linked);
		}

		[[nodiscard]] auto runner_of(const workflow_action& aAction) noexcept -> checklist_runner
		{
			return aAction.Binding == workflow_binding::agent ? checklist_runner::agent : checklist_runner::companion;
		}

		/// What the action's last answer says. An agent answers in
		/// BetterPullReview's own lane, so its answer is seen, not reported.
		[[nodiscard]] auto answered_mark(const workflow_action& aAction, const workflow_action_record& aRecord) -> checklist_mark
		{
			switch (aRecord.State) {
			case workflow_action_state::idle:
				return checklist_mark::not_yet;
			case workflow_action_state::handed_off:
			case workflow_action_state::awaiting_input:
				return checklist_mark::in_hand;
			case workflow_action_state::reported:
				break;
			case workflow_action_state::failed:
			case workflow_action_state::interrupted:
			case workflow_action_state::uncertain:
				return checklist_mark::reported_unmet;
			}
			if (aAction.Binding == workflow_binding::agent) {
				return checklist_mark::observed;
			}
			const auto unmet = std::ranges::any_of(aRecord.Findings, [](const workflow_finding& aFinding) { return aFinding.State != workflow_finding_state::met; });
			return unmet || !aRecord.Limitations.empty() ? checklist_mark::reported_unmet : checklist_mark::reported;
		}

		[[nodiscard]] auto action_mark(const draft_workflow& aWorkflow, const workflow_action& aAction) -> checklist_mark
		{
			const auto* const record = find_record(aWorkflow, aAction.Id);
			return record == nullptr || record->Invocation == 0U ? checklist_mark::not_yet : answered_mark(aAction, *record);
		}

		/// A required check reported on an earlier candidate, or under an
		/// earlier revision, is not met on this one: it is to run again.
		[[nodiscard]] auto required_mark(const checklist_mark aMark, const std::optional<check_gap> aGap) noexcept -> checklist_mark
		{
			if (!aGap || aMark != checklist_mark::reported) {
				return aMark;
			}
			switch (*aGap) {
			case check_gap::earlier_candidate:
			case check_gap::earlier_configuration:
			case check_gap::never_run:
				return checklist_mark::not_yet;
			case check_gap::pending:
				return checklist_mark::in_hand;
			case check_gap::upstream_changed:
			case check_gap::finding:
			case check_gap::limitation:
				break;
			}
			return checklist_mark::reported_unmet;
		}

		void add_preparation(const local_draft& aDraft, const std::vector<check_status>& aChecks, std::vector<checklist_row>& aRows)
		{
			const auto& workflow = *aDraft.Workflow;
			for (const auto& action : workflow.Workflow.Actions) {
				if (action.Slot == workflow_slot::publish_draft || action.Slot == workflow_slot::finish_publication) {
					continue;
				}
				const auto check = std::ranges::find(aChecks, action.Id, &check_status::Action);
				auto       mark  = action_mark(workflow, action);
				if (check != aChecks.end()) {
					mark = required_mark(mark, check->Gap);
				}
				aRows.push_back({ .Id = action.Id, .Task = action.Task, .Section = checklist_section::prepare, .Runner = runner_of(action), .Mark = mark, .Required = action.Required });
			}
		}

		void add_publication(const local_draft& aDraft, const hosted_branch_state aBranch, const std::size_t aUnmet, std::vector<checklist_row>& aRows)
		{
			const auto isLinked = linked(aDraft);
			aRows.push_back({
				.Id      = "branch",
				.Task    = "Branch is on GitHub",
				.Section = checklist_section::publish,
				.Runner  = checklist_runner::betterpullreview,
				.Mark    = isLinked || aBranch == hosted_branch_state::matches ? checklist_mark::observed : checklist_mark::not_yet,
			});
			const auto* const workflow = aDraft.Workflow ? &*aDraft.Workflow : nullptr;
			const auto* const publish  = workflow == nullptr ? nullptr : [workflow]() -> const workflow_action* {
				const auto found = std::ranges::find(workflow->Workflow.Actions, workflow_slot::publish_draft, &workflow_action::Slot);
				return found == workflow->Workflow.Actions.end() ? nullptr : &*found;
			}();
			if (publish != nullptr && workflow->Publisher == workflow_publisher::external) {
				auto mark = isLinked ? checklist_mark::observed : action_mark(*workflow, *publish);
				auto wait = checklist_wait::none;
				if (mark == checklist_mark::not_yet && aUnmet > 0U) {
					mark = checklist_mark::waiting;
					wait = checklist_wait::required_check;
				}
				aRows.push_back({ .Id = publish->Id, .Task = publish->Task, .Section = checklist_section::publish, .Runner = checklist_runner::companion, .Mark = mark, .Wait = wait });
				return;
			}
			aRows.push_back({
				.Id      = "create",
				.Task    = "Create the draft pull request",
				.Section = checklist_section::publish,
				.Runner  = checklist_runner::betterpullreview,
				.Mark    = isLinked ? checklist_mark::observed : checklist_mark::not_yet,
			});
		}

		void add_follow_up(const local_draft& aDraft, std::vector<checklist_row>& aRows)
		{
			const auto& workflow = *aDraft.Workflow;
			const auto  isLinked = linked(aDraft);
			for (const auto& action : workflow.Workflow.Actions) {
				if (action.Slot != workflow_slot::finish_publication) {
					continue;
				}
				const auto mark = isLinked ? action_mark(workflow, action) : checklist_mark::waiting;
				aRows.push_back({
					.Id      = action.Id,
					.Task    = action.Task,
					.Section = checklist_section::follow_up,
					.Runner  = runner_of(action),
					.Mark    = mark,
					.Wait    = isLinked ? checklist_wait::none : checklist_wait::pull_request,
				});
			}
		}
	} // namespace

	auto stable_name(const author_case aCase) noexcept -> std::string_view
	{
		switch (aCase) {
		case author_case::local_draft:        return "local_draft";
		case author_case::draft_pull_request: return "draft_pull_request";
		case author_case::open_pull_request:  return "open_pull_request";
		}
		return "local_draft";
	}

	auto author_case_of(const author_facts& aFacts) noexcept -> std::optional<author_case>
	{
		if (aFacts.LocalDraft) {
			if (!aFacts.Linked) {
				return author_case::local_draft;
			}
			if (aFacts.PullRequest && !aFacts.Open) {
				return std::nullopt;
			}
			return aFacts.Draft ? author_case::draft_pull_request : author_case::open_pull_request;
		}
		if (!aFacts.PullRequest || !aFacts.Open || aFacts.Author.empty() || !same_login(aFacts.Author, aFacts.Viewer)) {
			return std::nullopt;
		}
		return aFacts.Draft ? author_case::draft_pull_request : author_case::open_pull_request;
	}

	auto stable_name(const author_mode aMode) noexcept -> std::string_view
	{
		switch (aMode) {
		case author_mode::overview: return "overview";
		case author_mode::review:   return "review";
		case author_mode::discuss:  return "discuss";
		case author_mode::threads:  return "threads";
		case author_mode::author:   return "author";
		}
		return "overview";
	}

	auto opening_mode(const std::optional<author_case> aCase, const std::optional<author_mode> aLeftIn, const author_mode aOtherwise) noexcept -> author_mode
	{
		const auto local = aCase == std::optional{ author_case::local_draft };
		if (aLeftIn) {
			const auto absent = (*aLeftIn == author_mode::author && !aCase) || (local && (*aLeftIn == author_mode::discuss || *aLeftIn == author_mode::threads));
			if (!absent) {
				return *aLeftIn;
			}
		}
		if (local || aCase == std::optional{ author_case::draft_pull_request }) {
			return author_mode::author;
		}
		return aOtherwise == author_mode::author && !aCase ? author_mode::overview : aOtherwise;
	}

	auto stable_name(const author_handoff aHandoff) noexcept -> std::string_view
	{
		switch (aHandoff) {
		case author_handoff::create_draft_pull_request: return "create_draft_pull_request";
		case author_handoff::hand_off_plan:             return "hand_off_plan";
		case author_handoff::ready_for_review:          return "ready_for_review";
		case author_handoff::publish_update:            return "publish_update";
		}
		return "create_draft_pull_request";
	}

	auto author_handoff_of(const author_case aCase, const workflow_publisher aPublisher) noexcept -> author_handoff
	{
		switch (aCase) {
		case author_case::local_draft:
			return aPublisher == workflow_publisher::external ? author_handoff::hand_off_plan : author_handoff::create_draft_pull_request;
		case author_case::draft_pull_request:
			return author_handoff::ready_for_review;
		case author_case::open_pull_request:
			return author_handoff::publish_update;
		}
		return author_handoff::create_draft_pull_request;
	}

	auto stable_name(const checklist_mark aMark) noexcept -> std::string_view
	{
		switch (aMark) {
		case checklist_mark::not_yet:        return "not_yet";
		case checklist_mark::waiting:        return "waiting";
		case checklist_mark::in_hand:        return "in_hand";
		case checklist_mark::observed:       return "observed";
		case checklist_mark::reported:       return "reported";
		case checklist_mark::reported_unmet: return "reported_unmet";
		}
		return "not_yet";
	}

	auto decide_author_checklist(const checklist_facts& aFacts) -> author_checklist
	{
		auto checklist = author_checklist{ };
		if (aFacts.Draft == nullptr) {
			// A pull request with no worktree: the rows that need one wait for it.
			checklist.Rows.push_back({
				.Id      = "worktree",
				.Task    = "Choose your worktree",
				.Section = checklist_section::prepare,
				.Runner  = checklist_runner::author,
				.Mark    = checklist_mark::waiting,
				.Wait    = checklist_wait::worktree,
			});
			return checklist;
		}
		const auto& draft       = *aFacts.Draft;
		const auto  checks      = draft.Workflow ? required_checks(draft) : std::vector<check_status>{ };
		checklist.RequiredUnmet = static_cast<std::size_t>(std::ranges::count_if(checks, [](const check_status& aCheck) { return aCheck.Gap.has_value(); }));
		if (draft.Workflow) {
			add_preparation(draft, checks, checklist.Rows);
		}
		if (aFacts.Case == author_case::local_draft) {
			add_publication(draft, aFacts.Branch, checklist.RequiredUnmet, checklist.Rows);
		} else {
			checklist.Rows.push_back({
				.Id      = "pull-request",
				.Task    = aFacts.Case == author_case::draft_pull_request ? "The draft pull request is on GitHub" : "The pull request is on GitHub",
				.Section = checklist_section::publish,
				.Runner  = checklist_runner::betterpullreview,
				.Mark    = linked(draft) ? checklist_mark::observed : checklist_mark::not_yet,
			});
		}
		if (draft.Workflow) {
			add_follow_up(draft, checklist.Rows);
		}
		checklist.Ready = checklist.RequiredUnmet == 0U
			&& std::ranges::none_of(checklist.Rows, [](const checklist_row& aRow) {
							  return aRow.Section == checklist_section::prepare && (aRow.Mark == checklist_mark::in_hand || aRow.Mark == checklist_mark::reported_unmet);
						  });
		return checklist;
	}
} // namespace bpr::core
