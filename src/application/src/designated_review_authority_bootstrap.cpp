#include "review_mutation_execution_authority.hpp"

#include "bpr/core/sha256.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <mach-o/dyld.h>
		#include <sys/acl.h>
	#endif
	#if defined(__linux__)
		#include <linux/fs.h>
		#include <sys/syscall.h>
	#endif
#endif

namespace bpr::application::detail {
	namespace {
		constexpr auto maximum_approval_bytes   = std::size_t{ 8U * 1024U };
		constexpr auto maximum_executable_bytes = std::size_t{ 256U * 1024U * 1024U };
		constexpr auto authority_lifetime       = std::chrono::minutes{ 15 };
		constexpr char spent_directory_name[]   = ".bpr-spent";

		constexpr auto designated_hostname      = std::string_view{ "github.com" };
		constexpr auto designated_owner         = std::string_view{ "MikeMarcin" };
		constexpr auto designated_repository    = std::string_view{ "BPR-Test" };
		constexpr auto designated_repository_id = std::uint64_t{ 1'328'277'631U };
		constexpr auto designated_repository_node
			= std::string_view{ "R_kgDOTyvofw" };
		constexpr auto designated_pull_request_number = std::uint32_t{ 1U };
		constexpr auto designated_pull_request_id
			= std::uint64_t{ 4'246'446'933U };
		constexpr auto designated_pull_request_node
			= std::string_view{ "PR_kwDOTyvof879G6NV" };
		constexpr auto designated_base_ref = std::string_view{ "main" };
		constexpr auto designated_head_ref
			= std::string_view{ "codex/disposable-live-review-20260810" };
		constexpr auto designated_base_oid
			= std::string_view{ "e2a44aa8fef704de7373463222b1872e796918c6" };
		constexpr auto designated_head_oid
			= std::string_view{ "17990ba0c2fb50338141f35c0717f0f99639c7ba" };
		constexpr auto designated_approval_statement = std::string_view{
			"I authorize this exact BetterPullReview build to publish one source-line "
			"review comment to MikeMarcin/BPR-Test#1 at head "
			"17990ba0c2fb50338141f35c0717f0f99639c7ba."
		};

		[[nodiscard]] auto lowercase_sha256(const std::string_view aText) noexcept
			-> bool
		{
			if (aText.size() != 64U) {
				return false;
			}
			for (const auto character : aText) {
				if (!((character >= '0' && character <= '9')
					  || (character >= 'a' && character <= 'f'))) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto approval_id_from_subject(
			const std::string_view aSubject) -> std::optional<std::string>
		{
			constexpr auto field  = std::string_view{ "approval_id=" };
			const auto     offset = aSubject.find(field);
			if (offset == std::string_view::npos
				|| (offset != 0U && aSubject[offset - 1U] != '\n')
				|| aSubject.find(field, offset + field.size())
					!= std::string_view::npos) {
				return std::nullopt;
			}
			const auto first = offset + field.size();
			const auto last  = aSubject.find('\n', first);
			if (last == std::string_view::npos
				|| !lowercase_sha256(aSubject.substr(first, last - first))) {
				return std::nullopt;
			}
			return std::string{ aSubject.substr(first, last - first) };
		}

#if defined(BPR_APPLICATION_TESTING)
		std::atomic<std::uint32_t> FsyncFailureCountdown = 0U;
#endif

		[[nodiscard]] auto durable_sync(const int aDescriptor) noexcept -> bool
		{
#if defined(_WIN32)
			static_cast<void>(aDescriptor);
			return false;
#else
	#if defined(BPR_APPLICATION_TESTING)
			auto remaining = FsyncFailureCountdown.load(std::memory_order_acquire);
			while (remaining != 0U) {
				if (FsyncFailureCountdown.compare_exchange_weak(
						remaining,
						remaining - 1U,
						std::memory_order_acq_rel,
						std::memory_order_acquire)) {
					if (remaining == 1U) {
						errno = EIO;
						return false;
					}
					break;
				}
			}
	#endif
			return ::fsync(aDescriptor) == 0;
#endif
		}

		[[nodiscard]] auto hex_digest(
			const std::span<const std::uint8_t> aBytes) -> std::string
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			const auto     digest = core::sha256(aBytes);
			auto           result = std::string{ };
			result.reserve(64U);
			for (const auto byte : digest) {
				result.push_back(digits[(byte >> 4U) & 0x0fU]);
				result.push_back(digits[byte & 0x0fU]);
			}
			return result;
		}

		[[nodiscard]] auto exact_target()
			-> std::optional<review_mutation_target_binding>
		{
			const auto base = core::parse_git_commit_oid(designated_base_oid);
			const auto head = core::parse_git_commit_oid(designated_head_oid);
			if (!base.has_value() || !head.has_value()) {
				return std::nullopt;
			}
			return review_mutation_target_binding{
				.Semantic = {
					.Host         = core::git_host_id{ 1U },
					.Repository   = core::repository_id{ designated_repository_id },
					.PullRequest  = core::pull_request_id{ designated_pull_request_id },
					.ExpectedHead = *head,
				},
				.Hostname       = github_canonical_hostname{ designated_hostname },
				.ServerContract = github_server_contract::github_com_2022_11_28,
				.Repository     = {
					.Owner = std::string{ designated_owner },
					.Name  = std::string{ designated_repository },
				},
				.Number                   = core::pull_request_number{ designated_pull_request_number },
				.RepositoryDatabaseId     = designated_repository_id,
				.RepositoryNodeId         = std::string{ designated_repository_node },
				.PullRequestDatabaseId    = designated_pull_request_id,
				.PullRequestNodeId        = std::string{ designated_pull_request_node },
				.BaseRepositoryDatabaseId = designated_repository_id,
				.BaseOid                  = *base,
				.BaseRef                  = std::string{ designated_base_ref },
				.HeadRef                  = std::string{ designated_head_ref },
				.Open                     = true,
			};
		}

#if !defined(_WIN32)
		class file_descriptor final
		{
		public:
			explicit file_descriptor(const int aValue = -1) noexcept
				: Value{ aValue }
			{
			}
			~file_descriptor()
			{
				if (Value >= 0) {
					static_cast<void>(::close(Value));
				}
			}
			file_descriptor(const file_descriptor&)                    = delete;
			auto operator=(const file_descriptor&) -> file_descriptor& = delete;
			file_descriptor(file_descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{
			}
			auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
			{
				if (this != &aOther) {
					if (Value >= 0) {
						static_cast<void>(::close(Value));
					}
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
			[[nodiscard]] auto     get() const noexcept -> int { return Value; }
			[[nodiscard]] explicit operator bool() const noexcept { return Value >= 0; }

		private:
			int Value;
		};

		[[nodiscard]] auto has_only_trivial_acl(const int aDescriptor) noexcept
			-> bool
		{
	#if defined(__APPLE__)
			errno           = 0;
			auto* const acl = ::acl_get_fd(aDescriptor);
			if (acl == nullptr) {
				return errno == ENOENT;
			}
			static_cast<void>(::acl_free(acl));
			return false;
	#else
			static_cast<void>(aDescriptor);
			return true;
	#endif
		}

		struct retained_approval
		{
			file_descriptor Parent;
			file_descriptor File;
			std::string     Name;
			struct stat     Identity{ };
			std::string     Bytes;
		};

		[[nodiscard]] auto read_descriptor(
			const int aDescriptor, const std::size_t aLimit)
			-> std::optional<std::vector<std::uint8_t>>
		{
			auto bytes = std::vector<std::uint8_t>{ };
			bytes.reserve(std::min(aLimit, std::size_t{ 64U * 1024U }));
			auto buffer = std::array<std::uint8_t, 64U * 1024U>{ };
			for (;;) {
				const auto count = ::read(aDescriptor, buffer.data(), buffer.size());
				if (count < 0) {
					if (errno == EINTR) {
						continue;
					}
					return std::nullopt;
				}
				if (count == 0) {
					return bytes;
				}
				const auto size = static_cast<std::size_t>(count);
				if (size > aLimit - bytes.size()) {
					return std::nullopt;
				}
				bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
			}
		}

		[[nodiscard]] auto open_approval(
			const std::filesystem::path& aPath)
			-> std::expected<retained_approval, designated_review_bootstrap_failure>
		{
			if (!aPath.is_absolute() || aPath.filename().empty()
				|| aPath.lexically_normal() != aPath) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto parent = file_descriptor{ ::open(
				aPath.parent_path().c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			if (!parent) {
				return std::unexpected(designated_review_bootstrap_failure::unsafe_parent);
			}
			struct stat parentStatus{ };
			if (::fstat(parent.get(), &parentStatus) != 0
				|| !S_ISDIR(parentStatus.st_mode) || parentStatus.st_uid != ::geteuid()
				|| (parentStatus.st_mode & 0077) != 0
				|| !has_only_trivial_acl(parent.get())) {
				return std::unexpected(designated_review_bootstrap_failure::unsafe_parent);
			}
			const auto name = aPath.filename().string();
			auto       file = file_descriptor{ ::openat(
				parent.get(), name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
			if (!file) {
				return std::unexpected(
					errno == ENOENT ? designated_review_bootstrap_failure::artifact_absent
									: designated_review_bootstrap_failure::unsafe_artifact);
			}
			struct stat status{ };
			if (::fstat(file.get(), &status) != 0 || !S_ISREG(status.st_mode)
				|| status.st_uid != ::geteuid() || status.st_nlink != 1
				|| (status.st_mode & 0777) != 0600
				|| !has_only_trivial_acl(file.get())) {
				return std::unexpected(designated_review_bootstrap_failure::unsafe_artifact);
			}
			if (status.st_size <= 0
				|| static_cast<std::uintmax_t>(status.st_size) > maximum_approval_bytes) {
				return std::unexpected(designated_review_bootstrap_failure::artifact_limit);
			}
			auto bytes = read_descriptor(file.get(), maximum_approval_bytes);
			if (!bytes.has_value() || bytes->size() != static_cast<std::size_t>(status.st_size)) {
				return std::unexpected(designated_review_bootstrap_failure::artifact_limit);
			}
			return retained_approval{
				.Parent   = std::move(parent),
				.File     = std::move(file),
				.Name     = name,
				.Identity = status,
				.Bytes    = std::string{
					reinterpret_cast<const char*>(bytes->data()), bytes->size() },
			};
		}

		[[nodiscard]] auto record_spent(
			retained_approval&     aApproval,
			const std::string_view aApprovalId)
			-> std::expected<void, designated_review_bootstrap_failure>
		{
			if (!lowercase_sha256(aApprovalId)) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto createdDirectory = false;
			if (::mkdirat(aApproval.Parent.get(), spent_directory_name, 0700) == 0) {
				createdDirectory = true;
			} else if (errno != EEXIST) {
				return std::unexpected(
					designated_review_bootstrap_failure::spent_state_failure);
			}
			auto        spent = file_descriptor{ ::openat(
				aApproval.Parent.get(),
				spent_directory_name,
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			struct stat spentStatus{ };
			if (!spent || ::fstat(spent.get(), &spentStatus) != 0
				|| !S_ISDIR(spentStatus.st_mode) || spentStatus.st_uid != ::geteuid()
				|| (spentStatus.st_mode & 0777) != 0700
				|| !has_only_trivial_acl(spent.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::spent_state_failure);
			}
			if (createdDirectory && !durable_sync(aApproval.Parent.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			auto record = file_descriptor{ ::openat(
				spent.get(),
				std::string{ aApprovalId }.c_str(),
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600) };
			if (!record) {
				return std::unexpected(
					errno == EEXIST
						? designated_review_bootstrap_failure::artifact_replayed
						: designated_review_bootstrap_failure::spent_state_failure);
			}
			const auto bytes = std::string{ "approval_id=" } + std::string{ aApprovalId }
				+ "\n";
			auto written = std::size_t{ 0U };
			while (written != bytes.size()) {
				const auto count = ::write(
					record.get(), bytes.data() + written, bytes.size() - written);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					return std::unexpected(
						designated_review_bootstrap_failure::spent_state_failure);
				}
				written += static_cast<std::size_t>(count);
			}
			if (!durable_sync(record.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::file_sync_failure);
			}
			if (!durable_sync(spent.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			return { };
		}

		[[nodiscard]] auto refuse_known_replay(
			const retained_approval& aApproval,
			const std::string_view   aApprovalId)
			-> std::expected<void, designated_review_bootstrap_failure>
		{
			if (!lowercase_sha256(aApprovalId)) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto spent = file_descriptor{ ::openat(
				aApproval.Parent.get(),
				spent_directory_name,
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			if (!spent) {
				return errno == ENOENT
					? std::expected<void, designated_review_bootstrap_failure>{ }
					: std::unexpected(
						  designated_review_bootstrap_failure::spent_state_failure);
			}
			struct stat directoryStatus{ };
			if (::fstat(spent.get(), &directoryStatus) != 0
				|| !S_ISDIR(directoryStatus.st_mode)
				|| directoryStatus.st_uid != ::geteuid()
				|| (directoryStatus.st_mode & 0777) != 0700
				|| !has_only_trivial_acl(spent.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::spent_state_failure);
			}
			struct stat recordStatus{ };
			if (::fstatat(
					spent.get(),
					std::string{ aApprovalId }.c_str(),
					&recordStatus,
					AT_SYMLINK_NOFOLLOW)
				== 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_replayed);
			}
			return errno == ENOENT
				? std::expected<void, designated_review_bootstrap_failure>{ }
				: std::unexpected(
					  designated_review_bootstrap_failure::spent_state_failure);
		}

		[[nodiscard]] auto executable_sha256(
			const std::filesystem::path& aPath)
			-> std::expected<std::string, designated_review_bootstrap_failure>
		{
			if (!aPath.is_absolute() || aPath.lexically_normal() != aPath) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto file = file_descriptor{
				::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
			};
			if (!file) {
				return std::unexpected(
					designated_review_bootstrap_failure::executable_unavailable);
			}
			struct stat status{ };
			if (::fstat(file.get(), &status) != 0 || !S_ISREG(status.st_mode)
				|| status.st_size <= 0
				|| static_cast<std::uintmax_t>(status.st_size) > maximum_executable_bytes) {
				return std::unexpected(
					designated_review_bootstrap_failure::executable_limit);
			}
			auto running = file_descriptor{ };
	#if defined(__APPLE__)
			auto required = std::uint32_t{ 0U };
			static_cast<void>(::_NSGetExecutablePath(nullptr, &required));
			auto path = std::vector<char>(required, '\0');
			if (required == 0U || ::_NSGetExecutablePath(path.data(), &required) != 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::executable_unavailable);
			}
			running = file_descriptor{ ::open(path.data(), O_RDONLY | O_CLOEXEC) };
	#elif defined(__linux__)
			running = file_descriptor{ ::open("/proc/self/exe", O_RDONLY | O_CLOEXEC) };
	#else
			return std::unexpected(
				designated_review_bootstrap_failure::platform_unavailable);
	#endif
			struct stat runningStatus{ };
			if (!running || ::fstat(running.get(), &runningStatus) != 0
				|| runningStatus.st_dev != status.st_dev
				|| runningStatus.st_ino != status.st_ino) {
				return std::unexpected(
					designated_review_bootstrap_failure::executable_unavailable);
			}
			auto bytes = read_descriptor(file.get(), maximum_executable_bytes);
			if (!bytes.has_value() || bytes->size() != static_cast<std::size_t>(status.st_size)) {
				return std::unexpected(
					designated_review_bootstrap_failure::executable_limit);
			}
			return hex_digest(*bytes);
		}

		[[nodiscard]] auto consume(retained_approval& aApproval)
			-> std::expected<void, designated_review_bootstrap_failure>
		{
			const auto quarantine = "." + aApproval.Name + ".bpr-consumed";
	#if defined(__APPLE__)
			if (::renameatx_np(
					aApproval.Parent.get(),
					aApproval.Name.c_str(),
					aApproval.Parent.get(),
					quarantine.c_str(),
					RENAME_EXCL)
				!= 0) {
	#elif defined(__linux__)
			if (::syscall(
					SYS_renameat2,
					aApproval.Parent.get(),
					aApproval.Name.c_str(),
					aApproval.Parent.get(),
					quarantine.c_str(),
					RENAME_NOREPLACE)
				!= 0) {
	#else
			return std::unexpected(
				designated_review_bootstrap_failure::platform_unavailable);
	#endif
				return std::unexpected(
					errno == ENOENT
						? designated_review_bootstrap_failure::artifact_substituted
						: designated_review_bootstrap_failure::consume_failure);
			}
			struct stat moved{ };
			if (::fstatat(
					aApproval.Parent.get(),
					quarantine.c_str(),
					&moved,
					AT_SYMLINK_NOFOLLOW)
					!= 0
				|| moved.st_dev != aApproval.Identity.st_dev
				|| moved.st_ino != aApproval.Identity.st_ino) {
				static_cast<void>(::unlinkat(
					aApproval.Parent.get(), quarantine.c_str(), 0));
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			if (::unlinkat(aApproval.Parent.get(), quarantine.c_str(), 0) != 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::consume_failure);
			}
			struct stat consumed{ };
			if (::fstat(aApproval.File.get(), &consumed) != 0 || consumed.st_nlink != 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			if (!durable_sync(aApproval.Parent.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			return { };
		}
#endif
	} // namespace

	auto designated_review_approval_subject(
		const designated_review_bootstrap_request& aRequest,
		const std::string_view                     aApplicationSha256,
		const std::string_view                     aApprovalId)
		-> std::expected<std::string, designated_review_bootstrap_failure>
	{
		if (aRequest.BuildTesting || aRequest.BuildConfiguration != "Release"
			|| !lowercase_sha256(aRequest.SourceSha256)
			|| !lowercase_sha256(aApprovalId)
			|| !lowercase_sha256(aApplicationSha256)) {
			return std::unexpected(
				designated_review_bootstrap_failure::invalid_configuration);
		}
		auto result = std::string{ };
		result.reserve(1'024U);
		result.append("schema_name=bpr.designated_live_review_approval\n")
			.append("schema_version=1\n")
			.append("approval_statement=")
			.append(designated_approval_statement)
			.append("\napplication_sha256=")
			.append(aApplicationSha256)
			.append("\nsource_sha256=")
			.append(aRequest.SourceSha256)
			.append("\napproval_id=")
			.append(aApprovalId)
			.append("\nbuild_configuration=Release\n")
			.append("build_testing=off\n")
			.append("hostname=")
			.append(designated_hostname)
			.append("\nserver_contract=github.com-2022-11-28\n")
			.append("repository_owner=")
			.append(designated_owner)
			.append("\nrepository_name=")
			.append(designated_repository)
			.append("\nrepository_database_id=")
			.append(std::to_string(designated_repository_id))
			.append("\nrepository_node_id=")
			.append(designated_repository_node)
			.append("\npull_request_number=")
			.append(std::to_string(designated_pull_request_number))
			.append("\npull_request_database_id=")
			.append(std::to_string(designated_pull_request_id))
			.append("\npull_request_node_id=")
			.append(designated_pull_request_node)
			.append("\nbase_repository_database_id=")
			.append(std::to_string(designated_repository_id))
			.append("\nbase_oid=")
			.append(designated_base_oid)
			.append("\nbase_ref=")
			.append(designated_base_ref)
			.append("\nhead_oid=")
			.append(designated_head_oid)
			.append("\nhead_ref=")
			.append(designated_head_ref)
			.append("\nstate=open\n")
			.append("effect=one-source-line-review-comment\n");
		return result;
	}

	auto designated_review_authority_bootstrap::run(
		const designated_review_bootstrap_request& aRequest,
		designated_review_public_observation_port& aObservation)
		-> std::expected<
			review_mutation_execution_grant,
			designated_review_bootstrap_failure>
	{
#if defined(_WIN32)
		static_cast<void>(aRequest);
		static_cast<void>(aObservation);
		return std::unexpected(
			designated_review_bootstrap_failure::platform_unavailable);
#else
		try {
			auto approval = open_approval(aRequest.ApprovalPath);
			if (!approval.has_value()) {
				return std::unexpected(approval.error());
			}
			if (approval->Bytes.contains('\0') || approval->Bytes.contains('\r')
				|| !approval->Bytes.ends_with('\n')) {
				return std::unexpected(
					designated_review_bootstrap_failure::malformed_approval);
			}
			auto applicationSha = executable_sha256(aRequest.ExecutablePath);
			if (!applicationSha.has_value()) {
				return std::unexpected(applicationSha.error());
			}
			const auto     digestField  = approval->Bytes.rfind("subject_sha256=");
			constexpr auto digestPrefix = std::string_view{ "subject_sha256=" };
			if (digestField == std::string::npos
				|| digestField + digestPrefix.size() + 65U != approval->Bytes.size()) {
				return std::unexpected(
					designated_review_bootstrap_failure::malformed_approval);
			}
			const auto artifactSubject = std::string_view{ approval->Bytes }.substr(
				0U, digestField);
			const auto artifactDigest = std::string_view{ approval->Bytes }.substr(
				digestField + digestPrefix.size(), 64U);
			auto approvalId = approval_id_from_subject(artifactSubject);
			if (!approvalId.has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::malformed_approval);
			}
			if (!lowercase_sha256(artifactDigest)
				|| hex_digest(std::span{
					   reinterpret_cast<const std::uint8_t*>(artifactSubject.data()),
					   artifactSubject.size() })
					!= artifactDigest) {
				return std::unexpected(
					designated_review_bootstrap_failure::approval_digest_mismatch);
			}
			auto subject = designated_review_approval_subject(
				aRequest, *applicationSha, *approvalId);
			if (!subject.has_value() || artifactSubject != *subject) {
				return std::unexpected(
					designated_review_bootstrap_failure::approval_mismatch);
			}
			if (const auto replay = refuse_known_replay(*approval, *approvalId);
				!replay.has_value()) {
				return std::unexpected(replay.error());
			}
			auto target = exact_target();
			if (!target.has_value() || !valid(*target)) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			if (!aObservation.observe(*target).has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::public_observation);
			}
			const auto spent = record_spent(*approval, *approvalId);
			if (!spent.has_value()) {
				return std::unexpected(spent.error());
			}
			const auto consumed = consume(*approval);
			if (!consumed.has_value()) {
				return std::unexpected(consumed.error());
			}
			return review_mutation_execution_grant{
				new review_mutation_execution_authority{
					std::move(*target),
					std::chrono::steady_clock::now() + authority_lifetime }
			};
		} catch (...) {
			return std::unexpected(designated_review_bootstrap_failure::consume_failure);
		}
#endif
	}

#if defined(BPR_APPLICATION_TESTING)
	void fail_designated_review_fsync_after_for_testing(
		const std::uint32_t aCalls) noexcept
	{
		FsyncFailureCountdown.store(aCalls, std::memory_order_release);
	}
#endif
} // namespace bpr::application::detail
