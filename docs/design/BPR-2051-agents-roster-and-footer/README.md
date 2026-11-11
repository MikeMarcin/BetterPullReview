# Agents roster and the pull request window's agent footer (accepted design)

The owner chose these two mockups on 2026-11-11 as the accepted design:
"A + footer 1". They follow the rejection of the earlier Agents & context
top ("all ugly", owner look `5ea0ce22`, `06aceb8f`, recorded on
[BPR-2051](../../issues/40-parked/BPR-2051-enhance-draft-comments-and-notes.md))
and the owner's request: "As we make agent a bigger part of this tool, I'm
thinking we might need to move agent picker out of the individual drawers and
into an always present footer on the PR window."

## A · Roster (Settings › Agents & context, top)

[option-a-roster.html](option-a-roster.html), renders
[light](option-a-roster-light.png), [dark](option-a-roster-dark.png), and the
row's sheet [light](option-a-roster-sheet-light.png) and
[dark](option-a-roster-sheet-dark.png).

- The page opens on the agents, one row each, in the order a pull request's
  footer offers them ("Drag to reorder"). A row is the agent's last check as
  a ring (passed, failed, never checked) and in words at the right ("Signed
  in, version 2.1.321", "Not signed in", "Not checked"), its name over its
  model and effort, or what to do ("Run `codex login` in a terminal, then
  check again."), and a chevron. A row opens its sheet.
- Each agent's last check is kept across restarts.
- Check all and Add agent… follow the roster. Under them, one settings
  group: Stop a run after; Share agents and edited prompts with Import
  configuration… and Export configuration… (RFC-BPR-0107's names, shown once
  that RFC is implemented); Remove every agent and set up again with Reset
  agents….
- The next request's choices (agent, notes, draft, working copy) are not on
  this page: the footer picks the agent; the Agent tab what a question
  includes.

## Footer 1 · in the status bar (every pull request window)

[pr-window-agent-footer.html](pr-window-agent-footer.html), renders
[light](footer-1-status-bar-light.png), [dark](footer-1-status-bar-dark.png),
and each state [light](footer-1-status-bar-states-light.png) and
[dark](footer-1-status-bar-states-dark.png).

- The window's existing status bar carries, at its right, the one agent
  picker for the pull request ("Claude Code · Opus 5.5 · High"), used by the
  Agent tab, the tour chat, Enhance and Draft the tour, and the pull
  request's session cost ("$0.53": the Agent tab, the tour chat, Enhance and
  drafting together).
- A run says where it started, what it does and how long, with Stop
  ("Agent tab  waiting on 2 helpers  6:42  Stop"). Several runs read as a
  count that opens the list, each with its own Stop. Quiet for two minutes,
  the tone changes and nothing else moves. Just answered, its ending shows
  for a few seconds, then idle.
- The chosen agent needing setup is a state of the footer: the picker says
  what is missing in the failed check's colour ("Codex · not signed in") and
  Agent settings… leads to Settings. No drawer has a picker or a setup band.
- The picker's menu holds the agents in Settings order, Model, Effort and
  Agent settings…; the scratch folder grant for this pull request joins it
  when RFC-BPR-0104 (amended to put it there) is implemented.
- The review's own status line, to the left, gives way first in a narrow
  window.

## Where it is implemented

`src/adapters/qt/src/agent_footer.cpp` (the footer),
`src/adapters/qt/src/agent_setup_settings.cpp` (the roster),
`application::agent_setup_host::check_all` and `EntryChecks` (each agent's
kept check). RFC-BPR-0101, RFC-BPR-0103, RFC-BPR-0104 and RFC-BPR-0107 are
amended to match.
