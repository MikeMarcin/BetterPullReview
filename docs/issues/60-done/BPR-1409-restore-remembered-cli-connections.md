---
schema: "implement-rfc.card.v1"
id: "BPR-1409"
title: "Restore and persist exact remembered connections through the shared profile runtime"
status: "Done"
rfcs: ["RFC-BPR-0071"]
depends_on: ["BPR-1408"]
priority: "P1"
metadata: {"milestone_plan": {"id": "remember-cli-connections-v1", "path": "docs/rfcs/plans/remember-cli-connections-v1/milestone-plan.json"}, "mandate_revision": "remember-cli-r1"}
acceptance: [{"criterion":"Implement RFC-BPR-0071 AC01–AC04 through shared application startup, exact-host navigation and replacement/sign-out/forget effects.","outcome":"pass","revision":"a09ff90c704f534c7e13c809d50b113042bd262a","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m2-verification.json; exact-source writer and composed result in m2-focused-evidence.json. Full RFC qualification remains BPR-1410."}},{"criterion":"Persist successful selection promptly, report session-only or disable-save failure, drain admitted saves on Quit, and reject late completions.","outcome":"pass","revision":"a09ff90c704f534c7e13c809d50b113042bd262a","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m2-verification.json; exact-source writer and composed result in m2-focused-evidence.json. Full RFC qualification remains BPR-1410."}},{"criterion":"Restore pinned host/login/numeric account without inventory or trust prompts; failures remain visible and retry exact authority without private-state exposure.","outcome":"pass","revision":"a09ff90c704f534c7e13c809d50b113042bd262a","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m2-verification.json; exact-source writer and composed result in m2-focused-evidence.json. Full RFC qualification remains BPR-1410."}}]
---

# BPR-1409: Restore and persist exact remembered connections through the shared profile runtime

RFC base: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.

## Acceptance

- Implement RFC-BPR-0071 AC01–AC04 through shared application startup, exact-host navigation and replacement/sign-out/forget effects.
- Persist successful selection promptly, report session-only or disable-save failure, drain admitted saves on Quit, and reject late completions.
- Restore pinned host/login/numeric account without inventory or trust prompts; failures remain visible and retry exact authority without private-state exposure.
