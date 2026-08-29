---
id: BPR-0476
title: The git diagnostic wrapper replies in ANSI while the adapter compares UTF-8
status: Done
rfc: RFC-BPR-0011
depends_on: []
---

# The git diagnostic wrapper replies in ANSI while the adapter compares UTF-8

Carded 2026-08-28 from [[BPR-0240]]'s 2026-08-26 workspace-diagnostic trace,
which recorded this as a latent gap deliberately left unrepaired.
`tests/fixtures/git/git_diagnostic_wrapper_main.cpp` answers `--exec-path`
with `executable.parent_path().string()` — the ANSI code page on Windows —
while `src/adapters/git/src/installed_git_process.cpp` compares u8-encoded
text (`u8string()` at the bootstrap probe, `generic_u8string()` in the
handshake helpers). The two agree only while every byte is ASCII, which is why
`application_agent_invocation_workspace_diagnostic_contract` passes on the
current host. A wrapper staged under a path with a non-ASCII component would
fail the earliest wire check with the same bare-detail `process_failure` that
card spent three diagnoses chasing.

## Required final state

- The wrapper's replies are byte-identical to the adapter's u8 expectation on
  every platform, non-ASCII paths included.
- A target-native Windows run stages the wrapper in a directory containing a
  non-ASCII component and passes
  `application_agent_invocation_workspace_diagnostic_contract`; the case fails
  without the repair.
- POSIX behavior is unchanged, proven by the same course's portable selector.

## Why this lane

The divergence manifests only where `path::string()` and `path::u8string()`
differ, which is Windows, and the proof is a target-native run with a
non-ASCII staging path. The repair itself is small — the fixture's reply
encoding plus the test arrangement that makes it falsifiable.

## The portable repair is in — 2026-08-28

The scout falsified this card's one-line framing before anything was edited:
the defect is two-sited. The adapter's bootstrap probe constructed the path
from the reply with the narrow native constructor
(`std::filesystem::path{ execPathText }`) while its handshake probe compares
`path_text`'s u8 re-encoding — so under a non-UTF-8 Windows code page no
consistent child reply could satisfy both probes for a non-ASCII path, real
git included. A wrapper-only repair would have traded the second probe's
failure for the first's.

Three portable edits landed, each byte-identical on POSIX and each the fix on
Windows:

- the wrapper answers `--exec-path` with `u8string()` bytes;
- the adapter decodes the reply through a new `path_from_text` u8 helper,
  placed beside `path_text` as its inverse, with the wire-encoding constraint
  stated at both sides;
- the workspace-diagnostic course stages the wrapper under `wräpper/` — a
  component whose ANSI and UTF-8 encodings differ — built from Qt's UTF-16
  without the lossy narrow `toStdString()` round-trip the driver used to
  take.

With all three in place the ordinary Windows run of
`application_agent_invocation_workspace_diagnostic_contract` becomes the
falsifiable proof: revert the wrapper half and the bootstrap probe u8-decodes
an ANSI reply; revert the adapter half and it ANSI-decodes a UTF-8 reply;
the ASCII-only staging was the inert state this card was opened about.

macOS evidence at this state: clean incremental dev build, format gate
passing, and the ten-selector focused matrix — all eight
`application_agent_invocation_*_contract` courses plus
`git_process_contract` and `git_cache_contract`, the two suites that compile
`installed_git_process.cpp` directly — passed 10/10 twice (449.8 s, then
454.0 s after a clang-format-only touch), the workspace-diagnostic course
running over the non-ASCII staging both times. Documentation impact beyond
this card: none — `git-and-epoch-model.md` and the RFC-BPR-0003 ledger
describe the handshake without asserting a reply encoding, and both stay
true.

What remains is exactly the target-native run this lane names, so the card
returns to `21-ready-win/`.

## The target-native proof — 2026-08-28, evening

A Remote Control lane on the JaegAW host (WSL2 shell driving the native
Windows toolchain — MSVC 18.9.1, VS 18 Community, CMake 4.4.2, Visual Studio
generator, `hostSystemName == Windows`; the executables, filesystem, and
ctest are genuinely Windows, and only the launching shell is not — recorded
here so no reviewer discovers it later) ran the course three ways in the
`windows-lane` worktree at `C:\code\git\bpr-windows`, fast-forwarded
`7160c283` → `b594abed` from a verified-clean state. The build was
incremental (19.6 s, zero configure), and all three freshly linked images —
the wrapper, the course driver, and the relinked canary — were launch-probed
ALLOWED before any run, so no host-policy refusal touches these results.

- **At `b594abed`: PASS, 6.20 s** — the ordinary run over the non-ASCII
  `wräpper/` staging, on a host where ANSI and UTF-8 genuinely differ.
- **Counter-run: FAIL, typed, the original signature.** With only
  `installed_git_process.cpp` restored to `b594abed~1` (narrow ANSI decode
  back, wrapper still replying UTF-8) and rebuilt, the course fails in
  32.52 s printing `EPOCH_CACHE_FAILURE:10:` — `git_cache_failure` index 10
  is `process_failure` with an EMPTY detail — and
  `EPOCH_EFFECT_FAILED:revision-prepare:6`, `core::epoch_failure` index 6,
  `process_failure`. That is verbatim the bare-detail `process_failure`
  this card's opening section names, so the counter-run reproduces
  BPR-0240's original defect specifically rather than merely failing.
- **Restored: PASS, 8.14 s.** Both checkouts ended `git status --short`
  clean; no commits, pushes, or tracker edits were made on that host.

The ten-selector focused sweep then matched macOS target-native: all eight
`application_agent_invocation_*_contract` courses plus
`git_process_contract` (35.37 s) and `git_cache_contract` (53.69 s), 10/10
in 176.49 s. The lane flagged `application_agent_invocation_cleanup_retry_contract`
passing against matrix-10's record of it as a designed gap; resolved — that
gap was [[BPR-0465]]'s, closed 2026-08-27 after matrix-10's snapshot, so the
pass is expected.

Every required-final-state bullet now has direct evidence: byte-identical
replies proven by the Windows pass over the non-ASCII component, the
fails-without-the-repair criterion demonstrated by the counter-run's exact
original signature, and POSIX behavior proven unchanged by the macOS 10/10
runs recorded above. Host facts worth keeping: the reputation-settled
Windows build tree is the `windows-lane` worktree at `C:\code\git\bpr-windows`
(its gitdir is Windows-form, so `git worktree list` from WSL2 shows it as
"prunable" — it is not); the main checkout's `build/windows-dev` is a bare
2026-08-17 configure whose from-scratch path was refused by Smart App
Control on the nested KSyntaxHighlighting Release indexer, a per-image
refusal still standing past 71 minutes on that tree, immaterial to these
results and left to the owner's recorded disposition.
