---
id: BPR-0252
title: Implement the live review mutation provider contract
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0229]
---

# Implement the live review mutation provider contract

BPR-0142 cannot start while BPR-0235 remains in progress, but its first
production provider and mutation protocol can be implemented and proved
without a credential, network access, composition, or a live write. This card
owns that preparatory portable slice so the remaining attended lane is small
and does not weaken BPR-0235's fixed harmless canary.

## Required final state

- The contracted `bpr::adapters::github::live_review_mutation_provider` and
  `make_live_review_mutation_provider` exist at the exact paths reserved by the
  standing isolation verifier and are linked into the production GitHub
  adapter.
- The application-to-provider request binds one accepted pull request and one
  semantic source-line proposal to the exact canonical owner/name, repository
  and pull-request identities, pull-request number, open state, expected head,
  acting identity, file path, commit, side, line, body, and idempotency key
  required for an ordinary GitHub pull-request review comment.
- Mutation wire compilation and response parsing are separate from the
  read-only GitHub request contract. They are bounded, canonical, reject
  redirects and malformed or mismatched target/identity/anchor data, and never
  include a credential in errors or diagnostics.
- `native_https_transport.cpp` remains the only Qt network effect owner. The
  provider and its contract do not construct `QNetworkAccessManager`, read
  process environment, or acquire credentials directly.
- Hermetic tests prove exact observe-before-submit and immediate pre-write
  revalidation, request bytes and headers, successful receipt identity,
  duplicate/idempotency handling, typed server/refusal/cancellation outcomes,
  hostile response bounds, and zero transport submissions for every invalid
  request.
- The existing fixture coordinator/public-path contracts and BPR-0235
  canary/isolation gates remain green.

## Non-goals

- Do not compose the provider into the shipped application in this card.
- Do not change or execute the designated live-mutation CTest exception.
- Do not acquire, log, persist, or use a write token.
- Do not make a network request or claim BPR-0142's web-UI evidence.
- Do not substitute a generic issue comment for the selected source-line
  review comment.

## Verification

- Focused application mutation/coordinator tests.
- Focused GitHub mutation-contract/provider/native-transport tests.
- `live_mutation_isolation` and `live_mutation_isolation_contract`.
- `architecture_boundaries` and `architecture_boundary_contract`.
- Documentation validation, scoped format/tidy, and `git diff --check`.

