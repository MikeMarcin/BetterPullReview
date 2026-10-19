---
schema: "implement-rfc.card.v1"
id: "BPR-1645"
title: "Keep relocated release smoke offscreen"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"6e610e44dc02ab4e25b75d061b0755549a318d96","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
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

## Actual producer failure and bounded follow-up

The coordinator's signed source93897 producer r2 reaches the original signature
guard, then the offscreen smoke aborts134: the intentionally restricted package
contains Cocoa only. It reaches no notary submission or completed distribution
asset. The raw log/receipt and separate partial source ZIP are retained.

The helper now reads only that release build's absolute Qt6_DIR configuration
and refuses a missing cache/configuration/plugin or missing universal slice
before launch. The actual pinned Qt6.11.2 offscreen plugin has x86_64 and arm64
slices and Qt teamA5GTH44LYL; its private staging copy is sealed with the selected
application identity for hardened library validation, or ad-hoc in prepare-only.
The copy admits only packaged QtCore/QtGui and canonical system dependencies;
its RPATH is redirected to the existing staged frameworks. It verifies the
signature and required app team, and logs source/copy SHA-256, architecture,
team and paths. It overrides the platform-plugin path and removes the ambient
general plugin path only for the child. Neither the cache nor bundle changes.
No entitlement, delivered plugin, dependency or signature guard is relaxed.

All20 release-source script tests pass, including nine real shell-helper tests
for exact environment/arguments, status0/1, original marker refusals, missing or
relative Qt binding, absent plugin/framework, wrong architecture, external
dependency, wrong signer and separate prepare-only signing. Bash syntax,
documentation and diff checks pass. Only synthetic executables and tool stubs
run during these checks. The actual signed relocated smoke remains coordinator
work on the resulting clean commit; native Cocoa qualification remains pending.
