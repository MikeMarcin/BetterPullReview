---
schema: "implement-rfc.card.v1"
id: "BPR-0939"
title: "Match file drawer added and removed colors to Overview"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r20-usable-review-amendments"}
---

# Match file drawer added and removed colors to Overview

Owner amendment after observing a rendered file diff in the repaired signed r21
UnrealEngine#1773 CLI course. Preserve that success without asking for another
owner test. This card belongs to the existing selected-diff-exact-epoch-pair
milestone; no live GitHub mutations or private source in committed evidence.
Overview folder collapse remains separate in [[BPR-0934]].

## Acceptance

- AC1: Use theme-derived green for added and red for removed file operations consistently between the file drawer and Overview.
- AC2: Preserve native selection/focus, light/dark/high-contrast readability, literal filenames and accessible operation wording; color is not the only signal.
- AC3: Focused model/delegate tests and rendered composed file-drawer evidence cover added, removed, modified and selected rows.

## Evidence

`10e9716d` carries a typed operation role to the drawer delegate and applies the accepted appearance profile in the concrete workbench. Added/removed names and operation/count text use Overview's semantic green/red; modified files remain neutral, selected rows retain native highlighted text, and accessible operation words do not rely on color. Pixel/role/accessibility checks pass for light, dark and both high-contrast themes.

See the [dark/light rasters and exact candidate](../../verification/rfc-bpr-0013-usable-review-20260920.md).

The final production-composition regression paints safe added/removed rows
through the actual application's delegate after Settings selects each theme.
It fails when the concrete appearance handoff is removed and passes when it is
restored; no manual delegate profile application masks a missing wrapper call.
