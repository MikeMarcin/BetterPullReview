---
schema: "implement-rfc.card.v1"
id: "BPR-1817"
title: "An agent chat send goes to the chosen agent, and a refused one says why"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1817--BPR-1817--87e01cb1fff1.json","kind":"defect","found_by":"owner report, 2026-10-29","related_cards":["BPR-0514","BPR-1753","BPR-1773"],"frontend_design":"frontend-design:frontend-design loaded before the composer's refusal strip and the failed-run entry"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "323ab60bf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_chat_custom_agent_contract$'", "detail": "Regression course through the composed application and the fixture server, with no built-in program offered: a custom agent added through Settings › Agents (neutral name, Codex JSONL, the fixture agent program) and chosen in the composer's menu is the one asked by the Agent tab's own Send; the turn runs under its entry slot, its program echoes the typed message exactly once, no refusal shows. Before the fix the same course ran the built-in Codex JSONL profile instead and ended failed_to_start."}}, {"criterion": "AC2", "outcome": "pass", "revision": "323ab60bf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_agent_chat_scope|application_agent_chat_replies)$'", "detail": "Core: agent_send_refusal_of names every refused verdict (host_not_mapped, agent_forbidden_on_host, agent_not_required_one, agent_unknown, session_profile_mismatch, selection_required, unknown_action) and never reads a refused verdict as a send; decide_agent_send orders no_review, session_stale, request_in_flight, no_agent, empty_message. Application: on an explicit GHES host the default profile's send is refused as agent_forbidden_on_host with no turn; choose_agent starts the chosen agent's conversation at the observed revision; the next send's turn names the custom agent's slot; an unknown agent is agent_unknown."}}, {"criterion": "AC3", "outcome": "pass", "revision": "323ab60bf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_workbench_agent_send_refusal$'", "detail": "Send is disabled with no review and its tooltip names why; an empty send sends nothing and says 'Not sent. Type a question, then send.'; a host refusal shows the danger strip above the field naming the agent, keeps the question and shows no turn; Dismiss closes it; a launch-path refusal after a send is said in the same words, one no send waits for is not; a Ctrl+Return while the agent works is refused as still answering; a nonzero exit becomes the turn's answer in words with the agent's last stderr lines, the stopped banner uses the same words and the in-flight strip goes."}}, {"criterion": "AC4", "outcome": "pass", "revision": "323ab60bf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_chat_.*_contract$'", "detail": "The existing positive, stale, continued, anchored-continuation, construction-cut and conversation courses choose the Codex JSONL agent their fixture speaks in the composer's menu, and pass with the send following the menu."}}, {"criterion": "AC5", "outcome": "pass", "revision": "323ab60bf", "evidence": {"kind": "artifact", "detail": "With BPR_AGENT_RENDER_ROOT set, qt_workbench_window --agent-send-refusal writes agent-send-refused-light.png, agent-send-refused-dark.png, agent-run-failed-light.png and agent-run-failed-dark.png offscreen; copies are outside the repository."}}]
---

# BPR-1817: An agent chat send goes to the chosen agent, and a refused one says why

## Report

The owner, 2026-10-29 (paraphrased; the report's own repository details are
private and are not recorded): with a custom agent chosen in the Agent tab's
agent menu, the "is ready" card showing a passing program check, suggestion
chips and the Selected lines scope, pressing Send with a question typed did
nothing. No error, no disabled state, no status. "I can't use an agent search
with my custom agent."

## Root cause

Two defects, one behind the other.

1. **The send never asked the chosen agent.** `agent_chat_session_host`
   started its first session under the composition's default profile (the
   built-in Codex JSONL harness) and sent every turn under its session's
   profile. The composer's agent menu (`agent_launch_panel`'s profile
   selector) was read only by the launch panel's start gesture, the tour
   writing hook and the conversation history, never by a chat send. So a
   custom agent was never run: on a github.com review the built-in Codex was
   asked instead (and failed to start where it is not installed); on a GitHub
   Enterprise Server review, whose policy allows only Claude-protocol and
   plain-text agents, `core::decide_agent_action` refused the default as
   `profile_refused`.
2. **Every refusal before a turn began was silent in the Agent tab.** The
   composition said a chat refusal only with `agent_launch_panel::refuse`,
   which writes "Not started: …" on the launch panel's status label. That
   panel is not shown in the Agent tab, and the tab derived its state from
   "Finished:" alone, so it stayed idle with the question in the field. The
   same applied to the launch path's own refusals (`agent_invocation_already_running`,
   `stale_or_mismatched_agent_context`, `profile_forbidden_on_host`, ...),
   which the panel also dropped outright while an invocation was active; to
   a send with an empty field; to a disabled Send action triggered from the
   keyboard; and to a Ctrl+Return while the agent worked.


## Built

- **Core** (`agent_chat_scope.hpp`). `agent_send_refusal`, sixteen named
  reasons a send produced no turn, with `stable_name`;
  `agent_send_refusal_of(agent_action_verdict)`, which tells a forbidden
  agent, an unmapped server, a required other agent and an unknown agent
  apart by the invocation verdict; and `decide_agent_send(agent_send_facts)`
  for what the composer knows before sending.
- **Application.** `agent_chat_outcome::Send` carries the core's reason for
  every refused send. `agent_chat_session_host::choose_agent` starts the
  chosen agent's conversation at the observed revision (a conversation
  belongs to one agent, owner decision 2026-10-23); the same agent changes
  nothing.
- **Composition.** A send reads the menu's agent at the gesture, as it reads
  the selection, and a refused outcome reaches the Agent tab typed, with the
  launch path's own words as the detail of `launch_refused`.
- **Qt.** The Agent tab asks `decide_agent_send` before sending and shows
  every refusal in a danger strip directly above the field ("Not sent." and
  what to do), keeping the question; Send's tooltip says why it is disabled.
  Launch-panel refusals are counted (also while a turn runs) and said beside
  the composer when a send is waiting. A run that ends without an answer is
  the turn's answer in words plus the last twelve lines of its error stream,
  and the stopped banner says the same words rather than "Finished:
  nonzero_exit".
- Docs: `docs/ux/workbench-specification.md` (Agent tab).

## Design

Loaded `frontend-design:frontend-design` before the strip and the failed-run
entry. The strip reuses the tab's own `rounded` danger look (the stopped
banner's), one plain sentence in sentence case that says what happened and
what to do, and the drawer's close glyph; it sits on the field it is about,
so it reads as the composer's answer rather than a page alert. No QSS; colours
derive from the palette.

## Not verified

- The owner's exact setup (their custom agent, server and repository) was
  not reproduced; the regression course reproduces the mechanism with a
  fixture agent on the fixture github.com server, and the GHES refusal is
  covered at the application level.
- macOS offscreen only; Windows and Linux were not run. No owner look was
  requested.
- Offering the agents again (each setup projection, and opening a review
  thread's Ask agent) selects the saved agent, or the first with none saved,
  as [[BPR-1749]] specifies. Since a send now follows the menu, such a re-offer
  changes which agent the next send asks; the pill shows it, so it is not
  silent, but a reviewer's choice can move under them. Keeping the choice
  contradicts `qt_agent_launch_panel`'s BPR-1749 case and was left for the
  owner to decide; the chat courses choose their agent before each send.
