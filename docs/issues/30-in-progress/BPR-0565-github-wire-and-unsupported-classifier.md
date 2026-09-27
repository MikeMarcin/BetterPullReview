---
schema: "implement-rfc.card.v1"
id: "BPR-0565"
title: "Add native and file-subject GitHub wire contracts"
status: "In Progress"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0563","BPR-0564"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Add native and file-subject GitHub wire contracts

Extend the GitHub adapter with explicit native-line and file-comment-with-permalink request/response variants and an exact revisioned unsupported-response classifier.

## Acceptance

- Native request bytes preserve the current line/side contract.
- Fallback request contains body, commit_id, path, and subject_type=file and omits all line/range side fields.
- Response parsing retains honest remote subject and verifies comment ID, path, commit, body, and acting identity.
- Exact supported and unsupported fixtures are keyed by server/API/operation/classifier revision.
- Generic 422, spam/rate/permission, malformed, unknown, timeout, cancellation, and 5xx counterexamples do not classify as unsupported.

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
