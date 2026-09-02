---
id: BPR-0519
title: The MCP helper reads its environment with getenv, which MSVC refuses
status: Ready
rfc: RFC-BPR-0012
depends_on: [BPR-0501]
---

# The MCP helper reads its environment with `getenv`, which MSVC refuses

## Why

`src/adapters/mcp/src/mcp_context_helper_main.cpp` ([[BPR-0501]]) read
`BPR_MCP_SNAPSHOT_ROOT` and `BPR_MCP_PINNED_BINDING` with `std::getenv`.
MSVC's secure-CRT policy rejects that as C4996 under warnings-as-errors, so
the Windows Debug build failed on it when `bpr-win` merged main for
[[BPR-0516]]. The application's portable idiom is Qt's
`qEnvironmentVariable`, which this helper deliberately cannot use: it links
the core only, for a stdio tool with no Qt.

## Required final state

- The helper reads its two variables through one small wrapper that uses
  `_dupenv_s` under MSVC and `std::getenv` elsewhere, copies the value at
  once, and reports absence as absence.
- The helper's behaviour is unchanged on every platform: missing variables
  still refuse with `missing_environment`; `application_mcp_context_helper`
  stays green.
- MSVC builds the helper.

## Repair — 2026-09-02

`environment_value(const char*) -> std::optional<std::string>` in the
helper's main, with the reason in its comment; both reads and the path
construction use it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The file compiles for Clang with the tree's flags | the `build/dev` compile command with `-fsyntax-only` | exit 0 | this commit |
| The helper's canary stays green on macOS | `ctest --preset dev -R '^application_mcp_context_helper$' --output-on-failure` | Pending on this host until the tree is free of a concurrent slice's build; recorded below when run | Pending |
| MSVC builds the helper | `bpr-win`'s windows-dev Debug build from a checkout of this commit | **Pending** — target-native, the closing row | Pending |

## The test's own copy, and a gate — 2026-09-02

`bpr-win` reported the tree still failing on
`tests/application/mcp_context_helper_tests.cpp`'s same-named
`environment_value`, the test's own `std::getenv` copy, with that the only
error left in the whole graph. It now carries the scoped C4996 suppression
`tests/fixtures/agent/agent_canary_main.cpp` already uses, with the reason.
`tests/scripts/msvc_secure_crt_tests.py`, registered as
`msvc_secure_crt_contract`, refuses any `std::getenv` in `src/` or `tests/`
that carries neither the suppression nor a `_dupenv_s` branch, allowlisting
by name the two POSIX-only native suites Windows never builds.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The gate recognises both accepted spellings and finds no bare read | `python3 tests/scripts/msvc_secure_crt_tests.py` | 3/3 | this commit |
| The test file still compiles for Clang with the tree's flags | the `build/dev` compile command with `-fsyntax-only` | exit 0 | this commit |
