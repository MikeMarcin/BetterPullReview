#include "bpr/adapters/git/installed_git_process.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <algorithm>
#include <array>
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
		if (!aCondition) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(1);
		}
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			const auto seed = std::to_string(
				std::chrono::steady_clock::now()
					.time_since_epoch()
					.count());
			Path = std::filesystem::temp_directory_path()
				/ ("bpr-" + std::string{ aLabel } + "-" + seed);
			std::filesystem::create_directories(Path);
			std::filesystem::permissions(
				Path,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			Path = std::filesystem::canonical(Path);
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
#if defined(Q_OS_UNIX)
		write_text(
			aPath,
			"#!/bin/sh\n"
			"printf '%s\\n' '"
				+ std::string{ aLabel }
				+ "' >> '" + aMarker.string() + "'\n"
												"exit 91\n");
		std::filesystem::permissions(
			aPath,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
#else
		static_cast<void>(aPath);
		static_cast<void>(aMarker);
		static_cast<void>(aLabel);
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
		std::ofstream output{ log, std::ios::app };
		output << "BEGIN\n";
		output << "cwd=" << QDir::currentPath().toStdString() << '\n';
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
			return 0;
		}
		if (aArguments.contains("fsck")) {
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
				.GitExecutable = std::filesystem::canonical(aExecutable),
				.IsolationRoot = std::filesystem::canonical(aRoot),
			});
		if (!made.has_value()) {
			const auto message = "construct installed Git adapter, failure="
				+ std::to_string(static_cast<unsigned int>(made.error()));
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

			const auto chain   = make_rename_chain_fixture(repository, aAlgorithm);
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
		const auto     fakeExec = root.Path / "fake-exec";
		std::filesystem::create_directories(fakeExec);
		std::filesystem::permissions(
			fakeExec,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto fakeGitPath = fakeExec
#if defined(Q_OS_WIN)
			/ "git.exe";
#else
			/ "git";
#endif
		std::filesystem::copy_file(
			QCoreApplication::applicationFilePath().toStdString(),
			fakeGitPath);
		std::filesystem::permissions(
			fakeGitPath,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);

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
		require(
			fake_index_request(
				*adapter,
				repository,
				operation,
				"BPR_BIG_ERROR")
					.error()
				== application::git_process_failure::standard_error_limit,
			"standard error limit");
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
					.GitExecutable = fakeGitPath,
					.IsolationRoot = root.Path,
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
				.GitExecutable = fakeGitPath,
				.IsolationRoot = root.Path,
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
				.GitExecutable = fakeGitPath,
				.IsolationRoot = root.Path,
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
					.GitExecutable = fakeGitPath,
					.IsolationRoot = root.Path,
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
			.GitExecutable = fakeGitPath,
			.IsolationRoot = root.Path,
		};
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
				.GitExecutable = executable,
				.IsolationRoot = std::filesystem::canonical(directoryIsolation),
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
				.GitExecutable = executable,
				.IsolationRoot = std::filesystem::canonical(fileIsolation),
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
				.GitExecutable = executable,
				.IsolationRoot = std::filesystem::canonical(contentIsolation),
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
				.GitExecutable = executable,
				.IsolationRoot = std::filesystem::canonical(policyIsolation),
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
}

auto main(int argc, char** argv) -> int
{
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

	run_real_git_contract();
	run_connectivity_contract();
	run_rename_parser_contract();
	run_real_hostile_canary_contract();
	run_hostile_process_contract();
	run_capability_probe_cleanup_contract();
	run_factory_cancellation_contract();
	run_input_validation_contract();
	run_isolation_symlink_contract();
	run_agent_workspace_parent_cleanup_contract();
	std::cout << "Git process contract passed\n";
	return 0;
}
