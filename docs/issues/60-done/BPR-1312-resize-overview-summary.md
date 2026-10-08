---
schema: "implement-rfc.card.v1"
id: "BPR-1312"
title: "Resize and scroll the Overview summary while keeping Files visible"
status: "Done"
rfcs: ["RFC-BPR-0063"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"f9c243018","evidence":{"kind":"command","detail":"Focused Qt Overview passes: long half allocation, natural short/compact empty, visible native grip, pointer and one-line keyboard resizing, accessible value, reset, font-relative range and short-height clamp."}},{"criterion":"C2","outcome":"pass","revision":"f9c243018","evidence":{"kind":"command","detail":"Retained source reader and composed Qt Overview pass: independent normal wheel, full byte-boundary Unicode/literal copy, shared documents, source anchors/selections and Files indexes without model reset. Changed-line warnings-as-errors tidy and selected format pass. Protected Home source export contaminates main-root architecture scan; exact clean-export check remains BPR1314."}}]
---

# Resize and scroll the Overview summary while keeping Files visible

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Normal Overview follows content-aware half allocation, compact empty body, noncollapsing font-relative limits and visible accessible pointer/keyboard reset grip.
- C2: Description and Files scroll independently, retain selection/reading/file state, and share accepted documents with the expanded reader; no outer scroll trap.
