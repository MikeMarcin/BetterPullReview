---
schema: "implement-rfc.card.v1"
id: "BPR-1767"
title: "Investigate the Ubuntu large-review memory failures waived for v0.3.3"
status: "Backlog"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"origin":"Owner's v0.3.3-only memory-budget waiver and after-release follow-up request","reservation":"docs/issues/_reservations/BPR-1767--BPR-1767--a33c90c65f7d.json","related_cards":["BPR-1694","BPR-0967","BPR-1371","BPR-1764"],"published_source":"193ed10fe24f36c3c6c6498e367afd7ac88c6dca","observed_memory_candidate":"e7514e7d7b3239e7e051e160bb79fe2e308d25f2","external_evidence":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032"}
---

# Investigate the Ubuntu large-review memory failures waived for v0.3.3

## Owner decision and boundary

The owner directed: "waive the memory budget failures and release this thing
already". The release coordinator recorded that decision at
`2026-10-25T02:21:25.918293+00:00` in the external
`owner-waiver-v033-memory-budget.json`. It applies only to v0.3.3 and the
known `application_large_review_resources` and
`application_large_review_loaded_close` memory-budget failures. Their actual
FAIL results remain failures with an owner exception; they are not passes.
The coordinator stopped further memory diagnosis and reruns for that release.
This after-release card authorizes no test run or source change by itself.

The published source is `193ed10fe24f36c3c6c6498e367afd7ac88c6dca`.
This documentation follow-up must not change its tag, source, assets or release
state. Functional, rendering, source, package, security and native-trust
obligations were separate from the memory waiver. The Debug timeout exceptions
are separately tracked in [[BPR-1765]] and [[BPR-1766]]. No waiver is renewed
for another release or another failure.

## Existing work and duplicate check

[[BPR-1371]] concerns the different macOS byte-pressure course and its 184 MiB
footprint behavior. [[BPR-0967]] records historical native resource-budget
reconciliation and [[BPR-1764]] owns RFC-BPR-0093 allocator acceptance and its
current retention investigation. Neither is an accepted repair of these exact
Ubuntu v0.3.3 resource and loaded-close failures. Reconcile and reuse their
relevant evidence when this card is picked up; do not duplicate allocator work
or import a measurement change as a resource fix.

## Retained observations

The selected environment was Ubuntu 26.04 under WSL2/WSLg, native optimized
Release, Qt 6.12.0. The original rows and their source, binary/runtime,
fixture, registered command and sampling identities must remain bound in the
external receipts. Instrumented or profiler runs are diagnostic evidence and
cannot replace the original uninstrumented resource assertions.

- R193 and R195 retain the original selected-source failures. R201's bounded
  investigation records both rows failing at `large_file_ready`: RSS growth
  179,855,360 B and 179,814,400 B respectively, against 176,160,768 B. Neither
  had reached its close lifecycle, so later reclamation could not repair that
  observed peak.
- The reviewed QImage ownership corrections reduced demonstrated copy/storage
  costs. R216 retains an expected failing RED assertion and passing GREEN
  owning/rendering controls, while both original resource rows still fail at
  later boundaries. These controls are not memory-budget acceptance.
- R217, candidate `e7514e7d7b3239e7e051e160bb79fe2e308d25f2`, again retains
  the expected failing RED control and passing GREEN `qt_dif_runtime` and
  `qt_dif_workshop_placement` courses. The original loaded-close row fails on
  sampled footprint growth at `before_loaded_close` (176,459,776 B; lifetime
  peak growth 180,846,592 B). The resources row reaches `after_prune` but fails
  the lifetime-peak footprint assertion (180,338,688 B). The configured
  growth bound remains 176,160,768 B. Reaching a later phase, releasing a
  cache, or passing pixel controls does not turn either row into a pass.

Retained evidence under the external root includes `ubuntu-v033-r193`,
`r201-memory-causal-investigation.md`, `inspect-r217-memory.stdout.log`, the
owner-waiver JSON and the original native directories
`/home/mmarcin/bpr-release-v033-r195-gates`,
`/home/mmarcin/bpr-release-v033-r216-memory` and
`/home/mmarcin/bpr-release-v033-r217-memory`. Preserve their failures,
source bindings, raw outputs and partial measurements before any new work.

## Investigation and acceptance

- AC1: Reconcile the existing resource, rendering and allocator investigations
  and bind a bounded investigation to the exact current source, native
  binary/runtime, allocator, fixture, sampling and execution properties.
  Distinguish live allocations, allocator-retained pages, process RSS and
  lifetime peak with direct causal evidence; do not infer success from a
  cleanup sample, an instrumented pass or another platform's result.
- AC2: Implement a scoped causal correction with a meaningful before/after
  counterexample or allocation/work-count control. Preserve source text,
  decoded and rendered pixel identity, visible behavior, lifetime ownership,
  fixture workload and every original growth/cleanup/peak assertion.
  Forced allocator collection, delayed samples, warmed baselines, moving
  workload outside the interval, higher thresholds or dropped assertions are
  not acceptance. Any proposed criterion change requires a separate explicit
  owner decision before application.
- AC3: Run both original Ubuntu Release rows on the exact corrected candidate
  with their unchanged registered commands, bounds, sampling phases,
  containment, resource locks and scheduling. Complete affected native
  prerequisites first and select the relevant functional/rendering and
  integration checks. Preserve valid unchanged evidence; do not restart every
  full suite or retry until a pass. Record every actual failure and skip.
- AC4: Review the final source and any affected input seals against the exact
  correction, remove temporary diagnostic-only mechanisms when no longer
  needed, and pass the documentation gate. Keep this card open until the
  direct resource criteria pass or a separately recorded owner amendment
  changes them. The v0.3.3 waiver supplies no follow-up acceptance.
