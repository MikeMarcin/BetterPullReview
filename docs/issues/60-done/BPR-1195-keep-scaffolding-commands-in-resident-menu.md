---
schema: "implement-rfc.card.v1"
id: "BPR-1195"
title: "Keep the scaffolding commands in the windowless macOS menu bar"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_at":"f6de40b6","related":["BPR-1035","BPR-1130","BPR-1132","BPR-1129"],"assignment":"native_menu_resident_fix","mandate_revision":"r1","milestone_plan":{"id":"bpr1195-native-menu","path":"docs/rfcs/plans/bpr1195-native-menu/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1ebd9a0708b1e7b2de818ff2406a22dccbbc73f3","evidence":{"kind":"command","detail":"Own unchanged-product baseline failed in 5.91s on missing resident File. Product candidate native course passed 6.17s with exact whole-menu equality, all later reopen/Settings/Quit assertions, 0ms missing-bar gap, native key windows throughout and 5/5 active closes retaining activation.","path":"docs/rfcs/plans/bpr1195-native-menu/HANDOFF-2026-09-25.md"}},{"criterion":"AC2","outcome":"pass","revision":"1ebd9a0708b1e7b2de818ff2406a22dccbbc73f3","evidence":{"kind":"command","detail":"Qt profile-management course passed 1.05s: six scaffolding actions preserve order and shortcuts, have disabled state and no desktop effect; File/View/Pull Request retain accessible names. Native course also checks all six disabled. Source inspection confirms no connections for the six mirrors.","path":"docs/rfcs/plans/bpr1195-native-menu/HANDOFF-2026-09-25.md"}},{"criterion":"AC3","outcome":"pass","revision":"1ebd9a0708b1e7b2de818ff2406a22dccbbc73f3","evidence":{"kind":"command","detail":"Composed profile-management final run passed 4.43s after updating its obsolete four-entry View assertion to seven and requiring every non-separator command disabled. Four-file format and clang-tidy pass. Documentation gate and all 64 verifier tests pass.","path":"docs/rfcs/plans/bpr1195-native-menu/HANDOFF-2026-09-25.md"}}]
---

# BPR-1195: Keep the scaffolding commands in the windowless macOS menu bar

The full Mac integration pass and its serial rerun fail
`application_profile_empty_desktop_native_menu_bar` on the exact menu comparison.
The workbench has Close Tab under File, Updates Line and tab navigation under
View, and the two composition commands under Pull Request. The resident bar
has none of them. Qt moves Quit into the Apple menu and hides the empty File
menu when the last window closes.

## Acceptance

- AC1: The native course preserves its exact windowed/windowless menu comparison
  and passes with all later reopening, Settings-focus and Quit assertions. It
  fails on the menu comparison before the fix.
- AC2: Close Tab, Updates Line, Next Tab, Previous Tab, Compose review and
  Comment retain their menu order and shortcuts while resident. They are
  disabled and have no effect; accessible menu names remain present.
- AC3: The focused Qt profile-management and application profile-management
  courses pass, along with the documentation gate and touched-file format check.

## Boundary

This is a standalone follow-up to the completed cards, on the coordinator's
assigned `f6de40b6` baseline. It does not reopen their historical acceptance.
The isolated implementer owns the desktop menu and focused checks only. No
remote platform execution, primary-checkout changes, push, or new attended
question is authorized here. The frozen RFC-0052 attended candidate is kept.
Locked-console activation observations remain explicitly unobserved.
