---
schema: "implement-rfc.card.v1"
id: "BPR-1018"
title: "The native transport treats every review-mutation POST as a REST write, so no GraphQL review operation can complete over it"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0896 implementer, reading the transports while settling BPR-0896's CLI-transport decision","found_at":"d8da166e"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"210e68b5","evidence":{"kind":"command","detail":"native_https_transport_fixture scenario mutation-graphql-course: the production review_mutation_session runs the reply course (ObserveThread, ObserveThreadComments, GET /user, GET pulls/17, ReplyToThread, ObserveThreadComments), then ResolveThread and UnresolveThread after their thread pre-flights, over the native transport to the loopback CONNECT/TLS fixture at the GitHub.com authority; every answer 200 with the graphql or core rate-limit tuple. The witness is supplied only for the reply and consulted exactly once; reads and resolutions complete unwitnessed. Failing before at dbaf7040 (child exits at the first GraphQL read); marking the reply unwitnessed fails it.","path":"docs/issues/60-done/BPR-1018-the-native-transport-treats-every-review-mutation-post-as-a-rest-write.md#evidence"}},{"criterion":"AC2","outcome":"pass","revision":"210e68b5","evidence":{"kind":"command","detail":"mutation-witness (REST comment POST answered 201, one witness) and mutation-witness-failure pass before and after; mutation-status-refusal refuses a 200 answer to the REST comment write after one witness; native_https_transport_fixture passes all scenarios.","path":"docs/issues/60-done/BPR-1018-the-native-transport-treats-every-review-mutation-post-as-a-rest-write.md#evidence"}},{"criterion":"AC3","outcome":"pass","revision":"210e68b5","evidence":{"kind":"command","detail":"mutation-status-refusal refuses, each as unavailable with transmission indeterminate: a 200 to the REST comment write, and a 201 to the review submission, the reply, a resolution and a thread read. Fails before (the unfixed transport accepts the 201 to the review). mutation-graphql-core-resource refuses a GraphQL answer accounted to core.","path":"docs/issues/60-done/BPR-1018-the-native-transport-treats-every-review-mutation-post-as-a-rest-write.md#evidence"}},{"criterion":"AC4","outcome":"pass","revision":"210e68b5","evidence":{"kind":"observation","detail":"The fixture pins the reply course request by request in the order reply_course_wires gives in live_review_mutation_provider_tests.cpp (method, path, GraphQL document kind and name), and the client asserts the confirmed receipt PRRC_reply_45 and the succeeded recovery record, through the native transport rather than the session suite's fixture transport. live_review_mutation_provider and live_review_mutation_restart_recovery still pass.","path":"docs/issues/60-done/BPR-1018-the-native-transport-treats-every-review-mutation-post-as-a-rest-write.md#evidence"}}]
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

## Resolution — 2026-09-23, Mac integrator

Fixed at `210e68b5`. Fixture-only: no live server, credential or GitHub
mutation was involved.

### What the reading above got right, and three corrections

The two rules were real and each was enough on its own. Running the new
fixture course against the unfixed transport also showed:

1. **The rate-limit resource was a third blocker, not a smaller point.** On
   GitHub.com the transport requires the rate-limit tuple and refuses an
   answer whose `X-RateLimit-Resource` is not the one the request was
   accounted to. GitHub's GraphQL answers name `graphql`, so accounting the
   GraphQL operations to `core` would have refused every one of them even
   with the witness and status rules fixed. The mutation check below proves
   it.
2. **The REST review submission was refused the same way.** `create_review`
   (`POST …/pulls/N/reviews`) is a POST on the review-mutation surface, so the
   transport required 201; GitHub answers it 200 OK, and
   `parse_review_submission_response` already requires 200. A whole review
   could therefore not be submitted over native HTTPS either.
3. **The provider's account-checking wrapper is not on this path.** The
   wrapper at `github_provider.cpp:853` is `cli_observed_account_transport`,
   which the provider composes only for a gh CLI account, and the gh CLI
   transport refuses every GraphQL-rooted operation by design. It never sees
   an unwitnessed POST, so it is unchanged.

### Decisions

The classification is `review_mutation_exchange_for`
(`src/adapters/github/src/github_mutation_contract.cpp`). It is derived from
`review_mutation_operation` alone, and its switch has no default, so a new
operation does not build until it is classified. A value outside the
enumeration has no exchange and the native transport refuses it before the
network.

| Operation | Method | Root, rate-limit resource | Success | Witness |
|---|---|---|---|---|
| `observe_pull_request`, `observe_viewer`, `observe_review_comments`, `observe_reviews` | GET | REST, `core` | 200 | no |
| `create_review_comment` | POST | REST, `core` | 201 | yes |
| `create_review` | POST | REST, `core` | 200 | yes |
| `reply_review_thread` | POST | GraphQL, `graphql` | 200 | yes |
| `resolve_review_thread`, `unresolve_review_thread` | POST | GraphQL, `graphql` | 200 | no |
| `observe_review_thread`, `observe_review_thread_comments` | POST | GraphQL, `graphql` | 200 | no |

- **Witness.** Exactly the writes a session keeps a recovery record for: the
  comment, the review and the reply. A resolution is not witnessed. It keeps
  no recovery record ([[BPR-0657]]) because the server treats it as
  idempotent, so repeating one after an ambiguous outcome is safe and there
  is nothing a witness would make durable first. The native transport now
  records that a POST may have left for every review POST (so transmission is
  still reported `indeterminate` after one is handed to Qt, and no review POST
  falls back to another proxy), and consults the caller's witness only for a
  witnessed operation. A witness supplied for anything else is not consulted.
- **Success status.** 201 for the REST comment write, 200 for everything else,
  and nothing else is admitted.
- **Rate-limit resource.** `graphql` for the GraphQL operations, `core` for the
  REST ones.
- The native transport also refuses an exchange whose method is not its
  operation's, or whose path disagrees with the profile's GraphQL route in
  either direction.

## Evidence

At `210e68b5` on macOS (arm64), `build/dev`:

- **Failing before.** The new fixture scenarios run against the unfixed
  transport (tests at the same content, product at `dbaf7040`):
  `mutation-graphql-course` fails (the child exits at the first GraphQL
  read), `mutation-review-submission` fails, `mutation-status-refusal` fails
  (the unfixed transport accepts the 201 answer to the review),
  `mutation-graphql-core-resource` fails its observation (the unfixed
  transport never sends the read). `mutation-graphql-witness-failure`,
  `mutation-witness` and `mutation-witness-failure` pass before and after.
- **After.** All seven pass.
- **Mutation checks,** each restored afterwards: marking the reply unwitnessed
  fails `mutation-graphql-course` and `mutation-graphql-witness-failure`;
  accounting GraphQL to `core` fails `mutation-graphql-course` and
  `mutation-graphql-core-resource`.
- `mutation-graphql-course` drives the production `review_mutation_session`
  over the native transport to the loopback CONNECT/TLS fixture at the
  GitHub.com authority. The reply is [[BPR-0896]]'s course as
  `reply_course_wires` pins it in
  `tests/github_contract/live_review_mutation_provider_tests.cpp`: the fixture
  pins each request's method, path and GraphQL document (`query
  ObserveThread`, `query ObserveThreadComments`, `GET /user`, `GET
  …/pulls/17`, `mutation ReplyToThread`, `query ObserveThreadComments`) in
  order and answers each 200 with the rate-limit tuple; the client asserts
  the confirmed receipt `PRRC_reply_45`, the succeeded recovery record, and
  per exchange that the session supplied a witness only for the reply and the
  transport consulted it exactly once. The resolution and unresolution follow
  (`query ObserveThread`, `mutation ResolveThread`, `query ObserveThread`,
  `mutation UnresolveThread`) and return the thread in the state asked for.
- `github_mutation_contract` gains `test_every_operation_names_its_exchange`:
  one row per operation, a value past the enumeration with no exchange, and
  every compiled request agreeing with its row's method and root.
- `ctest --test-dir build/dev -R` over `native_https_transport_contract`,
  `native_https_transport_fixture`, `rfc0017_credential_free_native_https`,
  `native_https_anonymous_read_fixture`, `rfc0017_test_inventory`,
  `rfc0017_test_inventory_contract`, `gh_cli_transport`,
  `github_mutation_contract`, `live_review_mutation_provider`,
  `live_review_mutation_restart_recovery`, `live_review_submission_provider`,
  `live_review_submission_restart_recovery`, `github_provider`,
  `release_update_native_https`,
  `application_review_submission_composition_public_path`,
  `live_mutation_isolation` and `live_mutation_isolation_contract`: 17 of 17
  pass. `test_network_isolation_manifest` fails on the pre-existing
  `release_update_native_https has loopback count 1, expected 0` row that
  Blocked [[BPR-0962]] owns; this change touches no registration.
- clang-format 23.1.1: the five touched C++ files pass the whole-file check.
  clang-tidy on the four touched translation units reports only findings on
  untouched lines (glaze option names at `github_mutation_contract.cpp:683`,
  `#pragma once` in two test headers, `home_view_state.hpp:35`).

## Limits

- Nothing here was sent to a real server. The status and resource facts are
  GitHub's documented behavior and the parsers' existing expectations; a
  live reply, resolution or review over native HTTPS remains unobserved.
- GHES answers are accepted without a rate-limit tuple, as before; if one is
  present it must name the resource above.
- The gh CLI transport still refuses every GraphQL-rooted operation
  ([[BPR-0896]]'s decision). Over the CLI a reply, resolution or
  unresolution is still refused; over native HTTPS it can now complete.
