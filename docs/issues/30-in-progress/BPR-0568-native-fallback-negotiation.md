---
schema: "implement-rfc.card.v1"
id: "BPR-0568"
title: "Implement native-first off-hunk negotiation and fallback"
status: "In Progress"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0566","BPR-0567"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Implement native-first off-hunk negotiation and fallback

Extend the existing review mutation coordinator/provider session with one serialized logical-publication state machine.

## Acceptance

- Legacy lines remain native regardless of off-hunk capability evidence.
- Proven supported off-hunk lines publish natively.
- Unknown support uses the user-approved native write as negotiation.
- Native negotiation durably records exact unsupported rejection before fallback
  is prepared. A first attempt with proven off-hunk placement and fresh scoped
  unsupported evidence uses the separate durable evidence-admission path.
- All ambiguous/native-possibly-created outcomes perform recovery without fallback.
- Existing double target observation, stale-head policy, acting identity, transport lease, cancellation, one-use production carrier, and server refresh remain intact.

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

Cover both first-attempt unsupported-evidence admission and exact native-rejection
negotiation. Persist the authorization before a fallback child exists; never
fabricate a rejected native child or authorize fallback from a lost response.
