---
schema: "implement-rfc.card.v1"
id: "BPR-2285"
title: "The status bar says thinking or working and its cost as it arrives; a shortened command in the Agent tab opens to the whole of it"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2285--BPR-2285--706f527c5355.json","related_cards":["BPR-2051","BPR-2005","BPR-2007","BPR-2252"]}
---

# BPR-2285: The status bar says thinking or working and its cost as it arrives; a shortened command in the Agent tab opens to the whole of it

## Asked

Owner, 2026-11-19: "In the status bar I don't like that we show the tool the
agent is running because it causes the status bar to shift all over. Just
one of 'thinking' or 'working' is enough so it stays out. The status bar is
also seemingly not showing cost until it's finished it'd be nice to update
that dynamically. The tool calls that show in the agents tab get shortened
which makes sense but there should be a disclosure to see the full command."

## Acceptance

- AC1. While a turn runs, the window footer ([[BPR-2051]]) says one word:
  "thinking" until the runtime says the agent does something else, then
  "working". The tool, file or command is never that word; it stays in the
  word's tooltip. The word's room is the wider of the two words, so changing
  between them moves nothing beside it. Before the run starts (making or
  checking the workspace, [[BPR-2261]]) and once a run is quiet
  (RFC-BPR-0103's two minutes), the footer keeps saying so.
- AC2. The footer's cost adds each running turn's cost so far
  (`agent_run_status::Cost`, [[BPR-2252]]) to the settled total as the agent
  reports it; when the turn settles its place's figure holds the turn once.
  Its tooltip names the turn's figure so far.
- AC3. A command keeps every line, up to 4096 bytes
  (`core::maximum_agent_command_bytes`), from the decoders (Claude Bash, Codex
  exec and App Server, the agent's own and its helpers') through the core's
  activity. Other subjects keep the 240-byte bound.
- AC4. In the Agent tab's folded activity rows, a command is its first line,
  at most 100 characters. Where anything was left out, a closed disclosure
  before it ("Show the full command") opens to the whole command in the fixed
  face, selectable; it takes keyboard focus and Space opens it. An opened
  command stays open when the rows are drawn again. The live line and helper
  rows say a command by its first line, shortened, with the whole of it in
  the helper row's tooltip.
- AC5. No widget performs an effect; `coding-format-check` passes on the
  touched files.

## Cost reporting, as found

Claude Code reports `total_cost_usd` only on a `result` frame, and Codex
reports no cost. Mid-turn figures therefore arrive only where a turn has
more than one result (a steered part, a result written while helpers are
still out). AC2 shows each figure the moment it arrives; a single-part turn
still gets its figure at its end. A token-priced running estimate would need
a price table per model and was not built.
