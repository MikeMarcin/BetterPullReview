---
schema: "implement-rfc.card.v1"
id: "BPR-1763"
title: "Measure composed allocator behavior and preserve tool paths"
status: "Done"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC06","outcome":"pass","revision":"df6e8be48ff17271e2258f0040e4ca87795f44bd","evidence":{"kind":"observation","detail":"Native C/C++/Qt/loaded-module/cross-free/4096/cross-thread/failure rows; actual fresh-configure bad_alloc and generic-arch regression on Mac/Windows/Linux; ASan/UBSan plus seven libFuzzer corpus rows and instrumented public CLI; actual archive/public CLI and paired desktop prune/delete. Exact identities and limits: docs/verification/RFC-BPR-0093.md#distribution-result--mandate-r4. Mac Intel/signed-installed remains AC03 pending."}},{"criterion":"AC08","outcome":"pass","revision":"b449356dd4f9ae81ea3a9792517fe33309d8ecab","evidence":{"kind":"observation","detail":"Matched native Release reports and unchanged six-row memory course complete with exact baseline/candidate hashes and platform conditions. Baseline to MI passes Mac6/6 to3/6, Windows6/6 to2/6, Linux4/6 to2/6; extra footprint failures/retention are recorded regressions, no limit/trim change and no BPR-0967 fix. Supplemental nongating close releases all tracked owners. Ledger records raw paths, timings, memory and heap-counter limitations; reporting pass grants no performance/release approval."}}]
---

# Measure composed allocator behavior and preserve tool paths

## Scope and acceptance

Run representative product workflows and allocator-boundary stress after
native interception is known (AC06). Verify sanitizer/fuzzer selection and
ordinary allocation-failure behavior. Compare matched Release workloads for
launch, large-review navigation and memory/close/prune on the same hardware
with the developer baseline switch (AC08). Re-run unchanged existing memory
budgets and keep BPR-0967's pre-existing failures distinct. Record conditions,
candidate identities, raw outcomes and limitations; no speedup is presumed.

## Scheduling amendment — mandate r4, 2026-10-24

The owner's instruction to keep working without an Intel Mac authorizes the
independent package and measurement work. Accepted [[BPR-1758]], [[BPR-1760]]
and [[BPR-1761]] remain prerequisites; full Mac acceptance no longer blocks
starting this card. The live distribution milestone already depended only
on Windows/Linux. The checked plan amendment makes the pending
`pinned-macos-integration` dependency explicit at final closeout. Every AC07
and AC08 requirement, authored outcome, work selector and exit remains intact;
this card cannot be Done while its own required evidence is incomplete.

Source basis: the independent native Windows/Linux installed courses in the
[ledger](../../verification/RFC-BPR-0093.md), RFC AC03 versus AC06–AC08, and
the separate package/instrumentation/developer-baseline paths in
`packaging/`, `cmake/BprMimallocProvider.cmake` and `CMakePresets.json`.
Native Intel/signed-installed Mac AC03 remains on [[BPR-1759]] and final
[[BPR-1764]] still depends on it. No acceptance or release approval is waived.
The check receipt is under `build/mimalloc-distribution/amendment-check.json`.

## Result — mandate r4

AC06 and AC08 are complete as engineering checks and a measurement report;
there is no performance-benefit requirement. The [final ledger](../../verification/RFC-BPR-0093.md#distribution-result--mandate-r4)
binds source `b449356dd`'s fresh-configure correction, native product stress and
failure semantics, the real ASan/UBSan/libFuzzer path, public standalone CLI
workflows, and paired Release desktop prune/delete courses on all three stations.
Latest package/entrypoint producer is `df6e8be48`; prior native installed
candidates and their evidence remain exact and unchanged.

Corrected matched six-row Release results, baseline → mimalloc: Mac 6/6 → 3/6,
Windows 6/6 → 2/6, Ubuntu 4/6 → 2/6. Failing rows exceed the unchanged process
footprint budget. The supplemental nongating close diagnostic releases every
tracked owner yet retains more footprint with mimalloc on every station. This
is a recorded regression, not a memory pass or a BPR-0967 fix. Exact candidate
hashes, native conditions, timing/memory values, failed rows, raw record paths
and heap-counter limitations are in the ledger. No limit or trim behavior
changed. No native Intel Mac, signed-installed, release or trust acceptance is
claimed; BPR-1759, BPR-1762 and final BPR-1764 keep those obligations.

## Retention diagnostic follow-up under owner r5

The [follow-up ledger](../../verification/RFC-BPR-0093.md#retention-follow-up--benchmark-only-collection)
binds a test-only manual-collection mode, real default/100/10 ms idle timelines,
and unchanged ordinary failures on native Mac/Linux. The owner rejected a
product zero-delay default: the isolated prototype was reverted and never
integrated. Main changes only the benchmark and the optimized allocation probe;
all product/provider/package settings and budgets remain unchanged. Manual
`mi_collect(true)` runs after owner release and original sampling, reports its
cost separately, and cannot produce a normal acceptance marker. This augments
this card's completed reporting, not performance acceptance or a BPR-0967 fix.
The Release probe's optimized-away impossible malloc call is corrected without
weakening null/ENOMEM; native Mac/Ubuntu Release rows pass. Owner-r5 no longer
requires Intel execution; actual signed-installed/Cocoa Mac and final package
acceptance remain pending on BPR-1759/1762/1764.


## Cleanup-policy reassessment under owner r5

The [completed-boundary comparison](../../verification/RFC-BPR-0093.md#cleanup-policy-reassessment--completed-boundaries)
adds a test-only opt-in policy with enforced original active/peak limits,
pre/post cleanup samples, native baseline release APIs and collection latency.
All original flags remain authoritative. Full Mac baseline/mimalloc ordinary
results are 6/6 and 3/6; their collected counterparts are 6/6 and 4/6. Ubuntu
stays 4/6 and 2/6 under either policy. The one Mac resources verdict change
cannot be attributed solely to collection because its pre-collection peak
was already lower. Final focused Mac proof drops prune current growth
162.33→119.69 MiB while retaining the failed 162.34 MiB lifetime peak against
160. Linux also retains active/peak and some cleanup-current failures.
Neither baseline behavior nor allocator heap coverage is assumed.

Test producers `806df31d2`/`84a177214` integrate as `c6be37f34`/`42f0ea86b`.
Raw failed samples are now retained in partial JSON. A lower post-collection
Linux native-peak reading cannot hide the larger pre-collection reading.
No product default/hook, budget, fixture, package or CTest registration changes.
This augments reporting acceptance; it is not a BPR-0967 repair. The owner's
subsequent useful-test/allocator-neutral-gate and supported-option mandate
continues as a separate evidence-backed investigation, preserving these limits
and failures. Native signed-installed/Cocoa/package acceptance stays pending.
