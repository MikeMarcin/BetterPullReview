---
schema: "implement-rfc.card.v1"
id: "BPR-1645"
title: "Keep relocated release smoke offscreen"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"6e610e44dc02ab4e25b75d061b0755549a318d96","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1645: Keep relocated release smoke offscreen

The coordinator stopped its owned release driver before launch after finding
that verify_relocated_launch removes QT_QPA_PLATFORM. The tests-disabled
self-test shows its QApplication window, so that invocation could drive the
owner's live Mac desktop contrary to AGENTS. No actual smoke launch or signing
is claimed from the interrupted driver.

## Acceptance

- AC1: Use the existing offscreen Qt mechanism explicitly for the relocated self-test, overriding ambient Cocoa while retaining isolated HOME, exact self-test/budget arguments, accepted status0/1 and private-profile/bundle-identity predicates. Keep native Cocoa launch qualification separate and pending.
- AC2: Execute the real shell helper with a synthetic executable that observes effective environment/argv, and reject missing private-profile or wrong bundle identity and unexpected child failure without build, signing, Keychain or owner-desktop operations. Verify shell syntax, focused script tests and documentation.
- AC3: Commit intentional source/test/docs paths and retain a source-bound actual coordinator offscreen relocated smoke before claiming native release readiness. Keep original gate failures, configured skips, native foreground qualifications and trust/notarization checks unchanged; no signing or publication delegated.

## Source repair and command evidence

The helper now explicitly sets QT_QPA_PLATFORM=offscreen and removes only
QT_STYLE_OVERRIDE. It retains the exact isolated HOME and application arguments,
status0/1 handling and both original profile/bundle markers. A success log names
the offscreen scope; no C++/UI/trust/signing/notary code changes.

Four real shell-helper tests pass with a synthetic executable in an owned
private temporary root: actual environment overrides ambient Cocoa and style,
HOME/arguments are exact, both0/1 complete, and missing profile, wrong identity
and unexpected exit2 refuse. Bash syntax, documentation and diff checks pass.
The coordinator owns the interrupted driver and its fresh exact-source actual
producer smoke. This card stays In Progress for that observed native result;
no native Cocoa qualification is claimed.
