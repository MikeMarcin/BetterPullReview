---
schema: "implement-rfc.card.v1"
id: "BPR-2241"
title: "A restored report's byline says how the turn ended, not \"success\""
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-2241--BPR-2241--9140e5a8ea24.json","origin":"seen while building BPR-2231, 2026-11-16 (lane checkpoint docs/issues/_checkpoints/BPR-2231/handoff.md, \"Seen, not filed\"); pre-existing, not caused by BPR-2231","acceptance":["AC-a","AC-b","AC-c"]}
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

## Findings

- A native session's turn is stored by the host with its outcome's name as
  its ending (`src/application/src/agent_invocation_host.cpp:1207` at
  `6aa6883c9`: `turn.Ending = name(result.Outcome)`, "success"), and so is a
  turn the review's agent runtime ran with no window watching
  (`src/application/src/review_agent_runtime.cpp:441`). Neither kept how
  long the run took.
- The window maps the stored ending to the restored turn's footer
  (`src/app/src/github_application/window_agent_setup.cpp:765`, `:1220`,
  `window_quick_ask.cpp:606`), and the Agent tab takes the footer's first
  segment as the turn's ending (`src/adapters/qt/src/agent_tab_view.cpp:2218`),
  which the report's byline shows (`agent_tab_view.cpp:3011`). Only a turn
  this window saw end had its footer replaced, and nothing replaced its
  ending. A turn kept in the window's own conversation store keeps the line
  it settled with and was already right.

## Fix

AC-b's second form: the restored ending is derived from the stored outcome
by the code the live footer uses.

- Host: `agent_conversation_turn::Seconds`, the run's whole seconds, set by
  `agent_invocation_host` (from the turn's admission) and
  `review_agent_runtime` (from its send) beside the outcome's name.
- SQLite: `native_agent_turns.seconds` (nullable; added to an existing file
  by `ALTER TABLE`), written on append and replay, read on history.
- Qt: `agent_turn_ending_words` (`agent_run_status`) is the one wording: the
  live footer's first segment and a restored turn's ending both come from
  it. A restored turn whose stored footer is not worded (no space) gets its
  ending and footer from its outcome and seconds; one stored without seconds
  says "Claude answered" or "Stopped". A worded footer is used as before.
- Window: the restored turns carry `Seconds`; turns adopted from the agent
  runtime are recorded in the conversation store with the line the tab
  showed, since that store keeps no duration.

## Evidence

Lane commit `36042ef05`, 2026-11-17, macOS dev preset; full
`cmake --build --preset dev`.

| Criterion | Check | Result |
|---|---|---|
| AC-a | `qt_agent_restored_byline`: a successful, a stopped (`cancelled`) and a failed (`nonzero_exit`) turn settled live, each report's byline read; in a fresh window the same turns restored as the host stores them (`success`/`cancelled`/`nonzero_exit`, their outcome, their seconds) begin with the live endings ("Claude answered in 0:00", "Stopped after 0:00"). With the restore's derivation disabled the course fails: "...for a successful turn: success" | Pass |
| AC-b | One wording: `agent_turn_ending_words` makes the live footer's first segment and the restored ending; the stored outcome and seconds are data, worded only in Qt. The restored course also checks 72 s ("Claude answered in 1:12") and a turn kept without seconds ("Claude answered") | Pass |
| AC-c | the same course: a footer stored as "Claude answered in 0:12 · $0.41" gives the byline "Claude answered in 0:12" | Pass |
| Persistence | `sqlite_agent_setup_storage`: a native turn's seconds survive a reopen and a replay, none where none was kept; `application_review_agent_runtime`: every turn no window saw end keeps its seconds | Pass |

Also passed: `qt_agent_finding_cards_restart`,
`application_agent_invocation_host`, and the 77-test agent selection named
in [[BPR-2250]]'s evidence. Format check on every changed C++ file;
clang-tidy on the changed translation units reports only findings on lines
this card did not touch; documentation gate.

## Remaining risks

- The host measures from admission (or the runtime's send) to the end; the
  live tab measures from its own start. They can differ by a second, so a
  restored "0:12" may read "0:13".
- A native turn stored before this change has no seconds and reads
  "Claude answered" or "Stopped", not its duration, and not "success".
- The restored agent name is the Agent tab's current agent, which is the
  conversation's agent since conversations are per agent.
- The window's own conversation store keeps no seconds; a runtime turn it
  adopts is recorded with the line the tab showed.
