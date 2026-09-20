---
schema: "implement-rfc.card.v1"
id: "BPR-0945"
title: "Resolve existing strict tidy diagnostics in shared Qt and Home headers"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P2
metadata: {"origin":"r23 focused integration","scope":"Unchanged shared-header diagnostics, outside BPR-0941..0943"}
---

# Resolve existing strict tidy diagnostics in shared headers

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
