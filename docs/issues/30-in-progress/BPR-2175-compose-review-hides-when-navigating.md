---
schema: "implement-rfc.card.v1"
id: "BPR-2175"
title: "Compose review hides when I navigate elsewhere"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2172--BPR-2175--e382f3fed4fa.json", "origin": "owner aside, look v007 of RFC-BPR-0127 (attended session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)"}
---

# BPR-2175: Compose review hides when I navigate elsewhere

The owner, in the same note on `finish-review-local-line`:

> I feel like clicking off compose review into the diff or drawers or review bar or using the mode tabs should hide it. It feels more in the way than helpful staying open when i'm trying to navigate the app.

Then, on this card:

> hmm maybe compose review should use the light box we mocked up but decided not to use for big agent responses instead

And:

> Yeah just make sure we can still do basic window controls while lightbox stays open like drag window or resize it

## Proposed direction (needs a mockup look before building)

Compose review / Finish your review opens as the lightbox from the Ask mockups, the variant mocked for big agent responses and not chosen there ([docs/design/ask-v1](../../design/ask-v1/README.md), `png/lbx-agent-light.png` and `png/lbx-tour-light.png`). It is dismissed by a click outside it, Esc, or a mode tab, keeping its words as Close does; opening it again shows it as it was.

## Outcome

1. A mockup of Compose review as that lightbox (light and dark, the narrow and wide windows), shown to the owner as an attended look, one Accept/Reject. No building before it is accepted.
2. Then: a click inside the window's content outside the lightbox (the diff, a drawer, the review bar), Esc, or choosing another mode tab closes it with its words kept; a test drives each gesture; owner look on a frozen build.
3. While the lightbox is open the window keeps its own controls: it can be dragged by its title bar, resized, minimised, zoomed and made full screen. The lightbox stays open and re-fits the new size. Window-chrome interaction never dismisses it; only the gestures in 2 do. A test resizes, minimises and restores the window with the lightbox open and checks it is still open and fitted.

## Mockup look v001 (session `e77aa2f7d163c62e21a368b7c023b3dc`, handed back 2026-11-14T05:12:50Z)

Mockups: [compose-review-lightbox-v1](../../design/compose-review-lightbox-v1/README.md), commit `4e2087576217`.

- `BPR-2175-window-controls`: **Accept** (no note). The window's own controls
  stay live while the lightbox is open, as drawn.
- `BPR-2175-lightbox-look`: **Reject**. The owner, verbatim:

  > It looks like we changed way too much compared with the existing compose review. We lost the 1 click inteface we have right now and replaced it with a 2 stage it looks like by adding Submit review. The commnets in this mockup look worse than how they look in the current app window.
  >
  > I would like the comment box and my action to be on the right or bottom instead of the mockup's left or top.
  >
  > I would take the existing window over this design, we should try again.

Next round: today's Compose review content exactly as it is (its one-click
actions, its comment rendering), only put in the lightbox, with the comment
box and the action on the right or at the bottom; no Submit review step. Two
variants, right and bottom. Nothing is built before a variant is accepted.

## Mockup round 2 (2026-11-14)

[compose-review-lightbox-v2](../../design/compose-review-lightbox-v2/README.md) (`fab8ae093`, a separate design worker): options A to E, each today's Finish your review content with its one-click verdicts, comments and actions on the right or at the bottom. Attended session `fd03fb4ab1abc68d8f5f9ff1b903a7c0`, state `/Users/jaeger/.attended-review-state/compose-review-lightbox-v002-2026-11-14/state`: one choice among A to E or "Keep the existing window", and one Accept/Reject that the content matches today's panel.

## Mockup look v002 (session `fd03fb4ab1abc68d8f5f9ff1b903a7c0`, handed back 2026-11-14)

- `BPR-2175-v2-choice`: **D** (no note), at 2026-11-14T16:40:56Z.
- `BPR-2175-v2-content`: **Accept** (no note).

Option D is the one to build: today's Finish your review in the lightbox, two
columns (comments left; summary, one-click verdicts and actions right), each
comment's words in full up to four lines and a suggestion's change as its
diff; the window's own controls stay live while it is open (v001's accepted
point).

## Built (2026-11-14, branch `claude/parallel-backlog-2156-2172-2173`)

Option D: `compose_review_sheet` is a lightbox, the window's own child over
everything below the title row (the strip with the mode tabs and the window
controls stays live and undimmed); its panel is centred, as wide as the
window allows up to 1180 pixels and as tall as its content.
`review_submission_editor::set_two_columns` puts the comments on the left and
the summary, the one-click verdicts and the actions on the right once the
panel is 820 pixels or wider; each comment shows its words in full up to four
lines and a suggestion's lines under them; "Double-click a comment…" moves
under the comments box. The expanded page keeps today's one column.

A press on the dimmed window (the diff, a drawer, the review bar under it),
Esc, its Close and choosing a mode tab close it with its words kept; a press
inside it or on the title row does not. Resizing, minimising and restoring,
zooming and full screen keep it open and fitted.
`test_compose_review_lightbox_gestures_and_window_controls` drives each
gesture; `qt_workbench_window` at 100, 125 and 200 %,
`qt_drawer_surfaces_fit`, `application_review_submission_composition` (three
scales) and the other composed courses that open it pass. Pending: the
owner's look on a frozen build.
