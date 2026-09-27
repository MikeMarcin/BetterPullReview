---
schema: "implement-rfc.card.v1"
id: "BPR-1221"
title: "Show tour stop status before the number"
status: "Done"
rfcs: ["RFC-BPR-0039"]
depends_on: ["BPR-0995"]
metadata: {"milestone":"qualification-closeout","milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"},"owner_amendment":"2026-09-27 hand-back 90: replace trailing em-dash Editing with a compact icon to the left of the number"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"95b71e99cc658871a886df66a7a7be22dd2fe3e7","evidence":{"kind":"observation","detail":"Four distinct vector status shapes before the number; no trailing label. Exact Cocoa course and native screenshot pass."}},{"criterion":"AC2","outcome":"pass","revision":"95b71e99cc658871a886df66a7a7be22dd2fe3e7","evidence":{"kind":"observation","detail":"AccessibleTextRole and tooltip include ordinal/title/state for all four statuses; actual native row accessible Name query passes."}},{"criterion":"AC3","outcome":"pass","revision":"95b71e99cc658871a886df66a7a7be22dd2fe3e7","evidence":{"kind":"observation","detail":"Exact Cocoa composition passes normal/monochrome selected/unselected shape comparisons, DPR2, normal/large font, local structural selection/undo/SQLite/no-effects; separate grip hit area preserved."}},{"criterion":"AC4","outcome":"pass","revision":"95b71e99cc658871a886df66a7a7be22dd2fe3e7","evidence":{"kind":"observation","detail":"Owner Accept at session 7834eb10dc5260f8cc14257a669c1109, event 98 / hand-back 99, candidate BPR-1013-layout-ordinary, fingerprint 5440d0b5b6e774fa8309cfefebcd08454a96d9d3a451e50e9871b04f92929a8e, decision 92b832d8-a930-44fe-a09d-3d979dc05180; exact r4 binary SHA256 2159aaf20cd8de14c66c4f6157d5dd6ca6e22338661f4cefd81fda6e2dc2db98."}}]
---

# BPR-1221: Show tour stop status before the number

The owner requested a follow-up after accepting r3's ordinary layout: replace
the trailing status text with a legible status icon before the stop number.
Keep the six-dot drag handle separate and preserve the core preparation state.
This independently eligible presentation slice belongs to qualification-closeout,
mandate `r1-20260926-land-each-milestone`.

## Acceptance criteria

- **AC1:** Suggested, Editing, Ready and Needs attention have distinct compact
  shapes before the stop number; the title has no appended preparation label.
- **AC2:** Every row exposes its complete ordinal, title and preparation state
  through its accessible name and hover tooltip. Status cannot depend on color.
- **AC3:** The real composed native course verifies selected/unselected rendering,
  high contrast and larger scale/font legibility, while the separate drag handle,
  selection, keyboard reordering and local editing retain their behavior.
- **AC4:** The owner judges the revised row affordance in the affected
  ordinary-layout attended candidate. Existing accepts stay bound to their exact
  source/binary; this card remains human-pending until that verdict.

## Scope and evidence

Qt presentation and the existing public composition course only. No core state,
provider authority, persistence format or remote effects change. Keyboard feedback
correction remains BPR-1013's existing AC39-06 obligation.

## Implemented and owner accepted

Source `95b71e99cc658871a886df66a7a7be22dd2fe3e7` paints palette-derived vector shapes before the ordinal,
keeps the grip separate, and retains full tooltip/accessible labels. The exact
Cocoa composition verifies all four states at normal/large font and DPR2 in
selected/unselected normal and monochrome palettes, live accessible-row naming,
viewport focus feedback, structural selection/undo and local-save/no-effects.
The [r4 evidence ledger](../../verification/RFC-BPR-0039-0043.md#post-review-r4--hand-back-90-and-bpr-1221)
records the physical observations and test-driver correction.

AC1–AC3 retain the exact r4 verification above. AC4 passed at event **98**,
hand-back **99**, in session `7834eb10dc5260f8cc14257a669c1109`: the owner
accepted `BPR-1013-layout-ordinary`, fingerprint
`5440d0b5b6e774fa8309cfefebcd08454a96d9d3a451e50e9871b04f92929a8e`,
decision `92b832d8-a930-44fe-a09d-3d979dc05180`. That verdict names source
`95b71e99cc658871a886df66a7a7be22dd2fe3e7` and binary SHA-256
`2159aaf20cd8de14c66c4f6157d5dd6ca6e22338661f4cefd81fda6e2dc2db98`.
All four criteria pass. This completed row-presentation slice leaves BPR-1013's
other platform, live-service and installed-provider/model requirements open.
