---
schema: "implement-rfc.card.v1"
id: "BPR-1274"
title: "A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1184 large-project course, macOS arm64 Release at 397908dec","reservation":"docs/issues/_reservations/ BPR-1274--BPR-1274--d78638593452.json","related_cards":["BPR-1184","BPR-1161"]}
acceptance: [{"criterion":"BPR-1274:profile","outcome":"pass","revision":"80e72fd70","evidence":{"kind":"observation","path":"docs/verification/BPR-1274-large-project-2026-10-04.md","detail":"Profiled with sample(1) on the macOS Release 514-item stress round before changing anything: 68% of the effect lane was each store call opening and schema-checking its own SQLite connection; a note change re-read every note through two round trips a note and reconciled every note; a share assembled the round three times; Share notes built and laid out 514 rows in one turn and relaid them as each note's code arrived."}},{"criterion":"BPR-1274:note-change-scales","outcome":"pass","revision":"80e72fd70","evidence":{"kind":"artifact","path":"docs/verification/BPR-1274-large-project/","detail":"Release reports on all three platforms, run.enforced false off macOS, failures empty, 0 tracked changes: a suggestion in the 514-item round 67.3 ms macOS (80e72fd70), 66.7 ms Linux (4cd65344e), 106.0 ms Windows (8d838c696), against 3,026 / 2,931 / 16,129 ms in BPR-1184. Enforced note_change_ratio met everywhere: 2.32, 1.94, 1.433 (limit 7.0; was about 66). --check-report re-judged the Linux and Windows reports: REVIEW_CYCLE_LARGE_PROJECT_TARGETS_OK."}},{"criterion":"BPR-1274:freeze-scales","outcome":"pass","revision":"80e72fd70","evidence":{"kind":"artifact","path":"docs/verification/BPR-1274-large-project/","detail":"A freeze assembles, encodes and digests the round once, with one store connection. Copy (freezes F1) 91.9 / 161.0 / 273.4 ms on macOS / Linux / Windows against 4,268 / 5,003 / 27,145 ms before; Save 114.8 / 194.3 / 369.4 ms. Windows Copy and Save miss the 250 ms LIMITS target, which is reported, not enforced, off macOS by the course's design; recorded on the card and record."}},{"criterion":"BPR-1274:share-notes-under-100ms","outcome":"pass","revision":"80e72fd70","evidence":{"kind":"artifact","path":"docs/verification/BPR-1274-large-project/","detail":"Share notes opening with 514 rows holds the GUI thread at most 34.2 ms (macOS), 49.95 ms (Linux), 74.6 ms (Windows), against 234.8 / 270.9 / 379.5 ms before; it lists rows 64 a turn. Its whole open (340 / 488 / 796 ms) is over 250 ms with no progress while the draft is read; named in the record's 'What still grows' and reported, not enforced."}},{"criterion":"BPR-1274:enforced-in-course","outcome":"pass","revision":"80e72fd70","evidence":{"kind":"command","path":"tests/end_to_end/review_cycle_large_project_contract.py","detail":"The large-project course enforces the 100 ms stress window turns and the 250 ms note change, Copy and Save in optimized macOS builds and the 7.0 note-change ratio in every optimized build; review_cycle_large_project_targets shows the BPR-1184 report failing exactly the four timed targets and this report meeting all 17. macOS Release row 1/1 (172.65 s) with targets enforced; the three new contracts (shared connection, one-note change, a pass a turn) each fail with their change removed."}},{"criterion":"BPR-1274:station-debug-rows","outcome":"pass","revision":"b668900d4","evidence":{"kind":"command","path":"docs/verification/BPR-1274-large-project-2026-10-04.md","detail":"Linux Debug 11/11 at 4cd65344e; macOS Debug application_review_cycle_large_project 1/1 (26.55 s) and review_cycle_large_project_targets 1/1 at b668900d4; Windows Debug 10/11 at 8d838c696. The Windows failure (navigate-during-capture: Next file reads the file after d000/s0/f000008.txt) is classified to BPR-1278, not this card: the lost press comes from review_cycle_workbench::load_tabs restoring a tab record read before the press, code from 2026-09-29 that 80e72fd70 did not touch, and BPR-1184 met the same stall at e9c7eaa98, before this card. BPR-1278 reproduces it deterministically and fixes it at b668900d4; its Windows confirmation is requested there."}}]
---

# BPR-1274: A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes

## Observation

[[BPR-1184]]'s `application_review_cycle_large_project` stress round (514 items)
measured, on macOS arm64 Release at `397908dec`
(`docs/verification/BPR-1184-large-project-2026-10-04.md`):

- each note change about 3 s;
- freezing the round 4.3 s;
- opening Share notes freezes the window for 234.8 ms, over the 100 ms input
  target.

BPR-1184 recorded these without changing them.

## Outcome

Find where each cost goes (profile first), make note changes and freezing scale
with the change rather than the round, and bring Share notes' open under the
100 ms target at 514 rows, or record a measured, named reason it cannot. Add the
case to the large-project course so it stays enforced in optimized builds.

## Result on macOS (2026-10-04)

Profiled with `sample(1)` on the Release stress round first; the findings, the
changes, before and after numbers, the checks and their negative controls are
in [the BPR-1274 record](../../verification/BPR-1274-large-project-2026-10-04.md),
with the report
[`mac-release-report.json`](../../verification/BPR-1274-large-project/mac-release-report.json)
at `80e72fd70`.

- **Where it went.** Every store call opened and schema-checked its own SQLite
  connection (68% of the effect lane); a note change re-read every note, two
  runtime round trips a note, and reconciled every note; a share assembled the
  round three times; Share notes built and laid out 514 rows in one turn and
  then relaid them as each note's code arrived.
- **macOS Release, 514 items, before → after:** a note change 3,026 → 67 ms
  (2.32 times the cycle's first, was about 66); Copy, which freezes,
  4,268 → 92 ms; Save 4,339 → 115 ms; resume 3,034 → 212 ms; Share notes
  holds the window 234.8 → 34.2 ms, listing its rows 64 a turn.
- **Freezing** now assembles, encodes and digests the round once, which is the
  least a freeze of the whole round can do; it no longer costs a store
  connection, or a round trip, a note.
- **Enforced:** the stress round's 100 ms window turns and 250 ms note change,
  Copy and Save in optimized macOS builds; a note change within 7.0 times the
  cycle's first in every optimized build. `review_cycle_large_project_targets`
  shows the BPR-1184 report failing them and this one meeting them.

## Checkpoint

- Candidate: `80e72fd70` (product, contracts, course; on `main`); the record,
  report, targets row and this note follow it.
- Verified on macOS arm64 Release: the large-project row 1/1 (172.65 s,
  targets enforced); 58 `cycle` rows with `review_cycle_large_project_targets`,
  `architecture_boundaries`, `architecture_boundary_contract`,
  `test_network_isolation_manifest`, `cycle_cli_public_path`;
  `coding-format-check`. Each new contract was shown to fail with its change
  removed.
- Pending: the Linux and Windows Release row and Debug rows, as the record's
  station requests give them. Off macOS the timed stress targets are reported
  only; the scaling check is enforced there too, and its first station
  measurements decide whether 7.0 holds.
- Next: commit the station reports with their figures here, then finish.

**Note: Linux station run at `91a9265b9` (station `bpr-main-linux`, WSL2, GCC
15.2.0; the station was idle, load 0.03). The Release tree does not build, so
there is no Linux report yet.** `cmake --build build/bpr1274-release` (the
`app-release` preset, `-O3 -Werror`) stops at:

```text
src/application/src/review_cycle_promotion.cpp:342:52: error: '*(bpr::application::review_cycle_tour_failure*)((char*)&<unnamed> + offsetof(std::review_cycle_tour_result, ...))' may be used uninitialized [-Werror=maybe-uninitialized]
  342 |                                 switch (aDone.error()) {
review_cycle_promotion.cpp:133:45: note: '<anonymous>' declared here
  133 |                                         task(*host, std::move(value));
```

The flagged object is the `review_cycle_tour_result` temporary that
`guarded` move-constructs for the by-value parameter of the `Tours.execute`
lambda ([[BPR-1273]] code). This is GCC's `-O3` `std::expected` false
positive; the Debug `dev` tree compiles it. Reading the error into a local
first does not silence it. Taking that lambda's parameter as
`const review_cycle_tour_result&` (line 332) does: the object then compiles
at `-O3 -Werror`. That change is not committed here, and the Release row was
not run.

The Debug tree (`build/dev-tour-cli`) passed 11/11 in 117.07 s, including
`application_review_cycle_large_project` (26.2 s, its 2,000-file correctness
pass) and `review_cycle_large_project_targets`.

**Note: Linux Release return at `4cd65344e` (station `bpr-main-linux`).** The
build failure noted above was fixed at its source in `4cd65344e` ([[BPR-1273]]
note). The Release row `application_review_cycle_large_project` then passed
1/1 in 107.14 s. No other jobs were running, but the 1-minute load average
was 6.9 when it started, decaying from this station's own build, which had
just finished. Host: i9-10900KF, 20 logical CPUs, 62.7 GiB, WSL2 6.18.33.2,
GCC 15.2.0, Qt 6.11.2, offscreen.

The report is [`linux-release-report.json`](../../verification/BPR-1274-large-project/linux-release-report.json),
byte for byte, SHA-256
`772d23e17b773dd5802edfad50ebd202c7917f80680ac46f12b6e7790304d83a`. It has
`run.enforced` false, `failures` empty and `source.revision` `4cd65344e…` with
0 tracked changes.

The 514-item stress round:

| Measure | Result |
|---|---|
| A note change (suggestion saved and listed) | 66.7 ms (busy 1.5) |
| The first suggestion | 34.4 ms |
| `note_change_ratio` (enforced, met) | 1.94, limit 7.0 |
| Copy (freezes F1) | 161.0 ms (busy 10.7) |
| Save to File | 194.3 ms (busy 10.8) |
| Share notes opens | 488.0 ms; the GUI thread held at most 49.95 ms |
| Resume with 512 notes | 274.0 ms |

The enforced `open_cold_ratio` targets were met (1.045 and 1.028, limit 3.0).
Typing p95 while capturing was 0.53 and 0.66 ms, and navigation p95 5.00 and
4.38 ms.

The Debug tree (`build/dev-tour-cli`) at `4cd65344e` passed 11/11 in
116.15 s; at `91a9265b9`, before the fix, it was 11/11 in 117.07 s. The
course removed its corpora.

**Note: Windows station return (`bpr-main-win`, 2026-10-04).** Host: Intel
i9-10900KF, 20 logical CPUs, 127.8 GB, Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, offscreen. No Aito build ran during the measured run;
desktop applications held the CPU at 8–9% when it started.

- **Release, `build/bpr1274-release` at `8d838c696`** (holds `80e72fd70`).
  `application_review_cycle_large_project` passed 1/1 in 1,430.06 s,
  uncontained as pinned, with [[BPR-1275]]'s warm-up. The report is
  [`windows-release-report.json`](../../verification/BPR-1274-large-project/windows-release-report.json),
  SHA-256 `15bdded3e900c8557a8f7c90e7c6c53caab613fa3483c9c05d92cf9f38d921aa`,
  byte for byte. It has `run.enforced` false, `failures` empty and
  `source.revision` `8d838c696…` with 0 tracked changes.
- **The note-change ratio is met: 1.433** (the first suggestion 73.9 ms, one
  in the 514-item round 106.0 ms, limit 7.0). Windows' timer did not tip it.
- **Against [[BPR-1184]]'s Windows run, on the 514-item round:**

  | Phase | BPR-1184 | Now |
  |---|---:|---:|
  | A desktop suggestion | 16.1 s | 106.0 ms |
  | Freeze and Copy | 27,144.7 ms | 273.4 ms |
  | Share notes opens | 12,736.6 ms (379.5 ms GUI turn) | 796.3 ms (74.6 ms turn) |
  | Save to File | 27,372.3 ms | 369.4 ms |
  | Resume with 512 notes | 15,950.6 ms | 533.4 ms |

  `bpr cycle note` takes 126.0 / 172.9 ms (p50 / p95).
- **Cold-open ratios:** 0.990 at 20,000 files and 1.041 at 100,001. `bpr cycle
  open` peaks at 23.3 and 89.7 MiB.
- **Debug, `build/windows-dev-tour-cli` at `8d838c696`: 10 of 11 pass.**
  `application_review_cycle_large_project` failed in its 2,000-file pass on the
  tree corpus, and was not rerun. With [[BPR-1184]]'s per-command budget the
  stall now speaks:
  `navigate-during-capture …: Next file reads the file after d000/s0/f000008.txt`,
  state `source d000/s0/f000008.txt @ R1`, `revisions [R1, R2]`,
  `newer: true`. The capture ended while Next file was being pressed, and the
  next press never moved off the file. This is the race the earlier Windows
  Debug run met silently at request 69. Recorded, not changed.

## Closeout (2026-10-04)

All three Release reports are in, and `--check-report` judges them again
([the record](../../verification/BPR-1274-large-project-2026-10-04.md),
"Results — Linux and Windows Release"):

- **Note change scales with the change** on every platform: the enforced
  `note_change_ratio` is 2.32 (macOS), 1.94 (Linux) and 1.433 (Windows),
  under 7.0; it was about 66. The bound stands.
- **Freezing** assembles the round once: Copy 91.9, 161.0 and 273.4 ms
  (4,268, 5,003 and 27,145 ms before).
- **Share notes' open holds the window under 100 ms** at 514 rows on every
  platform: 34.2, 49.95 and 74.6 ms turns (234.8, 270.9 and 379.5 before).
- **Enforced** in optimized builds by the course and
  `review_cycle_large_project_targets`. Linux meets all 17 targets; on
  Windows Copy (273.4 ms) and Save (369.4 ms) miss the 250 ms target, which
  is reported, not enforced, off macOS as designed, and is recorded here
  rather than hidden.
- **Debug rows:** Linux 11/11; macOS `application_review_cycle_large_project`
  and `review_cycle_large_project_targets` 1/1 each at `b668900d4`; Windows
  10/11. The Windows failure is not a regression of this card: it is a lost
  Next file press when a capture's reload restores a tab record read before
  the press. That code dates from 2026-09-29 and `80e72fd70` did not touch
  it, and [[BPR-1184]] met the same stall at `e9c7eaa98`, before this card.
  It is [[BPR-1278]]'s, fixed at `b668900d4`; its Windows Debug confirmation
  is requested there.
