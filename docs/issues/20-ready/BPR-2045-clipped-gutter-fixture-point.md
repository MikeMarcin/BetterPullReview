---
schema: "implement-rfc.card.v1"
id: "BPR-2045"
title: "Keep scaled gutter fixture clicks inside clipped native rows"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"mandate_revision":"release54-clipped-gutter-point-r1","reservation":"docs/issues/_reservations/BPR-2045--BPR-2045--ca9beb1ba418.json","milestone_plan":{"id":"bpr-2045-clipped-gutter-point","path":"docs/rfcs/plans/bpr-2045-clipped-gutter-point/milestone-plan.json"},"source_baseline":"899056e7de7187edc0ee206a7dc499a1f5487af4","allocation_main":"58c42952fd5b66386ad8df5860d83bace8d7728d","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1","excluded":"125paint remains unresolved; no native qualification credit from projections."}
---

# Keep scaled gutter fixture clicks inside clipped native rows

Frozen54 Windows full Release100/200courses fail the native source-line pick.
The one-shot failure-only diagnostic reproduces both: row1 occupies the visible
half-open interval `[18,19)`, but the original `max(heading,top)+1` clicks19.
Native cursor mapping, production hit testing and actual selection agree on
row2; scrolling is unchanged. This is a fixture-input defect, not a product or
Qt attribution. The125paint failure remains separately unresolved.

## Acceptance

- AC1. Preserve the original54failure and numeric100/200negative. Change only
  the owning test input to choose an integer point within the visible half-open
  native row interval, with pure nonempty/inside assertions. Keep every clipped
  row, original exact source-line pick assertion and gesture; add no pre-click
  layout or hit queries and change no production code.
- AC2. Corrected uninstrumented100/200originalcourses pass once each through
  the same contained wrapper, actual row properties, environment, all original
 575named assertions, one-device-pixel paint limit and120s watchdog. Only the
  private diagnostic/candidate EXE and strictly output-only PNG namespace may
  differ in engineering projections. Format22, scoped static/docs/board/diff
  checks pass. Raw failed54results remain failed; this supplies no release
  qualification or125acceptance.
- AC3. Land only the owned release fix and workflow on canonical main with
  actual allocation/plan/claim/source provenance and monotonic NEXT. Preserve
  source899, normal images/objects/cache/PNGs and other agents. Finish owned
  commands and reconcile native leases. Fresh common-source release gates and
  unresolved125paint remain separate.

[Current handoff](../_checkpoints/BPR-2045/handoff.md).
