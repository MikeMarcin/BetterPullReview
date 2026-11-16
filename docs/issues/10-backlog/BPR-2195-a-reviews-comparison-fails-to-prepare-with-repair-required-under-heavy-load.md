---
schema: "implement-rfc.card.v1"
id: "BPR-2195"
title: "A review's comparison fails to prepare with repair_required under heavy load"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"seen while investigating BPR-2191, 2026-11-15","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-2191"],"reservation":"docs/issues/_reservations/BPR-2194--BPR-2195--007cce25d132.json"}
---

# BPR-2195: A review's comparison fails to prepare with repair_required under heavy load

While investigating [[BPR-2191]], the review's comparison failed to prepare a
few times when the machine was heavily loaded. The epoch went from
`preparing` to `failed`, and the transition's only diagnostic was
`epoch_reason_code::repair_required`. An opened file then shows as failed. In
`application_pending_states_public_path` this stops the course at "the code
veil shows until the file arrives" after about 12 s:

```text
BPR2191DBG epoch update transition=1 comparison=0 pair=0 avail=0 status=2 accepted=0 pending=0 selected=1 diag=
BPR2191DBG epoch update transition=1 comparison=0 pair=0 avail=0 status=8 accepted=0 pending=0 selected=1 diag=26,
comparison at the gap, 12170 ms: waiting 'review.cpp' held '' context 0 result 0 look 8
FAILED: the code veil shows until the file arrives
```

In that output, `status` is `core::epoch_status` (2 is `preparing`, 8 is
`failed`), `diag` is `core::epoch_reason_code` (26 is `repair_required`), and
`look` is `core::pending_look` (8 is `failed`). The `BPR2191DBG` lines came
from temporary logging in `window_workbench.cpp` that was never committed.

## Where it was seen

| Run | Load | Result |
|---|---|---|
| 6 copies of the course at once, instrumented lane tree (pre-1eab5b868) | very heavy | 2 of 12 failed this way; one also hit it earlier, in the course's opening steps, without failing the course |
| `ctest --repeat until-fail:10`, 1eab5b868, other agents' builds running | load average about 80 | failed at run 9, same signature (look 8 after 12.8 s); no logging, so the reason code was not captured |
| 3 or 4 copies at once, or one under shared build load (load 40 to 47) | heavy | about 45 runs, none failed |

The logs were kept only in that session's temporary scratch folder and may
not survive:
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview--claude-worktrees-v0-5-0-prerelease-targets-52265d/9a077569-92c3-4453-ab25-aee1a83768f1/scratchpad/`
(`q6.log`, `r5.log`, `b2191-fix-pending.log`). The text above is the part
that matters.

## What is not known

Nothing confirms the cause. Many host paths turn a failure into
`repair_required`, in `epoch_host_dispatch_and_imports.cpp` and
`epoch_host_catalog_and_history.cpp`. One is a catalog snapshot whose binding
no longer matches the context's (`start_revision_preparation`); others cover
failed materialization. Which of them fired was not captured. The host's
diagnostic log, which records the stage (`diagnostics.failed`), was inside
the course's temporary profile and was removed with it.

It is also not known whether this happens only with several copies of a
course competing for one machine's resources, or whether one heavily loaded
app can reach it. The copies run with separate temporary profiles but share
the fixture remotes and the system Git.

## Acceptance

- AC01: The stage and reason that turn this preparation into
  `repair_required` are identified from the host's own diagnostics, with the
  course keeping its diagnostic log on failure if it does not already.
- AC02: If the cause is contention between processes or a timing assumption
  in the host, it is fixed, or shown to be a real repair case that the product
  already reports correctly. A deterministic course reproduces the condition.
- AC03: `application_pending_states_public_path` passes `ctest --repeat
  until-fail:10` while other builds load the machine.

## Investigation, 2026-11-16 (lane `claude/v0-5-0-prerelease-targets-52265d`)

AC01's log keeping is done (`ade6b9a55`). When the course fails it copies the
host's diagnostic log from its data root to `BPR_COURSE_LOG_ROOT`, which its
CTest entry sets to `build/dev/tests/end_to_end/course-logs`, and prints
`COURSE_LOGS_KEPT:<folder>`.

**Reproduced once.** In `ctest --repeat until-fail:10`, run 3 of 10 failed
while other agents' builds and 5 concurrent copies loaded the machine (load
about 68): "comparison after 90 s: waiting 'review.cpp' held 'review.cpp'
context 0 result 0 look 3", then "FAILED: the clicked file arrives". The kept
log (`session-*.jsonl`, 307 records) shows the course's repeated reads, about
every 17 s. Each read prepares a revision, and each preparation is cancelled
while running:

| t (s) | record |
|---|---|
| 28.5, 45.3 | `revision_prepare` started; `catalog_prepare` runs beside it; `revision_prepare` failed, domain cache, `cancelled`. The abort succeeds and the epoch reports cancelled |
| 62.1 | the same, then 0.2 s later `revision_prepare` failed, domain epoch, **`repair_required`** |
| 78.8 | the same again, cancelled, then `repair_required` |
| 95.5 | the next read: **`catalog_recovery` failed `repair_required`**, and the clicked file never arrives |

**The stage.** `start_revision_preparation`'s `cancelPrepared`
(`epoch_host_dispatch_and_imports.cpp`) calls `abort_preparation`
(`epoch_host_requests_and_recovery.cpp`). That returned false, so the
cancelled preparation became `repair_required`. Its operation record is then
left in the repair phase, and the next read's catalog recovery stops on it.
The binding check the card names (`snapshot->Binding != binding()`) is not
the one that fired: `catalog_prepare` succeeded in each preparation.

**Not yet known: which step of `abort_preparation` fails.** It returns false
for any of these: the record already past the point of no cancellation; the
`abort_requested` append; the cache open; the recovery request; the cache's
`abort_recovered_operation` (tried twice); the `aborted` append. A temporary
probe on each of these branches, which was not committed, never fired:

| Run | Load | Result |
|---|---|---|
| 5 copies x 3 rounds, beside the ctest run | 30 to 68 | 15/15 pass |
| 6 copies x 3 rounds, probes in | 6 to 21 | 18/18 pass, no abort failure |
| 6 copies x 2 rounds, 40 CPU burners | about 100 | 12/12 pass, no abort failure |

CPU load alone does not reproduce it. The one failure ran beside compiles, so
disk and SQLite contention are the likely difference. In each failing
preparation a `catalog_prepare` ran on the same catalog just before the
cancel. The leading suspect is therefore one of the two catalog appends in
`abort_preparation` (`abort_requested` or `aborted`), refused as `busy` or as
a revision or phase conflict because another write landed in between.

**Next.** Make `abort_preparation` say which step refused, through the
host's diagnostics (a new `diagnostic_epoch_stage` for the abort, with the
catalog or cache failure). Then run the course under compile-like I/O load
and read the kept log. If the refusal is a transient `busy` or conflict, the
abort should retry or re-read the record rather than mark a cancelled
preparation for repair. AC02 needs a deterministic host course: a catalog
port that refuses the `abort_requested` append once.
