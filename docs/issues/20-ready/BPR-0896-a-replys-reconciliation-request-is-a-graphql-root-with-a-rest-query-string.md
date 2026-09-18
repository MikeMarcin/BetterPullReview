---
schema: "implement-rfc.card.v1"
id: "BPR-0896"
title: "A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006 worker on BPR-0879, while adding the reply mutation's pull-request binding","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0896: A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover

## Defect

`compile_review_comment_observation_request` builds its reconciliation GET as
the mutation's own path plus `"?per_page=100"`. That is right for an ordinary
REST comment mutation, whose path is the comments collection.

**For a reply it is not.** A reply is a GraphQL mutation
(`addPullRequestReviewThreadReply`), so its path is the GraphQL root, and the
reconciliation request comes out as `GET /graphql?per_page=100` with an empty
body — a request no real server can answer meaningfully.

The consequence is that **a reply through the live session can never reconcile,
and therefore can never recover.** Reconciliation is how the session confirms
what the server actually recorded and how a restart resumes an interrupted
write. Both are unavailable for exactly one kind of mutation, and it is the one
a reviewer uses most.

This strongly suggests the shipped reply path has never been exercised end to
end against a real server: the first live reply would have produced a
nonsensical GET. It was found by reading, while [[BPR-0879]] was adding the
reply mutation's pull-request binding.

## Scope

- Give a GraphQL-rooted mutation its own reconciliation observation rather than
  deriving one from a REST path. A reply's natural observation is the thread it
  landed in, which [[BPR-0879]] already compiles and parses
  (`compile_thread_target_observation_request`, and the `pullRequest { ... }`
  selection now on the reply document) — reuse that rather than inventing a
  third shape.
- Check the same question for every other operation whose path is the GraphQL
  root. Resolve and unresolve reconcile through their own response; a reply is
  the one that goes back for a second look. Confirm that by reading, not by
  assuming this is the only case.
- Decide what a reply's restart recovery should do. `live_review_mutation_restart_recovery`
  passes today, which means it is not covering this.

## Acceptance

- A reply's reconciliation request is one a real server can answer, and the card
  names what it asks for.
- A contract row pins the reply's reconciliation shape, so a REST-derived path
  cannot come back.
- Restart recovery for an interrupted reply is either proven or recorded as
  deliberately unsupported with its reason.
- [[BPR-0879]]'s pull-request binding on the reply still holds.

## Verified survey, dsp-007 — and the title understates it

Everything above was written from a reading during other work. It has now been
checked against the code, and the defect is **worse than the card says**. The
card is otherwise accurate; the corrections are named rather than edited in, so
a later reader can see which claims were verified and which were revised.

**Correction 1, material: a live reply cannot be submitted at all.** The
sequence in `review_mutation_session::submit_and_reconcile`
(`src/adapters/github/src/github_mutation_contract.cpp:2008-`) is thread
observation (`:2109`), then a **baseline** `reconcile` (`:2116`) that
establishes `BaselineExactCommentIds` (`:2124`), and only then admission and
the POST. The baseline reconcile uses the same broken compiler. Against a real
server it fails there, so the POST is never reached. So the title's "can never
reconcile or recover" is too kind: the reply path is a **total loss** against a
real server, not a loss of its recovery properties. The reviewer's most-used
operation does not work at all outside fixtures. That is why this card is now
**P1**.

**Correction 2: the machinery the card names cannot be reused as-is.** The
scope says to reuse `compile_thread_target_observation_request` and the reply
document's `pullRequest { ... }` selection "rather than inventing a third
shape". Neither returns comment data. `observe_thread_document`
(`:119-122`) selects `id isResolved pullRequest{...}` and nothing else;
`parse_thread_target_observation` (`:1576-1613`) returns
`review_thread_target_observation { std::string Thread; bool Resolved; }`
(`github_mutation_contract.hpp:159-165`); the reply document's `pullRequest`
selection is on the created comment and is used only to bind the pull request.
A reply's reconciliation needs `review_mutation_server_reconciliation`
(`Projection` + `ExactReceipts`). Four concrete things are missing: the
thread's `comments(...)` selection, a type carrying a comment vector, a
GraphQL-envelope comment-list parser (`parse_review_comment_observations`
at `:1724-1775` is hard-wired to a REST `std::vector<comment_response_json>`),
and a signature bridge — though that last one exists already as
`thread_observation_subject` (`:1183-1194`), which fabricates a resolution
request from a reply.

**Correction 3, minor: there are three GraphQL-rooted operations, not two.**
`graphql_path()` is used at exactly three compile sites: the reply (`:1283`),
resolve/unresolve (`:1329`), and the thread observation (`:1347`). The card's
claim about resolve and unresolve holds — `resolve_thread` (`:1968-2006`)
parses its own response and returns, with no derived GET and no recovery
record. `observe_review_thread` is the third; it is an observation rather than
a mutation, so it needs nothing, but it should be named when the scope says
"every other operation whose path is the GraphQL root".

**Why no test caught it, and why none could.** All four
`live_review_mutation_restart_recovery` fixtures
(`tests/github_contract/live_review_mutation_provider_tests.cpp:1327-1410`)
call `request()` (`:131-152`), which never sets `Proposal.ReplyToThread` — they
are all ordinary REST root comments with REST array completions. The only reply
row in that suite, `test_a_reply_observes_its_thread_before_it_writes`
(`:1167-1186`), asserts a refusal after exactly one request and never reaches
`reconcile`. And even a reply restart fixture would have passed, because
`fake_transport` answers from a positional queue and ignores method and path.
That is now [[BPR-0904]], and it is the reason this card's third acceptance
point cannot be satisfied by adding one row in isolation.

**The egress validator cannot help.**
`validate_outbound_review_mutation_request` (`:1424-1462`, the
`observe_review_comments` branch at `:1455`) validates by recompiling the
canonical form and comparing, so it reproduces the same defect and the
malformed GET passes its own check. Also on [[BPR-0904]].

**Where the fix lives.** Entirely in `src/adapters/github/src/`
(`github_mutation_contract.{hpp,cpp}`, `live_review_mutation_provider.*`), with
rows in `tests/github_contract/`. No inward crossing:
`review_mutation_server_reconciliation` and `review_thread_target_observation`
are adapter-internal, and `application::review_mutation_port`
(`src/application/include/bpr/application/review_mutation_port.hpp:306`)
exposes only `submit_and_reconcile` and never names a path. One thing to check
while editing: `gh_cli_transport.cpp:1476-1479` deliberately refuses all four
GraphQL-rooted operations, so any new GraphQL-rooted observation is refused
there by design and the CLI transport path needs its own decision.

**Two server-behaviour questions this repository cannot answer by reading.**
Both need a schema or a safe live target, and both change the shape of the fix:

- does GraphQL `PullRequestReviewThread.comments` return fields sufficient for
  `comment_observation`'s exactness check (path, line, commit, author,
  `viewerDidAuthor`)?
- would replies returned by the REST `/comments` collection satisfy that same
  check, given they carry `in_reply_to_id` and a synthesized position?

If the first is yes, the fix is a thread-scoped GraphQL observation. If it is
no and the second is yes, the fix is to keep a REST reconciliation but derive
its path from the **thread's pull request** rather than from the mutation's own
GraphQL path. Answer them before building either.
