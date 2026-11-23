---
schema: "implement-rfc.card.v1"
id: "BPR-2361"
title: "Start an authored pull request update on its first press"
status: "Done"
rfcs: []
depends_on: ["BPR-2357", "BPR-2360"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner attended handback, 2026-11-23","mandate_revision":"8832deb4707a9f06903d2c1b664d31ab696e01b0","reservation":"docs/issues/_reservations/BPR-2361--BPR-2361--b0cc236d3778.json","related_rfcs":["RFC-BPR-0085","RFC-BPR-0092","RFC-BPR-0101"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d68ca8ba72e036acf9e1c78b85ff5f946e533405","evidence":{"kind":"observation","detail":"Network-isolated application_update_from_target_public_path on macOS arm64 with Qt 6.12.1 showed the indigo Status action under the authored inline-drawer-v1 #22 conflict on main while Merge details were closed and the workspace check was held. One press opened Agent and sent one turn after the check completed. The fixture answered the turn bound to target main, head inline-drawer and accepted head 89df5e592e5db97a526a05a445735733395e4832. The cold main Overview route also passed. Hidden Cocoa Light and Dark rendered the Status action, and the narrowest-width Qt check passed."}},{"criterion":"AC2","outcome":"pass","revision":"d68ca8ba72e036acf9e1c78b85ff5f946e533405","evidence":{"kind":"observation","detail":"The same composition course coalesced repeated Overview presses, sent zero after Close review or Agents settings Stop, preserved the warm Status fork destination and replacement guidance, and sent zero for the reviewer even when pressed programmatically. The Qt course also refused a programmatic Status press without a current conflict offer. Terminal refusal uses the existing setup readiness path."}},{"criterion":"AC3","outcome":"pass","revision":"28c6640c2f2675345d5e46b5ed767c01dfac4014","evidence":{"kind":"observation","detail":"Owner accepted the frozen Qt 6.12.1 Status conflict and indigo agent action in attended session 2aff71f637fc1c4e97bd3bf9a6b95a62, decision ecb95afa-f200-42f5-b794-a231c9d4d327, candidate fingerprint 64d850d4c9bd69da7b92404f52fe59a6115913235d17a83a8d66c4672fe3080a."}}]
---

# BPR-2361: Start an authored pull request update on its first press

The owner rejected the first frozen PR update candidate with “Doesn't work
with the fixture.” On the authored inline-drawer-v1 pull request, pressing
**Update from target with agent** while setup checks its workspace opens Agent
but drops the request. A later press sends once setup is ready.

## Owner amendment, 2026-11-23

After rejecting the frozen candidate that showed “Merge state unavailable”,
the owner specified the action's location at the Merge row in Status and
requested the indigo Agent colour.
The rejected build showed “Merge state unavailable” because the fixture had
no merge assessment. The new look opens the same authored PR with a synthetic
GitHub conflict assessment for its accepted head and base. The action follows
the conflict line in Status, visible before expanding the details, and wears
the configured Agent accent. The existing Overview action wears the same accent.

## Prior attended decision

The owner rejected revision `157a4487e14e3d73c4b8014099fa25c4278b18a2` in
session `484776b1fd455a800012d47912774f04`, decision
`61b45a64-0197-4606-a7fc-2d15727b79c1`, candidate fingerprint
`50617e9874397f13110e089a428428496483b79024edfffde30b587f24702921`.
The note was “Still doesn't show the option.” The attached screenshot shows
the Overview action and “Merge state unavailable” in Status.

## Acceptance

- AC1. On the authored inline-drawer-v1 PR with an accepted conflict, the
  Agent-accented action is visible at the conflict in Status with Merge
  details closed. From Status or the main Overview, one press during
  transient setup sends one bound turn when ready. The fixture agent answers.
- AC2. Repeated presses while waiting coalesce. Setup cancellation, terminal
  refusal or a stale accepted review clears the pending press. The warm
  fork Status path preserves its exact destination and replacement guidance.
- AC3. Offer the repaired flow in a fresh attended review running Qt 6.12.1,
  then record the owner's decision against its frozen revision.

## Plan

Keep one pending press in window composition while setup settles. Recheck the
accepted author, PR and revision against the Agent session before release.
Serve the exact synthetic assessment through the fixture GitHub transport and
exercise the cold Status action alongside the Overview, cancellation, warm
fork and reviewer paths.
