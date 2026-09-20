---
schema: "implement-rfc.card.v1"
id: "BPR-0936"
title: "Render source tabs at the selected persisted width"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r20-usable-review-amendments"}
---

# Render source tabs at the selected persisted width

Owner amendment after observing a rendered file diff in the repaired signed r21
UnrealEngine#1773 CLI course. Preserve that success without asking for another
owner test. This card belongs to the existing selected-diff-exact-epoch-pair
milestone; no live GitHub mutations or private source in committed evidence.
Overview folder collapse remains separate in [[BPR-0934]].

## Acceptance

- AC1: Trace the accepted tab-width setting from persistence through core/host appearance projection into every source pane.
- AC2: Freshly loaded source, live previews, committed changes, cancellation and reload use the selected tab-stop width without changing source bytes or anchors.
- AC3: A regression checks actual QTextLine/cursor geometry and rendered production-composed tab stops at widths two, four and eight.

## Evidence

Implementation and direct verification pending.
