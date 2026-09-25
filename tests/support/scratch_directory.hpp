#ifndef BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP
#define BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP

/// \file
/// \brief A scratch directory that a test makes for itself in the system
/// temporary directory, at a name nobody could have taken first. POSIX only.
///
/// \details [[BPR-1128]]. Tests named their roots from the clock, or from an
/// address and the process id, and made them with `create_directory`, which
/// does not fail when a directory, or a link to one, is already at the name.
/// So what was there was used: the course worked in it and removed it, with
/// everything in it, at the end. A link was followed, and some tests then
/// narrowed and resolved it, so they worked in the link's target and removed
/// that.
///
/// `mkdtemp` makes the directory only if nothing is at the name, so nothing
/// already there is removed, followed or reused. It is mode `0700` from
/// creation. The SQLite contracts' `tests/sqlite_contract/scratch_root.hpp`
/// ([[BPR-1126]]) does the same for roots the stores must admit.
///
/// There is no Windows branch. On Windows each test still makes its root as
/// it did before, because a root there has to carry the access its row needs,
/// and a row inside the AppContainer needs the container's SID. Unifying this
/// with the SQLite contracts' helper is left until the Windows station's
/// change to that helper has landed.

#include "test_expectation.hpp"

#include <filesystem>

#if !defined(_WIN32)
	#include <cerrno>
	#include <string>
	#include <system_error>
	#include <unistd.h>

namespace bpr::tests {
	/// \brief Makes a directory at `<aStem>-` and six random characters, in the
	/// system temporary directory, with `mkdtemp`.
	/// \details The directory is made only if nothing is at that name, and is
	/// mode `0700` from creation. It is removed again if it cannot be resolved.
	/// \param aStem Names the test, so that a directory a killed run leaves can
	/// be traced.
	/// \returns The directory, as it resolves: the temporary directory on macOS
	/// is reached through a link. The caller owns it from the return, and holds
	/// a `scratch_guard` or another owner for it at once.
	[[nodiscard]] inline auto make_scratch_directory(const std::filesystem::path& aStem)
		-> std::filesystem::path
	{
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
	}
} // namespace bpr::tests
#endif

#endif // BPR_TESTS_SUPPORT_SCRATCH_DIRECTORY_HPP
