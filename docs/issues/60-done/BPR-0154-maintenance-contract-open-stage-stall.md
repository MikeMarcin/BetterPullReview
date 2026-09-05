---
schema: "implement-rfc.card.v1"
id: "BPR-0154"
title: "Attribute the epoch-maintenance open-stage stall if it recurs"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0131"]
---

# Attribute the epoch-maintenance open-stage stall if it recurs

`application_epoch_maintenance_public_path` failed once at commit `e7ababa`,
on the `verify-pruned-delete` phase, in an otherwise green end-to-end run. It
has not recurred in the four runs since — one idle, one under six busy CPU
workers, and two full `dev` suites.

The recorded evidence was:

```
GITHUB_FIXTURE_FAILED:github.com
GITHUB_FIXTURE_EVIDENCE:11111110:-1:10:9
PUBLIC_PATH_EPOCH_PAIR:::-1:-1:0:0
PUBLIC_PATH_MAINTENANCE_EVIDENCE:0:0:0:0:0:0:0:0:0:0:0:0:0:-1:-1
```

That is the same signature `--automated-stall-probe` produces when the `open`
stage never becomes ready: one fixture presentation fact false, no resolved
epoch pair, and every maintenance counter zero. It is consistent with a stall
in `open` rather than a failed check.

**Consistent with is not the same as shown to be.** The run predates BPR-0131,
so it carried no `PUBLIC_PATH_BUDGET` line, and nothing in the captured output
distinguishes a stage that stalled from one whose readiness condition was
genuinely false at the moment the old attempt budget ran out. Guessing between
those two would put a fix on the wrong side of the question.

## What this card is waiting for

A recurrence. Since BPR-0131 the same failure prints
`PUBLIC_PATH_BUDGET:expired=<0|1> budget_ms=... elapsed_ms=... stage_ticks=...`,
which settles it in one line:

- `expired=1` with a high `stage_ticks` — the run was waiting on a condition
  that never became true. Find which of the eight fixture presentation facts is
  the `0` in `GITHUB_FIXTURE_EVIDENCE` and fix the wait, as BPR-0119 and
  BPR-0133 each did for their own stage.
- `expired=1` with a low `stage_ticks` — the event loop was blocked. A
  different investigation: something in the tick is doing blocking work.
- `expired=0` — not a stall at all. A check genuinely failed, and
  `PUBLIC_PATH_GROUPS` names which group to look in.

## Required direct evidence

- The recurrence is captured with its full marker set, including
  `PUBLIC_PATH_BUDGET`.
- The failing fixture presentation fact is named, not inferred.
- The fix is shown to make the failure reproducible-then-absent, in the manner
  of BPR-0119 and BPR-0133 — a repeat count that would have caught the original
  rate.

## A recurrence, whose evidence was thrown away

It happened again during BPR-0164's gate, at commit `2ef53fe`. The test failed
in a run where everything else passed, and passed on the next invocation in
56.66 seconds.

**The `PUBLIC_PATH_BUDGET` line was not captured.** The gate command piped
`ctest --output-on-failure` through `tail -8`, so the summary survived and the
failure output did not. Re-running overwrote `LastTest.log` with the passing
run, taking the last copy with it.

That is a harness mistake and worth naming as one: `--output-on-failure` exists
to print the failure, and truncating its output discards exactly the diagnostic
BPR-0131 was written to produce. The gate now keeps the whole run and greps it
for a summary rather than the other way round.

So the count is two observations, neither attributable. The rate is roughly one
in ten full-suite runs.

## Why this is not being fixed now

Two observations, neither carrying the line that would distinguish the two
candidate causes — the first because it predates the instrumentation, the
second because the harness discarded it. Guessing between a stalled stage and a
genuinely false check would put a fix on the wrong side of the question.

The instrumentation is in place and the harness no longer throws it away. What
is missing is one more occurrence, captured.

## Captured, 2026-08-07 — and it is not a stall

The signature reproduced while running `--native-appearance-probe` for
[[BPR-0114]], and this time the budget line survived. Evidence:
`docs/verification/reviews/appearance-probe-open-stage-v001/`.

```
PUBLIC_PATH_BUDGET:expired=0 budget_ms=600000 elapsed_ms=11907 stage_ticks=1077
PUBLIC_PATH_EPOCH_STATUS:Review history is inactive.
PUBLIC_PATH_EPOCH_CORE_STATUS:-1
GITHUB_FIXTURE_FAILED:github.com
GITHUB_FIXTURE_EVIDENCE:11111110:-1:10:9
```

`GITHUB_FIXTURE_EVIDENCE` matches the first observation character for
character. The budget line settles the question BPR-0131 was written to answer:

**`expired=0`, at 11.9 s of a 600 s budget, after 1077 stage ticks.** The
budget did not expire, and the event loop was turning the whole time. The
`open` stage did not stall — it ran, completed, and reported failure. The
candidate cause this card has carried since it was opened is **eliminated**.

What remains is the other candidate: a genuinely false check. One of the eight
fixture presentation facts is false (`11111110`) and no epoch pair resolves
(`-1`), and it does so *fast*.

### Two things this does not establish

1. **The path is different.** This is the appearance-probe entry point, not
   `application_epoch_maintenance_public_path`. The signature is identical and
   that is strong, but "same signature" is not "same cause" until the
   maintenance path is caught with its own budget line showing `expired=0`.
2. **This occurrence was deterministic, and the original was not.** Three
   consecutive probe runs failed identically at ~12 s. The maintenance test
   fails roughly one run in ten. A deterministic failure and an intermittent
   one with the same signature may be one defect reached two ways, or two
   defects — and reasoning from one to the other is exactly the leap this card
   has twice refused to make.

So: the stall hypothesis is dead on the probe path, and the fixture-presentation
fact that reads false is now the thing to identify. That is a bounded
investigation against a deterministic reproducer, which is a considerably better
position than waiting for a one-in-ten flake.

## Attributed, 2026-08-08 — and the answer was in the log

Decoding `GITHUB_FIXTURE_EVIDENCE:11111110`, bit by bit against the order
`main.cpp` prints them:

| bit | fact | value |
|---|---|---|
| 1 | `RecordingConsumed` | 1 |
| 2 | `CapabilityProbeConsumed` | 1 |
| 3 | `BootstrapTransportSelected` | 1 |
| 4 | `AccountTransportSelected` | 1 |
| 5 | `CredentialStored` | 1 |
| 6 | `CredentialRetrieved` | 1 |
| 7 | `CredentialDeleted` | 1 |
| 8 | `SecretRetained` | **0** |

**`11111110` is the all-good pattern, not seven of eight.** The pass condition
requires `!fixtureEvidence.SecretRetained` — a retained secret is the defect, so
a zero there is the credential correctly not surviving its deletion. Every one of
the eight facts is as required.

The smart-call counts match too. `GITHUB_FIXTURE_SMART_EVIDENCE:1:1:1:1` against
`expected_fixture_smart_calls(single) == 1`.

So no fixture condition failed, and `GITHUB_FIXTURE_FAILED` fired because
`passed` was **already false** when the fixture checks ran:

```
passed = aRun.PresentationPassed && status.ShutdownComplete && ...
```

The recorded evidence says which fact, on the two lines this card never read:

```
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0
                    epoch_projection_failed=0 epoch_handoff_failed=0
PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0 render=0
```

`presentation=0`, and everything else in the core group is fine. `main.cpp`'s
own comment on that second line reads *"the first `0` names the group to look
in"* — the first zero is `core`, so `fixture=0` and `maintenance=0` are
**cascading, not independent**. They are the same `passed` narrowed further.

### What this card believed, and what was true

It opened on the reading that the signature meant "one fixture presentation fact
false, no resolved epoch pair, and every maintenance counter zero", and treated
that as consistent with a stalled `open` stage. Every part of that was a
misreading of cascading zeros through a marker whose name says *fixture* while
its condition covers presentation and shutdown.

The stall hypothesis was already eliminated by the budget line (`expired=0` at
11.9 s of 600 s). What remained was never a fixture question at all.

### Closed, with the real defect carded

There is nothing left to attribute: presentation did not pass, `RENDER_FAILED`
appeared in the same log, and the fixture was working correctly throughout.
Why presentation fails in that probe is [[BPR-0114]]'s own blocker and is
recorded there.

The misleading marker is [[BPR-0213]]. `GITHUB_FIXTURE_FAILED` should not be the
line a reader sees when the fixture is fine — it cost this card three
occurrences and two wrong hypotheses.
