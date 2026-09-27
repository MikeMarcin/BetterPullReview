---
schema: "implement-rfc.card.v1"
id: "BPR-0563"
title: "Bind exact off-hunk anchor eligibility and placement observations"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0561","BPR-0562"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Bind exact off-hunk anchor eligibility and placement observations

Extend the application-host binding so publication proves changed-file membership, exact current/right head, path/blob/line validity, and bounded provider placement evidence.

## Acceptance

- A file absent from the PR changed-file set is refused before provider work.
- Left, historical, absent, stale, out-of-range, or mismatched anchors are refused.
- Placement is legacy/off-hunk/unknown with explicit evidence identity.
- Qt hunk geometry, visible expansion state, and y-coordinates are never treated as provider authority.
- Missing or incomplete exact provider placement data remains unknown and can still negotiate through the real write.

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
