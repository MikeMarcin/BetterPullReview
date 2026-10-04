---
schema: "implement-rfc.card.v1"
id: "BPR-1275"
title: "On Windows the desktop's first capture from Open Project takes 14 times as long as bpr cycle open"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"origin":"BPR-1184 large-project course, Windows Release at 61c96b9a3 (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1275--BPR-1276--cc9fb8ce4d80.json","related_cards":["BPR-1184","BPR-1276"]}
---

# BPR-1275: On Windows the desktop's first capture from Open Project takes 14 times as long as bpr cycle open

## Observation

[[BPR-1184]]'s `application_review_cycle_large_project` measured these on
Windows 11, i9-10900KF, Release at `61c96b9a3`, with the host otherwise idle,
on the same 20,000-file folder:

- desktop Open Project, cold (create, bind, first capture, list):
  110,983.6 ms;
- `bpr cycle open`, cold: 7,865.1 ms;
- macOS desktop, same course: 2,112.0 ms.

The desktop's later captures keep pace with the terminal's (unchanged:
6,235.1 ms desktop, 5,849.2 ms terminal), so only the first capture from Open
Project is slow. On the 100,001-file folder the cold open was still capturing
when the gesture's 540 s ran out ("Capturing: 86,016 files read"), so the
Windows course writes no Release report. Earlier runs went silent at the same
step.

## Outcome

Profile the desktop's Open Project path against `bpr cycle open` on Windows.
Find out whether it is slow throughout or stops in stretches, and what the
desktop path does that the terminal's does not. Fix it at its cause, so the
cold open is within a small factor of the terminal's on the same folder.

## Acceptance

- The cause is named, with the measurement that shows it.
- An optimized-build timing pin (the [[BPR-0905]] / [[BPR-0967]] pattern) fails
  with the cause restored (negative control) and passes with the fix.
- The [[BPR-1184]] Windows Release measurement completes and its
  `windows-release-report.json` is committed byte for byte.
- Layering (`architecture_boundaries`) and the round-trip composition pin stay
  green.
