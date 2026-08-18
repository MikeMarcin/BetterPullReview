---
id: BPR-0333
title: Prove the agent output ceilings instead of asserting them
status: Done
rfc: RFC-BPR-0011
depends_on: []
---

# Prove the agent output ceilings instead of asserting them

[[BPR-0240]] requires proof of "bounded output/backpressure". Backpressure is
genuinely proven — a throwing sink drives the publish path in
`agent_launch_canary_tests.cpp`. The bounds are not. `output_too_large` and
`frame_too_large` appear four times under `src/` and **zero times under
`tests/`**.

Three declared ceilings, and what actually covers each:

| Ceiling | Where | Covered? |
|---|---|---|
| Total stdout, `maximum_agent_stdout_bytes` (1 MiB) | `src/application/src/agent_launch.cpp:1355` | No test reaches it |
| Retained stderr, `maximum_agent_stderr_bytes` (1 MiB) | `src/adapters/agent/src/agent_process_launcher.cpp:226` | No test reaches it |
| Per-frame, `frame_too_large` | reached twice | Reached, never asserted by value |

The stderr case is the one that looks covered and is not. The canary writes
exactly 256 KiB and asserts `result.StandardError.size() == 256U * 1024U` —
a quarter of the ceiling. That is a real proof of draining without deadlock, and
it is not a proof of the bound.

The per-frame case is reached but the outcome is never distinguished.
`agent_launch_tests.cpp:756` asserts only `!oversized.push(...)`, and
`agent_launch_canary_tests.cpp:462` asserts only `Outcome == protocol_failure`.
A regression that collapsed `frame_too_large` into `malformed_frame` would pass
both.

`docs/verification/RFC-BPR-0011.md:115` currently lists "bounds" among what the
component canaries cover. That is the overclaim this card removes.

## Required final state

- A canary switch emits more than `maximum_agent_stdout_bytes` of stdout, and a
  test asserts the run ends in the exact typed outcome the ceiling declares.
- A canary switch emits more than `maximum_agent_stderr_bytes` of stderr, with
  the same exactness, and the existing 256 KiB drain case is kept rather than
  replaced — it proves a different property.
- The two existing oversize-frame cases assert `frame_too_large` by value, so
  collapsing it into another protocol failure fails the suite.
- `RFC-BPR-0011.md:115` claims bounds only to the extent the tests now prove.

## Non-goals

- Do not raise, lower, or make configurable any ceiling. This card proves the
  values that exist.
- No Windows work. Every ceiling here is portable, and the POSIX path is the one
  under test.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_agent_launch|qt_agent_launch_canary)$'
python3 scripts/verify_documentation.py --root .
```

Each new assertion must fail if the corresponding ceiling is removed from the
source.

## Result — 2026-08-17

All four required final states are met, and the change set is tests-only. No
ceiling was raised, lowered, or made configurable, and no Windows file was
touched.

`--stdout-flood` and `--stderr-flood` join the canary's switch list. Each writes
1.5 MiB, well past its 1 MiB ceiling, in blocks rather than through
`write_partial` — that helper emits three bytes per millisecond, which is right
for proving partial-frame handling and would take four days for a megabyte. The
stdout flood keeps every line at 32 KiB so it stays inside the 64 KiB frame
bound and can only be refused by the total.

Both ceilings report the same enumerator, `output_too_large`, so neither test
can name its ceiling by the cause alone. Each case is pinned by the channel it
did not use: the stdout course retains no standard error, the stderr course
publishes no event. The 256 KiB `--stderr-pressure` course is untouched and
still asserts exact retention — it proves draining without deadlock, which the
bound does not.

The two oversize-frame cases now assert `frame_too_large` by value, at
`agent_launch_tests.cpp` on the decoder directly and at
`agent_launch_canary_tests.cpp` through `agent_run_result::ProtocolFailure`. A
third case proves the total ceiling at the decoder level, where it is
deterministic and costs a fifth of a second.

### The assertions fail when the ceilings do not exist

Each mutation below was applied to the source, rebuilt, and run. This is the
card's actual requirement, and it is the only evidence that separates these
assertions from the ones they replace.

| Mutation | Result |
|---|---|
| `agent_launch.cpp:1355` total-stdout check made unreachable | `application_agent_launch` fails "output past the total ceiling is refused as its own exact cause"; `qt_agent_launch_canary` fails "stdout past the total ceiling is refused as its own exact cause" |
| `agent_process_launcher.cpp:222-229` retained-stderr check deleted | `qt_agent_launch_canary` fails "retained stderr stops at its ceiling instead of growing to the flood" |
| Both `frame_too_large` sites rewritten to `malformed_frame` | `application_agent_launch` fails "a frame beyond the fixed bound is refused before further buffering"; `qt_agent_launch_canary` fails "an output frame beyond the bound fails without unbounded retention" |

The third row is the regression the card predicted would pass both tests before
this slice. It now fails both.

Green afterwards, three consecutive runs of
`-R '^(application_agent_launch|qt_agent_launch_canary)$'`, 2/2 each.
`coding-format-check` passes for all three touched files.

### One change was written and then dropped

`agent_protocol_decoder::finish` answers `malformed_frame` once the decoder has
refused, and the launcher's terminal path overwrites its recorded cause with
that answer. This slice wrote the one-line repair and then removed it: with the
repair reverted the canary passes three times out of three, because every switch
carrying an exact cause either outruns a pipe buffer or waits on parent input,
so the child is still running when the parent drains and the loop returns before
`finish` is reached. A repair with no failing-if-wrong evidence does not belong
in a card whose whole subject is unproven assertions. Tracked as [[BPR-0356]].

`coding-format-check` also fails at HEAD on a file this card did not touch,
committed by `82c7d1c` under [[BPR-0340]]. Left alone rather than swept into an
unrelated slice; tracked as [[BPR-0355]].

### What this does not close

[[BPR-0240]] keeps its target-native Windows compile and runtime matrix. The
`ai-agent-boundary.md` and `RFC-BPR-0011.md` pre-start-refusal overclaims stay
with [[BPR-0332]], and the duplicated protocol loop stays with [[BPR-0334]].
