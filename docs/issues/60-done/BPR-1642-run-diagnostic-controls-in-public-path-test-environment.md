---
schema: "implement-rfc.card.v1"
id: "BPR-1642"
title: "Run diagnostic controls in public-path test environment"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f8b8f3383713713c2c580038f27534dbd170a824","evidence":{"kind":"observation","detail":"Only application_diagnostic_controls_public_path ENVIRONMENT now references existing public-path offscreen/Linux Windows-style environment. Isolation registration,fixture C++,backstops and assertions unchanged; no real-desktop qualification claimed.","artifact":".mask/sessions/release-v0.3.0/native-linux/v030-linux-r2-ctest.xml"}},{"criterion":"AC2","outcome":"pass","revision":"f8b8f3383713713c2c580038f27534dbd170a824","evidence":{"kind":"observation","detail":"Actual generated native CTest environment verifies both QT_QPA_PLATFORM=offscreen and QT_STYLE_OVERRIDE=Windows. Complete diagnostic-controls course passes6.60s and mandatory manifest0.42s atf8b8. Original abort-6 retained; focused native logs/JUnit plus docs/diff pass. All control,persistence,privacy,shutdown and cleanup assertions run.","artifact":".mask/sessions/release-v0.3.0/native-linux/v030-linux-r2-ctest.xml"}}]
---

# BPR-1642: Run diagnostic controls in public-path test environment

The actual Ubuntu row application_diagnostic_controls_public_path aborts at
QApplication construction after Wayland/XCB cannot connect to a desktop.
The offscreen plugin is available. Unlike adjacent production-composition
fixtures, this row omits its CTest ENVIRONMENT property; its main neither
supplies an offscreen platform nor changes the application environment.

## Acceptance

- AC1: Bind only this automated row to the existing bpr_public_path_fixture_environment, preserving its isolation registration, assertions, public composition and backstops. Product startup and native desktop qualification remain unchanged.
- AC2: Preserve the original abort log; inspect generated native CTest environment and run the actual diagnostic-controls public path plus mandatory manifest/documentation checks after explicit host release. Retain any newly exposed assertion failure rather than claim plugin startup is sufficient acceptance.

## Diagnosis

The shared public-path environment already supplies QT_QPA_PLATFORM=offscreen
and the existing headless Linux Windows-style policy. Neighboring scaling and
cold-import rows bind it explicitly. The diagnostic-controls fixture creates
QApplication immediately; no parent process or nested app loses an explicitly
supplied value. Its registration simply has no value to propagate. The retained
failure reports unavailable wl_display, failed DISPLAY :0 and abort-6 before
any test assertion. No real desktop, native styling or installed-package claim
is supplied by offscreen automation.

Evidence: ignored ubuntu-gate-r5.partial6 raw/normalized logs, lines5310–5321.
The coordinator retains both native heavy slots; no build, render, tidy or
native probe is authorized before explicit release.


## Source repair checkpoint

Only this row's ENVIRONMENT property now references the existing shared
public-path fixture environment. Isolation, timeout/backstops, fixture C++ and
all assertions are unchanged. Source self-review and diff/documentation checks
pass. Generated native environment and the full course result remain pending
until the shared PC is explicitly released; no desktop acceptance is claimed.
