---
id: "RFC-BPR-0017"
title: "Unauthenticated Public Review Read"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002"]
pack_version: "post-v2"
---

# RFC-BPR-0017: Unauthenticated Public Review Read

## Status and scope

The anonymous-service measurements and the owner's positive build decision
are recorded, and the implementation enables Public preview on macOS.
The [platform amendment r1](amendments/RFC-BPR-0017-public-preview-platforms-r1.md)
adds Windows and Linux under BPR-1664; its native acceptance is recorded separately
in the [verification ledger](../verification/RFC-BPR-0017.md#platform-expansion-r1).
BPR-0309 closed the
RFC on 2026-09-04 with focused macOS acceptance evidence and clean independent
review of `d9006237`; its ledger records omitted matrices and platform limits.

The current read catalog adds REST inline review comments to the existing
provider/pager/normalizer. Public preview admits twelve REST operation kinds (including the later
commit-status, timeline and merge-base reads),
refuses GraphQL and viewer identity, and uses explicit anonymous native HTTPS
and public Git/cache/epoch/diff authority. Thread resolution, viewed state,
account-relative associations, triage, and publishing remain unavailable.
See [the product contract](../architecture/product-contract.md#public-preview)
and [verification ledger](../verification/RFC-BPR-0017.md) for current behavior
and exact evidence.

The findings below describe the immutable pre-implementation base
`a45750f6bd84716240aa27a20f0734f33808ab8e`. Statements about the missing REST
comment operation, credential-mandatory factories, and absent anonymous
composition are historical motivation; they are not claims about today's tree.

## Findings established at the pre-implementation base

The source locations in this historical analysis refer to that base.

**Exactly one read operation uses GraphQL.** The route selection in
[github_read_contract.cpp:1780](../../src/adapters/github/src/github_read_contract.cpp#L1780)
and again at
[github_read_contract.cpp:1891](../../src/adapters/github/src/github_read_contract.cpp#L1891)
reduces to a single comparison against `read_operation::list_review_threads`,
and the route switch gives that operation alone a `/graphql` path with a query
body. The other nine operations in `read_operation_catalog` — viewer identity,
repository, pull request, files, commits, reviews, issue comments, checks, and
search — each build a REST path, seven of them from `repository_path` or
`pull_request_path` and the remaining two from `/user` and `/search/issues`.

**The read catalog has no REST review-comment operation.**
`read_operation_catalog` in
[github_read_contract.hpp:35](../../src/adapters/github/src/github_read_contract.hpp#L35)
holds exactly ten entries, and the one named for comments,
`list_issue_comments`, builds `/repos/{owner}/{repo}/issues/{n}/comments`
([github_read_contract.cpp:1838](../../src/adapters/github/src/github_read_contract.cpp#L1838)).
That is the issue-level conversation, not the inline review comments anchored
to source lines. The read contract builds no `/pulls/{n}/comments` path at all.

That path does exist in the tree, but only under the mutation authority, and
the distinction matters enough to state precisely.
`compile_review_comment_request` builds it as a `POST`
([github_mutation_contract.cpp:646](../../src/adapters/github/src/github_mutation_contract.cpp#L646)),
and `compile_review_comment_observation_request` reuses the same path as a
`GET` for post-submit reconciliation
([github_mutation_contract.cpp:652](../../src/adapters/github/src/github_mutation_contract.cpp#L652)),
whose response `parse_review_comment_observations` already normalizes into
`application::review_comment_projection`
([github_mutation_contract.cpp:852](../../src/adapters/github/src/github_mutation_contract.cpp#L852)).
So the repository can already read inline review comments over REST,
authenticated — but only from the private `review_mutation_session::reconcile`
([github_mutation_contract.cpp:1025](../../src/adapters/github/src/github_mutation_contract.cpp#L1025))
inside a submit-and-reconcile sequence, as a pre-write baseline
([github_mutation_contract.cpp:1184](../../src/adapters/github/src/github_mutation_contract.cpp#L1184))
and again after transmission
([github_mutation_contract.cpp:1250](../../src/adapters/github/src/github_mutation_contract.cpp#L1250)).
Neither the read catalog nor `perform_paged_github_read` can select it.

On the read path, therefore, GraphQL is presently the only source of inline
review comments and threads, not merely the only source of their resolution
state. Whether any REST source answers such a request anonymously is a service
claim this RFC has not proven and must not assume; it belongs to the probe
evidence below. An anonymous mode that refuses the GraphQL operation loses
every inline review comment until that question is answered and a read
operation is wired to it. The parsing work is partly done; the read-path
operation, its fixtures, and its anonymous behavior are not.

**Degraded capability is already a first-class type.** `github_read_shape` in
[github_host_authority.hpp](../../src/application/include/bpr/application/github_host_authority.hpp)
carries four booleans — `ReviewThreads`, `ViewedFileState`,
`ResolvePermission`, `UnresolvePermission` — and the GraphQL query body in
`paged_review_body` is assembled from them. The mechanism for expressing "this
host cannot do threads" therefore already exists and is already exercised by
capability probing; an unauthenticated profile is a further instance of a
condition the system models rather than a new kind of condition.

**The selected path already skips GraphQL for an all-false shape; the helper
that does not is tests-only.** `perform_graphql_projection` in
[github_paged_read.cpp:2612](../../src/adapters/github/src/github_paged_read.cpp#L2612)
marks review threads unavailable and returns before constructing a request
when `ViewedFileState` and `ReviewThreads` are both false, so no GraphQL body
is built and none is sent. This is the production path:
`perform_paged_github_read` has exactly one non-test call site, in
[github_provider.cpp:1438](../../src/adapters/github/src/github_provider.cpp#L1438).

`perform_normalized_github_read`
([github_read_normalizer.cpp:867](../../src/adapters/github/src/github_read_normalizer.cpp#L867))
behaves differently and must not be read as evidence about the selected path.
It iterates the whole catalog unconditionally, issuing all ten operations
including `list_review_threads`, and hardcodes the GraphQL shape to all-true
regardless of capability state
([github_read_normalizer.cpp:420](../../src/adapters/github/src/github_read_normalizer.cpp#L420)).
Its only call site is
[github_read_contract_tests.cpp:493](../../tests/github_contract/github_read_contract_tests.cpp#L493),
whose fixture format requires a manifest of exactly ten interactions. It is a
recorded-contract helper for asserting wire shape, not a read path a profile
selects.

**Credential-mandatory native HTTPS is implementation work, not
configuration.** Both production factories take a credential as a required
parameter and refuse invalid or empty material before a transport exists
([native_https_transport.cpp:3812](../../src/adapters/github/src/native_https_transport.cpp#L3812)
and
[native_https_transport.cpp:3841](../../src/adapters/github/src/native_https_transport.cpp#L3841)),
and once a transport exists every request carries an `Authorization` header —
`Bearer` on the REST surface
([native_https_transport.cpp:2966](../../src/adapters/github/src/native_https_transport.cpp#L2966))
and `Basic` on the Git smart-HTTP surface
([native_https_transport.cpp:3036](../../src/adapters/github/src/native_https_transport.cpp#L3036))
— with no branch that omits it. There is no existing code path through that
class that issues an unauthenticated request. Omitting the header as a
deliberate typed state is new work in the transport, not a parameter that
already exists.

**Nonzero viewer and account identity is a hard gate, not an assumption.** The
strictest gate is in the session read model itself:
`validate(github_session_read_model, …)` returns `missing_identity` when
`Viewer.Account.value() == 0U`
([github_read_model.hpp:1509](../../src/application/include/bpr/application/github_read_model.hpp#L1509)),
and the live host enforces it when accepting a session, failing the session
outright when validation does not hold
([github_application_host.cpp:1869](../../src/application/src/github_application_host.cpp#L1869)).
A profile with no authenticated account cannot form a valid session read model
at all. Below that, `valid(github_credential_authority_key)`
([github_credential_vault.hpp:94](../../src/application/include/bpr/application/github_credential_vault.hpp#L94))
and `valid(github_capability_authority_key)`
([github_capability_cache.hpp:149](../../src/application/include/bpr/application/github_capability_cache.hpp#L149))
both require `Account.value() != 0U`, and both keys are constructed from the
authenticated `/user` response as `src/application/AGENTS.md` requires.
Anonymous mode therefore needs typed state that routes around these
authorities rather than a relaxed account value, which is again implementation
work — and it bears directly on the acceptance criterion that an anonymous
profile open a pull request through the existing normalized provider surface.

**Anonymous REST against a public pull request is already performed
in-process, by two different mechanisms, neither of which is the production
transport.** They must be distinguished, because one is a verification-only
probe and the other is compiled product code.

The first is
[designated_live_public_observer.cpp](../../src/adapters/github/src/designated_live_public_observer.cpp).
It observes a public pull request with no credential and matches its
repository, pull request, base, and head fields. It is not reusable evidence:
it spawns `/usr/bin/curl` through `posix_spawn`, compiles in a single fixed
path and origin, and is built only when `BPR_BUILD_QT AND APPLE AND
(BUILD_TESTING OR BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE)`
([CMakeLists.txt:2](../../src/adapters/github/CMakeLists.txt#L2), nested
inside the `BPR_BUILD_QT` guard at
[CMakeLists.txt:1](../../src/adapters/github/CMakeLists.txt#L1)). It is a
probe, not product code.

The second is `native_designated_review_public_observer`
([native_https_transport.cpp:4027](../../src/adapters/github/src/native_https_transport.cpp#L4027)),
returned by `make_designated_review_public_observer`. This one is product
code: it lives in `native_https_transport.cpp`, which is compiled into
`better_pull_review_adapter_github` whenever `BPR_BUILD_QT` is on, with no
platform or `BUILD_TESTING` guard. It issues a genuine credential-free
`QNetworkAccessManager::get`, refusing `Authorization`, `Cookie`, and
`Proxy-Authorization` headers outright rather than merely omitting them
([native_https_transport.cpp:4092](../../src/adapters/github/src/native_https_transport.cpp#L4092)).

It is still not the evidence this RFC's gate requires, and the reason is
narrower than "it ignores the rules". It duplicates them instead of exercising
them. Its TLS block
([native_https_transport.cpp:4104](../../src/adapters/github/src/native_https_transport.cpp#L4104))
is an inline copy of the transport's own `ssl_configuration`
([native_https_transport.cpp:2880](../../src/adapters/github/src/native_https_transport.cpp#L2880))
— same system CA set, cleared local chain and key, ALPN, `VerifyPeer`, depth
zero, TLS 1.2 or later, same option disables — and its redirect handling is
stricter than the transport's, refusing every redirect outright
([native_https_transport.cpp:4083](../../src/adapters/github/src/native_https_transport.cpp#L4083)
and
[native_https_transport.cpp:4207](../../src/adapters/github/src/native_https_transport.cpp#L4207))
where the transport follows bounded validated ones. What genuinely differs is
proxy handling: the observer sets `NoProxy`
([native_https_transport.cpp:4062](../../src/adapters/github/src/native_https_transport.cpp#L4062))
where the transport performs system-proxy discovery
([native_https_transport.cpp:714](../../src/adapters/github/src/native_https_transport.cpp#L714)),
and it runs on a local manager rather than the transport's shared worker.

Its reachability is narrow by construction: production composition creates it
only inside the attended designated-live approval branch
([github_application.cpp:1736](../../src/app/src/github_application.cpp#L1736)),
and that branch throws in a tests-enabled build. So the tree does contain a
credential-free GET against `api.github.com` compiled into a shipped build.
That narrows the open question without closing it. A duplicated policy proves
that a copy of the rules was written, not that the transport the read path
selects can issue a credential-free request with its own rules intact; and the
proxy guarantee is never exercised — the observer refuses proxy authentication
but performs no system-proxy discovery. Any inventory or symbol scan written
for the gate below must also treat this observer as a known pre-existing
credential-free symbol rather than as a regression.

## Findings requiring direct probe evidence

The following describe GitHub rather than this repository. Per the repository's
evidence rules they are stated as claims to be proven against the real
service, not as settled facts, and each one must be captured as recorded
fixture evidence before any implementation begins.

- **GraphQL rejects unauthenticated requests entirely.** If true, review
  threads and their resolution state are unavailable anonymously by any route,
  and no amount of REST work recovers them.
- **Unauthenticated REST is limited to 60 requests per hour per source
  address**, against 5,000 per hour authenticated. Opening one pull request
  costs several requests across metadata, files, comments, reviews, and
  commits, and the budget is shared by every client behind the same address.
  The practical ceiling is therefore a handful of pull requests per hour, and
  fewer behind office or carrier network address translation.
- **Whether conditional requests returning 304 are exempt from that budget.**
  This materially changes whether refresh is viable anonymously and must not be
  assumed in either direction.
- **Whether any REST source returns inline review comments to an anonymous
  caller, and whether any REST field carries thread resolution state.** The
  read catalog calls no such source today, so this determines whether the mode
  can show the source-anchored review conversation at all or only the code, and
  whether triage is degraded or simply absent. It must be resolved before the
  product decision, because it decides what the mode is.
- **Private repositories return 404 rather than 403 to anonymous callers**, so
  the mode cannot distinguish "does not exist" from "not yours" and must not
  claim to.
- **GHES instances in private mode reject all unauthenticated API access**,
  which would confine this mode to GitHub.com public repositories.

## What an unauthenticated read delivers

Assuming the claims above hold, an anonymous profile retains pull request
metadata, the changed-file set and diff content, commits, review summaries and
their states, issue-level conversation comments, checks, and search. That is
the substance of reading a pull request.

It loses three things:

- **Every inline review comment, along with thread grouping and resolution
  state.** This is the correction that most changes the shape of the mode. As
  the read catalog stands, the GraphQL operation is its only source of inline
  review comments; refusing it leaves the anonymous view with no
  source-anchored comments at all, not a flat list missing a resolution flag.
  Recovering them means a read-path review-comment operation with its own
  fixtures. The response parsing already exists on the mutation side; the
  read operation does not, and no card currently owns adding one. Whether
  any REST source carries thread resolution state, and whether it answers
  anonymously, are service claims for the probe list rather than settled
  facts; resolution is the signal the workbench uses to decide what still
  needs attention, so the answer decides whether triage is degraded or absent.
- **Viewed-file state.** This is per-viewer by construction and has no
  anonymous meaning. It is absent rather than degraded.
- **Viewer identity.** `get_viewer_identity` has no anonymous answer, and
  every downstream behavior keyed to "is this mine" — authorship, review
  eligibility, actionability — is unavailable rather than defaulted.

The honest characterization is that anonymous mode delivers reading the code
and denies both the source-anchored review conversation and triage. That is a
coherent product mode, but it is a different one from the authenticated
workbench, and it must not be presented as the same surface with some fields
missing.

## Recommendation

**Do not build a parallel read path.** The finding that nine of ten operations
are already REST removes the motivation for one. A parallel path would
duplicate normalization, fixtures, paging, and error handling in order to
avoid a single GraphQL operation, and would then need its own verification
surface for behavior the primary path already proves.

**If this is built, build it as a credential-optional profile on the existing
path.** The shape is an authentication mode in which the transport omits the
authorization header, `github_read_shape` is fixed all-false, the
GraphQL-routed operation and the viewer-identity operation are refused with a
typed diagnostic before any request is constructed, and every remaining
operation runs unchanged. The refusal must be structural rather than a runtime
failure, in the same manner as the existing `cli_unavailable` treatment.

**Do not build it at all if the rate limit claim holds and refresh is not
exempt.** A mode that permits roughly six to twelve pull request opens per
hour, shared across everything on the operator's address, and that silently
degrades when a colleague on the same network opens one too, is worse than
requiring authentication. Under that constraint the correct answer to "I want
to look at a pull request without signing in" is to make signing in cheap,
which is
[RFC-BPR-0016](RFC-BPR-0016-github-device-authorization-flow.md), not to make
not signing in possible.

The two RFCs are therefore alternatives addressing one complaint, and
RFC-BPR-0016 is the stronger answer. This RFC should proceed only if the probe
evidence shows the rate budget is workable, and only as a preview mode with
explicit limits presented up front.

## Eligibility gates

Implementation may begin only after all of the following exist:

- Recorded probe evidence resolving every claim in the second findings section
  against the real service, captured as sanitized fixtures. This is
  [BPR-0298](../issues/60-done/BPR-0298-record-anonymous-github-contracts.md).
- A demonstration that the native HTTPS transport issues a credential-free
  request with all RFC-BPR-0002 redirect, certificate, and proxy rules intact,
  and that omitting the authorization header is a deliberate typed state rather
  than an absent credential. This is
  [BPR-0313](../issues/60-done/BPR-0313-demonstrate-credential-free-native-https.md),
  and it comes before the product decision, not after it.
- A product decision that a read-only, triage-less, rate-limited preview of
  public GitHub.com pull requests is a mode the product wants, recorded with
  its intended presentation of those limits. This is
  [BPR-0299](../issues/60-done/BPR-0299-decide-anonymous-public-preview.md),
  which depends on both of the above.
- RFC-BPR-0002 Auto-Verified, since this mode reuses its transport and
  normalization guarantees rather than restating them.

The second gate is the one that risks being satisfied dishonestly, so its
shape is fixed here. The demonstration is a `BUILD_TESTING`-only, non-product
loopback course. It constructs an explicit typed credential-free request and
proves zero vault lookup, zero credential discovery, no authorization header,
no subprocess, and no non-loopback outbound traffic, while reusing the existing
native-HTTPS system-trust, proxy, redirect, cancellation, timeout, and
response-limit mechanics, fail-closed, so the guarantees under test are
the real ones rather than a copy of them. Its factory and symbol must be absent
from a tests-disabled Release build, and missing, malformed, or revoked
authenticated material must never fall back into the credential-free state;
both a positive course and a cut-wire canary are required. BPR-0313 also
creates `scripts/verify_rfc0017_test_inventory.py`, the count-checked CTest
inventory and tests-disabled Release symbol-absence check that the later cards
in this program extend, and that check must be written against the
pre-existing credential-free observer named in the findings rather than
against an assumption that none exists.

That is deliberately a probe rather than a beginning. Neither observer
described in the findings can stand in for it: the `curl` observer is not the
transport, and `native_designated_review_public_observer` is product-compiled
but duplicates the transport's TLS and redirect policy rather than exercising
it, skips system-proxy discovery outright, and is reachable only from the
attended designated-live branch. Satisfying this gate
creates no anonymous product profile, no partial or degraded public mode, and
no change to the credential-mandatory production factories. Anything that would
be reachable from a shipped build is product implementation and belongs after
the product decision, not inside the evidence that informs it.

## Prohibited designs

- A second read path, normalizer, fixture set, or provider for anonymous
  reads.
- Presenting an anonymous view as equivalent to the authenticated workbench,
  or rendering thread resolution, viewed state, or actionability as a default
  value where the observation is unavailable.
- Inferring resolution state from comment position, ordering, reply linkage, or
  any other REST-derivable proxy.
- Sending an authorization header, a stored credential, or any vault lookup on
  an anonymous profile.
- Retrying past the rate limit, or presenting a rate-limit refusal as a network
  error.
- Attempting anonymous access to a GHES profile.
- Using GitHub Actions for build, test, package, release, or verification.

## Acceptance criteria

- An anonymous GitHub.com profile opens a public fixture pull request through
  the existing normalized provider surface, with no second read path
  introduced.
- The GraphQL-routed operation and the viewer-identity operation are refused
  with distinct typed diagnostics before request construction, and provider
  counters observe zero requests for them.
- `github_read_shape` is all-false on an anonymous profile, and every
  capability-gated action is unavailable rather than approximated.
- No vault lookup, credential enrollment, or authorization header occurs on an
  anonymous profile, proven by vault and transport counters.
- Rate-limit exhaustion produces a distinct typed diagnostic naming the limit,
  not a network or authentication failure.
- A private-repository request produces a diagnostic that does not assert the
  repository's existence either way.
- A GHES profile refuses anonymous selection with zero requests sent.
- Switching from anonymous to authenticated on the same host invalidates all
  identity-sensitive caches and projections exactly as a host or account switch
  does.
- Build and verification evidence is produced locally or on target-native
  release-operator environments, with no GitHub Actions involvement.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Nine REST operation kinds are admitted anonymously | Read contract route tests | Assert the admitted set includes REST review comments and excludes GraphQL thread state and viewer identity before request construction |
| Inline review comments use the existing REST read path | Read contract catalog, pager and normalizer tests | Assert `/pulls/{n}/comments` differs from issue comments, preserves reply linkage and line identity, and grants no mutation authority |
| Missing observations are unavailable, not defaulted | Application state tests | Assert observed REST comments remain readable while GraphQL-only resolution and viewer-relative state remain unavailable; unobserved comments are distinct from an observed empty result |
| No parallel path was introduced | Architecture boundary tests | Prove the anonymous mode adds no provider, normalizer, or fixture set and reuses the existing contract |
| Unavailable observations are refused, not defaulted | Application state tests | Assert thread resolution, viewed state, and actionability are unavailable states and no default value reaches a projection |
| No credential is touched | Vault and transport counters | Run a full anonymous open and require zero vault lookups and zero authorization headers |
| Rate limiting is typed | Provider contract with a limit fixture | Assert a distinct limit diagnostic and no retry past it |
| Private repositories are not disclosed | Provider contract with a 404 fixture | Assert the diagnostic asserts neither existence nor absence |
| GHES refuses anonymously | Public self-test and provider counters | Select anonymous on a GHES profile and require refusal with zero requests |
| Mode switching invalidates identity state | Application host invalidation tests | Switch anonymous to authenticated and assert the same invalidation as an account switch |

## Documentation impact

The anonymous mode's exact capability set, its rate-limit behavior and how
those limits are presented, the observations that are unavailable rather than
defaulted, the private-repository non-disclosure rule, and the GHES exclusion
are mandatory.

The documentation ledger must identify functional-core, application-host, and
Qt-presentation-adapter ownership; typed intents, events, observations,
requests, and deltas; authority; concurrency and generation; and any
evidence-backed no-impact region.
