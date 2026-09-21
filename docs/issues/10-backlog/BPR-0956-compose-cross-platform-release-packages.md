---
schema: "implement-rfc.card.v1"
id: "BPR-0956"
title: "Compose cross-platform release packages"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0952","BPR-0955"]
milestone: "M6 Composition and public-path integration"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Compose cross-platform release packages

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Implement the complete install/export/package graph for Windows per-user x64, macOS 13+ universal and the declared oldest-supported Linux x64 DEB/RPM baseline, retaining an independently buildable Qt/provider-free core.
- AC2: Reuse the macOS release path and product identity; produce exact-artifact checksums, SBOM, dependency provenance, runtime notices and source-offer obligations while collecting only declared required runtime libraries/plugins.
- AC3: Exercise each available tests-disabled package through the real composition to a fixture PR and exact selected-file diff with one alignment controller and DiffLinkMap; preserve restricted install paths and the qualified macOS-only updater exception.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
