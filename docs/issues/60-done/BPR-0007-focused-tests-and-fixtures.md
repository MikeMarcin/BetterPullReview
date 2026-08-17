---
id: BPR-0007
title: Complete RFC-BPR-0001 focused tests and fixtures
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0006]
completed: 2026-07-29
---

# Complete RFC-BPR-0001 focused tests and fixtures

Add replay, authority, manual-executor, cancellation, stale-completion,
dependency, action-projection, composition, and hostile-boundary fixtures.
Prove identical replay outputs and a Qt-free core-only build.

## Result

- Added typed foundation failure events, deterministic core recovery, serialized
  host delivery, and presentation-owned failure wording.
- Expanded deterministic replay across 256 shared/private revision pairs.
- Added a dedicated product-contract test layer for authority separation and
  semantic request vocabulary.
- Added sanitized fixture-root policy and a foundation authority replay fixture.
- Added automated source/build boundary checks for Qt/provider leakage,
  widget/effect bypasses, browser runtimes, listeners, telemetry, and updaters.
- Preserved the existing reordering, cancellation, stale-result, QAction,
  accessibility, incremental-projection, and public-path coverage.

## Evidence

The fresh Qt-enabled preset passes seven tests:
`core_dependency_boundary`, `architecture_boundaries`, `core_foundation`,
`application_host`, `product_contract`, `qt_foundation_window`, and
`application_public_path`.

The fresh `core-only` preset passes the five applicable non-Qt tests. Direct
invocations of both boundary validators also pass.
