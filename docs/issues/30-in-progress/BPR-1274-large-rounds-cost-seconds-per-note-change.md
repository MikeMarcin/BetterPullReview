---
schema: "implement-rfc.card.v1"
id: "BPR-1274"
title: "A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1184 large-project course, macOS arm64 Release at 397908dec","reservation":"docs/issues/_reservations/ BPR-1274--BPR-1274--d78638593452.json","related_cards":["BPR-1184","BPR-1161"]}
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
