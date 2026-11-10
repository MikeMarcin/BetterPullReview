---
schema: "implement-rfc.card.v1"
id: "BPR-1982"
title: "Preserve legacy retarget destinations after saved receipt reopen"
status: "In Progress"
rfcs: ["RFC-BPR-0042"]
depends_on: ["BPR-1981"]
priority: "P2"
metadata: {"kind":"bug","related_cards":["BPR-1981"],"mandate_revision":"tour-destination-repairs-r1-legacy-destination-fix","milestone_plan":{"id":"tour-legacy-destination-v1","path":"docs/rfcs/plans/tour-legacy-destination-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1982--BPR-1982--899551a62f40.json"}
---

# BPR-1982: Preserve saved legacy retarget destinations

Post-review follow-up to terminal BPR-1981. Baseline `992ebba9c6ea35822a24ea6c315676a865fb5f65`. The pre-upgrade receipt reducer could retain imported Destination A while Original B, no Mapped range and exact_original recorded a confirmed replacement at B. The current verifier trusts A and falsely conflicts after a real SQLite save/reopen. Preserve the original acceptance records.

## Acceptance

- AC1. A permanent regression saves and reopens the stale legacy receipt through actual SQLite, then production-session maintenance succeeds at B with one body edit; demonstrate failure before the fix.
- AC2. Wrong remote identity/source retains zero-write refusal; confirmed modern Destination, Original/Mapped bytes and legitimate subsequent retargets remain correct.
- AC3. Affected focused tests and static/documentation checks pass for the exact candidate; explicit commits, correction evidence and quiescent command state are durable.
