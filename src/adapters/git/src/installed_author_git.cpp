#include "bpr/adapters/git/installed_author_git.hpp"

#include "bpr/adapters/process_support/process_supervision.hpp"
#include "bpr/core/hex.hpp"

#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <ranges>
#include <string_view>
#include <utility>

#if defined(Q_OS_UNIX)
	#include "bpr/platform/unix_descriptor_sweep.hpp"

	#include <cerrno>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::git {
	namespace {
		using application::author_git_error;
		using application::author_git_failure;
		using application::author_git_result;

		struct run_output
		{
			int        ExitCode = -1;
			QByteArray Output;
			QByteArray Error;
		};

		[[nodiscard]] auto qt(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto message_of(const run_output& aRun) -> std::string
		{
			auto text = aRun.Error.toStdString();
			if (!aRun.Output.isEmpty()) {
				text += (text.empty() ? "" : "\n") + aRun.Output.toStdString();
			}
			if (text.size() > application::maximum_author_git_message_bytes) {
				text.resize(application::maximum_author_git_message_bytes);
			}
			return text;
		}

		[[nodiscard]] auto failure(const author_git_failure aFailure, const run_output& aRun = { }) -> author_git_error
		{
			return { .Failure = aFailure, .ExitCode = aRun.ExitCode, .Message = message_of(aRun) };
		}

		/// The application's environment without what would point Git at
		/// another repository, and with Git's terminal prompt off.
		[[nodiscard]] auto author_environment() -> QProcessEnvironment
		{
			auto environment = QProcessEnvironment::systemEnvironment();
			for (const auto* const name : { "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_NAMESPACE", "GIT_COMMON_DIR",
											"GIT_CEILING_DIRECTORIES", "GIT_DISCOVERY_ACROSS_FILESYSTEM", "GIT_EXTERNAL_DIFF", "GIT_PAGER", "PAGER" }) {
				environment.remove(QString::fromLatin1(name));
			}
			environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
			environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
			return environment;
		}

		class installed_author_git final : public application::author_git_port
		{
		public:
			installed_author_git(std::filesystem::path aGit, const author_git_deadlines aDeadlines)
				: Git{ std::move(aGit) }
				, Deadlines{ aDeadlines }
			{
			}

			auto status(const std::string& aWorktree, std::stop_token aStop) -> author_git_result<core::local_worktree_status> override
			{
				auto run = launch(aWorktree, read_flags({ "status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all" }), { }, 32U * 1024U * 1024U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				auto parsed = parse_author_status(std::string_view{ run->Output.constData(), static_cast<std::size_t>(run->Output.size()) });
				if (!parsed) {
					return std::unexpected{ failure(author_git_failure::malformed, *run) };
				}
				return core::normalized(std::move(*parsed));
			}

			auto read_blob(const std::string& aWorktree, const application::author_git_blob& aBlob, const std::uint64_t aMaximum, std::stop_token aStop)
				-> author_git_result<std::optional<std::vector<std::uint8_t>>> override
			{
				if (aBlob.Path.empty() || aBlob.Path.front() == '-' || aBlob.Path.find('\n') != std::string::npos) {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				const auto object = (aBlob.Commit ? std::string{ core::canonical_hex(*aBlob.Commit).view() } : std::string{ }) + ":" + aBlob.Path;
				auto       exists = launch(aWorktree, read_flags({ "cat-file", "-e", object }), { }, 4096U, Deadlines.Read, aStop);
				if (!exists) {
					return std::unexpected{ exists.error() };
				}
				if (exists->ExitCode != 0) {
					return std::optional<std::vector<std::uint8_t>>{ };
				}
				auto run = launch(aWorktree, read_flags({ "cat-file", "blob", object }), { }, aMaximum, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				return std::optional{ std::vector<std::uint8_t>(run->Output.begin(), run->Output.end()) };
			}

			auto changed_since(const std::string& aWorktree, const core::git_commit_oid& aCommit, std::stop_token aStop)
				-> author_git_result<std::vector<application::author_git_changed_path>> override
			{
				auto run = launch(aWorktree, read_flags({ "diff", "--name-status", "-z", "--no-renames", "--no-ext-diff", std::string{ core::canonical_hex(aCommit).view() }, "--" }), { },
								  16U * 1024U * 1024U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				auto       paths  = std::vector<application::author_git_changed_path>{ };
				const auto fields = split_nul(run->Output);
				for (std::size_t index = 0U; index + 1U < fields.size(); index += 2U) {
					const auto letter = fields[index].empty() ? 'M' : fields[index].front();
					paths.push_back({ .Path = std::string{ fields[index + 1U] }, .Original = { }, .Status = letter == 'A' ? core::local_file_status::added : letter == 'D' ? core::local_file_status::deleted
										  : letter == 'T'                                                                                                                  ? core::local_file_status::type_changed
																																										   : core::local_file_status::modified });
				}
				return paths;
			}

			auto has_commit(const std::string& aWorktree, const core::git_commit_oid& aCommit, std::stop_token aStop) -> author_git_result<bool> override
			{
				auto run = launch(aWorktree, read_flags({ "cat-file", "-e", std::string{ core::canonical_hex(aCommit).view() } + "^{commit}" }), { }, 4096U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				return run->ExitCode == 0;
			}

			auto count_commits(const std::string& aWorktree, const core::git_commit_oid& aFrom, const core::git_commit_oid& aExclude, std::stop_token aStop)
				-> author_git_result<std::uint32_t> override
			{
				auto run = launch(aWorktree, read_flags({ "rev-list", "--count", std::string{ core::canonical_hex(aFrom).view() }, "^" + std::string{ core::canonical_hex(aExclude).view() }, "--" }), { }, 4096U,
								  Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				auto ok    = false;
				auto value = run->Output.trimmed().toUInt(&ok);
				if (!ok) {
					return std::unexpected{ failure(author_git_failure::malformed, *run) };
				}
				return static_cast<std::uint32_t>(value);
			}

			auto recent_subjects(const std::string& aWorktree, const std::size_t aCount, std::stop_token aStop) -> author_git_result<std::vector<std::string>> override
			{
				auto run = launch(aWorktree, read_flags({ "log", "-n", std::to_string(aCount), "--format=%s", "--no-color", "HEAD", "--" }), { }, 256U * 1024U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				auto subjects = std::vector<std::string>{ };
				if (run->ExitCode != 0) {
					// An unborn branch has no subjects.
					return subjects;
				}
				for (const auto& line : run->Output.split('\n')) {
					if (!line.trimmed().isEmpty()) {
						subjects.push_back(line.toStdString());
					}
				}
				return subjects;
			}

			auto staged_diff(const std::string& aWorktree, const std::uint64_t aMaximum, std::stop_token aStop) -> author_git_result<std::string> override
			{
				// The bound is the caller's; a longer diff is cut, not refused.
				auto run = launch(aWorktree, read_flags({ "diff", "--cached", "--no-color", "--no-ext-diff", "--no-textconv" }), { }, aMaximum, Deadlines.Read, aStop, true);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0 && run->ExitCode != -2) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				return run->Output.toStdString();
			}

			auto filtered(const std::string& aWorktree, const std::string& aPath, std::stop_token aStop) -> author_git_result<bool> override
			{
				auto run = launch(aWorktree, read_flags({ "check-attr", "-z", "filter", "--", aPath }), { }, 64U * 1024U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				const auto fields = split_nul(run->Output);
				return fields.size() >= 3U && fields[2] != "unspecified" && fields[2] != "unset";
			}

			auto hosted_tip(const std::string& aWorktree, const std::string& aUrl, const std::string& aBranch, std::stop_token aStop)
				-> author_git_result<std::optional<core::git_commit_oid>> override
			{
				if (aUrl.empty() || aUrl.front() == '-' || aBranch.empty() || aBranch.front() == '-') {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				const auto ref = "refs/heads/" + aBranch;
				auto       run = launch(aWorktree, { "ls-remote", "--heads", "--", aUrl, ref }, { }, 64U * 1024U, Deadlines.Network, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *run) };
				}
				for (const auto& line : run->Output.split('\n')) {
					const auto tab = line.indexOf('\t');
					if (tab > 0 && line.mid(tab + 1).trimmed().toStdString() == ref) {
						const auto oid = core::parse_git_commit_oid(line.left(tab).toStdString());
						if (!oid) {
							return std::unexpected{ failure(author_git_failure::malformed, *run) };
						}
						return std::optional{ *oid };
					}
				}
				return std::optional<core::git_commit_oid>{ };
			}

			auto remote_url(const std::string& aClone, const std::string& aRemote, std::stop_token aStop) -> author_git_result<std::optional<std::string>> override
			{
				if (aRemote.empty() || aRemote.front() == '-' || aRemote.find_first_of(" \n\t=") != std::string::npos) {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				auto run = launch(aClone, read_flags({ "config", "--get", "remote." + aRemote + ".url" }), { }, 64U * 1024U, Deadlines.Read, aStop);
				if (!run) {
					return std::unexpected{ run.error() };
				}
				if (run->ExitCode != 0) {
					return std::optional<std::string>{ };
				}
				const auto url = run->Output.trimmed().toStdString();
				return url.empty() ? std::optional<std::string>{ } : std::optional{ url };
			}

			auto stage_paths(const std::string& aWorktree, const std::vector<std::string>& aPaths, std::stop_token aStop) -> author_git_result<void> override
			{
				auto arguments = std::vector<std::string>{ "add", "--all", "--" };
				arguments.insert(arguments.end(), aPaths.begin(), aPaths.end());
				return expect_success(launch(aWorktree, arguments, { }, 1024U * 1024U, Deadlines.Read, aStop));
			}

			auto unstage_paths(const std::string& aWorktree, const std::vector<std::string>& aPaths, std::stop_token aStop) -> author_git_result<void> override
			{
				auto head = launch(aWorktree, read_flags({ "rev-parse", "--verify", "-q", "HEAD^{commit}" }), { }, 4096U, Deadlines.Read, aStop);
				if (!head) {
					return std::unexpected{ head.error() };
				}
				auto arguments = head->ExitCode == 0 ? std::vector<std::string>{ "restore", "--staged", "--" } : std::vector<std::string>{ "rm", "--cached", "-r", "-q", "--ignore-unmatch", "--" };
				arguments.insert(arguments.end(), aPaths.begin(), aPaths.end());
				return expect_success(launch(aWorktree, arguments, { }, 1024U * 1024U, Deadlines.Read, aStop));
			}

			auto write_index_entry(const std::string& aWorktree, const std::string& aPath, const std::vector<std::uint8_t>& aBytes, std::stop_token aStop) -> author_git_result<void> override
			{
				if (aPath.empty() || aPath.front() == '-' || aPath.find(',') != std::string::npos || aPath.find('\n') != std::string::npos) {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				auto mode  = std::string{ "100644" };
				auto entry = launch(aWorktree, read_flags({ "ls-files", "-s", "-z", "--", aPath }), { }, 64U * 1024U, Deadlines.Read, aStop);
				if (!entry) {
					return std::unexpected{ entry.error() };
				}
				if (entry->ExitCode == 0 && entry->Output.size() > 6) {
					mode = entry->Output.left(6).toStdString();
				} else {
					auto tree = launch(aWorktree, read_flags({ "ls-tree", "-z", "HEAD", "--", aPath }), { }, 64U * 1024U, Deadlines.Read, aStop);
					if (tree && tree->ExitCode == 0 && tree->Output.size() > 6) {
						mode = tree->Output.left(6).toStdString();
					}
				}
				if (mode != "100644" && mode != "100755") {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				auto hashed = launch(aWorktree, { "hash-object", "-w", "--no-filters", "--stdin" }, QByteArray{ reinterpret_cast<const char*>(aBytes.data()), static_cast<qsizetype>(aBytes.size()) }, 4096U,
									 Deadlines.Read, aStop);
				if (!hashed) {
					return std::unexpected{ hashed.error() };
				}
				if (hashed->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *hashed) };
				}
				const auto oid = hashed->Output.trimmed().toStdString();
				if (oid.empty() || !std::ranges::all_of(oid, [](const char aChar) { return (aChar >= '0' && aChar <= '9') || (aChar >= 'a' && aChar <= 'f'); })) {
					return std::unexpected{ failure(author_git_failure::malformed, *hashed) };
				}
				return expect_success(launch(aWorktree, { "update-index", "--add", "--cacheinfo", mode + "," + oid + "," + aPath }, { }, 64U * 1024U, Deadlines.Read, aStop));
			}

			auto commit(const std::string& aWorktree, const core::local_commit_plan& aPlan, std::stop_token aStop) -> author_git_result<void> override
			{
				auto arguments = std::vector<std::string>{ "commit", "--file=-" };
				if (aPlan.Amend) {
					arguments.emplace_back("--amend");
				}
				return expect_success(launch(aWorktree, arguments, QByteArray::fromStdString(aPlan.Message), 1024U * 1024U, Deadlines.Hooks, aStop, true));
			}

			auto push(const core::local_push_command& aCommand, std::stop_token aStop) -> author_git_result<void> override
			{
				// The command is the one the push line shows; it is checked again here.
				const auto& arguments = aCommand.Arguments;
				const auto  forced    = std::ranges::any_of(arguments, [](const std::string& aArgument) {
					return aArgument.starts_with('+') || aArgument.find("force") != std::string::npos || aArgument == "--tags" || aArgument == "--delete" || aArgument == "-d"
						|| aArgument == "-f" || aArgument == "--mirror" || aArgument == "--all" || aArgument == "--prune";
				});
				if (arguments.empty() || arguments.front() != "push" || forced) {
					return std::unexpected{ failure(author_git_failure::malformed) };
				}
				return expect_success(launch(aCommand.Directory, arguments, { }, 1024U * 1024U, Deadlines.Hooks, aStop, true));
			}

		private:
			/// Reads take no optional lock and run no fsmonitor.
			[[nodiscard]] static auto read_flags(std::vector<std::string> aArguments) -> std::vector<std::string>
			{
				aArguments.insert(aArguments.begin(), { "--no-optional-locks", "-c", "core.fsmonitor=false", "-c", "core.quotepath=false" });
				return aArguments;
			}

			[[nodiscard]] static auto split_nul(const QByteArray& aOutput) -> std::vector<std::string>
			{
				auto fields = std::vector<std::string>{ };
				for (const auto& field : aOutput.split('\0')) {
					fields.push_back(field.toStdString());
				}
				if (!fields.empty() && fields.back().empty()) {
					fields.pop_back();
				}
				return fields;
			}

			[[nodiscard]] static auto expect_success(author_git_result<run_output> aRun) -> author_git_result<void>
			{
				if (!aRun) {
					return std::unexpected{ aRun.error() };
				}
				if (aRun->ExitCode != 0) {
					return std::unexpected{ failure(author_git_failure::failed, *aRun) };
				}
				return { };
			}

			/// One bounded, stoppable launch of the author's Git in `aDirectory`.
			/// With `aCut`, output past `aMaximum` is cut instead of refused.
			[[nodiscard]] auto launch(const std::string& aDirectory, const std::vector<std::string>& aArguments, const QByteArray& aInput, const std::uint64_t aMaximum,
									  const std::chrono::milliseconds aDeadline, std::stop_token aStop, const bool aCut = false) const -> author_git_result<run_output>
			{
#if defined(Q_OS_UNIX)
				if (Git.empty() || aDirectory.empty() || aStop.stop_requested()) {
					return std::unexpected{ failure(aStop.stop_requested() ? author_git_failure::cancelled : author_git_failure::unavailable) };
				}
				auto arguments = QStringList{ QStringLiteral("-C"), qt(aDirectory) };
				for (const auto& argument : aArguments) {
					arguments.push_back(qt(argument));
				}
				QProcess process;
				process.setProgram(QString::fromStdString(Git.string()));
				process.setArguments(arguments);
				process.setProcessEnvironment(author_environment());
				process.setProcessChannelMode(QProcess::SeparateChannels);
				if (aInput.isNull()) {
					process.setStandardInputFile(QProcess::nullDevice());
				}
				process.setChildProcessModifier([] { bpr::platform::unix_descriptor_sweep::bound(); });
				process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps | QProcess::UnixProcessFlag::ResetSignalHandlers
												 | QProcess::UnixProcessFlag::CreateNewSession);
				process.start();
				if (!process.waitForStarted(static_cast<int>(std::min<std::int64_t>(aDeadline.count(), 30'000)))) {
					return std::unexpected{ failure(author_git_failure::start) };
				}
				if (!aInput.isNull()) {
					process.write(aInput);
					process.closeWriteChannel();
				}
				const auto deadline = process_support::supervision_clock::now() + aDeadline;
				auto       out      = run_output{ };
				auto       stopped  = std::optional<author_git_failure>{ };
				auto       cut      = false;
				while (process.state() != QProcess::NotRunning) {
					process.waitForReadyRead(50);
					out.Output += process.readAllStandardOutput();
					out.Error += process.readAllStandardError();
					if (out.Error.size() > static_cast<qsizetype>(application::maximum_author_git_message_bytes)) {
						out.Error.truncate(static_cast<qsizetype>(application::maximum_author_git_message_bytes));
					}
					if (static_cast<std::uint64_t>(out.Output.size()) > aMaximum) {
						if (!aCut) {
							stopped = author_git_failure::bounds;
							break;
						}
						out.Output.truncate(static_cast<qsizetype>(aMaximum));
						cut = true;
					}
					if (aStop.stop_requested()) {
						stopped = author_git_failure::cancelled;
						break;
					}
					if (process_support::supervision_clock::now() >= deadline) {
						stopped = author_git_failure::timeout;
						break;
					}
				}
				if (stopped) {
					const auto group = process_support::resolve_process_group(process, 0);
					if (!process_support::kill_process_group_and_reap(process, group, std::chrono::milliseconds{ 5'000 })) {
						process.kill();
						process.waitForFinished(1'000);
					}
					out.ExitCode = -1;
					return std::unexpected{ failure(*stopped, out) };
				}
				out.Output += process.readAllStandardOutput();
				out.Error += process.readAllStandardError();
				if (static_cast<std::uint64_t>(out.Output.size()) > aMaximum) {
					if (!aCut) {
						return std::unexpected{ failure(author_git_failure::bounds) };
					}
					out.Output.truncate(static_cast<qsizetype>(aMaximum));
				}
				out.ExitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
				if (cut && out.ExitCode != 0) {
					// Git may stop on a closed pipe after a cut; the cut output stands.
					out.ExitCode = -2;
				}
				return out;
#else
				static_cast<void>(aDirectory);
				static_cast<void>(aArguments);
				static_cast<void>(aInput);
				static_cast<void>(aMaximum);
				static_cast<void>(aDeadline);
				static_cast<void>(aStop);
				static_cast<void>(aCut);
				return std::unexpected{ failure(author_git_failure::unavailable) };
#endif
			}

			std::filesystem::path Git;
			author_git_deadlines  Deadlines;
		};

		[[nodiscard]] auto status_letter(const char aLetter) -> std::optional<core::local_file_status>
		{
			switch (aLetter) {
			case 'M': return core::local_file_status::modified;
			case 'A': return core::local_file_status::added;
			case 'D': return core::local_file_status::deleted;
			case 'R': return core::local_file_status::renamed;
			case 'C': return core::local_file_status::added;
			case 'T': return core::local_file_status::type_changed;
			case 'U': return core::local_file_status::unmerged;
			default:  return std::nullopt;
			}
		}

		/// The text after the `aCount`th space of a record.
		[[nodiscard]] auto after_fields(const std::string_view aRecord, const std::size_t aCount) -> std::optional<std::string_view>
		{
			auto position = std::size_t{ 0 };
			for (std::size_t field = 0U; field < aCount; ++field) {
				position = aRecord.find(' ', position);
				if (position == std::string_view::npos) {
					return std::nullopt;
				}
				++position;
			}
			return aRecord.substr(position);
		}

#if defined(Q_OS_UNIX)
		/// Walks a relative path from the root without following any link.
		/// \returns The parent directory's descriptor and the last component.
		[[nodiscard]] auto open_parent(const std::string& aRoot, const std::string& aPath, std::string& aLeaf) -> std::expected<int, application::worktree_file_failure>
		{
			if (aPath.empty() || aPath.front() == '/' || aPath.find('\0') != std::string::npos) {
				return std::unexpected{ application::worktree_file_failure::outside };
			}
			auto directory = ::open(aRoot.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (directory < 0) {
				return std::unexpected{ application::worktree_file_failure::missing };
			}
			auto rest = std::string_view{ aPath };
			while (true) {
				const auto slash = rest.find('/');
				const auto part  = std::string{ rest.substr(0U, slash) };
				if (part.empty() || part == "." || part == "..") {
					::close(directory);
					return std::unexpected{ application::worktree_file_failure::outside };
				}
				if (slash == std::string_view::npos) {
					aLeaf = part;
					return directory;
				}
				const auto next = ::openat(directory, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
				::close(directory);
				if (next < 0) {
					return std::unexpected{ errno == ELOOP || errno == ENOTDIR ? application::worktree_file_failure::link : application::worktree_file_failure::missing };
				}
				directory = next;
				rest.remove_prefix(slash + 1U);
			}
		}

		[[nodiscard]] auto read_all(const int aFile, const std::uint64_t aMaximum, std::vector<std::uint8_t>& aBytes) -> bool
		{
			auto buffer = std::array<std::uint8_t, 65536>{ };
			while (true) {
				const auto count = ::read(aFile, buffer.data(), buffer.size());
				if (count < 0) {
					if (errno == EINTR) {
						continue;
					}
					return false;
				}
				if (count == 0) {
					return true;
				}
				aBytes.insert(aBytes.end(), buffer.begin(), buffer.begin() + count);
				if (aBytes.size() > aMaximum) {
					return false;
				}
			}
		}
#endif

		class worktree_files final : public application::worktree_file_port
		{
		public:
			auto read(const std::string& aRoot, const std::string& aPath, const std::uint64_t aMaximum) -> std::expected<application::worktree_file, application::worktree_file_failure> override
			{
#if defined(Q_OS_UNIX)
				auto leaf   = std::string{ };
				auto parent = open_parent(aRoot, aPath, leaf);
				auto file   = application::worktree_file{ };
				if (!parent) {
					if (parent.error() == application::worktree_file_failure::missing) {
						file.Inside = true;
						return file;
					}
					return std::unexpected{ parent.error() };
				}
				file.Inside = true;
				struct stat facts{ };
				if (::fstatat(*parent, leaf.c_str(), &facts, AT_SYMLINK_NOFOLLOW) != 0) {
					::close(*parent);
					return file;
				}
				file.Exists  = true;
				file.Link    = S_ISLNK(facts.st_mode);
				file.Regular = S_ISREG(facts.st_mode);
				if (!file.Regular) {
					::close(*parent);
					return file;
				}
				if (static_cast<std::uint64_t>(facts.st_size) > aMaximum) {
					::close(*parent);
					return std::unexpected{ application::worktree_file_failure::too_large };
				}
				const auto opened = ::openat(*parent, leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
				::close(*parent);
				if (opened < 0 || !read_all(opened, aMaximum, file.Bytes)) {
					if (opened >= 0) {
						::close(opened);
					}
					return std::unexpected{ application::worktree_file_failure::io };
				}
				::close(opened);
				return file;
#else
				static_cast<void>(aRoot);
				static_cast<void>(aPath);
				static_cast<void>(aMaximum);
				return std::unexpected{ application::worktree_file_failure::io };
#endif
			}

			auto write(const std::string& aRoot, const std::string& aPath, const std::vector<std::uint8_t>& aExpected, const std::vector<std::uint8_t>& aBytes)
				-> std::expected<void, application::worktree_file_failure> override
			{
#if defined(Q_OS_UNIX)
				auto leaf   = std::string{ };
				auto parent = open_parent(aRoot, aPath, leaf);
				if (!parent) {
					return std::unexpected{ parent.error() };
				}
				const auto  close_parent = [&parent] { ::close(*parent); };
				struct stat facts{ };
				if (::fstatat(*parent, leaf.c_str(), &facts, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(facts.st_mode)) {
					close_parent();
					return std::unexpected{ application::worktree_file_failure::not_a_file };
				}
				// The disk still holds what the editor read or last wrote.
				const auto current = ::openat(*parent, leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
				auto       held    = std::vector<std::uint8_t>{ };
				if (current < 0 || !read_all(current, aExpected.size() + 1U, held)) {
					if (current >= 0) {
						::close(current);
					}
					close_parent();
					return std::unexpected{ application::worktree_file_failure::changed };
				}
				::close(current);
				if (held != aExpected) {
					close_parent();
					return std::unexpected{ application::worktree_file_failure::changed };
				}
				// A complete new file beside it, then one rename: the old or the new, never half.
				const auto temporary = "." + leaf + ".bpr-save";
				::unlinkat(*parent, temporary.c_str(), 0);
				const auto created = ::openat(*parent, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, facts.st_mode & 07777);
				if (created < 0) {
					close_parent();
					return std::unexpected{ application::worktree_file_failure::io };
				}
				auto written = std::size_t{ 0 };
				while (written < aBytes.size()) {
					const auto count = ::write(created, aBytes.data() + written, aBytes.size() - written);
					if (count < 0 && errno == EINTR) {
						continue;
					}
					if (count <= 0) {
						break;
					}
					written += static_cast<std::size_t>(count);
				}
				const auto complete = written == aBytes.size() && ::fchmod(created, facts.st_mode & 07777) == 0 && ::fsync(created) == 0;
				::close(created);
				if (!complete || ::renameat(*parent, temporary.c_str(), *parent, leaf.c_str()) != 0) {
					::unlinkat(*parent, temporary.c_str(), 0);
					close_parent();
					return std::unexpected{ application::worktree_file_failure::io };
				}
				close_parent();
				return { };
#else
				static_cast<void>(aRoot);
				static_cast<void>(aPath);
				static_cast<void>(aExpected);
				static_cast<void>(aBytes);
				return std::unexpected{ application::worktree_file_failure::io };
#endif
			}
		};
	}

	auto make_installed_author_git(std::filesystem::path aGitExecutable, const author_git_deadlines aDeadlines) -> std::unique_ptr<application::author_git_port>
	{
		return std::make_unique<installed_author_git>(std::move(aGitExecutable), aDeadlines);
	}

	auto make_worktree_files() -> std::unique_ptr<application::worktree_file_port>
	{
		return std::make_unique<worktree_files>();
	}

	auto parse_author_status(const std::string_view aOutput) -> std::optional<core::local_worktree_status>
	{
		auto status  = core::local_worktree_status{ };
		auto records = std::vector<std::string_view>{ };
		for (auto rest = aOutput; !rest.empty();) {
			const auto end = rest.find('\0');
			records.push_back(rest.substr(0U, end));
			if (end == std::string_view::npos) {
				break;
			}
			rest.remove_prefix(end + 1U);
		}
		for (std::size_t index = 0U; index < records.size(); ++index) {
			const auto record = records[index];
			if (record.empty()) {
				continue;
			}
			if (record.starts_with("# branch.oid ")) {
				const auto oid = record.substr(13U);
				if (oid != "(initial)") {
					const auto parsed = core::parse_git_commit_oid(oid);
					if (!parsed) {
						return std::nullopt;
					}
					status.Head = *parsed;
				}
				continue;
			}
			if (record.starts_with("# branch.head ")) {
				const auto head = record.substr(14U);
				status.Branch   = head == "(detached)" ? std::string{ } : std::string{ head };
				continue;
			}
			if (record.starts_with("# ") || record.starts_with("! ")) {
				continue;
			}
			if (record.starts_with("? ")) {
				status.Unstaged.push_back({ .Path = std::string{ record.substr(2U) }, .Status = core::local_file_status::untracked });
				continue;
			}
			const auto kind = record.front();
			if ((kind != '1' && kind != '2' && kind != 'u') || record.size() < 4U) {
				return std::nullopt;
			}
			const auto path = after_fields(record, kind == '1' ? 8U : kind == '2' ? 9U
																				  : 10U);
			if (!path) {
				return std::nullopt;
			}
			auto original = std::string{ };
			if (kind == '2') {
				if (index + 1U >= records.size()) {
					return std::nullopt;
				}
				original = std::string{ records[++index] };
			}
			if (kind == 'u') {
				status.Unstaged.push_back({ .Path = std::string{ *path }, .Status = core::local_file_status::unmerged });
				continue;
			}
			const auto staged   = status_letter(record[2]);
			const auto unstaged = status_letter(record[3]);
			if (staged) {
				status.Staged.push_back({ .Path = std::string{ *path }, .Original = *staged == core::local_file_status::renamed ? original : std::string{ }, .Status = *staged });
			}
			if (unstaged) {
				status.Unstaged.push_back({ .Path = std::string{ *path }, .Status = *unstaged == core::local_file_status::renamed ? core::local_file_status::modified : *unstaged });
			}
		}
		return status;
	}
}
