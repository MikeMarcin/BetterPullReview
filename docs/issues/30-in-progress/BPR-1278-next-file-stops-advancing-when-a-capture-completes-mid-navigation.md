---
schema: "implement-rfc.card.v1"
id: "BPR-1278"
title: "Next file stops advancing when a capture completes mid-navigation"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1274 Windows Debug large-project row at 8d838c696 (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1278--BPR-1278--d362f90a34fd.json","related_cards":["BPR-1274","BPR-1184"]}
---

# BPR-1278: Next file stops advancing when a capture completes mid-navigation

## Observation

[[BPR-1274]]'s Windows Debug run of `application_review_cycle_large_project`
(`build/windows-dev-tour-cli` at `8d838c696`) failed in its 2,000-file pass on
the tree corpus, in the course's navigate-during-capture step:

```text
navigate-during-capture …: Next file reads the file after d000/s0/f000008.txt
```

The window's state then was `source d000/s0/f000008.txt @ R1`, `revisions
[R1, R2]`, `newer: true`. The capture had ended while Next file was being
pressed, and that press never moved off the file. [[BPR-1184]]'s earlier
Windows Debug run (at `e9c7eaa98`, before BPR-1274's change) stalled silently
at the same step, request 69.

## Outcome

Reproduce it deterministically by landing the capture's completion between a
Next file press and its result, find the cause and fix it, so that Next file
always advances on the revision being read and never jumps to the new capture
unasked. Add a negative control. Run the large-project Debug row and the
navigation tests.

## Cause (macOS, 2026-10-04)

A product race in the desktop's local cycle host,
`review_cycle_workbench`, not the course and not [[BPR-1274]].

1. A capture's end reloads the cycle: state, revisions, the file list,
   facts, links, history, then the tab record (`load_tabs`), then the
   active file.
2. Next file pressed during that reload moves the active tab to the next
   file, saves the tab record and reads the file. The store runs calls in
   the order asked, so a save asked after the reload's `load_tabs` read
   lands behind it.
3. That read returns the record from before the press. `load_tabs`
   restored it whole, which put the tab back on the file it had left, and
   read that file again. The press's own read was then superseded and
   dropped.

The window showed the same file on the revision it was reading, with the
newer capture listed: exactly the Windows state, `source
d000/s0/f000008.txt @ R1`, `revisions [R1, R2]`, `newer: true`. The press
was lost, not slow, so nothing ever advanced. The window is one store round
trip wide, which a Debug build on Windows makes wide enough to hit.

The restore dates from 2026-09-29 and [[BPR-1274]]'s `80e72fd70` did not
touch it; [[BPR-1184]] met the same stall at `e9c7eaa98`, before BPR-1274.

## Fix

The host counts its tab saves. `load_tabs` notes the count when it asks for
the record; if the window saved its tabs since, the record read is older
than the window's own tabs, and those are restored instead (through the same
`core::restore` checks). The press stands, the tab keeps its revision, and
new tabs still take the new capture. Nothing changes when no tab gesture
falls inside the reload.

## Evidence (macOS arm64, `build/dev` Debug, at `b668900d4`)

- **Deterministic reproduction and negative control.**
  `review_cycle_workbench` adds
  `next_file_advances_through_a_capture_reload`: with a capture lane, it
  ends a capture and runs the reload 0, 1, 2 … lane turns before pressing
  Next file (from the page's last projection, as the review bar does), then
  requires the next file, on the tab's revision, with newer material
  flagged, and the active tab naming it; a new window then reads the same
  file back from the record. It covers all 28 turns of the reload. Without
  the guard it fails with the field signature: "Next file pressed 16 turns
  into the capture's reload reads f001.txt after f000.txt, not f000.txt".
- **Large-project Debug row.** `application_review_cycle_large_project`
  1/1 in 26.55 s and `review_cycle_large_project_targets` 1/1.
- **Navigation and cycle rows.** `review_cycle_workbench`,
  `qt_review_cycle_view_1_0`/`2_0`, `application_review_cycle_two_round_public_path`,
  `application_review_cycle_parity_public_path`,
  `application_review_cycle_desktop_public_path`,
  `application_profile_cycle_handoff_public_path`,
  `sqlite_review_cycle_storage`, `architecture_boundaries`,
  `architecture_boundary_contract`,
  `application_review_round_trip_composition_contract` (the change is
  outside the pinned `src/app` scope; no reseal) and
  `test_network_isolation_manifest` pass. `coding-format-check` passes with
  clang-format 23.1.1, and clang-format 22.1.8 agrees on the changed files.

## Station requests

The race was seen only on Windows Debug. Each station runs its Debug tree at
`b668900d4` or later and returns the rows below, with the large-project row
three times in a row.

Windows (`bpr-main-win`):

```powershell
cd C:\code\git\BetterPullReview; git pull --rebase --autostash; cmake --preset windows-dev -B C:\code\git\BetterPullReview\build\windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build C:\code\git\BetterPullReview\build\windows-dev-tour-cli --config Debug; ctest --test-dir C:\code\git\BetterPullReview\build\windows-dev-tour-cli -C Debug -R "^review_cycle_workbench$|^qt_review_cycle_view_1_0$|^qt_review_cycle_view_2_0$|^application_review_cycle_two_round_public_path$|^review_cycle_large_project_targets$|^test_network_isolation_manifest$" --output-on-failure; ctest --test-dir C:\code\git\BetterPullReview\build\windows-dev-tour-cli -C Debug -R "^application_review_cycle_large_project$" --repeat until-fail:3 --output-on-failure
```

Linux (`bpr-main-linux`):

```sh
cd /home/mmarcin/BetterPullReview && git pull --rebase --autostash && cmake --preset dev -B /home/mmarcin/BetterPullReview/build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build /home/mmarcin/BetterPullReview/build/dev-tour-cli && ctest --test-dir /home/mmarcin/BetterPullReview/build/dev-tour-cli -R "^review_cycle_workbench$|^qt_review_cycle_view_1_0$|^qt_review_cycle_view_2_0$|^application_review_cycle_two_round_public_path$|^review_cycle_large_project_targets$|^test_network_isolation_manifest$" --output-on-failure && ctest --test-dir /home/mmarcin/BetterPullReview/build/dev-tour-cli -R "^application_review_cycle_large_project$" --repeat until-fail:3 --output-on-failure
```

Expected: every row passes, `review_cycle_workbench` printing its `NATIVE: Next
file pressed at each of … turns of a capture's reload …` line, and the
large-project row 3/3. A failure returns its first FAILED line and the
window state it prints.

## Checkpoint

- Candidate: `b668900d4` (product and test, on `main`).
- Verified on macOS as above, with the negative control.
- Pending: the Windows Debug confirmation, where the race was seen, and the
  Linux Debug run (station requests above).
- Next: record the station returns here, then finish.

**Note: Linux run at `b874334c2` (holds fix `b668900d4`; station
`bpr-main-linux`, WSL2, `build/dev-tour-cli`, offscreen).** The build
succeeded and 6 of 6 passed: `review_cycle_workbench` (36.0 s),
`qt_review_cycle_view_1_0`, `qt_review_cycle_view_2_0`,
`application_review_cycle_two_round_public_path`,
`review_cycle_large_project_targets` and `test_network_isolation_manifest`.
`application_review_cycle_large_project` with `--repeat until-fail:3` passed
all three runs (25.69, 25.42 and 25.85 s).

**Note: Windows run (bpr-main-win, 2026-10-04).** This is the platform the
race was seen on. Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2,
`build/windows-dev-tour-cli` Debug, at `ed4c4cbd5` (holds `b668900d4`), with
BPR-1180's stand-in fixture fixed for MSVC (`_wfopen_s`). The build printed no
`warning C` or `warning LNK` line.

- `review_cycle_workbench` passes and prints `NATIVE: Next file pressed at
  each of 28 turns of a capture's reload reads the next file on the tab's
  revision`. `qt_review_cycle_view_1_0`, `qt_review_cycle_view_2_0`,
  `application_review_cycle_two_round_public_path` and
  `review_cycle_large_project_targets` pass too.
- `application_review_cycle_large_project` with `--repeat until-fail:3`
  passes all three runs, in 128.85, 102.49 and 105.57 s. Before the fix this
  row stalled at "Next file reads the file after d000/s0/f000008.txt"
  ([[BPR-1274]]'s Windows note).
- `test_network_isolation_manifest` fails, but only on [[BPR-1180]]'s two new
  rows, `review_cycle_send` and `application_review_cycle_send_public_path`.
  They are `WINDOWS_UNCONTAINED` with no measured pin yet; that is BPR-1180's
  station work, not this card's.
