---
schema: "implement-rfc.card.v1"
id: "BPR-2090"
title: "Home shows when asked while the Tour page is open"
status: "Backlog"
rfcs: ["RFC-BPR-0105"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2088--BPR-2090--e27a6d8aee2e.json","origin":"agent-run live course of 2026-11-11 (handoff, Observed, not changed); layout round of 2026-11-11"}
depends_on: []
---

# Home shows when asked while the Tour page is open

## Scope and acceptance

In the agent-run live course of 2026-11-11, Home did not show while the Tour
page (the tour expanded into the main pane, RFC-BPR-0085 D4) was open. Going
Home is always available in a pull request window.

- Home (its toolbar action, its shortcut and the menu item) shows Home while
  the Tour page is open, and the tour page closes, keeping its step, as Back
  to the code does.
- A Qt window test reproduces it first, then passes.

Not scheduled in any milestone; found outside the layout round's surfaces.
