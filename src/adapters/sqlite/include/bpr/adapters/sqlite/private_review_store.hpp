#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PRIVATE_REVIEW_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PRIVATE_REVIEW_STORE_HPP
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/application/private_review_store.hpp"
#include "bpr/application/release_update_recovery.hpp"
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
	/// Records the durable identity needed to compare a recovery copy against the review ledger.
	[[nodiscard]] auto release_update_ledger_draft_identity(const core::private_review_scope&, core::review_draft_id, const core::git_commit_oid&) -> std::string;
	/// Returns the candidate indices whose exact active draft text is readable in the existing ledger.
	/// Unknown, unavailable, legacy and indeterminate entries never count as a match.
	[[nodiscard]] auto restorable_release_update_drafts(const std::filesystem::path&, std::span<const application::release_update_saved_draft>) -> std::vector<std::size_t>;
}
#endif
