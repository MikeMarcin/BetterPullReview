---
schema: "implement-rfc.card.v1"
id: "BPR-1639"
title: "Diagnose Ubuntu shortcut column overflow"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1639: Diagnose Ubuntu shortcut column overflow

Ubuntu source8eef fails keyboard shortcut scales100,125,200 at the complete title/key column fit assertion. Preserve title room, key visibility, resizability and no horizontal clipping; no assertion or scale bypass.

The coordinator owns the still-running native Ubuntu suite and has not released
the shared PC. Current diagnosis is source-only; no product/test repair or native
reproduction is claimed. The canonical frontend-design skill was read earlier
in the same repair milestone; it applies before any visual surface change.

## Acceptance

- AC1: Measure exact initial/minimum sizes, title/key columns, viewport, native label widths and fonts at the failing scales; bind the actual layout cause without assuming a font issue.
- AC2: Implement only a bounded agreed layout repair preserving approved native labels, key caps, search and accessibility; frontend-design applies and meaningful overflow controls remain.
- AC3: Verify focused native/faithful offscreen scale and font evidence plus scoped format/tidy/architecture/docs, commit explicit paths and reconcile commands/pending observations.

## Source diagnosis checkpoint

The initial/minimum size comes from 42/24 em, while the key column measures
actual native key-cap strings using ResizeToContents. A platform label/font
width mismatch is a hypothesis until observed. Failure-only diagnostics now
retain dialog/minimum sizes, title/key column and viewport widths, title floor,
DPR and font. The complete original fit predicate and subsequent widest-key
assertion remain unchanged. No layout or minimum-size repair is made; native
measurements and compilation of these diagnostics remain pending.
