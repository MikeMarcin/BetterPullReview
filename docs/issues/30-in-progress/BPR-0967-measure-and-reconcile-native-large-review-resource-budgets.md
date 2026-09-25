---
schema: "implement-rfc.card.v1"
id: "BPR-0967"
title: "Measure and reconcile native large-review resource budgets"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0967-mac-responsiveness","path":"docs/rfcs/plans/bpr0967-mac-responsiveness/milestone-plan.json"}}
---

# Measure and reconcile native large-review resource budgets

## Transferred obligation

Original RFC0013 performance-budget paragraph and acceptance rows 2–3, including full active-work responsiveness and sampled/peak memory on declared target hardware.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

The historical macOS Qt 6.10.3 courses passed provisional 100 ms input and 160/96 MiB sampled growth/cleanup budgets at their recorded candidates. The r4 closeout at source 3efe61dfcf6874d35400bf777a7e86dbbf0a4693, Debug Qt 6.11.2 arm64, freshly runs the current public benchmark successfully, but both application_large_review_loaded_close and application_large_review_resources fail at the unchanged 167,772,160-byte growth bound before close/prune. The first loaded-close sample is 197,149,656 bytes versus 4,932,232 baseline (192,217,424 bytes growth). The cause is unproved; a changed Qt version is context, not attribution. Later cleanup assertions were not reached. No threshold was changed or failed run called passing. The fixed-path repeat is retained in build/rfc0013-owner-closeout/memory-recheck.log; the RFC ledger records its result.

## Acceptance

The unchanged repeat fails 2/2 in 26.69 seconds. Loaded-close growth is
192,217,376 bytes (197,231,528 minus 5,014,152); resources growth is
191,136,128 bytes (196,133,872 minus 4,997,744). Each cache is one entry at
3,517,688 bytes with no diff/epoch job at that sample, so the complete process
footprint is distinct from the bounded diff cache. No source attribution or
leak diagnosis follows from these counters. The repeat log SHA-256 is
`a6634710efb5129ff8b262a11b7b040b9bdb4dee3dbe387d9bebd5a148f8f3ee`.

The passing ordinary benchmark records a 321.700 ms initial metadata/load
timer gap, 98.723 ms large-source and 32.004 ms rapid-selection maxima. Its
pass is not a blanket 100 ms responsiveness observation for every phase.
The large-review render currently shows Overview while its source-document
assertions are exact; BPR-0940 owns that fixture/presentation reconciliation.

- AC1: Attribute the current Mac growth failure with exact artifact, hardware, toolkit and representative workload evidence; distinguish retained live owners, transient peaks, allocator retention and leak evidence before changing policy or thresholds.
- AC2: Preserve existing cache ceilings, exact source/anchors, eviction/revisit, close/prune ownership and timing assertions. A repair needs a failing-before control and the complete affected public courses; a budget change requires measured justification and an explicit documented amendment.
- AC3: Establish justified target-specific launch, metadata, first-render, navigation, drawers, theme/fallback, active-work input/cancellation and memory/cache budgets on actual eligible Windows/Linux/macOS hardware. Report unsampled peaks and unavailable platforms explicitly.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## macOS attribution and Mac budget amendment, from BPR-0893 — 2026-09-23

[[BPR-0893]] attributed the Mac failure the transfer records. This is AC1
evidence for macOS only. AC2's amendment is recorded there, and AC3 is
untouched:

- **Retained live owners, and a toolkit change.** The growth past 160 MiB at
  `large_file_ready` is Qt 6.11.2 keeping a HarfBuzz buffer in every shaped
  line's layout (qtbase `8209078e0e`): 70.4 MiB live across the 40,000
  measured lines, and none under Qt 6.10.3. The pair `92c4e159^` /
  `92c4e159` isolates it, 121.6–122.6 MiB against 184.2–186.2 MiB.
  [[BPR-1032]] owns the product side.
- **Allocator retention, not a leak.** After cleanup the live heap returns to
  60–68 MiB with the application open and 13–14 MiB once it is destroyed, in
  every run. The footprint stays at 128–178 MiB because the default zone keeps
  freed pages dirty (87% of it free at `application_destroyed`), and
  `malloc_zone_pressure_relief` does not return them.
- **Amendment, with its measured basis.** On macOS the sampled footprint budget
  is 224 MiB (highest observed peak 202.9 MiB), and cleanup is bounded on the
  live heap at 96 MiB with the application open and 32 MiB once it is
  destroyed, in `tests/end_to_end/large_review_benchmark.cpp`. All four
  resource courses pass.

Unsampled peaks, responsiveness budgets, and Windows and Linux budgets on
eligible hardware remain this card's.

## The Mac memory budgets after BPR-1032 — 2026-09-23

[[BPR-1032]] (`27c35729`) removes the per-line HarfBuzz retention that
[[BPR-0893]]'s 224 MiB bound accepted. On the same M4 Pro, Debug build,
`large_file_ready` live growth falls from 164.2 to 101.1 MiB. The Mac bounds
in `tests/end_to_end/large_review_benchmark.cpp` are re-derived from the new
measurement:

- **Footprint, resources / loaded-close / cache-entry courses:** 160 MiB.
  The highest observed peak is 138.7 MiB.
- **Footprint, cache-byte course:** 184 MiB. Its peak is 166.7 MiB, because
  the course fills the diff cache to its byte budget.
- **Live heap after cleanup:** 84 MiB with the application open (observed
  67.6) and 32 MiB once it is destroyed (observed 13.8).

That is AC1/AC2 evidence for macOS only. AC3 is untouched: launch,
metadata, first render, navigation, drawers, theme and cancellation budgets,
and every Windows and Linux measurement, remain this card's.

While measuring, the Mac station found `application_large_review_history`
red. Every epoch refresh holds the GUI thread for about 167 ms against the
100 ms input budget, with or without BPR-1032. That is filed as
[[BPR-1043]], and fixed there (`7b5d78be`): the conversation delegate no
longer reshapes every post on each relayout, and refreshes are back to a
median worst gap of 68.8 ms. The same change lowers the history course's
initial-load gap from 292–309 ms to 231–243 ms. That gap is still over
100 ms, and it remains this card's responsiveness question.

## Mac unsampled footprint high-water gate — 2026-09-25

At code candidate `8b6407fd`, the native Mac resource sampler now records the
process-lifetime physical-footprint peak and checks its growth against the
existing course-specific 160/184 MiB bounds. The four affected composed
courses pass 4/4 with the gate enabled. The resources course reveals a
139.22 MiB lifetime-peak growth where its largest checkpoint was 124.27 MiB;
the cache-byte course peaks at 171.41 MiB against 184 MiB. Exact hardware,
toolkit, report digests and the other two rows are in the
[Mac peak evidence](../../verification/BPR-0967-mac-lifetime-peaks-2026-09-25.md).
The counter bounds unsampled physical-footprint peaks on this Mac, but it does
not time their phases or measure peak live heap. The remaining AC3 platforms
and responsiveness dimensions above are still unobserved; this card stays
Backlog.
