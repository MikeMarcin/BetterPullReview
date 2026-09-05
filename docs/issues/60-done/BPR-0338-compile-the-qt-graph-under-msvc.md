---
schema: "implement-rfc.card.v1"
id: "BPR-0338"
title: "Compile the Qt application graph under MSVC"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0336","BPR-0337"]
---

# Compile the Qt application graph under MSVC

[[BPR-0336]] made the Qt-free graph build. [[BPR-0337]] made Qt available. This
card is what happened when the two met: the full `dev` graph reached the
compiler for the first time and 14 translation units failed.

`better_pull_review.exe` now links on Windows x64. That has never happened
before.

## Six defect classes

Four were anticipated from the first failing build; two were only reachable
after earlier ones were cleared.

### C2589 — the `min`/`max` macros

`<Windows.h>` defines `min` and `max` as macros, which breaks every
`std::max(` and `std::min(` that follows. Eight sites in
`src/app/src/github_application.cpp`, one cascade in
`tests/github_contract/github_cli_process_runner_tests.cpp`.

The repository already had the answer: `windows_credential_manager.cpp:3-8`
carries a `NOMINMAX` / `WIN32_LEAN_AND_MEAN` guard pair, and
`catalog_database.cpp` carries the same. `github_application.cpp` included the
Windows header without it. The existing convention was copied rather than a
global compile definition added.

### C7560 — designated initializers out of declaration order

C++20 requires designators in member-declaration order. Two aggregates
violated it, in `github_application::status()` (35 members) and the
`automated_run` initializer in `src/app/main.cpp` (4 designators misplaced).

Both were resolved by reordering alone. Every designator named a real member,
all members were initialized exactly once, and no initializer expression
changed. Reordering a braced-init-list does change C++20's left-to-right
evaluation order, so that was checked rather than assumed: the `status()`
expressions are pure reads, and the five reordered `automated_run` entries
contain no `std::move`, so both reorders are observationally inert.

### C2065 — stale identifiers in branches nothing had compiled

The [[BPR-0155]] and [[BPR-0336]] pattern again, four more sites. An `aPrefix`
rename landed and the non-Unix branch kept the old spelling.

### C4996 — deprecations, fatal under `/WX`

`std::filesystem::u8path` replaced with the construction MSVC's own
deprecation text prescribes. `std::getenv` replaced with `qEnvironmentVariable`
in the four Qt tests, which is the established house spelling in six sibling
files.

### C4702 — unreachable code

`native_https_transport.cpp`'s `credential_matches` ended an `if constexpr`
branch with a trailing `return true;` that is unreachable in one instantiation.
Converted to an explicit `else`.

### C4273 — inconsistent dll linkage

`agent_canary_main.cpp` hand-declared `extern "C" char** _environ;`. In the
UCRT, `_environ` is a macro expanding to an accessor call, so that line
redeclared a `_DCRTIMP` function without import linkage. The standard headers
already supply it.

## Two defects that are not portability problems

Both compile-only failures on their face. Both are dead code that would have
misbehaved the moment it went live, on the only platform that runs it.

1. **`installed_git_process.cpp:3556-3558`** — the non-Unix
   `initialize_owned_bare_repository` branch referenced `request.Repository`
   and `create_error`, neither of which exists in that scope. This is not a
   discarded diagnostic; it is the actual directory-creation call. **Every
   Windows owned-bare-repository initialization was unreachable by
   construction.**
2. **`windows_agent_process.cpp:873`** — the call site passed `aDeadline`, the
   *callee's* parameter name, not the caller's local. **The agent timeout was
   never wired through on the only platform this file serves.**

Both received the prescribed identifier correction and no logic change. Neither
can be exercised here, so neither is verified — see below.

## Evidence

- `cmake --build --preset dev -- -k 0`: zero failed edges; an immediately
  repeated build reports `ninja: no work to do`.
- `build/dev/src/app/better_pull_review.exe`, 21,521,408 bytes, links.
- Zero warnings anywhere in the post-edit log.
- `clang-format --output-replacements-xml`: zero replacements on all twelve
  changed files.

## Judgement calls worth knowing about

- **A `#pragma warning` was introduced**, the first in the tree, around one
  `std::getenv` in `agent_canary_main.cpp`. That fixture is deliberately
  Qt-free, so `qEnvironmentVariable` was unavailable, and there is no standard
  replacement for `getenv`. Deriving the value from the environment block the
  fixture already walks was rejected because Windows `getenv` is
  case-insensitive and a manual scan is not — a semantic change inside a
  hostile-behaviour fixture. The pragma is scoped to one statement and guarded
  by `_MSC_VER`.
- **`qEnvironmentVariable` treats set-but-empty as absent; `std::getenv` does
  not.** For `BPR_EXPECTED_QT_SCALE=""` the old code would have run its
  assertion with `scale == 0.0` and hard-failed. Consistency with the six
  sibling files was chosen over bug-compatibility with an unreachable failure
  mode; `qEnvironmentVariableIsSet` restores strict equivalence if wanted.

## What this card does not claim

**Nothing here is a runtime claim.** The graph compiles and links; not one line
of it has executed on Windows. Every test still fails at launch on the
default-deny network boundary [[BPR-0235]] owns, so the two defects named above
are corrected but unverified.

One is explicitly open: the Windows `create_directory` path has no POSIX-mode
equivalent to the Unix branch's `::mkdir(..., S_IRWXU)`, and whether the
following `private_directory()` check is satisfied by Windows default ACLs is a
runtime question a compile gate cannot answer. It is left as-is rather than
guessed at, and [[BPR-0036]] should exercise it once tests can run.

Also corrected here: the [[BPR-0337]] provider ran `aqtinstall` with the
repository as its working directory, leaving an untracked `aqtinstall.log`
behind. The tool now runs in its own cache directory.
