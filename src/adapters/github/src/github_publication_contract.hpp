#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PUBLICATION_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PUBLICATION_CONTRACT_HPP

#include "github_mutation_contract.hpp"
#include "github_read_contract.hpp"

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/github_publication_port.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

/// \file
/// \brief The GitHub REST contract of the publication catalog (RFC-BPR-0059,
/// BPR-1175, BPR-1176).
/// \details A closed list of exact requests — repository, branch, commit,
/// tree, blob and pull request reads; blob, tree, one-parent commit, new
/// branch and pull request creation — compiled from typed requests, and
/// strict parsers that admit an answer only when it names what was asked.
/// Nothing here holds a credential or performs an exchange.
namespace bpr::adapters::github::detail {
	/// The native transport's request body bound.
	inline constexpr std::size_t native_publication_body_limit = 256U * 1024U;

	enum class publication_operation : std::uint8_t {
		observe_repository,
		observe_branch,
		observe_commit,
		observe_tree,
		observe_blob,
		create_blob,
		create_tree,
		create_commit,
		create_branch,
		create_pull_request,
		find_pull_requests,
		observe_pull_request,
	};

	/// \brief What an operation's exchange is, derived from the operation alone.
	struct publication_exchange
	{
		http_method Method           = http_method::get;
		int         SuccessfulStatus = 200;
		/// The one other status whose answer is an observation, not a
		/// failure: 404 for a read of something absent, 422 for a branch or
		/// pull request that already exists. Zero admits none.
		int  AdmittedStatus                                 = 0;
		bool Write                                          = false;
		auto operator<=>(const publication_exchange&) const = default;
	};
	[[nodiscard]] auto publication_exchange_for(publication_operation aOperation) noexcept -> std::optional<publication_exchange>;

	/// \brief What a request asks, so a transport can recompile it exactly.
	struct publication_observe_repository
	{
		auto operator==(const publication_observe_repository&) const -> bool = default;
	};
	struct publication_observe_branch
	{
		std::string Branch;
		auto        operator==(const publication_observe_branch&) const -> bool = default;
	};
	struct publication_observe_commit
	{
		core::git_commit_oid Commit;
		auto                 operator==(const publication_observe_commit&) const -> bool = default;
	};
	struct publication_observe_tree
	{
		core::git_tree_oid Tree;
		auto               operator==(const publication_observe_tree&) const -> bool = default;
	};
	struct publication_observe_blob
	{
		core::git_entry_oid Blob;
		auto                operator==(const publication_observe_blob&) const -> bool = default;
	};
	struct publication_create_blob
	{
		std::vector<std::uint8_t> Bytes;
		auto                      operator==(const publication_create_blob&) const -> bool = default;
	};
	struct publication_create_tree
	{
		core::git_tree_oid                Base;
		std::vector<core::git_tree_entry> Changes;
		std::vector<std::string>          Deletions;
		auto                              operator==(const publication_create_tree&) const -> bool = default;
	};
	struct publication_create_commit
	{
		core::git_commit_content Commit;
		auto                     operator==(const publication_create_commit&) const -> bool = default;
	};
	struct publication_create_branch
	{
		std::string          Branch;
		core::git_commit_oid Commit;
		auto                 operator==(const publication_create_branch&) const -> bool = default;
	};
	struct publication_create_pull_request
	{
		application::github_pull_request_draft Draft;
		auto                                   operator==(const publication_create_pull_request&) const -> bool = default;
	};
	struct publication_find_pull_requests
	{
		std::string Head;
		std::string Base;
		auto        operator==(const publication_find_pull_requests&) const -> bool = default;
	};
	struct publication_observe_pull_request
	{
		std::uint64_t Number                                                            = 0;
		auto          operator==(const publication_observe_pull_request&) const -> bool = default;
	};
	using publication_semantic = std::variant<publication_observe_repository, publication_observe_branch, publication_observe_commit, publication_observe_tree, publication_observe_blob,
											  publication_create_blob, publication_create_tree, publication_create_commit, publication_create_branch, publication_create_pull_request,
											  publication_find_pull_requests, publication_observe_pull_request>;

	struct publication_wire_request
	{
		publication_semantic                   Semantic;
		application::github_publication_target Target;
		publication_operation                  Operation = publication_operation::observe_repository;
		http_method                            Method    = http_method::get;
		std::string                            Path;
		std::string                            Query;
		std::vector<http_header>               Headers;
		std::string                            Body;
		auto                                   operator==(const publication_wire_request&) const -> bool = default;
	};

	using publication_wire_response = review_mutation_wire_response;

	enum class publication_contract_failure : std::uint8_t {
		invalid_request,
		unexpected_status,
		malformed_response,
		identity_mismatch,
	};
	template <typename T>
	using publication_contract_result = std::expected<T, publication_contract_failure>;

	/// \brief The admitted server contracts for native publication: GitHub.com only.
	[[nodiscard]] auto admitted_publication_server(application::github_server_contract aServer) noexcept -> bool;

	[[nodiscard]] auto compile_observe_repository(const application::github_publication_target& aTarget) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_create_blob(const application::github_publication_target& aTarget, std::span<const std::uint8_t> aBytes) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
										   const std::vector<std::string>& aDeletions) -> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit)
		-> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit)
		-> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft)
		-> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase)
		-> publication_contract_result<publication_wire_request>;
	[[nodiscard]] auto compile_observe_pull_request(const application::github_publication_target& aTarget, std::uint64_t aNumber) -> publication_contract_result<publication_wire_request>;

	/// \brief Compiles one typed request.
	[[nodiscard]] auto compile_publication(const application::github_publication_target& aTarget, const publication_semantic& aSemantic)
		-> publication_contract_result<publication_wire_request>;
	/// \brief True when the request is exactly what the compiler produces
	/// for its own typed request and target; a transport refuses anything else.
	[[nodiscard]] auto validate_outbound_publication_request(const publication_wire_request& aRequest) -> bool;

	[[nodiscard]] auto parse_repository(const publication_wire_response& aResponse, const application::github_publication_target& aTarget)
		-> publication_contract_result<application::github_publication_capability>;
	[[nodiscard]] auto parse_branch(const publication_wire_response& aResponse, const std::string& aBranch, core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::optional<core::git_commit_oid>>;
	[[nodiscard]] auto parse_commit(const publication_wire_response& aResponse, const core::git_commit_oid& aCommit) -> publication_contract_result<std::optional<application::github_publication_commit>>;
	[[nodiscard]] auto parse_tree_presence(const publication_wire_response& aResponse, const core::git_tree_oid& aTree) -> publication_contract_result<bool>;
	[[nodiscard]] auto parse_blob_presence(const publication_wire_response& aResponse, const core::git_entry_oid& aBlob) -> publication_contract_result<bool>;
	/// \brief The identity the server gave a created blob, tree or commit.
	[[nodiscard]] auto parse_created_sha(const publication_wire_response& aResponse, core::git_hash_algorithm aFormat) -> publication_contract_result<std::array<std::uint8_t, 32>>;
	[[nodiscard]] auto parse_created_commit(const publication_wire_response& aResponse, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected)
		-> publication_contract_result<void>;
	[[nodiscard]] auto parse_created_branch(const publication_wire_response& aResponse, const std::string& aBranch, const core::git_commit_oid& aCommit)
		-> publication_contract_result<application::github_branch_creation>;
	[[nodiscard]] auto parse_pull_request(const publication_wire_response& aResponse, core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::optional<application::github_pull_request_observation>>;
	/// \brief Every pull request of one listing; a further page is refused.
	[[nodiscard]] auto parse_pull_requests(const publication_wire_response& aResponse, core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::vector<application::github_pull_request_observation>>;

	/// \brief The ISO 8601 form of a signature's time and offset, as the
	/// commit endpoint takes it.
	[[nodiscard]] auto publication_date(const core::git_signature& aSignature) -> std::string;

	/// \brief One exchange under one account's credential context.
	struct github_publication_wire_call
	{
		application::github_credential_operation_context Context;
		publication_wire_request                         Wire;
	};

	struct github_publication_wire_success
	{
		publication_wire_response Response;
	};
	struct github_publication_wire_error
	{
		application::github_provider_error Error{
			.Failure           = application::github_provider_failure::unavailable,
			.RetryAfterSeconds = std::nullopt,
			.RateLimits        = { },
			.RateLimitCause    = std::nullopt,
		};
		github_mutation_transmission Transmission = github_mutation_transmission::definitely_not_sent;
	};
	struct github_publication_wire_cancelled
	{
		github_mutation_transmission Transmission = github_mutation_transmission::definitely_not_sent;
	};
	using github_publication_wire_completion = std::variant<github_publication_wire_success, github_publication_wire_error, github_publication_wire_cancelled>;

	/// \brief The account-bound transport a publication session runs over.
	class github_publication_wire_transport
	{
	public:
		github_publication_wire_transport()                                                            = default;
		github_publication_wire_transport(const github_publication_wire_transport&)                    = delete;
		auto operator=(const github_publication_wire_transport&) -> github_publication_wire_transport& = delete;
		github_publication_wire_transport(github_publication_wire_transport&&)                         = delete;
		auto operator=(github_publication_wire_transport&&) -> github_publication_wire_transport&      = delete;
		virtual ~github_publication_wire_transport()                                                   = default;

		[[nodiscard]] virtual auto perform_publication(const github_publication_wire_call& aCall, std::stop_token aStop) -> github_publication_wire_completion = 0;
	};

	/// \brief The publication catalog over one account's transport.
	/// \details Refuses any target whose host, account or server contract is
	/// not the credential's, and any server contract not admitted for native
	/// publication, before a request is built.
	class github_publication_session final : public application::github_publication_port
	{
	public:
		github_publication_session(github_publication_wire_transport& aTransport, application::github_credential_operation_context aContext) noexcept;

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token aStop)
			-> application::github_publication_result<application::github_publication_capability> override;
		auto observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override;
		auto observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override;
		auto observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree, std::stop_token aStop) -> application::github_publication_result<bool> override;
		auto observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob, std::stop_token aStop) -> application::github_publication_result<bool> override;
		auto create_blob(const application::github_publication_target& aTarget, std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override;
		auto create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
						 const std::vector<std::string>& aDeletions, const core::git_tree_oid& aExpected, std::stop_token aStop) -> application::github_publication_result<void> override;
		auto create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override;
		auto create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<application::github_branch_creation> override;
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token aStop)
			-> application::github_publication_result<application::github_pull_request_observation> override;
		auto find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override;
		auto observe_pull_request(const application::github_publication_target& aTarget, std::uint64_t aNumber, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override;

	private:
		/// Admission, then one exchange; a non-success status the operation
		/// does not admit, or a transport failure, is an error.
		[[nodiscard]] auto exchange(const application::github_publication_target& aTarget, publication_contract_result<publication_wire_request> aRequest, std::stop_token aStop)
			-> application::github_publication_result<publication_wire_response>;
		[[nodiscard]] auto admits(const application::github_publication_target& aTarget) const noexcept -> bool;

		github_publication_wire_transport*               Transport = nullptr;
		application::github_credential_operation_context Context;
	};
}

#endif
