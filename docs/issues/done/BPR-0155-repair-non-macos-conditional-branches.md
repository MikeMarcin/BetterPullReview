---
id: BPR-0155
title: Repair the Windows and Linux conditional branches
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# Repair the Windows and Linux conditional branches

Parameters in this repository are named `aXxx`. A rename to that convention was
applied only to the code the local compiler builds. Every `#if defined(_WIN32)`
and `#elif defined(__linux__)` branch in the affected functions still referred
to the old names, which are not declared there.

**The Windows and Linux builds did not compile.** Confirmed for Windows
by `x86_64-w64-mingw32-g++ -fsyntax-only`: nine errors in
`catalog_database.cpp` alone, beginning
`'storage_root' was not declared in this scope; did you mean 'aStorageRoot'?`.

This is invisible on macOS because the preprocessor removes the broken text
before the compiler sees it. Nothing in the local gate compiles those branches,
so nothing failed.

## Why it matters beyond the compile

The repository carries verification claims for both platforms — BPR-0044's
MinGW compile/link evidence and BPR-0050's Ubuntu 26.04 runtime matrix. Both
predate the rename. Neither can be reproduced at HEAD, and the RFC-BPR-0003
ledger presents them as current. A claim that cannot be reproduced is not
evidence, and the Windows campaign in BPR-0036 and BPR-0053 would have started
by discovering this rather than by testing anything.

## Sites

| File | Branch | Old name | Parameter |
|---|---|---|---|
| `catalog_database.cpp` | `_WIN32` | `path`, `requested_vfs` | `aPath`, `aRequestedVfs` |
| `catalog_database.cpp` | `__linux__`/else | `parent`, `name` | `aParent`, `aName` |
| `catalog_database.cpp` | `__linux__`/else | `output` | `aOutput` |
| `catalog_database.cpp` | `__linux__` | `parent` | `aParent` |
| `catalog_database.cpp` | `_WIN32` | `storage_root`, `stop` | `aStorageRoot`, `aStop` |
| `gh_cli_transport.cpp` | `_WIN32` | `path` | `aPath` |
| `gh_cli_transport.cpp` | `_WIN32`, `__linux__`, else | `facts`, `environment` | `aFacts`, `aEnvironment` |
| `gh_cli_transport.cpp` | `__linux__` | `candidate` | `aCandidate` |
| `epoch_runtime_ports.cpp` | `_WIN32`, `__linux__` | `bytes` | `aBytes` |

## Required direct evidence

- Every non-macOS translation unit that can be reached without Qt compiles
  under the MinGW cross-compiler, where it did not before this card.
- A gate exists that would have caught this, and it covers the Linux branches
  as well — no compiler on this host builds those, so a compile cannot be the
  only check.
- The gate is shown to fail when a single identifier is reverted.
- What the gate cannot reach is named, not implied.

## What was done

The scan found the drift is wider than the compiler could show. `installed_git_process.cpp`
and `installed_git_cache.cpp` use Qt, so no cross-compiler here reaches them,
and four test files were broken the same way. Nine files, 114 lines.

| Surface | Result |
|---|---|
| MinGW x86-64 `-fsyntax-only`, non-Qt sources | 47 clean, 0 failed. Before: `catalog_database.cpp` alone had 9 errors |
| Unreachable without Qt for Windows | 8 translation units |
| Unreachable without libsecret, CoreFoundation, or KSyntaxHighlighting | 7 translation units |
| macOS `dev` build | Unchanged, as expected — none of these branches is compiled here |

The gate is `scripts/verify_conditional_branches.py`, wired as
`conditional_branch_identifiers`. It reads only the branches that are false for
the host running it, and inside them reports a bare `xxx` only where the
enclosing function has a parameter `aXxx` and the branch declares no local
`xxx`. Restricting it to code no local compiler reads is what keeps it quiet:
across `src` and `tests` it reports nothing now, and it reported 78 real sites
before the repair.

`fill_entropy` in `epoch_runtime_ports.cpp` has one branch per platform, so the
same break can be placed in each in turn:

| Break | Gate | macOS compiler |
|---|---|---|
| line 39, `#if defined(_WIN32)` | names the line | silent |
| line 54, `#elif defined(__linux__)` | names the line | silent |
| line 45, `#elif defined(__APPLE__)` | silent | `error: use of undeclared identifier 'bytes'` |

Neither covers the other. That is the design: the gate reads what no compiler
here can, and stays out of the way everywhere else. A gate that also reported
the third row would be re-checking the compiler's work and would have to be
kept quiet about locals, which is where a source-reading check goes wrong.

The condition parser is written out rather than handed to `eval`. Nothing in
this repository executes text it read from a file, and a gate that reads every
source file is the last place to start.

`docs/verification/RFC-BPR-0003.md` now records that BPR-0044's MinGW evidence
and BPR-0050's Ubuntu matrix predate the rename. BPR-0156 carries the Linux
re-verification, which needs a host this one is not.

## What the repair exposed in the architecture contract

Repairing the sources broke `architecture_boundaries`. The Installed-Git runner
contract requires the pattern `stop\.stop_requested\(\)`, and after the rename
nothing in the runner matched it.

That is not a consequence of the repair. It is the same rot one layer up. The
runner uses `aStop.stop_requested()` in twelve places, and the *only* line that
still spelled it `stop.` was line 561, inside `#elif defined(Q_OS_WIN)`. The
contract's evidence that the runner honours cancellation was a line no compiler
on this host built, and eleven compiled ones did not count toward it.

Every other cancellation pattern in that file already accepts `(stop|aStop)`.
This one now does too, so it matches the compiled occurrences rather than the
one that was never built.

## What this card does not do

It does not re-establish Windows or Linux verification. Compiling is not
running. The Windows runtime claims stay in BPR-0053 and the Linux ones in a
card of their own; this only removes the reason those campaigns would have
failed at their first step.
