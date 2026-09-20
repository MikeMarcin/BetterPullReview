---
schema: "implement-rfc.card.v1"
id: "BPR-0935"
title: "Keep dark-mode syntax legible across intraline edit emphasis"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r20-usable-review-amendments"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"af4e2d38","evidence":{"kind":"artifact","detail":"Owner screenshot inspected without retention; production overlay diagnosis and per-token repair recorded in docs/verification/rfc-bpr-0013-usable-review-20260920.md."}},{"criterion":"AC2","outcome":"pass","revision":"af4e2d38","evidence":{"kind":"artifact","detail":"Exact ranges, selected/unselected syntax hues, contrast headroom, customization and Earlier/Later non-color cues pass focused core and rendered Qt tests."}},{"criterion":"AC3","outcome":"pass","revision":"af4e2d38","evidence":{"kind":"artifact","detail":"Fail-before monochrome regression; 1x/1.25x/2x rendered macro/string tests and contrast audit pass. Signed tests-disabled r22 independently renders safe exact source; committed safe dark-intraline raster inspected."}}]
---

# Keep dark-mode syntax legible across intraline edit emphasis

Owner amendment after observing a rendered file diff in the repaired signed r21
UnrealEngine#1773 CLI course. Preserve that success without asking for another
owner test. This card belongs to the existing selected-diff-exact-epoch-pair
milestone; no live GitHub mutations or private source in committed evidence.
Overview folder collapse remains separate in [[BPR-0934]].

## Acceptance

- AC1: Diagnose the composed diff syntax, row and intraline layers against the owner observation without retaining private source.
- AC2: Improve dark-mode edit emphasis and syntax legibility while preserving exact intraline ranges, selection, accessibility and theme customization.
- AC3: Focused contrast/layering tests and rendered production-workbench evidence cover representative changed identifiers and strings.

## Evidence

`af4e2d38` diagnoses and repairs the monochrome intraline override. New rendered macro/string regression fails before repair and passes with per-token contrast-safe ink; exact ranges and non-color Earlier/Later cues remain. The contrast audit, dark/light/high-contrast surfaces and 1x/1.25x/2x diff tests pass. Signed tests-disabled r22 independently renders a selected safe source through the complete CLI/Git/cache/workbench path.

See the [diagnosis, exact candidate and safe rendered evidence](../../verification/rfc-bpr-0013-usable-review-20260920.md).
