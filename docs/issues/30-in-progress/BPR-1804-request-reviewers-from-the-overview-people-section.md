---
schema: "implement-rfc.card.v1"
id: "BPR-1804"
title: "Request reviewers from Overview's People section with a people search"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"feature","found_by":"owner chat, 2026-10-28","related_cards":["BPR-1774","BPR-1741","BPR-1405"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surface"}
acceptance: []
---

# BPR-1804: Request reviewers from Overview's People section with a people search

## Request

The owner, 2026-10-28, verbatim:

> Can I add a review from the people section of the overview page? The
> website gives a nice people search for that

## Before this card

- People (Overview's Status drawer) shows faces, a summary line, the
  participants and the open review requests ([[BPR-1405]], [[BPR-1589]]),
  read-only. The read model carries `requested_reviewers` and GraphQL
  `reviewRequests`.
- No source requested, re-requested or removed a reviewer: no
  `requestReviews`, `rerequestReviews`, `suggestedReviewers`,
  `assignableUsers` or `requested_reviewers` write anywhere in `src/`, and no
  people search. Nothing hidden or partial existed.

## Outcome

- People has **Request review**. It opens a searchable people picker under
  it: GitHub's suggested reviewers first, then the people GitHub lets be
  asked on this repository (`assignableUsers(query:)`), filtered as typed.
  Each row says whether a request is open, a review is in (choosing asks
  again), or neither. The author is never listed.
- Choosing a person asks GitHub once; the row and a line in the picker say
  what GitHub answered, and People refreshes through the ordinary read.
- Keyboard: Up and Down move through the list from the search field, Return
  chooses, Escape closes. Every row has an accessible name with its standing.

## Design

- Core (`bpr/core/reviewer_request.hpp`): the picker state, its transitions
  (open, type, accept or fail a search, choose, finish or fail a request,
  close), search and request identities so a late answer is dropped, the row
  order, and the provider's pre-write decision (`decide_reviewer_write`).
- GitHub adapter: one GraphQL read `ObserveReviewers` (the pull request by
  its node, author, `suggestedReviewers`, `repository.assignableUsers(query,
  first:20)`, `reviewRequests`, `latestReviews`) and one write
  `RequestReviews` (`requestReviews`, one user, `union:true`). Both work on
  github.com and GHES 3.18 (the recorded schemas have every field). The write
  observes first; GitHub's own answer must list the person, because
  `requestReviews` answers success and silently drops someone who may not
  review (measured on BPR-Test #140, 2026-10-28). A lost answer is settled by
  one read back, never sent twice.
- Application: the review mutation port, coordinator and host carry a
  reviewer operation; searches are reads with no grant and the latest one
  wins, requests take a one-shot grant bound to the exact person.
- Qt: a button and popup in People; no effects.
- Composition: debounces typing (250 ms), performs the core's effects, and
  presents the state.

## Acceptance

- RR01 core transitions and decisions, without Qt.
- RR02 fixed GraphQL documents on both servers and both connections, held to
  the designated pull request.
- RR03 a request is written exactly once, never for an open request or the
  author; a dropped person is said, not claimed.
- RR04 Qt picker: rows, standing, keyboard and accessibility; no effects.
- RR05 composition: the button reaches the host, a search lists people, a
  choice writes once.
- RR06 live course on MikeMarcin/BPR-Test.
- RR07 owner look (pending until the owner looks).

## Out of scope here (follow-up card)

- Removing a request (GraphQL has no removal; REST `DELETE
  requested_reviewers` needs a body the native transport does not send yet).
- Teams as reviewers (needs `read:org` and an organization repository).
