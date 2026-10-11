---
schema: "implement-rfc.card.v1"
id: "BPR-1385"
title: "Stabilize the review bar during normal navigation"
status: "Done"
rfcs: ["RFC-BPR-0005", "RFC-BPR-0052"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"corrective","origin":"Owner report and toolbar screenshot, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1385--BPR-1385--e781f12baaa1.json","related_cards":["BPR-1129","BPR-1130","BPR-1255"],"milestone_plan":{"id":"bpr1385-review-bar-stability","path":"docs/rfcs/plans/bpr1385-review-bar-stability/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dc534e7142f9d804be7105d1d562397e20018491","evidence":{"kind":"observation","detail":"Unchanged comparison/counter/arrow rectangles across readiness, recovery and all named comparisons at fixed widths; full tooltip/accessibility and menu/focus checks pass at scales 1.0, 1.25 and 2.0.","reference":"docs/verification/BPR-1385/README.md"}},{"criterion":"AC2","outcome":"pass","revision":"dc534e7142f9d804be7105d1d562397e20018491","evidence":{"kind":"observation","detail":"File ordinals 0/1/8/9/10/11/99/100/101, loading and computed diffs with 1/11/101 navigated change groups preserve exact geometry; warning arrival and clearance also preserve geometry.","reference":"docs/verification/BPR-1385/README.md"}},{"criterion":"AC3","outcome":"pass","revision":"dc534e7142f9d804be7105d1d562397e20018491","evidence":{"kind":"observation","detail":"Resize sweeps, date/word/number yield order, enlarged-font floor, restoration, full workbench and composed GitHub tests pass. Architecture boundaries passed; changes remain in Qt.","reference":"docs/verification/BPR-1385/README.md"}},{"criterion":"AC4","outcome":"pass","revision":"dc534e7142f9d804be7105d1d562397e20018491","evidence":{"kind":"observation","detail":"Fail-before geometry regression at 068db28fd; final 9/9 focused and integration tests pass, including real composed Git/cache failure/recovery and full-window renders. clang-format 22.1.8, documentation gate, 67 documentation tests and diff whitespace check pass.","reference":"docs/verification/BPR-1385/README.md"}}]
---

# Stabilize the review bar during normal navigation

## Mandate and diagnosis

The owner asks for stability while working without resizing the window. The
highlighted comparison/status menu expands and shrinks frequently; file and change
navigation adds smaller shifts. File and work a corrective card directly.

The comparison button currently sizes itself from readiness and comparison text.
The file and change labels also use their current text's natural width. Both move
the adjacent arrow hit targets even though the available window width is unchanged.

## Acceptance

- AC1: At a fixed window size, comparison readiness, failure, recovery and named-comparison text changes preserve the comparison button and downstream navigation geometry. Full text remains available through tooltip and accessibility, and the comparison menu remains usable.
- AC2: File and change navigation, including unselected/loading states, differing glyph widths and digit boundaries, preserves arrow hit targets without hiding the current position at widths that have room for it.
- AC3: Actual window resizing and font/style changes still adapt the row, preserve the supported minimum width and keep primary actions accessible. No effects or semantic policy move into Qt.
- AC4: A regression fails before the fix, passes afterward, and exercises the rendered workbench and a composed application path. Focused checks, touched-file formatting and the documentation gate pass.

## Design and scope

Use the loaded frontend-design skill with the existing native palette, typography,
focus treatment and control order. Reserve stable comparison and position slots;
shorten visible text to its allocated space while keeping complete descriptions.
Width changes follow available space and appearance, not the current sentence or
ordinal. This is a measurable layout correction; no new subjective design approval
is introduced. Existing RFC-wide native and human obligations remain unchanged.

## Evidence

Implemented at `dc534e7142f9d804be7105d1d562397e20018491`. All four criteria
pass; the [verification record](../../verification/BPR-1385/README.md) binds
the regression, geometry courses, rendered application path, source hashes and
limitations. Nine focused/integration tests passed on the final source, plus
the architecture boundary check; exact clang-format 22, the documentation gate
and its 67-test suite passed. No human criterion remains on this corrective card.
