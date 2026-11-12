#include "bpr/application/local_draft_publishing.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto target_of(const core::local_draft& aDraft, const github_publication_account& aAccount) -> github_publication_target
		{
			const auto& repository = aDraft.Target.Target.Repository;
			return { .Host = aAccount.Host, .Server = aAccount.Server, .Account = aAccount.Account, .Login = aAccount.Login, .Owner = repository.Owner, .Repository = repository.Name };
		}

		/// Writes the next version of the draft; on a lost race, the stored one.
		[[nodiscard]] auto put(local_draft_store& aStore, const core::local_draft& aPrevious, core::local_draft aNext, local_publication_step& aStep) -> bool
		{
			if (auto written = aStore.put(aNext, aPrevious.Version, { }); !written) {
				aStep.Store = written.error();
				if (auto current = aStore.find(aPrevious.Id)) {
					aStep.Draft = std::move(*current);
				}
				return false;
			}
			aStep.Draft = std::move(aNext);
			return true;
		}

		/// The open pull requests from the draft's branch into its target.
		[[nodiscard]] auto open_pulls(github_publication_port& aProvider, const github_publication_target& aTarget, const core::local_draft& aDraft, std::stop_token aStop)
			-> github_publication_result<std::vector<core::local_publication_link>>
		{
			auto found = aProvider.find_pull_requests(aTarget, aDraft.Branch, aDraft.Target.Target.Branch, aStop);
			if (!found) {
				return std::unexpected{ found.error() };
			}
			auto out = std::vector<core::local_publication_link>{ };
			for (const auto& pull : *found) {
				if (pull.State == "open" && pull.HeadRef == aDraft.Branch && pull.BaseRef == aDraft.Target.Target.Branch) {
					out.push_back(local_link_of(pull));
				}
			}
			return out;
		}

		[[nodiscard]] auto reason_of(const github_publication_error& aError) -> std::string
		{
			switch (aError.Failure) {
			case github_publication_failure::unavailable:     return "GitHub did not answer.";
			case github_publication_failure::unsupported_host: return "This account's host is not qualified for creating pull requests here.";
			case github_publication_failure::refused:
				return aError.HttpStatus == 422U ? std::string{ "GitHub refused it (HTTP 422): a pull request may already exist, or the branch has no commits the target lacks." }
												 : "GitHub refused it" + (aError.HttpStatus != 0U ? " (HTTP " + std::to_string(aError.HttpStatus) + ")" : std::string{ }) + ".";
			case github_publication_failure::rate_limited: return "GitHub's rate limit for this account is spent.";
			case github_publication_failure::mismatch:     return "GitHub answered with something other than this pull request.";
			case github_publication_failure::too_large:    return "The description is larger than GitHub takes.";
			case github_publication_failure::cancelled:    return "Stopped.";
			}
			return "GitHub did not answer.";
		}
	}

	auto local_link_of(const github_pull_request_observation& aPull) -> core::local_publication_link
	{
		return { .Number     = aPull.Number,
				 .Url        = aPull.Url,
				 .NodeId     = aPull.NodeId,
				 .Owner      = aPull.HeadOwner,
				 .Repository = aPull.HeadRepository,
				 .HeadBranch = aPull.HeadRef,
				 .Head       = aPull.HeadCommit,
				 .BaseBranch = aPull.BaseRef,
				 .Author     = aPull.Author,
				 .Draft      = aPull.Draft };
	}

	auto check_local_publication(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account* const aAccount, const bool aTour, const std::int64_t aNow,
								 std::stop_token aStop) -> local_publication_step
	{
		auto        step      = local_publication_step{ .Draft = aDraft };
		const auto* candidate = core::latest_candidate(aDraft);
		if (candidate == nullptr) {
			step.Refusal = core::local_publication_refusal::no_candidate;
			return step;
		}
		step.Push     = core::name_push(aDraft, *candidate);
		auto observed = core::local_publication_observation{ };
		if (aAccount != nullptr && aAccount->Provider) {
			auto&      provider = *aAccount->Provider;
			const auto target   = target_of(aDraft, *aAccount);
			observed.Host       = aAccount->Host;
			observed.Account    = aAccount->Account;
			observed.Login      = aAccount->Login;
			auto capability     = provider.observe_repository(target, aStop);
			auto head           = capability ? provider.observe_branch(target, aDraft.Branch, aStop) : github_publication_result<std::optional<core::git_commit_oid>>{ std::unexpected{ capability.error() } };
			if (!capability || !head) {
				step.Provider = capability ? head.error() : capability.error();
			} else {
				observed.CanPush        = capability->CanPush;
				observed.Archived       = capability->Archived;
				observed.BranchObserved = true;
				observed.BranchHead     = *head;
				if (auto tip = provider.observe_branch(target, aDraft.Target.Target.Branch, aStop); tip) {
					observed.TargetTip = *tip;
				}
				auto pulls = open_pulls(provider, target, aDraft, aStop);
				if (!pulls) {
					step.Provider           = pulls.error();
					observed.BranchObserved = false;
				} else if (!pulls->empty()) {
					// The plan is still recorded, so the author can associate
					// it explicitly; creating is refused below and again at Create.
					step.Existing = pulls->front();
				}
			}
		}
		step.Branch = core::decide_hosted_branch(*candidate, observed.BranchObserved, observed.BranchHead);
		if (step.Branch == core::hosted_branch_state::matches) {
			step.Push.reset();
		}
		auto plan = core::make_local_publication(aDraft, observed, aTour);
		if (!plan) {
			step.Refusal = plan.error();
			return step;
		}
		if (step.Existing) {
			step.Refusal = core::local_publication_refusal::pull_request_exists;
		}
		// The same plan again keeps its journal; a changed one is recorded anew.
		if (aDraft.Publication && aDraft.Publication->Journal.Phase == core::local_publication_phase::prepared
			&& aDraft.Publication->Journal.Digest == core::local_publication_digest(*plan)) {
			return step;
		}
		auto recorded = core::record_local_publication(aDraft, aDraft.Version, std::move(*plan), aNow);
		if (!recorded) {
			step.Refusal = recorded.error();
			return step;
		}
		static_cast<void>(put(aStore, aDraft, std::move(*recorded), step));
		return step;
	}

	auto create_local_pull_request(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, const std::string& aDigest, const std::int64_t aNow,
								   std::stop_token aStop) -> local_publication_step
	{
		auto step = local_publication_step{ .Draft = aDraft, .Branch = core::hosted_branch_state::matches };
		if (!aAccount.Provider) {
			step.Refusal = core::local_publication_refusal::no_account;
			return step;
		}
		auto authorized = core::authorize_local_publication(aDraft, aDraft.Version, aDigest, aAccount.Account, aNow);
		if (!authorized) {
			step.Refusal = authorized.error();
			return step;
		}
		const auto& plan     = authorized->Publication->Plan;
		auto&       provider = *aAccount.Provider;
		const auto  target   = target_of(aDraft, aAccount);
		// Read again just before: a branch that moved or a pull request that
		// appeared since the preview sends nothing.
		auto head = provider.observe_branch(target, aDraft.Branch, aStop);
		if (!head) {
			step.Provider = head.error();
			return step;
		}
		if (*head != std::optional{ plan.Head }) {
			step.Branch  = *head ? core::hosted_branch_state::differs : core::hosted_branch_state::missing;
			step.Push    = core::name_push(aDraft, *core::latest_candidate(aDraft));
			step.Refusal = *head ? core::local_publication_refusal::branch_differs : core::local_publication_refusal::branch_missing;
			return step;
		}
		auto pulls = open_pulls(provider, target, aDraft, aStop);
		if (!pulls) {
			step.Provider = pulls.error();
			return step;
		}
		if (!pulls->empty()) {
			step.Existing = pulls->front();
			step.Refusal  = core::local_publication_refusal::pull_request_exists;
			return step;
		}
		const auto generation = authorized->Publication->Journal.Generation;
		auto       begun      = core::begin_local_creation(*authorized, authorized->Version, generation, aAccount.Account, aNow);
		if (!begun) {
			step.Refusal = begun.error();
			return step;
		}
		// The creation is on disk before it is sent.
		if (!put(aStore, aDraft, std::move(*begun), step)) {
			return step;
		}
		const auto before  = *step.Draft;
		const auto created = provider.create_pull_request(
			target, { .Title = plan.Title, .Body = plan.Body, .Head = plan.Destination.HeadBranch, .Base = plan.Destination.BaseBranch, .Draft = plan.Draft }, aStop);
		auto outcome = core::local_creation_outcome::created;
		auto link    = std::optional<core::local_publication_link>{ };
		auto reason  = std::string{ };
		if (created) {
			link = local_link_of(*created);
		} else {
			step.Provider = created.error();
			reason        = reason_of(created.error());
			if (created.error().Transmission == github_publication_transmission::indeterminate) {
				outcome = core::local_creation_outcome::uncertain;
			} else if (created.error().Failure == github_publication_failure::refused) {
				outcome = core::local_creation_outcome::refused;
			} else {
				outcome = core::local_creation_outcome::not_sent;
			}
		}
		auto completed = core::complete_local_creation(before, before.Version, outcome, std::move(link), std::move(reason), aNow);
		if (!completed) {
			// GitHub answered with a pull request that is not this plan's: keep
			// it uncertain and let a read decide.
			completed = core::complete_local_creation(before, before.Version, core::local_creation_outcome::uncertain, std::nullopt, "GitHub's answer named another pull request.", aNow);
		}
		if (!completed || !put(aStore, before, std::move(*completed), step)) {
			return step;
		}
		// A lost answer is read for at once: recovery reads, it never resends.
		if (step.Draft->Publication->Journal.Phase == core::local_publication_phase::uncertain && !aStop.stop_requested()) {
			auto recovered = recover_local_publication(aStore, *step.Draft, aAccount, aNow, aStop);
			recovered.Provider = recovered.Provider ? recovered.Provider : step.Provider;
			return recovered;
		}
		return step;
	}

	auto recover_local_publication(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, const std::int64_t aNow, std::stop_token aStop)
		-> local_publication_step
	{
		auto step = local_publication_step{ .Draft = aDraft, .Branch = core::hosted_branch_state::matches };
		if (!aDraft.Publication || aDraft.Publication->Journal.Phase != core::local_publication_phase::uncertain) {
			return step;
		}
		if (!aAccount.Provider || aAccount.Account != aDraft.Publication->Plan.Destination.Account) {
			step.Refusal = core::local_publication_refusal::authority_changed;
			return step;
		}
		auto found = aAccount.Provider->find_pull_requests(target_of(aDraft, aAccount), aDraft.Branch, aDraft.Target.Target.Branch, aStop);
		if (!found) {
			step.Provider = found.error();
			return step;
		}
		auto ours = std::optional<core::local_publication_link>{ };
		for (const auto& pull : *found) {
			auto link = local_link_of(pull);
			if (pull.State == "open" && link.Author == aDraft.Publication->Plan.Destination.Login && core::local_pull_request_matches(aDraft.Publication->Plan, link)) {
				ours = std::move(link);
				break;
			}
		}
		auto observed = core::observe_local_creation(aDraft, aDraft.Version, std::move(ours), aNow);
		if (!observed) {
			step.Refusal = observed.error();
			return step;
		}
		static_cast<void>(put(aStore, aDraft, std::move(*observed), step));
		return step;
	}

	auto associate_local_pull_request(local_draft_store& aStore, const core::local_draft& aDraft, const github_publication_account& aAccount, const std::uint64_t aNumber,
									  const std::int64_t aNow, std::stop_token aStop) -> local_publication_step
	{
		auto step = local_publication_step{ .Draft = aDraft, .Branch = core::hosted_branch_state::matches };
		if (!aAccount.Provider) {
			step.Refusal = core::local_publication_refusal::no_account;
			return step;
		}
		auto pull = aAccount.Provider->observe_pull_request(target_of(aDraft, aAccount), aNumber, aStop);
		if (!pull) {
			step.Provider = pull.error();
			return step;
		}
		if (!*pull || (*pull)->State != "open") {
			step.Refusal = core::local_publication_refusal::invalid;
			return step;
		}
		auto adopted = core::adopt_local_pull_request(aDraft, aDraft.Version, local_link_of(**pull), aNow);
		if (!adopted) {
			step.Refusal = adopted.error();
			return step;
		}
		static_cast<void>(put(aStore, aDraft, std::move(*adopted), step));
		return step;
	}
}
