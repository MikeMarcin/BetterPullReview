#include "review_mutation_execution_authority.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <dirent.h>
	#include <fcntl.h>
	#include <sys/file.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <mach-o/dyld.h>
		#include <sys/acl.h>
	#endif
	#if defined(__linux__)
		#include <linux/fs.h>
		#include <sys/random.h>
		#include <sys/syscall.h>
	#endif
#endif

namespace bpr::application::detail {
	namespace {
		constexpr auto maximum_approval_bytes        = std::size_t{ 8U * 1024U };
		constexpr auto maximum_executable_bytes      = std::size_t{ 256U * 1024U * 1024U };
		constexpr auto maximum_path_components       = std::size_t{ 128U };
		constexpr auto authority_lifetime            = std::chrono::minutes{ 15 };
		constexpr auto maximum_replay_metadata_bytes = std::size_t{ 16U * 1024U };
		constexpr char replay_metadata_name[]        = ".bpr-replay-namespace";
		constexpr char replay_metadata_next_name[]   = ".bpr-replay-namespace.next";

		constexpr auto designated_hostname      = std::string_view{ "github.com" };
		constexpr auto designated_owner         = std::string_view{ "MikeMarcin" };
		constexpr auto designated_repository    = std::string_view{ "BPR-Test" };
		constexpr auto designated_repository_id = std::uint64_t{ 1'328'277'631U };
		constexpr auto designated_repository_node
			= std::string_view{ "R_kgDOTyvofw" };
		constexpr auto designated_pull_request_number = std::uint32_t{ 2U };
		constexpr auto designated_pull_request_id
			= std::uint64_t{ 4'650'736'621U };
		constexpr auto designated_pull_request_node
			= std::string_view{ "PR_kwDOTyvof88AAAABFTSb7Q" };
		constexpr auto designated_base_ref = std::string_view{ "codex/rfc0022-fixture-base-20260926" };
		constexpr auto designated_head_ref
			= std::string_view{ "codex/rfc0022-live-comments-20260926" };
		constexpr auto designated_base_oid
			= std::string_view{ "a939a85a5525994fe9e9cb4c973ad9362ed427c7" };
		constexpr auto designated_head_oid
			= std::string_view{ "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a" };
		constexpr auto designated_approval_statement = std::string_view{
			"I authorize this exact BetterPullReview build to publish one source-line "
			"review comment to MikeMarcin/BPR-Test#2 at head "
			"61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a."
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
		std::atomic<std::uint32_t>                                 FsyncFailureCountdown = 0U;
		std::atomic<designated_review_marker_mutation_for_testing> MarkerMutation{
			designated_review_marker_mutation_for_testing::none
		};
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
	#if defined(__APPLE__)
			// fsync(2) only hands the write to the drive; macOS documents
			// F_FULLFSYNC as the barrier that survives power loss or a panic.
			// A spent marker that a later run cannot see is a replay, so an
			// unsupported or failing barrier is a durability failure here.
			return ::fcntl(aDescriptor, F_FULLFSYNC) != -1;
	#else
			return ::fsync(aDescriptor) == 0;
	#endif
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
				result.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
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
			[[nodiscard]] auto get() const noexcept -> int { return Value; }
			[[nodiscard]] auto release() noexcept -> int
			{
				return std::exchange(Value, -1);
			}
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

		class directory_stream final
		{
		public:
			explicit directory_stream(const int aDescriptor) noexcept
			{
				auto directory = file_descriptor{ ::openat(
					aDescriptor,
					".",
					O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
				if (!directory) {
					return;
				}
				Value = ::fdopendir(directory.get());
				if (Value != nullptr) {
					static_cast<void>(directory.release());
				}
			}
			~directory_stream()
			{
				if (Value != nullptr) {
					static_cast<void>(::closedir(Value));
				}
			}
			directory_stream(const directory_stream&)                    = delete;
			auto operator=(const directory_stream&) -> directory_stream& = delete;

			[[nodiscard]] auto     get() const noexcept -> DIR* { return Value; }
			[[nodiscard]] explicit operator bool() const noexcept
			{
				return Value != nullptr;
			}

		private:
			DIR* Value = nullptr;
		};

		class directory_lock final
		{
		public:
			explicit directory_lock(const int aDescriptor) noexcept
				: Descriptor{ aDescriptor }
			{
				for (;;) {
					if (::flock(Descriptor, LOCK_EX) == 0) {
						Locked = true;
						return;
					}
					if (errno != EINTR) {
						return;
					}
				}
			}
			~directory_lock()
			{
				if (Locked) {
					static_cast<void>(::flock(Descriptor, LOCK_UN));
				}
			}
			directory_lock(const directory_lock&)                    = delete;
			auto operator=(const directory_lock&) -> directory_lock& = delete;

			[[nodiscard]] explicit operator bool() const noexcept { return Locked; }

		private:
			int  Descriptor;
			bool Locked = false;
		};

		struct retained_directory_path
		{
			file_descriptor       Parent;
			file_descriptor       Directory;
			std::filesystem::path AbsolutePath;
			std::string           Name;
			struct stat           Identity{ };
		};

		struct retained_regular_file
		{
			file_descriptor File;
			struct stat     Identity{ };
			std::string     Bytes;
		};

		struct retained_approval
		{
			retained_directory_path Parent;
			file_descriptor         File;
			std::string             Name;
			struct stat             Identity{ };
			std::string             Bytes;
		};

		struct retained_replay_namespace
		{
			retained_directory_path                Path;
			retained_regular_file                  Metadata;
			designated_review_replay_configuration Configuration;
		};

		struct replay_inventory
		{
			std::vector<std::string> Markers;
			bool                     HasMetadata = false;
			bool                     HasNext     = false;
		};

		[[nodiscard]] auto same_identity(
			const struct stat& aLeft, const struct stat& aRight) noexcept -> bool
		{
			return aLeft.st_dev == aRight.st_dev && aLeft.st_ino == aRight.st_ino;
		}

		[[nodiscard]] auto safe_private_directory(
			const int aDescriptor, struct stat* const aIdentity = nullptr) noexcept
			-> bool
		{
			struct stat status{ };
			if (::fstat(aDescriptor, &status) != 0 || !S_ISDIR(status.st_mode)
				|| status.st_uid != ::geteuid() || (status.st_mode & static_cast<mode_t>(0777)) != 0700
				|| !has_only_trivial_acl(aDescriptor)) {
				return false;
			}
			if (aIdentity != nullptr) {
				*aIdentity = status;
			}
			return true;
		}

		[[nodiscard]] auto safe_approval_parent(
			const int aDescriptor, struct stat* const aIdentity = nullptr) noexcept
			-> bool
		{
			struct stat status{ };
			if (::fstat(aDescriptor, &status) != 0 || !S_ISDIR(status.st_mode)
				|| status.st_uid != ::geteuid() || (status.st_mode & static_cast<mode_t>(0077)) != 0
				|| !has_only_trivial_acl(aDescriptor)) {
				return false;
			}
			if (aIdentity != nullptr) {
				*aIdentity = status;
			}
			return true;
		}

		[[nodiscard]] auto safe_private_regular(
			const int aDescriptor, struct stat* const aIdentity = nullptr) noexcept
			-> bool
		{
			struct stat status{ };
			if (::fstat(aDescriptor, &status) != 0 || !S_ISREG(status.st_mode)
				|| status.st_uid != ::geteuid() || status.st_nlink != 1
				|| (status.st_mode & static_cast<mode_t>(0777)) != 0600
				|| !has_only_trivial_acl(aDescriptor)) {
				return false;
			}
			if (aIdentity != nullptr) {
				*aIdentity = status;
			}
			return true;
		}

		[[nodiscard]] auto path_components(
			const std::filesystem::path& aPath)
			-> std::optional<std::vector<std::string>>
		{
			if (aPath.empty() || !aPath.is_absolute()
				|| aPath.lexically_normal() != aPath || aPath == aPath.root_path()
				|| aPath.filename().empty()) {
				return std::nullopt;
			}
			auto result = std::vector<std::string>{ };
			result.reserve(16U);
			for (const auto& component : aPath.relative_path()) {
				const auto value = component.string();
				if (value.empty() || value == "." || value == ".."
					|| value.contains('\0') || value.contains('\n')
					|| value.contains('\r') || result.size() == maximum_path_components) {
					return std::nullopt;
				}
				result.push_back(value);
			}
			return result.empty() ? std::nullopt
								  : std::optional<std::vector<std::string>>{ std::move(result) };
		}

		[[nodiscard]] auto open_retained_directory(
			const std::filesystem::path&              aPath,
			const designated_review_bootstrap_failure aAbsent,
			const designated_review_bootstrap_failure aUnsafe)
			-> std::expected<retained_directory_path, designated_review_bootstrap_failure>
		{
			auto components = path_components(aPath);
			if (!components.has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto current = file_descriptor{ ::open(
				aPath.root_path().c_str(),
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			if (!current) {
				return std::unexpected(aUnsafe);
			}
			for (auto index = std::size_t{ 0U }; index + 1U < components->size(); ++index) {
				auto next = file_descriptor{ ::openat(
					current.get(),
					(*components)[index].c_str(),
					O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
				if (!next) {
					return std::unexpected(errno == ENOENT ? aAbsent : aUnsafe);
				}
				current = std::move(next);
			}
			const auto& name      = components->back();
			auto        directory = file_descriptor{ ::openat(
				current.get(),
				name.c_str(),
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			if (!directory) {
				return std::unexpected(errno == ENOENT ? aAbsent : aUnsafe);
			}
			struct stat identity{ };
			if (::fstat(directory.get(), &identity) != 0 || !S_ISDIR(identity.st_mode)) {
				return std::unexpected(aUnsafe);
			}
			return retained_directory_path{
				.Parent       = std::move(current),
				.Directory    = std::move(directory),
				.AbsolutePath = aPath,
				.Name         = name,
				.Identity     = identity,
			};
		}

		[[nodiscard]] auto create_retained_directory(
			const std::filesystem::path& aPath)
			-> std::expected<retained_directory_path, designated_review_bootstrap_failure>
		{
			auto components = path_components(aPath);
			if (!components.has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto current = file_descriptor{ ::open(
				aPath.root_path().c_str(),
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
			if (!current) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			for (auto index = std::size_t{ 0U }; index < components->size(); ++index) {
				const auto& name    = (*components)[index];
				auto        created = false;
				auto        next    = file_descriptor{ ::openat(
					current.get(),
					name.c_str(),
					O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
				if (!next && errno == ENOENT) {
					if (::mkdirat(current.get(), name.c_str(), 0700) != 0) {
						return std::unexpected(
							designated_review_bootstrap_failure::unsafe_replay_state);
					}
					created = true;
					next    = file_descriptor{ ::openat(
						current.get(),
						name.c_str(),
						O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
				}
				if (!next) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
				if (created && !safe_private_directory(next.get())) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
				if (created && !durable_sync(current.get())) {
					return std::unexpected(
						designated_review_bootstrap_failure::directory_sync_failure);
				}
				if (index + 1U == components->size()) {
					struct stat identity{ };
					if (::fstat(next.get(), &identity) != 0) {
						return std::unexpected(
							designated_review_bootstrap_failure::unsafe_replay_state);
					}
					return retained_directory_path{
						.Parent       = std::move(current),
						.Directory    = std::move(next),
						.AbsolutePath = aPath,
						.Name         = name,
						.Identity     = identity,
					};
				}
				current = std::move(next);
			}
			return std::unexpected(
				designated_review_bootstrap_failure::unsafe_replay_state);
		}

		[[nodiscard]] auto retained_path_matches(
			const retained_directory_path& aPath) -> bool
		{
			struct stat retained{ };
			struct stat named{ };
			if (::fstat(aPath.Directory.get(), &retained) != 0
				|| ::fstatat(
					   aPath.Parent.get(),
					   aPath.Name.c_str(),
					   &named,
					   AT_SYMLINK_NOFOLLOW)
					!= 0
				|| !S_ISDIR(named.st_mode) || !same_identity(retained, aPath.Identity)
				|| !same_identity(named, aPath.Identity)) {
				return false;
			}
			auto reopened = open_retained_directory(
				aPath.AbsolutePath,
				designated_review_bootstrap_failure::replay_state_substituted,
				designated_review_bootstrap_failure::replay_state_substituted);
			return reopened.has_value()
				&& same_identity(reopened->Identity, aPath.Identity)
				&& ::fstat(reopened->Directory.get(), &named) == 0
				&& same_identity(named, aPath.Identity);
		}

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

		[[nodiscard]] auto write_descriptor(
			const int aDescriptor, const std::string_view aBytes) noexcept -> bool
		{
			auto written = std::size_t{ 0U };
			while (written != aBytes.size()) {
				const auto count = ::write(
					aDescriptor, aBytes.data() + written, aBytes.size() - written);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					return false;
				}
				written += static_cast<std::size_t>(count);
			}
			return true;
		}

		[[nodiscard]] auto replay_metadata_bytes(
			const designated_review_replay_configuration& aConfiguration)
			-> std::optional<std::string>
		{
			const auto path = aConfiguration.NamespacePath.generic_string();
			if (!path_components(aConfiguration.NamespacePath).has_value()
				|| path.contains('\0') || path.contains('\n') || path.contains('\r')
				|| !lowercase_sha256(aConfiguration.NamespaceId)) {
				return std::nullopt;
			}
			auto result = std::string{ };
			result.reserve(path.size() + 256U);
			result.append("schema_name=")
				.append(designated_review_replay_schema)
				.append("\nschema_version=1\nnamespace_path=")
				.append(path)
				.append("\nnamespace_id=")
				.append(aConfiguration.NamespaceId)
				.append("\n");
			return result;
		}

		[[nodiscard]] auto open_replay_regular(
			const int                                 aDirectory,
			const std::string_view                    aName,
			const std::size_t                         aLimit,
			const designated_review_bootstrap_failure aAbsent,
			const designated_review_bootstrap_failure aUnsafe)
			-> std::expected<retained_regular_file, designated_review_bootstrap_failure>
		{
			auto file = file_descriptor{ ::openat(
				aDirectory,
				std::string{ aName }.c_str(),
				O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
			if (!file) {
				return std::unexpected(errno == ENOENT ? aAbsent : aUnsafe);
			}
			struct stat identity{ };
			if (!safe_private_regular(file.get(), &identity) || identity.st_size <= 0
				|| static_cast<std::uintmax_t>(identity.st_size) > aLimit) {
				return std::unexpected(aUnsafe);
			}
			auto bytes = read_descriptor(file.get(), aLimit);
			if (!bytes.has_value()
				|| bytes->size() != static_cast<std::size_t>(identity.st_size)) {
				return std::unexpected(aUnsafe);
			}
			return retained_regular_file{
				.File     = std::move(file),
				.Identity = identity,
				.Bytes    = std::string{
					reinterpret_cast<const char*>(bytes->data()), bytes->size() },
			};
		}

		[[nodiscard]] auto retained_regular_matches(
			const int                    aDirectory,
			const std::string_view       aName,
			const retained_regular_file& aRetained) noexcept -> bool
		{
			struct stat descriptorStatus{ };
			struct stat namedStatus{ };
			// This runs on the revalidation barrier of a noexcept spend, so the
			// name is copied into fixed storage rather than an allocating string.
			auto namedPath = std::array<char, 256>{ };
			if (aName.empty() || aName.size() >= namedPath.size()) {
				return false;
			}
			std::ranges::copy(aName, namedPath.begin());
			return safe_private_regular(aRetained.File.get(), &descriptorStatus)
				&& ::fstatat(
					   aDirectory, namedPath.data(), &namedStatus, AT_SYMLINK_NOFOLLOW)
				== 0
				&& S_ISREG(namedStatus.st_mode) && namedStatus.st_uid == ::geteuid()
				&& namedStatus.st_nlink == 1 && (namedStatus.st_mode & static_cast<mode_t>(0777)) == 0600
				&& same_identity(descriptorStatus, aRetained.Identity)
				&& same_identity(namedStatus, aRetained.Identity);
		}

		[[nodiscard]] auto marker_bytes(const std::string_view aApprovalId)
			-> std::string
		{
			return std::string{ "approval_id=" } + std::string{ aApprovalId } + "\n";
		}

		[[nodiscard]] auto inspect_replay_namespace(
			const int aDirectory, const bool aAllowNext)
			-> std::expected<replay_inventory, designated_review_bootstrap_failure>
		{
			auto stream = directory_stream{ aDirectory };
			if (!stream) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto result = replay_inventory{ };
			result.Markers.reserve(designated_review_replay_marker_capacity);
			auto entryCount = std::size_t{ 0U };
			for (;;) {
				// readdir reports end-of-directory and failure the same way, so
				// errno is cleared immediately before every call; work inside the
				// loop is allowed to set errno on success.
				errno                   = 0;
				const auto* const entry = ::readdir(stream.get());
				if (entry == nullptr) {
					break;
				}
				const auto name = std::string_view{ entry->d_name };
				if (name == "." || name == "..") {
					continue;
				}
				++entryCount;
				if (entryCount > designated_review_replay_marker_capacity + 2U) {
					return std::unexpected(
						designated_review_bootstrap_failure::replay_state_capacity);
				}
				if (name == replay_metadata_name) {
					if (result.HasMetadata) {
						return std::unexpected(
							designated_review_bootstrap_failure::unsafe_replay_state);
					}
					result.HasMetadata = true;
				} else if (name == replay_metadata_next_name && aAllowNext) {
					if (result.HasNext) {
						return std::unexpected(
							designated_review_bootstrap_failure::unsafe_replay_state);
					}
					result.HasNext = true;
				} else if (lowercase_sha256(name)) {
					if (result.Markers.size()
						== designated_review_replay_marker_capacity) {
						return std::unexpected(
							designated_review_bootstrap_failure::replay_state_capacity);
					}
					result.Markers.emplace_back(name);
				} else {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
			}
			if (errno != 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			for (const auto& marker : result.Markers) {
				auto file = open_replay_regular(
					aDirectory,
					marker,
					maximum_approval_bytes,
					designated_review_bootstrap_failure::unsafe_replay_state,
					designated_review_bootstrap_failure::unsafe_replay_state);
				if (!file.has_value() || file->Bytes != marker_bytes(marker)) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
			}
			return result;
		}

		[[nodiscard]] auto random_namespace_id()
			-> std::expected<std::string, designated_review_bootstrap_failure>
		{
			auto bytes = std::array<std::uint8_t, 32U>{ };
	#if defined(__APPLE__)
			::arc4random_buf(bytes.data(), bytes.size());
	#elif defined(__linux__)
			auto offset = std::size_t{ 0U };
			while (offset != bytes.size()) {
				const auto count = ::getrandom(
					bytes.data() + offset, bytes.size() - offset, 0U);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
				offset += static_cast<std::size_t>(count);
			}
	#else
			return std::unexpected(
				designated_review_bootstrap_failure::platform_unavailable);
	#endif
			return hex_digest(bytes);
		}

		[[nodiscard]] auto replay_namespace_matches(
			const retained_replay_namespace& aNamespace) -> bool
		{
			return retained_path_matches(aNamespace.Path)
				&& safe_private_directory(aNamespace.Path.Directory.get())
				&& retained_regular_matches(
					   aNamespace.Path.Directory.get(),
					   replay_metadata_name,
					   aNamespace.Metadata);
		}

		[[nodiscard]] auto open_replay_namespace(
			const designated_review_replay_configuration& aConfiguration)
			-> std::expected<retained_replay_namespace, designated_review_bootstrap_failure>
		{
			auto expectedMetadata = replay_metadata_bytes(aConfiguration);
			if (!expectedMetadata.has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto path = open_retained_directory(
				aConfiguration.NamespacePath,
				designated_review_bootstrap_failure::replay_state_absent,
				designated_review_bootstrap_failure::unsafe_replay_state);
			if (!path.has_value()) {
				return std::unexpected(path.error());
			}
			if (!safe_private_directory(path->Directory.get(), &path->Identity)
				|| !retained_path_matches(*path)) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto metadata = open_replay_regular(
				path->Directory.get(),
				replay_metadata_name,
				maximum_replay_metadata_bytes,
				designated_review_bootstrap_failure::replay_state_absent,
				designated_review_bootstrap_failure::unsafe_replay_state);
			if (!metadata.has_value()) {
				return std::unexpected(metadata.error());
			}
			if (metadata->Bytes != *expectedMetadata) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_mismatch);
			}
			return retained_replay_namespace{
				.Path          = std::move(*path),
				.Metadata      = std::move(*metadata),
				.Configuration = aConfiguration,
			};
		}

		[[nodiscard]] auto open_approval(
			const std::filesystem::path& aPath)
			-> std::expected<retained_approval, designated_review_bootstrap_failure>
		{
			if (!path_components(aPath).has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto parent = open_retained_directory(
				aPath.parent_path(),
				designated_review_bootstrap_failure::unsafe_parent,
				designated_review_bootstrap_failure::unsafe_parent);
			if (!parent.has_value()
				|| !safe_approval_parent(parent->Directory.get(), &parent->Identity)
				|| !retained_path_matches(*parent)) {
				return std::unexpected(designated_review_bootstrap_failure::unsafe_parent);
			}
			const auto name = aPath.filename().string();
			auto       file = file_descriptor{ ::openat(
				parent->Directory.get(),
				name.c_str(),
				O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
			if (!file) {
				return std::unexpected(
					errno == ENOENT ? designated_review_bootstrap_failure::artifact_absent
									: designated_review_bootstrap_failure::unsafe_artifact);
			}
			struct stat status{ };
			if (!safe_private_regular(file.get(), &status)) {
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
				.Parent   = std::move(*parent),
				.File     = std::move(file),
				.Name     = name,
				.Identity = status,
				.Bytes    = std::string{
					reinterpret_cast<const char*>(bytes->data()), bytes->size() },
			};
		}

		void stabilize_failed_marker(
			const retained_replay_namespace& aNamespace,
			const std::string&               aName,
			const std::string&               aBytes) noexcept
		{
			if (::unlinkat(aNamespace.Path.Directory.get(), aName.c_str(), 0) != 0
				&& errno != ENOENT) {
				return;
			}
			auto record = file_descriptor{ ::openat(
				aNamespace.Path.Directory.get(),
				aName.c_str(),
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600) };
			if (!record || !write_descriptor(record.get(), aBytes)) {
				return;
			}
			static_cast<void>(::fsync(record.get()));
			static_cast<void>(::fsync(aNamespace.Path.Directory.get()));
		}

		void apply_marker_mutation_for_testing(
			const retained_replay_namespace& aNamespace,
			const std::string&               aName,
			const std::string&               aBytes) noexcept
		{
	#if defined(BPR_APPLICATION_TESTING)
			const auto mutation = MarkerMutation.exchange(
				designated_review_marker_mutation_for_testing::none,
				std::memory_order_acq_rel);
			switch (mutation) {
			case designated_review_marker_mutation_for_testing::none:
				break;
			case designated_review_marker_mutation_for_testing::unlink:
				static_cast<void>(::unlinkat(
					aNamespace.Path.Directory.get(), aName.c_str(), 0));
				break;
			case designated_review_marker_mutation_for_testing::substitute: {
				static_cast<void>(::unlinkat(
					aNamespace.Path.Directory.get(), aName.c_str(), 0));
				auto replacement = file_descriptor{ ::openat(
					aNamespace.Path.Directory.get(),
					aName.c_str(),
					O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
					0600) };
				if (replacement) {
					static_cast<void>(write_descriptor(replacement.get(), aBytes));
				}
				break;
			}
			case designated_review_marker_mutation_for_testing::hard_link: {
				const auto linkName = ".bpr-test-link-" + aName;
				// A failed link leaves the marker singly linked, and the course
				// that asked for a second link then reports the refusal it did
				// not see. GCC does not let a void cast discard glibc's
				// warn_unused_result under _FORTIFY_SOURCE ([[BPR-1262]]).
				[[maybe_unused]] const auto linked = ::linkat(
					aNamespace.Path.Directory.get(),
					aName.c_str(),
					aNamespace.Path.Directory.get(),
					linkName.c_str(),
					0);
				break;
			}
			}
	#else
			static_cast<void>(aNamespace);
			static_cast<void>(aName);
			static_cast<void>(aBytes);
	#endif
		}

		[[nodiscard]] auto refuse_known_replay(
			const retained_replay_namespace& aNamespace,
			const std::string_view           aApprovalId)
			-> std::expected<void, designated_review_bootstrap_failure>
		{
			if (!lowercase_sha256(aApprovalId)) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto lock = directory_lock{ aNamespace.Path.Directory.get() };
			if (!lock || !replay_namespace_matches(aNamespace)) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_substituted);
			}
			auto inventory = inspect_replay_namespace(
				aNamespace.Path.Directory.get(), false);
			if (!inventory.has_value()) {
				return std::unexpected(inventory.error());
			}
			if (!inventory->HasMetadata || inventory->HasNext) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			if (std::ranges::find(inventory->Markers, aApprovalId)
				!= inventory->Markers.end()) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_replayed);
			}
			if (inventory->Markers.size() == designated_review_replay_marker_capacity) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_capacity);
			}
			return { };
		}

		[[nodiscard]] auto record_spent(
			const retained_replay_namespace& aNamespace,
			const std::string_view           aApprovalId,
			bool&                            aMarkerCreationBegan)
			-> std::expected<void, designated_review_bootstrap_failure>
		{
			aMarkerCreationBegan = false;
			if (!lowercase_sha256(aApprovalId)) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto lock = directory_lock{ aNamespace.Path.Directory.get() };
			if (!lock || !replay_namespace_matches(aNamespace)) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_substituted);
			}
			auto inventory = inspect_replay_namespace(
				aNamespace.Path.Directory.get(), false);
			if (!inventory.has_value()) {
				return std::unexpected(inventory.error());
			}
			if (!inventory->HasMetadata || inventory->HasNext) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			if (std::ranges::find(inventory->Markers, aApprovalId)
				!= inventory->Markers.end()) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_replayed);
			}
			if (inventory->Markers.size() == designated_review_replay_marker_capacity) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_capacity);
			}
			const auto name   = std::string{ aApprovalId };
			const auto bytes  = marker_bytes(aApprovalId);
			auto       record = file_descriptor{ ::openat(
				aNamespace.Path.Directory.get(),
				name.c_str(),
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600) };
			if (!record) {
				return std::unexpected(
					errno == EEXIST
						? designated_review_bootstrap_failure::artifact_replayed
						: designated_review_bootstrap_failure::spent_state_failure);
			}
			aMarkerCreationBegan = true;
			struct stat identity{ };
			if (!safe_private_regular(record.get(), &identity)
				|| !write_descriptor(record.get(), bytes)) {
				stabilize_failed_marker(aNamespace, name, bytes);
				return std::unexpected(
					designated_review_bootstrap_failure::spent_state_failure);
			}
			if (!durable_sync(record.get())) {
				stabilize_failed_marker(aNamespace, name, bytes);
				return std::unexpected(
					designated_review_bootstrap_failure::file_sync_failure);
			}
			if (!durable_sync(aNamespace.Path.Directory.get())) {
				stabilize_failed_marker(aNamespace, name, bytes);
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			apply_marker_mutation_for_testing(aNamespace, name, bytes);
			struct stat retainedStatus{ };
			struct stat namedStatus{ };
			if (!safe_private_regular(record.get(), &retainedStatus)
				|| !same_identity(retainedStatus, identity)
				|| ::fstatat(
					   aNamespace.Path.Directory.get(),
					   name.c_str(),
					   &namedStatus,
					   AT_SYMLINK_NOFOLLOW)
					!= 0
				|| namedStatus.st_nlink != 1 || !S_ISREG(namedStatus.st_mode)
				|| !same_identity(namedStatus, identity)
				|| !replay_namespace_matches(aNamespace)) {
				stabilize_failed_marker(aNamespace, name, bytes);
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_substituted);
			}
			return { };
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
			if (!safe_approval_parent(aApproval.Parent.Directory.get())
				|| !retained_path_matches(aApproval.Parent)) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			const auto quarantine = "." + aApproval.Name + ".bpr-consumed";
	#if defined(__APPLE__)
			if (::renameatx_np(
					aApproval.Parent.Directory.get(),
					aApproval.Name.c_str(),
					aApproval.Parent.Directory.get(),
					quarantine.c_str(),
					RENAME_EXCL)
				!= 0) {
	#elif defined(__linux__)
			if (::syscall(
					SYS_renameat2,
					aApproval.Parent.Directory.get(),
					aApproval.Name.c_str(),
					aApproval.Parent.Directory.get(),
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
					aApproval.Parent.Directory.get(),
					quarantine.c_str(),
					&moved,
					AT_SYMLINK_NOFOLLOW)
					!= 0
				|| moved.st_dev != aApproval.Identity.st_dev
				|| moved.st_ino != aApproval.Identity.st_ino) {
				static_cast<void>(::unlinkat(
					aApproval.Parent.Directory.get(), quarantine.c_str(), 0));
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			if (::unlinkat(
					aApproval.Parent.Directory.get(), quarantine.c_str(), 0)
				!= 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::consume_failure);
			}
			struct stat consumed{ };
			if (::fstat(aApproval.File.get(), &consumed) != 0 || consumed.st_nlink != 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			if (!durable_sync(aApproval.Parent.Directory.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			if (!safe_approval_parent(aApproval.Parent.Directory.get())
				|| !retained_path_matches(aApproval.Parent)) {
				return std::unexpected(
					designated_review_bootstrap_failure::artifact_substituted);
			}
			return { };
		}

		[[nodiscard]] auto load_replay_configuration(
			const std::filesystem::path& aNamespacePath)
			-> std::expected<
				designated_review_replay_configuration,
				designated_review_bootstrap_failure>
		{
			if (!path_components(aNamespacePath).has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto path = open_retained_directory(
				aNamespacePath,
				designated_review_bootstrap_failure::replay_state_absent,
				designated_review_bootstrap_failure::unsafe_replay_state);
			if (!path.has_value()) {
				return std::unexpected(path.error());
			}
			if (!safe_private_directory(path->Directory.get(), &path->Identity)
				|| !retained_path_matches(*path)) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto lock = directory_lock{ path->Directory.get() };
			if (!lock) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto inventory = inspect_replay_namespace(path->Directory.get(), false);
			if (!inventory.has_value()) {
				return std::unexpected(inventory.error());
			}
			if (!inventory->HasMetadata || inventory->HasNext) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_absent);
			}
			auto metadata = open_replay_regular(
				path->Directory.get(),
				replay_metadata_name,
				maximum_replay_metadata_bytes,
				designated_review_bootstrap_failure::replay_state_absent,
				designated_review_bootstrap_failure::unsafe_replay_state);
			if (!metadata.has_value()) {
				return std::unexpected(metadata.error());
			}
			const auto normalizedPath = aNamespacePath.generic_string();
			auto       prefix         = std::string{ };
			prefix.reserve(normalizedPath.size() + 192U);
			prefix.append("schema_name=")
				.append(designated_review_replay_schema)
				.append("\nschema_version=1\nnamespace_path=")
				.append(normalizedPath)
				.append("\nnamespace_id=");
			if (metadata->Bytes.size() != prefix.size() + 65U
				|| !metadata->Bytes.starts_with(prefix)
				|| metadata->Bytes.back() != '\n') {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_mismatch);
			}
			auto configuration = designated_review_replay_configuration{
				.NamespacePath = aNamespacePath,
				.NamespaceId   = metadata->Bytes.substr(prefix.size(), 64U),
			};
			auto expected = replay_metadata_bytes(configuration);
			if (!expected.has_value() || *expected != metadata->Bytes
				|| !retained_path_matches(*path)
				|| !retained_regular_matches(
					path->Directory.get(), replay_metadata_name, *metadata)) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_mismatch);
			}
			return configuration;
		}

		[[nodiscard]] auto initialize_or_reset_replay_namespace(
			const std::filesystem::path& aNamespacePath)
			-> std::expected<
				designated_review_replay_configuration,
				designated_review_bootstrap_failure>
		{
			auto path = create_retained_directory(aNamespacePath);
			if (!path.has_value()) {
				return std::unexpected(path.error());
			}
			if (!safe_private_directory(path->Directory.get(), &path->Identity)
				|| !retained_path_matches(*path)) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto lock = directory_lock{ path->Directory.get() };
			if (!lock) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto inventory = inspect_replay_namespace(path->Directory.get(), true);
			if (!inventory.has_value()) {
				return std::unexpected(inventory.error());
			}
			for (const auto name : { std::string_view{ replay_metadata_name },
									 std::string_view{ replay_metadata_next_name } }) {
				const auto present = name == replay_metadata_name
					? inventory->HasMetadata
					: inventory->HasNext;
				if (!present) {
					continue;
				}
				auto existing = file_descriptor{ ::openat(
					path->Directory.get(),
					std::string{ name }.c_str(),
					O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
				if (!existing || !safe_private_regular(existing.get())) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
			}
			if (inventory->HasNext
				&& ::unlinkat(
					   path->Directory.get(), replay_metadata_next_name, 0)
					!= 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			auto namespaceId = random_namespace_id();
			if (!namespaceId.has_value()) {
				return std::unexpected(namespaceId.error());
			}
			auto configuration = designated_review_replay_configuration{
				.NamespacePath = aNamespacePath,
				.NamespaceId   = std::move(*namespaceId),
			};
			auto metadataBytes = replay_metadata_bytes(configuration);
			if (!metadataBytes.has_value()) {
				return std::unexpected(
					designated_review_bootstrap_failure::invalid_configuration);
			}
			auto        next = file_descriptor{ ::openat(
				path->Directory.get(),
				replay_metadata_next_name,
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600) };
			struct stat nextIdentity{ };
			if (!next || !safe_private_regular(next.get(), &nextIdentity)
				|| !write_descriptor(next.get(), *metadataBytes)) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			if (!durable_sync(next.get())) {
				static_cast<void>(::unlinkat(
					path->Directory.get(), replay_metadata_next_name, 0));
				return std::unexpected(
					designated_review_bootstrap_failure::file_sync_failure);
			}
			if (::renameat(
					path->Directory.get(),
					replay_metadata_next_name,
					path->Directory.get(),
					replay_metadata_name)
				!= 0) {
				return std::unexpected(
					designated_review_bootstrap_failure::unsafe_replay_state);
			}
			if (!durable_sync(path->Directory.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			for (const auto& marker : inventory->Markers) {
				if (::unlinkat(path->Directory.get(), marker.c_str(), 0) != 0) {
					return std::unexpected(
						designated_review_bootstrap_failure::unsafe_replay_state);
				}
			}
			if (!inventory->Markers.empty()
				&& !durable_sync(path->Directory.get())) {
				return std::unexpected(
					designated_review_bootstrap_failure::directory_sync_failure);
			}
			if (!retained_path_matches(*path)) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_substituted);
			}
			auto metadata = open_replay_regular(
				path->Directory.get(),
				replay_metadata_name,
				maximum_replay_metadata_bytes,
				designated_review_bootstrap_failure::replay_state_absent,
				designated_review_bootstrap_failure::unsafe_replay_state);
			if (!metadata.has_value() || metadata->Bytes != *metadataBytes) {
				return std::unexpected(
					designated_review_bootstrap_failure::replay_state_mismatch);
			}
			return configuration;
		}
#endif
	} // namespace

	auto designated_review_replay_initializer::initialize_or_reset(
		const std::filesystem::path& aNamespacePath)
		-> std::expected<
			designated_review_replay_configuration,
			designated_review_bootstrap_failure>
	{
#if defined(_WIN32)
		static_cast<void>(aNamespacePath);
		return std::unexpected(
			designated_review_bootstrap_failure::platform_unavailable);
#else
		try {
			return initialize_or_reset_replay_namespace(aNamespacePath);
		} catch (...) {
			return std::unexpected(
				designated_review_bootstrap_failure::unsafe_replay_state);
		}
#endif
	}

	auto designated_review_replay_initializer::load(
		const std::filesystem::path& aNamespacePath)
		-> std::expected<
			designated_review_replay_configuration,
			designated_review_bootstrap_failure>
	{
#if defined(_WIN32)
		static_cast<void>(aNamespacePath);
		return std::unexpected(
			designated_review_bootstrap_failure::platform_unavailable);
#else
		try {
			return load_replay_configuration(aNamespacePath);
		} catch (...) {
			return std::unexpected(
				designated_review_bootstrap_failure::unsafe_replay_state);
		}
#endif
	}

	auto designated_review_approval_subject(
		const designated_review_bootstrap_request& aRequest,
		const std::string_view                     aApplicationSha256,
		const std::string_view                     aApprovalId)
		-> std::expected<std::string, designated_review_bootstrap_failure>
	{
		const auto replayPath = aRequest.Replay.NamespacePath.generic_string();
		if (aRequest.Replay.NamespacePath.empty()
			|| !aRequest.Replay.NamespacePath.is_absolute()
			|| aRequest.Replay.NamespacePath.lexically_normal()
				!= aRequest.Replay.NamespacePath
			|| aRequest.Replay.NamespacePath.filename().empty()
			|| replayPath.contains('\0') || replayPath.contains('\n')
			|| replayPath.contains('\r')
			|| !lowercase_sha256(aRequest.Replay.NamespaceId)
			|| aRequest.BuildTesting
			|| aRequest.BuildConfiguration != "Release"
			|| !lowercase_sha256(aRequest.SourceSha256)
			|| !lowercase_sha256(aRequest.SupervisorSha256)
			|| !lowercase_sha256(aRequest.InventorySha256)
			|| !lowercase_sha256(aApprovalId)
			|| !lowercase_sha256(aApplicationSha256)) {
			return std::unexpected(
				designated_review_bootstrap_failure::invalid_configuration);
		}
		auto result = std::string{ };
		result.reserve(1'024U + replayPath.size());
		result.append("schema_name=bpr.designated_live_review_approval\n")
			// Three, not two. The subject now says which supervisor and which
			// inventory the approval was granted against, so an approval bound
			// to the two-field subject describes a different question and must
			// be refused rather than read as though it answered this one.
			.append("schema_version=3\n")
			.append("approval_statement=")
			.append(designated_approval_statement)
			.append("\napplication_sha256=")
			.append(aApplicationSha256)
			.append("\nsource_sha256=")
			.append(aRequest.SourceSha256)
			.append("\nsupervisor_sha256=")
			.append(aRequest.SupervisorSha256)
			.append("\ninventory_sha256=")
			.append(aRequest.InventorySha256)
			.append("\napproval_id=")
			.append(aApprovalId)
			.append("\nbuild_configuration=Release\n")
			.append("build_testing=off\n")
			.append("replay_schema=")
			.append(designated_review_replay_schema)
			.append("\nreplay_namespace_path=")
			.append(replayPath)
			.append("\nreplay_namespace_id=")
			.append(aRequest.Replay.NamespaceId)
			.append("\n")
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
			auto replayNamespace = open_replay_namespace(aRequest.Replay);
			if (!replayNamespace.has_value()) {
				return std::unexpected(replayNamespace.error());
			}
			if (const auto replay = refuse_known_replay(
					*replayNamespace, *approvalId);
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
			auto       markerCreationBegan = false;
			const auto spent               = record_spent(
				*replayNamespace, *approvalId, markerCreationBegan);
			if (!spent.has_value()) {
				if (markerCreationBegan) {
					static_cast<void>(consume(*approval));
				}
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

	void mutate_designated_review_marker_after_sync_for_testing(
		const designated_review_marker_mutation_for_testing aMutation) noexcept
	{
		MarkerMutation.store(aMutation, std::memory_order_release);
	}
#endif
} // namespace bpr::application::detail
