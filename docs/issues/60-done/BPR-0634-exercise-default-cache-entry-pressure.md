---
schema: "implement-rfc.card.v1"
id: "BPR-0634"
title: "Exercise default cache entry pressure"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0633"]
platforms: ["mac"]
acceptance: [{"criterion":"default-entry-eviction","outcome":"pass","revision":"470adea7d7683577893b954f7a0c3d38c1f60689","evidence":{"kind":"observation","detail":"Real public selection of 65 small files grows the default cache to 64 entries. The first raw diff/index weak owners survive through capacity and expire on selection 65. Peak 212,992 bytes stays below 64 MiB, isolating entry pressure."}},{"criterion":"exact-revisit-and-cleanup","outcome":"pass","revision":"470adea7d7683577893b954f7a0c3d38c1f60689","evidence":{"kind":"observation","detail":"Every selected blob and all eight source lines match the independent fixture. Public evicted-key revisit runs a normal diff job and renders exact source. Window close drains cache/jobs, then ordered destruction releases raw/index/window/documents; exact Git/catalog identities persist with zero maintenance."}},{"criterion":"measurements-and-verification","outcome":"pass","revision":"470adea7d7683577893b954f7a0c3d38c1f60689","evidence":{"kind":"observation","detail":"Same M4 Pro Mac Debug course: 65 selections plus revisit take 967.378–1,076.604 ms each, maximum UI interval 54.600 ms. Sampled peaks are one diff, one workbench and two epoch jobs. Footprint is 52.954 MiB before close, 52.173 after destruction; unchanged 160/96 MiB budgets pass. Four focused tests, build, documentation and diff checks pass."}},{"criterion":"scope-and-next","outcome":"pass","revision":"470adea7d7683577893b954f7a0c3d38c1f60689","evidence":{"kind":"observation","detail":"BPR-0635 remains required for 64-MiB byte pressure; no broader RFC/native/live/human closure. No production source/account changes, Actions, push or paid CI. Task artifacts are consumed and removed."}}]
---

# Exercise default cache entry pressure

Host unit tests prove small-capacity eviction. The real application has not yet
been driven past its default 64-entry immutable diff/index cache capacity.

## Bounded acceptance and ownership

- Add an independent public large-review course selecting 65 distinct small
  files through the normal composition, provider/installed Git/cache actor and
  file drawer. Do not reduce configured cache limits or inject cached results.
- Verify exact blob/source identities, the growing cache up to its default
  capacity, eviction at the next unique selection, and weak ownership release
  of both the evicted raw diff and its prepared index. Revisit the evicted file
  through the same public selection path and verify correct rematerialized data.
- Record entry/byte/job and phase/input counters plus native memory samples;
  render the real window. Close/drain/destroy after pressure, using the existing
  ownership and preserved Git/catalog checks. Retain provisional Mac resource
  budgets and avoid portable timing claims.
- Own tests/end_to_end/large_review_benchmark.cpp and CMakeLists.txt. Reuse the
  current fixture/inspection ports; correct a contained demonstrated defect
  with focused product tests if needed. Keep original courses intact.
- Byte pressure remains separately required in BPR-0635; this card cannot close
  the broader cache-budget criterion by itself.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Local Mac, synthetic and network-denied only. No Actions, dispatch, push, paid
CI, credentials or production account/process operations. Earlier human/live
and Windows/Linux gates and deferred work remain open.


## Acceptance evidence

Source `470adea7d7683577893b954f7a0c3d38c1f60689` adds `application_large_review_cache_entries`.
The normal public application opens the stored latest-head 515-file/1,000-thread
review, then selects files 100–164 through the real file drawer. Each accepted
comparison's base/head blob identities match the independent manifest and all
eight source lines match the generator recipe. No cache limits are reduced,
no fixture diff helper is invoked and no production behavior needed a change.

The empty default cache grows from one to exactly 64 entries. Weak references
to the first raw diff and prepared index remain alive through capacity, then
both expire on the 65th distinct selection. Entry count remains 64 and peak
accounted bytes are 212,992 (208 KiB), far below the 64-MiB byte ceiling. A public
revisit to file 100 exercises a real diff materialization job and displays its
correct exact source again. The full 1600×1000/DPR-1 window is rendered and
inspected. This demonstrates entry-pressure eviction, not byte pressure.

After pressure, public window close starts normal shutdown, empties the derived
cache and drains jobs in 2.185 ms. Ordered application destruction releases
the current weak diff/index and watched window/native documents. Exact retained
Git refs and reopened catalog identities survive with zero prune/delete actions.

Same recorded M4 Pro Mac, 48 GiB RAM, 14 logical processors, macOS 26.6.2 arm64,
Qt 6.10.3 offscreen/Fusion, Debug:

- 65 new selections plus the evicted-key revisit: elapsed 967.378–1,076.604 ms
  per phase; maximum UI interval 54.600 ms, highest phase p95 5.817 ms.
- Sampled peak jobs: one diff, one workbench request, two epoch jobs.
- Baseline RSS/physical footprint: 21.172/4.547 MiB. At entry capacity:
  101.297/46.766 MiB. Before close after rendering: 107.531/52.954 MiB.
  After drain: 106.875/52.251 MiB. After destruction: 106.703/52.173 MiB.
- Existing 160-MiB sampled-growth / 96-MiB cleanup-growth limits pass unchanged.
  Retained allocator/Qt memory is explicit; these are sampled process values,
  not unsampled peaks or portable timing guarantees.

Focused local CTest passes 4/4: network isolation 4.06s, entry pressure 77.49s,
independent loaded-close 13.44s and original prune/delete resources 14.41s.
The changed benchmark target builds; documentation and diff checks pass. Shared
close/drain/destruction assertions remain intact in both original resource
courses. No app/core/host production source changed; other unchanged acceptance
retains BPR-0632/BPR-0633 evidence rather than repeating their matrices.

BPR-0634 is complete. BPR-0635 remains required to exceed the independent default
64-MiB byte ceiling with large comparisons; this course cannot close the broader
cache-budget criterion. Remaining RFC, native-platform and live/human gates stay
open. No Actions, remote CI, push, paid CI or production account/process operation
occurred. Task-created logs, reports and rasters are consumed into this record
and removed; reusable build trees remain.
