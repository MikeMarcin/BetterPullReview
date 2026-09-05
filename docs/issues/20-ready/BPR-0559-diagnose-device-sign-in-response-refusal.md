---
id: BPR-0559
title: Diagnose device sign-in response refusal after browser authorization
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# Diagnose device sign-in response refusal after browser authorization

Reproduce and repair the response-handling failure observed in the v007 attended run. Browser authorization completed, while the app reported unusable GitHub data before session establishment. Identify the actual failing contract; do not infer the failure stage from the generic message.

## Acceptance

- A concrete source-backed cause is discriminated by a focused offline fixture or contract test.
- The valid supported response establishes the intended session; malformed and authority-mismatched responses still refuse without weakening token/identity validation.
- Focused adapter/host and one composed-path checks cover the repaired boundary.
- Record uncertainty honestly; no credentials, live retry, or raw token response logging. A changed publication build needs its own exact approval.

Observed source: `2a043335`; [v007 attended result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md).
