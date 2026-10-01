#ifndef BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP
#define BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP

/// \file
/// \brief A scratch directory that a test makes for itself in the system
/// temporary directory, at a name nobody could have taken first.
///
/// \details [[BPR-1128]]. Tests named their roots from the clock, or from an
/// address and the process id, and made them with `create_directory`, which
/// does not fail when a directory, or a link to one, is already at the name.
/// So what was there was used: the course worked in it and removed it, with
/// everything in it, at the end. A link was followed, and some tests then
/// narrowed and resolved it, so they worked in the link's target and removed
/// that.
///
/// On POSIX `mkdtemp` makes the directory only if nothing is at the name, so
/// nothing already there is removed, followed or reused. It is mode `0700` from
/// creation.
///
/// [[BPR-1138]]. Windows has no `mkdtemp`, so the directory is made the way it
/// makes one: at a random name, by `create_directory_only_if_absent`, which
/// fails rather than reuse a name that is taken. The directory carries the
/// platform's owner-private descriptor from creation ([[BPR-0825]]). The token
/// user holds the one full-access grant, and so does the AppContainer the
/// process runs in, if any, so a contained test can still use it
/// ([[BPR-1126]]). The SQLite contracts' `tests/sqlite_contract/scratch_root.hpp`
/// shares this Windows branch, and adds the stores' own check of the root.

#include "test_expectation.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#if defined(_WIN32)
	#include "../sqlite_contract/platform_test_support.hpp"
	#include "bpr/platform/windows_private_object.hpp"
#else
	#include <cerrno>
	#include <unistd.h>
#endif

namespace bpr::tests {
	/// \brief How many random names a Windows directory tries before the test
	/// fails.
	inline constexpr auto scratch_directory_attempts = 16;

	/// \brief Creates a directory at `aPath` only if nothing is at that name.
	/// \details On Windows it carries the platform's owner-private descriptor
	/// from creation. Elsewhere it is `std::filesystem::create_directory`, and
	/// the caller narrows it.
	/// \returns Whether the directory was made. A name that is taken sets
	/// `aError` to `std::errc::file_exists`; any other failure sets it to that
	/// failure.
	[[nodiscard]] inline auto create_directory_only_if_absent(
		const std::filesystem::path& aPath,
		std::error_code&             aError) -> bool
	{
#if defined(_WIN32)
		namespace object = platform::windows_private_object;
		auto user        = sqlite_contract::current_user_sid_storage();
		auto container   = sqlite_contract::current_appcontainer_sid_storage();
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
		if (std::filesystem::create_directory(aPath, aError)) {
			return true;
		}
		if (!aError) {
			aError = std::make_error_code(std::errc::file_exists);
		}
		return false;
#endif
	}

	/// \brief Makes a directory at `<aStem>` and a random part, in the system
	/// temporary directory, the way `mkdtemp` does where there is none.
	/// \details A name that something already has is left as it is, and another
	/// is drawn. On Windows the directory is owner-private from creation;
	/// elsewhere the caller narrows it, and `make_scratch_directory` uses
	/// `mkdtemp` instead.
	/// \returns The directory made, not yet resolved. The caller owns it from
	/// the return.
	[[nodiscard]] inline auto make_directory_at_random_name(const std::filesystem::path& aStem)
		-> std::filesystem::path
	{
		auto       entropy = std::random_device{ };
		const auto base    = std::filesystem::temp_directory_path();
		for (auto attempt = 0; attempt < scratch_directory_attempts; ++attempt) {
			const auto value = (std::uint64_t{ entropy() } << 32U) | std::uint64_t{ entropy() };
			auto       text  = std::array<char, 16>{ };
			const auto end   = std::to_chars(text.data(), text.data() + text.size(), value, 16).ptr;
			auto       name  = aStem;
			name += std::string(text.data(), end);
			auto path  = base / name;
			auto error = std::error_code{ };
			if (create_directory_only_if_absent(path, error)) {
				return path;
			}
			expect(error == std::errc::file_exists, "a scratch directory fails to be made only because its name is taken: " + error.message());
		}
		fail("a scratch directory is made at a name nothing had");
	}

#if defined(_WIN32)
	/// \brief The directory as it resolves, or as it was made inside an
	/// AppContainer.
	/// \details Inside an AppContainer `canonical` refuses every path with
	/// "Access is denied", the container's own temporary directory included. It
	/// asks for the name's DOS form, and a container may not read which drive
	/// letter a volume has ([[BPR-0362]], `dos-volume-name`). The directory's
	/// list is not the cause: it grants the container full control. So there
	/// the directory is kept as it was made, at a name nothing had, directly in
	/// the temporary directory the system gave the process. Outside a container,
	/// and for any other refusal, the test fails.
	[[nodiscard]] inline auto resolved_scratch_directory(const std::filesystem::path& aMade)
		-> std::filesystem::path
	{
		auto error    = std::error_code{ };
		auto resolved = std::filesystem::canonical(aMade, error);
		if (!error) {
			return resolved;
		}
		expect(error == std::errc::permission_denied && !sqlite_contract::current_appcontainer_sid_storage().empty(),
			   "the scratch directory resolves, unless an AppContainer may not ask: " + error.message());
		return aMade;
	}
#endif

	/// \brief Makes a directory at `<aStem>-` and a random part, in the system
	/// temporary directory, at a name nothing had, owner-private from creation.
	/// \details It is removed again if it cannot be resolved.
	/// \param aStem Names the test, so that a directory a killed run leaves can
	/// be traced.
	/// \returns The directory, as it resolves: the temporary directory on macOS
	/// is reached through a link. The caller owns it from the return, and holds
	/// a `scratch_guard` or another owner for it at once.
	[[nodiscard]] inline auto make_scratch_directory(const std::filesystem::path& aStem)
		-> std::filesystem::path
	{
#if defined(_WIN32)
		// The stem stays a path: `string()` is the ANSI code page on Windows, which
		// cannot hold every name a test gives.
		auto stem = aStem;
		stem += "-";
		const auto made = make_directory_at_random_name(stem);
		// [[BPR-1121]]. Owned from the moment it exists.
		auto owned    = scratch_guard{ made };
		auto resolved = resolved_scratch_directory(made);
		owned.release();
		return resolved;
#else
		auto pattern = (std::filesystem::temp_directory_path() / aStem).native() + "-XXXXXX";
		if (::mkdtemp(pattern.data()) == nullptr) {
			const auto error = errno;
			fail("a scratch directory is made at a name nothing had: " + std::generic_category().message(error));
		}
		// [[BPR-1121]]. Owned from the moment it exists.
		auto made     = scratch_guard{ pattern };
		auto resolved = std::filesystem::canonical(pattern);
		made.release();
		return resolved;
#endif
	}
} // namespace bpr::tests

#endif // BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP
