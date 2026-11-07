---
schema: "implement-rfc.card.v1"
id: "BPR-1930"
title: "General feedback on the whole tour: the agent revises stops, focus and structure as one proposal"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1925"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07, coordinator chat","related_cards":["BPR-1900","BPR-1914","BPR-1925"]}
acceptance: []
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
