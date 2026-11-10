---
schema: "implement-rfc.card.v1"
id: "BPR-2031"
title: "Verify and close RFC-BPR-0111"
status: "Backlog"
rfcs: ["RFC-BPR-0111"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"resolved_choices":["PUBLISH-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0016","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0043-close-built-in-draft-publication-and-continuity.md","draft_title":"Close built-in draft publication and continuity"}
depends_on: ["BPR-2029", "BPR-2030"]
closeout_for: ["RFC-BPR-0111"]
---

# BPR-2031: Verify and close RFC-BPR-0111

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0111](../../rfcs/RFC-BPR-0111-draft-publication-and-continuity.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for the complete built-in author journey.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge every criterion under the selected native behavior and preserve actual platform/host limitations.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was answered on 2026-11-10; see the [decision record](../../rfcs/assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** From a branch the author has pushed, the built-in path creates a GitHub draft from a local preparation without enterprise tools, publishing the author's own commits unchanged. When the remote branch is missing or differs from the candidate, BPR names the push to the hosted repository and pushes nothing itself; checkout data is unchanged throughout. | Pending | Pending | Not implemented |
| **AC02:** Publication previews bind exact candidate, destination, account and selected public content; changed inputs invalidate authorization and private unselected records remain local. | Pending | Pending | Not implemented |
| **AC03:** Different inspected, outgoing or observed remote material cannot inherit current evidence or tour anchors without an explicit justified reconciliation; dirty material and base movement are represented honestly. | Pending | Pending | Not implemented |
| **AC04:** Lost answers, cancellation, concurrent drift and a pre-existing PR are handled without blind retry, duplicate creation, force update or pretend rollback. | Pending | Pending | Not implemented |
| **AC05:** A created PR with failed follow-up remains associated and recoverable; rerunning follow-up cannot recreate the PR or overwrite an independently edited description. | Pending | Pending | Not implemented |
| **AC06:** The resulting ordinary GitHub draft, description and selected tour can be followed without BPR, while the author's local preparation remains resumable and native-host limitations remain explicit. | Pending | Pending | Not implemented |
| **AC07:** Creating a draft pull request with follow-ups shows each step advancing and a failed step as an error-role banner with the recovering action; a tour with unaccepted stops can be created as a draft pull request but its tour cannot publish until they are accepted; and a review submission for another pull request completes while a creation is in flight. | Pending | Pending | Not implemented |
