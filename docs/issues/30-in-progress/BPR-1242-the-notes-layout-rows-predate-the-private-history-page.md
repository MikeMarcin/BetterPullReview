---
schema: "implement-rfc.card.v1"
id: "BPR-1242"
title: "The Notes layout rows predate the Private history page and inspect the hidden Agent tools page"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"origin":"recorded as pre-existing under BPR-1157 (RFC-BPR-0053-0061 ledger); filed by the main stabilization milestone on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1157","BPR-0585","BPR-1134"]}
---

# BPR-1242: The Notes layout rows predate the Private history page and inspect the hidden Agent tools page

## Observation

At `main` e9e65eab2 on macOS arm64, `qt_composed_github_scale_{100,125,200}`
fail at `the agent launcher, the side chat, and the finding queue stack down one
visible column`, and `qt_workbench_window_scale_{100,125,200}` at `no Notes tab
control is clipped at 200 pixels; the column scrolls` (the helper reports `the
content is wider than the scroll range reaches`). All six passed on
2026-09-26 at beddf1f55.

17e6f85d1 ([[BPR-0585]], RFC-BPR-0023, closed and owner-accepted) gave the
Notes tray tab two pages in a `privateConversationTabs` tab widget: Private
history first, and Agent tools, the old Notes column, second. The rows still
treat the Notes tab as that one column. Choosing Notes now shows Private
history, so the agent surfaces are hidden (the composed row), and the
reachability helper measures the hidden Agent tools column, whose geometry is
never laid out at 200 pixels (the window row).

## Outcome

The rows follow the accepted arrangement: a course that inspects the agent
surfaces opens the Agent tools page as a reviewer would, and the 200-pixel
reachability check covers both Notes pages. No product layout changes.

## Direct acceptance

- The six rows pass on macOS.
- `python3 scripts/verify_documentation.py --root .` passes.
