---
schema: "implement-rfc.card.v1"
id: "BPR-1881"
title: "Tour tab: a call to action to set up the agent, and agent features in one colour"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
parked_on: "human-review"
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, in coordinator chat","related_cards":["BPR-1878","BPR-1879"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "101c59fe2", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_setup_call (tests/adapters_qt/review_tour_agent_setup_call_tests.cpp): set unfinished, tourAgentSetupCall is the tab's first widget (top 0) above the steps, and nothing asks for the setup by itself; its Set up the agent asks the window once; checking shows no call; ready hides it. qt_workbench_window_scale_100 (test_an_agent_error_is_an_error_and_suggestions_wait_for_setup): the Tour editor's agent_readiness follows the Agent tab's verdict (ready with no setup adopted, unfinished on a failed check with the call shown and the Agent tab not setting up, ready with the Ready line and the call hidden), and the Tour tab's Set up the agent puts the Agent tab in setting_up at Check it works, as Finish setup does. Frozen build's look: ATTENDED_TOUR_SETUP_CALL_SHOWN, and with BPR_ATTENDED_TOUR_SETUP=open ATTENDED_TOUR_SETUP_OPENED (the Agent tab at its setup)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "101c59fe2", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_setup_call: in light (#E7E7E7 window, white base) and dark (#323232 window, #1E1E1E base) palettes, before and after setup, agent_accent is at least 4.5:1 on window and base; every agent feature (tourProblemMagic, tourSolutionMagic, tourStopMagic, tourSuggestOutline, tourInsertOutline, tourAgentField with the picker) has the accent as WindowText and ButtonText, each button carries the agent spark icon, the call's title and button wear the same accent, an ordinary control (tourProblemMap) does not, and an application palette change re-derives it. Accent #A8226E light (6.7:1 white, 5.4:1 #E7E7E7), #F27AC0 dark (7.5:1 #0D1117, 5.1:1 #323232)."}}, {"criterion": "AC3", "outcome": "pass", "revision": "101c59fe2", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_setup_call: unfinished, every agent feature but the bar's picker is disabled with a tooltip naming 'not set up' and 'Set up the agent'; a click asks nothing; disabled words are the faded accent; checking says 'Waiting'; ready, all are enabled, tooltips say what they do, a click asks, the accent stays. The picker stays usable: it chooses the Agent tab's agent and runs nothing. qt_review_tour_agent_help and qt_review_tour_steps still pass."}}, {"criterion": "AC5", "outcome": "pass", "revision": "101c59fe2", "evidence": {"kind": "command", "detail": "python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK (run with PYTHONPATH at the main checkout's .agents/skills/implement-rfc/scripts, which the worktree lacks); coding-format-check passed."}}]
---

# BPR-1881: Tour tab: a call to action to set up the agent, and agent features in one colour

## Owner request (2026-11-06)

The Tour tab's AI drafting uses the agent that the Agent tab sets up. Without
that setup (agent, Repository, Check it works), the agent has much less
context. The owner does not want the wizard to open by itself, on the Tour tab
or by sending the reviewer to the Agent tab, because either would be
disorienting. Instead:

1. When the agent is not yet set up, show a call to action at the top of the
   Tour tab saying to set up the agent, with one click that takes the reviewer
   to the Agent tab's setup.
2. Colour-code every agent feature button on the Tour tab. Use the same colour
   as the call to action.
3. Disable those buttons until setup is finished. A disabled button's tooltip
   says why.
4. Agent features keep that colour after setup too, so the reviewer can tell
   at a glance what is ordinary and what is agentic.

## Acceptance

- AC1. With no finished agent setup, the call to action sits at the top of
  the Tour tab. Its click opens the Agent tab at its setup. It disappears once
  setup is finished.
- AC2. Every agent-driven control on the Tour tab, including the BPR-1878
  bottom picker bar where it fits, uses one agent accent colour that matches
  the call to action. It is legible in light and dark themes and meets
  contrast requirements.
- AC3. Before setup those controls are disabled, with a tooltip naming the
  reason. After setup they are enabled and keep the accent.
- AC4. Owner look: an attended Accept/Reject on a frozen build that shows
  both states.
- AC5. `python3 scripts/verify_documentation.py --root .` passes.

## Implementation

v001, commit `101c59fe2` (frontend-design skill loaded first; it chose the
accent):

- The setup-finished state is the Agent tab's own: the core's
  `decide_agent_suggestions` verdict that already holds the suggested
  prompts back, read through `agent_tab_view::setup_readiness()`
  (`checking` until the setup has looked for agents, `unfinished` for
  `no_agent` or `setup_unfinished`, else `ready`). The Agent tab tells a
  readiness observer when it changes; `workbench_window` hands it to
  `review_tour_editor::set_agent_readiness`. No second source of truth.
- The call to action (`tourAgentSetupCall`) is the tab's first row, compact
  so the BPR-1878 bottom bar keeps its place: "Drafting help needs the
  agent." with **Set up the agent**, and "You can still write the tour
  yourself." under it. Its click asks the window, which calls
  `agent_tab_view::open_setup()` (the same path as **Finish setup**: the
  setup at its first unfinished step) and shows the Agent tab. Nothing
  opens the setup by itself.
- `bpr/qt/agent_feature.hpp`: `agent_accent` (#A8226E in a light window,
  #F27AC0 in a dark one; magenta, a hue no state in the application uses:
  blue unread and focus, green accepted, red defect, violet fixed, teal
  verified, amber the route) and `apply_agent_accent`, which sets only the
  text roles and is re-run on palette change. Every agent feature also
  carries the agent's spark glyph, so colour is not the only sign.
- Showing the Tour tab lets the setup look for agents
  (`agent_tab_view::look_for_agents`, the Agent tab's one-shot first-shown
  observer), as showing the Agent tab does (RFC-BPR-0092 Q2: the setup
  looks when it shows; the call to action is its way in). Without this the
  tab would stay `checking` until the Agent tab was opened.
- The `--attended-tour-authoring` look now waits for the setup and ends on
  the call to action (`ATTENDED_TOUR_SETUP_CALL_SHOWN`), the frozen build's
  ready line. `BPR_ATTENDED_TOUR_AGENT_PATH` gives the setup a search path of
  its own (only a stub `claude`), which it counts as set up
  (`ATTENDED_TOUR_AGENT_READY`); `BPR_ATTENDED_TOUR_SETUP=open` presses the
  call (`ATTENDED_TOUR_SETUP_OPENED`); `BPR_ATTENDED_TOUR_THEME=dark` draws
  dark. A whole walk through the wizard was not automated: the person's
  session finds this Mac's own `claude` and `codex`, and the check would run
  the owner's real agent.
- `docs/architecture/review-tours.md` describes it.

## Attended review (AC4)

- v001: frozen build of `101c59fe2` at
  `/Users/jaeger/bpr1881-tour-agent-setup-v001.x0l4w0_9` (executable SHA-256
  `4209fa1a2909a21ad65535cd68400d244b721ee84ab42d0cbd50e6ca0c816750`),
  fixture `conventional-comments-v1`, `--attended-open-file
  src/engine/renderer.cpp --attended-tour-authoring
  {build}/tests/end_to_end/scoped_writing_agent`, ready line
  `ATTENDED_TOUR_SETUP_CALL_SHOWN`. The launcher's own offscreen check
  reached it. Launch: `python3
  /Users/jaeger/bpr1881-tour-agent-setup-v001.x0l4w0_9/launch.py`.
- Pictures from that executable, offscreen: not set up and set up, light and
  dark, and the setup opened by the call to action.
- New short session `abd3d5ed1c27a189082a3b4dc12ab0ee`, state
  `/Users/jaeger/.codex/attended-review/bpr1881-v001-SLFlPX/state`, repo root
  the implementing worktree, host started with `serve --tailscale`. One
  candidate, `BPR-1881-tour-agent-setup` (revision `101c59fe2-v001`), with one
  Accept/Reject question. Status: ready, awaiting the owner.
