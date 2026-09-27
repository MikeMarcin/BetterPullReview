---
schema: "implement-rfc.card.v1"
id: "BPR-0566"
title: "Add repository-scoped off-hunk write capability evidence"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0565"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"complete-scope","outcome":"pass","revision":"df611ce678acade351aabcb557e45155bd477dbd","evidence":{"kind":"command","detail":"application_review_publication_capability_cache passes: each key dimension, conclusive evidence pairing, expiry/backward clock, capacity and lifetime bounds. Production ownership is BPR-0568; read/schema cache unchanged."}},{"criterion":"conclusive-evidence","outcome":"pass","revision":"df611ce678acade351aabcb557e45155bd477dbd","evidence":{"kind":"command","detail":"application_review_publication_capability_cache passes: each key dimension, conclusive evidence pairing, expiry/backward clock, capacity and lifetime bounds. Production ownership is BPR-0568; read/schema cache unchanged."}},{"criterion":"no-inference","outcome":"pass","revision":"df611ce678acade351aabcb557e45155bd477dbd","evidence":{"kind":"command","detail":"application_review_publication_capability_cache passes: each key dimension, conclusive evidence pairing, expiry/backward clock, capacity and lifetime bounds. Production ownership is BPR-0568; read/schema cache unchanged."}},{"criterion":"expiry-unknown","outcome":"pass","revision":"df611ce678acade351aabcb557e45155bd477dbd","evidence":{"kind":"command","detail":"application_review_publication_capability_cache passes: each key dimension, conclusive evidence pairing, expiry/backward clock, capacity and lifetime bounds. Production ownership is BPR-0568; read/schema cache unchanged."}},{"criterion":"read-cache-separate","outcome":"pass","revision":"df611ce678acade351aabcb557e45155bd477dbd","evidence":{"kind":"command","detail":"application_review_publication_capability_cache passes: each key dimension, conclusive evidence pairing, expiry/backward clock, capacity and lifetime bounds. Production ownership is BPR-0568; read/schema cache unchanged."}}]
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

## Implementation evidence

Candidate `df611ce678acade351aabcb557e45155bd477dbd`: `application_review_publication_capability_cache` passes.
The cache is separate, bounded to 128 entries and 300-second lifetime, and
refuses evidence from every changed scope dimension. Production session ownership
is exercised by BPR-0568. Initial strong-ID accessor compile errors were corrected.
[Acceptance](../_checkpoints/BPR-0575/bpr0566-acceptance.json).
