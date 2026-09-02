---
id: BPR-0522
title: The MCP helper's test builds a command cmd.exe mangles, so the helper never starts on Windows
status: Ready
rfc: RFC-BPR-0012
depends_on: [BPR-0501]
---

# The MCP helper's test builds a command `cmd.exe` mangles, so the helper never starts on Windows

## Why

`application_mcp_context_helper` failed on the Windows station the first
time its translation unit compiled there ([[BPR-0519]]), with
`FAILED: the child's complete environment equals the helper's allowlist`
and `Access is denied.` on stderr. The helper is correct: run directly with
the two allowlisted variables it retains exactly the allowlist. The test's
`run_helper` builds `"<exe>" < "<request>" 2> "<errors>"` and hands it to
`_popen`, which is `cmd.exe /c <string>`; cmd's quoting rule strips the outer
quote pair from a string that begins with a quote and carries more, so the
executable path and both redirections are mangled, nothing runs, stderr is
empty, and the first assertion that reads it fails. `bpr-win` reproduced
both forms: the test's string exits 1 with "The filename, directory name,
or volume label syntax is incorrect", and the same string wrapped in one
more quote pair starts the helper. `_popen` succeeds either way, so the
"starts" expectation cannot catch it.

## Required final state

- On Windows the test wraps the entire command in one more quote pair
  before `_popen`, with the reason beside it; macOS and Linux are
  unchanged.
- `application_mcp_context_helper` passes on the Windows station.

## Repair — 2026-09-02

The wrap, under `_WIN32`, in `tests/application/mcp_context_helper_tests.cpp`.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The suite is unchanged on macOS | `ctest --preset dev -R '^application_mcp_context_helper$' --output-on-failure` | Passed 0.28 s | this commit |
| The helper's test passes under MSVC | `bpr-win`'s `ctest --test-dir build/windows-dev -C Debug -R '^application_mcp_context_helper$'` from a checkout of this commit | **Pending** — target-native, the closing row | Pending |
