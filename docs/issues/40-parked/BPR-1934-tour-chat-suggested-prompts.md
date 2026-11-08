---
schema: "implement-rfc.card.v1"
id: "BPR-1934"
title: "Tour chats offer suggested prompts, as the Agent tab does"
status: "Parked"
rfcs: []
depends_on: ["BPR-1925", "BPR-1930"]
priority: "P2"
parked_on: "human-review"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07: 'Tour side chat should get suggestions like the agent tab has'","related_cards":["BPR-1877","BPR-1925","BPR-1930"]}
acceptance: [{"criterion": "AC1 Each tour chat kind, when empty, shows its prompts. A click sends it, and the prompts hide after the first turn (Qt test)", "outcome": "pass", "revision": "5173d9274", "evidence": {"kind": "command", "detail": "qt_tour_side_chat: empty Solution, stop and whole-tour chats offer their lists; one click on 'Too many stops?' asks it in the tour thread; 'Focus on ...' goes to the composer; none while the agent is not set up; hidden after the first turn. Frozen look prints ATTENDED_TOUR_PROMPTS_SHOWN", "path": "docs/issues/40-parked/BPR-1934-tour-chat-suggested-prompts.md#verification-v001-focused"}}, {"criterion": "AC2 Reuses the Agent tab's suggestion component", "outcome": "pass", "revision": "5173d9274", "evidence": {"kind": "command", "detail": "agent_suggestion_list is the Agent tab's component (agent_tab_view uses it) and the tour chat's; qt_workbench_agent_send_refusal and qt_tour_side_chat pass", "path": "docs/issues/40-parked/BPR-1934-tour-chat-suggested-prompts.md#implementation-v001-2026-11-07"}}, {"criterion": "AC3 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "5173d9274", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/.codex/attended-builds/bpr1934-tour-chat-prompts-v001.qfzgw748 of 5173d9274; new short session f8b3689b6fcc266cd0a0467817211a15, candidate BPR-1934-tour-chat-prompts-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1934-tour-chat-suggested-prompts.md#attended-review-v001"}}, {"criterion": "AC4 The documentation gate passes", "outcome": "pass", "revision": "5173d9274", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK", "path": "docs/issues/40-parked/BPR-1934-tour-chat-suggested-prompts.md#verification-v001-focused"}}]
---

# BPR-1934: Tour chats offer suggested prompts, as the Agent tab does

## Change

Load `frontend-design:frontend-design` first.

- An empty tour chat shows suggested prompts in the Agent tab's style and
  component (BPR-1877). This covers each part's chat (Problem, Solution, a
  stop) and the whole-tour chat (BPR-1930). One click sends a prompt, and
  they disappear once the chat has turns. Reuse the Agent tab's suggestion
  component; don't build a second one.
- The prompts fit the chat's scope. Examples:
  - **A stop:** "What does this code do?", "Why does this change matter?",
    "Rewrite this explanation more simply", "Is this the right place for this
    stop?".
  - **Problem or Solution:** "Make this clearer", "What's missing?",
    "Shorter".
  - **The whole tour:** "Too many stops?", "Suggest a better order",
    "Focus on …", "What's not covered?".
  
  Keep the prompt lists in one place in the app layer, not in widgets, so
  Agent settings can later make them editable.
- They are offered only when the agent is set up (BPR-1881's gate), in the
  agent accent.

## Acceptance

- AC1. Each tour chat kind, when empty, shows its prompts. A click sends it,
  and the prompts hide after the first turn (Qt test).
- AC2. Reuses the Agent tab's suggestion component.
- AC3. Owner look: Accept/Reject on a frozen build.
- AC4. The documentation gate passes.

## Implementation v001 (2026-11-07)

`frontend-design:frontend-design` was loaded first. The one bold thing is the
prompts' agent accent; they otherwise keep the Agent tab's quiet chips, so the
two chats read as one product.

- **One component.** The Agent tab's suggestions ([[BPR-1877]]) are now
  `agent_suggestion_list` (`src/adapters/qt/include/bpr/qt/agent_suggestion_list.hpp`):
  the quiet lead line, one `quiet_push_button` chip per prompt (object name
  `agentSuggestion`, the whole prompt in `agentPrompt`, its tooltip and its
  accessible name, a long one shortened in the middle), and [[BPR-1872]]'s
  keys (Shift-click, Shift+Space and Shift+Return edit first; Return and
  Enter choose). `agent_tab_view` uses it in place of its own column and
  event filter; its prompts, gate and press are unchanged.
- **The tour chats.** An empty `tour_side_chat` shows the component under
  its invitation, with the lead "Or try:", in the agent's accent (it joins
  the chat's accented widgets). It goes with the first turn (the chat is
  rebuilt from the session). It shows only while the chat is `Ready`, which
  is [[BPR-1881]]'s readiness from the Tour tab, and its chips are usable
  whenever a question could be sent, including feedback while the draft
  runs. A choice goes through `core::decide_agent_suggestion_press`, as in
  the Agent tab: sent as written with any quoted passage, put in the
  composer to edit (Shift), or sent keeping the reviewer's own words. A
  prompt the reviewer finishes ("Focus on …") always goes to the composer.
- **The lists.** `application::tour_chat_prompts(kind)`
  (`src/application/include/bpr/application/tour_chat_prompts.hpp`) holds
  the three lists in one place: a stop's four, the Problem's and Solution's
  three ("Make this clearer", "What's missing?", "Make it shorter"), and the
  whole tour's four with "Focus on …" marked `Finish`. The window hands the
  chat the source (`tour_side_chat::set_prompt_source`), so Agent settings
  can later make them editable without touching the widget.
- **Look.** `--attended-tour-drafting --attended-tour-prompts` drafts with
  the stub, accepts all, checks each kind of empty chat, leaves the whole
  tour's chat open and prints `ATTENDED_TOUR_PROMPTS:…` then
  `ATTENDED_TOUR_PROMPTS_SHOWN`.
- `review-tours.md` says what the chats offer and where the lists live.

### Verification (v001, focused)

Dev build:
- `qt_tour_side_chat` (extended): the empty Solution chat offers its three
  prompts in `agentSuggestion` chips under "Or try:", in the agent accent;
  none while the agent is not set up, back once it is; gone after the first
  turn; the empty stop chat offers its four; the empty tour chat offers its
  four; "Focus on …" lands in the composer and sends nothing; one click on
  "Too many stops?" asks it in the tour's thread; the prompts hide after
  that turn.
- `qt_workbench_agent_send_refusal` (the Agent tab's suggestions wait for
  setup, on the shared component), `qt_tour_chat_in_tab`.
- `application_review_round_trip_composition_contract`, its app source pin
  resealed from its own output (`87cf2778…`).
- `coding-format-check` passes (clang-format 22.1.8).
- The offscreen look on the dev build, light and dark, prints
  `ATTENDED_TOUR_PROMPTS_SHOWN`.
- `verify_documentation.py` prints `DOCUMENTATION_OK`.

### Attended review (v001)

- Frozen build `/Users/jaeger/.codex/attended-builds/bpr1934-tour-chat-prompts-v001.qfzgw748`
  of `5173d9274` (executable SHA-256
  `718fc3199092548bfb6c46b6d43dd11037c8909a67057a14f6bc3590e87743de`). Its
  launcher's own offscreen check reached `ATTENDED_TOUR_PROMPTS_SHOWN`; the
  pictures in the session are the frozen executable's own offscreen renders,
  light and dark.
- Launch: `python3 /Users/jaeger/.codex/attended-builds/bpr1934-tour-chat-prompts-v001.qfzgw748/launch.py`.
- New short session `f8b3689b6fcc266cd0a0467817211a15`, state
  `/Users/jaeger/.codex/attended-review/bpr1934-v001-0vNUF6/state`, host
  started with `serve --tailscale`.
- One candidate, `BPR-1934-tour-chat-prompts-v001` (revision
  `5173d9274-v001`), with one Accept/Reject question.
- Status: ready, awaiting the owner.
