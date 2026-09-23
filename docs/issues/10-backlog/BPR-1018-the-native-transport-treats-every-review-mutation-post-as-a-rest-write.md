---
schema: "implement-rfc.card.v1"
id: "BPR-1018"
title: "The native transport treats every review-mutation POST as a REST write, so no GraphQL review operation can complete over it"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0896 implementer, reading the transports while settling BPR-0896's CLI-transport decision","found_at":"d8da166e"}
---

# BPR-1018: The native transport treats every review-mutation POST as a REST write, so no GraphQL review operation can complete over it

## Defect

Found by reading, not by any test, while [[BPR-0896]] settled which transport
can carry its new thread observation. Nothing here was run against a server.

The native HTTPS transport handles every POST on the review-mutation surface
as if it were the REST comment write. Two rules follow from that, and each on
its own stops every GraphQL-rooted review operation:

1. **Every review-mutation POST demands a transmission witness.**
   `execute_hop` (`src/adapters/github/src/native_https_transport.cpp:3609-3622`)
   calls the operation's witness for any review-mutation POST and aborts the
   hop when it returns false. `perform_review_mutation` (`:2530`) wraps the
   caller's witness so that an absent one returns false, and the provider's
   account-checking wrapper (`src/adapters/github/src/github_provider.cpp:853`)
   does the same. The session supplies no witness for its GraphQL reads or for
   a resolution: `observe_thread_target` (ObserveThread,
   `src/adapters/github/src/github_mutation_contract.cpp:2169`), `reconcile`
   (ObserveThreadComments for a reply since [[BPR-0896]], `:2193`) and
   `resolve_thread` (`:2278`). Each is aborted after the POST is handed to
   Qt and before any answer is read, and reported `unavailable` with
   transmission `indeterminate`.
2. **A review-mutation POST succeeds only with HTTP 201.**
   `successfulStatus` (`:3754-3761`) is 201 for any review-mutation POST.
   GitHub's GraphQL endpoint answers 200. So the one GraphQL write that does
   carry a witness, the reply (`ReplyToThread`), would be executed by the
   server and then classified as an HTTP failure.

The consequence: over native HTTPS a reply stops at the [[BPR-0879]]
thread pre-flight with nothing written, and resolve and unresolve stop at the
same pre-flight. The gh CLI transport refuses every GraphQL-rooted operation
by design (the [[BPR-0896]] decision records why), so **no transport in the
product can reply to, resolve or unresolve a thread on a real server.**
[[BPR-0896]] made the reply's reconciliation a request a server can answer;
this card is why the product still cannot send it.

Nothing caught this because no native fixture sends a GraphQL review POST.
The only mutation scenarios (`mutation_witness`, `mutation_witness_failure`,
`mutation_proxy_no_fallback`, declared at
`tests/github_contract/native_https_transport_tests.cpp:958-960` and run at
`:1986-2025`) send the REST comment POST and expect 201. The session suites
use a fixture transport, so they never reach this code.

A smaller point to settle at the same time: `native_request_from` for a
mutation (`native_https_transport.cpp:504-517`) accounts every review
mutation to the `core` rate-limit resource, including the GraphQL ones.

## Scope

- Distinguish, in the native transport, a GraphQL review read
  (`observe_review_thread`, `observe_review_thread_comments`) from a write,
  and a GraphQL write (`reply_review_thread`, `resolve_review_thread`,
  `unresolve_review_thread`) from the REST one. Derive the distinction from
  `review_mutation_operation`, not from the path, so a new operation is
  visibly unclassified.
- Decide the witness rule per class. The session witnesses a reply and never
  a read or a resolution; say whether a resolution should be witnessed, since
  it keeps no recovery record.
- Decide the success status per class: 201 for the REST comment write, 200 for
  every GraphQL operation, and nothing else admitted.
- Settle the rate-limit resource for the GraphQL operations.

## Acceptance

- A native loopback fixture course completes ObserveThread,
  ObserveThreadComments, ReplyToThread, ResolveThread and UnresolveThread
  with 200 answers, demanding a witness exactly where the session supplies
  one.
- The REST comment POST still requires 201 and its witness, and the existing
  mutation scenarios still pass.
- A 201 to a GraphQL operation and a 200 to the REST comment write are both
  refused.
- [[BPR-0896]]'s reply course, as pinned in
  `tests/github_contract/live_review_mutation_provider_tests.cpp`
  (`reply_course_wires`), is shown to pass through the native transport
  fixture, not only through the session suite's fixture transport.
