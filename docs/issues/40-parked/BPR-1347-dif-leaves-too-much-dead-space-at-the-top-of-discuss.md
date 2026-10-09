---
schema: "implement-rfc.card.v1"
id: "BPR-1347"
title: "Dif leaves too much dead space at the top of Discuss"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"polish","origin":"BPR-1091 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1091-workshop-candidate-line-v002 Accept note","decision_id":"945adb1b-962e-45ec-a113-3534de2187e5","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1091","BPR-1209","BPR-1212","BPR-1346"],"acceptance":["BPR-1347:space","BPR-1347:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1347:space","outcome":"pass","revision":"e5063a79c","evidence":{"kind":"command","path":"tests/adapters_qt/workbench_window_tests.cpp","detail":"qt_dif_workshop_placement measures review bar to Back to the code on composed Discuss with an offered proposal, Dif shown and hidden: before 100 and 58 (fails), after 54 and 54 at 1x and 2x; Dif at 28 px before the workshop title, name kept, Accept and Reject visible. Focused rows 28/28 at 2f9682d21 and 8/8 at e5063a79c; DOCUMENTATION_OK. macOS."}},{"criterion":"BPR-1347:owner-look","outcome":"pending","revision":"e5063a79c","evidence":{"kind":"observation","detail":"Session 41bb446e7df8954d6162fe3592b78cec, candidate BPR-1347-dif-workshop-row-v001 (fingerprint 4ee8a611b164...), frozen root /Users/jaeger/bpr1346-1347-v001.wegp_apd, ready at cursor 6."}}]
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

## What changed (`e5063a79c`)

Cause: `WorkshopHeader` put Dif in a 64-pixel slot after a stretch, at the
far right of a row that spans the whole centre, with the style's default
layout margins. The row was about 86 pixels tall, and over the composition
column it read as an empty band with Dif in its corner; "Comment workshop"
and its buttons sat at the row's other end, above the threads list.

Design (frontend-design loaded): Dif belongs beside what he illustrates.
He now opens the workshop's row, before "Comment workshop", Accept revision
and Reject revision, at the corner's 28 pixels (`dif_workshop_size`). The
row's top and bottom margins are 4 pixels and a 28-pixel strut keeps its
height when Dif is hidden, so Settings' hide leaves neither a hole nor a
height change. The slot keeps its object and accessible names, and the
decision stays visible. `docs/ux/workbench-specification.md` records it.

## Evidence (`BPR-1347:space`, pass)

- `qt_dif_workshop_placement`
  (`test_dif_uses_the_corner_except_for_prime_spots`) now measures, on the
  composed window with a review draft and an offered proposal in Discuss,
  the distance from the review bar's bottom to "Back to the code"'s top with
  Dif shown and hidden in Settings. It asserts the two are equal and at
  most 56, that hiding Dif leaves the workshop's title at the row's start
  and Accept and Reject visible, and that Dif returns at 28 pixels before
  the title with his name.
- Before the change: 100 with Dif, 58 without, and the row fails ("Dif adds
  no height above the composition"). After: 54 and 54, at 1x and 2x.
- Renders: the top of Discuss with Dif, light and dark, 1x and 2x, and the
  before, inline in the session. The app's own workshop-look render grabs
  before Dif arrives, so the window renders come from the placement row.
- Rows: as recorded on [[BPR-1346]] (the same runs at `2f9682d21` and
  `e5063a79c`). macOS only.

## Owner look, session v001 (pending)

One new short session, two candidates, one Accept or Reject question each
(shared with [[BPR-1346]]).

- Session `41bb446e7df8954d6162fe3592b78cec`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1346-1347-v001`,
  repo root the main checkout, ready and protected at event cursor 6. The host
  was started detached and answered on loopback; the offline page is
  `review.html` in the state directory.
- Candidate `BPR-1347-dif-workshop-row-v001` (revision `frozen-e5063a79c-v001`, fingerprint
  `4ee8a611b164…`), criterion `BPR-1347:owner-look`, with offscreen renders inline.
- Frozen root `/Users/jaeger/bpr1346-1347-v001.wegp_apd`, made by
  `scripts/freeze_attended_build.py freeze` from `e5063a79c` (fixture
  `conventional-comments-v1`, file `src/engine/renderer.cpp`, the
  `--attended-workshop-agent` look with the fixture agent, ready line
  `ATTENDED_WORKSHOP_SHOWN`). Its own `--check` reached
  `ATTENDED_WORKSHOP_SHOWN` on head `b20cb443…`; executable SHA-256
  `804f06b44c3970811e96a2f782caeae00dc41c26802573a896d327afb1f7312f`.
- Launch: `python3 /Users/jaeger/bpr1346-1347-v001.wegp_apd/launch.py`.

## Next

Read session `41bb446e…` from cursor 6. Accept on `BPR-1347-dif-workshop-row-v001` satisfies
`BPR-1347:owner-look` and the card finishes; a Reject comes back with the change in its
note. When both cards are settled, remove the root with
`freeze_attended_build.py remove`. Windows and Linux runs of the rows are
owed to the stations.

## Linux station run (bpr-main-linux, 2026-10-09)

At `b0ef8d7b9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner: `qt_feedback_label_controls` and `qt_dif_workshop_placement` pass.

## Windows station run (bpr-main-win, 2026-10-09)

At `7672d5a8b`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `qt_feedback_label_controls` and `qt_dif_workshop_placement`.
