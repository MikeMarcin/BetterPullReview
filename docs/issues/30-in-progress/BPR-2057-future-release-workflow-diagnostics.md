---
schema: "implement-rfc.card.v1"
id: "BPR-2057"
title: "Reduce release dependency waits and expose structured build diagnostics"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
---

# BPR-2057: Future release workflow and diagnostics

## Scope

Owner request, 2026-11-11: leave the current v0.4.2 release alone, improve future
build time on main on a best-effort basis, and instrument builds for diagnostics
that do not require scraping logs. No performance measurement campaign, current
release intervention, acceptance weakening, signing or publication is authorized.

## Acceptance criteria

- AC1: Compile required local graphs before long suites and let independent
  physical hosts advance without a global compilation wait. Keep every required
  final gate and shared Windows/WSL host exclusion.
- AC2: Expose structured phase identity, source/candidate/platform context,
  timestamps, duration, actual exit, child identity and qualification report
  locations while retaining human output and unresolved failures.
- AC3: Document evidence-bound profiles, failure reconciliation and bounded
  timing diagnosis. Verify focused tests and documentation without a benchmark
  campaign or touching the frozen v0.4.2 candidate.
