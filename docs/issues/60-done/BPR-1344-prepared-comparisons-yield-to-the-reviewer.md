---
schema: "implement-rfc.card.v1"
id: "BPR-1344"
title: "Prepared comparisons never hold up a comparison the reviewer asked for"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win, BPR-1327's Windows run, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1344--BPR-1344--7a3656cbb78d.json","related_cards":["BPR-1327","BPR-1325","BPR-0967","BPR-1359","BPR-1360","BPR-1369"],"caused_by":"8b71105f7","decision":"owner, chat 2026-10-09, relayed by the coordinator: preparation waits for an idle reviewer; prepared blob reads are preparation's jobs; actor priority, job split and BPR-1359's stop kept; budgets unchanged"}
acceptance: [{"criterion":"Host tests: actor priority, preparation and reviewer job counts (with active_reviewer_jobs), a prepared blob read is preparation's job (unmarked control), the diff host marks prepared reads and not the reviewer's","outcome":"pass","revision":"c518a2e4e","evidence":{"kind":"command","command":"ctest -C Release -R \"^(application_epoch_host|application_diff_host|application_workbench_host|core_comparison_preparation)$\"","detail":"Windows 11, MSVC 19.50, Release, on the merged tree: all pass."}},{"criterion":"On Windows, alternated against main, application_large_review_active_work and application_large_review_history pass within their unchanged budgets, with active_supersession_shown enforced","outcome":"pass","revision":"c518a2e4e","evidence":{"kind":"command","command":"ctest -j1 -V -R BPR-1327's large-review selection, three pairs alternating candidate and control (bad010633)","detail":"Candidate 19/19 in all three; control 16, 17, 17 (active work and history each time). active_supersession_final_drain 610/671/721 ms vs control 2548/1998/2743; active_supersession_shown 610/670/721 ms (limit 1250); history source selections median/worst 734/814, 700/905, 704/808 ms vs control 3293/6075, 3973/5772, 3307/5987 (limit 2000); worst pulse 72.2 ms."}},{"criterion":"application_large_review_prepared_open passes (pass criterion; intermittent on Linux before BPR-1359)","outcome":"pass","revision":"c518a2e4e","evidence":{"kind":"command","detail":"3/3 under BPR-1369's rule (work-avoided assertions and wall guard), timed to shown-and-quiet: slowest prepared 427.1-430.9 ms against an unprepared median 599.5-604.1 ms plus 149.9-151.0 ms; also passes on the merged tree."}},{"criterion":"BPR-1327's Windows selection passes, apart from rows that fail the same way on main","outcome":"pass","revision":"c518a2e4e","evidence":{"kind":"command","detail":"19/19 in each of three alternating runs and 19/19 on the merged tree at 970796e2b. test_network_isolation_manifest fails only on BPR-1345's unmeasured application_start_review_contract, not this card's."}},{"criterion":"The macOS and Linux runs are requested from the coordinator","outcome":"pass","revision":"c518a2e4e","evidence":{"kind":"observation","detail":"Requested from the coordinator on 2026-10-09, with the Debug margin of BPR-1369's wall guard."}}]
---

# BPR-1344: Prepared comparisons never hold up a comparison the reviewer asked for

## Observation

On Windows, [[BPR-1327]]'s rows failed two interaction budgets on every run:

- `application_large_review_active_work`'s `active_supersession_final_drain`
  took 2.2 to 3.3 s, against 1250 ms;
- `application_large_review_history`'s epoch source selections took 2.6 to
  3.7 s, against 2000 ms.

A bisect named `8b71105f7`, from which each prepared request really prepares up
to 16 comparisons. With preparation switched off in the course (a probe), the
same build's drain was 312 ms and the rows passed.

## Cause

Two things, both proven on Windows (Release, i9-10900KF) with probes that were
never committed.

1. **Preparation ran while the reviewer was waiting.** A round started on every
   publication, including in the middle of the reviewer's own selection. Work
   already in flight (a prepared diff computation, an actor job) cannot be
   preempted, so the reviewer's next request waited behind it.
2. **Preparation's blob reads were counted and queued as the reviewer's.** A
   prepared diff reads its two blobs through the epoch host's `materialize`,
   which admitted every read as a reviewer job at reviewer priority.
   - Instrumenting the slow history selections showed nothing else holding
     them once the file was shown. Diff, workbench and pending were 0 and
     classification held 68 to 226 ticks, while the "reviewer" epoch ticks
     equalled the ticks with a prepared diff computing (3637 against 3637,
     2664 against 2665).

Split probe (`p0`: reviewer-only settle and actor priority; `p1`: no prepared
diff computation; `p2`: a round starts only when the reviewer is idle), Release,
three runs each of active work and two of history:

| | Preparation off | As merged | p0 | p1 | p2 |
|---|---|---|---|---|---|
| Active: final file shown and quiet (ms) | 312 | 601–609 | 383–450 | 789–868 | 307–374 |
| History: worst source selection (ms, limit 2000) | 546 | 4635–6868 | 2325–5139, fails | 1140–1154 | 702–1244, passes |

Two earlier attempts, a separate diff lane for prepared comparisons and the
actor priority alone, did not bring either budget within its limit.

## The benefit, measured

**Live remote reads** (Windows, `gh` against MikeMarcin/BPR-Test#43, 60
modified files, 12 runs):

| Read | Median | p90 |
|---|---|---|
| One exact blob read (viewer, blob, viewer) | 1044 ms | 1163 ms |
| A batch of 16 | 1323 ms | 1528 ms |
| Sparse clone | 1028 ms | 1302 ms |

`application_epoch_host`'s live-shaped test with those figures: a cold modified
file 2097–2110 ms, the same file prepared 0.40–0.64 ms. The Mac's live run of
the real application on #43 found a cold open 4.3 s and a prepared one 22 ms
shown (113 quiet). [[BPR-1359]] then fixed the batch body that had made every
live batch fail, and [[BPR-1360]] reads both sides of a cold open at once
(2.35 s).

**Local opens** (the prepared-open course to shown-and-quiet, `p0` against
`p2`, three runs each): cold 306–435 ms either way; prepared 253–343 ms with
`p0`, 146–226 ms with `p2`, where the next round no longer competes with the
open just made.

## Design decision

The owner approved this design in chat on 2026-10-09, relayed by the
coordinator. The owner asked why prefetch cannot simply be asynchronous with
interactive priority; the answer is that priority cannot preempt work already
in flight. The owner agreed.

- **A round waits for an idle reviewer.** A preparation round starts only when
  the reviewer has no diff, workbench or active epoch work and no
  classification of what is on screen still to arrive. A round deferred
  meanwhile is resumed from the decision lane, not by polling:
  - the window's epoch, diff and workbench hosts post through a thin
    `decision_executor` wrapper that re-checks after each of their tasks (its
    callback a weak handle, because the lane is the profile's and outlives a
    window);
  - an adopted syntax classification re-checks too, since the classifier's
    blob reads come back outside those lanes.

  A reviewer request still stops a round in flight, as before.
- **Prepared blob reads are preparation's.** The diff host marks its prepared
  job's reads (`diff_blob_request::Prepared`). The epoch host admits them as
  preparation jobs at preparation priority. The reviewer's own selected file
  never is ([[BPR-1359]]'s `reviewerBlob` test still wins), and
  `materialize_both` serves only the selected pair.
- **Kept:** the actor priority (two queues, the reviewer's drained first), the
  reviewer/preparation job split, and [[BPR-1359]]'s stop of a prepared read in
  flight as the backstop.
- **Budgets unchanged.** Each phase settles when the reviewer's epoch jobs reach
  zero. Preparation's tail after it is measured as its own phase and not
  enforced. The time from the active-work course's last key to its final file
  shown with the diff quiet is enforced as `active_supersession_shown` under
  the same 1250 ms.

## Outcome

- `profile_epoch_actor` keeps a second FIFO for preparation, drained only while
  the reviewer's is empty (`profile_epoch_actor_priority`).
- The epoch host:
  - marks each job preparation's or the reviewer's;
  - `outstanding_preparation_jobs()` and `outstanding_reviewer_jobs()` split
    `outstanding_jobs()` under the one jobs lock;
  - `active_reviewer_jobs()` leaves out finished actor work that only awaits
    its retirement, whose completion has already been posted.
- The application (`github_application.cpp`):
  - schedules a round only for an idle reviewer and resumes a deferred one from
    the decision lane;
  - cancels a deferred or running round on a prune, a cache delete or any other
    cancellation, so nothing prepared is stored after the cache is cleared.
- Fixture evidence: `EpochReviewerJobs`, `EpochPreparationJobs` and
  `PreparationPending`.
- The benchmark:
  - settles on the reviewer's epoch jobs;
  - waits for each preparation tail, including a deferred round that has yet to
    start;
  - enforces `active_supersession_shown`;
  - times the prepared-open course to shown-and-quiet, under [[BPR-1369]]'s
    rule (work avoided, with a wall-time guard).

## Relation to BPR-1359

Copied from [[BPR-1359]]'s card, as that card asked:

> The Windows station is redesigning preparation scheduling under [[BPR-1344]]:
> preparation waits for an idle reviewer, and prepared blob reads count as
> preparation. This card keeps to the transport body and to the fallback's
> cancellation and back-off, all inside the epoch host's prepared exact read
> (`authority_effect_actor`'s prepared-read fields and `materialize`'s prepared
> branch). When BPR-1344's scheduler owns when prepared reads run, the yield
> here becomes a second line of defence for a read already in flight, and the
> back-off stays the rule for a transport whose batch fails. BPR-1344's live
> figures from the probe were all taken with the array body, so they measured
> the fallback, not batching; they should be retaken on this fix.

## Acceptance

- Host tests:
  - `profile_actor_runs_reviewer_work_before_queued_preparation`;
  - `preparation_jobs_are_counted_apart_from_the_reviewers`, which also checks
    `active_reviewer_jobs`;
  - `a_prepared_blob_read_is_preparations_job`, with an unmarked control;
  - the diff host's prepared-open test: preparation's reads are marked and the
    reviewer's are not.
- On Windows, alternated against main, `application_large_review_active_work`
  and `application_large_review_history` pass within their unchanged budgets,
  with `active_supersession_shown` enforced.
- `application_large_review_prepared_open` passes. It is a pass criterion here,
  because Linux found it intermittent before [[BPR-1359]] ("a prepared open
  was no faster than every unprepared one"): in Debug 3 of 10 at `ad2ebe11f`
  and 8 of 17 later, in Release 1 of 6 at `fc89f79e3`. That is the contention
  this card removes.
- [[BPR-1327]]'s Windows selection passes, apart from rows that fail the same
  way on main.
- The macOS and Linux runs are requested from the coordinator.

## Windows results (bpr-main-win, 2026-10-09)

Release tests-on, MSVC 19.50, Qt 6.11.2, an otherwise idle station.

- **Candidate:** this change on `bad010633`, which includes [[BPR-1359]],
  [[BPR-1360]] and [[BPR-1369]].
- **Control:** `bad010633` itself.

[[BPR-1327]]'s large-review selection (19 rows on Windows) ran at `-j1` in
three alternating pairs.

| Run | Candidate | Control |
|---|---|---|
| 1 | 19/19 | 16/19 (active work, history, cache bytes) |
| 2 | 19/19 | 17/19 (active work, history) |
| 3 | 19/19 | 17/19 (active work, history) |

| | Candidate | Control |
|---|---|---|
| `active_supersession_final_drain` (limit 1250 ms) | 610, 671, 721 | 2548, 1998, 2743 |
| `active_supersession_shown` (limit 1250 ms) | 610, 670, 721 | not measured |
| History source selections, median / worst (limit 2000 ms) | 734 / 814, 700 / 905, 704 / 808 | 3293 / 6075, 3973 / 5772, 3307 / 5987 |
| Worst pulse of any row (target 100 ms) | 65.1, 72.0, 72.2 | 60.7, 76.8, 74.8 |

- **Prepared-open:** passes 3 of 3 on the candidate under [[BPR-1369]]'s rule,
  every work-avoided assertion included. To shown-and-quiet, the unprepared
  median is 599.5–604.1 ms and the slowest prepared open 427.1–430.9 ms, inside
  the guard (median plus 149.9–151.0 ms) with a margin of about 320 ms. The
  control times the same course to first paint (unprepared median 200.9–244.1
  ms, prepared at most 25.5–28.6 ms), so its times are not comparable; it passes
  3 of 3 too.
- **The control's `cache_bytes` failure** in run 1 ("stale comparison
  published while superseding file B remained selected") is the intermittent
  one also seen in [[BPR-1334]]'s C4 control. It is not this card's; the
  candidate passed it 3 of 3.
- **Before the last fix:** the first round of pairs failed
  `application_large_review_resources` on the candidate 3 of 3. A round that
  started once the reviewer went idle outlived the course's prune and cache
  delete, and stored a prepared comparison after it. A prune, a cache delete
  and any cancellation now cancel preparation and empty the diff host's queue,
  and resources passes 3 of 3 alone and in every pair above.
- **Host rows:** `application_epoch_host`, `application_diff_host`,
  `application_workbench_host`, `core_comparison_preparation`,
  `gh_cli_transport`, `github_provider`, `architecture_boundaries`, the large-
  review fixture and replay contracts and `test_network_isolation_manifest`
  pass. The round-trip pins are resealed over main's (mutation pin unchanged;
  `PIN_CHECK_OK`).

**On the absolute times:** on this base both sides' shown-and-quiet times are
higher than on the base where the probes ran (an unprepared local open was
about 310 ms there and about 600 ms here). The candidate stays within every
limit with room. A preparation-off control on this base was not run.

**Not run here:** the Debug margin of [[BPR-1369]]'s wall guard (Linux saw its
tightest at 6.0 ms in Debug, timed to first paint), and the macOS and Linux
selections. Both are requested from the coordinator.

**On the merged tree** (this change on `970796e2b`, Release): [[BPR-1327]]'s
selection passes 19 of 19. `application_epoch_host`, `application_diff_host`,
`application_workbench_host`, `core_comparison_preparation`, `gh_cli_transport`,
`github_provider`, `architecture_boundaries` and the round-trip contract pass.
`test_network_isolation_manifest` fails only on [[BPR-1345]]'s
`application_start_review_contract`, which was registered "unmeasured
(registered on macOS)" in between; the Windows station measures it next.

## GCC build fix (bpr-main-linux, 2026-10-09)

At `3947245ca`, the Linux dev build failed:
`tests/application/diff_application_host_tests.cpp` calls `std::ranges::none_of`
and `std::ranges::all_of` without `<algorithm>`, which GCC 15 refuses (MSVC and
Apple clang reach it through other headers). The include is added; test-only.
`application_diff_host` passes on Linux and the format check passes.

## Linux station run (bpr-main-linux, 2026-10-09)

At `3313967c3` (`3947245ca` plus the `<algorithm>` fix above), WSL2 Ubuntu
26.04, GCC 15.2.0, i9-10900KF.

**[[BPR-1327]]'s large-review selection** passes 26 of 26 in the optimized
tests-on tree (Release `-O3`, `-j1`, 216.6 s). The load average was 3.0 at the
start, falling. That covers `core_comparison_preparation`, `application_diff_host`,
`application_epoch_host`, `application_workbench_host`, `github_provider`,
`architecture_boundaries`, the round-trip contract, both large-review
contracts, every `application_large_review_*` row, the unavailable and
approximate diff public paths and `application_first_tab_comparison`.
`gh_cli_git_smart_transport` is not registered on Linux.

**`application_large_review_prepared_open` in Debug** (`build/dev`, run at a
load below 3) passes 10 of 10. [[BPR-1369]]'s wall guard requires the slowest
prepared open to be at most the median unprepared open plus
max(15 ms, median/4). Now that prepared opens are timed to shown-and-quiet,
its margins were:

| Run | Unprepared median | Tolerance | Prepared max | Margin |
|---|---|---|---|---|
| 1 | 145.1 | 36.3 | 134.8 | 46.6 |
| 2 | 130.0 | 32.5 | 139.0 | 23.5 |
| 3 | 131.6 | 32.9 | 147.7 | 16.8 |
| 4 | 142.3 | 35.6 | 123.3 | 54.6 |
| 5 | 141.6 | 35.4 | 131.6 | 45.4 |
| 6 | 138.8 | 34.7 | 143.6 | 29.9 |
| 7 | 150.9 | 37.7 | 149.8 | 38.8 |
| 8 | 143.6 | 35.9 | 127.4 | 52.1 |
| 9 | 142.7 | 35.7 | 139.4 | 39.0 |
| 10 | 139.8 | 34.9 | 126.2 | 48.5 |

All times are in ms. The smallest margin is 16.8 ms, against 6.0 ms at first
paint before this card. A first set of 10 runs was discarded: the dev build had
stopped at the missing include, so its benchmark binary predated this card.
