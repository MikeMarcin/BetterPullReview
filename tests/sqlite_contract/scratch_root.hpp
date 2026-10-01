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

#include "../support/scratch_directory.hpp"
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
	/// \brief Makes a directory at `aPrefix` and a random part, in the system
	/// temporary directory, the way `mkdtemp` does where there is none.
	/// \details A name that something already has is left as it is, and another
	/// is drawn. The directory is made private by
	/// `bpr::tests::create_directory_only_if_absent` ([[BPR-1138]]), and the stores' own rule checks it
	/// before this returns. Used on Windows, and compiled everywhere.
	/// \returns The directory made. The caller owns it from the return.
	[[nodiscard]] inline auto make_directory_at_random_name(const std::string_view aPrefix)
		-> std::filesystem::path
	{
		const auto path = bpr::tests::make_directory_at_random_name(std::string{ aPrefix });
		// [[BPR-1121]]. Removed again if it cannot be made private.
		auto made = scratch_guard{ path };
		expect(adapters::sqlite::make_storage_root_owner_private(path), "the scratch root is owner-private before anything is written in it");
		made.release();
		return path;
	}

	/// \brief A directory in the system temporary directory that the course
	/// made, at a random name, owner-private from creation. It is removed, with
	/// everything in it, when its owner goes.
	/// \details A contract derives its own scratch type from this one, to put its
	/// label in the name. On POSIX the directory is made by `mkdtemp`, which
	/// creates it mode `0700`; on Windows by `make_directory_at_random_name`,
	/// and resolved by `bpr::tests::resolved_scratch_directory`.
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
			Path = bpr::tests::resolved_scratch_directory(made);
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
