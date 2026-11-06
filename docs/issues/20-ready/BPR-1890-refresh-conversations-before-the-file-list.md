---
schema: "implement-rfc.card.v1"
id: "BPR-1890"
title: "Refresh conversations before the file list"
status: "Ready"
rfcs: []
depends_on: ["BPR-1889"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1890--BPR-1890--32071319c6e9.json","milestone_plan":{"id":"responsiveness-latency-v7","path":"docs/rfcs/plans/responsiveness-latency-v7/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; retain the existing partial loading plate and conversation layout."}
---

# BPR-1890: Refresh conversations before the file list

The owner requested continued direct responsiveness work, particularly independent
loads with clear loading states, preserving other behavior and landing each round
on main with a card and one-line journal entry.

A refresh still reads every changed-file page before its reviews and threads.
When the accepted head and base are unchanged, the host already has the files
needed to validate and display that conversation. This independent catalogue
read should not hold up new replies on the file the reviewer is reading.

## Acceptance

- AC1: An observed authenticated refresh can publish its complete reviews and
  threads before changed-file pagination; first-open, signed-out and unobserved
  reads retain their order. The final snapshot, read count, shared limits,
  validation, error and cancellation obligations remain intact.
- AC2: Core and host admit metadata followed by files and conversation in either
  order, once each. Only an exact current refresh with the same accepted head,
  base and mode can update the conversation; no new snapshot or epoch authority
  is granted, and stale, duplicate or terminal observations remain excluded.
- AC3: Through the composed offscreen application, a new reply appears while
  the file-list read is held, the comparison and reading position remain, and
  the existing "Refreshing remaining details…" plate stays visible until the
  refresh completes. The dependency-breaking control fails on the old order.
- AC4: Record optimized before/after latency and focused checks, add one journal
  line, complete the card and land the verified round on main.

## Plan

Move the refresh's file read behind its complete conversation and ahead of
viewed-file validation. Keep a single paged-read execution and its bounds. Let
the core admit the two independent observations after metadata; bound host
callback posts to one of each. Reuse the existing refresh partial plate without
changing its layout. Test file failures/cancellation after the early reply as
well as the final successful records and the composed held-file path.
