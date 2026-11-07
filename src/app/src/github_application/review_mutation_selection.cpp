#include "github_application/review_mutation_selection.hpp"

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "review_mutation_execution_authority.hpp"

#include <utility>

namespace bpr::app {
	auto designated_review_replay_namespace_path(
		const std::filesystem::path& aApplicationDataRoot)
		-> std::filesystem::path
	{
		return (aApplicationDataRoot
				/ std::filesystem::path{ application::detail::
											 designated_review_replay_namespace_child_name })
			.lexically_normal();
	}

	auto select_designated_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot)
		-> std::optional<designated_review_mutation_selection>
	{
		const auto expectedBase = core::parse_git_commit_oid(designated_review_base_oid);
		const auto expectedHead = core::parse_git_commit_oid(designated_review_head_oid);
		const auto snapshotBase = core::parse_git_commit_oid(
			aSnapshot.PullRequest.BaseOid.value());
		const auto snapshotHead = core::parse_git_commit_oid(
			aSnapshot.PullRequest.HeadOid.value());
		if (!expectedBase.has_value() || !expectedHead.has_value()
			|| !snapshotBase.has_value() || !snapshotHead.has_value()
			|| aHandoff.Hostname.value() != designated_review_hostname
			|| aHandoff.ServerContract
				!= application::github_server_contract::github_com_2022_11_28
			|| aHandoff.Locator.Owner != designated_review_owner
			|| aHandoff.Locator.Name != designated_review_name
			|| aHandoff.Repository.value() != designated_review_repository_id
			|| aHandoff.PullRequest.value() != designated_review_pull_request_id
			|| aHandoff.Number.value() != designated_review_number
			// [[BPR-1560]]. The designated pull request's merge base is
			// its recorded base; both must be that one commit.
			|| application::recorded_base(aHandoff) != *expectedBase || aHandoff.Base != *expectedBase || aHandoff.Head != *expectedHead
			|| aHandoff.BaseRef.view() != designated_review_base_full_ref
			|| aSnapshot.Profile != aHandoff.Profile
			|| aSnapshot.Host != aHandoff.Host
			|| aSnapshot.Account != aHandoff.Account
			|| aSnapshot.Viewer.Actor.value().empty()
			|| aSnapshot.Viewer.Login.empty()
			|| aSnapshot.Repository.Repository != aHandoff.Repository
			|| aSnapshot.Repository.Repository.value()
				!= designated_review_repository_id
			|| aSnapshot.Repository.Node.value() != designated_review_repository_node
			|| aSnapshot.Repository.Owner != designated_review_owner
			|| aSnapshot.Repository.Name != designated_review_name
			|| aSnapshot.PullRequest.PullRequest != aHandoff.PullRequest
			|| aSnapshot.PullRequest.PullRequest.value()
				!= designated_review_pull_request_id
			|| aSnapshot.PullRequest.Repository != aHandoff.Repository
			|| aSnapshot.PullRequest.Number != aHandoff.Number
			|| aSnapshot.PullRequest.Node.value()
				!= designated_review_pull_request_node
			|| aSnapshot.PullRequest.State
				!= application::github_pull_request_state::open
			|| *snapshotBase != *expectedBase || *snapshotHead != *expectedHead
			|| aSnapshot.PullRequest.BaseRef != designated_review_base_ref
			|| aSnapshot.PullRequest.HeadRef != designated_review_head_ref) {
			return std::nullopt;
		}

		auto selection = designated_review_mutation_selection{
			.Target = {
				.Semantic = {
					.Host         = aHandoff.Host,
					.Repository   = aHandoff.Repository,
					.PullRequest  = aHandoff.PullRequest,
					.ExpectedHead = *expectedHead,
				},
				.Hostname       = application::github_canonical_hostname{ designated_review_hostname },
				.ServerContract = application::github_server_contract::github_com_2022_11_28,
				.Repository     = {
					.Owner = std::string{ designated_review_owner },
					.Name  = std::string{ designated_review_name },
				},
				.Number                   = aHandoff.Number,
				.RepositoryDatabaseId     = aSnapshot.Repository.Repository.value(),
				.RepositoryNodeId         = std::string{ aSnapshot.Repository.Node.value() },
				.PullRequestDatabaseId    = aSnapshot.PullRequest.PullRequest.value(),
				.PullRequestNodeId        = std::string{ aSnapshot.PullRequest.Node.value() },
				.BaseRepositoryDatabaseId = aSnapshot.PullRequest.Repository.value(),
				.BaseOid                  = *expectedBase,
				.BaseRef                  = std::string{ designated_review_base_ref },
				.HeadRef                  = std::string{ designated_review_head_ref },
				.Open                     = true,
				.ComparisonBaseOid        = application::distinct_comparison_base(application::recorded_base(aHandoff), aHandoff.Base),
			},
			.ActingIdentity = {
				.Account    = aHandoff.Account,
				.DatabaseId = aSnapshot.Account.value(),
				.NodeId     = std::string{ aSnapshot.Viewer.Actor.value() },
				.Login      = aSnapshot.Viewer.Login,
			},
		};
		if (!application::valid(selection.Target)
			|| !application::valid(selection.ActingIdentity)) {
			return std::nullopt;
		}
		return selection;
	}

	auto select_observed_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot,
		const bool                                        aAdmitClosed)
		-> std::optional<designated_review_mutation_selection>
	{
		const auto open = aSnapshot.PullRequest.State == application::github_pull_request_state::open;
		const auto base = core::parse_git_commit_oid(
			aSnapshot.PullRequest.BaseOid.value());
		const auto head = core::parse_git_commit_oid(
			aSnapshot.PullRequest.HeadOid.value());
		if (!base.has_value() || !head.has_value()
			|| *base != application::recorded_base(aHandoff) || *head != aHandoff.Head
			|| aSnapshot.Profile != aHandoff.Profile
			|| aSnapshot.Host != aHandoff.Host
			|| aSnapshot.Account != aHandoff.Account
			|| aSnapshot.Repository.Repository != aHandoff.Repository
			|| aSnapshot.Repository.Owner != aHandoff.Locator.Owner
			|| aSnapshot.Repository.Name != aHandoff.Locator.Name
			|| aSnapshot.PullRequest.PullRequest != aHandoff.PullRequest
			|| aSnapshot.PullRequest.Repository != aHandoff.Repository
			|| aSnapshot.PullRequest.Number != aHandoff.Number
			|| (!open && !aAdmitClosed)) {
			return std::nullopt;
		}
		auto selection = designated_review_mutation_selection{
			.Target = {
				.Semantic = {
					.Host         = aHandoff.Host,
					.Repository   = aHandoff.Repository,
					.PullRequest  = aHandoff.PullRequest,
					.ExpectedHead = *head,
				},
				.Hostname                 = aHandoff.Hostname,
				.ServerContract           = aHandoff.ServerContract,
				.Repository               = aHandoff.Locator,
				.Number                   = aHandoff.Number,
				.RepositoryDatabaseId     = aSnapshot.Repository.Repository.value(),
				.RepositoryNodeId         = std::string{ aSnapshot.Repository.Node.value() },
				.PullRequestDatabaseId    = aSnapshot.PullRequest.PullRequest.value(),
				.PullRequestNodeId        = std::string{ aSnapshot.PullRequest.Node.value() },
				.BaseRepositoryDatabaseId = aSnapshot.PullRequest.Repository.value(),
				.BaseOid                  = *base,
				.BaseRef                  = aSnapshot.PullRequest.BaseRef,
				.HeadRef                  = aSnapshot.PullRequest.HeadRef,
				.Open                     = open,
				.ComparisonBaseOid        = application::distinct_comparison_base(application::recorded_base(aHandoff), aHandoff.Base),
			},
			.ActingIdentity = {
				.Account    = aHandoff.Account,
				.DatabaseId = aSnapshot.Account.value(),
				.NodeId     = std::string{ aSnapshot.Viewer.Actor.value() },
				.Login      = aSnapshot.Viewer.Login,
			},
		};
		return application::valid_identity(selection.Target)
				&& application::valid(selection.ActingIdentity)
			? std::optional{ std::move(selection) }
			: std::nullopt;
	}
#if defined(BPR_BUILD_TESTING)
	gated_review_mutation_decision_executor::gated_review_mutation_decision_executor(
		application::decision_executor& aTarget)
		: Target{ aTarget }
	{
	}

	void gated_review_mutation_decision_executor::post(std::function<void()> aTask)
	{
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Holding) {
				Held.push_back(std::move(aTask));
				return;
			}
		}
		Target.post(std::move(aTask));
	}

	void gated_review_mutation_decision_executor::hold()
	{
		const auto lock = std::scoped_lock{ Mutex };
		Holding         = true;
	}

	void gated_review_mutation_decision_executor::release()
	{
		auto held = std::deque<std::function<void()>>{ };
		{
			const auto lock = std::scoped_lock{ Mutex };
			Holding         = false;
			held.swap(Held);
		}
		for (auto& task : held) {
			Target.post(std::move(task));
		}
	}

	auto gated_review_mutation_decision_executor::release_next() -> bool
	{
		auto task = std::function<void()>{ };
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Held.empty()) {
				return false;
			}
			task = std::move(Held.front());
			Held.pop_front();
		}
		Target.post(std::move(task));
		return true;
	}

	auto gated_review_mutation_decision_executor::pending() const -> std::size_t
	{
		const auto lock = std::scoped_lock{ Mutex };
		return Held.size();
	}
#endif
}
