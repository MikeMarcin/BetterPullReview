---
schema: "implement-rfc.card.v1"
id: "BPR-0896"
title: "A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006 worker on BPR-0879, while adding the reply mutation's pull-request binding","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"AC1 a reply's reconciliation request is one a real server can answer, and the card names what it asks for","outcome":"pass","revision":"d8da166e","evidence":{"kind":"command","detail":"github_mutation_contract test_a_reply_is_reconciled_through_its_thread pins POST /graphql (GHES /api/graphql), four headers and the exact body {query: ObserveThreadComments document, variables:{thread}}; the document's selection is the dsp-008 field mapping against GitHub's published schema (fullDatabaseId BigInt string, thread diffSide, author ... on User). The card names the request. Limits: schema checked by reading only, no live server queried; the native HTTPS transport cannot yet carry any GraphQL review POST, carded as BPR-1018 (P1)."}},{"criterion":"AC2 a contract row pins the reply's reconciliation shape, so a REST-derived path cannot come back","outcome":"pass","revision":"d8da166e","evidence":{"kind":"command","detail":"ctest github_mutation_contract, live_review_mutation_provider and live_review_mutation_restart_recovery passed. The contract row makes GET /graphql?per_page=100 and both relabellings fail canonical validation, and the reply rows pin all six requests of the course. Negative control: with the REST-derived compile restored, all three rows failed at 'unscripted request #2: GET /graphql?per_page=100' (each new row alone exit 1). Restored byte for byte, they passed."}},{"criterion":"AC3 restart recovery for an interrupted reply is proven or recorded as deliberately unsupported with its reason","outcome":"pass","revision":"d8da166e","evidence":{"kind":"command","detail":"Proven under the BPR-0904 keyed transport (on main at 234a6f67), with full wire pins. The in-process row an_interrupted_reply_is_recovered_from_its_thread and the two-process SQLite course (--write-unresolved-reply / --recover-unresolved-reply in ctest live_review_mutation_restart_recovery) both passed. The restarted session asks only GET /user, GET pulls/1 and POST /graphql ObserveThreadComments, never the write. It confirms landed reply 45 past identical reply 44; an unlanded reply stays unresolved. Negative control: a thread read yielding no exact receipts failed only these courses and the identical-baseline course."}},{"criterion":"AC4 BPR-0879's pull-request binding on the reply still holds","outcome":"pass","revision":"d8da166e","evidence":{"kind":"command","detail":"a_reply_observes_its_thread_before_it_writes passes: a foreign thread is refused after one request. Every reply course begins with ObserveThread. The thread-comments parse refuses a thread naming another pull request as identity_mismatch (github_mutation_contract)."}},{"criterion":"Open point (a): nullable line/commit on outdated thread comments decided, documented and tested","outcome":"pass","revision":"d8da166e","evidence":{"kind":"observation","detail":"Decision: a tolerated non-match, not mapped to originalLine/originalCommit; the reasons are on the card and in thread_comment_observation. Rows: the contract row projects our outdated comment as a non-match, and the live course 'a reply into a thread whose root is outdated' is written and confirmed. Negative control: outdated-as-malformed failed both by name."}},{"criterion":"Open point (b): CLI transport decision for the GraphQL-rooted observation","outcome":"pass","revision":"d8da166e","evidence":{"kind":"observation","detail":"Decision: gh_cli_transport refuses observe_review_thread_comments with the other GraphQL-rooted shapes. It never writes a reply, and a reply attempt it recovers stays unresolved, never resubmitted. The gh_cli_transport row refuses all five shapes before launch. Negative control: dropping the refusal failed 'a GraphQL-rooted shape never launches the CLI'."}},{"criterion":"Focused verification, formatting and tidy on the candidate","outcome":"pass","revision":"d8da166e","evidence":{"kind":"command","detail":"ctest dev: github_mutation_contract, gh_cli_transport, live_review_mutation_provider, live_review_mutation_restart_recovery, live_review_submission_provider, live_review_submission_restart_recovery and github_provider passed 7/7. architecture_boundaries, architecture_boundary_contract, live_mutation_isolation and live_mutation_isolation_contract passed before a comment-only edit. format_check passed on the 7 touched C++ files. clang-tidy on the 6 touched TUs found 13 issues, all on lines blamed to earlier commits or in untouched headers. No composition contract pins the touched sources."}}]
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

## Implemented and closed, 2026-09-23

**Claim.** The claim at `a87c0c2a` belonged to an owner whose worktree was
abandoned with its work uncommitted. That work was recovered as a patch,
resolved against main, reviewed line by line rather than trusted, and
continued under the same claim. Product and tests landed in `d8da166e`; the
finding below is `7d95f10b`.

**What a reply's reconciliation asks for now.** `POST` to the GraphQL root
(`/graphql`, `/api/graphql` on GHES) with the reserved document
`query ObserveThreadComments($thread:ID!)`, variable `thread` set to the
reply's thread. It selects the thread's `id`, `diffSide` and
`pullRequest{id databaseId number repository{id databaseId nameWithOwner}}`,
and `comments(first:100)` with `pageInfo{hasNextPage}` and, per comment,
`id fullDatabaseId body path line commit{oid} viewerDidAuthor
author{login ... on User{databaseId id}}` — the dsp-008 mapping, including its
three traps. The answer reduces to the same projection and exact receipts a
REST comment list does. It must name the reply's thread and the designated
pull request; a further page is refused as a REST `Link` header is; more than
100 comments, a repeated identity, a side outside `LEFT`/`RIGHT` and a
`fullDatabaseId` spelled as a JSON number are all refused. A new root comment
still reconciles through the REST list. The four GraphQL-rooted operations
are now the reply, resolve/unresolve, `ObserveThread` and this read. The
first two reconcile through their own response, and the last two are reads
that need no reconciliation of their own.

**Open point (a), outdated comments: a tolerated non-match, not mapped.** A
comment with null `line` or `commit`, an author with no `User` fragment, or a
missing database identity is projected and is never an exact receipt; it
never aborts the reconciliation. A present field spelled outside its scalar's
vocabulary is still malformed. Null `line`/`commit` are not mapped to
`originalLine`/`originalCommit`. The reply's own answer already demands a live
line and commit for our comment, so demanding the same of a receipt is
consistent. The original position is stale against the anchor the app read.
The schema does not say what `originalCommit` means for a reply. The cost is
that a reply of ours that goes outdated before a later reconciliation stays
unresolved. That is the safe direction: a known attempt is never resubmitted,
and no stale position is confirmed as ours. Rows:
`github_mutation_contract` (`test_a_reply_is_reconciled_through_its_thread`:
our outdated comment and a bot's are projected non-matches, plus eight named
non-match and twenty-one named malformed spellings) and
`live_review_mutation_provider` course "a reply into a thread whose root is
outdated" (written and confirmed).

**Open point (b), the CLI transport: refuses the new read, deliberately.**
`gh_cli_transport` admits REST review and inline-comment shapes only, and
refuses `observe_review_thread_comments` with the other GraphQL-rooted shapes.
It never writes a reply: under it a reply stops at the [[BPR-0879]]
`ObserveThread` pre-flight, with nothing admitted or written, so its own
writes never need this read. The only way it meets one is recovering a reply
attempt written through another transport for the same scope. That recovery
is refused (`unavailable`) and stays unresolved rather than being
resubmitted. Admitting GraphQL to the CLI is [[BPR-1001]]'s kind of change,
not this card's. If that happens, question 2 above (would REST `/comments`
satisfy the check) is still unanswered and is not needed. Row:
`gh_cli_transport` refuses all five GraphQL-rooted shapes before launch.

**Third acceptance point, restart recovery: proven.** The [[BPR-0904]] gate
was that a fixture transport ignoring method and path would pass any reply
fixture. That transport is gone from this suite: the spike's keyed transport
is on main (`234a6f67`, `1aabc2f3`) and answers only the request each answer
is keyed to. The reply rows also pin every request's count, order, method,
path, operation label and GraphQL document kind and name. That pin holds
whichever way BPR-0904 finally chooses. The proof has two parts:

- `live_review_mutation_provider`,
  `an_interrupted_reply_is_recovered_from_its_thread`: the write's answer is
  lost (`indeterminate`), and the record is unresolved and possibly
  transmitted over its exact baseline. A fresh session asks only `GET /user`,
  `GET .../pulls/1` and `POST /graphql ObserveThreadComments`, never the
  write. It confirms the landed reply as a duplicate receipt. It leaves an
  unlanded one unresolved, with and without an identical earlier reply of
  ours in the baseline, which is never adopted.
- `live_review_mutation_restart_recovery` now also runs
  `--write-unresolved-reply` / `--recover-unresolved-reply`: two processes
  over the real SQLite recovery store. The second resolves reply 45 past
  identical reply 44 from the thread alone, and a repeat answers from the
  store without transport.

The fixture transport now witnesses a reply write as it witnesses a root
write, as the production transports witness every write POST.

**[[BPR-0879]]'s binding still holds.**
`a_reply_observes_its_thread_before_it_writes` passes. Every reply course
starts with `ObserveThread`. A thread answer naming another pull request is
an identity mismatch (`undesignated_target`).

**Verification on `d8da166e`** (dev preset, macOS). ctest
`github_mutation_contract`, `gh_cli_transport`,
`live_review_mutation_provider`, `live_review_mutation_restart_recovery`,
`live_review_submission_provider`, `live_review_submission_restart_recovery`
and `github_provider` passed, 7/7. The reply rows were also run alone with
`--only`, and the restart driver was run directly with a private `TMPDIR`
(the store refuses the session's symlinked `/var` default, which is
unrelated). `architecture_boundaries`, `architecture_boundary_contract`,
`live_mutation_isolation` and `live_mutation_isolation_contract` passed on
the same source before a comment-only edit. The format check passed on all
seven touched C++ files. clang-tidy on the six touched translation units
reported nothing on a changed line; its thirteen findings are all on
pre-existing lines or untouched headers. The touched sources are not pinned
by any composition contract, so nothing was resealed.

Negative controls, each a temporary product edit restored byte for byte:

- The REST-derived reconciliation turned all three rows red at
  `FAILED: unscripted request #2: GET /graphql?per_page=100`, and each new
  row alone exited 1.
- Outdated-as-malformed turned the contract row red, and the outdated-root
  course red by name.
- A thread read that yields no exact receipts turned only the restart courses
  and the identical-baseline course red; the plain write path stayed green.
- Dropping the CLI refusal turned "a GraphQL-rooted shape never launches the
  CLI" red.

**What this does not prove, and where it went.** Nothing ran against a real
server, and the document is checked against the published schema by reading
only. Reading the transports for (b) found that the native HTTPS transport
cannot carry any GraphQL review operation. Every review-mutation POST there
must be witnessed and answered 201, the session witnesses none of its
GraphQL reads, and GraphQL answers 200. So a live reply still stops at the
[[BPR-0879]] pre-flight before anything is written. That is
[[BPR-1018]] (P1), a transport defect separate from this card's request
shape.
