---
schema: "implement-rfc.card.v1"
id: "BPR-1522"
title: "Repair review-control regressions exposed by the patch release gate"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"0.2.1 normal Mac release gate","assigned_implementer":"/root","baseline":"40bc43a8f17e6871593c3dde539b546155af4d2c","milestone_plan":{"id":"bpr-1522-release-review-controls","path":"docs/rfcs/plans/bpr-1522-release-review-controls/milestone-plan.json"}}
---

# BPR-1522: Repair review-control regressions exposed by the patch release gate

The 0.2.1 full Mac gate exposes two three-scale failure groups. The Saved comments button fits its editor but has TabFocus (1), while the established editor contract requires StrongFocus. The added one-click Accept button also triggers the documented review bar's final numeric fallback at the width floor; an older assertion still requires visible numbers. BPR-1255's owner decision explicitly allows no numeric position in the fully yielded bar, with complete position tooltips.

## Acceptance

- AC1: Reproduce both failure groups on the release source and identify the failing button, geometry and focus policy. Retain negative evidence.
- AC2: Saved comments fits the narrow editor and takes StrongFocus. At the width floor the file label shows only one of the documented full, compact or group-only forms and retains its complete tooltip and accessible description. Preserve all other control/overflow checks.
- AC3: Run affected Qt courses at 100%, 125% and 200%, inspect real offscreen output where relevant, run focused composition/recovery checks plus format22 and documentation. Bind the release's subsequent normal gate to the corrected committed source; no failed gate is reported as passing.

## Ownership

The release coordinator implements this bounded correction as a leaf. Frontend-design is loaded; preserve the existing native palette, fonts, spacing and responsive policy. No new visual design or owner desktop operation is introduced. Original BPR-1507 and BPR-1520 Done evidence stays historical.
