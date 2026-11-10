---
schema: "implement-rfc.card.v1"
id: "DRAFT-0023"
title: "Open every review in the default tray state"
status: "Backlog"
rfcs: ["RFC-DRAFT-0008"]
depends_on: []
---

# Open every review in the default tray state

## Scope and acceptance

Implements
[RFC-DRAFT-0008](../RFC-DRAFT-0008-a-review-opens-fresh-and-a-submission-always-ends.md).

Reset the tray to Comments, close the tour's side chat and page, clear the
selected chat thread and show the pull request's own agent session whenever a
pull request is opened in a window, while keeping the remembered filter
defaults, the window layout, sessions, drafts and reading position.

Covers AC06 and AC07.

This is part of `dependable-session` in the [plan](../milestone-plan.json).
