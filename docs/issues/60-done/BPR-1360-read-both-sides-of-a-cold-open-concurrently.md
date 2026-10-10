---
schema: "implement-rfc.card.v1"
id: "BPR-1360"
title: "A cold open reads both sides of its comparison at once"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"BPR-1344's live probe on macOS (BPR-Test PR #43), 2026-10-09","reservation":"docs/issues/_reservations/BPR-1360--BPR-1360--54fd657dec33.json","related_cards":["BPR-1344","BPR-1327"],"filed_by":"bpr-main-win, at the coordinator's request","coordinator_amendment":{"date":"2026-10-09","decision":"assigned to bpr-main-linux for the portable epoch host and application work; the live macOS gain stays owed on the Mac"},"acceptance":["BPR-1360:host-test","BPR-1360:bpr-1359-rule","BPR-1360:cancellation","BPR-1360:rows","BPR-1360:mac-live"]}
acceptance: [{"criterion":"BPR-1360:host-test","outcome":"pass","revision":"248ae8117","evidence":{"kind":"command","path":"tests/application/epoch_application_host_tests.cpp","detail":"application_epoch_host a_cold_open_reads_both_sides_at_once: the real diff host over the real epoch host, a fake transport whose exact reads each take 300 ms. Before (5fd933c6a product): open 603-607 ms, most in flight 1, fails at 'both sides' reads are on the wire together'. After: open 302-305 ms in every Debug run, most in flight 2, the second read starts about 300 ms before the first ends; asserts most in flight 2, second start before first end, open under 1.6 x one read (480 ms, sum 600). With --live-single-blob-ms=1044: 1046 ms (sum 2088); 2100: 2103 ms (sum 4200). Linux x86_64 WSL2, GCC 15.2 Debug; also passes in the GCC 15 -O3 Release tests-on tree."}},{"criterion":"BPR-1360:bpr-1359-rule","outcome":"pass","revision":"248ae8117","evidence":{"kind":"command","path":"tests/application/epoch_application_host_tests.cpp","detail":"application_epoch_host preparation_cannot_stop_or_delay_either_concurrent_reviewer_read: a prepared exact read held 20 s on the actor is stopped by the reviewer's cold open (cancelled, one stop observed); while both reviewer reads are on the wire, preparation asks for another prepared read, starts another window and stops it, and the diff host's preparation is emptied: neither reviewer read is stopped (stops observed stays 1), they overlap, nothing of preparation starts while they are on the wire, and the open takes 302-306 ms. Fails before at 'both of the reviewer's reads run together and preparation stops neither' (open 605 ms)."}},{"criterion":"BPR-1360:cancellation","outcome":"pass","revision":"248ae8117","evidence":{"kind":"command","path":"tests/application/epoch_application_host_tests.cpp","detail":"application_epoch_host a_superseded_cold_open_stops_both_reads: with both 20 s reads on the wire, the diff host's cancel stops both (two stops observed) and the job finishes well inside 5 s. Fails before at 'both of the open's reads are on the wire'."}},{"criterion":"BPR-1360:rows","outcome":"pass","revision":"22590d9f0","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1360-read-both-sides-of-a-cold-open-concurrently.md#rows-on-macos","detail":"macOS, build/dev (Debug) -j1 at 22590d9f0: application_epoch_host, application_diff_host, gh_cli_git_smart_transport (macOS only), application_first_tab_comparison and every application_large_review_* row pass (cache_bytes skips in Debug by design). application_large_review_prepared_open under BPR-1369's rule: 10 of 10 in Release tests-on (read counts unprepared 1+1 during, prepared 0+0 during; slowest prepared 21.2-23.0 ms against median unprepared 68.0-74.7 ms) and 10 of 10 in Debug. Linux: the same rows passed at 803a0ba3f, and BPR-1369 passes 10 of 10 in Release and Debug there."}},{"criterion":"BPR-1360:mac-live","outcome":"pass","revision":"22590d9f0","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1360-read-both-sides-of-a-cold-open-concurrently.md#live-cold-open-on-bpr-test43","detail":"Live, read only, MikeMarcin/BPR-Test#43, preparation off, the shipped composition offscreen through BPR-1344's probe course rebased onto 22590d9f0 (local, unpushed), Release tests-on, four runs at load average 9-24: 16 cold opens, median 2348 ms, p90 3149 ms, range 2229-4407 ms (per-run medians 2367, 2348, 2331, 3015). Before (BPR-1344 probe, 2026-10-08): 12 opens, median 4312 ms, p90 4766 ms. Both sides' exact reads overlapped in 16 of 16 opens by 631-1074 ms (0 of 12 before). About 0.25 s above one read: the per-read account checks share one gh process slot, so the second side starts one gh call later. X-RateLimit-Remaining core 4995 to 4606, GraphQL 4990 to 4941."}}]
---

# BPR-1360: A cold open reads both sides of its comparison at once

## Observation

On live macOS, opening a modified file that is not prepared takes about 4.3 s
(BPR-Test PR #43, 60 modified files; shown, median; offscreen, real app). Most
of that is two exact blob reads, one per side, run one after the other. Each
costs about 2.1 s: the shipped single read is five `gh` processes.

The comparison's own work, `diff_application_host`'s `perform`, materializes
the left side, waits for it, and only then asks for the right
(`src/application/src/diff_application_host.cpp`, the two `materialize` calls).
Each read goes through the repository's epoch actor, so nothing overlaps.

Measured on Windows over the same `gh` calls (12 runs): a single exact read as
the transport makes it is 1044 ms median (p90 1163). A cold open's two reads
are 2097–2110 ms in `application_epoch_host`'s live-shaped test with those
figures.

## Outcome

A cold open asks for both sides' blobs together, so their network round trips
overlap: one batched read for both sides where the transport reads batches,
or two reads in flight at once. The reviewer then waits about one round trip,
not two.

Unchanged:
- every read stays authorized by the selected comparison's grant;
- a stopped request reads nothing more;
- a prepared comparison is answered without any read.

## Acceptance

- A host test: a cold open over a transport whose reads each take a fixed
  delay completes in about one delay, not two. A control with the current
  sequential reads takes two.
- The Mac live course of BPR-1344 (c), or its successor, shows the cold open
  on PR #43 near one round trip. Watch `X-RateLimit-Remaining` as AGENTS.md
  requires.
- `application_epoch_host`, `application_diff_host`, `gh_cli_git_smart_transport`
  and the BPR-1327 large-review selection pass.

## Notes

Filed separately from [[BPR-1344]], which keeps preparation out of the
reviewer's way. It depends on nothing there.

## Coordinator amendment (2026-10-09)

The coordinator assigned this card to `bpr-main-linux`. Most of it is
portable epoch-host and application code, so `platforms` gains `linux`.
- **Fake transport:** a fake transport with per-read delays shows a cold
  open's wall time dropping from the sum of the two sides' reads to the
  larger of them, failing before and passing after.
- **BPR-1359's rule:** the reviewer's own two reads stay unstoppable by
  preparation.
- **Narrow diff:** keep it to the selected-diff read path. The Windows station
  holds an unlanded [[BPR-1344]] change to the epoch host (preparation waits
  for an idle reviewer, prepared blob reads as preparation jobs).
- **Live gain:** the macOS gain is measurable only on the Mac and stays owed
  there.

## Linux station run (bpr-main-linux, 2026-10-09)

Product at `248ae8117`, with a clang-tidy fix in `803a0ba3f` that does not
change behaviour. Measured below as `8a24f8dab` and `8297e420c`, the same
changes before they were cherry-picked onto `main`. WSL2 Ubuntu 26.04, GCC 15.2.0, offscreen, no live GitHub
call and no model call.

### Design

- **Diff host.** `perform` asks for a modified file's two blobs together
  through a new port call, `diff_blob_port::materialize_both`. By default it
  returns nothing, and the host then reads one side at a time as before. Each
  answer is still checked left first, as a single read would be. Preparation
  goes through the same `perform`, but the epoch host only answers the pair for
  the reviewer's own blobs.
- **Epoch host.** `materialize_both` runs one job on the repository actor that
  does both sides' cache and grant work. When both sides must be read exactly
  from the network, the actor lends the left read to the diff worker, which is
  blocked waiting for the result anyway. The actor makes the right read
  itself, then waits for the lent read to come back. Two reads are on the wire
  with no new thread. The lent read touches no cache session. Both answers are
  rechecked and published under the actor's authority lock, which is held
  until the waiter has them, as `materialize` does. Suppose the caller stops
  waiting before taking the read, because it was stopped or hit `BlobWait`.
  The actor then takes the read back and makes it itself; a stopped job's read
  ends at once.
- **BPR-1359.** The pair job is reviewer work. Constructing it stops a
  prepared exact read in flight, and no prepared read is admitted while the
  job is queued or running. Only the job's own stop stops either read: the
  diff job's stop, forwarded from a superseded selection or cancel, or the
  host's shutdown.
- **Order and cancellation.** Results still reach the diff host's decision
  lane as one terminal in order. A superseded selection stops both reads.

### Test, before and after

The new tests are in `application_epoch_host`. They run the real diff host
over the real epoch host, with the diff host on its own worker thread and the
actor on another. The fake transport records every read's start and end and
the most reads in flight at once.

| | open, 300 ms reads | most in flight | result |
|---|---|---|---|
| before (`5fd933c6a` product) | 603-607 ms | 1 | fails "both sides' reads are on the wire together" |
| after | 302-305 ms (every run) | 2 | passes; second read starts about 300 ms before the first ends |

The other two new tests also fail before the change:
`preparation_cannot_stop_or_delay_either_concurrent_reviewer_read` fails at
"both of the reviewer's reads run together and preparation stops neither",
and `a_superseded_cold_open_stops_both_reads` fails at "both of the open's
reads are on the wire". Measured round trips can be passed in: with
`--live-single-blob-ms=1044` (Windows' median) the open takes 1046 ms against
a sum of 2088, and with `2100` (macOS) it takes 2103 ms against 4200.

### Touched functions (for the BPR-1344 rebase)

- `diff_runtime_ports.hpp`: new `diff_blob_port::materialize_both`, a
  virtual that returns nothing by default.
- `epoch_application_host.hpp`: the `materialize_both` override and the
  class comment on concurrent ingress.
- `diff_application_host.cpp`: `implementation::perform` asks for both
  sides; new `implementation::blob_request`. Preparation scheduling
  (`pump_preparation`, `start_preparation`, `execute`) is untouched.
- `epoch_application_host.cpp`:
  - New `blob_pair_waiter` and `actor_blob_read`, after `blob_waiter`.
  - New `begin_actor_blob_read`, `read_remote_blob` and
    `finish_actor_blob_read`, before `materialize`. `materialize`'s actor
    closure now calls these three, and its logic moved there unchanged.
    BPR-1359's prepared branch is now in `begin_actor_blob_read`: the block
    after `durably_sparse_anchor`, where `PreparedGrantSequence &&
    !SelectedGrantSequence` holds. `read_remote_blob` releases the
    `prepared_read_registration` after the network read, as the old block's
    scope did. BPR-1344's "prepared blob reads become preparation jobs"
    belongs in those two places.
  - New `materialize_both` and `read_both_remote_blobs`, after
    `materialize`, and the public forwarder.
  - Untouched: `reviewer_work`, `prepared_read_registration`,
    `enqueue_actor_work`, `run_on_actor`, `prepare_selected_diffs`,
    `fetch_prepared_blobs`, and the actor FIFO.
- `docs/architecture/system-architecture.md`: one paragraph after the
  preparation section.

### Rows

`build/dev` (Debug) passed:

- `application_epoch_host` (10 of 10 repeated runs), `application_diff_host`,
  `architecture_boundaries`, `test_network_isolation_manifest`,
  `application_review_round_trip_composition_contract`,
  `large_review_fixture_contract`, `large_review_replay_contract` and
  `gh_cli_transport`.
- `application_first_tab_comparison`, and every large-review row but
  `application_large_review_prepared_open`. `cache_bytes` skips in Debug by
  design.
- The core-only preset, 259 of 259.

`coding-format-check` passes with clang-format 22.1.8. clang-tidy on the two
touched host sources reports nothing in the new code; the
`performance-no-automatic-move` findings on `noImport` predate this card.
`verify_documentation.py` prints DOCUMENTATION_OK. No `src/app` source
changed and the round-trip pin passes as is, so there was no reseal.

`gh_cli_git_smart_transport` is macOS-only and not registered here; the
transport is untouched.

### The 100 ms rows (optimized tree)

`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON
-DCMAKE_TEST_BUILD_DEPENDS=ON` builds clean at GCC 15 -O3 -Werror. The only
warning is in the vendored `sqlite3.c`. Run idle with `-j1`:

- `application_large_review_responsiveness` passes: BudgetsEnforced, no input
  budget failures or misses, largest pulse 63.7 ms.
- `application_first_tab_comparison` passes: BudgetsEnforced, largest pulse
  60.2 ms.
- `application_epoch_host` passes.

The tree was deleted afterwards.

### `application_large_review_prepared_open`: this card makes it fail

The row's rule is "every prepared open is faster than every unprepared one".
Two interleaved A/B runs used the same idle machine and swapped only the
`better_pull_review_large_review` binary between `5fd933c6a`'s product and
`8a24f8dab`:

| tree | before: failures | after: failures | unprepared min visible | prepared max visible |
|---|---|---|---|---|
| Release tests-on, -j1 idle | 0 of 10 | 8 of 10 | 47.5-50.6 ms before, 38.6-42.6 ms after | 40.1-43.2 ms before, 40.1-43.9 ms after |
| Debug | 5 of 10 | 7 of 10 | 87.5-266 ms before, 77-265 ms after | noisy (shared host) |

Earlier on this station, 6 more Release runs at `8a24f8dab` failed 4 times,
and 10 Debug runs failed 9 times while the shared host was loaded. Every
failure gives the same message.

The prepared opens did not get slower. The unprepared opens got faster: in
this local, complete-clone fixture both sides now come from the cache in one
actor job, which saves about 8 ms. Prepared opens stay at about 41 ms, so the
two distributions now overlap and the rule fails most of the time. In
Release it passed every time before this card. This is a change in the row's
outcome caused by this card, not BPR-1344's contention alone, so it waits on
the coordinator's decision. The rule is not re-specified here.

**Re-specified by [[BPR-1369]]** (`7edbe699f`, pushed with this card). The
rule now judges the work an open avoids: an unprepared open reads both blobs
of its comparison, and a prepared open reads neither. Wall time only bounds
the slowest prepared open by the median unprepared one plus max(15 ms, a
quarter of that median). With it the row passes 10 of 10 in Release and 10 of
10 in Debug on this station. BPR-1369 records the counts.

### Owed on the Mac

The live gain: on `MikeMarcin/BPR-Test#43`, the cold open of an unprepared
modified file should take about one exact read (about 2.1 s) instead of two
(4.3 s). On the Mac station, at a `main` that contains `248ae8117`:

```sh
gh api rate_limit --jq '.resources.core.remaining, .resources.graphql.remaining'
build/dev/tests/application/better_pull_review_application_epoch_host_tests --live-single-blob-ms=2100 --live-batch-ms=1881
```

Then rerun BPR-1344's live probe course, which made the 4.3 s figure,
offscreen with preparation off. The probe is the Mac-local branch
`claude/bpr-1344-live-probe` (`9bf77510d`, `c83e0596b`), rebased onto that
`main`. Record the median shown time over the same files. Its exact
invocation is recorded only on the Mac, so the command is not written out
here.

## macOS station run (2026-10-09)

At `22590d9f0` (`main` with `248ae8117`, `803a0ba3f` and [[BPR-1369]]),
Apple clang, offscreen. The machine was shared with other agents: 14 cores,
load average 9-24 during the live runs.

### Host test with the macOS round trip

`build/dev/tests/application/better_pull_review_application_epoch_host_tests --live-single-blob-ms=2100 --live-batch-ms=1881`
exits 0:

- cold open, two exact reads of 2100 ms: open 2102.8 ms (sum 4200), most in
  flight 2, the second read started 2100.4 ms before the first ended;
- the reviewer's cold open behind a prepared exact read: 302.3 ms.

### Live cold open on BPR-Test#43

BPR-1344's probe course (`bench_bpr1344_live_prefetch off`), rebased onto
`22590d9f0` on a local, unpushed branch. Only the conflicting timeline lines
in `epoch_application_host.cpp` and the batch-body toggle that
[[BPR-1359]] made obsolete were dropped. It runs the shipped composition
offscreen with no fixture: GitHub CLI sign-in, opens `MikeMarcin/BPR-Test#43`
(60 files), turns preparation off and times four cold opens of modified
files (`module_05`, `_20`, `_35`, `_50`) from the selection until the
comparison is shown. Release tests-on build, four runs, read only.

| | opens | median | p90 | range | exact reads overlapping |
|---|---|---|---|---|---|
| before (BPR-1344's probe, 2026-10-08, three runs) | 12 | 4312 ms | 4766 ms | 3549-4823 ms | 0 of 12 |
| after (this run, four runs) | 16 | 2348 ms | 3149 ms | 2229-4407 ms | 16 of 16, by 631-1074 ms |

Per-run medians after: 2367, 2348, 2331 and 3015 ms. The fourth run had two
slow opens (3515 and 4407 ms): each `gh` process there took 470-690 ms
instead of about 330, and the load average rose to 24. Network conditions
were otherwise comparable: the median `gh` process took 321 ms (351 before),
and an exact read's five-process core 1020 ms (1086 before). Before the
change the earlier probe ran at a load average of 60-340.

The two sides now overlap, and a cold open costs about one exact read
instead of two. It lands near 2.35 s, about 0.25 s above the 2.1 s
estimate. The cause is in the timeline. The account check that
`cli_observed_account_transport` makes before and after each read uses one
`gh` process slot. The second side's check waits for the first side's
(`proc.slot_wait` 300-500 ms), so the second read starts one `gh` call later.
The inner reads themselves are not serialized: each read gets its own
transport, and its lock never waited (`exact.locked wait_ms=0.0`). Making
the two account checks share one observation would remove that stagger. It is
recorded here and not filed.

Rate limit, read from `X-RateLimit-Remaining` response headers before each
run: core 4995 to 4606 after four runs (about 97 a run), GraphQL 4990 to
4941. The run stopped well above the 1500 margin. No write was made to the
repository.

### Rows on macOS

In `build/dev`, `-j1`: `application_epoch_host`, `application_diff_host`,
`gh_cli_git_smart_transport` (macOS only; not run on Linux),
`application_first_tab_comparison` and every `application_large_review_*`
row pass (`cache_bytes` skips in Debug by design).
`application_large_review_prepared_open` under [[BPR-1369]]'s rule passes 10
of 10 in Release tests-on and 10 of 10 in Debug. BPR-1369 records the
margins. Both trees were deleted afterwards.

## Windows station run (bpr-main-win, 2026-10-09)

Run on Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2. The `windows-dev-tour-cli` Debug tree was built at `3947245ca`, and the rows ran with `-j4` at `6cf49d91a`, whose later changes are cards and the pin list only. The quiet re-runs were serial: Debug at `b406b2e3b`, and Release in a Ninja tests-on tree at `f2af6cea3` (BPR-1344 over BPR-1369's rule). All of this card's Windows rows pass:

- `application_epoch_host` 1.81 s and `application_diff_host` 0.08 s;
- `application_review_round_trip_composition_contract` 12.67 s;
- `application_first_tab_comparison` 30.62 s;
- `application_large_review_responsiveness` 82.45 s;
- `application_large_review_prepared_open` 28.80 s;
- `gh_cli_transport` 1.12 s;
- `architecture_boundaries` 50.81 s and `test_network_isolation_manifest` 27.28 s.

`gh_cli_git_smart_transport` is registered on macOS only (`if(APPLE)`), so
Windows owes nothing for it.
