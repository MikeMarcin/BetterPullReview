---
schema: "implement-rfc.card.v1"
id: "BPR-0946"
title: "Bind updater core policy and host contract"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: []
milestone: "RFC-0035 milestone 1"
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Bind updater core policy and host contract

## Scope

First authored milestone of [[BPR-0911]]. Implement deterministic release,
version, exact universal ZIP selection, scheduling and command policy, plus a
narrow application host exercised with fake ports. No production transport,
credential retrieval, UI, package replacement or release publication.

## Acceptance

- AC-01: Bind the fixed private distribution source, native GitHub.com release
  access, API/redirect contract, version grammar, exact asset identity and limits.
- AC-02: Core tables reject malformed/equal/older/prerelease/draft releases and
  ambiguous/incompatible assets; candidate identities include release, tag,
  asset, size and optional digest.
- AC-03: Deterministic time facts implement daily and manual admission, failed
  attempt spacing, rate limits, Later, exact-version Skip, disabled/managed and
  unsupported capabilities. No policy reads clocks or performs effects.
- AC-04: The public host with fake narrow ports exercises check, offer, download,
  verification and failure projections; every async completion is bound to its
  operation/generation/access/candidate. Account replacement, disable, cancel and
  shutdown reject stale and duplicate completion. Install stays unavailable
  until a qualified installer is composed in milestone 3.
- AC-05: Core-only build, focused policy/host checks, architecture boundaries and
  documentation gate pass; no product-shipping or native-upgrade claim is made.

## Evidence

Claim pending. RFC AC-01/03/04/08 receive bounded component evidence here; full
UI, native private-service and installation acceptance remains on [[BPR-0912]].
