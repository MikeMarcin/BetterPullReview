#include "bpr/adapters/git/installed_git_process.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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

namespace
{
namespace application = bpr::application;
namespace git_adapter = bpr::adapters::git;

inline constexpr std::string_view sha1_commit_hex =
    "826bb464b84352d60b3d7c56481a599a838d998d";
inline constexpr std::string_view sha1_tree_hex =
    "4ce9ffbf7ccd31debb0a343022131f0fde64e35d";
inline constexpr std::string_view sha1_pack_hex =
    "e248b08f09a75116fdb2397c928a84f6a93f6c34";
inline constexpr std::string_view sha256_tree_hex =
    "d1b1a86a6b5182208ff0345ff6f64d9c48b9d058e0e8a88fbaefcf192384aa8d";
inline constexpr std::string_view sha256_pack_hex =
    "3e3468388a66c6475d2c1fdb8fb7286bc0ae1f934b6352358cc98e4e5e823b69";

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
    explicit temporary_root(const std::string_view label)
    {
        const auto seed = std::to_string(
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count());
        Path = std::filesystem::temp_directory_path()
            / ("bpr-" + std::string{label} + "-" + seed);
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

class scoped_environment final
{
public:
    scoped_environment() = default;
    scoped_environment(const scoped_environment&) = delete;
    auto operator=(const scoped_environment&)
        -> scoped_environment& = delete;

    ~scoped_environment()
    {
        for(auto entry = Saved.rbegin(); entry != Saved.rend(); ++entry)
        {
            if(entry->WasSet)
            {
                static_cast<void>(
                    qputenv(entry->Name.constData(), entry->Value));
            }
            else
            {
                static_cast<void>(
                    qunsetenv(entry->Name.constData()));
            }
        }
    }

    auto set(
        const char* name,
        const QByteArray& value) -> void
    {
        Saved.push_back({
            .Name = QByteArray{name},
            .WasSet = qEnvironmentVariableIsSet(name),
            .Value = qgetenv(name),
        });
        require(qputenv(name, value), "set hostile parent environment");
    }

    auto set(
        const char* name,
        const std::filesystem::path& value) -> void
    {
        set(name, QByteArray::fromStdString(value.string()));
    }

private:
    struct saved_value
    {
        QByteArray Name;
        bool WasSet = false;
        QByteArray Value;
    };

    std::vector<saved_value> Saved;
};

auto write_bytes(
    const std::filesystem::path& path,
    const std::span<const std::byte> bytes) -> void
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    require(static_cast<bool>(output), "open fixture file");
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(static_cast<bool>(output), "write fixture file");
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
}

auto write_text(
    const std::filesystem::path& path,
    const std::string_view text) -> void
{
    write_bytes(path, std::as_bytes(std::span{text}));
}

auto write_executable_canary(
    const std::filesystem::path& path,
    const std::filesystem::path& marker,
    const std::string_view label) -> void
{
#if defined(Q_OS_UNIX)
    write_text(
        path,
        "#!/bin/sh\n"
        "printf '%s\\n' '" + std::string{label}
            + "' >> '" + marker.string() + "'\n"
        "exit 91\n");
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
#else
    static_cast<void>(path);
    static_cast<void>(marker);
    static_cast<void>(label);
#endif
}

[[nodiscard]] auto read_text(
    const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto make_operation(
    const std::uint8_t value) -> application::git_operation_identity
{
    application::git_operation_identity result{};
    result.Bytes.fill(value);
    return result;
}

[[nodiscard]] auto make_scope(
    const std::uint8_t value)
    -> application::git_pull_request_scope_identity
{
    application::git_pull_request_scope_identity result{};
    result.Bytes.fill(value);
    return result;
}

[[nodiscard]] constexpr auto nibble(
    const char value) -> std::uint8_t
{
    return value <= '9'
        ? static_cast<std::uint8_t>(value - '0')
        : static_cast<std::uint8_t>(value - 'a' + 10);
}

template<typename Object>
[[nodiscard]] auto object(
    const std::string_view text,
    const application::git_hash_algorithm algorithm) -> Object
{
    Object result{.Algorithm = algorithm};
    for(std::size_t index = 0; index < text.size() / 2U; ++index)
    {
        result.Digest[index] = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(nibble(text[index * 2U]) << 4U)
            | nibble(text[index * 2U + 1U]));
    }
    require(application::valid(result), "valid fixture object");
    return result;
}

[[nodiscard]] auto repository_path(
    const std::filesystem::path& root,
    const char digit) -> std::filesystem::path
{
    return root / "repositories" / "v1"
        / (std::string(64U, digit) + ".git");
}

auto fake_log_invocation(const QStringList& arguments) -> void
{
    const auto home = qEnvironmentVariable("HOME");
    if(home.isEmpty())
    {
        return;
    }
    const auto log =
        std::filesystem::path{home.toStdString()}.parent_path()
        / "invocations.log";
    std::ofstream output{log, std::ios::app};
    output << "BEGIN\n";
    output << "cwd=" << QDir::currentPath().toStdString() << '\n';
    for(const auto& argument : arguments)
    {
        output << "arg=" << argument.toStdString() << '\n';
    }
    auto environment = QProcessEnvironment::systemEnvironment();
    auto names = environment.keys();
    std::sort(names.begin(), names.end());
    for(const auto& name : names)
    {
        output << "env=" << name.toStdString() << "="
               << environment.value(name).toStdString() << '\n';
    }
    output << "END\n";
}

[[nodiscard]] auto fake_repository(
    const QStringList& arguments) -> std::filesystem::path
{
    for(const auto& argument : arguments)
    {
        if(argument.startsWith("--git-dir="))
        {
            return {
                argument.sliced(
                    static_cast<qsizetype>(
                        std::string_view{"--git-dir="}.size()))
                    .toStdString()};
        }
    }
    return {};
}

[[nodiscard]] auto read_standard_input() -> std::string
{
    std::string result;
    std::array<char, 4096U> buffer{};
    for(;;)
    {
        std::cin.read(
            buffer.data(),
            static_cast<std::streamsize>(buffer.size()));
        const auto count = std::cin.gcount();
        if(count > 0)
        {
            result.append(
                buffer.data(),
                static_cast<std::size_t>(count));
            if(result.starts_with("BPR_SLOW_INPUT:"))
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds{1});
            }
        }
        if(count != static_cast<std::streamsize>(buffer.size()))
        {
            break;
        }
    }
    return result;
}

[[nodiscard]] auto split_nul(
    const std::string_view input) -> std::vector<std::string>
{
    std::vector<std::string> fields;
    std::size_t offset = 0U;
    while(offset < input.size())
    {
        const auto end = input.find('\0', offset);
        if(end == std::string_view::npos)
        {
            break;
        }
        fields.emplace_back(input.substr(offset, end - offset));
        offset = end + 1U;
    }
    return fields;
}

[[nodiscard]] auto fake_refs(
    const std::filesystem::path& repository)
    -> std::map<std::string, std::string>
{
    std::map<std::string, std::string> refs;
    std::ifstream input{repository / "fake-refs"};
    std::string ref;
    std::string oid;
    while(input >> ref >> oid)
    {
        refs.emplace(std::move(ref), std::move(oid));
    }
    return refs;
}

auto write_fake_refs(
    const std::filesystem::path& repository,
    const std::map<std::string, std::string>& refs) -> void
{
    std::ofstream output{
        repository / "fake-refs",
        std::ios::trunc};
    for(const auto& [ref, oid] : refs)
    {
        output << ref << ' ' << oid << '\n';
    }
}

[[nodiscard]] auto fake_update_refs(
    const std::filesystem::path& repository,
    const std::string_view input) -> int
{
    auto refs = fake_refs(repository);
    const auto fields = split_nul(input);
    if(fields.empty() || fields.front() != "start")
    {
        return 2;
    }
    for(std::size_t index = 1U; index < fields.size();)
    {
        if(fields[index] == "prepare" || fields[index] == "commit")
        {
            ++index;
            continue;
        }
        const auto separator = fields[index].find(' ');
        if(separator == std::string::npos)
        {
            return 2;
        }
        const auto verb = fields[index].substr(0U, separator);
        const auto ref = fields[index].substr(separator + 1U);
        if(verb == "create")
        {
            if(index + 1U >= fields.size() || refs.contains(ref))
            {
                return 3;
            }
            refs[ref] = fields[index + 1U];
            index += 2U;
        }
        else if(verb == "update")
        {
            if(
                index + 2U >= fields.size()
                || !refs.contains(ref)
                || refs[ref] != fields[index + 2U])
            {
                return 3;
            }
            refs[ref] = fields[index + 1U];
            index += 3U;
        }
        else if(verb == "delete")
        {
            if(
                index + 1U >= fields.size()
                || !refs.contains(ref)
                || refs[ref] != fields[index + 1U])
            {
                return 3;
            }
            refs.erase(ref);
            index += 2U;
        }
        else
        {
            return 2;
        }
    }
    write_fake_refs(repository, refs);
    std::cout << "start: ok\nprepare: ok\ncommit: ok\n";
    return 0;
}

[[nodiscard]] auto fake_git(
    const QStringList& arguments,
    const std::filesystem::path& executable) -> int
{
    fake_log_invocation(arguments);
    if(arguments == QStringList{"--exec-path"})
    {
        std::cout
            << executable.parent_path().string()
            << '\n';
        return 0;
    }
    if(arguments == QStringList{"--version"})
    {
        std::cout << "git version 2.50.1\n";
        return 0;
    }
    if(arguments.contains("init"))
    {
        const auto repository =
            std::filesystem::path{arguments.back().toStdString()};
        std::filesystem::create_directories(repository / "objects");
        std::filesystem::create_directories(repository / "refs");
        std::filesystem::permissions(
            repository,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
        write_text(repository / "config", "");
        write_text(repository / "HEAD", "");
        return 0;
    }

    const auto repository = fake_repository(arguments);
    if(arguments.contains("index-pack"))
    {
        const auto input = read_standard_input();
        if(input.starts_with("BPR_BIG_OUTPUT"))
        {
            std::cout << std::string(2048U, 'x');
            return 0;
        }
        if(input.starts_with("BPR_BIG_ERROR"))
        {
            std::cerr << std::string(20U * 1024U, 'x');
            return 0;
        }
        if(input.starts_with("BPR_MALFORMED"))
        {
            std::cout << "pack\tinvalid\n";
            return 0;
        }
        if(input.starts_with("BPR_BLOCK"))
        {
#if defined(Q_OS_UNIX)
            const auto child = ::fork();
            if(child == 0)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds{1500});
                const auto marker =
                    std::filesystem::path{
                        qEnvironmentVariable("HOME").toStdString()}
                        .parent_path()
                    / "descendant-survived";
                write_text(marker, "survived");
                ::_exit(0);
            }
#endif
            std::this_thread::sleep_for(std::chrono::seconds{20});
            return 0;
        }
        if(input.starts_with("BPR_SLOW_INPUT:"))
        {
            const auto newline = input.find('\n');
            if(newline == std::string::npos)
            {
                return 4;
            }
            const auto expected = static_cast<std::size_t>(
                std::stoull(input.substr(15U, newline - 15U)));
            if(expected != input.size())
            {
                return 4;
            }
        }
        const auto sha256 =
            read_text(repository / "config").find("sha256")
            != std::string::npos;
        const auto pack_identity =
            sha256 ? sha256_pack_hex : sha1_pack_hex;
        const auto pack_directory =
            repository / "objects" / "pack";
        std::filesystem::create_directories(pack_directory);
        std::filesystem::permissions(
            pack_directory,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
        write_text(
            pack_directory
                / ("pack-" + std::string{pack_identity} + ".pack"),
            input);
        write_text(
            pack_directory
                / ("pack-" + std::string{pack_identity} + ".idx"),
            "fixture-index");
        std::cout << "pack\t"
                  << pack_identity
                  << '\n';
        return 0;
    }
    if(arguments.contains("cat-file"))
    {
        static_cast<void>(read_standard_input());
        return 0;
    }
    if(arguments.contains("rev-parse"))
    {
        const auto sha256 =
            read_text(repository / "config").find("sha256")
            != std::string::npos;
        std::cout << (sha256 ? sha256_tree_hex : sha1_tree_hex)
                  << '\n';
        return 0;
    }
    if(arguments.contains("merge-base"))
    {
        return 0;
    }
    if(arguments.contains("update-ref"))
    {
        return fake_update_refs(
            repository,
            read_standard_input());
    }
    if(arguments.contains("show-ref"))
    {
        const auto refs = fake_refs(repository);
        const auto ref = arguments.back().toStdString();
        const auto found = refs.find(ref);
        if(found == refs.end())
        {
            return 1;
        }
        if(!arguments.contains("--quiet"))
        {
            std::cout << found->second << '\n';
        }
        return 0;
    }
    return 9;
}

[[nodiscard]] auto make_adapter(
    const std::filesystem::path& executable,
    const std::filesystem::path& root)
    -> std::unique_ptr<application::git_process_port>
{
    auto made = git_adapter::make_installed_git_process(
        {
            .GitExecutable = std::filesystem::canonical(executable),
            .IsolationRoot = std::filesystem::canonical(root),
        });
    if(!made.has_value())
    {
        const auto message =
            "construct installed Git adapter, failure="
            + std::to_string(static_cast<unsigned int>(made.error()));
        require(false, message);
    }
    return std::move(*made);
}

auto initialize_repository(
    application::git_process_port& adapter,
    const std::filesystem::path& repository,
    const application::git_hash_algorithm algorithm) -> void
{
    const auto initialized = adapter.initialize_bare_repository(
        {.Repository = repository, .Algorithm = algorithm},
        {});
    require(initialized.has_value(), "initialize app-owned bare repository");
}

auto write_spool(
    application::git_process_port& adapter,
    const application::git_operation_identity& operation,
    const std::span<const std::byte> bytes) -> void
{
    const auto path = adapter.pack_spool_path(operation);
    require(path.has_value(), "derive exact operation spool");
    write_bytes(*path, bytes);
}

auto run_real_git_contract() -> void
{
    temporary_root root{"git-real"};
    auto adapter = make_adapter(
        std::filesystem::path{BPR_TEST_GIT_EXECUTABLE},
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
    const auto spool_path = adapter->pack_spool_path(operation);
    require(
        spool_path.has_value()
            && *spool_path
                == root.Path / "incoming" / "v1"
                    / (std::string(64U, '2') + ".pack.part"),
        "spool uses exact incoming/v1 operation pack.part grammar");
    write_spool(
        *adapter,
        operation,
        std::as_bytes(std::span{sha1_pack}));
    const auto pack =
        object<application::git_pack_id>(
            sha1_pack_hex,
            application::git_hash_algorithm::sha1);
    const auto commit =
        object<application::git_commit_id>(
            sha1_commit_hex,
            application::git_hash_algorithm::sha1);
    const auto tree =
        object<application::git_tree_id>(
            sha1_tree_hex,
            application::git_hash_algorithm::sha1);
    const auto indexed = adapter->index_pack(
        {
            .Repository = repository,
            .Operation = operation,
            .SpoolBytes = sha1_pack.size(),
            .MaximumInputBytes = sha1_pack.size(),
            .ExpectedPack = pack,
        },
        {});
    require(indexed.has_value() && indexed->Pack == pack, "strict pack import");
    const auto installed_pack =
        repository / "objects" / "pack"
        / ("pack-" + std::string{sha1_pack_hex} + ".pack");
    const auto installed_index =
        repository / "objects" / "pack"
        / ("pack-" + std::string{sha1_pack_hex} + ".idx");
    const auto reverse_index =
        repository / "objects" / "pack"
        / ("pack-" + std::string{sha1_pack_hex} + ".rev");
    const auto installed_bytes = read_text(installed_pack);
    require(
        installed_bytes.size() == sha1_pack.size()
            && std::equal(
                installed_bytes.begin(),
                installed_bytes.end(),
                reinterpret_cast<const char*>(sha1_pack.data())),
        "installed pack is the exact request spool");
    require(
        std::filesystem::is_regular_file(installed_index)
            && std::filesystem::file_size(installed_index) > 0U,
        "matching nonempty pack index exists");
    require(
        !std::filesystem::exists(reverse_index),
        "strict import creates no reverse index");
    const auto inspected = adapter->inspect_commit(
        {.Repository = repository, .Commit = commit},
        {});
    require(
        inspected.has_value() && inspected->Tree == tree,
        "commit and tree objects verified");
    const auto ancestry = adapter->is_ancestor(
        {
            .Repository = repository,
            .Ancestor = commit,
            .Descendant = commit,
        },
        {});
    require(
        ancestry == application::git_ancestry::ancestor,
        "fixed ancestry query");

    const auto hook_marker = root.Path / "hook-ran";
    std::filesystem::create_directories(repository / "hooks");
    const auto hook = repository / "hooks" / "reference-transaction";
    write_text(
        hook,
        "#!/bin/sh\n"
        "touch '" + hook_marker.string() + "'\n"
        "exit 91\n");
    std::filesystem::permissions(
        hook,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);

    const application::git_staging_ref stage_base{
        .Operation = operation,
        .Endpoint = application::git_private_ref_endpoint::base,
    };
    const application::git_staging_ref stage_head{
        .Operation = operation,
        .Endpoint = application::git_private_ref_endpoint::head,
    };
    const std::array<application::git_ref_mutation, 2U> stage{
        application::git_ref_write{
            .Ref = stage_base,
            .DesiredObject = commit,
            .ExpectedOld = application::git_ref_expected_absent{},
        },
        application::git_ref_write{
            .Ref = stage_head,
            .DesiredObject = commit,
            .ExpectedOld = application::git_ref_expected_absent{},
        },
    };
    require(
        adapter->update_refs(
            {.Repository = repository, .Mutations = stage},
            {})
            .has_value(),
        "atomic staging ref transaction");
    require(
        !std::filesystem::exists(hook_marker),
        "repository reference-transaction hook not invoked");

    const auto scope = make_scope(0x33U);
    const application::git_epoch_ref final_base{
        .Scope = scope,
        .Ordinal = 1U,
        .Endpoint = application::git_private_ref_endpoint::base,
    };
    const application::git_epoch_ref final_head{
        .Scope = scope,
        .Ordinal = 1U,
        .Endpoint = application::git_private_ref_endpoint::head,
    };
    const std::array<application::git_ref_mutation, 4U> publish{
        application::git_ref_write{
            .Ref = final_base,
            .DesiredObject = commit,
            .ExpectedOld = application::git_ref_expected_absent{},
        },
        application::git_ref_write{
            .Ref = final_head,
            .DesiredObject = commit,
            .ExpectedOld = application::git_ref_expected_absent{},
        },
        application::git_ref_remove{
            .Ref = stage_base,
            .ExpectedOld = commit,
        },
        application::git_ref_remove{
            .Ref = stage_head,
            .ExpectedOld = commit,
        },
    };
    require(
        adapter->update_refs(
            {.Repository = repository, .Mutations = publish},
            {})
            .has_value(),
        "four-operation exact publication transaction");
    const auto final = adapter->inspect_ref(
        {.Repository = repository, .Ref = final_head},
        {});
    require(
        final.has_value() && *final == commit,
        "exact final ref inspection");
    const auto staging = adapter->inspect_ref(
        {.Repository = repository, .Ref = stage_head},
        {});
    require(
        staging.has_value() && !staging->has_value(),
        "exact staging ref removed");

    auto wrong_commit = commit;
    wrong_commit.Digest.front() ^= 0x01U;
    const std::array<application::git_ref_mutation, 1U> wrong_remove{
        application::git_ref_remove{
            .Ref = final_head,
            .ExpectedOld = wrong_commit,
        }};
    require(
        adapter
                ->update_refs(
                    {
                        .Repository = repository,
                        .Mutations = wrong_remove,
                    },
                    {})
                .error()
            == application::git_process_failure::nonzero_exit,
        "wrong expected-old ref removal fails closed");

    write_text(
        repository / "config",
        read_text(repository / "config")
            + "[include]\n\tpath = /tmp/bpr-hostile-config\n");
    require(
        adapter
                ->inspect_commit(
                    {.Repository = repository, .Commit = commit},
                    {})
                .error()
            == application::git_process_failure::invalid_repository,
        "hostile repository config rejected before Git");
    adapter.reset();
}

auto run_real_hostile_canary_contract() -> void
{
#if defined(Q_OS_UNIX)
    temporary_root root{"git-real-hostile"};
    const auto marker = root.Path / "repository-executable-ran";
    const auto trace_marker = root.Path / "ambient-trace-ran";
    const auto canary = root.Path / "hostile-command";
    write_executable_canary(canary, marker, "ambient-command");

    const auto hostile_hooks = root.Path / "ambient-hooks";
    const auto hostile_templates = root.Path / "ambient-templates";
    const auto hostile_template_hooks = hostile_templates / "hooks";
    const auto hostile_home = root.Path / "ambient-home";
    const auto hostile_xdg_git =
        root.Path / "ambient-xdg" / "git";
    const auto hostile_bin = root.Path / "ambient-bin";
    for(const auto& directory : {
            hostile_hooks,
            hostile_template_hooks,
            hostile_home,
            hostile_xdg_git,
            hostile_bin})
    {
        std::filesystem::create_directories(directory);
        std::filesystem::permissions(
            directory,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
    }
    write_executable_canary(
        hostile_hooks / "reference-transaction",
        marker,
        "ambient-hook");
    write_executable_canary(
        hostile_template_hooks / "reference-transaction",
        marker,
        "ambient-template-hook");
    for(const auto* name : {
            "git-lfs",
            "ssh",
            "editor",
            "pager"})
    {
        const auto executable = hostile_bin / name;
        std::filesystem::copy_file(canary, executable);
        std::filesystem::permissions(
            executable,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
    }

    const auto hostile_attributes =
        root.Path / "ambient-attributes";
    write_text(
        hostile_attributes,
        "*.txt filter=lfs diff=bpr\n");
    const auto hostile_config_text =
        "[core]\n"
        "\thooksPath = " + hostile_hooks.string() + "\n"
        "\tfsmonitor = !" + canary.string() + " fsmonitor\n"
        "\teditor = " + canary.string() + " editor\n"
        "\tpager = " + canary.string() + " pager\n"
        "\tattributesFile = " + hostile_attributes.string() + "\n"
        "[credential]\n"
        "\thelper = !" + canary.string() + " credential\n"
        "[filter \"lfs\"]\n"
        "\tprocess = " + canary.string() + " filter-process\n"
        "\tsmudge = " + canary.string() + " smudge\n"
        "\tclean = " + canary.string() + " clean\n"
        "\trequired = true\n"
        "[diff]\n"
        "\texternal = " + canary.string() + " external-diff\n"
        "[diff \"bpr\"]\n"
        "\ttextconv = " + canary.string() + " textconv\n"
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
        "\tshow = " + canary.string() + " pager-show\n"
        "[maintenance]\n"
        "\tauto = true\n";
    const auto system_config = root.Path / "ambient-system-config";
    const auto global_config = root.Path / "ambient-global-config";
    write_text(system_config, hostile_config_text);
    write_text(global_config, hostile_config_text);
    write_text(hostile_home / ".gitconfig", hostile_config_text);
    write_text(hostile_xdg_git / "config", hostile_config_text);

    scoped_environment environment;
    environment.set("HOME", hostile_home);
    environment.set(
        "XDG_CONFIG_HOME",
        root.Path / "ambient-xdg");
    environment.set("GIT_CONFIG_SYSTEM", system_config);
    environment.set("GIT_CONFIG_GLOBAL", global_config);
    environment.set("GIT_ATTR_SYSTEM", hostile_attributes);
    environment.set("GIT_TEMPLATE_DIR", hostile_templates);
    environment.set("GIT_ASKPASS", canary);
    environment.set("SSH_ASKPASS", canary);
    environment.set(
        "SSH_ASKPASS_REQUIRE",
        QByteArray{"force"});
    environment.set("GIT_SSH", canary);
    environment.set("GIT_SSH_COMMAND", canary);
    environment.set("GIT_PROXY_COMMAND", canary);
    environment.set("GIT_EDITOR", canary);
    environment.set("GIT_SEQUENCE_EDITOR", canary);
    environment.set("GIT_PAGER", canary);
    environment.set("PAGER", canary);
    environment.set("GIT_EXTERNAL_DIFF", canary);
    environment.set("GIT_TRACE", trace_marker);
    environment.set("GIT_TRACE2", trace_marker);
    environment.set("GIT_TRACE2_EVENT", trace_marker);
    environment.set("GIT_TRACE2_PERF", trace_marker);
    environment.set(
        "GIT_CONFIG_COUNT",
        QByteArray{"1"});
    environment.set(
        "GIT_CONFIG_KEY_0",
        QByteArray{"core.hooksPath"});
    environment.set(
        "GIT_CONFIG_VALUE_0",
        hostile_hooks);
    environment.set(
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        root.Path / "ambient-alternates");
    environment.set(
        "GIT_OBJECT_DIRECTORY",
        root.Path / "ambient-objects");
    environment.set(
        "GIT_REPLACE_REF_BASE",
        QByteArray{"refs/ambient-replace"});
    environment.set(
        "GIT_ALLOW_PROTOCOL",
        QByteArray{"file:ext:ssh"});
    environment.set(
        "GIT_PROTOCOL_FROM_USER",
        QByteArray{"1"});
    environment.set(
        "GIT_TERMINAL_PROMPT",
        QByteArray{"1"});
    environment.set(
        "PATH",
        hostile_bin);

    auto adapter = make_adapter(
        std::filesystem::path{BPR_TEST_GIT_EXECUTABLE},
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
        std::as_bytes(std::span{sha1_pack}));
    const auto pack =
        object<application::git_pack_id>(
            sha1_pack_hex,
            application::git_hash_algorithm::sha1);
    require(
        adapter
            ->index_pack(
                {
                    .Repository = repository,
                    .Operation = operation,
                    .SpoolBytes = sha1_pack.size(),
                    .MaximumInputBytes = sha1_pack.size(),
                    .ExpectedPack = pack,
                },
                {})
            .has_value(),
        "real Git imports under the hostile parent environment");
    const auto commit =
        object<application::git_commit_id>(
            sha1_commit_hex,
            application::git_hash_algorithm::sha1);
    require(
        adapter
            ->inspect_commit(
                {.Repository = repository, .Commit = commit},
                {})
            .has_value(),
        "real object inspection ignores ambient execution policy");
    require(
        adapter
                ->is_ancestor(
                    {
                        .Repository = repository,
                        .Ancestor = commit,
                        .Descendant = commit,
                    },
                    {})
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
        .Endpoint = application::git_private_ref_endpoint::base,
    };
    const std::array<application::git_ref_mutation, 1U> create{
        application::git_ref_write{
            .Ref = ref,
            .DesiredObject = commit,
            .ExpectedOld = application::git_ref_expected_absent{},
        }};
    require(
        adapter
            ->update_refs(
                {.Repository = repository, .Mutations = create},
                {})
            .has_value(),
        "real ref transaction ignores repository executables");
    require(
        !std::filesystem::exists(marker)
            && !std::filesystem::exists(trace_marker),
        "ambient and repository execution canaries have zero invocations");

    const auto alternates =
        repository / "objects" / "info" / "alternates";
    write_text(
        alternates,
        (root.Path / "ambient-alternates").string() + "\n");
    require(
        adapter
                ->inspect_commit(
                    {.Repository = repository, .Commit = commit},
                    {})
                .error()
            == application::git_process_failure::invalid_repository,
        "alternate object store is rejected before Git");
    require(
        std::filesystem::remove(alternates),
        "remove exact temporary alternates canary");

    const auto replace_root = repository / "refs" / "replace";
    std::filesystem::create_directories(replace_root);
    write_text(replace_root / std::string{sha1_commit_hex}, sha1_commit_hex);
    require(
        adapter
                ->inspect_commit(
                    {.Repository = repository, .Commit = commit},
                    {})
                .error()
            == application::git_process_failure::invalid_repository,
        "replace objects are rejected before Git");
    std::error_code cleanup_error;
    std::filesystem::remove_all(replace_root, cleanup_error);
    require(
        !cleanup_error,
        "remove exact temporary replace-ref canary");
    require(
        !std::filesystem::exists(marker)
            && !std::filesystem::exists(trace_marker),
        "rejected repository canaries launch no process");
#endif
}

auto fake_index_request(
    application::git_process_port& adapter,
    const std::filesystem::path& repository,
    const application::git_operation_identity& operation,
    const std::string_view input)
    -> application::git_process_result<
        application::git_index_pack_success>
{
    write_spool(
        adapter,
        operation,
        std::as_bytes(std::span{input}));
    return adapter.index_pack(
        {
            .Repository = repository,
            .Operation = operation,
            .SpoolBytes = input.size(),
            .MaximumInputBytes = input.size(),
            .ExpectedPack =
                object<application::git_pack_id>(
                    sha1_pack_hex,
                    application::git_hash_algorithm::sha1),
        },
        {});
}

auto run_hostile_process_contract() -> void
{
    temporary_root root{"git-hostile"};
    const auto fake_exec = root.Path / "fake-exec";
    std::filesystem::create_directories(fake_exec);
    std::filesystem::permissions(
        fake_exec,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto fake_git_path =
        fake_exec
#if defined(Q_OS_WIN)
        / "git.exe";
#else
        / "git";
#endif
    std::filesystem::copy_file(
        QCoreApplication::applicationFilePath().toStdString(),
        fake_git_path);
    std::filesystem::permissions(
        fake_git_path,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);

    const auto hostile_config = root.Path / "hostile-global-config";
    write_text(
        hostile_config,
        "[credential]\n\thelper = !false\n"
        "[core]\n\thooksPath = /tmp/bpr-hostile-hooks\n");
    scoped_environment environment;
    environment.set(
        "BPR_PARENT_SECRET",
        QByteArray{"synthetic-parent-secret"});
    environment.set(
        "GIT_CONFIG_GLOBAL",
        QByteArray::fromStdString(hostile_config.string()));
    environment.set(
        "GIT_CONFIG_SYSTEM",
        QByteArray{"/tmp/bpr-hostile-system-config"});
    environment.set(
        "GIT_ATTR_SYSTEM",
        QByteArray{"/tmp/bpr-hostile-system-attributes"});
    environment.set(
        "GIT_TEMPLATE_DIR",
        QByteArray{"/tmp/bpr-hostile-templates"});
    environment.set(
        "GIT_ASKPASS",
        QByteArray{"/tmp/bpr-hostile-askpass"});
    environment.set(
        "SSH_ASKPASS",
        QByteArray{"/tmp/bpr-hostile-ssh-askpass"});
    environment.set(
        "GIT_SSH_COMMAND",
        QByteArray{"/tmp/bpr-hostile-ssh"});
    environment.set(
        "GIT_EDITOR",
        QByteArray{"/tmp/bpr-hostile-editor"});
    environment.set(
        "GIT_PAGER",
        QByteArray{"/tmp/bpr-hostile-pager"});
    environment.set(
        "GIT_EXTERNAL_DIFF",
        QByteArray{"/tmp/bpr-hostile-diff"});
    environment.set("GIT_TRACE", QByteArray{"1"});
    environment.set("GIT_TRACE2", QByteArray{"1"});
    environment.set(
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        QByteArray{"/tmp/bpr-hostile-alternates"});
    environment.set(
        "GIT_REPLACE_REF_BASE",
        QByteArray{"refs/bpr-hostile-replace"});
    environment.set(
        "GIT_ALLOW_PROTOCOL",
        QByteArray{"file:ext:ssh"});
    environment.set(
        "HTTPS_PROXY",
        QByteArray{"http://synthetic.invalid"});

    auto adapter = make_adapter(fake_git_path, root.Path);
    const auto repository = repository_path(root.Path, '4');
    initialize_repository(
        *adapter,
        repository,
        application::git_hash_algorithm::sha1);
    const auto operation = make_operation(0x44U);

    constexpr std::size_t slow_size = 2U * 1024U * 1024U;
    auto slow = std::string{
        "BPR_SLOW_INPUT:" + std::to_string(slow_size) + "\n"};
    slow.resize(slow_size, 'z');
    const auto slow_result = fake_index_request(
        *adapter,
        repository,
        operation,
        slow);
    require(
        slow_result.has_value(),
        "large backpressured input arrives without dropped partial writes");
    const auto reverse_index =
        repository / "objects" / "pack"
        / ("pack-" + std::string{sha1_pack_hex} + ".rev");
    write_text(reverse_index, "hostile-reverse-index");
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
        std::filesystem::remove(reverse_index),
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
        std::as_bytes(std::span{std::string_view{"BPR_BLOCK"}}));
    std::stop_source cancellation;
    std::optional<
        application::git_process_result<
            application::git_index_pack_success>> cancelled;
    std::thread call{
        [&]
        {
            cancelled = adapter->index_pack(
                {
                    .Repository = repository,
                    .Operation = operation,
                    .SpoolBytes = 9U,
                    .MaximumInputBytes = 9U,
                    .ExpectedPack =
                        object<application::git_pack_id>(
                            sha1_pack_hex,
                            application::git_hash_algorithm::sha1),
                },
                cancellation.get_token());
        }};
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    cancellation.request_stop();
    call.join();
    require(
        cancelled.has_value()
            && !cancelled->has_value()
            && cancelled->error()
                == application::git_process_failure::cancelled,
        "external stop token cancels blocked Git");
    std::this_thread::sleep_for(std::chrono::milliseconds{1800});
    require(
        !std::filesystem::exists(root.Path / "descendant-survived"),
        "cancel kills descendant process group");

    adapter.reset();
    const auto log = read_text(root.Path / "invocations.log");
    for(const auto forbidden : {
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
            "bpr-hostile-templates"})
    {
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

auto run_input_validation_contract() -> void
{
    application::git_pack_id invalid{};
    invalid.Algorithm =
        static_cast<application::git_hash_algorithm>(0xffU);
    invalid.Digest.front() = 1U;
    require(
        !application::valid(invalid),
        "invalid hash algorithm is rejected");

    temporary_root root{"git-input"};
    auto adapter = make_adapter(
        std::filesystem::path{BPR_TEST_GIT_EXECUTABLE},
        root.Path);
    const auto outside = root.Path / (std::string(64U, 'a') + ".git");
    require(
        adapter
                ->initialize_bare_repository(
                    {
                        .Repository = outside,
                        .Algorithm =
                            application::git_hash_algorithm::sha1,
                    },
                    {})
                .error()
            == application::git_process_failure::invalid_request,
        "repository path authority cannot escape repositories/v1");
}

auto run_isolation_symlink_contract() -> void
{
#if defined(Q_OS_UNIX)
    temporary_root root{"git-isolation-symlink"};
    const auto executable =
        std::filesystem::canonical(
            std::filesystem::path{BPR_TEST_GIT_EXECUTABLE});
    const auto outside_directory = root.Path / "outside-directory";
    const auto directory_isolation = root.Path / "directory-isolation";
    std::filesystem::create_directory(outside_directory);
    std::filesystem::create_directory(directory_isolation);
    std::filesystem::permissions(
        outside_directory,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(
        directory_isolation,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto directory_sentinel =
        outside_directory / "sentinel";
    write_text(directory_sentinel, "unchanged-directory-target");
    std::filesystem::create_directory_symlink(
        outside_directory,
        directory_isolation / "home");
    const auto rejected_directory =
        git_adapter::make_installed_git_process(
            {
                .GitExecutable = executable,
                .IsolationRoot =
                    std::filesystem::canonical(directory_isolation),
            });
    require(
        !rejected_directory.has_value()
            && rejected_directory.error()
                == application::git_process_failure::
                    invalid_configuration,
        "preexisting isolation directory symlink fails closed");
    require(
        read_text(directory_sentinel)
                == "unchanged-directory-target"
            && !std::filesystem::exists(
                directory_isolation / "xdg"),
        "directory symlink target and remaining topology are untouched");

    const auto outside_file = root.Path / "outside-file";
    write_text(outside_file, "unchanged-file-target");
    const auto file_isolation = root.Path / "file-isolation";
    std::filesystem::create_directory(file_isolation);
    std::filesystem::permissions(
        file_isolation,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::filesystem::create_symlink(
        outside_file,
        file_isolation / "config-system");
    const auto rejected_file =
        git_adapter::make_installed_git_process(
            {
                .GitExecutable = executable,
                .IsolationRoot =
                    std::filesystem::canonical(file_isolation),
            });
    require(
        !rejected_file.has_value()
            && rejected_file.error()
                == application::git_process_failure::
                    invalid_configuration,
        "preexisting isolation file symlink fails closed");
    require(
        read_text(outside_file) == "unchanged-file-target"
            && !std::filesystem::exists(file_isolation / "home"),
        "file symlink target and isolation topology are untouched");

    const auto content_isolation = root.Path / "content-isolation";
    std::filesystem::create_directory(content_isolation);
    std::filesystem::permissions(
        content_isolation,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    const auto existing_config =
        content_isolation / "config-system";
    write_text(existing_config, "must-not-be-truncated");
    const auto rejected_content =
        git_adapter::make_installed_git_process(
            {
                .GitExecutable = executable,
                .IsolationRoot =
                    std::filesystem::canonical(content_isolation),
            });
    require(
        !rejected_content.has_value()
            && rejected_content.error()
                == application::git_process_failure::
                    invalid_configuration
            && read_text(existing_config)
                == "must-not-be-truncated"
            && !std::filesystem::exists(
                content_isolation / "home"),
        "nonempty isolation policy file is rejected before any mutation");
#endif
}
}

auto main(int argc, char** argv) -> int
{
    const auto executable_path =
        argc > 0
        ? std::filesystem::absolute(argv[0])
        : std::filesystem::path{};
    if(
        argc > 0
        && executable_path.stem() == "git")
    {
        QStringList arguments;
        for(int index = 1; index < argc; ++index)
        {
            arguments.push_back(QString::fromUtf8(argv[index]));
        }
        return fake_git(arguments, executable_path);
    }
    QCoreApplication application{argc, argv};

    run_real_git_contract();
    run_real_hostile_canary_contract();
    run_hostile_process_contract();
    run_input_validation_contract();
    run_isolation_symlink_contract();
    std::cout << "Git process contract passed\n";
    return 0;
}
