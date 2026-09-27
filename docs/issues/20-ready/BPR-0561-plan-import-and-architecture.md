---
schema: "implement-rfc.card.v1"
id: "BPR-0561"
title: "Plan and import RFC-BPR-0022 semantic source-line comments"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: []
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Plan and import RFC-BPR-0022 semantic source-line comments

Reconcile the pack with the live BetterPullReview tree, allocate all live issue IDs, add the RFC only to the post-pack index, create the live verification ledger, and record the architecture allocation and RFC base SHA.

## Acceptance

- The retained pack and `LIVE_IMPORT` record bind the exact imported bytes and ID mapping.
- RFC-BPR-0008 is not edited to absorb the new requirements.
- All fifteen canonical acceptance criteria have one live verification row.
- Source paths, current schema version, CTest selectors, and existing open dependencies are re-audited from live state.
- `python3 scripts/verify_documentation.py --root .` and `git diff --check` pass.

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

The pack retention, identity allocation, RFC/index import, and initial source
reconciliation are complete in the import slice. Detailed implementation-readiness
planning and evidence selection remain open; this card is not claimed Done.
