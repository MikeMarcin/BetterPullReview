---
schema: "implement-rfc.card.v1"
id: "BPR-0818"
title: "Epoch refresh drops the review and issue-comment conversation roots"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"integration run at the BPR-0810/BPR-0811 merge; confirmed at a clean 8b2bea5b worktree","found_at":"8b2bea5b83639c78f69f431e24a724e0fccdb3e9","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0818: Epoch refresh drops the review and issue-comment conversation roots

## Defect

`application_review_behaviour_contract` (`tests/end_to_end/CMakeLists.txt:1876`)
fails on macOS. It failed at `f33a10c9`, and again, twice, at a clean scratch
worktree of `8b2bea5b`, which is before that merge. Each run reports:

    RUN_BUDGET_EXPIRED: elapsed_ms=40053 budget_ms=40000 over_ms=53 stage=4
    EPOCH_REFRESH_PRESENTATION_IDENTITIES
      expected: review:PRR_fixture_1, review-thread:PRRT_fixture_1, review-comment:PRRC_fixture_1,
                review-thread:PRRT_fixture_3, review-comment:PRRC_fixture_4, review-comment:PRRC_fixture_5,
                review-thread:PRRT_fixture_2, review-comment:PRRC_fixture_2, review-comment:PRRC_fixture_3,
                review-thread:PRRT_fixture_file, review-comment:PRRC_fixture_file, issue-comment:IC_fixture_1
      actual:   the same without review:PRR_fixture_1 and issue-comment:IC_fixture_1
    expected conversation roots 6, actual 4

The marker is printed at `src/app/main.cpp:4427`. After an epoch refresh, the
review summary root and the issue-comment root no longer appear in the
presented conversation. Two things are unknown:

- whether the budget overrun is a cause, a consequence of waiting for roots
  that never arrive, or a separate slowdown;
- when the regression started. A red architecture gate and a nested checkout
  on `main` hid failures like this one for days.

## Scope

- Find the first bad revision and the mechanism. It may be a product
  presentation regression (for example from RFC-BPR-0028 conversation or
  drawer changes, or RFC-BPR-0032 Home and accounts work), or a fixture or
  course drift.
- Fix the product if a person no longer sees review summaries or issue
  comments after a refresh. Change the course only if an accepted RFC decision
  removed those roots, and cite that decision.
- Name what fills the 40 s budget at stage 4. Do not raise the budget to make
  the course pass.

## Acceptance

- `application_review_behaviour_contract` passes on macOS within its budget,
  and the cause is named on this card.
- The Qt conversation courses covering the changed presentation still pass.
