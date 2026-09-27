---
schema: "implement-rfc.card.v1"
id: "BPR-1217"
title: "Compose persisted diagnostic detail and privacy controls"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1216"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6089117d7bd13ad52d2b34c49713b842cd0f3954","evidence":{"kind":"observation","detail":"Rendered public Help/Settings course verifies signed-out reachability, disclosure cancel/accept, persisted independent selections, restart-Off with no startup bytes and invalid preferences; private SQLite direct tests pass. Exact commands, failures/fixes and limitations: docs/verification/RFC-BPR-0027.md#M3. Full native/human and rich export obligations remain M4/M5."}},{"criterion":"AC2","outcome":"pass","revision":"6089117d7bd13ad52d2b34c49713b842cd0f3954","evidence":{"kind":"observation","detail":"Four detail/privacy combinations, real provider Verbose file output, independent parsed JSON labels, forbidden-content file/ring canaries, ring/loss bounds, actual 180-second age cutoff and reserved warning/error drainage pass. Exact commands, failures/fixes and limitations: docs/verification/RFC-BPR-0027.md#M3. Full native/human and rich export obligations remain M4/M5."}},{"criterion":"AC3","outcome":"pass","revision":"6089117d7bd13ad52d2b34c49713b842cd0f3954","evidence":{"kind":"observation","detail":"Manual effect/decision acknowledgement and persistence refusal, Off/Clear barriers, late revoked-generation refusal, actual Accounts and hosts Sign out, exact profile folder handoff, shared-window profile controls and prior profile isolation/lifecycle checks pass. Exact commands, failures/fixes and limitations: docs/verification/RFC-BPR-0027.md#M3. Full native/human and rich export obligations remain M4/M5."}}]
---

# Compose persisted diagnostic detail and privacy controls

Signed-out profile Settings own Standard/Verbose, independent identity privacy, Off, recent context, Clear and Open logs; transitions and sign-out clear transient state under acknowledged barriers.

## Acceptance

- AC1: Reach and persist controls safely across restart and invalid preferences through the shipped UI.
- AC2: Enforce detail/privacy combinations, typed identity labels, no forbidden material, bounded priority/loss and ring behavior.
- AC3: Verify mode/Off/Clear/privacy/sign-out barriers and profile/window scope.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
