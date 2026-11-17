---
schema: "implement-rfc.card.v1"
id: "BPR-2241"
title: "A restored report's byline says how the turn ended, not \"success\""
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-2241--BPR-2241--9140e5a8ea24.json","origin":"seen while building BPR-2231, 2026-11-16 (lane checkpoint docs/issues/_checkpoints/BPR-2231/handoff.md, \"Seen, not filed\"); pre-existing, not caused by BPR-2231"}
---

# A restored report's byline says how the turn ended, not "success"

## What happened

After a restart, an agent report opened from a restored turn shows, under its
title, a first segment of "success" where a live turn shows how it ended
("Claude answered in 12s"). It was seen in look report-keeps-v001 while
building [[BPR-2231]]; it predates that card.

## What is known (grounded in source, not yet reproduced under a test)

- The line is `agent_report_byline` ([[BPR-2184]]): `Ending`, `When`, `Head`,
  joined by " · " in `src/adapters/qt/src/agent_report_view.cpp` (about line
  404). `Ending` comes from the turn's `Ending` in
  `agent_tab_view.cpp` (the byline is built at about line 2957).
- A live turn sets `current.Ending = parts.front()` of its footer (about line
  3142), the footer's first segment.
- A redrawn or restored turn takes `shown.Footer.section(" · ", 0, 0)`
  (about line 2186). The restored `Footer` is the persisted turn's `Ending`
  (`window_quick_ask.cpp` and `window_agent_setup.cpp` map
  `turn.Ending` to `.Footer`).
- `src/application/src/agent_invocation_host.cpp` (about line 1207) stores
  `turn.Ending = name(result.Outcome)` for a turn, and
  `agent_launch.cpp`'s `name(agent_run_outcome)` returns "success" for a
  successful run. So the persisted ending can be the raw outcome name, not
  the footer text the live turn showed. Unconfirmed: which persistence path
  the report's turn took.

## Scope and acceptance

- **AC-a.** A failing test: persist a settled turn, restore it, open its
  report; the byline's first segment equals the live turn's ending (not the
  outcome identifier), for a successful, a stopped and a failed turn.
- **AC-b.** The fix keeps one owner of the wording (the footer text the turn
  settled with is persisted, or the restored ending is derived from the
  persisted outcome by the same code the live footer uses). The core decides,
  Qt presents: no Qt string is derived from a provider identifier.
- **AC-c.** The restored report's byline is unchanged for turns that already
  persisted a footer-style ending.
