---
id: BPR-0558
title: Expose public preview through the ordinary session controls
status: Done
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

## Completed — 2026-09-04

The session panel now exposes an accessible Public preview control after
Connect in keyboard order. It emits exactly one existing profile-activation
intent with the selected profile and explicit public-preview mode. Composition
keeps CLI on its typed unavailable path, routes native preview before the
credential prompt, and leaves GHES refusal to the existing host policy.
The automation helper selects the exact native profile and clicks this same
control, so neither fixture nor tests-disabled probes bypass the user entry.

The connection and locator actions reuse the existing compact-layout helper
from the window's resize event. The integrator inspected the final 640px
render: the new control is visible and the existing minimum-size overflow
remains in its scroll surface. No unrelated workbench redesign is claimed.

Verification: dev and tests-disabled Release builds passed. The worker ran
12 selected tests: `qt_(composed_github|anonymous_public_preview)_scale_(100|125|200)`
and `application_anonymous_public_preview_(public_path|unavailable|release_contract|ghes_refusal|enabled_probe|live_candidate_wiring)`,
all passing. Both composition contracts passed (167.64 s). Tests-disabled
Release probes under network denial first proved CLI unavailable, then
GitHub.com enabled or GHES refused, with zero request/header/vault/sign-in
effects from fresh start. Scoped strict tidy, format, architecture boundaries,
code-simplifier, and diff checks passed. All 54 documentation contract tests
passed; the structure gate passes after this card's terminal move.

The integrator inspected all source changes and the rendered layout and
reproduced `ctest --preset dev -R
'^(qt_anonymous_public_preview_scale_(100|125|200)|application_anonymous_public_preview_(public_path|unavailable|release_contract|ghes_refusal))$'
--output-on-failure`: 7/7 passed in 2.16 s. Source pins bind the inspected app
changes. No live service or operator credential was used. Linux/Windows were
not run natively; their selection remains unavailable. Existing build roots
were reused, and all 13 worker `/tmp/bpr0558-*.log` files were removed.
