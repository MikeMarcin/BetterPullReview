---
schema: "implement-rfc.card.v1"
id: "BPR-1449"
title: "Run the hash evaluation on the Linux station"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1449: Run the hash evaluation on the Linux station

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`station-evaluations`.

## Outcome

The BPR-1446 evaluation target runs on the Linux station (Comet Lake, expected
without SHA-NI, so it measures fallback paths), with a system libcrypto EVP
SHA-1 and SHA-256 arm added to the evaluation. The report gains the Linux rows.

## Acceptance

- **AC1 (RFC AC02, AC03):** The correctness gate and collision checks pass on
  Linux (GCC) for sha1dc, HACL* and libcrypto.
- **AC2 (RFC AC04):** W1 to W3 medians of 7 in a Release build, with
  `/proc/cpuinfo` `sha_ni`, the OS and compiler, and the decision rules
  re-applied with the Linux rows.
- **AC3:** The libcrypto backend's rule outcome is recorded. If it passes, RFC
  Q3 (link the system libcrypto directly) goes to the owner with the
  measurement, and the BPR-1447 port gains the backend only after an Accept;
  otherwise Linux keeps core.
