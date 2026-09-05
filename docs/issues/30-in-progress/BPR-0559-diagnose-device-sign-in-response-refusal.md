---
id: BPR-0559
title: Diagnose device sign-in response refusal after browser authorization
status: In Progress
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

## Current source-backed finding

Read-only scouting traces the captured title to GitHub `session_failed` and
the body to `malformed_response`. Token-exchange parsing alone does not set
that session state. Account identity, capability discovery, or later session
acceptance can refuse with the same message; no safe live response metadata
was retained to discriminate them. Nullable user display name and missing
OAuth scopes are already handled correctly. A capability GraphQL error is a
hypothesis, not an established cause.

Next bounded work adds safe typed bootstrap-stage context to the existing
host diagnostic projection and copyable UI, with focused host/composed tests.
No raw response, credential, remote error text, or new network operation is
permitted. This improves the next observation; it does not itself prove the
live response failure repaired. Root-cause acceptance remains open until the
actual failing contract is known.
