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
