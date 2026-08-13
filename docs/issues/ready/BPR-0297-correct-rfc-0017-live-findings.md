---
id: BPR-0297
title: Correct RFC-BPR-0017 findings against the live read path
status: Ready
rfc: RFC-BPR-0017
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
