---
schema: "implement-rfc.card.v1"
id: "BPR-1350"
title: "The Overview Files heading is clipped without an ellipsis at a narrow width"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"bpr-main-linux, BPR-1094's rendered checks, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","requested_skill":"frontend-design","owner_look":"required, on the Mac","related_rfcs":["RFC-BPR-0049","RFC-BPR-0050"],"related_cards":["BPR-1094","BPR-1089"]}
---

# BPR-1350: The Overview Files heading is clipped without an ellipsis at a narrow width

## Finding

[[BPR-1094]] rendered the Overview offscreen at `269f7ccff`, with the page at
1,000 px (its files column about 680 px) and the "?" chip showing. The Files
heading is cut mid-word, reading "…2 not rea" under the Open button. It has no
ellipsis, and its accessible name has no full text. The composed 1,400 px
window shows its shorter heading ("Files: 17 in this pull request, 17 not
read") whole at 100%, 125% and 200%. The chips themselves fit whole.

## Outcome (coordinator, 2026-10-08)

When the heading does not fit, it is shortened with an ellipsis. The full
sentence is its tooltip and its accessible name.

## Acceptance

- At a narrow Overview, with "?" and Open/All showing, the heading never cuts
  mid-word without an ellipsis. Check 100%, 125% and 200% and every theme.
- The tooltip and accessible name carry the full sentence. A Qt test asserts
  both, with a failing-before control.
- **Owner look on the Mac.** The design pass uses the `frontend-design` skill
  (AGENTS.md), and the owner reviews the result in a short attended session.

## Boundaries

Presentation only: no count, drill-down or settings behaviour changes.
