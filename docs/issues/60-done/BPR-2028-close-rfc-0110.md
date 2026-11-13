---
schema: "implement-rfc.card.v1"
id: "BPR-2028"
title: "Verify and close RFC-BPR-0110"
status: "Done"
rfcs: ["RFC-BPR-0110"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0013","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0040-close-pre-pr-description-and-tour-preparation.md","draft_title":"Close pre-PR description and tour preparation"}
depends_on: ["BPR-2026", "BPR-2027"]
closeout_for: ["RFC-BPR-0110"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path; re-run at a143adfc1"}},{"criterion":"AC02","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path (compose_local_publication); re-run at a143adfc1"}},{"criterion":"AC03","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path; re-run at a143adfc1"}},{"criterion":"AC04","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path after restart; re-run at a143adfc1"}},{"criterion":"AC05","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path; re-run at a143adfc1"}},{"criterion":"AC06","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_publication_desktop_public_path (carry_tour); re-run at a143adfc1"}},{"criterion":"AC07","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"Withdrawn by the owner, 2026-11-12"}},{"criterion":"AC08","outcome":"pass","revision":"3b385c04d","evidence":{"kind":"observation","detail":"Tour courses and qt_tour_chat_in_tab; owner looks accepted (sessions a0cbb9dd, d420cccb); re-run at a143adfc1"}},{"criterion":"AC09","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"application_local_draft_tour_desktop_public_path; re-run at a143adfc1"}}]
---

# BPR-2028: Verify and close RFC-BPR-0110

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0110](../../rfcs/RFC-BPR-0110-local-tours-and-description.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

Acceptance assessment for local narrative and tours.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge every criterion, including default description preservation and existing PR-tour compatibility.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** A signed-out author writes and resumes a description and ordered code tour against an uncommitted local candidate without a fabricated PR/account/commit identity. | application_local_draft_tour_desktop_public_path; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2026) | `6887928bd` |
| **AC02:** Private deliberation (notes, conversations and proposals) does not appear in the exact public preview, which carries the title, the accepted description and the tour. (Amended 2026-11-12; see above.) | application_local_draft_tour_desktop_public_path (compose_local_publication); re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2027) | `6887928bd` |
| **AC03:** Candidate changes and late proposals preserve original material and later author edits, with explicit stale/ambiguous/missing destinations rather than guessed code links. Authoring runs use captured candidate repository inputs through the candidate workspace or tool-free context; unselected live-helper edits cannot enter that context even when a proposed stop has valid candidate lines. Out-of-candidate source references cannot be accepted as written. An accepted refresh starts a new session on the successor candidate while the earlier conversation stays readable against its own. | application_local_draft_tour_desktop_public_path; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2026) | `6887928bd` |
| **AC04:** Local authoring, conversation history and accepted proposals survive restart with no live agent process and no fabricated account or pull-request scope, while bounded context selection and no-agent editing remain usable; the local-authoring prompts are editable in Settings and round-trip through the sharing format. | application_local_draft_tour_desktop_public_path after restart; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2026) | `6887928bd` |
| **AC05:** Description and tour can be edited together deliberately but never overwrite one another implicitly; the prepared-tour publication projection preserves the accepted full PR description. | application_local_draft_tour_desktop_public_path; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2027) | `6887928bd` |
| **AC06:** Against an exact simulated publication result, matching stops acquire the actual PR anchors while mismatched stops and unresolved URLs remain local and visibly incomplete; existing PR-owned tour workflows retain their behavior. | application_local_draft_publication_desktop_public_path (carry_tour); re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2027) | `6887928bd` |
| **AC07:** Withdrawn by the owner's 2026-11-12 amendment: with RFC-BPR-0109 deferred there is no evidence record for a conversation to propose into. | Withdrawn by the owner, 2026-11-12; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Withdrawn (BPR-2027) | `6887928bd` |
| **AC08:** A local tour shows the three bands with no Publish action; arriving at a stop reveals its candidate lines; a stop is accepted with Accept, Save or its own proposal Accept; after an accepted refresh only stops whose correspondence is not exact are Changed since accepted, each with its reason; and its chats show the session line, cost, reference links and running block as a pull request's tour chats do. | Tour courses and qt_tour_chat_in_tab; owner looks accepted (sessions a0cbb9dd, d420cccb); re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2027) | `3b385c04d` |
| **AC09:** For a pull request associated with a local draft, "Update PR description after publishing" starts off and cannot change the body without the composed preview, while a pull request with no local draft keeps the existing default. (Amended 2026-11-12; see above.) | application_local_draft_tour_desktop_public_path; re-run at `a143adfc1` (25 local-draft, tour publication and Tour rows pass) | Pass (BPR-2027) | `6887928bd` |

## Owner verdicts (attended session 411e2c7497822b381ac2601afe7909f3, handed back 2026-11-13T08:47:51Z)

Session state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`, frozen build from `89156b02d99e` (product as `6887928bd`). Verbatim notes, each the candidate's final decision:

| Candidate | Verdict | Owner's note |
|---|---|---|
| `local-tour-in-tour-panel` | **Reject** (08:46:23Z) | "Seems like a lot of different button styles. I see 3 buttons that say accept and they all look totally different and I'm not sure why." |
| `local-description-in-overview` | **Reject** (08:44:19Z) | "Seems like the other mode's drawers aren't being closed when going to overview." (first worded "Seems like the other drawers aren't being closed when going to overview."; superseded by the owner's edit) |
| `publish-steps-in-status` | **Accept** (08:47:49Z), with a note | "Looks pretty good, but i cant really test it, I'm not sure if those circles are meant to be misaligned vertically with the text beside them." |

Each decision carries a screenshot attachment in the session state. Not fixed in this round (coordinator, 2026-11-13: the next round fixes them). Host 6331/6340 stopped with the attended-review `stop`.

RFC-BPR-0110 AC08 is not met: its look was rejected. This card waits for the rework and a new look.

## Closeout (2026-11-13)

Every criterion holds: AC01, AC03 and AC04 by BPR-2026; AC02, AC05, AC06, AC08 and AC09 by BPR-2027; AC07 is withdrawn (owner, 2026-11-12). AC08's owner looks are accepted: `overview-closes-drawers` and `publish-step-marks-aligned` (session a0cbb9dd) and `tour-one-button-style` (session d420cccb). At `a143adfc1` the 25 local-draft, tour-publication and Tour rows pass (`application_review_tour_publication_stop` failed once under parallel GUI load and passes alone, three times). The documentation gate is OK. RFC-BPR-0110 is Auto-Verified.
