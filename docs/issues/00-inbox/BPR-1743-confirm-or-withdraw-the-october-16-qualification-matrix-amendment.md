---
schema: "implement-rfc.card.v1"
id: "BPR-1743"
title: "Confirm or withdraw the 2026-10-16 qualification-matrix amendment"
status: "Inbox"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1742--BPR-1743--04a7cd1de032.json","found_by":"RFC-0038 security-audit port, 2026-10-22","found_at":"4e318b625","related_cards":["BPR-0960","BPR-0961","BPR-0962","BPR-0973"]}
---

# BPR-1743: Confirm or withdraw the 2026-10-16 qualification-matrix amendment

## Finding

The [owner matrix record](../_checkpoints/RFC-BPR-0038/owner-matrix-amendment-20261016.json)
says the owner amended RFC-BPR-0038 on 2026-10-16:

- Windows qualification moves to JaegAW Windows 11, and Windows 10 is
  removed.
- Linux qualification moves to JaegAW Ubuntu 26.04 under WSL2/WSLg, and
  Linux claims are limited to that environment.

That amendment stayed on the unmerged branch `codex/rfc0038-security-audit-20261016`.
Main still names Windows 10/11 and a clean Ubuntu desktop in RFC-BPR-0038,
BPR-0960, BPR-0961 and the RFC-0014 plan's m8 exit criteria. README offers
the v0.3.1 MSI for "Windows 10/11", and the package guide lists Windows
10/11 x64. The record does not quote the owner's words. The 0.3.x releases
shipped afterwards without the amendment.

## Acceptance

- AC1: The owner answers one Accept/Reject question in a short attended
  session: does the 2026-10-16 matrix amendment stand?
- AC2: If it stands, RFC-BPR-0038's matrix, BPR-0960, BPR-0961, BPR-0973,
  the package guide and the security overview agree with it. The RFC-0014
  plan changes only through its amendment process. README's support claim
  then either matches the qualified matrix or says plainly what is
  unqualified. If it is withdrawn, the ledger records that and the record
  stays as history.
- AC3: The documentation gate passes.

## Boundary

Wording and scope only. No platform is newly qualified by this card.
