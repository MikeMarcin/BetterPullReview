---
schema: "implement-rfc.card.v1"
id: "BPR-0649"
title: "Discuss mode: thread list, map and feed"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---

# Discuss mode: thread list, map and feed

RFC-BPR-0028 mockup state 3. The Discuss modality widens the conversation
over the code: a thread list with file-and-line roots, replies under a guide
line, unread dots, counts, and Whole PR, This file, Unread and Unresolved
filters; the accepted thread map (`github_thread_map`) for the selected
thread with unread nodes filled; a feed with one card per thread carrying the
code excerpt on the product theme, the root and replies on a continuous rail,
per-post headers (author, role, replying to, unread dot, time) and Reply,
Resolve, Mark read and Open in code.

## Acceptance

- RFC-BPR-0028 AC-05 for Discuss and AC-08.
- Open in code returns to Read at the thread's line.
- Focused tests for the list model, map selection and feed delegate.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.
