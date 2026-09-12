---
schema: "implement-rfc.card.v1"
id: "BPR-0745"
title: "The one-pane Read state is redrawn over the link map"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"bd627bcb-7e53-4842-85f1-1efcf3f5e363"}
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
