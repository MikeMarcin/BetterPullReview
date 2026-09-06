---
schema: "implement-rfc.card.v1"
id: "BPR-0630"
title: "Measure and verify large-review resource reclamation"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0629"]
platforms: ["mac"]
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
