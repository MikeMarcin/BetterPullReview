---
schema: "implement-rfc.card.v1"
id: "BPR-0630"
title: "Measure and verify large-review resource reclamation"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0629"]
platforms: ["mac"]
acceptance: [{"criterion":"public-resource-course","outcome":"pass","revision":"316390ce13411ff1dfbae1ba3896256a62629a78","evidence":{"kind":"observation","detail":"Network-denied actual composed prune/delete/window-close/destruction passes; exact refs/catalog, zero cache/jobs, weak diff/index expiry and before/pruned renders verified; 3 public courses pass in 51.96s."}},{"criterion":"precise-invalidation","outcome":"pass","revision":"316390ce13411ff1dfbae1ba3896256a62629a78","evidence":{"kind":"observation","detail":"Core and host tests prove full epoch identity, unrelated pending/current/cache preservation and cancelled or undelivered pruned completion rejection. Dev focused 7/7 plus core-only transition pass."}},{"criterion":"native-measurement","outcome":"pass","revision":"316390ce13411ff1dfbae1ba3896256a62629a78","evidence":{"kind":"observation","detail":"Mac phase RSS/physical-footprint recorded on card; measured 160/96 MiB sampled-growth/cleanup budgets pass; object reclamation distinguished from allocator/native retention and wider RFC limitations."}},{"criterion":"integration-and-hygiene","outcome":"pass","revision":"316390ce13411ff1dfbae1ba3896256a62629a78","evidence":{"kind":"observation","detail":"Normal application and focused targets build; source composition contract passes 50.48s, documentation gate and 62 tests pass, diff check clean. Local fixtures only; no Actions or production account operation."}}]
---

# Measure and verify large-review resource reclamation

BPR-0629 establishes responsive initial publication and explicit diff/index cache
accounting, but its final two-entry snapshot does not prove reclamation. Existing
`application_epoch_maintenance_public_path` covers retained refs/catalog records
and cross-process prune/delete, not large running-process memory or immutable
object lifetime. Syntax cache policy is separately bounded by entry/span limits;
it is not the same resource as the selected raw diff and its semantic index.

## Bounded acceptance and ownership

- Extend the stored `large-review-v1` application course with a separately named,
  network-denied local resource lane. Drive the actual public epoch prune/cache
  deletion and application-close paths in a disposable test-owned data root.
  Never delete a user cache or use a production account/process.
- Sample this test process's native resident/physical-footprint values at
  meaningful baseline/load/large-selection/cleanup/destruction phases on the Mac.
  Report hardware, configuration, elapsed context and explicit cache counters;
  distinguish live ownership from allocator/Qt retention. Measure before choosing
  documented, justified local thresholds; do not infer a universal memory bound.
- Use weak ownership to prove the large raw diff and prepared index are released
  after their review data becomes unavailable and after close/destruction. Check
  cache accounting, jobs and selected UI against completed cleanup; old source
  must not remain usable after successful prune/delete. Retained catalog identity
  and documented maintenance consequences remain correct.
- Correct any demonstrated in-scope reclamation or stale-projection defect;
  preserve cancellation, unrelated retained epochs/reviews and immutable cache
  semantics. Add focused host/Qt checks for the actual defect and rerun the public
  lane. Split unrelated or materially wider findings before expanding ownership.
- Own `tests/end_to_end/large_review_benchmark.cpp` and its CMake registration,
  narrow application fixture evidence in `src/app/src/github_application*`,
  and necessary prune/invalidation integration in the existing diff/epoch
  composition. Source-manifest bookkeeping follows reviewed composition edits.
  A new generic performance subsystem or platform adapter is outside this card.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
The preceding source is `fb55c797`. Broader syntax/cache-budget pressure,
many-epoch replay and Windows/Linux resource evidence remain RFC acceptance;
this card cannot by itself close the memory or entire performance RFC claim.
All work stays on this Mac. No GitHub Actions, push, paid CI, credentials or
production app launch.

## Demonstrated defect and focused fix

The first resource course removed the selected epoch's refs and marked both
anchors pruned, but retained the raw diff/index and visible source. Cache deletion
subsequently released them. Extend this card's ownership to the existing core
diff transition, diff application host and their focused tests: consume accepted
prune completion, invalidate only comparisons using each exact epoch identity,
and preserve other epochs, catalog incarnations and pending selections. No new
subsystem or live operation is required.

## Acceptance evidence

Source revision `316390ce13411ff1dfbae1ba3896256a62629a78`. The accepted prune catalog delta now invalidates only
comparisons whose retained left or right anchor uses the exact authority scope,
catalog incarnation and epoch ordinal. Core cancellation rejects late results;
the host drops corresponding cache/job ownership. A newer unrelated pending
selection keeps its projection, and unrelated epoch caches remain reusable.
The composition clears correlated selected-diff and syntax bindings when the
visible comparison is affected. Failed/cancelled prune intents do not enter this
path; only the already accepted availability change does.

The initial public resource course failed in 15.14s: refs/catalog were pruned
but the raw diff, semantic index and source remained. The final network-denied
course passes in 14.32s. It drives public maintenance confirmations only inside
a QTemporaryDir fixture, verifies both retained refs are absent and both catalog
anchors remain pruned, then verifies the cache-deletion marker, removed
repository and cache-deleted anchors. Weak diff/index references expire at
prune and remain expired through delete, close and destruction. Selected source
is empty, cache entries/bytes and diff/epoch jobs are zero after cleanup. The
last-window-close signal starts the ordinary shutdown API; all hosts drain and
the application is destroyed. The driver now enters QApplication's main loop,
which is necessary for native last-window-close delivery; nested measurement
loops alone had left the close-triggered shutdown unstarted.

MacBook Pro / Apple M4 Pro, 48 GiB, 14 logical processors; macOS 26.6.2 arm64,
Qt 6.10.3, Debug, offscreen/Fusion, 1600×1000 at DPR 1. Stored fixture has 515
files, 1,000 review threads and the 20,000-line selected comparison. Native Mach
task samples apply to this process only; all values below are MiB except cache
counts/bytes. Full-window before/pruned renders were inspected: 20 changes and
exact fixture source before, empty panes and explicitly pruned epoch selectors
afterward. The fixtures contain no production account data.

| Phase | Resident MiB | Physical footprint MiB | Diff cache entries / bytes |
|---|---:|---:|---:|
| before_application | 20.97 | 4.61 | 0 / 0 |
| application_constructed | 72.16 | 22.77 | 0 / 0 |
| review_loaded | 99.08 | 45.88 | 0 / 0 |
| large_file_ready | 140.78 | 86.69 | 1 / 3517680 |
| after_prune | 141.80 | 61.09 | 0 / 0 |
| after_cache_delete | 148.00 | 67.30 | 0 / 0 |
| shutdown_drained | 147.34 | 66.59 | 0 / 0 |
| application_destroyed | 147.20 | 66.56 | 0 / 0 |

The initial corrected course measured 81.4 MiB footprint growth above baseline
at large selection and 56.0 MiB after prune. The checked-in provisional Mac
budgets are 160 MiB sampled growth and 96 MiB after cleanup: approximately
2×/1.7× those observations, allowing allocator/native rendering variation while
catching substantial regressions. The final course remains within both. These
are sampled phase budgets, not an unsampled peak or universal process ceiling.
RSS need not return to baseline: Qt, rendering and allocator pages remain even
when weak ownership and exact derived-cache counters show released data. The
additional post-prune window grab also affects subsequent allocation retention.
No allocator trimming or artificial sleep is used to force memory downward.

Final phase durations: paged load 10,485.38 ms, large selection 1,937.78 ms, prune
895.00 ms, cache delete 16.44 ms, close/shutdown 2.57 ms. Large-selection maximum
UI interval is 67.52 ms. Existing scroll/navigation and exact displayed-source
checks remain in this course; the separate original benchmark still exercises
32 rapid selections. Close follows maintenance in this course; this is not
independent native-memory evidence for closing an otherwise loaded review.

Focused validation, all local and network-denied:

- Dev `core_diff_transition`, `application_diff_host`,
  `qt_diff_workbench_scale_100`, `qt_workbench_window_scale_100`, both architecture
  boundary checks and `test_network_isolation_manifest`: 7/7, 99.80s.
- `application_large_review_benchmark`,
  `application_unavailable_diff_public_path`, `application_large_review_resources`:
  3/3, 51.96s (22.81s, 14.81s, 14.32s).
- `application_review_round_trip_composition_contract`: 1/1, 50.48s. Reviewed
  application source digests were updated; mutation-dispatch scope is unchanged.
- Core-only `core_diff_transition`: 1/1, 0.35s. Exact epoch identity, unrelated
  pending/current isolation and stale pruned completion are core-only evidence.
- Relevant dev/core-only targets and the normal application build pass. The
  latter was built, not opened with a production account.
- Documentation gate, its 62 unit tests and `git diff --check` pass.

This completes the measured maintenance/reclamation slice. Broader cache
pressure, independently loaded close, many-epoch replay and Windows/Linux
resource budgets remain RFC-level work. No GitHub Actions, push, paid CI, live
account operation or change to the remembered production sign-in occurred.
Task-created editing scripts, command logs and generated measurement/render
files are removed after these durable observations are recorded; reusable
build targets remain in the existing build tree.
