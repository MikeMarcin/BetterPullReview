# RFC-BPR-0008 Verification Plan and Ledger

RFC base: `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`

Status: **components complete; product composition absent.** BPR-0135 through
BPR-0139 delivered the draft policy, target revalidation, mutation coordinator,
synchronization, and Qt editor against fixture/component surfaces. BPR-0225
found no construction or invocation of those surfaces in the public
application. BPR-0229 owns the missing gesture-to-coordinator-to-refresh path.

RFC-BPR-0008 stays `Proposed`. No card here makes a closure claim, and three
acceptance criteria remain blocked on a designated repository, a token, and a
person. RFC-BPR-0008 stays `Proposed`.

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
| External changes round-trip | Host synchronization | Apply an externally created, edited, and deleted comment to the projection | Each is reflected after refresh, and a local assumption never overwrites an unknown external change | Verified against the fake. BPR-0138 round-trips created, edited, deleted, resolved, and reopened; an edit updates rather than duplicating; and an unpublished local edit colliding with a server change keeps the server's text and reports a conflict rather than resolving it. That GitHub produces these shapes is BPR-0143 |
| The mutation path cannot bypass the host | Qt plus trace | Drive every publish and resolve action | Every action produces a core proposal, then a host coordination, then a result event; no widget holds a provider | Verified. BPR-0139's editor is given two dispatch callbacks and nothing else, so there is no provider to hold; every action emits a command and leaves the editor unchanged until an accepted projection arrives, and enablement is derived from that projection rather than from what was typed |
| Capability fallback is explicit | Host | Offer a provider without native resolve or viewed-state | The degradation is reported in typed form rather than silently skipped | Verified. BPR-0137 reports a provider that resolves no threads and records no viewed state rather than skipping the operation, which would leave a reviewer believing they had resolved something |
| Acting identity is carried | Host | Publish under a fixture identity | The proposal and the request carry the active profile's identity | Verified for the request. BPR-0137 asserts the submitted request carries the acting identity and the coordinator's own designated target, so no caller can name a destination. Whether GitHub attributes the review to that account is BPR-0144 |
| No automated test mutates an undesignated target | Whole suite | `ctest -R live_mutation_isolation` — a standing gate under both `dev` and `core-only` | Three clauses with positive controls: the port's implementations are a subset of a three-path allowlist **and the fake must be found**; no non-production binary in the build-generated manifest carries a `live_review_mutation_provider` symbol **and `review_mutation_tests` must be present and must contain `review_mutation_coordinator`**; and the factory is called from nowhere under `tests/` | Verified by a standing gate rather than by a property that expires. **The condition is discharged.** This row formerly read *"Verified, and conditionally: it holds because no live provider exists yet"* — true, and due to stop being true the moment BPR-0142 landed. BPR-0221 replaced the structural argument with an asserted one before any live code exists, which is the only point at which the guard could be shown to fail on a tree whose answer was known: a rogue implementation compiled into `review_mutation_tests` was caught by both the source and binary halves, a factory call under `tests/` by the third, and a manifest with the control binary removed by the positive control. 87 non-production binaries are covered on macOS `dev`; the four platform-guarded credential-vault targets appear only in the configurations that build them |

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

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Review, comment, and thread mapping; drafts; publish semantics; identity; stale-head behaviour; retries; capability fallback; safe-test policy; mixed-client contract | [Review Round Trip](../architecture/review-round-trip.md) | Written. Records ownership, the command/decision/proposal/request/completion/observation/delta vocabulary, the draft lifecycle, semantic anchoring, the two unequal target mismatches, safe-target enforcement, identity, idempotency, synchronization and the mixed-client contract, capability fallback, and what is not proven without a live target |
| Private review ledger boundary | [Review Ledger](../architecture/review-ledger.md) | No impact. The ledger holds private progress and this RFC holds shared state; the two share no table and no authority. Recorded rather than assumed |

The document states plainly that **nothing in this repository designates a safe
mutation target today**, and that weakening the coordinator's target check to
let a test through would remove the property the test exists to exercise.

## Closeout blockers

RFC-BPR-0008 cannot become `Auto-Verified` until RFC-BPR-0007, RFC-BPR-0005,
RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are, and therefore not before real
Windows x64 evidence in BPR-0036 and BPR-0053. It additionally cannot close
before BPR-0142, BPR-0143, and BPR-0144, which need a designated repository, a
token, and a human. BPR-0141 will make no closure claim.
