---
schema: "implement-rfc.card.v1"
id: "BPR-0945"
title: "Resolve existing strict tidy diagnostics in shared Qt and Home headers"
status: "Ready"
rfcs: []
depends_on: []
priority: P2
metadata: {"origin":"r23 focused integration","scope":"Unchanged shared-header diagnostics, outside BPR-0941..0943","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency","milestone_plan":{"id":"bpr0945-tidy-headers","path":"docs/rfcs/plans/bpr0945-tidy-headers/milestone-plan.json"}}
---

# Resolve existing strict tidy diagnostics in shared headers

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Canonical strict tidy header diagnostics; no broad suppression or passing tidy result is inferred.
No owner decision, native observation or repair is supplied by this transfer.

The canonical tidy helper, with the proper macOS SDK and warnings promoted to
errors, reports two shared-header diagnostics while checking the r23 touched
translation units: `portability-avoid-pragma-once` at `dif_runtime.hpp:1`, and
`bugprone-signed-bitwise` at `home_view_state.hpp:35`. Both files are byte-unchanged
from entry revision d47a6e18. No changed-line diagnostic was reported. This is
recorded debt, not a passing strict tidy result or authority to broaden the
selected-diff repair.

## Acceptance

- AC1: Reconcile both diagnostics with the coding standard without broad rule
  suppression or behavior changes.
- AC2: Run the canonical helper with warnings as errors on the affected Qt
  workbench and application translation units, with the proper host SDK.
- AC3: Re-run focused Home/Qt compilation and tests appropriate to any change.
