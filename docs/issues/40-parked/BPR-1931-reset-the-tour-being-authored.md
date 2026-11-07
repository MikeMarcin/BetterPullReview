---
schema: "implement-rfc.card.v1"
id: "BPR-1931"
title: "The author can reset or clear the tour they are authoring"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07, coordinator chat: 'I want the ability to reset/clear the tour that I'm authoring'","related_cards":["BPR-1900","BPR-1925"]}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 Reset clears the local tour draft, proposals, chats and focus. Undo restores them exactly (Qt and host tests)", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "core_tour_agent_session (reset_tour, tally, restore gives the draft back exactly); qt_tour_agent_session_view (question, Keep, Reset, Undo dispatch); frozen look ATTENDED_TOUR_RESET_DONE:stops=0:chats=0:focus=0:proposal=0 then ATTENDED_TOUR_RESET_UNDONE:stops=4:chats=1:focus and merge back", "path": "docs/issues/40-parked/BPR-1931-reset-the-tour-being-authored.md#verification-v001-focused"}}, {"criterion": "AC2 A published tour on GitHub is untouched (a test with a fixture)", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "core_tour_agent_session: reset of a fixture draft bound to a published tour keeps the binding and retires the published stop whole; the host's reset makes no GitHub request (restore through the tour host only)", "path": "docs/issues/40-parked/BPR-1931-reset-the-tour-being-authored.md#verification-v001-focused"}}, {"criterion": "AC3 Owner look: may share a session with the whole-tour feedback card", "outcome": "pending", "revision": "bdd57d419", "evidence": {"kind": "observation", "detail": "Same frozen build and session d2e623413a8acad6fc6e91bb4f68a7b7, candidate BPR-1931-reset-tour-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1931-reset-the-tour-being-authored.md#attended-review-v001"}}, {"criterion": "AC4 The documentation gate passes", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK", "path": "docs/issues/40-parked/BPR-1931-reset-the-tour-being-authored.md#implementation-v001-2026-11-07"}}]
---

# BPR-1931: The author can reset or clear the tour they are authoring

## Change

- The Tour tab offers **Reset tour**, which clears Problem, Solution, every
  stop, any pending proposal, and the tour's chats and focus. It confirms
  with a plain sentence that names what is lost. It can be undone, at least
  for the session.
- It never touches a published tour. If the tour was published, say that
  resetting affects only the local draft.
- Persistence goes through the host. The widget performs no effects.

## Acceptance

- AC1. Reset clears the local tour draft, proposals, chats and focus. Undo
  restores them exactly (Qt and host tests).
- AC2. A published tour on GitHub is untouched (a test with a fixture).
- AC3. Owner look: may share a session with the whole-tour feedback card.
- AC4. The documentation gate passes.

## Implementation v001 (2026-11-07)

`frontend-design:frontend-design` was loaded first. Implemented with
[[BPR-1930]] on branch `worktree-agent-a2168bac3b72f3c50`. RFC-BPR-0092
Amendment BPR-1930, BPR-1931 records it.

- **Core**:
  - `reset_tour` empties the draft. Its identity, head and published binding
    stay, and a published stop is retired as Remove retires it.
  - `reset_tally_of` counts what a reset would clear.
- **Composition** (`window_tours.cpp`):
  - `reset_tour` restores the empty draft through the tour host, as one
    edit, and keeps the draft, session (chats, focus) and proposal it
    cleared;
  - `undo_tour_reset` restores all of them while the draft's revision is the
    reset's, and Undo reset goes once the tour changes;
  - no GitHub request is made, and nothing is reset while the agent runs.
- **Qt**: Reset tour is a quiet action in the tour's header.
  - It asks first in one sentence naming what is lost; for a published tour
    it says the tour on GitHub stays as it is.
  - Keep the tour or Reset tour answer the question.
  - Undo reset is offered after a reset.
  - The widget only dispatches.

### Verification (v001, focused)

On `bdd57d419`, dev build:
- `core_tour_agent_session`: the revision's checks, accept-as-shown for merge, removal, split and
  reorder, refusals, waiting-stop replacement, focus and waiting feedback, reset and its restore.
- `application_tour_agent_conversation`: the revise prompt (waiting stops, focus) and its strict
  decoding.
- `qt_tour_agent_session_view`: in-place marks, boxes on the stops' cards, Reorder above the
  route, the focus chip, feedback while drafting, Reset's question, Keep, Reset and Undo.
- Unchanged and passing: `qt_tour_chat_in_tab`, `qt_tour_side_chat`, `qt_tour_click_scroll`,
  `qt_tour_stop_card`, `qt_review_tour_*`, `qt_scoped_prose_editor`,
  `qt_drawer_surfaces_fit_scale_100/125/200`, `application_agent_launch`,
  `application_review_tour_writing` (+ wrapper), `architecture_boundaries`,
  `architecture_boundary_contract`.
- `application_review_round_trip_composition_contract`, resealed from its own output
  (`b88fea15…`).
- `coding-format-check` passes (clang-format 22.1.8), and `verify_documentation.py` prints
  `DOCUMENTATION_OK`.
- Offscreen look on the frozen executable, with `BPR_ATTENDED_TOUR_RESET=1`:
  `ATTENDED_TOUR_FEEDBACK_WAITING:1`, `ATTENDED_TOUR_DRAFT_REVISED:waiting=4:header=0`,
  `ATTENDED_TOUR_REVISION:Merge=1,Remove=1,Set focus=1`,
  `ATTENDED_TOUR_ROUTE_MARKS:1=-,2=Merge with 3,3=Merges into 2,4=Remove`,
  `ATTENDED_TOUR_FOCUS_CHIP:Focus: the renderer changes`, `ATTENDED_TOUR_RESET_ASKS:...`,
  `ATTENDED_TOUR_RESET_DONE:stops=0:problem=0:chats=0:focus=0:proposal=0`,
  `ATTENDED_TOUR_RESET_UNDONE:stops=4:chats=1:focus=Focus: the renderer changes:merge=1`.
- Not mine: `tests/core/agent_context_supplement_tests.cpp` fails to compile on the base
  `b16722732` (structured binding count); untouched here.

## Attended review (v001)

- Frozen build `/Users/jaeger/.codex/attended-builds/bpr1930-whole-tour-v001.5kfcmu7t` of `bdd57d419` (executable SHA-256
  `e5e812e2c5ae2e87e7df3039826ea541bc4e82d5b87fb3483358ea5159d88188`).
  Its launcher's own offscreen check reached `ATTENDED_TOUR_REVISION_SHOWN`.
  The session's pictures are the frozen executable's own offscreen renders.
- Launch: `python3 /Users/jaeger/.codex/attended-builds/bpr1930-whole-tour-v001.5kfcmu7t/launch.py`.
- New short session `d2e623413a8acad6fc6e91bb4f68a7b7`, state
  `/Users/jaeger/.codex/attended-review/bpr1930-v001-k7Qx2m/state`, host started with `serve --tailscale`.
- Candidates `BPR-1930-whole-tour-feedback-v001` and `BPR-1931-reset-tour-v001`
  (revision `bdd57d419-v001`), one Accept/Reject question each.
- Status: ready, awaiting the owner.
