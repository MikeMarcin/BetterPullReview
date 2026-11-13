---
schema: "implement-rfc.card.v1"
id: "BPR-2031"
title: "Verify and close RFC-BPR-0111"
status: "Done"
rfcs: ["RFC-BPR-0111"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"resolved_choices":["PUBLISH-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0016","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0043-close-built-in-draft-publication-and-continuity.md","draft_title":"Close built-in draft publication and continuity"}
depends_on: ["BPR-2029", "BPR-2030"]
closeout_for: ["RFC-BPR-0111"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"1237dbefd","evidence":{"kind":"command","detail":"live draft #184 on BPR-Test; local_draft_publication re-run at 0f444ac0a"}},{"criterion":"AC02","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication, local_draft_publication"}},{"criterion":"AC03","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication; application_local_draft_publication_desktop_public_path"}},{"criterion":"AC04","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"local_draft_publication"}},{"criterion":"AC05","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication, local_draft_publication, application_local_draft_publication_desktop_public_path"}},{"criterion":"AC06","outcome":"pass","revision":"1237dbefd","evidence":{"kind":"command","detail":"live draft #184 with the carried stop; core_local_draft_publication re-run at 0f444ac0a"}},{"criterion":"AC07","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"application_local_draft_publication_desktop_public_path; application_dependable_session_public_path step 2c; publish steps accepted (sessions 411e2c74, a0cbb9dd)"}}]
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
| **AC01:** From a branch the author has pushed, the built-in path creates a GitHub draft from a local preparation without enterprise tools, publishing the author's own commits unchanged. When the remote branch is missing or differs from the candidate, BPR names the push to the hosted repository and pushes nothing itself; checkout data is unchanged throughout. | Live draft #184 (`live_draft_course.py`, BPR-Test); `local_draft_publication`; re-run at `0f444ac0a` | Pass (BPR-2029) | `1237dbefd` |
| **AC02:** Publication previews bind exact candidate, destination, account and selected public content; changed inputs invalidate authorization and private unselected records remain local. | `core_local_draft_publication`, `local_draft_publication`; re-run at `0f444ac0a` | Pass (BPR-2029) | `0f444ac0a` |
| **AC03:** Different inspected, outgoing or observed remote material cannot inherit tour anchors without an explicit justified reconciliation; dirty material and base movement are represented honestly. (Amended 2026-11-12; see above.) | `core_local_draft_publication`; `application_local_draft_publication_desktop_public_path` (tour carry at the head and another head); re-run at `0f444ac0a` | Pass (BPR-2030) | `0f444ac0a` |
| **AC04:** Lost answers, cancellation, concurrent drift and a pre-existing PR are handled without blind retry, duplicate creation, force update or pretend rollback. | `local_draft_publication` (lost answers, restart, drift with negative control, existing pull request); re-run at `0f444ac0a` | Pass (BPR-2030) | `0f444ac0a` |
| **AC05:** A created PR with failed follow-up remains associated and recoverable; rerunning follow-up cannot recreate the PR or overwrite an independently edited description. | `core_local_draft_publication`, `local_draft_publication`, `application_local_draft_publication_desktop_public_path`; re-run at `0f444ac0a` | Pass (BPR-2030) | `0f444ac0a` |
| **AC06:** The resulting ordinary GitHub draft, description and selected tour can be followed without BPR, while the author's local preparation remains resumable and native-host limitations remain explicit. | Live draft #184 with the carried stop anchored and Accepted; `core_local_draft_publication`; re-run at `0f444ac0a` | Pass (BPR-2030) | `1237dbefd` |
| **AC07:** Creating a draft pull request with follow-ups shows each step advancing and a failed step as an error-role banner with the recovering action; a tour with unaccepted stops can be created as a draft pull request but its tour cannot publish until they are accepted; and a review submission for another pull request completes while a creation is in flight. | `application_local_draft_publication_desktop_public_path`; `application_dependable_session_public_path` step 2c; publish steps accepted (sessions 411e2c74, a0cbb9dd); re-run at `0f444ac0a` | Pass (BPR-2029) | `0f444ac0a` |

## Closeout (2026-11-13)

Every criterion passes: AC01, AC02 and AC07 by BPR-2029; AC03 to AC06 by BPR-2030. AC07's last clause (a review submission for another pull request completes while a creation is in flight) has its course since `0f444ac0a`. At `0f444ac0a` the seven RFC-BPR-0111 rows, `application_composition_source_contract` and `architecture_boundaries` pass (the RFC-BPR-0111 ledger on the lane). GHES and fork heads stay unsupported, as the RFC states. PUBLISH-01's reopening (RFC-BPR-0114) and the comments-absorb-notes amendment are carried by their own RFCs. RFC-BPR-0111 is Auto-Verified.
