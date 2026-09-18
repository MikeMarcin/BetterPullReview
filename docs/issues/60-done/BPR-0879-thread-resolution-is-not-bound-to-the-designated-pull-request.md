---
schema: "implement-rfc.card.v1"
id: "BPR-0879"
title: "Thread resolve and unresolve are not bound to the designated pull request by any server observation"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review: independently by the Claude/fable github+git partition and by Codex, which filed it as its only blocking finding","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"A resolve or unresolve of a thread outside the designated pull request is refused with undesignated_target, proven by a contract row","outcome":"pass","revision":"e9c9d5c68eef7512bde19a3e6f941ad9451c5d3a","evidence":{"kind":"command","detail":"Refused BEFORE the write, not detected after it: review_mutation_session::resolve_thread performs a pre-flight node(id:$thread){... on PullRequestReviewThread{pullRequest{...}}} read, compiled from the mutation's own request so it inherits every guard the write has, and refuses undesignated_target. New rows in tests/github_contract/live_review_mutation_provider_tests.cpp: 'a thread outside the designated pull request is refused before any write' asserts exactly one wire request and that the one request is the observation, for both resolve and unresolve; 'a foreign thread is named undesignated_target at the coordinator and never written' proves the refusal reaches the coordinator's return value. Contract-level rows in github_mutation_contract_tests.cpp cover inside/outside/wrong-thread/non-thread-node/invisible-node/non-200/unbound. live_review_mutation_provider Passed 0.42 s; github_mutation_contract Passed 0.07 s (build/r2-logs/dsp6-0879-test-stats.log, dsp6-0879-test-composition.log). Identity is compared on node ids and database ids as well as owner/name/number, because a rename moves nameWithOwner and the ids do not. The reviewer's response-side binding is kept as a second check since the fields are free in the same round trip."}},{"criterion":"The designated case still resolves and unresolves, on a recorded or designated safe target only","outcome":"pass","revision":"e9c9d5c68eef7512bde19a3e6f941ad9451c5d3a","evidence":{"kind":"command","detail":"'the coordinator observes the target, resolves, and counts the write as a submission' plus the both-directions row. End to end: application_threaded_reply_contract Passed 22.72 s, which does press Resolve on a thread card, so the added target confirmation is proven not to disturb the real course rather than only argued (build/r2-logs/dsp6-0879-test-e2e.log). application_composition_contract Passed 154.54 s and application_review_round_trip_composition_contract Passed 11.25 s. No live GitHub or GHES contact at any point; fixtures and contract doubles only."}},{"criterion":"The coordinator's submission statistics count resolution writes","outcome":"pass","revision":"e9c9d5c68eef7512bde19a3e6f941ad9451c5d3a","evidence":{"kind":"command","detail":"Both sides pinned by assertion, not just the increment: Attempts==1, TargetObservations==1, Submissions==1, RefusedBeforeSubmit==0 on a real review_mutation_session; and the drifted-target companion asserting Submissions==0, RefusedBeforeSubmit==1 with one wire request and no write. The increment sits immediately before the request is built, which is exactly where publication's own ++Statistics.Submissions sits at review_mutation_coordinator.cpp:203, so the two paths now count alike. The row needed one PORT_IMPLEMENTATION_ALLOWLIST entry in scripts/verify_live_mutation_isolation.py for a double forwarding to review_mutation_session; the guard correctly refused a first attempt built on make_live_review_mutation_provider, which would have dragged FORBIDDEN_SYMBOL into a test binary. live_mutation_isolation Passed 91.27 s with the entry in place, so the symbol clause is still doing its job."}},{"criterion":"The card records whether any real pull request was ever mutated across this gap, or states that it cannot be determined","outcome":"pass","revision":"e9c9d5c68eef7512bde19a3e6f941ad9451c5d3a","evidence":{"kind":"observation","detail":"Answered no, from the code rather than from luck, and the residual is stated. Every tests-enabled composition binds the fixture provider and never the live one, and tests run network-denied by default, so no resolve in any test reached a socket. The one lane that may write to a real pull request is gated twice: designated_live_mutation_gate proves the pull request field by field before launch, and select_designated_review_mutation refuses unless hostname, owner, name, repository id, pull-request id, number, both node ids, both OIDs and both refs equal compile-time constants, so on a mismatch no provider and no mutation host exist at all. BPR-0657, which introduced resolve, records only fixture and contract evidence, and there are no release tags in this repository. Residual recorded and carded: this rules out writes by this project's lanes and cannot rule out a non-attended release-configuration build, where select_observed_review_mutation composes the live provider for whatever pull request is open. That is BPR-0897, parked for the owner."}}]
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
