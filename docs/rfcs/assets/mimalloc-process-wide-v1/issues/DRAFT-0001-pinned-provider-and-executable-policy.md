---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "Pin mimalloc and select it for BPR product executables"
status: "Backlog"
rfcs: ["RFC-DRAFT-0028"]
depends_on: []
---

# Pin mimalloc and select it for BPR product executables

## Scope and acceptance

Implement the RFC's pinned source/provider and executable policy (AC01–AC02).
The selected product graph includes desktop, optional `bpr`, MCP helper,
supervisor and macOS update helper according to their existing platform and
family switches. Keep mimalloc out of
core and other layer contracts, and keep a core-only graph allocator-free.
Provide the non-distribution comparison switch,
make distribution refuse that switch and preserve offline/substitution controls.
Record the exact build graph and negative configure observations.
