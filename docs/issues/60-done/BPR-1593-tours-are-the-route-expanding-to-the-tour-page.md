---
schema: "implement-rfc.card.v1"
id: "BPR-1593"
title: "Tours are the route, expanding to the tour page"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1587"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "tours-route", "criteria": ["AC06"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC06","outcome":"pass","revision":"a1222c7f2","evidence":{"kind":"observation","detail":"44 focused rows pass serially (373.20 s), including every tour row, three complete window scales, Notes, scoped prose, submission, start-review and all composition contracts. Merged main a7a45e1c9, reviewed tests-on src/app fixture delta and resealed exact pins with dated note. Format22, docs and diff checks pass."}},{"criterion":"owner-route-and-page","outcome":"pass","revision":"eb4990c98","evidence":{"kind":"observation","detail":"Owner accepted frozen-eb4990c98-v001 in session 9a9c8a15076be0d2dbf02051bf3e0b6b at sequence 4, no note; handback sequence 5 consumed. Explicit exception to prior exclusion authorized in chat 2026-10-18."}}]
---
# BPR-1593: Tours are the route, expanding to the tour page

RFC-BPR-0085 D4: the Tour tab as the route (brief card, numbered stops on an
amber line, the current stop in place, Preview and publish in the foot) with
Expand to the tour page (outline, stop editor with code excerpt, live
preview) and a read-only route for reviewers.

## Acceptance

- RFC-BPR-0085 AC06; the existing tour courses pass.
- Owner, attended: "A tour reads as a route, and expands to its page."

## Boundaries

Qt presentation of RFC-BPR-0039 to RFC-BPR-0045's model; publication and
recovery unchanged.
