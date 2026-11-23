---
schema: "implement-rfc.card.v1"
id: "BPR-2361"
title: "Start an authored pull request update on its first press"
status: "Parked"
rfcs: []
depends_on: ["BPR-2357", "BPR-2360"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner attended handback, 2026-11-23","mandate_revision":"8832deb4707a9f06903d2c1b664d31ab696e01b0","reservation":"docs/issues/_reservations/BPR-2361--BPR-2361--b0cc236d3778.json","related_rfcs":["RFC-BPR-0085","RFC-BPR-0092","RFC-BPR-0101"]}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7eee9f0940a44cd4ed8566803eb4c1cf25e80855","evidence":{"kind":"observation","detail":"The network-isolated application_update_from_target_public_path ran offscreen on macOS arm64 and reported Qt 6.12.1. On authored inline-drawer-v1 #22, one Overview press with an active workspace check opened Agent, then sent one answered turn for target main, head inline-drawer and the exact accepted head."}},{"criterion":"AC2","outcome":"pass","revision":"7eee9f0940a44cd4ed8566803eb4c1cf25e80855","evidence":{"kind":"observation","detail":"The composed course proved repeated presses sent one turn, Home Close review and Agents settings Stop sent zero, the warm Status suggestion kept exact fork and target routing and replacement guidance, and the reviewer sent zero. Source inspection confirmed terminal setup still takes the existing readiness refusal."}},{"criterion":"AC3","outcome":"pending","revision":"7eee9f0940a44cd4ed8566803eb4c1cf25e80855","evidence":{"kind":"artifact","detail":"Coordinator owns integration and a fresh frozen inline-drawer-v1 review. Owner acceptance is pending on the resulting frozen revision."}}]
---

# BPR-2361: Start an authored pull request update on its first press

The owner rejected the first frozen PR update candidate with “Doesn't work
with the fixture.” On the authored inline-drawer-v1 pull request, pressing
**Update from target with agent** while setup checks its workspace opens Agent
but drops the request. A later press sends once setup is ready.

## Acceptance

- AC1. On the authored inline-drawer-v1 Overview, one press during transient
  setup sends one bound turn when ready. The fixture agent answers.
- AC2. Repeated presses while waiting coalesce. Setup cancellation, terminal
  refusal or a stale accepted review clears the pending press. The warm
  fork Status path preserves its exact destination and replacement guidance.
- AC3. Offer the repaired flow in a fresh attended review running Qt 6.12.1,
  then record the owner's decision against its frozen revision.

## Plan

Keep one pending press in window composition while setup settles. Recheck the
accepted author, PR and revision against the Agent session before release.
Exercise cold, repeated and stale presses through the composed fixture and
retain the warm fork and reviewer course.
