---
id: BPR-0610
title: Compile and validate whole-review submission wire contracts
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0142]
---

# Compile and validate whole-review submission wire contracts

BPR-0144 revealed that the shipped adapter only creates source-line comments;
non-comment dispositions are refused. Introduce an explicit whole-review
wire contract as the first implementation slice. A review summary is separate
from a selected source-line draft, avoiding discarded anchors or duplicated
summary/inline prose. Existing source-line publication keeps its semantics.

## Ownership and scope

Own `src/adapters/github/src/github_mutation_contract.hpp`, its `.cpp`, and
`tests/github_contract/github_mutation_contract_tests.cpp`. Add a typed
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
