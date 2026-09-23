---
schema: "implement-rfc.card.v1"
id: "BPR-0864"
title: "The workbench composition probe's remaining phases can still expire the run budget with only a stage number"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005, from the BPR-0860 fix","found_at":"7d548233","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0864: The workbench composition probe's remaining phases can still expire the run budget with only a stage number

## Defect

[[BPR-0860]] capped the workbench composition probe's drawer-width phases and
gave them named terms, because an uncapped wait there burned the whole 40-second
run budget and reported nothing but `RUN_BUDGET_EXPIRED … stage=2`. Diagnosing
it needed instrumentation and a hunt for the introducing commit.

The same probe's other phases in
`src/app/src/github_application.cpp` are still uncapped and would fail the
same unhelpful way:

- `awaiting_observation`
- `awaiting_diff`
- `awaiting_change_projection`
- `awaiting_invalidation`
- `awaiting_failure`

A stall in any of them costs a full budget and a fresh investigation, when a
named term plus the phase's own state would cost one run. BPR-0860's
`stall(term)` lambda and its `WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED` /
`WORKBENCH_DRAWER_WIDTH_STORE_STALLED` / `WORKBENCH_FILES_DRAWER_EXPAND_STALLED`
terms are the pattern to extend; the work is mostly choosing what each phase
should print.

This is not a live failure: every phase passes today. It is a diagnosability
debt with a known, cheap shape, carded so the next stall is not another bisect.

## Scope

- Give each remaining phase a bounded wait, well inside the run budget, and a
  named term.
- Print the state that phase actually depends on, the way the width terms
  print splitter sizes, drawer state, modality and outstanding requests.
- Reuse BPR-0860's `stall()` lambda rather than adding a second mechanism.
- `src/app/src/github_application.cpp` is pinned by the round-trip contract, so
  the change reseals with a recorded review.
- Check whether sibling probes in the same file share the pattern; if they do,
  say so here rather than widening this card silently.

## Acceptance

- No phase of the workbench composition probe can reach `RUN_BUDGET_EXPIRED`
  before naming why; each names its own stall.
- Each named term is shown to fire, by a deliberately broken run per phase or
  by an equivalent check.
- The row still passes on macOS and Linux.
