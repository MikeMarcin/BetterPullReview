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
- The RFC records credential-mandatory native HTTPS, nonzero viewer/account
  host assumptions, all-ten-response normalizer admission, and hardcoded full
  read shape as implementation work rather than existing capability.
- Eligibility sequencing distinguishes non-product probes from product code and
  names how the typed credential-free transport demonstration can be obtained
  without silently beginning a partial product mode.
- The recommendation and `do not build` branch remain intact; no service fact,
  network authority, dependency exception, or implementation is inferred.
