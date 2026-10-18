#include "bpr/adapters/git/installed_git_cache.hpp"

#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "windows_system_root.hpp"

#include <QByteArray>
#include <QCoreApplication>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <array>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(Q_OS_UNIX)
	#include <sys/stat.h>
	#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
	#include <qt_windows.h>
#endif

namespace {
	namespace application = bpr::application;
	namespace core        = bpr::core;
	namespace git_adapter = bpr::adapters::git;

	template <typename Value>
	concept exposes_path = requires(Value aValue) {
		aValue.Path;
	};

	template <typename Value>
	concept exposes_url = requires(Value aValue) {
		aValue.Url;
	};

	template <typename Value>
	concept exposes_credential = requires(Value aValue) {
		aValue.Credential;
	};

	static_assert(!exposes_path<application::completed_git_pack_receipt>);
	static_assert(!exposes_path<application::git_preimport_observation>);
	static_assert(!exposes_path<application::git_prepared_revision>);
	static_assert(!exposes_path<application::git_retained_ref_pair>);
	static_assert(!exposes_path<application::git_operation_recovery_request>);
	static_assert(!exposes_path<application::git_operation_recovery_observation>);
	static_assert(!exposes_url<application::completed_git_pack_receipt>);
	static_assert(!exposes_url<application::git_preimport_observation>);
	static_assert(!exposes_credential<application::completed_git_pack_receipt>);
	static_assert(!exposes_credential<application::git_preimport_observation>);

	inline constexpr std::string_view sha1_commit_hex = "826bb464b84352d60b3d7c56481a599a838d998d";
	inline constexpr std::string_view sha1_tree_hex   = "4ce9ffbf7ccd31debb0a343022131f0fde64e35d";
	inline constexpr std::string_view sha1_pack_hex   = "e248b08f09a75116fdb2397c928a84f6a93f6c34";
	inline constexpr std::string_view raw_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "https://fixture-token@remote.invalid/owner/repo.git\n";
	inline constexpr std::string_view redacted_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "<redacted-url>\n";

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
		temporary_root()
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation.
			Path = bpr::tests::make_scratch_directory("bpr-git-cache");
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

	[[nodiscard]] auto read_file(
		const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		require(static_cast<bool>(input), "open private test file");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ }
		};
	}

	auto write_file(
		const std::filesystem::path&        aPath,
		const std::span<const std::uint8_t> aBytes) -> void
	{
		auto output = std::ofstream{ aPath, std::ios::binary | std::ios::trunc };
		require(static_cast<bool>(output), "open private test file for write");
		output.write(
			reinterpret_cast<const char*>(aBytes.data()),
			static_cast<std::streamsize>(aBytes.size()));
		output.close();
		require(static_cast<bool>(output), "write private test file");
		std::filesystem::permissions(
			aPath,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
	}

	auto write_file(
		const std::filesystem::path& aPath,
		const std::string_view       aText) -> void
	{
		write_file(
			aPath,
			std::span{
				reinterpret_cast<const std::uint8_t*>(aText.data()),
				aText.size() });
	}

	// Exact residue evidence: an operation that fails or aborts has to leave a
	// directory it touched byte-for-name identical, quarantine temporaries
	// included, which a single existence check cannot say.
	[[nodiscard]] auto sorted_directory_names(
		const std::filesystem::path& aDirectory) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto& entry :
			 std::filesystem::directory_iterator{ aDirectory }) {
			names.push_back(entry.path().filename().string());
		}
		std::ranges::sort(names);
		return names;
	}

	auto make_tree_private(
		const std::filesystem::path& aRoot) -> void
	{
		std::filesystem::permissions(
			aRoot,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::error_code error;
		auto            iterator = std::filesystem::recursive_directory_iterator{
			aRoot,
			std::filesystem::directory_options::none,
			error
		};
		const auto end = std::filesystem::recursive_directory_iterator{ };
		while (!error && iterator != end) {
			const auto path   = iterator->path();
			const auto status = std::filesystem::symlink_status(path, error);
			require(!error, "inspect copied private tree entry");
			require(
				!std::filesystem::is_symlink(status),
				"copied private tree contains no links");
			std::filesystem::permissions(
				path,
				status.type()
						== std::filesystem::file_type::directory
					? std::filesystem::perms::owner_all
					: std::filesystem::perms::owner_read
						| std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace,
				error);
			require(!error, "make copied tree entry private");
			iterator.increment(error);
		}
		require(!error, "walk copied private tree");
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aValue) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aValue);
		return Identity{ bytes };
	}

	[[nodiscard]] auto token(const std::uint8_t aValue)
		-> std::array<std::uint8_t, 32U>
	{
		auto result = std::array<std::uint8_t, 32U>{ };
		result.fill(aValue);
		return result;
	}

	[[nodiscard]] constexpr auto nibble(const char aValue) -> std::uint8_t
	{
		return aValue <= '9'
			? static_cast<std::uint8_t>(aValue - '0')
			: static_cast<std::uint8_t>(aValue - 'a' + 10);
	}

	[[nodiscard]] auto pack_oid(
		const std::string_view aText) -> application::git_pack_oid
	{
		auto result = application::git_pack_oid{
			.Algorithm = core::git_hash_algorithm::sha1
		};
		for (std::size_t index = 0U; index < aText.size() / 2U; ++index) {
			result.Digest[index] = static_cast<std::uint8_t>(
				static_cast<std::uint8_t>(
					nibble(aText[index * 2U]) << 4U)
				| nibble(aText[index * 2U + 1U]));
		}
		require(application::valid(result), "valid pack fixture identity");
		return result;
	}

	[[nodiscard]] auto request(
		const std::uint8_t aCacheByte,
		const std::uint8_t aScopeByte)
		-> application::git_cache_open_request
	{
		auto result = application::git_cache_open_request{
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 2U },
				.ProfileRevision = core::github_profile_revision{ 3U },
				.Host            = core::git_host_id{ 4U },
				.HostAuthority   = identity<core::host_authority_fingerprint>(5U),
				.Account         = core::github_account_id{ 6U },
				.Viewer          = core::github_viewer_actor_id{ 7U },
				.Repository      = core::repository_id{ 8U },
				.Cache           = identity<core::repository_cache_scope_id>(
					aCacheByte),
				.PullRequest = core::pull_request_id{ 9U },
				.Number      = core::pull_request_number{ 10U },
				.Scope       = identity<
					core::pull_request_authority_scope_id>(
					aScopeByte),
			},
			.ObjectFormat = core::git_hash_algorithm::sha1,
		};
		require(application::valid(result), "valid cache request fixture");
		return result;
	}

	auto append_u64(
		std::vector<std::uint8_t>& aOutput,
		const std::uint64_t        aValue) -> void
	{
		for (unsigned int shift = 56U;; shift -= 8U) {
			aOutput.push_back(
				static_cast<std::uint8_t>(aValue >> shift));
			if (shift == 0U) {
				break;
			}
		}
	}

	[[nodiscard]] auto deletion_tombstone(
		const application::git_cache_open_request& aCacheRequest,
		const std::array<std::uint8_t, 32U>&       aIntent,
		const std::filesystem::path&               aRepository,
		const std::filesystem::path&               aLock)
		-> std::vector<std::uint8_t>
	{
		constexpr std::array<std::uint8_t, 12U> tombstoneMagic{
			0x42U,
			0x50U,
			0x52U,
			0x44U,
			0x45U,
			0x4cU,
			0x45U,
			0x54U,
			0x45U,
			0x00U,
			0x00U,
			0x02U,
		};
		constexpr std::array<std::uint8_t, 12U> manifestMagic{
			0x42U,
			0x50U,
			0x52U,
			0x43U,
			0x41U,
			0x43U,
			0x48U,
			0x45U,
			0x00U,
			0x00U,
			0x00U,
			0x01U,
		};
		auto result = std::vector<std::uint8_t>{ };
		result.insert(
			result.end(),
			tombstoneMagic.begin(),
			tombstoneMagic.end());
		result.insert(
			result.end(),
			manifestMagic.begin(),
			manifestMagic.end());
		result.push_back(
			aCacheRequest.ObjectFormat == core::git_hash_algorithm::sha1
				? 1U
				: 2U);
		append_u64(result, aCacheRequest.Authority.Profile.value());
		append_u64(
			result,
			aCacheRequest.Authority.ProfileRevision.value());
		append_u64(result, aCacheRequest.Authority.Host.value());
		result.insert(
			result.end(),
			aCacheRequest.Authority.HostAuthority.bytes().begin(),
			aCacheRequest.Authority.HostAuthority.bytes().end());
		append_u64(result, aCacheRequest.Authority.Account.value());
		append_u64(result, aCacheRequest.Authority.Repository.value());
		result.insert(
			result.end(),
			aCacheRequest.Authority.Cache.bytes().begin(),
			aCacheRequest.Authority.Cache.bytes().end());
		result.insert(result.end(), aIntent.begin(), aIntent.end());
		const auto appendFileIdentity =
			[&](const std::filesystem::path& aPath) {
#if defined(Q_OS_UNIX)
				struct stat information{ };
				require(
					::lstat(aPath.c_str(), &information) == 0,
					"inspect deletion fixture identity");
				append_u64(
					result,
					static_cast<std::uint64_t>(information.st_dev));
				append_u64(result, 0U);
				append_u64(
					result,
					static_cast<std::uint64_t>(information.st_ino));
#elif defined(Q_OS_WIN)
				const auto handle = CreateFileW(
					aPath.c_str(),
					FILE_READ_ATTRIBUTES,
					FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					nullptr,
					OPEN_EXISTING,
					FILE_FLAG_OPEN_REPARSE_POINT
						| FILE_FLAG_BACKUP_SEMANTICS,
					nullptr);
				require(
					handle != INVALID_HANDLE_VALUE,
					"open deletion fixture identity");
				BY_HANDLE_FILE_INFORMATION information{ };
				require(
					GetFileInformationByHandle(handle, &information) != 0,
					"inspect deletion fixture identity");
				static_cast<void>(CloseHandle(handle));
				append_u64(
					result,
					static_cast<std::uint64_t>(
						information.dwVolumeSerialNumber));
				append_u64(
					result,
					static_cast<std::uint64_t>(
						information.nFileIndexHigh));
				append_u64(
					result,
					static_cast<std::uint64_t>(
						information.nFileIndexLow));
#endif
			};
		appendFileIdentity(aRepository);
		appendFileIdentity(aLock);
		return result;
	}

	[[nodiscard]] auto lower_hex(
		const std::span<const std::uint8_t> aBytes) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       result = std::string{ };
		result.reserve(aBytes.size() * 2U);
		for (const auto byte : aBytes) {
			result.push_back(digits[byte >> 4U]);
			result.push_back(digits[byte & 0x0fU]);
		}
		return result;
	}

	[[nodiscard]] auto repository_path(
		const std::filesystem::path&               aRoot,
		const application::git_cache_open_request& aCacheRequest)
		-> std::filesystem::path
	{
		return aRoot / "repositories" / "v1"
			/ (lower_hex(aCacheRequest.Authority.Cache.bytes()) + ".git");
	}

	struct fixture_process_result
	{
		int        ExitCode = 0;
		QByteArray Output;
	};

	[[nodiscard]] auto run_fixture_git(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot,
		const QStringList&           aArguments,
		const QByteArray&            aInput       = { },
		const bool                   aLlowFailure = false) -> fixture_process_result
	{
		auto environment = QProcessEnvironment{ };
		environment.insert(
			"HOME",
			QString::fromStdString((aRoot / "home").string()));
		environment.insert(
			"XDG_CONFIG_HOME",
			QString::fromStdString((aRoot / "xdg").string()));
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert("GIT_CONFIG_GLOBAL", "/dev/null");
		environment.insert("GIT_AUTHOR_NAME", "BetterPullReview Fixture");
		environment.insert("GIT_AUTHOR_EMAIL", "fixture.invalid@example.invalid");
		environment.insert("GIT_AUTHOR_DATE", "2001-01-01T00:00:00Z");
		environment.insert("GIT_COMMITTER_NAME", "BetterPullReview Fixture");
		environment.insert(
			"GIT_COMMITTER_EMAIL",
			"fixture.invalid@example.invalid");
		environment.insert("GIT_COMMITTER_DATE", "2001-01-01T00:00:00Z");
		environment.insert("LC_ALL", "C");
		environment.insert("TZ", "UTC");

		auto process = QProcess{ };
		// Not an avoidable copy: the Windows branch below prepends to it. On
		// every other platform that branch is preprocessed away, so the check
		// sees a local that is only read and asks for a reference - which
		// would then not compile on the one platform that writes to it. The
		// suppression is per-platform truth, not a preference. BPR-0458.
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
		auto fixtureArguments = aArguments;
#if defined(Q_OS_WIN)
		// The refs this fixture writes cross MAX_PATH the same way the
		// adapter's do; the adapter's isolation carries core.longpaths for
		// Git, and this helper runs Git with its own environment, so it says
		// so itself (BPR-0437).
		fixtureArguments.prepend(QStringLiteral("core.longpaths=true"));
		fixtureArguments.prepend(QStringLiteral("-c"));
#endif
		process.setProgram(QString::fromStdString(aExecutable.string()));
		process.setArguments(fixtureArguments);
		process.setWorkingDirectory(QString::fromStdString(aRoot.string()));
		process.setProcessEnvironment(environment);
		process.start();
		require(process.waitForStarted(5'000), "start fixture Git");
		if (!aInput.isEmpty()) {
			require(
				process.write(aInput) == aInput.size(),
				"write complete fixture Git input");
		}
		process.closeWriteChannel();
		require(process.waitForFinished(30'000), "finish fixture Git");
		const auto output = process.readAllStandardOutput();
		const auto error  = process.readAllStandardError();
		require(
			output.size() <= 1024 * 1024
				&& error.size() <= 64 * 1024,
			"bound fixture Git output");
		if (!aLlowFailure && process.exitCode() != 0) {
			// Name the command and what Git said before failing; a bare
			// verdict sent this investigation through a probe rebuild
			// (BPR-0437).
			std::cerr << "fixture Git failed, exit=" << process.exitCode()
					  << " argv=" << aArguments.join(' ').toStdString()
					  << " stderr=[" << error.toStdString() << "]\n";
		}
		require(
			aLlowFailure || process.exitCode() == 0,
			"fixture Git exits successfully");
		return {
			.ExitCode = process.exitCode(),
			.Output   = output,
		};
	}

	[[nodiscard]] auto trimmed_hex(
		const fixture_process_result& aResult,
		const qsizetype               aExpectedCharacters) -> std::string
	{
		auto output = aResult.Output;
		require(
			output.endsWith('\n'),
			"fixture Git identity has newline terminator");
		output.chop(1);
		require(
			output.size() == aExpectedCharacters,
			"fixture Git identity has exact width");
		return output.toStdString();
	}

	struct force_push_fixture
	{
		std::filesystem::path     Remote;
		std::vector<std::uint8_t> NewPack;
		core::git_commit_oid      NewCommit;
		core::git_tree_oid        NewTree;
	};

	[[nodiscard]] auto make_force_push_fixture(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot) -> force_push_fixture
	{
		std::filesystem::create_directories(aRoot / "home");
		std::filesystem::create_directories(aRoot / "xdg");
		std::filesystem::permissions(
			aRoot / "home",
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::filesystem::permissions(
			aRoot / "xdg",
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto remote = aRoot / "fixture-remote.git";
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{
				"init",
				"--bare",
				"--quiet",
				"--object-format=sha1",
				QString::fromStdString(remote.string()),
			}));
		const auto gitDir = "--git-dir=" + QString::fromStdString(remote.string());
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{ gitDir, "index-pack", "--stdin" },
			QByteArray{
				reinterpret_cast<const char*>(sha1_pack.data()),
				static_cast<qsizetype>(sha1_pack.size()) }));
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{
				gitDir,
				"update-ref",
				"refs/heads/main",
				QString::fromUtf8(sha1_commit_hex.data()),
			}));

		const auto blob = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{ gitDir, "hash-object", "-w", "--stdin" },
				QByteArray{ "force-pushed content\n" }),
			40);
		const auto treeInput = QByteArray{
			"100644 blob " + QByteArray::fromStdString(blob)
			+ "\tfile.txt\n"
		};
		const auto treeText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{ gitDir, "mktree" },
				treeInput),
			40);
		const auto commitText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{
					gitDir,
					"commit-tree",
					QString::fromStdString(treeText),
				},
				QByteArray{ "force push\n" }),
			40);
		const auto pack = run_fixture_git(
			aExecutable,
			aRoot,
			{ gitDir, "pack-objects", "--stdout", "--revs" },
			QByteArray::fromStdString(commitText + "\n"));
		require(
			pack.Output.size() > 32,
			"force-push fixture emits a complete pack");

		const auto commit = core::parse_git_commit_oid(commitText);
		const auto tree   = core::parse_git_tree_oid(treeText);
		require(
			commit.has_value() && tree.has_value(),
			"parse force-push fixture identities");
		auto packBytes = std::vector<std::uint8_t>(
			static_cast<std::size_t>(pack.Output.size()));
		std::transform(
			pack.Output.begin(),
			pack.Output.end(),
			packBytes.begin(),
			[](const char aValue) {
				return static_cast<std::uint8_t>(
					static_cast<unsigned char>(aValue));
			});
		return {
			.Remote    = remote,
			.NewPack   = std::move(packBytes),
			.NewCommit = *commit,
			.NewTree   = *tree,
		};
	}

	auto force_push_and_prune(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot,
		const force_push_fixture&    aFixture) -> void
	{
		const auto gitDir      = "--git-dir=" + QString::fromStdString(aFixture.Remote.string());
		const auto newHead     = core::canonical_hex(aFixture.NewCommit);
		const auto newHeadView = newHead.view();
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{
				gitDir,
				"update-ref",
				"refs/heads/main",
				QString::fromUtf8(
					newHeadView.data(),
					static_cast<qsizetype>(newHeadView.size())),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{ gitDir, "reflog", "expire", "--expire=now", "--all" }));
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{
				gitDir,
				"-c",
				"gc.cruftPacks=false",
				"gc",
				"--prune=now",
			}));
		const auto oldPresence = run_fixture_git(
			aExecutable,
			aRoot,
			{
				gitDir,
				"cat-file",
				"-e",
				QString::fromStdString(
					std::string{ sha1_commit_hex } + "^{commit}"),
			},
			{ },
			true);
		require(
			oldPresence.ExitCode != 0,
			"fixture remote removes and prunes the already-retained old head");
	}

	[[nodiscard]] auto make_sha256_fixture(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot) -> force_push_fixture
	{
		const auto remote = aRoot / "sha256-fixture.git";
		static_cast<void>(run_fixture_git(
			aExecutable,
			aRoot,
			{
				"init",
				"--bare",
				"--quiet",
				"--object-format=sha256",
				QString::fromStdString(remote.string()),
			}));
		const auto gitDir = "--git-dir=" + QString::fromStdString(remote.string());
		const auto blob   = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{ gitDir, "hash-object", "-w", "--stdin" },
				QByteArray{ "sha256 cache content\n" }),
			64);
		const auto treeInput = QByteArray{
			"100644 blob " + QByteArray::fromStdString(blob)
			+ "\tsha256.txt\n"
		};
		const auto treeText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{ gitDir, "mktree" },
				treeInput),
			64);
		const auto commitText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{
					gitDir,
					"commit-tree",
					QString::fromStdString(treeText),
				},
				QByteArray{ "sha256 cache\n" }),
			64);
		const auto pack = run_fixture_git(
			aExecutable,
			aRoot,
			{ gitDir, "pack-objects", "--stdout", "--revs" },
			QByteArray::fromStdString(commitText + "\n"));
		const auto commit = core::parse_git_commit_oid(commitText);
		const auto tree   = core::parse_git_tree_oid(treeText);
		require(
			pack.Output.size() > 44 && commit.has_value()
				&& tree.has_value(),
			"build complete SHA-256 pack fixture");
		auto packBytes = std::vector<std::uint8_t>(
			static_cast<std::size_t>(pack.Output.size()));
		std::transform(
			pack.Output.begin(),
			pack.Output.end(),
			packBytes.begin(),
			[](const char aValue) {
				return static_cast<std::uint8_t>(
					static_cast<unsigned char>(aValue));
			});
		return {
			.Remote    = remote,
			.NewPack   = std::move(packBytes),
			.NewCommit = *commit,
			.NewTree   = *tree,
		};
	}

	[[nodiscard]] auto make_descendant_fixture(
		const std::filesystem::path& aExecutable,
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aRemote) -> force_push_fixture
	{
		const auto gitDir   = "--git-dir=" + QString::fromStdString(aRemote.string());
		const auto treeText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{
					gitDir,
					"rev-parse",
					QString::fromStdString(
						std::string{ sha1_commit_hex } + "^{tree}"),
				}),
			40);
		const auto commitText = trimmed_hex(
			run_fixture_git(
				aExecutable,
				aRoot,
				{
					gitDir,
					"commit-tree",
					QString::fromStdString(treeText),
					"-p",
					QString::fromUtf8(sha1_commit_hex.data()),
				},
				QByteArray{ "descendant upload\n" }),
			40);
		const auto pack = run_fixture_git(
			aExecutable,
			aRoot,
			{ gitDir, "pack-objects", "--stdout", "--revs" },
			QByteArray::fromStdString(commitText + "\n"));
		const auto commit = core::parse_git_commit_oid(commitText);
		const auto tree   = core::parse_git_tree_oid(treeText);
		require(
			pack.Output.size() > 32 && commit.has_value()
				&& tree.has_value(),
			"build complete descendant pack fixture");
		auto packBytes = std::vector<std::uint8_t>(
			static_cast<std::size_t>(pack.Output.size()));
		std::transform(
			pack.Output.begin(),
			pack.Output.end(),
			packBytes.begin(),
			[](const char aValue) {
				return static_cast<std::uint8_t>(
					static_cast<unsigned char>(aValue));
			});
		return {
			.Remote    = aRemote,
			.NewPack   = std::move(packBytes),
			.NewCommit = *commit,
			.NewTree   = *tree,
		};
	}

	auto append_pack(
		application::git_pack_stream_sink&  aSink,
		const std::span<const std::uint8_t> aBytes) -> void
	{
		const auto all        = std::as_bytes(aBytes);
		const auto firstSize  = std::min<std::size_t>(17U, all.size());
		const auto middleSize = std::min<std::size_t>(
			101U,
			all.size() - firstSize);
		const auto first = aSink.append(all.first(firstSize), { });
		require(first.has_value(), "append first pack fragment");
		const auto middle = aSink.append(all.subspan(firstSize, middleSize), { });
		require(middle.has_value(), "append middle pack fragment");
		const auto last = aSink.append(all.subspan(firstSize + middleSize), { });
		require(last.has_value(), "append final pack fragment");
	}

	auto verify_receiving_ownership_recovery(
		application::git_cache_port& aCache,
		const std::filesystem::path& aRoot) -> void
	{
		for (const auto completed : { false, true }) {
			const auto seed         = static_cast<std::uint8_t>(completed ? 0xbbU : 0xb9U);
			const auto cacheRequest = request(seed, static_cast<std::uint8_t>(seed + 1U));
			auto       opened       = aCache.open(cacheRequest, { });
			require(opened.has_value(), "open receiving recovery cache");
			const auto  refusedOperation = identity<core::epoch_operation_id>(static_cast<std::uint8_t>(seed + 20U));
			std::size_t refusedWitnesses = 0U;
			const auto  refused          = (*opened)->create_pack_sink(refusedOperation, sha1_pack.size(), { },
																	   [&](const application::git_spool_ownership& aOwnership) {
																 ++refusedWitnesses;
																 require(application::valid(aOwnership) && aOwnership.Operation == refusedOperation
																			 && aOwnership.MaximumBytes == sha1_pack.size(),
																		 "receiving witness exposes exact operation, identity and bound");
																 return false;
																	   });
			require(!refused && refusedWitnesses == 1U
						&& std::filesystem::is_empty(aRoot / "incoming" / "v1"),
					"refused receiving witness cleans its empty file without deadlocking");
			const auto                                      operation = identity<core::epoch_operation_id>(seed);
			std::optional<application::git_spool_ownership> ownership;
			auto                                            sink = (*opened)->create_pack_sink(operation, sha1_pack.size(), { },
																							   [&](const application::git_spool_ownership& aOwnership) { ownership = aOwnership; return true; });
			require(sink && ownership, "receiving witness admits bounded payload");
			if (completed) {
				append_pack(**sink, sha1_pack);
				const auto receipt = (*sink)->finish({ });
				require(receipt && receipt->Spool.Identity == ownership->Identity, "completed publication preserves the witnessed identity");
			} else {
				require((*sink)->append(std::as_bytes(std::span{ sha1_pack }.first(17U)), { }).has_value(), "receive a partial payload");
			}
			const auto name     = (completed ? "" : ".") + lower_hex(operation.bytes()) + (completed ? ".pack.part" : ".pack.receiving");
			const auto spool    = aRoot / "incoming" / "v1" / name;
			const auto original = aRoot / ("original-" + std::to_string(seed));
			std::filesystem::rename(spool, original);
			write_file(spool, "unrelated fixture artifact");
			sink->reset();
			opened->reset();
			opened = aCache.open(cacheRequest, { });
			require(opened.has_value(), "reopen receiving cache without process-local spool registrations");
			const auto commit = core::parse_git_commit_oid(sha1_commit_hex);
			require(commit.has_value(), "fixture commit parses");
			const auto recovery = application::git_operation_recovery_request{
				.Operation = operation,
				.Scope     = cacheRequest.Authority.Scope,
				.Ordinal   = core::epoch_ordinal{ 1 },
				.Base      = *commit,
				.Head      = *commit,
				.Receiving = ownership,
			};
			const auto observed = (*opened)->inspect_operation_recovery(recovery, { });
			const auto rejected = (*opened)->abort_recovered_operation(recovery, { });
			require(observed && observed->Spool == application::git_recovery_match::mismatch
						&& !rejected && rejected.error() == application::git_cache_failure::repair_required
						&& std::filesystem::file_size(spool) == std::string_view{ "unrelated fixture artifact" }.size()
						&& std::filesystem::file_size(original) == (completed ? sha1_pack.size() : 17U),
					"receiving ownership refuses substituted partial or completed artifacts and preserves both files");
			std::filesystem::remove(spool);
			std::filesystem::rename(original, spool);
			require((*opened)->abort_recovered_operation(recovery, { }).has_value()
						&& !std::filesystem::exists(spool)
						&& (*opened)->abort_recovered_operation(recovery, { }).has_value(),
					"restored exact ownership permits idempotent cleanup after reopen");
			if (!completed) {
				auto unowned = recovery;
				unowned.Receiving.reset();
				write_file(spool, "");
				const auto empty = (*opened)->inspect_operation_recovery(unowned, { });
				require(empty && empty->Spool == application::git_recovery_match::unowned_empty
							&& (*opened)->abort_recovered_operation(unowned, { })
							&& (*opened)->abort_recovered_operation(unowned, { })
							&& std::filesystem::exists(spool) && std::filesystem::file_size(spool) == 0U
							&& !(*opened)->create_pack_sink(operation, sha1_pack.size(), { }),
						"unowned empty receiving file is preserved on abort and cannot be reused");
				require(!(*opened)->abort_recovered_operation(recovery, { }) && std::filesystem::exists(spool),
						"an empty substitute does not override committed ownership");
				write_file(spool, "unowned payload");
				require(!(*opened)->abort_recovered_operation(unowned, { })
							&& std::filesystem::file_size(spool) == std::string_view{ "unowned payload" }.size(),
						"unowned receiving payload remains ambiguous and untouched");
				std::filesystem::remove(spool);

				const auto incoming = aRoot / "incoming" / "v1";
				for (std::uint32_t index = 0U; index < application::maximum_git_cache_pack_count; ++index) {
					write_file(incoming / ("quota-canary-" + std::to_string(index)), "");
				}
				const auto limitedOperation = identity<core::epoch_operation_id>(0xefU);
				const auto limited          = (*opened)->create_pack_sink(limitedOperation, sha1_pack.size(), { });
				require(!limited && limited.error() == application::git_cache_failure::quota_exceeded,
						"incoming entry budget refuses admission without deleting unknown files");
				for (std::uint32_t index = 0U; index < application::maximum_git_cache_pack_count; ++index) {
					const auto canary = incoming / ("quota-canary-" + std::to_string(index));
					require(std::filesystem::file_size(canary) == 0U && std::filesystem::remove(canary), "quota refusal preserves each unrelated entry");
				}
				auto admitted = (*opened)->create_pack_sink(limitedOperation, sha1_pack.size(), { });
				require(admitted.has_value(), "admission recovers after fixture removes its own quota canaries");
				admitted->reset();
			}
			opened->reset();
			const auto deletion = application::git_cache_delete_authorization{
				.ApplicationSession = cacheRequest.ApplicationSession,
				.Cache              = cacheRequest.Authority.Cache,
				.DurableIntentToken = token(seed),
			};
			require(aCache.delete_cache(cacheRequest, deletion, { }).has_value(), "remove receiving recovery fixture cache");
		}
	}

	auto verify_refused_installation_witness(
		application::git_cache_port& aCache,
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aExecutable) -> void
	{
		const auto cacheRequest = request(0xb7U, 0xb8U);
		auto       opened       = aCache.open(cacheRequest, { });
		require(opened.has_value(), "open installation refusal cache");
		auto&      session   = **opened;
		const auto operation = identity<core::epoch_operation_id>(0xb7U);
		auto       sink      = session.create_pack_sink(operation, sha1_pack.size(), { });
		require(sink.has_value(), "create installation refusal spool");
		append_pack(**sink, sha1_pack);
		const auto receipt = (*sink)->finish({ });
		require(receipt.has_value(), "finish installation refusal spool");
		sink->reset();
		const auto inventory = session.observe_preimport(*receipt, { });
		const auto commit    = core::parse_git_commit_oid(sha1_commit_hex);
		require(inventory.has_value() && commit.has_value(), "observe installation refusal inventory");
		const auto  repository     = repository_path(aRoot, cacheRequest);
		const auto  spool          = aRoot / "incoming" / "v1" / (lower_hex(operation.bytes()) + ".pack.part");
		std::size_t witnesses      = 0U;
		const auto  prepareRequest = application::git_prepare_revision_request{
			.Receipt       = *receipt,
			.Inventory     = *inventory,
			.Authorization = {
				.CatalogIncarnation      = identity<core::epoch_catalog_incarnation_id>(0xb7U),
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
				.Operation               = operation,
				.ExpectedPack            = receipt->ExpectedPack,
				.ReceiptToken            = receipt->OpaqueToken,
				.InventoryToken          = inventory->OpaqueToken,
				.DurableRecordToken      = token(0xb7U),
			},
			.Base = *commit,
			.Head = *commit,
		};
		const auto refuseInstallation = [&](const application::git_prepared_revision& aInstallation) {
			++witnesses;
			require(application::valid(aInstallation) && aInstallation.Pack == receipt->ExpectedPack,
					"refused witness receives complete installation facts");
			return false;
		};
		const auto prepared = session.prepare_revision(prepareRequest, { }, refuseInstallation);
		const auto refs     = run_fixture_git(aExecutable, aRoot,
											  { "--git-dir=" + QString::fromStdString(repository.string()), "for-each-ref" });
		const auto pack     = repository / "objects" / "pack" / ("pack-" + std::string{ sha1_pack_hex } + ".pack");
		const auto index    = repository / "objects" / "pack" / ("pack-" + std::string{ sha1_pack_hex } + ".idx");
		require(!prepared && prepared.error() == application::git_cache_failure::authorization_mismatch
					&& witnesses == 1U && refs.Output.isEmpty()
					&& std::filesystem::file_size(spool) == sha1_pack.size()
					&& std::filesystem::file_size(pack) == sha1_pack.size()
					&& std::filesystem::file_size(index) > 0U,
				"refusal preserves possibly journaled spool and install artifacts without staging refs");
		require(session.abort_operation(operation, { }).has_value(), "clean exact refusal fixture operation after inspection");
		opened->reset();
		require(aCache.delete_cache(cacheRequest, {
													  .ApplicationSession = cacheRequest.ApplicationSession,
													  .Cache              = cacheRequest.Authority.Cache,
													  .DurableIntentToken = token(0xb8U),
												  },
									{ })
					.has_value(),
				"delete isolated installation refusal cache after verification");
	}

	// Every `objects/pack` directory under `aRoot`, sorted.
	[[nodiscard]] auto pack_directories(const std::filesystem::path& aRoot) -> std::vector<std::filesystem::path>
	{
		auto directories = std::vector<std::filesystem::path>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_directory() && entry.path().filename() == "pack" && entry.path().parent_path().filename() == "objects") {
				directories.push_back(entry.path());
			}
		}
		std::ranges::sort(directories);
		return directories;
	}

	// Every file in one pack directory with index-pack's temporary name, sorted.
	[[nodiscard]] auto index_residue(const std::filesystem::path& aPackDirectory) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto& entry : std::filesystem::directory_iterator{ aPackDirectory }) {
			const auto name = entry.path().filename().string();
			if (name.starts_with("tmp_pack_") || name.starts_with("tmp_idx_")) {
				names.push_back(name);
			}
		}
		std::ranges::sort(names);
		return names;
	}

	auto verify_sparse_residency(
		application::git_cache_port& aCache,
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aExecutable) -> void
	{
		using residency   = application::git_revision_residency;
		const auto source = aRoot / "sparse-source.git";
		static_cast<void>(run_fixture_git(aExecutable, aRoot,
										  { "init", "--bare", "--quiet", "--object-format=sha1", QString::fromStdString(source.string()) }));
		const auto gitDir     = "--git-dir=" + QString::fromStdString(source.string());
		const auto blobText   = trimmed_hex(run_fixture_git(aExecutable, aRoot,
															{ gitDir, "hash-object", "-w", "--stdin" }, QByteArray{ "selected source\n" }),
											40);
		const auto nestedTree = trimmed_hex(run_fixture_git(aExecutable, aRoot,
															{ gitDir, "mktree" }, QByteArray::fromStdString("100644 blob " + blobText + "\tnested.txt\n")),
											40);
		const auto treeText   = trimmed_hex(run_fixture_git(aExecutable, aRoot,
															{ gitDir, "mktree" }, QByteArray::fromStdString("100644 blob " + blobText + "\tfile.txt\n040000 tree " + nestedTree + "\tsubdir\n")),
											40);
		const auto parentText = trimmed_hex(run_fixture_git(aExecutable, aRoot,
															{ gitDir, "commit-tree", QString::fromStdString(treeText) }, QByteArray{ "parent\n" }),
											40);
		const auto headText   = trimmed_hex(run_fixture_git(aExecutable, aRoot,
															{ gitDir, "commit-tree", QString::fromStdString(treeText), "-p", QString::fromStdString(parentText) }, QByteArray{ "head\n" }),
											40);
		const auto head       = *core::parse_git_commit_oid(headText);
		const auto tree       = *core::parse_git_tree_oid(treeText);
		const auto blob       = *core::parse_git_blob_oid(blobText);
		const auto parent     = *core::parse_git_commit_oid(parentText);
		const auto makePack   = [&](const std::string& aObjects) {
			const auto packed = run_fixture_git(aExecutable, aRoot,
												{ gitDir, "pack-objects", "--stdout" }, QByteArray::fromStdString(aObjects));
			return std::vector<std::uint8_t>{ packed.Output.begin(), packed.Output.end() };
		};
		const auto sparsePack = makePack(headText + "\n" + treeText + "\n");
		// Courses 5-8 import the sparse pack strictly first. Index-pack dies and
		// leaves tmp_pack_XXXXXX; 6-8 plant entries the cleanup must leave (BPR-0851).
		for (std::uint8_t course = 0U; course != 9U; ++course) {
			const auto cacheRequest = request(static_cast<std::uint8_t>(0xd0U + course), static_cast<std::uint8_t>(0xe0U + course));
			const auto storesBefore = course >= 5U ? pack_directories(aRoot) : std::vector<std::filesystem::path>{ };
			auto       opened       = aCache.open(cacheRequest, { });
			require(opened.has_value(), "open sparse fixture cache");
			// Courses 5-8 inspect only their own object store. The caches of earlier
			// refusal courses stay under `aRoot`, and must not stand in for this one.
			auto coursePackDirectory = std::filesystem::path{ };
			if (course >= 5U) {
				auto added = std::vector<std::filesystem::path>{ };
				std::ranges::set_difference(pack_directories(aRoot), storesBefore, std::back_inserter(added));
				require(added.size() == 1U, "the course's cache adds exactly one object store");
				coursePackDirectory = added.front();
			}
			const auto operation = identity<core::epoch_operation_id>(static_cast<std::uint8_t>(0xc0U + course));
			// Course 2 omits the root tree; course 3 supplies a blob as the commit.
			const auto bytes = course == 2U ? makePack(headText + "\n") : course == 3U ? makePack(blobText + "\n" + treeText + "\n")
																					   : sparsePack;
			auto       sink  = (*opened)->create_pack_sink(operation, bytes.size(), { });
			require(sink.has_value(), "create sparse fixture spool");
			append_pack(**sink, bytes);
			const auto receipt = (*sink)->finish({ });
			require(receipt.has_value(), "finish sparse fixture spool");
			sink->reset();
			const auto inventory = (*opened)->observe_preimport(*receipt, { });
			require(inventory.has_value(), "observe sparse fixture inventory");
			auto prepareRequest = application::git_prepare_revision_request{
				.Receipt       = *receipt,
				.Inventory     = *inventory,
				.Authorization = {
					.CatalogIncarnation      = identity<core::epoch_catalog_incarnation_id>(0xd0U),
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
					.Operation               = operation,
					.ExpectedPack            = receipt->ExpectedPack,
					.ReceiptToken            = receipt->OpaqueToken,
					.InventoryToken          = inventory->OpaqueToken,
					.DurableRecordToken      = token(0xd0U),
				},
				.Base = course == 3U ? *core::parse_git_commit_oid(blobText) : head,
				.Head = head,
				// [[BPR-1572]]. Course 0's prior head is the head itself: only the
				// base moved, so there is no lineage to claim.
				.PriorHead = course == 4U ? std::nullopt : std::optional{ course == 0U ? head : parent },
			};
			std::optional<application::git_prepared_revision> witnessed;
			const auto                                        witness = [&](const application::git_prepared_revision& aPrepared) {
				witnessed = aPrepared;
				return course != 1U;
			};
			if (course == 0U) {
				require(!(*opened)->prepare_sparse_revision(prepareRequest, { }, { }), "sparse import requires durable witness");
				auto derivedRequest                 = prepareRequest;
				derivedRequest.DeriveComparisonBase = true;
				require(!(*opened)->prepare_sparse_revision(derivedRequest, { }, witness), "sparse import refuses derived ancestry");
				auto wrongAuthority                         = prepareRequest;
				wrongAuthority.Authorization.InventoryToken = token(0xffU);
				require(!(*opened)->prepare_sparse_revision(wrongAuthority, { }, witness), "sparse import refuses mismatched operation authority");
				std::stop_source cancelled;
				cancelled.request_stop();
				const auto rejected = (*opened)->prepare_sparse_revision(prepareRequest, cancelled.get_token(), witness);
				require(!rejected && rejected.error() == application::git_cache_failure::cancelled && !witnessed,
						"cancelled sparse import neither installs nor witnesses");
			}

			if (course >= 5U) {
				const auto marker = aRoot
					/ (course == 6U       ? "inject-cache-preexisting-index-residue"
						   : course == 7U ? "inject-cache-extra-pack-during-index"
										  : "inject-cache-foreign-index-residue-during-index");
				if (course != 5U) {
					write_file(marker, "inject");
				}
				const auto strict = (*opened)->prepare_revision(prepareRequest, { }, witness);
				if (course != 5U) {
					std::filesystem::remove(marker);
				}
				const auto residue = index_residue(coursePackDirectory);
				std::cout << "SPARSE_STRICT_REFUSAL:" << static_cast<int>(course) << ':'
						  << (strict ? -1 : static_cast<int>(strict.error().Code)) << ':' << residue.size() << '\n';
				require(!strict && !witnessed, "strict complete import refuses sparse pack without publication");
				if (course == 5U) {
					require(strict.error() == application::git_cache_failure::process_failure && residue.empty(),
							"a refused strict import reports index-pack's failure and leaves no temporary file");
				} else {
					require(strict.error() == application::git_cache_failure::repair_required,
							"a store changed beyond index-pack's own residue still needs repair");
					// Exactly what this course planted remains: Git's own residue is gone.
					auto planted = std::vector<std::string>{ };
					if (course == 6U) {
						planted.emplace_back("tmp_pack_BPRpre");
					}
					if (course == 8U) {
						planted.insert(planted.end(), { "tmp_idx_BPRln2", "tmp_pack_BPRln1" });
#if !defined(_WIN32)
						planted.emplace_back("tmp_idx_BPRshr");
#endif
						std::ranges::sort(planted);
					}
					require(residue == planted,
							"the cleanup removed Git's residue and left every temporary name this course planted");
					const auto extraPack = "pack-" + std::string(40U, 'e');
					require(std::filesystem::exists(coursePackDirectory / (extraPack + ".pack")) == (course == 7U)
								&& std::filesystem::exists(coursePackDirectory / (extraPack + ".idx")) == (course == 7U),
							"the cleanup leaves the extra pack this course planted, and no other course has one");
					opened->reset();
					continue;
				}
			}
			const auto prepared = (*opened)->prepare_sparse_revision(prepareRequest, { }, witness);
			if (!prepared && course != 1U && course != 2U && course != 3U) {
				std::cerr << "sparse fixture failure " << static_cast<int>(prepared.error().Code) << " " << prepared.error().Detail << '\n';
			}
			if (course == 2U || course == 3U) {
				require(!prepared && !witnessed, "sparse import refuses missing root tree and wrong commit object before witness");
				require((*opened)->abort_operation(operation, { }).has_value(), "abort rejected sparse import");
			} else {
				require(witnessed && application::valid(*witnessed)
							&& witnessed->Residency == residency::sparse_commit_trees
							&& witnessed->HeadLineage == (course == 4U || course == 0U ? core::head_lineage::not_applicable : core::head_lineage::unknown)
							&& witnessed->Head == head && witnessed->BaseTree == tree && witnessed->HeadTree == tree,
						"sparse witness binds exact operation objects without ancestry claim");
				require((course == 1U && !prepared && prepared.error() == application::git_cache_failure::authorization_mismatch)
							|| (course != 1U && prepared && *prepared == *witnessed),
						"witness refusal preserves recoverable installation");
				auto recovery = application::git_operation_recovery_request{
					.Operation = operation,
					.Scope     = cacheRequest.Authority.Scope,
					.Ordinal   = core::epoch_ordinal{ 1 },
					.Base      = head,
					.Head      = head,
					.Spool     = application::git_recovery_spool_expectation{
						.ExpectedPack             = receipt->ExpectedPack,
						.CompletedSpool           = receipt->Spool,
						.PreimportPacks           = inventory->Packs,
						.PreimportRepositoryBytes = inventory->RepositoryBytes,
						.ExpectedPackPreexisting  = inventory->ExpectedPackPreexisting,
						.Residency                = residency::sparse_commit_trees,
					},
					.Install = application::git_recovery_install_expectation{
						.Pack           = witnessed->Pack,
						.InstalledPack  = witnessed->InstalledPack,
						.InstalledIndex = witnessed->InstalledIndex,
						.BaseTree       = tree,
						.HeadTree       = tree,
						.Residency      = residency::sparse_commit_trees,
					},
				};
				opened->reset();
				opened = aCache.open(cacheRequest, { });
				require(opened.has_value(), "sparse cache reopens without promisor or shallow repository mutation");
				const auto observed = (*opened)->inspect_operation_recovery(recovery, { });
				require(observed && observed->Installed == application::git_recovery_match::exact
							&& observed->Objects == application::git_recovery_object_match::exact,
						"restart verifies exact sparse durable installation witness");
				auto strictRecovery               = recovery;
				strictRecovery.Install->Residency = residency::complete_closure;
				strictRecovery.Spool->Residency   = residency::complete_closure;
				const auto strictObserved         = (*opened)->inspect_operation_recovery(strictRecovery, { });
				require(strictObserved && strictObserved->Objects != application::git_recovery_object_match::exact,
						"sparse objects cannot satisfy complete recovery");
				auto wrongTree              = recovery;
				wrongTree.Install->HeadTree = *core::parse_git_tree_oid(nestedTree);
				const auto mismatched       = (*opened)->inspect_operation_recovery(wrongTree, { });
				require(mismatched && mismatched->Objects == application::git_recovery_object_match::mismatch,
						"restart refuses changed tree witness");
				auto preWitnessRecovery = recovery;
				preWitnessRecovery.Install.reset();
				if (course == 1U) {
					const auto preWitness = (*opened)->inspect_operation_recovery(preWitnessRecovery, { });
					require(preWitness && preWitness->Installed == application::git_recovery_match::inventory_only
								&& preWitness->Objects == application::git_recovery_object_match::exact,
							"pre-install witness crash recovers using durable sparse spool mode");
				}
				const auto inspected = (*opened)->inspect_sparse_commit_object(head, { });
				require(inspected && inspected->Tree == tree && inspected->Residency == residency::sparse_commit_trees
							&& !(*opened)->inspect_commit_object(head, { }) && !(*opened)->inspect_sparse_commit_object(parent, { }),
						"sparse root inspection never advertises missing ancestor closure");
				const auto path = (*opened)->find_blob_at_path({ .Commit = head, .Path = "file.txt" }, { });
				require(path && *path == blob && !(*opened)->materialize_blob({ .Blob = blob, .MaximumBytes = 1024U }, { }),
						"tree returns exact expected blob OID while missing content remains unavailable");
				require(!(*opened)->find_blob_at_path({ .Commit = head, .Path = "subdir/nested.txt" }, { }),
						"absent selected-path subtree is unavailable instead of fabricated absence");
				if (course == 4U) {
					const auto promoted = (*opened)->promote_retained(*witnessed, {
																					  .CatalogIncarnation      = prepareRequest.Authorization.CatalogIncarnation,
																					  .ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
																					  .Operation               = operation,
																					  .Scope                   = cacheRequest.Authority.Scope,
																					  .Ordinal                 = core::epoch_ordinal{ 1 },
																					  .Base                    = head,
																					  .Head                    = head,
																					  .DurableCommitToken      = token(0xd1U),
																				  },
																	  { });
					require(promoted.has_value(), "promote sparse exact refs under durable catalog grant");
					opened->reset();
					opened = aCache.open(cacheRequest, { });
					require(opened.has_value(), "reopen promoted sparse cache");
					const auto retained = (*opened)->inspect_sparse_retained(core::epoch_ordinal{ 1 }, { });
					require(retained && retained->Base == head && retained->Head == head
								&& retained->Residency == residency::sparse_commit_trees
								&& !(*opened)->inspect_retained(core::epoch_ordinal{ 1 }, { }),
							"retained sparse inspection stays distinct from full closure after restart");
					require((*opened)->complete_recovered_operation(recovery, { }).has_value(), "finalized sparse recovery cleans exact spool");
				} else {
					require((*opened)->abort_recovered_operation(course == 1U ? preWitnessRecovery : recovery, { }).has_value(), "abort sparse operation from exact restart witness");
				}
			}
			opened->reset();
			require(aCache.delete_cache(cacheRequest, {
														  .ApplicationSession = cacheRequest.ApplicationSession,
														  .Cache              = cacheRequest.Authority.Cache,
														  .DurableIntentToken = token(0xd2U),
													  },
										{ })
						.has_value(),
					"delete isolated sparse fixture cache");
		}
	}

	auto verify_descendant_lineage(
		application::git_cache_port& aCache,
		const force_push_fixture&    aDescendant) -> void
	{
		const auto cacheRequest = request(0x34U, 0x44U);
		auto       opened       = aCache.open(cacheRequest, { });
		require(opened.has_value(), "open isolated lineage cache");
		auto&      session           = **opened;
		const auto baselineOperation = identity<core::epoch_operation_id>(0x6aU);
		auto       baselineSink      = session.create_pack_sink(
			baselineOperation,
			sha1_pack.size(),
			{ });
		require(baselineSink.has_value(), "create lineage baseline spool");
		append_pack(**baselineSink, sha1_pack);
		const auto baselineReceipt = (*baselineSink)->finish({ });
		require(baselineReceipt.has_value(), "finish lineage baseline spool");
		baselineSink->reset();
		const auto baselineInventory = session.observe_preimport(
			*baselineReceipt,
			{ });
		const auto baselineCommit = core::parse_git_commit_oid(
			sha1_commit_hex);
		const auto incarnation = identity<core::epoch_catalog_incarnation_id>(0x6aU);
		require(
			baselineInventory.has_value()
				&& baselineCommit.has_value(),
			"observe lineage baseline inventory");
		const auto baselinePrepared = session.prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *baselineReceipt,
				.Inventory     = *baselineInventory,
				.Authorization = {
					.CatalogIncarnation      = incarnation,
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
					.Operation               = baselineOperation,
					.ExpectedPack            = baselineReceipt->ExpectedPack,
					.ReceiptToken            = baselineReceipt->OpaqueToken,
					.InventoryToken          = baselineInventory->OpaqueToken,
					.DurableRecordToken      = token(0x6aU),
				},
				.Base = *baselineCommit,
				.Head = *baselineCommit,
			},
			{ });
		require(
			baselinePrepared.has_value()
				&& baselinePrepared->HeadLineage
					== core::head_lineage::not_applicable,
			"prepare isolated lineage baseline");
		require(
			session
				.promote_retained(
					*baselinePrepared,
					application::git_ref_promotion_authorization{
						.CatalogIncarnation      = incarnation,
						.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
						.Operation               = baselineOperation,
						.Scope                   = cacheRequest.Authority.Scope,
						.Ordinal                 = core::epoch_ordinal{ 1 },
						.Base                    = *baselineCommit,
						.Head                    = *baselineCommit,
						.DurableCommitToken      = token(0x6bU),
					},
					{ })
				.has_value(),
			"retain isolated lineage baseline");

		const auto descendantOperation = identity<core::epoch_operation_id>(0x6cU);
		auto       descendantSink      = session.create_pack_sink(
			descendantOperation,
			aDescendant.NewPack.size(),
			{ });
		require(descendantSink.has_value(), "create descendant lineage spool");
		append_pack(**descendantSink, aDescendant.NewPack);
		const auto descendantReceipt = (*descendantSink)->finish({ });
		require(descendantReceipt.has_value(), "finish descendant lineage spool");
		descendantSink->reset();
		const auto descendantInventory = session.observe_preimport(
			*descendantReceipt,
			{ });
		require(
			descendantInventory.has_value(),
			"observe descendant lineage inventory");
		const auto descendantPrepared = session.prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *descendantReceipt,
				.Inventory     = *descendantInventory,
				.Authorization = {
					.CatalogIncarnation      = incarnation,
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
					.Operation               = descendantOperation,
					.ExpectedPack            = descendantReceipt->ExpectedPack,
					.ReceiptToken            = descendantReceipt->OpaqueToken,
					.InventoryToken          = descendantInventory->OpaqueToken,
					.DurableRecordToken      = token(0x6cU),
				},
				.Base      = aDescendant.NewCommit,
				.Head      = aDescendant.NewCommit,
				.PriorHead = baselineCommit,
			},
			{ });
		require(
			descendantPrepared.has_value()
				&& descendantPrepared->HeadLineage
					== core::head_lineage::descendant,
			"exact prior-to-current ancestry classifies a descendant upload");
		require(
			session.abort_operation(descendantOperation, { }).has_value(),
			"descendant lineage prepare exact-cleans staging refs");
		const auto historicalOperation = identity<core::epoch_operation_id>(0x6eU);
		auto       historicalSink      = session.create_pack_sink(historicalOperation, aDescendant.NewPack.size(), { });
		require(historicalSink.has_value(), "create historical derivation spool");
		append_pack(**historicalSink, aDescendant.NewPack);
		const auto historicalReceipt = (*historicalSink)->finish({ });
		require(historicalReceipt.has_value(), "finish historical derivation spool");
		historicalSink->reset();
		const auto historicalInventory = session.observe_preimport(*historicalReceipt, { });
		require(historicalInventory.has_value(), "observe historical derivation inventory");
		bool       derivedWitness     = false;
		const auto historicalPrepared = session.prepare_historical_revision({
																				.Receipt       = *historicalReceipt,
																				.Inventory     = *historicalInventory,
																				.Authorization = {
																					.CatalogIncarnation      = incarnation,
																					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
																					.Operation               = historicalOperation,
																					.ExpectedPack            = historicalReceipt->ExpectedPack,
																					.ReceiptToken            = historicalReceipt->OpaqueToken,
																					.InventoryToken          = historicalInventory->OpaqueToken,
																					.DurableRecordToken      = token(0x6eU),
																				},
																				.SelectedBase = aDescendant.NewCommit,
																				.Head         = *baselineCommit,
																			},
																			{ }, [&](const application::git_prepared_revision& aPrepared) {
																				derivedWitness = aPrepared.Base == *baselineCommit && aPrepared.Head == *baselineCommit;
																				return derivedWitness;
																			});
		require(historicalPrepared && derivedWitness && historicalPrepared->Base != aDescendant.NewCommit,
				"historical witness receives derived ancestor rather than selected current base");
		const auto historicalRecovery = application::git_operation_recovery_request{
			.Operation = historicalOperation,
			.Scope     = cacheRequest.Authority.Scope,
			.Ordinal   = core::epoch_ordinal{ 2 },
			.Base      = historicalPrepared->Base,
			.Head      = historicalPrepared->Head,
			.Install   = application::git_recovery_install_expectation{
				.Pack           = historicalPrepared->Pack,
				.InstalledPack  = historicalPrepared->InstalledPack,
				.InstalledIndex = historicalPrepared->InstalledIndex,
				.BaseTree       = historicalPrepared->BaseTree,
				.HeadTree       = historicalPrepared->HeadTree,
			},
		};
		opened->reset();
		opened = aCache.open(cacheRequest, { });
		require(opened.has_value(), "restart historical derived cache");
		const auto recovered = (*opened)->inspect_operation_recovery(historicalRecovery, { });
		require(recovered && recovered->Staging == application::git_recovery_match::exact
					&& recovered->Objects == application::git_recovery_object_match::exact,
				"restart validates exact derived staging and object closure");
		require((*opened)->abort_recovered_operation(historicalRecovery, { }).has_value(),
				"recovered historical staging safely aborts");

		opened->reset();
		require(
			aCache
				.delete_cache(
					cacheRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = cacheRequest.ApplicationSession,
						.Cache              = cacheRequest.Authority.Cache,
						.DurableIntentToken = token(0x6dU),
					},
					{ })
				.has_value(),
			"delete isolated lineage cache after direct verification");
	}

	auto verify_pristine_remount_recovery() -> void
	{
#if defined(Q_OS_MACOS)
		const auto executable = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE);
		for (const auto course : { "empty", "object", "ref", "unknown", "authority", "lock", "uuid", "cancelled" }) {
			auto root  = temporary_root{ };
			auto cache = git_adapter::make_installed_git_cache({ .GitExecutable = executable, .StorageRoot = root.Path });
			require(cache.has_value(), "remount cache adapter opens");
			const auto expected = request(0x91U, 0x92U);
			auto       session  = (*cache)->open(expected, { });
			require(session.has_value(), "create pristine cache before remount");
			session->reset();
			const auto repository = repository_path(root.Path, expected);
			const auto path       = repository / "betterpullreview-cache-manifest-v1";
			const auto stable     = read_file(path);
			require(stable.size() == 141U && stable[11U] == 2U, "new Mac manifest declares durable volume identity");
			auto legacy = stable;
			legacy[11U] = 1U;
			legacy.resize(legacy.size() - 24U);
			struct stat lock{ };
			const auto  lockPath = root.Path / "locks/v1" / (lower_hex(expected.Authority.Cache.bytes()) + ".lock");
			require(::lstat(lockPath.c_str(), &lock) == 0, "observe owned fixture lock");
			append_u64(legacy, static_cast<std::uint64_t>(lock.st_dev) ^ 1U);
			append_u64(legacy, 0U);
			append_u64(legacy, static_cast<std::uint64_t>(lock.st_ino));
			const auto mode = std::string_view{ course };
			if (mode == "object") {
				write_file(repository / "objects/pack/retained-canary", "retain object");
			} else if (mode == "ref") {
				write_file(repository / "refs/heads/retained-canary", "retain ref");
			} else if (mode == "unknown") {
				write_file(repository / "unknown-canary", "retain unknown");
			} else if (mode == "authority") {
				legacy[13U] ^= 1U;
			} else if (mode == "lock") {
				legacy.back() ^= 1U;
			} else if (mode == "uuid") {
				legacy = stable;
				legacy[legacy.size() - 24U] ^= 1U;
			}
			write_file(path, legacy);
			auto stop = std::stop_source{ };
			if (mode == "cancelled") {
				stop.request_stop();
			}
			auto reopened = (*cache)->open(expected, stop.get_token());
			if (mode != "empty") {
				require(!reopened && read_file(path) == legacy,
						"populated, unknown, substituted, mismatched or cancelled cache is preserved and refused");
				continue;
			}
			require(reopened.has_value() && read_file(path) == stable,
					"pristine same-inode old-mount cache reinitializes with exact durable authority");
			auto retiredCount = 0U;
			for (const auto& entry : std::filesystem::directory_iterator{ repository.parent_path() }) {
				if (entry.path().filename().string().starts_with(".bpr-pristine-legacy-")) {
					++retiredCount;
					require(read_file(entry.path() / "betterpullreview-cache-manifest-v1") == legacy,
							"recovery preserves the old empty cache metadata without rewriting or deleting it");
				}
			}
			require(retiredCount == 1U, "exactly one old empty cache is retired");
			reopened->reset();
			require((*cache)->open(expected, { }).has_value(), "durable manifest reopens without a second recovery");
		}
#endif
	}

	auto run_lock_holder(
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aExecutable,
		const std::uint8_t           aCacheByte,
		const std::uint8_t           aScopeByte,
		const std::filesystem::path& aReady,
		const std::filesystem::path& aRelease) -> int
	{
		auto cache = git_adapter::make_installed_git_cache({
			.GitExecutable     = aExecutable,
			.StorageRoot       = aRoot,
			.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
		});
		if (!cache.has_value()) {
			return 20;
		}
		const auto cacheRequest = request(aCacheByte, aScopeByte);
		auto       session      = (*cache)->open(cacheRequest, { });
		if (!session.has_value()) {
			return 21;
		}
		write_file(aReady, "ready");
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 90 };
		while (std::chrono::steady_clock::now() < deadline) {
			std::error_code error;
			if (std::filesystem::exists(aRelease, error) && !error) {
				return 0;
			}
			QThread::msleep(10U);
		}
		return 22;
	}

	auto run_cache_contract() -> void
	{
		{
			auto diagnosticRoot  = temporary_root{ };
			auto diagnosticCache = git_adapter::make_installed_git_cache({
				.GitExecutable = std::filesystem::path{
					QCoreApplication::applicationFilePath().toStdString() },
				.StorageRoot       = diagnosticRoot.Path,
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			require(
				diagnosticCache.has_value(),
				"construct cache around deterministic diagnostic Git candidate");
			const auto opened = (*diagnosticCache)->open(request(0x31U, 0x41U), { });
			if (
				opened.has_value()
				|| opened.error()
					!= application::git_cache_failure::process_failure
				|| opened.error().Detail != redacted_git_diagnostic) {
				std::cerr << "diagnostic cache result: value=" << opened.has_value();
				if (!opened.has_value()) {
					std::cerr << " code="
							  << static_cast<unsigned int>(opened.error().Code)
							  << " detail=[" << opened.error().Detail << ']';
				}
				std::cerr << '\n';
			}
			require(
				!opened.has_value()
					&& opened.error()
						== application::git_cache_failure::process_failure
					&& opened.error().Detail == redacted_git_diagnostic,
				"cache open preserves the exact redacted Git bootstrap diagnostic");
		}

		const auto executable = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE);
		auto       root       = temporary_root{ };
		const auto forcePush  = make_force_push_fixture(executable, root.Path);
		const auto descendant = make_descendant_fixture(
			executable,
			root.Path,
			forcePush.Remote);
		const auto sha256Fixture = make_sha256_fixture(executable, root.Path);
		auto       cache         = git_adapter::make_installed_git_cache({
			.GitExecutable     = executable,
			.StorageRoot       = root.Path,
			.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
		});
		require(cache.has_value(), "construct installed Git cache");
		verify_descendant_lineage(**cache, descendant);
		verify_refused_installation_witness(**cache, root.Path, executable);
		verify_receiving_ownership_recovery(**cache, root.Path);

		const auto crossProcessRequest = request(0x36U, 0x46U);
		const auto lockReady           = root.Path / "cross-process-lock-ready";
		const auto lockRelease         = root.Path / "cross-process-lock-release";
		auto       lockHolder          = QProcess{ };
		lockHolder.setProgram(QCoreApplication::applicationFilePath());
		lockHolder.setArguments({
			"--hold-cache-lock",
			QString::fromStdString(root.Path.string()),
			QString::fromStdString(executable.string()),
			QString::number(0x36U),
			QString::number(0x46U),
			QString::fromStdString(lockReady.string()),
			QString::fromStdString(lockRelease.string()),
		});
		lockHolder.start();
		require(
			lockHolder.waitForStarted(5'000),
			"start cross-process cache lock holder");
		const auto lockDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 90 };
		while (
			!std::filesystem::exists(lockReady)
			&& lockHolder.state() != QProcess::NotRunning
			&& std::chrono::steady_clock::now() < lockDeadline) {
			QThread::msleep(10U);
		}
		require(
			std::filesystem::is_regular_file(lockReady),
			"cross-process holder acquires exact cache lock");
		const auto crossProcessConflict = (*cache)->open(crossProcessRequest, { });
		require(
			!crossProcessConflict.has_value()
				&& crossProcessConflict.error()
					== application::git_cache_failure::lock_unavailable,
			"independent process cannot acquire held cache writer lock");
		write_file(lockRelease, "release");
		require(
			lockHolder.waitForFinished(30'000)
				&& lockHolder.exitStatus() == QProcess::NormalExit
				&& lockHolder.exitCode() == 0,
			"cross-process cache lock holder exits cleanly");
		require(
			(*cache)
				->delete_cache(
					crossProcessRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = crossProcessRequest.ApplicationSession,
						.Cache              = crossProcessRequest.Authority.Cache,
						.DurableIntentToken = token(0x76U),
					},
					{ })
				.has_value(),
			"released cross-process cache lock admits exact cleanup");

		const auto manifestFailureRequest    = request(0x37U, 0x47U);
		const auto manifestFailureRepository = repository_path(root.Path, manifestFailureRequest);
		const auto manifestFailureMarker     = root.Path / "inject-cache-manifest-failure";
		write_file(manifestFailureMarker, "inject");
		const auto manifestFailure = (*cache)->open(manifestFailureRequest, { });
		require(
			!manifestFailure.has_value()
				&& manifestFailure.error()
					== application::git_cache_failure::invalid_manifest
				&& !std::filesystem::exists(
					manifestFailureRepository),
			"manifest publication failure leaves no final cache path");
		std::filesystem::remove(manifestFailureMarker);
		auto manifestRetry = (*cache)->open(manifestFailureRequest, { });
		require(
			manifestRetry.has_value(),
			"manifest publication failure remains cleanly retryable");
		manifestRetry->reset();
		require(
			(*cache)
				->delete_cache(
					manifestFailureRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = manifestFailureRequest
												  .ApplicationSession,
						.Cache              = manifestFailureRequest.Authority.Cache,
						.DurableIntentToken = token(0x77U),
					},
					{ })
				.has_value(),
			"retry-created manifest fixture exact-cleans");

		const auto validationCleanupRequest = request(0x3cU, 0x4cU);
		const auto validationCleanupMarker  = root.Path
			/ "inject-cache-post-manifest-substitution";
		write_file(validationCleanupMarker, "inject");
		const auto validationCleanup = (*cache)->open(validationCleanupRequest, { });
		std::filesystem::remove(validationCleanupMarker);
		const auto displacedStaging = root.Path / "repositories" / "v1"
			/ ".bpr-test-displaced-cache-staging.git";
		auto replacementStaging = std::filesystem::path{ };
		for (const auto& entry :
			 std::filesystem::directory_iterator{
				 root.Path / "repositories" / "v1" }) {
			if (
				entry.path() != displacedStaging
				&& std::filesystem::is_regular_file(
					entry.path() / "replacement-canary")) {
				replacementStaging = entry.path();
				break;
			}
		}
		require(
			!validationCleanup.has_value()
				&& validationCleanup.error()
					== application::git_cache_failure::repair_required
				&& !std::filesystem::exists(
					repository_path(
						root.Path,
						validationCleanupRequest))
				&& std::filesystem::is_directory(displacedStaging)
				&& !replacementStaging.empty()
				&& std::ranges::equal(
					read_file(
						replacementStaging
						/ "replacement-canary"),
					std::string_view{
						"post-manifest-root-canary-dat" }),
			"unprovable post-manifest cleanup preserves replacement and requires repair");
		std::filesystem::remove_all(replacementStaging);
		std::filesystem::remove_all(displacedStaging);

		const auto destinationRequest    = request(0x38U, 0x48U);
		const auto destinationRepository = repository_path(root.Path, destinationRequest);
		const auto destinationMarker     = root.Path / "inject-cache-final-destination";
		write_file(destinationMarker, "inject");
		const auto destinationCollision = (*cache)->open(destinationRequest, { });
		require(
			!destinationCollision.has_value()
				&& destinationCollision.error()
					== application::git_cache_failure::repair_required
				&& read_file(destinationRepository / "canary")
					== std::vector<std::uint8_t>{
						'c', 'a', 'n', 'a', 'r', 'y' },
			"cache publication never adopts or deletes a final-path canary");
		std::filesystem::remove(destinationMarker);
		std::filesystem::remove_all(destinationRepository);

		const auto deleteRaceRequest = request(0x3aU, 0x4aU);
		auto       deleteRaceSession = (*cache)->open(deleteRaceRequest, { });
		require(
			deleteRaceSession.has_value(),
			"create delete-destination race fixture");
		deleteRaceSession->reset();
		const auto deleteRaceToken         = token(0x74U);
		const auto deleteRaceAuthorization = application::git_cache_delete_authorization{
			.ApplicationSession = deleteRaceRequest.ApplicationSession,
			.Cache              = deleteRaceRequest.Authority.Cache,
			.DurableIntentToken = deleteRaceToken,
		};
		const auto deleteRaceMarker = root.Path / "inject-cache-delete-destination-race";
		write_file(deleteRaceMarker, "inject");
		const auto deleteRace = (*cache)->delete_cache(
			deleteRaceRequest,
			deleteRaceAuthorization,
			{ });
		const auto deleteRaceQuarantine = root.Path / "quarantine" / "v1"
			/ (lower_hex(
				   deleteRaceRequest.Authority.Cache.bytes())
			   + "-" + lower_hex(deleteRaceToken) + ".git");
		require(
			!deleteRace.has_value()
				&& deleteRace.error()
					== application::git_cache_failure::repair_required
				&& std::filesystem::is_directory(
					repository_path(root.Path, deleteRaceRequest))
				&& read_file(deleteRaceQuarantine / "canary")
					== std::vector<std::uint8_t>{
						'c', 'a', 'n', 'a', 'r', 'y' },
			"no-replace delete rename preserves a raced destination canary");
		std::filesystem::remove(deleteRaceMarker);
		std::filesystem::remove_all(deleteRaceQuarantine);
		require(
			(*cache)
				->delete_cache(
					deleteRaceRequest,
					deleteRaceAuthorization,
					{ })
				.has_value(),
			"same delete intent retries after destination collision removal");

		const auto childSubstitutionRequest = request(0x3eU, 0x4eU);
		auto       childSubstitutionSession = (*cache)->open(childSubstitutionRequest, { });
		require(
			childSubstitutionSession.has_value(),
			"create descendant-substitution delete fixture");
		childSubstitutionSession->reset();
		const auto childSubstitutionToken         = token(0x73U);
		const auto childSubstitutionAuthorization = application::git_cache_delete_authorization{
			.ApplicationSession = childSubstitutionRequest
									  .ApplicationSession,
			.Cache              = childSubstitutionRequest.Authority.Cache,
			.DurableIntentToken = childSubstitutionToken,
		};
		const auto childSubstitutionMarker = root.Path
			/ "inject-cache-delete-child-substitution";
		write_file(childSubstitutionMarker, "inject");
		const auto childSubstitutionDelete = (*cache)->delete_cache(
			childSubstitutionRequest,
			childSubstitutionAuthorization,
			{ });
		std::filesystem::remove(childSubstitutionMarker);
		const auto childSubstitutionQuarantine = root.Path / "quarantine" / "v1"
			/ (lower_hex(
				   childSubstitutionRequest
					   .Authority.Cache.bytes())
			   + "-" + lower_hex(childSubstitutionToken)
			   + ".git");
		const auto childSubstitutionManifest = childSubstitutionQuarantine
			/ "betterpullreview-cache-manifest-v1";
		const auto childSubstitutionOriginal = childSubstitutionQuarantine
			/ ".bpr-test-original-manifest";
		require(
			!childSubstitutionDelete.has_value()
				&& childSubstitutionDelete.error()
					== application::git_cache_failure::repair_required
				&& std::ranges::equal(
					read_file(childSubstitutionManifest),
					std::string_view{
						"replacement-child-canary" })
				&& std::filesystem::is_regular_file(
					childSubstitutionOriginal),
			"recursive deletion revalidates descendants and preserves a replacement child");
		std::filesystem::remove(childSubstitutionManifest);
		std::filesystem::rename(
			childSubstitutionOriginal,
			childSubstitutionManifest);
		require(
			(*cache)
				->delete_cache(
					childSubstitutionRequest,
					childSubstitutionAuthorization,
					{ })
				.has_value(),
			"same tombstoned delete intent retries after exact child restoration");

		const auto orphanQuarantineRequest = request(0x3bU, 0x4bU);
		const auto orphanQuarantineToken   = token(0x75U);
		const auto orphanQuarantine        = root.Path / "quarantine" / "v1"
			/ (lower_hex(
				   orphanQuarantineRequest.Authority.Cache.bytes())
			   + "-" + lower_hex(orphanQuarantineToken) + ".git");
		std::filesystem::create_directory(orphanQuarantine);
		std::filesystem::permissions(
			orphanQuarantine,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		write_file(orphanQuarantine / "canary", "orphan-canary");
		const auto orphanQuarantineDelete = (*cache)->delete_cache(
			orphanQuarantineRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = orphanQuarantineRequest
										  .ApplicationSession,
				.Cache              = orphanQuarantineRequest.Authority.Cache,
				.DurableIntentToken = orphanQuarantineToken,
			},
			{ });
		require(
			!orphanQuarantineDelete.has_value()
				&& orphanQuarantineDelete.error()
					== application::git_cache_failure::repair_required
				&& read_file(orphanQuarantine / "canary")
					== std::vector<std::uint8_t>{
						'o', 'r', 'p', 'h', 'a', 'n', '-',
						'c', 'a', 'n', 'a', 'r', 'y' },
			"deletion never adopts an untombstoned quarantine canary");
		std::filesystem::remove_all(orphanQuarantine);

		const auto primaryRequest                 = request(0x31U, 0x41U);
		auto       isolatedRequest                = request(0x32U, 0x42U);
		isolatedRequest.Authority.ProfileRevision = core::github_profile_revision{ 33U };
		isolatedRequest.Authority.Account         = core::github_account_id{ 66U };
		isolatedRequest.Limits.PackCount          = 1U;
		const auto primaryRepository              = repository_path(root.Path, primaryRequest);
		const auto isolatedRepository             = repository_path(root.Path, isolatedRequest);

		auto primary = (*cache)->open(primaryRequest, { });
		require(primary.has_value(), "create exact primary cache");
		require(
			std::filesystem::is_directory(primaryRepository)
				&& primaryRepository.filename()
					== lower_hex(primaryRequest.Authority.Cache.bytes())
						+ ".git",
			"repository uses exact repositories/v1 scope path");
		const auto manifest = primaryRepository / "betterpullreview-cache-manifest-v1";
		require(
			std::filesystem::is_regular_file(manifest),
			"cache contains immutable versioned manifest");

		const auto duplicate = (*cache)->open(primaryRequest, { });
		require(
			!duplicate.has_value()
				&& duplicate.error()
					== application::git_cache_failure::lock_unavailable,
			"same cache scope has one cross-process writer");

		auto isolated = (*cache)->open(isolatedRequest, { });
		require(
			isolated.has_value()
				&& std::filesystem::is_directory(isolatedRepository),
			"unrelated cache scope opens while primary lock is held");
		require(
			std::ranges::distance(
				std::filesystem::directory_iterator{
					root.Path / "repositories" / "v1" },
				std::filesystem::directory_iterator{ })
					== 2
				&& std::filesystem::is_empty(
					root.Path / "incoming" / "v1"),
			"successful process probes leave no repository or spool residue");

#if defined(Q_OS_UNIX)
		const auto substitutedLockRequest = request(0x39U, 0x49U);
		auto       substitutedLockSession = (*cache)->open(substitutedLockRequest, { });
		require(
			substitutedLockSession.has_value(),
			"create lock-substitution fixture cache");
		const auto substitutedLock = root.Path / "locks" / "v1"
			/ (lower_hex(
				   substitutedLockRequest.Authority.Cache.bytes())
			   + ".lock");
		require(
			std::filesystem::remove(substitutedLock),
			"unlink held cache lock fixture");
		write_file(substitutedLock, "replacement-lock-canary");
		const auto substitutedLockOpen = (*cache)->open(substitutedLockRequest, { });
		const auto substitutedLockUse  = (*substitutedLockSession)->create_pack_sink(identity<core::epoch_operation_id>(0x45U), sha1_pack.size(), { });
		require(
			!substitutedLockOpen.has_value()
				&& substitutedLockOpen.error()
					== application::git_cache_failure::invalid_manifest
				&& !substitutedLockUse.has_value()
				&& substitutedLockUse.error()
					== application::git_cache_failure::
						invalid_repository
				&& read_file(substitutedLock)
					== std::vector<std::uint8_t>{
						'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
						'e', 'n', 't', '-', 'l', 'o', 'c', 'k',
						'-', 'c', 'a', 'n', 'a', 'r', 'y' },
			"lock inode substitution cannot admit a second writer or mutate cache");
		substitutedLockSession->reset();
#endif

#if defined(Q_OS_UNIX)
		const auto processBoundaryRequest = request(0x40U, 0x50U);
		auto       processBoundarySession = (*cache)->open(processBoundaryRequest, { });
		require(
			processBoundarySession.has_value(),
			"create process-boundary lock fixture cache");
		const auto processBoundaryOperation = identity<core::epoch_operation_id>(0x60U);
		auto       processBoundarySink      = (*processBoundarySession)->create_pack_sink(processBoundaryOperation, sha1_pack.size(), { });
		require(
			processBoundarySink.has_value(),
			"create process-boundary lock fixture spool");
		append_pack(**processBoundarySink, sha1_pack);
		const auto processBoundaryReceipt = (*processBoundarySink)->finish({ });
		require(
			processBoundaryReceipt.has_value(),
			"finish process-boundary lock fixture spool");
		processBoundarySink->reset();
		const auto processBoundaryObservation = (*processBoundarySession)->observe_preimport(*processBoundaryReceipt, { });
		const auto processBoundaryCommit      = core::parse_git_commit_oid(sha1_commit_hex);
		require(
			processBoundaryObservation.has_value()
				&& processBoundaryCommit.has_value(),
			"observe process-boundary lock fixture baseline");
		const auto processBoundaryMarker = root.Path
			/ "inject-cache-lock-substitution-during-index";
		write_file(processBoundaryMarker, "inject");
		const auto processBoundaryPrepare = (*processBoundarySession)->prepare_revision(application::git_prepare_revision_request{
																							.Receipt       = *processBoundaryReceipt,
																							.Inventory     = *processBoundaryObservation,
																							.Authorization = {
																								.CatalogIncarnation      = identity<core::epoch_catalog_incarnation_id>(0x73U),
																								.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
																								.Operation               = processBoundaryOperation,
																								.ExpectedPack            = processBoundaryReceipt->ExpectedPack,
																								.ReceiptToken            = processBoundaryReceipt->OpaqueToken,
																								.InventoryToken          = processBoundaryObservation->OpaqueToken,
																								.DurableRecordToken      = token(0x74U),
																							},
																							.Base = *processBoundaryCommit,
																							.Head = *processBoundaryCommit,
																						},
																						{ });
		std::filesystem::remove(processBoundaryMarker);
		const auto processBoundaryLock = root.Path / "locks" / "v1"
			/ (lower_hex(
				   processBoundaryRequest.Authority.Cache.bytes())
			   + ".lock");
		const auto processBoundaryDisplacedLock = processBoundaryLock.parent_path()
			/ (".bpr-test-original-"
			   + processBoundaryLock.filename().string());
		const auto processBoundarySpool = root.Path / "incoming" / "v1"
			/ (lower_hex(processBoundaryOperation.bytes())
			   + ".pack.part");
		const auto processBoundaryPackDirectory = repository_path(root.Path, processBoundaryRequest)
			/ "objects" / "pack";
		const auto processBoundarySecondOpen = (*cache)->open(processBoundaryRequest, { });
		require(
			!processBoundaryPrepare.has_value()
				&& processBoundaryPrepare.error()
					== application::git_cache_failure::repair_required
				&& std::filesystem::is_regular_file(
					processBoundarySpool)
				&& std::filesystem::is_regular_file(
					processBoundaryPackDirectory
					/ ("pack-" + std::string{ sha1_pack_hex }
					   + ".pack"))
				&& std::filesystem::is_regular_file(
					processBoundaryPackDirectory
					/ ("pack-" + std::string{ sha1_pack_hex }
					   + ".idx"))
				&& read_file(processBoundaryLock)
					== std::vector<std::uint8_t>{
						'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
						'e', 'n', 't', '-', 'l', 'o', 'c', 'k',
						'-', 'c', 'a', 'n', 'a', 'r', 'y' }
				&& !processBoundarySecondOpen.has_value() && processBoundarySecondOpen.error() == application::git_cache_failure::invalid_manifest,
			"process postcheck binds repository and exact held lock while retaining uncertain state");
		require(
			std::filesystem::remove(processBoundaryLock),
			"remove process-boundary replacement lock canary");
		std::filesystem::rename(
			processBoundaryDisplacedLock,
			processBoundaryLock);
		require(
			(*processBoundarySession)
					->abort_operation(processBoundaryOperation, { })
					.has_value()
				&& !std::filesystem::exists(processBoundarySpool),
			"restored process-boundary lock permits exact spool cleanup");
		processBoundarySession->reset();
		require(
			(*cache)
				->delete_cache(
					processBoundaryRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = processBoundaryRequest.ApplicationSession,
						.Cache              = processBoundaryRequest.Authority.Cache,
						.DurableIntentToken = token(0x75U),
					},
					{ })
				.has_value(),
			"process-boundary lock fixture exact-cleans");

		const auto deleteBoundaryRequest = request(0x41U, 0x51U);
		auto       deleteBoundarySession = (*cache)->open(deleteBoundaryRequest, { });
		require(
			deleteBoundarySession.has_value(),
			"create delete-boundary lock fixture cache");
		deleteBoundarySession->reset();
		const auto deleteBoundaryLock = root.Path / "locks" / "v1"
			/ (lower_hex(deleteBoundaryRequest.Authority.Cache.bytes())
			   + ".lock");
		const auto deleteBoundaryDisplacedLock = deleteBoundaryLock.parent_path()
			/ (".bpr-test-original-delete-"
			   + deleteBoundaryLock.filename().string());
		const auto deleteBoundaryRepository = repository_path(root.Path, deleteBoundaryRequest);
		const auto deleteBoundaryMarker     = root.Path
			/ "inject-cache-lock-substitution-during-delete";
		const auto deleteBoundaryAuthorization = application::git_cache_delete_authorization{
			.ApplicationSession = deleteBoundaryRequest.ApplicationSession,
			.Cache              = deleteBoundaryRequest.Authority.Cache,
			.DurableIntentToken = token(0x76U),
		};
		write_file(deleteBoundaryMarker, "inject");
		const auto deleteBoundaryResult = (*cache)->delete_cache(
			deleteBoundaryRequest,
			deleteBoundaryAuthorization,
			{ });
		std::filesystem::remove(deleteBoundaryMarker);
		const auto deleteBoundarySecondOpen = (*cache)->open(deleteBoundaryRequest, { });
		const auto deleteBoundaryTombstone  = root.Path / "quarantine" / "v1"
			/ (lower_hex(deleteBoundaryRequest.Authority.Cache.bytes())
			   + ".deleted");
		require(
			!deleteBoundaryResult.has_value()
				&& deleteBoundaryResult.error()
					== application::git_cache_failure::repair_required
				&& std::filesystem::is_directory(
					deleteBoundaryRepository)
				&& !std::filesystem::exists(deleteBoundaryTombstone)
				&& read_file(deleteBoundaryLock)
					== std::vector<std::uint8_t>{
						'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
						'e', 'n', 't', '-', 'd', 'e', 'l', 'e',
						't', 'e', '-', 'l', 'o', 'c', 'k', '-',
						'c', 'a', 'n', 'a', 'r', 'y' }
				&& !deleteBoundarySecondOpen.has_value() && deleteBoundarySecondOpen.error() == application::git_cache_failure::invalid_manifest,
			"mid-delete lock substitution preserves the canary and stops before destructive publication");
		require(
			std::filesystem::remove(deleteBoundaryLock),
			"remove delete-boundary replacement lock canary");
		std::filesystem::rename(
			deleteBoundaryDisplacedLock,
			deleteBoundaryLock);
		require(
			(*cache)
				->delete_cache(
					deleteBoundaryRequest,
					deleteBoundaryAuthorization,
					{ })
				.has_value(),
			"restored delete-boundary lock permits authorized cleanup");
#endif

		const auto rootSubstitutionRequest = request(0x3fU, 0x4fU);
		auto       rootSubstitutionSession = (*cache)->open(rootSubstitutionRequest, { });
		require(
			rootSubstitutionSession.has_value(),
			"create repository-root substitution fixture");
		const auto rootSubstitutionOperation = identity<core::epoch_operation_id>(0x5dU);
		auto       rootSubstitutionSink      = (*rootSubstitutionSession)->create_pack_sink(rootSubstitutionOperation, sha1_pack.size(), { });
		require(
			rootSubstitutionSink.has_value(),
			"create repository-root substitution spool");
		append_pack(**rootSubstitutionSink, sha1_pack);
		const auto rootSubstitutionReceipt = (*rootSubstitutionSink)->finish({ });
		require(
			rootSubstitutionReceipt.has_value(),
			"finish repository-root substitution spool");
		rootSubstitutionSink->reset();
		const auto rootSubstitutionObservation = (*rootSubstitutionSession)->observe_preimport(*rootSubstitutionReceipt, { });
		const auto rootSubstitutionCommit      = core::parse_git_commit_oid(sha1_commit_hex);
		require(
			rootSubstitutionObservation.has_value()
				&& rootSubstitutionCommit.has_value(),
			"observe repository-root substitution baseline");
		const auto rootSubstitutionRepository = repository_path(
			root.Path,
			rootSubstitutionRequest);
		const auto rootSubstitutionOriginal = root.Path / "repositories" / "v1"
			/ ".bpr-test-original-root-substitution.git";
		std::filesystem::rename(
			rootSubstitutionRepository,
			rootSubstitutionOriginal);
		std::filesystem::copy(
			rootSubstitutionOriginal,
			rootSubstitutionRepository,
			std::filesystem::copy_options::recursive);
		make_tree_private(rootSubstitutionRepository);
		write_file(
			rootSubstitutionRepository
				/ "root-substitution-canary",
			"root-substitution-canary");
		const auto rootSubstitutionMarker = root.Path / "inject-cache-index-terminal-failure";
		write_file(rootSubstitutionMarker, "inject");
		const auto rootSubstitutionPrepare = (*rootSubstitutionSession)->prepare_revision(application::git_prepare_revision_request{
																							  .Receipt       = *rootSubstitutionReceipt,
																							  .Inventory     = *rootSubstitutionObservation,
																							  .Authorization = {
																								  .CatalogIncarnation      = identity<core::epoch_catalog_incarnation_id>(0x5eU),
																								  .ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
																								  .Operation               = rootSubstitutionOperation,
																								  .ExpectedPack            = rootSubstitutionReceipt->ExpectedPack,
																								  .ReceiptToken            = rootSubstitutionReceipt->OpaqueToken,
																								  .InventoryToken          = rootSubstitutionObservation->OpaqueToken,
																								  .DurableRecordToken      = token(0x5fU),
																							  },
																							  .Base = *rootSubstitutionCommit,
																							  .Head = *rootSubstitutionCommit,
																						  },
																						  { });
		std::filesystem::remove(rootSubstitutionMarker);
		const auto rootSubstitutionPackDirectory = rootSubstitutionRepository / "objects" / "pack";
		require(
			!rootSubstitutionPrepare.has_value()
				&& rootSubstitutionPrepare.error()
					== application::git_cache_failure::
						invalid_repository
				&& std::ranges::equal(
					read_file(
						rootSubstitutionRepository
						/ "root-substitution-canary"),
					std::string_view{ "root-substitution-canary" })
				&& std::filesystem::is_empty(
					rootSubstitutionPackDirectory),
			"bound repository root rejects same-manifest substitution before Git");
		std::filesystem::remove_all(
			rootSubstitutionRepository);
		std::filesystem::rename(
			rootSubstitutionOriginal,
			rootSubstitutionRepository);
		require(
			(*rootSubstitutionSession)
				->abort_operation(
					rootSubstitutionOperation,
					{ })
				.has_value(),
			"restored exact repository root remains recoverable");
		rootSubstitutionSession->reset();
		require(
			(*cache)
				->delete_cache(
					rootSubstitutionRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = rootSubstitutionRequest
												  .ApplicationSession,
						.Cache              = rootSubstitutionRequest
												  .Authority.Cache,
						.DurableIntentToken = token(0x72U),
					},
					{ })
				.has_value(),
			"repository-root substitution fixture exact-cleans");

		const auto firstReceiptOperation = identity<core::epoch_operation_id>(0x46U);
		auto       firstReceiptSink      = (*isolated)->create_pack_sink(
			firstReceiptOperation,
			sha1_pack.size(),
			{ });
		require(
			firstReceiptSink.has_value(),
			"create first receipt-cap operation");
		append_pack(**firstReceiptSink, sha1_pack);
		const auto firstReceipt = (*firstReceiptSink)->finish({ });
		require(firstReceipt.has_value(), "register first bounded receipt");
		firstReceiptSink->reset();
		const auto isolatedBaseline = (*isolated)->observe_preimport(*firstReceipt, { });
		require(
			isolatedBaseline.has_value(),
			"observe empty-cache quota baseline");

		const auto rejectedReceiptOperation = identity<core::epoch_operation_id>(0x47U);
		auto       rejectedReceiptSink      = (*isolated)->create_pack_sink(
			rejectedReceiptOperation,
			sha1_pack.size(),
			{ });
		require(
			rejectedReceiptSink.has_value(),
			"create receipt-cap rejection operation");
		append_pack(**rejectedReceiptSink, sha1_pack);
		const auto rejectedReceipt = (*rejectedReceiptSink)->finish({ });
		require(
			!rejectedReceipt.has_value()
				&& rejectedReceipt.error()
					== application::git_pack_stream_failure::
						invalid_state,
			"receipt registry cap fails after completed-spool publication");
		rejectedReceiptSink->reset();
		const auto rejectedReceiptPath = root.Path / "incoming" / "v1"
			/ (lower_hex(rejectedReceiptOperation.bytes())
			   + ".pack.part");
		require(
			!std::filesystem::exists(rejectedReceiptPath),
			"post-publication registration failure removes its owned spool");
		require(
			(*isolated)
				->abort_operation(firstReceiptOperation, { })
				.has_value(),
			"clear first receipt-cap operation");

		isolated->reset();
		auto isolatedQuotaRequest                      = isolatedRequest;
		isolatedQuotaRequest.Limits.OperationPackBytes = sha1_pack.size();
		isolatedQuotaRequest.Limits.RepositoryBytes    = isolatedBaseline->RepositoryBytes + sha1_pack.size();
		require(
			application::valid(isolatedQuotaRequest),
			"valid exact tight new-pack quota");
		isolated = (*cache)->open(isolatedQuotaRequest, { });
		require(isolated.has_value(), "open tight new-pack quota cache");
		const auto quotaOperation = identity<core::epoch_operation_id>(0x48U);
		auto       quotaSink      = (*isolated)->create_pack_sink(
			quotaOperation,
			sha1_pack.size(),
			{ });
		require(quotaSink.has_value(), "create tight-quota pack sink");
		append_pack(**quotaSink, sha1_pack);
		const auto quotaReceipt = (*quotaSink)->finish({ });
		require(quotaReceipt.has_value(), "finish tight-quota pack");
		quotaSink->reset();
		const auto quotaObservation = (*isolated)->observe_preimport(*quotaReceipt, { });
		require(
			!quotaObservation.has_value()
				&& quotaObservation.error()
					== application::git_cache_failure::quota_exceeded,
			"new-pack admission reserves index and paired-ref headroom");
		const auto isolatedPackDirectory = isolatedRepository / "objects" / "pack";
		require(
			!std::filesystem::exists(isolatedPackDirectory)
				|| std::filesystem::is_empty(isolatedPackDirectory),
			"new-pack quota rejection installs no pack or index");
		require(
			(*isolated)->abort_operation(quotaOperation, { }).has_value(),
			"tight-quota rejection exact-cleans its completed spool");
		isolated->reset();
		isolated = (*cache)->open(isolatedRequest, { });
		require(
			isolated.has_value(),
			"tight-quota rejection leaves cache reusable");

		const auto preobserveSubstitutionOperation = identity<core::epoch_operation_id>(0x58U);
		auto       preobserveSubstitutionSink      = (*isolated)->create_pack_sink(
			preobserveSubstitutionOperation,
			sha1_pack.size(),
			{ });
		require(
			preobserveSubstitutionSink.has_value(),
			"create pre-observation spool-substitution sink");
		append_pack(**preobserveSubstitutionSink, sha1_pack);
		const auto preobserveSubstitutionReceipt = (*preobserveSubstitutionSink)->finish({ });
		require(
			preobserveSubstitutionReceipt.has_value(),
			"finish pre-observation spool-substitution receipt");
		preobserveSubstitutionSink->reset();
		const auto preobserveSubstitutionPath = root.Path / "incoming" / "v1"
			/ (lower_hex(
				   preobserveSubstitutionOperation.bytes())
			   + ".pack.part");
		const auto preobserveSubstitutionOriginal = preobserveSubstitutionPath.parent_path()
			/ "preobserve-substitution-original.pack";
		std::filesystem::rename(
			preobserveSubstitutionPath,
			preobserveSubstitutionOriginal);
		write_file(preobserveSubstitutionPath, sha1_pack);
		const auto substitutedObservation = (*isolated)->observe_preimport(
			*preobserveSubstitutionReceipt,
			{ });
		require(
			!substitutedObservation.has_value()
				&& substitutedObservation.error()
					== application::git_cache_failure::spool_failure
				&& std::ranges::equal(
					read_file(preobserveSubstitutionPath),
					sha1_pack),
			"observation rejects and preserves a same-content replacement inode");
		std::filesystem::remove(preobserveSubstitutionPath);
		std::filesystem::rename(
			preobserveSubstitutionOriginal,
			preobserveSubstitutionPath);
		require(
			(*isolated)
				->abort_operation(
					preobserveSubstitutionOperation,
					{ })
				.has_value(),
			"pre-observation substitution leaves exact original recoverable");

		const auto preindexSubstitutionOperation = identity<core::epoch_operation_id>(0x59U);
		auto       preindexSubstitutionSink      = (*isolated)->create_pack_sink(
			preindexSubstitutionOperation,
			sha1_pack.size(),
			{ });
		require(
			preindexSubstitutionSink.has_value(),
			"create pre-index spool-substitution sink");
		append_pack(**preindexSubstitutionSink, sha1_pack);
		const auto preindexSubstitutionReceipt = (*preindexSubstitutionSink)->finish({ });
		require(
			preindexSubstitutionReceipt.has_value(),
			"finish pre-index spool-substitution receipt");
		preindexSubstitutionSink->reset();
		const auto preindexSubstitutionObservation = (*isolated)->observe_preimport(
			*preindexSubstitutionReceipt,
			{ });
		const auto preindexSubstitutionHead = core::parse_git_commit_oid(sha1_commit_hex);
		require(
			preindexSubstitutionObservation.has_value()
				&& preindexSubstitutionHead.has_value(),
			"observe pre-index spool-substitution baseline");
		const auto preindexSubstitutionPath = root.Path / "incoming" / "v1"
			/ (lower_hex(
				   preindexSubstitutionOperation.bytes())
			   + ".pack.part");
		const auto preindexSubstitutionOriginal = preindexSubstitutionPath.parent_path()
			/ "preindex-substitution-original.pack";
		std::filesystem::rename(
			preindexSubstitutionPath,
			preindexSubstitutionOriginal);
		write_file(preindexSubstitutionPath, sha1_pack);
		const auto indexFailureMarker = root.Path / "inject-cache-index-terminal-failure";
		write_file(indexFailureMarker, "inject");
		const auto substitutedPrepare = (*isolated)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *preindexSubstitutionReceipt,
				.Inventory     = *preindexSubstitutionObservation,
				.Authorization = {
					.CatalogIncarnation = identity<
						core::epoch_catalog_incarnation_id>(
						0x5aU),
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
					.Operation               = preindexSubstitutionOperation,
					.ExpectedPack            = preindexSubstitutionReceipt
												   ->ExpectedPack,
					.ReceiptToken            = preindexSubstitutionReceipt
												   ->OpaqueToken,
					.InventoryToken          = preindexSubstitutionObservation
												   ->OpaqueToken,
					.DurableRecordToken      = token(0x5bU),
				},
				.Base = *preindexSubstitutionHead,
				.Head = *preindexSubstitutionHead,
			},
			{ });
		std::filesystem::remove(indexFailureMarker);
		require(
			!substitutedPrepare.has_value()
				&& substitutedPrepare.error()
					== application::git_cache_failure::spool_failure
				&& std::ranges::equal(
					read_file(preindexSubstitutionPath),
					sha1_pack)
				&& (!std::filesystem::exists(
						isolatedPackDirectory)
					|| std::filesystem::is_empty(
						isolatedPackDirectory)),
			"pre-index identity check prevents Git handoff/import and preserves replacement");
		std::filesystem::remove(preindexSubstitutionPath);
		std::filesystem::rename(
			preindexSubstitutionOriginal,
			preindexSubstitutionPath);
		require(
			(*isolated)
				->abort_operation(
					preindexSubstitutionOperation,
					{ })
				.has_value(),
			"pre-index substitution leaves exact original recoverable");

		const auto rollbackOperation = identity<core::epoch_operation_id>(0x4aU);
		auto       rollbackSink      = (*isolated)->create_pack_sink(
			rollbackOperation,
			sha1_pack.size(),
			{ });
		require(rollbackSink.has_value(), "create rollback pack sink");
		append_pack(**rollbackSink, sha1_pack);
		const auto rollbackReceipt = (*rollbackSink)->finish({ });
		require(rollbackReceipt.has_value(), "finish rollback pack spool");
		rollbackSink->reset();
		const auto rollbackObservation = (*isolated)->observe_preimport(*rollbackReceipt, { });
		const auto rollbackHead        = core::parse_git_commit_oid(sha1_commit_hex);
		require(
			rollbackObservation.has_value()
				&& rollbackHead.has_value(),
			"prepare rollback fixture identities");
		auto missingBytes = rollbackHead->bytes();
		missingBytes.front() ^= 0x01U;
		const auto missingCommit = core::git_commit_oid{
			core::git_hash_algorithm::sha1,
			missingBytes
		};
		const auto rollbackPrepare = (*isolated)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *rollbackReceipt,
				.Inventory     = *rollbackObservation,
				.Authorization = {
					.CatalogIncarnation = identity<
						core::epoch_catalog_incarnation_id>(
						0x4bU),
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
					.Operation               = rollbackOperation,
					.ExpectedPack            = rollbackReceipt->ExpectedPack,
					.ReceiptToken            = rollbackReceipt->OpaqueToken,
					.InventoryToken          = rollbackObservation->OpaqueToken,
					.DurableRecordToken      = token(0x4cU),
				},
				.Base = missingCommit,
				.Head = *rollbackHead,
			},
			{ });
		require(
			!rollbackPrepare.has_value()
				&& rollbackPrepare.error()
					== application::git_cache_failure::process_failure,
			"missing requested commit fails after local pack import");
		require(
			!std::filesystem::exists(isolatedPackDirectory)
				|| std::filesystem::is_empty(isolatedPackDirectory),
			"failed pre-staging import rolls back only its new pack and index");
		require(
			(*isolated)
				->abort_operation(rollbackOperation, { })
				.has_value(),
			"failed pre-staging import retains exact spool for explicit abort");
		const auto committedFailureOperation = identity<core::epoch_operation_id>(0x4dU);
		auto       committedFailureSink      = (*isolated)->create_pack_sink(
			committedFailureOperation,
			sha1_pack.size(),
			{ });
		require(
			committedFailureSink.has_value(),
			"create committed-terminal-failure sink");
		append_pack(**committedFailureSink, sha1_pack);
		const auto committedFailureReceipt = (*committedFailureSink)->finish({ });
		require(
			committedFailureReceipt.has_value(),
			"finish committed-terminal-failure spool");
		committedFailureSink->reset();
		const auto committedFailureObservation = (*isolated)->observe_preimport(
			*committedFailureReceipt,
			{ });
		require(
			committedFailureObservation.has_value(),
			"observe committed-terminal-failure inventory");
		write_file(indexFailureMarker, "inject");
		const auto committedTerminalFailure = (*isolated)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *committedFailureReceipt,
				.Inventory     = *committedFailureObservation,
				.Authorization = {
					.CatalogIncarnation = identity<
						core::epoch_catalog_incarnation_id>(
						0x4eU),
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
					.Operation               = committedFailureOperation,
					.ExpectedPack            = committedFailureReceipt
												   ->ExpectedPack,
					.ReceiptToken            = committedFailureReceipt
												   ->OpaqueToken,
					.InventoryToken          = committedFailureObservation
												   ->OpaqueToken,
					.DurableRecordToken      = token(0x4fU),
				},
				.Base = *rollbackHead,
				.Head = *rollbackHead,
			},
			{ });
		std::filesystem::remove(indexFailureMarker);
		require(
			!committedTerminalFailure.has_value()
				&& committedTerminalFailure.error()
					== application::git_cache_failure::process_failure
				&& (!std::filesystem::exists(
						isolatedPackDirectory)
					|| std::filesystem::is_empty(
						isolatedPackDirectory)),
			"post-terminal index failure reconciles and rolls back only its new pack");
		require(
			(*isolated)
				->abort_operation(
					committedFailureOperation,
					{ })
				.has_value(),
			"reconciled index failure retains exact spool for abort");

		const auto malformedOperation = identity<core::epoch_operation_id>(0x51U);
		auto       malformedSink      = (*primary)->create_pack_sink(
			malformedOperation,
			sha1_pack.size(),
			{ });
		require(malformedSink.has_value(), "create malformed-pack sink");
		auto corrupted = sha1_pack;
		corrupted[40U] ^= 0x01U;
		append_pack(**malformedSink, corrupted);
		const auto malformed = (*malformedSink)->finish({ });
		require(
			!malformed.has_value()
				&& malformed.error()
					== application::git_pack_stream_failure::
						malformed_pack,
			"pack body/trailer checksum disagreement fails closed");
		malformedSink->reset();
		const auto malformedText = lower_hex(malformedOperation.bytes());
		require(
			!std::filesystem::exists(
				root.Path / "incoming" / "v1"
				/ (malformedText + ".pack.part"))
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ ("." + malformedText + ".pack.receiving")),
			"failed sink leaves no completed or receiving spool residue");

		const auto cancelledOperation = identity<core::epoch_operation_id>(0x49U);
		auto       cancelledSink      = (*primary)->create_pack_sink(
			cancelledOperation,
			sha1_pack.size(),
			{ });
		require(cancelledSink.has_value(), "create cancellable pack sink");
		auto cancelledSource = std::stop_source{ };
		cancelledSource.request_stop();
		const auto cancelledAppend = (*cancelledSink)->append(std::as_bytes(std::span{ sha1_pack }), cancelledSource.get_token());
		require(
			!cancelledAppend.has_value()
				&& cancelledAppend.error()
					== application::git_pack_stream_failure::cancelled,
			"cancelled pack append fails before admission");
		cancelledSink->reset();
		const auto cancelledText = lower_hex(cancelledOperation.bytes());
		require(
			!std::filesystem::exists(
				root.Path / "incoming" / "v1"
				/ (cancelledText + ".pack.part"))
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ ("." + cancelledText + ".pack.receiving")),
			"cancelled pack sink leaves no spool residue");
		const auto cancelledFinishOperation = identity<core::epoch_operation_id>(0x48U);
		auto       cancelledFinishSink      = (*primary)->create_pack_sink(
			cancelledFinishOperation,
			sha1_pack.size(),
			{ });
		require(
			cancelledFinishSink.has_value(),
			"create finish-cancellation pack sink");
		append_pack(**cancelledFinishSink, sha1_pack);
		const auto cancelledFinish = (*cancelledFinishSink)
										 ->finish(cancelledSource.get_token());
		require(
			!cancelledFinish.has_value()
				&& cancelledFinish.error()
					== application::git_pack_stream_failure::cancelled,
			"cancelled pack finish stops before publication");
		cancelledFinishSink->reset();
		const auto cancelledFinishText = lower_hex(cancelledFinishOperation.bytes());
		require(
			!std::filesystem::exists(
				root.Path / "incoming" / "v1"
				/ (cancelledFinishText + ".pack.part"))
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ ("." + cancelledFinishText
					   + ".pack.receiving")),
			"cancelled pack finish leaves no spool residue");

		// [[BPR-0889]]. The protocol parser refuses a stream that stops
		// mid-payload, and by then it has written part of the pack into the
		// real sink's receiving spool. The refusal must remove it. The parser
		// aborts the sink itself, so the check runs while the sink still exists
		// and cannot lean on its destructor.
		const auto truncatedOperation = identity<core::epoch_operation_id>(0x4eU);
		const auto truncatedText      = lower_hex(truncatedOperation.bytes());
		const auto truncatedReceiving = root.Path / "incoming" / "v1" / ("." + truncatedText + ".pack.receiving");
		const auto truncatedCompleted = root.Path / "incoming" / "v1" / (truncatedText + ".pack.part");
		auto       truncatedSink      = (*primary)->create_pack_sink(truncatedOperation, sha1_pack.size(), { });
		require(truncatedSink.has_value(), "create truncated-stream pack sink");
		{
			const auto pktLine = [](const std::string_view aPayload) {
				constexpr std::string_view digits = "0123456789abcdef";
				const auto                 total  = aPayload.size() + 4U;
				auto                       line   = std::string{ digits[(total >> 12U) & 0x0fU], digits[(total >> 8U) & 0x0fU],
																 digits[(total >> 4U) & 0x0fU], digits[total & 0x0fU] };
				line += aPayload;
				return line;
			};
			auto packPayload = std::string(1U, static_cast<char>(1));
			packPayload.append(reinterpret_cast<const char*>(sha1_pack.data()), sha1_pack.size());
			const auto                                        sectionHeader = pktLine("packfile\n");
			const auto                                        whole         = sectionHeader + pktLine(packPayload) + "0000";
			constexpr auto                                    partialBytes  = std::size_t{ 40U };
			const auto                                        cut           = sectionHeader.size() + 5U + partialBytes;
			application::detail::git_protocol_v2_fetch_parser parser{
				**truncatedSink,
				application::git_protocol_v2_fetch_limits{
					.MaximumWireBytes     = 4096U,
					.MaximumPackBytes     = sha1_pack.size(),
					.MaximumProgressBytes = 128U,
					.MaximumPackets       = 16U,
				},
				core::git_hash_algorithm::sha1,
				{ },
			};
			require(parser.consume(std::as_bytes(std::span{ whole.data(), cut })), "a stream cut mid-payload is valid up to the cut");
			require(std::filesystem::is_regular_file(truncatedReceiving) && std::filesystem::file_size(truncatedReceiving) == partialBytes,
					"the parser wrote the partial pack into the receiving spool");
			const auto refused = parser.finish();
			require(!refused.has_value() && refused.error().Failure == application::git_protocol_v2_failure::incomplete_response,
					"the truncated stream finishes as incomplete_response");
			require(!std::filesystem::exists(truncatedReceiving) && !std::filesystem::exists(truncatedCompleted),
					"the refusal removes the partial pack while the sink still exists");
		}
		truncatedSink->reset();
		require(!std::filesystem::exists(truncatedReceiving) && !std::filesystem::exists(truncatedCompleted),
				"a truncated stream leaves no spool residue");

		const auto operation = identity<core::epoch_operation_id>(0x52U);
		auto       sink      = (*primary)->create_pack_sink(
			operation,
			sha1_pack.size(),
			{ });
		require(sink.has_value(), "create bounded private pack sink");
		append_pack(**sink, sha1_pack);
		const auto receipt = (*sink)->finish({ });
		require(
			receipt.has_value() && application::agrees(*receipt)
				&& receipt->ExpectedPack == pack_oid(sha1_pack_hex)
				&& receipt->Spool.Role
					== application::git_artifact_role::
						completed_spool
				&& receipt->Spool.Bytes == sha1_pack.size()
				&& application::valid(receipt->Spool.Identity)
				&& application::valid_cache_token(receipt->OpaqueToken),
			"sink validates PACK header/trailer and issues opaque receipt");
		const auto serializedSpoolIdentity = receipt->Spool.Identity.Bytes;
		const auto restoredSpoolIdentity   = application::git_artifact_identity{
			.Bytes = serializedSpoolIdentity
		};
		require(
			restoredSpoolIdentity == receipt->Spool.Identity
				&& application::valid(restoredSpoolIdentity),
			"completed spool identity round-trips as fixed opaque bytes");
		sink->reset();
		const auto completedSpool = root.Path / "incoming" / "v1"
			/ (lower_hex(operation.bytes()) + ".pack.part");
		require(
			std::filesystem::is_regular_file(completedSpool),
			"validated receipt publishes only the exact completed spool");

		const auto observation = (*primary)->observe_preimport(*receipt, { });
		require(
			observation.has_value() && observation->Packs.empty()
				&& !observation->ExpectedPackPreexisting
				&& observation->RepositoryBytes != 0U
				&& application::valid_cache_token(
					observation->OpaqueToken),
			"preimport inventory is bounded and tokenized");
		auto forgedSpoolReceipt = *receipt;
		forgedSpoolReceipt.Spool.Identity.Bytes.back() ^= 0x01U;
		const auto forgedSpoolObservation = (*primary)->observe_preimport(
			forgedSpoolReceipt,
			{ });
		require(
			application::agrees(forgedSpoolReceipt)
				&& !forgedSpoolObservation.has_value()
				&& forgedSpoolObservation.error()
					== application::git_cache_failure::spool_failure,
			"opaque durable spool identity is authorization-bound");

		const auto commit = core::parse_git_commit_oid(sha1_commit_hex);
		const auto tree   = core::parse_git_tree_oid(sha1_tree_hex);
		require(
			commit.has_value() && tree.has_value(),
			"parse fixture commit and tree");
		const auto incarnation   = identity<core::epoch_catalog_incarnation_id>(0x61U);
		auto       authorization = application::git_import_authorization{
			.CatalogIncarnation      = incarnation,
			.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
			.Operation               = operation,
			.ExpectedPack            = receipt->ExpectedPack,
			.ReceiptToken            = receipt->OpaqueToken,
			.InventoryToken          = observation->OpaqueToken,
			.DurableRecordToken      = token(0x62U),
		};
		auto forgedObservation             = *observation;
		forgedObservation.OpaqueToken      = token(0x63U);
		auto forgedAuthorization           = authorization;
		forgedAuthorization.InventoryToken = forgedObservation.OpaqueToken;
		const auto sha256Prior             = core::parse_git_commit_oid(
			std::string(64U, 'f'));
		auto mixedAlgorithmRequest = application::git_prepare_revision_request{
			.Receipt       = *receipt,
			.Inventory     = *observation,
			.Authorization = authorization,
			.Base          = *commit,
			.Head          = *commit,
			.PriorHead     = sha256Prior,
		};
		require(
			sha256Prior.has_value()
				&& !application::valid(mixedAlgorithmRequest),
			"prior head must use the exact current object format");
		const auto unauthorized = (*primary)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *receipt,
				.Inventory     = forgedObservation,
				.Authorization = forgedAuthorization,
				.Base          = *commit,
				.Head          = *commit,
			},
			{ });
		require(
			!unauthorized.has_value()
				&& unauthorized.error()
					== application::git_cache_failure::
						authorization_mismatch,
			"caller cannot forge preimport authorization facts");

		std::size_t installationWitnesses = 0U;
		const auto  prepared              = (*primary)->prepare_historical_revision(
			application::git_prepare_historical_revision_request{
				.Receipt       = *receipt,
				.Inventory     = *observation,
				.Authorization = authorization,
				.SelectedBase  = *commit,
				.Head          = *commit,
			},
			{ }, [&](const application::git_prepared_revision& aInstallation) {
				++installationWitnesses;
				const auto refs = run_fixture_git(executable, root.Path,
												  { "--git-dir=" + QString::fromStdString(primaryRepository.string()), "for-each-ref" });
				require(application::valid(aInstallation) && aInstallation.Pack == receipt->ExpectedPack
							&& refs.Output.isEmpty() && std::filesystem::is_regular_file(completedSpool),
						"exact installation witness precedes staging refs and completed spool removal");
				return true;
			});
		require(installationWitnesses == 1U, "historical head installation witness runs exactly once");
		require(
			prepared.has_value() && application::valid(*prepared)
				&& prepared->Base == *commit && prepared->Head == *commit
				&& prepared->HeadLineage == core::head_lineage::not_applicable
				&& prepared->BaseTree == *tree
				&& prepared->HeadTree == *tree
				&& prepared->Pack == receipt->ExpectedPack
				&& prepared->ImportedBytes == sha1_pack.size()
				&& prepared->InstalledPack.Role
					== application::git_artifact_role::
						installed_pack
				&& prepared->InstalledPack.Bytes
					== sha1_pack.size()
				&& prepared->InstalledIndex.Role
					== application::git_artifact_role::
						installed_index
				&& prepared->InstalledIndex.Bytes != 0U
				&& prepared->InstalledPack.Identity
					!= prepared->InstalledIndex.Identity
				&& prepared->HeadLineage
					== core::head_lineage::not_applicable,
			"authorized import exposes exact durable pack/index facts, "
			"no-prior lineage, and stages refs");
		const auto primaryGitDir = "--git-dir="
			+ QString::fromStdString(primaryRepository.string());
		const auto treeListing = run_fixture_git(
			executable,
			root.Path,
			{
				primaryGitDir,
				"ls-tree",
				"-z",
				"-r",
				QString::fromUtf8(sha1_commit_hex.data()),
			});
		const auto treePathSeparator = treeListing.Output.indexOf('\t');
		require(
			treeListing.Output.startsWith("100644 blob ")
				&& treePathSeparator > 0
				&& treeListing.Output.endsWith('\0')
				&& treeListing.Output.indexOf('\0')
					== treeListing.Output.size() - 1,
			"imported fixture exposes one typed blob identity");
		const auto blobText = treeListing.Output.mid(12, 40).toStdString();
		const auto blob     = core::parse_git_blob_oid(blobText);
		require(blob.has_value(), "parse imported fixture blob identity");
		const auto treePath = treeListing.Output
								  .mid(
									  treePathSeparator + 1,
									  treeListing.Output.size() - treePathSeparator - 2)
								  .toStdString();
		require(!treePath.empty(), "parse imported fixture tree path");
		const auto resolvedBlob = (*primary)->find_blob_at_path(
			{
				.Commit = *commit,
				.Path   = treePath,
			},
			{ });
		const auto absentBlob = (*primary)->find_blob_at_path(
			{
				.Commit = *commit,
				.Path   = treePath + ".bpr-absent",
			},
			{ });
		const auto unchangedCatalogue = (*primary)->changed_entries({ *commit, *commit }, { });
		require(unchangedCatalogue && unchangedCatalogue->empty(), "installed cache forwards complete exact-tree catalogue through its retained session");
		const auto missingCatalogue = (*primary)->changed_entries({ *core::parse_git_commit_oid(std::string(40U, 'f')), *commit }, { });
		require(!missingCatalogue, "installed cache retains missing historical trees as failure, never empty membership");
		const auto unchangedRenames = (*primary)->find_rename_pairs(
			{
				.Left         = *commit,
				.Right        = *commit,
				.CurrentPath  = treePath,
				.PreviousPath = treePath + ".previous",
			},
			{ });
		require(
			resolvedBlob.has_value() && resolvedBlob->has_value(),
			"retained cache session resolves an exact tree path");
		require(
			**resolvedBlob == *blob,
			"retained cache session returns the exact tree blob");
		require(
			absentBlob.has_value() && !absentBlob->has_value(),
			"retained cache session reports an absent tree path");
		require(
			unchangedRenames.has_value() && unchangedRenames->Count == 0U,
			"retained cache session forwards an exact unchanged rename query");
		const auto invalidPath = (*primary)->find_blob_at_path(
			{
				.Commit = *commit,
				.Path   = "../file.txt",
			},
			{ });
		require(
			!invalidPath.has_value()
				&& invalidPath.error()
					== application::git_cache_failure::invalid_request,
			"retained cache refuses non-normal repository paths");
		auto lookupCancelledSource = std::stop_source{ };
		static_cast<void>(lookupCancelledSource.request_stop());
		const auto cancelledLookup = (*primary)->find_blob_at_path(
			{
				.Commit = *commit,
				.Path   = treePath,
			},
			lookupCancelledSource.get_token());
		require(
			!cancelledLookup.has_value()
				&& cancelledLookup.error()
					== application::git_cache_failure::cancelled,
			"pre-cancelled retained-cache lookup never reaches Git");
		const auto expectedBlob = run_fixture_git(
			executable,
			root.Path,
			{
				primaryGitDir,
				"cat-file",
				"blob",
				QString::fromStdString(blobText),
			});
		const auto materializedBlob = (*primary)->materialize_blob(
			{
				.Blob         = *blob,
				.MaximumBytes = static_cast<std::uint64_t>(expectedBlob.Output.size()),
			},
			{ });
		require(
			materializedBlob.has_value()
				&& std::ranges::equal(*materializedBlob, expectedBlob.Output),
			"cache session materializes the exact imported blob bytes");
		const auto limitedBlob = (*primary)->materialize_blob(
			{
				.Blob         = *blob,
				.MaximumBytes = static_cast<std::uint64_t>(expectedBlob.Output.size() - 1),
			},
			{ });
		require(
			!limitedBlob.has_value()
				&& limitedBlob.error()
					== application::git_cache_failure::quota_exceeded,
			"cache session preserves the pre-retention blob ceiling");
		const auto wrongTypeBlob = (*primary)->materialize_blob(
			{
				.Blob = core::git_blob_oid{
					commit->algorithm(), commit->bytes() },
				.MaximumBytes = 1'024U,
			},
			{ });
		require(
			!wrongTypeBlob.has_value()
				&& wrongTypeBlob.error()
					== application::git_cache_failure::process_failure,
			"cache session fails closed on a non-blob object identity");

		auto missingHeadBytes = std::array<std::uint8_t, 32U>{ };
		std::fill_n(missingHeadBytes.begin(), 20U, 0xeeU);
		const auto missingHead = core::git_commit_oid{
			core::git_hash_algorithm::sha1,
			missingHeadBytes,
		};
		auto       workspaceRoot        = temporary_root{ };
		const auto workspaceParent      = workspaceRoot.Path;
		const auto unavailableWorkspace = (*primary)->materialize_agent_workspace(
			{
				.ApplicationSession = primaryRequest.ApplicationSession,
				.Authority          = primaryRequest.Authority,
				.Session            = {
					.Id      = core::agent_session_id{ 77U },
					.Profile = core::agent_profile_id{ 88U },
					.Binding = {
						.Host        = primaryRequest.Authority.Host,
						.Repository  = primaryRequest.Authority.Repository,
						.PullRequest = primaryRequest.Authority.PullRequest,
						.BaseOid     = *commit,
						.HeadOid     = missingHead,
						.LeftEpoch   = core::epoch_ordinal{ 1 },
						.RightEpoch  = core::epoch_ordinal{ 2 },
					},
				},
				.ParentDirectory = workspaceParent,
			},
			{ });
		require(
			!unavailableWorkspace.has_value()
				&& unavailableWorkspace.error()
					== application::git_cache_failure::process_failure,
			"cache maps an unmaterializable exact head to process failure");
		require(
			!unavailableWorkspace.error().Detail.empty(),
			"cache retains the installed-Git diagnosis for the exact missing head");
		auto invalidLineage        = *prepared;
		invalidLineage.HeadLineage = static_cast<core::head_lineage>(0xffU);
		require(
			!application::valid(invalidLineage),
			"prepared revision rejects an out-of-domain lineage");
		require(
			!std::filesystem::exists(completedSpool),
			"successful import exact-deletes its completed spool");

		const auto recoveryRequest = application::git_operation_recovery_request{
			.Operation = operation,
			.Scope     = primaryRequest.Authority.Scope,
			.Ordinal   = core::epoch_ordinal{ 1 },
			.Base      = *commit,
			.Head      = *commit,
			.Spool     = application::git_recovery_spool_expectation{
				.ExpectedPack             = receipt->ExpectedPack,
				.CompletedSpool           = receipt->Spool,
				.PreimportPacks           = observation->Packs,
				.PreimportRepositoryBytes = observation->RepositoryBytes,
				.ExpectedPackPreexisting  = observation->ExpectedPackPreexisting,
			},
			.Install = application::git_recovery_install_expectation{
				.Pack           = prepared->Pack,
				.InstalledPack  = prepared->InstalledPack,
				.InstalledIndex = prepared->InstalledIndex,
				.BaseTree       = prepared->BaseTree,
				.HeadTree       = prepared->HeadTree,
			},
		};
		require(
			application::valid(recoveryRequest),
			"durable append facts form a valid recovery request");
		const auto liveRecovery = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		require(
			liveRecovery.has_value()
				&& liveRecovery->Spool
					== application::git_recovery_match::absent
				&& liveRecovery->Installed
					== application::git_recovery_match::exact
				&& liveRecovery->Staging
					== application::git_recovery_match::exact
				&& liveRecovery->Final
					== application::git_recovery_match::absent
				&& liveRecovery->Objects
					== application::git_recovery_object_match::exact
				&& liveRecovery->Refs
					== application::git_recovery_ref_topology::staging,
			"live recovery inspection agrees with exact staged durable facts");
		auto unrecordedStaging = recoveryRequest;
		unrecordedStaging.Install.reset();
		const auto unrecordedStagingObservation = (*primary)->inspect_operation_recovery(unrecordedStaging, { });
		const auto unrecordedStagingAbort       = (*primary)->abort_recovered_operation(unrecordedStaging, { });
		require(unrecordedStagingObservation
					&& unrecordedStagingObservation->Installed == application::git_recovery_match::inventory_only
					&& !unrecordedStagingAbort
					&& unrecordedStagingAbort.error() == application::git_cache_failure::repair_required,
				"inventory-only installation cannot authorize removal of staged refs");
		auto recoveryCancel = std::stop_source{ };
		recoveryCancel.request_stop();
		const auto cancelledRecovery = (*primary)->inspect_operation_recovery(
			recoveryRequest,
			recoveryCancel.get_token());
		const auto cancelledCommit = (*primary)->inspect_commit_object(
			*commit,
			recoveryCancel.get_token());
		const auto cancelledAbort = (*primary)->abort_recovered_operation(
			recoveryRequest,
			recoveryCancel.get_token());
		const auto cancelledCompletion = (*primary)->complete_recovered_operation(
			recoveryRequest,
			recoveryCancel.get_token());
		require(
			!cancelledRecovery.has_value()
				&& cancelledRecovery.error()
					== application::git_cache_failure::cancelled
				&& !cancelledCommit.has_value()
				&& cancelledCommit.error()
					== application::git_cache_failure::cancelled
				&& !cancelledAbort.has_value()
				&& cancelledAbort.error()
					== application::git_cache_failure::cancelled
				&& !cancelledCompletion.has_value()
				&& cancelledCompletion.error()
					== application::git_cache_failure::cancelled,
			"restart inspection and cleanup entry points honor cancellation without mutation");
		primary->reset();
		primary = (*cache)->open(primaryRequest, { });
		require(primary.has_value(), "reopen cache for restart recovery");
		const auto restartedRecovery = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		require(
			restartedRecovery == liveRecovery,
			"restart reconstructs the same spool/install/staging/final evidence without process-local records");
		const auto installedIndexPath = primaryRepository / "objects" / "pack"
			/ ("pack-" + std::string{ sha1_pack_hex } + ".idx");
		const auto savedInstalledIndex = root.Path / "saved-recovery-index";
		std::filesystem::rename(
			installedIndexPath,
			savedInstalledIndex);
		require(
			std::filesystem::copy_file(
				savedInstalledIndex,
				installedIndexPath),
			"substitute installed recovery index identity");
		std::filesystem::permissions(
			installedIndexPath,
			std::filesystem::perms::owner_read,
			std::filesystem::perm_options::replace);
		const auto substitutedInstall             = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		const auto refusedSubstitutedInstallAbort = (*primary)->abort_recovered_operation(recoveryRequest, { });
		require(
			substitutedInstall.has_value()
				&& substitutedInstall->Installed
					== application::git_recovery_match::mismatch
				&& !refusedSubstitutedInstallAbort.has_value()
				&& refusedSubstitutedInstallAbort.error()
					== application::git_cache_failure::repair_required,
			"restart inspection rejects a byte-identical installed artifact with the wrong durable identity");
		require(
			std::filesystem::remove(installedIndexPath),
			"remove substituted recovery index");
		std::filesystem::rename(
			savedInstalledIndex,
			installedIndexPath);
		require(
			(*primary)->inspect_operation_recovery(recoveryRequest, { })
				== liveRecovery,
			"restoring the exact installed artifact restores recovery evidence");
		const auto verifiedCommit    = (*primary)->inspect_commit_object(*commit, { });
		const auto unavailableCommit = core::parse_git_commit_oid(
			std::string(40U, 'f'));
		require(
			unavailableCommit.has_value(),
			"parse unavailable exact commit fixture");
		const auto rejectedUnavailable = (*primary)->inspect_commit_object(*unavailableCommit, { });
		require(
			verifiedCommit.has_value()
				&& verifiedCommit->Commit == *commit
				&& verifiedCommit->Tree == *tree
				&& !rejectedUnavailable.has_value()
				&& rejectedUnavailable.error()
					== application::git_cache_failure::object_mismatch,
			"exact commit inspection verifies tree/closure and rejects catalog-only availability");

		const auto derived        = (*primary)->derive_comparison_base({ .Head = *commit, .SelectedBase = *commit }, { });
		const auto missingDerived = (*primary)->derive_comparison_base({ .Head = *unavailableCommit, .SelectedBase = *commit }, { });
		const auto commitTime     = (*primary)->read_committer_time_seconds(*commit, { });
		const auto missingTime    = (*primary)->read_committer_time_seconds(*unavailableCommit, { });
		require(derived && derived->Status == application::git_comparison_base_status::unique && derived->Base == *commit
					&& missingDerived && missingDerived->Status == application::git_comparison_base_status::objects_unavailable && !missingDerived->Base
					&& commitTime && commitTime->has_value() && missingTime && !*missingTime,
				"active cache session reads exact derived base and optional committer date after restart");
		const auto cancelledDerived = (*primary)->derive_comparison_base(
			{ .Head = *commit, .SelectedBase = *commit }, recoveryCancel.get_token());
		const auto cancelledTime = (*primary)->read_committer_time_seconds(*commit, recoveryCancel.get_token());
		require(!cancelledDerived && cancelledDerived.error() == application::git_cache_failure::cancelled
					&& !cancelledTime && cancelledTime.error() == application::git_cache_failure::cancelled,
				"active cache history queries honor cancellation before effects");
		const auto wrongAlgorithm = core::parse_git_commit_oid(std::string(64U, 'f'));
		require(wrongAlgorithm.has_value(), "parse mismatched cache history query algorithm");
		const auto invalidDerived = (*primary)->derive_comparison_base({ .Head = *commit, .SelectedBase = *wrongAlgorithm }, { });
		const auto invalidTime    = (*primary)->read_committer_time_seconds(*wrongAlgorithm, { });
		require(!invalidDerived && invalidDerived.error() == application::git_cache_failure::invalid_request
					&& !invalidTime && invalidTime.error() == application::git_cache_failure::invalid_request,
				"cache history queries cannot cross object-format authority");

		const auto gitDir        = "--git-dir=" + QString::fromStdString(primaryRepository.string());
		const auto stagingPrefix = "refs/betterpullreview/v1/stage/"
			+ lower_hex(operation.bytes());
		const auto stagingHead = stagingPrefix + "/head";
		const auto finalPrefix = "refs/betterpullreview/v1/scope/"
			+ lower_hex(primaryRequest.Authority.Scope.bytes())
			+ "/epoch/0000000000000001";
		const auto finalBase      = finalPrefix + "/base";
		const auto makeRefPrivate = [&](const std::string_view aRef) {
			std::error_code error;
			auto            directory = (primaryRepository / aRef).parent_path();
			while (directory != primaryRepository) {
				std::filesystem::permissions(
					directory,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace,
					error);
				require(!error, "make fixture recovery ref directory private");
				directory = directory.parent_path();
			}
			std::filesystem::permissions(
				primaryRepository / aRef,
				std::filesystem::perms::owner_read
					| std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace,
				error);
			require(!error, "make fixture recovery ref private");
		};
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				"-d",
				QString::fromStdString(stagingHead),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		const auto mixedStaging      = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		const auto refusedMixedAbort = (*primary)->abort_recovered_operation(recoveryRequest, { });
		require(
			mixedStaging.has_value()
				&& mixedStaging->Staging
					== application::git_recovery_match::ambiguous
				&& mixedStaging->Refs
					== application::git_recovery_ref_topology::ambiguous
				&& !refusedMixedAbort.has_value()
				&& refusedMixedAbort.error()
					== application::git_cache_failure::repair_required,
			"one-sided staging state is ambiguous and exact abort preserves it");
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(stagingHead),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		makeRefPrivate(stagingHead);
		const auto wrongRefOid = std::string{ sha1_tree_hex };
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(stagingHead),
				QString::fromStdString(wrongRefOid),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		makeRefPrivate(stagingHead);
		const auto wrongStaging      = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		const auto refusedWrongAbort = (*primary)->abort_recovered_operation(recoveryRequest, { });
		require(
			wrongStaging.has_value()
				&& wrongStaging->Staging
					== application::git_recovery_match::mismatch
				&& !refusedWrongAbort.has_value()
				&& refusedWrongAbort.error()
					== application::git_cache_failure::repair_required,
			"wrong staging target remains visible as mismatch and is never exact-cleaned");
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(stagingHead),
				QString::fromUtf8(sha1_commit_hex.data()),
				QString::fromStdString(wrongRefOid),
			}));
		makeRefPrivate(stagingHead);
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(finalBase),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		makeRefPrivate(finalBase);
		const auto mixedFinal             = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		const auto refusedMixedCompletion = (*primary)->complete_recovered_operation(recoveryRequest, { });
		require(
			mixedFinal.has_value()
				&& mixedFinal->Staging
					== application::git_recovery_match::exact
				&& mixedFinal->Final
					== application::git_recovery_match::ambiguous
				&& mixedFinal->Refs
					== application::git_recovery_ref_topology::ambiguous
				&& !refusedMixedCompletion.has_value()
				&& refusedMixedCompletion.error()
					== application::git_cache_failure::repair_required,
			"one-sided final state never authorizes catalog success or cleanup");
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				"-d",
				QString::fromStdString(finalBase),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		const auto restoredStaging = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		require(
			restoredStaging.has_value()
				&& restoredStaging->Staging
					== application::git_recovery_match::exact
				&& restoredStaging->Final
					== application::git_recovery_match::absent,
			"restoring exact refs restores deterministic recovery evidence");

		auto wrongPromotion = application::git_ref_promotion_authorization{
			.CatalogIncarnation      = incarnation,
			.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
			.Operation               = operation,
			.Scope                   = identity<core::pull_request_authority_scope_id>(
				0x43U),
			.Ordinal            = core::epoch_ordinal{ 1 },
			.Base               = *commit,
			.Head               = *commit,
			.DurableCommitToken = token(0x64U),
		};
		const auto rejectedPromotion = (*primary)->promote_retained(
			*prepared,
			wrongPromotion,
			{ });
		require(
			!rejectedPromotion.has_value()
				&& rejectedPromotion.error()
					== application::git_cache_failure::
						authorization_mismatch,
			"promotion requires exact durable scope authorization");

		auto promotion          = wrongPromotion;
		promotion.Scope         = primaryRequest.Authority.Scope;
		auto forgedPreparedFact = *prepared;
		forgedPreparedFact.InstalledIndex
			.Identity.Bytes.back() ^= 0x01U;
		const auto rejectedPreparedFact = (*primary)->promote_retained(
			forgedPreparedFact,
			promotion,
			{ });
		require(
			application::valid(forgedPreparedFact)
				&& !rejectedPreparedFact.has_value()
				&& rejectedPreparedFact.error()
					== application::git_cache_failure::
						authorization_mismatch,
			"promotion revalidates durable installed-artifact facts");
		const auto retained = (*primary)->promote_retained(*prepared, promotion, { });
		require(
			retained.has_value() && retained->Base == *commit
				&& retained->Head == *commit
				&& retained->Ordinal == core::epoch_ordinal{ 1 },
			"paired staging refs atomically promote to final refs");
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(stagingPrefix + "/base"),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		makeRefPrivate(stagingPrefix + "/base");
		static_cast<void>(run_fixture_git(
			executable,
			root.Path,
			{
				gitDir,
				"update-ref",
				QString::fromStdString(stagingHead),
				QString::fromUtf8(sha1_commit_hex.data()),
			}));
		makeRefPrivate(stagingHead);
		const auto committedWithResidue = (*primary)->inspect_operation_recovery(recoveryRequest, { });
		const auto completedRecovery    = (*primary)->complete_recovered_operation(recoveryRequest, { });
		const auto completedReplay      = (*primary)->complete_recovered_operation(recoveryRequest, { });
		require(
			committedWithResidue.has_value()
				&& committedWithResidue->Staging
					== application::git_recovery_match::exact
				&& committedWithResidue->Final
					== application::git_recovery_match::exact
				&& committedWithResidue->Refs
					== application::git_recovery_ref_topology::ambiguous
				&& completedRecovery.has_value()
				&& completedReplay.has_value(),
			"successful cleanup exact-deletes committed staging residue, is idempotent, and never enters abort lifecycle");
		auto operationAfterSuccess = (*primary)->create_pack_sink(
			operation,
			sha1_pack.size(),
			{ });
		require(
			operationAfterSuccess.has_value(),
			"successful cleanup leaves its operation identity outside the bounded abort history");
		operationAfterSuccess->reset();
		const auto inspected = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		require(
			inspected.has_value() && *inspected == *retained,
			"retained pair inspection preserves exact stable identity");

		const auto recursiveScanErrorMarker = root.Path
			/ "inject-cache-recursive-scan-terminal-error";
		write_file(recursiveScanErrorMarker, "inject");
		const auto recursiveScanError = (*primary)->inspect_retained(
			core::epoch_ordinal{ 1 },
			{ });
		std::filesystem::remove(recursiveScanErrorMarker);
		const auto recursiveScanRetry = (*primary)->inspect_retained(
			core::epoch_ordinal{ 1 },
			{ });
		require(
			!recursiveScanError.has_value()
				&& recursiveScanError.error()
					== application::git_cache_failure::
						invalid_repository
				&& recursiveScanRetry.has_value()
				&& *recursiveScanRetry == *retained,
			"terminal recursive inventory errors stay typed and retryable");

		const auto packScanErrorMarker = root.Path
			/ "inject-cache-pack-scan-terminal-error";
		write_file(packScanErrorMarker, "inject");
		const auto packScanError = (*primary)->inspect_retained(
			core::epoch_ordinal{ 1 },
			{ });
		std::filesystem::remove(packScanErrorMarker);
		const auto packScanRetry = (*primary)->inspect_retained(
			core::epoch_ordinal{ 1 },
			{ });
		require(
			!packScanError.has_value()
				&& packScanError.error()
					== application::git_cache_failure::
						invalid_repository
				&& packScanRetry.has_value()
				&& *packScanRetry == *retained,
			"terminal pack-directory scan errors stay typed and retryable");

		const auto preexistingPrepareOperation = identity<core::epoch_operation_id>(0x5cU);
		auto       preexistingPrepareSink      = (*primary)->create_pack_sink(
			preexistingPrepareOperation,
			sha1_pack.size(),
			{ });
		require(
			preexistingPrepareSink.has_value(),
			"create preexisting-pack durable-fact sink");
		append_pack(**preexistingPrepareSink, sha1_pack);
		const auto preexistingPrepareReceipt = (*preexistingPrepareSink)->finish({ });
		require(
			preexistingPrepareReceipt.has_value(),
			"finish preexisting-pack durable-fact spool");
		preexistingPrepareSink->reset();
		const auto preexistingPrepareObservation = (*primary)->observe_preimport(
			*preexistingPrepareReceipt,
			{ });
		require(
			preexistingPrepareObservation.has_value()
				&& preexistingPrepareObservation
					   ->ExpectedPackPreexisting,
			"observe exact preexisting pack before repeat prepare");
		const auto preexistingPrepared = (*primary)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *preexistingPrepareReceipt,
				.Inventory     = *preexistingPrepareObservation,
				.Authorization = {
					.CatalogIncarnation      = incarnation,
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
					.Operation               = preexistingPrepareOperation,
					.ExpectedPack            = preexistingPrepareReceipt
												   ->ExpectedPack,
					.ReceiptToken            = preexistingPrepareReceipt
												   ->OpaqueToken,
					.InventoryToken          = preexistingPrepareObservation
												   ->OpaqueToken,
					.DurableRecordToken      = token(0x60U),
				},
				.Base      = *commit,
				.Head      = *commit,
				.PriorHead = commit,
			},
			{ });
		require(
			preexistingPrepared.has_value()
				&& preexistingPrepared->InstalledPack
					== prepared->InstalledPack
				&& preexistingPrepared->InstalledIndex
					== prepared->InstalledIndex
				&& preexistingPrepared->HeadLineage
					== core::head_lineage::not_applicable,
			"same-head prepare returns durable facts with not-applicable lineage");
		require(
			(*primary)
				->abort_operation(
					preexistingPrepareOperation,
					{ })
				.has_value(),
			"preexisting durable-fact prepare exact-cleans staging refs");

		const auto missingHistoricalOperation = identity<core::epoch_operation_id>(0x6aU);
		auto       missingHistoricalSink      = (*primary)->create_pack_sink(missingHistoricalOperation, sha1_pack.size(), { });
		require(missingHistoricalSink.has_value(), "create missing historical head spool");
		append_pack(**missingHistoricalSink, sha1_pack);
		const auto missingHistoricalReceipt = (*missingHistoricalSink)->finish({ });
		require(missingHistoricalReceipt.has_value(), "finish missing historical head spool");
		missingHistoricalSink->reset();
		const auto missingHistoricalInventory = (*primary)->observe_preimport(*missingHistoricalReceipt, { });
		require(missingHistoricalInventory.has_value(), "observe missing historical head inventory");
		std::size_t missingHistoricalWitnesses = 0U;
		const auto  missingHistorical          = (*primary)->prepare_historical_revision({
																							 .Receipt       = *missingHistoricalReceipt,
																							 .Inventory     = *missingHistoricalInventory,
																							 .Authorization = {
																								 .CatalogIncarnation      = incarnation,
																								 .ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
																								 .Operation               = missingHistoricalOperation,
																								 .ExpectedPack            = missingHistoricalReceipt->ExpectedPack,
																								 .ReceiptToken            = missingHistoricalReceipt->OpaqueToken,
																								 .InventoryToken          = missingHistoricalInventory->OpaqueToken,
																								 .DurableRecordToken      = token(0x6aU),
																							 },
																							 .SelectedBase = *commit,
																							 .Head         = missingHead,
																						 },
																						 { }, [&](const application::git_prepared_revision&) {
																				   ++missingHistoricalWitnesses;
																				   return true;
																						 });
		require(!missingHistorical && missingHistoricalWitnesses == 0U,
				"unavailable historical head never witnesses or stages a comparison");

		const auto unavailablePriorOperation = identity<core::epoch_operation_id>(0x69U);
		auto       unavailablePriorSink      = (*primary)->create_pack_sink(
			unavailablePriorOperation,
			sha1_pack.size(),
			{ });
		require(
			unavailablePriorSink.has_value(),
			"create unavailable-prior pack sink");
		append_pack(**unavailablePriorSink, sha1_pack);
		const auto unavailablePriorReceipt = (*unavailablePriorSink)->finish({ });
		require(
			unavailablePriorReceipt.has_value(),
			"finish unavailable-prior pack receipt");
		unavailablePriorSink->reset();
		const auto unavailablePriorObservation = (*primary)->observe_preimport(*unavailablePriorReceipt, { });
		const auto unavailablePriorOid         = core::parse_git_commit_oid(
			std::string(40U, 'f'));
		require(
			unavailablePriorObservation.has_value()
				&& unavailablePriorOid.has_value(),
			"observe unavailable-prior pre-import facts");
		const auto unavailablePriorPrepared = (*primary)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *unavailablePriorReceipt,
				.Inventory     = *unavailablePriorObservation,
				.Authorization = {
					.CatalogIncarnation      = incarnation,
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
					.Operation               = unavailablePriorOperation,
					.ExpectedPack            = unavailablePriorReceipt->ExpectedPack,
					.ReceiptToken            = unavailablePriorReceipt->OpaqueToken,
					.InventoryToken          = unavailablePriorObservation->OpaqueToken,
					.DurableRecordToken      = token(0x69U),
				},
				.Base      = *commit,
				.Head      = *commit,
				.PriorHead = unavailablePriorOid,
			},
			{ });
		require(
			unavailablePriorPrepared.has_value()
				&& unavailablePriorPrepared->HeadLineage
					== core::head_lineage::unknown,
			"missing exact prior head yields unknown lineage without failing import");
		require(
			(*primary)
				->abort_operation(unavailablePriorOperation, { })
				.has_value(),
			"unavailable-prior prepare exact-cleans staging refs");

		const auto cancelledToken = cancelledSource.get_token();
		const auto cacheCancelled = [](const auto& aResult) {
			return !aResult.has_value()
				&& aResult.error()
				== application::git_cache_failure::cancelled;
		};
		require(
			cacheCancelled(
				(*cache)->open(primaryRequest, cancelledToken))
				&& cacheCancelled(
					(*primary)->create_pack_sink(
						identity<core::epoch_operation_id>(0x78U),
						sha1_pack.size(),
						cancelledToken))
				&& cacheCancelled(
					(*primary)->observe_preimport(
						*receipt,
						cancelledToken))
				&& cacheCancelled(
					(*primary)->prepare_revision(
						application::git_prepare_revision_request{
							.Receipt       = *receipt,
							.Inventory     = *observation,
							.Authorization = authorization,
							.Base          = *commit,
							.Head          = *commit,
						},
						cancelledToken))
				&& cacheCancelled(
					(*primary)->promote_retained(
						*prepared,
						promotion,
						cancelledToken))
				&& cacheCancelled(
					(*primary)->inspect_retained(
						core::epoch_ordinal{ 1 },
						cancelledToken))
				&& cacheCancelled(
					(*primary)->prune_retained(
						application::git_ref_prune_authorization{
							.CatalogIncarnation      = incarnation,
							.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
							.Expected                = *retained,
							.DurableIntentToken      = token(0x7cU),
						},
						cancelledToken))
				&& cacheCancelled(
					(*primary)->abort_operation(
						identity<core::epoch_operation_id>(0x7dU),
						cancelledToken))
				&& cacheCancelled(
					(*cache)->delete_cache(
						primaryRequest,
						application::git_cache_delete_authorization{
							.ApplicationSession = primaryRequest.ApplicationSession,
							.Cache              = primaryRequest.Authority.Cache,
							.DurableIntentToken = token(0x7eU),
						},
						cancelledToken)),
			"pre-cancelled cache operations all stop before mutation");

		force_push_and_prune(executable, root.Path, forcePush);
		const auto cacheGitDir = "--git-dir="
			+ QString::fromStdString(primaryRepository.string());
		const auto retainedPresence = run_fixture_git(
			executable,
			root.Path,
			{
				cacheGitDir,
				"cat-file",
				"-e",
				QString::fromStdString(
					std::string{ sha1_commit_hex } + "^{commit}"),
			},
			{ },
			true);
		require(
			retainedPresence.ExitCode == 0,
			"retained old commit remains directly readable after remote prune");

		const auto oversizedOperation = identity<core::epoch_operation_id>(0x53U);
		auto       boundedSink        = (*primary)->create_pack_sink(
			oversizedOperation,
			forcePush.NewPack.size() - 1U,
			{ });
		require(boundedSink.has_value(), "create operation-bounded pack sink");
		const auto refusedBytes = (*boundedSink)->append(std::as_bytes(std::span{ forcePush.NewPack }), { });
		require(
			!refusedBytes.has_value()
				&& refusedBytes.error()
					== application::git_pack_stream_failure::input_limit,
			"operation pack-byte quota fails before disk admission");
		boundedSink->reset();
		const auto oldAfterPackQuota = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		require(
			oldAfterPackQuota.has_value()
				&& *oldAfterPackQuota == *retained,
			"pack-byte quota failure preserves retained history");

		const auto forceOperation = identity<core::epoch_operation_id>(0x54U);
		auto       forceSink      = (*primary)->create_pack_sink(
			forceOperation,
			forcePush.NewPack.size(),
			{ });
		require(forceSink.has_value(), "create force-push pack sink");
		append_pack(**forceSink, forcePush.NewPack);
		const auto forceReceipt = (*forceSink)->finish({ });
		require(
			forceReceipt.has_value()
				&& application::agrees(*forceReceipt),
			"validate force-push pack receipt");
		forceSink->reset();
		const auto forceObservation = (*primary)->observe_preimport(*forceReceipt, { });
		require(
			forceObservation.has_value()
				&& forceObservation->Packs.size() == 1U
				&& !forceObservation->ExpectedPackPreexisting,
			"second import observes the already retained old pack");
		const auto forceAuthorization = application::git_import_authorization{
			.CatalogIncarnation      = incarnation,
			.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
			.Operation               = forceOperation,
			.ExpectedPack            = forceReceipt->ExpectedPack,
			.ReceiptToken            = forceReceipt->OpaqueToken,
			.InventoryToken          = forceObservation->OpaqueToken,
			.DurableRecordToken      = token(0x6bU),
		};
		const auto forcePrepared = (*primary)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *forceReceipt,
				.Inventory     = *forceObservation,
				.Authorization = forceAuthorization,
				.Base          = forcePush.NewCommit,
				.Head          = forcePush.NewCommit,
				.PriorHead     = commit,
			},
			{ });
		require(
			forcePrepared.has_value()
				&& forcePrepared->BaseTree == forcePush.NewTree
				&& forcePrepared->HeadTree == forcePush.NewTree
				&& forcePrepared->HeadLineage
					== core::head_lineage::non_fast_forward,
			"import force-pushed commit and classify its prior head as non-fast-forward");
		const auto forceRetained = (*primary)->promote_retained(
			*forcePrepared,
			application::git_ref_promotion_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
				.Operation               = forceOperation,
				.Scope                   = primaryRequest.Authority.Scope,
				.Ordinal                 = core::epoch_ordinal{ 2 },
				.Base                    = forcePush.NewCommit,
				.Head                    = forcePush.NewCommit,
				.DurableCommitToken      = token(0x6cU),
			},
			{ });
		require(
			forceRetained.has_value(),
			"retain force-pushed revision as a second exact epoch");
		const auto oldAfterForcePush = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		const auto newAfterForcePush = (*primary)->inspect_retained(core::epoch_ordinal{ 2 }, { });
		require(
			oldAfterForcePush.has_value()
				&& *oldAfterForcePush == *retained
				&& newAfterForcePush.has_value()
				&& *newAfterForcePush == *forceRetained,
			"old head remains resident after fixture remote removal and prune");

		const auto oldDerived       = (*primary)->derive_comparison_base({ .Head = *commit, .SelectedBase = *commit }, { });
		const auto rewrittenDerived = (*primary)->derive_comparison_base({ .Head = *commit, .SelectedBase = forcePush.NewCommit }, { });
		require(oldDerived && oldDerived->Base == *commit
					&& rewrittenDerived && rewrittenDerived->Status == application::git_comparison_base_status::no_common_ancestor && !rewrittenDerived->Base
					&& (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { }) == oldAfterForcePush,
				"force-pushed disconnected base is unavailable without changing the old retained epoch");

		const auto abandonedOperation = identity<core::epoch_operation_id>(0x55U);
		auto       abandonedSink      = (*primary)->create_pack_sink(
			abandonedOperation,
			sha1_pack.size(),
			{ });
		require(abandonedSink.has_value(), "create abortable operation spool");
		append_pack(**abandonedSink, sha1_pack);
		const auto abandonedReceipt = (*abandonedSink)->finish({ });
		require(
			abandonedReceipt.has_value(),
			"finish abortable operation spool");
		abandonedSink->reset();
		const auto preexistingObservation = (*primary)->observe_preimport(*abandonedReceipt, { });
		require(
			preexistingObservation.has_value()
				&& preexistingObservation->Packs.size() == 2U
				&& preexistingObservation->ExpectedPackPreexisting,
			"inventory identifies an exact already-installed pack");
		auto disagreeingPreexisting                    = *preexistingObservation;
		disagreeingPreexisting.ExpectedPackPreexisting = false;
		auto duplicateInventory                        = *preexistingObservation;
		duplicateInventory.Packs.insert(
			duplicateInventory.Packs.begin() + 1,
			duplicateInventory.Packs.front());
		auto unsortedInventory = *preexistingObservation;
		std::ranges::reverse(unsortedInventory.Packs);
		require(
			!application::valid(disagreeingPreexisting)
				&& !application::valid(duplicateInventory)
				&& !application::valid(unsortedInventory),
			"inventory validity requires exact preexisting agreement and sorted uniqueness");
		const auto abandonedPath = root.Path / "incoming" / "v1"
			/ (lower_hex(abandonedOperation.bytes()) + ".pack.part");
		require(
			std::filesystem::is_regular_file(abandonedPath),
			"completed abandoned operation remains exact and recoverable");
		const auto abandonedRecovery = application::git_operation_recovery_request{
			.Operation = abandonedOperation,
			.Scope     = primaryRequest.Authority.Scope,
			.Ordinal   = core::epoch_ordinal{ 4 },
			.Base      = *commit,
			.Head      = *commit,
			.Spool     = application::git_recovery_spool_expectation{
				.ExpectedPack             = abandonedReceipt->ExpectedPack,
				.CompletedSpool           = abandonedReceipt->Spool,
				.PreimportPacks           = preexistingObservation->Packs,
				.PreimportRepositoryBytes = preexistingObservation->RepositoryBytes,
				.ExpectedPackPreexisting  = true,
			},
		};
		const auto abandonedObservation = (*primary)->inspect_operation_recovery(
			abandonedRecovery,
			{ });
		require(
			application::valid(abandonedRecovery)
				&& abandonedObservation.has_value()
				&& abandonedObservation->Spool
					== application::git_recovery_match::exact
				&& abandonedObservation->Installed
					== application::git_recovery_match::exact
				&& abandonedObservation->Refs
					== application::git_recovery_ref_topology::absent
				&& abandonedObservation->Objects
					== application::git_recovery_object_match::exact,
			"durable preexisting-pack facts identify exact abortable spool residue");
		const auto abandonedOriginal = abandonedPath.parent_path() / "abandoned-original.pack";
		std::filesystem::rename(abandonedPath, abandonedOriginal);
		write_file(abandonedPath, "replacement-canary");
		const auto substitutedAbort = (*primary)->abort_recovered_operation(
			abandonedRecovery,
			{ });
		require(
			!substitutedAbort.has_value()
				&& substitutedAbort.error()
					== application::git_cache_failure::repair_required
				&& read_file(abandonedPath)
					== std::vector<std::uint8_t>{
						'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
						'e', 'n', 't', '-', 'c', 'a', 'n', 'a',
						'r', 'y' },
			"abort refuses and preserves a substituted completed spool");
		std::filesystem::remove(abandonedPath);
		std::filesystem::rename(abandonedOriginal, abandonedPath);
		primary->reset();
		primary = (*cache)->open(primaryRequest, { });
		require(primary.has_value(), "reopen for exact durable spool abort");
		// Model an installation whose identity transaction rolled back. Its
		// durable preimport inventory excludes the expected pack, while the real
		// cache contains that pack plus every prior entry. Abort may retain it,
		// but must not promote inventory agreement into file deletion authority.
		auto unrecordedInstall                           = abandonedRecovery;
		unrecordedInstall.Spool->ExpectedPackPreexisting = false;
		std::erase_if(unrecordedInstall.Spool->PreimportPacks, [&](const auto& aPack) {
			return aPack.Pack == unrecordedInstall.Spool->ExpectedPack;
		});
		const auto unrecordedObservation = (*primary)->inspect_operation_recovery(unrecordedInstall, { });
		require(application::valid(unrecordedInstall) && unrecordedObservation
					&& unrecordedObservation->Installed == application::git_recovery_match::inventory_only
					&& unrecordedObservation->Objects == application::git_recovery_object_match::exact,
				"unjournaled installed pack has inventory evidence without exact ownership");
		auto unrelatedInventory = unrecordedInstall;
		unrelatedInventory.Spool->PreimportPacks.clear();
		require(!(*primary)->abort_recovered_operation(unrelatedInventory, { })
					&& std::filesystem::exists(abandonedPath),
				"an unrelated installed pack still prevents inventory-only abort");
		const auto retainedIndexBytes = read_file(installedIndexPath);
		require((*primary)->abort_recovered_operation(unrecordedInstall, { })
					&& (*primary)->abort_recovered_operation(unrecordedInstall, { })
					&& read_file(installedIndexPath) == retainedIndexBytes,
				"inventory-only abort exact-cleans the spool and preserves installed files on replay");
		const auto aborted = (*primary)->abort_recovered_operation(
			abandonedRecovery,
			{ });
		require(
			aborted.has_value()
				&& !std::filesystem::exists(abandonedPath),
			"abort exact-deletes only its operation residue");
		const auto oldAfterAbort = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		const auto newAfterAbort = (*primary)->inspect_retained(core::epoch_ordinal{ 2 }, { });
		require(
			oldAfterAbort.has_value() && newAfterAbort.has_value(),
			"operation abort preserves both retained epochs");

		const auto abortFirstOperation = identity<core::epoch_operation_id>(0x44U);
		auto       abortFirstSink      = (*primary)->create_pack_sink(
			abortFirstOperation,
			sha1_pack.size(),
			{ });
		require(
			abortFirstSink.has_value(),
			"create deterministic abort-first sink");
		append_pack(**abortFirstSink, sha1_pack);
		require(
			(*primary)
				->abort_operation(abortFirstOperation, { })
				.has_value(),
			"abort-first operation reaches a terminal lifecycle");
		const auto finishAfterAbort = (*abortFirstSink)->finish({ });
		const auto abortFirstText   = lower_hex(abortFirstOperation.bytes());
		require(
			!finishAfterAbort.has_value()
				&& finishAfterAbort.error()
					== application::git_pack_stream_failure::
						invalid_state
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ (abortFirstText + ".pack.part"))
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ ("." + abortFirstText
					   + ".pack.receiving")),
			"finish cannot publish after a linearized abort");
		abortFirstSink->reset();

		// The one path the receiving spool's `FILE_SHARE_DELETE` exists for:
		// the abort renames and unlinks the spool while its sink still holds
		// the write handle open. This narrows that to the smallest
		// deterministic shape — a partial stream, so nothing but the open
		// handle can be holding the name — and requires the spool directory,
		// quarantine residue included, to come back exactly as it was
		// (BPR-0053).
		const auto openSpoolOperation = identity<core::epoch_operation_id>(0x4bU);
		const auto spoolDirectory     = root.Path / "incoming" / "v1";
		const auto spoolBefore        = sorted_directory_names(spoolDirectory);
		auto       openSpoolSink      = (*primary)->create_pack_sink(
			openSpoolOperation,
			sha1_pack.size(),
			{ });
		require(
			openSpoolSink.has_value(),
			"create a sink that keeps its receiving spool open");
		const auto openSpoolPartial = std::as_bytes(std::span{ sha1_pack }).first(17U);
		require(
			(*openSpoolSink)->append(openSpoolPartial, { }).has_value(),
			"the open sink receives a partial pack");
		const auto openSpoolText = lower_hex(openSpoolOperation.bytes());
		const auto openSpoolPath = spoolDirectory
			/ ("." + openSpoolText + ".pack.receiving");
		require(
			std::filesystem::is_regular_file(openSpoolPath),
			"the receiving spool is present while its sink holds it open");
		require(
			(*primary)
				->abort_operation(openSpoolOperation, { })
				.has_value(),
			"abort removes a receiving spool its sink still holds open");
		require(
			!std::filesystem::exists(openSpoolPath)
				&& sorted_directory_names(spoolDirectory) == spoolBefore,
			"the aborted spool leaves neither its own name nor cleanup residue");
		const auto appendAfterOpenAbort = (*openSpoolSink)
											  ->append(openSpoolPartial, { });
		require(
			!appendAfterOpenAbort.has_value()
				&& appendAfterOpenAbort.error()
					== application::git_pack_stream_failure::invalid_state
				&& !std::filesystem::exists(openSpoolPath),
			"the still-open sink cannot write or resurrect the deleted spool");
		openSpoolSink->reset();
		require(
			sorted_directory_names(spoolDirectory) == spoolBefore,
			"closing the sink after the abort changes nothing on disk");

		const auto racingOperation = identity<core::epoch_operation_id>(0x47U);
		auto       racingSink      = (*primary)->create_pack_sink(
			racingOperation,
			sha1_pack.size(),
			{ });
		require(racingSink.has_value(), "create finish-versus-abort sink");
		append_pack(**racingSink, sha1_pack);
		auto raceGate     = std::barrier{ 3 };
		auto racingFinish = std::optional<
			application::git_pack_stream_result<
				application::completed_git_pack_receipt>>{ };
		auto racingAbort = std::optional<
			application::git_cache_result<void>>{ };
		{
			auto finishThread = std::jthread{
				[&] {
					raceGate.arrive_and_wait();
					racingFinish = (*racingSink)->finish({ });
				}
			};
			auto abortThread = std::jthread{
				[&] {
					raceGate.arrive_and_wait();
					racingAbort = (*primary)->abort_operation(
						racingOperation,
						{ });
				}
			};
			raceGate.arrive_and_wait();
		}
		racingSink->reset();
		const auto racingText = lower_hex(racingOperation.bytes());
		require(
			racingFinish.has_value() && racingAbort.has_value()
				&& racingAbort->has_value()
				&& (racingFinish->has_value()
					|| racingFinish->error()
						== application::git_pack_stream_failure::
							storage
					|| racingFinish->error()
						== application::git_pack_stream_failure::
							invalid_state)
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ (racingText + ".pack.part"))
				&& !std::filesystem::exists(
					root.Path / "incoming" / "v1"
					/ ("." + racingText + ".pack.receiving")),
			"operation abort linearizes with finish and cannot resurrect a spool");

		primary->reset();
		auto reuseLimited                      = primaryRequest;
		reuseLimited.Limits.OperationPackBytes = sha1_pack.size();
		reuseLimited.Limits.RepositoryBytes    = preexistingObservation->RepositoryBytes + 82U;
		reuseLimited.Limits.PackCount          = 2U;
		reuseLimited.Limits.PrivateRefCount    = 6U;
		require(
			application::valid(reuseLimited),
			"valid tight preexisting-pack reuse quota");
		primary = (*cache)->open(reuseLimited, { });
		require(primary.has_value(), "open tight preexisting-pack cache");
		const auto reuseOperation = identity<core::epoch_operation_id>(0x56U);
		auto       reuseSink      = (*primary)->create_pack_sink(
			reuseOperation,
			sha1_pack.size(),
			{ });
		require(reuseSink.has_value(), "create preexisting-pack reuse sink");
		append_pack(**reuseSink, sha1_pack);
		const auto reuseReceipt = (*reuseSink)->finish({ });
		require(reuseReceipt.has_value(), "finish preexisting-pack reuse spool");
		reuseSink->reset();
		const auto reuseObservation = (*primary)->observe_preimport(*reuseReceipt, { });
		require(
			reuseObservation.has_value()
				&& reuseObservation->ExpectedPackPreexisting,
			"full pack quota still admits exact preexisting pack plus ref headroom");
		const auto reusePrepared = (*primary)->prepare_revision(
			application::git_prepare_revision_request{
				.Receipt       = *reuseReceipt,
				.Inventory     = *reuseObservation,
				.Authorization = {
					.CatalogIncarnation      = incarnation,
					.ExpectedCatalogRevision = core::epoch_catalog_revision{ 2 },
					.Operation               = reuseOperation,
					.ExpectedPack            = reuseReceipt->ExpectedPack,
					.ReceiptToken            = reuseReceipt->OpaqueToken,
					.InventoryToken          = reuseObservation->OpaqueToken,
					.DurableRecordToken      = token(0x6eU),
				},
				.Base = *commit,
				.Head = *commit,
			},
			{ });
		require(
			reusePrepared.has_value(),
			"preexisting pack revalidation prepares without pack/index growth");
		const auto reuseRetained = (*primary)->promote_retained(
			*reusePrepared,
			application::git_ref_promotion_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 2 },
				.Operation               = reuseOperation,
				.Scope                   = primaryRequest.Authority.Scope,
				.Ordinal                 = core::epoch_ordinal{ 3 },
				.Base                    = *commit,
				.Head                    = *commit,
				.DurableCommitToken      = token(0x6fU),
			},
			{ });
		require(
			reuseRetained.has_value()
				&& (*primary)
					   ->inspect_retained(core::epoch_ordinal{ 3 }, { })
					   .has_value(),
			"preexisting pack reuse retains a third exact pair at the quota edge");

		primary->reset();
		auto repositoryLimited                      = primaryRequest;
		repositoryLimited.Limits.OperationPackBytes = std::max<std::uint64_t>(
			sha1_pack.size(),
			forcePush.NewPack.size());
		repositoryLimited.Limits.RepositoryBytes = repositoryLimited.Limits.OperationPackBytes;
		require(
			application::valid(repositoryLimited),
			"valid tight repository quota request");
		auto repositoryQuota = (*cache)->open(repositoryLimited, { });
		require(
			repositoryQuota.has_value()
				&& (*repositoryQuota)
					   ->inspect_retained(core::epoch_ordinal{ 1 }, { })
					   .has_value(),
			"oversized cache remains open for inspection and reduction");
		const auto repositoryAddition = (*repositoryQuota)->create_pack_sink(identity<core::epoch_operation_id>(0x79U), sha1_pack.size(), { });
		require(
			!repositoryAddition.has_value()
				&& repositoryAddition.error()
					== application::git_cache_failure::quota_exceeded,
			"hard repository-byte quota rejects additive work");
		repositoryQuota->reset();

		auto packCountLimited             = primaryRequest;
		packCountLimited.Limits.PackCount = 1U;
		auto packCountQuota               = (*cache)->open(packCountLimited, { });
		require(
			packCountQuota.has_value()
				&& (*packCountQuota)
					   ->inspect_retained(core::epoch_ordinal{ 1 }, { })
					   .has_value(),
			"over-pack-count cache remains open for explicit recovery");
		const auto packCountAddition = (*packCountQuota)->create_pack_sink(identity<core::epoch_operation_id>(0x7aU), sha1_pack.size(), { });
		require(
			!packCountAddition.has_value()
				&& packCountAddition.error()
					== application::git_cache_failure::quota_exceeded,
			"hard pack-count quota rejects additive work");
		packCountQuota->reset();

		auto refLimited                   = primaryRequest;
		refLimited.Limits.PrivateRefCount = 2U;
		auto refQuota                     = (*cache)->open(refLimited, { });
		require(
			refQuota.has_value()
				&& (*refQuota)
					   ->inspect_retained(core::epoch_ordinal{ 1 }, { })
					   .has_value(),
			"over-ref-count cache remains open for explicit pruning");
		const auto refAddition = (*refQuota)->create_pack_sink(identity<core::epoch_operation_id>(0x7bU), sha1_pack.size(), { });
		require(
			!refAddition.has_value()
				&& refAddition.error()
					== application::git_cache_failure::quota_exceeded,
			"hard private-ref quota rejects additive work");
		refQuota->reset();

		primary = (*cache)->open(primaryRequest, { });
		require(primary.has_value(), "reopen after quota failures");
		const auto oldAfterQuotas = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		const auto newAfterQuotas = (*primary)->inspect_retained(core::epoch_ordinal{ 2 }, { });
		require(
			oldAfterQuotas.has_value()
				&& *oldAfterQuotas == *retained
				&& newAfterQuotas.has_value()
				&& *newAfterQuotas == *forceRetained,
			"quota failures never prune or remap retained history");

		const auto packedRefs = primaryRepository / "packed-refs";
		write_file(packedRefs, "");
		const auto driftedPrune = (*primary)->prune_retained(
			application::git_ref_prune_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 2 },
				.Expected                = *retained,
				.DurableIntentToken      = token(0x70U),
			},
			{ });
		require(
			!driftedPrune.has_value()
				&& driftedPrune.error()
					== application::git_cache_failure::
						invalid_repository,
			"post-open packed-ref drift blocks prune before Git execution");
		std::filesystem::remove(packedRefs);

		auto wrongPair  = *retained;
		auto wrongBytes = wrongPair.Head.bytes();
		wrongBytes[0] ^= 0x01U;
		wrongPair.Head = core::git_commit_oid{
			core::git_hash_algorithm::sha1,
			wrongBytes
		};
		const auto rejectedPrune = (*primary)->prune_retained(
			application::git_ref_prune_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
				.Expected                = wrongPair,
				.DurableIntentToken      = token(0x65U),
			},
			{ });
		require(
			!rejectedPrune.has_value()
				&& rejectedPrune.error()
					== application::git_cache_failure::ref_conflict,
			"prune refuses an unexpected retained object");

		const auto pruned = (*primary)->prune_retained(
			application::git_ref_prune_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
				.Expected                = *retained,
				.DurableIntentToken      = token(0x66U),
			},
			{ });
		require(pruned.has_value(), "exact paired prune succeeds");
		const auto absent = (*primary)->inspect_retained(core::epoch_ordinal{ 1 }, { });
		require(
			!absent.has_value()
				&& absent.error()
					== application::git_cache_failure::ref_conflict,
			"pruned retained pair is absent without remapping");
		const auto replayedPrune = (*primary)->prune_retained(
			application::git_ref_prune_authorization{
				.CatalogIncarnation      = incarnation,
				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 1 },
				.Expected                = *retained,
				.DurableIntentToken      = token(0x66U),
			},
			{ });
		require(
			replayedPrune.has_value(),
			"exact paired prune is idempotent after both refs are absent");

		const auto lockedDelete = (*cache)->delete_cache(
			primaryRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = primaryRequest.ApplicationSession,
				.Cache              = primaryRequest.Authority.Cache,
				.DurableIntentToken = token(0x67U),
			},
			{ });
		require(
			!lockedDelete.has_value()
				&& lockedDelete.error()
					== application::git_cache_failure::lock_unavailable,
			"whole-cache deletion obeys the exact writer lock");

		primary->reset();
		auto reused = (*cache)->open(primaryRequest, { });
		require(reused.has_value(), "exact manifest cache is reusable");
		reused->reset();

		auto mismatchedRequest                      = primaryRequest;
		mismatchedRequest.Authority.ProfileRevision = core::github_profile_revision{ 99U };
		const auto mismatched                       = (*cache)->open(mismatchedRequest, { });
		require(
			!mismatched.has_value()
				&& mismatched.error()
					== application::git_cache_failure::invalid_manifest,
			"same opaque cache scope rejects profile-revision drift");

		auto accountMismatch              = primaryRequest;
		accountMismatch.Authority.Account = core::github_account_id{ 100U };
		const auto mismatchedAccount      = (*cache)->open(accountMismatch, { });
		require(
			!mismatchedAccount.has_value()
				&& mismatchedAccount.error()
					== application::git_cache_failure::invalid_manifest,
			"same opaque cache scope rejects account drift");

		const auto expectTopologyRejection =
			[&](const std::string_view aMessage) {
				const auto rejected = (*cache)->open(primaryRequest, { });
				require(
					!rejected.has_value()
						&& rejected.error()
							== application::git_cache_failure::
								invalid_manifest,
					aMessage);
			};

		const auto                 config          = primaryRepository / "config";
		const auto                 originalConfig  = read_file(config);
		auto                       remoteConfig    = originalConfig;
		constexpr std::string_view forbiddenRemote = "[remote \"origin\"]\n"
													 "\turl = https://credential-canary.invalid/repository.git\n";
		remoteConfig.insert(
			remoteConfig.end(),
			forbiddenRemote.begin(),
			forbiddenRemote.end());
		write_file(config, remoteConfig);
		expectTopologyRejection(
			"cache rejects configuration and remote drift");
		write_file(config, originalConfig);

		const auto alternates = primaryRepository / "objects" / "info" / "alternates";
		write_file(alternates, "../escape\n");
		expectTopologyRejection("cache rejects alternate-object drift");
		std::filesystem::remove(alternates);

		const auto shallow = primaryRepository / "shallow";
		write_file(shallow, std::string{ sha1_commit_hex } + "\n");
		expectTopologyRejection("cache rejects shallow-state drift");
		std::filesystem::remove(shallow);

		const auto info = primaryRepository / "info";
		std::filesystem::create_directory(info);
		std::filesystem::permissions(
			info,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto grafts = info / "grafts";
		write_file(grafts, std::string{ sha1_commit_hex } + "\n");
		expectTopologyRejection("cache rejects graft drift");
		std::filesystem::remove(grafts);
		std::filesystem::remove(info);

		const auto worktrees = primaryRepository / "worktrees";
		std::filesystem::create_directory(worktrees);
		std::filesystem::permissions(
			worktrees,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		expectTopologyRejection("cache rejects worktree drift");
		std::filesystem::remove(worktrees);

#if defined(Q_OS_WIN)
		// A POSIX group-read bit has no Windows expression:
		// std::filesystem::permissions toggles the read-only attribute and
		// nothing else, so the drift this case plants does not exist here.
		// What keeps another principal out on Windows is the storage root's
		// protected owner-only DACL (BPR-0436), which BPR-0053's target-native
		// adversaries exercise directly.
		std::cout << "SKIP: private-permission drift is a POSIX mode "
					 "property; Windows privacy is the root's protected "
					 "DACL\n";
#else
		std::filesystem::permissions(
			manifest,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write
				| std::filesystem::perms::group_read,
			std::filesystem::perm_options::replace);
		expectTopologyRejection(
			"cache rejects private-permission drift");
		std::filesystem::permissions(
			manifest,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
#endif

		const auto manifestBackup = primaryRepository / "manifest-backup";
		std::filesystem::rename(manifest, manifestBackup);
		std::filesystem::create_hard_link(manifestBackup, manifest);
		expectTopologyRejection(
			"cache rejects hardlinked manifest drift");
		std::filesystem::remove(manifest);
		std::filesystem::rename(manifestBackup, manifest);

		std::filesystem::rename(manifest, manifestBackup);
		std::error_code symlinkError;
		std::filesystem::create_symlink(manifestBackup, manifest, symlinkError);
		if (symlinkError) {
			// The same boundary BPR-0425 records for the storage contract: an
			// unelevated host without Developer Mode cannot create the file
			// symlink this adversary needs, and the junction cases above
			// already cover reparse-point substitution.
			std::cout << "SKIP: manifest-symlink rejection requires Windows "
						 "symlink privilege; junction rejection remains "
						 "covered\n";
			std::filesystem::rename(manifestBackup, manifest);
		} else {
			const auto followed = (*cache)->open(primaryRequest, { });
			require(
				!followed.has_value()
					&& followed.error()
						== application::git_cache_failure::invalid_manifest,
				"manifest symlink is never followed");
			std::filesystem::remove(manifest);
			std::filesystem::rename(manifestBackup, manifest);
		}

		auto sha256Request         = request(0x35U, 0x45U);
		sha256Request.ObjectFormat = core::git_hash_algorithm::sha256;
		require(
			application::valid(sha256Request),
			"valid SHA-256 cache authority");
		auto sha256Cache = (*cache)->open(sha256Request, { });
		require(sha256Cache.has_value(), "create SHA-256 bare cache");
		const auto sha256Operation = identity<core::epoch_operation_id>(0x57U);
		auto       sha256Sink      = (*sha256Cache)->create_pack_sink(sha256Operation, sha256Fixture.NewPack.size(), { });
		require(sha256Sink.has_value(), "create SHA-256 pack sink");
		append_pack(**sha256Sink, sha256Fixture.NewPack);
		const auto sha256Receipt = (*sha256Sink)->finish({ });
		require(
			sha256Receipt.has_value()
				&& sha256Receipt->ExpectedPack.Algorithm
					== core::git_hash_algorithm::sha256,
			"validate SHA-256 pack checksum and opaque receipt");
		sha256Sink->reset();
		const auto sha256Observation = (*sha256Cache)->observe_preimport(*sha256Receipt, { });
		require(
			sha256Observation.has_value()
				&& !sha256Observation->ExpectedPackPreexisting,
			"observe empty SHA-256 pack inventory");
		const auto sha256Incarnation = identity<core::epoch_catalog_incarnation_id>(0x72U);
		const auto sha256Prepared    = (*sha256Cache)->prepare_revision(application::git_prepare_revision_request{
																			.Receipt       = *sha256Receipt,
																			.Inventory     = *sha256Observation,
																			.Authorization = {
																				.CatalogIncarnation      = sha256Incarnation,
																				.ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
																				.Operation               = sha256Operation,
																				.ExpectedPack            = sha256Receipt->ExpectedPack,
																				.ReceiptToken            = sha256Receipt->OpaqueToken,
																				.InventoryToken          = sha256Observation->OpaqueToken,
																				.DurableRecordToken      = token(0x73U),
																			},
																			.Base = sha256Fixture.NewCommit,
																			.Head = sha256Fixture.NewCommit,
																		},
																		{ });
		require(
			sha256Prepared.has_value()
				&& sha256Prepared->BaseTree
					== sha256Fixture.NewTree
				&& sha256Prepared->HeadTree
					== sha256Fixture.NewTree,
			"SHA-256 import verifies full commit connectivity and trees");
		const auto sha256Retained = (*sha256Cache)->promote_retained(*sha256Prepared, application::git_ref_promotion_authorization{
																						  .CatalogIncarnation      = sha256Incarnation,
																						  .ExpectedCatalogRevision = core::epoch_catalog_revision{ 0 },
																						  .Operation               = sha256Operation,
																						  .Scope                   = sha256Request.Authority.Scope,
																						  .Ordinal                 = core::epoch_ordinal{ 1 },
																						  .Base                    = sha256Fixture.NewCommit,
																						  .Head                    = sha256Fixture.NewCommit,
																						  .DurableCommitToken      = token(0x74U),
																					  },
																	 { });
		require(
			sha256Retained.has_value()
				&& (*sha256Cache)
					   ->inspect_retained(core::epoch_ordinal{ 1 }, { })
					   .has_value(),
			"SHA-256 retained refs preserve exact 64-digit identities");
		sha256Cache->reset();
		require(
			(*cache)
				->delete_cache(
					sha256Request,
					application::git_cache_delete_authorization{
						.ApplicationSession = sha256Request.ApplicationSession,
						.Cache              = sha256Request.Authority.Cache,
						.DurableIntentToken = token(0x75U),
					},
					{ })
				.has_value(),
			"SHA-256 cache exact deletion succeeds");

		const auto wrongDelete = (*cache)->delete_cache(
			primaryRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = primaryRequest.ApplicationSession,
				.Cache              = identity<core::repository_cache_scope_id>(0x77U),
				.DurableIntentToken = token(0x68U),
			},
			{ });
		require(
			!wrongDelete.has_value()
				&& wrongDelete.error()
					== application::git_cache_failure::
						authorization_mismatch,
			"whole-cache deletion requires exact scope authorization");

		const auto collisionToken = token(0x6dU);
		const auto quarantineRoot = root.Path / "quarantine" / "v1";
		std::filesystem::create_directories(quarantineRoot);
		std::filesystem::permissions(
			root.Path / "quarantine",
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		std::filesystem::permissions(
			quarantineRoot,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		const auto collision = quarantineRoot
			/ (lower_hex(primaryRequest.Authority.Cache.bytes())
			   + "-" + lower_hex(collisionToken) + ".git");
		std::error_code collisionError;
		std::filesystem::create_directory_symlink(
			isolatedRepository,
			collision,
			collisionError);
		if (collisionError) {
			// The same boundary BPR-0425 records: no symlink privilege on an
			// unelevated host without Developer Mode.
			std::cout << "SKIP: quarantine-collision rejection requires "
						 "Windows symlink privilege\n";
		} else {
			const auto collisionDelete = (*cache)->delete_cache(
				primaryRequest,
				application::git_cache_delete_authorization{
					.ApplicationSession = primaryRequest.ApplicationSession,
					.Cache              = primaryRequest.Authority.Cache,
					.DurableIntentToken = collisionToken,
				},
				{ });
			require(
				!collisionDelete.has_value()
					&& collisionDelete.error()
						== application::git_cache_failure::repair_required
					&& std::filesystem::is_directory(primaryRepository)
					&& std::filesystem::is_directory(isolatedRepository),
				"cache deletion refuses a quarantine collision without escaping");
			std::filesystem::remove(collision);
		}

		const auto primaryDeleteAuthorization = application::git_cache_delete_authorization{
			.ApplicationSession = primaryRequest.ApplicationSession,
			.Cache              = primaryRequest.Authority.Cache,
			.DurableIntentToken = token(0x69U),
		};
		const auto deleted = (*cache)->delete_cache(
			primaryRequest,
			primaryDeleteAuthorization,
			{ });
		const auto primaryTombstone = quarantineRoot
			/ (lower_hex(primaryRequest.Authority.Cache.bytes())
			   + ".deleted");
		require(
			deleted.has_value() && !std::filesystem::exists(primaryRepository)
				&& std::filesystem::is_regular_file(primaryTombstone)
				&& std::filesystem::is_directory(isolatedRepository),
			"exact cache deletion leaves its authority-bound terminal tombstone");
		require(
			(*cache)
				->delete_cache(
					primaryRequest,
					primaryDeleteAuthorization,
					{ })
				.has_value(),
			"same delete intent is idempotent after terminal cleanup");
		const auto differentDelete = (*cache)->delete_cache(
			primaryRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = primaryRequest.ApplicationSession,
				.Cache              = primaryRequest.Authority.Cache,
				.DurableIntentToken = token(0x71U),
			},
			{ });
		require(
			!differentDelete.has_value()
				&& differentDelete.error()
					== application::git_cache_failure::
						authorization_mismatch,
			"terminal tombstone rejects a different delete intent");
		const auto deletedReopen = (*cache)->open(primaryRequest, { });
		require(
			!deletedReopen.has_value()
				&& deletedReopen.error()
					== application::git_cache_failure::repair_required,
			"terminal tombstone prevents deleted cache-scope reuse");

		isolated->reset();
		const auto isolatedDeleteToken = token(0x6aU);
		const auto isolatedQuarantine  = quarantineRoot
			/ (lower_hex(isolatedRequest.Authority.Cache.bytes())
			   + "-" + lower_hex(isolatedDeleteToken) + ".git");
		const auto isolatedTombstone = quarantineRoot
			/ (lower_hex(isolatedRequest.Authority.Cache.bytes())
			   + ".deleted");
		write_file(
			isolatedTombstone,
			deletion_tombstone(
				isolatedRequest,
				isolatedDeleteToken,
				isolatedRepository,
				root.Path / "locks" / "v1"
					/ (lower_hex(
						   isolatedRequest.Authority.Cache.bytes())
					   + ".lock")));
		std::filesystem::rename(
			isolatedRepository,
			isolatedQuarantine);
		const auto displacedIsolated = quarantineRoot / "displaced-isolated-cache.git";
		std::filesystem::rename(
			isolatedQuarantine,
			displacedIsolated);
		std::filesystem::create_directory(isolatedQuarantine);
		std::filesystem::permissions(
			isolatedQuarantine,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
		write_file(
			isolatedQuarantine / "replacement-canary",
			"replacement-quarantine");
		const auto substitutedQuarantineDelete = (*cache)->delete_cache(
			isolatedRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = isolatedRequest.ApplicationSession,
				.Cache              = isolatedRequest.Authority.Cache,
				.DurableIntentToken = isolatedDeleteToken,
			},
			{ });
		require(
			!substitutedQuarantineDelete.has_value()
				&& substitutedQuarantineDelete.error()
					== application::git_cache_failure::
						repair_required
				&& read_file(
					   isolatedQuarantine
					   / "replacement-canary")
					== std::vector<std::uint8_t>{
						'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
						'e', 'n', 't', '-', 'q', 'u', 'a', 'r',
						'a', 'n', 't', 'i', 'n', 'e' },
			"tombstone retry refuses and preserves a substituted quarantine");
		std::filesystem::remove_all(isolatedQuarantine);
		std::filesystem::rename(
			displacedIsolated,
			isolatedQuarantine);
		std::filesystem::remove(isolatedQuarantine / "config");
		const auto isolatedDeleted = (*cache)->delete_cache(
			isolatedRequest,
			application::git_cache_delete_authorization{
				.ApplicationSession = isolatedRequest.ApplicationSession,
				.Cache              = isolatedRequest.Authority.Cache,
				.DurableIntentToken = isolatedDeleteToken,
			},
			{ });
		require(
			isolatedDeleted.has_value()
				&& !std::filesystem::exists(isolatedRepository)
				&& !std::filesystem::exists(isolatedQuarantine)
				&& std::filesystem::is_regular_file(
					quarantineRoot
					/ (lower_hex(
						   isolatedRequest.Authority.Cache.bytes())
					   + ".deleted")),
			"owned tombstone makes interrupted quarantine cleanup retryable");
		require(
			(*cache)
				->delete_cache(
					isolatedRequest,
					application::git_cache_delete_authorization{
						.ApplicationSession = isolatedRequest.ApplicationSession,
						.Cache              = isolatedRequest.Authority.Cache,
						.DurableIntentToken = isolatedDeleteToken,
					},
					{ })
				.has_value(),
			"completed interrupted deletion is idempotent");
	}
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
	if (argc == 2 && QString::fromUtf8(argv[1]) == "--exec-path") {
		std::cerr << raw_git_diagnostic;
		return 128;
	}
	QCoreApplication application{ argc, argv };
	if (argc == 2 && QString::fromUtf8(argv[1]) == "--pristine-remount") {
		verify_pristine_remount_recovery();
		std::cout << "Git pristine remount recovery contract passed\n";
		return 0;
	}
	if (
		argc == 8
		&& QString::fromUtf8(argv[1]) == "--hold-cache-lock") {
		bool       cacheOk   = false;
		bool       scopeOk   = false;
		const auto cacheByte = QString::fromUtf8(argv[4]).toUInt(&cacheOk);
		const auto scopeByte = QString::fromUtf8(argv[5]).toUInt(&scopeOk);
		if (
			!cacheOk || !scopeOk || cacheByte > 0xffU
			|| scopeByte > 0xffU) {
			return 19;
		}
		return run_lock_holder(
			std::filesystem::path{ argv[2] },
			std::filesystem::path{ argv[3] },
			static_cast<std::uint8_t>(cacheByte),
			static_cast<std::uint8_t>(scopeByte),
			std::filesystem::path{ argv[6] },
			std::filesystem::path{ argv[7] });
	}
	if (argc == 2 && QString::fromUtf8(argv[1]) == "--sparse") {
		const auto executable = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE);
		auto       root       = temporary_root{ };
		auto       cache      = git_adapter::make_installed_git_cache({ .GitExecutable = executable, .StorageRoot = root.Path, .WindowsSystemRoot = bpr::tests::git_contract::windows_system_root() });
		require(cache.has_value(), "sparse cache adapter opens");
		verify_sparse_residency(**cache, root.Path, executable);
		std::cout << "Git sparse cache contract passed\n";
		return 0;
	}

	verify_pristine_remount_recovery();
	run_cache_contract();
	std::cout << "Git cache contract passed\n";
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
