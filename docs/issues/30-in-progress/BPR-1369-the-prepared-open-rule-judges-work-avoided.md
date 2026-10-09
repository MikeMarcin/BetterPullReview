---
schema: "implement-rfc.card.v1"
id: "BPR-1369"
title: "The prepared-open rule judges work avoided, not a wall-time race"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1360's Linux station run, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1369--BPR-1369--7cac3359b2b2.json","related_cards":["BPR-1360","BPR-1327","BPR-1344"],"acceptance":["BPR-1369:old-rule","BPR-1369:work-avoided","BPR-1369:wall-time","BPR-1369:control","BPR-1369:rows"]}
acceptance: [{"criterion":"BPR-1369:old-rule","outcome":"pass","revision":"555cfdb17","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","detail":"Old rule (slowest prepared open < fastest unprepared open), application_large_review_prepared_open, GCC 15 -O3 tests-on tree, -j1 idle, at main with BPR-1360 (pre-rebase 072e3beba = 555cfdb17) plus the src/app counter of 7edbe699f: 4 of 10 fail ('a prepared open was no faster than every unprepared one'). Unprepared 40.9-69.8 ms, slowest prepared 40.9-50.0 ms. Applied to the visible times the new rule's 10 Release runs logged, the old comparison fails 8 of 10; to the 20 Debug runs, 20 of 20. BPR-1360's own run recorded 8 of 10 in Release."}},{"criterion":"BPR-1369:work-avoided","outcome":"pass","revision":"7edbe699f","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","detail":"New counts, from a testing-only fixture-access counter (diff_blob_reads: blobs the diff host read through its port, per blob). Every unprepared open: before 0+0, during 1+1 (both sides read). Every prepared open: before 1+1 (preparation read both sides), during 0+0, and PreparedHits +1. 10 of 10 Release (GCC 15 -O3 tests-on, -j1 idle) and 10 of 10 Debug (build/dev), 60 unprepared and 50 prepared opens each, no exception. Runs were on the commit's content before a lambda rename (identifier only); the renamed candidate passed once in Debug before and once after the rebase onto ae9379272."}},{"criterion":"BPR-1369:wall-time","outcome":"pass","revision":"7edbe699f","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","detail":"Slowest prepared open <= median unprepared + max(15 ms, median/4). Release 10/10: median unprepared 48.0-55.3 ms, slowest prepared 39.8-46.7 ms, least margin 19.1 ms (the 10 old-rule runs would also pass, least margin 8.0 ms). Debug 10/10: median 79.8-89.4 ms, slowest prepared 85.3-94.3 ms, least margin 6.0 ms; a fixed 15 ms would have left 1.0 ms, hence the relative term. In Debug prepared opens run 5-14 ms slower than the median cold open while the next window is prepared beside them."}},{"criterion":"BPR-1369:control","outcome":"pass","revision":"7edbe699f","evidence":{"kind":"command","path":"src/application/src/diff_application_host.cpp","detail":"Control, Release, reverted: execute() treats a prepared cache entry as a miss, so the open computes. 3 of 3 runs fail at 'a prepared open read its comparison from Git or the network' (prepared_open_1 during 1+1). Its visible times, 57.8-67.6 ms against unprepared medians near 55 ms, would pass the wall-time check alone: only the count catches it. Reverted; the reverted candidate passed once more before the tree was deleted."}},{"criterion":"BPR-1369:rows","outcome":"pass","revision":"7edbe699f","evidence":{"kind":"command","detail":"build/dev (Debug), -j1: every application_large_review_* row (cache_bytes skips in Debug by design), application_first_tab_comparison, application_epoch_host, application_diff_host, large_review_fixture_contract, large_review_replay_contract, architecture_boundaries, test_network_isolation_manifest and application_review_round_trip_composition_contract pass. After the rebase: prepared_open, epoch_host and the round-trip contract pass again. coding-format-check passes (/usr/bin/clang-format 22.1.8); clang-tidy on the two changed sources reports nothing new in changed lines; verify_documentation.py prints DOCUMENTATION_OK. Pins resealed: origin/main f3c65bc6a's pins matched in a detached worktree; full application and app manifest moved, mutation pin unchanged; still match after the rebase."}}]
---

# BPR-1369: The prepared-open rule judges work avoided, not a wall-time race

## Finding

`application_large_review_prepared_open` ([[BPR-1327]]) requires that every
prepared open appear sooner than every unprepared one
(`tests/end_to_end/large_review_benchmark.cpp`, `run_prepared_open`).
[[BPR-1360]] made a cold open read both sides of its comparison at once. On
the local, complete-clone fixture both sides now come from the cache in one
actor job, so unprepared opens got about 8 ms faster (38.6-42.6 ms in
Release) while prepared opens stayed at 40-44 ms. The two distributions
overlap and the row fails about 8 of 10 times in Release. The prepared opens
did not get slower.

On this fixture a cold open is cheap, so a race between two wall times no
longer says whether preparation did its job.

## Outcome

The rule says what preparation is for:

- **Work avoided.** A prepared open reads no blob of its comparison, from Git
  or the network, and computes no diff for it. Every unprepared open reads
  its comparison. Judged from counts, not from wall time.
- **Wall time.** Prepared opens are no slower than unprepared ones beyond a
  small, justified tolerance, so a prepared open cannot turn into a slow one
  unnoticed.

## Acceptance

- The old rule fails on `main` with [[BPR-1360]]: the row run 10 times in
  Release, failures counted.
- The new rule passes 10 of 10 in Release and 10 of 10 in Debug.
- A control that breaks preparation fails the new rule, and is reverted.
- The large-review rows, `application_epoch_host` and
  `test_network_isolation_manifest` pass; format and documentation gates
  pass.

## Boundaries

Only the prepared-open course's assertion changes. If no existing count
attributes reads to the selected open, the coordinator pre-approved
(2026-10-09) the smallest testing-only fixture-access counter in `src/app`,
with no production behaviour change and the round-trip pins resealed in the
same commit.

## Built and verified (bpr-main-linux, 2026-10-09)

Assertion commit `7edbe699f`, on `main` with [[BPR-1360]]'s
`248ae8117` and `803a0ba3f`. WSL2, GCC 15.2.0, offscreen; no live GitHub
call and no model call.

### The counter

No existing count could tie a read to the reviewer's open. Preparation of
the next window runs during a prepared open (`PeakEpochJobs` about 3),
`PulsesWithDiffWork` also counts preparation's deferred ingress, and the
selection's own tree lookups run on every open. So, as the coordinator
pre-approved, `src/app` gains one testing-only counter. Inside
`BPR_BUILD_TESTING`, the diff host's blob port is a `counting_diff_blob_port`.
It forwards `materialize` and `materialize_both` to the epoch host unchanged
and counts each blob it answers. Fixture access reports the count for one
blob (`diff_blob_reads`). A tests-off build hands the epoch host over
directly, as before. The round-trip pins are resealed in the same commit.
The full application and app manifest pins moved; the mutation pin did not.

### The rule

Old: the slowest prepared open must be faster than the fastest unprepared
one.

New, in `run_prepared_open`:

- **Unprepared opens.** Each reads both blobs of its comparison during the
  open.
- **Prepared opens.** Each finds both blobs already read by preparation,
  reads neither during the open, and is answered by its prepared comparison
  (`PreparedHits` + 1). The diff host computes a comparison only from blobs
  it has just read, so no read also means no diff computed.
- **Wall time.** The slowest prepared open is at most the median unprepared
  open plus max(15 ms, a quarter of that median). 15 ms is three of the
  course's 5 ms pulses. The relative term widens the margin for slower
  builds and machines: in Debug a fixed 15 ms left 1.0 ms in the worst run.
  The new check is looser than the old one in every case, so a platform
  that passed the old rule passes this check.

"No Git read" means no read of the comparison's blobs. Resolving the
selection's grant still looks up both paths' tree entries on every open.

### Counts

| | old rule | new rule | read counts |
|---|---|---|---|
| Release, -j1 idle | 4 of 10 fail | 10 of 10 pass | unprepared 1+1 during; prepared 1+1 before, 0+0 during |
| Debug | (20 of 20 logged runs would fail) | 10 of 10 pass | same |

Release: median unprepared 48.0-55.3 ms, slowest prepared 39.8-46.7 ms,
least margin 19.1 ms. Debug: median unprepared 79.8-89.4 ms, slowest prepared
85.3-94.3 ms, least margin 6.0 ms. In Debug a prepared open is 5-14 ms slower
than a typical cold one while the next window is prepared beside it. This is
the contention [[BPR-1344]] addresses.

### Control

`execute()` treated a prepared entry as a miss, so each prepared open
computed. Run 3 times in Release, it failed 3 of 3 at "a prepared open read
its comparison from Git or the network". Its prepared opens took
57.8-67.6 ms against unprepared medians near 55 ms, which the wall-time
check alone would accept. It was then reverted.

### Rows

`build/dev`: every `application_large_review_*` row passes (`cache_bytes`
skips in Debug). Also passing: `application_first_tab_comparison`,
`application_epoch_host`, `application_diff_host`,
`large_review_fixture_contract`, `large_review_replay_contract`,
`architecture_boundaries`, `test_network_isolation_manifest` and
`application_review_round_trip_composition_contract`. Format check, the
documentation gate and clang-tidy on changed lines are clean. The optimized
tree was deleted afterwards.

## Coordinator verification (bpr-main-linux, 2026-10-09)

At `249a17cd7`, in `build/dev` (Debug):
- `application_large_review_prepared_open` passed 36 of 37 runs. The one
  failure was the first run, straight after a full build, while the station
  was still loaded (load average about 3.7). The 32 runs after it, at a load
  near 1, all passed. Its failure message was not caught. The agent's
  tightest Debug wall-time margin was 6.0 ms, so a loaded Debug host can
  reach the wall-time check; the work-avoided checks are the rule's substance.
- `application_epoch_host`, `application_diff_host`, the round-trip contract,
  `architecture_boundaries` and `test_network_isolation_manifest` pass.

Done on Linux evidence. macOS and Windows runs of the new rule are owed. The
Windows station rebases its [[BPR-1344]] change to this course onto
`7edbe699f`.
