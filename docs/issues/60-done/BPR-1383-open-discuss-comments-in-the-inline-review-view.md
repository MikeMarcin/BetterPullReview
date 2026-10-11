---
schema: "implement-rfc.card.v1"
id: "BPR-1383"
title: "Open Discuss comments in the inline Review view"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1383--BPR-1383--381c664cbc85.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-0649","BPR-1293","BPR-1356"],"milestone_plan":{"id":"bpr1383-discuss-inline-review","path":"docs/rfcs/plans/bpr1383-discuss-inline-review/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1383:AC1","outcome":"pass","revision":"5484b3fb514cb0f05565e2647aac6b2ca9e725ac","evidence":{"kind":"command","path":"docs/verification/BPR-1383/README.md","detail":"Discuss line button enters Review on the correct file with Comments open and its tab selected, even from collapsed and Notes; composed public path and workbench pass."}},{"criterion":"BPR-1383:AC2","outcome":"pass","revision":"5484b3fb514cb0f05565e2647aac6b2ca9e725ac","evidence":{"kind":"command","path":"docs/verification/BPR-1383/README.md","detail":"Exact thread and first post remain pending across a different-file comparison load, then open at its exact source line; full composition and workbench verify no substituted thread."}},{"criterion":"BPR-1383:AC3","outcome":"pass","revision":"5484b3fb514cb0f05565e2647aac6b2ca9e725ac","evidence":{"kind":"command","path":"docs/verification/BPR-1383/README.md","detail":"Same-file menu pick opens immediately; tooltip/accessibility name destination; PR-wide and missing-current-line anchors retain disabled menu reasons; focused Qt tests pass."}},{"criterion":"BPR-1383:AC4","outcome":"pass","revision":"5484b3fb514cb0f05565e2647aac6b2ca9e725ac","evidence":{"kind":"command","path":"docs/verification/BPR-1383/README.md","detail":"Public composed application course failed before and passes after; 9/9 integration tests including 100/125/200% workbench pass, offscreen render inspected, clang-format 22.1.8 and documentation gate pass, documentation test suite 67/67."}}]
---

# Open Discuss comments in the inline Review view

## Mandate

Owner, 2026-10-10: "we should be able to go from a comment in discuss to the inline comment view on the review tab; then work the card".

## Problem and scope

Discuss's existing Open in code gesture changes modality and line selection, but does not open a collapsed Comments drawer or retain the thread across an asynchronous file load. Reuse the same exact thread route as Threads → Review. The line button's tooltip and accessible name and the thread menu name the destination Open in Review; the button stays keyboard accessible. General comments and unavailable anchors keep their existing unavailability.

## Acceptance

- AC1: A Discuss thread's line button and Open in Review menu action enter Review on that thread's file, open the Comments drawer and select Comments even when another tray tab was selected.
- AC2: The exact thread opens at its first post beside the correct source line after the selected file has loaded, including a different-file asynchronous load. No other thread substitutes for it.
- AC3: Current same-file navigation remains immediate; PR-wide or unavailable code anchors offer no enabled Open in Review menu action.
- AC4: A full composed application course exercises the Discuss gesture through the core, selected Git/cache comparison and rendered inline thread, with fail-before/pass-after evidence; focused Qt checks and documentation gate pass.

## Plan and verification

Single bounded milestone; no new provider, persistence or publication authority. Designed with frontend-design and its desktop guidance, preserving the existing native line button and thread menu. The existing broader RFC human/native criteria remain separate; this repair is judged by direct deterministic application behavior.
