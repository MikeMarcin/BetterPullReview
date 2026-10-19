---
schema: "implement-rfc.card.v1"
id: "BPR-1448"
title: "Run the hash evaluation on the Windows station"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["win"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1448: Run the hash evaluation on the Windows station

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`station-evaluations`.

## Outcome

The BPR-1446 evaluation target runs on the Windows station (expected
i9-10900KF, no SHA-NI), with a CNG `BCrypt*` SHA-1 and SHA-256 arm added to
the evaluation. The report gains the Windows rows; a missing station stays
recorded as missing.

## Acceptance

- **AC1 (RFC AC02, AC03):** The correctness gate and collision checks pass on
  Windows (MSVC) for sha1dc, HACL* and CNG.
- **AC2 (RFC AC04):** W1 to W3 medians of 7 in a Release build, with the CPU,
  SHA-extension flags, OS and compiler, and the decision rules re-applied with
  the Windows rows (sha1dc at least 1.0x core SHA-1, HACL* at least 0.9x core
  SHA-256, CNG at least 0.9x core on a target without SHA extensions).
- **AC3:** If the CNG backend passes, the BPR-1447 port gains it for Windows
  with the window-thread rows re-measured there; otherwise the rejection is
  recorded and Windows keeps core.

## Owner r4 reconciliation — 2026-10-18

[[BPR-1617]] implements the directly selected CNG backend in core. This
supersedes AC3's choice of whether Windows keeps a portable-only core; the
host port still delegates to core off the window thread. The original full
station evaluation, SHA-1/collision sweep, W1-W3 and window-thread rows remain
pending. A focused real Windows SHA-256/MSVC course for the amendment passed
with CNG and forced HACL (known answers, empty/chunked updates, copies and
moves); see the [RFC ledger](../../verification/RFC-BPR-0080.md).
This is partial native correctness evidence, not completion of AC1-AC3.
