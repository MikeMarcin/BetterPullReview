---
schema: "implement-rfc.card.v1"
id: "BPR-0947"
title: "Compose updater download and signed staging"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0946"]
milestone: "RFC-0035 milestone 2"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Compose updater download and signed staging

Second authored milestone of [[BPR-0911]]. Compose one application-owned updater
with real menu and Settings controls, persisted preferences and separately selected
native GitHub.com release-access authority. Implement fixed private release and
asset transport, pinned redirect origins/API version, streamed bounds and native
signed macOS ZIP staging. Preserve existing PR redirect prohibitions.

## Acceptance

- AC-01: Real menu/Settings gestures traverse host/core and incremental Qt status;
  automatic/manual checks, Later, Skip and restart preferences behave as specified.
- AC-02: Private fixture access, revocation and cross-origin secret-canaries prove
  the release-only authority and redirect contract; inaccessible is never current.
- AC-03: Stream/archive/native validation rejects wrong bounds, bundle, version,
  architecture, digest, signer, unsafe entries and changed staged candidates.
- AC-04: Demonstrate fixture menu-to-host-to-projection composition and truthful
  unsupported/managed/manual fallback, without enabling replacement prematurely.

Record RFC AC-01 through AC-04 and AC-08 evidence and native limits on [[BPR-0912]].
