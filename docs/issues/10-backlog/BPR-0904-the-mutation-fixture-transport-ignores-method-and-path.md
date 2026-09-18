---
schema: "implement-rfc.card.v1"
id: "BPR-0904"
title: "The mutation fixtures' transport ignores method and path, so no row can fail a wrong request URL"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-007 scout, surveying BPR-0896","found_at":"424a64cc","run":"run-20260916-stations","dispatch":"dsp-007"}
---

# BPR-0904: The mutation fixtures' transport ignores method and path, so no row can fail a wrong request URL

## Defect

This card exists because of how [[BPR-0896]] was found: by reading, during
other work, and not by any test. It could not have been found by a test.

`fake_transport` in the review-mutation suites answers from a scripted queue of
completions and **ignores the request's method and path entirely**. A fixture
therefore cannot fail because the product asked the wrong server for the wrong
thing; it can only fail on what it does with the answer it is handed. The
scripted answer arrives whatever was asked.

The consequence is measurable rather than theoretical. Across the whole
mutation surface, exactly **two** assertions pin a request path:

- `tests/github_contract/github_mutation_contract_tests.cpp:635` — the review
  submission list GET,
  `/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100`;
- `tests/github_contract/live_review_mutation_provider_tests.cpp:568` — the
  REST root-comment reconciliation, asserted only as
  `ends_with("/comments?per_page=100")`, inside
  `test_observe_then_immediate_revalidate_then_submit`.

[[BPR-0879]]'s thread observation additionally pins its GraphQL body and both
accepted roots at `github_mutation_contract_tests.cpp:437-459`, which is the
shape the rest of the surface should have and does not.

Everything else — every reply, every resolve and unresolve, every
reconciliation derived from a mutation's own path — is unpinned. [[BPR-0896]]
is what that costs: a request that no real server can answer, on the operation
a reviewer uses most, sitting in a suite that was green throughout.

There is a second layer to it. `validate_outbound_review_mutation_request`
(`src/adapters/github/src/github_mutation_contract.cpp:1424-1462`) exists to
catch exactly this class of fault on egress, but it validates by **recompiling
the canonical form and comparing**. When the compiler is wrong the canonical
form is wrong in the same way, so the malformed request passes its own
self-check. A validator that reproduces the thing it validates cannot fail.

## Scope

- Decide what the fixture transport should do about a request it was not
  scripted for. The two candidates are opposite in spirit: make the queue
  keyed by method and path so an unexpected request has no answer and the row
  fails, or keep the queue positional and require rows to assert `Wire.Method`
  and `Wire.Path` themselves. The first makes every existing row a path pin
  for free and will surface unknown breakage on the first run; the second is
  smaller and pins only what someone remembers to pin. Measure how many rows
  the first breaks before choosing it — that number is the real cost, and it
  is also the most interesting thing this card can produce.
- Whichever is chosen, every mutation operation ends with its request shape
  pinned: method, path, and for a GraphQL root the operation name in the body.
  Enumerate the operations from `review_mutation_operation`
  (`github_mutation_contract.hpp:29-44`) rather than from a list in prose, so
  a new operation added later is visibly unpinned.
- Separately, decide whether `validate_outbound_review_mutation_request` can
  be made to validate against something other than its own recompilation. If
  it cannot, say so on the card and stop calling it a validation of the
  request — its real guarantee is that the request was not mutated after
  compilation, which is a narrower and still useful claim, but it is not the
  one its name makes.

## Acceptance

- A wrong request path fails at least one row. Demonstrate it by making the
  [[BPR-0896]] defect (or an equivalent deliberate breakage) red, then green.
- Every operation in `review_mutation_operation` has its request shape pinned,
  or the card names the ones that do not and why.
- The number of rows affected by the chosen approach is recorded, not
  estimated.
- The egress validator's real guarantee is stated accurately wherever it is
  described.
