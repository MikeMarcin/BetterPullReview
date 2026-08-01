#include "bpr/adapters/git/installed_git_cache.hpp"

#include "bpr/application/git_cache_port.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <QCoreApplication>
#include <QByteArray>
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

namespace
{
namespace application = bpr::application;
namespace core = bpr::core;
namespace git_adapter = bpr::adapters::git;

template<typename Value>
concept exposes_path = requires(Value value) {
    value.Path;
};

template<typename Value>
concept exposes_url = requires(Value value) {
    value.Url;
};

template<typename Value>
concept exposes_credential = requires(Value value) {
    value.Credential;
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

inline constexpr std::string_view sha1_commit_hex =
    "826bb464b84352d60b3d7c56481a599a838d998d";
inline constexpr std::string_view sha1_tree_hex =
    "4ce9ffbf7ccd31debb0a343022131f0fde64e35d";
inline constexpr std::string_view sha1_pack_hex =
    "e248b08f09a75116fdb2397c928a84f6a93f6c34";

inline constexpr std::array<std::uint8_t, 230U> sha1_pack{
    0x50, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    0x97, 0x0b, 0x78, 0x9c, 0x95, 0xcb, 0xc1, 0x0a, 0x02, 0x21, 0x10, 0x80,
    0xe1, 0xbb, 0x4f, 0x31, 0xf7, 0x20, 0x74, 0x35, 0xdb, 0x85, 0x58, 0xa2,
    0x4b, 0xd7, 0xe8, 0x0d, 0x46, 0x1d, 0x69, 0xc0, 0x5d, 0xc5, 0xa6, 0xa8,
    0xb7, 0xaf, 0x1e, 0xa1, 0xff, 0xfc, 0xfd, 0xd2, 0x89, 0xc0, 0x45, 0x9a,
    0x72, 0x0e, 0x79, 0x1f, 0x63, 0xb2, 0x26, 0x51, 0x08, 0x1a, 0xad, 0xb3,
    0x7a, 0x18, 0x8c, 0x35, 0x59, 0xe7, 0x44, 0xde, 0x91, 0xdd, 0x25, 0x85,
    0x0f, 0xb9, 0xd5, 0x0e, 0xa7, 0xcb, 0x15, 0x0e, 0xa1, 0xf5, 0x23, 0xbd,
    0x70, 0x69, 0x85, 0xb6, 0xbc, 0x3e, 0xb1, 0x70, 0x9a, 0x61, 0x72, 0xde,
    0x8f, 0x6e, 0xd4, 0x1a, 0x36, 0xfa, 0x9b, 0x8a, 0x75, 0x59, 0x58, 0x84,
    0xfe, 0x79, 0xd4, 0x8f, 0xf2, 0x7a, 0x17, 0x2c, 0x85, 0x12, 0x9c, 0x59,
    0x20, 0x62, 0xc3, 0xc0, 0x85, 0xe5, 0x0d, 0xad, 0xd7, 0x40, 0xea, 0x03,
    0x67, 0xd5, 0x39, 0x11, 0xa5, 0x02, 0x78, 0x9c, 0x33, 0x34, 0x30, 0x30,
    0x33, 0x31, 0x51, 0x28, 0x28, 0xca, 0x4f, 0x4a, 0xd5, 0x2b, 0xa9, 0x28,
    0x61, 0xb8, 0xc5, 0xe3, 0xb7, 0xe3, 0xe6, 0x92, 0x5e, 0x71, 0x29, 0xe3,
    0x70, 0x6f, 0x0b, 0xf6, 0xa0, 0x87, 0xcd, 0x1a, 0xe9, 0x81, 0x00, 0xe4,
    0xb2, 0x0c, 0xc7, 0x36, 0x78, 0x9c, 0x2b, 0x28, 0xca, 0x4f, 0x4a, 0xe5,
    0x02, 0x00, 0x08, 0x96, 0x02, 0x23, 0xe2, 0x48, 0xb0, 0x8f, 0x09, 0xa7,
    0x51, 0x16, 0xfd, 0xb2, 0x39, 0x7c, 0x92, 0x8a, 0x84, 0xf6, 0xa9, 0x3f,
    0x6c, 0x34,
};

auto require(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

class temporary_root final
{
public:
    temporary_root()
    {
        const auto seed = std::to_string(
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count());
        Path = std::filesystem::temp_directory_path()
            / ("bpr-git-cache-" + seed);
        std::filesystem::create_directories(Path);
        std::filesystem::permissions(
            Path,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
        Path = std::filesystem::canonical(Path);
    }

    temporary_root(const temporary_root&) = delete;
    auto operator=(const temporary_root&) -> temporary_root& = delete;

    ~temporary_root()
    {
        std::error_code error;
        std::filesystem::remove_all(Path, error);
    }

    std::filesystem::path Path;
};

[[nodiscard]] auto read_file(
    const std::filesystem::path& path) -> std::vector<std::uint8_t>
{
    auto input = std::ifstream{path, std::ios::binary};
    require(static_cast<bool>(input), "open private test file");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

auto write_file(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> bytes) -> void
{
    auto output =
        std::ofstream{path, std::ios::binary | std::ios::trunc};
    require(static_cast<bool>(output), "open private test file for write");
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(static_cast<bool>(output), "write private test file");
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
}

auto write_file(
    const std::filesystem::path& path,
    const std::string_view text) -> void
{
    write_file(
        path,
        std::span{
            reinterpret_cast<const std::uint8_t*>(text.data()),
            text.size()});
}

auto make_tree_private(
    const std::filesystem::path& root) -> void
{
    std::filesystem::permissions(
        root,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::error_code error;
    auto iterator = std::filesystem::recursive_directory_iterator{
        root,
        std::filesystem::directory_options::none,
        error};
    const auto end =
        std::filesystem::recursive_directory_iterator{};
    while(!error && iterator != end)
    {
        const auto path = iterator->path();
        const auto status =
            std::filesystem::symlink_status(path, error);
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

template<typename Identity>
[[nodiscard]] auto identity(const std::uint8_t value) -> Identity
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes.fill(value);
    return Identity{bytes};
}

[[nodiscard]] auto token(const std::uint8_t value)
    -> std::array<std::uint8_t, 32U>
{
    auto result = std::array<std::uint8_t, 32U>{};
    result.fill(value);
    return result;
}

[[nodiscard]] constexpr auto nibble(const char value) -> std::uint8_t
{
    return value <= '9'
        ? static_cast<std::uint8_t>(value - '0')
        : static_cast<std::uint8_t>(value - 'a' + 10);
}

[[nodiscard]] auto pack_oid(
    const std::string_view text) -> application::git_pack_oid
{
    auto result = application::git_pack_oid{
        .Algorithm = core::git_hash_algorithm::sha1};
    for(std::size_t index = 0U; index < text.size() / 2U; ++index)
    {
        result.Digest[index] = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(
                nibble(text[index * 2U]) << 4U)
            | nibble(text[index * 2U + 1U]));
    }
    require(application::valid(result), "valid pack fixture identity");
    return result;
}

[[nodiscard]] auto request(
    const std::uint8_t cache_byte,
    const std::uint8_t scope_byte)
    -> application::git_cache_open_request
{
    auto result = application::git_cache_open_request{
        .ApplicationSession = core::application_session_id{1U},
        .Authority =
            {
                .Profile = core::host_profile_id{2U},
                .ProfileRevision = core::github_profile_revision{3U},
                .Host = core::git_host_id{4U},
                .HostAuthority =
                    identity<core::host_authority_fingerprint>(5U),
                .Account = core::github_account_id{6U},
                .Viewer = core::github_viewer_actor_id{7U},
                .Repository = core::repository_id{8U},
                .Cache =
                    identity<core::repository_cache_scope_id>(
                        cache_byte),
                .PullRequest = core::pull_request_id{9U},
                .Number = core::pull_request_number{10U},
                .Scope =
                    identity<
                        core::pull_request_authority_scope_id>(
                        scope_byte),
            },
        .ObjectFormat = core::git_hash_algorithm::sha1,
    };
    require(application::valid(result), "valid cache request fixture");
    return result;
}

auto append_u64(
    std::vector<std::uint8_t>& output,
    const std::uint64_t value) -> void
{
    for(unsigned int shift = 56U;; shift -= 8U)
    {
        output.push_back(
            static_cast<std::uint8_t>(value >> shift));
        if(shift == 0U)
        {
            break;
        }
    }
}

[[nodiscard]] auto deletion_tombstone(
    const application::git_cache_open_request& cache_request,
    const std::array<std::uint8_t, 32U>& intent,
    const std::filesystem::path& repository,
    const std::filesystem::path& lock)
    -> std::vector<std::uint8_t>
{
    constexpr std::array<std::uint8_t, 12U> tombstone_magic{
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
    constexpr std::array<std::uint8_t, 12U> manifest_magic{
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
    auto result = std::vector<std::uint8_t>{};
    result.insert(
        result.end(),
        tombstone_magic.begin(),
        tombstone_magic.end());
    result.insert(
        result.end(),
        manifest_magic.begin(),
        manifest_magic.end());
    result.push_back(
        cache_request.ObjectFormat == core::git_hash_algorithm::sha1
        ? 1U
        : 2U);
    append_u64(result, cache_request.Authority.Profile.value());
    append_u64(
        result,
        cache_request.Authority.ProfileRevision.value());
    append_u64(result, cache_request.Authority.Host.value());
    result.insert(
        result.end(),
        cache_request.Authority.HostAuthority.bytes().begin(),
        cache_request.Authority.HostAuthority.bytes().end());
    append_u64(result, cache_request.Authority.Account.value());
    append_u64(result, cache_request.Authority.Repository.value());
    result.insert(
        result.end(),
        cache_request.Authority.Cache.bytes().begin(),
        cache_request.Authority.Cache.bytes().end());
    result.insert(result.end(), intent.begin(), intent.end());
    const auto append_file_identity =
        [&](const std::filesystem::path& path)
    {
#if defined(Q_OS_UNIX)
        struct stat information
        {
        };
        require(
            ::lstat(path.c_str(), &information) == 0,
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
            path.c_str(),
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
        BY_HANDLE_FILE_INFORMATION information{};
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
    append_file_identity(repository);
    append_file_identity(lock);
    return result;
}

[[nodiscard]] auto lower_hex(
    const std::span<const std::uint8_t> bytes) -> std::string
{
    constexpr std::string_view digits = "0123456789abcdef";
    auto result = std::string{};
    result.reserve(bytes.size() * 2U);
    for(const auto byte : bytes)
    {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

[[nodiscard]] auto repository_path(
    const std::filesystem::path& root,
    const application::git_cache_open_request& cache_request)
    -> std::filesystem::path
{
    return root / "repositories" / "v1"
        / (lower_hex(cache_request.Authority.Cache.bytes()) + ".git");
}

struct fixture_process_result
{
    int ExitCode = 0;
    QByteArray Output;
};

[[nodiscard]] auto run_fixture_git(
    const std::filesystem::path& executable,
    const std::filesystem::path& root,
    const QStringList& arguments,
    const QByteArray& input = {},
    const bool allow_failure = false) -> fixture_process_result
{
    auto environment = QProcessEnvironment{};
    environment.insert(
        "HOME",
        QString::fromStdString((root / "home").string()));
    environment.insert(
        "XDG_CONFIG_HOME",
        QString::fromStdString((root / "xdg").string()));
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

    auto process = QProcess{};
    process.setProgram(QString::fromStdString(executable.string()));
    process.setArguments(arguments);
    process.setWorkingDirectory(QString::fromStdString(root.string()));
    process.setProcessEnvironment(environment);
    process.start();
    require(process.waitForStarted(5'000), "start fixture Git");
    if(!input.isEmpty())
    {
        require(
            process.write(input) == input.size(),
            "write complete fixture Git input");
    }
    process.closeWriteChannel();
    require(process.waitForFinished(30'000), "finish fixture Git");
    const auto output = process.readAllStandardOutput();
    const auto error = process.readAllStandardError();
    require(
        output.size() <= 1024 * 1024
            && error.size() <= 64 * 1024,
        "bound fixture Git output");
    require(
        allow_failure || process.exitCode() == 0,
        "fixture Git exits successfully");
    return {
        .ExitCode = process.exitCode(),
        .Output = output,
    };
}

[[nodiscard]] auto trimmed_hex(
    const fixture_process_result& result,
    const qsizetype expected_characters) -> std::string
{
    auto output = result.Output;
    require(
        output.endsWith('\n'),
        "fixture Git identity has newline terminator");
    output.chop(1);
    require(
        output.size() == expected_characters,
        "fixture Git identity has exact width");
    return output.toStdString();
}

struct force_push_fixture
{
    std::filesystem::path Remote;
    std::vector<std::uint8_t> NewPack;
    core::git_commit_oid NewCommit;
    core::git_tree_oid NewTree;
};

[[nodiscard]] auto make_force_push_fixture(
    const std::filesystem::path& executable,
    const std::filesystem::path& root) -> force_push_fixture
{
    std::filesystem::create_directories(root / "home");
    std::filesystem::create_directories(root / "xdg");
    std::filesystem::permissions(
        root / "home",
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(
        root / "xdg",
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto remote = root / "fixture-remote.git";
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {
            "init",
            "--bare",
            "--quiet",
            "--object-format=sha1",
            QString::fromStdString(remote.string()),
        }));
    const auto git_dir =
        "--git-dir=" + QString::fromStdString(remote.string());
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {git_dir, "index-pack", "--stdin"},
        QByteArray{
            reinterpret_cast<const char*>(sha1_pack.data()),
            static_cast<qsizetype>(sha1_pack.size())}));
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {
            git_dir,
            "update-ref",
            "refs/heads/main",
            QString::fromUtf8(sha1_commit_hex.data()),
        }));

    const auto blob = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {git_dir, "hash-object", "-w", "--stdin"},
            QByteArray{"force-pushed content\n"}),
        40);
    const auto tree_input = QByteArray{
        "100644 blob " + QByteArray::fromStdString(blob)
        + "\tfile.txt\n"};
    const auto tree_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {git_dir, "mktree"},
            tree_input),
        40);
    const auto commit_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {
                git_dir,
                "commit-tree",
                QString::fromStdString(tree_text),
            },
            QByteArray{"force push\n"}),
        40);
    const auto pack = run_fixture_git(
        executable,
        root,
        {git_dir, "pack-objects", "--stdout", "--revs"},
        QByteArray::fromStdString(commit_text + "\n"));
    require(
        pack.Output.size() > 32,
        "force-push fixture emits a complete pack");

    const auto commit = core::parse_git_commit_oid(commit_text);
    const auto tree = core::parse_git_tree_oid(tree_text);
    require(
        commit.has_value() && tree.has_value(),
        "parse force-push fixture identities");
    auto pack_bytes = std::vector<std::uint8_t>(
        static_cast<std::size_t>(pack.Output.size()));
    std::transform(
        pack.Output.begin(),
        pack.Output.end(),
        pack_bytes.begin(),
        [](const char value)
        {
            return static_cast<std::uint8_t>(
                static_cast<unsigned char>(value));
        });
    return {
        .Remote = remote,
        .NewPack = std::move(pack_bytes),
        .NewCommit = *commit,
        .NewTree = *tree,
    };
}

auto force_push_and_prune(
    const std::filesystem::path& executable,
    const std::filesystem::path& root,
    const force_push_fixture& fixture) -> void
{
    const auto git_dir =
        "--git-dir=" + QString::fromStdString(fixture.Remote.string());
    const auto new_head =
        core::canonical_hex(fixture.NewCommit).view();
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {
            git_dir,
            "update-ref",
            "refs/heads/main",
            QString::fromUtf8(
                new_head.data(),
                static_cast<qsizetype>(new_head.size())),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {git_dir, "reflog", "expire", "--expire=now", "--all"}));
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {
            git_dir,
            "-c",
            "gc.cruftPacks=false",
            "gc",
            "--prune=now",
        }));
    const auto old_presence = run_fixture_git(
        executable,
        root,
        {
            git_dir,
            "cat-file",
            "-e",
            QString::fromStdString(
                std::string{sha1_commit_hex} + "^{commit}"),
        },
        {},
        true);
    require(
        old_presence.ExitCode != 0,
        "fixture remote removes and prunes the already-retained old head");
}

[[nodiscard]] auto make_sha256_fixture(
    const std::filesystem::path& executable,
    const std::filesystem::path& root) -> force_push_fixture
{
    const auto remote = root / "sha256-fixture.git";
    static_cast<void>(run_fixture_git(
        executable,
        root,
        {
            "init",
            "--bare",
            "--quiet",
            "--object-format=sha256",
            QString::fromStdString(remote.string()),
        }));
    const auto git_dir =
        "--git-dir=" + QString::fromStdString(remote.string());
    const auto blob = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {git_dir, "hash-object", "-w", "--stdin"},
            QByteArray{"sha256 cache content\n"}),
        64);
    const auto tree_input = QByteArray{
        "100644 blob " + QByteArray::fromStdString(blob)
        + "\tsha256.txt\n"};
    const auto tree_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {git_dir, "mktree"},
            tree_input),
        64);
    const auto commit_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {
                git_dir,
                "commit-tree",
                QString::fromStdString(tree_text),
            },
            QByteArray{"sha256 cache\n"}),
        64);
    const auto pack = run_fixture_git(
        executable,
        root,
        {git_dir, "pack-objects", "--stdout", "--revs"},
        QByteArray::fromStdString(commit_text + "\n"));
    const auto commit = core::parse_git_commit_oid(commit_text);
    const auto tree = core::parse_git_tree_oid(tree_text);
    require(
        pack.Output.size() > 44 && commit.has_value()
            && tree.has_value(),
        "build complete SHA-256 pack fixture");
    auto pack_bytes = std::vector<std::uint8_t>(
        static_cast<std::size_t>(pack.Output.size()));
    std::transform(
        pack.Output.begin(),
        pack.Output.end(),
        pack_bytes.begin(),
        [](const char value)
        {
            return static_cast<std::uint8_t>(
                static_cast<unsigned char>(value));
        });
    return {
        .Remote = remote,
        .NewPack = std::move(pack_bytes),
        .NewCommit = *commit,
        .NewTree = *tree,
    };
}

[[nodiscard]] auto make_descendant_fixture(
    const std::filesystem::path& executable,
    const std::filesystem::path& root,
    const std::filesystem::path& remote) -> force_push_fixture
{
    const auto git_dir =
        "--git-dir=" + QString::fromStdString(remote.string());
    const auto tree_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {
                git_dir,
                "rev-parse",
                QString::fromStdString(
                    std::string{sha1_commit_hex} + "^{tree}"),
            }),
        40);
    const auto commit_text = trimmed_hex(
        run_fixture_git(
            executable,
            root,
            {
                git_dir,
                "commit-tree",
                QString::fromStdString(tree_text),
                "-p",
                QString::fromUtf8(sha1_commit_hex.data()),
            },
            QByteArray{"descendant upload\n"}),
        40);
    const auto pack = run_fixture_git(
        executable,
        root,
        {git_dir, "pack-objects", "--stdout", "--revs"},
        QByteArray::fromStdString(commit_text + "\n"));
    const auto commit = core::parse_git_commit_oid(commit_text);
    const auto tree = core::parse_git_tree_oid(tree_text);
    require(
        pack.Output.size() > 32 && commit.has_value()
            && tree.has_value(),
        "build complete descendant pack fixture");
    auto pack_bytes = std::vector<std::uint8_t>(
        static_cast<std::size_t>(pack.Output.size()));
    std::transform(
        pack.Output.begin(),
        pack.Output.end(),
        pack_bytes.begin(),
        [](const char value)
        {
            return static_cast<std::uint8_t>(
                static_cast<unsigned char>(value));
        });
    return {
        .Remote = remote,
        .NewPack = std::move(pack_bytes),
        .NewCommit = *commit,
        .NewTree = *tree,
    };
}

auto append_pack(
    application::git_pack_stream_sink& sink,
    const std::span<const std::uint8_t> bytes) -> void
{
    const auto all = std::as_bytes(bytes);
    const auto first_size =
        std::min<std::size_t>(17U, all.size());
    const auto middle_size = std::min<std::size_t>(
        101U,
        all.size() - first_size);
    const auto first = sink.append(all.first(first_size), {});
    require(first.has_value(), "append first pack fragment");
    const auto middle =
        sink.append(all.subspan(first_size, middle_size), {});
    require(middle.has_value(), "append middle pack fragment");
    const auto last =
        sink.append(all.subspan(first_size + middle_size), {});
    require(last.has_value(), "append final pack fragment");
}

auto verify_descendant_lineage(
    application::git_cache_port& cache,
    const force_push_fixture& descendant) -> void
{
    const auto cache_request = request(0x34U, 0x44U);
    auto opened = cache.open(cache_request, {});
    require(opened.has_value(), "open isolated lineage cache");
    auto& session = **opened;
    const auto baseline_operation =
        identity<core::epoch_operation_id>(0x6aU);
    auto baseline_sink = session.create_pack_sink(
        baseline_operation,
        sha1_pack.size(),
        {});
    require(baseline_sink.has_value(), "create lineage baseline spool");
    append_pack(**baseline_sink, sha1_pack);
    const auto baseline_receipt = (*baseline_sink)->finish({});
    require(baseline_receipt.has_value(), "finish lineage baseline spool");
    baseline_sink->reset();
    const auto baseline_inventory = session.observe_preimport(
        *baseline_receipt,
        {});
    const auto baseline_commit = core::parse_git_commit_oid(
        sha1_commit_hex);
    const auto incarnation =
        identity<core::epoch_catalog_incarnation_id>(0x6aU);
    require(
        baseline_inventory.has_value()
            && baseline_commit.has_value(),
        "observe lineage baseline inventory");
    const auto baseline_prepared = session.prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *baseline_receipt,
            .Inventory = *baseline_inventory,
            .Authorization =
                {
                    .CatalogIncarnation = incarnation,
                    .ExpectedCatalogRevision =
                        core::epoch_catalog_revision{0},
                    .Operation = baseline_operation,
                    .ExpectedPack = baseline_receipt->ExpectedPack,
                    .ReceiptToken = baseline_receipt->OpaqueToken,
                    .InventoryToken = baseline_inventory->OpaqueToken,
                    .DurableRecordToken = token(0x6aU),
                },
            .Base = *baseline_commit,
            .Head = *baseline_commit,
        },
        {});
    require(
        baseline_prepared.has_value()
            && baseline_prepared->HeadLineage
                == core::head_lineage::not_applicable,
        "prepare isolated lineage baseline");
    require(
        session
            .promote_retained(
                *baseline_prepared,
                application::git_ref_promotion_authorization{
                    .CatalogIncarnation = incarnation,
                    .ExpectedCatalogRevision =
                        core::epoch_catalog_revision{0},
                    .Operation = baseline_operation,
                    .Scope = cache_request.Authority.Scope,
                    .Ordinal = core::epoch_ordinal{1},
                    .Base = *baseline_commit,
                    .Head = *baseline_commit,
                    .DurableCommitToken = token(0x6bU),
                },
                {})
            .has_value(),
        "retain isolated lineage baseline");

    const auto descendant_operation =
        identity<core::epoch_operation_id>(0x6cU);
    auto descendant_sink = session.create_pack_sink(
        descendant_operation,
        descendant.NewPack.size(),
        {});
    require(descendant_sink.has_value(), "create descendant lineage spool");
    append_pack(**descendant_sink, descendant.NewPack);
    const auto descendant_receipt = (*descendant_sink)->finish({});
    require(descendant_receipt.has_value(), "finish descendant lineage spool");
    descendant_sink->reset();
    const auto descendant_inventory = session.observe_preimport(
        *descendant_receipt,
        {});
    require(
        descendant_inventory.has_value(),
        "observe descendant lineage inventory");
    const auto descendant_prepared = session.prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *descendant_receipt,
            .Inventory = *descendant_inventory,
            .Authorization =
                {
                    .CatalogIncarnation = incarnation,
                    .ExpectedCatalogRevision =
                        core::epoch_catalog_revision{1},
                    .Operation = descendant_operation,
                    .ExpectedPack = descendant_receipt->ExpectedPack,
                    .ReceiptToken = descendant_receipt->OpaqueToken,
                    .InventoryToken = descendant_inventory->OpaqueToken,
                    .DurableRecordToken = token(0x6cU),
                },
            .Base = descendant.NewCommit,
            .Head = descendant.NewCommit,
            .PriorHead = *baseline_commit,
        },
        {});
    require(
        descendant_prepared.has_value()
            && descendant_prepared->HeadLineage
                == core::head_lineage::descendant,
        "exact prior-to-current ancestry classifies a descendant upload");
    require(
        session.abort_operation(descendant_operation, {}).has_value(),
        "descendant lineage prepare exact-cleans staging refs");
    opened->reset();
    require(
        cache
            .delete_cache(
                cache_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        cache_request.ApplicationSession,
                    .Cache = cache_request.Authority.Cache,
                    .DurableIntentToken = token(0x6dU),
                },
                {})
            .has_value(),
        "delete isolated lineage cache after direct verification");
}

auto run_lock_holder(
    const std::filesystem::path& root,
    const std::filesystem::path& executable,
    const std::uint8_t cache_byte,
    const std::uint8_t scope_byte,
    const std::filesystem::path& ready,
    const std::filesystem::path& release) -> int
{
    auto cache = git_adapter::make_installed_git_cache({
        .GitExecutable = executable,
        .StorageRoot = root,
    });
    if(!cache.has_value())
    {
        return 20;
    }
    const auto cache_request = request(cache_byte, scope_byte);
    auto session = (*cache)->open(cache_request, {});
    if(!session.has_value())
    {
        return 21;
    }
    write_file(ready, "ready");
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{90};
    while(std::chrono::steady_clock::now() < deadline)
    {
        std::error_code error;
        if(std::filesystem::exists(release, error) && !error)
        {
            return 0;
        }
        QThread::msleep(10U);
    }
    return 22;
}

auto run_cache_contract() -> void
{
    const auto executable =
        std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE);
    auto root = temporary_root{};
    const auto force_push =
        make_force_push_fixture(executable, root.Path);
    const auto descendant = make_descendant_fixture(
        executable,
        root.Path,
        force_push.Remote);
    const auto sha256_fixture =
        make_sha256_fixture(executable, root.Path);
    auto cache = git_adapter::make_installed_git_cache({
        .GitExecutable = executable,
        .StorageRoot = root.Path,
    });
    require(cache.has_value(), "construct installed Git cache");
    verify_descendant_lineage(**cache, descendant);

    const auto cross_process_request = request(0x36U, 0x46U);
    const auto lock_ready = root.Path / "cross-process-lock-ready";
    const auto lock_release = root.Path / "cross-process-lock-release";
    auto lock_holder = QProcess{};
    lock_holder.setProgram(QCoreApplication::applicationFilePath());
    lock_holder.setArguments({
        "--hold-cache-lock",
        QString::fromStdString(root.Path.string()),
        QString::fromStdString(executable.string()),
        QString::number(0x36U),
        QString::number(0x46U),
        QString::fromStdString(lock_ready.string()),
        QString::fromStdString(lock_release.string()),
    });
    lock_holder.start();
    require(
        lock_holder.waitForStarted(5'000),
        "start cross-process cache lock holder");
    const auto lock_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{90};
    while(
        !std::filesystem::exists(lock_ready)
        && lock_holder.state() != QProcess::NotRunning
        && std::chrono::steady_clock::now() < lock_deadline)
    {
        QThread::msleep(10U);
    }
    require(
        std::filesystem::is_regular_file(lock_ready),
        "cross-process holder acquires exact cache lock");
    const auto cross_process_conflict =
        (*cache)->open(cross_process_request, {});
    require(
        !cross_process_conflict.has_value()
            && cross_process_conflict.error()
                == application::git_cache_failure::lock_unavailable,
        "independent process cannot acquire held cache writer lock");
    write_file(lock_release, "release");
    require(
        lock_holder.waitForFinished(30'000)
            && lock_holder.exitStatus() == QProcess::NormalExit
            && lock_holder.exitCode() == 0,
        "cross-process cache lock holder exits cleanly");
    require(
        (*cache)
            ->delete_cache(
                cross_process_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        cross_process_request.ApplicationSession,
                    .Cache =
                        cross_process_request.Authority.Cache,
                    .DurableIntentToken = token(0x76U),
                },
                {})
            .has_value(),
        "released cross-process cache lock admits exact cleanup");

    const auto manifest_failure_request = request(0x37U, 0x47U);
    const auto manifest_failure_repository =
        repository_path(root.Path, manifest_failure_request);
    const auto manifest_failure_marker =
        root.Path / "inject-cache-manifest-failure";
    write_file(manifest_failure_marker, "inject");
    const auto manifest_failure =
        (*cache)->open(manifest_failure_request, {});
    require(
        !manifest_failure.has_value()
            && manifest_failure.error()
                == application::git_cache_failure::invalid_manifest
            && !std::filesystem::exists(
                manifest_failure_repository),
        "manifest publication failure leaves no final cache path");
    std::filesystem::remove(manifest_failure_marker);
    auto manifest_retry =
        (*cache)->open(manifest_failure_request, {});
    require(
        manifest_retry.has_value(),
        "manifest publication failure remains cleanly retryable");
    manifest_retry->reset();
    require(
        (*cache)
            ->delete_cache(
                manifest_failure_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        manifest_failure_request
                            .ApplicationSession,
                    .Cache =
                        manifest_failure_request.Authority.Cache,
                    .DurableIntentToken = token(0x77U),
                },
                {})
            .has_value(),
        "retry-created manifest fixture exact-cleans");

    const auto validation_cleanup_request =
        request(0x3cU, 0x4cU);
    const auto validation_cleanup_marker =
        root.Path
        / "inject-cache-post-manifest-substitution";
    write_file(validation_cleanup_marker, "inject");
    const auto validation_cleanup =
        (*cache)->open(validation_cleanup_request, {});
    std::filesystem::remove(validation_cleanup_marker);
    const auto displaced_staging =
        root.Path / "repositories" / "v1"
        / ".bpr-test-displaced-cache-staging.git";
    auto replacement_staging =
        std::filesystem::path{};
    for(const auto& entry :
        std::filesystem::directory_iterator{
            root.Path / "repositories" / "v1"})
    {
        if(
            entry.path() != displaced_staging
            && std::filesystem::is_regular_file(
                entry.path() / "replacement-canary"))
        {
            replacement_staging = entry.path();
            break;
        }
    }
    require(
        !validation_cleanup.has_value()
            && validation_cleanup.error()
                == application::git_cache_failure::repair_required
            && !std::filesystem::exists(
                repository_path(
                    root.Path,
                    validation_cleanup_request))
            && std::filesystem::is_directory(displaced_staging)
            && !replacement_staging.empty()
            && std::ranges::equal(
                read_file(
                    replacement_staging
                    / "replacement-canary"),
                std::string_view{
                    "post-manifest-root-canary-dat"}),
        "unprovable post-manifest cleanup preserves replacement and requires repair");
    std::filesystem::remove_all(replacement_staging);
    std::filesystem::remove_all(displaced_staging);

    const auto destination_request = request(0x38U, 0x48U);
    const auto destination_repository =
        repository_path(root.Path, destination_request);
    const auto destination_marker =
        root.Path / "inject-cache-final-destination";
    write_file(destination_marker, "inject");
    const auto destination_collision =
        (*cache)->open(destination_request, {});
    require(
        !destination_collision.has_value()
            && destination_collision.error()
                == application::git_cache_failure::repair_required
            && read_file(destination_repository / "canary")
                == std::vector<std::uint8_t>{
                    'c', 'a', 'n', 'a', 'r', 'y'},
        "cache publication never adopts or deletes a final-path canary");
    std::filesystem::remove(destination_marker);
    std::filesystem::remove_all(destination_repository);

    const auto delete_race_request = request(0x3aU, 0x4aU);
    auto delete_race_session =
        (*cache)->open(delete_race_request, {});
    require(
        delete_race_session.has_value(),
        "create delete-destination race fixture");
    delete_race_session->reset();
    const auto delete_race_token = token(0x74U);
    const auto delete_race_authorization =
        application::git_cache_delete_authorization{
            .ApplicationSession =
                delete_race_request.ApplicationSession,
            .Cache = delete_race_request.Authority.Cache,
            .DurableIntentToken = delete_race_token,
        };
    const auto delete_race_marker =
        root.Path / "inject-cache-delete-destination-race";
    write_file(delete_race_marker, "inject");
    const auto delete_race = (*cache)->delete_cache(
        delete_race_request,
        delete_race_authorization,
        {});
    const auto delete_race_quarantine =
        root.Path / "quarantine" / "v1"
        / (lower_hex(
               delete_race_request.Authority.Cache.bytes())
            + "-" + lower_hex(delete_race_token) + ".git");
    require(
        !delete_race.has_value()
            && delete_race.error()
                == application::git_cache_failure::repair_required
            && std::filesystem::is_directory(
                repository_path(root.Path, delete_race_request))
            && read_file(delete_race_quarantine / "canary")
                == std::vector<std::uint8_t>{
                    'c', 'a', 'n', 'a', 'r', 'y'},
        "no-replace delete rename preserves a raced destination canary");
    std::filesystem::remove(delete_race_marker);
    std::filesystem::remove_all(delete_race_quarantine);
    require(
        (*cache)
            ->delete_cache(
                delete_race_request,
                delete_race_authorization,
                {})
            .has_value(),
        "same delete intent retries after destination collision removal");

    const auto child_substitution_request =
        request(0x3eU, 0x4eU);
    auto child_substitution_session =
        (*cache)->open(child_substitution_request, {});
    require(
        child_substitution_session.has_value(),
        "create descendant-substitution delete fixture");
    child_substitution_session->reset();
    const auto child_substitution_token = token(0x73U);
    const auto child_substitution_authorization =
        application::git_cache_delete_authorization{
            .ApplicationSession =
                child_substitution_request
                    .ApplicationSession,
            .Cache =
                child_substitution_request.Authority.Cache,
            .DurableIntentToken =
                child_substitution_token,
        };
    const auto child_substitution_marker =
        root.Path
        / "inject-cache-delete-child-substitution";
    write_file(child_substitution_marker, "inject");
    const auto child_substitution_delete =
        (*cache)->delete_cache(
            child_substitution_request,
            child_substitution_authorization,
            {});
    std::filesystem::remove(child_substitution_marker);
    const auto child_substitution_quarantine =
        root.Path / "quarantine" / "v1"
        / (lower_hex(
               child_substitution_request
                   .Authority.Cache.bytes())
            + "-" + lower_hex(child_substitution_token)
            + ".git");
    const auto child_substitution_manifest =
        child_substitution_quarantine
        / "betterpullreview-cache-manifest-v1";
    const auto child_substitution_original =
        child_substitution_quarantine
        / ".bpr-test-original-manifest";
    require(
        !child_substitution_delete.has_value()
            && child_substitution_delete.error()
                == application::git_cache_failure::repair_required
            && std::ranges::equal(
                read_file(child_substitution_manifest),
                std::string_view{
                    "replacement-child-canary"})
            && std::filesystem::is_regular_file(
                child_substitution_original),
        "recursive deletion revalidates descendants and preserves a replacement child");
    std::filesystem::remove(child_substitution_manifest);
    std::filesystem::rename(
        child_substitution_original,
        child_substitution_manifest);
    require(
        (*cache)
            ->delete_cache(
                child_substitution_request,
                child_substitution_authorization,
                {})
            .has_value(),
        "same tombstoned delete intent retries after exact child restoration");

    const auto orphan_quarantine_request =
        request(0x3bU, 0x4bU);
    const auto orphan_quarantine_token = token(0x75U);
    const auto orphan_quarantine =
        root.Path / "quarantine" / "v1"
        / (lower_hex(
               orphan_quarantine_request.Authority.Cache.bytes())
            + "-" + lower_hex(orphan_quarantine_token) + ".git");
    std::filesystem::create_directory(orphan_quarantine);
    std::filesystem::permissions(
        orphan_quarantine,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    write_file(orphan_quarantine / "canary", "orphan-canary");
    const auto orphan_quarantine_delete =
        (*cache)->delete_cache(
            orphan_quarantine_request,
            application::git_cache_delete_authorization{
                .ApplicationSession =
                    orphan_quarantine_request
                        .ApplicationSession,
                .Cache =
                    orphan_quarantine_request.Authority.Cache,
                .DurableIntentToken =
                    orphan_quarantine_token,
            },
            {});
    require(
        !orphan_quarantine_delete.has_value()
            && orphan_quarantine_delete.error()
                == application::git_cache_failure::repair_required
            && read_file(orphan_quarantine / "canary")
                == std::vector<std::uint8_t>{
                    'o', 'r', 'p', 'h', 'a', 'n', '-',
                    'c', 'a', 'n', 'a', 'r', 'y'},
        "deletion never adopts an untombstoned quarantine canary");
    std::filesystem::remove_all(orphan_quarantine);

    const auto primary_request = request(0x31U, 0x41U);
    auto isolated_request = request(0x32U, 0x42U);
    isolated_request.Authority.ProfileRevision =
        core::github_profile_revision{33U};
    isolated_request.Authority.Account =
        core::github_account_id{66U};
    isolated_request.Limits.PackCount = 1U;
    const auto primary_repository =
        repository_path(root.Path, primary_request);
    const auto isolated_repository =
        repository_path(root.Path, isolated_request);

    auto primary = (*cache)->open(primary_request, {});
    require(primary.has_value(), "create exact primary cache");
    require(
        std::filesystem::is_directory(primary_repository)
            && primary_repository.filename()
                == lower_hex(primary_request.Authority.Cache.bytes())
                    + ".git",
        "repository uses exact repositories/v1 scope path");
    const auto manifest =
        primary_repository / "betterpullreview-cache-manifest-v1";
    require(
        std::filesystem::is_regular_file(manifest),
        "cache contains immutable versioned manifest");

    const auto duplicate = (*cache)->open(primary_request, {});
    require(
        !duplicate.has_value()
            && duplicate.error()
                == application::git_cache_failure::lock_unavailable,
        "same cache scope has one cross-process writer");

    auto isolated = (*cache)->open(isolated_request, {});
    require(
        isolated.has_value()
            && std::filesystem::is_directory(isolated_repository),
        "unrelated cache scope opens while primary lock is held");
    require(
        std::ranges::distance(
            std::filesystem::directory_iterator{
                root.Path / "repositories" / "v1"},
            std::filesystem::directory_iterator{})
                == 2
            && std::filesystem::is_empty(
                root.Path / "incoming" / "v1"),
        "successful process probes leave no repository or spool residue");

#if defined(Q_OS_UNIX)
    const auto substituted_lock_request = request(0x39U, 0x49U);
    auto substituted_lock_session =
        (*cache)->open(substituted_lock_request, {});
    require(
        substituted_lock_session.has_value(),
        "create lock-substitution fixture cache");
    const auto substituted_lock =
        root.Path / "locks" / "v1"
        / (lower_hex(
               substituted_lock_request.Authority.Cache.bytes())
            + ".lock");
    require(
        std::filesystem::remove(substituted_lock),
        "unlink held cache lock fixture");
    write_file(substituted_lock, "replacement-lock-canary");
    const auto substituted_lock_open =
        (*cache)->open(substituted_lock_request, {});
    const auto substituted_lock_use =
        (*substituted_lock_session)->create_pack_sink(
            identity<core::epoch_operation_id>(0x45U),
            sha1_pack.size(),
            {});
    require(
        !substituted_lock_open.has_value()
            && substituted_lock_open.error()
                == application::git_cache_failure::invalid_manifest
            && !substituted_lock_use.has_value()
            && substituted_lock_use.error()
                == application::git_cache_failure::
                    invalid_repository
            && read_file(substituted_lock)
                == std::vector<std::uint8_t>{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'l', 'o', 'c', 'k',
                    '-', 'c', 'a', 'n', 'a', 'r', 'y'},
        "lock inode substitution cannot admit a second writer or mutate cache");
    substituted_lock_session->reset();
#endif

#if defined(Q_OS_UNIX)
    const auto process_boundary_request = request(0x40U, 0x50U);
    auto process_boundary_session =
        (*cache)->open(process_boundary_request, {});
    require(
        process_boundary_session.has_value(),
        "create process-boundary lock fixture cache");
    const auto process_boundary_operation =
        identity<core::epoch_operation_id>(0x60U);
    auto process_boundary_sink =
        (*process_boundary_session)->create_pack_sink(
            process_boundary_operation,
            sha1_pack.size(),
            {});
    require(
        process_boundary_sink.has_value(),
        "create process-boundary lock fixture spool");
    append_pack(**process_boundary_sink, sha1_pack);
    const auto process_boundary_receipt =
        (*process_boundary_sink)->finish({});
    require(
        process_boundary_receipt.has_value(),
        "finish process-boundary lock fixture spool");
    process_boundary_sink->reset();
    const auto process_boundary_observation =
        (*process_boundary_session)->observe_preimport(
            *process_boundary_receipt,
            {});
    const auto process_boundary_commit =
        core::parse_git_commit_oid(sha1_commit_hex);
    require(
        process_boundary_observation.has_value()
            && process_boundary_commit.has_value(),
        "observe process-boundary lock fixture baseline");
    const auto process_boundary_marker =
        root.Path
        / "inject-cache-lock-substitution-during-index";
    write_file(process_boundary_marker, "inject");
    const auto process_boundary_prepare =
        (*process_boundary_session)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *process_boundary_receipt,
                .Inventory = *process_boundary_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            identity<
                                core::epoch_catalog_incarnation_id>(
                                0x73U),
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation = process_boundary_operation,
                        .ExpectedPack =
                            process_boundary_receipt->ExpectedPack,
                        .ReceiptToken =
                            process_boundary_receipt->OpaqueToken,
                        .InventoryToken =
                            process_boundary_observation->OpaqueToken,
                        .DurableRecordToken = token(0x74U),
                    },
                .Base = *process_boundary_commit,
                .Head = *process_boundary_commit,
            },
            {});
    std::filesystem::remove(process_boundary_marker);
    const auto process_boundary_lock =
        root.Path / "locks" / "v1"
        / (lower_hex(
               process_boundary_request.Authority.Cache.bytes())
            + ".lock");
    const auto process_boundary_displaced_lock =
        process_boundary_lock.parent_path()
        / (".bpr-test-original-"
            + process_boundary_lock.filename().string());
    const auto process_boundary_spool =
        root.Path / "incoming" / "v1"
        / (lower_hex(process_boundary_operation.bytes())
            + ".pack.part");
    const auto process_boundary_pack_directory =
        repository_path(root.Path, process_boundary_request)
        / "objects" / "pack";
    const auto process_boundary_second_open =
        (*cache)->open(process_boundary_request, {});
    require(
        !process_boundary_prepare.has_value()
            && process_boundary_prepare.error()
                == application::git_cache_failure::repair_required
            && std::filesystem::is_regular_file(
                process_boundary_spool)
            && std::filesystem::is_regular_file(
                process_boundary_pack_directory
                    / ("pack-" + std::string{sha1_pack_hex}
                        + ".pack"))
            && std::filesystem::is_regular_file(
                process_boundary_pack_directory
                    / ("pack-" + std::string{sha1_pack_hex}
                        + ".idx"))
            && read_file(process_boundary_lock)
                == std::vector<std::uint8_t>{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'l', 'o', 'c', 'k',
                    '-', 'c', 'a', 'n', 'a', 'r', 'y'}
            && !process_boundary_second_open.has_value()
            && process_boundary_second_open.error()
                == application::git_cache_failure::
                    invalid_manifest,
        "process postcheck binds repository and exact held lock while retaining uncertain state");
    require(
        std::filesystem::remove(process_boundary_lock),
        "remove process-boundary replacement lock canary");
    std::filesystem::rename(
        process_boundary_displaced_lock,
        process_boundary_lock);
    require(
        (*process_boundary_session)
            ->abort_operation(process_boundary_operation, {})
            .has_value()
            && !std::filesystem::exists(process_boundary_spool),
        "restored process-boundary lock permits exact spool cleanup");
    process_boundary_session->reset();
    require(
        (*cache)
            ->delete_cache(
                process_boundary_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        process_boundary_request.ApplicationSession,
                    .Cache =
                        process_boundary_request.Authority.Cache,
                    .DurableIntentToken = token(0x75U),
                },
                {})
            .has_value(),
        "process-boundary lock fixture exact-cleans");

    const auto delete_boundary_request = request(0x41U, 0x51U);
    auto delete_boundary_session =
        (*cache)->open(delete_boundary_request, {});
    require(
        delete_boundary_session.has_value(),
        "create delete-boundary lock fixture cache");
    delete_boundary_session->reset();
    const auto delete_boundary_lock =
        root.Path / "locks" / "v1"
        / (lower_hex(delete_boundary_request.Authority.Cache.bytes())
            + ".lock");
    const auto delete_boundary_displaced_lock =
        delete_boundary_lock.parent_path()
        / (".bpr-test-original-delete-"
            + delete_boundary_lock.filename().string());
    const auto delete_boundary_repository =
        repository_path(root.Path, delete_boundary_request);
    const auto delete_boundary_marker =
        root.Path
        / "inject-cache-lock-substitution-during-delete";
    const auto delete_boundary_authorization =
        application::git_cache_delete_authorization{
            .ApplicationSession =
                delete_boundary_request.ApplicationSession,
            .Cache = delete_boundary_request.Authority.Cache,
            .DurableIntentToken = token(0x76U),
        };
    write_file(delete_boundary_marker, "inject");
    const auto delete_boundary_result =
        (*cache)->delete_cache(
            delete_boundary_request,
            delete_boundary_authorization,
            {});
    std::filesystem::remove(delete_boundary_marker);
    const auto delete_boundary_second_open =
        (*cache)->open(delete_boundary_request, {});
    const auto delete_boundary_tombstone =
        root.Path / "quarantine" / "v1"
        / (lower_hex(delete_boundary_request.Authority.Cache.bytes())
            + ".deleted");
    require(
        !delete_boundary_result.has_value()
            && delete_boundary_result.error()
                == application::git_cache_failure::repair_required
            && std::filesystem::is_directory(
                delete_boundary_repository)
            && !std::filesystem::exists(delete_boundary_tombstone)
            && read_file(delete_boundary_lock)
                == std::vector<std::uint8_t>{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'd', 'e', 'l', 'e',
                    't', 'e', '-', 'l', 'o', 'c', 'k', '-',
                    'c', 'a', 'n', 'a', 'r', 'y'}
            && !delete_boundary_second_open.has_value()
            && delete_boundary_second_open.error()
                == application::git_cache_failure::
                    invalid_manifest,
        "mid-delete lock substitution preserves the canary and stops before destructive publication");
    require(
        std::filesystem::remove(delete_boundary_lock),
        "remove delete-boundary replacement lock canary");
    std::filesystem::rename(
        delete_boundary_displaced_lock,
        delete_boundary_lock);
    require(
        (*cache)
            ->delete_cache(
                delete_boundary_request,
                delete_boundary_authorization,
                {})
            .has_value(),
        "restored delete-boundary lock permits authorized cleanup");
#endif

    const auto root_substitution_request =
        request(0x3fU, 0x4fU);
    auto root_substitution_session =
        (*cache)->open(root_substitution_request, {});
    require(
        root_substitution_session.has_value(),
        "create repository-root substitution fixture");
    const auto root_substitution_operation =
        identity<core::epoch_operation_id>(0x5dU);
    auto root_substitution_sink =
        (*root_substitution_session)->create_pack_sink(
            root_substitution_operation,
            sha1_pack.size(),
            {});
    require(
        root_substitution_sink.has_value(),
        "create repository-root substitution spool");
    append_pack(**root_substitution_sink, sha1_pack);
    const auto root_substitution_receipt =
        (*root_substitution_sink)->finish({});
    require(
        root_substitution_receipt.has_value(),
        "finish repository-root substitution spool");
    root_substitution_sink->reset();
    const auto root_substitution_observation =
        (*root_substitution_session)->observe_preimport(
            *root_substitution_receipt,
            {});
    const auto root_substitution_commit =
        core::parse_git_commit_oid(sha1_commit_hex);
    require(
        root_substitution_observation.has_value()
            && root_substitution_commit.has_value(),
        "observe repository-root substitution baseline");
    const auto root_substitution_repository =
        repository_path(
            root.Path,
            root_substitution_request);
    const auto root_substitution_original =
        root.Path / "repositories" / "v1"
        / ".bpr-test-original-root-substitution.git";
    std::filesystem::rename(
        root_substitution_repository,
        root_substitution_original);
    std::filesystem::copy(
        root_substitution_original,
        root_substitution_repository,
        std::filesystem::copy_options::recursive);
    make_tree_private(root_substitution_repository);
    write_file(
        root_substitution_repository
            / "root-substitution-canary",
        "root-substitution-canary");
    const auto root_substitution_marker =
        root.Path / "inject-cache-index-terminal-failure";
    write_file(root_substitution_marker, "inject");
    const auto root_substitution_prepare =
        (*root_substitution_session)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *root_substitution_receipt,
                .Inventory =
                    *root_substitution_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            identity<
                                core::epoch_catalog_incarnation_id>(
                                0x5eU),
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation =
                            root_substitution_operation,
                        .ExpectedPack =
                            root_substitution_receipt
                                ->ExpectedPack,
                        .ReceiptToken =
                            root_substitution_receipt
                                ->OpaqueToken,
                        .InventoryToken =
                            root_substitution_observation
                                ->OpaqueToken,
                        .DurableRecordToken = token(0x5fU),
                    },
                .Base = *root_substitution_commit,
                .Head = *root_substitution_commit,
            },
            {});
    std::filesystem::remove(root_substitution_marker);
    const auto root_substitution_pack_directory =
        root_substitution_repository / "objects" / "pack";
    require(
        !root_substitution_prepare.has_value()
            && root_substitution_prepare.error()
                == application::git_cache_failure::
                    invalid_repository
            && std::ranges::equal(
                read_file(
                    root_substitution_repository
                    / "root-substitution-canary"),
                std::string_view{"root-substitution-canary"})
            && std::filesystem::is_empty(
                root_substitution_pack_directory),
        "bound repository root rejects same-manifest substitution before Git");
    std::filesystem::remove_all(
        root_substitution_repository);
    std::filesystem::rename(
        root_substitution_original,
        root_substitution_repository);
    require(
        (*root_substitution_session)
            ->abort_operation(
                root_substitution_operation,
                {})
            .has_value(),
        "restored exact repository root remains recoverable");
    root_substitution_session->reset();
    require(
        (*cache)
            ->delete_cache(
                root_substitution_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        root_substitution_request
                            .ApplicationSession,
                    .Cache =
                        root_substitution_request
                            .Authority.Cache,
                    .DurableIntentToken = token(0x72U),
                },
                {})
            .has_value(),
        "repository-root substitution fixture exact-cleans");

    const auto first_receipt_operation =
        identity<core::epoch_operation_id>(0x46U);
    auto first_receipt_sink = (*isolated)->create_pack_sink(
        first_receipt_operation,
        sha1_pack.size(),
        {});
    require(
        first_receipt_sink.has_value(),
        "create first receipt-cap operation");
    append_pack(**first_receipt_sink, sha1_pack);
    const auto first_receipt = (*first_receipt_sink)->finish({});
    require(first_receipt.has_value(), "register first bounded receipt");
    first_receipt_sink->reset();
    const auto isolated_baseline =
        (*isolated)->observe_preimport(*first_receipt, {});
    require(
        isolated_baseline.has_value(),
        "observe empty-cache quota baseline");

    const auto rejected_receipt_operation =
        identity<core::epoch_operation_id>(0x47U);
    auto rejected_receipt_sink = (*isolated)->create_pack_sink(
        rejected_receipt_operation,
        sha1_pack.size(),
        {});
    require(
        rejected_receipt_sink.has_value(),
        "create receipt-cap rejection operation");
    append_pack(**rejected_receipt_sink, sha1_pack);
    const auto rejected_receipt =
        (*rejected_receipt_sink)->finish({});
    require(
        !rejected_receipt.has_value()
            && rejected_receipt.error()
                == application::git_pack_stream_failure::
                    invalid_state,
        "receipt registry cap fails after completed-spool publication");
    rejected_receipt_sink->reset();
    const auto rejected_receipt_path =
        root.Path / "incoming" / "v1"
        / (lower_hex(rejected_receipt_operation.bytes())
            + ".pack.part");
    require(
        !std::filesystem::exists(rejected_receipt_path),
        "post-publication registration failure removes its owned spool");
    require(
        (*isolated)
            ->abort_operation(first_receipt_operation, {})
            .has_value(),
        "clear first receipt-cap operation");

    isolated->reset();
    auto isolated_quota_request = isolated_request;
    isolated_quota_request.Limits.OperationPackBytes =
        sha1_pack.size();
    isolated_quota_request.Limits.RepositoryBytes =
        isolated_baseline->RepositoryBytes + sha1_pack.size();
    require(
        application::valid(isolated_quota_request),
        "valid exact tight new-pack quota");
    isolated = (*cache)->open(isolated_quota_request, {});
    require(isolated.has_value(), "open tight new-pack quota cache");
    const auto quota_operation =
        identity<core::epoch_operation_id>(0x48U);
    auto quota_sink = (*isolated)->create_pack_sink(
        quota_operation,
        sha1_pack.size(),
        {});
    require(quota_sink.has_value(), "create tight-quota pack sink");
    append_pack(**quota_sink, sha1_pack);
    const auto quota_receipt = (*quota_sink)->finish({});
    require(quota_receipt.has_value(), "finish tight-quota pack");
    quota_sink->reset();
    const auto quota_observation =
        (*isolated)->observe_preimport(*quota_receipt, {});
    require(
        !quota_observation.has_value()
            && quota_observation.error()
                == application::git_cache_failure::quota_exceeded,
        "new-pack admission reserves index and paired-ref headroom");
    const auto isolated_pack_directory =
        isolated_repository / "objects" / "pack";
    require(
        !std::filesystem::exists(isolated_pack_directory)
            || std::filesystem::is_empty(isolated_pack_directory),
        "new-pack quota rejection installs no pack or index");
    require(
        (*isolated)->abort_operation(quota_operation, {}).has_value(),
        "tight-quota rejection exact-cleans its completed spool");
    isolated->reset();
    isolated = (*cache)->open(isolated_request, {});
    require(
        isolated.has_value(),
        "tight-quota rejection leaves cache reusable");

    const auto preobserve_substitution_operation =
        identity<core::epoch_operation_id>(0x58U);
    auto preobserve_substitution_sink =
        (*isolated)->create_pack_sink(
            preobserve_substitution_operation,
            sha1_pack.size(),
            {});
    require(
        preobserve_substitution_sink.has_value(),
        "create pre-observation spool-substitution sink");
    append_pack(**preobserve_substitution_sink, sha1_pack);
    const auto preobserve_substitution_receipt =
        (*preobserve_substitution_sink)->finish({});
    require(
        preobserve_substitution_receipt.has_value(),
        "finish pre-observation spool-substitution receipt");
    preobserve_substitution_sink->reset();
    const auto preobserve_substitution_path =
        root.Path / "incoming" / "v1"
        / (lower_hex(
               preobserve_substitution_operation.bytes())
            + ".pack.part");
    const auto preobserve_substitution_original =
        preobserve_substitution_path.parent_path()
        / "preobserve-substitution-original.pack";
    std::filesystem::rename(
        preobserve_substitution_path,
        preobserve_substitution_original);
    write_file(preobserve_substitution_path, sha1_pack);
    const auto substituted_observation =
        (*isolated)->observe_preimport(
            *preobserve_substitution_receipt,
            {});
    require(
        !substituted_observation.has_value()
            && substituted_observation.error()
                == application::git_cache_failure::spool_failure
            && std::ranges::equal(
                read_file(preobserve_substitution_path),
                sha1_pack),
        "observation rejects and preserves a same-content replacement inode");
    std::filesystem::remove(preobserve_substitution_path);
    std::filesystem::rename(
        preobserve_substitution_original,
        preobserve_substitution_path);
    require(
        (*isolated)
            ->abort_operation(
                preobserve_substitution_operation,
                {})
            .has_value(),
        "pre-observation substitution leaves exact original recoverable");

    const auto preindex_substitution_operation =
        identity<core::epoch_operation_id>(0x59U);
    auto preindex_substitution_sink =
        (*isolated)->create_pack_sink(
            preindex_substitution_operation,
            sha1_pack.size(),
            {});
    require(
        preindex_substitution_sink.has_value(),
        "create pre-index spool-substitution sink");
    append_pack(**preindex_substitution_sink, sha1_pack);
    const auto preindex_substitution_receipt =
        (*preindex_substitution_sink)->finish({});
    require(
        preindex_substitution_receipt.has_value(),
        "finish pre-index spool-substitution receipt");
    preindex_substitution_sink->reset();
    const auto preindex_substitution_observation =
        (*isolated)->observe_preimport(
            *preindex_substitution_receipt,
            {});
    const auto preindex_substitution_head =
        core::parse_git_commit_oid(sha1_commit_hex);
    require(
        preindex_substitution_observation.has_value()
            && preindex_substitution_head.has_value(),
        "observe pre-index spool-substitution baseline");
    const auto preindex_substitution_path =
        root.Path / "incoming" / "v1"
        / (lower_hex(
               preindex_substitution_operation.bytes())
            + ".pack.part");
    const auto preindex_substitution_original =
        preindex_substitution_path.parent_path()
        / "preindex-substitution-original.pack";
    std::filesystem::rename(
        preindex_substitution_path,
        preindex_substitution_original);
    write_file(preindex_substitution_path, sha1_pack);
    const auto index_failure_marker =
        root.Path / "inject-cache-index-terminal-failure";
    write_file(index_failure_marker, "inject");
    const auto substituted_prepare =
        (*isolated)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *preindex_substitution_receipt,
                .Inventory =
                    *preindex_substitution_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            identity<
                                core::epoch_catalog_incarnation_id>(
                                0x5aU),
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation =
                            preindex_substitution_operation,
                        .ExpectedPack =
                            preindex_substitution_receipt
                                ->ExpectedPack,
                        .ReceiptToken =
                            preindex_substitution_receipt
                                ->OpaqueToken,
                        .InventoryToken =
                            preindex_substitution_observation
                                ->OpaqueToken,
                        .DurableRecordToken = token(0x5bU),
                    },
                .Base = *preindex_substitution_head,
                .Head = *preindex_substitution_head,
            },
            {});
    std::filesystem::remove(index_failure_marker);
    require(
        !substituted_prepare.has_value()
            && substituted_prepare.error()
                == application::git_cache_failure::spool_failure
            && std::ranges::equal(
                read_file(preindex_substitution_path),
                sha1_pack)
            && (
                !std::filesystem::exists(
                    isolated_pack_directory)
                || std::filesystem::is_empty(
                    isolated_pack_directory)),
        "pre-index identity check prevents Git handoff/import and preserves replacement");
    std::filesystem::remove(preindex_substitution_path);
    std::filesystem::rename(
        preindex_substitution_original,
        preindex_substitution_path);
    require(
        (*isolated)
            ->abort_operation(
                preindex_substitution_operation,
                {})
            .has_value(),
        "pre-index substitution leaves exact original recoverable");

    const auto rollback_operation =
        identity<core::epoch_operation_id>(0x4aU);
    auto rollback_sink = (*isolated)->create_pack_sink(
        rollback_operation,
        sha1_pack.size(),
        {});
    require(rollback_sink.has_value(), "create rollback pack sink");
    append_pack(**rollback_sink, sha1_pack);
    const auto rollback_receipt = (*rollback_sink)->finish({});
    require(rollback_receipt.has_value(), "finish rollback pack spool");
    rollback_sink->reset();
    const auto rollback_observation =
        (*isolated)->observe_preimport(*rollback_receipt, {});
    const auto rollback_head =
        core::parse_git_commit_oid(sha1_commit_hex);
    require(
        rollback_observation.has_value()
            && rollback_head.has_value(),
        "prepare rollback fixture identities");
    auto missing_bytes = rollback_head->bytes();
    missing_bytes.front() ^= 0x01U;
    const auto missing_commit = core::git_commit_oid{
        core::git_hash_algorithm::sha1,
        missing_bytes};
    const auto rollback_prepare =
        (*isolated)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *rollback_receipt,
                .Inventory = *rollback_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            identity<
                                core::epoch_catalog_incarnation_id>(
                                0x4bU),
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation = rollback_operation,
                        .ExpectedPack =
                            rollback_receipt->ExpectedPack,
                        .ReceiptToken =
                            rollback_receipt->OpaqueToken,
                        .InventoryToken =
                            rollback_observation->OpaqueToken,
                        .DurableRecordToken = token(0x4cU),
                    },
                .Base = missing_commit,
                .Head = *rollback_head,
            },
            {});
    require(
        !rollback_prepare.has_value()
            && rollback_prepare.error()
                == application::git_cache_failure::process_failure,
        "missing requested commit fails after local pack import");
    require(
        !std::filesystem::exists(isolated_pack_directory)
            || std::filesystem::is_empty(isolated_pack_directory),
        "failed pre-staging import rolls back only its new pack and index");
    require(
        (*isolated)
            ->abort_operation(rollback_operation, {})
            .has_value(),
        "failed pre-staging import retains exact spool for explicit abort");
    const auto committed_failure_operation =
        identity<core::epoch_operation_id>(0x4dU);
    auto committed_failure_sink =
        (*isolated)->create_pack_sink(
            committed_failure_operation,
            sha1_pack.size(),
            {});
    require(
        committed_failure_sink.has_value(),
        "create committed-terminal-failure sink");
    append_pack(**committed_failure_sink, sha1_pack);
    const auto committed_failure_receipt =
        (*committed_failure_sink)->finish({});
    require(
        committed_failure_receipt.has_value(),
        "finish committed-terminal-failure spool");
    committed_failure_sink->reset();
    const auto committed_failure_observation =
        (*isolated)->observe_preimport(
            *committed_failure_receipt,
            {});
    require(
        committed_failure_observation.has_value(),
        "observe committed-terminal-failure inventory");
    write_file(index_failure_marker, "inject");
    const auto committed_terminal_failure =
        (*isolated)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *committed_failure_receipt,
                .Inventory = *committed_failure_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            identity<
                                core::epoch_catalog_incarnation_id>(
                                0x4eU),
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation =
                            committed_failure_operation,
                        .ExpectedPack =
                            committed_failure_receipt
                                ->ExpectedPack,
                        .ReceiptToken =
                            committed_failure_receipt
                                ->OpaqueToken,
                        .InventoryToken =
                            committed_failure_observation
                                ->OpaqueToken,
                        .DurableRecordToken = token(0x4fU),
                    },
                .Base = *rollback_head,
                .Head = *rollback_head,
            },
            {});
    std::filesystem::remove(index_failure_marker);
    require(
        !committed_terminal_failure.has_value()
            && committed_terminal_failure.error()
                == application::git_cache_failure::process_failure
            && (
                !std::filesystem::exists(
                    isolated_pack_directory)
                || std::filesystem::is_empty(
                    isolated_pack_directory)),
        "post-terminal index failure reconciles and rolls back only its new pack");
    require(
        (*isolated)
            ->abort_operation(
                committed_failure_operation,
                {})
            .has_value(),
        "reconciled index failure retains exact spool for abort");

    const auto malformed_operation =
        identity<core::epoch_operation_id>(0x51U);
    auto malformed_sink = (*primary)->create_pack_sink(
        malformed_operation,
        sha1_pack.size(),
        {});
    require(malformed_sink.has_value(), "create malformed-pack sink");
    auto corrupted = sha1_pack;
    corrupted[40U] ^= 0x01U;
    append_pack(**malformed_sink, corrupted);
    const auto malformed = (*malformed_sink)->finish({});
    require(
        !malformed.has_value()
            && malformed.error()
                == application::git_pack_stream_failure::
                    malformed_pack,
        "pack body/trailer checksum disagreement fails closed");
    malformed_sink->reset();
    const auto malformed_text =
        lower_hex(malformed_operation.bytes());
    require(
        !std::filesystem::exists(
            root.Path / "incoming" / "v1"
            / (malformed_text + ".pack.part"))
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / ("." + malformed_text + ".pack.receiving")),
        "failed sink leaves no completed or receiving spool residue");

    const auto cancelled_operation =
        identity<core::epoch_operation_id>(0x49U);
    auto cancelled_sink = (*primary)->create_pack_sink(
        cancelled_operation,
        sha1_pack.size(),
        {});
    require(cancelled_sink.has_value(), "create cancellable pack sink");
    auto cancelled_source = std::stop_source{};
    cancelled_source.request_stop();
    const auto cancelled_append = (*cancelled_sink)->append(
        std::as_bytes(std::span{sha1_pack}),
        cancelled_source.get_token());
    require(
        !cancelled_append.has_value()
            && cancelled_append.error()
                == application::git_pack_stream_failure::cancelled,
        "cancelled pack append fails before admission");
    cancelled_sink->reset();
    const auto cancelled_text =
        lower_hex(cancelled_operation.bytes());
    require(
        !std::filesystem::exists(
            root.Path / "incoming" / "v1"
            / (cancelled_text + ".pack.part"))
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / ("." + cancelled_text + ".pack.receiving")),
        "cancelled pack sink leaves no spool residue");
    const auto cancelled_finish_operation =
        identity<core::epoch_operation_id>(0x48U);
    auto cancelled_finish_sink = (*primary)->create_pack_sink(
        cancelled_finish_operation,
        sha1_pack.size(),
        {});
    require(
        cancelled_finish_sink.has_value(),
        "create finish-cancellation pack sink");
    append_pack(**cancelled_finish_sink, sha1_pack);
    const auto cancelled_finish =
        (*cancelled_finish_sink)
            ->finish(cancelled_source.get_token());
    require(
        !cancelled_finish.has_value()
            && cancelled_finish.error()
                == application::git_pack_stream_failure::cancelled,
        "cancelled pack finish stops before publication");
    cancelled_finish_sink->reset();
    const auto cancelled_finish_text =
        lower_hex(cancelled_finish_operation.bytes());
    require(
        !std::filesystem::exists(
            root.Path / "incoming" / "v1"
            / (cancelled_finish_text + ".pack.part"))
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / ("." + cancelled_finish_text
                    + ".pack.receiving")),
        "cancelled pack finish leaves no spool residue");

    const auto operation =
        identity<core::epoch_operation_id>(0x52U);
    auto sink = (*primary)->create_pack_sink(
        operation,
        sha1_pack.size(),
        {});
    require(sink.has_value(), "create bounded private pack sink");
    append_pack(**sink, sha1_pack);
    const auto receipt = (*sink)->finish({});
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
    const auto serialized_spool_identity =
        receipt->Spool.Identity.Bytes;
    const auto restored_spool_identity =
        application::git_artifact_identity{
            .Bytes = serialized_spool_identity};
    require(
        restored_spool_identity == receipt->Spool.Identity
            && application::valid(restored_spool_identity),
        "completed spool identity round-trips as fixed opaque bytes");
    sink->reset();
    const auto completed_spool =
        root.Path / "incoming" / "v1"
        / (lower_hex(operation.bytes()) + ".pack.part");
    require(
        std::filesystem::is_regular_file(completed_spool),
        "validated receipt publishes only the exact completed spool");

    const auto observation =
        (*primary)->observe_preimport(*receipt, {});
    require(
        observation.has_value() && observation->Packs.empty()
            && !observation->ExpectedPackPreexisting
            && observation->RepositoryBytes != 0U
            && application::valid_cache_token(
                observation->OpaqueToken),
        "preimport inventory is bounded and tokenized");
    auto forged_spool_receipt = *receipt;
    forged_spool_receipt.Spool.Identity.Bytes.back() ^= 0x01U;
    const auto forged_spool_observation =
        (*primary)->observe_preimport(
            forged_spool_receipt,
            {});
    require(
        application::agrees(forged_spool_receipt)
            && !forged_spool_observation.has_value()
            && forged_spool_observation.error()
                == application::git_cache_failure::spool_failure,
        "opaque durable spool identity is authorization-bound");

    const auto commit = core::parse_git_commit_oid(sha1_commit_hex);
    const auto tree = core::parse_git_tree_oid(sha1_tree_hex);
    require(
        commit.has_value() && tree.has_value(),
        "parse fixture commit and tree");
    const auto incarnation =
        identity<core::epoch_catalog_incarnation_id>(0x61U);
    auto authorization = application::git_import_authorization{
        .CatalogIncarnation = incarnation,
        .ExpectedCatalogRevision = core::epoch_catalog_revision{0},
        .Operation = operation,
        .ExpectedPack = receipt->ExpectedPack,
        .ReceiptToken = receipt->OpaqueToken,
        .InventoryToken = observation->OpaqueToken,
        .DurableRecordToken = token(0x62U),
    };
    auto forged_observation = *observation;
    forged_observation.OpaqueToken = token(0x63U);
    auto forged_authorization = authorization;
    forged_authorization.InventoryToken =
        forged_observation.OpaqueToken;
    const auto sha256_prior = core::parse_git_commit_oid(
        std::string(64U, 'f'));
    auto mixed_algorithm_request =
        application::git_prepare_revision_request{
            .Receipt = *receipt,
            .Inventory = *observation,
            .Authorization = authorization,
            .Base = *commit,
            .Head = *commit,
            .PriorHead = sha256_prior,
        };
    require(
        sha256_prior.has_value()
            && !application::valid(mixed_algorithm_request),
        "prior head must use the exact current object format");
    const auto unauthorized = (*primary)->prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *receipt,
            .Inventory = forged_observation,
            .Authorization = forged_authorization,
            .Base = *commit,
            .Head = *commit,
        },
        {});
    require(
        !unauthorized.has_value()
            && unauthorized.error()
                == application::git_cache_failure::
                    authorization_mismatch,
        "caller cannot forge preimport authorization facts");

    const auto prepared = (*primary)->prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *receipt,
            .Inventory = *observation,
            .Authorization = authorization,
            .Base = *commit,
            .Head = *commit,
        },
        {});
    require(
        prepared.has_value() && application::valid(*prepared)
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
    auto invalid_lineage = *prepared;
    invalid_lineage.HeadLineage =
        static_cast<core::head_lineage>(0xffU);
    require(
        !application::valid(invalid_lineage),
        "prepared revision rejects an out-of-domain lineage");
    require(
        !std::filesystem::exists(completed_spool),
        "successful import exact-deletes its completed spool");

    const auto recovery_request =
        application::git_operation_recovery_request{
            .Operation = operation,
            .Scope = primary_request.Authority.Scope,
            .Ordinal = core::epoch_ordinal{1},
            .Base = *commit,
            .Head = *commit,
            .Spool =
                application::git_recovery_spool_expectation{
                    .ExpectedPack = receipt->ExpectedPack,
                    .CompletedSpool = receipt->Spool,
                    .PreimportPacks = observation->Packs,
                    .PreimportRepositoryBytes =
                        observation->RepositoryBytes,
                    .ExpectedPackPreexisting =
                        observation->ExpectedPackPreexisting,
                },
            .Install =
                application::git_recovery_install_expectation{
                    .Pack = prepared->Pack,
                    .InstalledPack = prepared->InstalledPack,
                    .InstalledIndex = prepared->InstalledIndex,
                    .BaseTree = prepared->BaseTree,
                    .HeadTree = prepared->HeadTree,
                },
        };
    require(
        application::valid(recovery_request),
        "durable append facts form a valid recovery request");
    const auto live_recovery =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    require(
        live_recovery.has_value()
            && live_recovery->Spool
                == application::git_recovery_match::absent
            && live_recovery->Installed
                == application::git_recovery_match::exact
            && live_recovery->Staging
                == application::git_recovery_match::exact
            && live_recovery->Final
                == application::git_recovery_match::absent
            && live_recovery->Objects
                == application::git_recovery_object_match::exact
            && live_recovery->Refs
                == application::git_recovery_ref_topology::staging,
        "live recovery inspection agrees with exact staged durable facts");
    auto recovery_cancel = std::stop_source{};
    recovery_cancel.request_stop();
    const auto cancelled_recovery =
        (*primary)->inspect_operation_recovery(
            recovery_request,
            recovery_cancel.get_token());
    const auto cancelled_commit = (*primary)->inspect_commit_object(
        *commit,
        recovery_cancel.get_token());
    const auto cancelled_abort = (*primary)->abort_recovered_operation(
        recovery_request,
        recovery_cancel.get_token());
    const auto cancelled_completion =
        (*primary)->complete_recovered_operation(
            recovery_request,
            recovery_cancel.get_token());
    require(
        !cancelled_recovery.has_value()
            && cancelled_recovery.error()
                == application::git_cache_failure::cancelled
            && !cancelled_commit.has_value()
            && cancelled_commit.error()
                == application::git_cache_failure::cancelled
            && !cancelled_abort.has_value()
            && cancelled_abort.error()
                == application::git_cache_failure::cancelled
            && !cancelled_completion.has_value()
            && cancelled_completion.error()
                == application::git_cache_failure::cancelled,
        "restart inspection and cleanup entry points honor cancellation without mutation");
    primary->reset();
    primary = (*cache)->open(primary_request, {});
    require(primary.has_value(), "reopen cache for restart recovery");
    const auto restarted_recovery =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    require(
        restarted_recovery == live_recovery,
        "restart reconstructs the same spool/install/staging/final evidence without process-local records");
    const auto installed_index_path =
        primary_repository / "objects" / "pack"
        / ("pack-" + std::string{sha1_pack_hex} + ".idx");
    const auto saved_installed_index =
        root.Path / "saved-recovery-index";
    std::filesystem::rename(
        installed_index_path,
        saved_installed_index);
    require(
        std::filesystem::copy_file(
            saved_installed_index,
            installed_index_path),
        "substitute installed recovery index identity");
    std::filesystem::permissions(
        installed_index_path,
        std::filesystem::perms::owner_read,
        std::filesystem::perm_options::replace);
    const auto substituted_install =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    const auto refused_substituted_install_abort =
        (*primary)->abort_recovered_operation(recovery_request, {});
    require(
        substituted_install.has_value()
            && substituted_install->Installed
                == application::git_recovery_match::mismatch
            && !refused_substituted_install_abort.has_value()
            && refused_substituted_install_abort.error()
                == application::git_cache_failure::repair_required,
        "restart inspection rejects a byte-identical installed artifact with the wrong durable identity");
    require(
        std::filesystem::remove(installed_index_path),
        "remove substituted recovery index");
    std::filesystem::rename(
        saved_installed_index,
        installed_index_path);
    require(
        (*primary)->inspect_operation_recovery(recovery_request, {})
            == live_recovery,
        "restoring the exact installed artifact restores recovery evidence");
    const auto verified_commit =
        (*primary)->inspect_commit_object(*commit, {});
    const auto unavailable_commit = core::parse_git_commit_oid(
        std::string(40U, 'f'));
    require(
        unavailable_commit.has_value(),
        "parse unavailable exact commit fixture");
    const auto rejected_unavailable =
        (*primary)->inspect_commit_object(*unavailable_commit, {});
    require(
        verified_commit.has_value()
            && verified_commit->Commit == *commit
            && verified_commit->Tree == *tree
            && !rejected_unavailable.has_value()
            && rejected_unavailable.error()
                == application::git_cache_failure::object_mismatch,
        "exact commit inspection verifies tree/closure and rejects catalog-only availability");

    const auto git_dir =
        "--git-dir=" + QString::fromStdString(primary_repository.string());
    const auto staging_prefix =
        "refs/betterpullreview/v1/stage/"
        + lower_hex(operation.bytes());
    const auto staging_head = staging_prefix + "/head";
    const auto final_prefix =
        "refs/betterpullreview/v1/scope/"
        + lower_hex(primary_request.Authority.Scope.bytes())
        + "/epoch/0000000000000001";
    const auto final_base = final_prefix + "/base";
    const auto make_ref_private = [&](const std::string_view ref)
    {
        std::error_code error;
        auto directory = (primary_repository / ref).parent_path();
        while(directory != primary_repository)
        {
            std::filesystem::permissions(
                directory,
                std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace,
                error);
            require(!error, "make fixture recovery ref directory private");
            directory = directory.parent_path();
        }
        std::filesystem::permissions(
            primary_repository / ref,
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
            git_dir,
            "update-ref",
            "-d",
            QString::fromStdString(staging_head),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    const auto mixed_staging =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    const auto refused_mixed_abort =
        (*primary)->abort_recovered_operation(recovery_request, {});
    require(
        mixed_staging.has_value()
            && mixed_staging->Staging
                == application::git_recovery_match::ambiguous
            && mixed_staging->Refs
                == application::git_recovery_ref_topology::ambiguous
            && !refused_mixed_abort.has_value()
            && refused_mixed_abort.error()
                == application::git_cache_failure::repair_required,
        "one-sided staging state is ambiguous and exact abort preserves it");
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(staging_head),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    make_ref_private(staging_head);
    const auto wrong_ref_oid = std::string{sha1_tree_hex};
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(staging_head),
            QString::fromStdString(wrong_ref_oid),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    make_ref_private(staging_head);
    const auto wrong_staging =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    const auto refused_wrong_abort =
        (*primary)->abort_recovered_operation(recovery_request, {});
    require(
        wrong_staging.has_value()
            && wrong_staging->Staging
                == application::git_recovery_match::mismatch
            && !refused_wrong_abort.has_value()
            && refused_wrong_abort.error()
                == application::git_cache_failure::repair_required,
        "wrong staging target remains visible as mismatch and is never exact-cleaned");
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(staging_head),
            QString::fromUtf8(sha1_commit_hex.data()),
            QString::fromStdString(wrong_ref_oid),
        }));
    make_ref_private(staging_head);
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(final_base),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    make_ref_private(final_base);
    const auto mixed_final =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    const auto refused_mixed_completion =
        (*primary)->complete_recovered_operation(recovery_request, {});
    require(
        mixed_final.has_value()
            && mixed_final->Staging
                == application::git_recovery_match::exact
            && mixed_final->Final
                == application::git_recovery_match::ambiguous
            && mixed_final->Refs
                == application::git_recovery_ref_topology::ambiguous
            && !refused_mixed_completion.has_value()
            && refused_mixed_completion.error()
                == application::git_cache_failure::repair_required,
        "one-sided final state never authorizes catalog success or cleanup");
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            "-d",
            QString::fromStdString(final_base),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    const auto restored_staging =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    require(
        restored_staging.has_value()
            && restored_staging->Staging
                == application::git_recovery_match::exact
            && restored_staging->Final
                == application::git_recovery_match::absent,
        "restoring exact refs restores deterministic recovery evidence");

    auto wrong_promotion =
        application::git_ref_promotion_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{0},
            .Operation = operation,
            .Scope =
                identity<core::pull_request_authority_scope_id>(
                    0x43U),
            .Ordinal = core::epoch_ordinal{1},
            .Base = *commit,
            .Head = *commit,
            .DurableCommitToken = token(0x64U),
        };
    const auto rejected_promotion = (*primary)->promote_retained(
        *prepared,
        wrong_promotion,
        {});
    require(
        !rejected_promotion.has_value()
            && rejected_promotion.error()
                == application::git_cache_failure::
                    authorization_mismatch,
        "promotion requires exact durable scope authorization");

    auto promotion = wrong_promotion;
    promotion.Scope = primary_request.Authority.Scope;
    auto forged_prepared_fact = *prepared;
    forged_prepared_fact.InstalledIndex
        .Identity.Bytes.back() ^= 0x01U;
    const auto rejected_prepared_fact =
        (*primary)->promote_retained(
            forged_prepared_fact,
            promotion,
            {});
    require(
        application::valid(forged_prepared_fact)
            && !rejected_prepared_fact.has_value()
            && rejected_prepared_fact.error()
                == application::git_cache_failure::
                    authorization_mismatch,
        "promotion revalidates durable installed-artifact facts");
    const auto retained =
        (*primary)->promote_retained(*prepared, promotion, {});
    require(
        retained.has_value() && retained->Base == *commit
            && retained->Head == *commit
            && retained->Ordinal == core::epoch_ordinal{1},
        "paired staging refs atomically promote to final refs");
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(staging_prefix + "/base"),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    make_ref_private(staging_prefix + "/base");
    static_cast<void>(run_fixture_git(
        executable,
        root.Path,
        {
            git_dir,
            "update-ref",
            QString::fromStdString(staging_head),
            QString::fromUtf8(sha1_commit_hex.data()),
        }));
    make_ref_private(staging_head);
    const auto committed_with_residue =
        (*primary)->inspect_operation_recovery(recovery_request, {});
    const auto completed_recovery =
        (*primary)->complete_recovered_operation(recovery_request, {});
    const auto completed_replay =
        (*primary)->complete_recovered_operation(recovery_request, {});
    require(
        committed_with_residue.has_value()
            && committed_with_residue->Staging
                == application::git_recovery_match::exact
            && committed_with_residue->Final
                == application::git_recovery_match::exact
            && committed_with_residue->Refs
                == application::git_recovery_ref_topology::ambiguous
            && completed_recovery.has_value()
            && completed_replay.has_value(),
        "successful cleanup exact-deletes committed staging residue, is idempotent, and never enters abort lifecycle");
    auto operation_after_success = (*primary)->create_pack_sink(
        operation,
        sha1_pack.size(),
        {});
    require(
        operation_after_success.has_value(),
        "successful cleanup leaves its operation identity outside the bounded abort history");
    operation_after_success->reset();
    const auto inspected =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    require(
        inspected.has_value() && *inspected == *retained,
        "retained pair inspection preserves exact stable identity");

    const auto recursive_scan_error_marker =
        root.Path
        / "inject-cache-recursive-scan-terminal-error";
    write_file(recursive_scan_error_marker, "inject");
    const auto recursive_scan_error =
        (*primary)->inspect_retained(
            core::epoch_ordinal{1},
            {});
    std::filesystem::remove(recursive_scan_error_marker);
    const auto recursive_scan_retry =
        (*primary)->inspect_retained(
            core::epoch_ordinal{1},
            {});
    require(
        !recursive_scan_error.has_value()
            && recursive_scan_error.error()
                == application::git_cache_failure::
                    invalid_repository
            && recursive_scan_retry.has_value()
            && *recursive_scan_retry == *retained,
        "terminal recursive inventory errors stay typed and retryable");

    const auto pack_scan_error_marker =
        root.Path
        / "inject-cache-pack-scan-terminal-error";
    write_file(pack_scan_error_marker, "inject");
    const auto pack_scan_error =
        (*primary)->inspect_retained(
            core::epoch_ordinal{1},
            {});
    std::filesystem::remove(pack_scan_error_marker);
    const auto pack_scan_retry =
        (*primary)->inspect_retained(
            core::epoch_ordinal{1},
            {});
    require(
        !pack_scan_error.has_value()
            && pack_scan_error.error()
                == application::git_cache_failure::
                    invalid_repository
            && pack_scan_retry.has_value()
            && *pack_scan_retry == *retained,
        "terminal pack-directory scan errors stay typed and retryable");

    const auto preexisting_prepare_operation =
        identity<core::epoch_operation_id>(0x5cU);
    auto preexisting_prepare_sink =
        (*primary)->create_pack_sink(
            preexisting_prepare_operation,
            sha1_pack.size(),
            {});
    require(
        preexisting_prepare_sink.has_value(),
        "create preexisting-pack durable-fact sink");
    append_pack(**preexisting_prepare_sink, sha1_pack);
    const auto preexisting_prepare_receipt =
        (*preexisting_prepare_sink)->finish({});
    require(
        preexisting_prepare_receipt.has_value(),
        "finish preexisting-pack durable-fact spool");
    preexisting_prepare_sink->reset();
    const auto preexisting_prepare_observation =
        (*primary)->observe_preimport(
            *preexisting_prepare_receipt,
            {});
    require(
        preexisting_prepare_observation.has_value()
            && preexisting_prepare_observation
                ->ExpectedPackPreexisting,
        "observe exact preexisting pack before repeat prepare");
    const auto preexisting_prepared =
        (*primary)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *preexisting_prepare_receipt,
                .Inventory =
                    *preexisting_prepare_observation,
                .Authorization =
                    {
                        .CatalogIncarnation = incarnation,
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{1},
                        .Operation =
                            preexisting_prepare_operation,
                        .ExpectedPack =
                            preexisting_prepare_receipt
                                ->ExpectedPack,
                        .ReceiptToken =
                            preexisting_prepare_receipt
                                ->OpaqueToken,
                        .InventoryToken =
                            preexisting_prepare_observation
                                ->OpaqueToken,
                        .DurableRecordToken = token(0x60U),
                    },
                .Base = *commit,
                .Head = *commit,
                .PriorHead = *commit,
            },
            {});
    require(
        preexisting_prepared.has_value()
            && preexisting_prepared->InstalledPack
                == prepared->InstalledPack
            && preexisting_prepared->InstalledIndex
                == prepared->InstalledIndex
            && preexisting_prepared->HeadLineage
                == core::head_lineage::not_applicable,
        "same-head prepare returns durable facts with not-applicable lineage");
    require(
        (*primary)
            ->abort_operation(
                preexisting_prepare_operation,
                {})
            .has_value(),
        "preexisting durable-fact prepare exact-cleans staging refs");

    const auto unavailable_prior_operation =
        identity<core::epoch_operation_id>(0x69U);
    auto unavailable_prior_sink = (*primary)->create_pack_sink(
        unavailable_prior_operation,
        sha1_pack.size(),
        {});
    require(
        unavailable_prior_sink.has_value(),
        "create unavailable-prior pack sink");
    append_pack(**unavailable_prior_sink, sha1_pack);
    const auto unavailable_prior_receipt =
        (*unavailable_prior_sink)->finish({});
    require(
        unavailable_prior_receipt.has_value(),
        "finish unavailable-prior pack receipt");
    unavailable_prior_sink->reset();
    const auto unavailable_prior_observation =
        (*primary)->observe_preimport(*unavailable_prior_receipt, {});
    const auto unavailable_prior_oid = core::parse_git_commit_oid(
        std::string(40U, 'f'));
    require(
        unavailable_prior_observation.has_value()
            && unavailable_prior_oid.has_value(),
        "observe unavailable-prior pre-import facts");
    const auto unavailable_prior_prepared =
        (*primary)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *unavailable_prior_receipt,
                .Inventory = *unavailable_prior_observation,
                .Authorization =
                    {
                        .CatalogIncarnation = incarnation,
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{1},
                        .Operation = unavailable_prior_operation,
                        .ExpectedPack =
                            unavailable_prior_receipt->ExpectedPack,
                        .ReceiptToken =
                            unavailable_prior_receipt->OpaqueToken,
                        .InventoryToken =
                            unavailable_prior_observation->OpaqueToken,
                        .DurableRecordToken = token(0x69U),
                    },
                .Base = *commit,
                .Head = *commit,
                .PriorHead = *unavailable_prior_oid,
            },
            {});
    require(
        unavailable_prior_prepared.has_value()
            && unavailable_prior_prepared->HeadLineage
                == core::head_lineage::unknown,
        "missing exact prior head yields unknown lineage without failing import");
    require(
        (*primary)
            ->abort_operation(unavailable_prior_operation, {})
            .has_value(),
        "unavailable-prior prepare exact-cleans staging refs");

    const auto cancelled_token = cancelled_source.get_token();
    const auto cache_cancelled = [](const auto& result)
    {
        return !result.has_value()
            && result.error()
                == application::git_cache_failure::cancelled;
    };
    require(
        cache_cancelled(
            (*cache)->open(primary_request, cancelled_token))
            && cache_cancelled(
                (*primary)->create_pack_sink(
                    identity<core::epoch_operation_id>(0x78U),
                    sha1_pack.size(),
                    cancelled_token))
            && cache_cancelled(
                (*primary)->observe_preimport(
                    *receipt,
                    cancelled_token))
            && cache_cancelled(
                (*primary)->prepare_revision(
                    application::git_prepare_revision_request{
                        .Receipt = *receipt,
                        .Inventory = *observation,
                        .Authorization = authorization,
                        .Base = *commit,
                        .Head = *commit,
                    },
                    cancelled_token))
            && cache_cancelled(
                (*primary)->promote_retained(
                    *prepared,
                    promotion,
                    cancelled_token))
            && cache_cancelled(
                (*primary)->inspect_retained(
                    core::epoch_ordinal{1},
                    cancelled_token))
            && cache_cancelled(
                (*primary)->prune_retained(
                    application::git_ref_prune_authorization{
                        .CatalogIncarnation = incarnation,
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{1},
                        .Expected = *retained,
                        .DurableIntentToken = token(0x7cU),
                    },
                    cancelled_token))
            && cache_cancelled(
                (*primary)->abort_operation(
                    identity<core::epoch_operation_id>(0x7dU),
                    cancelled_token))
            && cache_cancelled(
                (*cache)->delete_cache(
                    primary_request,
                    application::git_cache_delete_authorization{
                        .ApplicationSession =
                            primary_request.ApplicationSession,
                        .Cache = primary_request.Authority.Cache,
                        .DurableIntentToken = token(0x7eU),
                    },
                    cancelled_token)),
        "pre-cancelled cache operations all stop before mutation");

    force_push_and_prune(executable, root.Path, force_push);
    const auto cache_git_dir =
        "--git-dir="
        + QString::fromStdString(primary_repository.string());
    const auto retained_presence = run_fixture_git(
        executable,
        root.Path,
        {
            cache_git_dir,
            "cat-file",
            "-e",
            QString::fromStdString(
                std::string{sha1_commit_hex} + "^{commit}"),
        },
        {},
        true);
    require(
        retained_presence.ExitCode == 0,
        "retained old commit remains directly readable after remote prune");

    const auto oversized_operation =
        identity<core::epoch_operation_id>(0x53U);
    auto bounded_sink = (*primary)->create_pack_sink(
        oversized_operation,
        force_push.NewPack.size() - 1U,
        {});
    require(bounded_sink.has_value(), "create operation-bounded pack sink");
    const auto refused_bytes = (*bounded_sink)->append(
        std::as_bytes(std::span{force_push.NewPack}),
        {});
    require(
        !refused_bytes.has_value()
            && refused_bytes.error()
                == application::git_pack_stream_failure::input_limit,
        "operation pack-byte quota fails before disk admission");
    bounded_sink->reset();
    const auto old_after_pack_quota =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    require(
        old_after_pack_quota.has_value()
            && *old_after_pack_quota == *retained,
        "pack-byte quota failure preserves retained history");

    const auto force_operation =
        identity<core::epoch_operation_id>(0x54U);
    auto force_sink = (*primary)->create_pack_sink(
        force_operation,
        force_push.NewPack.size(),
        {});
    require(force_sink.has_value(), "create force-push pack sink");
    append_pack(**force_sink, force_push.NewPack);
    const auto force_receipt = (*force_sink)->finish({});
    require(
        force_receipt.has_value()
            && application::agrees(*force_receipt),
        "validate force-push pack receipt");
    force_sink->reset();
    const auto force_observation =
        (*primary)->observe_preimport(*force_receipt, {});
    require(
        force_observation.has_value()
            && force_observation->Packs.size() == 1U
            && !force_observation->ExpectedPackPreexisting,
        "second import observes the already retained old pack");
    const auto force_authorization =
        application::git_import_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{1},
            .Operation = force_operation,
            .ExpectedPack = force_receipt->ExpectedPack,
            .ReceiptToken = force_receipt->OpaqueToken,
            .InventoryToken = force_observation->OpaqueToken,
            .DurableRecordToken = token(0x6bU),
        };
    const auto force_prepared = (*primary)->prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *force_receipt,
            .Inventory = *force_observation,
            .Authorization = force_authorization,
            .Base = force_push.NewCommit,
            .Head = force_push.NewCommit,
            .PriorHead = *commit,
        },
        {});
    require(
        force_prepared.has_value()
            && force_prepared->BaseTree == force_push.NewTree
            && force_prepared->HeadTree == force_push.NewTree
            && force_prepared->HeadLineage
                == core::head_lineage::non_fast_forward,
        "import force-pushed commit and classify its prior head as non-fast-forward");
    const auto force_retained = (*primary)->promote_retained(
        *force_prepared,
        application::git_ref_promotion_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{1},
            .Operation = force_operation,
            .Scope = primary_request.Authority.Scope,
            .Ordinal = core::epoch_ordinal{2},
            .Base = force_push.NewCommit,
            .Head = force_push.NewCommit,
            .DurableCommitToken = token(0x6cU),
        },
        {});
    require(
        force_retained.has_value(),
        "retain force-pushed revision as a second exact epoch");
    const auto old_after_force_push =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    const auto new_after_force_push =
        (*primary)->inspect_retained(core::epoch_ordinal{2}, {});
    require(
        old_after_force_push.has_value()
            && *old_after_force_push == *retained
            && new_after_force_push.has_value()
            && *new_after_force_push == *force_retained,
        "old head remains resident after fixture remote removal and prune");

    const auto abandoned_operation =
        identity<core::epoch_operation_id>(0x55U);
    auto abandoned_sink = (*primary)->create_pack_sink(
        abandoned_operation,
        sha1_pack.size(),
        {});
    require(abandoned_sink.has_value(), "create abortable operation spool");
    append_pack(**abandoned_sink, sha1_pack);
    const auto abandoned_receipt = (*abandoned_sink)->finish({});
    require(
        abandoned_receipt.has_value(),
        "finish abortable operation spool");
    abandoned_sink->reset();
    const auto preexisting_observation =
        (*primary)->observe_preimport(*abandoned_receipt, {});
    require(
        preexisting_observation.has_value()
            && preexisting_observation->Packs.size() == 2U
            && preexisting_observation->ExpectedPackPreexisting,
        "inventory identifies an exact already-installed pack");
    auto disagreeing_preexisting =
        *preexisting_observation;
    disagreeing_preexisting.ExpectedPackPreexisting = false;
    auto duplicate_inventory =
        *preexisting_observation;
    duplicate_inventory.Packs.insert(
        duplicate_inventory.Packs.begin() + 1,
        duplicate_inventory.Packs.front());
    auto unsorted_inventory =
        *preexisting_observation;
    std::ranges::reverse(unsorted_inventory.Packs);
    require(
        !application::valid(disagreeing_preexisting)
            && !application::valid(duplicate_inventory)
            && !application::valid(unsorted_inventory),
        "inventory validity requires exact preexisting agreement and sorted uniqueness");
    const auto abandoned_path =
        root.Path / "incoming" / "v1"
        / (lower_hex(abandoned_operation.bytes()) + ".pack.part");
    require(
        std::filesystem::is_regular_file(abandoned_path),
        "completed abandoned operation remains exact and recoverable");
    const auto abandoned_recovery =
        application::git_operation_recovery_request{
            .Operation = abandoned_operation,
            .Scope = primary_request.Authority.Scope,
            .Ordinal = core::epoch_ordinal{4},
            .Base = *commit,
            .Head = *commit,
            .Spool =
                application::git_recovery_spool_expectation{
                    .ExpectedPack =
                        abandoned_receipt->ExpectedPack,
                    .CompletedSpool = abandoned_receipt->Spool,
                    .PreimportPacks =
                        preexisting_observation->Packs,
                    .PreimportRepositoryBytes =
                        preexisting_observation->RepositoryBytes,
                    .ExpectedPackPreexisting = true,
                },
        };
    const auto abandoned_observation =
        (*primary)->inspect_operation_recovery(
            abandoned_recovery,
            {});
    require(
        application::valid(abandoned_recovery)
            && abandoned_observation.has_value()
            && abandoned_observation->Spool
                == application::git_recovery_match::exact
            && abandoned_observation->Installed
                == application::git_recovery_match::exact
            && abandoned_observation->Refs
                == application::git_recovery_ref_topology::absent
            && abandoned_observation->Objects
                == application::git_recovery_object_match::exact,
        "durable preexisting-pack facts identify exact abortable spool residue");
    const auto abandoned_original =
        abandoned_path.parent_path() / "abandoned-original.pack";
    std::filesystem::rename(abandoned_path, abandoned_original);
    write_file(abandoned_path, "replacement-canary");
    const auto substituted_abort =
        (*primary)->abort_recovered_operation(
            abandoned_recovery,
            {});
    require(
        !substituted_abort.has_value()
            && substituted_abort.error()
                == application::git_cache_failure::repair_required
            && read_file(abandoned_path)
                == std::vector<std::uint8_t>{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'c', 'a', 'n', 'a',
                    'r', 'y'},
        "abort refuses and preserves a substituted completed spool");
    std::filesystem::remove(abandoned_path);
    std::filesystem::rename(abandoned_original, abandoned_path);
    primary->reset();
    primary = (*cache)->open(primary_request, {});
    require(primary.has_value(), "reopen for exact durable spool abort");
    const auto aborted = (*primary)->abort_recovered_operation(
        abandoned_recovery,
        {});
    require(
        aborted.has_value()
            && !std::filesystem::exists(abandoned_path),
        "abort exact-deletes only its operation residue");
    const auto old_after_abort =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    const auto new_after_abort =
        (*primary)->inspect_retained(core::epoch_ordinal{2}, {});
    require(
        old_after_abort.has_value() && new_after_abort.has_value(),
        "operation abort preserves both retained epochs");

    const auto abort_first_operation =
        identity<core::epoch_operation_id>(0x44U);
    auto abort_first_sink = (*primary)->create_pack_sink(
        abort_first_operation,
        sha1_pack.size(),
        {});
    require(
        abort_first_sink.has_value(),
        "create deterministic abort-first sink");
    append_pack(**abort_first_sink, sha1_pack);
    require(
        (*primary)
            ->abort_operation(abort_first_operation, {})
            .has_value(),
        "abort-first operation reaches a terminal lifecycle");
    const auto finish_after_abort =
        (*abort_first_sink)->finish({});
    const auto abort_first_text =
        lower_hex(abort_first_operation.bytes());
    require(
        !finish_after_abort.has_value()
            && finish_after_abort.error()
                == application::git_pack_stream_failure::
                    invalid_state
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / (abort_first_text + ".pack.part"))
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / ("." + abort_first_text
                    + ".pack.receiving")),
        "finish cannot publish after a linearized abort");
    abort_first_sink->reset();

    const auto racing_operation =
        identity<core::epoch_operation_id>(0x47U);
    auto racing_sink = (*primary)->create_pack_sink(
        racing_operation,
        sha1_pack.size(),
        {});
    require(racing_sink.has_value(), "create finish-versus-abort sink");
    append_pack(**racing_sink, sha1_pack);
    auto race_gate = std::barrier{3};
    auto racing_finish = std::optional<
        application::git_pack_stream_result<
            application::completed_git_pack_receipt>>{};
    auto racing_abort = std::optional<
        application::git_cache_result<void>>{};
    {
        auto finish_thread = std::jthread{
            [&]
            {
                race_gate.arrive_and_wait();
                racing_finish = (*racing_sink)->finish({});
            }};
        auto abort_thread = std::jthread{
            [&]
            {
                race_gate.arrive_and_wait();
                racing_abort =
                    (*primary)->abort_operation(
                        racing_operation,
                        {});
            }};
        race_gate.arrive_and_wait();
    }
    racing_sink->reset();
    const auto racing_text =
        lower_hex(racing_operation.bytes());
    require(
        racing_finish.has_value() && racing_abort.has_value()
            && racing_abort->has_value()
            && (
                racing_finish->has_value()
                || racing_finish->error()
                    == application::git_pack_stream_failure::
                        storage
                || racing_finish->error()
                    == application::git_pack_stream_failure::
                        invalid_state)
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / (racing_text + ".pack.part"))
            && !std::filesystem::exists(
                root.Path / "incoming" / "v1"
                / ("." + racing_text + ".pack.receiving")),
        "operation abort linearizes with finish and cannot resurrect a spool");

    primary->reset();
    auto reuse_limited = primary_request;
    reuse_limited.Limits.OperationPackBytes = sha1_pack.size();
    reuse_limited.Limits.RepositoryBytes =
        preexisting_observation->RepositoryBytes + 82U;
    reuse_limited.Limits.PackCount = 2U;
    reuse_limited.Limits.PrivateRefCount = 6U;
    require(
        application::valid(reuse_limited),
        "valid tight preexisting-pack reuse quota");
    primary = (*cache)->open(reuse_limited, {});
    require(primary.has_value(), "open tight preexisting-pack cache");
    const auto reuse_operation =
        identity<core::epoch_operation_id>(0x56U);
    auto reuse_sink = (*primary)->create_pack_sink(
        reuse_operation,
        sha1_pack.size(),
        {});
    require(reuse_sink.has_value(), "create preexisting-pack reuse sink");
    append_pack(**reuse_sink, sha1_pack);
    const auto reuse_receipt = (*reuse_sink)->finish({});
    require(reuse_receipt.has_value(), "finish preexisting-pack reuse spool");
    reuse_sink->reset();
    const auto reuse_observation =
        (*primary)->observe_preimport(*reuse_receipt, {});
    require(
        reuse_observation.has_value()
            && reuse_observation->ExpectedPackPreexisting,
        "full pack quota still admits exact preexisting pack plus ref headroom");
    const auto reuse_prepared = (*primary)->prepare_revision(
        application::git_prepare_revision_request{
            .Receipt = *reuse_receipt,
            .Inventory = *reuse_observation,
            .Authorization =
                {
                    .CatalogIncarnation = incarnation,
                    .ExpectedCatalogRevision =
                        core::epoch_catalog_revision{2},
                    .Operation = reuse_operation,
                    .ExpectedPack = reuse_receipt->ExpectedPack,
                    .ReceiptToken = reuse_receipt->OpaqueToken,
                    .InventoryToken =
                        reuse_observation->OpaqueToken,
                    .DurableRecordToken = token(0x6eU),
                },
            .Base = *commit,
            .Head = *commit,
        },
        {});
    require(
        reuse_prepared.has_value(),
        "preexisting pack revalidation prepares without pack/index growth");
    const auto reuse_retained = (*primary)->promote_retained(
        *reuse_prepared,
        application::git_ref_promotion_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{2},
            .Operation = reuse_operation,
            .Scope = primary_request.Authority.Scope,
            .Ordinal = core::epoch_ordinal{3},
            .Base = *commit,
            .Head = *commit,
            .DurableCommitToken = token(0x6fU),
        },
        {});
    require(
        reuse_retained.has_value()
            && (*primary)
                ->inspect_retained(core::epoch_ordinal{3}, {})
                .has_value(),
        "preexisting pack reuse retains a third exact pair at the quota edge");

    primary->reset();
    auto repository_limited = primary_request;
    repository_limited.Limits.OperationPackBytes =
        std::max<std::uint64_t>(
            sha1_pack.size(),
            force_push.NewPack.size());
    repository_limited.Limits.RepositoryBytes =
        repository_limited.Limits.OperationPackBytes;
    require(
        application::valid(repository_limited),
        "valid tight repository quota request");
    auto repository_quota =
        (*cache)->open(repository_limited, {});
    require(
        repository_quota.has_value()
            && (*repository_quota)
                ->inspect_retained(core::epoch_ordinal{1}, {})
                .has_value(),
        "oversized cache remains open for inspection and reduction");
    const auto repository_addition =
        (*repository_quota)->create_pack_sink(
            identity<core::epoch_operation_id>(0x79U),
            sha1_pack.size(),
            {});
    require(
        !repository_addition.has_value()
            && repository_addition.error()
                == application::git_cache_failure::quota_exceeded,
        "hard repository-byte quota rejects additive work");
    repository_quota->reset();

    auto pack_count_limited = primary_request;
    pack_count_limited.Limits.PackCount = 1U;
    auto pack_count_quota =
        (*cache)->open(pack_count_limited, {});
    require(
        pack_count_quota.has_value()
            && (*pack_count_quota)
                ->inspect_retained(core::epoch_ordinal{1}, {})
                .has_value(),
        "over-pack-count cache remains open for explicit recovery");
    const auto pack_count_addition =
        (*pack_count_quota)->create_pack_sink(
            identity<core::epoch_operation_id>(0x7aU),
            sha1_pack.size(),
            {});
    require(
        !pack_count_addition.has_value()
            && pack_count_addition.error()
                == application::git_cache_failure::quota_exceeded,
        "hard pack-count quota rejects additive work");
    pack_count_quota->reset();

    auto ref_limited = primary_request;
    ref_limited.Limits.PrivateRefCount = 2U;
    auto ref_quota = (*cache)->open(ref_limited, {});
    require(
        ref_quota.has_value()
            && (*ref_quota)
                ->inspect_retained(core::epoch_ordinal{1}, {})
                .has_value(),
        "over-ref-count cache remains open for explicit pruning");
    const auto ref_addition = (*ref_quota)->create_pack_sink(
        identity<core::epoch_operation_id>(0x7bU),
        sha1_pack.size(),
        {});
    require(
        !ref_addition.has_value()
            && ref_addition.error()
                == application::git_cache_failure::quota_exceeded,
        "hard private-ref quota rejects additive work");
    ref_quota->reset();

    primary = (*cache)->open(primary_request, {});
    require(primary.has_value(), "reopen after quota failures");
    const auto old_after_quotas =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    const auto new_after_quotas =
        (*primary)->inspect_retained(core::epoch_ordinal{2}, {});
    require(
        old_after_quotas.has_value()
            && *old_after_quotas == *retained
            && new_after_quotas.has_value()
            && *new_after_quotas == *force_retained,
        "quota failures never prune or remap retained history");

    const auto packed_refs = primary_repository / "packed-refs";
    write_file(packed_refs, "");
    const auto drifted_prune = (*primary)->prune_retained(
        application::git_ref_prune_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{2},
            .Expected = *retained,
            .DurableIntentToken = token(0x70U),
        },
        {});
    require(
        !drifted_prune.has_value()
            && drifted_prune.error()
                == application::git_cache_failure::
                    invalid_repository,
        "post-open packed-ref drift blocks prune before Git execution");
    std::filesystem::remove(packed_refs);

    auto wrong_pair = *retained;
    auto wrong_bytes = wrong_pair.Head.bytes();
    wrong_bytes[0] ^= 0x01U;
    wrong_pair.Head = core::git_commit_oid{
        core::git_hash_algorithm::sha1,
        wrong_bytes};
    const auto rejected_prune = (*primary)->prune_retained(
        application::git_ref_prune_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{1},
            .Expected = wrong_pair,
            .DurableIntentToken = token(0x65U),
        },
        {});
    require(
        !rejected_prune.has_value()
            && rejected_prune.error()
                == application::git_cache_failure::ref_conflict,
        "prune refuses an unexpected retained object");

    const auto pruned = (*primary)->prune_retained(
        application::git_ref_prune_authorization{
            .CatalogIncarnation = incarnation,
            .ExpectedCatalogRevision =
                core::epoch_catalog_revision{1},
            .Expected = *retained,
            .DurableIntentToken = token(0x66U),
        },
        {});
    require(pruned.has_value(), "exact paired prune succeeds");
    const auto absent =
        (*primary)->inspect_retained(core::epoch_ordinal{1}, {});
    require(
        !absent.has_value()
            && absent.error()
                == application::git_cache_failure::ref_conflict,
        "pruned retained pair is absent without remapping");

    const auto locked_delete = (*cache)->delete_cache(
        primary_request,
        application::git_cache_delete_authorization{
            .ApplicationSession =
                primary_request.ApplicationSession,
            .Cache = primary_request.Authority.Cache,
            .DurableIntentToken = token(0x67U),
        },
        {});
    require(
        !locked_delete.has_value()
            && locked_delete.error()
                == application::git_cache_failure::lock_unavailable,
        "whole-cache deletion obeys the exact writer lock");

    primary->reset();
    auto reused = (*cache)->open(primary_request, {});
    require(reused.has_value(), "exact manifest cache is reusable");
    reused->reset();

    auto mismatched_request = primary_request;
    mismatched_request.Authority.ProfileRevision =
        core::github_profile_revision{99U};
    const auto mismatched = (*cache)->open(mismatched_request, {});
    require(
        !mismatched.has_value()
            && mismatched.error()
                == application::git_cache_failure::invalid_manifest,
        "same opaque cache scope rejects profile-revision drift");

    auto account_mismatch = primary_request;
    account_mismatch.Authority.Account =
        core::github_account_id{100U};
    const auto mismatched_account =
        (*cache)->open(account_mismatch, {});
    require(
        !mismatched_account.has_value()
            && mismatched_account.error()
                == application::git_cache_failure::invalid_manifest,
        "same opaque cache scope rejects account drift");

    const auto expect_topology_rejection =
        [&](const std::string_view message)
    {
        const auto rejected = (*cache)->open(primary_request, {});
        require(
            !rejected.has_value()
                && rejected.error()
                    == application::git_cache_failure::
                        invalid_manifest,
            message);
    };

    const auto config = primary_repository / "config";
    const auto original_config = read_file(config);
    auto remote_config = original_config;
    constexpr std::string_view forbidden_remote =
        "[remote \"origin\"]\n"
        "\turl = https://credential-canary.invalid/repository.git\n";
    remote_config.insert(
        remote_config.end(),
        forbidden_remote.begin(),
        forbidden_remote.end());
    write_file(config, remote_config);
    expect_topology_rejection(
        "cache rejects configuration and remote drift");
    write_file(config, original_config);

    const auto alternates =
        primary_repository / "objects" / "info" / "alternates";
    write_file(alternates, "../escape\n");
    expect_topology_rejection("cache rejects alternate-object drift");
    std::filesystem::remove(alternates);

    const auto shallow = primary_repository / "shallow";
    write_file(shallow, std::string{sha1_commit_hex} + "\n");
    expect_topology_rejection("cache rejects shallow-state drift");
    std::filesystem::remove(shallow);

    const auto info = primary_repository / "info";
    std::filesystem::create_directory(info);
    std::filesystem::permissions(
        info,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto grafts = info / "grafts";
    write_file(grafts, std::string{sha1_commit_hex} + "\n");
    expect_topology_rejection("cache rejects graft drift");
    std::filesystem::remove(grafts);
    std::filesystem::remove(info);

    const auto worktrees = primary_repository / "worktrees";
    std::filesystem::create_directory(worktrees);
    std::filesystem::permissions(
        worktrees,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    expect_topology_rejection("cache rejects worktree drift");
    std::filesystem::remove(worktrees);

    std::filesystem::permissions(
        manifest,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::owner_write
            | std::filesystem::perms::group_read,
        std::filesystem::perm_options::replace);
    expect_topology_rejection(
        "cache rejects private-permission drift");
    std::filesystem::permissions(
        manifest,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);

    const auto manifest_backup =
        primary_repository / "manifest-backup";
    std::filesystem::rename(manifest, manifest_backup);
    std::filesystem::create_hard_link(manifest_backup, manifest);
    expect_topology_rejection(
        "cache rejects hardlinked manifest drift");
    std::filesystem::remove(manifest);
    std::filesystem::rename(manifest_backup, manifest);

    std::filesystem::rename(manifest, manifest_backup);
    std::filesystem::create_symlink(manifest_backup, manifest);
    const auto followed = (*cache)->open(primary_request, {});
    require(
        !followed.has_value()
            && followed.error()
                == application::git_cache_failure::invalid_manifest,
        "manifest symlink is never followed");
    std::filesystem::remove(manifest);
    std::filesystem::rename(manifest_backup, manifest);

    auto sha256_request = request(0x35U, 0x45U);
    sha256_request.ObjectFormat = core::git_hash_algorithm::sha256;
    require(
        application::valid(sha256_request),
        "valid SHA-256 cache authority");
    auto sha256_cache = (*cache)->open(sha256_request, {});
    require(sha256_cache.has_value(), "create SHA-256 bare cache");
    const auto sha256_operation =
        identity<core::epoch_operation_id>(0x57U);
    auto sha256_sink = (*sha256_cache)->create_pack_sink(
        sha256_operation,
        sha256_fixture.NewPack.size(),
        {});
    require(sha256_sink.has_value(), "create SHA-256 pack sink");
    append_pack(**sha256_sink, sha256_fixture.NewPack);
    const auto sha256_receipt = (*sha256_sink)->finish({});
    require(
        sha256_receipt.has_value()
            && sha256_receipt->ExpectedPack.Algorithm
                == core::git_hash_algorithm::sha256,
        "validate SHA-256 pack checksum and opaque receipt");
    sha256_sink->reset();
    const auto sha256_observation =
        (*sha256_cache)->observe_preimport(*sha256_receipt, {});
    require(
        sha256_observation.has_value()
            && !sha256_observation->ExpectedPackPreexisting,
        "observe empty SHA-256 pack inventory");
    const auto sha256_incarnation =
        identity<core::epoch_catalog_incarnation_id>(0x72U);
    const auto sha256_prepared =
        (*sha256_cache)->prepare_revision(
            application::git_prepare_revision_request{
                .Receipt = *sha256_receipt,
                .Inventory = *sha256_observation,
                .Authorization =
                    {
                        .CatalogIncarnation =
                            sha256_incarnation,
                        .ExpectedCatalogRevision =
                            core::epoch_catalog_revision{0},
                        .Operation = sha256_operation,
                        .ExpectedPack =
                            sha256_receipt->ExpectedPack,
                        .ReceiptToken =
                            sha256_receipt->OpaqueToken,
                        .InventoryToken =
                            sha256_observation->OpaqueToken,
                        .DurableRecordToken = token(0x73U),
                    },
                .Base = sha256_fixture.NewCommit,
                .Head = sha256_fixture.NewCommit,
            },
            {});
    require(
        sha256_prepared.has_value()
            && sha256_prepared->BaseTree
                == sha256_fixture.NewTree
            && sha256_prepared->HeadTree
                == sha256_fixture.NewTree,
        "SHA-256 import verifies full commit connectivity and trees");
    const auto sha256_retained =
        (*sha256_cache)->promote_retained(
            *sha256_prepared,
            application::git_ref_promotion_authorization{
                .CatalogIncarnation = sha256_incarnation,
                .ExpectedCatalogRevision =
                    core::epoch_catalog_revision{0},
                .Operation = sha256_operation,
                .Scope = sha256_request.Authority.Scope,
                .Ordinal = core::epoch_ordinal{1},
                .Base = sha256_fixture.NewCommit,
                .Head = sha256_fixture.NewCommit,
                .DurableCommitToken = token(0x74U),
            },
            {});
    require(
        sha256_retained.has_value()
            && (*sha256_cache)
                ->inspect_retained(core::epoch_ordinal{1}, {})
                .has_value(),
        "SHA-256 retained refs preserve exact 64-digit identities");
    sha256_cache->reset();
    require(
        (*cache)
            ->delete_cache(
                sha256_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        sha256_request.ApplicationSession,
                    .Cache = sha256_request.Authority.Cache,
                    .DurableIntentToken = token(0x75U),
                },
                {})
            .has_value(),
        "SHA-256 cache exact deletion succeeds");

    const auto wrong_delete = (*cache)->delete_cache(
        primary_request,
        application::git_cache_delete_authorization{
            .ApplicationSession =
                primary_request.ApplicationSession,
            .Cache =
                identity<core::repository_cache_scope_id>(0x77U),
            .DurableIntentToken = token(0x68U),
        },
        {});
    require(
        !wrong_delete.has_value()
            && wrong_delete.error()
                == application::git_cache_failure::
                    authorization_mismatch,
        "whole-cache deletion requires exact scope authorization");

    const auto collision_token = token(0x6dU);
    const auto quarantine_root =
        root.Path / "quarantine" / "v1";
    std::filesystem::create_directories(quarantine_root);
    std::filesystem::permissions(
        root.Path / "quarantine",
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(
        quarantine_root,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto collision =
        quarantine_root
        / (lower_hex(primary_request.Authority.Cache.bytes())
            + "-" + lower_hex(collision_token) + ".git");
    std::filesystem::create_directory_symlink(
        isolated_repository,
        collision);
    const auto collision_delete = (*cache)->delete_cache(
        primary_request,
        application::git_cache_delete_authorization{
            .ApplicationSession =
                primary_request.ApplicationSession,
            .Cache = primary_request.Authority.Cache,
            .DurableIntentToken = collision_token,
        },
        {});
    require(
        !collision_delete.has_value()
            && collision_delete.error()
                == application::git_cache_failure::repair_required
            && std::filesystem::is_directory(primary_repository)
            && std::filesystem::is_directory(isolated_repository),
        "cache deletion refuses a quarantine collision without escaping");
    std::filesystem::remove(collision);

    const auto primary_delete_authorization =
        application::git_cache_delete_authorization{
            .ApplicationSession =
                primary_request.ApplicationSession,
            .Cache = primary_request.Authority.Cache,
            .DurableIntentToken = token(0x69U),
        };
    const auto deleted = (*cache)->delete_cache(
        primary_request,
        primary_delete_authorization,
        {});
    const auto primary_tombstone =
        quarantine_root
        / (lower_hex(primary_request.Authority.Cache.bytes())
            + ".deleted");
    require(
        deleted.has_value() && !std::filesystem::exists(primary_repository)
            && std::filesystem::is_regular_file(primary_tombstone)
            && std::filesystem::is_directory(isolated_repository),
        "exact cache deletion leaves its authority-bound terminal tombstone");
    require(
        (*cache)
            ->delete_cache(
                primary_request,
                primary_delete_authorization,
                {})
            .has_value(),
        "same delete intent is idempotent after terminal cleanup");
    const auto different_delete = (*cache)->delete_cache(
        primary_request,
        application::git_cache_delete_authorization{
            .ApplicationSession =
                primary_request.ApplicationSession,
            .Cache = primary_request.Authority.Cache,
            .DurableIntentToken = token(0x71U),
        },
        {});
    require(
        !different_delete.has_value()
            && different_delete.error()
                == application::git_cache_failure::
                    authorization_mismatch,
        "terminal tombstone rejects a different delete intent");
    const auto deleted_reopen = (*cache)->open(primary_request, {});
    require(
        !deleted_reopen.has_value()
            && deleted_reopen.error()
                == application::git_cache_failure::repair_required,
        "terminal tombstone prevents deleted cache-scope reuse");

    isolated->reset();
    const auto isolated_delete_token = token(0x6aU);
    const auto isolated_quarantine =
        quarantine_root
        / (lower_hex(isolated_request.Authority.Cache.bytes())
            + "-" + lower_hex(isolated_delete_token) + ".git");
    const auto isolated_tombstone =
        quarantine_root
        / (lower_hex(isolated_request.Authority.Cache.bytes())
            + ".deleted");
    write_file(
        isolated_tombstone,
        deletion_tombstone(
            isolated_request,
            isolated_delete_token,
            isolated_repository,
            root.Path / "locks" / "v1"
                / (lower_hex(
                       isolated_request.Authority.Cache.bytes())
                    + ".lock")));
    std::filesystem::rename(
        isolated_repository,
        isolated_quarantine);
    const auto displaced_isolated =
        quarantine_root / "displaced-isolated-cache.git";
    std::filesystem::rename(
        isolated_quarantine,
        displaced_isolated);
    std::filesystem::create_directory(isolated_quarantine);
    std::filesystem::permissions(
        isolated_quarantine,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    write_file(
        isolated_quarantine / "replacement-canary",
        "replacement-quarantine");
    const auto substituted_quarantine_delete =
        (*cache)->delete_cache(
            isolated_request,
            application::git_cache_delete_authorization{
                .ApplicationSession =
                    isolated_request.ApplicationSession,
                .Cache = isolated_request.Authority.Cache,
                .DurableIntentToken = isolated_delete_token,
            },
            {});
    require(
        !substituted_quarantine_delete.has_value()
            && substituted_quarantine_delete.error()
                == application::git_cache_failure::
                    repair_required
            && read_file(
                isolated_quarantine
                    / "replacement-canary")
                == std::vector<std::uint8_t>{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'q', 'u', 'a', 'r',
                    'a', 'n', 't', 'i', 'n', 'e'},
        "tombstone retry refuses and preserves a substituted quarantine");
    std::filesystem::remove_all(isolated_quarantine);
    std::filesystem::rename(
        displaced_isolated,
        isolated_quarantine);
    std::filesystem::remove(isolated_quarantine / "config");
    const auto isolated_deleted = (*cache)->delete_cache(
        isolated_request,
        application::git_cache_delete_authorization{
            .ApplicationSession =
                isolated_request.ApplicationSession,
            .Cache = isolated_request.Authority.Cache,
            .DurableIntentToken = isolated_delete_token,
        },
        {});
    require(
        isolated_deleted.has_value()
            && !std::filesystem::exists(isolated_repository)
            && !std::filesystem::exists(isolated_quarantine)
            && std::filesystem::is_regular_file(
                quarantine_root
                / (lower_hex(
                       isolated_request.Authority.Cache.bytes())
                    + ".deleted")),
        "owned tombstone makes interrupted quarantine cleanup retryable");
    require(
        (*cache)
            ->delete_cache(
                isolated_request,
                application::git_cache_delete_authorization{
                    .ApplicationSession =
                        isolated_request.ApplicationSession,
                    .Cache =
                        isolated_request.Authority.Cache,
                    .DurableIntentToken =
                        isolated_delete_token,
                },
                {})
            .has_value(),
        "completed interrupted deletion is idempotent");
}
}

auto main(int argc, char** argv) -> int
{
    QCoreApplication application{argc, argv};
    if(
        argc == 8
        && QString::fromUtf8(argv[1]) == "--hold-cache-lock")
    {
        bool cache_ok = false;
        bool scope_ok = false;
        const auto cache_byte =
            QString::fromUtf8(argv[4]).toUInt(&cache_ok);
        const auto scope_byte =
            QString::fromUtf8(argv[5]).toUInt(&scope_ok);
        if(
            !cache_ok || !scope_ok || cache_byte > 0xffU
            || scope_byte > 0xffU)
        {
            return 19;
        }
        return run_lock_holder(
            std::filesystem::path{argv[2]},
            std::filesystem::path{argv[3]},
            static_cast<std::uint8_t>(cache_byte),
            static_cast<std::uint8_t>(scope_byte),
            std::filesystem::path{argv[6]},
            std::filesystem::path{argv[7]});
    }
    run_cache_contract();
    std::cout << "Git cache contract passed\n";
    return 0;
}
