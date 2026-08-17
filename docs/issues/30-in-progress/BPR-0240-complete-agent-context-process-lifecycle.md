---
id: BPR-0240
title: Complete agent context and process lifecycle semantics
status: In Progress
rfc: RFC-BPR-0011
depends_on: [BPR-0238]
---

# Complete agent context and process lifecycle semantics

The current launch builder omits required context from child input. The process
adapter waits for completion, reads stdout once, does not drain stderr, and does
not classify nonzero exit status. Those are component gaps, not merely missing
composition.

## Required final state

- Serialize the PR URL, expected base/head OIDs, selected epochs, previous
  upload, last-reviewed head, changed/unread/accepted file state, local defect
  state, current selected file/semantic selection, and only explicitly included
  private notes or drafts. Version the supplement and keep absent fields
  distinguishable from empty values.
- Carry exact host/repository/PR/base/head/epoch/session identity and the
  permitted semantic segments, groups, mapping confidence, and selected source
  text without presentation coordinates or live scroll state.
- Implement and test Codex App Server, Codex JSONL, Claude stream-JSON, and
  external-terminal protocol behavior rather than treating one canary framing
  as proof for all profiles.
- Stream stdout incrementally, drain stderr without deadlock, and preserve
  structured protocol framing across partial reads.
- Distinguish failed-to-start, nonzero exit, protocol failure, cancellation,
  timeout, stale result, and successful completion as typed outcomes.
- Prove bounded output/backpressure and teardown without leaking credentials or
  placing context in argv.
- A canary asserts all required context fields, stderr pressure, partial
  frames, nonzero exit, cancellation, and stale-result rejection.

## Session handoff — 2026-08-11

The operator ended the current implementation session and requested that the
in-flight Windows process work be preserved rather than discarded. The saved
slice adds a private WIN32 backend, launcher/CMake integration, canary support,
and architecture hostile checks for explicit UTF-16 argv/CWD/environment,
allowlisted handles, suspended process creation, pre-resume Job Object
assignment, incremental stdout/stderr draining, cancellation, reaping, and
descendant containment.

This card remains in progress. No compile or target-native Windows run was
completed after these edits, and no Linux/macOS regression matrix, format/tidy,
documentation gate, or independent review was run. The next session must first
build the affected agent/Qt canary targets on a portable host, run the
architecture boundary pair, then obtain the required target-native Windows
compile/runtime evidence. None of this handoff closes BPR-0240, BPR-0245, or
RFC-BPR-0011.

## macOS formatting checkpoint — 2026-08-11

The preserved slice's three format-gate findings were repaired with the
repository's configured `clang-format`, changing whitespace only in
`agent_process_launcher.cpp`, `windows_agent_process.cpp`, and
`agent_launch_canary_tests.cpp`. Direct `--dry-run --Werror` verification now
passes for all three files. On macOS, the adapter and Qt canary targets rebuilt,
and the exact `architecture_boundaries`, `architecture_boundary_contract`, and
`qt_agent_launch_canary` selector passed 3/3 in 128.33 seconds.

This checkpoint is not target-native Windows evidence: the private WIN32
backend remains excluded by the macOS preprocessor and has not been compiled or
executed on Windows. The card therefore remains in progress with its Windows
compile/runtime matrix, complete portable regression matrix, strict tidy,
documentation gate, and independent review still required for terminal
closure.

## macOS portable lifecycle checkpoint — 2026-08-12

The affected agent launch, invocation-host, Qt launch-panel, and Qt canary
targets rebuilt on macOS. The exact `application_agent_launch`,
`application_agent_invocation_host`, `qt_agent_launch_panel`,
`qt_agent_launch_canary`, `application_agent_invocation_composition_contract`,
`architecture_boundaries`, and `architecture_boundary_contract` selector then
passed 7/7 in 157.95 seconds under the repository network-isolation wrapper.

The repository format check passed for `agent_process_launcher.cpp`,
`windows_agent_process.cpp`, and `agent_launch_canary_tests.cpp`. Strict scoped
clang-tidy passed the two macOS compilation-database translation units,
`agent_process_launcher.cpp` and `agent_launch_canary_tests.cpp`; the WIN32-only
`windows_agent_process.cpp` translation unit was reported absent from that
database and remains target-native Windows work.

This checkpoint closes the selected portable macOS evidence gap only. The card
remains in progress pending the target-native Windows compile/runtime matrix,
the remaining portable regression and closeout gates, and independent terminal
review.

## Portable gates and independent review — 2026-08-17

[[BPR-0319]] deferred this card's lane classification until the portable gates
it owed had actually run here. They have now run, and the answer is that the
card is **not** platform-blocked. It stays in progress.

The complete portable `dev` matrix ran at `be42eb7`: 184 of 186 passed. Both
failures were environmental and each passes on its own.
`application_native_notification_attended_contract` failed on
`sh: /bin/ps: Operation not permitted` — a sandbox restriction on the runner,
not the product — and passes with `/bin/ps` reachable.
`application_epoch_maintenance_public_path` failed under contention while other
work shared the machine, and passes alone in 53.56 s. Neither touches this
card's surface. A clean uncontended replay is still the RFC-closeout gate; this
is checkpoint evidence, not closeout evidence.

That run also supplies evidence this card had never recorded. `8886017` rewrote
`agent_context_supplement.cpp`, `agent_profile.cpp`, and both of their test
files, but neither earlier checkpoint ran the core tier it changed. In this
matrix `core_agent_profile` (#52) and `core_agent_context_supplement` (#54)
both passed.

The independent review returned **PORTABLE-WORK-REMAINS** and it is right. Four
findings, each reproduced directly rather than taken on the reviewer's word:

1. **The Windows pre-start refusal is gone and six documents still assert it.**
   `c6f3d96` replaced the `failed_to_start` guard in
   `agent_process_launcher.cpp:129` with a dispatch into the never-compiled
   WIN32 backend. `RFC-BPR-0011.md:13,109,115,216` and
   `ai-agent-boundary.md:140,335` all still say the adapter refuses before
   `QProcess::start`, and line 109 is a claim-matrix **Result** cell. A seventh
   row, `ai-agent-boundary.md:314`, cites a Windows-conditional canary as proof
   of refusal whose final assertion is now `Outcome == success`. Tracked as
   [[BPR-0332]], which also carries the choice between restoring the guard and
   documenting the real state.
2. **Two of three output ceilings are unproven.** `output_too_large` and
   `frame_too_large` appear four times in `src/` and zero times in `tests/`.
   The 256 KiB stderr canary proves draining without deadlock, not the 1 MiB
   bound. Tracked as [[BPR-0333]].
3. **Core-tier evidence was missing.** Closed by this matrix run, above.
4. **~250 lines of portable protocol logic were duplicated into the
   Windows-only translation unit.** `windows_agent_process.cpp:480-729`
   re-implements the decoder loop, follow-up gating, publish path, stderr
   arithmetic, and outcome classification from
   `agent_process_launcher.cpp:201-345`. Tracked as [[BPR-0334]].

The review also corrected an overclaim on this card. The required final state
says "A canary asserts all required context fields"; the canary checks eight
substrings and never checks `base`, `head`, either epoch, `host`, `repository`,
or any segment or group field. The obligation is met — but by
`application_agent_invocation_composition_contract`, whose whole-map equality
covers them exactly. This card credited the wrong artifact.

What is genuinely Windows-bound is unchanged and remains correctly deferred:
`windows_agent_process.cpp` has never been compiled, strict clang-tidy cannot
reach it because it is absent from the macOS compilation database, and Job
Object pre-resume assignment and descendant containment need real Windows. It is
also the only `Q_OS_WIN`-gated source file, so the file-level claim in the
earlier checkpoints was accurate. What was not accurate was treating everything
that remained as Windows work.
