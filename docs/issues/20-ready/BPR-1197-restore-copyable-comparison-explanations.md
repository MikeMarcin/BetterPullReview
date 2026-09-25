---
schema: "implement-rfc.card.v1"
id: "BPR-1197"
title: "Restore copyable comparison explanations beside document tabs"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1132"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"deterministic macOS integration failures at f6de40b6, 2026-09-25","reservation":"docs/issues/_reservations/BPR-1197--BPR-1197--cdb13ac29612.json"}
---

# BPR-1197: Restore copyable comparison explanations beside document tabs

## Outcome

An unavailable comparison presents its complete explanation visibly, with
wrapping, selection, Copy and accessibility. The compact document-tab status
keeps its full tooltip and accessible description when its painted line elides.
Successful text comparisons retain the compact header and their reading area.

## Acceptance

- The real missing-endpoint and comparison-limit public paths clear stale source
  and present the complete visible explanation, with an actual Copy gesture and
  full accessible text. A supported file recovers normally.
- The tab status preserves the complete tooltip and accessible description,
  allows legitimate single-line elision, and keeps the 28-pixel strip at narrow
  and wide windows and 100, 125 and 200 percent scale.
- Successful text comparisons retain their prior layout budget; focused Qt and
  composed checks, formatting, tidy and the documentation gate pass.

## Boundaries

This follows terminal [[BPR-1132]] without reopening it. It owns only Qt status
presentation and affected tests. No application request/cache semantics change.
Work is isolated from the frozen m2 attended candidate and primary checkout.
Windows/Linux execution and native accessibility acceptance remain owed on
[[BPR-1135]]; macOS automated evidence does not supply those decisions.

## Starting evidence

The full macOS suite at `f6de40b633ea8b879d3643ad5a259169fd976975`
and serial reruns fail `application_unavailable_diff_public_path` at Copy support
and `application_selected_file_safety_public_path` at the hidden label's height.
The replacement tab status retains full text for its tooltip and accessibility,
but the old wrapping selectable label is hidden unconditionally. Inspect a
fresh missing-material render and retain fail-before/pass-after evidence.
