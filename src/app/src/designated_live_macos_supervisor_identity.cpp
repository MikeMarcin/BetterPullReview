#include "designated_live_macos_supervisor_identity.hpp"

#if defined(__APPLE__)

	#include <CommonCrypto/CommonDigest.h>
	#include <fcntl.h>
	#include <sys/socket.h>
	#include <unistd.h>

	#include <algorithm>
	#include <array>
	#include <cerrno>
	#include <cstddef>
	#include <cstdint>
	#include <span>
	#include <string_view>
	#include <utility>

namespace bpr::app::designated_live_supervisor_identity {
	namespace {
		constexpr auto maximum_inventory_bytes = std::size_t{ 16U * 1024U * 1024U };
		constexpr auto maximum_verifier_bytes  = std::size_t{ 4U * 1024U * 1024U };
		constexpr auto read_chunk_bytes        = std::size_t{ 64U * 1024U };
		constexpr auto verifier_relative       = std::string_view{
			"BetterPullReview.app/Contents/Resources/"
			"designated_live_macos_capsule.py"
		};

		[[nodiscard]] auto normalized_absolute(
			const std::filesystem::path& aPath) -> bool
		{
			return aPath.is_absolute() && aPath.lexically_normal() == aPath
				&& !aPath.filename().empty();
		}

		[[nodiscard]] auto private_directory(const struct stat& aStatus) -> bool
		{
			return S_ISDIR(aStatus.st_mode) && aStatus.st_uid == ::geteuid()
				&& (aStatus.st_mode & static_cast<mode_t>(0077)) == 0;
		}

		[[nodiscard]] auto private_regular_file(
			const struct stat& aStatus) -> bool
		{
			return S_ISREG(aStatus.st_mode) && aStatus.st_uid == ::geteuid()
				&& (aStatus.st_mode & static_cast<mode_t>(0077)) == 0 && aStatus.st_nlink == 1
				&& aStatus.st_size > 0
				&& static_cast<std::uintmax_t>(aStatus.st_size)
				<= maximum_inventory_bytes;
		}

		[[nodiscard]] auto same_identity(
			const struct stat& aLeft, const struct stat& aRight) -> bool
		{
			return aLeft.st_dev == aRight.st_dev && aLeft.st_ino == aRight.st_ino
				&& aLeft.st_mode == aRight.st_mode && aLeft.st_uid == aRight.st_uid
				&& aLeft.st_gid == aRight.st_gid && aLeft.st_nlink == aRight.st_nlink
				&& aLeft.st_size == aRight.st_size
				&& aLeft.st_mtimespec.tv_sec == aRight.st_mtimespec.tv_sec
				&& aLeft.st_mtimespec.tv_nsec == aRight.st_mtimespec.tv_nsec;
		}

		[[nodiscard]] auto path_still_names(
			const int aParent, const std::string& aName, const struct stat& aExpected)
			-> bool
		{
			struct stat observed{ };
			return aParent >= 0
				&& ::fstatat(
					   aParent, aName.c_str(), &observed, AT_SYMLINK_NOFOLLOW)
				== 0
				&& same_identity(observed, aExpected);
		}

		[[nodiscard]] auto descriptor_still_names(
			const int aDescriptor, const struct stat& aExpected) -> bool
		{
			struct stat observed{ };
			return aDescriptor >= 0 && ::fstat(aDescriptor, &observed) == 0
				&& same_identity(observed, aExpected);
		}

		[[nodiscard]] auto read_exact_file(const int aDescriptor)
			-> std::optional<std::vector<std::uint8_t>>
		{
			auto bytes  = std::vector<std::uint8_t>{ };
			auto buffer = std::array<std::uint8_t, read_chunk_bytes>{ };
			auto offset = off_t{ 0 };
			for (;;) {
				const auto count = ::pread(
					aDescriptor, buffer.data(), buffer.size(), offset);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count < 0) {
					return std::nullopt;
				}
				if (count == 0) {
					break;
				}
				const auto size = static_cast<std::size_t>(count);
				if (size > maximum_inventory_bytes - bytes.size()) {
					return std::nullopt;
				}
				bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
				offset += count;
			}
			return bytes.empty() ? std::nullopt
								 : std::optional{ std::move(bytes) };
		}

		[[nodiscard]] auto canonical_sha256(const std::string_view aValue) noexcept
			-> bool
		{
			if (aValue.size() != 64U) {
				return false;
			}
			for (const auto character : aValue) {
				if (!((character >= '0' && character <= '9')
					  || (character >= 'a' && character <= 'f'))) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto open_verifier(const int aRootDescriptor)
			-> std::optional<int>
		{
			auto directory = ::openat(
				aRootDescriptor, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (directory < 0) {
				return std::nullopt;
			}
			const auto relative = std::filesystem::path{ verifier_relative };
			for (const auto& component : relative.parent_path()) {
				const auto child = ::openat(
					directory,
					component.c_str(),
					O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
				static_cast<void>(::close(directory));
				if (child < 0) {
					return std::nullopt;
				}
				directory = child;
			}
			const auto verifier = ::openat(
				directory,
				relative.filename().c_str(),
				O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			static_cast<void>(::close(directory));
			return verifier < 0 ? std::nullopt : std::optional{ verifier };
		}

		[[nodiscard]] auto sha256_hex(
			const std::span<const std::uint8_t> aBytes) -> std::string
		{
			auto digest = std::array<unsigned char, CC_SHA256_DIGEST_LENGTH>{ };
			CC_SHA256(
				aBytes.data(), static_cast<CC_LONG>(aBytes.size()), digest.data());
			static constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto                  result = std::string{ };
			result.reserve(digest.size() * 2U);
			for (const auto byte : digest) {
				result.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
				result.push_back(digits[byte & 0x0fU]);
			}
			return result;
		}

		[[nodiscard]] auto transmit_stream(
			int& aDescriptor, const std::span<const std::uint8_t> aBytes) -> bool
		{
			auto abort = [&] {
				if (aDescriptor >= 0) {
					static_cast<void>(::shutdown(aDescriptor, SHUT_RDWR));
					static_cast<void>(::close(aDescriptor));
					aDescriptor = -1;
				}
				return false;
			};
			if (aDescriptor < 0 || aBytes.empty()) {
				return abort();
			}
			const auto enabled = int{ 1 };
			if (::setsockopt(
					aDescriptor,
					SOL_SOCKET,
					SO_NOSIGPIPE,
					&enabled,
					static_cast<socklen_t>(sizeof(enabled)))
				!= 0) {
				return abort();
			}
			auto written = std::size_t{ 0U };
			while (written < aBytes.size()) {
				const auto count = ::send(
					aDescriptor,
					aBytes.data() + written,
					aBytes.size() - written,
					0);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					return abort();
				}
				written += static_cast<std::size_t>(count);
			}
			// Keep the shut-down endpoint alive until the child exits. This still
			// delivers EOF, while preserving the anonymous peer identity long
			// enough for the verifier to inspect it deterministically.
			if (::shutdown(aDescriptor, SHUT_WR) != 0) {
				return abort();
			}
			return true;
		}
	} // namespace

	verifier_transport::verifier_transport(
		const int aParentVerifierDescriptor,
		const int aChildVerifierDescriptor,
		const int aParentInventoryDescriptor,
		const int aChildInventoryDescriptor) noexcept
		: ParentVerifierDescriptor{ aParentVerifierDescriptor }
		, ChildVerifierDescriptor{ aChildVerifierDescriptor }
		, ParentInventoryDescriptor{ aParentInventoryDescriptor }
		, ChildInventoryDescriptor{ aChildInventoryDescriptor }
	{
	}

	verifier_transport::verifier_transport(verifier_transport&& aOther) noexcept
		: ParentVerifierDescriptor{ std::exchange(
			  aOther.ParentVerifierDescriptor, -1) }
		, ChildVerifierDescriptor{ std::exchange(
			  aOther.ChildVerifierDescriptor, -1) }
		, ParentInventoryDescriptor{ std::exchange(
			  aOther.ParentInventoryDescriptor, -1) }
		, ChildInventoryDescriptor{ std::exchange(
			  aOther.ChildInventoryDescriptor, -1) }
	{
	}

	auto verifier_transport::operator=(verifier_transport&& aOther) noexcept
		-> verifier_transport&
	{
		if (this != &aOther) {
			close_all();
			ParentVerifierDescriptor = std::exchange(
				aOther.ParentVerifierDescriptor, -1);
			ChildVerifierDescriptor = std::exchange(
				aOther.ChildVerifierDescriptor, -1);
			ParentInventoryDescriptor = std::exchange(
				aOther.ParentInventoryDescriptor, -1);
			ChildInventoryDescriptor = std::exchange(
				aOther.ChildInventoryDescriptor, -1);
		}
		return *this;
	}

	verifier_transport::~verifier_transport() { close_all(); }

	void verifier_transport::close_all() noexcept
	{
		for (const auto descriptor : {
				 ChildInventoryDescriptor,
				 ParentInventoryDescriptor,
				 ChildVerifierDescriptor,
				 ParentVerifierDescriptor,
			 }) {
			if (descriptor >= 0) {
				static_cast<void>(::close(descriptor));
			}
		}
		ChildInventoryDescriptor  = -1;
		ParentInventoryDescriptor = -1;
		ChildVerifierDescriptor   = -1;
		ParentVerifierDescriptor  = -1;
	}

	auto session::open(
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aInventory,
		const std::string_view       aTrustedVerifierSha256)
		-> std::optional<session>
	{
		if (!normalized_absolute(aRoot) || !normalized_absolute(aInventory)
			|| !canonical_sha256(aTrustedVerifierSha256)) {
			return std::nullopt;
		}
		auto result                 = session{ };
		result.RootPath             = aRoot;
		result.RootName             = aRoot.filename().string();
		result.InventoryName        = aInventory.filename().string();
		result.RootParentDescriptor = ::open(
			aRoot.parent_path().c_str(),
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		result.InventoryParentDescriptor = ::open(
			aInventory.parent_path().c_str(),
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		struct stat rootParent{ };
		struct stat inventoryParent{ };
		if (result.RootParentDescriptor < 0
			|| result.InventoryParentDescriptor < 0
			|| ::fstat(result.RootParentDescriptor, &rootParent) != 0
			|| ::fstat(result.InventoryParentDescriptor, &inventoryParent) != 0
			|| !private_directory(rootParent) || !private_directory(inventoryParent)) {
			return std::nullopt;
		}
		result.RootDescriptor = ::openat(
			result.RootParentDescriptor,
			result.RootName.c_str(),
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		result.InventoryDescriptor = ::openat(
			result.InventoryParentDescriptor,
			result.InventoryName.c_str(),
			O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
		if (result.RootDescriptor < 0 || result.InventoryDescriptor < 0
			|| ::fstat(result.RootDescriptor, &result.RootIdentity) != 0
			|| ::fstat(result.InventoryDescriptor, &result.InventoryIdentity) != 0
			|| !private_directory(result.RootIdentity)
			|| !private_regular_file(result.InventoryIdentity)) {
			return std::nullopt;
		}
		auto        inventoryBytes = read_exact_file(result.InventoryDescriptor);
		struct stat inventoryAfter{ };
		if (!inventoryBytes.has_value()
			|| ::fstat(result.InventoryDescriptor, &inventoryAfter) != 0
			|| !same_identity(result.InventoryIdentity, inventoryAfter)) {
			return std::nullopt;
		}
		const auto verifier = open_verifier(result.RootDescriptor);
		if (!verifier.has_value()) {
			return std::nullopt;
		}
		result.VerifierDescriptor = *verifier;
		if (::fstat(result.VerifierDescriptor, &result.VerifierIdentity) != 0
			|| !S_ISREG(result.VerifierIdentity.st_mode)
			|| result.VerifierIdentity.st_uid != ::geteuid()
			|| (result.VerifierIdentity.st_mode & static_cast<mode_t>(0022)) != 0
			|| result.VerifierIdentity.st_size <= 0
			|| static_cast<std::uintmax_t>(result.VerifierIdentity.st_size)
				> maximum_verifier_bytes) {
			return std::nullopt;
		}
		auto        verifierBytes = read_exact_file(result.VerifierDescriptor);
		struct stat verifierAfter{ };
		if (!verifierBytes.has_value()
			|| verifierBytes->size() > maximum_verifier_bytes
			|| ::fstat(result.VerifierDescriptor, &verifierAfter) != 0
			|| !same_identity(result.VerifierIdentity, verifierAfter)) {
			return std::nullopt;
		}
		result.VerifierSha256 = sha256_hex(*verifierBytes);
		if (result.VerifierSha256 != aTrustedVerifierSha256) {
			return std::nullopt;
		}
		result.VerifierBytes   = std::move(*verifierBytes);
		result.InventoryBytes  = std::move(*inventoryBytes);
		result.InventorySha256 = sha256_hex(result.InventoryBytes);
		auto captured          = designated_live_capsule_identity::capture(
			result.RootDescriptor);
		if (!captured.has_value()) {
			return std::nullopt;
		}
		result.CapsuleIdentity = std::move(*captured);
		return result.revalidate() ? std::optional{ std::move(result) }
								   : std::nullopt;
	}

	session::session(session&& aOther) noexcept
		: RootPath{ std::move(aOther.RootPath) }
		, RootName{ std::move(aOther.RootName) }
		, InventoryName{ std::move(aOther.InventoryName) }
		, InventorySha256{ std::move(aOther.InventorySha256) }
		, VerifierSha256{ std::move(aOther.VerifierSha256) }
		, RootParentDescriptor{ std::exchange(aOther.RootParentDescriptor, -1) }
		, RootDescriptor{ std::exchange(aOther.RootDescriptor, -1) }
		, InventoryParentDescriptor{ std::exchange(
			  aOther.InventoryParentDescriptor, -1) }
		, InventoryDescriptor{ std::exchange(aOther.InventoryDescriptor, -1) }
		, VerifierDescriptor{ std::exchange(aOther.VerifierDescriptor, -1) }
		, RootIdentity{ aOther.RootIdentity }
		, InventoryIdentity{ aOther.InventoryIdentity }
		, VerifierIdentity{ aOther.VerifierIdentity }
		, InventoryBytes{ std::move(aOther.InventoryBytes) }
		, VerifierBytes{ std::move(aOther.VerifierBytes) }
		, CapsuleIdentity{ std::move(aOther.CapsuleIdentity) }
	{
	}

	auto session::operator=(session&& aOther) noexcept -> session&
	{
		if (this != &aOther) {
			close_all();
			RootPath                  = std::move(aOther.RootPath);
			RootName                  = std::move(aOther.RootName);
			InventoryName             = std::move(aOther.InventoryName);
			InventorySha256           = std::move(aOther.InventorySha256);
			VerifierSha256            = std::move(aOther.VerifierSha256);
			RootParentDescriptor      = std::exchange(aOther.RootParentDescriptor, -1);
			RootDescriptor            = std::exchange(aOther.RootDescriptor, -1);
			InventoryParentDescriptor = std::exchange(
				aOther.InventoryParentDescriptor, -1);
			InventoryDescriptor = std::exchange(aOther.InventoryDescriptor, -1);
			VerifierDescriptor  = std::exchange(aOther.VerifierDescriptor, -1);
			RootIdentity        = aOther.RootIdentity;
			InventoryIdentity   = aOther.InventoryIdentity;
			VerifierIdentity    = aOther.VerifierIdentity;
			InventoryBytes      = std::move(aOther.InventoryBytes);
			VerifierBytes       = std::move(aOther.VerifierBytes);
			CapsuleIdentity     = std::move(aOther.CapsuleIdentity);
		}
		return *this;
	}

	session::~session() { close_all(); }

	auto session::root_path() const noexcept -> const std::filesystem::path&
	{
		return RootPath;
	}

	auto session::root_descriptor() const noexcept -> int
	{
		return RootDescriptor;
	}

	auto session::inventory_sha256() const noexcept -> std::string_view
	{
		return InventorySha256;
	}

	auto session::revalidate() const -> bool
	{
		auto verifierBytes = read_exact_file(VerifierDescriptor);
		return path_still_names(
				   RootParentDescriptor, RootName, RootIdentity)
			&& descriptor_still_names(RootDescriptor, RootIdentity)
			&& path_still_names(
				   InventoryParentDescriptor,
				   InventoryName,
				   InventoryIdentity)
			&& descriptor_still_names(InventoryDescriptor, InventoryIdentity)
			&& descriptor_still_names(VerifierDescriptor, VerifierIdentity)
			&& verifierBytes.has_value()
			&& verifierBytes->size() <= maximum_verifier_bytes
			&& sha256_hex(*verifierBytes) == VerifierSha256
			&& designated_live_capsule_identity::unchanged(
				   RootDescriptor, CapsuleIdentity);
	}

	auto session::configure_verifier_spawn(posix_spawn_file_actions_t& aActions) const
		-> std::optional<verifier_transport>
	{
		auto verifierDescriptors  = std::array<int, 2>{ -1, -1 };
		auto inventoryDescriptors = std::array<int, 2>{ -1, -1 };
		if (RootDescriptor < 0 || VerifierDescriptor < 0
			|| ::socketpair(
				   AF_UNIX, SOCK_STREAM, 0, verifierDescriptors.data())
				!= 0) {
			return std::nullopt;
		}
		if (::socketpair(
				AF_UNIX, SOCK_STREAM, 0, inventoryDescriptors.data())
			!= 0) {
			static_cast<void>(::close(verifierDescriptors[0]));
			static_cast<void>(::close(verifierDescriptors[1]));
			return std::nullopt;
		}
		auto transport = verifier_transport{
			verifierDescriptors[0],
			verifierDescriptors[1],
			inventoryDescriptors[0],
			inventoryDescriptors[1],
		};
		const auto descriptorCollision = std::ranges::any_of(
			std::array{
				RootDescriptor,
				VerifierDescriptor,
				transport.ParentVerifierDescriptor,
				transport.ChildVerifierDescriptor,
				transport.ParentInventoryDescriptor,
				transport.ChildInventoryDescriptor,
			},
			[](const int aDescriptor) {
				return aDescriptor == session::verifier_child_descriptor()
					|| aDescriptor == session::inventory_child_descriptor();
			});
		if (descriptorCollision
			|| ::fcntl(transport.ParentVerifierDescriptor, F_SETFD, FD_CLOEXEC) != 0
			|| ::fcntl(transport.ChildVerifierDescriptor, F_SETFD, FD_CLOEXEC) != 0
			|| ::fcntl(transport.ParentInventoryDescriptor, F_SETFD, FD_CLOEXEC) != 0
			|| ::fcntl(transport.ChildInventoryDescriptor, F_SETFD, FD_CLOEXEC) != 0
			|| posix_spawn_file_actions_addfchdir_np(&aActions, RootDescriptor) != 0
			|| posix_spawn_file_actions_adddup2(
				   &aActions,
				   transport.ChildVerifierDescriptor,
				   verifier_child_descriptor())
				!= 0
			|| posix_spawn_file_actions_adddup2(
				   &aActions,
				   transport.ChildInventoryDescriptor,
				   inventory_child_descriptor())
				!= 0
			|| posix_spawn_file_actions_addclose(
				   &aActions, transport.ParentVerifierDescriptor)
				!= 0
			|| posix_spawn_file_actions_addclose(
				   &aActions, transport.ParentInventoryDescriptor)
				!= 0) {
			return std::nullopt;
		}
		return transport;
	}

	auto session::transmit_verifier_and_inventory(
		verifier_transport& aTransport) const -> bool
	{
		if (aTransport.ChildVerifierDescriptor < 0
			|| aTransport.ChildInventoryDescriptor < 0) {
			return false;
		}
		static_cast<void>(::close(aTransport.ChildVerifierDescriptor));
		static_cast<void>(::close(aTransport.ChildInventoryDescriptor));
		aTransport.ChildVerifierDescriptor  = -1;
		aTransport.ChildInventoryDescriptor = -1;
		if (!transmit_stream(
				aTransport.ParentVerifierDescriptor, VerifierBytes)) {
			static_cast<void>(::shutdown(
				aTransport.ParentInventoryDescriptor, SHUT_RDWR));
			static_cast<void>(::close(aTransport.ParentInventoryDescriptor));
			aTransport.ParentInventoryDescriptor = -1;
			return false;
		}
		return transmit_stream(
			aTransport.ParentInventoryDescriptor, InventoryBytes);
	}

	auto session::configure_application_spawn(
		posix_spawn_file_actions_t& aActions) const -> bool
	{
		return RootDescriptor >= 0
			&& posix_spawn_file_actions_addfchdir_np(&aActions, RootDescriptor) == 0;
	}

	void session::close_all() noexcept
	{
		for (const auto descriptor : {
				 VerifierDescriptor,
				 InventoryDescriptor,
				 InventoryParentDescriptor,
				 RootDescriptor,
				 RootParentDescriptor,
			 }) {
			if (descriptor >= 0) {
				static_cast<void>(::close(descriptor));
			}
		}
		VerifierDescriptor        = -1;
		InventoryDescriptor       = -1;
		InventoryParentDescriptor = -1;
		RootDescriptor            = -1;
		RootParentDescriptor      = -1;
	}
}

#endif // defined(__APPLE__)
