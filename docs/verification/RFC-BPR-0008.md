# RFC-BPR-0008 Verification Plan and Ledger

RFC base: `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`

Status: **the fixture-safe product round trip is composed; macOS default-deny
test isolation is directly verified, while cross-platform and attended/live
acceptance remain incomplete.** BPR-0135 through
BPR-0139 delivered the draft policy, target revalidation, mutation coordinator,
synchronization, and Qt editor against fixture/component surfaces. BPR-0225
found no construction or invocation of those surfaces in the public
application. BPR-0229 now connects the shipped editor gesture through core,
the exact-target fixture coordinator, server refresh, and Qt projection.
The BPR-0221 structural guard remains defense in depth. BPR-0235 now places the
entire configured macOS CTest graph behind a process boundary and includes
generic network, script descendant, exact fixture endpoint, unrelated local
endpoint, and production-app counterexamples.
Linux code awaits native replay, Windows remains fail-closed before every test
command, and the newly designated pull-request identity is not yet connected
to a single pre-launch-validated live lane.

RFC-BPR-0008 stays `Proposed`. No card here makes a closure claim. The safe
repository is designated, but the live criteria still require session-scoped
write authority and attended/mixed-client execution. RFC-BPR-0008 stays
`Proposed`.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007. All three
remain incomplete and `Proposed`; their live cards name the implementation,
composition, target-native, and attended work still open. Done BPR-0088 records
the product owner's direction to advance the implementation frontier despite
that work. Closure order is unchanged.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Draft and publish semantic state, review command legality, the shared/private distinction, target and head revalidation policy, and acceptance or rejection of mutation results. It emits semantic proposals, never HTTP | Qt-free and provider-free replay tests; the `core-only` preset links no provider |
| Application host | Synchronization, mutation coordination, acting identity, request execution, idempotency, retries, reconciliation, safe-target enforcement, and conversion of results into typed events | Fake-provider tests over the coordinator, including target substitution between prepare and execute |
| Qt adapter | Editors, pending-review presentation, target and head confirmation, progress, errors, and action binding. A widget never calls a mutation endpoint | Qt adapter tests asserting every action produces an intent and no widget holds a provider |
| Composition root | Wiring the coordinator into the real application | Selected public-path evidence through the real executable against the fixture provider |

GitHub remains the authority after every mutation: the host re-reads server
state rather than assuming its own request succeeded as sent.

## The safety rule this RFC turns into a test

"No automated test can mutate a non-designated live repository/PR" is an
acceptance criterion, and it is the one criterion that must hold *before* any
mutation code is trusted. It is not satisfied by intending to be careful.

Every slice here runs against a fixture provider. The coordinator refuses a
target that is not the designated one, and that refusal is asserted rather than
assumed — including when the target is altered between prepare and execute,
which is the case a check performed only at prepare time would miss.

No slice in this plan performs a live mutation. The cards that would require
one are listed under deferred gates and stay in `backlog/`.

## Claim matrix

A row is claimed only when its card lands with direct evidence.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Draft and publish legality is decided in the core | Core | Drive every draft, edit, discard, and publish transition | Illegal transitions carry typed reasons; the suite runs under `core-only` with no provider linked | Verified. BPR-0135 models `publishing` as its own state so a second publish in flight is refused, keeps a failed publication's text, derives `shared` from `published` rather than a settable flag, and refuses six distinct mistakes with six distinct reasons |
| Head movement cannot silently publish | Core | Move the head between draft and publish | Publication is refused with a distinct reason until explicitly revalidated or remapped | Verified. BPR-0136 refuses a moved head until acknowledged, refuses a changed host, repository, or pull request *even when acknowledged*, refuses an unnameable head outright, and reports the changed target ahead of the moved head when both are wrong |
| The wrong target is impossible | Host coordinator | Alter host, repository, pull request, and head between prepare and execute | Each substitution is refused, and refusal is proven for a change made *after* the prepare check | Verified. BPR-0137 observes the target twice; the fake reports the designated target first and another repository second, and the statistics show two observations with zero submissions, so the second check is proven to have run |
| Retries do not duplicate | Host coordinator | Replay a mutation whose result was lost | The second attempt is recognised rather than creating a second comment | Verified. BPR-0137 derives the key from the draft, target, anchor, disposition, and body; the retry returns the original comment identifier and the provider holds one comment, and four cases assert the key changes when body, anchor, or head does |
| External changes round-trip | Host synchronization | Apply an externally created, edited, and deleted comment to the projection | Each is reflected after refresh, and a local assumption never overwrites an unknown external change | Verified against the fake. BPR-0138 round-trips created, edited, deleted, resolved, and reopened; an edit updates rather than duplicating; and an unpublished local edit colliding with a server change keeps the server's text and reports a conflict rather than resolving it. That GitHub produces these shapes is BPR-0143 |
| The mutation path cannot bypass the host | Qt plus trace | Drive every publish and resolve action | Every action produces a core proposal, then a host coordination, then a result event; no widget holds a provider | Verified through the shipped fixture path. BPR-0139's editor is given dispatch callbacks and no provider. BPR-0229 drives real text and Publish actions through core proposal, exact-target coordinator, provider result, server refresh, and typed Qt projection; its source guard rejects direct coordinator/provider calls from the probe |
| Capability fallback is explicit | Host | Offer a provider without native resolve or viewed-state | The degradation is reported in typed form rather than silently skipped | Verified through the fixture product path. BPR-0137 defines the typed capability absence, and BPR-0229 projects the fixture provider's unsupported resolve/viewed capabilities into the shipped editor rather than pretending success |
| Acting identity is carried | Host | Publish under a fixture identity | The proposal and the request carry the active profile's identity | Verified for the request. BPR-0137 asserts the submitted request carries the acting identity and the coordinator's own designated target, so no caller can name a destination. Whether GitHub attributes the review to that account is BPR-0144 |
| No automated test mutates an undesignated target | Whole suite | Structural `live_mutation_isolation` plus the default-deny network/process lane | Every CTest executable, script, helper subprocess, and production-app launch is network-denied unless it belongs to one explicit designated-target lane | **Verified on the configured macOS graph; cross-platform/live exception still open.** Every configured command carries the exact sandbox-runner prefix, label, isolated Python startup, and pre-boundary environment scrubs; the manifest rejects injection overrides. Direct HTTPS-port access, a Python descendant, and the shipped native-HTTPS application course receive process-boundary denial. The runner grants only selected free ports plus a fresh private Unix-socket directory; exact IPv4, IPv6, and Unix fixtures pass, while separately held unrelated loopback and Unix listeners are denied and receive no connections. The structural guard still inspects provider construction and all non-production binaries, with a non-skipping Windows inspector fallback. The Linux namespace launcher has no target-native record and Windows intentionally launches no tests. Disposable `MikeMarcin/BPR-Test#1` is now bound to repository ID `1328277631`, pull-request ID `4246446933`, node ID `PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, and head `17990ba0c2fb50338141f35c0717f0f99639c7ba`; the single live lane and its no-launch mismatch matrix do not yet exist. BPR-0235 remains open. |

## Standing composed-host linkage audit

BPR-0248 records the fixture/production boundary in the binary verifier. The
shared inventory requires the exact demangled
`bpr::application::review_mutation_coordinator` constructor in both binaries,
because the shared component definition may be linked without being
constructed. The development-fixture profile additionally requires the exact
test-only
`bpr::app::detail::github_public_path_fixture_components::review_mutation_provider`
boundary and `bpr::application::reconcile`; the production profile forbids
both fixture-only symbols. Both profiles forbid the exact
`bpr::adapters::github::live_review_mutation_provider` type until BPR-0142
lands.

Those results prove only what each binary links. In particular, the shared
coordinator constructor is not evidence that production constructs a mutation
authority. The real fixture editor, core proposal, coordinator, server refresh,
and Qt projection trace remains the construction and runtime evidence for
BPR-0229. The production absence of the fixture provider boundary is not
evidence of a live provider; the same audit requires that provider to remain
absent. Missing binaries, unsupported tool chains, nonzero tools, failed
demanglers, and malformed or zero-defined symbol tables fail closed. Only
parsed defined records count; `U`/`UND` references and anchors embedded in
surrounding template text do not. Windows declaration prefixes are accepted
only through the first allowlisted calling convention outside balanced
template, parameter, and array delimiters; nested conventions do not select the
anchor boundary and unbalanced declarations fail closed. Absolute, common,
debug, and unknown symbol kinds or sections do not count. CTest treats the
verifier's exit status as authoritative.

## Deferred gates

Three acceptance criteria cannot be satisfied on this host, and each names
exactly what is missing.

| Card | Gate | What is missing |
|---|---|---|
| BPR-0142 | A comment is readable and replyable in the GitHub web UI with no tool-specific marker | A session-scoped write token for the already designated safe repository, plus a human to read the web UI |
| BPR-0143 | Mixed-client acceptance: external edit, delete, resolve, reopen | The designated repository, session authority, and a second client acting on it |
| BPR-0144 | Reviews appear under the authenticated user's identity with ordinary semantics | A real account and token; the fixture proves the identity is carried, not that GitHub honours it |

The product owner designated `MikeMarcin/BPR-Test` before any run. Session
authority and the required human/mixed-client actions remain unexercised. The
repository is bound to GitHub database ID `1328277631` and node ID
`R_kgDOTyvofw`. On 2026-08-10 the product owner also authorized and designated
disposable PR `#1`, database ID `4246446933`, node ID
`PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, and head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`, before any BetterPullReview live
child or review mutation ran. BPR-0235's macOS default-deny boundary does not
authorize a live provider, implement the exact-target exception, or substitute
for its deferred Linux/Windows evidence.

## Card allocation

| Card | Slice |
|---|---|
| BPR-0134 | This planning slice |
| BPR-0135 | Core draft, edit, discard, and publish state with typed legality |
| BPR-0136 | Core target and head revalidation, and the stale-head refusal |
| BPR-0137 | Host mutation coordinator: safe-target enforcement, idempotency, retries |
| BPR-0138 | Host synchronization, refresh cursors, and external-change reconciliation |
| BPR-0139 | Qt draft editors, publish confirmation, and the no-bypass proof |
| BPR-0140 | Documentation: mapping, drafts, publish, identity, stale head, retries, capability fallback, safe-test policy, mixed-client contract |
| BPR-0141 | Simplification and closeout preparation |
| BPR-0229 | Shipped editor-to-core-to-fixture-coordinator-to-server-refresh composition and non-bypass executable trace |

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Review, comment, and thread mapping; drafts; publish semantics; identity; stale-head behaviour; retries; capability fallback; safe-test policy; mixed-client contract | [Review Round Trip](../architecture/review-round-trip.md) | Written. Records ownership, the command/decision/proposal/request/completion/observation/delta vocabulary, the draft lifecycle, semantic anchoring, the two unequal target mismatches, safe-target enforcement, identity, idempotency, synchronization and the mixed-client contract, capability fallback, and what is not proven without a live target |
| Private review ledger boundary | [Review Ledger](../architecture/review-ledger.md) | No impact. The ledger holds private progress and this RFC holds shared state; the two share no table and no authority. Recorded rather than assumed |

The document names the safe mutation target and states that no other target is
reachable. It also records that weakening the coordinator's target check to let
a test through would remove the property the test exists to exercise. BPR-0235
adds the process-boundary isolation the current structural inventory lacks.

## Closeout blockers

RFC-BPR-0008 cannot become `Auto-Verified` until RFC-BPR-0007, RFC-BPR-0005,
RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are, and therefore not before real
Windows x64 evidence in BPR-0036 and BPR-0053. It additionally cannot close
before BPR-0142, BPR-0143, BPR-0144, and BPR-0235. The repository is
designated; session authority, human/mixed-client execution, and durable
default-deny isolation remain open. BPR-0141 will make no closure claim.
