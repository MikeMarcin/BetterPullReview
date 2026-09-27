#include "designated_live_mutation_gate.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#if !defined(_WIN32)
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::tests::network_isolation {
	namespace {

		constexpr std::size_t      ApprovalLimit     = 4096;
		constexpr std::string_view ApprovalVersion   = "bpr.designated-live-approval/v1";
		constexpr std::string_view CapabilityVersion = "bpr.designated-live-capability/v1";

		struct parsed_approval
		{
			designated_live_identity Identity;
			std::string              CanarySurfaceSha256;
			std::string              Nonce;
		};

		[[nodiscard]] bool is_lower_hex_digest(std::string_view aValue)
		{
			if (aValue.size() != 64) {
				return false;
			}
			for (const char value : aValue) {
				if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] std::optional<std::uint64_t> parse_unsigned(std::string_view aText)
		{
			std::uint64_t value{ };
			const auto*   begin  = aText.data();
			const auto*   end    = begin + aText.size();
			const auto    result = std::from_chars(begin, end, value);
			if (result.ec != std::errc{ } || result.ptr != end) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] std::optional<parsed_approval> parse_approval(std::string_view aBody)
		{
			constexpr std::array<std::string_view, 15> keys{
				"host",
				"owner",
				"repository",
				"repository_database_id",
				"repository_node_id",
				"pull_request_number",
				"pull_request_database_id",
				"pull_request_node_id",
				"base_repository_database_id",
				"state",
				"head_sha",
				"base_sha",
				"head_branch",
				"canary_surface_sha256",
				"session_nonce",
			};
			std::array<std::string_view, keys.size()> values;
			std::size_t                               cursor{ };
			const auto                                takeLine = [&aBody, &cursor]() -> std::optional<std::string_view> {
				if (cursor >= aBody.size()) {
					return std::nullopt;
				}
				const auto end = aBody.find('\n', cursor);
				if (end == std::string_view::npos) {
					return std::nullopt;
				}
				const auto line = aBody.substr(cursor, end - cursor);
				cursor          = end + 1;
				return line;
			};

			const auto version = takeLine();
			if (!version || *version != ApprovalVersion) {
				return std::nullopt;
			}
			for (std::size_t index = 0; index < keys.size(); ++index) {
				const auto line = takeLine();
				if (!line) {
					return std::nullopt;
				}
				const auto prefix = std::string(keys[index]) + "=";
				if (!line->starts_with(prefix)) {
					return std::nullopt;
				}
				values[index] = line->substr(prefix.size());
				if (values[index].empty()) {
					return std::nullopt;
				}
			}
			if (cursor != aBody.size()) {
				return std::nullopt;
			}

			const auto repositoryId      = parse_unsigned(values[3]);
			const auto pullRequestNumber = parse_unsigned(values[5]);
			const auto pullRequestId     = parse_unsigned(values[6]);
			const auto baseRepositoryId  = parse_unsigned(values[8]);
			if (!repositoryId || !pullRequestNumber || !pullRequestId || !baseRepositoryId || !is_lower_hex_digest(values[13]) || !is_lower_hex_digest(values[14])) {
				return std::nullopt;
			}

			return parsed_approval{
				.Identity = designated_live_identity{
					.Host                     = std::string(values[0]),
					.Owner                    = std::string(values[1]),
					.Repository               = std::string(values[2]),
					.RepositoryDatabaseId     = *repositoryId,
					.RepositoryNodeId         = std::string(values[4]),
					.PullRequestNumber        = *pullRequestNumber,
					.PullRequestDatabaseId    = *pullRequestId,
					.PullRequestNodeId        = std::string(values[7]),
					.BaseRepositoryDatabaseId = *baseRepositoryId,
					.State                    = std::string(values[9]),
					.HeadSha                  = std::string(values[10]),
					.BaseSha                  = std::string(values[11]),
					.HeadBranch               = std::string(values[12]),
				},
				.CanarySurfaceSha256 = std::string(values[13]),
				.Nonce               = std::string(values[14]),
			};
		}

		[[nodiscard]] live_gate_status status_for(approval_read_status aStatus)
		{
			switch (aStatus) {
			case approval_read_status::missing:
				return live_gate_status::approval_missing;
			case approval_read_status::unsafe:
				return live_gate_status::approval_unsafe;
			case approval_read_status::read_failed:
				return live_gate_status::approval_read_failed;
			case approval_read_status::oversize:
				return live_gate_status::approval_oversize;
			case approval_read_status::unsupported_platform:
				return live_gate_status::approval_unsupported_platform;
			case approval_read_status::ready:
				break;
			}
			return live_gate_status::approval_read_failed;
		}

		[[nodiscard]] live_gate_status status_for(remote_read_status aStatus)
		{
			switch (aStatus) {
			case remote_read_status::network_failure:
				return live_gate_status::remote_network_failure;
			case remote_read_status::timeout:
				return live_gate_status::remote_timeout;
			case remote_read_status::http_failure:
				return live_gate_status::remote_http_failure;
			case remote_read_status::malformed:
				return live_gate_status::remote_malformed;
			case remote_read_status::oversize:
				return live_gate_status::remote_oversize;
			case remote_read_status::ready:
				break;
			}
			return live_gate_status::remote_network_failure;
		}

#if !defined(_WIN32)
		constexpr char ClaimDirectoryName[]  = ".bpr-designated-live-approval-claim-v1";
		constexpr char ClaimedApprovalName[] = "approval";

		[[nodiscard]] bool safe_private_file_stat(const struct stat& aStatus)
		{
			return S_ISREG(aStatus.st_mode) && aStatus.st_uid == geteuid() && (aStatus.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0 && (aStatus.st_mode & S_IRUSR) != 0 && aStatus.st_nlink == 1;
		}

		[[nodiscard]] bool safe_private_directory_stat(const struct stat& aStatus)
		{
			return S_ISDIR(aStatus.st_mode) && aStatus.st_uid == geteuid() && (aStatus.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0 && (aStatus.st_mode & S_IRWXU) == S_IRWXU;
		}

		[[nodiscard]] bool same_object(const struct stat& aLeft, const struct stat& aRight)
		{
			return aLeft.st_dev == aRight.st_dev && aLeft.st_ino == aRight.st_ino;
		}

		class posix_approval_authority final : public approval_snapshot_authority
		{
		public:
			~posix_approval_authority() override
			{
				release_claim();
				if (ApprovalDescriptor >= 0) {
					close(ApprovalDescriptor);
				}
				if (ParentDescriptor >= 0) {
					close(ParentDescriptor);
				}
			}

			void release_claim()
			{
				if (ClaimDescriptor >= 0) {
					static_cast<void>(unlinkat(ClaimDescriptor, ClaimedApprovalName, 0));
					close(ClaimDescriptor);
					ClaimDescriptor = -1;
				}
				if (ClaimCreated && ParentDescriptor >= 0) {
					static_cast<void>(unlinkat(ParentDescriptor, ClaimDirectoryName, AT_REMOVEDIR));
					ClaimCreated = false;
				}
			}

			int         ParentDescriptor{ -1 };
			int         ApprovalDescriptor{ -1 };
			int         ClaimDescriptor{ -1 };
			std::string ApprovalName;
			bool        ClaimCreated{ };
			bool        Consumed{ };
			std::mutex  Mutex;
		};

		[[nodiscard]] approval_read_result inspect_posix(const std::filesystem::path& aPath)
		{
			if (!aPath.is_absolute() || aPath.filename().empty()) {
				return { .Status = approval_read_status::unsafe };
			}
			const auto parentPath   = aPath.parent_path();
			const auto approvalName = aPath.filename().string();
			if (approvalName == "." || approvalName == "..") {
				return { .Status = approval_read_status::unsafe };
			}

			auto authority              = std::make_shared<posix_approval_authority>();
			authority->ParentDescriptor = open(parentPath.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
			if (authority->ParentDescriptor < 0) {
				return { .Status = errno == ELOOP ? approval_read_status::unsafe : approval_read_status::read_failed };
			}
			struct stat parentStatus{ };
			if (fstat(authority->ParentDescriptor, &parentStatus) != 0 || !safe_private_directory_stat(parentStatus)) {
				return { .Status = approval_read_status::unsafe };
			}

			authority->ApprovalDescriptor = openat(authority->ParentDescriptor, approvalName.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			if (authority->ApprovalDescriptor < 0) {
				return { .Status = errno == ENOENT ? approval_read_status::missing : (errno == ELOOP ? approval_read_status::unsafe : approval_read_status::read_failed) };
			}
			struct stat opened{ };
			if (fstat(authority->ApprovalDescriptor, &opened) != 0 || !safe_private_file_stat(opened)) {
				return { .Status = approval_read_status::unsafe };
			}
			authority->ApprovalName = approvalName;

			std::string body;
			body.reserve(ApprovalLimit);
			std::array<char, 1024> buffer{ };
			for (;;) {
				const auto count = read(authority->ApprovalDescriptor, buffer.data(), buffer.size());
				if (count == 0) {
					break;
				}
				if (count < 0) {
					if (errno == EINTR) {
						continue;
					}
					return { .Status = approval_read_status::read_failed };
				}
				body.append(buffer.data(), static_cast<std::size_t>(count));
				if (body.size() > ApprovalLimit) {
					return { .Status = approval_read_status::oversize };
				}
			}
			return {
				.Status   = approval_read_status::ready,
				.Snapshot = approval_snapshot{
					.Body      = std::move(body),
					.Path      = aPath,
					.Device    = static_cast<std::uint64_t>(opened.st_dev),
					.Inode     = static_cast<std::uint64_t>(opened.st_ino),
					.Authority = std::move(authority),
				},
			};
		}
#endif

	} // namespace

	designated_live_identity designated_identity()
	{
		return {
			.Host                     = "github.com",
			.Owner                    = "MikeMarcin",
			.Repository               = "BPR-Test",
			.RepositoryDatabaseId     = 1328277631,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestNumber        = 2,
			.PullRequestDatabaseId    = 4650736621,
			.PullRequestNodeId        = "PR_kwDOTyvof88AAAABFTSb7Q",
			.BaseRepositoryDatabaseId = 1328277631,
			.State                    = "open",
			.HeadSha                  = "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a",
			.BaseSha                  = "a939a85a5525994fe9e9cb4c973ad9362ed427c7",
			.HeadBranch               = "codex/rfc0022-live-comments-20260926",
		};
	}

	identity_field first_identity_mismatch(const designated_live_identity& aObserved)
	{
		const auto expected = designated_identity();
		if (aObserved.Host != expected.Host) {
			return identity_field::host;
		}
		if (aObserved.Owner != expected.Owner) {
			return identity_field::owner;
		}
		if (aObserved.Repository != expected.Repository) {
			return identity_field::repository;
		}
		if (aObserved.RepositoryDatabaseId != expected.RepositoryDatabaseId) {
			return identity_field::repository_database_id;
		}
		if (aObserved.RepositoryNodeId != expected.RepositoryNodeId) {
			return identity_field::repository_node_id;
		}
		if (aObserved.PullRequestNumber != expected.PullRequestNumber) {
			return identity_field::pull_request_number;
		}
		if (aObserved.PullRequestDatabaseId != expected.PullRequestDatabaseId) {
			return identity_field::pull_request_database_id;
		}
		if (aObserved.PullRequestNodeId != expected.PullRequestNodeId) {
			return identity_field::pull_request_node_id;
		}
		if (aObserved.BaseRepositoryDatabaseId != expected.BaseRepositoryDatabaseId) {
			return identity_field::base_repository_database_id;
		}
		if (aObserved.State != expected.State) {
			return identity_field::state;
		}
		if (aObserved.HeadSha != expected.HeadSha) {
			return identity_field::head_sha;
		}
		if (aObserved.BaseSha != expected.BaseSha) {
			return identity_field::base_sha;
		}
		if (aObserved.HeadBranch != expected.HeadBranch) {
			return identity_field::head_branch;
		}
		return identity_field::none;
	}

	std::string_view name_of(identity_field aField)
	{
		switch (aField) {
		case identity_field::none:
			return "none";
		case identity_field::host:
			return "host";
		case identity_field::owner:
			return "owner";
		case identity_field::repository:
			return "repository";
		case identity_field::repository_database_id:
			return "repository_database_id";
		case identity_field::repository_node_id:
			return "repository_node_id";
		case identity_field::pull_request_number:
			return "pull_request_number";
		case identity_field::pull_request_database_id:
			return "pull_request_database_id";
		case identity_field::pull_request_node_id:
			return "pull_request_node_id";
		case identity_field::base_repository_database_id:
			return "base_repository_database_id";
		case identity_field::state:
			return "state";
		case identity_field::head_sha:
			return "head_sha";
		case identity_field::base_sha:
			return "base_sha";
		case identity_field::head_branch:
			return "head_branch";
		}
		return "unknown";
	}

	live_gate_result designated_live_gate::run(
		approval_artifact_port& aApproval,
		remote_identity_port&   aRemote,
		capability_child_port&  aChild) const
	{
		const auto approval = aApproval.inspect();
		if (approval.Status != approval_read_status::ready) {
			return { .Status = status_for(approval.Status) };
		}
		const auto parsed = parse_approval(approval.Snapshot.Body);
		if (!parsed) {
			return { .Status = live_gate_status::approval_malformed };
		}
		const auto approvalMismatch = first_identity_mismatch(parsed->Identity);
		if (approvalMismatch != identity_field::none) {
			return {
				.Status   = live_gate_status::approval_identity_mismatch,
				.Mismatch = approvalMismatch,
			};
		}

		const auto remote = aRemote.read();
		if (remote.Status != remote_read_status::ready) {
			return { .Status = status_for(remote.Status) };
		}
		const auto remoteMismatch = first_identity_mismatch(remote.Identity);
		if (remoteMismatch != identity_field::none) {
			return {
				.Status   = live_gate_status::remote_identity_mismatch,
				.Mismatch = remoteMismatch,
			};
		}

		switch (aApproval.consume(approval.Snapshot)) {
		case approval_consume_status::missing:
			return { .Status = live_gate_status::approval_consume_missing };
		case approval_consume_status::substituted:
			return { .Status = live_gate_status::approval_substituted };
		case approval_consume_status::failed:
			return { .Status = live_gate_status::approval_consume_failed };
		case approval_consume_status::consumed:
			break;
		}

		const auto child = aChild.start(capability_for(parsed->Nonce), parsed->CanarySurfaceSha256);
		if (child.Status == child_run_status::not_started
			|| child.Status == child_run_status::parent_thread_state_failed
			|| child.Status == child_run_status::parent_multithreaded
			|| child.Status == child_run_status::descriptor_inventory_failed
			|| child.Status == child_run_status::descriptor_inventory_unstable) {
			return { .Status = live_gate_status::child_start_failed };
		}
		if (child.Status == child_run_status::timed_out) {
			return {
				.Status           = live_gate_status::child_timed_out,
				.ChildLaunchCount = 1,
			};
		}
		if (child.Status == child_run_status::failed) {
			return {
				.Status           = live_gate_status::child_failed,
				.ChildLaunchCount = 1,
			};
		}
		return {
			.Status           = live_gate_status::launched,
			.ChildLaunchCount = 1,
		};
	}

	std::string approval_body_for(
		const designated_live_identity& aIdentity,
		std::string_view                aCanarySurfaceSha256,
		std::string_view                aSessionNonce)
	{
		std::string result;
		result.reserve(768);
		result.append(ApprovalVersion).append("\n");
		result.append("host=").append(aIdentity.Host).append("\n");
		result.append("owner=").append(aIdentity.Owner).append("\n");
		result.append("repository=").append(aIdentity.Repository).append("\n");
		result.append("repository_database_id=").append(std::to_string(aIdentity.RepositoryDatabaseId)).append("\n");
		result.append("repository_node_id=").append(aIdentity.RepositoryNodeId).append("\n");
		result.append("pull_request_number=").append(std::to_string(aIdentity.PullRequestNumber)).append("\n");
		result.append("pull_request_database_id=").append(std::to_string(aIdentity.PullRequestDatabaseId)).append("\n");
		result.append("pull_request_node_id=").append(aIdentity.PullRequestNodeId).append("\n");
		result.append("base_repository_database_id=").append(std::to_string(aIdentity.BaseRepositoryDatabaseId)).append("\n");
		result.append("state=").append(aIdentity.State).append("\n");
		result.append("head_sha=").append(aIdentity.HeadSha).append("\n");
		result.append("base_sha=").append(aIdentity.BaseSha).append("\n");
		result.append("head_branch=").append(aIdentity.HeadBranch).append("\n");
		result.append("canary_surface_sha256=").append(aCanarySurfaceSha256).append("\n");
		result.append("session_nonce=").append(aSessionNonce).append("\n");
		return result;
	}

	std::string capability_for(std::string_view aSessionNonce)
	{
		std::string result;
		result.reserve(128);
		result.append(CapabilityVersion).append("\n");
		result.append("session_nonce=").append(aSessionNonce).append("\n");
		return result;
	}

	bool exact_runner_arguments(const std::vector<std::string_view>& aArguments)
	{
		return aArguments.size() == 3 && aArguments[1] == "--approval" && !aArguments[2].empty();
	}

	std::string_view name_of(live_gate_status aStatus)
	{
		switch (aStatus) {
		case live_gate_status::launched:
			return "launched";
		case live_gate_status::approval_missing:
			return "approval_missing";
		case live_gate_status::approval_unsafe:
			return "approval_unsafe";
		case live_gate_status::approval_read_failed:
			return "approval_read_failed";
		case live_gate_status::approval_oversize:
			return "approval_oversize";
		case live_gate_status::approval_unsupported_platform:
			return "approval_unsupported_platform";
		case live_gate_status::approval_malformed:
			return "approval_malformed";
		case live_gate_status::approval_identity_mismatch:
			return "approval_identity_mismatch";
		case live_gate_status::remote_network_failure:
			return "remote_network_failure";
		case live_gate_status::remote_timeout:
			return "remote_timeout";
		case live_gate_status::remote_http_failure:
			return "remote_http_failure";
		case live_gate_status::remote_malformed:
			return "remote_malformed";
		case live_gate_status::remote_oversize:
			return "remote_oversize";
		case live_gate_status::remote_identity_mismatch:
			return "remote_identity_mismatch";
		case live_gate_status::approval_consume_missing:
			return "approval_consume_missing";
		case live_gate_status::approval_substituted:
			return "approval_substituted";
		case live_gate_status::approval_consume_failed:
			return "approval_consume_failed";
		case live_gate_status::child_start_failed:
			return "child_start_failed";
		case live_gate_status::child_failed:
			return "child_failed";
		case live_gate_status::child_timed_out:
			return "child_timed_out";
		}
		return "unknown";
	}

	native_approval_artifact::native_approval_artifact(
		std::filesystem::path  aPath,
		approval_consume_hook* aHook)
		: Path(std::move(aPath))
		, Hook(aHook)
	{
	}

	approval_read_result native_approval_artifact::inspect()
	{
#if defined(_WIN32)
		return { .Status = approval_read_status::unsupported_platform };
#else
		return inspect_posix(Path);
#endif
	}

	approval_consume_status native_approval_artifact::consume(
		const approval_snapshot& aSnapshot)
	{
#if defined(_WIN32)
		static_cast<void>(aSnapshot);
		return approval_consume_status::failed;
#else
		auto authority = std::dynamic_pointer_cast<posix_approval_authority>(aSnapshot.Authority);
		if (!authority || aSnapshot.Path != Path) {
			return approval_consume_status::substituted;
		}
		std::scoped_lock lock(authority->Mutex);
		if (authority->Consumed) {
			return approval_consume_status::missing;
		}

		struct stat retained{ };
		if (fstat(authority->ApprovalDescriptor, &retained) != 0 || !safe_private_file_stat(retained) || static_cast<std::uint64_t>(retained.st_dev) != aSnapshot.Device || static_cast<std::uint64_t>(retained.st_ino) != aSnapshot.Inode) {
			return approval_consume_status::substituted;
		}
		if (mkdirat(authority->ParentDescriptor, ClaimDirectoryName, S_IRWXU) != 0) {
			return approval_consume_status::failed;
		}
		authority->ClaimCreated    = true;
		authority->ClaimDescriptor = openat(authority->ParentDescriptor, ClaimDirectoryName, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		struct stat claimDirectory{ };
		if (authority->ClaimDescriptor < 0 || fstat(authority->ClaimDescriptor, &claimDirectory) != 0 || !safe_private_directory_stat(claimDirectory)) {
			authority->release_claim();
			return approval_consume_status::failed;
		}

		struct stat current{ };
		if (fstatat(authority->ParentDescriptor, authority->ApprovalName.c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0) {
			const auto error = errno;
			authority->release_claim();
			return error == ENOENT ? approval_consume_status::missing : approval_consume_status::failed;
		}
		if (!same_object(current, retained)) {
			authority->release_claim();
			return approval_consume_status::substituted;
		}
		if (Hook != nullptr) {
			Hook->after_current_identity_check();
		}
		if (renameat(authority->ParentDescriptor, authority->ApprovalName.c_str(), authority->ClaimDescriptor, ClaimedApprovalName) != 0) {
			const auto error = errno;
			authority->release_claim();
			return error == ENOENT ? approval_consume_status::missing : approval_consume_status::failed;
		}

		struct stat claimed{ };
		if (fstatat(authority->ClaimDescriptor, ClaimedApprovalName, &claimed, AT_SYMLINK_NOFOLLOW) != 0 || !same_object(claimed, retained)) {
			authority->release_claim();
			return approval_consume_status::substituted;
		}
		if (unlinkat(authority->ClaimDescriptor, ClaimedApprovalName, 0) != 0) {
			authority->release_claim();
			return approval_consume_status::failed;
		}
		struct stat unlinked{ };
		if (fstat(authority->ApprovalDescriptor, &unlinked) != 0 || !same_object(unlinked, retained) || unlinked.st_nlink != 0) {
			authority->release_claim();
			return approval_consume_status::failed;
		}
		authority->Consumed = true;
		return approval_consume_status::consumed;
#endif
	}

} // namespace bpr::tests::network_isolation
