#include "../../src/adapters/git/src/git_process_output.hpp"
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "windows_system_root.hpp"

#include <QByteArray>
#include <QCoreApplication>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
	#include <windows.h>
#endif
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace {
	namespace application = bpr::application;
	namespace core        = bpr::core;
	namespace git_adapter = bpr::adapters::git;

	inline constexpr std::string_view sha1_commit_hex = "826bb464b84352d60b3d7c56481a599a838d998d";
	inline constexpr std::string_view sha1_tree_hex   = "4ce9ffbf7ccd31debb0a343022131f0fde64e35d";
	inline constexpr std::string_view sha1_pack_hex   = "e248b08f09a75116fdb2397c928a84f6a93f6c34";
	inline constexpr std::string_view raw_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "https://fixture-token@remote.invalid/owner/repo.git\n";
	inline constexpr std::string_view redacted_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "<redacted-url>\n";
	inline constexpr std::string_view sha256_tree_hex = "d1b1a86a6b5182208ff0345ff6f64d9c48b9d058e0e8a88fbaefcf192384aa8d";
	inline constexpr std::string_view sha256_pack_hex = "3e3468388a66c6475d2c1fdb8fb7286bc0ae1f934b6352358cc98e4e5e823b69";

	class recording_blob_sink final
		: public application::git_blob_materialization_sink
	{
	public:
		auto accept(
			const application::git_blob_id&  aBlob,
			const std::span<const std::byte> aBytes) -> void override
		{
			Blob = aBlob;
			Bytes.clear();
			Bytes.reserve(aBytes.size());
			std::ranges::transform(
				aBytes,
				std::back_inserter(Bytes),
				[](const std::byte aByte) {
					return std::to_integer<std::uint8_t>(aByte);
				});
		}

		std::optional<application::git_blob_id> Blob;
		std::vector<std::uint8_t>               Bytes;
	};

	class recording_changed_entry_sink final : public application::git_changed_entry_sink
	{
	public:
		auto accept(const application::git_changed_entry& aEntry) -> bool override
		{
			Entries.push_back(aEntry);
			return true;
		}

		std::vector<application::git_changed_entry> Entries;
	};

	inline constexpr std::array<std::uint8_t, 230U> sha1_pack{
		0x50,
		0x41,
		0x43,
		0x4b,
		0x00,
		0x00,
		0x00,
		0x02,
		0x00,
		0x00,
		0x00,
		0x03,
		0x97,
		0x0b,
		0x78,
		0x9c,
		0x95,
		0xcb,
		0xc1,
		0x0a,
		0x02,
		0x21,
		0x10,
		0x80,
		0xe1,
		0xbb,
		0x4f,
		0x31,
		0xf7,
		0x20,
		0x74,
		0x35,
		0xdb,
		0x85,
		0x58,
		0xa2,
		0x4b,
		0xd7,
		0xe8,
		0x0d,
		0x46,
		0x1d,
		0x69,
		0xc0,
		0x5d,
		0xc5,
		0xa6,
		0xa8,
		0xb7,
		0xaf,
		0x1e,
		0xa1,
		0xff,
		0xfc,
		0xfd,
		0xd2,
		0x89,
		0xc0,
		0x45,
		0x9a,
		0x72,
		0x0e,
		0x79,
		0x1f,
		0x63,
		0xb2,
		0x26,
		0x51,
		0x08,
		0x1a,
		0xad,
		0xb3,
		0x7a,
		0x18,
		0x8c,
		0x35,
		0x59,
		0xe7,
		0x44,
		0xde,
		0x91,
		0xdd,
		0x25,
		0x85,
		0x0f,
		0xb9,
		0xd5,
		0x0e,
		0xa7,
		0xcb,
		0x15,
		0x0e,
		0xa1,
		0xf5,
		0x23,
		0xbd,
		0x70,
		0x69,
		0x85,
		0xb6,
		0xbc,
		0x3e,
		0xb1,
		0x70,
		0x9a,
		0x61,
		0x72,
		0xde,
		0x8f,
		0x6e,
		0xd4,
		0x1a,
		0x36,
		0xfa,
		0x9b,
		0x8a,
		0x75,
		0x59,
		0x58,
		0x84,
		0xfe,
		0x79,
		0xd4,
		0x8f,
		0xf2,
		0x7a,
		0x17,
		0x2c,
		0x85,
		0x12,
		0x9c,
		0x59,
		0x20,
		0x62,
		0xc3,
		0xc0,
		0x85,
		0xe5,
		0x0d,
		0xad,
		0xd7,
		0x40,
		0xea,
		0x03,
		0x67,
		0xd5,
		0x39,
		0x11,
		0xa5,
		0x02,
		0x78,
		0x9c,
		0x33,
		0x34,
		0x30,
		0x30,
		0x33,
		0x31,
		0x51,
		0x28,
		0x28,
		0xca,
		0x4f,
		0x4a,
		0xd5,
		0x2b,
		0xa9,
		0x28,
		0x61,
		0xb8,
		0xc5,
		0xe3,
		0xb7,
		0xe3,
		0xe6,
		0x92,
		0x5e,
		0x71,
		0x29,
		0xe3,
		0x70,
		0x6f,
		0x0b,
		0xf6,
		0xa0,
		0x87,
		0xcd,
		0x1a,
		0xe9,
		0x81,
		0x00,
		0xe4,
		0xb2,
		0x0c,
		0xc7,
		0x36,
		0x78,
		0x9c,
		0x2b,
		0x28,
		0xca,
		0x4f,
		0x4a,
		0xe5,
		0x02,
		0x00,
		0x08,
		0x96,
		0x02,
		0x23,
		0xe2,
		0x48,
		0xb0,
		0x8f,
		0x09,
		0xa7,
		0x51,
		0x16,
		0xfd,
		0xb2,
		0x39,
		0x7c,
		0x92,
		0x8a,
		0x84,
		0xf6,
		0xa9,
		0x3f,
		0x6c,
		0x34,
	};

	auto require(const bool aCondition, const std::string_view aMessage) -> void
	{
		bpr::tests::expect(aCondition, aMessage);
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation.
			Path = bpr::tests::make_scratch_directory("bpr-" + std::string{ aLabel });
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			std::filesystem::permissions(
				Path,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			Path = std::filesystem::canonical(Path);
			made.release();
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		~temporary_root()
		{
			std::error_code error;
			std::filesystem::remove_all(Path, error);
		}

		std::filesystem::path Path;
	};

	class scoped_environment final
	{
	public:
		scoped_environment()                          = default;
		scoped_environment(const scoped_environment&) = delete;
		auto operator=(const scoped_environment&)
			-> scoped_environment& = delete;

		~scoped_environment()
		{
			for (auto entry = Saved.rbegin(); entry != Saved.rend(); ++entry) {
				if (entry->WasSet) {
					static_cast<void>(
						qputenv(entry->Name.constData(), entry->Value));
				} else {
					static_cast<void>(
						qunsetenv(entry->Name.constData()));
				}
			}
		}

		auto set(
			const char*       aName,
			const QByteArray& aValue) -> void
		{
			Saved.push_back({
				.Name   = QByteArray{ aName },
				.WasSet = qEnvironmentVariableIsSet(aName),
				.Value  = qgetenv(aName),
			});
			require(qputenv(aName, aValue), "set hostile parent environment");
		}

		auto set(
			const char*                  aName,
			const std::filesystem::path& aValue) -> void
		{
			set(aName, QByteArray::fromStdString(aValue.string()));
		}

	private:
		struct saved_value
		{
			QByteArray Name;
			bool       WasSet = false;
			QByteArray Value;
		};

		std::vector<saved_value> Saved;
	};

	auto write_bytes(
		const std::filesystem::path&     aPath,
		const std::span<const std::byte> aBytes) -> void
	{
		std::ofstream output{ aPath, std::ios::binary | std::ios::trunc };
		require(static_cast<bool>(output), "open fixture file");
		output.write(
			reinterpret_cast<const char*>(aBytes.data()),
			static_cast<std::streamsize>(aBytes.size()));
		output.close();
		require(static_cast<bool>(output), "write fixture file");
		std::filesystem::permissions(
			aPath,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
	}

	auto write_text(
		const std::filesystem::path& aPath,
		const std::string_view       aText) -> void
	{
		write_bytes(aPath, std::as_bytes(std::span{ aText }));
	}

	auto write_executable_canary(
		const std::filesystem::path& aPath,
		const std::filesystem::path& aMarker,
		const std::string_view       aLabel) -> void
	{
		// The canary has two jobs: make the policy directory non-empty, which
		// the adapter must refuse before Git runs, and record an invocation
		// if Git ever does run it. On Windows the file is written the same
		// way without the mode bits; the adapter's refusal needs only the
		// entry, and a hook Git cannot execute records nothing, which is the
		// expected count (BPR-0437).
		write_text(
			aPath,
			"#!/bin/sh\n"
			"printf '%s\\n' '"
				+ std::string{ aLabel }
				+ "' >> '" + aMarker.string() + "'\n"
												"exit 91\n");
#if defined(Q_OS_UNIX)
		std::filesystem::permissions(
			aPath,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
#endif
	}

	[[nodiscard]] auto read_text(
		const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ }
		};
	}

	[[nodiscard]] auto make_operation(
		const std::uint8_t aValue) -> application::git_operation_identity
	{
		application::git_operation_identity result{ };
		result.Bytes.fill(aValue);
		return result;
	}

	[[nodiscard]] auto make_scope(
		const std::uint8_t aValue)
		-> application::git_pull_request_scope_identity
	{
		application::git_pull_request_scope_identity result{ };
		result.Bytes.fill(aValue);
		return result;
	}

	template <typename Identity>
	[[nodiscard]] auto core_identity(const std::uint8_t aValue) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aValue);
		return Identity{ bytes };
	}

	[[nodiscard]] auto workspace_authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 1U },
			.ProfileRevision = core::github_profile_revision{ 1U },
			.Host            = core::git_host_id{ 1U },
			.HostAuthority   = core_identity<core::host_authority_fingerprint>(1U),
			.Account         = core::github_account_id{ 1U },
			.Viewer          = core::github_viewer_actor_id{ 1U },
			.Repository      = core::repository_id{ 1U },
			.Cache           = core_identity<core::repository_cache_scope_id>(2U),
			.PullRequest     = core::pull_request_id{ 1U },
			.Number          = core::pull_request_number{ 1U },
			.Scope           = core_identity<core::pull_request_authority_scope_id>(3U),
		};
	}

	[[nodiscard]] constexpr auto nibble(
		const char aValue) -> std::uint8_t
	{
		return aValue <= '9'
			? static_cast<std::uint8_t>(aValue - '0')
			: static_cast<std::uint8_t>(aValue - 'a' + 10);
	}

	template <typename Object>
	[[nodiscard]] auto object(
		const std::string_view                aText,
		const application::git_hash_algorithm aAlgorithm) -> Object
	{
		Object result{ .Algorithm = aAlgorithm };
		for (std::size_t index = 0; index < aText.size() / 2U; ++index) {
			result.Digest[index] = static_cast<std::uint8_t>(
				static_cast<std::uint8_t>(nibble(aText[index * 2U]) << 4U)
				| nibble(aText[index * 2U + 1U]));
		}
		require(application::valid(result), "valid fixture object");
		return result;
	}

	[[nodiscard]] auto repository_path(
		const std::filesystem::path& aRoot,
		const char                   aDigit) -> std::filesystem::path
	{
		return aRoot / "repositories" / "v1"
			/ (std::string(64U, aDigit) + ".git");
	}

	auto fake_log_invocation(const QStringList& aArguments) -> void
	{
		const auto home = qEnvironmentVariable("HOME");
		if (home.isEmpty()) {
			return;
		}
		const auto log = std::filesystem::path{ home.toStdString() }.parent_path()
			/ "invocations.log";
		// Binary, like every other fixture write: the assertions search for
		// exact `\n`-terminated records (BPR-0437).
		std::ofstream output{ log, std::ios::app | std::ios::binary };
		output << "BEGIN\n";
		output << "cwd="
			   << QDir::toNativeSeparators(QDir::currentPath()).toStdString()
			   << '\n';
		for (const auto& argument : aArguments) {
			output << "arg=" << argument.toStdString() << '\n';
		}
		auto environment = QProcessEnvironment::systemEnvironment();
		auto names       = environment.keys();
		std::sort(names.begin(), names.end());
		for (const auto& name : names) {
			output << "env=" << name.toStdString() << "="
				   << environment.value(name).toStdString() << '\n';
		}
		output << "END\n";
	}

	[[nodiscard]] auto fake_isolation_root()
		-> std::filesystem::path
	{
		const auto home = qEnvironmentVariable("HOME");
		return home.isEmpty()
			? std::filesystem::path{ }
			: std::filesystem::path{ home.toStdString() }.parent_path();
	}

	[[nodiscard]] auto fake_repository(
		const QStringList& aArguments) -> std::filesystem::path
	{
		for (const auto& argument : aArguments) {
			if (argument.startsWith("--git-dir=")) {
				return {
					argument.sliced(
								static_cast<qsizetype>(
									std::string_view{ "--git-dir=" }.size()))
						.toStdString()
				};
			}
		}
		return { };
	}

	[[nodiscard]] auto read_standard_input() -> std::string
	{
		std::string             result;
		std::array<char, 4096U> buffer{ };
		for (;;) {
			std::cin.read(
				buffer.data(),
				static_cast<std::streamsize>(buffer.size()));
			const auto count = std::cin.gcount();
			if (count > 0) {
				result.append(
					buffer.data(),
					static_cast<std::size_t>(count));
				if (result.starts_with("BPR_SLOW_INPUT:")) {
					std::this_thread::sleep_for(
						std::chrono::milliseconds{ 1 });
				}
			}
			if (count != static_cast<std::streamsize>(buffer.size())) {
				break;
			}
		}
		return result;
	}

	[[nodiscard]] auto split_nul(
		const std::string_view aInput) -> std::vector<std::string>
	{
		std::vector<std::string> fields;
		std::size_t              offset = 0U;
		while (offset < aInput.size()) {
			const auto end = aInput.find('\0', offset);
			if (end == std::string_view::npos) {
				break;
			}
			fields.emplace_back(aInput.substr(offset, end - offset));
			offset = end + 1U;
		}
		return fields;
	}

	[[nodiscard]] auto fake_refs(
		const std::filesystem::path& aRepository)
		-> std::map<std::string, std::string>
	{
		std::map<std::string, std::string> refs;
		std::ifstream                      input{ aRepository / "fake-refs" };
		std::string                        ref;
		std::string                        oid;
		while (input >> ref >> oid) {
			refs.emplace(std::move(ref), std::move(oid));
		}
		return refs;
	}

	auto write_fake_refs(
		const std::filesystem::path&              aRepository,
		const std::map<std::string, std::string>& aRefs) -> void
	{
		std::ofstream output{
			aRepository / "fake-refs",
			std::ios::trunc
		};
		for (const auto& [ref, oid] : aRefs) {
			output << ref << ' ' << oid << '\n';
		}
	}

	[[nodiscard]] auto fake_update_refs(
		const std::filesystem::path& aRepository,
		const std::string_view       aInput) -> int
	{
		auto       refs   = fake_refs(aRepository);
		const auto fields = split_nul(aInput);
		if (fields.empty() || fields.front() != "start") {
			return 2;
		}
		for (std::size_t index = 1U; index < fields.size();) {
			if (fields[index] == "prepare" || fields[index] == "commit") {
				++index;
				continue;
			}
			const auto separator = fields[index].find(' ');
			if (separator == std::string::npos) {
				return 2;
			}
			const auto verb = fields[index].substr(0U, separator);
			const auto ref  = fields[index].substr(separator + 1U);
			if (verb == "create") {
				if (index + 1U >= fields.size() || refs.contains(ref)) {
					return 3;
				}
				refs[ref] = fields[index + 1U];
				index += 2U;
			} else if (verb == "update") {
				if (
					index + 2U >= fields.size()
					|| !refs.contains(ref)
					|| refs[ref] != fields[index + 2U]) {
					return 3;
				}
				refs[ref] = fields[index + 1U];
				index += 3U;
			} else if (verb == "delete") {
				if (
					index + 1U >= fields.size()
					|| !refs.contains(ref)
					|| refs[ref] != fields[index + 1U]) {
					return 3;
				}
				refs.erase(ref);
				index += 2U;
			} else {
				return 2;
			}
		}
		write_fake_refs(aRepository, refs);
		std::cout << "start: ok\nprepare: ok\ncommit: ok\n";
		return 0;
	}

	[[nodiscard]] auto fake_git(
		const QStringList&           aArguments,
		const std::filesystem::path& aExecutable) -> int
	{
		fake_log_invocation(aArguments);
		if (aArguments == QStringList{ "--exec-path" }) {
			if (
				std::filesystem::exists(
					aExecutable.parent_path()
					/ "fail-bootstrap-exec-path")) {
				std::cerr << raw_git_diagnostic;
				return 128;
			}
			std::cout
				<< aExecutable.parent_path().string()
				<< '\n';
			return 0;
		}
		if (aArguments == QStringList{ "--version" }) {
			std::cout << "git version 2.50.1\n";
			return 0;
		}
		if (aArguments.contains("init")) {
			const auto repository = std::filesystem::path{ aArguments.back().toStdString() };
			std::filesystem::create_directories(repository / "objects");
			std::filesystem::create_directories(repository / "refs");
			std::filesystem::permissions(
				repository,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			write_text(repository / "config", "");
			write_text(repository / "HEAD", "");
			if (
				std::filesystem::exists(
					fake_isolation_root()
					/ "fail-capability-probe-after-init")) {
				return 91;
			}
			return 0;
		}

		const auto repository = fake_repository(aArguments);
		if (aArguments.contains("index-pack")) {
			const auto input = read_standard_input();
			if (
				std::filesystem::exists(
					fake_isolation_root()
					/ "block-capability-probe-index-pack")) {
				write_text(
					fake_isolation_root()
						/ "capability-probe-block-started",
					"started");
				std::this_thread::sleep_for(std::chrono::seconds{ 20 });
				return 93;
			}
			if (
				std::filesystem::exists(
					fake_isolation_root()
					/ "fail-capability-probe-index-pack")) {
				return 92;
			}
			if (input.starts_with("BPR_BIG_OUTPUT")) {
				std::cout << std::string(2048U, 'x');
				return 0;
			}
			if (input.starts_with("BPR_BIG_ERROR")) {
				std::cerr << std::string(20U * 1024U, 'x');
				return 0;
			}
			if (input.starts_with("BPR_ERROR_AT_LIMIT")) {
				std::cerr << std::string(16U * 1024U, 'x');
				return 128;
			}
			if (input.starts_with("BPR_ERROR_OVER_LIMIT")) {
				std::cerr << std::string(16U * 1024U + 1U, 'x');
				return 128;
			}
			if (input.starts_with("BPR_ERROR_REDACTION_EXPANSION")) {
				for (auto index = 0U; index < 3000U; ++index) {
					std::cerr << "a:// ";
				}
				return 128;
			}
			if (input.starts_with("BPR_FAIL_MESSAGE")) {
				std::cerr << raw_git_diagnostic;
				return 128;
			}
			if (input.starts_with("BPR_REMOTE_SHAPES")) {
				std::cerr
					<< "fatal: local /tmp/user@host:dir/pack-f041bc5e.pack "
					   "remains diagnostic\n"
					   "fatal: fallback remote "
					   "git@remote.invalid:owner/repo.git denied\n"
					   "fatal: local reflog HEAD@{1} remains diagnostic\n";
				return 128;
			}
			if (input.starts_with("BPR_QUIET_ERROR")) {
				std::cerr << "note: accepted exit still wrote here\n";
				return 0;
			}
			if (input.starts_with("BPR_EXIT_ONE_ERROR")) {
				// The accepted BPR-0846 exit, with the exact pack line, but
				// something on standard error (BPR-0851).
				std::cout << "pack\t" << sha1_pack_hex << '\n';
				std::cerr << "warning: accepted exit one still wrote here\n";
				return 1;
			}
			if (input.starts_with("BPR_MALFORMED")) {
				std::cout << "pack\tinvalid\n";
				return 0;
			}
			if (input.starts_with("BPR_BLOCK")) {
#if defined(Q_OS_UNIX)
				const auto child = ::fork();
				if (child == 0) {
					std::this_thread::sleep_for(
						std::chrono::milliseconds{ 1500 });
					const auto marker = std::filesystem::path{
						qEnvironmentVariable("HOME").toStdString()
					}
											.parent_path()
						/ "descendant-survived";
					write_text(marker, "survived");
					::_exit(0);
				}
#endif
				// [[BPR-1115]]. The inherited-handle row waits for this before it
				// looks at what the running Git holds.
				write_text(fake_isolation_root() / "block-started", "started");
				std::this_thread::sleep_for(std::chrono::seconds{ 20 });
				return 0;
			}
			if (input.starts_with("BPR_SLOW_INPUT:")) {
				const auto newline = input.find('\n');
				if (newline == std::string::npos) {
					return 4;
				}
				const auto expected = static_cast<std::size_t>(
					std::stoull(input.substr(15U, newline - 15U)));
				if (expected != input.size()) {
					return 4;
				}
			}
			const auto sha256 = read_text(repository / "config").find("sha256")
				!= std::string::npos;
			const auto packIdentity  = sha256 ? sha256_pack_hex : sha1_pack_hex;
			const auto packDirectory = repository / "objects" / "pack";
			std::filesystem::create_directories(packDirectory);
			std::filesystem::permissions(
				packDirectory,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			write_text(
				packDirectory
					/ ("pack-" + std::string{ packIdentity } + ".pack"),
				input);
			write_text(
				packDirectory
					/ ("pack-" + std::string{ packIdentity } + ".idx"),
				"fixture-index");
			std::cout << "pack\t"
					  << packIdentity
					  << '\n';
			return 0;
		}
		if (aArguments.contains("cat-file")) {
			const auto input = read_standard_input();
			if (std::ranges::any_of(
					aArguments,
					[](const QString& aArgument) {
						return aArgument.startsWith("--batch-check=");
					})) {
				if (input.empty() || input.back() != '\n') {
					return 4;
				}
				std::cout << input.substr(0U, input.size() - 1U)
						  << " commit\n";
			}
			return 0;
		}
		if (aArguments.contains("rev-parse")) {
			const auto sha256 = read_text(repository / "config").find("sha256")
				!= std::string::npos;
			std::cout << (sha256 ? sha256_tree_hex : sha1_tree_hex)
					  << '\n';
			return 0;
		}
		if (aArguments.contains("merge-base")) {
			if (aArguments.contains("--all")) {
				std::cout << read_text(fake_isolation_root() / "comparison-output");
			}
			return 0;
		}
		if (aArguments.contains("log") && aArguments.contains("--no-walk") && aArguments.contains("--format=%ct")) {
			std::cout << read_text(fake_isolation_root() / "committer-output");
			return 0;
		}
		if (aArguments.contains("fsck")) {
			// A broken closure: findings on standard output, nothing on
			// standard error, exit 2, as Git's fsck reports it (BPR-0852).
			if (std::filesystem::exists(fake_isolation_root() / "fsck-output")) {
				const auto output = read_text(fake_isolation_root() / "fsck-output");
				std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
				return 2;
			}
			return 0;
		}
		if (aArguments.contains("update-ref")) {
			return fake_update_refs(
				repository,
				read_standard_input());
		}
		if (aArguments.contains("for-each-ref")) {
			const auto refs  = fake_refs(repository);
			const auto ref   = aArguments.back().toStdString();
			const auto found = refs.find(ref);
			if (found != refs.end()) {
				std::cout << found->first << '\0' << found->second
						  << '\0' << '\n';
			}
			return 0;
		}
		if (aArguments.contains("diff-tree")) {
			const auto output = read_text(fake_isolation_root() / "rename-output");
			std::cout.write(
				output.data(),
				static_cast<std::streamsize>(output.size()));
			return 0;
		}
		return 9;
	}

	[[nodiscard]] auto make_adapter(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot)
		-> std::unique_ptr<application::git_process_port>
	{
		auto made = git_adapter::make_installed_git_process(
			{
				.GitExecutable     = std::filesystem::canonical(aExecutable),
				.IsolationRoot     = std::filesystem::canonical(aRoot),
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
		if (!made.has_value()) {
			const auto message = "construct installed Git adapter, failure="
				+ std::to_string(static_cast<unsigned int>(made.error().Code))
				+ " detail=[" + made.error().Detail + "]";
			require(false, message);
		}
		return std::move(*made);
	}

	[[nodiscard]] auto make_fake_git_executable(
		const std::filesystem::path& aRoot) -> std::filesystem::path
	{
		const auto directory = aRoot / "fake-exec";
		std::filesystem::create_directories(directory);
		std::filesystem::permissions(
			directory,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto executable = directory
#if defined(Q_OS_WIN)
			/ "git.exe";
#else
			/ "git";
#endif
		std::filesystem::copy_file(
			QCoreApplication::applicationFilePath().toStdString(),
			executable);
		std::filesystem::permissions(
			executable,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
#if defined(Q_OS_WIN)
		// The copy is launched through the adapter's own environment, whose
		// PATH is this directory and nothing else, so the shared Qt the copy
		// links has to travel with it: every DLL beside this executable —
		// put there after each link for the same reason — is copied beside
		// the fake (BPR-0437).
		const auto origin = std::filesystem::path{
			QCoreApplication::applicationDirPath().toStdString()
		};
		for (const auto& entry : std::filesystem::directory_iterator{ origin }) {
			if (entry.is_regular_file() && entry.path().extension() == ".dll") {
				std::filesystem::copy_file(
					entry.path(),
					directory / entry.path().filename(),
					std::filesystem::copy_options::overwrite_existing);
			}
		}
#endif
		return executable;
	}

	[[nodiscard]] auto run_fixture_git(
		const std::filesystem::path&    aRepository,
		const std::vector<std::string>& aArguments,
		const std::string_view          aInput          = { },
		const bool                      aExpectIdentity = true) -> std::string
	{
		QProcess process;
		process.setProgram(QString::fromStdString(BPR_TEST_GIT_EXECUTABLE));
		auto command = QStringList{
			QString::fromStdString(
				"--git-dir=" + aRepository.string())
		};
		for (const auto& argument : aArguments) {
			command.push_back(QString::fromStdString(argument));
		}
		process.setArguments(command);
		auto environment = QProcessEnvironment::systemEnvironment();
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert(
			"GIT_CONFIG_GLOBAL",
			QString::fromStdString(
				(aRepository.parent_path() / "fixture-global-config")
					.string()));
		environment.insert("GIT_ATTR_NOSYSTEM", "1");
		environment.insert("GIT_TERMINAL_PROMPT", "0");
		environment.insert("GIT_AUTHOR_NAME", "BetterPullReview Fixture");
		environment.insert(
			"GIT_AUTHOR_EMAIL",
			"fixture@betterpullreview.invalid");
		environment.insert(
			"GIT_AUTHOR_DATE",
			"2000-01-01T00:00:00+0000");
		environment.insert("GIT_COMMITTER_NAME", "BetterPullReview Fixture");
		environment.insert(
			"GIT_COMMITTER_EMAIL",
			"fixture@betterpullreview.invalid");
		environment.insert(
			"GIT_COMMITTER_DATE",
			"2000-01-01T00:00:00+0000");
		environment.insert("LC_ALL", "C");
		environment.insert("LANG", "C");
		process.setProcessEnvironment(environment);
		process.setWorkingDirectory(
			QString::fromStdString(aRepository.parent_path().string()));
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.start();
		require(
			process.waitForStarted(5000),
			"start local connectivity fixture Git");
		if (!aInput.empty()) {
			require(
				process.write(
					aInput.data(),
					static_cast<qint64>(aInput.size()))
					== static_cast<qint64>(aInput.size()),
				"write complete connectivity fixture input");
		}
		process.closeWriteChannel();
		require(
			process.waitForFinished(10000),
			"finish local connectivity fixture Git");
		const auto standardOutput = process.readAllStandardOutput();
		const auto standardError  = process.readAllStandardError();
		require(
			process.exitStatus() == QProcess::NormalExit
				&& process.exitCode() == 0
				&& standardError.isEmpty(),
			"local connectivity fixture Git succeeds silently");
		auto result = standardOutput.toStdString();
		if (!aExpectIdentity) {
			require(
				result.empty(),
				"local connectivity fixture mutation is silent");
			return { };
		}
		require(
			!result.empty() && result.back() == '\n'
				&& result.find('\n') == result.size() - 1U,
			"local connectivity fixture emits one object identity");
		result.pop_back();
		return result;
	}

	struct connectivity_fixture
	{
		std::string                Blob;
		std::string                NestedTree;
		std::string                RootTree;
		std::string                MissingParent;
		application::git_commit_id Base;
		application::git_commit_id Head;
	};

	struct rename_chain_fixture
	{
		application::git_commit_id Base;
		application::git_commit_id HeadOne;
		application::git_commit_id HeadTwo;
	};

	[[nodiscard]] auto make_connectivity_fixture(
		const std::filesystem::path&          aRepository,
		const application::git_hash_algorithm aAlgorithm)
		-> connectivity_fixture
	{
		const auto blob = run_fixture_git(
			aRepository,
			{ "hash-object", "-w", "--stdin" },
			"connected fixture blob\n");
		const auto nestedTree = run_fixture_git(
			aRepository,
			{ "mktree" },
			"100644 blob " + blob + "\tfile.txt\n");
		const auto rootTree = run_fixture_git(
			aRepository,
			{ "mktree" },
			"040000 tree " + nestedTree + "\tdirectory\n"
				+ "100755 blob " + blob + "\texecutable.sh\n"
				+ "120000 blob " + blob + "\tlink\n");
		const auto base = run_fixture_git(
			aRepository,
			{ "commit-tree", rootTree },
			"base\n");
		const auto missingParent = run_fixture_git(
			aRepository,
			{ "commit-tree", rootTree },
			"missing parent\n");
		const auto head = run_fixture_git(
			aRepository,
			{ "commit-tree", rootTree, "-p", missingParent },
			"head\n");
		return {
			.Blob          = blob,
			.NestedTree    = nestedTree,
			.RootTree      = rootTree,
			.MissingParent = missingParent,
			.Base          = object<application::git_commit_id>(base, aAlgorithm),
			.Head          = object<application::git_commit_id>(head, aAlgorithm),
		};
	}

	[[nodiscard]] auto make_rename_chain_fixture(
		const std::filesystem::path&          aRepository,
		const application::git_hash_algorithm aAlgorithm)
		-> rename_chain_fixture
	{
		const auto blob = run_fixture_git(
			aRepository,
			{ "hash-object", "-w", "--stdin" },
			"rename chain\n");
		const auto tree = [&](const std::string_view aPath) {
			return run_fixture_git(
				aRepository,
				{ "mktree" },
				"100644 blob " + blob + "\t" + std::string{ aPath } + "\n");
		};
		const auto base = run_fixture_git(
			aRepository,
			{ "commit-tree", tree("a.cpp") },
			"base\n");
		const auto headOne = run_fixture_git(
			aRepository,
			{ "commit-tree", tree("b.cpp"), "-p", base },
			"head one\n");
		const auto headTwo = run_fixture_git(
			aRepository,
			{ "commit-tree", tree("c.cpp"), "-p", headOne },
			"head two\n");
		return {
			.Base    = object<application::git_commit_id>(base, aAlgorithm),
			.HeadOne = object<application::git_commit_id>(headOne, aAlgorithm),
			.HeadTwo = object<application::git_commit_id>(headTwo, aAlgorithm),
		};
	}

	[[nodiscard]] auto loose_object_path(
		const std::filesystem::path& aRepository,
		const std::string_view       aIdentity) -> std::filesystem::path
	{
		require(aIdentity.size() == 40U || aIdentity.size() == 64U,
				"complete loose-object identity");
		return aRepository / "objects"
			/ std::string{ aIdentity.substr(0U, 2U) }
		/ std::string{ aIdentity.substr(2U) };
	}

	auto initialize_repository(
		application::git_process_port&        aDapter,
		const std::filesystem::path&          aRepository,
		const application::git_hash_algorithm aAlgorithm) -> void
	{
		const auto initialized = aDapter.initialize_bare_repository(
			{ .Repository = aRepository, .Algorithm = aAlgorithm },
			{ });
		require(initialized.has_value(), "initialize app-owned bare repository");
	}

	auto write_spool(
		application::git_process_port&             aDapter,
		const application::git_operation_identity& aOperation,
		const std::span<const std::byte>           aBytes) -> void
	{
		const auto path = aDapter.pack_spool_path(aOperation);
		require(path.has_value(), "derive exact operation spool");
		write_bytes(*path, aBytes);
	}

	auto run_real_git_contract() -> void
	{
		temporary_root root{ "git-real" };
		auto           adapter = make_adapter(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
			root.Path);
		require(
			adapter->installation().VersionMajor > 2U
				|| (adapter->installation().VersionMajor == 2U
					&& adapter->installation().VersionMinor >= 39U),
			"bound installation is Git 2.39 or newer");
		const auto repository = repository_path(root.Path, '1');
		initialize_repository(
			*adapter,
			repository,
			application::git_hash_algorithm::sha1);

		const auto operation = make_operation(0x22U);
		const auto spoolPath = adapter->pack_spool_path(operation);
		require(
			spoolPath.has_value()
				&& *spoolPath
					== root.Path / "incoming" / "v1"
						/ (std::string(64U, '2') + ".pack.part"),
			"spool uses exact incoming/v1 operation pack.part grammar");
		write_spool(
			*adapter,
			operation,
			std::as_bytes(std::span{ sha1_pack }));
		const auto pack = object<application::git_pack_id>(
			sha1_pack_hex,
			application::git_hash_algorithm::sha1);
		const auto commit = object<application::git_commit_id>(
			sha1_commit_hex,
			application::git_hash_algorithm::sha1);
		const auto tree = object<application::git_tree_id>(
			sha1_tree_hex,
			application::git_hash_algorithm::sha1);
		const auto indexed = adapter->index_pack(
			{
				.Repository        = repository,
				.Operation         = operation,
				.SpoolBytes        = sha1_pack.size(),
				.MaximumInputBytes = sha1_pack.size(),
				.ExpectedPack      = pack,
			},
			{ });
		require(indexed.has_value() && indexed->Pack == pack, "strict pack import");
		const auto installedPack = repository / "objects" / "pack"
			/ ("pack-" + std::string{ sha1_pack_hex } + ".pack");
		const auto installedIndex = repository / "objects" / "pack"
			/ ("pack-" + std::string{ sha1_pack_hex } + ".idx");
		const auto reverseIndex = repository / "objects" / "pack"
			/ ("pack-" + std::string{ sha1_pack_hex } + ".rev");
		const auto installedBytes = read_text(installedPack);
		require(
			installedBytes.size() == sha1_pack.size()
				&& std::equal(
					installedBytes.begin(),
					installedBytes.end(),
					reinterpret_cast<const char*>(sha1_pack.data())),
			"installed pack is the exact request spool");
		require(
			std::filesystem::is_regular_file(installedIndex)
				&& std::filesystem::file_size(installedIndex) > 0U,
			"matching nonempty pack index exists");
		require(
			!std::filesystem::exists(reverseIndex),
			"strict import creates no reverse index");
		const auto inspected = adapter->inspect_commit(
			{ .Repository = repository, .Commit = commit },
			{ });
		require(
			inspected.has_value() && inspected->Tree == tree,
			"commit and tree objects verified");
		const auto ancestry = adapter->is_ancestor(
			{
				.Repository = repository,
				.Ancestor   = commit,
				.Descendant = commit,
			},
			{ });
		require(
			ancestry == application::git_ancestry::ancestor,
			"fixed ancestry query");

		{
			temporary_root workspaceParent{ "git-agent-workspace" };
			const auto     head = core::git_commit_oid{
				core::git_hash_algorithm::sha1,
				commit.Digest,
			};
			const auto authority    = workspace_authority();
			const auto materialized = adapter->materialize_agent_workspace(
				{
					.Repository      = repository,
					.Materialization = {
						.ApplicationSession = core::application_session_id{ 1U },
						.Authority          = authority,
						.Session            = {
							.Id      = core::agent_session_id{ 1U },
							.Profile = core::external_terminal_profile_id,
							.Binding = {
								.Host        = authority.Host,
								.Repository  = authority.Repository,
								.PullRequest = authority.PullRequest,
								.BaseOid     = head,
								.HeadOid     = head,
								.LeftEpoch   = core::epoch_ordinal{ 1U },
								.RightEpoch  = core::epoch_ordinal{ 1U },
							},
						},
						.ParentDirectory = workspaceParent.Path,
					},
				},
				{ });
			require(materialized.has_value(), "materialize exact agent workspace");
			const auto destination = std::filesystem::path{
				materialized->Workspace.Path
			};
			const auto displaced   = workspaceParent.Path / "displaced-original";
			const auto replacement = workspaceParent.Path / "replacement-substitute";
			std::filesystem::rename(destination, displaced);
			std::filesystem::create_directories(destination / ".git");
			for (const auto& directory : { destination, destination / ".git" }) {
				std::filesystem::permissions(
					directory,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace);
			}
			write_text(
				destination / ".git" / "bpr-owned-workspace",
				materialized->OwnershipMarker);
			const auto rejected = adapter->cleanup_agent_workspace(*materialized, { });
			require(
				!rejected.has_value()
					&& std::filesystem::is_directory(destination)
					&& std::filesystem::is_directory(displaced),
				"same-path substitute is preserved and original identity is not reauthorized");

			std::filesystem::rename(destination, replacement);
			std::filesystem::rename(displaced, destination);
			const auto cleaned = adapter->cleanup_agent_workspace(*materialized, { });
			require(
				cleaned.has_value() && !std::filesystem::exists(destination)
					&& std::filesystem::is_directory(replacement),
				"restored exact original cleans normally without deleting substitute");
		}

		const auto hookMarker = root.Path / "hook-ran";
		std::filesystem::create_directories(repository / "hooks");
		const auto hook = repository / "hooks" / "reference-transaction";
		write_text(
			hook,
			"#!/bin/sh\n"
			"touch '"
				+ hookMarker.string() + "'\n"
										"exit 91\n");
		std::filesystem::permissions(
			hook,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);

		const application::git_staging_ref stageBase{
			.Operation = operation,
			.Endpoint  = application::git_private_ref_endpoint::base,
		};
		const application::git_staging_ref stageHead{
			.Operation = operation,
			.Endpoint  = application::git_private_ref_endpoint::head,
		};
		const std::array<application::git_ref_mutation, 2U> stage{
			application::git_ref_write{
				.Ref           = stageBase,
				.DesiredObject = commit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			},
			application::git_ref_write{
				.Ref           = stageHead,
				.DesiredObject = commit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			},
		};
		require(
			adapter->update_refs(
					   { .Repository = repository, .Mutations = stage },
					   { })
				.has_value(),
			"atomic staging ref transaction");
		require(
			!std::filesystem::exists(hookMarker),
			"repository reference-transaction hook not invoked");
		require(
			std::filesystem::remove(hook),
			"remove hostile repository hook after isolation proof");

		const auto                       scope = make_scope(0x33U);
		const application::git_epoch_ref finalBase{
			.Scope    = scope,
			.Ordinal  = 1U,
			.Endpoint = application::git_private_ref_endpoint::base,
		};
		const application::git_epoch_ref finalHead{
			.Scope    = scope,
			.Ordinal  = 1U,
			.Endpoint = application::git_private_ref_endpoint::head,
		};
		const std::array<application::git_ref_mutation, 4U> publish{
			application::git_ref_write{
				.Ref           = finalBase,
				.DesiredObject = commit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			},
			application::git_ref_write{
				.Ref           = finalHead,
				.DesiredObject = commit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			},
			application::git_ref_remove{
				.Ref         = stageBase,
				.ExpectedOld = commit,
			},
			application::git_ref_remove{
				.Ref         = stageHead,
				.ExpectedOld = commit,
			},
		};
		require(
			adapter->update_refs(
					   { .Repository = repository, .Mutations = publish },
					   { })
				.has_value(),
			"four-operation exact publication transaction");
		const auto final = adapter->inspect_ref(
			{ .Repository = repository, .Ref = finalHead },
			{ });
		require(
			final.has_value() && *final == commit,
			"exact final ref inspection");
		const auto staging = adapter->inspect_ref(
			{ .Repository = repository, .Ref = stageHead },
			{ });
		require(
			staging.has_value() && !staging->has_value(),
			"exact staging ref removed");

		const auto                         recoveryOperation = make_operation(0x24U);
		const application::git_staging_ref recoveryRef{
			.Operation = recoveryOperation,
			.Endpoint  = application::git_private_ref_endpoint::base,
		};
		const auto recoveryCommitText = run_fixture_git(
			repository,
			{ "commit-tree", std::string{ sha1_tree_hex } },
			"recovery target\n");
		const auto recoveryCommit = object<application::git_commit_id>(
			recoveryCommitText,
			application::git_hash_algorithm::sha1);
		const std::array<application::git_ref_mutation, 1U> recoveryCreate{
			application::git_ref_write{
				.Ref           = recoveryRef,
				.DesiredObject = recoveryCommit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			}
		};
		require(
			adapter
				->update_refs(
					{ .Repository = repository, .Mutations = recoveryCreate },
					{ })
				.has_value(),
			"create recovery ref with an exact target");
		const auto recoveryObject = repository / "objects"
			/ recoveryCommitText.substr(0U, 2U)
			/ recoveryCommitText.substr(2U);
		require(
			std::filesystem::remove(recoveryObject),
			"remove recovery ref target object");
		const auto rawRecoveryTarget = adapter->inspect_ref_target(
			{ .Repository = repository, .Ref = recoveryRef },
			{ });
		const auto verifiedRecoveryTarget = adapter->inspect_ref(
			{ .Repository = repository, .Ref = recoveryRef },
			{ });
		require(
			rawRecoveryTarget.has_value()
				&& *rawRecoveryTarget == recoveryCommit
				&& !verifiedRecoveryTarget.has_value()
				&& verifiedRecoveryTarget.error()
					== application::git_process_failure::object_unavailable,
			"recovery reads a wrong physical target separately from missing-object verification");
		const std::array<application::git_ref_mutation, 1U> recoveryRemove{
			application::git_ref_remove{
				.Ref         = recoveryRef,
				.ExpectedOld = recoveryCommit,
			}
		};
		require(
			adapter
				->update_refs(
					{ .Repository = repository, .Mutations = recoveryRemove },
					{ })
				.has_value(),
			"exact expected target removes missing-object recovery ref");
		const auto repeatedIdentityHex = [](const std::string_view aByte) {
			auto result = std::string{ };
			result.reserve(64U);
			for (auto index = 0U; index < 32U; ++index) {
				result += aByte;
			}
			return result;
		};
		const auto recoveryBaseName = "refs/betterpullreview/v1/stage/"
			+ repeatedIdentityHex("24") + "/base";
		const auto finalHeadName = "refs/betterpullreview/v1/scope/"
			+ repeatedIdentityHex("33")
			+ "/epoch/0000000000000001/head";
		static_cast<void>(run_fixture_git(
			repository,
			{ "symbolic-ref", recoveryBaseName, finalHeadName },
			{ },
			false));
		const auto indirectRecoveryTarget = adapter->inspect_ref_target(
			{ .Repository = repository, .Ref = recoveryRef },
			{ });
		require(
			!indirectRecoveryTarget.has_value()
				&& indirectRecoveryTarget.error()
					== application::git_process_failure::malformed_output,
			"recovery target inspection rejects indirect symbolic refs");
		static_cast<void>(run_fixture_git(
			repository,
			{ "symbolic-ref", "--delete", recoveryBaseName },
			{ },
			false));

		auto wrongCommit = commit;
		wrongCommit.Digest.front() ^= 0x01U;
		const std::array<application::git_ref_mutation, 1U> wrongRemove{
			application::git_ref_remove{
				.Ref         = finalHead,
				.ExpectedOld = wrongCommit,
			}
		};
		require(
			adapter
					->update_refs(
						{
							.Repository = repository,
							.Mutations  = wrongRemove,
						},
						{ })
					.error()
				== application::git_process_failure::nonzero_exit,
			"wrong expected-old ref removal fails closed");

		const auto configuredHookMarker = root.Path / "configured-hook-ran";
		const auto configuredHook       = root.Path / "hooks" / "reference-transaction";
		write_executable_canary(
			configuredHook,
			configuredHookMarker,
			"configured-hook");
		const std::array<application::git_ref_mutation, 1U> validRemove{
			application::git_ref_remove{
				.Ref         = finalHead,
				.ExpectedOld = commit,
			}
		};
		require(
			adapter
					->update_refs(
						{
							.Repository = repository,
							.Mutations  = validRemove,
						},
						{ })
					.error()
				== application::git_process_failure::invalid_configuration,
			"nonempty configured hooks directory fails before Git");
		require(
			!std::filesystem::exists(configuredHookMarker),
			"configured hook has zero invocations");
		require(
			std::filesystem::remove(configuredHook),
			"remove configured hook canary");
		const auto stillRetained = adapter->inspect_ref(
			{ .Repository = repository, .Ref = finalHead },
			{ });
		require(
			stillRetained.has_value() && *stillRetained == commit,
			"rejected configured hook cannot mutate refs");

		const auto configuredTemplate = root.Path / "templates" / "hostile-template";
		write_text(configuredTemplate, "must-not-be-consumed");
		const auto blockedRepository = repository_path(root.Path, '6');
		require(
			adapter
					->initialize_bare_repository(
						{
							.Repository = blockedRepository,
							.Algorithm  = application::git_hash_algorithm::sha1,
						},
						{ })
					.error()
				== application::git_process_failure::invalid_configuration,
			"nonempty configured templates directory fails before Git");
		require(
			!std::filesystem::exists(blockedRepository),
			"rejected template creates no repository");
		require(
			std::filesystem::remove(configuredTemplate),
			"remove configured template canary");

		write_text(
			repository / "config",
			read_text(repository / "config")
				+ "[include]\n\tpath = /tmp/bpr-hostile-config\n");
		require(
			adapter
					->inspect_commit(
						{ .Repository = repository, .Commit = commit },
						{ })
					.error()
				== application::git_process_failure::invalid_repository,
			"hostile repository config rejected before Git");
		adapter.reset();
	}

	auto run_comparison_parser_contract() -> void
	{
		temporary_root root{ "git-comparison-parser" };
		auto           adapter    = make_adapter(make_fake_git_executable(root.Path), root.Path);
		const auto     repository = repository_path(root.Path, '3');
		initialize_repository(*adapter, repository, application::git_hash_algorithm::sha1);
		const auto commit = object<application::git_commit_id>(sha1_commit_hex, application::git_hash_algorithm::sha1);
		for (const auto& output : std::vector<std::string>{ "", "invalid\n", std::string{ sha1_commit_hex },
															std::string{ sha1_commit_hex } + "\n\n", std::string{ sha1_commit_hex } + "\n" + std::string{ sha1_commit_hex } + "\n" }) {
			write_text(root.Path / "comparison-output", output);
			const auto result = adapter->derive_comparison_base(
				{ .Repository = repository, .Head = commit, .SelectedBase = commit }, { });
			require(!result && result.error() == application::git_process_failure::malformed_output,
					"empty, truncated, invalid or duplicate merge-base output cannot claim a candidate");
		}
		write_text(root.Path / "comparison-output", std::string(2048U, 'f'));
		const auto large = adapter->derive_comparison_base(
			{ .Repository = repository, .Head = commit, .SelectedBase = commit }, { });
		require(!large && large.error() == application::git_process_failure::standard_output_limit,
				"merge-base output remains bounded before parsing");
		for (const auto& output : std::vector<std::string>{ "", "1", "1\n2\n", "1x\n", "9223372036854775808\n" }) {
			write_text(root.Path / "committer-output", output);
			const auto result = adapter->read_committer_time_seconds({ .Repository = repository, .Commit = commit }, { });
			require(!result && result.error() == application::git_process_failure::malformed_output,
					"missing, multi-line or overflowing committer dates are unavailable errors");
		}
		write_text(root.Path / "committer-output", std::string(64U, '1'));
		const auto largeTime = adapter->read_committer_time_seconds({ .Repository = repository, .Commit = commit }, { });
		require(!largeTime && largeTime.error() == application::git_process_failure::standard_output_limit,
				"committer timestamp output has a dedicated small bound");
	}

	auto run_comparison_base_contract() -> void
	{
		temporary_root root{ "git-comparison-base" };
		auto           adapter  = make_adapter(std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE }, root.Path);
		const auto     exercise = [&](const application::git_hash_algorithm aAlgorithm, const char aDigit) {
			using status          = application::git_comparison_base_status;
			const auto repository = repository_path(root.Path, aDigit);
			initialize_repository(*adapter, repository, aAlgorithm);
			const auto tree   = run_fixture_git(repository, { "mktree" });
			const auto commit = [&](const std::string& aMessage, const std::vector<std::string>& aParents) {
				auto arguments = std::vector<std::string>{ "commit-tree", tree };
				for (const auto& parent : aParents) {
					arguments.insert(arguments.end(), { "-p", parent });
				}
				return run_fixture_git(repository, arguments, aMessage + "\n");
			};
			const auto initial       = commit("initial", { });
			const auto shared        = commit("shared", { initial });
			const auto observedBase  = commit("observed base", { shared });
			const auto oldHead       = commit("retained old head", { observedBase });
			const auto rewrittenBase = commit("rewritten base", { shared });
			const auto unrelated     = commit("unrelated root", { });
			const auto crossOne      = commit("criss cross one", { observedBase, rewrittenBase });
			const auto crossTwo      = commit("criss cross two", { rewrittenBase, observedBase });
			const auto query         = [&](const std::string& aHead, const std::string& aBase) {
				return adapter->derive_comparison_base({
														   .Repository   = repository,
														   .Head         = object<application::git_commit_id>(aHead, aAlgorithm),
														   .SelectedBase = object<application::git_commit_id>(aBase, aAlgorithm),
													   },
													   { });
			};
			const auto original  = query(oldHead, observedBase);
			const auto rewritten = query(oldHead, rewrittenBase);
			require(original && original->Status == status::unique
						&& original->Base == object<application::git_commit_id>(observedBase, aAlgorithm)
						&& rewritten && rewritten->Status == status::unique
						&& rewritten->Base == object<application::git_commit_id>(shared, aAlgorithm),
					"rewritten base changes derived comparison without altering the exact observed base");
			const auto disconnected = query(oldHead, unrelated);
			require(disconnected && disconnected->Status == status::no_common_ancestor && !disconnected->Base,
					"unrelated rewritten base has no comparison candidate");
			const auto cross = query(crossOne, crossTwo);
			require(cross && cross->Status == status::multiple_merge_bases && !cross->Base,
					"criss-cross ancestry never selects one of multiple best merge bases");
			const auto missingText = std::string(aAlgorithm == application::git_hash_algorithm::sha1 ? 40U : 64U, 'f');
			const auto missing     = query(missingText, observedBase);
			const auto missingBase = query(oldHead, missingText);
			require(missing && missing->Status == status::objects_unavailable && !missing->Base
						&& missingBase && missingBase->Status == status::objects_unavailable && !missingBase->Base,
					"missing exact head or selected base has explicit unavailable ancestry");
			const auto dated = run_fixture_git(repository, { "hash-object", "-t", "commit", "-w", "--stdin" },
											   "tree " + tree + "\nauthor Fixture <fixture@example.invalid> 946684800 +0000\n"
																"committer Fixture <fixture@example.invalid> 946688400 +0130\n\ndifferent committer UTC time\n");
			const auto time  = adapter->read_committer_time_seconds(
				{ .Repository = repository, .Commit = object<application::git_commit_id>(dated, aAlgorithm) }, { });
			require(time && *time == 946688400,
					"committer UTC seconds are distinct from author time and do not apply the local offset twice");
			// [[BPR-1340]]. A sparse acquisition stores a wanted head with its
			// tree but without its parent. Its committer time is in the commit
			// itself, so the missing ancestor must not make it unavailable.
			const auto sparse     = run_fixture_git(repository, { "hash-object", "-t", "commit", "-w", "--stdin" },
													"tree " + tree + "\nparent " + missingText + "\nauthor Fixture <fixture@example.invalid> 946684800 +0000\n"
																								 "committer Fixture <fixture@example.invalid> 946692000 +0000\n\nsparse head without its parent\n");
			const auto sparseTime = adapter->read_committer_time_seconds(
				{ .Repository = repository, .Commit = object<application::git_commit_id>(sparse, aAlgorithm) }, { });
			require(sparseTime && *sparseTime == 946692000,
					"a head stored without its parent still has its committer date");
			const auto missingTime = adapter->read_committer_time_seconds(
				{ .Repository = repository, .Commit = object<application::git_commit_id>(missingText, aAlgorithm) }, { });
			require(missingTime && !*missingTime, "missing commit leaves committer date unavailable");
			auto stop = std::stop_source{ };
			stop.request_stop();
			const auto stopped     = adapter->derive_comparison_base({ .Repository   = repository,
																	   .Head         = object<application::git_commit_id>(oldHead, aAlgorithm),
																	   .SelectedBase = object<application::git_commit_id>(observedBase, aAlgorithm) },
																	 stop.get_token());
			const auto stoppedTime = adapter->read_committer_time_seconds(
				{ .Repository = repository, .Commit = object<application::git_commit_id>(dated, aAlgorithm) }, stop.get_token());
			require(!stopped && stopped.error() == application::git_process_failure::cancelled
						&& !stoppedTime && stoppedTime.error() == application::git_process_failure::cancelled,
					"comparison and committer-time queries honor cancellation");
			write_text(repository / "shallow", observedBase + "\n");
			const auto shallow = query(oldHead, observedBase);
			require(!shallow && shallow.error() == application::git_process_failure::invalid_repository,
					"shallow boundaries cannot masquerade as complete comparison history");
			require(std::filesystem::remove(repository / "shallow"), "remove exact shallow fixture marker");
			const auto initialObject = repository / "objects" / initial.substr(0U, 2U) / initial.substr(2U);
			require(std::filesystem::remove(initialObject), "remove one exact fixture ancestor");
			const auto incomplete = query(oldHead, oldHead);
			require(incomplete && incomplete->Status == status::objects_unavailable && !incomplete->Base,
					"even identical endpoints cannot conceal missing reachable parents");
		};
		exercise(application::git_hash_algorithm::sha1, '1');
		exercise(application::git_hash_algorithm::sha256, '2');
	}

	[[nodiscard]] auto raw_change(std::string_view aModes, std::string_view aLeft, std::string_view aRight, std::string_view aStatus, std::initializer_list<std::string_view> aPaths) -> std::string
	{
		auto raw = std::string{ ":" };
		raw.append(aModes).append(" ").append(aLeft).append(" ").append(aRight).append(" ").append(aStatus).push_back('\0');
		for (const auto path : aPaths) {
			raw.append(path).push_back('\0');
		}
		return raw;
	}
	auto run_changed_catalogue_parser_contract() -> void
	{
		const auto blob   = std::string(40U, 'a');
		const auto raw    = raw_change("100644 100644", blob, blob, "R100", { "old/path", "new/path" });
		const auto parsed = git_adapter::detail::parse_changed_entries(raw, application::git_hash_algorithm::sha1);
		require(parsed && parsed->size() == 1U && parsed->at(0U).Left->Path == "old/path" && parsed->at(0U).Right->Path == "new/path", "raw exact rename parser preserves both paths");
		for (const auto& bad : { raw.substr(0U, raw.size() - 1U), raw_change("100644 100644", blob, std::string(40U, 'b'), "R100", { "old", "new" }), raw_change("000000 100644", blob, blob, "A", { "path" }), raw_change("100644 100644", blob, blob, "M", { "../escape" }) }) {
			require(!git_adapter::detail::parse_changed_entries(bad, application::git_hash_algorithm::sha1), "malformed/truncated/unproven/escaping raw catalogue is refused atomically");
		}
		require(!git_adapter::detail::parse_changed_entries(std::string(application::maximum_git_changed_catalogue_bytes + 1U, 'x'), application::git_hash_algorithm::sha1), "raw catalogue output bound refuses before membership");
	}

	auto run_connectivity_contract() -> void
	{
		temporary_root root{ "git-connectivity" };
		auto           adapter = make_adapter(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
			root.Path);
		const auto exercise =
			[&](const application::git_hash_algorithm aAlgorithm,
				const char                            aRepositoryDigit,
				const std::string_view                aLabel)
			-> connectivity_fixture {
			const auto repository = repository_path(root.Path, aRepositoryDigit);
			initialize_repository(*adapter, repository, aAlgorithm);
			const auto fixture = make_connectivity_fixture(repository, aAlgorithm);
			const auto expect =
				[&](const bool aCondition, const std::string_view aClaim) {
					require(
						aCondition,
						std::string{ aLabel } + ": " + std::string{ aClaim });
				};
			const auto fixtureBlob = object<application::git_blob_id>(
				fixture.Blob, aAlgorithm);
			const auto nestedBlob = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "directory/file.txt",
				},
				{ });
			expect(
				nestedBlob.has_value() && *nestedBlob == std::optional{ fixtureBlob },
				"exact nested path resolves its blob identity");
			const auto executableBlob = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "executable.sh",
				},
				{ });
			const auto symlinkBlob = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "link",
				},
				{ });
			expect(
				executableBlob.has_value()
					&& *executableBlob == std::optional{ fixtureBlob }
					&& symlinkBlob.has_value()
					&& *symlinkBlob == std::optional{ fixtureBlob },
				"regular executable and symlink modes retain blob semantics");
			const auto absentBlob = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "absent.txt",
				},
				{ });
			expect(
				absentBlob.has_value() && !absentBlob->has_value(),
				"absent exact path remains a typed empty result");
			const auto treeEntry = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "directory",
				},
				{ });
			expect(
				!treeEntry.has_value()
					&& treeEntry.error()
						== application::git_process_failure::malformed_output,
				"tree entry is refused instead of being mistaken for a blob");
			const auto parentPath = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "../file.txt",
				},
				{ });
			expect(
				!parentPath.has_value()
					&& parentPath.error()
						== application::git_process_failure::invalid_request,
				"non-normal repository path is refused before Git");
			auto cancelledSource = std::stop_source{ };
			static_cast<void>(cancelledSource.request_stop());
			const auto cancelled = adapter->find_blob_at_path(
				{
					.Repository = repository,
					.Commit     = fixture.Base,
					.Path       = "directory/file.txt",
				},
				cancelledSource.get_token());
			expect(
				!cancelled.has_value()
					&& cancelled.error()
						== application::git_process_failure::cancelled,
				"pre-cancelled lookup never reaches Git");

			const auto chain     = make_rename_chain_fixture(repository, aAlgorithm);
			auto       catalogue = recording_changed_entry_sink{ };
			const auto changed   = adapter->changed_entries({ repository, chain.HeadOne, chain.HeadTwo }, catalogue, { });
			expect(changed.has_value() && catalogue.Entries.size() == 1U && catalogue.Entries[0].Change == 'R'
					   && catalogue.Entries[0].Left && catalogue.Entries[0].Right && catalogue.Entries[0].Left->Path == "b.cpp" && catalogue.Entries[0].Right->Path == "c.cpp"
					   && catalogue.Entries[0].Left->Object == catalogue.Entries[0].Right->Object,
				   "installed Git exact pair emits complete pure-rename catalogue with both exact paths and blob proof");
			catalogue.Entries.clear();
			const auto historical = adapter->changed_entries({ repository, chain.Base, chain.HeadOne }, catalogue, { });
			expect(historical.has_value() && catalogue.Entries.size() == 1U && catalogue.Entries[0].Left->Path == "a.cpp" && catalogue.Entries[0].Right->Path == "b.cpp", "historical exact-tree catalogue differs from today's c.cpp path");
			catalogue.Entries.clear();
			const auto absent = adapter->changed_entries({ repository, object<application::git_commit_id>(std::string(aAlgorithm == application::git_hash_algorithm::sha1 ? 40U : 64U, 'f'), aAlgorithm), chain.HeadTwo }, catalogue, { });
			expect(!absent && catalogue.Entries.empty(), "missing exact historical commit cannot report an empty successful catalogue");
			const auto copySource      = run_fixture_git(repository, { "hash-object", "-w", "--stdin" }, "rename chain\n");
			const auto modifiedSource  = run_fixture_git(repository, { "hash-object", "-w", "--stdin" }, "modified source\n");
			auto       copiedTreeInput = std::string{ "100644 blob " };
			copiedTreeInput.append(modifiedSource).append("\ta.cpp\n100644 blob ").append(copySource).append("\tcopy.cpp\n");
			const auto copiedTree   = run_fixture_git(repository, { "mktree" }, copiedTreeInput);
			const auto copiedCommit = run_fixture_git(repository, { "commit-tree", copiedTree }, "copied\n");
			catalogue.Entries.clear();
			const auto copied = adapter->changed_entries({ repository, chain.Base, object<application::git_commit_id>(copiedCommit, aAlgorithm) }, catalogue, { });
			expect(copied && catalogue.Entries.size() == 2U && std::ranges::any_of(catalogue.Entries, [](const auto& aEntry) { return aEntry.Change == 'C' && aEntry.Left && aEntry.Right && aEntry.Left->Path == "a.cpp" && aEntry.Right->Path == "copy.cpp" && aEntry.Left->Object == aEntry.Right->Object; }), "installed Git copies retain their evidenced source path and exact blob on both sides");
			const auto forward = adapter->find_rename_pairs(
				{
					.Repository   = repository,
					.Left         = chain.HeadOne,
					.Right        = chain.HeadTwo,
					.CurrentPath  = "c.cpp",
					.PreviousPath = "a.cpp",
				},
				{ });
			expect(
				forward.has_value() && forward->Count == 1U
					&& forward->Records[0U]
						== application::git_rename_pair{
							.LeftPath  = "b.cpp",
							.RightPath = "c.cpp",
						},
				"three-commit a-to-b-to-c chain resolves the selected H1-to-H2 rename");
			const auto reverse = adapter->find_rename_pairs(
				{
					.Repository   = repository,
					.Left         = chain.HeadTwo,
					.Right        = chain.HeadOne,
					.CurrentPath  = "c.cpp",
					.PreviousPath = "a.cpp",
				},
				{ });
			expect(
				reverse.has_value() && reverse->Count == 1U
					&& reverse->Records[0U]
						== application::git_rename_pair{
							.LeftPath  = "c.cpp",
							.RightPath = "b.cpp",
						},
				"the same selected rename reverses with the selected commit order");
			const auto unchangedBefore = adapter->find_rename_pairs(
				{
					.Repository   = repository,
					.Left         = chain.Base,
					.Right        = chain.Base,
					.CurrentPath  = "c.cpp",
					.PreviousPath = "a.cpp",
				},
				{ });
			const auto unchangedAfter = adapter->find_rename_pairs(
				{
					.Repository   = repository,
					.Left         = chain.HeadTwo,
					.Right        = chain.HeadTwo,
					.CurrentPath  = "c.cpp",
					.PreviousPath = "a.cpp",
				},
				{ });
			expect(
				unchangedBefore.has_value() && unchangedBefore->Count == 0U
					&& unchangedAfter.has_value() && unchangedAfter->Count == 0U,
				"unchanged selected endpoints produce no invented rename record");
			const auto emptyBlob = object<application::git_blob_id>(
				run_fixture_git(
					repository,
					{ "hash-object", "-w", "--stdin" },
					{ }),
				aAlgorithm);
			auto       emptySink         = recording_blob_sink{ };
			const auto emptyMaterialized = adapter->materialize_blob(
				{
					.Repository   = repository,
					.Blob         = emptyBlob,
					.MaximumBytes = 1U,
				},
				emptySink,
				{ });
			expect(
				emptyMaterialized.has_value()
					&& emptySink.Blob == emptyBlob
					&& emptySink.Bytes.empty(),
				"empty blob materialization returns an exact empty byte vector");
			auto       materializedSink = recording_blob_sink{ };
			const auto materialized     = adapter->materialize_blob(
				{
					.Repository   = repository,
					.Blob         = fixtureBlob,
					.MaximumBytes = application::maximum_git_blob_materialization_bytes,
				},
				materializedSink,
				{ });
			const auto fixtureBytes = std::string_view{ "connected fixture blob\n" };
			expect(
				materialized.has_value() && materializedSink.Blob == fixtureBlob
					&& std::ranges::equal(materializedSink.Bytes, fixtureBytes),
				"exact blob materialization returns byte-for-byte content");
			auto       limitedSink = recording_blob_sink{ };
			const auto limited     = adapter->materialize_blob(
				{
					.Repository   = repository,
					.Blob         = fixtureBlob,
					.MaximumBytes = fixtureBytes.size() - 1U,
				},
				limitedSink,
				{ });
			expect(
				!limited.has_value()
					&& limited.error()
						== application::git_process_failure::standard_output_limit
					&& !limitedSink.Blob.has_value(),
				"declared oversize blob is rejected before content retention");
			auto       wrongTypeSink = recording_blob_sink{ };
			const auto wrongType     = adapter->materialize_blob(
				{
					.Repository = repository,
					.Blob       = application::git_blob_id{
						.Algorithm = fixture.Base.Algorithm,
						.Digest    = fixture.Base.Digest,
					},
					.MaximumBytes = 1024U,
				},
				wrongTypeSink, { });
			expect(
				!wrongType.has_value()
					&& wrongType.error()
						== application::git_process_failure::malformed_output
					&& !wrongTypeSink.Blob.has_value(),
				"non-blob object identity fails typed materialization closed");
			const auto verify =
				[&] {
					return adapter->verify_connectivity(
						{
							.Repository = repository,
							.Base       = fixture.Base,
							.Head       = fixture.Head,
						},
						{ });
				};
			expect(
				verify().has_value(),
				"base/head graph is completely connected");

			const auto missingIdentity = std::string(
				aAlgorithm == application::git_hash_algorithm::sha1
					? 40U
					: 64U,
				'f');
			const auto unavailableAncestor = adapter->is_ancestor(
				{
					.Repository = repository,
					.Ancestor   = object<application::git_commit_id>(
						missingIdentity,
						aAlgorithm),
					.Descendant = fixture.Head,
				},
				{ });
			expect(
				!unavailableAncestor.has_value()
					&& unavailableAncestor.error()
						== application::git_process_failure::
							object_unavailable,
				"missing exact ancestor is distinguished from other process failures");

			expect(
				std::filesystem::remove(
					loose_object_path(
						repository,
						fixture.MissingParent)),
				"remove exact parent object fixture");
			const auto missingParent = verify();
			expect(
				!missingParent.has_value()
					&& missingParent.error()
						== application::git_process_failure::nonzero_exit,
				"missing head parent fails complete connectivity");
			// fsck reports the closure on standard output; the detail counts
			// it without naming the object (BPR-0852).
			expect(
				!missingParent.has_value()
					&& missingParent.error().Detail.find("1 missing commit") != std::string::npos
					&& missingParent.error().Detail.find(fixture.MissingParent) == std::string::npos,
				"a connectivity refusal names the missing object's kind, not its name");
			expect(
				run_fixture_git(
					repository,
					{ "commit-tree", fixture.RootTree },
					"missing parent\n")
					== fixture.MissingParent,
				"restore exact parent object fixture");
			expect(
				verify().has_value(),
				"restored parent restores complete connectivity");

			expect(
				std::filesystem::remove(
					loose_object_path(
						repository,
						fixture.NestedTree)),
				"remove exact nested-tree object fixture");
			const auto missingTree = verify();
			expect(
				!missingTree.has_value()
					&& missingTree.error()
						== application::git_process_failure::nonzero_exit,
				"missing nested tree fails complete connectivity");
			expect(
				run_fixture_git(
					repository,
					{ "mktree" },
					"100644 blob " + fixture.Blob + "\tfile.txt\n")
					== fixture.NestedTree,
				"restore exact nested-tree object fixture");
			expect(
				verify().has_value(),
				"restored nested tree restores complete connectivity");

			expect(
				std::filesystem::remove(
					loose_object_path(repository, fixture.Blob)),
				"remove exact blob object fixture");
			const auto missingBlob = verify();
			expect(
				!missingBlob.has_value()
					&& missingBlob.error()
						== application::git_process_failure::nonzero_exit,
				"missing nested blob fails complete connectivity");
			expect(
				run_fixture_git(
					repository,
					{ "hash-object", "-w", "--stdin" },
					"connected fixture blob\n")
					== fixture.Blob,
				"restore exact blob object fixture");
			expect(
				verify().has_value(),
				"restored blob restores complete connectivity");

			expect(
				std::filesystem::remove(
					loose_object_path(repository, fixture.RootTree)),
				"remove exact commit tree fixture");
			const auto corruptCommit = adapter->inspect_commit(
				{
					.Repository = repository,
					.Commit     = fixture.Head,
				},
				{ });
			expect(
				!corruptCommit.has_value()
					&& corruptCommit.error()
						== application::git_process_failure::nonzero_exit,
				"missing tree is not weakened into object-unavailable ancestry");
			expect(
				run_fixture_git(
					repository,
					{ "mktree" },
					"040000 tree " + fixture.NestedTree
						+ "\tdirectory\n"
						+ "100755 blob " + fixture.Blob
						+ "\texecutable.sh\n"
						+ "120000 blob " + fixture.Blob + "\tlink\n")
					== fixture.RootTree,
				"restore exact commit tree fixture");
			expect(
				verify().has_value(),
				"restored commit tree restores complete connectivity");
			return fixture;
		};

		static_cast<void>(
			exercise(
				application::git_hash_algorithm::sha1,
				'7',
				"SHA-1"));
		const auto sha256 = exercise(
			application::git_hash_algorithm::sha256,
			'8',
			"SHA-256");

		auto mismatchedHead      = sha256.Head;
		mismatchedHead.Algorithm = application::git_hash_algorithm::sha1;
		const auto mismatched    = adapter->verify_connectivity(
			{
				.Repository = repository_path(root.Path, '8'),
				.Base       = sha256.Base,
				.Head       = mismatchedHead,
			},
			{ });
		require(
			!mismatched.has_value()
				&& mismatched.error()
					== application::git_process_failure::invalid_request,
			"mixed-algorithm connectivity request fails before Git");
	}

	auto run_rename_parser_contract() -> void
	{
		temporary_root root{ "git-rename-parser" };
		const auto     fakeGitPath = make_fake_git_executable(root.Path);
		auto           adapter     = make_adapter(fakeGitPath, root.Path);
		const auto     repository  = repository_path(root.Path, '7');
		initialize_repository(
			*adapter,
			repository,
			application::git_hash_algorithm::sha1);
		const auto commit = object<application::git_commit_id>(
			sha1_commit_hex,
			application::git_hash_algorithm::sha1);
		const auto request = application::git_rename_pair_request{
			.Repository   = repository,
			.Left         = commit,
			.Right        = commit,
			.CurrentPath  = "c.cpp",
			.PreviousPath = "a.cpp",
		};
		const auto record = [](const std::string_view aStatus,
							   const std::string_view aLeft,
							   const std::string_view aRight) {
			auto result = std::string{ aStatus };
			result.push_back('\0');
			result.append(aLeft);
			result.push_back('\0');
			result.append(aRight);
			result.push_back('\0');
			return result;
		};
		const auto output = root.Path / "rename-output";

		write_text(output, record("R100", "b.cpp", "c.cpp"));
		const auto parsed = adapter->find_rename_pairs(request, { });
		require(
			parsed.has_value() && parsed->Count == 1U
				&& parsed->Records[0U]
					== application::git_rename_pair{
						.LeftPath  = "b.cpp",
						.RightPath = "c.cpp",
					},
			"strict rename parser preserves one causally bound NUL record");

		write_text(
			output,
			record("R100", "b.cpp", "c.cpp")
				+ record("R075", "a.cpp", "d.cpp"));
		const auto multiple = adapter->find_rename_pairs(request, { });
		require(
			multiple.has_value() && multiple->Count == 2U,
			"strict parser retains bounded multiple records for host ambiguity refusal");

		auto malformed = record("R100", "b.cpp", "c.cpp");
		malformed.pop_back();
		write_text(output, malformed);
		require(
			adapter->find_rename_pairs(request, { }).error()
				== application::git_process_failure::malformed_output,
			"rename parser rejects a truncated NUL record");
		write_text(output, record("R049", "a.cpp", "b.cpp"));
		require(
			adapter->find_rename_pairs(request, { }).error()
				== application::git_process_failure::malformed_output,
			"rename parser rejects a score outside the fixed threshold");
		write_text(output, record("R100", "x.cpp", "y.cpp"));
		const auto unbound = adapter->find_rename_pairs(request, { });
		require(
			unbound.has_value() && unbound->Count == 0U,
			"rename parser filters a record unbound to accepted row candidates");

		auto unrelated = std::string{ };
		for (auto index = std::size_t{ 0U };
			 index <= application::maximum_git_rename_pair_records;
			 ++index) {
			unrelated += record(
				"R100",
				"unrelated-left-" + std::to_string(index) + ".cpp",
				"unrelated-right-" + std::to_string(index) + ".cpp");
		}
		write_text(output, unrelated + record("R100", "b.cpp", "c.cpp"));
		const auto filtered = adapter->find_rename_pairs(request, { });
		require(
			filtered.has_value() && filtered->Count == 1U
				&& filtered->Records[0U]
					== application::git_rename_pair{
						.LeftPath  = "b.cpp",
						.RightPath = "c.cpp",
					},
			"raw scan filters unrelated renames before applying the retained bound");

		auto overBound = std::string{ };
		for (auto index = std::size_t{ 0U };
			 index <= application::maximum_git_rename_pair_records;
			 ++index) {
			overBound += record("R100", "a.cpp", "c.cpp");
		}
		write_text(output, overBound);
		require(
			adapter->find_rename_pairs(request, { }).error()
				== application::git_process_failure::standard_output_limit,
			"rename parser refuses more than the fixed retained record bound");

		auto overScanBound = std::string{ };
		for (auto index = std::size_t{ 0U };
			 index <= application::maximum_git_rename_scan_records;
			 ++index) {
			overScanBound += record(
				"R100",
				"scan-left-" + std::to_string(index) + ".cpp",
				"scan-right-" + std::to_string(index) + ".cpp");
		}
		write_text(output, overScanBound);
		require(
			adapter->find_rename_pairs(request, { }).error()
				== application::git_process_failure::standard_output_limit,
			"rename parser refuses more than the fixed raw scan bound");

		auto cancelled = std::stop_source{ };
		static_cast<void>(cancelled.request_stop());
		require(
			adapter->find_rename_pairs(request, cancelled.get_token()).error()
				== application::git_process_failure::cancelled,
			"pre-cancelled rename discovery never enters Git");
		auto invalidPath        = request;
		invalidPath.CurrentPath = "../c.cpp";
		require(
			adapter->find_rename_pairs(invalidPath, { }).error()
				== application::git_process_failure::invalid_request,
			"rename discovery refuses non-normal accepted paths before Git");

		const auto log = read_text(root.Path / "invocations.log");
		require(
			log.find("arg=diff-tree\n") != std::string::npos
				&& log.find("arg=-z\n") != std::string::npos
				&& log.find("arg=--name-status\n") != std::string::npos
				&& log.find("arg=--diff-filter=R\n") != std::string::npos
				&& log.find("arg=--find-renames=50%\n") != std::string::npos
				&& log.find("arg=--follow\n") == std::string::npos,
			"rename discovery uses the fixed local-only command and never follows history");
	}

	auto run_real_hostile_canary_contract() -> void
	{
#if defined(Q_OS_UNIX)
		temporary_root root{ "git-real-hostile" };
		const auto     marker      = root.Path / "repository-executable-ran";
		const auto     traceMarker = root.Path / "ambient-trace-ran";
		const auto     canary      = root.Path / "hostile-command";
		write_executable_canary(canary, marker, "ambient-command");

		const auto hostileHooks         = root.Path / "ambient-hooks";
		const auto hostileTemplates     = root.Path / "ambient-templates";
		const auto hostileTemplateHooks = hostileTemplates / "hooks";
		const auto hostileHome          = root.Path / "ambient-home";
		const auto hostileXdgGit        = root.Path / "ambient-xdg" / "git";
		const auto hostileBin           = root.Path / "ambient-bin";
		for (const auto& directory : {
				 hostileHooks,
				 hostileTemplateHooks,
				 hostileHome,
				 hostileXdgGit,
				 hostileBin }) {
			std::filesystem::create_directories(directory);
			std::filesystem::permissions(
				directory,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
		}
		write_executable_canary(
			hostileHooks / "reference-transaction",
			marker,
			"ambient-hook");
		write_executable_canary(
			hostileTemplateHooks / "reference-transaction",
			marker,
			"ambient-template-hook");
		for (const auto* name : {
				 "git-lfs",
				 "ssh",
				 "editor",
				 "pager" }) {
			const auto executable = hostileBin / name;
			std::filesystem::copy_file(canary, executable);
			std::filesystem::permissions(
				executable,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
		}

		const auto hostileAttributes = root.Path / "ambient-attributes";
		write_text(
			hostileAttributes,
			"*.txt filter=lfs diff=bpr\n");
		const auto hostileConfigText = "[core]\n"
									   "\thooksPath = "
			+ hostileHooks.string() + "\n"
									  "\tfsmonitor = !"
			+ canary.string() + " fsmonitor\n"
								"\teditor = "
			+ canary.string() + " editor\n"
								"\tpager = "
			+ canary.string() + " pager\n"
								"\tattributesFile = "
			+ hostileAttributes.string() + "\n"
										   "[credential]\n"
										   "\thelper = !"
			+ canary.string() + " credential\n"
								"[filter \"lfs\"]\n"
								"\tprocess = "
			+ canary.string() + " filter-process\n"
								"\tsmudge = "
			+ canary.string() + " smudge\n"
								"\tclean = "
			+ canary.string() + " clean\n"
								"\trequired = true\n"
								"[diff]\n"
								"\texternal = "
			+ canary.string() + " external-diff\n"
								"[diff \"bpr\"]\n"
								"\ttextconv = "
			+ canary.string() + " textconv\n"
								"[submodule]\n"
								"\trecurse = true\n"
								"[protocol]\n"
								"\tallow = always\n"
								"[protocol \"file\"]\n"
								"\tallow = always\n"
								"[protocol \"ext\"]\n"
								"\tallow = always\n"
								"[protocol \"ssh\"]\n"
								"\tallow = always\n"
								"[pager]\n"
								"\tshow = "
			+ canary.string() + " pager-show\n"
								"[maintenance]\n"
								"\tauto = true\n";
		const auto systemConfig = root.Path / "ambient-system-config";
		const auto globalConfig = root.Path / "ambient-global-config";
		write_text(systemConfig, hostileConfigText);
		write_text(globalConfig, hostileConfigText);
		write_text(hostileHome / ".gitconfig", hostileConfigText);
		write_text(hostileXdgGit / "config", hostileConfigText);

		scoped_environment environment;
		environment.set("HOME", hostileHome);
		environment.set(
			"XDG_CONFIG_HOME",
			root.Path / "ambient-xdg");
		environment.set("GIT_CONFIG_SYSTEM", systemConfig);
		environment.set("GIT_CONFIG_GLOBAL", globalConfig);
		environment.set("GIT_ATTR_SYSTEM", hostileAttributes);
		environment.set("GIT_TEMPLATE_DIR", hostileTemplates);
		environment.set("GIT_ASKPASS", canary);
		environment.set("SSH_ASKPASS", canary);
		environment.set(
			"SSH_ASKPASS_REQUIRE",
			QByteArray{ "force" });
		environment.set("GIT_SSH", canary);
		environment.set("GIT_SSH_COMMAND", canary);
		environment.set("GIT_PROXY_COMMAND", canary);
		environment.set("GIT_EDITOR", canary);
		environment.set("GIT_SEQUENCE_EDITOR", canary);
		environment.set("GIT_PAGER", canary);
		environment.set("PAGER", canary);
		environment.set("GIT_EXTERNAL_DIFF", canary);
		environment.set("GIT_TRACE", traceMarker);
		environment.set("GIT_TRACE2", traceMarker);
		environment.set("GIT_TRACE2_EVENT", traceMarker);
		environment.set("GIT_TRACE2_PERF", traceMarker);
		environment.set(
			"GIT_CONFIG_COUNT",
			QByteArray{ "1" });
		environment.set(
			"GIT_CONFIG_KEY_0",
			QByteArray{ "core.hooksPath" });
		environment.set(
			"GIT_CONFIG_VALUE_0",
			hostileHooks);
		environment.set(
			"GIT_ALTERNATE_OBJECT_DIRECTORIES",
			root.Path / "ambient-alternates");
		environment.set(
			"GIT_OBJECT_DIRECTORY",
			root.Path / "ambient-objects");
		environment.set(
			"GIT_REPLACE_REF_BASE",
			QByteArray{ "refs/ambient-replace" });
		environment.set(
			"GIT_ALLOW_PROTOCOL",
			QByteArray{ "file:ext:ssh" });
		environment.set(
			"GIT_PROTOCOL_FROM_USER",
			QByteArray{ "1" });
		environment.set(
			"GIT_TERMINAL_PROMPT",
			QByteArray{ "1" });
		environment.set(
			"PATH",
			hostileBin);

		auto adapter = make_adapter(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
			root.Path);
		const auto repository = repository_path(root.Path, '5');
		initialize_repository(
			*adapter,
			repository,
			application::git_hash_algorithm::sha1);
		require(
			!std::filesystem::exists(
				repository / "hooks" / "reference-transaction"),
			"ambient template hooks are not copied");

		const auto operation = make_operation(0x55U);
		write_spool(
			*adapter,
			operation,
			std::as_bytes(std::span{ sha1_pack }));
		const auto pack = object<application::git_pack_id>(
			sha1_pack_hex,
			application::git_hash_algorithm::sha1);
		require(
			adapter
				->index_pack(
					{
						.Repository        = repository,
						.Operation         = operation,
						.SpoolBytes        = sha1_pack.size(),
						.MaximumInputBytes = sha1_pack.size(),
						.ExpectedPack      = pack,
					},
					{ })
				.has_value(),
			"real Git imports under the hostile parent environment");
		const auto commit = object<application::git_commit_id>(
			sha1_commit_hex,
			application::git_hash_algorithm::sha1);
		require(
			adapter
				->inspect_commit(
					{ .Repository = repository, .Commit = commit },
					{ })
				.has_value(),
			"real object inspection ignores ambient execution policy");
		require(
			adapter
					->is_ancestor(
						{
							.Repository = repository,
							.Ancestor   = commit,
							.Descendant = commit,
						},
						{ })
				== application::git_ancestry::ancestor,
			"real ancestry ignores ambient execution policy");

		std::filesystem::create_directories(repository / "hooks");
		std::filesystem::create_directories(repository / "info");
		write_executable_canary(
			repository / "hooks" / "reference-transaction",
			marker,
			"repository-hook");
		write_text(
			repository / "info" / "attributes",
			"*.txt filter=lfs diff=bpr\n");
		const application::git_staging_ref ref{
			.Operation = operation,
			.Endpoint  = application::git_private_ref_endpoint::base,
		};
		const std::array<application::git_ref_mutation, 1U> create{
			application::git_ref_write{
				.Ref           = ref,
				.DesiredObject = commit,
				.ExpectedOld   = application::git_ref_expected_absent{ },
			}
		};
		require(
			adapter
				->update_refs(
					{ .Repository = repository, .Mutations = create },
					{ })
				.has_value(),
			"real ref transaction ignores repository executables");
		require(
			!std::filesystem::exists(marker)
				&& !std::filesystem::exists(traceMarker),
			"ambient and repository execution canaries have zero invocations");

		const auto alternates = repository / "objects" / "info" / "alternates";
		write_text(
			alternates,
			(root.Path / "ambient-alternates").string() + "\n");
		require(
			adapter
					->inspect_commit(
						{ .Repository = repository, .Commit = commit },
						{ })
					.error()
				== application::git_process_failure::invalid_repository,
			"alternate object store is rejected before Git");
		require(
			std::filesystem::remove(alternates),
			"remove exact temporary alternates canary");

		const auto replaceRoot = repository / "refs" / "replace";
		std::filesystem::create_directories(replaceRoot);
		write_text(replaceRoot / std::string{ sha1_commit_hex }, sha1_commit_hex);
		require(
			adapter
					->inspect_commit(
						{ .Repository = repository, .Commit = commit },
						{ })
					.error()
				== application::git_process_failure::invalid_repository,
			"replace objects are rejected before Git");
		std::error_code cleanupError;
		std::filesystem::remove_all(replaceRoot, cleanupError);
		require(
			!cleanupError,
			"remove exact temporary replace-ref canary");
		require(
			!std::filesystem::exists(marker)
				&& !std::filesystem::exists(traceMarker),
			"rejected repository canaries launch no process");
#endif
	}

	auto fake_index_request(
		application::git_process_port&             aDapter,
		const std::filesystem::path&               aRepository,
		const application::git_operation_identity& aOperation,
		const std::string_view                     aInput)
		-> application::git_process_result<
			application::git_index_pack_success>
	{
		write_spool(
			aDapter,
			aOperation,
			std::as_bytes(std::span{ aInput }));
		return aDapter.index_pack(
			{
				.Repository        = aRepository,
				.Operation         = aOperation,
				.SpoolBytes        = aInput.size(),
				.MaximumInputBytes = aInput.size(),
				.ExpectedPack      = object<application::git_pack_id>(
					sha1_pack_hex,
					application::git_hash_algorithm::sha1),
			},
			{ });
	}

	auto run_hostile_process_contract() -> void
	{
		temporary_root root{ "git-hostile" };
		// The shared helper, not an inline copy: on Windows the fake's Qt has
		// to travel beside it (BPR-0437).
		const auto fakeGitPath = make_fake_git_executable(root.Path);

		const auto hostileConfig = root.Path / "hostile-global-config";
		write_text(
			hostileConfig,
			"[credential]\n\thelper = !false\n"
			"[core]\n\thooksPath = /tmp/bpr-hostile-hooks\n");
		scoped_environment environment;
		environment.set(
			"BPR_PARENT_SECRET",
			QByteArray{ "synthetic-parent-secret" });
		environment.set(
			"GIT_CONFIG_GLOBAL",
			QByteArray::fromStdString(hostileConfig.string()));
		environment.set(
			"GIT_CONFIG_SYSTEM",
			QByteArray{ "/tmp/bpr-hostile-system-config" });
		environment.set(
			"GIT_ATTR_SYSTEM",
			QByteArray{ "/tmp/bpr-hostile-system-attributes" });
		environment.set(
			"GIT_TEMPLATE_DIR",
			QByteArray{ "/tmp/bpr-hostile-templates" });
		environment.set(
			"GIT_ASKPASS",
			QByteArray{ "/tmp/bpr-hostile-askpass" });
		environment.set(
			"SSH_ASKPASS",
			QByteArray{ "/tmp/bpr-hostile-ssh-askpass" });
		environment.set(
			"GIT_SSH_COMMAND",
			QByteArray{ "/tmp/bpr-hostile-ssh" });
		environment.set(
			"GIT_EDITOR",
			QByteArray{ "/tmp/bpr-hostile-editor" });
		environment.set(
			"GIT_PAGER",
			QByteArray{ "/tmp/bpr-hostile-pager" });
		environment.set(
			"GIT_EXTERNAL_DIFF",
			QByteArray{ "/tmp/bpr-hostile-diff" });
		environment.set("GIT_TRACE", QByteArray{ "1" });
		environment.set("GIT_TRACE2", QByteArray{ "1" });
		environment.set(
			"GIT_ALTERNATE_OBJECT_DIRECTORIES",
			QByteArray{ "/tmp/bpr-hostile-alternates" });
		environment.set(
			"GIT_REPLACE_REF_BASE",
			QByteArray{ "refs/bpr-hostile-replace" });
		environment.set(
			"GIT_ALLOW_PROTOCOL",
			QByteArray{ "file:ext:ssh" });
		environment.set(
			"HTTPS_PROXY",
			QByteArray{ "http://synthetic.invalid" });

		auto       adapter    = make_adapter(fakeGitPath, root.Path);
		const auto repository = repository_path(root.Path, '4');
		initialize_repository(
			*adapter,
			repository,
			application::git_hash_algorithm::sha1);
		const auto operation = make_operation(0x44U);

		constexpr std::size_t slowSize = 2U * 1024U * 1024U;
		auto                  slow     = std::string{
			"BPR_SLOW_INPUT:" + std::to_string(slowSize) + "\n"
		};
		slow.resize(slowSize, 'z');
		const auto slowResult = fake_index_request(
			*adapter,
			repository,
			operation,
			slow);
		require(
			slowResult.has_value(),
			"large backpressured input arrives without dropped partial writes");
		const auto reverseIndex = repository / "objects" / "pack"
			/ ("pack-" + std::string{ sha1_pack_hex } + ".rev");
		write_text(reverseIndex, "hostile-reverse-index");
		require(
			fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_REV_PRESENT")
					.error()
				== application::git_process_failure::invalid_repository,
			"unexpected reverse index fails exact on-disk validation");
		require(
			std::filesystem::remove(reverseIndex),
			"remove exact temporary reverse-index canary");
		require(
			fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_BIG_OUTPUT")
					.error()
				== application::git_process_failure::standard_output_limit,
			"standard output limit");
		const auto bigError = fake_index_request(
			*adapter,
			repository,
			operation,
			"BPR_BIG_ERROR");
		require(
			!bigError.has_value()
				&& bigError.error()
					== application::git_process_failure::standard_error_limit
				&& bigError.error().Detail.empty(),
			"standard error limit remains textless rather than retaining partial stderr");
		require(
			fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_MALFORMED")
					.error()
				== application::git_process_failure::malformed_output,
			"exact pack output grammar");

		write_spool(
			*adapter,
			operation,
			std::as_bytes(std::span{ std::string_view{ "BPR_BLOCK" } }));
		// [[BPR-1121]]. Until the calls below are joined, unwinding would destroy a
		// joinable thread, and they wait on a cancellation that comes after the
		// checks in between; so a failure in this window exits, as it always did.
		auto untilJoined = bpr::tests::exit_on_failure{ };

		std::stop_source cancellation;
		std::optional<
			application::git_process_result<
				application::git_index_pack_success>>
					cancelled;
		std::thread call{
			[&] {
				cancelled = adapter->index_pack(
					{
						.Repository        = repository,
						.Operation         = operation,
						.SpoolBytes        = 9U,
						.MaximumInputBytes = 9U,
						.ExpectedPack      = object<application::git_pack_id>(
							sha1_pack_hex,
							application::git_hash_algorithm::sha1),
					},
					cancellation.get_token());
			}
		};
		std::this_thread::sleep_for(std::chrono::milliseconds{ 150 });

#if defined(Q_OS_UNIX)
		const auto swappedOperation = make_operation(0x66U);
		write_spool(
			*adapter,
			swappedOperation,
			std::as_bytes(std::span{ sha1_pack }));
		const auto swappedSpool = adapter->pack_spool_path(swappedOperation);
		require(
			swappedSpool.has_value(),
			"derive queued spool swap path");
		const auto swappedTarget = root.Path / "swapped-pack-target";
		write_bytes(
			swappedTarget,
			std::as_bytes(std::span{ sha1_pack }));
		std::optional<
			application::git_process_result<
				application::git_index_pack_success>>
					swapped;
		std::thread swappedCall{
			[&] {
				swapped = adapter->index_pack(
					{
						.Repository        = repository,
						.Operation         = swappedOperation,
						.SpoolBytes        = sha1_pack.size(),
						.MaximumInputBytes = sha1_pack.size(),
						.ExpectedPack      = object<application::git_pack_id>(
							sha1_pack_hex,
							application::git_hash_algorithm::sha1),
					},
					{ });
			}
		};
		std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
		require(
			std::filesystem::remove(*swappedSpool),
			"remove validated queued spool");
		std::filesystem::create_symlink(
			swappedTarget,
			*swappedSpool);
#endif

		cancellation.request_stop();
		call.join();
#if defined(Q_OS_UNIX)
		swappedCall.join();
#endif
		untilJoined.end();
		require(
			cancelled.has_value()
				&& !cancelled->has_value()
				&& cancelled->error()
					== application::git_process_failure::cancelled,
			"external stop token cancels blocked Git");
#if defined(Q_OS_UNIX)
		require(
			swapped.has_value() && !swapped->has_value()
				&& swapped->error()
					== application::git_process_failure::standard_input,
			"queued spool swap is rejected by no-follow handle validation");
#endif
		std::this_thread::sleep_for(std::chrono::milliseconds{ 1800 });
		require(
			!std::filesystem::exists(root.Path / "descendant-survived"),
			"cancel kills descendant process group");

		adapter.reset();
		const auto log = read_text(root.Path / "invocations.log");
		for (const auto forbidden : {
				 "synthetic-parent-secret",
				 "BPR_PARENT_SECRET",
				 "GIT_ASKPASS",
				 "SSH_ASKPASS",
				 "GIT_SSH_COMMAND",
				 "GIT_EDITOR",
				 "GIT_PAGER",
				 "GIT_EXTERNAL_DIFF",
				 "GIT_TRACE",
				 "GIT_TRACE2",
				 "GIT_ALTERNATE_OBJECT_DIRECTORIES",
				 "GIT_REPLACE_REF_BASE",
				 "GIT_ALLOW_PROTOCOL",
				 "HTTPS_PROXY",
				 "bpr-hostile-system-config",
				 "bpr-hostile-system-attributes",
				 "bpr-hostile-templates" }) {
			require(
				log.find(forbidden) == std::string::npos,
				"ambient process/config/execution input omitted");
		}
		require(
			log.find("env=GIT_CONFIG_NOSYSTEM=1") != std::string::npos
				&& log.find("env=GIT_TERMINAL_PROMPT=0")
					!= std::string::npos
				&& log.find(
					   "env=GIT_CONFIG_SYSTEM="
					   + (root.Path / "config-system").string())
					!= std::string::npos
				&& log.find(
					   "env=GIT_CONFIG_GLOBAL="
					   + (root.Path / "config-global").string())
					!= std::string::npos
				&& log.find(
					   "env=GIT_TEMPLATE_DIR="
					   + (root.Path / "templates").string())
					!= std::string::npos
				&& log.find(
					   "env=HOME="
					   + (root.Path / "home").string())
					!= std::string::npos
				&& log.find(
					   "env=XDG_CONFIG_HOME="
					   + (root.Path / "xdg").string())
					!= std::string::npos
				&& log.find(
					   "cwd="
					   + (root.Path / "work").string())
					!= std::string::npos,
			"minimal isolated environment and non-repository cwd");
		require(
			log.find("arg=fetch-pack\n") == std::string::npos
				&& log.find("arg=upload-pack\n") == std::string::npos
				&& log.find("arg=clone\n") == std::string::npos
				&& log.find("arg=checkout\n") == std::string::npos
				&& log.find("arg=diff\n") == std::string::npos
				&& log.find("arg=submodule\n") == std::string::npos
				&& log.find("arg=lfs\n") == std::string::npos
				&& log.find("arg=maintenance\n") == std::string::npos
				&& log.find("arg=gc\n") == std::string::npos
				&& log.find("http://") == std::string::npos
				&& log.find("https://") == std::string::npos,
			"no network, worktree, filter, submodule, diff, or maintenance verb reaches Git");
		require(
			log.find(
				"arg=-c\narg=core.fsync=all\narg=-c\n"
				"arg=core.fsyncMethod=fsync")
				!= std::string::npos,
			"durability options have canonical order");
	}

	auto run_capability_probe_cleanup_contract() -> void
	{
		const auto entryCount =
			[](const std::filesystem::path& aDirectory) {
				return static_cast<std::size_t>(
					std::distance(
						std::filesystem::directory_iterator{ aDirectory },
						std::filesystem::directory_iterator{ }));
			};
		const auto runFailure =
			[](const std::string_view aLabel,
			   const std::string_view aMarker) {
				temporary_root root{ aLabel };
				const auto     fakeGitPath = make_fake_git_executable(root.Path);
				write_text(root.Path / aMarker, "induced probe failure");

				const auto adapter = git_adapter::make_installed_git_process({
					.GitExecutable     = fakeGitPath,
					.IsolationRoot     = root.Path,
					.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
				});
				require(
					!adapter.has_value()
						&& adapter.error()
							== application::git_process_failure::
								unsupported_git,
					"induced capability-probe failure is rejected");
				require(
					std::filesystem::is_empty(
						root.Path / "repositories" / "v1")
						&& std::filesystem::is_empty(
							root.Path / "incoming" / "v1"),
					"failed capability probe leaves no repository or spool residue");
			};

		runFailure(
			"git-probe-init-cleanup",
			"fail-capability-probe-after-init");
		runFailure(
			"git-probe-index-cleanup",
			"fail-capability-probe-index-pack");

		{
			temporary_root root{ "git-probe-success-cleanup" };
			const auto     fakeGitPath = make_fake_git_executable(root.Path);
			auto           adapter     = make_adapter(fakeGitPath, root.Path);
			require(
				std::filesystem::is_empty(
					root.Path / "repositories" / "v1")
					&& std::filesystem::is_empty(
						root.Path / "incoming" / "v1"),
				"successful capability probes leave no repository or spool residue");
			adapter.reset();
		}

		{
			temporary_root root{ "git-probe-collision" };
			const auto     fakeGitPath  = make_fake_git_executable(root.Path);
			const auto     repositories = root.Path / "repositories" / "v1";
			const auto     incoming     = root.Path / "incoming" / "v1";
			std::filesystem::create_directories(repositories);
			std::filesystem::create_directories(incoming);
			for (const auto& directory : {
					 root.Path / "repositories",
					 repositories,
					 root.Path / "incoming",
					 incoming }) {
				std::filesystem::permissions(
					directory,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace);
			}
			std::string collisionIdentity;
			collisionIdentity.reserve(64U);
			for (std::size_t index = 0U; index < 32U; ++index) {
				collisionIdentity += "0c";
			}
			const auto collisionRepository = repositories / (collisionIdentity + ".git");
			const auto collisionSpool      = incoming / (collisionIdentity + ".pack.part");
			std::filesystem::create_directory(collisionRepository);
			std::filesystem::permissions(
				collisionRepository,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			write_text(
				collisionRepository / "collision-canary",
				"repository-canary");
			write_text(collisionSpool, "spool-canary");
			write_text(
				root.Path / "inject-probe-collision",
				"first candidate only");

			auto adapter = make_adapter(fakeGitPath, root.Path);
			require(
				read_text(
					collisionRepository / "collision-canary")
						== "repository-canary"
					&& read_text(collisionSpool) == "spool-canary",
				"preexisting probe collisions are neither overwritten nor deleted");
			require(
				entryCount(repositories) == 1U
					&& entryCount(incoming) == 1U,
				"probe retries a collision and cleans only its new artifacts");
			adapter.reset();
		}

		{
			temporary_root root{ "git-probe-quarantine-destination" };
			const auto     fakeGitPath = make_fake_git_executable(root.Path);
			write_text(
				root.Path / "inject-probe-quarantine-destination",
				"race both no-replace destinations");
			auto       adapter          = make_adapter(fakeGitPath, root.Path);
			const auto repositories     = root.Path / "repositories" / "v1";
			const auto incoming         = root.Path / "incoming" / "v1";
			const auto repositoryCanary = repositories
				/ ".bpr-cleanup-destination-repository"
				/ "destination-canary";
			const auto spoolCanary = incoming / ".bpr-cleanup-destination-spool";
			require(
				read_text(repositoryCanary)
						== "quarantine-repository-canary"
					&& read_text(spoolCanary)
						== "quarantine-spool-canary",
				"exclusive quarantine rename preserves raced destinations");
			require(
				entryCount(repositories) == 1U
					&& entryCount(incoming) == 1U,
				"probe retries quarantine destination without owned residue");
			adapter.reset();
		}

		{
			temporary_root root{ "git-probe-post-create-substitution" };
			const auto     fakeGitPath = make_fake_git_executable(root.Path);
			write_text(
				root.Path / "inject-probe-post-create-substitution",
				"replace both exact artifact names");
			const auto adapter = git_adapter::make_installed_git_process({
				.GitExecutable     = fakeGitPath,
				.IsolationRoot     = root.Path,
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			require(
				!adapter.has_value()
					&& adapter.error()
						== application::git_process_failure::
							unsupported_git,
				"post-creation substitution fails the capability probe closed");

			const auto repositories        = root.Path / "repositories" / "v1";
			const auto incoming            = root.Path / "incoming" / "v1";
			const auto displacedRepository = repositories / ".displaced-owned-probe.git";
			const auto displacedSpool      = incoming / ".displaced-owned-probe.pack.part";
			require(
				std::filesystem::is_directory(displacedRepository)
					&& std::filesystem::is_regular_file(displacedSpool),
				"displaced owned probe artifacts remain quarantined");

			auto replacementRepositories = std::size_t{ 0U };
			for (const auto& entry :
				 std::filesystem::directory_iterator{ repositories }) {
				if (entry.path() == displacedRepository) {
					continue;
				}
				require(
					read_text(entry.path() / "replacement-canary")
						== "replacement-repository-canary",
					"replacement repository canary survives cleanup");
				++replacementRepositories;
			}
			auto replacementSpools = std::size_t{ 0U };
			for (const auto& entry :
				 std::filesystem::directory_iterator{ incoming }) {
				if (entry.path() == displacedSpool) {
					continue;
				}
				require(
					read_text(entry.path())
						== "replacement-spool-canary",
					"replacement spool canary survives cleanup");
				++replacementSpools;
			}
			require(
				replacementRepositories == 1U
					&& replacementSpools == 1U
					&& entryCount(repositories) == 2U
					&& entryCount(incoming) == 2U,
				"identity mismatch preserves replacements and displaced owners");
		}

		{
			temporary_root root{ "git-probe-reap-retain" };
			const auto     fakeGitPath = make_fake_git_executable(root.Path);
			write_text(
				root.Path / "inject-probe-reap-failure",
				"retain exact probe boundary");
			const auto adapter = git_adapter::make_installed_git_process({
				.GitExecutable     = fakeGitPath,
				.IsolationRoot     = root.Path,
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			require(
				!adapter.has_value()
					&& adapter.error()
						== application::git_process_failure::reap_failure,
				"injected probe reap failure remains distinguishable");
			const auto repositories = root.Path / "repositories" / "v1";
			const auto incoming     = root.Path / "incoming" / "v1";
			require(
				entryCount(repositories) == 1U
					&& entryCount(incoming) == 1U
					&& std::filesystem::is_directory(
						*std::filesystem::directory_iterator{ repositories })
					&& std::filesystem::is_regular_file(
						*std::filesystem::directory_iterator{ incoming }),
				"reap failure retains the probe repository and spool quarantine");
		}
	}

	auto run_factory_cancellation_contract() -> void
	{
		{
			temporary_root   root{ "git-factory-pre-cancel" };
			const auto       fakeGitPath = make_fake_git_executable(root.Path);
			std::stop_source cancellation;
			cancellation.request_stop();
			const auto adapter = git_adapter::make_installed_git_process(
				{
					.GitExecutable     = fakeGitPath,
					.IsolationRoot     = root.Path,
					.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
				},
				cancellation.get_token());
			require(
				!adapter.has_value()
					&& adapter.error()
						== application::git_process_failure::cancelled,
				"pre-cancelled factory returns typed cancellation");
			require(
				!std::filesystem::exists(root.Path / "home")
					&& !std::filesystem::exists(
						root.Path / "repositories")
					&& !std::filesystem::exists(
						root.Path / "invocations.log"),
				"pre-cancelled factory creates no isolation topology or child");
		}

		temporary_root root{ "git-factory-live-cancel" };
		const auto     fakeGitPath = make_fake_git_executable(root.Path);
		write_text(
			root.Path / "block-capability-probe-index-pack",
			"block exact probe command");
		const auto configuration = git_adapter::installed_git_process_configuration{
			.GitExecutable     = fakeGitPath,
			.IsolationRoot     = root.Path,
			.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
		};
		// [[BPR-1121]]. Until the calls below are joined, unwinding would destroy a
		// joinable thread, and they wait on a cancellation that comes after the
		// checks in between; so a failure in this window exits, as it always did.
		auto untilJoined = bpr::tests::exit_on_failure{ };

		using factory_result = decltype(git_adapter::make_installed_git_process(configuration));
		std::optional<factory_result> probing;
		std::optional<factory_result> waiting;
		std::stop_source              probeCancellation;
		std::stop_source              waiterCancellation;
		std::thread                   probe{
			[&] {
				probing.emplace(
					git_adapter::make_installed_git_process(
						configuration,
						probeCancellation.get_token()));
			}
		};
		const auto waitFor =
			[](const auto& aPredicate) {
				const auto deadline = std::chrono::steady_clock::now()
					+ std::chrono::seconds{ 5 };
				while (
					!aPredicate()
					&& std::chrono::steady_clock::now() < deadline) {
					std::this_thread::sleep_for(
						std::chrono::milliseconds{ 10 });
				}
				return aPredicate();
			};
		require(
			waitFor(
				[&] {
					return std::filesystem::exists(
						root.Path / "capability-probe-block-started");
				}),
			"first factory reaches the cancellable capability probe");

		std::thread waiter{
			[&] {
				waiting.emplace(
					git_adapter::make_installed_git_process(
						configuration,
						waiterCancellation.get_token()));
			}
		};
		require(
			waitFor(
				[&] {
					const auto log   = read_text(root.Path / "invocations.log");
					const auto first = log.find("arg=--version\n");
					return first != std::string::npos
						&& log.find(
							   "arg=--version\n",
							   first + 1U)
						!= std::string::npos;
				}),
			"second factory reaches the process-boundary lock wait");
		waiterCancellation.request_stop();
		waiter.join();
		require(
			waiting.has_value()
				&& !waiting->has_value()
				&& waiting->error()
					== application::git_process_failure::cancelled,
			"boundary-lock wait returns typed cancellation");

		probeCancellation.request_stop();
		probe.join();
		untilJoined.end();
		require(
			probing.has_value()
				&& !probing->has_value()
				&& probing->error()
					== application::git_process_failure::cancelled,
			"live capability child cancellation returns typed cancellation");
		require(
			std::filesystem::is_empty(
				root.Path / "repositories" / "v1")
				&& std::filesystem::is_empty(
					root.Path / "incoming" / "v1"),
			"cancelled probe exact-cleans owned repository and spool");
	}

	auto run_input_validation_contract() -> void
	{
		application::git_pack_id invalid{ };
		invalid.Algorithm      = static_cast<application::git_hash_algorithm>(0xffU);
		invalid.Digest.front() = 1U;
		require(
			!application::valid(invalid),
			"invalid hash algorithm is rejected");

		temporary_root root{ "git-input" };
		auto           adapter = make_adapter(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
			root.Path);
		const auto outside = root.Path / (std::string(64U, 'a') + ".git");
		require(
			adapter
					->initialize_bare_repository(
						{
							.Repository = outside,
							.Algorithm  = application::git_hash_algorithm::sha1,
						},
						{ })
					.error()
				== application::git_process_failure::invalid_request,
			"repository path authority cannot escape repositories/v1");
	}

	auto run_isolation_symlink_contract() -> void
	{
#if defined(Q_OS_UNIX)
		temporary_root root{ "git-isolation-symlink" };
		const auto     executable = std::filesystem::canonical(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE });
		const auto outsideDirectory   = root.Path / "outside-directory";
		const auto directoryIsolation = root.Path / "directory-isolation";
		std::filesystem::create_directory(outsideDirectory);
		std::filesystem::create_directory(directoryIsolation);
		std::filesystem::permissions(
			outsideDirectory,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::filesystem::permissions(
			directoryIsolation,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto directorySentinel = outsideDirectory / "sentinel";
		write_text(directorySentinel, "unchanged-directory-target");
		std::filesystem::create_directory_symlink(
			outsideDirectory,
			directoryIsolation / "home");
		const auto rejectedDirectory = git_adapter::make_installed_git_process(
			{
				.GitExecutable     = executable,
				.IsolationRoot     = std::filesystem::canonical(directoryIsolation),
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
		require(
			!rejectedDirectory.has_value()
				&& rejectedDirectory.error()
					== application::git_process_failure::
						invalid_configuration,
			"preexisting isolation directory symlink fails closed");
		require(
			read_text(directorySentinel)
					== "unchanged-directory-target"
				&& !std::filesystem::exists(
					directoryIsolation / "xdg"),
			"directory symlink target and remaining topology are untouched");

		const auto outsideFile = root.Path / "outside-file";
		write_text(outsideFile, "unchanged-file-target");
		const auto fileIsolation = root.Path / "file-isolation";
		std::filesystem::create_directory(fileIsolation);
		std::filesystem::permissions(
			fileIsolation,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::filesystem::create_symlink(
			outsideFile,
			fileIsolation / "config-system");
		const auto rejectedFile = git_adapter::make_installed_git_process(
			{
				.GitExecutable     = executable,
				.IsolationRoot     = std::filesystem::canonical(fileIsolation),
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
		require(
			!rejectedFile.has_value()
				&& rejectedFile.error()
					== application::git_process_failure::
						invalid_configuration,
			"preexisting isolation file symlink fails closed");
		require(
			read_text(outsideFile) == "unchanged-file-target"
				&& !std::filesystem::exists(fileIsolation / "home"),
			"file symlink target and isolation topology are untouched");

		const auto contentIsolation = root.Path / "content-isolation";
		std::filesystem::create_directory(contentIsolation);
		std::filesystem::permissions(
			contentIsolation,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto existingConfig = contentIsolation / "config-system";
		write_text(existingConfig, "must-not-be-truncated");
		const auto rejectedContent = git_adapter::make_installed_git_process(
			{
				.GitExecutable     = executable,
				.IsolationRoot     = std::filesystem::canonical(contentIsolation),
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
		require(
			!rejectedContent.has_value()
				&& rejectedContent.error()
					== application::git_process_failure::
						invalid_configuration
				&& read_text(existingConfig)
					== "must-not-be-truncated"
				&& !std::filesystem::exists(
					contentIsolation / "home"),
			"nonempty isolation policy file is rejected before any mutation");

		const auto policyIsolation = root.Path / "policy-isolation";
		std::filesystem::create_directories(
			policyIsolation / "hooks");
		std::filesystem::permissions(
			policyIsolation,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::filesystem::permissions(
			policyIsolation / "hooks",
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto policyMarker = root.Path / "policy-hook-ran";
		write_executable_canary(
			policyIsolation / "hooks" / "reference-transaction",
			policyMarker,
			"preexisting-policy-hook");
		const auto rejectedPolicy = git_adapter::make_installed_git_process(
			{
				.GitExecutable     = executable,
				.IsolationRoot     = std::filesystem::canonical(policyIsolation),
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
		require(
			!rejectedPolicy.has_value()
				&& rejectedPolicy.error()
					== application::git_process_failure::
						invalid_configuration,
			"nonempty persistent hook policy fails closed");
		require(
			!std::filesystem::exists(policyMarker)
				&& !std::filesystem::exists(
					policyIsolation / "home"),
			"policy hook is never launched and topology remains untouched");
#endif
	}

	// The isolation's policy files are the part of the topology the adapter
	// adopts instead of creating, and `config-global` is the one BPR-0437 made
	// load-bearing on Windows: it carries `core.longpaths`, without which Git
	// cannot create the lock directory for a staging ref beneath a cache. Both
	// the exact bytes and the exact link count of a preexisting policy file are
	// therefore contract. A hard link is the substitution attributes cannot
	// see — a regular file, owned by this user, carrying the adapter's own
	// bytes, whose only tell is that a second name reaches the same data — and
	// the adapter's one-link rule is what refuses it, the rule the cache
	// already proves for its manifest (BPR-0053).
	auto run_isolation_policy_file_contract() -> void
	{
#if defined(Q_OS_WIN)
		constexpr std::string_view expectedGlobalConfig = "[core]\n\tlongpaths = true\n";
#else
		constexpr std::string_view expectedGlobalConfig{ };
#endif
		constexpr std::string_view foreignGlobalConfig
			= "[core]\n\tlongpaths = false\n\thooksPath = /must-not-be-adopted\n";

		temporary_root root{ "git-isolation-policy-file" };
		const auto     executable = std::filesystem::canonical(
			std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE });
		const auto makeIsolation = [&root](const std::string_view aName) {
			auto path = root.Path / aName;
			std::filesystem::create_directory(path);
			std::filesystem::permissions(
				path,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			return std::filesystem::canonical(path);
		};
		const auto construct =
			[&executable](const std::filesystem::path& aIsolation) {
				return git_adapter::make_installed_git_process({
					.GitExecutable     = executable,
					.IsolationRoot     = aIsolation,
					.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
				});
			};

		// The control the two rejections below are measured against: policy
		// files already holding the adapter's exact bytes are adopted, which
		// is what makes this platform's expected `config-global` text the
		// contract rather than an unobserved implementation detail.
		{
			const auto adopted = makeIsolation("adopted-policy");
			write_text(adopted / "config-system", "");
			write_text(adopted / "config-global", expectedGlobalConfig);
			write_text(adopted / "attributes-system", "");
			const auto accepted = construct(adopted);
			require(
				accepted.has_value(),
				"preexisting policy files holding the adapter's exact bytes are adopted");
			require(
				read_text(adopted / "config-global") == expectedGlobalConfig,
				"the adopted global configuration is this platform's exact expected text");
		}

#if defined(Q_OS_WIN)
		{
			const auto shared = makeIsolation("read-shared-policy");
			write_text(shared / "config-system", "");
			write_text(shared / "config-global", expectedGlobalConfig);
			write_text(shared / "attributes-system", "");
			const auto reader = ::CreateFileW((shared / "config-global").c_str(), GENERIC_READ,
											  FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			require(reader != INVALID_HANDLE_VALUE, "a Git reader holds the valid global policy open");
			const auto accepted = construct(shared);
			const auto closed   = ::CloseHandle(reader) != 0;
			require(closed, "the policy reader closes its own handle");
			require(accepted.has_value(), "bootstrap adopts valid policy while another Git reader holds it open");
			require(read_text(shared / "config-global") == expectedGlobalConfig,
					"adoption preserves the policy bytes held by the reader");
		}
#endif

		{
			const auto readOnly = makeIsolation("read-only-policy");
			for (const auto& [name, contents] : std::array{
					 std::pair{ "config-system", std::string_view{ } },
					 std::pair{ "config-global", expectedGlobalConfig },
					 std::pair{ "attributes-system", std::string_view{ } } }) {
				write_text(readOnly / name, contents);
				std::filesystem::permissions(readOnly / name, std::filesystem::perms::owner_read,
											 std::filesystem::perm_options::replace);
			}
			const auto accepted = construct(readOnly);
			// Restore write access before the scratch root removes its files.
			for (const auto* name : { "config-system", "config-global", "attributes-system" }) {
				std::filesystem::permissions(readOnly / name, std::filesystem::perms::owner_write,
											 std::filesystem::perm_options::add);
			}
			require(accepted.has_value(), "bootstrap adopts valid read-only policy without rewriting it");
			require(read_text(readOnly / "config-global") == expectedGlobalConfig,
					"read-only policy adoption preserves its exact bytes");
		}

		// The link carries the exact bytes the adapter would have written, so
		// the link count is the only difference left to refuse it by. Adoption
		// must check the single-link identity without opening for
		// writing. The planted link and its target must survive the refusal.
		const auto outsideEmpty = root.Path / "outside-empty-policy";
		write_text(outsideEmpty, "");
		const auto hardlinked = makeIsolation("hardlinked-policy");
		std::filesystem::create_hard_link(
			outsideEmpty,
			hardlinked / "config-system");
		const auto rejectedLink = construct(hardlinked);
		require(
			!rejectedLink.has_value()
				&& rejectedLink.error()
					== application::git_process_failure::
						invalid_configuration,
			"a hardlinked isolation policy file fails closed");
		require(
			std::filesystem::hard_link_count(outsideEmpty) == 2U
				&& read_text(outsideEmpty).empty(),
			"the second link and its target survive the refusal untouched");

		const auto foreign       = makeIsolation("foreign-global-policy");
		const auto foreignGlobal = foreign / "config-global";
		write_text(foreignGlobal, foreignGlobalConfig);
		const auto rejectedGlobal = construct(foreign);
		require(
			!rejectedGlobal.has_value()
				&& rejectedGlobal.error()
					== application::git_process_failure::
						invalid_configuration
				&& read_text(foreignGlobal) == foreignGlobalConfig
				&& !std::filesystem::exists(foreign / "home"),
			"foreign global-configuration content is rejected before any mutation");
	}

	auto run_agent_workspace_parent_cleanup_contract() -> void
	{
		temporary_root root{ "git-agent-parent-cleanup" };
		const auto     makeParent = [&root](const std::string_view aName) {
			auto path = root.Path / aName;
			std::filesystem::create_directory(path);
			std::filesystem::permissions(
				path,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			return std::filesystem::canonical(path);
		};

		auto exactPath = makeParent("exact-empty");
		auto exact     = git_adapter::capture_installed_agent_workspace_parent(
			exactPath);
		require(
			exact.has_value()
				&& git_adapter::remove_installed_agent_workspace_parent(*exact)
				&& !std::filesystem::exists(exactPath),
			"exact empty agent parent is removed non-recursively");

		auto substitutedPath = makeParent("substituted");
		auto substituted     = git_adapter::capture_installed_agent_workspace_parent(
			substitutedPath);
		require(substituted.has_value(), "capture substituted parent identity");
		const auto displaced = root.Path / "substituted-original";
		std::filesystem::rename(substitutedPath, displaced);
		std::filesystem::create_directory(substitutedPath);
		std::filesystem::permissions(
			substitutedPath,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		write_text(substitutedPath / "substitute-canary", "preserve");
		require(
			!git_adapter::remove_installed_agent_workspace_parent(*substituted)
				&& read_text(substitutedPath / "substitute-canary") == "preserve"
				&& std::filesystem::is_directory(displaced),
			"same-path substitute and displaced original are retained");

		auto nonemptyPath = makeParent("nonempty");
		auto nonempty     = git_adapter::capture_installed_agent_workspace_parent(
			nonemptyPath);
		require(nonempty.has_value(), "capture nonempty parent identity");
		write_text(nonemptyPath / "owned-canary", "preserve");
		require(
			!git_adapter::remove_installed_agent_workspace_parent(*nonempty)
				&& read_text(nonemptyPath / "owned-canary") == "preserve"
				&& nonempty->Path == nonemptyPath,
			"nonempty exact parent is retained with its recovery capability");
	}

	auto run_process_error_detail_contract() -> void
	{
		const auto repeatedIdentityHex = [](const std::string_view aByte) {
			auto result = std::string{ };
			result.reserve(64U);
			for (auto index = 0U; index < 32U; ++index) {
				result += aByte;
			}
			return result;
		};

		{
			temporary_root root{ "git-factory-error-detail" };
			const auto     executable = make_fake_git_executable(root.Path);
			write_text(
				executable.parent_path() / "fail-bootstrap-exec-path",
				"fail exact bootstrap command");
			const auto made = git_adapter::make_installed_git_process({
				.GitExecutable     = executable,
				.IsolationRoot     = root.Path,
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			require(
				!made.has_value()
					&& made.error()
						== application::git_process_failure::nonzero_exit,
				"the installed-Git factory preserves its failing process code");
			require(
				made.error().Detail == redacted_git_diagnostic,
				"the installed-Git factory preserves the bounded redacted diagnostic");
		}

		{
			temporary_root root{ "git-error-detail" };
			auto           adapter = make_adapter(
				std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
				root.Path);
			const auto repository = repository_path(root.Path, '7');
			initialize_repository(
				*adapter,
				repository,
				application::git_hash_algorithm::sha1);
			const application::git_staging_ref absentRef{
				.Operation = make_operation(0x77U),
				.Endpoint  = application::git_private_ref_endpoint::base,
			};
			const std::array<application::git_ref_mutation, 1U> removeAbsent{
				application::git_ref_remove{
					.Ref         = absentRef,
					.ExpectedOld = object<application::git_commit_id>(
						sha1_commit_hex,
						application::git_hash_algorithm::sha1),
				}
			};
			const auto removed = adapter->update_refs(
				{ .Repository = repository, .Mutations = removeAbsent },
				{ });
			require(
				!removed.has_value()
					&& removed.error()
						== application::git_process_failure::nonzero_exit,
				"a real failing Git step reports a nonzero exit");
			require(
				removed.error().Detail.find("cannot lock ref")
					!= std::string::npos,
				"the nonzero exit retains Git's own diagnosis");
			require(
				removed.error().Detail.find(
					"refs/betterpullreview/v1/stage/"
					+ repeatedIdentityHex("77") + "/base")
					!= std::string::npos,
				"the retained diagnosis preserves the exact ref");
			const auto absent = adapter->inspect_ref(
				{ .Repository = repository, .Ref = absentRef },
				{ });
			require(
				absent.has_value() && !absent->has_value(),
				"a successful Git step has no error text");
		}

		{
			temporary_root root{ "git-error-detail-redaction" };
			auto           adapter = make_adapter(
				make_fake_git_executable(root.Path),
				root.Path);
			const auto repository = repository_path(root.Path, '8');
			initialize_repository(
				*adapter,
				repository,
				application::git_hash_algorithm::sha1);
			const auto operation = make_operation(0x88U);
			const auto failed    = fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_FAIL_MESSAGE");
			require(
				!failed.has_value()
					&& failed.error()
						== application::git_process_failure::nonzero_exit,
				"a remote-naming failure reports a nonzero exit");
			require(
				failed.error().Detail == redacted_git_diagnostic,
				"the URL-bearing failure retains the exact bounded redacted diagnostic");

			const auto remoteShapes = fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_REMOTE_SHAPES");
			require(
				!remoteShapes.has_value()
					&& remoteShapes.error()
						== application::git_process_failure::nonzero_exit
					&& remoteShapes.error().Detail
						== "fatal: local /tmp/user@host:dir/pack-f041bc5e.pack "
						   "remains diagnostic\n"
						   "fatal: fallback remote <redacted-url> denied\n"
						   "fatal: local reflog HEAD@{1} remains diagnostic\n",
				"local paths and reflogs remain exact while SSH-style remotes are redacted");

			const auto errorAtLimit = fake_index_request(*adapter, repository, operation, "BPR_ERROR_AT_LIMIT");
			require(!errorAtLimit.has_value() && errorAtLimit.error() == application::git_process_failure::nonzero_exit
						&& errorAtLimit.error().Detail == std::string(16U * 1024U, 'x'),
					"stderr at exactly 16 KiB preserves the diagnosis and nonzero-exit terminal");
			const auto errorOverLimit = fake_index_request(*adapter, repository, operation, "BPR_ERROR_OVER_LIMIT");
			require(!errorOverLimit.has_value() && errorOverLimit.error() == application::git_process_failure::standard_error_limit
						&& errorOverLimit.error().Detail.empty(),
					"one byte over the stderr limit refuses with no partial diagnostic");
			const auto expanded = fake_index_request(*adapter, repository, operation, "BPR_ERROR_REDACTION_EXPANSION");
			require(!expanded.has_value() && expanded.error() == application::git_process_failure::nonzero_exit
						&& expanded.error().Detail.size() == 16U * 1024U
						&& expanded.error().Detail.starts_with("<redacted-url> ")
						&& expanded.error().Detail.find("a://") == std::string::npos,
					"URL replacement expansion preserves the byte limit without raw fallback");

			const auto quiet = fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_QUIET_ERROR");
			require(
				!quiet.has_value()
					&& quiet.error()
						== application::git_process_failure::malformed_output
					&& quiet.error().Detail.empty(),
				"accepted output with stderr is malformed and textless");
			const auto exitOneError = fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_EXIT_ONE_ERROR");
			require(
				!exitOneError.has_value()
					&& exitOneError.error()
						== application::git_process_failure::malformed_output
					&& exitOneError.error().Detail.empty(),
				"an accepted index-pack exit 1 with standard error is malformed and textless");
			require(
				fake_index_request(
					*adapter,
					repository,
					operation,
					"BPR_SILENT_SUCCESS")
					.has_value(),
				"a silent accepted invocation succeeds");

			// BPR-0852: fsck's standard output becomes counts, never text.
			const auto verify = [&] {
				return adapter->verify_connectivity(
					{
						.Repository = repository,
						.Base       = object<application::git_commit_id>(std::string(40U, 'a'), application::git_hash_algorithm::sha1),
						.Head       = object<application::git_commit_id>(std::string(40U, 'b'), application::git_hash_algorithm::sha1),
					},
					{ });
			};
			const auto blobName = std::string(40U, 'c');
			write_text(
				root.Path / "fsck-output",
				"broken link from    tree " + std::string(40U, 'd') + "\n"
					+ "              to    blob " + blobName + "\n"
					+ "missing blob " + blobName + "\n"
					+ "missing commit " + std::string(40U, 'a') + "\n"
					+ "missing tree " + std::string(40U, 'd') + "\n"
					+ "missing tree " + std::string(40U, 'e') + "\n"
					+ "fatal: /private/bpr-canary https://user:bpr-canary@remote.invalid/x\n");
			const auto summarized = verify();
			require(
				!summarized.has_value()
					&& summarized.error() == application::git_process_failure::nonzero_exit
					&& summarized.error().Detail
						== "fsck reported 1 missing commit, 2 missing trees, 1 missing blob, 1 broken link, 1 other line",
				"a connectivity refusal carries fixed counts of fsck's findings");
			require(
				summarized.error().Detail.find(blobName) == std::string::npos
					&& summarized.error().Detail.find("bpr-canary") == std::string::npos,
				"the connectivity summary repeats no object name, path or unrecognised text");
			// The bound is the standard-output limit: 18 lines of 54 bytes and a
			// 52-byte line fill 1024, and one byte more is refused without text.
			auto atLimit = std::string{ };
			for (auto line = 0U; line != 18U; ++line) {
				atLimit += "missing blob " + std::string(40U, 'c') + "\n";
			}
			atLimit += std::string(1024U - atLimit.size() - 1U, 'x') + "\n";
			write_text(root.Path / "fsck-output", atLimit);
			const auto bounded = verify();
			require(
				!bounded.has_value()
					&& bounded.error() == application::git_process_failure::nonzero_exit
					&& bounded.error().Detail == "fsck reported 18 missing blobs, 1 other line",
				"the connectivity summary counts retained output up to the standard-output limit");
			write_text(root.Path / "fsck-output", atLimit + "x");
			const auto overLimit = verify();
			require(
				!overLimit.has_value()
					&& overLimit.error() == application::git_process_failure::standard_output_limit
					&& overLimit.error().Detail.empty(),
				"fsck output past the standard-output limit is refused without text");
			std::filesystem::remove(root.Path / "fsck-output");
			require(verify().has_value(), "an empty fsck report still verifies");
		}
	}

	// [[BPR-0880]] The committed fixture packs and what real Git does with
	// them. `tests/fixtures/git/fsck-severity/v1/README.md` records how they
	// are rebuilt; `tests/scripts/fsck_severity_fixture.py` rebuilds them and
	// proves the installed Git knows every message identifier this adapter
	// ships.
	// Files in one pack directory carrying index-pack's temporary name.
	[[nodiscard]] auto index_pack_residue(
		const std::filesystem::path& aPackDirectory) -> std::size_t
	{
		std::error_code error;
		std::size_t     residue = 0U;
		for (
			const auto& entry :
			std::filesystem::directory_iterator{ aPackDirectory, error }) {
			if (entry.path().filename().string().starts_with("tmp_pack_")) {
				++residue;
			}
		}
		return residue;
	}

	auto run_sparse_special_blob_contract() -> void
	{
		temporary_root root{ "git-sparse-special-blobs" };
		auto           adapter      = make_adapter(std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE }, root.Path);
		const auto     installation = adapter->installation();
		if (installation.VersionMajor < 2U || (installation.VersionMajor == 2U && installation.VersionMinor < 44U)) {
			return; // Older Git cannot configure sparse fsck severities.
		}
		const auto source = repository_path(root.Path, '1');
		initialize_repository(*adapter, source, application::git_hash_algorithm::sha1);
		std::uint8_t operation = 0x80U;
		char         digit     = '2';
		struct special_blob_case
		{
			std::string Name;
			std::string Content;
			std::string ExpectedError; // Empty means the blob is deliberately absent.
		};
		const std::array<special_blob_case, 4U> cases{ {
			{ ".gitattributes", "*.cpp text\n", "" },
			{ ".gitmodules", "", "" },
			{ ".gitattributes", std::string(4096U, 'a') + " text\n", "gitattributesLineLength" },
			{ ".gitmodules", "[submodule \"bad\"]\npath = -unsafe\n", "gitmodulesPath" },
		} };
		for (const auto& testCase : cases) {
			const auto malformed = !testCase.ExpectedError.empty();
			const auto blob      = run_fixture_git(source, { "hash-object", "-w", "--stdin" }, testCase.Content);
			const auto tree      = run_fixture_git(source, { "mktree" }, "100644 blob " + blob + "\t" + testCase.Name + "\n");
			const auto prefix    = root.Path / ("special-" + std::to_string(operation));
			const auto packHex   = run_fixture_git(source, { "pack-objects", prefix.string() }, tree + "\n" + (malformed ? blob + "\n" : ""));
			const auto bytes     = read_text(prefix.string() + "-" + packHex + ".pack");
			for (const auto sparse : { false, true }) {
				const auto repository = repository_path(root.Path, digit++);
				initialize_repository(*adapter, repository, application::git_hash_algorithm::sha1);
				const auto identity = make_operation(operation++);
				write_spool(*adapter, identity, std::as_bytes(std::span{ bytes }));
				const application::git_index_pack_request request{
					.Repository        = repository,
					.Operation         = identity,
					.SpoolBytes        = bytes.size(),
					.MaximumInputBytes = bytes.size(),
					.ExpectedPack      = object<application::git_pack_id>(packHex, application::git_hash_algorithm::sha1),
				};
				const auto result = sparse ? adapter->index_sparse_pack(request, { }) : adapter->index_pack(request, { });
				if (sparse && !malformed) {
					require(result.has_value(), "sparse import accepts intentionally absent special blobs without extra stdout");
				} else {
					require(!result, "complete imports refuse absent special blobs and both paths refuse malformed present blobs");
					if (malformed) {
						require(result.error() == application::git_process_failure::unsupported_object
									&& result.error().Detail == testCase.ExpectedError,
								"sparse import retains the exact present-object fsck error");
					}
				}
			}
		}
	}

	/// [[BPR-1738]]. A sparse review's cache holds the head's commit and
	/// trees and no blob or parent, as the gh transport's sparse acquisition
	/// installs them. Copying a workspace out of it packed the head's closure
	/// and stopped at the first missing parent. A snapshot pack of the head
	/// alone makes a working, shallow workspace whose history ends at the head.
	auto run_sparse_agent_workspace_contract() -> void
	{
		temporary_root root{ "git-sparse-agent-workspace" };
		auto           adapter      = make_adapter(std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE }, root.Path);
		const auto     installation = adapter->installation();
		if (installation.VersionMajor < 2U || (installation.VersionMajor == 2U && installation.VersionMinor < 44U)) {
			return; // Older Git cannot configure sparse fsck severities.
		}
		const auto source = repository_path(root.Path, '1');
		initialize_repository(*adapter, source, application::git_hash_algorithm::sha1);
		const auto readme = run_fixture_git(source, { "hash-object", "-w", "--stdin" }, "the head's readme\n");
		const auto code   = run_fixture_git(source, { "hash-object", "-w", "--stdin" }, "int main() { return 0; }\n");
		const auto nested = run_fixture_git(source, { "mktree" }, "100644 blob " + code + "\tmain.cpp\n");
		const auto tree   = run_fixture_git(source, { "mktree" }, "100644 blob " + readme + "\tREADME.md\n040000 tree " + nested + "\tsrc\n");
		const auto parent = run_fixture_git(source, { "commit-tree", tree }, "parent the review never fetched\n");
		const auto head   = run_fixture_git(source, { "commit-tree", tree, "-p", parent }, "head\n");

		// The sparse cache: the head's commit and trees, as the review holds them.
		const auto cache       = repository_path(root.Path, '2');
		const auto sparsePack  = root.Path / "sparse";
		const auto sparseHex   = run_fixture_git(source, { "pack-objects", sparsePack.string() }, head + "\n" + tree + "\n" + nested + "\n");
		const auto sparseBytes = read_text(sparsePack.string() + "-" + sparseHex + ".pack");
		initialize_repository(*adapter, cache, application::git_hash_algorithm::sha1);
		const auto operation = make_operation(0x91U);
		write_spool(*adapter, operation, std::as_bytes(std::span{ sparseBytes }));
		const auto imported = adapter->index_sparse_pack(
			{
				.Repository        = cache,
				.Operation         = operation,
				.SpoolBytes        = sparseBytes.size(),
				.MaximumInputBytes = sparseBytes.size(),
				.ExpectedPack      = object<application::git_pack_id>(sparseHex, application::git_hash_algorithm::sha1),
			},
			{ });
		require(imported.has_value(), "the sparse cache holds the head's commit and trees");

		// The snapshot: the same commit, its trees and its blobs, still no parent.
		const auto snapshotDirectory = root.Path / "snapshot";
		std::filesystem::create_directory(snapshotDirectory);
		std::filesystem::permissions(snapshotDirectory, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
		const auto snapshotHex  = run_fixture_git(source, { "pack-objects", (snapshotDirectory / "snapshot").string() },
												  head + "\n" + tree + "\n" + nested + "\n" + readme + "\n" + code + "\n");
		const auto snapshotPack = snapshotDirectory / ("snapshot-" + snapshotHex + ".pack");
		std::filesystem::remove(snapshotDirectory / ("snapshot-" + snapshotHex + ".idx"));
		std::filesystem::permissions(snapshotPack, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
									 std::filesystem::perm_options::replace);

		temporary_root workspaceParent{ "git-sparse-agent-workspace-parent" };
		const auto     headOid   = core::git_commit_oid{ core::git_hash_algorithm::sha1,
														 object<application::git_commit_id>(head, application::git_hash_algorithm::sha1).Digest };
		const auto     authority = workspace_authority();
		const auto     request   = [&](const std::uint64_t aSession, std::optional<application::git_workspace_snapshot_pack> aSnapshot) {
			return application::git_agent_workspace_request{
				.Repository      = cache,
				.Materialization = {
					.ApplicationSession = core::application_session_id{ 1U },
					.Authority          = authority,
					.Session            = {
						.Id      = core::agent_session_id{ aSession },
						.Profile = core::external_terminal_profile_id,
						.Binding = {
							.Host        = authority.Host,
							.Repository  = authority.Repository,
							.PullRequest = authority.PullRequest,
							.BaseOid     = headOid,
							.HeadOid     = headOid,
							.LeftEpoch   = core::epoch_ordinal{ 1U },
							.RightEpoch  = core::epoch_ordinal{ 1U },
						},
					},
					.ParentDirectory = workspaceParent.Path,
				},
				.Snapshot = std::move(aSnapshot),
			};
		};

		const auto copied = adapter->materialize_agent_workspace(request(1U, std::nullopt), { });
		require(!copied.has_value() && copied.error().Detail.find("parents") != std::string::npos,
				"copying the head's closure out of a sparse cache stops at its missing parent, as the reported failure did");
		require(std::filesystem::is_empty(workspaceParent.Path), "the refused copy leaves no workspace behind");

		const auto snapshot = application::git_workspace_snapshot_pack{
			.Pack  = snapshotPack,
			.Bytes = std::filesystem::file_size(snapshotPack),
			.Head  = headOid,
		};
		auto other = snapshot;
		other.Head = core::git_commit_oid{ core::git_hash_algorithm::sha1, object<application::git_commit_id>(parent, application::git_hash_algorithm::sha1).Digest };
		require(!adapter->materialize_agent_workspace(request(2U, other), { }).has_value(), "a snapshot of another commit is refused");
		auto misstated = snapshot;
		misstated.Bytes += 1U;
		require(!adapter->materialize_agent_workspace(request(3U, misstated), { }).has_value(), "a snapshot whose size is misstated is refused");

		const auto made = adapter->materialize_agent_workspace(request(4U, snapshot), { });
		require(made.has_value(), "a sparse review's workspace is made from the head's snapshot");
		const auto workspace = std::filesystem::path{ made->Workspace.Path };
		require(read_text(workspace / "README.md") == "the head's readme\n" && read_text(workspace / "src" / "main.cpp") == "int main() { return 0; }\n",
				"the workspace holds every file at the head");
		require(read_text(workspace / ".git" / "shallow") == head + "\n", "the workspace records the head as its shallow boundary");
		require(made->Workspace.Shallow, "and says it is shallow, for the brief ([[BPR-1752]])");
		require(run_fixture_git(workspace / ".git", { "rev-list", "--count", "HEAD" }) == "1",
				"history in the workspace ends at the head instead of failing at its missing parent");
		require(adapter->cleanup_agent_workspace(*made, { }).has_value() && !std::filesystem::exists(workspace), "the snapshot workspace cleans up normally");

		// [[BPR-1751]]. A linked clone that holds the head's parent ends
		// the snapshot's shallow boundary: its history is borrowed.
		const auto clone = root.Path / "clone";
		std::filesystem::create_directories(clone);
		static_cast<void>(run_fixture_git(clone / ".git", { "init", "--quiet" }, { }, false));
		static_cast<void>(run_fixture_git(source, { "pack-objects", "--revs", (clone / ".git" / "objects" / "pack" / "pack").string() }, parent + "\n"));
		auto borrowedRequest                         = request(5U, snapshot);
		borrowedRequest.Materialization.HistoryClone = clone;
		const auto borrowed                          = adapter->materialize_agent_workspace(borrowedRequest, { });
		require(borrowed.has_value() && borrowed->Workspace.BorrowsHistory, "a sparse review's workspace borrows the clone's history");
		require(!borrowed->Workspace.Shallow, "and is not shallow once its parent is borrowed");
		const auto borrowedWorkspace = std::filesystem::path{ borrowed->Workspace.Path };
		require(!std::filesystem::exists(borrowedWorkspace / ".git" / "shallow"), "with the head's parent in the clone the workspace is not shallow");
		require(run_fixture_git(borrowedWorkspace / ".git", { "rev-list", "--count", "HEAD" }) == "2", "history goes past the head into the clone");
		require(adapter->cleanup_agent_workspace(*borrowed, { }).has_value(), "and it cleans up");
	}

	/// [[BPR-1751]], RFC-BPR-0092 Q7. A workspace that borrows a linked
	/// clone's history: one alternates line naming the clone's objects, only
	/// what the clone lacks copied from the cache, nothing written to the clone,
	/// no remote, and a clone that is not a repository refused.
	struct inventory_entry
	{
		std::filesystem::file_type Type;
		std::uintmax_t             Bytes;
		std::int64_t               Written;
		auto                       operator==(const inventory_entry&) const -> bool = default;
	};
	using inventory_snapshot = std::map<std::string, inventory_entry>;
	enum class inventory_reader { cached_entry,
								  current_handle };

	[[nodiscard]] auto current_inventory_entry(const std::filesystem::path& aPath, const std::filesystem::file_type aType, std::error_code& aError) -> inventory_entry
	{
		auto result = inventory_entry{ aType, 0U, 0 };
#if defined(_WIN32)
		const auto handle = ::CreateFileW(aPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
										  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (handle == INVALID_HANDLE_VALUE) {
			aError = std::error_code{ static_cast<int>(::GetLastError()), std::system_category() };
			return result;
		}
		auto       basic    = FILE_BASIC_INFO{ };
		auto       standard = FILE_STANDARD_INFO{ };
		const auto read     = ::GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, static_cast<DWORD>(sizeof(basic)))
			&& ::GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, static_cast<DWORD>(sizeof(standard)));
		const auto failure = read ? ERROR_SUCCESS : ::GetLastError();
		const auto closed  = ::CloseHandle(handle);
		if (!read) {
			aError = std::error_code{ static_cast<int>(failure), std::system_category() };
		} else if (!closed) {
			aError = std::error_code{ static_cast<int>(::GetLastError()), std::system_category() };
		} else if (standard.EndOfFile.QuadPart < 0) {
			aError = std::make_error_code(std::errc::invalid_argument);
		} else {
			aError.clear();
			result.Written = basic.LastWriteTime.QuadPart;
			result.Bytes   = aType == std::filesystem::file_type::regular ? static_cast<std::uintmax_t>(standard.EndOfFile.QuadPart) : 0U;
		}
#else
		result.Written = static_cast<std::int64_t>(std::filesystem::last_write_time(aPath, aError).time_since_epoch().count());
		if (!aError && aType == std::filesystem::file_type::regular) {
			result.Bytes = std::filesystem::file_size(aPath, aError);
		}
#endif
		return result;
	}

	[[nodiscard]] auto file_inventory(const std::filesystem::path& aRoot, const inventory_reader aReader = inventory_reader::cached_entry) -> inventory_snapshot
	{
		auto seen  = inventory_snapshot{ };
		auto error = std::error_code{ };
		auto path  = aRoot;
		for (auto entry = std::filesystem::recursive_directory_iterator{ aRoot, error }; !error && entry != std::filesystem::recursive_directory_iterator{ }; entry.increment(error)) {
			path                   = entry->path();
			const auto type        = std::filesystem::symlink_status(path, error).type();
			auto       observation = inventory_entry{ type, 0U, 0 };
			if (!error && aReader == inventory_reader::current_handle) {
				observation = current_inventory_entry(path, type, error);
			} else if (!error) {
				if (entry->is_regular_file(error) && !error) {
					observation.Bytes = entry->file_size(error);
				}
				if (!error) {
					observation.Written = static_cast<std::int64_t>(entry->last_write_time(error).time_since_epoch().count());
				}
			}
			if (error) {
				break;
			}
			seen.emplace(path.string(), observation);
		}
		if (error) {
			std::cerr << "GIT_CLONE_INVENTORY_ERROR path=" << path.string() << " error=" << error.value() << '\n';
		}
		require(!error, "the reviewer-clone inventory reads every fixture entry");
		return seen;
	}

	void report_inventory_changes(const inventory_snapshot& aBefore, const inventory_snapshot& aAfter, const std::string_view aStage)
	{
		if (aBefore == aAfter) {
			return;
		}
		std::cerr << "GIT_CLONE_INVENTORY stage=" << aStage << " before=" << aBefore.size() << " after=" << aAfter.size() << '\n';
		auto remaining = 8U;
		for (const auto& [path, old] : aBefore) {
			const auto current = aAfter.find(path);
			if (remaining != 0U && (current == aAfter.end() || current->second != old)) {
				std::cerr << "GIT_CLONE_CHANGED path=" << path << " present=" << (current != aAfter.end())
						  << " before-type=" << static_cast<int>(old.Type) << " before-size=" << old.Bytes << " before-ticks=" << old.Written;
				if (current != aAfter.end()) {
					std::cerr << " after-type=" << static_cast<int>(current->second.Type) << " after-size=" << current->second.Bytes << " after-ticks=" << current->second.Written;
				}
				std::cerr << '\n';
				--remaining;
			}
		}
		for (const auto& [path, entry] : aAfter) {
			if (remaining != 0U && !aBefore.contains(path)) {
				std::cerr << "GIT_CLONE_ADDED path=" << path << " type=" << static_cast<int>(entry.Type) << " size=" << entry.Bytes << " ticks=" << entry.Written << '\n';
				--remaining;
			}
		}
	}

	/// The objects in a repository's own packs, from `count-objects -v`,
	/// which counts no alternate.
	[[nodiscard]] auto in_pack(const std::filesystem::path& aGitDirectory) -> std::string
	{
		QProcess process;
		process.setProgram(QString::fromStdString(BPR_TEST_GIT_EXECUTABLE));
		process.setArguments({ QString::fromStdString("--git-dir=" + aGitDirectory.string()), QStringLiteral("count-objects"), QStringLiteral("-v") });
		auto environment = QProcessEnvironment::systemEnvironment();
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert("LC_ALL", "C");
		process.setProcessEnvironment(environment);
		process.start();
		require(process.waitForStarted(5000) && process.waitForFinished(10000) && process.exitCode() == 0, "count-objects runs");
		const auto counted = process.readAllStandardOutput().toStdString();
		const auto at      = counted.find("in-pack: ");
		return at == std::string::npos ? std::string{ } : counted.substr(at + 9U, counted.find('\n', at) - at - 9U);
	}

	auto run_borrowed_history_workspace_contract(const bool aInventoryControl = false) -> void
	{
		temporary_root root{ "git-borrowed-history-workspace" };
		auto           adapter = make_adapter(std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE }, root.Path);
		// The complete cache: the base and the head with their whole history.
		const auto cache = repository_path(root.Path, '1');
		initialize_repository(*adapter, cache, application::git_hash_algorithm::sha1);
		const auto code     = run_fixture_git(cache, { "hash-object", "-w", "--stdin" }, "int main() { return 0; }\n");
		const auto nested   = run_fixture_git(cache, { "mktree" }, "100644 blob " + code + "\tmain.cpp\n");
		const auto oldText  = run_fixture_git(cache, { "hash-object", "-w", "--stdin" }, "the base's readme\n");
		const auto newText  = run_fixture_git(cache, { "hash-object", "-w", "--stdin" }, "the head's readme\n");
		const auto baseTree = run_fixture_git(cache, { "mktree" }, "100644 blob " + oldText + "\tREADME.md\n040000 tree " + nested + "\tsrc\n");
		const auto headTree = run_fixture_git(cache, { "mktree" }, "100644 blob " + newText + "\tREADME.md\n040000 tree " + nested + "\tsrc\n");
		const auto base     = run_fixture_git(cache, { "commit-tree", baseTree }, "base\n");
		const auto head     = run_fixture_git(cache, { "commit-tree", headTree, "-p", base }, "head\n");

		// The reviewer's clone: a working tree whose .git holds the base only.
		const auto clone = root.Path / "clone";
		std::filesystem::create_directories(clone);
		static_cast<void>(run_fixture_git(clone / ".git", { "init", "--quiet" }, { }, false));
		static_cast<void>(run_fixture_git(cache, { "pack-objects", "--revs", (clone / ".git" / "objects" / "pack" / "pack").string() }, base + "\n"));
		if (aInventoryControl) {
			const auto cachedBefore       = file_inventory(clone);
			const auto cachedAfter        = file_inventory(clone);
			const auto freshBefore        = file_inventory(clone, inventory_reader::current_handle);
			const auto freshAfter         = file_inventory(clone, inventory_reader::current_handle);
			const auto cachedAfterHandles = file_inventory(clone);
			std::cout << "GIT_CLONE_INVENTORY_CONTROL cached_equal_without_operation=" << (cachedBefore == cachedAfter)
					  << " cached_vs_handle_equal_without_operation=" << (cachedBefore == freshBefore)
					  << " handle_equal_without_operation=" << (freshBefore == freshAfter)
					  << " cached_equal_after_handles_without_operation=" << (cachedBefore == cachedAfterHandles) << '\n';
			report_inventory_changes(cachedBefore, cachedAfter, "cached-no-operation");
			report_inventory_changes(cachedBefore, freshBefore, "cached-vs-handle-no-operation");
			report_inventory_changes(cachedBefore, cachedAfterHandles, "cached-after-handles-no-operation");
			require(freshBefore == freshAfter, "current clone metadata stays exact with no adapter operation");
		}
		const auto reader      = aInventoryControl ? inventory_reader::current_handle : inventory_reader::cached_entry;
		const auto cloneBefore = file_inventory(clone, reader);

		temporary_root workspaceParent{ "git-borrowed-history-workspace-parent" };
		const auto     authority = workspace_authority();
		const auto     oid       = [](const std::string& aHex) {
			return core::git_commit_oid{ core::git_hash_algorithm::sha1, object<application::git_commit_id>(aHex, application::git_hash_algorithm::sha1).Digest };
		};
		const auto request = [&](const std::uint64_t aSession, std::optional<std::filesystem::path> aClone) {
			auto made                                        = application::git_agent_workspace_request{ };
			made.Repository                                  = cache;
			made.Materialization.ApplicationSession          = core::application_session_id{ 1U };
			made.Materialization.Authority                   = authority;
			made.Materialization.Session.Id                  = core::agent_session_id{ aSession };
			made.Materialization.Session.Profile             = core::external_terminal_profile_id;
			made.Materialization.Session.Binding.Host        = authority.Host;
			made.Materialization.Session.Binding.Repository  = authority.Repository;
			made.Materialization.Session.Binding.PullRequest = authority.PullRequest;
			made.Materialization.Session.Binding.BaseOid     = oid(base);
			made.Materialization.Session.Binding.HeadOid     = oid(head);
			made.Materialization.Session.Binding.LeftEpoch   = core::epoch_ordinal{ 1U };
			made.Materialization.Session.Binding.RightEpoch  = core::epoch_ordinal{ 1U };
			made.Materialization.ParentDirectory             = workspaceParent.Path;
			made.Materialization.HistoryClone                = std::move(aClone);
			return made;
		};

		const auto made = adapter->materialize_agent_workspace(request(1U, clone), { });
		require(made.has_value() && made->Workspace.BorrowsHistory, "a workspace borrows the linked clone's history");
		const auto workspace = std::filesystem::path{ made->Workspace.Path };
		require(read_text(workspace / ".git" / "objects" / "info" / "alternates") == std::filesystem::canonical(clone / ".git" / "objects").string() + "\n",
				"its one alternates line names the clone's objects");
		require(read_text(workspace / "README.md") == "the head's readme\n", "the workspace holds the head's files");
		require(run_fixture_git(workspace / ".git", { "rev-list", "--count", "HEAD" }) == "2", "the head's history reaches the base through the clone");
		require(in_pack(workspace / ".git") == "3", "only the head's commit, tree and new blob are copied: the base's objects are the clone's");
		require(!std::filesystem::exists(workspace / ".git" / "shallow"), "a complete review's borrowed workspace is not shallow");
		require(read_text(workspace / ".git" / "config").find("[remote") == std::string::npos, "the workspace has no remote to push to");
		const auto cloneAfter = file_inventory(clone, reader);
		report_inventory_changes(cloneBefore, cloneAfter, "materialization");
		require(cloneAfter == cloneBefore, "nothing is written to the reviewer's clone");
		require(adapter->cleanup_agent_workspace(*made, { }).has_value() && !std::filesystem::exists(workspace), "the borrowed workspace cleans up normally");
		const auto cloneCleaned = file_inventory(clone, reader);
		report_inventory_changes(cloneBefore, cloneCleaned, "cleanup");
		require(cloneCleaned == cloneBefore, "cleanup leaves the clone as it was");
		if (aInventoryControl) {
			std::cout << "GIT_CLONE_INVENTORY_CONTROL handle_equal_across_materialization_and_cleanup=1\n";
			const auto negative = root.Path / "inventory-negative";
			std::filesystem::create_directory(negative);
			const auto file = negative / "original";
			write_text(file, "a");
			const auto originalTime = std::filesystem::last_write_time(file);
			const auto beforeWrite  = file_inventory(negative, inventory_reader::current_handle);
			write_text(file, "longer");
			std::filesystem::last_write_time(file, originalTime);
			require(file_inventory(negative, inventory_reader::current_handle) != beforeWrite, "current inventory detects a size write even with the original timestamp");
			write_text(file, "a");
			std::filesystem::last_write_time(file, originalTime - std::chrono::hours{ 1 });
			require(file_inventory(negative, inventory_reader::current_handle) != beforeWrite, "current inventory detects a timestamp write at the original size");
			const auto beforeAdd = file_inventory(negative, inventory_reader::current_handle);
			write_text(negative / "added", "added");
			require(file_inventory(negative, inventory_reader::current_handle) != beforeAdd, "current inventory detects an added entry");
			std::cout << "GIT_CLONE_INVENTORY_CONTROL size_write_detected=1 timestamp_write_detected=1 added_name_detected=1\n";
		}

		// The clone already holds the head: nothing is copied at all.
		static_cast<void>(run_fixture_git(cache, { "pack-objects", "--revs", (clone / ".git" / "objects" / "pack" / "pack").string() }, head + "\n"));
		const auto again = adapter->materialize_agent_workspace(request(2U, clone), { });
		require(again.has_value() && in_pack(std::filesystem::path{ again->Workspace.Path } / ".git") == "0", "a clone that holds the head gives the whole workspace");
		require(read_text(std::filesystem::path{ again->Workspace.Path } / "README.md") == "the head's readme\n", "and its files are checked out from the clone's objects");
		require(adapter->cleanup_agent_workspace(*again, { }).has_value(), "and it cleans up");

		const auto notClone = root.Path / "not-a-clone";
		std::filesystem::create_directories(notClone);
		const auto refused = adapter->materialize_agent_workspace(request(3U, notClone), { });
		require(!refused.has_value(), "a folder that is not a clone is refused");
		require(std::filesystem::is_empty(workspaceParent.Path), "and leaves no workspace behind");

		const auto plain = adapter->materialize_agent_workspace(request(4U, std::nullopt), { });
		require(plain.has_value() && !plain->Workspace.BorrowsHistory && !std::filesystem::exists(std::filesystem::path{ plain->Workspace.Path } / ".git" / "objects" / "info" / "alternates"),
				"without a clone the workspace is the private copy, as before");
		require(!plain->Workspace.Shallow && run_fixture_git(std::filesystem::path{ plain->Workspace.Path } / ".git", { "cat-file", "-t", base }) == "commit",
				"and holds the base, so the brief's diff against it works ([[BPR-1752]])");
		require(adapter->cleanup_agent_workspace(*plain, { }).has_value(), "and it cleans up");
	}

	auto run_fsck_severity_contract() -> void
	{
		const std::filesystem::path fixtures{ BPR_FSCK_SEVERITY_FIXTURE_ROOT };
		const auto                  refused = read_text(fixtures / "error-class.pack");
		// Both classes Git's own defaults call acceptable. The warning class
		// exited 128 before BPR-0880; the information class exited 0 but wrote
		// a `warning:` line, which this adapter reads as malformed output, so
		// it dead-ended just as completely.
		const std::array<std::pair<std::string, std::string>, 2U> accepted{ {
			{ read_text(fixtures / "warning-class.pack"),
			  "ab5b0be4ad29df83374b6d75335d7fcadb295a00" },
			{ read_text(fixtures / "information-class.pack"),
			  "e4b2c53b9d045fb126a1c1760f8f83aa6ca22e9b" },
		} };
		require(
			!refused.empty()
				&& std::ranges::none_of(
					accepted,
					[](const auto& aPack) { return aPack.first.empty(); }),
			"the committed fsck-severity packs are readable");

		const auto refusedPack = object<application::git_pack_id>(
			"14222642622265351fd047f0c72eabc0c03f24e6",
			application::git_hash_algorithm::sha1);

		// Each row gets its own repository and operation so that a refusal
		// cannot be read as a consequence of an earlier import.
		const auto attempt = [&](
								 application::git_process_port&  aDapter,
								 const std::filesystem::path&    aRepository,
								 const std::uint8_t              aOperation,
								 const std::string&              aPack,
								 const application::git_pack_id& aExpected,
								 const bool                      aSparse) {
			const auto operation = make_operation(aOperation);
			initialize_repository(
				aDapter,
				aRepository,
				application::git_hash_algorithm::sha1);
			write_spool(
				aDapter,
				operation,
				std::as_bytes(std::span{ aPack }));
			const application::git_index_pack_request request{
				.Repository        = aRepository,
				.Operation         = operation,
				.SpoolBytes        = aPack.size(),
				.MaximumInputBytes = aPack.size(),
				.ExpectedPack      = aExpected,
			};
			return aSparse
				? aDapter.index_sparse_pack(request, { })
				: aDapter.index_pack(request, { });
		};

		for (const auto sparse : { false, true }) {
			// (a) The strict path and (b) the sparse path each import both
			// classes Git accepts. A zero-padded file mode is the warning
			// class the report reproduced; a symlinked `.gitignore` is the
			// information class it did not notice. Before BPR-0880 the first
			// died with `fatal: fsck error in packed object` and the second was
			// refused as malformed output for the `warning:` line it wrote. A
			// successful return here proves standard error stayed empty,
			// because an accepted exit that wrote to it would still be refused.
			temporary_root root{
				sparse ? "git-fsck-accepted-sparse" : "git-fsck-accepted-strict"
			};
			auto adapter = make_adapter(
				std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
				root.Path);
			const auto installation = adapter->installation();
			// `--fsck-objects=<msg-id>=<severity>` arrived in Git 2.44. An
			// older installation keeps the bare option, so its sparse path
			// stays exactly as strict as it is today rather than being handed
			// an argument it would reject outright.
			const auto policyReachesGit = !sparse
				|| installation.VersionMajor > 2U
				|| (installation.VersionMajor == 2U
					&& installation.VersionMinor >= 44U);
			auto digit     = static_cast<char>(sparse ? '5' : '4');
			auto operation = static_cast<std::uint8_t>(sparse ? 0x55U : 0x44U);
			for (const auto& [pack, identity] : accepted) {
				const auto expected = object<application::git_pack_id>(
					identity,
					application::git_hash_algorithm::sha1);
				const auto repository = repository_path(root.Path, digit);
				const auto indexed    = attempt(
					*adapter,
					repository,
					operation,
					pack,
					expected,
					sparse);
				++digit;
				++operation;
				if (!policyReachesGit) {
					// Exactly today's behaviour, by class: the warning dies
					// inside Git and is named, the information finding is
					// accepted by Git and refused here for the line it wrote.
					require(
						!indexed.has_value()
							&& indexed.error()
								== (identity == accepted.front().second
										? application::git_process_failure::unsupported_object
										: application::git_process_failure::malformed_output),
						"a sparse path older than Git 2.44 refuses exactly as it does today");
					continue;
				}
				require(
					indexed.has_value() && indexed->Pack == expected,
					sparse
						? "the sparse path imports what Git's own fsck accepts"
						: "the strict path imports what Git's own fsck accepts");
				require(
					std::filesystem::is_regular_file(
						repository / "objects" / "pack"
						/ ("pack-" + identity + ".idx")),
					"the accepted import installs a matching pack index");
				require(
					index_pack_residue(repository / "objects" / "pack") == 0U,
					"the accepted import leaves no temporary file");
			}
		}

		{
			// (c) An error-class object is still refused, on both paths, and
			// the refusal names the class instead of reporting that some Git
			// command exited nonzero.
			temporary_root root{ "git-fsck-error" };
			auto           adapter = make_adapter(
				std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE },
				root.Path);
			for (const auto sparse : { false, true }) {
				const auto repository = repository_path(
					root.Path,
					sparse ? '7' : '6');
				const auto indexed = attempt(
					*adapter,
					repository,
					sparse ? 0x77U : 0x66U,
					refused,
					refusedPack,
					sparse);
				require(
					!indexed.has_value(),
					"an error-class fsck object is still refused");
				require(
					indexed.error()
						== application::git_process_failure::unsupported_object,
					"the refusal is an unsupported object, not a bare process failure");
				require(
					indexed.error().Detail == "duplicateEntries",
					"the refusal names the exact fsck message class");
				// [[BPR-0880]]. No residue claim here, deliberately. Removing
				// Git's `tmp_pack_XXXXXX` after a refusal is the *cache's*
				// guarantee, not this layer's: `installed_git_cache.cpp` owns
				// the exact removal ([[BPR-0851]]) and
				// `git_cache_contract_tests.cpp` pins it on the refused strict
				// import. Asserting it here passed on Git 2.53 and failed on
				// Apple Git 2.50.1, which leaves the temporary behind on an
				// error-class refusal exactly as this card's original
				// reproduction recorded -- so the assertion was reading a Git
				// version difference as a product promise. What this layer does
				// promise is the refusal itself, which the three requirements
				// above pin.
			}
		}
	}

#if defined(_WIN32)
	/// \brief Counts every process this test process creates from now on.
	///
	/// \details This process joins a job of its own, which allows no breakaway,
	/// so every process it creates afterwards is born in that job, and the job's
	/// accounting counts it even after it has exited. A process cannot leave a
	/// job, so the one row that needs this runs last.
	class created_processes final
	{
	public:
		created_processes()
			: Job{ ::CreateJobObjectW(nullptr, nullptr) }
		{
			require(Job != nullptr, "create the process-creation accounting job");
			require(
				::AssignProcessToJobObject(Job, ::GetCurrentProcess()) != FALSE,
				"this test process joins its accounting job");
		}
		created_processes(const created_processes&)                    = delete;
		auto operator=(const created_processes&) -> created_processes& = delete;
		~created_processes()
		{
			static_cast<void>(::CloseHandle(Job));
		}

		[[nodiscard]] auto total() const -> DWORD
		{
			JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{ };
			require(
				::QueryInformationJobObject(Job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr) != FALSE,
				"read the process-creation accounting job");
			return accounting.TotalProcesses;
		}

	private:
		HANDLE Job = nullptr;
	};

	[[nodiscard]] auto fake_invocation_count(const std::filesystem::path& aRoot) -> std::size_t
	{
		const auto log   = read_text(aRoot / "invocations.log");
		auto       count = std::size_t{ 0U };
		for (auto offset = log.find("BEGIN\n"); offset != std::string::npos; offset = log.find("BEGIN\n", offset + 1U)) {
			++count;
		}
		return count;
	}

	// [[BPR-1115]]. An unguarded Git inherits its three standard handles and
	// nothing else. The parent holds a C runtime stream on a file, which the C
	// runtime makes inheritable, while it starts Git; the stream then closes,
	// and while that Git still runs the parent opens the file with no sharing
	// at all. A Git that inherited the stream's handle would still hold the
	// file, and the open would be refused with ERROR_SHARING_VIOLATION, which
	// is what [[BPR-1104]] found on a pack spool. The Git is the contract's fake
	// (this executable), blocked in index-pack, because a real Git never stays
	// alive on the adapter's closed input long enough to look.
	auto run_windows_inherited_handle_contract() -> void
	{
		temporary_root root{ "git-inherited-handle" };
		const auto     fakeGitPath = make_fake_git_executable(root.Path);
		auto           adapter     = make_adapter(fakeGitPath, root.Path);
		const auto     repository  = repository_path(root.Path, '5');
		initialize_repository(*adapter, repository, application::git_hash_algorithm::sha1);
		const auto operation = make_operation(0x55U);
		write_spool(*adapter, operation, std::as_bytes(std::span{ std::string_view{ "BPR_BLOCK" } }));

		const auto held = root.Path / "held-by-the-parent.bin";
		write_text(held, "held by the parent while Git starts");
		auto stream = std::make_optional<std::ifstream>(held, std::ios::binary);
		require(stream->is_open(), "the parent holds a C runtime stream on the file");

		using index_result = application::git_process_result<application::git_index_pack_success>;
		std::stop_source            cancellation;
		std::atomic_bool            returned{ false };
		std::optional<index_result> blocked;
		std::thread                 call{ [&] {
			blocked = adapter->index_pack(
				{
					.Repository        = repository,
					.Operation         = operation,
					.SpoolBytes        = 9U,
					.MaximumInputBytes = 9U,
					.ExpectedPack      = object<application::git_pack_id>(sha1_pack_hex, application::git_hash_algorithm::sha1),
				},
				cancellation.get_token());
			returned.store(true);
		} };
		const auto started  = root.Path / "block-started";
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 15 };
		while (!std::filesystem::exists(started) && !returned.load() && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
		}
		const auto running = std::filesystem::exists(started) && !returned.load();
		stream.reset();

		const auto exclusive = ::CreateFileW(held.c_str(), GENERIC_READ, 0U, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		const auto error     = exclusive == INVALID_HANDLE_VALUE ? ::GetLastError() : DWORD{ ERROR_SUCCESS };
		const auto stillRan  = !returned.load();
		if (exclusive != INVALID_HANDLE_VALUE) {
			static_cast<void>(::CloseHandle(exclusive));
		}
		cancellation.request_stop();
		call.join();
		require(running, "the unguarded fake Git is running, blocked in index-pack");
		require(stillRan, "the exclusive open is made while that Git still runs");
		require(
			error == ERROR_SUCCESS,
			"the parent opens the file with no sharing while Git runs: Git inherited none of the "
			"parent's handles (a refusal is error "
				+ std::to_string(error) + ", and 32 is ERROR_SHARING_VIOLATION)");
		require(
			blocked.has_value() && !blocked->has_value()
				&& blocked->error() == application::git_process_failure::cancelled,
			"the blocked Git is cancelled");
		std::cout << "BPR-1115 inherited handle: the parent opened its file with no sharing while the unguarded Git ran\n";
	}

	// [[BPR-1115]]. A launch whose handle list cannot be built is refused by
	// process creation itself: nothing is created, so no Git runs, and the
	// request fails as `process_start`. The fault seam withholds the standard
	// error handle, as a null standard handle would. The job's accounting shows
	// that no process was created at all, not that one was created and killed,
	// and the fake Git's invocation log shows that it never ran. The same
	// request without the seam creates a process and succeeds, so the count
	// can see a launch.
	auto run_windows_refused_launch_contract() -> void
	{
		temporary_root root{ "git-refused-launch" };
		const auto     fakeGitPath = make_fake_git_executable(root.Path);
		auto           adapter     = make_adapter(fakeGitPath, root.Path);
		const auto     repository  = repository_path(root.Path, '6');
		initialize_repository(*adapter, repository, application::git_hash_algorithm::sha1);
		const auto operation = make_operation(0x66U);

		const created_processes created;
		const auto              marker = root.Path / "inject-standard-handle-list-failure";
		write_text(marker, "withhold standard error");
		const auto invocationsBefore = fake_invocation_count(root.Path);
		const auto createdBefore     = created.total();
		const auto refused           = fake_index_request(*adapter, repository, operation, "BPR_REFUSED_LAUNCH");
		const auto createdRefused    = created.total() - createdBefore;
		const auto invocationsAfter  = fake_invocation_count(root.Path);
		require(
			!refused.has_value() && refused.error() == application::git_process_failure::process_start,
			"a launch refused for its handle list fails as process_start");
		require(
			createdRefused == 0U,
			"a launch refused for its handle list creates no process at all (created "
				+ std::to_string(createdRefused) + ")");
		require(invocationsAfter == invocationsBefore, "the refused Git never ran");

		require(std::filesystem::remove(marker), "remove the handle-list fault marker");
		const auto createdAcceptedBefore = created.total();
		const auto accepted              = fake_index_request(*adapter, repository, operation, "BPR_ACCEPTED_LAUNCH");
		const auto createdAccepted       = created.total() - createdAcceptedBefore;
		require(accepted.has_value(), "the same launch with its handle list succeeds");
		require(createdAccepted >= 1U, "the accounting job counts the launch it allowed");
		require(fake_invocation_count(root.Path) > invocationsAfter, "the allowed Git ran");
		std::cout << "BPR-1115 refused launch: process_start, " << createdRefused << " processes created; the allowed launch created "
				  << createdAccepted << '\n';
	}

	// [[BPR-1265]]. A Git that finishes before its launcher checks the job is
	// still a successful run. The seam holds the launcher for a second after the
	// start, so the fake Git has exited before the check. Assigning the child
	// to the job after the start, as before, failed exactly then
	// (`ERROR_ACCESS_DENIED` on an exited process) and the request came back as
	// `process_start`; with the job named at creation the child was in it all
	// along. The accounting job shows the launch was created, once.
	auto run_windows_finished_before_job_check_contract() -> void
	{
		temporary_root root{ "git-finished-before-job-check" };
		const auto     fakeGitPath = make_fake_git_executable(root.Path);
		auto           adapter     = make_adapter(fakeGitPath, root.Path);

		// `git init` reads no input, so the fake Git exits at once and is gone
		// well before the delayed check.
		const created_processes created;
		const auto              marker = root.Path / "inject-delay-before-job-check";
		write_text(marker, "check the job after the child has exited");
		const auto createdBefore = created.total();
		const auto finished      = adapter->initialize_bare_repository(
			{ .Repository = repository_path(root.Path, '7'), .Algorithm = application::git_hash_algorithm::sha1 }, { });
		const auto createdNow = created.total() - createdBefore;
		require(std::filesystem::remove(marker), "remove the job-check delay marker");
		require(finished.has_value(), "a Git that finished before the job check succeeds");
		require(createdNow >= 1U, "the launch was created");
		std::cout << "BPR-1265 finished before the job check: succeeded, " << createdNow << " processes created" << std::endl;
	}
#endif
}

auto main(int argc, char** argv) -> int
try {
#if defined(_WIN32)
	// As the fake Git this executable writes what Git writes: bytes with
	// `\n`, not the `\r\n` the CRT's text mode would substitute (BPR-0437).
	static_cast<void>(::_setmode(::_fileno(stdin), _O_BINARY));
	static_cast<void>(::_setmode(::_fileno(stdout), _O_BINARY));
	static_cast<void>(::_setmode(::_fileno(stderr), _O_BINARY));
#endif
	const auto executablePath = argc > 0
		? std::filesystem::absolute(argv[0])
		: std::filesystem::path{ };
	if (
		argc > 0
		&& executablePath.stem() == "git") {
		QStringList arguments;
		for (int index = 1; index < argc; ++index) {
			arguments.push_back(QString::fromUtf8(argv[index]));
		}
		return fake_git(arguments, executablePath);
	}
	QCoreApplication application{ argc, argv };

	if (argc == 2 && std::string_view{ argv[1] } == "--borrowed-history-inventory-control") {
		run_borrowed_history_workspace_contract(true);
		std::cout << "GIT_BORROWED_HISTORY_INVENTORY_CONTROL_OK\n";
		return 0;
	}

	run_real_git_contract();
	run_comparison_base_contract();
	run_comparison_parser_contract();
	run_connectivity_contract();
	run_changed_catalogue_parser_contract();
	run_rename_parser_contract();
	run_real_hostile_canary_contract();
	run_hostile_process_contract();
	run_capability_probe_cleanup_contract();
	run_factory_cancellation_contract();
	run_input_validation_contract();
	run_isolation_symlink_contract();
	run_isolation_policy_file_contract();
	run_agent_workspace_parent_cleanup_contract();
	run_process_error_detail_contract();
	run_fsck_severity_contract();
	run_sparse_special_blob_contract();
	run_sparse_agent_workspace_contract();
	run_borrowed_history_workspace_contract();
#if defined(_WIN32)
	run_windows_inherited_handle_contract();
	// Last: it moves this process into an accounting job it cannot leave.
	run_windows_refused_launch_contract();
	run_windows_finished_before_job_check_contract();
#endif
	std::cout << "Git process contract passed\n";
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
