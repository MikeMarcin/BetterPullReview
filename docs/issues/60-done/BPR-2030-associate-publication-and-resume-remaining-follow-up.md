---
schema: "implement-rfc.card.v1"
id: "BPR-2030"
title: "Associate publication and resume remaining follow-up"
status: "Done"
rfcs: ["RFC-BPR-0111"]
metadata: {"criteria":["AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0015","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0042-associate-publication-and-resume-remaining-follow-up.md"}
depends_on: ["BPR-2029"]
acceptance: [{"criterion":"AC03","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication; application_local_draft_publication_desktop_public_path (carry_tour at the head and another head)"}},{"criterion":"AC04","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"local_draft_publication (lost answers, restart, drift with negative control, existing pull request); core_local_draft_publication"}},{"criterion":"AC05","outcome":"pass","revision":"0f444ac0a","evidence":{"kind":"command","detail":"core_local_draft_publication; local_draft_publication; application_local_draft_publication_desktop_public_path"}},{"criterion":"AC06","outcome":"pass","revision":"1237dbefd","evidence":{"kind":"command","detail":"live_draft_course.py live-draft on BPR-Test #184 (carried stop anchored and Accepted); core_local_draft_publication re-run at 0f444ac0a"}}]
---

# Associate publication and resume remaining follow-up

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0111](../../rfcs/RFC-BPR-0111-draft-publication-and-continuity.md), criteria `AC03`, `AC04`, `AC05`, `AC06`.

Publication journal integration, actual candidate/PR association, evidence/tour reconciliation and incomplete follow-up.

## Intended verification

Exercise lost answers, created-PR/failed-follow-up and remote drift; preserve private preparation and avoid duplicate creation.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

Owner amendment, 2026-11-12 ("Mockups; cut Evidence"): RFC-BPR-0109 is Deferred; reconciliation covers the tour only (RFC-BPR-0111 AC03 amended).

## Closeout (2026-11-13)

BPR-2029 is Done. Evidence from the RFC-BPR-0111 ledger on the lane `claude/v0-5-0-prerelease-targets-52265d` (`docs/verification/RFC-BPR-0111.md`), re-run at `0f444ac0a`:

- AC03 (amended 2026-11-12: tour anchors only): Pass. `core_local_draft_publication`: working files are never the pushed branch, base movement is said in the preview, a drifted pull request refuses the tour follow-up. `application_local_draft_publication_desktop_public_path`: the carried tour anchors at the pull request's head and at another head anchors nothing, the stop kept without lines.
- AC04: Pass. `local_draft_publication`: lost answers read for and never resent, a restart mid-creation recovered by reading, concurrent drift sends nothing (negative control fails without the re-read), an existing pull request associated without a creation; no confirmation survives a restart.
- AC05: Pass. A failed follow-up leaves the pull request associated and a new plan is refused as linked; an associated pull request's description is untouched; carrying the tour again creates nothing (`Creates` stays 2).
- AC06: Pass. Live #184 is an ordinary GitHub draft whose window holds the carried, Accepted stop; the draft record stays resumable after association; GHES and fork heads are stated as unsupported.
