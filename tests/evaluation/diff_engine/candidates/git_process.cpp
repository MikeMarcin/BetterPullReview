// [[RFC-BPR-0082]] [[BPR-1466]]. The process shape's shared pieces: the
// pinned `git diff` options, the zero-context hunk parser, hunks to matches,
// and an unsupervised `git diff --no-index` run for the suites. The harness
// measures launches, stops, kills and reaps through the Git adapter's
// supervision instead (harness/supervised_git.cpp).

#include "candidates/candidates.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace bpr::evaluation {
	auto name(const git_algorithm aAlgorithm) noexcept -> std::string_view
	{
		switch (aAlgorithm) {
		case git_algorithm::myers:
			return "myers";
		case git_algorithm::minimal:
			return "minimal";
		case git_algorithm::patience:
			return "patience";
		case git_algorithm::histogram:
			return "histogram";
		}
		return "myers";
	}

	auto matches_from_hunks(const std::span<const line_hunk> aHunks, const std::uint32_t aLeftLines, const std::uint32_t aRightLines)
		-> std::optional<std::vector<core::diff_match>>
	{
		auto result = std::vector<core::diff_match>{ };
		auto left   = std::uint32_t{ 0U };
		auto right  = std::uint32_t{ 0U };
		for (const auto& hunk : aHunks) {
			if (hunk.OldBegin < left || hunk.NewBegin < right || hunk.OldBegin - left != hunk.NewBegin - right) {
				return std::nullopt;
			}
			while (left < hunk.OldBegin) {
				result.push_back({ .Left = left++, .Right = right++ });
			}
			left += hunk.OldCount;
			right += hunk.NewCount;
		}
		if (left > aLeftLines || right > aRightLines || aLeftLines - left != aRightLines - right) {
			return std::nullopt;
		}
		while (left < aLeftLines) {
			result.push_back({ .Left = left++, .Right = right++ });
		}
		return result;
	}

	namespace {
		[[nodiscard]] auto parse_number(std::string_view& aText, std::uint32_t& aValue) -> bool
		{
			const auto* end    = aText.data() + aText.size();
			const auto  parsed = std::from_chars(aText.data(), end, aValue);
			if (parsed.ec != std::errc{ } || parsed.ptr == aText.data()) {
				return false;
			}
			aText.remove_prefix(static_cast<std::size_t>(parsed.ptr - aText.data()));
			return true;
		}

		/// One side of a header: `-a,b` or `-a`, converted to zero-based.
		[[nodiscard]] auto parse_range(std::string_view& aText, const char aSign, std::uint32_t& aBegin, std::uint32_t& aCount) -> bool
		{
			if (aText.empty() || aText.front() != aSign) {
				return false;
			}
			aText.remove_prefix(1U);
			auto start = std::uint32_t{ 0U };
			if (!parse_number(aText, start)) {
				return false;
			}
			aCount = 1U;
			if (!aText.empty() && aText.front() == ',') {
				aText.remove_prefix(1U);
				if (!parse_number(aText, aCount)) {
					return false;
				}
			}
			// A range of zero lines names the line it follows; otherwise its
			// first line, one-based.
			if (aCount == 0U) {
				aBegin = start;
			} else {
				if (start == 0U) {
					return false;
				}
				aBegin = start - 1U;
			}
			return true;
		}
	} // namespace

	auto parse_zero_context_hunks(const std::string_view aOutput) -> std::optional<std::vector<line_hunk>>
	{
		auto result = std::vector<line_hunk>{ };
		auto rest   = aOutput;
		while (!rest.empty()) {
			const auto end  = rest.find('\n');
			auto       line = rest.substr(0U, end);
			rest            = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
			if (!line.starts_with("@@ ")) {
				continue;
			}
			line.remove_prefix(3U);
			auto hunk = line_hunk{ };
			if (!parse_range(line, '-', hunk.OldBegin, hunk.OldCount) || line.empty() || line.front() != ' ') {
				return std::nullopt;
			}
			line.remove_prefix(1U);
			if (!parse_range(line, '+', hunk.NewBegin, hunk.NewCount) || !line.starts_with(" @@")) {
				return std::nullopt;
			}
			result.push_back(hunk);
		}
		return result;
	}

	auto git_diff_arguments(const git_algorithm aAlgorithm, const bool aIndentHeuristic, const core::diff_options& aOptions)
		-> std::vector<std::string>
	{
		auto arguments = std::vector<std::string>{
			"-c",
			"core.quotePath=false",
			"diff",
			"--no-color",
			"--no-ext-diff",
			"--no-textconv",
			"--no-renames",
			"--text",
			"-U0",
			"--diff-algorithm=" + std::string{ name(aAlgorithm) },
			aIndentHeuristic ? "--indent-heuristic" : "--no-indent-heuristic",
		};
		switch (aOptions.Whitespace) {
		case core::whitespace_policy::exact:
			break;
		case core::whitespace_policy::ignore_trailing:
			arguments.emplace_back("--ignore-space-at-eol");
			break;
		case core::whitespace_policy::ignore_all:
			arguments.emplace_back("--ignore-all-space");
			break;
		}
		if (aOptions.LineEndings == core::line_ending_policy::normalize) {
			arguments.emplace_back("--ignore-cr-at-eol");
		}
		return arguments;
	}

	namespace {
		[[nodiscard]] auto write_file(const std::filesystem::path& aPath, const std::span<const std::uint8_t> aBytes) -> bool
		{
			auto* file = std::fopen(aPath.c_str(), "wb");
			if (file == nullptr) {
				return false;
			}
			const auto written = aBytes.empty() ? 0U : std::fwrite(aBytes.data(), 1U, aBytes.size(), file);
			return std::fclose(file) == 0 && written == aBytes.size();
		}
	} // namespace

	auto run_git_no_index(const std::string& aGit, const std::span<const std::uint8_t> aLeft, const std::span<const std::uint8_t> aRight,
						  const std::vector<std::string>& aArguments) -> std::optional<std::string>
	{
		auto pattern = (std::filesystem::temp_directory_path() / "bpr-diff-eval-XXXXXX").string();
		if (::mkdtemp(pattern.data()) == nullptr) {
			return std::nullopt;
		}
		const auto directory = std::filesystem::path{ pattern };
		const auto cleanup   = [&directory] {
			auto error = std::error_code{ };
			std::filesystem::remove_all(directory, error);
		};
		if (!write_file(directory / "left", aLeft) || !write_file(directory / "right", aRight)) {
			cleanup();
			return std::nullopt;
		}
		auto arguments = std::vector<std::string>{ aGit };
		arguments.insert(arguments.end(), aArguments.begin(), aArguments.end());
		arguments.emplace_back("--no-index");
		arguments.emplace_back("--");
		arguments.emplace_back("left");
		arguments.emplace_back("right");
		auto argv = std::vector<char*>{ };
		for (auto& argument : arguments) {
			argv.push_back(argument.data());
		}
		argv.push_back(nullptr);
		auto environment = std::vector<std::string>{
			"GIT_CONFIG_NOSYSTEM=1",
			"GIT_CONFIG_GLOBAL=/dev/null",
			"GIT_ATTR_NOSYSTEM=1",
			"HOME=" + directory.string(),
			"LC_ALL=C",
			"PATH=/usr/bin:/bin",
		};
		auto envp = std::vector<char*>{ };
		for (auto& entry : environment) {
			envp.push_back(entry.data());
		}
		envp.push_back(nullptr);

		auto output = std::array<int, 2U>{ };
		if (::pipe(output.data()) != 0) {
			cleanup();
			return std::nullopt;
		}
		auto actions = posix_spawn_file_actions_t{ };
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&actions, output[0]);
		posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
		posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
		posix_spawn_file_actions_addchdir_np(&actions, directory.c_str());
		auto       child   = pid_t{ 0 };
		const auto spawned = ::posix_spawn(&child, aGit.c_str(), &actions, nullptr, argv.data(), envp.data());
		posix_spawn_file_actions_destroy(&actions);
		::close(output[1]);
		if (spawned != 0) {
			::close(output[0]);
			cleanup();
			return std::nullopt;
		}
		auto text   = std::string{ };
		auto buffer = std::array<char, 64U * 1024U>{ };
		for (;;) {
			const auto count = ::read(output[0], buffer.data(), buffer.size());
			if (count <= 0) {
				break;
			}
			text.append(buffer.data(), static_cast<std::size_t>(count));
		}
		::close(output[0]);
		auto status = 0;
		while (::waitpid(child, &status, 0) < 0) {
		}
		cleanup();
		// `git diff --no-index` exits 1 when the files differ.
		if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1)) {
			return std::nullopt;
		}
		return text;
	}
} // namespace bpr::evaluation
