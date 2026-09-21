---
schema: "implement-rfc.card.v1"
id: "BPR-0949"
title: "Qualify updater packages and document release security"
status: "In Progress"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0948"]
milestone: "RFC-0035 milestone 4"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Qualify updater packages and document release security

Fourth authored milestone of [[BPR-0911]]. Obtain target-native upgrade evidence
and complete release/security/installation/recovery documentation. Windows/Linux
self-installation remains future scope; manual fallback is required now.

## Acceptance

- AC-01: An older signed package upgrades to the offered signed package on Apple
  Silicon and Intel, relaunches with retained user state, and reports truthful
  progress/result. Bind artifact digests, candidate revisions and actual platforms.
- AC-02: Document authority, destinations/redirects, bounds, helper inventory,
  signing/notarization, state ownership, cleanup and recovery. Explicitly amend
  RFC-0014 and packaging's no-updater statements only for qualified macOS packages.
- AC-03: Reconcile every RFC criterion with direct evidence; unavailable native,
  human or external authority stays open. No live private access or publication is
  implied. Finish [[BPR-0911]] only once its complete acceptance is met.

[[BPR-0912]] owns dependency-ordered closeout. RFC status cannot bypass RFC-0014.

## Verification and execution record

M3 mandate-r2 handoff: [[BPR-0948]] supplies exact Apple Silicon disposable signed
upgrade/timeout/receipt/recovery evidence and the enabled installer source. Reuse
the [ledger](../../verification/RFC-BPR-0035.md); do not repeat unchanged evidence
or infer Intel/live private-service acceptance. The final unpublished 0.0.352
candidate and recoverable qualification branch are recorded on [[BPR-0912]].
No M4 work has been claimed. Preserve the native two-window finding of duplicate
Check for Updates menu entries for this milestone's public-path review, along
with the existing self-service recovery/operator/security obligations. Publication,
tags, pushes and installed/developer-app replacement remain unauthorized.
