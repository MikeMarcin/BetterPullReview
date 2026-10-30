---
schema: "implement-rfc.card.v1"
id: "BPR-1823"
title: "The agent picker keeps the reviewer's choice"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1823--BPR-1823--9230875e1d2f.json","kind":"defect","found_by":"BPR-1817 not-verified note; owner ruling, 2026-10-29","related_cards":["BPR-1749","BPR-1753","BPR-1773","BPR-1817"],"frontend_design":"frontend-design:frontend-design loaded before the unavailable pill, its menu and the refusal words"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "29068c3fa", "evidence": {"kind": "command", "command": "ctest --preset core-only -R '^core_agent_chat_scope$'", "detail": "core::decide_agent_pick: before setup is known the first offered is a provisional default; the saved agent, once known, is the first pick and is kept; offered again reordered with another saved agent the kept pick stays; the reviewer's choice is the pick and a later setup update does not move it back; the setup wizard's choice is a choice; a removed agent stays the pick, unavailable, nothing put in its place; with nothing offered nothing is kept. decide_agent_send refuses a held but unoffered agent as agent_unavailable, before no_agent."}}, {"criterion": "AC2", "outcome": "pass", "revision": "29068c3fa", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_agent_launch_panel|qt_workbench_agent_send_refusal|qt_workbench_window_scale_(100|125|200)|qt_workbench_conversation_reveal|qt_workbench_overview)$'", "detail": "qt_agent_launch_panel: the BPR-1749 case is rewritten to the ruling (the saved profile is the first pick only; a re-offer, a setup update with the same or no saved profile keep the reviewer's choice); the setup wizard's choice (choose_profile_from_setup) moves the picker and is kept, and after Reset agents the saved agent is the first pick again; a removed choice leaves no selected profile, one disabled '<name> (unavailable)' entry, Start disabled, and choosing another ends it. qt_workbench_agent_send_refusal: in the Agent tab a setup update and re-offer keep the reviewer's agent in the pill; removed, the pill reads 'Unavailable: Fixture agent', its menu opens with 'Fixture agent isn't available. Choose an agent:' and the one remaining agent, a send sends nothing and says 'Not sent. Fixture agent isn't available. Choose an agent in the agent menu, then send.' with the question kept; choosing the other agent from the menu restores the pill. The Settings mirror case in qt_workbench_window now expects the removed choice kept as unavailable rather than replaced by the first agent."}}, {"criterion": "AC3", "outcome": "pass", "revision": "29068c3fa", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_chat_.*_contract$'", "detail": "Through the composed application and the fixture server: each course chooses the Codex JSONL agent once, and before every later send (first, stale, continued) checks it is still chosen instead of choosing it again, through the refreshes, setup projections and the Agent tab's first showing between. The conversation course checks it right after a review thread's Ask agent. Opening a pull request no longer switches the picker to the agent it was last asked with; the thread follows the picker."}}, {"criterion": "AC4", "outcome": "pass", "revision": "29068c3fa", "evidence": {"kind": "artifact", "detail": "With BPR_AGENT_RENDER_ROOT set, qt_workbench_window --agent-send-refusal writes agent-unavailable-light.png and agent-unavailable-dark.png offscreen; copies are outside the repository. Offscreen forced-style renders are layout evidence only."}}]
---

# BPR-1823: The agent picker keeps the reviewer's choice

## Owner ruling

2026-10-29: "The picker should keep your choice. It should never
automatically switch agents for you, outside of course of the wizard where you
set it up."

This overrules the part of [[BPR-1749]] that selected the saved agent "when
offered, and whenever profiles are offered again", and the [[BPR-1753]]
behaviour (owner decision 2026-10-23) of opening a pull request on the agent
it was last asked with. `qt_agent_launch_panel`'s BPR-1749 case encoded the
first and is rewritten to the ruling.

## Report

[[BPR-1817]] made a chat send go to the agent the composer's picker shows.
The picker jumped back to the saved agent, or to the first agent when none was
saved, whenever agents were offered again: on every agent setup projection,
and so when the Agent tab first showed after a thread's Ask agent. The next
question could go to a different agent than the reviewer chose.

## Every place that set the selection

- `agent_launch_panel::offer_profiles` (every host offer and setup
  projection): rebuilt the menu and selected the saved or first profile.
- `agent_launch_panel::prefer_profile` (every setup projection): selected the
  saved profile at once.
- `github_application`'s `present_conversation`: on opening a pull request,
  chose the agent it was last asked with in the Agent tab's menu.
- Reviewer gestures, kept as choices: the pill menu's
  `agent_tab_view::choose_agent`, the Settings › Agents & context mirror combo
  that shares the panel's model, and test courses' direct `setCurrentIndex`.
- Not changed: Send to Agent's own sheet (`review_cycle_desktop::prefer_send_profile`),
  which follows the composer's pick through `observe_conversation_agent` and
  is a per-send dialog of its own.

## Built

- **Core** (`agent_chat_scope.hpp`). `agent_pick` (the agent held and whether
  it is the reviewer's kept choice) and `decide_agent_pick`: a choice made now
  (the reviewer's, or the setup wizard's) is the pick; a kept pick stays,
  offered or not; otherwise the saved agent when offered, else the first,
  which becomes kept once the setup is known. `agent_pick_available`.
  `agent_send_refusal::agent_unavailable` and `agent_send_facts::AgentUnavailable`,
  decided after a running turn and before `no_agent`.
- **Qt** (`agent_launch_panel`). The panel asks `decide_agent_pick` on every
  offer, setup update and menu change, and presents it. A change it did not
  make itself is the reviewer's choice. `choose_profile_from_setup` is the
  wizard's choice, `forget_choice` is Reset agents. A kept agent no longer
  offered is a disabled "*name* (unavailable)" entry with no profile, so
  nothing can start or be sent to it.
- **Agent tab.** The pill says "Unavailable: *name*" (state first, so a
  narrow, elided pill keeps it), its tooltip says why, and its menu opens with
  "*name* isn't available. Choose an agent:", the agents and Agent settings….
  A send is refused with "Not sent. *name* isn't available. Choose an agent in
  the agent menu, then send." and the question stays. The not-set-up state
  counts offered agents only.
- **Composition.** Each setup projection passes the saved agent as the first
  pick only. The wizard's Choose and its Custom agent… add take the setup's
  choice into the picker (`take_setup_choice`). Opening a pull request whose
  kept thread is another agent's switches the conversation to the chosen
  agent's own thread rather than the picker. Reset agents forgets the choice.
- Docs: `docs/ux/workbench-specification.md` (Agent tab).

## Design

Loaded `frontend-design:frontend-design` before the unavailable state. The
pill stays the one place the agent is chosen; it keeps its shape and palette
and changes only its words, putting the state first because the pill elides
on the right in the field's row. The refusal reuses BPR-1817's danger strip
and sentence shape ("Not sent." and what to do), so a reviewer meets one
vocabulary. No QSS; colours derive from the palette.

## Not verified

- macOS offscreen only; Windows and Linux were not run. No owner look was
  requested.
- An agent the review's server forbids is also not offered, so it reads as
  unavailable rather than as "not allowed here"; the tooltip names both
  causes. A host-specific word would need the host's refusal reason in the
  offer.
- RFC-BPR-0092's text still describes the pull request opening on its
  last-asked agent; the ruling and this card supersede it, and the RFC was
  not amended.
