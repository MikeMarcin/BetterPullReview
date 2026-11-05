---
schema: "implement-rfc.card.v1"
id: "BPR-1872"
title: "A click on a suggested prompt submits it"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-05: \"I think by default a click on a suggested prompt should submit it. Maybe a shift click to let you edit before submitting\"","reservation":"docs/issues/_reservations/BPR-1872--BPR-1872--9980bfde697a.json","related_cards":["BPR-1753","BPR-1755","BPR-1773","BPR-1817","BPR-1847"],"frontend_design":"loaded frontend-design:frontend-design; applied its writing guidance: the hint is two plain sentences in the app's tooltip voice, not a middle-dot meta string"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "bdd2e68cf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_agent_turn$'", "detail": "decide_agent_suggestion_press: plain press on an empty field sends; Shift press edits; plain press with a draft sends and keeps the draft; Shift press with a draft adds after it; nothing usable ignores every combination; stable names."}}, {"criterion": "AC2", "outcome": "pass", "revision": "bdd2e68cf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_workbench_window_scale_'", "detail": "Agent tab course (test_the_agent_is_a_drawer_tab, scales 100, 125 and 200): tooltip and accessible name and description; a mouse click starts a turn asking the prompt and empties the field; while it runs the prompts are hidden and disabled and choose_prompt neither sends nor touches typed text; Shift-click fills the field, focused, cursor at end, nothing sent; a click with a draft sends the prompt alone and the draft is still in the field after the turn begins; Shift-Return adds the prompt after the draft on its own line; Return and Space send; Shift-Space edits. Negative control: with the Qt side passing EditFirst=false the course fails at the Shift-click expectation."}}, {"criterion": "AC3", "outcome": "pass", "revision": "bdd2e68cf", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target coding-format-check; python3 scripts/verify_documentation.py --root .", "detail": "clang-format 22 (22.1.8) check passed over 1943 files; DOCUMENTATION_OK; architecture_boundaries passes."}}]
---

# BPR-1872: A click on a suggested prompt submits it

## Request

The owner, 2026-11-05: "I think by default a click on a suggested prompt
should submit it. Maybe a shift click to let you edit before submitting".

## What it was

The Agent tab's suggested prompts ([[BPR-1753]]) are `quiet_push_button`s
built by `agent_tab_view::refresh_suggestions`. A click called
`send_prompt`, which replaced whatever was in the question field with the
prompt and asked. So a click already sent when nothing stopped it, but:

- any words the reviewer had typed were silently replaced and lost;
- when the send was refused ([[BPR-1817]]: no agent chosen, a stale
  session and so on) the prompt was left in the field, which reads as "the
  click only filled the box";
- there was no way to edit a prompt before sending, and nothing said what a
  click does;
- Return on a focused prompt did nothing (a `QPushButton` outside a dialog
  is not auto-default); only Space pressed it.

## Design

The core decides; Qt presents.

- `core::decide_agent_suggestion_press` (`agent_turn.hpp`) takes whether
  Shift was held, whether the field holds typed words (not only whitespace)
  and whether a prompt can be sent now (the review is open and no turn
  runs). It returns `ignored`, `send`, `send_keeping_draft`, `edit` or
  `edit_after_draft`.
- `agent_tab_view::choose_prompt(prompt, editFirst)` carries it out.
  `send_prompt` (also used by an error turn's Ask again) is
  `choose_prompt(prompt, false)`.
  - **send**: the prompt goes in the field and `ask()` runs, so every
    existing guard and refusal ([[BPR-1817]]) applies exactly as for a typed
    question.
  - **send_keeping_draft**: the prompt is sent alone as above, then the
    reviewer's words are put back, and the turn's start does not clear the
    field (`PendingKeepsField`, set from the send action only for this
    send). If the send is refused, the words are put back as well and the
    refusal says why.
  - **edit**: the prompt goes in the field, which takes focus with the
    cursor at the end.
  - **edit_after_draft**: the prompt is added after the words already
    there, on its own line, through the text cursor so one Undo removes it.
- Shift is read from the press itself: an event filter on each prompt notes
  Shift on the mouse release or Space release that clicks it, and handles
  Return or Enter on a focused prompt directly (Shift-Return edits). A
  programmatic or accessibility press (VoiceOver's or Narrator's press
  action) has no Shift and sends.
- Discoverability: the tooltip is the whole prompt (the label may be
  elided), then "Click to send. Shift-click to edit before sending." The
  accessible name stays the prompt; the accessible description is "Sends
  this prompt to the agent. Shift-click or ⇧↩ puts it in the question field
  to edit before sending." (the key in the platform's native form).

### Why a draft is kept rather than refused or sent with the prompt

The request was that a click submits. Refusing the click while the field
holds words would undo that for the common case of a half-typed thought;
sending the words with the prompt would send something the reviewer did not
write as one question. Sending the prompt alone and leaving the words where
they were honours the click and loses nothing. Shift-click never replaces
words either: it adds the prompt after them.

### Busy guard

While a turn runs the prompts are hidden and disabled ([[BPR-1755]]); the
core also returns `ignored` when nothing can be sent, so a call that gets
through anyway neither sends nor touches the field.

## Acceptance

- **AC1:** Core: every combination of Shift, draft and usable decides as
  above.
- **AC2:** Qt public path: a mouse click starts a turn asking the prompt
  exactly; Shift-click fills the field to edit, focused and at its end,
  without sending; a click with typed words sends the prompt alone and the
  words survive the turn's start; Shift-Return adds after the words; Return
  and Space send, Shift-Space edits; nothing happens while a turn runs; the
  tooltip and accessible description say a click sends and Shift-click
  edits.
- **AC3:** clang-format 22 check and the documentation gate pass.

## Owner look

Not requested. The wording follows the existing tooltip voice ("Send the
question to the agent (⌘↩)"), and the behaviour is the one the owner
described, so no attended session was opened.
