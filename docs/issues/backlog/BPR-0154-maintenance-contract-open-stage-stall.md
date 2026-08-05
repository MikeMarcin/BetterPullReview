---
id: BPR-0154
title: Attribute the epoch-maintenance open-stage stall if it recurs
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0131]
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

## Why this is not being fixed now

One observation in five runs, with evidence that cannot distinguish the two
candidate causes, is not enough to change behaviour on. The instrumentation
that would settle it is now in place; what is missing is a second occurrence.
