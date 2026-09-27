---
schema: "implement-rfc.card.v1"
id: "BPR-0566"
title: "Add repository-scoped off-hunk write capability evidence"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0565"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
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

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
