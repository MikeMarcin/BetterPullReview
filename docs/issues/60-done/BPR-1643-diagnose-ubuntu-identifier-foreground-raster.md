---
schema: "implement-rfc.card.v1"
id: "BPR-1643"
title: "Diagnose Ubuntu identifier foreground raster"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Actual native fullyvisible Networktoken correctformat#1b5e70, monospace9, closestDPR1raster#256074 diff10; original710contrastpairs pass. Unchangedpredicate appDPR2 control passes complete4variants.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}},{"criterion":"AC2","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Approved actualwidget repaint atassessmentminimumDPR2 leaves appDPR/font/layout unchanged and scales logicalcrop correctly; RGBatmost3,710pairs/percharacter/bothpanes/4variants/freshprocess phases preserved. Wrongtoken paint rejects same reader, restoringformats passes.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}},{"criterion":"AC3","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Complete nativeLinuxc3bc freshprocesscourse passes2.79s and Mac4e0a passes, including actualwrongpaint controls. Source-bound scopedtidy22Werror/format22/architecture/docs/diff pass. This does not qualify DPR1 userlegibility.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}}]
---

# BPR-1643: Diagnose Ubuntu identifier foreground raster

The actual Ubuntu application_cpp_identifier_appearance course passes all710
required contrast pairs in four variants, then fails visible token foreground
pixels in its fresh render child. It aborts before completing the course.
This is distinct from the diagnostic-controls platform-plugin startup failure.

## Acceptance

- AC1: Retain the original failure and measure its exact variant/token, pane geometry, foreground format and actual raster/font evidence under the native headless fixture; establish the cause without assuming FreeType or a tolerance problem.
- AC2: Implement only a subsequently agreed bounded repair preserving all710 contrast pairs, four variants, per-character layout format checks, actual painted foreground evidence and exact fresh-process completion assertions; no blind color tolerance change.
- AC3: Verify actual native complete-course evidence and meaningful controls plus scoped format/tidy/documentation, commit explicit paths and reconcile native command ownership.

## Source diagnosis checkpoint

check_token first verifies the exact foreground format on every token character
in the actual pane text layout, computes and verifies a visible cursor-derived
rectangle, then scans its rendered pixels for RGB distance at most3 in every
channel. The retained failure is solely that final predicate; it does not
identify the failing token, variant or nearest actual color. Existing renders
are captured before the per-token assertions. Failure-only diagnostics may add
that missing context while preserving the complete original predicate.

Evidence: ignored ubuntu-gate-r5.partial7 raw/normalized logs, lines5364–5371.
The coordinator owns both active native heavy slots; no native build, render,
tidy or probe starts before explicit slot release. Frontend-design remains
required for any subsequent product visual repair.


## Observation preparation

Failure-only context now names the variant, pane, exact synthetic token/site,
expected and closest raster RGB, nearest maximum-channel difference, crop/DPR
and font. The existing RGB-at-most3 predicate, per-character format validation,
contrast matrix and fresh-process completion checks are unchanged. The nearest
color is diagnostic context, not a substituted assertion or accepted tolerance.
Source format22/diff checks pass; native compile, original-predicate measurement
and focused tidy remain pending until explicit host release.


## Native coverage control and assessment repair

Native395ae2393 shows the fully visible Network token in monospace9, correct
layout foreground#1b5e70 and closest DPR1 RGB#256074 (maximum channel difference10).
The same complete fresh-process course passes all four variants at appDPR2
in3.20 seconds with its original predicate unchanged. The coordinator approves
an assessment-only minimumDPR2 repaint of the actual viewport, leaving widget
font, logical layout and application DPR intact. The crop scales its original
logical rectangle into that assessment raster; RGB tolerance remains3.

A real wrong-foreground control changes only Network's layout-format interval
to the variant's distinct macro ink, preserving neighboring format prefixes
and suffixes. The identical raster reader must reject it; restoring the exact
formats must restore the required ink. All710 contrast pairs, every-character
format checks, both panes, four variants and fresh-process phases remain.
This is palette/paint evidence, not DPR1 user legibility qualification. Native
complete-course/negative-control and scoped tidy checks remain pending.

## Verified repair result

Actual native fullyvisible Networktoken correctformat#1b5e70, monospace9, closestDPR1raster#256074 diff10; original710contrastpairs pass. Unchangedpredicate appDPR2 control passes complete4variants.

Approved actualwidget repaint atassessmentminimumDPR2 leaves appDPR/font/layout unchanged and scales logicalcrop correctly; RGBatmost3,710pairs/percharacter/bothpanes/4variants/freshprocess phases preserved. Wrongtoken paint rejects same reader, restoringformats passes.

Complete nativeLinuxc3bc freshprocesscourse passes2.79s and Mac4e0a passes, including actualwrongpaint controls. Source-bound scopedtidy22Werror/format22/architecture/docs/diff pass. This does not qualify DPR1 userlegibility.

Exact receipts, raw failures, JUnit and source bindings remain under the ignored
release-v0.3.0 session; other release qualifications are separate obligations.
