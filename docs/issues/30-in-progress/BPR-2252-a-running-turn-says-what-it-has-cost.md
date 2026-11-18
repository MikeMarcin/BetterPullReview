---
schema: "implement-rfc.card.v1"
id: "BPR-2252"
title: "A running Agent-tab turn says what it has cost so far"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2252--BPR-2252--b931f3cc910b.json","origin":"owner report 2026-11-17 on current main (0.5.0 prerelease): \"I feel like we used to see cost while a turn was running. I don't see it\"; clarified: \"Yeah I thought we had per turn cost with the agent is thinking timer in the transcript window\"","acceptance":["AC-a","AC-b","AC-c","AC-d"]}
---

# A running Agent-tab turn says what it has cost so far

## What the owner saw

On current main the Agent tab's running line ("● Claude is thinking, 0:12."
or "● Claude is waiting on 2 helpers, 3:12 · last activity 4 s ago.") shows
no cost, and the owner remembers the turn's own cost beside that timer.

## What history says

- The running line (`agent_run_status_words`,
  `src/adapters/qt/src/agent_run_status.cpp`) has never carried a cost: no
  commit touching it ever mentions one. The cost has only been drawn on the
  same line once the turn settled ("Claude answered in 1:12 · $0.41",
  2c4372be5, [[BPR-1999]]; shortened by b1d66829d, [[BPR-2038]]).
- Claude Code reports `total_cost_usd` only on `result` frames; Codex reports
  tokens and never dollars (RFC-BPR-0101). A one-part Claude turn's only
  figure arrives with the frame that ends it, so there is nothing to say
  before it ends.
- 12e9c6fb4 ([[BPR-2004]], RFC-BPR-0103) kept a Claude turn open past its
  first `result` while helpers are out
  (`src/application/src/agent_launch.cpp`, `aPhase = open ? 3U : 2U` in the
  stream-JSON result branch). Before it, that first `result` settled the line
  with its cost while the helpers' work went on; after it, the cost reported
  by that frame (and by each later part, and by a steered run's parts,
  [[BPR-2153]]) is held in the protocol loop's result until the whole turn
  ends, and the line ticks on with no cost. The recorded trace
  `tests/fixtures/agent/traces/claude-2.1.319-helpers-stdin-open.jsonl` has
  four `result` frames with figures in one turn.

## Scope

Publish each figure the runtime reports during a run as a stream event; the
host accounts it against the native session's baseline exactly as the
turn's end does; the Agent tab's running line says it after the time. The
footer total is unchanged (it sums settled turns).

## Acceptance

- AC-a: While a turn runs and the agent has reported a figure, the Agent
  tab's running line shows that turn's cost so far after its time, in the
  RFC-BPR-0101 format (`$0.41`, `< $0.01`).
- AC-b: A run whose agent reported no figure (Codex, or a Claude part before
  its first `result`) shows no cost; `$0.00` is never invented, and a figure
  that went down leaves the running cost unknown (none shown).
- AC-c: A resumed native session's running cost is the turn's own (figure
  less the session's previous figure), equal to what the settled line then
  says.
- AC-d: A Qt test drives a running turn to a reported cost and reads it on
  the running line; the protocol loop test reads the cost events.

## Evidence

Pending.
