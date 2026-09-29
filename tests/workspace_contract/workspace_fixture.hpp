#ifndef BPR_TESTS_WORKSPACE_CONTRACT_WORKSPACE_FIXTURE_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_WORKSPACE_FIXTURE_HPP

/// \file
/// \brief Disposable local roots for the workspace contracts: plain files,
/// links and, when the build found Git, real repositories made by that Git.
/// \details Git runs only here, to build fixtures, with hooks pointed at a
/// directory that does not exist, so no fixture step runs a repository script.
/// The product under test never runs Git.

#include "../support/test_expectation.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace bpr::tests::workspace_contract {
	inline void write_file(const std::filesystem::path& aPath, const std::string_view aBytes)
	{
		std::filesystem::create_directories(aPath.parent_path());
		std::ofstream output{ aPath, std::ios::binary | std::ios::trunc };
		output.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
		expect(static_cast<bool>(output), "write fixture file");
	}

	[[nodiscard]] inline auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}

	[[nodiscard]] inline auto text(const std::span<const std::uint8_t> aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	/// \brief Tries to make a symbolic link; Windows without the privilege
	/// cannot, and says so rather than failing the course.
	[[nodiscard]] inline auto try_symlink(const std::filesystem::path& aTarget, const std::filesystem::path& aLink, const bool aDirectory) -> bool
	{
		auto error = std::error_code{ };
		if (aDirectory) {
			std::filesystem::create_directory_symlink(aTarget, aLink, error);
		} else {
			std::filesystem::create_symlink(aTarget, aLink, error);
		}
		return !error;
	}

	/// \brief Runs the build's Git to make fixtures, isolated from the
	/// caller's hooks, signing and line-ending settings.
	class git_runner
	{
	public:
		explicit git_runner(std::string aExecutable)
			: Executable{ std::move(aExecutable) }
		{
		}

		void operator()(const std::filesystem::path& aDirectory, const std::initializer_list<std::string_view> aArguments) const
		{
			auto arguments = std::vector<std::string>{ "-C", aDirectory.string(), "-c", "core.hooksPath=" + (aDirectory / "no-hooks-here").string(), "-c",
													   "user.name=BPR fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "-c",
													   "tag.gpgsign=false", "-c", "init.defaultBranch=main", "-c", "core.autocrlf=false", "-c", "core.fsmonitor=false", "-c",
													   "core.safecrlf=false" };
			for (const auto argument : aArguments) {
				arguments.emplace_back(argument);
			}
			auto command = quote(Executable);
			for (const auto& argument : arguments) {
				command += ' ';
				command += quote(argument);
			}
#if defined(_WIN32)
			command = "\"" + command + " >NUL 2>&1\"";
#else
			command += " >/dev/null 2>&1";
#endif
			// Fixture setup only: every argument is quoted above, and the product
			// under test never reaches a command processor.
			expect(std::system(command.c_str()) == 0, "fixture git command"); // NOLINT(bugprone-command-processor)
		}

	private:
		static auto quote(const std::string& aValue) -> std::string
		{
#if defined(_WIN32)
			return "\"" + aValue + "\"";
#else
			auto quoted = std::string{ "'" };
			for (const auto character : aValue) {
				quoted += character == '\'' ? std::string{ "'\\''" } : std::string{ character };
			}
			return quoted + "'";
#endif
		}

		std::string Executable;
	};

	/// \brief `--git <path>` from the command line, when the build found Git.
	[[nodiscard]] inline auto git_from(const int aArgc, char** aArgv) -> std::optional<git_runner>
	{
		for (int index = 1; index + 1 < aArgc; ++index) {
			if (std::string_view{ aArgv[index] } == "--git") {
				return git_runner{ aArgv[index + 1] };
			}
		}
		return std::nullopt;
	}
}

#endif
