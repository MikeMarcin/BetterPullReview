#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PUBLICATION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PUBLICATION_PORT_HPP

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief The finite GitHub publication catalog (RFC-BPR-0059, BPR-1175 and
/// BPR-1176).
/// \details A new provider-write capability, separate from review-comment
/// publication: bounded blob, tree and one-parent commit creation, a new
/// branch that never overwrites, and one pull request, plus the reads that
/// check capability first and recover after uncertainty. It runs over the
/// account's existing native HTTPS authority; it never pushes with Git, calls
/// a credential helper or `gh`, forks, force-updates, merges or closes.
/// Every created identity is admitted only when it is exactly the one the
/// core computed.
namespace bpr::application {
	/// \brief The largest blob the catalog uploads; larger ones are prepared
	/// externally.
	inline constexpr std::uint64_t maximum_github_publication_blob_bytes = 128U * 1024U;

	/// \brief One repository under one account's authority.
	struct github_publication_target
	{
		/// The canonical host name the account's authority names.
		std::string             Host;
		github_server_contract  Server = github_server_contract::github_com_2022_11_28;
		core::github_account_id Account;
		std::string             Login;
		std::string             Owner;
		std::string             Repository;
		auto                    operator<=>(const github_publication_target&) const = default;
	};

	enum class github_publication_failure : std::uint8_t {
		/// Nothing usable came back: connection, timeout, server error.
		unavailable,
		/// The host, account or server contract is not admitted for native
		/// publication.
		unsupported_host,
		/// The server definitively refused the write (permission, protected
		/// rules, validation).
		refused,
		/// Rate limited; see `RetryAfterSeconds`.
		rate_limited,
		/// The answer does not parse or names another object, repository or
		/// identity than the one requested.
		mismatch,
		/// A blob over the admitted bound.
		too_large,
		cancelled,
	};

	/// \brief Whether the request may have reached the server.
	enum class github_publication_transmission : std::uint8_t {
		definitely_not_sent,
		indeterminate,
	};

	struct github_publication_error
	{
		github_publication_failure      Failure      = github_publication_failure::unavailable;
		github_publication_transmission Transmission = github_publication_transmission::definitely_not_sent;
		std::uint16_t                   HttpStatus   = 0;
		std::optional<std::uint32_t>    RetryAfterSeconds;
		auto                            operator<=>(const github_publication_error&) const = default;
	};

	template <typename T>
	using github_publication_result = std::expected<T, github_publication_error>;

	/// \brief What the repository allows this account, read before any write.
	struct github_publication_capability
	{
		/// The repository's own name and owner, as the server spells them.
		std::string Owner;
		std::string Repository;
		bool        CanPush  = false;
		bool        Archived = false;
		std::string DefaultBranch;
		auto        operator<=>(const github_publication_capability&) const = default;
	};

	/// \brief A commit as the server holds it.
	struct github_publication_commit
	{
		core::git_commit_oid              Commit;
		core::git_tree_oid                Tree;
		std::vector<core::git_commit_oid> Parents;
		auto                              operator<=>(const github_publication_commit&) const = default;
	};

	enum class github_branch_creation : std::uint8_t {
		created,
		/// A reference of that name already exists; nothing was changed.
		exists,
	};

	/// \brief The pull request fields the catalog creates.
	struct github_pull_request_draft
	{
		std::string Title;
		std::string Body;
		/// The new branch, in this repository.
		std::string Head;
		std::string Base;
		bool        Draft                                               = false;
		auto        operator<=>(const github_pull_request_draft&) const = default;
	};

	/// \brief A pull request as the server reports it.
	struct github_pull_request_observation
	{
		std::uint64_t        Number = 0;
		std::string          NodeId;
		std::string          Url;
		std::string          State;
		bool                 Draft = false;
		std::string          Title;
		std::string          HeadRef;
		std::string          HeadOwner;
		std::string          HeadRepository;
		core::git_commit_oid HeadCommit;
		std::string          BaseRef;
		core::git_commit_oid BaseCommit;
		std::string          Author;
		auto                 operator<=>(const github_pull_request_observation&) const = default;
	};

	class github_publication_port
	{
	public:
		virtual ~github_publication_port() = default;

		virtual auto observe_repository(const github_publication_target& aTarget, std::stop_token aStop) -> github_publication_result<github_publication_capability> = 0;
		/// \brief A branch's head, or nothing when it does not exist.
		virtual auto observe_branch(const github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
			-> github_publication_result<std::optional<core::git_commit_oid>> = 0;
		virtual auto observe_commit(const github_publication_target& aTarget, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> github_publication_result<std::optional<github_publication_commit>>                                                                                      = 0;
		virtual auto observe_tree(const github_publication_target& aTarget, const core::git_tree_oid& aTree, std::stop_token aStop) -> github_publication_result<bool>  = 0;
		virtual auto observe_blob(const github_publication_target& aTarget, const core::git_entry_oid& aBlob, std::stop_token aStop) -> github_publication_result<bool> = 0;

		/// \brief Uploads exactly these bytes; succeeds only if the server names them `aExpected`.
		virtual auto create_blob(const github_publication_target& aTarget, std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected, std::stop_token aStop)
			-> github_publication_result<void> = 0;
		/// \brief Creates `aExpected` from `aBase` with these entries added or
		/// replaced and these paths removed.
		virtual auto create_tree(const github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
								 const std::vector<std::string>& aDeletions, const core::git_tree_oid& aExpected, std::stop_token aStop) -> github_publication_result<void> = 0;
		/// \brief Creates exactly this commit; succeeds only if the server
		/// names it `aExpected` with the same tree and parents.
		virtual auto create_commit(const github_publication_target& aTarget, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected, std::stop_token aStop)
			-> github_publication_result<void> = 0;
		/// \brief Creates `refs/heads/<aBranch>` at `aCommit`. An existing
		/// reference is reported, never updated.
		virtual auto create_branch(const github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> github_publication_result<github_branch_creation> = 0;

		virtual auto create_pull_request(const github_publication_target& aTarget, const github_pull_request_draft& aDraft, std::stop_token aStop)
			-> github_publication_result<github_pull_request_observation> = 0;
		/// \brief Every pull request, open or closed, whose head is this
		/// repository's branch and whose base is `aBase`.
		virtual auto find_pull_requests(const github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
			-> github_publication_result<std::vector<github_pull_request_observation>> = 0;
		virtual auto observe_pull_request(const github_publication_target& aTarget, std::uint64_t aNumber, std::stop_token aStop)
			-> github_publication_result<std::optional<github_pull_request_observation>> = 0;
	};
}

#endif
