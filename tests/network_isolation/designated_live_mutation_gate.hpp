#ifndef BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_GATE_HPP
#define BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_GATE_HPP

#include "designated_live_mutation_child.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::tests::network_isolation {

	struct designated_live_identity
	{
		std::string   Host;
		std::string   Owner;
		std::string   Repository;
		std::uint64_t RepositoryDatabaseId{ };
		std::string   RepositoryNodeId;
		std::uint64_t PullRequestNumber{ };
		std::uint64_t PullRequestDatabaseId{ };
		std::string   PullRequestNodeId;
		std::uint64_t BaseRepositoryDatabaseId{ };
		std::string   State;
		std::string   HeadSha;
		std::string   BaseSha;
		std::string   HeadBranch;
	};

	[[nodiscard]] designated_live_identity designated_identity();

	enum class identity_field {
		none,
		host,
		owner,
		repository,
		repository_database_id,
		repository_node_id,
		pull_request_number,
		pull_request_database_id,
		pull_request_node_id,
		base_repository_database_id,
		state,
		head_sha,
		base_sha,
		head_branch,
	};

	[[nodiscard]] identity_field first_identity_mismatch(
		const designated_live_identity& aObserved);
	[[nodiscard]] std::string_view name_of(identity_field aField);

	class approval_snapshot_authority
	{
	public:
		virtual ~approval_snapshot_authority() = default;
	};

	struct approval_snapshot
	{
		std::string                                  Body;
		std::filesystem::path                        Path;
		std::uint64_t                                Device{ };
		std::uint64_t                                Inode{ };
		std::shared_ptr<approval_snapshot_authority> Authority;
	};

	enum class approval_read_status {
		ready,
		missing,
		unsafe,
		read_failed,
		oversize,
		unsupported_platform,
	};

	struct approval_read_result
	{
		approval_read_status Status{ approval_read_status::read_failed };
		approval_snapshot    Snapshot;
	};

	enum class approval_consume_status {
		consumed,
		missing,
		substituted,
		failed,
	};

	class approval_artifact_port
	{
	public:
		virtual ~approval_artifact_port()                       = default;
		[[nodiscard]] virtual approval_read_result    inspect() = 0;
		[[nodiscard]] virtual approval_consume_status consume(
			const approval_snapshot& aSnapshot)
			= 0;
	};

	enum class remote_read_status {
		ready,
		network_failure,
		timeout,
		http_failure,
		malformed,
		oversize,
	};

	struct remote_read_result
	{
		remote_read_status       Status{ remote_read_status::network_failure };
		designated_live_identity Identity;
	};

	class remote_identity_port
	{
	public:
		virtual ~remote_identity_port()                 = default;
		[[nodiscard]] virtual remote_read_result read() = 0;
	};

	class capability_child_port
	{
	public:
		virtual ~capability_child_port() = default;
		[[nodiscard]] virtual child_start_result start(
			std::string_view aCapability,
			std::string_view aExpectedCanarySurfaceSha256)
			= 0;
	};

	enum class live_gate_status {
		launched,
		approval_missing,
		approval_unsafe,
		approval_read_failed,
		approval_oversize,
		approval_unsupported_platform,
		approval_malformed,
		approval_identity_mismatch,
		remote_network_failure,
		remote_timeout,
		remote_http_failure,
		remote_malformed,
		remote_oversize,
		remote_identity_mismatch,
		approval_consume_missing,
		approval_substituted,
		approval_consume_failed,
		child_start_failed,
		child_failed,
		child_timed_out,
	};

	struct live_gate_result
	{
		live_gate_status Status{ live_gate_status::approval_read_failed };
		identity_field   Mismatch{ identity_field::none };
		unsigned         ChildLaunchCount{ };
	};

	class designated_live_gate
	{
	public:
		[[nodiscard]] live_gate_result run(
			approval_artifact_port& aApproval,
			remote_identity_port&   aRemote,
			capability_child_port&  aChild) const;
	};

	[[nodiscard]] std::string approval_body_for(
		const designated_live_identity& aIdentity,
		std::string_view                aCanarySurfaceSha256,
		std::string_view                aSessionNonce);
	[[nodiscard]] std::string      capability_for(std::string_view aSessionNonce);
	[[nodiscard]] std::string_view name_of(live_gate_status aStatus);
	[[nodiscard]] bool             exact_runner_arguments(
		const std::vector<std::string_view>& aArguments);
	class approval_consume_hook
	{
	public:
		virtual ~approval_consume_hook()            = default;
		virtual void after_current_identity_check() = 0;
	};

	class native_approval_artifact final : public approval_artifact_port
	{
	public:
		explicit native_approval_artifact(
			std::filesystem::path  aPath,
			approval_consume_hook* aHook = nullptr);

		[[nodiscard]] approval_read_result    inspect() override;
		[[nodiscard]] approval_consume_status consume(
			const approval_snapshot& aSnapshot) override;

	private:
		std::filesystem::path  Path;
		approval_consume_hook* Hook{ };
	};

} // namespace bpr::tests::network_isolation

#endif
