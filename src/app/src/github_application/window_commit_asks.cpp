/// \file
/// \brief [[BPR-2248]]. The Commits tab's agent prompts in the window: the
/// rows its menu lists for a selection, a press queued on the Agent tab's
/// session, and, at its release, the selected commits' own changes read (the
/// commits the review lacks fetched, as What changed since my review does) and
/// sent beside the question. Merges of the base branch are named and left out.

#include "github_application/window_composition.hpp"

#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"

#include <QScopeGuard>

#include <algorithm>
#include <memory>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		[[nodiscard]] auto hex(const core::git_commit_oid& aOid) -> std::string { return std::string{ core::canonical_hex(aOid).view() }; }

		/// The pull request's commits as core reads them, oldest first.
		[[nodiscard]] auto listed_commits(const application::github_pull_request_snapshot& aSnapshot) -> std::vector<core::listed_commit>
		{
			auto commits = std::vector<core::listed_commit>{ };
			for (const auto& commit : aSnapshot.Commits) {
				auto listed = core::listed_commit{ .Oid = std::string{ commit.Oid.value() }, .Headline = commit.Headline };
				if (commit.Parents) {
					for (const auto& parent : *commit.Parents) {
						listed.Parents.emplace_back(parent.value());
					}
				}
				commits.push_back(std::move(listed));
			}
			return commits;
		}

		/// The commits as the tab names them, in the pull request's order.
		[[nodiscard]] auto in_order(const std::vector<std::string>& aSelected, const std::vector<core::listed_commit>& aCommits) -> std::vector<std::string>
		{
			auto ordered = std::vector<std::string>{ };
			for (const auto& commit : aCommits) {
				if (std::ranges::find(aSelected, commit.Oid) != aSelected.end()) {
					ordered.push_back(commit.Oid);
				}
			}
			return ordered;
		}
	} // namespace

	void github_application::implementation::bind_commit_asks()
	{
		if (Window == nullptr || Window->overview_page() == nullptr || Window->overview_page()->status_drawer() == nullptr) {
			return;
		}
		Window->overview_page()->status_drawer()->commits()->set_agent_asks([this](const std::vector<std::string>& aCommits) { return commit_asks_for(aCommits); },
																			[this](const core::commit_ask_row& aRow) { queue_commit_ask(aRow); });
	}

	auto github_application::implementation::commit_asks_for(const std::vector<std::string>& aCommits) const -> core::commit_asks
	{
		const auto& snapshot = WindowRead != nullptr ? WindowRead->accepted_snapshot() : std::nullopt;
		if (Closing || Window == nullptr || !snapshot) {
			return { };
		}
		const auto commits = listed_commits(*snapshot);
		// A commit no longer listed is named as selected, so the plan refuses it.
		auto selected = in_order(aCommits, commits);
		for (const auto& commit : aCommits) {
			if (std::ranges::find(selected, commit) == selected.end()) {
				selected.push_back(commit);
			}
		}
		const auto plan = core::plan_commit_selection(selected, commits, snapshot->PullRequest.BaseRef);
		return core::decide_commit_asks(placed_prompts(), quick_ask_facts(qt::ask_menu_request{ }), selected, plan.Refusal);
	}

	/// A plain click: the question, its commits named, is queued on the Agent
	/// tab's session, released when the agent is set up and free.
	void github_application::implementation::queue_commit_ask(const core::commit_ask_row& aRow)
	{
		const auto session = ask_session_key();
		if (Closing || Window == nullptr || !aRow.Enabled || !aRow.Offer || session.empty() || !AgentQueue) {
			return;
		}
		auto input = core::queued_input{ .QuickAsk = aRow.Id, .Scope = "commits", .Label = aRow.Label, .Prompt = aRow.Offer };
		if (const auto refused = AgentQueue->enqueue(session, std::move(input)); refused != core::queue_refusal::none) {
			Window->open_agent_question();
			Window->agent_panel()->refuse(refused == core::queue_refusal::not_saved ? "This question couldn't be saved to the queue, so it wasn't queued." : "The queue is full: nothing more was queued.");
			return;
		}
		Window->open_agent_question();
		if (Window->agent_tab()->setup_readiness() != qt::agent_setup_readiness::ready) {
			Window->open_agent_setup();
		}
		observe_ask_queue(false);
	}

	void github_application::implementation::refuse_commit_ask(const std::string& aLabel, const std::string& aWhy)
	{
		if (Window != nullptr) {
			Window->open_agent_question();
			Window->agent_panel()->refuse(aLabel + ": " + aWhy + " Nothing was sent.");
		}
	}

	/// The release: the selected commits' own changes, read in one job
	/// through the review's GitHub access, then the question with them.
	void github_application::implementation::ask_about_commits(const core::prompt_offer& aOffer)
	{
		if (Closing || Window == nullptr || EpochHost == nullptr) {
			return;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		const auto& state    = EpochHost->state();
		if (!snapshot || !state.AcceptedRevision || !LastAcceptedHandoff) {
			refuse_commit_ask(aOffer.Label, "the pull request has not been read yet.");
			return;
		}
		const auto commits = listed_commits(*snapshot);
		const auto plan    = core::plan_commit_selection(aOffer.Commits, commits, snapshot->PullRequest.BaseRef);
		switch (plan.Refusal) {
		case core::commit_selection_refusal::none:             break;
		case core::commit_selection_refusal::nothing_selected:
		case core::commit_selection_refusal::not_listed:       refuse_commit_ask(aOffer.Label, "a commit it named is no longer in this pull request."); return;
		case core::commit_selection_refusal::too_many:         refuse_commit_ask(aOffer.Label, "it names more commits than one question carries."); return;
		case core::commit_selection_refusal::no_own_change:    refuse_commit_ask(aOffer.Label, "these commits only merge the base branch."); return;
		}
		const auto head    = state.AcceptedRevision->Head;
		auto       request = application::returning_change_request{ .WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff), .Head = head, .Depth = plan.Depth };
		for (const auto& step : plan.Steps) {
			const auto parent = core::parse_git_commit_oid(step.Parent);
			const auto commit = core::parse_git_commit_oid(step.Commit);
			if (!parent || !commit) {
				refuse_commit_ask(aOffer.Label, "a commit it named is not a commit ID.");
				return;
			}
			request.Steps.push_back({ .Parent = *parent, .Commit = *commit });
		}
		// Each selected commit as the agent reads it, its message whole.
		auto selected = std::vector<core::selected_commit>{ };
		for (const auto& commit : plan.Selected) {
			const auto record = std::ranges::find_if(snapshot->Commits, [&commit](const application::github_commit_record& aRecord) { return aRecord.Oid.value() == commit.Oid; });
			selected.push_back({
				.Oid        = commit.Oid,
				.Headline   = commit.Headline,
				.Message    = record != snapshot->Commits.end() ? record->Body : std::string{ },
				.MergesBase = std::ranges::find(plan.BaseMerges, commit.Oid) != plan.BaseMerges.end(),
			});
		}
		const auto key = hex(head) + ":" + aOffer.Id;
		CommitAsk.Key  = key;
		CommitAsk.Material.reset();
		const auto answered = std::make_shared<bool>(false);
		const auto admitted = EpochHost->resolve_returning_change(std::move(request), [this, key, answered, offer = aOffer, steps = plan.Steps, read = std::move(selected)](const application::returning_change_observation& aRead) mutable {
			*answered = true;
			if (Closing || CommitAsk.Key != key) {
				return;
			}
			// A release this pass did not send was refused.
			const auto settled = qScopeGuard([this] { settle_pending_release(); });
			using status       = application::returning_change_status;
			if (aRead.Status != status::resolved) {
				refuse_commit_ask(offer.Label, aRead.Status == status::earlier_unavailable ? "a commit it names could not be fetched from GitHub." : aRead.Status == status::stale ? "the review changed while its commits were read. Ask again."
																																												   : "its commits could not be read.");
				return;
			}
			auto files = std::vector<core::returning_commit_files>{ };
			for (std::size_t index = 0U; index < aRead.Steps.size() && index < steps.size(); ++index) {
				auto& step = files.emplace_back(core::returning_commit_files{ .Commit = steps[index].Commit, .Headline = steps[index].Headline });
				for (const auto& file : aRead.Steps[index].Files) {
					step.Files.push_back(compare_change_file(file));
				}
			}
			auto changes = core::bound_returning_commits(std::move(files));
			changes.FilesOmitted += aRead.FilesOmitted;
			auto material = core::bound_selected_commits(std::move(read), std::move(changes));
			if (!material) {
				refuse_commit_ask(offer.Label, "its commits can't be sent.");
				return;
			}
			if (Window == nullptr || Window->agent_tab() == nullptr) {
				return;
			}
			// Carried by the turn this send starts, and by no other.
			CommitAsk.Material = std::move(material);
			const auto sent    = qScopeGuard([this] { CommitAsk.Material.reset(); });
			dismiss_asked_conversation();
			Window->open_agent_question();
			static_cast<void>(Window->agent_tab()->send_decided_prompt(offer));
		});
		if (!admitted) {
			refuse_commit_ask(aOffer.Label, "the review is still loading. Ask again in a moment.");
			return;
		}
		// The turn is sent once the commits are read.
		if (!*answered) {
			note_release_deferred();
		}
	}
#endif
} // namespace bpr::app
