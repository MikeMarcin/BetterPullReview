---
schema: "implement-rfc.card.v1"
id: "BPR-2027"
title: "Prepare an evidence-backed tour and publication mapping"
status: "Parked"
rfcs: ["RFC-BPR-0110"]
metadata: {"criteria":["AC02","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0012","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0039-prepare-an-evidence-backed-tour-and-publication.md"}
depends_on: ["BPR-2026"]
parked_on: "human-review"
acceptance: [{"criterion":"AC02","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path: compose_local_publication carries the description and tour, not the conversation"}},{"criterion":"AC05","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Same course: the brief never writes the description"}},{"criterion":"AC06","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_publication_desktop_public_path: carry_tour anchors exact stops at the pull request's head and keeps the rest without lines; the full composition path has no course"}},{"criterion":"AC07","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Withdrawn by the owner's 2026-11-12 amendment"}},{"criterion":"AC08","outcome":"pending","revision":"6887928bd","evidence":{"kind":"command","detail":"Tour course passes on fixture (no Publish, rings, arrival, side chat with cost, the draft's reason word); owner look of the local tour in the PR window's Tour panel filed (attended session, candidate local-tour-in-tour-panel)"}},{"criterion":"AC09","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Same course: description option off for a local draft's pull request, on otherwise"}}]
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
