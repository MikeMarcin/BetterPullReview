---
schema: "implement-rfc.card.v1"
id: "BPR-1804"
title: "Request reviewers from Overview's People section with a people search"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"feature","found_by":"owner chat, 2026-10-28","related_cards":["BPR-1774","BPR-1741","BPR-1405"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surface"}
acceptance: [{"criterion": "RR01 core transitions and decisions", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "ctest -R '^core_reviewer_request$'", "detail": "reviewer_rows puts matching suggestions first, each login once (ASCII case folded), never the author, at most 30, none without a node; normalize_reviewer_query drops spaces and one @ and cuts at a character boundary; a late search answer or failure is dropped by identity; choose refuses closed, busy, unknown and already requested and otherwise names one request (re-request for a reviewer); finish says requested, rerequested, already requested or not added from GitHub's list; decide_reviewer_write: write, already_requested, author. No Qt."}}, {"criterion": "RR02 fixed GraphQL documents, both servers and connections", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "ctest -R '^(github_mutation_contract|gh_cli_transport|github_read_contract)$'", "detail": "test_reviewers_go_by_graphql_exactly: ObserveReviewers and RequestReviews compile to exact bodies naming the accepted read's pull request node and one person, revalidate canonically, refuse a read passed off as the write and another person put in flight; unbound, foreign, other-actor, nameless, malformed-login, unnormalized, long, non-UTF-8 and person-carrying requests refused; GHES 3.18 at /api/graphql. Answers held to the designated pull request; suggestions (author's own left out), matches, user requests (a team request is not a person), reviews by known accounts read; a malformed login or more matches than asked are malformed; a write answer without the person is read for the core; requestReviews null is told apart. Every operation row and request-shape pin covered on both servers. gh_cli_transport: both documents launch gh api graphql with the exact body on stdin; a body naming another pull request never launches. RequestReviews reserved as a mutation name."}}, {"criterion": "RR03 written exactly once, never for an open request or the author", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "ctest -R '^(live_review_mutation_provider|application_review_mutation_application_host)$'", "detail": "test_a_review_request_is_observed_then_written_once: a search is one read; a request is observed for the person then written once; an open request and the author are not written; GitHub answering without the person is written once and returned for the core to say not added; a GraphQL refusal is refused_by_server, not read back; another pull request is refused before any write; a lost answer is read back once (confirmed or not_confirmed), never sent twice; the coordinator confirms the target before a request and not before a search. Host: refused before any effect without the capability, for a long query, a request without a login, or a grant issued for a publication; searches need no grant and the latest wins (the earlier one ends cancelled before the provider); a permitted request reaches the provider once."}}, {"criterion": "RR04 Qt picker: rows, standing, keyboard, accessibility, no effects", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "ctest -R '^qt_pull_request_overview_page$'", "detail": "test_reviewers_are_requested_from_people: Request review hidden until offered, named for what it does, pressing it only dispatches open; the picker opens focused in its search and says Searching; Suggested and Everyone who can review headings; rows in the core's order with accessible text naming the person and standing; Down and Up move past headings, Return chooses, typing dispatches the text; waiting, requested, not-added and no-match lines; Escape closes and reports it; a close the core asked for is not reported back; a withdrawn offer closes the picker."}}, {"criterion": "RR05 shipped composition course", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "ctest -R '^(application_reviewer_request_contract|application_review_round_trip_composition_contract|application_description_edit_contract)$'", "detail": "application_reviewer_request_contract runs --threaded-reply-probe --reviewer-request-probe on github-com and ghes-3.18 fixtures: REVIEWER_REQUEST_EVIDENCE:1111 (Request review offered in People, the picker lists the suggestion and two people and a typed query narrows to one after a debounced second search, a keyboard choice writes exactly once and says so, choosing them again sends nothing). The round-trip composition contract's application digests are resealed; the mutation-branch pin did not move."}}, {"criterion": "RR06 live course", "outcome": "pass", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "command", "command": "python3 tests/scripts/reviewer_live_course.py --tool BUILD/tests/github_contract/reaction_live_course --work DIR", "detail": "MikeMarcin/BPR-Test #141 (opened by the owner, closed, branch deleted), 2026-10-28, through the shipped review_mutation_session::reviewers over gh api: the owner's searches answered (one assignable person, the author, never a row); a request for MikeMarcinTest (not a collaborator) writes=1 and GitHub's answer left them out, core notice not_added; a request of the author refused_by_server writes=0. As MikeMarcinTest (GH_TOKEN in the child only): search answered; a request of itself was a GraphQL refusal, refused_by_server. No request left. REST X-RateLimit-Remaining 4996 to 4964. A successful added request could not be shown live: the owner is BPR-Test's only collaborator and every pull request's author (fixture evidence only). The native HTTPS connection was not run live."}}, {"criterion": "RR07 owner look", "outcome": "pending", "revision": "660a36ff25720e5c5610be265c1e15ba96ed5518", "evidence": {"kind": "observation", "detail": "Offscreen light renders (offered, searching, list with keyboard highlight, requested) are in the session scratchpad of 2026-10-28; no attended-review session was opened."}}]
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

## Built (2026-10-28)

- Core: `bpr/core/reviewer_request.hpp`.
- GitHub adapter: `ObserveReviewers` and `RequestReviews` in the mutation
  contract and `review_mutation_session::reviewers`; the GitHub CLI connection
  admits both through `gh api graphql`.
- Application: `review_reviewer_request`/`receipt`, the port's `reviewers`,
  the coordinator, the host's `reviewers` (searches without a grant, latest
  wins; requests under a one-shot `issue_reviewer` grant).
- Qt: `bpr/qt/reviewer_picker.hpp` and Request review in People.
- Composition: offer, core transitions, 250 ms typing debounce, host effects,
  refresh after a write; public-path fixture and `--reviewer-request-probe`.
- Docs: product contract "Requesting reviewers"; provider contract.
- Live: `tests/scripts/reviewer_live_course.py`, the live tool's
  `--reviewers` and `--reviewers-second`.

## Not done (follow-up, to be numbered by the coordinator)

- Removing a request. GraphQL has no removal; REST `DELETE
  requested_reviewers` takes a JSON body the native transport's
  `deleteResource` does not send.
- Teams as reviewers (`organization.teams(query:)` needs `read:org` and an
  organization repository; BPR-Test is a user repository).
- Re-request from a People roster row (the picker's Ask again covers it).
- A successful added request on a live repository with a second collaborator.

## Known gaps

- `test_network_isolation_manifest` also fails on main for
  `application_agent_chat_conversation_contract`,
  `application_description_edit_contract`,
  `application_comment_edit_contract` (no `WINDOWS_UNCONTAINED_TESTS` entry)
  and `gh_cli_git_smart_transport`; this card's row has its entry.
- `live_mutation_isolation` needs every test executable built; it was not
  run in a full tree here.
