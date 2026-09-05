---
schema: "implement-rfc.card.v1"
id: "BPR-0141"
title: "Simplification and closeout preparation for RFC-BPR-0008"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0140"]
closeout_for: ["RFC-BPR-0008"]
---

# Simplification and closeout preparation for RFC-BPR-0008

See the RFC-BPR-0008 verification plan for this slice's place in the sequence.

Every surface here is built against the fixture provider. No slice in this RFC
performs a live mutation; the criteria that need one are BPR-0142, BPR-0143,
and BPR-0144.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- Every refusal carries its own typed reason, so two different mistakes are
  distinguishable by a caller.
- No test reaches a live host.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

**This card makes no closure claim.** RFC-BPR-0008 stays `Proposed`.

### The safety criterion was still unproven at closeout

Row nine — "no automated test mutates an undesignated target" — was still
`Planned` when reconciliation began. It is the RFC's own safety criterion, and
closing out around it would have been the worst kind of tidy.

Verifying it produced a stronger answer than expected. `review_mutation_port`
has **exactly one implementation in the tree**, the fake in
`tests/application/review_mutation_tests.cpp`, and no production code
constructs a coordinator. No code path can mutate any repository, designated or
not.

That is recorded **conditionally**, because it holds for a reason that BPR-0142
destroys: it is true only while no live provider exists. A scan that kept
passing after the live implementation landed would be passing for a reason that
no longer applied, which is worse than no scan. BPR-0142 now carries the
requirement to replace the property with one that survives it.

### Claim matrix reconciled

Nine rows, each citing its own card and its own evidence. The result column was
extracted and read alone, as in RFC-BPR-0007, because bulk-editing a matrix has
twice given one row another row's marker. `BPR-0137` appears in four rows and
each citation names different evidence within it.

Two rows are deliberately partial rather than claimed whole:

- External changes round-trip is **verified against the fake**. It establishes
  that the projection handles each shape; it does not establish that GitHub
  produces them. That is BPR-0143.
- Acting identity is **verified for the request**. The identity is carried;
  whether GitHub attributes the review to that account is GitHub's behaviour.
  That is BPR-0144.

### Simplification

Nothing was extracted. The draft policy, the revalidation policy, the
coordinator, the reconciliation, and the editor each own one decision and share
no logic. The `opaque` and `commit` test helpers are duplicated across suites,
which is the pre-existing pattern recorded at RFC-BPR-0007's closeout and not
duplication these slices introduced.

### Replay

`dev` 107/107, `core-only` 44/44, format, tidy, documentation verifier, the
pack validator, a zero workflow count, and `git diff --check` all pass.

### Remaining gaps

Everything left in this RFC needs something this host cannot supply.

| Card | What is missing |
|---|---|
| BPR-0142 | A designated safe repository, a write-scoped token, a human to read the web UI — and the replacement guard above |
| BPR-0143 | The same repository plus a second client |
| BPR-0144 | A real account whose login is known in advance |

The product owner must designate the repository and supply the token. Nothing
in `docs/` designates one, and no slice invented one.
