---
schema: "implement-rfc.card.v1"
id: "BPR-1653"
title: "Diagnose Ubuntu optimized cache footprint growth"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"f164d6214664111c27eb4e89a33d68fce293925e","reservation":"docs/issues/_reservations/BPR-1653--BPR-1653--c646fbf2f786.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/end_to_end/large_review_benchmark.cpp","tests/end_to_end/large_review_native_memory.cpp","tests/end_to_end/large_review_observers.hpp"],"boundary":"Named memory-course diagnosis and subsequently grounded bounded repair; report product-input impact before candidate mutation, no budget/predicate relaxation or platform skip"}
---

# BPR-1653: Diagnose Ubuntu optimized cache footprint growth

The first actual Ubuntu tests-on Release cache-byte course at f164 fails in
49.6288 seconds, with sampled fixture footprint growth above its platform
budget. The original Debug configured Skip remains separate. A prior wrapper
failed before execution on an incorrect timeout assumption; the optimized
benchmark build itself passed in 350.403 seconds.

## Acceptance

- AC1: Preserve the exact original sample series, before-application baseline, lifetime high-water, unchanged 216 MiB Linux limit/formula, registered 900-second serial course, Release flags and process provenance. Ground the cause in actual fixture, retained/cache allocation or environmental observations; another platform passing is not a diagnosis.
- AC2: Implement only a justified bounded correction after reporting exact product/frozen-input impact to the coordinator. Preserve original footprint/live-heap/cache/eviction/cleanup/input predicates, platform limits, course configuration and deadlines. No platform skip, allocator tuning or measurement bypass supplies acceptance.
- AC3: Run the actual optimized native course on the exact candidate, retain the original failure and meaningful causal counterexample evidence, and complete appropriate focused verification. Any temporary diagnostics are restored and original binary rebuilt before acceptance; final source freeze follows all card/ledger upkeep.

## Preserved first failure

The original registered row uses TIMEOUT900, RUN_SERIAL true,
SKIP_RETURN_CODE77, the existing offscreen Windows-style environment and
Linux network-namespace launcher. At cache_byte_selection_18, resident/native
footprint is 259219456 bytes over a 29618176-byte baseline: growth229601280,
versus limit226492416. Live heap growth is156655760; cache accounting is
63371216 bytes including prepared entries, all jobs zero. The full 21-sample
series and JSON/JUnit/logs are retained under ignored release-session evidence.
No timeout or input-budget miss occurred. Both wrapper tasks are retired and
the shared producer is quiescent before the next bounded diagnostic.
