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

`e21476cc` repairs the omitted composed `apply_code_font_to` call. Persistence and accepted Settings were correct; Qt had retained its default 80-pixel tab stop. The new production-composition check fails before the repair, then proves default 4, gestures 2/8/4 and restart persistence for source/detail/unified panes. Actual QTextLayout cursor advances follow 2/4/8 selected space widths for one, two and three literal tabs without changing source or mappings.

See the [exact candidate and verification record](../../verification/rfc-bpr-0013-usable-review-20260920.md).
