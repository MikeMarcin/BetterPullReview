---
id: BPR-0091
title: Implement host observation assembly and settings persistence
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0090]
---

# Implement host observation assembly and settings persistence

Assemble bounded observations and incremental projections for the workbench, issue settings-persistence requests for drawer widths and layout state, publish changed domains, and handle cancellation and generation identity.

Stale, reordered, and duplicate terminals must not publish. Persistence restores across restart through the existing durable path.
