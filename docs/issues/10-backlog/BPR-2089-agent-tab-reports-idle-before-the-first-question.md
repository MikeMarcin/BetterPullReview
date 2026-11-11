---
schema: "implement-rfc.card.v1"
id: "BPR-2089"
title: "The Agent tab reports idle before its first question"
status: "Backlog"
rfcs: ["RFC-BPR-0103"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2088--BPR-2090--e27a6d8aee2e.json","origin":"agent-run live course of 2026-11-11 (RFC-BPR-0105 AC08 record, Observed, not changed); layout round of 2026-11-11"}
depends_on: []
---

# The Agent tab reports idle before its first question

## Scope and acceptance

In the agent-run live course of 2026-11-11 the course waited 120 seconds for
the Agent tab's state to be idle (`live_second_account_course.cpp`, "the
Agent tab is ready to ask"), never saw it, then sent anyway, and the question
was answered. `agent_tab_view::refresh` makes the state idle only when no
setup is needed, an agent is offered, nothing runs, no continuation is
offered and the last run did not fail; one of those held although asking
worked (likely a kept native conversation offering a continuation:
`head_moved`).

- Find which state the tab was in, from the course's own log with the state
  printed, and make the state say what the tab can do: a tab that can be
  asked reads idle (or a state the course and the window's agent footer,
  [[BPR-2051]], both treat as ready).
- The live course's wait passes without the 120-second fallback.

The window's agent footer ([[BPR-2051]], 2026-11-11) shows the agent idle and
ready from the first moment a pull request opens; this card is about the
tab's own state.
