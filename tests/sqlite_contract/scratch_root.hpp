#ifndef BPR_TESTS_SQLITE_CONTRACT_SCRATCH_ROOT_HPP
#define BPR_TESTS_SQLITE_CONTRACT_SCRATCH_ROOT_HPP

/// \file
/// \brief A scratch root that a course makes for itself, at a name nobody
/// could have taken first.
///
/// \details [[BPR-1126]]. Four storage contracts used to name their roots
/// `bpr-<store>-<N>` in the system temporary directory, from a counter, and to
/// remove whatever was at that name before they made it. In a shared `/tmp`
/// the name is known before the run starts. So the run removed a directory or
/// a link that someone else had put there. If a link was put back between the
/// removal and the creation, the course worked in the link's target, and
/// removed the target when it ended.
///
/// This root is made the way `mkdtemp` makes one. The name is random, and the
/// directory is made only if nothing is at that name, so nothing already there
/// is removed, followed or reused. The root is owner-private before anything
/// is written in it. A `scratch_guard` ([[BPR-1121]]) owns it from the moment
/// it exists, and only its owner removes it, with everything in it.

#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
	#include "bpr/platform/windows_private_object.hpp"
	#include "platform_test_support.hpp"
#else
	#include <cerrno>
	#include <unistd.h>
#endif

namespace bpr::tests::sqlite_contract {
	/// \brief How many random names a Windows root tries before the course fails.
	inline constexpr auto scratch_root_attempts = 16;

	/// \brief Creates a directory at `aPath` only if nothing is at that name.
	/// \details On Windows the directory is created with the platform's
	/// owner-private descriptor ([[BPR-0825]]): the token user owns it and holds
	/// the one full-access grant, and the AppContainer the process runs in, if
	/// any, holds the second. So it never carries the temporary directory's
	/// inherited list, which can grant other accounts, and a contained course
	/// can still use it ([[BPR-1126]]). Elsewhere it is
	/// `std::filesystem::create_directory`, and the caller narrows it.
	/// \returns Whether the directory was made. A name that is taken leaves
	/// `aError` clear or sets it to `std::errc::file_exists`; any other failure
	/// sets it to that failure.
	[[nodiscard]] inline auto create_directory_only_if_absent(
		const std::filesystem::path& aPath,
		std::error_code&             aError) -> bool
	{
#if defined(_WIN32)
		namespace object = platform::windows_private_object;
		auto user        = current_user_sid_storage();
		auto container   = current_appcontainer_sid_storage();
		expect(!user.empty(), "the process token names its user");
		auto descriptor = object::descriptor{ };
		expect(descriptor.initialize(
				   reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,
				   container.empty() ? nullptr : static_cast<PSID>(container.data()),
				   object::object_kind::directory),
			   "the owner-private descriptor is built");
		if (::CreateDirectoryW(aPath.c_str(), descriptor.attributes()) != 0) {
			aError.clear();
			return true;
		}
		const auto code = ::GetLastError();
		aError          = code == ERROR_ALREADY_EXISTS
			? std::make_error_code(std::errc::file_exists)
			: std::error_code{ static_cast<int>(code), std::system_category() };
		return false;
#else
		return std::filesystem::create_directory(aPath, aError);
#endif
	}

	/// \brief Makes a directory at `aPrefix` and a random part, in the system
	/// temporary directory, the way `mkdtemp` does where there is none.
	/// \details A name that something already has is left as it is, and another
	/// is drawn. The directory is made private by
	/// `create_directory_only_if_absent`, and the stores' own rule checks it
	/// before this returns. Used on Windows, and compiled everywhere.
	/// \returns The directory made. The caller owns it from the return.
	[[nodiscard]] inline auto make_directory_at_random_name(const std::string_view aPrefix)
		-> std::filesystem::path
	{
		auto       entropy = std::random_device{ };
		const auto base    = std::filesystem::temp_directory_path();
		for (auto attempt = 0; attempt < scratch_root_attempts; ++attempt) {
			const auto value = (std::uint64_t{ entropy() } << 32U) | std::uint64_t{ entropy() };
			auto       text  = std::array<char, 16>{ };
			const auto end   = std::to_chars(text.data(), text.data() + text.size(), value, 16).ptr;
			auto       path  = base / (std::string{ aPrefix } + std::string(text.data(), end));
			auto       error = std::error_code{ };
			if (create_directory_only_if_absent(path, error)) {
				// [[BPR-1121]]. Removed again if it cannot be made private.
				auto made = scratch_guard{ path };
				expect(adapters::sqlite::make_storage_root_owner_private(path), "the scratch root is owner-private before anything is written in it");
				made.release();
				return path;
			}
			expect(!error || error == std::errc::file_exists, "a scratch root fails to be made only because its name is taken: " + error.message());
		}
		fail("a scratch root is made at a name nothing had");
	}

#if defined(_WIN32)
	/// \brief The root as it resolves, or as it was made inside an AppContainer.
	/// \details Inside an AppContainer `canonical` refuses every path with
	/// "Access is denied", the container's own temporary directory included. It
	/// asks for the name's DOS form, and a container may not read which drive
	/// letter a volume has ([[BPR-0362]], `dos-volume-name`). The root's list is
	/// not the cause: it grants the container full control ([[BPR-1126]]). So
	/// there the root is kept as it was made, at a name nothing had, directly in
	/// the temporary directory the system gave the process. Outside a container,
	/// and for any other refusal, the course fails.
	[[nodiscard]] inline auto resolved_root(const std::filesystem::path& aMade)
		-> std::filesystem::path
	{
		auto error    = std::error_code{ };
		auto resolved = std::filesystem::canonical(aMade, error);
		if (!error) {
			return resolved;
		}
		expect(error == std::errc::permission_denied && !current_appcontainer_sid_storage().empty(),
			   "the scratch root resolves, unless an AppContainer may not ask: " + error.message());
		return aMade;
	}
#endif

	/// \brief A directory in the system temporary directory that the course
	/// made, at a random name, owner-private from creation. It is removed, with
	/// everything in it, when its owner goes.
	/// \details A contract derives its own scratch type from this one, to put its
	/// label in the name. On POSIX the directory is made by `mkdtemp`, which
	/// creates it mode `0700`; on Windows by `make_directory_at_random_name`,
	/// and resolved by `resolved_root`.
	class scratch_root
	{
	public:
		/// \param aLabel Names the contract: the root is `bpr-<label>-` and a
		/// random part, so that a root a killed run leaves can be traced.
		explicit scratch_root(const std::string_view aLabel)
		{
			const auto prefix = "bpr-" + std::string{ aLabel } + "-";
#if defined(_WIN32)
			const auto made = make_directory_at_random_name(prefix);
#else
			auto pattern = (std::filesystem::temp_directory_path() / (prefix + "XXXXXX")).string();
			if (::mkdtemp(pattern.data()) == nullptr) {
				const auto error = errno;
				fail("a scratch root is made at a name nothing had: " + std::generic_category().message(error));
			}
			const auto made = std::filesystem::path{ pattern };
#endif
			// [[BPR-1121]]. Owned from the moment it exists.
			auto owned = scratch_guard{ made };
			// The stores require a root to be the path it resolves to, and the
			// temporary directory on macOS is reached through a link.
#if defined(_WIN32)
			Path = resolved_root(made);
#else
			Path = std::filesystem::canonical(made);
#endif
			owned.release();
		}
		~scratch_root()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
		scratch_root(const scratch_root&)                    = delete;
		auto operator=(const scratch_root&) -> scratch_root& = delete;
		scratch_root(scratch_root&&)                         = delete;
		auto operator=(scratch_root&&) -> scratch_root&      = delete;

		/// \brief The root, as it resolves.
		[[nodiscard]] auto path() const -> const std::filesystem::path&
		{
			return Path;
		}

		/// \brief A name directly under the root.
		[[nodiscard]] auto file(const std::string_view aName) const
			-> std::filesystem::path
		{
			return Path / aName;
		}

	private:
		std::filesystem::path Path;
	};
} // namespace bpr::tests::sqlite_contract

#endif // BPR_TESTS_SQLITE_CONTRACT_SCRATCH_ROOT_HPP
