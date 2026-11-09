---
schema: "implement-rfc.card.v1"
id: "BPR-1965"
title: "Saving agent history preserves live thread reply context"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "bug", "reservation": "docs/issues/_reservations/BPR-1965--BPR-1966--df06dad6d452.json", "milestone_plan": {"id": "bpr-1965-1966-agent-contracts", "path": "docs/rfcs/plans/bpr-1965-1966-agent-contracts/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"bcc3a11883d0bed1bce4bfee10b0d45a6a87bc6e","evidence":{"kind":"observation","detail":"Boolean causal trace reproduces original save-status About/DraftTarget loss; typed content/status projection preserves live reply context and clear/history semantics. Raw diagnostic exit8 and restored-source seals retained."}},{"criterion":"AC2","outcome":"pass","revision":"bcc3a11883d0bed1bce4bfee10b0d45a6a87bc6e","evidence":{"kind":"observation","detail":"Final original registered host/chat-composition/conversation course passes; original reply/focus/no-mutation/budgets unchanged. Restoring generic ack publish makes new host assertion fail; corrected source restored and passes."}},{"criterion":"AC3","outcome":"pass","revision":"bcc3a11883d0bed1bce4bfee10b0d45a6a87bc6e","evidence":{"kind":"observation","detail":"Reviewed142manifest/197inventory unchanged; digest568258e616... independently agrees. Source/adversarial/format/docs/board pass; tidy raw1 is approved byte-identical historical784 finding, no changed-line findings. Normalmain bcc3 pushed/remoteexact. Fullnative/stable qualification remains coordinator-owned."}}]
---

# BPR-1965: Saving agent history preserves live thread reply context

## Acceptance

- AC1. Confirm causal metadata loss; storage-status acknowledgments never replace live turn About/DraftTarget; real content restoration and clear remain.
- AC2. Existing host and original composed conversation/reply/no-mutation oracles pass; meaningful negative control, exact source and budget preservation.
- AC3. Reviewed app binding where affected, focused format/tidy/docs/board and intentional latest-main integration pass; raw44failures retained and full stable/native qualification remains coordinator-owned.

## Mandate

Actual frozen44 focus75/77 passes. Coordinator approved this bounded slice and owning product expansion for1965 only. Do not mutate release R or waive first-party failures as Qt.

## Verification and handoff

The scoped source, causal/negative evidence, final4/4 composed checks and remaining native release obligations are recorded in [the engineering handoff](../_checkpoints/bpr44-agent-contracts/handoff.md). External packet: candidate44/implementer-agent-contract-fixes. No release approval or Windows acceptance is inferred.
