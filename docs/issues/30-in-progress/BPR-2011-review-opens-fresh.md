---
schema: "implement-rfc.card.v1"
id: "BPR-2011"
title: "Open every review in the default tray state"
status: "In Progress"
rfcs: ["RFC-BPR-0105"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0023-review-opens-fresh.md"}
depends_on: []
---

# Open every review in the default tray state

## Scope and acceptance

Implements
[RFC-BPR-0105](../../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md).

Reset the tray to Comments, close the tour's side chat and page, clear the
selected chat thread and show the pull request's own agent session whenever a
pull request is opened in a window, while keeping the remembered filter
defaults, the window layout, sessions, drafts and reading position.

Covers AC06 and AC07.

This is part of `dependable-session` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
