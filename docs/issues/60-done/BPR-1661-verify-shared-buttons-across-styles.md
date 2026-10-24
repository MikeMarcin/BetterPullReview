---
schema: "implement-rfc.card.v1"
id: "BPR-1661"
title: "Verify shared buttons across themes and platform styles"
status: "Done"
rfcs: ["RFC-BPR-0087"]
depends_on: ["BPR-1660"]
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1661:AC03-AC04","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Reconciled first-slice18/18 and app-rollout6/6 four-theme courses at1x/1.25x/2x Fusion/Windows styles, retained actual complete windows and hover/press/focus/disabled captures, inspected six additional representative captures. Final4/4 composition/interaction smoke passes at bb64e195d; source unchanged."}},{"criterion":"BPR-1661:platform-scope","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Recorded macOS arm64 Qt6.11.2 offscreen style evidence separately from unobserved native macOS/Windows/Linux desktop integration and owner judgment. Retained the unrelated full Appearance Fusion2x tooltip failure and exact native-choice reproduction; focused shared-buttons2x and normal-scale full Appearance pass."}}]
---

# BPR-1661: Verify shared buttons across themes and platform styles

## Acceptance

- Record RFC AC03–AC04 evidence at 1x, 1.25x and 2x across the four themes.
- Record which native macOS/Windows/Linux behaviors were observed versus offscreen layout evidence; fix regressions rather than claim missing results.

## Verification result

Both acceptance bullets pass for source `81a13267446e883c12ac0f8c7b5a43927c2efc68`
with unchanged relevant first-slice evidence from `9dc946997350ab23866ce75f39bbebf32b140be8`.
[Final evidence and criterion judgment](../../verification/RFC-BPR-0087.md#final-verification-and-closeout-proposal)
consolidate 18 first-slice and 6 app-rollout four-theme courses at 1x, 1.25x
and 2x under Fusion/Windows styles, actual composed captures and input checks.
The final serial smoke at `bb64e195d93b2a46e35352659cd6e23686337ed5` passes
4/4 (7.39 s), including composed GitHub, shared Settings, normal-scale full
Appearance and quiet push-button interaction.

All platform claims are bounded: macOS arm64, Qt 6.11.2, offscreen with Fusion
and Windows **styles**. Native macOS appearance integration and native
Windows/Linux desktop behavior were not observed. No human visual verdict is
claimed. The unrelated full Appearance Fusion-2x popover-placement failure
was reproduced with the original native choices and remains explicit in the
ledger; the relevant `--shared-buttons` courses pass at 2x.
