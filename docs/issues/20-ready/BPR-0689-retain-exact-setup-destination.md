---
schema: "implement-rfc.card.v1"
id: "BPR-0689"
title: "Retain exact setup destination"
status: "Ready"
rfcs: ["RFC-BPR-0018", "RFC-BPR-0026"]
depends_on: ["BPR-0679"]
---

# Retain exact setup destination

## Scope

Introduce the typed default-environment setup destination boundary shared by RFC-0018 and RFC-0026. The current locator drops URL hostname and opens under active authority. Preserve exact host/repository/PR/request identity; authenticate/configure only the intended server, return only its current pending destination on accepted setup, and leave current review usable on cancellation. No cross-host silent fallback.

Owned paths: Core destination model/parsing/transition; application setup coordinator/projection; Qt locator translation; composition and focused tests.
Routing: standard | Reason: default implementation (Astra low delegated route).
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-14 and RFC-0026 AC-09: signed-out/unknown-server request, success, cancel, new target, authority replacement and stale completion are exercised through a recording composed path. Typed core/host ownership; Qt does not decide host authority.

## Evidence

Read-only BPR-0683 acceptance scout identified this concrete missing behavior.
Not yet implemented or checked. This bounded bridge is needed for Settings
acceptance; it does not activate unrelated browser tabs or claim full RFC-0026.
