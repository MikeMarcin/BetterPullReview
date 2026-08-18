---
id: BPR-0349
title: Stop the trailing decode restating an exact protocol cause as the generic one
status: Ready
rfc: RFC-BPR-0011
depends_on: []
---

# Stop the trailing decode restating an exact protocol cause as the generic one

`agent_protocol_decoder::finish` returns `malformed_frame` for every call made
after the decoder has already refused (`src/application/src/agent_launch.cpp:1412`):

```cpp
if (Failed) {
    return std::unexpected(agent_protocol_failure::malformed_frame);
}
```

The POSIX launcher's terminal path then overwrites the cause it already holds
with that generic answer (`src/adapters/agent/src/agent_process_launcher.cpp:306`):

```cpp
const auto trailing = decoder.finish();
if (!trailing) {
    protocolFailure = trailing.error();   // discards an exact earlier cause
}
```

So a run whose overflow or oversize frame is detected by the *final* drain — the
one after the polling loop has already seen the child exit — reports
`malformed_frame` where `output_too_large` or `frame_too_large` is true. That is
the precise collapse [[BPR-0333]] exists to prevent, reachable in one code path.

## Why this is its own card

[[BPR-0333]] wrote the obvious one-line fix, `if (!trailing && !protocolFailure)`,
and then could not justify keeping it. With the fix reverted, that card's
by-value assertions still pass, three runs out of three. The reason is that every
canary switch which produces an exact cause writes more than a pipe buffer or
waits on parent input, so the child is always still running when the parent
drains, and the loop returns before `finish` is ever reached.

An unreachable-in-practice repair with no failing-if-wrong evidence is not
something to land quietly inside an unrelated slice. `tests/AGENTS.md` is
explicit: a test must fail or visibly disagree when its claimed behavior is
false. This card owes that evidence before the repair, not after.

## Required final state

- Whether the overwrite is reachable at all is answered from the code, not
  assumed. Either a deterministic course drives a child that exits before the
  parent's first drain while carrying an exact cause, or the path is shown to be
  unreachable and the redundant assignment is removed as dead rather than
  repaired as live.
- If reachable: the exact cause survives, and a test fails when it does not.
- If unreachable: that argument is written down where the next reader of
  `finish` will find it, so this is not rediscovered a third time.
- `src/adapters/agent/src/windows_agent_process.cpp:699` is checked for the same
  shape. It returns earlier on a recorded failure and so appears unaffected, but
  it is target-native work and [[BPR-0334]] is already slated to extract this
  loop; whichever card lands second inherits the answer.

## Non-goals

- Do not change any ceiling, or the meaning of any existing enumerator.
- No Windows work here beyond the read named above.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_agent_launch|qt_agent_launch_canary)$'
```
