---
schema: "implement-rfc.card.v1"
id: "BPR-0519"
title: "The MCP helper reads its environment with getenv, which MSVC refuses"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0501"]
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
| MSVC builds the helper | `bpr-win`'s windows-dev Debug build from a checkout of this commit | Recorded below | `37018b7c` |

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

## Target-native result — 2026-09-02, station `bpr-win` at `37018b7c`

`cmake --build --preset windows-dev --config Debug` exits **0**, 137 projects
linked, zero `error C` and zero `warning C` lines in the whole log.
`mcp_context_helper_main.cpp` compiles: `_dupenv_s` under `_MSC_VER`, the buffer
freed, `std::getenv` elsewhere, both branches answering
`std::optional<std::string>`.

The wrapper is also correct at runtime, which the build alone would not show.
Running the shipped helper directly with both allowlisted variables set:

```text
mcp-environment-retained	BPR_MCP_PINNED_BINDING
mcp-environment-retained	BPR_MCP_SNAPSHOT_ROOT
```

Exactly `mcp_environment_allowlist`, sorted, and nothing else — so
`reduce_environment_to_allowlist` reads and removes correctly on Windows, and
`_dupenv_s` returns what `std::getenv` returned elsewhere.

| Claim | Command | Result |
|---|---|---|
| MSVC builds the helper | `cmake --build --preset windows-dev --config Debug` | exit 0, 137 projects, 0 errors, 0 warnings |
| The secure-CRT gate is registered and passes on Windows | `ctest --test-dir build/windows-dev -C Debug -R '^msvc_secure_crt_contract$' --output-on-failure` | **Passed, 0.56 s** |
| The reduction keeps exactly the allowlist | the shipped helper run directly with both variables set | the two lines above, and nothing else |

### `application_mcp_context_helper` fails on Windows, and it is not this card

The canary fails here, and the failure is neither this wrapper's nor the
helper's — the row above shows the helper doing exactly what the canary
asserts. It is the test harness: `run_helper` shells through `_popen`, which on
Windows is `cmd.exe /c <whole string>`, and the string it builds begins with a
quote and carries two more quoted paths, so `cmd` strips the outer pair and
mangles it. The helper never starts, `StandardError` is empty, and the
assertion about the allowlist fires having measured nothing.

Reported to the integrator for its own card rather than recorded as a row here;
this card's only canary row is macOS's. Left deliberately unrecorded here so a
reader does not take this card as covering it.

The card is terminal.
