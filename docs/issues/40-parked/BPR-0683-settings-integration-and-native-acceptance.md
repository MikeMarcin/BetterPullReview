---
schema: "implement-rfc.card.v1"
id: "BPR-0683"
title: "Verify Settings integration and native acceptance"
status: "Parked"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0678", "BPR-0679", "BPR-0680", "BPR-0681", "BPR-0682","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688"]
parked_on: "human-review"
acceptance: [{"criterion":"AC-05 migration and failure isolation","outcome":"pass","revision":"22e30024","evidence":{"kind":"observation","detail":"Retained BPR-0593 integration evidence: malformed/future legacy migration refuses before writes; prior schemas reopen."}},{"criterion":"AC-07 keyboard and screen-reader course","outcome":"pending","revision":"af4acb4ab8477667068180a95ff24c149e5c840a","evidence":{"kind":"observation","detail":"Automated keyboard/focus checks retained; native screen-reader course unobserved."}},{"criterion":"AC-08 four appearances and scales","outcome":"pending","revision":"6c0ba02b","evidence":{"kind":"observation","detail":"112 deterministic renders and automated geometry pass; attended visual verdict remains open."}},{"criterion":"AC-10 bounded loads and effects","outcome":"pending","revision":"29bb2980","evidence":{"kind":"observation","detail":"Collection limits, coalescing and focused Settings capability-port instrumentation pass; exhaustive native effect observation is unproven."}},{"criterion":"AC-11 packaged platform inventory","outcome":"pending","revision":"af4acb4ab8477667068180a95ff24c149e5c840a","evidence":{"kind":"observation","detail":"Current candidate macOS, Windows and Linux packaged inventory remains unobserved."}},{"criterion":"AC-14 compact review and exact setup handoff","outcome":"pass","revision":"554564f8","evidence":{"kind":"observation","detail":"Retained visible setup course proves exact destination, cancelled/stale refusal and prior review preservation. Native acceptance remains separately open."}}]
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
