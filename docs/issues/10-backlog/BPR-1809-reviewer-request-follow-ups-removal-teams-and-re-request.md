---
schema: "implement-rfc.card.v1"
id: "BPR-1809"
title: "Reviewer request follow-ups: remove a request, team reviewers, re-request from People"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1804"]
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1809--BPR-1809--e7083e444896.json","kind":"feature","found_by":"BPR-1804 Not done, 2026-10-28","related_cards":["BPR-1804","BPR-1405","BPR-1589"]}
acceptance: []
---

# BPR-1809: Reviewer request follow-ups: remove a request, team reviewers, re-request from People

## Request

[[BPR-1804]] put Request review in Overview's People section. Its card
lists four things it left out. This card holds them so they are not lost.
Each one can ship as its own slice.

## Before this card

- People requests one person through the core's reviewer picker
  (`bpr/core/reviewer_request.hpp`). The write is the GraphQL
  `RequestReviews` mutation, which observes first and is sent at most once. A
  re-request is the picker's Ask again on a person whose review is in.
- Nothing removes a request. GraphQL has no mutation that removes a review
  request. REST `DELETE /repos/{owner}/{repo}/pulls/{n}/requested_reviewers`
  takes its reviewers in a JSON body. The native HTTPS connection sends a
  delete through `QNetworkAccessManager::deleteResource`
  (`native_https_transport.cpp`), which sends no body.
- The picker lists users only. `ObserveReviewers` reads `suggestedReviewers`
  and `assignableUsers`. A team in `reviewRequests` is not shown as a person.
- People's roster rows are read-only.
- The live course (`tests/scripts/reviewer_live_course.py`, BPR-Test #141)
  never showed a request that GitHub actually added. The owner is
  BPR-Test's only collaborator and authors every pull request, and
  `MikeMarcinTest` is not a collaborator, so GitHub dropped them.

## Scope

1. **Remove a requested reviewer.** An open request in People, or in the
   picker, can be withdrawn. Use REST `DELETE requested_reviewers` with a
   `{"reviewers":[login]}` body on both connections. The native connection
   needs a delete that carries a body (`sendCustomRequest("DELETE", body)`
   or something equivalent). The GitHub CLI connection needs
   `gh api --method DELETE --input -`. The write follows the request's rules:
   observe first, a one-shot grant bound to the exact person, at most one
   send, and a lost answer settled by a single read back. The core decides
   whether a removal is needed, which means a request must be open.
2. **Team reviewers.** The picker lists the teams that can be asked
   (`organization.teams(query:)`) and requests them through `requestReviews`
   `teamIds`. This needs the `read:org` scope and an organization
   repository. Without that scope, the picker says teams are unavailable
   and lists no teams. People shows open team requests as teams.
3. **Re-request from a People roster row (optional).** A reviewer row whose
   review is in offers Ask again, with the same transition and write as the
   picker's. Leave it out if the owner finds the picker enough.
4. **Live successful request (optional).** Run the live course on a
   repository that has a second collaborator, and show a request that GitHub
   adds, then removes (item 1). BPR-Test cannot show this unless the owner
   adds `MikeMarcinTest` as a collaborator. Agents must not change
   collaborators, so this waits on the owner.

## Acceptance

- RF01 core: removal decision and transitions, and team rows and standing,
  without Qt.
- RF02 GitHub adapter: the exact REST removal request with its body on both
  connections and both servers (github.com, GHES 3.18), held to the
  designated pull request. The fixed GraphQL documents for teams.
- RF03 a removal is written exactly once and never when no request is open.
  A lost answer is read back once.
- RF04 Qt: Remove on an open request, team rows, keyboard and accessible
  names. No effects.
- RF05 composition course over the public-path fixture
  (`--reviewer-request-probe` or a sibling probe).
- RF06 live course on MikeMarcin/BPR-Test for removal. Teams and the
  successful added request need an organization repository and a second
  collaborator. Until those exist, these items stay pending and the reason
  is recorded.
- RF07 owner look at the changed People and picker surfaces.

## Notes

- Load `frontend-design:frontend-design` before the Qt work.
- Reseal `application_review_round_trip_composition_contract`'s application
  digests when `github_application.cpp` changes. The mutation-branch pin
  moves only if the review mutation composition changes.
