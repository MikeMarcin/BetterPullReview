---
schema: "implement-rfc.card.v1"
id: "BPR-2285"
title: "The status bar says thinking or working and its cost as it arrives; a shortened command in the Agent tab opens to the whole of it"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2285--BPR-2285--706f527c5355.json","related_cards":["BPR-2051","BPR-2005","BPR-2007","BPR-2252"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8cbfe1f06","evidence":{"kind":"command","detail":"qt_agent_footer: running a long git command says working with the command in the tooltip; thinking, writing and the thinking stage say thinking or working; Stop all and the picker do not move"}},{"criterion":"AC2","outcome":"pass","revision":"8cbfe1f06","evidence":{"kind":"command","detail":"qt_agent_footer: $0.53 settled plus a running $0.20 shows $0.73 then $0.88 at $0.35, tooltip says this turn so far; the settled $0.47 counts the turn once"}},{"criterion":"AC3","outcome":"pass","revision":"8cbfe1f06","evidence":{"kind":"command","detail":"core_agent_grant (many-line command kept whole, cut at 4096, a file name at 240, a helper's command whole); application_agent_helper_turn (a Claude Bash heredoc reaches the activity whole and trimmed)"}},{"criterion":"AC4","outcome":"pass","revision":"8cbfe1f06","evidence":{"kind":"command","detail":"qt_agent_activity_view: a short command has no disclosure; a heredoc and a long command are shortened with a closed disclosure that takes focus, opens on Space to the whole command, stays open on a redraw and closes; the live line shortens the first line"}},{"criterion":"AC5","outcome":"pass","revision":"8cbfe1f06","evidence":{"kind":"command","detail":"architecture_boundaries, format_check on the touched files (clang-format 22.1.8), verify_documentation, qt_workbench_window_scale_100, application_ask_menu_public_path, qt_tour_side_chat"}}]
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

## Done (2026-11-19)

Built at `8cbfe1f06` on `claude/v0-5-0-prerelease-targets-52265d`. The
frontend-design skill was loaded for the Qt surfaces. No look was asked for.

- Footer (`agent_footer`): `agent_footer_state_words` gives the word; the
  label's minimum width is the wider of "thinking" and "working";
  `total_cost` adds each shown run's `Status.Cost`, and `present_cost` drops
  that run's figure once the settled total holds it.
- Core (`agent_activity`): a running command's subject, a helper's command
  and `CommandsRun` keep `maximum_agent_command_bytes`.
- Decoder (`agent_launch.cpp`): `whole_command` for Claude Bash, Codex exec
  and both App Server command sites.
- Agent tab (`agent_activity_view`): each folded command row keeps the
  disclosure's room, so rows line up; the chevron is a `quiet_tool_button`
  a line tall; the whole command is in the fixed face, selectable.
- Not done: no running cost estimate from token usage (see "Cost reporting,
  as found"); the live line has no disclosure, since it is a sentence redrawn
  every second, and says the command's first line shortened.

