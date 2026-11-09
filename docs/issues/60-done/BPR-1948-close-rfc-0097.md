---
schema: "implement-rfc.card.v1"
id: "BPR-1948"
title: "Verify and close RFC-BPR-0097"
status: "Done"
rfcs: ["RFC-BPR-0097"]
closeout_for: ["RFC-BPR-0097"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"go-to-line-r1","assigned_implementer":"/root/go_to_line","reservation":"docs/issues/_reservations/BPR-1948--BPR-1948--71d1d557b9a1.json","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"e3e600c9f5483bcec66b140d0473d232b4ae7564","evidence":{"kind":"observation","detail":"Production Git/cache Go to line in both presentations at 100/125/200%; 13-row integration plus strengthened native-cursor/cancellation course. Exact-line, draft, stale identity, input/focus, accessible/minimum layout and bounded-work assertions pass. See docs/verification/RFC-BPR-0097.md."}},{"criterion":"AC02","outcome":"pass","revision":"e3e600c9f5483bcec66b140d0473d232b4ae7564","evidence":{"kind":"observation","detail":"Production Git/cache Go to line in both presentations at 100/125/200%; 13-row integration plus strengthened native-cursor/cancellation course. Exact-line, draft, stale identity, input/focus, accessible/minimum layout and bounded-work assertions pass. See docs/verification/RFC-BPR-0097.md."}},{"criterion":"AC03","outcome":"pass","revision":"e3e600c9f5483bcec66b140d0473d232b4ae7564","evidence":{"kind":"observation","detail":"Production Git/cache Go to line in both presentations at 100/125/200%; 13-row integration plus strengthened native-cursor/cancellation course. Exact-line, draft, stale identity, input/focus, accessible/minimum layout and bounded-work assertions pass. See docs/verification/RFC-BPR-0097.md."}},{"criterion":"AC04","outcome":"pass","revision":"e3e600c9f5483bcec66b140d0473d232b4ae7564","evidence":{"kind":"observation","detail":"Production Git/cache Go to line in both presentations at 100/125/200%; 13-row integration plus strengthened native-cursor/cancellation course. Exact-line, draft, stale identity, input/focus, accessible/minimum layout and bounded-work assertions pass. See docs/verification/RFC-BPR-0097.md."}}]
---

# BPR-1948: Implement and close Go to line

Implement [RFC-BPR-0097](../../rfcs/RFC-BPR-0097-go-to-line.md) as one milestone.
All AC01–AC04 and the behavior they name remain unchanged. No human criterion.

## Acceptance

- AC01: Eligible active main diff menu/hotkey parity, platform help, focus and editing scope; preserve Find keys.
- AC02: Split/unified exact Base/Head navigation including unchanged, changed-group interiors, last and repeated offscreen lines; correct cursor and subsequent draft.
- AC03: Invalid input, unavailable side and cancellation preserve selection; stale file/tab/comparison requests refuse.
- AC04: Composed keyboard/menu flows, accessible focus states and full-window minimum-size scale renders; preserve shared geometry and bounded work.

## Verification and execution record

Accepted on `codex/go-to-line`. Product candidate
`52c8469b6a1c9d5fd9fc0fe7a6c39469abc73841`; strengthened final test candidate
`e3e600c9f5483bcec66b140d0473d232b4ae7564` has identical product bytes.
All 13 integration rows pass (334.83s), and the final three-scale native-cursor
and unified-cancellation course passes (66.58s). Format 22, focused tidy,
documentation structure and all 67 documentation controls pass. The disabled
navigation-dispatch control fails the exact-line assertion; the restored
candidate passes. [Verification](../../verification/RFC-BPR-0097.md).

Originally imported as RFC-BPR-0096, then renumbered to RFC-BPR-0097 to preserve
another active main import; BPR-1948 and its complete criteria are unchanged.
Main integration is deliberately left with the coordinator while that writer
is active; no feature publication or main-source overwrite was performed.
RFC base: `663137a9ed3825c87934c93926d414725f8bee22`.
[Checkpoint](../_checkpoints/go-to-line-v1/handoff.md).

## Closeout checklist

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Menu and hotkey invoke the same dialog for the eligible active diff, and the shortcut help shows the platform-correct binding. Ineligible views and editable fields obey Entry point and scope, and macOS Find keys retain their current behavior. | Composed Go to line at 100/125/200%, native cursor, draft, stale state, scale rasters and bounded work; see verification ledger | Pass | `e3e600c9f5483bcec66b140d0473d232b4ae7564` |
| **AC02:** In split and unified views, distinguishable Base/Head content demonstrates correct default-side selection and exact navigation to entered source numbers, including unchanged context, a changed-group interior and the last source line. Returning to an already selected offscreen line works; keyboard navigation and a subsequent draft target that line. | Composed Go to line at 100/125/200%, native cursor, draft, stale state, scale rasters and bounded work; see verification ledger | Pass | `e3e600c9f5483bcec66b140d0473d232b4ae7564` |
| **AC03:** Invalid input, an unavailable side and cancellation follow the dialog behavior without moving or replacing the source selection. A file, tab or comparison invalidated while the dialog is open cannot receive the old request. | Composed Go to line at 100/125/200%, native cursor, draft, stale state, scale rasters and bounded work; see verification ledger | Pass | `e3e600c9f5483bcec66b140d0473d232b4ae7564` |
| **AC04:** Through the composed production workbench, keyboard-only and menu flows demonstrate the defined focus, selection and accessible dialog states in both presentations. Full-window renders at minimum supported size and supported scaling show usable controls and a visible target; navigation retains the shared scroll geometry and the stated bounded-work behavior. | Composed Go to line at 100/125/200%, native cursor, draft, stale state, scale rasters and bounded work; see verification ledger | Pass | `e3e600c9f5483bcec66b140d0473d232b4ae7564` |
