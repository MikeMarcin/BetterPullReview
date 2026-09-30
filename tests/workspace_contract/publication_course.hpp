#ifndef BPR_TESTS_WORKSPACE_CONTRACT_PUBLICATION_COURSE_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_PUBLICATION_COURSE_HPP

/// \file
/// \brief The product's installed-Git publication catalog over the build's
/// Git, for the publication contracts (RFC-BPR-0059).

#include "../git_contract/windows_system_root.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "workspace_fixture.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::tests::workspace_contract {
	/// \brief The build's Git from `--git`, handed to the product as its installed Git.
	[[nodiscard]] inline auto git_executable(const int aArgc, char** aArgv) -> std::optional<std::filesystem::path>
	{
		for (int index = 1; index + 1 < aArgc; ++index) {
			if (std::string_view{ aArgv[index] } == "--git") {
				return std::filesystem::path{ aArgv[index + 1] };
			}
		}
		return std::nullopt;
	}

	/// \brief Every regular file under a root, the repository directory included.
	[[nodiscard]] inline auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files.emplace(entry.path().lexically_relative(aRoot).generic_string(), read_file(entry.path()));
			}
		}
		return files;
	}

	/// \brief The publication catalog in its own owner-private isolation root.
	struct publication_git
	{
		tests::sqlite_contract::scratch_root                     Root{ "cycle-publication-git" };
		std::unique_ptr<application::publication_candidate_port> Port;

		explicit publication_git(const std::filesystem::path& aExecutable)
		{
			auto made = adapters::git::make_installed_git_publication({
				.GitExecutable     = std::filesystem::canonical(aExecutable),
				.IsolationRoot     = std::filesystem::canonical(Root.path()),
				.WindowsSystemRoot = tests::git_contract::windows_system_root(),
			});
			expect(made.has_value(), "installed Git publication catalog");
			Port = std::move(*made);
		}

		/// \brief The private store the catalog writes for one object format.
		[[nodiscard]] auto store(const bool aSha256) const -> std::filesystem::path
		{
			return std::filesystem::canonical(Root.path()) / "publication" / "v1" / (aSha256 ? "sha256.git" : "sha1.git");
		}
	};
}

#endif
