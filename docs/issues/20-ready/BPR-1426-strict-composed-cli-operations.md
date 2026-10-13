---
schema: "implement-rfc.card.v1"
id: "BPR-1426"
title: "Bind every composed CLI operation to one transient credential"
status: "Ready"
rfcs: ["RFC-BPR-0078"]
depends_on: ["BPR-1425"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
---

# BPR-1426: Bind every composed CLI operation to one transient credential

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Connection/remembered validation, pager and discovery operations.
- **AC02:** Review/inline/tour publication scopes and recovery.
- **AC03:** Exact blob batch/fallback and clone/helper scopes, no local Git token.

Cover token-free automatic failed Connect/startup recovery, paginated reads and personal discovery, same-credential post-witness GET reconciliation, exact blobs/batches/fallbacks and actual helper inheritance. Subsequent operations reacquire; session/profile transport handles retain no token.
