---
schema: "implement-rfc.card.v1"
id: "BPR-1603"
title: "Pull-request comments wait at the Comments drawer's edge"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1594"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner idea relayed by the coordinator during BPR-1593, 2026-10-18: \"The notes-on-the-pull-request thing might actually be a good UX for comments on the pull request too.\"", "reservation": "docs/issues/_reservations/BPR-1603--BPR-1603--a7b51ca1327a.json", "milestone": "conversation-chips", "criteria": ["AC10"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC10","outcome":"pass","revision":"f80960a4087143939878a0ebe1841df309f26720","evidence":{"kind":"observation","detail":"Owner Accept of product 9eab5046a in session d28b9f71ffc1dfc153f9b731595c0dca decision 6, handback 7, no note. Focused checks, both composition contracts/reseal, docs and format/tidy pass; durable quote/restart/stale head/lost response stub controls and cleaned live BPR-Test PR 108 course pass. See BPR-1603 handoff and card acceptance evidence."}}]
---
# BPR-1603: Pull-request comments wait at the Comments drawer's edge

RFC-BPR-0085 D8 (amendment, 2026-10-18). Pull-request-level conversation
comments and review summaries, which are on no file, wait as a chip at the
Comments drawer's top edge, as [[BPR-1592]]'s notes on the pull request do in
Notes. The chip lists them; each opens in place, carries its unread mark from
[[BPR-1601]]'s read state, and offers Show in Discuss.

## Acceptance

- RFC-BPR-0085 AC10.
- A short design note with a mockup in Light and Dark comes first.
- Owner, attended: "Comments on the pull request wait at the drawer's edge and
  open in place."

## Boundaries

Qt presentation over the accepted conversation projection and BPR-1601's read
state; no new GitHub conversation read, and Discuss keeps every comment as it does today.
Uses BPR-1592's edge-chip and list pattern, and the shared drawer parts.

## Owner scope extension — 2026-10-18

Direction A selected without a note in session `537cac7b…`, sequence 4,
handback 5 consumed. The owner then explicitly confirmed drawer replies:
private durable draft first, explicit Send of a new issue comment quoting
the accepted post, existing staged publication and transport witness,
exact-PR reread before POST, changed-target refusal, visible failure/retry,
and restart recovery. Verify with stubs and one live create/delete course
on MikeMarcin/BPR-Test, observing X-RateLimit-Remaining. No live test mutation
is authorized for any other repository by this card.

The frontend-design skill was loaded for implementation; the accepted A
mockup supplies the layout. Native palette/type and Notes' pill dimensions
supply the tokens, with the blue unread badge and one in-place list. No new
style sheet or independent design direction is introduced.


## Acceptance evidence — 2026-10-18

Owner accepted built direction A v001 with no note in attended session
`d28b9f71ffc1dfc153f9b731595c0dca`: decision 6, handback 7 consumed.
Product 9eab5046a; the frozen launcher and Light/Dark checks reached
ATTENDED_CONVERSATION_SHOWN. The frontend-design skill was used; design A
was selected before implementation and compared in six review images.

Focused checks: 11 submission/storage/provider/Qt/public-path rows passed;
four touched rows passed after refinements; six finder/reveal/public-preview
and staged regressions passed; architecture and negative controls, production
composition, timing control, large-review responsiveness, and resealed
round-trip composition passed (six rows, 790.05 s). Format22, touched tidy,
documentation gate and 67 documentation tests passed. Stub courses cover
local-only draft, restart, stale-edit CAS, changed-head refusal and lost-response
reconciliation with exactly one issue-comment POST. Live production-session
course on MikeMarcin/BPR-Test PR 108 passed: reply created/read back/deleted,
source deleted, PR closed, disposable branch deleted; lowest observed
X-RateLimit-Remaining 4970. Detailed evidence and cleanup live in the handoff.

Origin/main 937f96880 merged before implementation and remained unchanged at
landing fetch. Both composition contracts and dated three-pin reseal apply
to the accepted merged product; only acceptance documentation followed.
