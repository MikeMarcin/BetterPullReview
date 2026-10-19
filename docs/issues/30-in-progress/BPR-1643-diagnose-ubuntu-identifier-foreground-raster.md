---
schema: "implement-rfc.card.v1"
id: "BPR-1643"
title: "Diagnose Ubuntu identifier foreground raster"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
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
