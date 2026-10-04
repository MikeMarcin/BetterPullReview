# BPR-1274 large-round responsiveness — 2026-10-04

[[BPR-1274]] takes the costs [[BPR-1184]] recorded on its 514-item stress round
(`application_review_cycle_large_project`, macOS arm64 Release at `397908dec`):
about 3 s for each note change, 4.3 s to freeze and 234.8 ms of a held window
when Share notes opens, against the 100 ms input target of the
[LIMITS contract](../rfcs/assets/bpr-review-cycles-v1/specs/LIMITS.md). The
measurements it starts from are in
[the BPR-1184 record](BPR-1184-large-project-2026-10-04.md) and the three
reports in [`BPR-1184-large-project/`](BPR-1184-large-project/).

## Profile first

The Release tree of `c104667d4` (the `dev` preset with
`CMAKE_BUILD_TYPE=Release` and `BPR_BUILD_TOUR_CLI=ON`, Apple M4 Pro, macOS
26.6.2, Qt 6.11.2, offscreen) ran the course's stress round with `sample(1)`
attached to the stress desktop at 1 ms for the whole round, then, after each
change, again with temporary timers around the share sheet's row building
and the reviewer's suggestion gesture (removed before commit). What the
samples showed, on the effect lane unless named:

1. **Each store call opened and checked a connection.** 11,168 of the effect
   lane's 16,352 samples (68%) were in `cycle_store::open`; 5,972 of them in
   `schema_valid`, which reads `sqlite_schema` and compares every table's
   canonical SQL. A 514-item round makes thousands of store calls.
2. **A note change read the round.** After each save the window read every
   note again, two runtime round trips a note (its revisions, its
   suggestions) — 1,028 trips through the effect and decision lanes for 514
   notes — then reconciled every note, reading each note's file from retained
   material (1,556 samples in `observe_feedback_target`).
3. **A share assembled the round three times.** Copy and Save each previewed
   the round, froze it by previewing it again, then read the draft back for
   the sheet (`open_share_draft`: every note's wording and suggestions, then a
   third preview). `open_share_draft` alone was 5,892 samples over three calls.
4. **Share notes built 514 rows in one turn.** On the GUI thread the first
   present made 514 rows (22 ms), filled them (6 ms) and laid out the whole
   list (110 ms in `place`, then 25 ms again when shown): one 224 ms turn.
   Each note's code then arrived one by one, and each arrival refilled all
   514 rows; the disclosure's `set_words` called `updateGeometry` every time,
   so every row was laid out again (about 13 ms an arrival).
5. **The notes tray was rebuilt on every publication** (`clear` and 514 new
   items), which lays out every word-wrapped item again (about 18 ms at 514).
6. **The course's wait sleeps 1 ms between event-loop turns**
   (`QThread::msleep(1)` in the driver's `wait_for`). With 1,028 sequential
   round trips per refresh that alone was about 1.3 s a note change on macOS;
   at Windows' 15.6 ms timer resolution it explains most of the 16 s Windows
   measured. The real event loop does not sleep, so Windows' product cost was
   lower than its report; with the round trips gone it no longer matters.

## Changes (`80e72fd70`)

- **One connection a runtime call.** `review_cycle_store::hold()` returns a
  scope; while it lives, the store calls the holding thread makes share one
  connection, opened and checked by the first and closed when the scope ends.
  A call nested inside another opens its own; another thread's scope (the
  capture lane) has its own. The runtime holds one around every call, so each
  call still stands alone: a write is one transaction, a read sees what is
  committed.
- **A note change reads that note.** Notes are read a feedback page (64) a
  call, each with its history and suggestions (`note_records_selected`).
  After a note change made in this window — a note, a suggestion, a related
  location, a decision — the window reads that note (`note_record_selected`)
  and reconciles it alone (`reconcile_selected` with `aOnly`). When another
  interface wrote the cycle meanwhile, or tells the window it did, every
  note is read again, as before. A full read is shown together with its
  replies and reconciliation, never a list the page has not reconciled.
- **A share freezes what it previewed.** `freeze_cycle_round` takes the
  preview the same effect-lane call assembled, so the round is assembled,
  encoded and digested once, and the share hands back the frozen draft
  (`cycle_share_output::Shown`) instead of the sheet reading it back.
- **Share notes opens a pass a turn.** The sheet makes at most 64 new rows a
  turn (`rows_per_pass`), in the round's order, numbered as read, heading
  counting every note from the start; `listing()` says rows are still
  arriving. A refresh fills only rows whose note, code, number or state
  changed, keeps the layout when the order holds, and the disclosure measures
  its words once. The excerpts were already read one note at a time and
  classified only when shown; they now refill one row each.
- **The notes tray** is rebuilt only when its words change (keeping its
  selection and scroll otherwise) and lays its items out 64 a batch.

Behaviour and the accepted Share notes design are unchanged: the same rows,
words, order, focus chain and disclosure; a long round's later rows appear
over the following turns (about 0.3 s for 514).

## Results — macOS Release

Apple M4 Pro, 14 logical processors, 48 GiB, macOS 26.6.2, Qt 6.11.2,
offscreen 1920 × 1200 at DPR 1, `build/bpr1274-release`, under the
network-deny runner. The row passed 1/1 in 172.65 s with every target
enforced (host load average 17 when it started, 9 when it ended). Report:
[`BPR-1274-large-project/mac-release-report.json`](BPR-1274-large-project/mac-release-report.json),
SHA-256 `80ed54b096afd44f335ba2e748feea7c9877b3ab3fa16b2631f02b22349f653e`,
`source.revision` `80e72fd70…` with 0 tracked changes.

Stress round, 514 items on the 20,000-file corpus, gesture to result inside
the process (each one sample, as before); "busy" is the GUI thread's longest
event-loop turn during the gesture:

| Phase | `397908dec` (BPR-1184) | `c104667d4` (same day, before) | `80e72fd70` (after) | Busy before → after |
|---|---:|---:|---:|---:|
| Resume with 512 notes | 3,034.0 | 3,140.5 | 211.9 | 35.0 → 32.9 |
| A suggestion in the 514-item round | 3,026.4 | 3,323.6 | **67.3** | 18.0 → 3.0 |
| The cycle's first suggestion (new phase) | – | – | 29.0 | 0.2 |
| Share notes opens with 514 rows | 2,179.3 | 2,764.5 | 340.1 | **234.8 → 34.2** |
| Copy (freezes F1) | 4,268.3 | 5,403.8 | **91.9** | 36.5 → 7.0 |
| Save to File | 4,338.9 | 5,330.9 | 114.8 | 35.6 → 8.4 |
| `bpr cycle note`, p50 of 512 | 30.3 | 35.4 | 29.1 | – |
| `bpr cycle export`, p50 of 10 | 38.0 | 43.3 | 35.1 | – |

The note change now costs 2.32 times the cycle's first (67.3 against
29.0 ms); it was about 66 times. Share notes lists its 514 rows in nine
turns, none over 35 ms. The rest of the course is unchanged: typing while
capturing p95 0.48 and 0.58 ms, navigation p95 4.1 and 4.3 ms; cold open
2,131.5 ms (20,000 files) and 10,459.4 ms (100,001), 0.92 and 1.00 times
`bpr cycle open`'s. An earlier run of the same product before the rebase
onto `afd13b2c1` measured 63.3, 89.5, 108.6 and 32.6 ms for the same four.

Off macOS, BPR-1184 measured the same shape at `397908dec`/`c906b0e7e`:
Linux (i9-10900KF, WSL2) 2,931 ms a suggestion, 5,003 ms Copy, 270.9 ms
held by Share notes; Windows 11 16,129 ms, 27,145 ms and 379.5 ms. Neither
has run this change yet (station requests below).

## What still grows with the round

- **Resume** reads every note (a call per 64) and reconciles every note in
  one call: 212 ms at 514 notes.
- **Share notes' open** reads the draft and every note's wording once
  (about 200 ms at 514) before the sheet shows, then lists its rows over the
  following turns: 340 ms in all, the window free throughout (≤ 35 ms turns).
  It is over 250 ms and shows no progress while the draft is read; it is
  reported, not enforced.
- **Freezing** assembles, encodes and digests the whole round once — the
  round is its whole content — and renders it once: 92 ms for 514 items.
- **The notes tray** lays out every item again when one changes: Qt's list
  view relays its whole list on any insertion or text change. It does so 64
  items a turn, so a change never holds the window for all of them (3.0 ms
  busy for a suggestion at 514).

## Checks enforced in optimized builds

The course now makes the cycle's first suggestion before the terminal's 512
notes, so the round is still 512 notes and 2 suggestions, and judges the
stress round against LIMITS, following [[BPR-0905]]'s rule that a bound says
what it is derived from and [[BPR-0967]]'s that a timed budget is enforced
only where measurements justify it:

| Target | Derivation | Enforced |
|---|---|---|
| Every stress gesture's longest window turn under 100 ms | LIMITS input target | `--enforce 1`: optimized macOS |
| A note change, Copy and Save each under 250 ms p95 | LIMITS: past 250 ms progress and cancel are owed; these show neither | `--enforce 1`: optimized macOS |
| A note change in the round within 7.0 times the cycle's first | measured 2.32, 2.16 and 2.34 here; three times that, rounded up; the build before measured 66.1 | every optimized build |

`--check-report PATH` judges every target again on a report the course
wrote, running nothing. The new row `review_cycle_large_project_targets`
uses it as a negative control in every build:

- the BPR-1184 macOS Release report fails exactly the four timed stress
  targets (Share notes 234.8 ms held; suggestion 3,059.9, Copy 4,268.3 and
  Save 4,338.9 ms over 250) and nothing else;
- the BPR-1274 report meets all 17 targets;
- the same report with its note change set to 66 times the first fails only
  the scaling check in Release, and in Debug reports it without failing;
- with `--enforce 0` the four misses are reported, not failed.

**Live negative control.** The new course run against the Release binaries
of `c104667d4` (before the change; 2,000-file corpus) failed all five stress
checks: Share notes held the window 248.4 ms; a suggestion took 2,932.4 ms,
66.1 times the first (44.4 ms); Copy 4,596.2 and Save 4,534.9 ms.

**Contracts, each shown to fail without its change:**

| Contract | What it proves | Broken by | Fails with |
|---|---|---|---|
| `sqlite_review_cycle_storage` (`shares_one_connection_within_a_scope`) | Five reads open five connections outside a scope and one inside; another thread's scope has its own; a write inside commits and is read back; a cancelled call is refused | the scope never lending its connection | "one connection for each thread's scope" |
| `review_cycle_workbench` (`changes_one_note_without_reading_the_round`) | A note change takes 6 host steps with 150 notes, as with 10; a suggestion, related location and decision leave the notes equal to a fresh read | reading every note after each change | "a note change takes 12 steps with 150 notes and 8 with 10" |
| `qt_review_cycle_view_1_0`/`2_0` (`lists_a_long_round_a_pass_a_turn`) | 514 notes open with 64 rows, the rest a pass a turn in order and numbered; a refresh leaves unchanged rows alone | building every row at once; refilling every row | "the sheet opens with its first pass of rows"; "a row whose note did not change is not filled again" |

All 58 rows matching `cycle` (the large-project row apart) passed in the
Release tree at `80e72fd70` with this record's targets row, with
`architecture_boundaries`, `architecture_boundary_contract`,
`test_network_isolation_manifest` and `cycle_cli_public_path`.
`coding-format-check` passed; clang-tidy on the changed product files
reported nothing.

The driver also answers a save dialog after moving focus off its name field:
`QFileDialog::selectFile` leaves that field alone while it has focus, and the
quicker window reached the focused dialog first, saving under the suggested
name.

## Station requests

Each station runs the optimized tree at `80e72fd70` or any later `main`,
then its ordinary Debug tree. Return the Release report byte for byte as
`docs/verification/BPR-1274-large-project/<platform>-release-report.json`
with the host, its load and both ctest times, and commit it with a note on
[[BPR-1274]]. The timed stress targets stay reported off macOS; the scaling
check is enforced in every optimized build, so a station whose ratio exceeds
7.0 fails the row — record its two suggestion times and leave the bound for
the next round to set from them. If the contained row stops before
measuring, record its first failing line, then rerun it uncontained as
[[BPR-1184]]'s requests describe.

Linux (`bpr-main-linux`):

```sh
cd /home/mmarcin/BetterPullReview && git pull --rebase --autostash && cmake --preset app-release -B /home/mmarcin/BetterPullReview/build/bpr1274-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DEPENDENCY_OFFLINE=ON && cmake --build /home/mmarcin/BetterPullReview/build/bpr1274-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox && ctest --test-dir /home/mmarcin/BetterPullReview/build/bpr1274-release -R "^application_review_cycle_large_project$" --output-on-failure
cmake --preset dev -B /home/mmarcin/BetterPullReview/build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build /home/mmarcin/BetterPullReview/build/dev-tour-cli && ctest --test-dir /home/mmarcin/BetterPullReview/build/dev-tour-cli -R "^review_cycle_large_project_targets$|^sqlite_review_cycle_storage$|^review_cycle_workbench$|^qt_review_cycle_view_1_0$|^qt_review_cycle_view_2_0$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^application_review_cycle_desktop_public_path$|^application_profile_cycle_handoff_public_path$|^application_review_cycle_large_project$|^test_network_isolation_manifest$" --output-on-failure
```

Windows (`bpr-main-win`):

```powershell
cd C:\code\git\BetterPullReview; git pull --rebase --autostash; cmake --preset windows-app-release -B C:\code\git\BetterPullReview\build\bpr1274-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DISTRIBUTION_BUILD=OFF -DBPR_DEPENDENCY_OFFLINE=ON; cmake --build C:\code\git\BetterPullReview\build\bpr1274-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox; ctest --test-dir C:\code\git\BetterPullReview\build\bpr1274-release -R "^application_review_cycle_large_project$" --output-on-failure
cmake --preset windows-dev -B C:\code\git\BetterPullReview\build\windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build C:\code\git\BetterPullReview\build\windows-dev-tour-cli --config Debug; ctest --test-dir C:\code\git\BetterPullReview\build\windows-dev-tour-cli -C Debug -R "^review_cycle_large_project_targets$|^sqlite_review_cycle_storage$|^review_cycle_workbench$|^qt_review_cycle_view_1_0$|^qt_review_cycle_view_2_0$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^application_review_cycle_desktop_public_path$|^application_profile_cycle_handoff_public_path$|^application_review_cycle_large_project$|^test_network_isolation_manifest$" --output-on-failure
```

Expected: `REVIEW_CYCLE_LARGE_PROJECT_OK` and 1/1 for the Release row, with a
report whose `run.enforced` is false, `failures` empty, `source.revision`
`80e72fd70…` or later and a `note_change_ratio` target that is met; every
Debug-tree row passes. The Release report is at
`build/bpr1274-release/tests/end_to_end/bpr1184-large-project-report.json`.
The Release row's timeout stays a provisional 2,400 s off macOS.
