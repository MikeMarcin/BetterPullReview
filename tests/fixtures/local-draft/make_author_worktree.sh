#!/bin/sh
# RFC-BPR-0108, 0113 and 0114 as amended 2026-11-12. Makes a worktree in author
# state for the attended look of the local draft in the pull request window:
# a branch fix/port-parsing proposing a change against main, one commit on
# "GitHub" and two not pushed, one file staged, two files with unstaged edits
# (one with two separate changes, so Stage Change shows per change) and an
# untracked file. origin names GitHub; the worktree's own configuration
# rewrites that URL to a local bare repository, so Check Again and Push reach
# it without a network and no real repository is touched.
# Usage: make_author_worktree.sh ROOT.
set -eu
mkdir -p "$1"
# Git for Windows' sh names the folder /c/...; its native Git needs C:/... in
# the insteadOf below, which `pwd -W` gives there and other shells refuse.
root=$(cd "$1" && { pwd -W 2>/dev/null || pwd; })
g() { git -c user.name="BPR fixture" -c user.email=fixture@example.invalid -c init.defaultBranch=main -c core.hooksPath=/nonexistent -c commit.gpgsign=false "$@"; }
mkdir -p "$root/seed"
cd "$root/seed"
g init -q
mkdir -p src tests/net docs
cat > src/endpoint.cpp <<'CPP'
#include "net/endpoint.hpp"

#include <cstdlib>
#include <string>

namespace demo::net {

auto parse_port(const std::string& aText) -> int
{
    return std::atoi(aText.c_str());
}

auto parse_endpoint(std::string_view aText) -> std::optional<endpoint>
{
    const auto colon = aText.rfind(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    const auto port = parse_port(std::string{ aText.substr(colon + 1) });
    return endpoint{ std::string{ aText.substr(0, colon) }, port };
}

}
CPP
cat > tests/net/endpoint_test.cpp <<'CPP'
#include "net/endpoint.hpp"

TEST_CASE("parse_endpoint splits host and port")
{
    const auto parsed = parse_endpoint("example.org:443");
    REQUIRE(parsed);
    CHECK(parsed->Host == "example.org");
    CHECK(parsed->Port == 443);
}

TEST_CASE("parse_endpoint needs a colon")
{
    CHECK(!parse_endpoint("example.org"));
}
CPP
printf '# Addresses\n\nAn address is a host and a port.\n' > docs/addresses.md
g add -A && g commit -qm "Endpoints"
cd "$root"
g clone -q --bare seed hosted.git
g clone -q hosted.git engine
cd engine
g remote set-url origin https://github.com/bpr-fixtures/engine.git
# The author's Git reaches "GitHub" here; the isolated reader reads only the URL.
g config url."$root/hosted.git".insteadOf https://github.com/bpr-fixtures/engine.git
g config user.name "BPR fixture"
g config user.email fixture@example.invalid
g config commit.gpgsign false
g fetch -q origin "+refs/heads/main:refs/remotes/origin/main"
g checkout -qb fix/port-parsing
cat > src/endpoint.cpp <<'CPP'
#include "net/endpoint.hpp"

#include <charconv>
#include <string>

namespace demo::net {

auto parse_port(std::string_view aText) -> std::optional<std::uint16_t>
{
    auto value = std::uint32_t{ };
    const auto* const end = aText.data() + aText.size();
    const auto [stop, error] = std::from_chars(aText.data(), end, value);
    if (error != std::errc{ } || stop != end) {
        return std::nullopt;
    }
    if (value == 0 || value > 65535) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

auto parse_endpoint(std::string_view aText) -> std::optional<endpoint>
{
    const auto colon = aText.rfind(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    const auto port = parse_port(aText.substr(colon + 1));
    if (!port) {
        return std::nullopt;
    }
    return endpoint{ std::string{ aText.substr(0, colon) }, *port };
}

}
CPP
g add -A && g commit -qm "Parse ports strictly"
g push -q origin fix/port-parsing
cat >> tests/net/endpoint_test.cpp <<'CPP'

TEST_CASE("parse_port accepts the whole valid range")
{
    CHECK(parse_port("1") == 1);
    CHECK(parse_port("65535") == 65535);
}
CPP
g add -A && g commit -qm "Test the valid port range"
printf '# Addresses\n\nAn address is a host and a port.\n\nA port is 1 to 65535.\n' > docs/addresses.md
g add -A && g commit -qm "Document the port range"
# Staged: the ports page.
printf '# Ports\n\nPorts are parsed strictly: the whole text, 1 to 65535.\n' > docs/ports.md
g add docs/ports.md
# Unstaged: two separate changes in the test, one in the doc, and an untracked note.
cat > tests/net/endpoint_test.cpp <<'CPP'
#include "net/endpoint.hpp"

TEST_CASE("parse_endpoint splits host and port")
{
    const auto parsed = parse_endpoint("example.org:443");
    REQUIRE(parsed);
    CHECK(parsed->Host == "example.org");
    CHECK(parsed->Port == 443);
    CHECK(parse_port("0080") == 80);
}

TEST_CASE("parse_endpoint needs a colon")
{
    CHECK(!parse_endpoint("example.org"));
}

TEST_CASE("parse_port accepts the whole valid range")
{
    CHECK(parse_port("1") == 1);
    CHECK(parse_port("65535") == 65535);
}

TEST_CASE("parse_port rejects what atoi accepted")
{
    CHECK(!parse_port(""));
    CHECK(!parse_port("0"));
    CHECK(!parse_port("80abc"));
    CHECK(!parse_port("65536"));
}
CPP
printf '# Addresses\n\nAn address is a host and a port.\n\nA port is 1 to 65535; anything else fails the whole address.\n' > docs/addresses.md
printf 'Ask about IPv6 literals.\n' > NOTES.txt
