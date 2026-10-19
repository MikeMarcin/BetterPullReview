---
schema: "implement-rfc.card.v1"
id: "BPR-1638"
title: "Diagnose Ubuntu folder ink raster failure"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1638: Diagnose Ubuntu folder ink raster failure

Ubuntu normal source8eef fails qt_files_tree at Overview folder ink. Preserve the approved PlaceholderText distinction, exact existing palette/count assertions and full raw failure; no blind pixel tolerance or product change before observed geometry/color evidence.

The coordinator owns the still-running native Ubuntu suite and has not released
the shared PC. Current diagnosis is source-only; no product/test repair or native
reproduction is claimed. The canonical frontend-design skill was read earlier
in the same repair milestone; it applies before any visual surface change.

## Acceptance

- AC1: Bind the exact failing row/mode/geometry and observed folder/file ink counts to native or faithful offscreen evidence, separating font/raster, selection, palette and layout causes.
- AC2: Apply only a source-backed agreed repair, retaining meaningful wrong-color negative evidence and approved UX; load frontend-design before any visual edit and verify affected platform/render courses.
- AC3: Run scoped format/tidy/architecture/documentation checks, retain failures, commit explicit paths and reconcile commands/native qualification.

## Source diagnosis checkpoint

The folder delegate explicitly selects PlaceholderText; the test currently
requires more than eight near-color pixels and zero ordinary Text pixels in
each folder-name cell, including the one-character root path. No measured
row/mode/count is present in the original Linux log. Failure-only diagnostics
now retain those counts, path, cell, DPR, font and palette colors with the
exact original predicate unchanged. A thin root glyph is only a hypothesis,
not an accepted cause or a reason to lower the threshold. Native observations
and compilation of the new diagnostics remain pending the coordinator's slot.
