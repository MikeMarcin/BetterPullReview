---
id: BPR-0558
title: Expose public preview through the ordinary session controls
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0316]
---

# Expose public preview through the ordinary session controls

## Finding and ownership

BPR-0308 found that `activate_public_preview` has only automated-run callers
in `main.cpp`. The normal session controls can sign in from preview but cannot
enter preview. Thus BPR-0316 proves the composed mode and Release selection,
not a discoverable ordinary user entry. This is required product-path repair,
not a new anonymous capability. Claimed by the integrator on 2026-09-04.

## Required final state

- A visible accessible Public preview control uses the current selected
  profile and the existing semantic intent/host path without a credential
  prompt. No Qt provider/vault effects or implicit auto-sign-in are added.
- macOS GitHub.com activates with zero requests; GHES refuses and other
  platforms remain unavailable. CLI choices do not become enabled.
- The composed preview course enters through that real control, proving
  mode activation and existing normal public read/diff behavior. The shipped
  tests-disabled probe also exercises the ordinary control path.
- A focused Qt assertion checks the emitted selected profile/mode. Existing
  preview, composition, Release, format/tidy and docs checks pass.

## Non-goals

No live service reads, credential operations, mutation authority, device-session
persistence, platform enablement beyond BPR-0316, or unrelated UI redesign.
