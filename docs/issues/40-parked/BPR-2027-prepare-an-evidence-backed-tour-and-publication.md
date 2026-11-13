---
schema: "implement-rfc.card.v1"
id: "BPR-2027"
title: "Prepare an evidence-backed tour and publication mapping"
status: "Parked"
rfcs: ["RFC-BPR-0110"]
metadata: {"criteria":["AC02","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0012","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0039-prepare-an-evidence-backed-tour-and-publication.md"}
depends_on: ["BPR-2026"]
parked_on: "human-review"
acceptance: [{"criterion":"AC02","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path: compose_local_publication carries the description and tour, not the conversation"}},{"criterion":"AC05","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Same course: the brief never writes the description"}},{"criterion":"AC06","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_publication_desktop_public_path: carry_tour anchors exact stops at the pull request's head and keeps the rest without lines; the full composition path has no course"}},{"criterion":"AC07","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Withdrawn by the owner's 2026-11-12 amendment"}},{"criterion":"AC08","outcome":"pending","revision":"6887928bd","evidence":{"kind":"command","detail":"Tour course passes on fixture (no Publish, rings, arrival, side chat with cost, the draft's reason word); owner look of the local tour in the PR window's Tour panel rejected 2026-11-13 (session 411e2c74, candidate local-tour-in-tour-panel: three differently styled Accept buttons); rework next round"}},{"criterion":"AC09","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Same course: description option off for a local draft's pull request, on otherwise"}}]
---

# Prepare an evidence-backed tour and publication mapping

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0110](../../rfcs/RFC-BPR-0110-local-tours-and-description.md), criteria `AC02`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

Native authoring proposals and exact preview; selected evidence references; claims and planned checks proposed from a conversation into Evidence; local-to-PR mapping projection.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Map a simulated exact published candidate and expose mismatches without changing the accepted full description or losing local stops. Accept one conversation-proposed check and reject another; neither appears as a reported result.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

Owner amendment, 2026-11-12 ("Mockups; cut Evidence"): RFC-BPR-0109 is Deferred. Selected evidence references and claims or planned checks proposed into Evidence are out of this card; RFC-BPR-0110 AC02 and AC09 are amended and AC07 is withdrawn.

## Owner verdicts (attended session 411e2c7497822b381ac2601afe7909f3, handed back 2026-11-13T08:47:51Z)

Session state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`, frozen build from `89156b02d99e` (product as `6887928bd`). Verbatim notes, each the candidate's final decision:

| Candidate | Verdict | Owner's note |
|---|---|---|
| `local-tour-in-tour-panel` | **Reject** (08:46:23Z) | "Seems like a lot of different button styles. I see 3 buttons that say accept and they all look totally different and I'm not sure why." |
| `local-description-in-overview` | **Reject** (08:44:19Z) | "Seems like the other mode's drawers aren't being closed when going to overview." (first worded "Seems like the other drawers aren't being closed when going to overview."; superseded by the owner's edit) |
| `publish-steps-in-status` | **Accept** (08:47:49Z), with a note | "Looks pretty good, but i cant really test it, I'm not sure if those circles are meant to be misaligned vertically with the text beside them." |

Each decision carries a screenshot attachment in the session state. Not fixed in this round (coordinator, 2026-11-13: the next round fixes them). Host 6331/6340 stopped with the attended-review `stop`.

AC08 stays pending: the tour look was rejected (three differently styled Accept buttons), so BPR-2027 stays parked `human-review` until a reworked look is accepted; the description look was rejected (other modes' drawers stay open on going to Overview).

## Rework (2026-11-13)

All three notes fixed on the lane (`f3fc567f3`, `b8dca6417`, `1f153d3b5`,
RFC-BPR-0106 amendment `195140d79`); evidence in the
RFC-BPR-0110 ledger on the lane `claude/v0-5-0-prerelease-targets-52265d` (`docs/verification/RFC-BPR-0110.md`, "Rework of the owner's verdicts").
AC08 still waits for the owner's look: session `a0cbb9ddda6d1ae524e5d63d4a3342d7`, state
`/Users/jaeger/.attended-review-state/local-draft-tour-panel-v006-2026-11-13/state`
(serve output beside it; the URL stays there), cursor 9, filed 2026-11-13 and
not waited on. Frozen root `/Users/jaeger/local-draft-tour-panel-v006.txmtt3kc`
from `195140d7968f` (launcher check reached `ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`).
One Accept/Reject each: `tour-one-accept`, `overview-closes-drawers`,
`publish-step-marks-aligned`.
