---
schema: "implement-rfc.card.v1"
id: "BPR-1347"
title: "Dif leaves too much dead space at the top of Discuss"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"polish","origin":"BPR-1091 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1091-workshop-candidate-line-v002 Accept note","decision_id":"945adb1b-962e-45ec-a113-3534de2187e5","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1091","BPR-1209","BPR-1212","BPR-1346"],"acceptance":["BPR-1347:space","BPR-1347:owner-look"]}
---

# BPR-1347: Dif leaves too much dead space at the top of Discuss

## Owner note

In attended session `0d079940affe6da5c393df07d82a22d8` the owner accepted
[[BPR-1091]]'s workshop candidate line (`BPR-1091-workshop-candidate-line-v002`,
decision `945adb1b-962e-45ec-a113-3534de2187e5`, event 10, 2026-10-09T04:07:11Z)
with two followups. This card is the second, verbatim:

> followup: Dif has become a liability here, there's way too much deadspace around him.

The attachment that goes with it shows Discuss (⌘3) with a review draft open
for `renderer.cpp`, Line 1: under the review bar (File 5 of 17, Changes 2) a
band roughly 100 logical pixels tall (the capture is 2x) holds nothing but Dif, small, at its right
end; "Back to the code" and "Line 1" sit below the band, then the draft's
excerpt. The band is empty to the left of Dif. The first followup, the label
picker's icon gap, is [[BPR-1346]].

## What exists

- `src/adapters/qt/src/workbench_window.cpp`: `WorkshopHeader` (object name
  `workbenchWorkshopHeader`) is inserted above `CentreStack`; it holds
  "Comment workshop", the workshop's Accept and Reject buttons, a stretch and
  `DifWorkshopSlot` (64 by 64). `update_dif_presentation` places Dif there
  while `showing_dif_workshop()` ([[BPR-1209]]: the offered workshop is a
  prime spot), and in the 28-pixel status corner everywhere else.
- The composition page ([[BPR-1212]], `makeCompositionPage`) puts "Back to
  the code" and the title at the top of its own column, under that header.
- `qt_dif_workshop_placement` pins where Dif goes.

## Scope

- Find what makes the band tall when only Dif is in it, and remove the dead
  space: Discuss's composition starts right under the review bar, and Dif takes
  no more room than its own drawing. Whether Dif stays in the band (beside
  "Back to the code", say) or moves to the status corner on this page is the
  design question; settle it with the frontend-design skill and show the owner.
- Dif never hides a semantic control (the workshop's Accept and Reject stay
  visible, as `WorkshopHeader`'s comment requires), and hiding Dif in Settings
  leaves no gap.

## Acceptance

- `BPR-1347:space`: a Qt test on the composed Discuss page with a review draft
  open measures the gap between the review bar and "Back to the code" with Dif
  shown and hidden, and fails against the code before the change.
- `BPR-1347:owner-look`: the owner accepts Discuss's top with Dif in one short
  attended session on a frozen build (one Accept/Reject).

Load `frontend-design:frontend-design` before the visual change.
