---
schema: "implement-rfc.card.v1"
id: "BPR-1425"
title: "Build the operation-owned sensitive CLI credential path"
status: "Ready"
rfcs: ["RFC-BPR-0078"]
depends_on: []
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
---

# BPR-1425: Build the operation-owned sensitive CLI credential path

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Sensitive capture and execution ownership.
- **AC02:** Closed named-account retrieval and typed refusal.
- **AC03:** Queued/process/descendant cleanup and no retained generic credentials.

Sensitive results and environment inputs never enter generic copyable request/output records. Clear owned buffers after queued work and descendants drain, including timeout/reap failure/shutdown. Stock-gh hostname mapping is exact or refuses.
