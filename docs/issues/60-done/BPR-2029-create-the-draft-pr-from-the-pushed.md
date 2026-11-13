---
schema: "implement-rfc.card.v1"
id: "BPR-2029"
title: "Create the draft PR from the pushed branch"
status: "Done"
rfcs: ["RFC-BPR-0111"]
metadata: {"criteria":["AC01","AC02","AC07"],"resolved_choices":["PUBLISH-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0014","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0041-create-the-draft-pr-from-the-pushed.md"}
depends_on: ["BPR-2028"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"1237dbefd","evidence":{"kind":"command","detail":"live_draft_course.py live-draft on BPR-Test #184; local_draft_publication re-run at 0f444ac0a"}},{"criterion":"AC02","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication, local_draft_publication"}},{"criterion":"AC07","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"application_local_draft_publication_desktop_public_path; application_dependable_session_public_path step 2c; publish steps accepted in sessions 411e2c74, a0cbb9dd"}}]
---

# Create the draft PR from the pushed branch

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0111](../../rfcs/RFC-BPR-0111-draft-publication-and-continuity.md), criteria `AC01`, `AC02`, `AC07`.

The explicit native publication capability and its exact preview: verify the remote head against the candidate, name the push to the hosted repository when it is missing or different, and create the draft PR through the admitted REST authority. BPR pushes nothing.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Create a draft from a pushed branch on the designated safe live target with the author's commits unchanged; show the named push for an unpushed branch, a different remote head and an isolated local clone. Do not infer host qualification from fixtures.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was answered on 2026-11-10: the author pushes and BPR creates the pull request from the verified remote head. See the [decision record](../../rfcs/assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).

## Implementation record (not claimed)

Engineered on the lane while its dependencies wait (the owner rejected the
RFC-BPR-0109 and 0110 looks on 2026-11-12); this card was not readied or
claimed. Commits `387f7298d` (core), `4675974cf` and `62f740fc6` (gh pull
request catalog), `6bc5217f5` and `433068905` (application), `36dcd7e21` (Qt
area, composition), `75c035a49` (real-worktree course). Evidence, including
the live BPR-Test drafts #178 and #179: [ledger](../../verification/RFC-BPR-0111.md).

Owner amendment, 2026-11-12 ("Mockups; cut Evidence"): RFC-BPR-0109 is Deferred and its closeout BPR-2025 is no longer a dependency; the evidence parts of this card are inactive (see the RFC's amendment).

## Owner verdicts (attended session 411e2c7497822b381ac2601afe7909f3, handed back 2026-11-13T08:47:51Z)

Session state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`, frozen build from `89156b02d99e` (product as `6887928bd`). Verbatim notes, each the candidate's final decision:

| Candidate | Verdict | Owner's note |
|---|---|---|
| `local-tour-in-tour-panel` | **Reject** (08:46:23Z) | "Seems like a lot of different button styles. I see 3 buttons that say accept and they all look totally different and I'm not sure why." |
| `local-description-in-overview` | **Reject** (08:44:19Z) | "Seems like the other mode's drawers aren't being closed when going to overview." (first worded "Seems like the other drawers aren't being closed when going to overview."; superseded by the owner's edit) |
| `publish-steps-in-status` | **Accept** (08:47:49Z), with a note | "Looks pretty good, but i cant really test it, I'm not sure if those circles are meant to be misaligned vertically with the text beside them." |

Each decision carries a screenshot attachment in the session state. Not fixed in this round (coordinator, 2026-11-13: the next round fixes them). Host 6331/6340 stopped with the attended-review `stop`.

The publish steps look (RFC-BPR-0111 AC01, AC07 surfaces) was accepted with the owner's question about the step circles' vertical alignment, to be checked in the next round.

## Readiness judgment (2026-11-13)

BPR-2028 is Done, so this card's dependency is met. Its evidence, from the [ledger](../../verification/RFC-BPR-0111.md):
- AC01 passes live: draft #184 at `1237dbefd`, with the push named, the author's own commit, and the carried tour stop anchored and Accepted.
- AC02 passes (`core_local_draft_publication`, `local_draft_publication`).
- AC07 passes on fixture, and the owner accepted the publish steps (session 411e2c74, aligned in a0cbb9dd), except one clause: "a review submission for another pull request completes while a creation is in flight". That clause has no course: `application_local_draft_publication_desktop_public_path` shows only the draft's own lane free during a held creation.

The card was not readied. `issue_board.py move` refuses, because `docs/rfcs/assets/local-draft-authoring-v1/AMENDMENTS.md` (protected) links this card's Backlog path, as it links BPR-2031's. Any lane move would edit protected content. Closing RFC-BPR-0111 needs a decision on that link (an exception for the link line, or a path-preserving approach) and a course for AC07's submission clause.

## Closeout (2026-11-13)

Both obstacles above are resolved. By the coordinator's decision the
local-draft-authoring amendment record names this card as a card reference
(`d3e7cb8a8`). AC07's submission clause has its course:
`application_dependable_session_public_path` step 2c (`0f444ac0a`) holds
Create Draft PR's lane in the composed desktop and #18's Approve still
reaches GitHub ([ledger](../../verification/RFC-BPR-0111.md#ac07s-submission-clause-and-closeout-2026-11-13)).

- AC01: Pass, live draft #184 at `1237dbefd` (and #178, #179); `local_draft_publication` re-run at `0f444ac0a`.
- AC02: Pass, `core_local_draft_publication`, `local_draft_publication` re-run at `0f444ac0a`.
- AC07: Pass, `application_local_draft_publication_desktop_public_path` and `application_dependable_session_public_path` at `0f444ac0a`; the owner accepted the publish steps (sessions 411e2c74, a0cbb9dd).
