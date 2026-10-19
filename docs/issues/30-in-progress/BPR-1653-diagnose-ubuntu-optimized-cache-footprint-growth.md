---
schema: "implement-rfc.card.v1"
id: "BPR-1653"
title: "Diagnose Ubuntu optimized cache footprint growth"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"f164d6214664111c27eb4e89a33d68fce293925e","reservation":"docs/issues/_reservations/BPR-1653--BPR-1653--c646fbf2f786.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/end_to_end/large_review_benchmark.cpp","tests/end_to_end/large_review_native_memory.cpp","tests/end_to_end/large_review_observers.hpp","src/app/src/large_review_fixture.hpp","tests/end_to_end/application_review_round_trip_composition_contract.cmake"],"boundary":"Named memory-course diagnosis and subsequently grounded bounded repair; report product-input impact before candidate mutation, no budget/predicate relaxation or platform skip"}
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

- AC4: Preserve the bounded fixture-move experiment, full32-head output equality and altered-field controls, original-header causal comparison and exact restoration. Revert a change that does not establish the memory repair. Any later app-input correction requires a separately reviewed composition reseal/actual contract and explicit frozen/distribution binding; no automatic adoption or limit change.

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

## Bounded diagnostic checkpoint

The exact unchanged binary reproduces the failure in49.9305s, with195
external status/smaps-rollup samples from only its own descendant process.
No swap is observed. Temporary existing-observer integer summaries reproduce
it in47.9282s: near failure glibc holds189018112 arena bytes, of which42189280
are free, alongside11550720 mapped allocation bytes. This shows substantial
allocator retention, not yet a complete causal attribution or permission to
change the limit.

Two approved additional calls to the same resource observer separate setup
from large selection, with the original predicate at both points. Initial
review live heap is64942912 bytes; after31 refreshes it is73522592 (including
4487224 cache bytes) with30152320 arena free. Before first file it is77045376,
then first large selection98407264 with42425488 arena free. The course still
fails in47.4685s. The replay replaces the completed script on refresh rather
than retaining all scripts; it recompiles the1000 synthetic comments for each
head. Further ownership/lifetime attribution remains pending.

Both temporary test sources are restored exactly after each diagnostic and
the original benchmark is rebuilt to identical SHA-256
`67f292c90c8ccb089cc70893360597a11dd6ce2cdb8b5cd55a3b9435f623d777`.
No diagnostic binary supplies acceptance. BPR-1371's existing separate Mac
allocator/assertion proposal remains unapplied and is not adopted here.

## Rejected bounded repair proposal

The pinned Glaze8.3 object initializer-list constructor explicitly copies its
const entries, including moved arrays. The connection and GraphQL envelope
currently nest each100-thread page through several of those lists on every
refresh. The approved experiment moved the same arrays/objects into the same
ordered envelope instead. There is no cache, skipped refresh, changed payload,
baseline shift or allocator tuning. Exact32-head output equivalence and a
changed-field control precede native acceptance; the original header supplies
a causal memory control. All original workload and limits remain.

This header is an app/helper input in tests-on Mac builds, invalidating the
previous frozen-input adoption until fresh rebuild or rigorous reconciliation.
The tests-OFF distribution fixture exclusion and unchanged production mutation
digest require separate evidence. No automatic guard-based adoption applies.

## Rejected fixture-move experiment

Candidate a661b333f preserves all32 heads' complete encoded request/response
output:992 exchanges, including one viewer bootstrap per compiled script plus
30 authenticated exchanges. Altered response status and request path refuse.
The first two probe wrappers incorrectly expected30 total entries and stopped
before cache execution; those refusals remain retained. The corrected probe
checks all31 entries per head and21882326 response-body bytes.

The actual unchanged-budget candidate still fails in49.7115s at selection19,
with233431040 footprint-growth bytes. Its original-header causal control fails
in50.4109s at selection17, with226967552 bytes. Matched live heap is essentially
identical, and candidate-minus-original RSS changes sign across phases. These
measurements do not support the move assembly as the memory repair.

Both temporary probe/control sources are restored and the candidate binary
rebuilt to identical d2a6cbe7d03909d486b3ece03edcfe7fd27f1cd24e9ba425c7311ce20fd7d156
before the experimental header is intentionally reverted. All evidence remains
under ignored linux-final-evidence/v030-linux-cache-move-candidate-r8-results.
No composition pin is changed for this rejected experiment. The original
header SHA256 cd5b441f57144b21f61fe1b9544d54754d07fd90c5dfb3587d5289d8f0fef7a7
is restored; frozen input eligibility requires fresh current comparison and
coordinator adoption rather than assuming the intermediate invalidation gone.
Further live/presentation-versus-allocator diagnosis remains open under the
original216MiB/900s predicates.

## Final bounded diagnosis and producer handoff

R9's unchanged-binary aggregate map observer reproduces the failure in48.9628s.
Application/Qt/system/font file pages plateau while anonymous residency grows;
independent category maxima are not added. Scratch-only Heaptrack R10 identifies
temporary preparation line splitting, C++ refinement and Qt publication
allocations, but stops at the first large selection because observation
overhead trips the original predicate. QtSvg has no allocation-stack events;
xdiff has no retained allocation at the trace's global peak or end. Aggregate
traffic and cohort windows do not establish individual allocation lifetimes or
a causal allocator-threshold change. The independent bounded review finds no
proven new leak, metric defect or accepted repair.

The temporary R12 lifecycle observer latches the two unchanged footprint
predicates, completes all original selections/eviction/revisit/cleanup checks,
suppresses acceptance and always exits failure. Eight footprint misses remain;
all other completed checks hold. Live heap falls from159909840 bytes before
close to2371136 after destruction, with cache/jobs zero, while RSS remains
206852096. Released live ownership therefore does not explain away resident
pages or supply a passing gate; allocator free bytes are not resident bytes
that may be subtracted from the measurement.

Every bounded causal control retains the original numeric predicates and
fails: R11 equivalent Dif conversion/move at selection18, R14 preparation
disabled at18, R15 C++ refinement bypassed at20, and R16 the existing direct
publication branch at18. Their matched original controls fail too. The preparation control omits
preparation work, the refinement control omits C++ refinement, and the direct
branch bypasses staged scheduling. None can qualify the original contract. Direct publication
and its original control differ by only565248 resident bytes at selection18,
with effectively equal live/cache bytes. No permanent feature removal,
staging-threshold increase or speculative storage repair is adopted.

The faithful e257b236a historical reconstruction uses the current station,
original declared pins and unmodified sources. Its original course passes
in32.396s, explicitly with BudgetsEnforced false. Its sampled live peak is
similar to R12, while resident high-water is lower. Source/static/runtime-image
and font inputs differ, so this is a bounded comparison rather than acceptance
or a station-only explanation. R17's first observer wrapper refuses before
launch because it incorrectly expects SKIP_RETURN_CODE; the old registration
has TIMEOUT900/RUN_SERIAL true and no skip property. The corrected R17-r2 runs
that unchanged historical binary in30.9226s and collects119 samples, preserving
the absent property. Its actual maximum concurrent map sample totals229085184
bytes (application21483520, anonymous167112704); R9's totals262025216
(application29863936, anonymous191332352). These points have different
phases/timings and are not a phase-matched causal comparison. Both image and
anonymous residency contribute to the observed difference; no single owner is
isolated.

All temporary benchmark, observer, syntax and Qt sources are restored exactly,
and the original benchmark is rebuilt to SHA-256
`67f292c90c8ccb089cc70893360597a11dd6ce2cdb8b5cd55a3b9435f623d777`.
Complete reports, original refusals, raw failed samples, causal controls,
archives and exact task-retirement receipts remain in the ignored release
session. R17-r2's archive SHA-256 is
`7d072893165f296772ff3bd0e6f1370393f54930014e24a225ef1518c027d07e`.
No diagnostic binary supplies acceptance; no production input or limit change
is retained.

The coordinator authorizes a reviewable producer source freeze with this
strict failure still open. Causal attribution and the justified correction/
original-course pass remain unsatisfied; the rejected experiment and exact
restoration are retained. Further native experiments require a new
coordinator lease and a named, phase-matched ownership/residency question.
Release candidate production is not card or milestone acceptance. Any later
source correction explicitly invalidates and reproduces affected candidates.
