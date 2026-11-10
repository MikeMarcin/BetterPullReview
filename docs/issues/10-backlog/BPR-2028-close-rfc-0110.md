---
schema: "implement-rfc.card.v1"
id: "BPR-2028"
title: "Verify and close RFC-BPR-0110"
status: "Backlog"
rfcs: ["RFC-BPR-0110"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0013","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0040-close-pre-pr-description-and-tour-preparation.md","draft_title":"Close pre-PR description and tour preparation"}
depends_on: ["BPR-2026", "BPR-2027"]
closeout_for: ["RFC-BPR-0110"]
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
| **AC01:** A signed-out author writes and resumes a description and ordered code tour against an uncommitted local candidate without a fabricated PR/account/commit identity. | Pending | Pending | Not implemented |
| **AC02:** Selected evidence enters the explanation with its origin, scope and limitations; private deliberation and unselected records do not appear in the exact public preview. | Pending | Pending | Not implemented |
| **AC03:** Candidate changes and late proposals preserve original material and later author edits, with explicit stale/ambiguous/missing destinations rather than guessed code links. Authoring runs use captured candidate repository inputs through the candidate workspace or tool-free context; unselected live-helper edits cannot enter that context even when a proposed stop has valid candidate lines. Out-of-candidate source references cannot be accepted as written. An accepted refresh starts a new session on the successor candidate while the earlier conversation stays readable against its own. | Pending | Pending | Not implemented |
| **AC04:** Local authoring, conversation history and accepted proposals survive restart with no live agent process and no fabricated account or pull-request scope, while bounded context selection and no-agent editing remain usable; the local-authoring prompts are editable in Settings and round-trip through the sharing format. | Pending | Pending | Not implemented |
| **AC05:** Description and tour can be edited together deliberately but never overwrite one another implicitly; the prepared-tour publication projection preserves the accepted full PR description. | Pending | Pending | Not implemented |
| **AC06:** Against an exact simulated publication result, matching stops acquire the actual PR anchors while mismatched stops and unresolved URLs remain local and visibly incomplete; existing PR-owned tour workflows retain their behavior. | Pending | Pending | Not implemented |
| **AC07:** A whole-change conversation proposes a claim or planned check that reaches the evidence record only through the author's acceptance, as a proposal with its agent origin and never as a reported or observed result. | Pending | Pending | Not implemented |
| **AC08:** A local tour shows the three bands with no Publish action; arriving at a stop reveals its candidate lines; a stop is accepted with Accept, Save or its own proposal Accept; after an accepted refresh only stops whose correspondence is not exact are Changed since accepted, each with its reason; and its chats show the session line, cost, reference links and running block as a pull request's tour chats do. | Pending | Pending | Not implemented |
| **AC09:** For a pull request associated with a local draft, "Update PR description after publishing" starts off and cannot change the body without the composed preview, while a pull request with no local draft keeps the existing default; a stop citing evidence that needs recheck is Changed since accepted. | Pending | Pending | Not implemented |
