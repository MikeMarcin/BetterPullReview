---
schema: "implement-rfc.card.v1"
id: "BPR-1275"
title: "The large-project course charged a Windows host's first scan of freshly written files to the desktop's cold open"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"origin":"BPR-1184 large-project course, Windows Release at 61c96b9a3 (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1275--BPR-1276--cc9fb8ce4d80.json","related_cards":["BPR-1184","BPR-1276"]}
acceptance: [{"criterion":"BPR-1275:cause","outcome":"pass","revision":"0fc1c1246","evidence":{"kind":"observation","path":"tests/end_to_end/review_cycle_large_project_contract.py","detail":"Not the desktop path: the host's first open of freshly written files (about 5 ms a file, Defender real-time and on-access on), charged to the desktop because the course drove it first on each new corpus. Probes: the desktop's first capture was slow throughout (4.3-7.2 s per 1,024 entries against 0.33 s), install_all 6.6 s in all, BEGIN IMMEDIATE never waited, and its second capture ran at the terminal's pace. Fresh 20k folder, no desktop: bpr cycle open 102,009 ms first, 7,680 ms again; plain Python read 99,044 ms first, 1,732 ms second; bpr cycle open after it 7,605 ms."}},{"criterion":"BPR-1275:pin-and-negative-control","outcome":"pass","revision":"0fc1c1246","evidence":{"kind":"command","path":"tests/end_to_end/review_cycle_large_project_contract.py","detail":"Untimed first read of each corpus, kept as corpus.first_read_ms. In optimized builds the desktop's cold open must stay within COLD_OPEN_RATIO_LIMIT 3.0 of bpr cycle open. Windows Release, 20k: warmed, 7,758.4 / 7,735.2 ms, ratio 1.003, pass. With --no-warm-up, 108,280.0 / 8,124.0 ms, ratio 13.328, and the course fails with the pin's message."}},{"criterion":"BPR-1275:windows-report","outcome":"pass","revision":"da6e49e9f","evidence":{"kind":"command","path":"docs/verification/BPR-1184-large-project/windows-release-report.json","detail":"application_review_cycle_large_project passed 1/1 in 1,629.14 s at c906b0e7e, Windows Release, uncontained as pinned. Report byte for byte, SHA-256 e865a9a94518295a9c3243ad38398329542e6964f4df135ef31adb0648cd744e; failures empty; cold-open ratios 1.033 (20k) and 1.027 (100k)."}},{"criterion":"BPR-1275:boundary","outcome":"pass","revision":"da6e49e9f","evidence":{"kind":"command","path":"scripts/verify_test_network_isolation.py","detail":"Test-only change. architecture_boundaries and test_network_isolation_manifest pass in windows-dev at da6e49e9f; the round-trip composition pin check (source-only) prints PIN_CHECK_OK. The contract's runtime run stalls at its 5 s draft cap on this host (REVIEW_ROUND_TRIP_DRAFT_STALLED) with product source unchanged since it last passed at d8fa5bfc2; reported separately."}}]
---

# BPR-1275: The large-project course charged a Windows host's first scan of freshly written files to the desktop's cold open

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

## Cause (bpr-main-win, 2026-10-04)

This was not the desktop's path. The cost is the host's first open of a newly
written file, and the course charged all of it to the desktop.

- **Probes.** Uncommitted timing probes on the 20,000-file corpus in the Release
  tree reproduced the gap: desktop 113.9 s, terminal 7.9 s. The desktop's first
  capture was slow throughout, not in stretches: about 4.3 s per 1,024 entries,
  rising to about 7.2 s. The terminal ran 0.33 s per 1,024. The store was not
  the cost: every `install_all` together took 6.6 s, `BEGIN IMMEDIATE` never
  waited, and 634 store opens took 3.0 s. The desktop's second capture of the
  same folder ran at the terminal's pace, 0.32 s per 1,024.
- **Course order.** The course generated each corpus and drove the desktop on
  it first. The terminal ran afterwards on the same folder, so the desktop
  always did the first read of every new file.
- **Controls on a freshly generated 20,000-file folder, with no desktop.**

  | Step | Time |
  |---|---:|
  | `bpr cycle open`, first read of the files | 102,009 ms |
  | `bpr cycle open`, again | 7,680 ms |
  | Plain Python `read_bytes` of every file, first time | 99,044 ms (about 5 ms a file) |
  | The same read, second time | 1,732 ms |
  | `bpr cycle open` after that read | 7,605 ms |

  Defender real-time and on-access protection were on. MsMpEng's processor
  time cannot be read without administrator rights, so Defender is named by
  inference.

## Change

The course only, with no product change. Each corpus is read once, untimed,
right after it is written, and the report keeps that read as
`corpus.first_read_ms`. In an optimized build, the desktop's cold open must stay
within `COLD_OPEN_RATIO_LIMIT` (3.0) of `bpr cycle open` cold on the same
folder. macOS measured 1.16 and 1.03. `--no-warm-up` is the negative control.

## For Windows users

The first-read cost is real for a person who opens a freshly cloned checkout on
Windows: about 5 ms a file while Defender scans it, so roughly 100 s for
20,000 new files. The product's answer today is [[BPR-1184]]'s progress
("Capturing: N files read") and Cancel capture. A possible future idea, not
work for now: a hint that a first capture of N files may take a while on
Windows.
