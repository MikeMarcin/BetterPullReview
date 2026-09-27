---
schema: "implement-rfc.card.v1"
id: "BPR-0570"
title: "Present one representation-blind comment-anywhere UX"
status: "In Progress"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0563","BPR-0569"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Present one representation-blind comment-anywhere UX

Expose the ordinary review-comment gesture on every eligible current-side source line in a changed text file and keep provider representation out of the normal workflow.

## Acceptance

- Hunk and off-hunk lines use the same affordance, editor, Publish action, pending state, and resulting selected conversation.
- Qt receives command projections and typed outcomes only; it owns no capability decision, permalink builder, or provider call.
- Representation appears only in an expandable support diagnostic.
- Line-only incompatibility is explained with zero POST.
- Keyboard, focus, accessibility, high-DPI, light/dark/high-contrast, and bounded incremental-update evidence passes through the shipped workbench.

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
