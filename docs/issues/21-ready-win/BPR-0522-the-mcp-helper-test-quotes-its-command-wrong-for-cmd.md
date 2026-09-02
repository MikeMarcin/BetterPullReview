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
| The helper's test passes under MSVC | `bpr-win`'s `ctest --test-dir build/windows-dev -C Debug -R '^application_mcp_context_helper$'` from a checkout of this commit | **Still failing** — the quoting repair works; a second, independent defect sits behind it, recorded below | Pending |

## Target-native result — 2026-09-02, station `bpr-win` at `3c62036c`

**The repair is correct and it is not sufficient.** The tree builds clean
(exit 0, 137 projects, zero errors, zero warnings) and the selector still
fails — but it fails *differently*, which is what says the wrap worked:

```text
1/1 Test #100: application_mcp_context_helper ...***Failed  Required regular expression not found. Regex=[mcp-context-helper-tests:pass
]  0.10 sec
FAILED: the child's complete environment equals the helper's allowlist
```

The `Access is denied.` / `The filename, directory name, or volume label
syntax is incorrect.` line this card was written about is **gone**. `cmd` now
parses the command and the helper now runs.

The exact command string the test builds, printed from a scratch reproduction
rather than by editing the test:

```text
"C:\code\git\bpr-windows\build\windows-dev\src\adapters\mcp\Debug\better_pull_review_mcp_context_helper.exe" < "<scratch>\requests.jsonl" 2> "<scratch>\stderr.txt"
```

which the repair wraps in one more pair before `_popen`. Handed to `cmd /c`
that way it starts the helper and both redirections land, which the retained
lines below prove.

### The second defect: the helper writes CRLF and the test splits on LF

Running that exact command through `cmd` and reading the stderr file it
produced, byte for byte — escape sequences written out, not the characters
themselves:

```text
mcp-environment-retained  \t  BPR_MCP_PINNED_BINDING  \r\n
mcp-environment-retained  \t  BPR_MCP_SNAPSHOT_ROOT   \r\n
```

The helper writes `std::cerr << ... << '\n'`, and a Windows text-mode stream
turns that into a carriage return plus newline in the redirected file. The
test then reads that file with `std::ios::binary`, so nothing translates it
back, and `split_lines` splits on newline alone without stripping the
carriage return. Every parsed name therefore carries a trailing one:

```text
parsed name: b'BPR_MCP_PINNED_BINDING\r'   equals allowlist entry? False
parsed name: b'BPR_MCP_SNAPSHOT_ROOT\r'    equals allowlist entry? False
```

So `retained == allowlist` compares two names against two different names and
fails. The assertion blames the environment, and the environment is right: the
reduction retains exactly `mcp_environment_allowlist`, sorted, and nothing
else. What is wrong is the parse.

Three hypotheses were ruled out before this one, each by measurement rather
than by reading:

| Hypothesis | Ruled out by |
|---|---|
| Windows' hidden per-drive `=C:` entries yield an unremovable empty name | The helper, run directly and again through `cmd`, retains exactly the two allowlisted names and nothing else |
| The AppContainer refuses the child ([[BPR-0362]]'s class) | The suite fails identically outside the sandbox, and without any `Access is denied.` |
| This card's wrap did not take | The `cmd` parse error is gone and the helper now runs; the failure moved from "nothing started" to "names do not compare equal" |

### What would fix it

Strip one trailing carriage return in `split_lines`, or open the stderr file
without `std::ios::binary` and let the CRT translate on read. The first is the
narrower change and keeps `read_file` honest for its other callers. Not done
here: `tests/**` is the integrator's for this card, and the station was
dispatched to measure rather than to repair.

The card stays in `21-ready-win/` with its closing row Pending, because that
row says the test passes on the Windows station and it does not.

## The second defect: carriage returns — 2026-09-02

With the command parsed, the helper runs and the suite fails on
`the child's complete environment equals the helper's allowlist` because a
Windows text-mode `std::cerr` writes each `'\n'` as `"\r\n"`, the file is
read back in binary, and `split_lines` kept the carriage return, so
`BPR_MCP_PINNED_BINDING\r` never equalled its allowlist entry. The helper's
environment was right both directly and through `cmd`; the parse was
wrong. `split_lines` now strips one trailing carriage return per line, which
keeps `read_file` binary for its other callers. Measured by `bpr-win` from
the bytes the exact command produced, with the per-drive `=C:` entries, the
AppContainer, and the wrap itself ruled out first.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The suite still compiles for Clang and is unchanged on POSIX by construction (no carriage return to strip) | the `build/dev` compile command with `-fsyntax-only` | exit 0 | this commit |
| The helper's test passes under MSVC | `bpr-win`'s `ctest --test-dir build/windows-dev -C Debug -R '^application_mcp_context_helper$'` from a checkout of this commit | **Pending** — target-native, the closing row | Pending |

## Second target-native result — 2026-09-02, station `bpr-win` at `1818d6a0`

The carriage-return strip works. The allowlist assertion this card's previous
run stopped at is gone, and the selector now fails further in, on a third and
final cause that is neither the quoting nor the parse:

```text
1/1 Test #100: application_mcp_context_helper ...***Failed  Required regular expression not found. Regex=[mcp-context-helper-tests:pass
]  0.13 sec
FAILED: refusal table entry 'read_above_snapshot_root' must be refused as path_outside_snapshot
FAILED: refusal table entry 'read_absolute_outside_snapshot' must be refused as path_outside_snapshot
FAILED: refusal table entry 'read_snapshot_metadata_above_files_root' must be refused as path_outside_snapshot
FAILED: every entry of the refusal table is refused by its own exact cause
```

### The cause is the AppContainer, and the bytes say so

Three of the table's rows fail and the rest pass. The three are the only rows
that reach the filesystem: every earlier row refuses on scheme or authority
inside `resolve_snapshot_path` before `weakly_canonical` is ever called.

Driving the shipped helper directly against an equivalent fixture, with the
same three requests, outside and then inside the sandbox launcher:

| Context | Answer for all three rows |
|---|---|
| Outside the AppContainer | `{"refusal":"path_outside_snapshot"}` — the expected cause |
| Inside the AppContainer | `{"refusal":"snapshot_unavailable"}` |

`snapshot_unavailable` is what `resolve_snapshot_path` returns when
`weakly_canonical(files_root(...))` sets an error code. Under a lowbox token
that canonicalization cannot translate the volume device into a drive letter,
which is [[BPR-0362]]'s measured cause, so the helper refuses correctly but for
the wrong reason and the table's exact-cause check fails.

The whole suite confirms it end to end: run outside the AppContainer at this
head it **passes**.

```text
=== DIRECT (no AppContainer) ===
mcp-context-helper-tests:pass
=== DIRECT EXIT 0 ===
```

### What would fix it

`WINDOWS_UNCONTAINED` on `application_mcp_context_helper`, the same keyword
`native_https_transport_fixture` already carries for the same cause. One
keyword, in the test's registration. Not applied here: this station's
`tests/**` authority for this task covers [[BPR-0525]]'s registrations, and
this is a different card's file.

### The three causes in order, so none is mistaken for another

| # | Cause | Found | Fixed |
|---|---|---|---|
| 1 | `_popen`'s command begins with a quote and `cmd` strips the outer pair | this card | this card's wrap |
| 2 | the helper writes CRLF and `split_lines` splits on LF, leaving `\r` on every name | [[BPR-0522]]'s first station run | the strip at `1818d6a0` |
| 3 | the AppContainer refuses the drive-letter translation, so the filesystem rows refuse as `snapshot_unavailable` | this run | not yet |

Each was hidden behind the one before it, which is why this took three passes
rather than one: the first stopped the helper from starting, the second stopped
the names comparing, and only then could the third be seen at all.

The card stays in `21-ready-win/` with its closing row Pending, because that
row says the test passes on the Windows station and it does not.
