---
id: BPR-0560
title: Make GitHub connection diagnostics selectable and copyable
status: In Progress
rfc: RFC-BPR-0002
depends_on: []
---

# Make GitHub connection diagnostics selectable and copyable

The owner could only share the v007 failure via a screenshot because the connection diagnostic was not easily selectable/copyable. Make the status and diagnostic text selectable with the mouse and keyboard through ordinary Qt controls, preserving plain-text rendering and safe diagnostic content. Expose the current text to accessibility rather than only a generic label.

## Acceptance

- The displayed GitHub status and diagnostic can be selected and copied using normal platform gestures.
- Accessible text includes the actual message and updates/clears with projection changes.
- Verify the real widget interaction and focused Qt session tests; no host/provider effects or credential copying are introduced.

Observed source: `2a043335`; [v007 attended result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md).
