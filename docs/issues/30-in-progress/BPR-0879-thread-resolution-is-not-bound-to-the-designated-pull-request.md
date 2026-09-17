---
schema: "implement-rfc.card.v1"
id: "BPR-0879"
title: "Thread resolve and unresolve are not bound to the designated pull request by any server observation"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review: independently by the Claude/fable github+git partition and by Codex, which filed it as its only blocking finding","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0879: Thread resolve and unresolve are not bound to the designated pull request by any server observation

## Defect

**Two independent critical reviews found this separately, and Codex filed it as
its only blocking finding.** That convergence is why this is P1: a thread node
ID is the one free identifier in the product that reaches a mutation, and
nothing on the wire or in the response ties it to the pull request the
reviewer is looking at.

The chain, with what each step does and does not check:

- `src/application/src/review_mutation_coordinator.cpp:99` forwards any
  non-empty thread identifier straight to the provider **without**
  `confirm_target()`, unlike ordinary publication at `:46` and `:151`.
- `src/adapters/github/src/github_mutation_contract.cpp:1204-1234`
  (`compile_thread_resolution_request`) checks the *request record's* target
  and actor, but validates `aRequest.Thread` only with `graphql_token` — that
  is, for syntax.
- The mutation itself binds only the thread:
  `resolveReviewThread(input:{threadId:$thread}){thread{id isResolved}}`
  (`:100-107`).
- `parse_thread_resolution_response` (`:1444-1472`) compares only `thread.id`
  and `isResolved`, so the response cannot catch what the request did not
  bind.
- `review_mutation_session::resolve_thread` (`:1780-1815`) makes no target
  observation.

So any thread the acting account may mutate — in another pull request, or
another repository entirely — would be resolved or unresolved with
`undesignated_target` never raised. The whole boundary rests on the caller,
and the caller derives `Thread` from a gesture identity string
(`src/app/src/github_application.cpp:8179`). Stale or mis-correlated
presentation state is enough; no hostile input is required.

This is an **inconsistent** authority boundary rather than a uniformly weak
one, which is what makes it worth fixing rather than accepting: comment
replies are bound by commit, path, line, body and author
(`github_mutation_contract.cpp:1000-1027`), and whole-review publication
revalidates immediately. Resolution is the one write that does not.

The mutation is idempotent and reversible, so this is a boundary gap rather
than a data-loss risk. It still writes to somebody's pull request.

`tests/github_contract/live_review_mutation_provider_tests.cpp` has no
foreign-thread case; grep for `resolve_thread|undesignated|foreign` finds
nothing.

## Scope

- Ask the server to identify what it is about to change, and refuse a
  mismatch. The reviewer's concrete suggestion: request
  `thread { id isResolved pullRequest { number repository { owner { login } name } } }`
  and compare against `Configuration.Target`, raising `undesignated_target` on
  any difference. Do the same for the reply mutation's
  `pullRequestReviewThreadId`, whose response binding is strong but is not a
  pull-request binding.
- Route resolution through `confirm_target()` like the other writes, so the
  coordinator applies one rule rather than two.
- Add the negative contract row: a thread ID belonging to another pull request
  must be refused, not resolved.
- **Also record the resolution in the coordinator's statistics.** Codex's
  second finding: the resolution path increments only `Attempts` and calls the
  provider directly, so diagnostics can report zero submissions after a real
  resolve or unresolve POST. That is exactly the wrong property for
  reconstructing an accidental write, and it is the same few lines.

## Acceptance

- A resolve or unresolve of a thread outside the designated pull request is
  refused with `undesignated_target`, proven by a contract row.
- The designated case still resolves and unresolves, on a recorded or
  designated safe target only.
- The coordinator's submission statistics count resolution writes.
- The card records whether any real pull request was ever mutated across this
  gap, or states that it cannot be determined.

## Notes (2026-09-17, dsp-006)

- **The reviewer's suggestion is a detection, not a refusal.** Binding
  `resolveReviewThread`'s response fires after the write has already
  happened. So the server is asked *first*: a
  `node(id:$thread){... on PullRequestReviewThread{id isResolved
  pullRequest{id databaseId number repository{id databaseId
  nameWithOwner}}}}` read, compiled by
  `compile_thread_target_observation_request` and checked by
  `parse_thread_target_observation` against `Configuration.Target`. Any
  difference is `identity_mismatch`, which the adapter already maps to
  `undesignated_target`. A thread cannot migrate between pull requests, so
  the extra round trip races with nothing. The observation is the mutation's
  own compiled request with a read document in its place, so it inherits
  every guard the write has and a request the contract would refuse to write
  is also refused to observe.
- **The response binding is kept as well**, since the fields are free in the
  same mutation: `resolve`, `unresolve` and the reply document all select the
  pull request, and `parse_thread_resolution_response` and
  `reply_observation` compare it. It cannot refuse a write that already
  happened; it catches what the pre-flight could not see.
- **Node and database identities, not just owner/name/number.** A repository
  rename moves `nameWithOwner`; `id` and `databaseId` do not move. All six
  fields are compared.
- **One rule in the coordinator.** `resolve_thread` now routes through
  `confirm_target()` like publication. Once, not twice: publication observes
  again immediately before submitting because a proposal is prepared against
  a revision, and a thread belongs to a pull request rather than to a commit.
  For the same reason a moved head is acknowledged rather than refused —
  refusing would make an ordinary push block resolving a thread, and a push
  cannot change which pull request a thread is in. `target_changed` is not
  overridable at any level and still refuses.
- **Statistics.** The resolution path increments `Submissions` beside
  `Attempts`. It counts the hand-off to the provider, so it is an upper bound
  on writes, never an under-count — which is the property diagnostics need
  when reconstructing an accidental write.
- **The reply too.** `addPullRequestReviewThreadReply` is addressed by the
  same free thread identity, so `submit_and_reconcile` performs the same
  pre-flight when the proposal names a thread. It runs before the recovery
  record is admitted, so a refusal leaves no prepared row behind to poison
  the idempotency key.

### Was any real pull request ever mutated across this gap?

**No, on the evidence in this tree — and the reason is the gate, not luck.**

- Automated tests run network-denied by default (BPR-0235), and every
  tests-enabled composition binds the *fixture* mutation provider
  (`github_application.cpp`, `BPR_BUILD_TESTING` branch of
  `compose_review_mutation`), never the live one. `issue_thread` returns an
  empty grant under `BPR_APPLICATION_TESTING`. No resolve in any test has
  ever reached a socket.
- The one lane permitted to write to a real pull request is the attended
  designated-live lane. It is gated twice: the launch-time
  `designated_live_mutation_gate` proves the pull request's identity field by
  field before the application starts, and inside the application
  `AttendedReviewMutationMode` selects `select_designated_review_mutation`,
  which refuses unless hostname, owner, name, repository id, pull-request id,
  number, both node ids, base and head OIDs and both refs equal compile-time
  constants (`MikeMarcin/BPR-Test#1`). If they do not match, no selection is
  made, so no provider, no mutation host and no resolve path exist at all.
- In that lane the conversation projection is built only from reads of that
  same pull request, and `compose_review_mutation` retires the host and
  clears `ReviewProjection` before composing another. So the `review-thread:`
  identities offered to Resolve came from the designated pull request.
- BPR-0657, which introduced resolve, records only fixture and contract
  evidence and states "no live mutation outside the designated test
  repository". There are no release tags in this repository.

The gap was real and worth closing: nothing in the *code* bound the thread,
and stale presentation state alone was enough. What kept it theoretical is
that the only lane that can reach a live server is pinned to one disposable
pull request, whose threads are the only ones that lane can name.

Residual honesty: this rules out writes by this project's own lanes. It
cannot rule out a developer running a non-attended release-configuration
build against their own account, where `select_observed_review_mutation`
composes the live provider for whatever pull request is open. No such build
is tagged or recorded here.

### The statistics assertion, and the guard it had to satisfy

`review_mutation_coordinator::resolve_thread` counts the write, and a row
asserts it. Getting there took two attempts, and the second is the point.

The first double reached the session through
`make_live_review_mutation_provider`, and `live_mutation_isolation` refused
the binary: a non-production executable may not contain a
`live_review_mutation_provider` symbol. That guard was right. The double now
forwards to `review_mutation_session` instead — the exact production protocol
without the concrete provider — and
`scripts/verify_live_mutation_isolation.py` carries
`tests/github_contract/live_review_mutation_provider_tests.cpp` in
`PORT_IMPLEMENTATION_ALLOWLIST` so the double is declared rather than merely
unnoticed. The gate passes with the entry in place, which is what makes the
entry honest: the symbol clause still covers that binary and did not fire.

The row drives a `review_mutation_coordinator` over a `fake_transport` primed
with the target, viewer, thread-observation and resolution answers, and pins
`Attempts == 1`, `TargetObservations == 1`, `Submissions == 1`,
`RefusedBeforeSubmit == 0`. A drifted target pins the other side:
`Submissions == 0`, `RefusedBeforeSubmit == 1`, one wire request, no write.

### End-to-end

`application_threaded_reply_contract` passes (22.7s) with the coordinator's
extra target confirmation on the resolve path, so the recorded course that
presses Resolve on a thread card is unaffected by it. The composition
contracts (`application_composition_contract`,
`application_review_round_trip_composition_contract`) pass unchanged.
