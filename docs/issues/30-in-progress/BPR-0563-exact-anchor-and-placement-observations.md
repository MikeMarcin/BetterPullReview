---
schema: "implement-rfc.card.v1"
id: "BPR-0563"
title: "Bind exact off-hunk anchor eligibility and placement observations"
status: "In Progress"
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

- Source review: baseline `9ee0350f` through this BPR-0563 product slice.
  The only pinned application edits capture exact material facts from the diff
  host, read accepted catalogue membership, validate the current epoch/PR/actor,
  and supply that observation port to both production and fixture hosts. No
  grant, provider mutation, or transport admission policy changes. The full-app,
  production-manifest and mutation-construction tripwires are resealed for this
  reviewed range. This is implementer self-review, not independent acceptance.
- Host cases exercise missing catalogue membership, snapshot/identity/key/blob/
  head/path/actor mismatch, absent material, zero/out-of-range lines, invalid
  placement and admitted unknown placement. Existing right-root, left-reply,
  one-shot authority and async cancellation cases remain required.
- Production has no complete provider patch-domain observation yet: placement
  is explicitly `unknown`, bound to the accepted snapshot, target, actor and
  exact file comparison. It remains eligible for native negotiation.
- Card's historical left-side refusal applies to new roots. Existing BPR-0657
  left-side replies retain their canonical thread identity and remain supported.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
