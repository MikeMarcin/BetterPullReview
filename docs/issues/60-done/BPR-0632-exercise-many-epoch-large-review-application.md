---
schema: "implement-rfc.card.v1"
id: "BPR-0632"
title: "Exercise many-epoch large-review application replay"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0631"]
platforms: ["mac"]
acceptance: [{"criterion":"composed-stored-epochs","outcome":"pass","revision":"75d1494e9b993b19ce2822fcdb7cc4d5023b837b","evidence":{"kind":"observation","detail":"All 32 stored heads traverse public refresh, production pager and installed Git/cache actor. Exactly 897 reads, complete workload, exact independent catalog/ref OIDs and 64-row history without ingestion/history model resets."}},{"criterion":"pending-selection-and-source","outcome":"pass","revision":"75d1494e9b993b19ce2822fcdb7cc4d5023b837b","evidence":{"kind":"observation","detail":"Old core handler fails the new second-selector regression; fix preserves pending explicit sides, cancels old resolution and rejects stale terminals/catalog revisions. Real app performs 31 changes (30 during active work), exact historical/latest/previous source and inspected full-window renders; shutdown passes."}},{"criterion":"measured-local-budgets","outcome":"pass","revision":"75d1494e9b993b19ce2822fcdb7cc4d5023b837b","evidence":{"kind":"observation","detail":"Same M4 Pro/Debug unchanged final course passes in 320.25s: refresh/source UI maxima 71.247/52.518 ms and configured 100-ms,12-s,2-s limits. An earlier 112.448-ms outlier is retained with unproven cause and did not recur on one unchanged repeat. No absolute/native-platform guarantee; cache35/116505 bytes, sampled jobs1diff/1workbench/3epoch."}},{"criterion":"verification-and-scope","outcome":"pass","revision":"75d1494e9b993b19ce2822fcdb7cc4d5023b837b","evidence":{"kind":"observation","detail":"Core/host2/2, core-only/both boundaries3/3, four unchanged application compatibility/cleanup/composition lanes and isolated history repeat pass. Network isolation, builds, documentation/diff checks pass. RFC-wide, independent loaded-close, native-platform and earlier human/live gates remain open; all work local, no Actions/push/paid CI/account access."}}]
---

# Exercise many-epoch large-review application replay

The large-review fixture contains 32 distinct Git heads, but the composed course
currently opens only the latest and therefore creates one application epoch.
RFC-BPR-0013 requires many-epoch benchmark and stale-selection evidence.

## Bounded acceptance and ownership

- Drive all 32 stored heads through the ordinary public refresh, provider pager,
  installed Git/cache actor and durable epoch catalog in a disposable local root.
  Extend only the existing test fixture transport with exact bounded head support.
- Exercise public epoch selection/history, current and historical source identity,
  selection supersession and cancellation while jobs are active. Check actual
  source and retained refs/catalog against stored independent head/delta values.
  Render the full application; unit/model counts alone do not establish acceptance.
- Record workload, Mac configuration, per-refresh/selection timing, UI input and
  bounded job/cache counters. Measure before setting justified local thresholds;
  retain native-platform and broader RFC limitations.
- Own existing fixture configuration/transport and narrow fixture evidence under
  src/app/, existing large_review_benchmark.cpp and CMake registration, plus
  focused tests for any contained demonstrated product defect. The reproduced
  rapid-selector refusal additionally owns the single-anchor handler in
  src/core/src/epoch_transition.cpp and tests/core/epoch_policy_tests.cpp:
  preserve the pending explicit pair, cancel superseded resolution and reject
  late results without weakening catalog/authority checks. Split materially
  wider defects before expanding ownership. Keep the ordinary latest-head course.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work is local, network-denied and synthetic. No Actions, workflow dispatch,
push, paid CI, credential access or production app/account operation. Existing
human/live gates and Windows/Linux work remain open.

## Current course design

The fixture compiles one completed refresh script at a time, without retaining
32 duplicated thread bodies. It accepts only the explicit stored head inventory
for Git transport; ordinary fixtures retain their previous exact head set. The
new course has a 540-second internal / 600-second CTest containment budget for
32 sequential paged refreshes, not a preclaimed latency target. Each refreshed
source is checked against the generator's independent source-N/head-N+2 rule.
The final real history action and selectors drive historical source, selection
supersession, and the previous-upload comparison. Narrow evidence reopens the
normal catalog and compares exact loose retained refs with every stored head.
No Qt/core product decision is bypassed, and no fixture-only diff request helper
is used.

The first complete ingestion course found a fixture-inspection defect: retained
ref paths encode epoch ordinals as 16 lowercase hexadecimal digits, while the
old getter used decimal digits (indistinguishable for ordinals 1–9). The getter
now follows the actual installed-Git encoding and exposes exact catalog/ref OIDs.
No stored ref format or production Git behavior changes. Rapid selection also
exposed a product defect: controls remain enabled while the first anchor is
resolving, but the one-sided core handler requires the just-cleared accepted
pair and rejects subsequent choices. The initial disabled-control diagnosis
was incorrect. The contained correction must use the pending exact explicit
pair while retaining current catalog/authority checks and stale-result refusal.
The isolated regression fails on the old handler at the second enabled selector,
then passes with the correction. It exercises alternating left/right changes,
the exact cancellation request, stale completion, stale catalog revision and
acceptance of only the final explicit pair. Existing catalog-incarnation and
epoch-host cancellation/recovery tests remain passing. Core-only and both
architecture-boundary checks pass together (3/3, 84.21s).

Before adding thresholds, the 32 refresh/source phases measured a maximum
67.903 ms input interval, 9.165 s refresh and 1.113 s source selection on the
recorded M4 Pro Mac/Debug configuration. Provisional checked-in limits are
100 ms input interval, 12 s refresh and 2 s source selection. These thresholds
apply to the named refresh/source phases of this Mac course, not portable CI
or an unmeasured native platform. Historical selection and supersession keep
their own reported timings rather than inheriting an unsupported latency claim.


## Acceptance evidence

Source `75d1494e9b993b19ce2822fcdb7cc4d5023b837b`. All 32 stored heads traverse real public refresh,
the provider pager, installed Git/cache actor and durable catalog in a disposable
network-denied root. Exactly 897 recorded account reads are consumed (one /user
plus 32 complete 28-read snapshots). Every epoch's base/head catalog OIDs and
both retained loose refs match the independent stored inventory. Each refreshed
source matches the independent generator rule. History reaches 32 epochs / 64
rows with zero model resets through ingestion and the public history refresh.

The final course performs 31 public right-selector changes, 30 while diff,
workbench or epoch work is outstanding. Final epoch 32 source survives all late
work; the first historical head displays `int fixture_0030 = 1;`, and previous
upload correctly compares that value at epoch 31 with `32` at epoch 32. Both
full 1600×1000/DPR-1 windows are rendered and inspected. No fixture diff-request
helper is invoked. Sampled peaks are one diff job, one workbench request and
three epoch jobs. Final immutable diff/index cache is 35 entries / 116,505 bytes,
below unchanged 64-entry / 64-MiB ceilings. Shutdown drains in 3.974 ms without
cleanup failure; this is not independent loaded-close memory evidence.

Measured on the previously recorded MacBook Pro M4 Pro, 48 GiB RAM, 14 logical
processors, macOS 26.6.2 arm64, Qt 6.10.3, offscreen/Fusion, Debug:

| Phase | Elapsed milliseconds | Maximum UI interval / highest phase p95 (ms) |
|---|---:|---:|
| 31 public refreshes | 8,363.974–9,829.245 | 71.247 / 6.082 |
| 32 source selections | 818.321–1,111.422 | 52.518 / 5.963 |
| Public retained-history refresh | 1,773.654 | 18.302 / 6.603 |
| First historical head | 3,063.894 | 18.845 / 6.560 |
| 31-choice supersession and final drain | 1,679.006 | 10.848 / 5.841 |
| Previous-upload comparison | 2,045.269 | 12.177 / 5.987 |

The final unchanged history replay passes in 320.25s, including the checked-in
100-ms refresh/source input, 12-s refresh and 2-s source-selection thresholds.
The first run after the functional fix passed all source/epoch/supersession
checks but recorded a single 112.448-ms input interval during source selection
20 (1,888.430-ms elapsed), failing the 100-ms target. The same code and limits
were rerun once; the pause did not recur. This observation is retained, its
cause is unproven, and the target remains provisional. A passing repeat does
not establish an absolute worst-case latency guarantee or erase that sample.
Broader native-platform and RFC-wide performance acceptance remains open.

Focused local verification:

- `core_epoch_policy` and `application_epoch_host`: 2/2 pass, 0.90s. The added
  regression demonstrably fails on the old one-sided handler before the fix.
- Core-only `core_epoch_policy`, `architecture_boundaries` and
  `architecture_boundary_contract`: 3/3 pass, 84.21s; core-only target builds.
- Latest-head benchmark, unavailable-diff public course, resource reclamation
  course and application review round-trip composition contract pass unchanged
  at 23.30s, 15.49s, 14.16s and 49.55s. The first aggregate was 4/5 due solely
  to the recorded history timing miss; the isolated unchanged repeat passes.
- `test_network_isolation_manifest`: pass, 3.88s. Changed dev targets build;
  documentation structure and `git diff --check` pass. The unchanged 62
  documentation-verifier unit tests retain BPR-0630's passing evidence.

BPR-0632's bounded many-epoch course and contained selector fix are complete.
RFC-0013, Windows/Linux work and earlier live/human gates remain open. Synthetic
thread contents are workload projections, not historical GitHub captures.
No Actions, workflow dispatch, push, paid CI, credential access or production
account/process operation was performed. Task-created logs, reports and rasters
are consumed into this record and removed; reusable build trees remain.
