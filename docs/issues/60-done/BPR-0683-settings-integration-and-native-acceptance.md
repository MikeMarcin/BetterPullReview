---
schema: "implement-rfc.card.v1"
id: "BPR-0683"
title: "Verify Settings integration and native acceptance"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0678", "BPR-0679", "BPR-0680", "BPR-0681", "BPR-0682","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688"]
acceptance: [{"criterion":"AC-05 migration and failure isolation","outcome":"pass","revision":"22e30024","evidence":{"kind":"observation","detail":"Retained BPR-0593 integration evidence: malformed/future legacy migration refuses before writes; prior schemas reopen."}},{"criterion":"AC-07 keyboard and screen-reader course","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"Automated keyboard/focus checks retained (qt_settings_window, focus-return, page scroll, reset and Cancel courses). macOS VoiceOver order: Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0683-settings-native-macos, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 3c3fd4a34092d4f5777e56f03fce5b4ebd3abd5384335eaa3e6f11f801c4b009, decision f2fb9b06-0968-41f0-895f-c2ef23c05acd, actor human, 2026-09-11T18:38:18Z: accept. The candidate's AC-07 ask was the VoiceOver walk of every sidebar item, row control, reset, confirmation, status and close action."}},{"criterion":"AC-08 four appearances and scales","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"112 deterministic renders and automated geometry pass (6c0ba02b). Attended visual verdict on the real window in Light, Dark, High-contrast Light and High-contrast Dark at the owner's interface size including the minimum window size: Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0683-settings-native-macos, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 3c3fd4a34092d4f5777e56f03fce5b4ebd3abd5384335eaa3e6f11f801c4b009, decision f2fb9b06-0968-41f0-895f-c2ef23c05acd, actor human, 2026-09-11T18:38:18Z: accept."}},{"criterion":"AC-10 bounded loads and effects","outcome":"pass","revision":"29bb2980","evidence":{"kind":"observation","detail":"Collection limits, coalescing and the focused qt_settings_effect_boundary fixture pass (29bb2980, e354b45e): real Settings gestures reach their owning hosts and recorded capability ports do their work on worker threads with nothing on the Qt thread. This is the focused instrumentation the RFC's verification matrix names; an exhaustive OS-level syscall trace was never planned and is not claimed. The BPR-0593 closeout ledger keeps that wording."}},{"criterion":"AC-11 packaged platform inventory","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"macOS: Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0683-settings-native-macos, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 3c3fd4a34092d4f5777e56f03fce5b4ebd3abd5384335eaa3e6f11f801c4b009, decision f2fb9b06-0968-41f0-895f-c2ef23c05acd, actor human, 2026-09-11T18:38:18Z: accept. The candidate's AC-11 ask was that this tests-disabled bundle shows all seven Settings categories with the same rows the dev build shows, with any missing row a platform capability row. Windows and Linux packaged inventory are platform work of their own, split to BPR-0730 (win) and BPR-0731 (linux); the BPR-0593 closeout depends on both, so RFC-BPR-0018 cannot close on this card alone."}},{"criterion":"AC-14 compact review and exact setup handoff","outcome":"pass","revision":"554564f8","evidence":{"kind":"observation","detail":"Retained visible setup course proves exact destination, cancelled/stale refusal and prior review preservation. Native acceptance remains separately open."}}]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Verify Settings integration and native acceptance

## Scope

Reconcile all Settings criteria with focused selected-path evidence, migration failures and native/attended inventory.

Owned paths: Settings integration tests and documentation only, product fixes get bounded follow-ups.

## Acceptance

AC-05/07/08/10/11/14: actual platform facts, four appearances and 100–200% scale, keyboard/accessibility, bounded loads, failure isolation; park unavailable required observations.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Integration evidence is retained in BPR-0593. Architecture checks and composed
GitHub passed; Settings minimum-size/four-appearance/four-scale rendered cases
passed, with representative rasters inspected. Migration preflight, queue bounds
and preview burst defects are fixed in BPR-0686/0687/0688. BPR-0689 exact setup is implemented and its visible public course passes
(commit554564f8). The focused runtime effect-boundary fixture also passes. Screen-reader,
target-native interaction and Windows/Linux packaged inventory remain unobserved.
Do not finish this card as Done or close RFC0018 from partial automated evidence.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.

The focused `qt_settings_effect_boundary` course passed0.82s with canonical
tidy. Real Settings preference/theme gestures invoke real owning hosts;
recorded capability ports enter worker threads and return results through the
GUI decision lane. It covers queue preference, appearance persistence, theme
import/export and Settings-close import cancellation. Test harness joins happen
after gestures return. This is not a claim of exhaustive OS syscall, native
chooser, vault, process or network instrumentation.

## Attended verdict, 2026-09-11 (v006): accepted

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; the session was handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0683-settings-native-macos`, decision `f2fb9b06`, 18:38:18Z: **accept**, no note,
covering the three macOS asks: AC-07 VoiceOver order, AC-08 the four appearances at the real
window including the minimum size, and AC-11 the tests-disabled packaged inventory on macOS.
AC-10 is recorded on the focused effect-boundary fixture, which is the instrumentation the
verification matrix names; the closeout ledger keeps its "exhaustive OS instrumentation
unproven" wording. Windows and Linux packaged inventory are platform work split to
[BPR-0730](BPR-0730-windows-packaged-settings-inventory.md) and
[BPR-0731](BPR-0731-linux-packaged-settings-inventory.md), which the
[BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md) closeout now depends on. Done.
