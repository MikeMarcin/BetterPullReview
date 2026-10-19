---
schema: "implement-rfc.card.v1"
id: "BPR-1645"
title: "Keep relocated release smoke offscreen"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"6e610e44dc02ab4e25b75d061b0755549a318d96","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"76974f7d0f89317a28f512a8cab3c2a3e1e4c5a2","evidence":{"kind":"observation","detail":"Actual signed assessment clone selected offscreen, cleared external plugin paths, retained isolated HOME, exact self-test/budget argv, status and private-profile/production-bundle identity predicates. Original sealed staged bundle remained byte-identical and signature valid; exact delivered Cocoa qualification remains pending.","artifact":".mask/sessions/release-v0.3.0/BPR-1645-actual-clone-smoke-r4.json"}},{"criterion":"AC2","outcome":"pass","revision":"76974f7d0f89317a28f512a8cab3c2a3e1e4c5a2","evidence":{"kind":"command","detail":"release_source_assets_tests.py:21 pass, including10 real shell-helper cases exercising actual environment/argv, exact predicates and refusal controls for corrupt/escaped clone, original mutation, Qt binding, dependencies, architecture and signer; bash syntax, documentation gate and diff check pass. Tests use owned synthetic children/tool stubs and do not drive native UI.","artifact":".mask/sessions/release-v0.3.0/offscreen-clone-7697-evidence.json"}},{"criterion":"AC3","outcome":"pass","revision":"76974f7d0f89317a28f512a8cab3c2a3e1e4c5a2","evidence":{"kind":"artifact","detail":"Intentional clean7697 source/test/docs commit and coordinator actual Mac distribution r4 complete0; assessment clone smoke PASS, original bundle fingerprint4612b2ee… and original executable8ef49571… identical before/after; strict/deep original signature valid. Native Cocoa/foreground, original timing failure, configured Debug Skip, other-platform gates and security/trust obligations retain their separate dispositions.","artifact":".mask/sessions/release-v0.3.0/macos-distribution-r4.log"}}]
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

The df7e540e candidate reads only that release build's absolute Qt6_DIR configuration
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

## Distribution boundary observation and assessment clone

The actual df7e540e producer r3 also aborts134 before Apple submission. Source
inspection confirms that distribution startup deliberately removes external
plugin paths and fixes libraryPaths to its own canonical PlugIns. That is the
required shipped boundary, so the environment-path candidate cannot supply
this smoke. Its failed raw log/receipt remain retained; it is not accepted.

The revised helper copies the sealed staged bundle into a fresh private
assessment root and compares complete file/link fingerprints before resealing.
Only that clone receives the exact cached offscreen plugin inside its own
PlugIns. It preserves the dependency/universal/signing checks above, then
reseals and verifies the clone. Signature bytes necessarily change there;
post-sign clone/executable equality is not claimed. The child runs offscreen
with all external plugin paths cleared and the original HOME/arguments/status
and private-profile/bundle-identity predicates. Afterward the original complete
bundle fingerprint and executable SHA-256 must equal their before values and
its original signature must still verify. Those identities are logged. No
delivered bytes, installed-runtime guard or entitlement changes.

All21 script tests pass, including ten shell-helper tests with original-marker
refusals, actual clone/env/argv observations, corrupt-copy and escaped-clone
controls, and mutation of the original during a synthetic child. Bash syntax,
documentation and diff checks pass. In tests-disabled source, native
notification scheduling is false; no device probe is selected; the offscreen
platform supplies no tray surface. The ordinary self-test checks the selected
vault adapter's status without a credential exercise. No owner desktop or
native Cocoa process is driven. Actual signed clone smoke remains coordinator
work, and does not qualify the exact delivered Cocoa runtime or native trust.

## Actual signed assessment and closeout

The coordinator's clean76974f7d0f89317a28f512a8cab3c2a3e1e4c5a2
Mac distribution r4 completes with8 assets and passing checksums/package audits.
Its actual signed assessment clone smoke passes the original private-profile
and production bundle-identity predicates. Original bundle fingerprint
`4612b2eeda70246808413f250f7699fd733a9d8ffcc187f5363bd7caebf7c3df`
and executable SHA-256
`8ef495711f4db619ffc3eed02eddb50a133b677408100b4af2a36bc696ca7e61`
remain equal before/after, and original strict/deep signature verification passes.
The original app and final DMG subsequently pass coordinator-owned publisher,
team, runtime, universal-architecture, stapler and Gatekeeper checks. These
actual trust results belong to that candidate; the clone supplies only the
scoped assessment smoke and does not qualify the exact delivered Cocoa launch
or any pending native foreground course. Raw r2/r3 failures stay retained.

The closeout is intentionally integrated with the next justified Windows
repair chain before its final candidate freeze. No credential, signing or
publication action was performed by the repair implementer.
