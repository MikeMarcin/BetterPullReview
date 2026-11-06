---
schema: "implement-rfc.card.v1"
id: "BPR-1877"
title: "Suggested prompts show while the agent-ready box is open"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: suggested prompts don't come up until the green '<agent> is ready' box is closed, which is confusing"}
acceptance: []
---

# BPR-1877: Suggested prompts show while the agent-ready box is open

## Observed

The owner, 2026-11-05. In the agent tab, the suggested prompts do not appear
until the green "<agent> is ready" setup-check box is dismissed. Nothing says
the prompts are waiting behind it, so the tab looks like it has none.

## Change

- A passing ready check never hides the suggested prompts: show them together
  (the ready box compact, the prompts usable at once), or let a passing check
  collapse itself.
- A failing check may still lead, but must not leave the prompt area blank
  without saying why.

## Acceptance

- AC1. With a passing check and the ready box open, suggested prompts are
  visible and clickable; covered by a Qt test.
- AC2. The owner accepts the look (attended review).
- AC3. `python3 scripts/verify_documentation.py --root .` passes.
