#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PRIVATE_REVIEW_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PRIVATE_REVIEW_STORE_HPP
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/application/private_review_store.hpp"
namespace bpr::adapters::sqlite {
	/// \brief Stable numeric identity admitted by the host for the existing string ledger scope.
	/// Incarnation is always read from the ledger; no caller may choose or restore it.
	struct private_review_configuration
	{
		review_ledger_configuration Ledger;
		core::bpr_profile_id        Profile;
		core::git_host_id           Host;
		core::github_account_id     Account;
		core::repository_id         Repository;
		core::pull_request_id       PullRequest;
	};
	[[nodiscard]] auto make_private_review_store(const private_review_configuration&, std::stop_token aStop = { })
		-> application::private_store_result<std::unique_ptr<application::private_review_store_port>>;
}
#endif
