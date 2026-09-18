---
schema: "implement-rfc.card.v1"
id: "BPR-0896"
title: "A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover"
status: "In Progress"
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

## Both questions answered from GitHub's published schema, dsp-008 — the answer is yes, and the shape is settled

Answered read-only from the published public schema
(`https://docs.github.com/public/fpt/schema.docs.graphql`, fetched
2026-09-17), against `comment_observation`
(`src/adapters/github/src/github_mutation_contract.cpp:1045-1076`) field by
field. No live GitHub or GHES request was made, and none is needed: **question
1 is yes, so question 2 is moot for choosing the shape.** The fix is a
thread-scoped GraphQL observation. Nobody has to wait for the owner or for a
named safe target to start it.

`PullRequestReviewThread.comments(first/last/after/before/skip)` returns
`PullRequestReviewCommentConnection!`, and `PullRequestReviewComment` carries
every field the exactness check consumes. The mapping, which is the part worth
having:

| `comment_observation` reads | GraphQL source |
|---|---|
| `aResponse.Id` (`std::uint64_t`, must be nonzero) | **`fullDatabaseId: BigInt`, not `databaseId: Int`** |
| `aResponse.NodeId` | `id: ID!` |
| `aResponse.CommitId` → `core::parse_git_commit_oid` | `commit { oid }`, `GitObjectID` |
| `aResponse.Line` (1-based, nonzero) | `line: Int` |
| `aResponse.Path` | `path: String!` |
| `aResponse.Side`, compared to `"LEFT"`/`"RIGHT"` | **not on the comment** — `PullRequestReviewThread.diffSide: DiffSide!` |
| `aResponse.Body` | `body: String!` |
| `aResponse.User.Id` | `author { ... on User { databaseId } }` |
| `aResponse.User.NodeId` | `author { ... on User { id } }` |
| `aResponse.User.Login` | `author { login }` |
| `viewerDidAuthor` (named in the question) | `viewerDidAuthor: Boolean!`, present |

**Three of those rows are not what an implementer would guess, and each one
would have been found only after building the wrong thing.**

1. **Use `fullDatabaseId`, not `databaseId`.** `databaseId: Int` is a GraphQL
   32-bit signed `Int`; the schema documents `BigInt` as "Since the value may
   exceed the size of a 32-bit integer, it's encoded as a string". The check
   stores it in a `std::uint64_t` and refuses zero, so the parser must read a
   **string-encoded** integer here, not a JSON number. This is the opposite of
   the REST shape's `id`.
2. **The side comes from the thread, not the comment.** `PullRequestReviewComment`
   has no side or `diffSide` field at all. The enclosing thread has
   `diffSide: DiffSide!` (and `startDiffSide: DiffSide`), and `enum DiffSide`
   is spelled exactly `LEFT` and `RIGHT` — the same two strings the check
   compares against. This costs nothing here because the thread is already the
   object being observed, but a fix written from the comment's fields alone
   cannot satisfy the check.
3. **The author's ids need an inline fragment.** `author: Actor`, and
   `interface Actor` exposes only `avatarUrl`, `login`, `resourcePath` and
   `url`. `login` is reachable directly; `databaseId` and `id` are not, and
   require `... on User { databaseId id }`. An author that is a Bot,
   Mannequin or Organization satisfies neither fragment, and the check demands
   a nonzero `User.Id`.

### The constraint that actually shapes the fix, found while checking the above

`parse_review_comment_observations` (`:1738-1753`) treats a comment that fails
the exactness check in two different ways. `identity_mismatch` is tolerated —
the comment is projected and simply not counted as an exact receipt. **Every
other failure aborts the whole reconciliation.** And `comment_observation`
returns `malformed_response`, not `identity_mismatch`, when `Id == 0`,
`User.Id == 0`, the commit fails to parse, or `Line == 0`.

In the REST shape that rarely bites: GitHub always sends a numeric line and a
`commit_id`. **In the GraphQL shape it does.** `line: Int` is genuinely
nullable — it is null for an outdated comment, where `originalLine` carries
the value — and `commit: Commit` is nullable too, with `originalCommit`
beside it. So a thread containing one outdated comment would abort the
reconciliation with `malformed_response`, and that thread is exactly what a
baseline observation walks *before* the reply is written
(`submit_and_reconcile` `:2116`).

That is a decision the fix has to make deliberately rather than discover:
either map null `line`/`commit` to `originalLine`/`originalCommit`, or treat
an outdated comment as a tolerated non-match rather than a malformed response.
Whichever is chosen, it needs a row, because a fixture author writing a happy
thread will never produce the case.

### What remains open on this card, and what does not

- **Not open:** which shape to build. Thread-scoped GraphQL observation.
- **Not open:** whether to wait for the owner. The blocking question is
  answered from published documentation, which is what the card asked for.
- **Still open, and unchanged:** the CLI transport decision. `gh_cli_transport.cpp:1476-1479`
  refuses all four GraphQL-rooted operations by design, so a new GraphQL-rooted
  observation is refused there and that path needs its own answer.
- **Still open:** the third acceptance point (restart recovery for an
  interrupted reply), which [[BPR-0904]] gates — a reply restart fixture
  passes today under a transport that ignores method and path.
- **Question 2 is moot but not refuted.** Nothing here establishes whether the
  REST `/comments` collection would satisfy the check; it was not pursued once
  question 1 came back yes. If the CLI-transport decision forces a REST
  fallback, that question becomes live again and is unanswered.
