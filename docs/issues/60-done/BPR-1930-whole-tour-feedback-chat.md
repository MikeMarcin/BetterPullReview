---
schema: "implement-rfc.card.v1"
id: "BPR-1930"
title: "General feedback on the whole tour: the agent revises stops, focus and structure as one proposal"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1925"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07, coordinator chat","related_cards":["BPR-1900","BPR-1914","BPR-1925"]}
acceptance: [{"criterion": "AC1 \"Too many stops\" or \"merge 2 and 3\" on a 6-stop tour returns a proposal that removes or merges stops. Accepting it changes the tour exactly as shown. Covered by core and Qt tests with a stub agent", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "core_tour_agent_session: on a 6-stop tour a merge of 2 and 3 and a removal of 5 are proposed and accepted exactly as shown (ids in order, merged title and text, both sources' lines kept); qt_tour_agent_session_view: marks and Remove/Merge in place; frozen look prints ATTENDED_TOUR_REVISION:Merge=1,Remove=1", "path": "docs/issues/40-parked/BPR-1930-whole-tour-feedback-chat.md#verification-v001-focused"}}, {"criterion": "AC2 \"Focus on <part>\" yields a proposal whose stops favour that part. The focus chip persists for later turns until cleared", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "core_tour_agent_session (set/clear focus); application_tour_agent_conversation (focus rides the revise and draft prompts); qt_tour_agent_session_view (chip, x dispatches clear); frozen look ATTENDED_TOUR_FOCUS_CHIP", "path": "docs/issues/40-parked/BPR-1930-whole-tour-feedback-chat.md#verification-v001-focused"}}, {"criterion": "AC3 The same feedback works while the initial draft's proposal is still pending, and revises it", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "core_tour_agent_session (queue_tour_feedback, replace waiting stops); qt_tour_agent_session_view (tour chat sends while drafting); frozen look ATTENDED_TOUR_FEEDBACK_WAITING:1 then ATTENDED_TOUR_DRAFT_REVISED:waiting=4:header=0", "path": "docs/issues/40-parked/BPR-1930-whole-tour-feedback-chat.md#verification-v001-focused"}}, {"criterion": "AC4 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "bdd57d419", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/.codex/attended-builds/bpr1930-whole-tour-v001.5kfcmu7t of bdd57d419; new short session d2e623413a8acad6fc6e91bb4f68a7b7, candidate BPR-1930-whole-tour-feedback-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1930-whole-tour-feedback-chat.md#attended-review-v001"}}, {"criterion": "AC5 The RFC amendment and the documentation gate pass", "outcome": "pass", "revision": "bdd57d419", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1930, BPR-1931; review-tours.md; verify_documentation.py prints DOCUMENTATION_OK", "path": "docs/issues/40-parked/BPR-1930-whole-tour-feedback-chat.md#implementation-v001-2026-11-07"}}]
---

# BPR-1930: General feedback on the whole tour: the agent revises stops, focus and structure as one proposal

## Owner (2026-11-07)

"I want to be able to give general feedback on the tour and have it react,
particularly during the initial agent draft but even after. For example I
want to say too many stops/too few stops add/remove some or merge some. Or I
want to say the tour should focus on <this particular part of the PR>."

## Change

Load `frontend-design:frontend-design` first.

- **A whole-tour chat.** It sits with the per-part chats from BPR-1925, inside
  the Tour tab, opened from the tour's header with the same toggle button on
  the right. The person gives feedback on the tour as a whole: "too many
  stops", "merge stops 2 and 3", "add a stop on the retry loop", "focus on the
  renderer changes", "reorder so it starts at main()".
- **During the initial draft.** The person can give the same feedback while
  "Draft the tour" runs, or right after it proposes. The next round revises
  the pending proposal instead of starting over.
- **A whole-tour proposal.** The agent answers with a structured revision of
  the tour, not just text. It can add, remove, merge, split and reorder stops,
  rewrite Problem and Solution, and set a focus. The revision arrives as one
  BPR-1900 proposal that shows the changes clearly:
  - removed and merged stops marked;
  - new stops with their BPR-1900 hunk placement and drop rules;
  - order changes visible.

  The person accepts it all, or accepts and discards per item. Nothing changes
  until they accept.
- **Focus is remembered.** A focus instruction ("focus on X") is remembered
  for the tour's later drafting and chat turns, shown as a small chip the
  person can clear.
- **The core decides.** The core validates the revision: stops anchor to the
  PR, merged stops cover their sources' lines, and the order is valid.

## Acceptance

- AC1. "Too many stops" or "merge 2 and 3" on a 6-stop tour returns a
  proposal that removes or merges stops. Accepting it changes the tour exactly
  as shown. Covered by core and Qt tests with a stub agent.
- AC2. "Focus on <part>" yields a proposal whose stops favour that part. The
  focus chip persists for later turns until cleared.
- AC3. The same feedback works while the initial draft's proposal is still
  pending, and revises it.
- AC4. Owner look: Accept/Reject on a frozen build.
- AC5. The RFC amendment and the documentation gate pass.

## Implementation v001 (2026-11-07)

`frontend-design:frontend-design` was loaded first. The one bold element stays
BPR-1925 v003's: proposals in the tour, grey and dashed until accepted. The
rest is the drawer's quiet chrome. On branch `worktree-agent-a2168bac3b72f3c50`
from `main` `b16722732`. RFC-BPR-0092 Amendment BPR-1930, BPR-1931 and
`review-tours.md` record it.

- **Core** (`tour_agent_session.hpp`):
  - new parts `remove_stop`, `merge_stops`, `split_stop`, `reorder` and
    `focus`;
  - `tour_revision` on `tour_ask_answer`, checked by `propose_tour_changes`
    (distinct stops, a merge keeps every line, a split stays inside its
    stop, an order is every stop once; refusals counted);
  - `apply_tour_revision` (one restorable tour) and `prune_tour_proposal`;
  - the session's `Focus` and `Waiting` feedback (`set_tour_focus`,
    `clear_tour_focus`, `queue_tour_feedback`, `take_tour_feedback`);
  - `merge_tour_proposal` lets a revision of the waiting stops replace them,
    and a structural part supersede one about the same stops;
  - `proposed_in_thread` is false for the tour's chat, so its parts wait in
    the tour.
- **Application**: the tour chat's turn answers in `bpr.tour-revise.v1` and is
  given `waiting_stops` and the focus. Draft the tour is given the focus.
- **Composition** (`window_tours.cpp`):
  - feedback given in the tour chat while Draft the tour runs is kept, then
    sent once the draft proposes;
  - an accepted structural part is restored as one edit;
  - Set focus sets the session's focus, and the chip's × clears it.
- **Qt**:
  - the tour's chat toggle sits at the right of the tour's header, after
    Conversation, and stays at the right when the header wraps;
  - the route marks the stops a part changes (grey dashed ground and ring,
    a pill, a struck title for a removal);
  - removal, merge and split boxes wait on the card of the stop they lead,
    with Remove, Merge or Split at the right;
  - a proposed order waits above the route with Reorder;
  - the focus waits under the panel's line and becomes a chip;
  - the tour chat says how many changes wait in the tour, and shows feedback
    waiting for the draft.
  - A box placed on a stop's card asks for its size afresh. Before this, the
    card kept room for the box's hidden editor, and lines spread apart.
- **Stub and look**: `scoped_writing_agent` answers `bpr.tour-revise.v1`.
  `--attended-tour-drafting --attended-tour-revision`:
  - feedback is given while the draft runs, and the draft is revised before
    it is accepted;
  - Accept all;
  - "Too many stops. Merge 2 and 3, and focus on the renderer changes.",
    shown in place, with its focus set;
  - with `BPR_ATTENDED_TOUR_RESET`, Reset tour, its question and Undo reset.

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

## Accepted

The owner accepted v001 on 2026-11-07 (attended review, no note).
