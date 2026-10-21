#ifndef BPR_TESTS_WORKSPACE_CONTRACT_GIT_METADATA_ORACLE_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_GIT_METADATA_ORACLE_HPP

/// \file
/// \brief RFC-BPR-0083's Git oracle: the installed Git answers the C1-C6
/// questions in the same standard types the candidates use.
/// \details The harness isolates every Git it starts from the caller's
/// system and global configuration (`isolate_git_environment`), so the
/// oracle reads only what the checkout holds, as capture does. Tests run
/// Git; the product never does.

#include "git_metadata_candidate.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <sys/stat.h>
#endif

namespace bpr::tests::workspace_contract {
	/// \brief Points Git at no system file, an empty global file and empty
	/// home and XDG directories, and turns off optional locks, for this
	/// process and every Git it starts. `aScratch` must already exist.
	inline void isolate_git_environment(const std::filesystem::path& aScratch)
	{
#if !defined(_WIN32)
		const auto home   = aScratch / "oracle-home";
		const auto xdg    = aScratch / "oracle-xdg";
		const auto global = aScratch / "oracle-global-config";
		std::filesystem::create_directories(home);
		std::filesystem::create_directories(xdg);
		write_file(global, "");
		::setenv("GIT_CONFIG_NOSYSTEM", "1", 1);
		::setenv("GIT_CONFIG_GLOBAL", global.c_str(), 1);
		::setenv("HOME", home.c_str(), 1);
		::setenv("XDG_CONFIG_HOME", xdg.c_str(), 1);
		::setenv("GIT_OPTIONAL_LOCKS", "0", 1);
		// Git folds case through the C library, so the locale changes C5's
		// answers under core.ignoreCase. The oracle is Git as the owner runs it,
		// under a UTF-8 locale; where none is installed Git falls back to C.
		::setenv("LC_ALL", "en_US.UTF-8", 1);
		// Nothing a caller set may redirect the repository Git reads.
		for (const auto* name : { "GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE", "GIT_CONFIG", "GIT_CONFIG_PARAMETERS", "GIT_CONFIG_COUNT", "GIT_CEILING_DIRECTORIES" }) {
			::unsetenv(name);
		}
#else
		static_cast<void>(aScratch);
#endif
	}

	/// \brief The installed Git's version as {major, minor}.
	[[nodiscard]] inline auto installed_git_version(const git_runner& aGit, const std::filesystem::path& aScratch) -> std::pair<int, int>
	{
		const auto text  = aGit.output(aScratch, { "version" }, aScratch / "git-version.out");
		auto       major = 0;
		auto       minor = 0;
		auto       at    = text.find_first_of("0123456789");
		for (; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at) {
			major = (major * 10) + (text[at] - '0');
		}
		if (at < text.size() && text[at] == '.') {
			for (++at; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at) {
				minor = (minor * 10) + (text[at] - '0');
			}
		}
		return { major, minor };
	}

	/// \brief A directory's identity, for comparing discovered Git directories.
	[[nodiscard]] inline auto identity_of(const std::filesystem::path& aPath) -> std::optional<file_identity>
	{
#if !defined(_WIN32)
		struct stat status{ };
		if (::stat(aPath.c_str(), &status) != 0) {
			return std::nullopt;
		}
		return file_identity{ .Device = static_cast<std::uint64_t>(status.st_dev), .Object = static_cast<std::uint64_t>(status.st_ino) };
#else
		const auto directory = ::CreateFileW(aPath.c_str(), FILE_READ_ATTRIBUTES,
											 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
											 FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (directory == INVALID_HANDLE_VALUE) {
			return std::nullopt;
		}
		auto       identity = FILE_ID_INFO{ };
		const auto observed = ::GetFileInformationByHandleEx(directory, FileIdInfo, &identity, sizeof(identity));
		static_cast<void>(::CloseHandle(directory));
		if (observed == FALSE) {
			return std::nullopt;
		}
		auto object = std::uint64_t{ 0 };
		for (std::size_t index = 0; index < sizeof(object); ++index) {
			object |= static_cast<std::uint64_t>(identity.FileId.Identifier[index]) << (8U * index);
		}
		return file_identity{ .Device = identity.VolumeSerialNumber, .Object = object };
#endif
	}

	/// \brief The installed Git, asked RFC-BPR-0083's oracle commands.
	class git_oracle
	{
	public:
		git_oracle(const git_runner& aGit, std::filesystem::path aScratch, const std::pair<int, int> aVersion)
			: Git{ aGit }
			, Scratch{ std::move(aScratch) }
			, Version{ aVersion }
		{
		}

		/// \brief Runs Git and returns its exit status and standard output.
		auto ask(const std::filesystem::path& aRoot, const std::vector<std::string>& aArguments, const std::optional<std::string>& aInput = std::nullopt) const
			-> std::pair<int, std::string>
		{
			const auto output = Scratch / "oracle.out";
			auto       input  = std::optional<std::filesystem::path>{ };
			if (aInput) {
				input = Scratch / "oracle.in";
				write_file(*input, *aInput);
			}
			const auto status = Git.status(aRoot, aArguments, input, output);
			return { status, read_file(output) };
		}

		/// \brief Git's refusal of the repository, when it refuses to read it.
		auto refusal(const std::filesystem::path& aRoot) const -> std::optional<std::string>
		{
			const auto [status, text] = ask(aRoot, { "rev-parse", "--git-dir" });
			if (status != 0) {
				return "git refuses the repository (exit " + std::to_string(status) + ")";
			}
			return std::nullopt;
		}

		auto index(const std::filesystem::path& aRoot) const -> std::expected<index_answer, candidate_refusal>
		{
			const auto [status, text] = ask(aRoot, { "ls-files", "--stage", "-t", "--sparse", "--debug", "-z" });
			if (status != 0) {
				return std::unexpected("ls-files exit " + std::to_string(status));
			}
			auto answer = index_answer{ };
			auto at     = std::size_t{ 0 };
			while (at < text.size()) {
				// `<tag> <mode> <object> <stage>\t<path>\0`, then five debug lines.
				const auto end = text.find('\0', at);
				if (end == std::string::npos) {
					return std::unexpected(std::string{ "ls-files output without NUL" });
				}
				const auto record = std::string_view{ text }.substr(at, end - at);
				at                = end + 1;
				const auto tab    = record.find('\t');
				const auto fields = record.substr(0, tab);
				auto       entry  = index_answer_entry{ .Path = std::string{ record.substr(tab + 1) } };
				const auto first  = fields.find(' ');
				const auto second = fields.find(' ', first + 1);
				const auto third  = fields.find(' ', second + 1);
				const auto tag    = fields.substr(0, first);
				entry.Mode        = static_cast<std::uint32_t>(std::strtoul(std::string{ fields.substr(first + 1, second - first - 1) }.c_str(), nullptr, 8));
				entry.Object      = std::string{ fields.substr(second + 1, third - second - 1) };
				entry.Stage       = static_cast<std::uint8_t>(std::strtoul(std::string{ fields.substr(third + 1) }.c_str(), nullptr, 10));
				auto flags        = std::uint32_t{ 0 };
				for (int line = 0; line < 5 && at < text.size(); ++line) {
					const auto newline = text.find('\n', at);
					const auto content = std::string_view{ text }.substr(at, newline - at);
					at                 = newline == std::string::npos ? text.size() : newline + 1;
					if (const auto marker = content.find("flags: "); marker != std::string_view::npos) {
						flags = static_cast<std::uint32_t>(std::strtoul(std::string{ content.substr(marker + 7) }.c_str(), nullptr, 16));
					}
				}
				// Git's in-memory CE_SKIP_WORKTREE and CE_INTENT_TO_ADD.
				entry.SkipWorktree = (flags & 0x4000'0000U) != 0 || tag == "S";
				entry.IntentToAdd  = (flags & 0x2000'0000U) != 0;
				answer.Entries.push_back(std::move(entry));
			}
			return answer;
		}

		auto format(const std::filesystem::path& aRoot) const -> std::expected<format_answer, candidate_refusal>
		{
			const auto [status, text] = ask(aRoot, { "rev-parse", "--show-object-format", "--show-ref-format" });
			if (status != 0) {
				return std::unexpected("rev-parse exit " + std::to_string(status));
			}
			auto       answer   = format_answer{ };
			const auto newline  = text.find('\n');
			answer.ObjectFormat = text.substr(0, newline);
			auto reference      = newline == std::string::npos ? std::string{ } : text.substr(newline + 1);
			while (!reference.empty() && (reference.back() == '\n' || reference.back() == '\r')) {
				reference.pop_back();
			}
			answer.ReferenceFormat            = reference;
			const auto [caseStatus, caseText] = ask(aRoot, { "config", "--type=bool", "core.ignorecase" });
			if (caseStatus != 0 && caseStatus != 1) {
				return std::unexpected("config exit " + std::to_string(caseStatus));
			}
			answer.IgnoreCase = caseStatus == 0 && caseText.starts_with("true");
			return answer;
		}

		auto head(const std::filesystem::path& aRoot) const -> std::expected<head_answer, candidate_refusal>
		{
			auto answer                         = head_answer{ };
			const auto [symbolic, symbolicText] = ask(aRoot, { "symbolic-ref", "--no-recurse", "-q", "HEAD" });
			if (symbolic == 0) {
				answer.Symbolic = trimmed(symbolicText);
			} else if (symbolic != 1) {
				return std::unexpected("symbolic-ref exit " + std::to_string(symbolic));
			}
			auto commit = reference(aRoot, "HEAD");
			if (!commit) {
				return std::unexpected(commit.error());
			}
			answer.Commit = *commit;
			return answer;
		}

		auto reference(const std::filesystem::path& aRoot, const std::string& aName) const -> std::expected<std::optional<std::string>, candidate_refusal>
		{
			const auto [status, text] = ask(aRoot, { "rev-parse", "--verify", "-q", aName });
			if (status == 0) {
				return std::optional{ trimmed(text) };
			}
			if (status == 1) {
				return std::optional<std::string>{ };
			}
			return std::unexpected("rev-parse --verify exit " + std::to_string(status));
		}

		auto discover(const std::filesystem::path& aRoot) const -> std::expected<discovery_answer, candidate_refusal>
		{
			const auto output = Scratch / "oracle.out";
			const auto error  = Scratch / "oracle.err";
			const auto status = Git.status(aRoot, { "rev-parse", "--path-format=absolute", "--git-dir", "--git-common-dir" }, std::nullopt, output, error);
			if (status != 0) {
				const auto reason = trimmed(read_file(error));
				if (status == 128 && reason == "fatal: compatibility hash algorithm support requires Rust") {
					return std::unexpected(std::string{ "rev-parse exit 128: compatibility hash algorithm support requires Rust" });
				}
				return std::unexpected("rev-parse exit " + std::to_string(status));
			}
			const auto text                = read_file(output);
			const auto newline             = text.find('\n');
			auto       answer              = discovery_answer{ .GitDirectory = text.substr(0, newline), .CommonDirectory = trimmed(text.substr(newline + 1)) };
			answer.GitDirectoryIdentity    = identity_of(answer.GitDirectory);
			answer.CommonDirectoryIdentity = identity_of(answer.CommonDirectory);
			answer.LinkedWorktree          = answer.GitDirectoryIdentity != answer.CommonDirectoryIdentity;
			return answer;
		}

		/// \brief `git check-ignore --no-index -v -n`: a path is ignored when
		/// its last matching pattern, a parent directory's included, is not a
		/// negation.
		auto ignored(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) const -> std::expected<std::vector<bool>, candidate_refusal>
		{
			if (aPaths.empty()) {
				return std::vector<bool>{ };
			}
			const auto [status, text] = ask(aRoot, { "check-ignore", "--no-index", "-v", "-n", "--stdin", "-z" }, joined(aPaths));
			if (status != 0 && status != 1) {
				return std::unexpected("check-ignore exit " + std::to_string(status));
			}
			const auto fields = split_nul(text);
			if (fields.size() != aPaths.size() * 4U) {
				return std::unexpected("check-ignore answered " + std::to_string(fields.size()) + " fields for " + std::to_string(aPaths.size()) + " paths");
			}
			auto answers = std::vector<bool>{ };
			for (std::size_t index = 0; index < aPaths.size(); ++index) {
				const auto& source  = fields[index * 4U];
				const auto& pattern = fields[(index * 4U) + 2U];
				answers.push_back(!source.empty() && !pattern.starts_with('!'));
			}
			return answers;
		}

		/// \brief `git check-attr filter`.
		auto filter(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) const -> std::expected<std::vector<attribute_answer>, candidate_refusal>
		{
			return attribute(aRoot, aPaths, "filter");
		}

		/// \brief [[BPR-1692]]. `git check-attr linguist-generated`.
		auto linguist_generated(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) const -> std::expected<std::vector<attribute_answer>, candidate_refusal>
		{
			return attribute(aRoot, aPaths, "linguist-generated");
		}

		/// \brief `git check-attr <name>`.
		auto attribute(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths, const std::string& aName) const
			-> std::expected<std::vector<attribute_answer>, candidate_refusal>
		{
			if (aPaths.empty()) {
				return std::vector<attribute_answer>{ };
			}
			const auto [status, text] = ask(aRoot, { "check-attr", aName, "--stdin", "-z" }, joined(aPaths));
			if (status != 0) {
				return std::unexpected("check-attr exit " + std::to_string(status));
			}
			const auto fields = split_nul(text);
			if (fields.size() != aPaths.size() * 3U) {
				return std::unexpected("check-attr answered " + std::to_string(fields.size()) + " fields for " + std::to_string(aPaths.size()) + " paths");
			}
			auto answers = std::vector<attribute_answer>{ };
			for (std::size_t index = 0; index < aPaths.size(); ++index) {
				answers.push_back(fields[(index * 3U) + 2U]);
			}
			return answers;
		}

		[[nodiscard]] auto version() const noexcept -> std::pair<int, int>
		{
			return Version;
		}

	private:
		static auto trimmed(std::string aText) -> std::string
		{
			while (!aText.empty() && (aText.back() == '\n' || aText.back() == '\r')) {
				aText.pop_back();
			}
			return aText;
		}

		static auto joined(const std::vector<path_question>& aPaths) -> std::string
		{
			auto input = std::string{ };
			for (const auto& question : aPaths) {
				input += question.Path;
				input += '\0';
			}
			return input;
		}

		static auto split_nul(const std::string& aText) -> std::vector<std::string>
		{
			auto fields = std::vector<std::string>{ };
			auto start  = std::size_t{ 0 };
			while (start < aText.size()) {
				const auto end = aText.find('\0', start);
				if (end == std::string::npos) {
					fields.push_back(aText.substr(start));
					break;
				}
				fields.push_back(aText.substr(start, end - start));
				start = end + 1;
			}
			return fields;
		}

		const git_runner&     Git;
		std::filesystem::path Scratch;
		std::pair<int, int>   Version;
	};
}

#endif
