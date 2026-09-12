---
schema: "implement-rfc.card.v1"
id: "BPR-0745"
title: "The one-pane Read state is redrawn over the link map"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"bd627bcb-7e53-4842-85f1-1efcf3f5e363"}
parked_on: "human-review"
acceptance: [{"criterion":"The redrawn state renders and is inspected headless at 1500 px, and verify_documentation.py is DOCUMENTATION_OK","outcome":"pass","revision":"c0d3c5da","evidence":{"kind":"observation","detail":"docs/design/RFC-BPR-0028-workbench-direction-v7.html rendered with Google Chrome --headless=new --window-size=1500,1180 --force-device-scale-factor=2 over state 2b extracted with the page's own stylesheet, and the PNG inspected rather than only produced: the two link-map ribbons and their spine land on the struck and added rows of the changes at base 413 and base 422, the current change's spine is the signal blue and the other is ink-3, the whole-file minimap and lens of v6 are gone, the strip's one-pane toggle is gone, and the View menu with Line numbers and One pane both checked renders below the window with its caption. A first render was wrong and was repaired rather than described: the new menu block collided with the page's existing .menu class, which is position:absolute for the epoch line's context menu (v7 line 62), and floated over the window; the block is .vmenu. python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK."}},{"criterion":"The owner accepts the redrawn state, the reworded Modalities amendment and AC-05 in an attended session, and answers block order and where the choice lives; the decision is recorded on BPR-0734, on this card and on RFC-BPR-0028","outcome":"pending","revision":"c0d3c5da","evidence":{"kind":"observation","detail":"The two answers arrived ahead of the session and are recorded on this card, on BPR-0734 and on RFC-BPR-0028: block order is unified-diff order ('don't care, unifed diff order is fine') and the choice lives in the View menu ('Should be in the view menu like line numbers'), the latter read against workbench_window.cpp:5919-5951, workbench_interaction.hpp:345 and workbench_settings_database.cpp:44-101 to mean per window, seeded from a profile default, with no modality of its own, so Review takes one pane too. The owner's acceptance of the redrawn state and of the reworded Modalities amendment is not given and is not synthesised: it is asked as candidate BPR-0734-read-one-pane in the v009 packet, from v7 rather than from a new candidate, with the Review consequence and the removed whole-file overview named beside the ask."}},{"criterion":"BPR-0729's scope and acceptance are rewritten from the accepted state before BPR-0729 is claimed","outcome":"pending","revision":"c0d3c5da","evidence":{"kind":"observation","detail":"Waits on the owner's acceptance of the v7 state; this criterion lives on BPR-0734 and stays there. BPR-0729 is unchanged and still Backlog behind BPR-0734. One fact for that rewrite is now on this card: a new boolean in the workbench settings DDL lands in both window_layout and workbench_preferences and moves workbench_settings_schema_version from 6 to 7 (workbench_settings_database.hpp:35, ladder at .cpp:113-116,141-143)."}}]
---

# The one-pane Read state is redrawn over the link map

The owner, 2026-09-11, rejecting `BPR-0734-read-one-pane` in the v008
attended session (decision `bd627bcb`, 04:32:44Z), verbatim:

> The one-page should also use a link map as the p4merge example provided.
> Relax the non-goals restriction to allow extending the link-map in this
> limit direction.

One screenshot,
[p4merge in its single-pane mode](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-p4merge-single-pane-link-map-2026-09-11.png):
one merged document in reading order, and at the window's left edge a
narrow column carrying the same curved connectors p4merge draws between two
panes. It is the third of the owner's p4merge captures, after
[the two from v006](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png)
that [[BPR-0729]] was carded from.

## What the verdict decides

[[BPR-0734]]'s round drew state 2b with a *change map*: "One column has
nothing to link across, so the strip the link map held between the panes
becomes a change map at the left … The two-pane state's link map is
untouched." Its third open question asked whether that new element should be
named as new, so the RFC's non-goal against changing the link map could stay
literal.

The owner answered by rejecting the premise. One pane is the link map's
limit case, not a different element: the map moves to the left edge and goes
on being the link map. RFC-BPR-0028's non-goals are relaxed to allow
extending it in that direction, and the relaxation is already recorded on
the RFC (2026-09-11, v008 verdict). This card draws the consequence.

Nothing else in state 2b was disputed, and the two amendment texts were
neither accepted nor refused — they were rejected with the state they
describe, so they come back with it.

## Still unanswered, and asked again

The owner answered one of BPR-0734's three questions. The other two carry
forward unchanged into this round's ask; they are not settled by silence and
must not be decided here:

1. **Block order.** Struck old rows above the new rows, the unified-diff
   order the state was drawn in, or the kept side's block first with the
   struck block below it, which is what the p4merge captures show.
2. **Where the choice lives.** One pane as a property of Read remembered per
   window, with Review keeping two panes, or a property of the window that
   Review inherits too.

Draw the state in the same position BPR-0734 chose, say so plainly beside
the ask, and offer the flip.

## Scope

- A new design asset version (`docs/design/RFC-BPR-0028-workbench-direction-v7.html`,
  a copy of v6 with state 2b redrawn; never an edit to an accepted one), in
  which the one-pane presentation keeps the link map at the left edge rather
  than a change map. Settle and draw what the map connects when there is one
  column: the two sides' line identities within the single document, the
  visible window as its lens, and how it reads where a change is pure
  insertion or pure deletion.
- The v5 and v6 colours and CSS variables stay as they are; the state
  invents no colour and keeps applying the shipped `--add`, `--del`,
  `--add2` and `--del2` fields to whole rows.
- Rewrite the proposed Modalities amendment so it describes the link map in
  both presentations instead of a new gutter element; the AC-05 amendment
  text is unchanged unless the redraw moves a thread's anchor.
- Word the RFC's relaxed non-goal precisely against the redrawn state. The
  non-goal now permits extending the link map to the one-pane presentation
  and nothing further: the two-pane ribbons, the diff themes and the thread
  map stay settled work.
- Owned paths: `docs/design/`, `docs/rfcs/RFC-BPR-0028-review-workbench-direction.md`,
  this card and [[BPR-0734]]. No product source.

## Acceptance

- The redrawn state renders and is inspected headless at 1500 px, and
  `verify_documentation.py` is DOCUMENTATION_OK.
- The owner accepts the redrawn state, the reworded Modalities amendment and
  AC-05 in an attended session, and answers block order and where the choice
  lives; the decision is recorded on [[BPR-0734]], on this card and on
  RFC-BPR-0028. BPR-0734's candidate is asked again from this state rather
  than a new candidate replacing it.
- [[BPR-0729]]'s scope and acceptance are rewritten from the accepted state
  before BPR-0729 is claimed, which is BPR-0734's second criterion and stays
  there.

## Owner answers, 2026-09-12 (mandate r11)

Both questions this card carried forward were put to the owner and both are
answered.

1. **Block order.** Verbatim: "**don't care, unifed diff order is fine**".
   That is the order [[BPR-0734]] already drew — a change is its struck rows
   and then its added rows — so no row moves. The question is closed and is
   not asked again.
2. **Where the choice lives.** Verbatim: "**Should be in the view menu like
   line numbers**". So it is not a strip control, which is what BPR-0734
   drew, and the answer names a model rather than a scope. The model was
   read out of the code rather than guessed:
   - The View menu exists and today holds exactly two items, `&Line numbers`
     and `&Settings…` (`src/adapters/qt/src/workbench_window.cpp:5919-5951`).
   - Line numbers is a checkable action with `Alt+Shift+5`
     (`:5925-5934`) that dispatches `core::toggle_workbench_line_numbers_intent`
     and never decides the check mark itself (`:5939`, `:6096-6116`); the
     core refuses it when no comparison surface is active
     (`src/core/src/workbench_interaction.cpp:154-161`, `:319-341`).
   - It is **one scalar with no modality dimension**
     (`src/core/include/bpr/core/workbench_interaction.hpp:345`), and
     `apply(set_workbench_modality_intent)`
     (`workbench_interaction.cpp:395-419`) does not touch it, so changing
     modality does not reset it.
   - It is persisted **per window** in `window_layout` and **seeded for a
     new window** from the single-row `workbench_preferences`, both from the
     same DDL (`src/adapters/sqlite/src/workbench_settings_database.cpp:44-89`,
     `:93-101`, `:52`); `tests/end_to_end/profile_window_layout_public_path_tests.cpp:191-215`
     is the proof that window A's toggle does not move window B and that a
     fresh window takes the profile default.

   Read faithfully, therefore: one pane is a checkable View item, per window,
   seeded from a profile default, with no modality of its own — **which means
   Review shows one pane too while it is checked.** That is the opposite of
   the position BPR-0734 drew (a property of Read, with Review keeping two
   panes). It is a reading of an analogy, not a verbatim instruction, so the
   redrawn state takes it and says so plainly, and the other reading is
   offered with the ask.

   One consequence is worth naming for whoever builds it: a new boolean in
   that DDL lands in both tables and **moves `workbench_settings_schema_version`
   from 6 to 7** (`workbench_settings_database.hpp:35`, the migration ladder
   at `.cpp:113-116,141-143`), with precedent at v3, v4 and v6. That is
   [[BPR-0729]]'s problem, not this card's, but it belongs on the record
   before BPR-0729 is rewritten.

## Round output, 2026-09-12

The round's state is
[workbench states v7, "Read, one pane"](../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane)
(state 2b, anchor `read-one-pane`). v7 is a copy of v6 with that one state
redrawn; every other state, the diff colours and the CSS variables are v6's,
and the state still invents no colour and still applies the shipped `--add`,
`--del`, `--add2` and `--del2` fields to whole rows.

### What the redraw changes

- **The gutter is the link map, not a change map.** v6 put a change map
  there: the whole file's changes as ticks, a lens for the visible window,
  and on-screen changes as bands. All of it is gone. In its place, for each
  change, two ribbons leave the code's edge over their own rows — the struck
  rows on the removed field with its edge, the added rows on the added field
  with its edge — and converge on a short spine in the gutter. The spine is
  the change. The current change's spine takes the same blue the current
  row's left edge already takes; the others take `--ink-3`. The fills and
  edge colours are the two-pane ribbons' own (`#3E1F1F`/`#933030` and
  `#153622`/`#145E20`), so nothing new is invented.
- **What it connects, settled.** In two panes the map joins a row on the
  left to the row it corresponds to on the right. With one column both sides
  are already in the same document, so what the map joins is the two halves
  of one change — the base side's rows and the head side's rows — which is
  the card's "the two sides' line identities within the single document".
- **Pure insertion and pure deletion, settled.** A pure insertion draws only
  the added wedge against the spine; a pure deletion only the struck one. An
  empty side reads as an empty side rather than as a ribbon that failed to
  draw.
- **The lens, settled.** The map spans the rows on screen and nothing more,
  exactly like the two-pane map. The whole-file overview v6 drew here has no
  counterpart in the two-pane state, so keeping it would have made this a
  different element again. If a whole-file overview is wanted it is a
  separate element and a separate decision; that is said in the state's own
  prose and in the ask.
- **The strip toggle is gone** and a View menu with `Line numbers` and
  `One pane` both checked is drawn below the window, with a caption saying
  why nothing in the window carries the choice: on macOS the menu bar is not
  in the window. No shortcut is proposed for One pane.
- **No row moved.** Unified-diff order was already what v6 drew.

### Reworded amendment, the Modalities paragraph

Replacing BPR-0734's proposed text, to follow "and keeps comments beside the
code;" in RFC-BPR-0028's Modalities paragraph:

> Read presents the comparison in one of two ways: two whole-document panes
> over the link map, or one document in reading order with the other side's
> lines kept in place and struck through on their theme field. The link map
> is the same map in both: with one column it has no second pane to cross
> to, so it joins the two halves of each change — the struck rows and the
> added rows — at a spine in the gutter at the left, and a change with an
> empty side draws one ribbon rather than two. The presentation is a View
> item beside Line numbers, remembered per window and seeded for a new
> window from the profile default, and it changes nothing about which lines
> the comparison contains ([[BPR-0729]], owner direction 2026-09-11 and
> 2026-09-12).

This is the sentence BPR-0734's version got wrong in two places: it called
the gutter element a change map and it put the choice in the strip.

### Amendment, AC-05: unchanged

BPR-0734's AC-05 text stands as proposed. The redraw moved the map, not the
threads: a thread still names a side and a line and still sits against the
row carrying that identity, and nothing about collision order changed.

### The non-goal, worded

RFC-BPR-0028's non-goal is worded against this state and nothing wider: the
link map may be extended to the one-pane presentation, where it joins a
change's two halves in one column instead of joining two panes. The two-pane
ribbons, the diff themes and the thread map stay settled work. That wording
is on the RFC.

### Verification

- `chrome --headless=new --window-size=1500,1180 --force-device-scale-factor=2`
  over the state, rendered and inspected: the two ribbons and the spine land
  on rows 413 and 422 as drawn, the current change's spine is blue and the
  other grey, and the View menu renders below the window. A first render was
  wrong and was fixed rather than described: the new menu block collided with
  the page's existing `.menu` class, which is `position: absolute` for the
  epoch line's context menu (`v7:62`), and floated over the window; the new
  block is `.vmenu`.
- `verify_documentation.py --root .` is recorded with this card's commit.

### Still to ask

Nothing about this state is settled by silence. The ask carries: the state
itself; the reworded Modalities amendment and the unchanged AC-05; that
checking One pane gives Review one pane too, with the flip offered; and that
the whole-file overview is gone.

## Attended candidate, 2026-09-12 (v009)

The redrawn state is asked as candidate `BPR-0734-read-one-pane` in
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
candidate revision `design-v7-2026-09-12`; prepared and protected at sequence
357, undecided, no host running until the owner says so. It keeps [[BPR-0734]]'s
candidate id rather than replacing it with a new one, which is what that card's
acceptance requires. No bundle is involved: it is a design page. The ask carries
the state, the reworded Modalities amendment, the unchanged AC-05, and the two
consequences the owner may want to refuse — Review taking one pane too, and the
whole-file overview being gone.
