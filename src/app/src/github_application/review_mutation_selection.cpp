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
	auto designated_live_publication_target(
		const application::review_mutation_target_binding& aTarget) -> bool
	{
		return aTarget.Hostname.value() == designated_review_hostname
			&& aTarget.Repository.Owner == designated_review_owner
			&& aTarget.Repository.Name == designated_review_name
			&& aTarget.RepositoryDatabaseId == designated_review_repository_id
			&& aTarget.Semantic.Repository.value() == designated_review_repository_id;
	}

	namespace {
		class designated_pull_request_port final : public application::github_publication_port
		{
		public:
			explicit designated_pull_request_port(std::shared_ptr<application::github_publication_port> aInner)
				: Inner{ std::move(aInner) }
			{
			}

			auto observe_repository(const application::github_publication_target& aTarget, std::stop_token aStop)
				-> application::github_publication_result<application::github_publication_capability> override
			{
				return admitted(aTarget) ? Inner->observe_repository(aTarget, aStop) : refused<application::github_publication_capability>();
			}
			auto observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
				-> application::github_publication_result<std::optional<core::git_commit_oid>> override
			{
				return admitted(aTarget) ? Inner->observe_branch(aTarget, aBranch, aStop) : refused<std::optional<core::git_commit_oid>>();
			}
			auto observe_commit(const application::github_publication_target&, const core::git_commit_oid&, std::stop_token)
				-> application::github_publication_result<std::optional<application::github_publication_commit>> override
			{
				return refused<std::optional<application::github_publication_commit>>();
			}
			auto observe_tree(const application::github_publication_target&, const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<bool> override
			{
				return refused<bool>();
			}
			auto observe_blob(const application::github_publication_target&, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<bool> override
			{
				return refused<bool>();
			}
			auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t>, const core::git_entry_oid&, std::stop_token)
				-> application::github_publication_result<void> override
			{
				return refused<void>();
			}
			auto create_tree(const application::github_publication_target&, const core::git_tree_oid&, const std::vector<core::git_tree_entry>&, const std::vector<std::string>&,
							 const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<void> override
			{
				return refused<void>();
			}
			auto create_commit(const application::github_publication_target&, const core::git_commit_content&, const core::git_commit_oid&, std::stop_token)
				-> application::github_publication_result<void> override
			{
				return refused<void>();
			}
			auto create_branch(const application::github_publication_target&, const std::string&, const core::git_commit_oid&, std::stop_token)
				-> application::github_publication_result<application::github_branch_creation> override
			{
				return refused<application::github_branch_creation>();
			}
			auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token aStop)
				-> application::github_publication_result<application::github_pull_request_observation> override
			{
				return admitted(aTarget) ? Inner->create_pull_request(aTarget, aDraft, aStop) : refused<application::github_pull_request_observation>();
			}
			auto find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
				-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
			{
				return admitted(aTarget) ? Inner->find_pull_requests(aTarget, aHead, aBase, aStop) : refused<std::vector<application::github_pull_request_observation>>();
			}
			auto observe_pull_request(const application::github_publication_target& aTarget, const std::uint64_t aNumber, std::stop_token aStop)
				-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
			{
				return admitted(aTarget) ? Inner->observe_pull_request(aTarget, aNumber, aStop) : refused<std::optional<application::github_pull_request_observation>>();
			}

		private:
			[[nodiscard]] static auto admitted(const application::github_publication_target& aTarget) -> bool
			{
				return aTarget.Host == designated_review_hostname && aTarget.Owner == designated_review_owner && aTarget.Repository == designated_review_name;
			}
			template <typename T>
			[[nodiscard]] static auto refused() -> application::github_publication_result<T>
			{
				return std::unexpected{ application::github_publication_error{ .Failure = application::github_publication_failure::unsupported_host } };
			}
			std::shared_ptr<application::github_publication_port> Inner;
		};
	}

	auto designated_pull_request_account(application::github_publication_account aAccount) -> application::github_publication_account
	{
		aAccount.Provider = std::make_shared<designated_pull_request_port>(std::move(aAccount.Provider));
		return aAccount;
	}

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
