---
id: BPR-0349
title: Stop the trailing decode restating an exact protocol cause as the generic one
status: Done
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

## Result — 2026-08-17

The card offered two dispositions — repair the assignment as live, or remove it
as dead. The second is refuted by evidence: **the assignment is live**, and the
card's framing of it as possibly redundant was wrong.

An exchange that simply stops early refuses nowhere except in the trailing
decode. `--truncated-exchange` writes a valid `codex_jsonl` prefix and exits, so
every byte the drain reads decodes cleanly, `protocolFailure` is unset, and
`finish` reporting `incomplete_frame` is the first and only refusal. Deleting
the assignment outright makes that run report `malformed_frame` through
`value_or`, and the new assertion fails. So the assignment must stay; only its
unconditional form was wrong.

The repair is therefore the guard, and both halves of the ordering it creates
now have a test that fails without them:

| Mutation | Result |
|---|---|
| Assignment deleted entirely | `qt_agent_launch_canary` fails "an exchange that stops early is refused as incomplete by the trailing decode" |
| `finish` returns an exact cause instead of the generic one | `application_agent_launch` fails "a refused decoder restates the generic cause rather than the exact one" |

The second row proves the premise the guard rests on. `finish` answers
`malformed_frame` for every call made after the decoder has refused, so a host
that adopts that answer adopts a restatement over a cause it already holds.

### Reachability: answered, and not the answer expected

The overwrite is **reachable in principle and not reproducible on demand here**.

Reachable, because the trailing `drain` exists precisely to read output that
QProcess still holds after the child is gone. If those bytes carry a refusal,
the unguarded assignment replaces its exact cause with the restatement. The code
path is not dead; the terminal drain would be pointless if it were.

Not reproducible, because reaching it needs the loop to exit on an iteration
where output arrived and was not drained, and the loop drains at the top of
every iteration. `--fast-remote-failure` was written for exactly this: it emits a
complete refused `codex_jsonl` exchange in one write and exits, with no
`write_partial` trickle for an earlier drain to catch. Against the **unguarded**
launcher it passed 9 runs out of 9 — `waitForReadyRead` returns on data before
Qt marks the process not-running, so the next iteration drains it and the exact
cause survives. Whether that ordering holds under load is a Qt scheduling
detail, not a guarantee this code should depend on.

So the guard is justified by the invariant rather than by a reproduction: the
first recorded cause wins, and the two mutations above fence both sides of it.
`--fast-remote-failure` is retained as coverage in its own right — it is the
only course where a child writes its whole refusal and exits at once, and no
other switch exercises that shape.

### Verification

`ctest -R 'agent'` passed 14/14 including all six invocation contracts. The two
named selectors passed three consecutive times. `coding-format-check` passed
across 374 files, `verify_documentation.py` returned `DOCUMENTATION_OK`, and
`git diff --check` was clean. The full `dev` graph builds.

`src/adapters/agent/src/windows_agent_process.cpp:699` was read and not touched.
It returns on a recorded failure before reaching its own `finish` call, so it
does not carry this shape. [[BPR-0334]] still owns extracting the shared loop,
and inherits this ordering when it does.
