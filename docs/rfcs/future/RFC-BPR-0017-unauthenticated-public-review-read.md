---
id: RFC-BPR-0017
title: Unauthenticated Public Review Read
status: Deferred
area: BPR
milestone: Post-Pack v2
depends_on: [RFC-BPR-0002]
pack_version: post-v2
---

# RFC-BPR-0017: Unauthenticated Public Review Read

## Status and scope

This is a deferred scoping RFC for work after the canonical fourteen-RFC
pack. It is not part of pack v2 and blocks nothing in it.

It exists to answer one question before any code is committed to it: can
BetterPullReview open a public pull request without the operator
authenticating, and what is actually lost when it does. The findings below
are the deliverable. The design that follows them is conditional on
eligibility gates that are not yet satisfied, and the recommendation section
states plainly when this work should not be done at all.

The central finding contradicts the intuition that motivated the question.
The read path is not broadly GraphQL-dependent and does not need a parallel
REST implementation. Nine of ten read operations already speak REST, one does
not, and the loss is confined to that one plus the two observations that have
no anonymous meaning.

## Findings established from the codebase

These are verified against the tree, not asserted.

**Exactly one read operation uses GraphQL.** The route selection in
[github_read_contract.cpp:1780](../../../src/adapters/github/src/github_read_contract.cpp#L1780)
and again at
[github_read_contract.cpp:1891](../../../src/adapters/github/src/github_read_contract.cpp#L1891)
reduces to a single comparison against `read_operation::list_review_threads`,
and the route switch gives that operation alone a `/graphql` path with a query
body. The other nine operations in `read_operation_catalog` — viewer identity,
repository, pull request, files, commits, reviews, issue comments, checks, and
search — each build a REST path, seven of them from `repository_path` or
`pull_request_path` and the remaining two from `/user` and `/search/issues`.

**Degraded capability is already a first-class type.** `github_read_shape` in
[github_host_authority.hpp](../../../src/application/include/bpr/application/github_host_authority.hpp)
carries four booleans — `ReviewThreads`, `ViewedFileState`,
`ResolvePermission`, `UnresolvePermission` — and the GraphQL query body in
`paged_review_body` is assembled from them. A shape with all four false emits
no thread selection and no file selection. The mechanism for expressing "this
host cannot do threads" therefore already exists and is already exercised by
capability probing; an unauthenticated profile is a further instance of a
condition the system models rather than a new kind of condition.

**Anonymous REST against a public pull request is already performed
in-process, but not by the production transport.**
[designated_live_public_observer.cpp](../../../src/adapters/github/src/designated_live_public_observer.cpp)
observes a public pull request with no credential and matches its repository,
pull request, base, and head fields. It is direct proof that the observation
is possible. It is not reusable evidence: it spawns `/usr/bin/curl` through
`posix_spawn`, compiles in a single fixed path and origin, and exists only for
the attended live lane. It says nothing about whether the native HTTPS
transport can issue a credential-free request, which is a separate question
this RFC's gates must answer.

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

- **Review thread grouping and resolution state.** REST exposes review
  comments as a flat list carrying reply linkage, so thread grouping is
  reconstructible from that linkage. Resolution state is not: it exists only on
  the GraphQL thread type, and there is no REST field that carries it. An
  anonymous view can therefore show every review comment and can group them,
  but cannot say whether a thread was resolved — which is precisely the signal
  the workbench uses to decide what still needs attention.
- **Viewed-file state.** This is per-viewer by construction and has no
  anonymous meaning. It is absent rather than degraded.
- **Viewer identity.** `get_viewer_identity` has no anonymous answer, and
  every downstream behavior keyed to "is this mine" — authorship, review
  eligibility, actionability — is unavailable rather than defaulted.

The honest characterization is that anonymous mode delivers reading and denies
triage. That is a coherent product mode, but it is a different one from the
authenticated workbench, and it must not be presented as the same surface with
some fields missing.

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
  against the real service, captured as sanitized fixtures.
- A demonstration that the native HTTPS transport issues a credential-free
  request with all RFC-BPR-0002 redirect, certificate, and proxy rules intact,
  and that omitting the authorization header is a deliberate typed state rather
  than an absent credential.
- A product decision that a read-only, triage-less, rate-limited preview of
  public GitHub.com pull requests is a mode the product wants, recorded with
  its intended presentation of those limits.
- RFC-BPR-0002 Auto-Verified, since this mode reuses its transport and
  normalization guarantees rather than restating them.

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
| Nine of ten operations need no credential | Read contract route tests | Assert every operation but `list_review_threads` builds a REST route and that the anonymous set omits only it and viewer identity |
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
