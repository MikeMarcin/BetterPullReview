---
schema: "implement-rfc.card.v1"
id: "BPR-1453"
title: "Verify and close RFC-BPR-0080"
status: "Backlog"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446", "BPR-1447", "BPR-1448", "BPR-1449", "BPR-1450", "BPR-1451"]
priority: "P2"
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
closeout_for: ["RFC-BPR-0080"]
---

# BPR-1453: Verify and close RFC-BPR-0080

RFC base: `5620a9f15a391dde7750112294f7562f2f64a32f`.

## Acceptance

- **AC1:** Reconcile every RFC criterion against the immutable baseline, with
  the owner's Q1, Q2 and Q3 answers recorded verbatim with dates.
- **AC2:** Focused integration and documentation checks pass; the report's
  decisions, the cards and the architecture text agree.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** A committed report records raw results, hardware, SHA-extension flags and builds, and applies the decision rules. It gives one adopt, reject or pending decision per component, and records the owner's answers verbatim with dates. | Pending. |
| **AC02:** Each adopted implementation shows zero mismatches across the known answers, the differential runs, the full repository object sweep and the existing digest-using rows. Data persisted before adoption still loads and matches. | Pending. |
| **AC03:** With sha1dc adopted, every collision input is refused at each listed point while the plain-SHA-1 control collides. Ordinary objects, and pack checksums of valid packs, are still accepted. | Pending (Q1). |
| **AC04:** Measurements on each available target support every adopt and reject decision. Each rejection names the failing measurement or the owner answer. | Pending. |
| **AC05:** The window-thread rows pass, with before-and-after results and counters recorded. No adopted change makes a recorded row worse by more than its run-to-run spread. | Pending. |
| **AC06:** `core-only` and `dev` build. The architecture checks admit only the approved pure targets in core. Any platform crypto sits in an adapter behind the host port. | Pending. |
| **AC07:** For each adopted library, the release inventory, provenance, notices, SPDX and corresponding source are complete, and the package audit passes on macOS. Unavailable stations are recorded explicitly. | Pending. |
