---
id: BPR-0476
title: The git diagnostic wrapper replies in ANSI while the adapter compares UTF-8
status: Ready
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
