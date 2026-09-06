---
schema: "implement-rfc.card.v1"
id: "BPR-0633"
title: "Measure independent loaded-review close recovery"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0632"]
platforms: ["mac"]
acceptance: [{"criterion":"independent-loaded-close","outcome":"pass","revision":"5590d3ff953bb3be29cf1445270048b201f15893","evidence":{"kind":"observation","detail":"Public 515-file/1000-thread course verifies/render20k source, retains weak diff/index and native documents at close, observes last-window-close shutdown, zero cache/jobs after drain and destruction of weak/native owners afterward."}},{"criterion":"retained-storage","outcome":"pass","revision":"5590d3ff953bb3be29cf1445270048b201f15893","evidence":{"kind":"observation","detail":"Post-shutdown exact retained Git refs and reopened catalog OIDs equal fixture base/head; zero prune/delete dispatches. Validated fixture-only retained comparison supplies inspection identity after live authority clears; no product authority/storage/lifecycle change."}},{"criterion":"measured-resource-budgets","outcome":"pass","revision":"5590d3ff953bb3be29cf1445270048b201f15893","evidence":{"kind":"observation","detail":"Recorded M4 Pro/Mac Debug footprint92.095MiB loaded,91.391MiB drained,55.595MiB destroyed versus4.516MiB baseline; unchanged160MiB sampled-growth/96MiB cleanup limits pass. RSS/allocator retention and unsampled/native-platform limits are explicit."}},{"criterion":"focused-verification","outcome":"pass","revision":"5590d3ff953bb3be29cf1445270048b201f15893","evidence":{"kind":"observation","detail":"Final loaded-close14.66s, originalresources14.29s and composition53.33s pass3/3. Networkmanifest4.04s, builds, documentation and diff checks pass. Full render inspected; broader cache pressure/RFC/platform/live/human criteria remain open. Local only, no Actions/push/paid CI/account operation."}}]
---

# Measure independent loaded-review close recovery

BPR-0630 closes the public window after prune/cache deletion already releases
the large comparison. BPR-0632 closes with retained history but does not measure
memory. Neither establishes independent close recovery for a loaded large file.

## Bounded acceptance and ownership

- Add a separate Mac course in the existing real-composition large-review
  benchmark and CMake registration. Open the same 515-file / 1,000-comment
  fixture through ordinary provider/installed-Git paths, select and render the
  exact 20,000-line comparison, then close the public window while it still
  retains the raw diff, prepared index and native source documents.
- Observe the real last-window-close shutdown handoff and bounded drain.
  Verify zero derived cache/jobs, release of weak diff/index ownership and
  destruction of the window/source documents. Preserve retained Git/catalog
  identities and prove no prune/cache-delete action caused the release.
- Measure native RSS/physical footprint before load, with the large comparison,
  after close/drain and after application destruction. Reuse the documented
  provisional 160-MiB sampled-growth / 96-MiB cleanup-growth budgets from
  BPR-0630; distinguish allocator retention and sampled values from object
  lifetime and unsampled peaks. Keep broader cache pressure and platforms open.
- Own tests/end_to_end/large_review_benchmark.cpp and CMakeLists.txt, plus a
  contained demonstrated lifecycle defect and its focused tests if found.
  Extend the existing fixture-only maintenance inspector and its declaration
  to accept a validated retained comparison identity after shutdown clears live
  authority; refresh the associated composition source digest. This remains
  read-only fixture inspection, not a production authority or storage change.
  Preserve existing resource/default courses and the public composition.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work is local, synthetic and network-denied. No Actions, dispatch, push,
paid CI, credentials or production account/process operation. Earlier live and
human gates, Windows/Linux and explicitly deferred work remain open.


## Acceptance evidence

Source `5590d3ff953bb3be29cf1445270048b201f15893` adds `application_large_review_loaded_close`.
The existing real-composition benchmark opens all 515 files and 1,000 threads
through the normal provider pager and installed Git/cache actor, verifies every
line of the exact 20,000-line diff, navigates changes, scrolls and renders the
full 1600×1000/DPR-1 window before closing. The raw diff/index are still live
and both native documents retain 20,001 blocks (20,000 source lines plus the
fixture's final empty newline block) at the close gesture.

The real last-window-close signal starts shutdown; drain completes in 2.703 ms
with no failure and zero derived cache/diff/epoch jobs. The diff/index are still
owned by the window at that intermediate point. Ordered application destruction
then expires both weak handles and destroys the watched window and both native
source documents. The test does not mistake cancellation/drain for destruction.

After shutdown, exact retained Git refs and reopened catalog base/head OIDs
still match the stored fixture, with zero prune/delete dispatches. The original
inspector could not report these values after shutdown cleared live authority;
its fixture-only optional retained-comparison input now validates the captured
key/target binding and reopens the same local catalog. The normal live and
retired-cache inspection paths remain intact. No production lifecycle, core
policy, storage format or authority behavior needed a change.

On the same recorded M4 Pro Mac (48 GiB RAM, 14 logical processors,
macOS 26.6.2 arm64, Qt 6.10.3 offscreen/Fusion, Debug):

| Phase | RSS MiB | Physical footprint MiB | Derived cache |
|---|---:|---:|---:|
| Before application | 21.047 | 4.516 | 0 |
| Large file ready | 140.016 | 85.704 | 1 entry / 3,517,680 bytes |
| Before loaded-window close | 146.453 | 92.095 | 1 entry / 3,517,680 bytes |
| Shutdown drained | 145.797 | 91.391 | 0 |
| Application destroyed | 139.531 | 55.595 | 0 |

The existing provisional 160-MiB sampled-footprint-growth and 96-MiB cleanup
budgets pass unchanged. Sampled footprint falls 36.5 MiB from loaded window to
application destruction. Remaining allocator/Qt/process retention is explicit;
RSS does not return to baseline, and these samples do not prove unsampled peaks
or another platform's ceiling. Weak ownership and native destruction supply
separate lifetime evidence.

Final focused CTest: loaded-close, original prune/delete resources and application
review round-trip composition pass 3/3 at 14.66s, 14.29s and 53.33s. The network
isolation manifest passes 4.04s; changed dev targets build. Documentation gate
and `git diff --check` pass. Unchanged core/host/architecture and other benchmark
courses retain BPR-0632's relevant evidence; no broad matrix is required for
fixture-only inspection and this additional course.

The first probe's native block-count expectation missed the trailing empty Qt
block; the second exposed the inspector's cleared-live-authority assumption.
Both were harness limitations and are corrected. Full-window evidence is
inspected. BPR-0633 is complete; broader cache pressure, native platforms and
remaining RFC-0013/live/human gates stay open. No remote CI, Actions, push, paid
work or production credentials/account/process operation occurred. Task-created
logs/reports/rasters are consumed into this record and removed; reusable build
trees remain.
