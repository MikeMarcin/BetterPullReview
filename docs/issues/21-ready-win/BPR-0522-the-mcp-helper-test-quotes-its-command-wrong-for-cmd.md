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
