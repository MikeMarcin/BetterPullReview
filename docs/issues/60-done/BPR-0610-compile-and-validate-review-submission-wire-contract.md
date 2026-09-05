---
schema: "implement-rfc.card.v1"
id: "BPR-0610"
title: "Compile and validate whole-review submission wire contracts"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0142"]
---

# Compile and validate whole-review submission wire contracts

BPR-0144 revealed that the shipped adapter only creates source-line comments;
non-comment dispositions are refused. Introduce an explicit whole-review
wire contract as the first implementation slice. A review summary is separate
from a selected source-line draft, avoiding discarded anchors or duplicated
summary/inline prose. Existing source-line publication keeps its semantics.

## Ownership and scope

Own `src/adapters/github/src/github_mutation_contract.hpp`, its `.cpp`, and
`tests/github_contract/github_mutation_contract_tests.cpp`, the central Glaze
provider and dependency registration, GitHub target linkage, Glaze license and
provenance, and focused dependency checks. The owner requested Glaze during
implementation. Pin Glaze v8.3.0 and use typed JSON parsing/serialization for
this new contract. Retain existing structural preflight bounds/duplicate-key
checks; do not add a parallel hand-written numeric parser or migrate unrelated
Qt parsers in this slice. Add a typed
adapter-private review submission input with target, acting identity,
disposition and summary body. Compile ordinary REST `POST /pulls/{n}/reviews`
with explicit exact head, event and literal body; never create a pending review.
This slice does not select the new contract in the live mutation session or UI.
Application submission policy, durable indeterminate recovery, provider
selection, Qt controls and real-account evidence remain BPR-0144 work.

## Acceptance criteria

- Comment, Approve and Request Changes compile to `COMMENT`, `APPROVE` and
  `REQUEST_CHANGES`; unknown dispositions are rejected. Comment and Request
  Changes require a nonblank summary; Approve permits an empty summary.
- Validate complete target and actor bindings, matching server contract, open
  target, exact head and bounded valid UTF-8 body. Canonical outbound validation
  rejects changed method, path, headers, body or operation/semantic pairing.
- Parse an ordinary successful review response into a distinct review
  observation, retaining provider review identity separately from comment IDs.
  Require exact PR URL, author ID/node/login, commit, body and resulting state
  (`COMMENTED`, `APPROVED`, `CHANGES_REQUESTED`) and submitted state. Reject
  malformed, oversized, redirected, wrong-status or mismatched responses.
- Fixture tests prove all dispositions, empty-approve behavior and hostile
  request/response counterexamples. Existing line-comment tests stay green.

## Verification

Build the focused `github_mutation_contract` test target and run
`ctest --test-dir build/dev --output-on-failure -R '^github_mutation_contract$'`.
Run scoped format checks, documentation structure and `git diff --check`.
This is direct wire-contract evidence; no live review or shipped UI claim.

## Protocol reference

[GitHub create-review contract](https://docs.github.com/en/rest/pulls/reviews?apiVersion=2022-11-28#create-a-review-for-a-pull-request),
consulted 2026-09-05: explicit event submits the review, `body` is required
for Comment and Request Changes, and creation responds with status 200.

## Parser correction and owner preference — 2026-09-05

The initial Qt-based candidate reproduced malformed `user.id` rounding:
`22.0000000000000001` became the expected integer `22`. Raw response probes
also cover malformed surrogate text and review IDs. These must pass using
Glaze before completion; an earlier green fixture run is superseded.

Closeout review runtime: native collaboration (gpt-6-astra, high).

## Completion — 2026-09-05

Implemented in `c304442f`. The new whole-review contract compiles all three
explicit dispositions with typed Glaze v8.3.0 JSON and parses exact submitted
review identities without passing integers through floating point. Glaze types
stay private to the adapter. Existing source-line session selection is unchanged.
The owner-supplied Entole provider informed the MIT license verification;
BetterPullReview retains its pinned-archive provider convention.

Validation on the committed source:

- `cmake --preset dev` and focused contract/provider target builds passed.
- `ctest --test-dir build/dev --output-on-failure -R '^(github_mutation_contract|live_review_mutation_provider)$'`:
  2/2 passed in 0.51 seconds. This exercises the new wire contract and the
  existing provider publication path; no live server was contacted.
- `ctest --test-dir build/dev --output-on-failure -R '^architecture_boundaries$'`:
  1/1 passed in 8.29 seconds.
- `DependencyModernizationTests.test_configured_dependency_audit_uses_supported_providers`:
  passed against `build/dev`, including the Glaze provider record.
- Three-file `clang-format --dry-run --Werror`, scoped simplifier review,
  documentation structure and `git diff --check` passed.

Raw fixtures reject fractional ID tokens, overflow and malformed surrogates;
positive fixtures preserve uint64 identities above 2^53 and at UINT64_MAX.
Control characters are escaped on write and survive round trip. Independent
review's initial numeric finding is resolved; its bounded Glaze recheck found
no remaining blocker. Native review was used because no opposite-family role
was callable. No full matrix, native Windows/Linux run or whole-review UI/live
submission is claimed.

BPR-0144 still needs whole-review policy, receipt/recovery, production selection
and UI before real-account evidence. BPR-0612 records the owner's separate
backlog request to replace every remaining Qt JSON parser/serializer.
