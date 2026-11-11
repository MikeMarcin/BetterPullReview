---
schema: "implement-rfc.card.v1"
id: "BPR-2100"
title: "Discover native release regressions before long qualification"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057", "BPR-2058"]
priority: "P1"
---

# Risk-focused native release preflight

## Scope

Owner direction, 2026-11-11: improve future releases on main, best effort without
a lengthy benchmark campaign. Leave the current frozen v0.4.2 release alone.
Discover cross-platform failures earlier with short Release and Debug checks,
including the Linux Git cache ownership regression. Prefer independent Linux
where available, otherwise discover Linux failures before extended Windows work.
Investigate evidence reuse without assuming unchanged validity.

## Acceptance

- AC1: An explicit bounded risk preflight runs the original Git cache ownership
  and profile reset courses plus representative core/host/composed/storage and
  containment checks. Missing required rows and narrowed selections refuse.
  Preserve native scheduling, reports, failures and Debug policy.
- AC2: Future Mac release entrypoint completes Release and Debug preflights before
  long suites and stops downstream work after failure. Windows/Linux instructions
  schedule early Linux discovery and preserve physical-host exclusion.
- AC3: Final qualification remains input-bound and fresh; no cross-run preflight
  or old-source pass reuse is enabled. Focused runner/entrypoint and documentation
  tests pass without full product builds or current-release interference.
