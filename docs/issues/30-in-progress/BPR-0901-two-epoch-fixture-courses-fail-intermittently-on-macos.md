---
schema: "implement-rfc.card.v1"
id: "BPR-0901"
title: "macOS: two epoch fixture courses fail intermittently, and they alternate"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-006 integrator, across four separate macOS runs at the merged head","found_at":"81844fef","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0901: macOS: two epoch fixture courses fail intermittently, and they alternate

## Defect

`application_overview_since_review_contract` and
`application_review_behaviour_contract` each fail **some** of the time on
macOS, at the epoch fixture's seed or reuse phase. Neither fails reliably, and
in four runs at the same commit they never failed together — when one failed,
the other passed.

| Run | #342 `overview_since_review` | #343 `review_behaviour` |
|---|---|---|
| 13 affected rows, merge verification | **pass**, 95.41 s | **FAIL**, 40.50 s |
| #343 alone, isolated | — | **pass**, 93.86 s |
| full 382-row sweep | **FAIL**, 117.82 s | **pass** |

The two failures are the same shape:

- **#342**: `EPOCH_FIXTURE_REUSE_FAILED`,
  `PUBLIC_PATH_EPOCH_STATUS:Review history is unavailable.`,
  `PUBLIC_PATH_EPOCH_CORE_STATUS:7`, `RENDER_FAILED`, child exit 1.
- **#343**: `EPOCH_FIXTURE_SEED_FAILED`,
  `EPOCH_EFFECT_FAILED:revision-prepare:5`,
  `GITHUB_FIXTURE_FAILED:github.com`, `REVIEW_LEDGER_FAILED`, child exit 1.

Both print `PUBLIC_PATH_FAILED`, which is a `FAIL_REGULAR_EXPRESSION`, so these
rows would have failed the same way before [[BPR-0890]]'s exit-code work — the
new instrumentation is **not** what fails them. It is worth saying that plainly,
because both failures were first seen in a tree that also carried BPR-0890's
work in progress, and the obvious reading was wrong.

## What is known, and what the discriminating experiment is

Both rows are among the **twelve courses [[BPR-0875]]'s fix changed** at
`81844fef`. Each gained `bpr_private_data_root(BPR_ROOT)`, which moves the data
root down one level (`<binary>/<row>` becomes `<binary>/<row>/data`) and makes
the new parent mode 0700 on POSIX. Their fixture roots are derived from
`BPR_ROOT` *after* the rewrite, so no stale absolute path survives — but the
paths are longer and the parent's mode changed, and this dispatch has already
been bitten once by a path-length limit ([[BPR-0881]]).

Against that: dsp-005's macOS sweep at `2b2e5549` was 378 of 381 with **only**
the three [[BPR-0893]] footprint rows failing, so neither of these failed there.
That is one data point, not a proof of stability — this dispatch ran these two
rows more times in one afternoon than the run had before, which is its own
explanation for why an old flake would surface now.

The experiment that separates the three hypotheses in one pass:

1. run both rows several times at `HEAD`;
2. run both rows several times with **only** those two
   `tests/end_to_end/*.cmake` course scripts reverted to their pre-`81844fef`
   content.

That distinguishes "the twelve-course change made them flaky" from "they were
always flaky" from "something else in this dispatch is involved". Do that
before changing anything.

## Scope

- Run the experiment above first. Do not fix on a hypothesis.
- If the extra level or the 0700 parent is implicated, check the other ten
  changed courses too rather than only these two — they took the same change.
- If it is a pre-existing flake, find what is actually racing.
  `EPOCH_EFFECT_FAILED:revision-prepare:5` and
  `PUBLIC_PATH_EPOCH_CORE_STATUS:7` both name specific states and are the
  thread to pull.
- Whatever the cause, these two rows are the suite's own evidence about epoch
  fixtures. An intermittent row is worse than a red one, because it teaches
  people to rerun.

## Acceptance

- The card names which of the three hypotheses is true, with the runs that
  showed it.
- Both rows pass repeatedly — say how many consecutive runs were taken as
  enough, and why.
- If the cause is shared with the other ten changed courses, they are checked
  too.
- Linux and Windows are checked for the same fault, or the card says why they
  cannot be affected.
