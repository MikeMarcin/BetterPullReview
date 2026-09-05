---
provisional_id: PACK-BPR-0022-06
live_id: allocate-on-import
title: Add repository-scoped off-hunk write capability evidence
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-05]
---

# Add repository-scoped off-hunk write capability evidence

Add a narrow bounded tri-state capability cache for the exact off-hunk line-creation operation.

## Acceptance

- Keys bind profile/revision, host, server contract, actor, repository database/node identity, operation/API revision, classifier revision, and freshness generation.
- Only exact recorded contracts, native creation success, or definitive unsupported rejection change state.
- Another repository, readable comment, host name, broad GHES version, schema field, or status code cannot supply evidence.
- Expired observed evidence becomes unknown, not unsupported.
- The design does not unnecessarily make unrelated read/schema capabilities repository-scoped.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
