---
id: BPR-0334
title: Extract the shared agent protocol loop out of the Windows-only translation unit
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0240]
---

# Extract the shared agent protocol loop out of the Windows-only translation unit

The preserved Windows slice [[BPR-0240]] describes duplicated roughly 250 lines
of *portable* protocol logic into a translation unit that no toolchain here
compiles. `windows_agent_process.cpp:480-729` re-implements, independently, what
`agent_process_launcher.cpp:201-345` already does:

- driving `agent_protocol_decoder::push`;
- the App Server `appServerPhase` 0 → 1 → 2 follow-up gate;
- `build_agent_protocol_followup` and `acknowledge_followup` ordering;
- the publish and backpressure path;
- the retained-stderr ceiling arithmetic;
- the seven-outcome classification.

None of that is platform logic. Handle plumbing, Job Object assignment, and
suspended process creation are; a protocol state machine is not. There are now
two implementations of the same rules, exactly one of which is compiled or
tested on any host this project has, and they can drift silently because nothing
compares them.

## Required final state

- The protocol loop, follow-up gating, publish/backpressure path, stderr ceiling
  arithmetic, and outcome classification exist once, behind a narrow transport
  seam — write, read-available, wait, terminate.
- The POSIX and Windows backends both use it; neither keeps a private copy.
- The shared loop is tested here against a fake transport, so its behaviour is
  provable on this host rather than only on Windows.
- No behaviour changes on POSIX. This is extraction, not redesign.

## Non-goals

- Do not attempt to make `windows_agent_process.cpp` compile on macOS, and do
  not weaken the seam to allow it.
- Do not fold in the POSIX stdin-write asymmetry. Windows has a bounded,
  deadline-aware `write_all` that distinguishes cancellation from timeout; POSIX
  writes into Qt's unbounded buffer and can only report `malformed_frame`. That
  is a real difference worth deciding, and it is a separate card, not a rider on
  an extraction.

## Why this waits on BPR-0240

[[BPR-0240]] still owns `windows_agent_process.cpp` and has an unfinished slice
in it. Extracting shared code from under an in-flight card would collide. This
card starts when that one settles.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_agent_launch|application_agent_invocation_host|qt_agent_launch_canary|architecture_boundaries|architecture_boundary_contract)$'
```

Plus a new focused selector for the shared loop against its fake transport, and
`clang-format` on every file touched.
