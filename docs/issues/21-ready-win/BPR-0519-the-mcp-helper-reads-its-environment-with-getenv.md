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
