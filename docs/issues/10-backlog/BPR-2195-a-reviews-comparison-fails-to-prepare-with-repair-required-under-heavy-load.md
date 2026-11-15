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
