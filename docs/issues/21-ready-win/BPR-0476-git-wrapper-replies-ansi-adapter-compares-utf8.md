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
