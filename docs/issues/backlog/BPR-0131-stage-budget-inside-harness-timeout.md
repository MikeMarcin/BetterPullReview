---
id: BPR-0131
title: Bring the automated stage budget inside the harness timeout
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0119]
---

# Bring the automated stage budget inside the harness timeout

The automated public-path run bounds each stage with an attempt budget, and on
exhaustion it calls `capture_automated_timeout_evidence`, which records the
stage, the epoch state, and the render evidence. That path is the only thing
that makes a hang diagnosable.

For at least one stage the budget outlasts the CTest timeout. BPR-0119 proved
it directly: forcing that stage's readiness condition to never hold produced a
**CTest timeout at 45 seconds** with no `PUBLIC_PATH_VERDICT`, no stage marker,
and no captured evidence — the process was killed from outside before its own
budget expired.

A hang that is killed externally reports nothing. That is precisely the
unattributable failure BPR-0119 spent its effort undoing, and it will recur for
any stage whose budget is larger than the harness allows.

## Why this is separate

BPR-0119 fixed one intermittent failure. This changes timeout behaviour for
*every* automated stage, so it needs its own verification: shortening a budget
can turn a slow-but-correct run on a loaded machine into a failure.

## Required direct evidence

- Every stage's worst-case budget is shown to expire before the CTest timeout
  for that test, with the arithmetic stated rather than assumed.
- A deliberately stalled stage produces `PUBLIC_PATH_VERDICT`, the correct
  stage number, and the captured evidence — not an external kill.
- The budgets still tolerate a slow run: the suite passes repeatedly under
  load, and the margin between the largest budget and the timeout is stated.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
