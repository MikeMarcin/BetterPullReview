# RFC-BPR-0008 Verification Plan and Ledger

RFC base: `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`

Status: implementation in progress. The core draft policy, target
revalidation, and the host mutation coordinator have landed (BPR-0135 through
BPR-0137). RFC-BPR-0008 stays `Proposed`.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007. All three are
implemented and verified on available targets; all three remain `Proposed`
because closure is strictly ordered behind RFC-BPR-0002's real Windows x64
evidence. Done BPR-0088 records the product owner's direction to advance the
implementation frontier on available targets. Closure order is unchanged.

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
| External changes round-trip | Host synchronization | Apply an externally created, edited, and deleted comment to the projection | Each is reflected after refresh, and a local assumption never overwrites an unknown external change | Planned |
| The mutation path cannot bypass the host | Qt plus trace | Drive every publish and resolve action | Every action produces a core proposal, then a host coordination, then a result event; no widget holds a provider | Planned |
| Capability fallback is explicit | Host | Offer a provider without native resolve or viewed-state | The degradation is reported in typed form rather than silently skipped | Verified. BPR-0137 reports a provider that resolves no threads and records no viewed state rather than skipping the operation, which would leave a reviewer believing they had resolved something |
| Acting identity is carried | Host | Publish under a fixture identity | The proposal and the request carry the active profile's identity | Verified for the request. BPR-0137 asserts the submitted request carries the acting identity and the coordinator's own designated target, so no caller can name a destination. Whether GitHub attributes the review to that account is BPR-0144 |
| No automated test mutates an undesignated target | Whole suite | Scan the suite's provider configuration | Every mutation test uses the fixture provider; no test names a live host | Planned |

## Deferred gates

Three acceptance criteria cannot be satisfied on this host, and each names
exactly what is missing.

| Card | Gate | What is missing |
|---|---|---|
| BPR-0142 | A comment is readable and replyable in the GitHub web UI with no tool-specific marker | A designated safe repository, a write-scoped token for it, and a human to read the web UI |
| BPR-0143 | Mixed-client acceptance: external edit, delete, resolve, reopen | The same designated repository plus a second client acting on it |
| BPR-0144 | Reviews appear under the authenticated user's identity with ordinary semantics | A real account and token; the fixture proves the identity is carried, not that GitHub honours it |

The product owner must designate the repository and supply the token. Nothing
in `docs/` designates one today, and no slice here will invent one.

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

## Closeout blockers

RFC-BPR-0008 cannot become `Auto-Verified` until RFC-BPR-0007, RFC-BPR-0005,
RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are, and therefore not before real
Windows x64 evidence in BPR-0036 and BPR-0053. It additionally cannot close
before BPR-0142, BPR-0143, and BPR-0144, which need a designated repository, a
token, and a human. BPR-0141 will make no closure claim.
