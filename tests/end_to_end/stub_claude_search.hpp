#ifndef BPR_TESTS_END_TO_END_STUB_CLAUDE_SEARCH_HPP
#define BPR_TESTS_END_TO_END_STUB_CLAUDE_SEARCH_HPP

/// \file
/// \brief A course's own `claude`: a stub agent put on a search `PATH` of the
/// course's own, which the agent setup finds as it finds a person's.
///
/// \details [[BPR-1894]]. Since [[BPR-1881]] the Tour tab's agent features
/// stay disabled until the Agent tab's setup has finished. A course that
/// presses them reaches that state as a person does: the setup looks for
/// agents when the Tour tab first shows, finds only this `claude`, chooses it
/// as the one agent found, and a review that holds every file needs no
/// clone ([[BPR-1879]]). Nothing in the product is told the course is a
/// course. The search names only the course's folder and no usual install
/// folders, so the developer's own `claude` is never found ([[BPR-1301]]).

#include "../../src/app/src/github_application_fixture_access.hpp"

#include <filesystem>
#include <optional>
#include <system_error>

namespace bpr::e2e {
	/// \brief Copies `aStub` into `aFolder` as `claude` (keeping the stub's
	/// extension, so `claude.exe` on Windows) and returns a search over only
	/// that folder.
	/// \returns Nothing when the folder or the copy is refused.
	[[nodiscard]] inline auto stub_claude_search(const std::filesystem::path& aFolder, const std::filesystem::path& aStub)
		-> std::optional<app::detail::claude_search_environment>
	{
		auto error = std::error_code{ };
		std::filesystem::create_directories(aFolder, error);
		if (error) {
			return std::nullopt;
		}
		const auto claude = aFolder / ("claude" + aStub.extension().string());
		std::filesystem::copy_file(aStub, claude, std::filesystem::copy_options::overwrite_existing, error);
		if (error) {
			return std::nullopt;
		}
		std::filesystem::permissions(claude, std::filesystem::perms::owner_all, error);
		if (error) {
			return std::nullopt;
		}
		return app::detail::claude_search_environment{ .PathVariable = aFolder.string() };
	}
} // namespace bpr::e2e

#endif
