---
schema: "implement-rfc.card.v1"
id: "BPR-0297"
title: "Correct RFC-BPR-0017 findings against the live read path"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: []
---

# Correct RFC-BPR-0017 findings against the live read path

## Required final state

- The RFC states that the current catalog has no REST review-comment operation;
  GraphQL is presently the only inline review-comment/thread source.
- The RFC records credential-mandatory native HTTPS and nonzero viewer/account
  host assumptions as implementation work. It distinguishes the selected
  `perform_paged_github_read` path—which already skips GraphQL for an all-false
  shape—from the tests-only fixed-ten/full-shape recorded-contract helper.
- Eligibility sequencing allocates BPR-0313's bounded test-only credential-free
  native-HTTPS demonstration before the decision and product implementation,
  distinguishes non-product probes from product code, and
  names how the typed credential-free transport demonstration can be obtained
  without silently beginning a partial product mode.
- The recommendation and `do not build` branch remain intact; no service fact,
  network authority, dependency exception, or implementation is inferred.

## Completed evidence

The corrected RFC is the whole deliverable; no source changed.

### What the correction establishes

- The read catalog has no REST review-comment operation. `list_issue_comments`
  resolves to the issue-level path, and the read contract builds no
  `/pulls/{n}/comments` path at all, so GraphQL is the read path's only source
  of inline review comments — not merely of their resolution state. The
  practical consequence is larger than the earlier text implied: refusing the
  GraphQL operation leaves an anonymous view with no source-anchored comments,
  not a flat list missing a flag.
- The mutation authority is recorded as the explicit exception. It both writes
  and reads `/pulls/{n}/comments`, and already normalizes the response into
  `review_comment_projection`, but only from a private reconcile inside a
  submit-and-reconcile sequence that neither the read catalog nor
  `perform_paged_github_read` can select. Recording this is what stops the
  mutation-side read from being mistaken for a read-path source later.
- Credential-mandatory native HTTPS and nonzero viewer/account identity are
  recorded as implementation work, evidenced by the two factories, the absence
  of any header-omitting branch, and the session read model returning
  `missing_identity` for a zero viewer account.
- The selected `perform_paged_github_read` path, which returns before building
  a GraphQL body for an all-false shape, is distinguished from the tests-only
  `perform_normalized_github_read` helper, which iterates the whole catalog and
  hardcodes an all-true shape against a fixed ten-interaction manifest.
- The two public observers are distinguished. The `curl` one is a
  verification-only probe. `native_designated_review_public_observer` is
  product-compiled and does perform a credential-free GET against
  `api.github.com`, but it duplicates the transport's TLS and redirect policy
  instead of exercising it and skips system-proxy discovery entirely, so it
  narrows the eligibility question without closing it.
- Eligibility sequencing now allocates BPR-0313's bounded test-only
  demonstration before BPR-0299's product decision, matching the live
  `depends_on` graph, names `scripts/verify_rfc0017_test_inventory.py`, and
  states that the symbol-absence check must be written against the existing
  credential-free observer rather than against an assumption that none exists.

### Independent review

Two review rounds. The first returned three blocking findings, all of them
false statements inside the section the RFC labels as verified against the
tree, and all of them mine: that the only `/pulls/{n}/comments` path was a
write; that GraphQL was the only inline review-comment source in the tree
rather than on the read path; and that the product observer satisfied none of
the RFC-BPR-0002 redirect, certificate, and proxy guarantees when in fact it
duplicates the first two and refuses redirects more strictly than the
transport does.

The second round found the observer correction had been applied in the
findings section while the original false phrase survived verbatim in the
eligibility gates, leaving the document contradicting itself. It also found
three citation defects: a `Basic` anchor off by one, a viewer-gate anchor
pointing at a predicate rather than at session acceptance, and a claim that the
mutation-side GET happens only after its own write when it also runs as a
pre-write baseline. All were reproduced against the source and corrected.

Two review observations were deliberately not acted on: the availability field
is thread-granular rather than comment-granular, which only matters if a future
REST review-comment operation lands without threads; and the new
unavailable-versus-empty matrix row overlaps in spirit with the existing
unavailable-versus-defaulted row, which is a readability nit rather than a
contradiction.

### Gates

- `python3 scripts/verify_documentation.py --root .` → `DOCUMENTATION_OK`.
- `git diff --check` clean; every relative link resolves and every line anchor
  is within its target file; no prose line exceeds 80 columns.
- The `## Recommendation` section, including the `Do not build it at all…`
  branch, is byte-identical to its pre-change state, verified by extraction and
  comparison rather than by reading the diff.

### What this does not establish

No service fact was resolved and none is asserted; every GitHub-side question
remains a claim for BPR-0298's probe evidence. No network authority, dependency
exception, or implementation was granted or begun, and RFC-BPR-0017 remains
`Deferred`. The reviewer's standing caution applies: none of the repository's
recorded commands could have distinguished the three blocking false statements
from true ones, so the documentation gate passing is not evidence that the
prose is correct.
